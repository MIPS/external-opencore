/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#ifndef OSCL_TICKCOUNT_H_INCLUDED
#include "oscl_tickcount.h"
#endif

#ifndef PVMF_FORMAT_TYPE_H_INCLUDED
#include "pvmf_format_type.h"
#endif

#include "pvmf_recognizer_registry_impl.h"

#define LOGERROR(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iLogger,PVLOGMSG_ERR,m);
#define LOGINFOHI(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_INFO,m);
#define LOGINFOMED(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG,iLogger,PVLOGMSG_INFO,m);
#define LOGINFOLOW(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_INFO,m);
#define LOGINFO(m) LOGINFOMED(m)

PVMFRecognizerRegistryImpl::PVMFRecognizerRegistryImpl() :
        OsclTimerObject(OsclActiveObject::EPriorityNominal, "PVMFRecognizerRegistryImpl")
{
    AddToScheduler();

    iRefCount = 1;

    iNextSessionId = 0;
    iRecognizerSessionList.reserve(1);

    iNextCommandId = 0;
    iRecognizerPendingCmdList.reserve(2);
    iRecognizerCurrentCmd.reserve(1);
    iRecognizerCmdToCancel.reserve(1);

    iDataStreamFactory = NULL;
    iDataStream = NULL;
    oRecognizePending = false;

    iLogger = PVLogger::GetLoggerObject("PVMFRecognizer");
}


PVMFRecognizerRegistryImpl::~PVMFRecognizerRegistryImpl()
{
    iDataStream = NULL;
    iDataStreamFactory = NULL;
    if (iPlugInParamsVec.empty() == false)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::~PVMFRecognizerRegistryImpl - Not all plugin factories were deregistered"));
        // The plug-in factories were not removed before shutting down the registry.
        Oscl_Vector<PVMFRecognizerPluginParams*, OsclMemAllocator>::iterator it;
        for (it = iPlugInParamsVec.begin(); it != iPlugInParamsVec.end(); ++it)
        {
            // Save factory pointer. We need the factory pointer around till we
            // destory *it, since PVMFRecognizerPluginParams destructor destroys
            // the recognizer plugin, if present.
            PVMFRecognizerPluginFactory* fac = (*it)->iPluginFactory;
            // Destroy PVMFRecognizerPluginParams
            OSCL_DELETE(*it);
            // Destroy factory
            OSCL_DELETE(fac);
        }
        iPlugInParamsVec.clear();
    }
    iLogger = NULL;
}


PVMFStatus PVMFRecognizerRegistryImpl::RegisterPlugin(PVMFRecognizerPluginFactory& aPluginFactory)
{
    PVMFRecognizerPluginParams* pluginParams = NULL;
    PVMFStatus retVal = FindPluginParams(&aPluginFactory, pluginParams);
    if (retVal == PVMFSuccess)
    {
        //this plugin factory has already been registered
        LOGINFOHI((0, "PVMFRecognizerRegistryImpl::RegisterPlugin - Plugin Already Exists!"));
        return PVMFErrAlreadyExists;
    }
    //create a new plugin params
    pluginParams = OSCL_NEW(PVMFRecognizerPluginParams, ());
    if (pluginParams == NULL)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::RegisterPlugin - PVMFRecognizerPluginParams Creation Failed!"));
        return PVMFErrNoMemory;
    }
    pluginParams->iPluginFactory = &aPluginFactory;
    pluginParams->iRecPlugin = pluginParams->iPluginFactory->CreateRecognizerPlugin();
    if (pluginParams->iRecPlugin == NULL)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::RegisterPlugin - CreateRecognizerPlugin Failed!"));
        OSCL_DELETE(pluginParams);
        return PVMFErrNoMemory;
    }
    retVal = pluginParams->iRecPlugin->SupportedFormats(pluginParams->iSupportedFormatList);
    if ((retVal != PVMFSuccess) || (pluginParams->iSupportedFormatList.size() == 0))
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::RegisterPlugin - SupportedFormats Failed!"));
        OSCL_DELETE(pluginParams);
        return retVal;
    }
    retVal = pluginParams->iRecPlugin->GetRequiredMinBytesForRecognition(pluginParams->iMinSizeRequiredForRecognition);
    if (retVal != PVMFSuccess)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::RegisterPlugin - GetRequiredMinBytesForRecognition Failed!"));
        OSCL_DELETE(pluginParams);
        return retVal;
    }
    // Add the plug-in params to the list
    iPlugInParamsVec.push_back(pluginParams);
    return PVMFSuccess;
}


