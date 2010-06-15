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
#include "dummy_output_mio.h"
#include "pv_mime_string_utils.h"

//#include "test_utility.h"

#define LIPSYNCDUMMYOUTPUTMIO_LOGERROR(m)       PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_ERR,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGWARNING(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_WARNING,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGINFO(m)        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_INFO,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG(m)       PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_DEBUG,m);


DummyOutputMIO::DummyOutputMIO(const DummyMIOSettings& aSettings)
        : OsclActiveObject(OsclActiveObject::EPriorityNominal, aSettings.iMediaFormat.isAudio() ? "LipSyncDummyAudioOutputMIO" : "LipSyncDummyVideoOutputMIO")
{
    iObserver = NULL;
    iCommandCounter = 0;
    iCommandPendingQueue.reserve(1);
    iCommandResponseQueue.reserve(5);
    iState = STATE_IDLE;
    iIsMIOConfigured = false;
    iSettings = aSettings;
    iIsAudioMIO = iSettings.iMediaFormat.isAudio();
    iIsVideoMIO = iSettings.iMediaFormat.isVideo();
    iIsCompressed = iSettings.iMediaFormat.isCompressed();
    iMIOObserver = iSettings.iDummyMIOObserver;
    iLogger = PVLogger::GetLoggerObject("DummyOutputMIO");

    iPeer = NULL;

    iMIOObserver = NULL;

}
void DummyOutputMIO::ResetData()
{
//reset all data from this session so that a new clip can be played.
    Cleanup();

    iIsMIOConfigured = false;
}

PVMFCommandId DummyOutputMIO::Reset(const OsclAny* aContext)
{
    // Reset all data from this session
    ResetData();

    // TEMP to properly behave asynchronously
    PVMFCommandId cmdid = iCommandCounter++;
    CommandResponse resp(PVMFSuccess, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

void DummyOutputMIO::Cleanup()
{
    // Send any pending command responses.
    while (!iCommandPendingQueue.empty())
    {
        if (iObserver)
        {
            iObserver->RequestCompleted(
                PVMFCmdResp(
                    iCommandPendingQueue[0].iCmdId,
                    iCommandPendingQueue[0].iContext,
                    PVMFFailure));
        }
        iCommandPendingQueue.erase(&iCommandPendingQueue[0]);
    }
    while (!iCommandResponseQueue.empty())
    {
        if (iObserver)
        {
            iObserver->RequestCompleted(
                PVMFCmdResp(
                    iCommandResponseQueue[0].iCmdId,
                    iCommandResponseQueue[0].iContext,
                    iCommandResponseQueue[0].iStatus));
        }
        iCommandResponseQueue.erase(&iCommandResponseQueue[0]);
    }
}

DummyOutputMIO::~DummyOutputMIO()
{
    Cleanup();
    // release semaphore
}

PVMFStatus DummyOutputMIO::connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver)
{
    OSCL_UNUSED_ARG(aSession);

    // Currently supports only one session
    if (iObserver)
    {
        LIPSYNCDUMMYOUTPUTMIO_LOGERROR((0, "DummyOutputMIO::connect Session already exists. Can only support one session"));
        return PVMFErrAlreadyExists;
    }

    iObserver = aObserver;

    return PVMFSuccess;
}


PVMFStatus DummyOutputMIO::disconnect(PvmiMIOSession aSession)
{
    OSCL_UNUSED_ARG(aSession);

    // Currently supports only one session
    if (iObserver == NULL)
    {
        LIPSYNCDUMMYOUTPUTMIO_LOGERROR((0, "DummyOutputMIO::disconnect No session to disconnect"));
        return PVMFErrArgument;
    }
    iObserver = NULL;
    return PVMFSuccess;
}


PvmiMediaTransfer* DummyOutputMIO::createMediaTransfer(
    PvmiMIOSession& aSession,
    PvmiKvp* read_formats, int32 read_flags,
    PvmiKvp* write_formats, int32 write_flags)
{
    return (PvmiMediaTransfer*)this;
}


void DummyOutputMIO::QueueCommandResponse(CommandResponse& aResp)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::QueueCommandResponse(), aResp.CmdId is %d ", aResp.iCmdId));
    //queue a command response and schedule processing.
    iCommandResponseQueue.push_back(aResp);

    //schedule the AO.
    if (!IsBusy())
        PendForExec();
    if (IsAdded() && iStatus == OSCL_REQUEST_PENDING)
        PendComplete(OSCL_REQUEST_ERR_NONE);
}