PVMFStatus PVMFRecognizerRegistryImpl::RemovePlugin(PVMFRecognizerPluginFactory& aPluginFactory)
{
    // Find the specified plug-in factory and remove from list
    Oscl_Vector<PVMFRecognizerPluginParams*, OsclMemAllocator>::iterator it;
    for (it = iPlugInParamsVec.begin(); it != iPlugInParamsVec.end(); ++it)
    {
        if ((*it)->iPluginFactory == &aPluginFactory)
        {
            PVMFRecognizerPluginParams* pluginParams = *it;
            iPlugInParamsVec.erase(it);
            //next delete plugin params
            OSCL_DELETE(pluginParams);
            return PVMFSuccess;
        }
    }
    LOGERROR((0, "PVMFRecognizerRegistryImpl::RemovePlugin Failed!"));
    return PVMFErrArgument;
}


PVMFStatus PVMFRecognizerRegistryImpl::OpenSession(PVMFSessionId& aSessionId, PVMFRecognizerCommmandHandler& aCmdHandler)
{
    // TEMP: Currently only allow one session at a time
    if (iRecognizerSessionList.empty() == false)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::OpenSession Failed!"));
        return PVMFErrBusy;
    }

    // Add this session to the list
    PVMFRecRegSessionInfo recsessioninfo;
    recsessioninfo.iRecRegSessionId = iNextSessionId;
    recsessioninfo.iRecRegCmdHandler = &aCmdHandler;
    int32 leavecode = 0;
    OSCL_TRY(leavecode, iRecognizerSessionList.push_back(recsessioninfo));
    OSCL_FIRST_CATCH_ANY(leavecode,
                         LOGERROR((0, "PVMFRecognizerRegistryImpl::OpenSession - No Memory"));
                         return PVMFErrNoMemory;
                        );

    aSessionId = recsessioninfo.iRecRegSessionId;

    // Increment the session ID counter
    ++iNextSessionId;

    return PVMFSuccess;
}


PVMFStatus PVMFRecognizerRegistryImpl::CloseSession(PVMFSessionId aSessionId)
{
    if (iRecognizerSessionList.empty() == true)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::CloseSession Failed!"));
        return PVMFErrInvalidState;
    }

    // Search for the session in the list by the ID
    uint32 i;
    for (i = 0; i < iRecognizerSessionList.size(); ++i)
    {
        if (iRecognizerSessionList[i].iRecRegSessionId == aSessionId)
        {
            break;
        }
    }

    // Check if the session was not found
    if (i >= iRecognizerSessionList.size())
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::CloseSession - Session Not Found"));
        return PVMFErrArgument;
    }

    // Erase the session from the list to close the session
    iRecognizerSessionList.erase(iRecognizerSessionList.begin() + i);
    return PVMFSuccess;
}


PVMFCommandId PVMFRecognizerRegistryImpl::Recognize(PVMFSessionId aSessionId,
        PVMFDataStreamFactory& aSourceDataStreamFactory,
        PVMFRecognizerMIMEStringList* aFormatHint,
        Oscl_Vector<PVMFRecognizerResult, OsclMemAllocator>& aRecognizerResult,
        OsclAny* aCmdContext,
        uint32 aTimeout)
{
    if ((iRecognizerSessionList.empty() == true) || (oRecognizePending == true))
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::Recognize OsclErrInvalidState"));
        OSCL_LEAVE(OsclErrInvalidState);
    }
    if (aSessionId != iRecognizerSessionList[0].iRecRegSessionId)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::Recognize OsclErrArgument"));
        OSCL_LEAVE(OsclErrArgument);
    }
    // TEMP: Only allow one recognize command at a time
    if (!(iRecognizerPendingCmdList.empty() == true && iRecognizerCurrentCmd.empty() == true))
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::Recognize OsclErrBusy"));
        OSCL_LEAVE(OsclErrBusy);
    }
    // TEMP: Only allow timeout of 0
    if (aTimeout > 0)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::Recognize OsclErrArgument - aTimeout>0"));
        OSCL_LEAVE(OsclErrArgument);
    }

    // Save the passed-in parameters in a vector
    Oscl_Vector<PVMFRecRegImplCommandParamUnion, OsclMemAllocator> paramvector;
    paramvector.reserve(4);
    PVMFRecRegImplCommandParamUnion paramval;
    paramval.pOsclAny_value = (OsclAny*) & aSourceDataStreamFactory;
    paramvector.push_back(paramval);
    paramval.pOsclAny_value = (OsclAny*)aFormatHint;
    paramvector.push_back(paramval);
    paramval.pOsclAny_value = (OsclAny*) & aRecognizerResult;
    paramvector.push_back(paramval);
    paramval.uint32_value = aTimeout;
    paramvector.push_back(paramval);

    // Add the command to the pending list
    return AddRecRegCommand(aSessionId, PVMFRECREG_COMMAND_RECOGNIZE, aCmdContext, &paramvector, true);
}


PVMFCommandId PVMFRecognizerRegistryImpl::CancelCommand(PVMFSessionId aSessionId, PVMFCommandId aCommandToCancelId, OsclAny* aCmdContext)
{
    if (iRecognizerSessionList.empty() == true)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::CancelCommand - Session List Empty - Leaving"));
        OSCL_LEAVE(OsclErrInvalidState);
    }
    if (aSessionId != iRecognizerSessionList[0].iRecRegSessionId)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::CancelCommand - Session Invalid - Leaving"));
        OSCL_LEAVE(OsclErrArgument);
    }

    // Save the passed-in parameters in a vector
    Oscl_Vector<PVMFRecRegImplCommandParamUnion, OsclMemAllocator> paramvector;
    paramvector.reserve(1);
    PVMFRecRegImplCommandParamUnion paramval;
    paramval.int32_value = aCommandToCancelId;
    paramvector.push_back(paramval);

    // Add the command to the pending list
    return AddRecRegCommand(aSessionId, PVMFRECREG_COMMAND_CANCELCOMMAND, aCmdContext, &paramvector, true);
}


void PVMFRecognizerRegistryImpl::Run()
{
    // Check if CancelCommand() request was made
    if (!iRecognizerPendingCmdList.empty())
    {
        if (iRecognizerPendingCmdList.top().GetCmdType() == PVMFRECREG_COMMAND_CANCELCOMMAND)
        {
            // Process it right away
            PVMFRecRegImplCommand cmd(iRecognizerPendingCmdList.top());
            iRecognizerPendingCmdList.pop();
            DoCancelCommand(cmd);
            return;
        }
    }

    int32 cmdType = PVMFRECREG_COMMAND_UNDEFINED;
    // Handle other requests normally
    if (iRecognizerCurrentCmd.empty())
    {
        if (!iRecognizerPendingCmdList.empty())
        {
            // Retrieve the first pending command from queue
            PVMFRecRegImplCommand cmd(iRecognizerPendingCmdList.top());
            iRecognizerPendingCmdList.pop();
            cmdType = cmd.GetCmdType();
            // Put in on the current command queue - A leave means system is out of memory
            // let it propagate up the call stack
            iRecognizerCurrentCmd.push_front(cmd);
        }
    }
    else
    {
        cmdType = iRecognizerCurrentCmd[0].GetCmdType();
    }

    if (cmdType != PVMFRECREG_COMMAND_UNDEFINED)
    {
        // Process the command according to the cmd type
        PVMFStatus cmdstatus = PVMFSuccess;
        switch (cmdType)
        {
            case PVMFRECREG_COMMAND_RECOGNIZE:
                //Recognize command always completes from inside of DoRecognize or DataStreamCommandCompleted
                DoRecognize();
                break;

            default:
                LOGERROR((0, "PVMFRecognizerRegistryImpl::Run - Unrecognized Command"));
                cmdstatus = PVMFErrNotSupported;
                break;
        }
        if (cmdstatus != PVMFSuccess)
        {
            CompleteCurrentRecRegCommand(cmdstatus);
        }
    }
}