PVMFCommandId DummyOutputMIO::QueryInterface(const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG((0, "DummyOutputMIO::QueryInterface()"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = PVMFSuccess;
    }
    else
    {
        status = PVMFFailure;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}


bool DummyOutputMIO::queryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr)
{
    aInterfacePtr = NULL;
    bool status = false;
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = true;
    }
    else
    {
        status = false;
    }

    return status;
}



void DummyOutputMIO::deleteMediaTransfer(
    PvmiMIOSession& aSession,
    PvmiMediaTransfer* media_transfer)
{
    // This class is implementing the media transfer so no cleanup is needed.
}


PVMFCommandId DummyOutputMIO::Init(const OsclAny* aContext)
{
    LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG((0, "DummyOutputMIO::Init()"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status;

    switch (iState)
    {
        case STATE_LOGGED_ON:
            //do nothing--this component initializes the device when all necessary
            //parameters are received in setParametersSync.
            status = PVMFSuccess;
            iState = STATE_INITIALIZED;
            break;

        default:
            status = PVMFErrInvalidState;
            break;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);

    return cmdid;
}


PVMFCommandId DummyOutputMIO::Start(const OsclAny* aContext)
{
    LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG((0, "LIPSYNCDUMMYOUTPUTMIO::Start()"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status;

    switch (iState)
    {
        case STATE_INITIALIZED:
            iState = STATE_STARTED;
            status = PVMFSuccess;
            break;

        case STATE_PAUSED:
            iState = STATE_STARTED;
            status = PVMFSuccess;
            break;

        case STATE_STARTED:
            status = PVMFSuccess;
            break;

        default:
            status = PVMFErrInvalidState;
            break;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId DummyOutputMIO::Pause(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::Pause() "));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status;

    switch (iState)
    {
        case STATE_INITIALIZED:
        case STATE_PAUSED:
        case STATE_STARTED:
        {
            //pause the device.
            iState = STATE_PAUSED;
            status = PVMFSuccess;
            break;
        }
        break;
        default:
            status = PVMFErrInvalidState;
            break;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId DummyOutputMIO::Flush(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::Flush() "));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status;

    switch (iState)
    {
        case STATE_STARTED:
            if (!iCommandPendingQueue.empty())
            {
                //cannot support multiple simultaneous flush commands.
                status = PVMFFailure;
            }
            else
            {
                //no data queued-- command succeeds now.
                iState = STATE_INITIALIZED;
                status = PVMFSuccess;
            }
            break;

        default:
            status = PVMFErrInvalidState;
            break;
    }

    CommandResponse resp(status, cmdid, aContext);
    if (status == PVMFPending)
        iCommandPendingQueue.push_back(resp);
    else
        QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId DummyOutputMIO::DiscardData(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::DiscardData() "));

    PVMFCommandId cmdid = iCommandCounter++;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::DiscardData, CmdId is %d ", cmdid));

    PVMFStatus status;
    status = PVMFSuccess;

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId DummyOutputMIO::DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::DiscardData() with TS"));
    return DiscardData(aContext);
}

PVMFCommandId DummyOutputMIO::Stop(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::Stop() "));


    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status;

    switch (iState)
    {
        case STATE_STARTED:
        case STATE_PAUSED:
        {
            iState = STATE_INITIALIZED;
            status = PVMFSuccess;
        }
        break;
        default:
            status = PVMFErrInvalidState;
            break;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);

    return cmdid;
}

PVMFCommandId DummyOutputMIO::CancelAllCommands(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::CancelAllCommands() "));

    PVMFCommandId cmdid = iCommandCounter++;

    //cancel any pending command.
    if (!iCommandPendingQueue.empty())
    {
        //cancel it.
        CommandResponse resp(PVMFErrCancelled, iCommandPendingQueue[0].iCmdId, iCommandPendingQueue[0].iContext);
        QueueCommandResponse(resp);
        iCommandPendingQueue.erase(&iCommandPendingQueue[0]);
    }

    CommandResponse resp(PVMFSuccess, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId DummyOutputMIO::CancelCommand(PVMFCommandId aCmdId, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::CancelCommand() "));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;

    //see if the command is pending
    if (!iCommandPendingQueue.empty()
            && iCommandPendingQueue[0].iCmdId == aCmdId)
    {
        //cancel it.
        CommandResponse resp(PVMFErrCancelled, aCmdId, iCommandPendingQueue[0].iContext);
        QueueCommandResponse(resp);
        iCommandPendingQueue.erase(&iCommandPendingQueue[0]);
        status = PVMFSuccess;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

void DummyOutputMIO::ThreadLogon()
{
    if (iState == STATE_IDLE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "DummyOutputMIO::ThreadLogon() "));
        AddToScheduler();
        iState = STATE_LOGGED_ON;
        iIsCompressed = iSettings.iMediaFormat.isCompressed();
        iMIOObserver = iSettings.iDummyMIOObserver;
    }
}

void DummyOutputMIO::ThreadLogoff()
{
    if (iState != STATE_IDLE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "DummyOutputMIO::ThreadLogoff() "));
        iLogger = NULL;
        RemoveFromScheduler();
        iState = STATE_IDLE;
        //Reset any data from this session.
        ResetData();
    }
}

void DummyOutputMIO::Run()
{

    //send command responses
    while (!iCommandResponseQueue.empty())
    {
        if (iObserver)
        {
            iObserver->RequestCompleted(
                PVMFCmdResp(
                    iCommandResponseQueue[0].iCmdId,
                    iCommandResponseQueue[0].iContext,
                    iCommandResponseQueue[0].iStatus));
        }
        iCommandResponseQueue.erase(&iCommandResponseQueue[0]);
    }

    //check for flush completion.
    if (!iCommandPendingQueue.empty())
    {
        CommandResponse resp(
            PVMFSuccess,
            iCommandPendingQueue[0].iCmdId,
            iCommandPendingQueue[0].iContext);
        QueueCommandResponse(resp);
        iCommandPendingQueue.erase(&iCommandPendingQueue[0]);
    }
}



void DummyOutputMIO::useMemoryAllocators(OsclMemAllocator* write_alloc)
{
    //not supported-- ignore
}


void DummyOutputMIO::DealWithData(PVMFTimestamp aTimestamp, uint8* aData, uint32 aDataLen)
{
    if (iMIOObserver)
    {
        if (iIsAudioMIO)
        {
            iMIOObserver->MIOFramesUpdate(true, aDataLen, 0);
        }
        else if (iIsVideoMIO)
        {
            iMIOObserver->MIOFramesUpdate(false, aDataLen, 0);
        }
        else
        {
            // temp

            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_INFO,
                            (0, "DummyOutputMIO::DealWithData - ERROR!!! Neither AUDIO NOR VIDEO"));
        }
    }
}

PVMFCommandId DummyOutputMIO::writeAsync(
    uint8 aFormatType, int32 aFormatIndex,
    uint8* aData, uint32 aDataLen,
    const PvmiMediaXferHeader& data_header_info,
    OsclAny* aContext)
{

    // Do a leave if MIO is not configured except when it is an EOS
    if (!iIsMIOConfigured
            &&
            !((PVMI_MEDIAXFER_FMT_TYPE_NOTIFICATION == aFormatType)
              && (PVMI_MEDIAXFER_FMT_INDEX_END_OF_STREAM == aFormatIndex)))
    {
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    PVMFTimestamp aTimestamp = data_header_info.timestamp;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "DummyOutputMIO::writeAsync() seqnum %d ts %d context %d", data_header_info.seq_num, aTimestamp, aContext));

    PVMFCommandId cmdid = iCommandCounter++;

    if (IsAdded() && !IsBusy())
        PendForExec();

    DealWithData(aTimestamp, aData, aDataLen);

    if (iPeer)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_INFO,
                        (0, "RefAudioIO::writeAsync() - writeComplete - status %d ts %d",
                         PVMFSuccess,
                         aTimestamp));

        iPeer->writeComplete(PVMFSuccess, cmdid, aContext);
    }

    return cmdid;
}



void DummyOutputMIO::writeComplete(PVMFStatus aStatus,
                                   PVMFCommandId write_cmd_id,
                                   OsclAny* aContext)
{
    //won't be called since this component is a sink.
}



PVMFCommandId DummyOutputMIO::readAsync(
    uint8* data, uint32 max_data_len,
    OsclAny* aContext,
    int32* formats, uint16 num_formats)
{
    //read not supported.
    OsclError::Leave(OsclErrNotSupported);
    return -1;
}


void DummyOutputMIO::readComplete(
    PVMFStatus aStatus,
    PVMFCommandId read_cmd_id,
    int32 format_index,
    const PvmiMediaXferHeader& data_header_info,
    OsclAny* aContext)
{
    //won't be called since this component is a sink.
}

void DummyOutputMIO::statusUpdate(uint32 status_flags)
{
    //resume data transfer
    if (iPeer)
        iPeer->statusUpdate(PVMI_MEDIAXFER_STATUS_WRITE);
    RunIfNotReady();
    //won't be called since this component is a sink.
}


void DummyOutputMIO::cancelCommand(PVMFCommandId aCmdId)
{
    //the purpose of this API is to cancel a writeAsync command and report
    //completion ASAP.
}

// lower-case cancel all commands for the node's ports only. not the engine commands
void DummyOutputMIO::cancelAllCommands()
{
}


void DummyOutputMIO::setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
{
    OSCL_UNUSED_ARG(aObserver);
    //not needed since this component only supports synchronous capability & config
    //APIs.
}


PVMFStatus DummyOutputMIO::releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements)
{
    OSCL_UNUSED_ARG(aSession);

    //release parameters that were allocated by this component.
    if (aParameters)
    {
        for (int i = 0; i < num_elements; i++)
        {
            oscl_free(aParameters[i].key);
        }
        oscl_free(aParameters);
        return PVMFSuccess;
    }
    return PVMFFailure;
}

void DummyOutputMIO::createContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OsclError::Leave(OsclErrNotSupported);
}

void DummyOutputMIO::setContextParameters(PvmiMIOSession session, PvmiCapabilityContext& context,
        PvmiKvp* parameters, int num_parameter_elements)
{
    OsclError::Leave(OsclErrNotSupported);
}


void DummyOutputMIO::DeleteContext(PvmiMIOSession session,
                                   PvmiCapabilityContext& context)
{
    OsclError::Leave(OsclErrNotSupported);
}


void DummyOutputMIO::setParametersSync(PvmiMIOSession aSession,
                                       PvmiKvp* aParameters,
                                       int num_elements,
                                       PvmiKvp*& aRet_kvp)
{
    OSCL_UNUSED_ARG(aSession);

    aRet_kvp = NULL;

    for (int32 i = 0; i < num_elements; i++)
    {
        if (pv_mime_strcmp(aParameters[i].key, MOUT_AUDIO_FORMAT_KEY) == 0)
        {
            //Do nothing since this is a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM) == 0)
        {
            //Do nothing since this a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
        {
            //We simply check it to make sure that an error isn't returned for this key.
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_FORMAT_KEY) == 0)
        {
            //Do nothing since this is a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV) == 0)
        {
            //Do nothing since this a dummy
        }
        else
        {
            //if we get here the key is unrecognized.

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVRefFileOutput::setParametersSync() Error, unrecognized key "));

            //set the return value to indicate the unrecognized key
            //and return.
            aRet_kvp = &aParameters[i];
        }
    }

    if (!iIsMIOConfigured)
    {
        iIsMIOConfigured = true;

        //MIO is configured. Send PVMFMIOConfigurationComplete event to observer.
        if (iObserver)
        {
            iObserver->ReportInfoEvent(PVMFMIOConfigurationComplete);
        }
    }
}


PVMFCommandId DummyOutputMIO::setParametersAsync(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements,
        PvmiKvp*& ret_kvp,
        OsclAny* context)
{
    OsclError::Leave(OsclErrNotSupported);
    return -1;
}


uint32 DummyOutputMIO::getCapabilityMetric(PvmiMIOSession session)
{
    return 0;
}


PVMFStatus DummyOutputMIO::verifyParametersSync(PvmiMIOSession session,
        PvmiKvp* parameters, int num_elements)
{
    return PVMFSuccess; // no restrictions on playback settings always return OK
}
////////////////////////////////////////////////////////////////////////////
void DummyOutputMIO::setPeer(PvmiMediaTransfer* aPeer)
{
    iPeer = aPeer;
}


PVMFStatus DummyOutputMIO::getParametersSync(PvmiMIOSession aSession,
        PvmiKeyType aIdentifier,
        PvmiKvp*& aParameters,
        int& num_parameter_elements,
        PvmiCapabilityContext   aContext)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);
    aParameters = NULL;
    num_parameter_elements = 0;

    if (pv_mime_strcmp(aIdentifier, INPUT_FORMATS_CAP_QUERY) == 0)
    {

        //This is a query for the supported formats.

        if ((iIsAudioMIO) && (!iIsCompressed))
        {
            int32 i = 0;
            int32 count = 2;
            aParameters = (PvmiKvp*)oscl_malloc(count * sizeof(PvmiKvp));
            if (aParameters)
            {
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_PCM8;
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_PCM16;
            }
            while (i < count)
            {
                aParameters[i].key = (PvmiKeyType)oscl_malloc(oscl_strlen(MOUT_AUDIO_FORMAT_KEY) + 1);
                if (!aParameters[i].key)
                {
                    return PVMFErrNoMemory;
                    // (hope it's safe to leave array partially
                    //  allocated, caller will free?)
                }
                oscl_strncpy(aParameters[i++].key, MOUT_AUDIO_FORMAT_KEY, oscl_strlen(MOUT_AUDIO_FORMAT_KEY) + 1);
            }

        }
        else if ((iIsAudioMIO) && (iIsCompressed))
        {
            int32 i = 0;
            int32 count = 3;
            aParameters = (PvmiKvp*)oscl_malloc(count * sizeof(PvmiKvp));
            if (aParameters)
            {
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_AMR_IF2;
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_AMR_IETF;
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_G723;
            }
            while (i < count)
            {
                aParameters[i].key = (PvmiKeyType)oscl_malloc(oscl_strlen(MOUT_AUDIO_FORMAT_KEY) + 1);
                if (!aParameters[i].key)
                {
                    return PVMFErrNoMemory;
                    // (hope it's safe to leave array partially
                    //  allocated, caller will free?)
                }
                oscl_strncpy(aParameters[i++].key, MOUT_AUDIO_FORMAT_KEY, oscl_strlen(MOUT_AUDIO_FORMAT_KEY) + 1);
            }
        }
        else if ((iIsVideoMIO) && (iIsCompressed))
        {
            int32 i = 0;
            int32 count = 3;
            aParameters = (PvmiKvp*)oscl_malloc(count * sizeof(PvmiKvp));
            if (aParameters)
            {
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_M4V;
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_H2631998;
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_H2632000;
                while (i < count)
                {
                    aParameters[i].key = (PvmiKeyType)oscl_malloc(oscl_strlen(MOUT_VIDEO_FORMAT_KEY) + 1);
                    if (!aParameters[i].key)
                    {
                        return PVMFErrNoMemory;
                        // (hope it's safe to leave array partially
                        //  allocated, caller will free?)
                    }
                    oscl_strncpy(aParameters[i++].key, MOUT_VIDEO_FORMAT_KEY, oscl_strlen(MOUT_VIDEO_FORMAT_KEY) + 1);
                }

            }
        }
        else if ((iIsVideoMIO) && (!iIsCompressed))
        {
            int32 i = 0;
            int32 count = 1;
            aParameters = (PvmiKvp*)oscl_malloc(count * sizeof(PvmiKvp));
            if (aParameters)
            {
                aParameters[num_parameter_elements++].value.pChar_value = (char*)PVMF_MIME_YUV420;
                while (i < count)
                {
                    aParameters[i].key = (PvmiKeyType)oscl_malloc(oscl_strlen(MOUT_VIDEO_FORMAT_KEY) + 1);
                    if (!aParameters[i].key)
                    {
                        return PVMFErrNoMemory;
                        // (hope it's safe to leave array partially
                        //  allocated, caller will free?)
                    }
                    oscl_strncpy(aParameters[i++].key, MOUT_VIDEO_FORMAT_KEY, oscl_strlen(MOUT_VIDEO_FORMAT_KEY) + 1);
                }

            }
        }
        return PVMFSuccess;
    }
    //unrecognized key.
    return PVMFFailure;

}