PVMFStatus PVMFRecognizerRegistryImpl::FindPluginParams(PVMFRecognizerPluginFactory* aFactory,
        PVMFRecognizerPluginParams*& aParams)
{
    PVMFStatus retval = PVMFErrArgument;
    aParams = NULL;
    // Check if the specified factory exists in the list
    Oscl_Vector<PVMFRecognizerPluginParams*, OsclMemAllocator>::iterator it;
    for (it = iPlugInParamsVec.begin(); it != iPlugInParamsVec.end(); ++it)
    {
        if ((*it)->iPluginFactory == aFactory)
        {
            aParams = *it;
            retval = PVMFSuccess;
            break;
        }
    }
    return retval;
}

PVMFCommandId PVMFRecognizerRegistryImpl::AddRecRegCommand(PVMFSessionId aSessionId, int32 aCmdType, OsclAny* aContextData, Oscl_Vector<PVMFRecRegImplCommandParamUnion, OsclMemAllocator>* aParamVector, bool aAPICommand)
{
    PVMFRecRegImplCommand cmd(aSessionId, aCmdType, iNextCommandId, aContextData, aParamVector, aAPICommand);
    int32 leavecode = 0;
    OSCL_TRY(leavecode, iRecognizerPendingCmdList.push(cmd));
    OSCL_FIRST_CATCH_ANY(leavecode,
                         OSCL_LEAVE(OsclErrNoMemory);
                         return 0;);
    RunIfNotReady();
    ++iNextCommandId;

    return cmd.GetCmdId();
}


void PVMFRecognizerRegistryImpl::CompleteCurrentRecRegCommand(PVMFStatus aStatus, const uint32 aCurrCmdIndex, PVInterface* aExtInterface)
{
    if (iRecognizerCurrentCmd.empty() == true)
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::CompleteCurrentRecRegCommand Current Cmd Q empty - Leaving"));
        // No command to complete. Leave
        OSCL_LEAVE(OsclErrInvalidState);
        return;
    }

    // Save the command complete on stack and remove from queue
    PVMFRecRegImplCommand cmdtocomplete(iRecognizerCurrentCmd[aCurrCmdIndex]);
    iRecognizerCurrentCmd.clear();

    //If it is recognize command make sure to delete iDataStream. This datastream was created using the factory
    //provided in recognize command and we need to make sure we delete this before the command completes.
    //Also reset fields in PVMFRecognizerPluginParams across all plugins that are specific to each recognize command.
    if (cmdtocomplete.GetCmdType() == PVMFRECREG_COMMAND_RECOGNIZE)
    {
        DestroyDataStream();
        PVMFStatus status = ResetPluginParamsPerRecognizeCmd();
        if (status != PVMFSuccess)
        {
            //fail recognize cmd
            aStatus = status;
        }
    }
    // Make callback if API command
    if (cmdtocomplete.IsAPICommand())
    {
        if (iRecognizerSessionList.empty() == false)
        {
            OSCL_ASSERT(iRecognizerSessionList[aCurrCmdIndex].iRecRegSessionId == cmdtocomplete.GetSessionId());
            PVMFCmdResp cmdresp(cmdtocomplete.GetCmdId(), cmdtocomplete.GetContext(), aStatus, aExtInterface);
            iRecognizerSessionList[aCurrCmdIndex].iRecRegCmdHandler->RecognizerCommandCompleted(cmdresp);
        }
    }

    // Need to make this AO active if there are pending commands
    if (iRecognizerPendingCmdList.empty() == false)
    {
        RunIfNotReady();
    }
}

PVMFStatus PVMFRecognizerRegistryImpl::GetMaxRequiredSizeForRecognition(TOsclFileOffset& aMaxSize)
{
    if (iPlugInParamsVec.size() == 0) return PVMFFailure;
    aMaxSize = 0;
    Oscl_Vector<PVMFRecognizerPluginParams*, OsclMemAllocator>::iterator it;
    for (it = iPlugInParamsVec.begin(); it != iPlugInParamsVec.end(); ++it)
    {
        if ((*it)->iInUse == true)
        {
            if ((TOsclFileOffset)((*it)->iMinSizeRequiredForRecognition) > aMaxSize)
            {
                aMaxSize = (*it)->iMinSizeRequiredForRecognition;
            }
        }
    }
    if (aMaxSize != 0)
    {
        return PVMFSuccess;
    }
    //no active plugins or all plugins say they need 0 bytes - in either case fail
    return PVMFFailure;
}

PVMFStatus PVMFRecognizerRegistryImpl::CreateDataStream()
{
    PVMFStatus status = PVMFFailure;
    iDataStream = NULL;
    if (iDataStreamFactory != NULL)
    {
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        PVInterface* intf =
            iDataStreamFactory->CreatePVMFCPMPluginAccessInterface(uuid);
        if (intf != NULL)
        {
            iDataStream = OSCL_STATIC_CAST(PVMIDataStreamSyncInterface*, intf);
            if (iDataStream->OpenSession(iDataStreamSessionID, PVDS_READ_ONLY) == PVDS_SUCCESS)
            {
                status = PVMFSuccess;
            }
            else
            {
                LOGERROR((0, "PVMFRecognizerRegistryImpl::CreateDataStream - OpenSession Failed"));
            }
        }
        else
        {
            LOGERROR((0, "PVMFRecognizerRegistryImpl::CreateDataStream - No Memory"));
            status = PVMFErrNoMemory;
        }
    }
    return status;
}

void PVMFRecognizerRegistryImpl::DestroyDataStream()
{
    if ((iDataStreamFactory != NULL) && (iDataStream != NULL))
    {
        iDataStream->CloseSession(iDataStreamSessionID);
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        iDataStreamFactory->DestroyPVMFCPMPluginAccessInterface(uuid,
                OSCL_STATIC_CAST(PVInterface*, iDataStream));
        iDataStream = NULL;
        iDataStreamFactory = NULL;
    }
    return;
}

PVMFStatus PVMFRecognizerRegistryImpl::ResetPluginParamsPerRecognizeCmd()
{
    PVMFStatus status = PVMFFailure;
    Oscl_Vector<PVMFRecognizerPluginParams*, OsclMemAllocator>::iterator it;
    for (it = iPlugInParamsVec.begin(); it != iPlugInParamsVec.end(); ++it)
    {
        status = (*it)->ResetParamsPerRecognizeCmd();
        if (status != PVMFSuccess)
        {
            return status;
        }
    }
    return status;
}

PVMFStatus PVMFRecognizerRegistryImpl::GetRequestReadCapacityNotificationID(TOsclFileOffset aMaxSize, TOsclFileOffset aCapacity)
{
    int32 errcode = 0;
    OSCL_TRY(errcode,
             iRequestReadCapacityNotificationID =
                 iDataStream->RequestReadCapacityNotification(iDataStreamSessionID,
                         *this,
                         aMaxSize);
            );
    OSCL_FIRST_CATCH_ANY(errcode,
                         LOGERROR((0, "PVMFRecognizerRegistryImpl::CheckForDataAvailability - RequestReadCapacityNotification Failed"));
                         return PVMFFailure);
    LOGINFO((0, "PVMFRecognizerRegistryImpl::CheckForDataAvailability - DSRequest - MaxSize=%d, Capacity=%d",
             aMaxSize, aCapacity));
    return PVMFSuccess;
}

PVMFStatus PVMFRecognizerRegistryImpl::CheckForDataAvailability()
{
    PVMFStatus retval = PVMFFailure;
    if (iDataStream != NULL)
    {
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        TOsclFileOffset maxSize = 0;
        retval = GetMaxRequiredSizeForRecognition(maxSize);
        if (retval == PVMFSuccess)
        {
            TOsclFileOffset capacity = 0;
            PvmiDataStreamStatus status =
                iDataStream->QueryReadCapacity(iDataStreamSessionID, capacity);

            if (capacity < maxSize)
            {
                // Get total content size to deal with cases where file being recognized is less than maxSize
                TOsclFileOffset totalSize = iDataStream->GetContentLength();
                if ((status == PVDS_END_OF_STREAM) || ((capacity == totalSize) && (capacity != 0)))
                {
                    LOGINFO((0, "PVMFRecognizerRegistryImpl::CheckForDataAvailability - EOS - TotalSize=%d, Capacity=%d",
                             totalSize, capacity));
                    retval = PVMFSuccess;
                }
                else
                {
                    if (GetRequestReadCapacityNotificationID(maxSize, capacity) != PVMFSuccess)
                    {
                        return PVMFFailure;
                    }
                    retval = PVMFPending;
                }
            }
            else
            {
                LOGINFO((0, "PVMFRecognizerRegistryImpl::CheckForDataAvailability - MaxSize=%d, Capacity=%d",
                         maxSize, capacity));
                retval = PVMFSuccess;
            }
        }
    }
    if ((retval != PVMFSuccess) && (retval != PVMFPending))
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::CheckForDataAvailability Failed"));
    }
    return retval;
}

PVMFStatus PVMFRecognizerRegistryImpl::RunRecognitionPass(PVMFRecognizerResult& aResult)
{
    if ((iRecognizerCurrentCmd.empty() == true) ||
            (iRecognizerCurrentCmd[0].GetCmdType() != PVMFRECREG_COMMAND_RECOGNIZE) ||
            (iDataStream == NULL))
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::RunRecognitionPass - Invalid State"));
        return PVMFErrInvalidState;
    }

    OSCL_HeapString<OsclMemAllocator> unknown_fmt = PVMF_MIME_FORMAT_UNKNOWN;
    bool oRunOneMorePass = false;
    bool oMultipleFormatsPossible = false;
    OSCL_HeapString<OsclMemAllocator> possibleFormat = PVMF_MIME_FORMAT_UNKNOWN; //this is used in case just one plugin reports possible
    uint32 i = 0;
    PVMFRecognizerMIMEStringList* hintlist =
        (PVMFRecognizerMIMEStringList*) iRecognizerCurrentCmd[0].GetParam(1).pOsclAny_value;
    Oscl_Vector<PVMFRecognizerPluginParams*, OsclMemAllocator>::iterator it;
    for (it = iPlugInParamsVec.begin(); it != iPlugInParamsVec.end(); ++it)
    {
        i++;
        if ((*it)->iInUse == true)
        {
            //Reset result structure
            aResult.Reset();

            LOGINFO((0, "PVMFRecognizerRegistryImpl::RunRecognitionPass Recognize plug-in Start i=%d", i));
            uint32 currticks = OsclTickCount::TickCount();
            uint32 starttime = OsclTickCount::TicksToMsec(currticks);
            OSCL_UNUSED_ARG(starttime);

            // Perform recognition with this recognizer plug-ing
            PVMFStatus recstatus =
                (*it)->iRecPlugin->Recognize(*iDataStreamFactory, hintlist, aResult);

            currticks = OsclTickCount::TickCount();
            uint32 endtime = OsclTickCount::TicksToMsec(currticks);
            OSCL_UNUSED_ARG(endtime);

            LOGINFO((0, "PVMFRecognizerRegistryImpl::RunRecognitionPass Recognize plug-in End i=%d  confidence=%d, time=%d, mime=%s, AddnlBytesReqd=%d",
                     i, aResult.iRecognitionConfidence, (endtime - starttime), aResult.iRecognizedFormat.get_cstr(), aResult.iAdditionalBytesRequired));

            if (recstatus != PVMFSuccess)
            {
                LOGERROR((0, "PVMFRecognizerRegistryImpl::RunRecognitionPass Recognize plug-in Failed i=%d", i));
                //something went wrong in this recognizer - stop using it
                (*it)->iInUse = false;
            }
            else
            {
                if (aResult.iRecognitionConfidence == PVMFRecognizerConfidenceCertain)
                {
                    //we are done with this recognizer, one way or other, do not use it anymore
                    (*it)->iInUse = false;
                    if (aResult.iRecognizedFormat != unknown_fmt)
                    {
                        //we have a definite positive result - we can stop now
                        return PVMFSuccess;
                    }
                }
                else if (aResult.iRecognitionConfidence == PVMFRecognizerConfidencePossible)
                {
                    //We need to keep using this recognizer till we get a certain result,
                    //provided recognizer is asking for more data. If it is not then
                    //it is not correct behaviour to say possible and not ask for more data.
                    //Recognizer must also provide a valid format type.
                    //So stop using the recognizer if it does not do either of these things.
                    if ((aResult.iAdditionalBytesRequired != 0) &&
                            (aResult.iRecognizedFormat != unknown_fmt))
                    {
                        (*it)->iMinSizeRequiredForRecognition += aResult.iAdditionalBytesRequired;
                        //just make sure that recognizer does not need more data than content length
                        TOsclFileOffset capacity = 0;
                        PvmiDataStreamStatus status =
                            iDataStream->QueryReadCapacity(iDataStreamSessionID, capacity);
                        TOsclFileOffset totalSize = iDataStream->GetContentLength();
                        if ((status == PVDS_END_OF_STREAM) ||
                                ((capacity == totalSize) && ((TOsclFileOffset)(*it)->iMinSizeRequiredForRecognition > totalSize)))
                        {
                            //we pretty much have the complete file and if a recognizer still
                            //cannot make up its mind, then stop using it.
                            LOGERROR((0, "PVMFRecognizerRegistryImpl::RunRecognitionPass - Plugin returns PVMFRecognizerConfidencePossible with a complete file/stream"));
                            (*it)->iInUse = false;
                        }
                    }
                    else
                    {
                        LOGERROR((0, "PVMFRecognizerRegistryImpl::RunRecognitionPass - Plugin returns PVMFRecognizerConfidencePossible with invalid formattype/addnlbytes"));
                        (*it)->iInUse = false;
                    }
                    //if this recognizer has not been disabled it means that we need to run one more pass
                    if ((*it)->iInUse == true)
                    {
                        if (oRunOneMorePass == true)
                        {
                            //this means that previously another plugin returned possible, asking for more data
                            oMultipleFormatsPossible = true;
                        }
                        //save the result
                        possibleFormat = aResult.iRecognizedFormat;
                        oRunOneMorePass = true;
                    }
                }
            }
        }
    }
    //if we get here it means one of two things:
    //(1) All recognizers have rejected the file for certain or we have deactivated all recognizers (due to incorrect behaviour)
    //(2) Atleast one recgonizer needs additional data to make up its mind, in which case oRunOneMorePass would be true.
    if (oRunOneMorePass == true)
    {
        //Additional recognition passes required if there is more than one plugin that reports possible.
        //If just one plugin reports possible, then we assume that it is the format. There is not much point in
        //running additional passes.
        if (oMultipleFormatsPossible == true)
        {
            return PVMFPending;
        }
    }
    //we are done - we could not recognize the format in which case possibleFormat would be PVMF_MIME_FORMAT_UNKNOWN
    //or just one plugin returned possible and we assume that the clip is of that format.
    aResult.iRecognitionConfidence = PVMFRecognizerConfidenceCertain;
    aResult.iRecognizedFormat = possibleFormat;
    aResult.iAdditionalBytesRequired = 0;
    return PVMFSuccess;
}

void PVMFRecognizerRegistryImpl::DoRecognize()
{
    if (iRecognizerCurrentCmd.empty() == true)
    {
        //error - this method should not have been called - do a leave
        LOGERROR((0, "PVMFRecognizerRegistryImpl::DoRecognize - Invalid State"));
        OSCL_LEAVE(OsclErrInvalidState);
    }

    Oscl_Vector<PVMFRecognizerResult, OsclMemAllocator>* recresult =
        (Oscl_Vector<PVMFRecognizerResult, OsclMemAllocator>*) iRecognizerCurrentCmd[0].GetParam(2).pOsclAny_value;

    //Perform additional setup steps in case we enter this function for the first time
    if (oRecognizePending == false)
    {
        // Retrieve the command parameters
        iDataStreamFactory =
            (PVMFDataStreamFactory*) iRecognizerCurrentCmd[0].GetParam(0).pOsclAny_value;

        // Validate the parameters
        if (iDataStreamFactory == NULL || recresult == NULL)
        {
            LOGERROR((0, "PVMFRecognizerRegistryImpl::DoRecognize - Invalid Cmd Arguments"));
            CompleteCurrentRecRegCommand(PVMFErrArgument);
            return;
        }

        if (iDataStream == NULL)
        {
            // Create datastream
            PVMFStatus status = CreateDataStream();
            if (status != PVMFSuccess)
            {
                LOGERROR((0, "PVMFRecognizerRegistryImpl::DoRecognize - CreateDataStream Failed"));
                CompleteCurrentRecRegCommand(status);
                return;
            }
        }
    }
    else
    {
        //set it to false - in case we need additional passes this will be set to true below
        oRecognizePending = false;
    }

    //Here is how recognize works. First we check to see if we have enough data to run a pass.
    //If we do, then we would, and in this pass some recognizers could: a) get a postive result
    //b) get a negative result c) or determine that we need additional data to know for sure.
    //In case of (a) or (b), we complete the recognize command. In case of (c) we run additional
    //pass(es). While running a pass, we could also eliminate some of the plugins.

    bool oRunRecognition = true;
    PVMFStatus status = PVMFFailure;
    while (oRunRecognition)
    {
        status = CheckForDataAvailability();
        if (status == PVMFSuccess)
        {
            // This implies that all plugins have the required minimum number of bytes to start the recognition
            // This does NOT mean that we will get a definite result in first pass.
            PVMFRecognizerResult result;
            status = RunRecognitionPass(result);
            if (status != PVMFPending)
            {
                //means success or failure - in either case we are done with recognition
                recresult->push_back(result);
                break;
            }
            //We intentionally do nothing here. RunRecognitionPass has returned PVMFPending
            //and we need additional passes to finish recognizing the file/stream. Therefore we just go back into the while loop.
            //RunRecognitionPass would have updated iMinSizeRequiredForRecognition in PVMFRecognizerPluginParams that corresponds
            //to the plugin that needs more data to make up its mind. CheckForDataAvailability will check to see if datastream
            //has enough data to run a pass right away. If it does not we would break out of the while loop and wait on datastream
            //callback.
        }
        else
        {
            //we either have a failure or need more data
            break;
        }
    }
    //intentional fall thru
    if (status == PVMFPending)
    {
        // wait for datastream call back
        oRecognizePending = true;
    }
    else
    {
        //success or failure
        CompleteCurrentRecRegCommand(status);
        return;
    }
}



void PVMFRecognizerRegistryImpl::DoCancelCommand(PVMFRecRegImplCommand& aCmd)
{
    // TEMP: For now only one command can be cancelled.
    // Since Recognize happens in one AO call, check in the pending cmd queue
    if (iRecognizerPendingCmdList.empty() == false)
    {
        iRecognizerCurrentCmd.push_front(iRecognizerPendingCmdList.top());
        iRecognizerPendingCmdList.pop();
        OSCL_ASSERT(iRecognizerCurrentCmd[0].GetCmdId() == aCmd.GetCmdId());
    }

    if (iRecognizerCurrentCmd.empty() == false)
    {
        // get the commandToCancelId
        PVMFRecRegImplCommandParamUnion paramval = aCmd.GetParam(0);
        PVMFCommandId commandToCancelId = paramval.int32_value;
        if (FindCommandByID(iRecognizerCurrentCmd, commandToCancelId) == false) return;
        CompleteCurrentRecRegCommand(PVMFErrCancelled, commandToCancelId);
    }

}

bool PVMFRecognizerRegistryImpl::FindCommandByID(Oscl_Vector<PVMFRecRegImplCommand, OsclMemAllocator> &aCmdQueue, const PVMFCommandId aCmdId)
{
    if (aCmdQueue.empty()) return false;
    for (uint32 i = 0; i < aCmdQueue.size(); i++)
    {
        if (aCmdQueue[i].GetCmdId() == aCmdId) return true;
    }
    return false;
}

void PVMFRecognizerRegistryImpl::DataStreamCommandCompleted(const PVMFCmdResp& aResponse)
{
    if (aResponse.GetCmdId() == iRequestReadCapacityNotificationID)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            LOGINFO((0, "PVMFRecognizerRegistryImpl::DataStreamCommandCompleted Success"));
            //reschedule AO to complete recongnize command
            RunIfNotReady();
        }
        else
        {
            LOGERROR((0, "PVMFRecognizerRegistryImpl::DataStreamCommandCompleted failed - status=%d", aResponse.GetCmdStatus()));
            if (iRecognizerCurrentCmd.empty() == false)
            {
                //fail ongoing command
                CompleteCurrentRecRegCommand(aResponse.GetCmdStatus());
            }
        }
    }
    else
    {
        LOGERROR((0, "PVMFRecognizerRegistryImpl::DataStreamCommandCompleted failed - Unknown cmd id"));
        if (iRecognizerCurrentCmd.empty() == false)
        {
            //fail ongoing command
            CompleteCurrentRecRegCommand(aResponse.GetCmdStatus());
        }
    }
}

void PVMFRecognizerRegistryImpl::DataStreamInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_LEAVE(OsclErrNotSupported);
}

void PVMFRecognizerRegistryImpl::DataStreamErrorEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_LEAVE(OsclErrNotSupported);
}








