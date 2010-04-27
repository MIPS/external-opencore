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
#ifndef PV_METADATA_ENGINE_H_INCLUDED
#include "pv_metadata_engine.h"
#endif

#ifndef PVLOGGER_CFG_FILE_PARSER_H_INCLUDED
#include "pvlogger_cfg_file_parser.h"
#endif


PVMetadataEngine* PVMetadataEngine::New(PVMetadataEngineInterfaceContainer& aPVMEContainer)
{
    if (aPVMEContainer.iCmdStatusObserver == NULL ||
            aPVMEContainer.iErrorEventObserver == NULL ||
            aPVMEContainer.iInfoEventObserver == NULL)
    {
        return NULL;
    }

    PVMetadataEngine* util = NULL;
    util = OSCL_NEW(PVMetadataEngine, ());
    if (util)
    {
        util->Construct(aPVMEContainer);
    }

    return util;
}


PVMetadataEngine::~PVMetadataEngine()
{
    aContainer = NULL;

}

PVMetadataEngine::PVMetadataEngine() :
        OsclTimerObject(OsclActiveObject::EPriorityNominal, "PVMetadataEngine"),
        iCommandId(0),
        iState(PVME_INTERNAL_STATE_IDLE),
        iCommandCompleteStatusInErrorHandling(PVMFSuccess),
        iCommandCompleteErrMsgInErrorHandling(NULL),
        iCmdStatusObserver(NULL),
        iErrorEventObserver(NULL),
        iInfoEventObserver(NULL),
        iLogger(NULL),
        iPerfLogger(NULL),
        iDataSource(NULL),
        iSourceFormatType(PVMF_MIME_FORMAT_UNKNOWN),
        iSourceNode(NULL),
        iSourceNodeSessionId(0),
        iSourceNodeInitIF(NULL),
        iSourceNodeMetadataExtIF(NULL),
        iSourceNodePVInterfaceInit(NULL),
        iSourceNodePVInterfaceMetadataExt(NULL),
        iPreviousSourceNode(NULL),
        iPreviousSourceNodeSessionId(0),
        iValueList(NULL)
{}


void PVMetadataEngine::Construct(PVMetadataEngineInterfaceContainer& aPVMEContainer)
{
    aContainer = &aPVMEContainer;

    iCmdStatusObserver  = aContainer->iCmdStatusObserver;
    iErrorEventObserver = aContainer->iErrorEventObserver;
    iInfoEventObserver  = aContainer->iInfoEventObserver;

    if (PV_METADATA_ENGINE_THREADED_MODE == aContainer->iMode)
    {
        OsclRefCounter* pRC = 0;
        OsclSharedPtr<PVLoggerAppender> appenderPtr;
        PVLoggerCfgFileParser::CreateLogAppender(aContainer->iAppenderType,
                aContainer->iLogfilename.get_str(), pRC, appenderPtr);

        Oscl_Vector<PVLoggerCfgFileParser::LogCfgElement, OsclMemAllocator>::iterator iter;
        for (iter = aContainer->iVectorLogNodeCfg.begin(); iter != aContainer->iVectorLogNodeCfg.end(); ++iter)
        {
            PVLogger* node = PVLogger::GetLoggerObject(iter->m_strNodeName.get_cstr());
            node->AddAppender(appenderPtr);
            node->SetLogLevel(iter->m_logLevel);
        }
    }

    iCommandIdMut.Create();
    iOOTSyncCommandSem.Create();
    iThreadSafeQueue.Configure(this);

    // allocate memory for vectors, if a leave occurs, let it bubble up
    iCurrentCmd.reserve(1);
    iPendingCmds.reserve(4);

    AddToScheduler(); // Add this AO to the scheduler

    // retrieve the logger objects
    iLogger = PVLogger::GetLoggerObject("PVMetadataEngine");
    iPerfLogger = PVLogger::GetLoggerObject("pvmediagnostics");

    PVMERegistryPopulator::Populate(iPVMENodeRegistry, iPVMERecognizerRegistry);
    SetPVMEState(PVME_INTERNAL_STATE_IDLE);
}



PVMFStatus PVMetadataEngine::GetPVMEStateSync(PVMetadataEngineState& aState)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::GetPVMEStateSync()"));
    if (iThreadSafeQueue.IsInThread())
    {
        aState = GetPVMEState();
        return PVMFSuccess;
    }
    else
    {
        Oscl_Vector<PVMECommandParamUnion, OsclMemAllocator> paramvec;
        paramvec.reserve(1);
        paramvec.clear();
        PVMECommandParamUnion param;
        param.pOsclAny_value = (OsclAny*) & aState;
        paramvec.push_back(param);
        return DoOOTSyncCommand(PVME_COMMAND_GET_PVME_STATE_OOTSYNC, &paramvec);
    }
}


PVCommandId PVMetadataEngine::Init(const OsclAny* aContextData)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::Init()"));
    return AddCommandToQueue(PVME_COMMAND_INIT, (OsclAny*)aContextData);
}

PVMFStatus PVMetadataEngine::SetMetaDataKeys(PVPMetadataList& aKeyList)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "PVMetadataEngine::SetMetaDataKeys Tick=%d", OsclTickCount::TickCount()));

    if (aKeyList.size() == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::SetMetaDataKeys() - Key list empty."));
        return PVMFErrArgument;
    }
    if (iThreadSafeQueue.IsInThread())
    {
        //Delete in case there is anything in the key list
        iRequestedMetadataKeyList.clear();
        for (uint32 reqKeys = 0; reqKeys < aKeyList.size(); reqKeys++)
        {
            iRequestedMetadataKeyList.push_back(aKeyList[reqKeys].get_cstr());
        }
    }
    else
    {
        Oscl_Vector<PVMECommandParamUnion, OsclMemAllocator> paramvec;
        paramvec.reserve(1);
        paramvec.clear();
        PVMECommandParamUnion param;
        param.pOsclAny_value = (OsclAny*) & aKeyList;
        paramvec.push_back(param);
        return DoOOTSyncCommand(PVME_COMMAND_SET_METADATAKEYS_OOTSYNC, &paramvec);
    }
    return PVMFSuccess;
}

PVCommandId PVMetadataEngine::GetMetadata(PVPlayerDataSource& aDataSource,
        Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
        const OsclAny* aContextData)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "PVMetadataEngine::GetMetadata Tick=%d", OsclTickCount::TickCount()));


    Oscl_Vector<PVMECommandParamUnion, OsclMemAllocator> paramvec;
    paramvec.reserve(5);
    paramvec.clear();
    PVMECommandParamUnion param;
    param.pOsclAny_value = (OsclAny*) & aDataSource;
    paramvec.push_back(param);
    param.pOsclAny_value = (OsclAny*) & aValueList;
    paramvec.push_back(param);
    return AddCommandToQueue(PVME_COMMAND_GET_METADATA, (OsclAny*)aContextData, &paramvec);
}


PVCommandId PVMetadataEngine::Reset(const OsclAny* aContextData)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::Reset()"));

    return AddCommandToQueue(PVME_COMMAND_RESET, (OsclAny*)aContextData);
}

void PVMetadataEngine::CleanUp()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::CleanUp()"));
    if (aContainer->iMode == PV_METADATA_ENGINE_NON_THREADED_MODE)
    {
        DoCleanUp();
        OSCL_DELETE(this);
    }
    else
    {
        AddCommandToQueue(PVME_COMMAND_CLEANUP);
    }
}

void PVMetadataEngine::PVMEThread(PVMetadataEngineInterfaceContainer* aPVMEContainer)
{
    OsclMem::Init();
    PVLogger::Init();

    // Construct and install the active scheduler
    OsclScheduler::Init("PVMEScheduler");

    PVMetadataEngine* enginePtr = New(*aPVMEContainer);
    aPVMEContainer->iPVMEInterface = enginePtr;

    aPVMEContainer->iSem.Signal();
    OsclExecScheduler* sched = OsclExecScheduler::Current();
    if (sched)
    {
        sched->StartScheduler();
    }

    //StartScheduler is a blocking call
    //Only way the control could get here is if "StopScheduler" was called in DoCleanUp
    //which inturn would have been called in response to CleanUp issued by DeletePVMetadataEngine
    OSCL_DELETE(enginePtr);

    OsclScheduler::Cleanup();
    PVLogger::Cleanup();
    OsclMem::Cleanup();

    //signal the semaphore - this guarantees that DeletePVMetadataEngine returns only after
    //this thread has been fully cleaned up
    aPVMEContainer->iSem.Signal();
}

void PVMetadataEngine::Run()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::Run() In"));
    int32 leavecode = 0;

    if (iState == PVME_INTERNAL_STATE_RESETTING)
    {
        //this means error handling, reset is still in progress
        //pls note that the state will be set to idle
        //in HandleSourceNodeReset
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMetadataEngine::Run() Return PVME in resetting state, No processing until pvme is in Idle state"));
        return;
    }


    /* Check if ErrorHandling request was made */
    if (!iPendingCmds.empty())
    {
        switch (iPendingCmds.top().GetCmdType())
        {
            case PVME_COMMAND_ERROR_HANDLING:
            {
                // go in error handling right away
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::Run() Processing Error Handling request"));
                PVMFStatus retVal = DoErrorHandling();
                if (retVal == PVMFSuccess)
                {
                    iPendingCmds.pop();
                    RunIfNotReady(); // schedule the pvme AO to process other commands in queue if any.
                }
                return;
            }

            default:
                break;
        }
    }


    // if current command being processed is reset and
    // pvme state is idle then delete the source node
    // and do reset command complete
    if (!iCurrentCmd.empty())
    {
        if ((iCurrentCmd[0].GetCmdType() == PVME_COMMAND_RESET))

        {
            if (iState != PVME_INTERNAL_STATE_IDLE)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::Run() PVME not in Idle State, asserting"));
                OSCL_ASSERT(false);
            }
            // now remove the source node.
            if (iSourceNode)
            {
                ReleaseSourceNodeInterfaces();
                ReleaseSourceNode(iSourceNodeUuid, iSourceNodeSessionId, iSourceNode);
            }

            PVMECommandCompleted(iCurrentCmd[0].GetCmdId(), iCurrentCmd[0].GetContext(), PVMFSuccess);
        }

    }


    // Handle other requests normally
    if (!iPendingCmds.empty() && iCurrentCmd.empty())
    {
        // Retrieve the first pending command from queue
        PVMECommand cmd(iPendingCmds.top());
        iPendingCmds.pop();

        // Put in on the current command queue
        leavecode = 0;
        OSCL_TRY(leavecode, iCurrentCmd.push_front(cmd));
        OSCL_FIRST_CATCH_ANY(leavecode,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::Run() Command could not be pushed onto iCurrentCmd vector"));
                             PVMECommandCompleted(cmd.GetCmdId(), cmd.GetContext(), PVMFErrNoMemory);
                             OSCL_ASSERT(false);
                             return;);

        // Process the command according to the cmd type
        PVMFStatus cmdstatus = PVMFSuccess;
        bool ootsync = false;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::Run() Processing command with type=%d", cmd.GetCmdType()));
        switch (cmd.GetCmdType())
        {
            case PVME_COMMAND_INIT:
                cmdstatus = DoInit(cmd);
                break;

            case PVME_COMMAND_GET_METADATA:
                cmdstatus = DoGetMetadata(cmd);
                break;

            case PVME_COMMAND_RESET:
                cmdstatus = DoReset(cmd);
                break;

            case PVME_COMMAND_CLEANUP:
                cmdstatus = DoCleanUp();
                break;

            case PVME_COMMAND_SET_METADATAKEYS_OOTSYNC:
                ootsync = true;
                cmdstatus = DoSetMetadatakeys(cmd);
                break;

            case PVME_COMMAND_GET_PVME_STATE_OOTSYNC:
                ootsync = true;
                cmdstatus = DoGetPVMEState(cmd);
                break;

            default:
                cmdstatus = PVMFErrNotSupported;
                break;

        }
        if (ootsync)
        {
            OOTSyncCommandComplete(cmd, cmdstatus);
            // Empty out the current cmd vector and set active if there are other pending commands
            iCurrentCmd.erase(iCurrentCmd.begin());
            if (!iPendingCmds.empty())
            {
                RunIfNotReady();
            }
        }
        else if (cmdstatus != PVMFSuccess && cmdstatus != PVMFPending)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::Run() Command failed CmdId %d Status %d",
                            cmd.GetCmdId(), cmdstatus));
            PVMECommandCompleted(cmd.GetCmdId(), cmd.GetContext(), cmdstatus);
        }
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::Run() Out"));
}

//A callback from the threadsafe queue
void PVMetadataEngine::ThreadSafeQueueDataAvailable(ThreadSafeQueue* aQueue)
{
    OSCL_UNUSED_ARG(aQueue);

    //pull all available data off the thread-safe queue and transfer
    //it to the internal queue.
    for (uint32 ndata = 1; ndata;)
    {
        ThreadSafeQueueId id;
        OsclAny* data;
        ndata = iThreadSafeQueue.DeQueue(id, data);
        if (ndata)
        {
            PVMECommand* cmd = (PVMECommand*)data;
            AddCommandToQueue(cmd->iCmdType
                              , cmd->iContextData
                              , &cmd->iParamVector
                              , &cmd->iUuid
                              , true//assume all out-of-thread data is an API command.
                              , (PVCommandId*)&id);//use the command ID that was returned to the caller.
            OSCL_DELETE(cmd);
        }
    }
}

PVMFStatus PVMetadataEngine::DoOOTSyncCommand(int32 aCmdType,
        Oscl_Vector<PVMECommandParamUnion, OsclMemAllocator>* aParamVector,
        const PVUuid* aUuid)
{
    //Called from out-of-thread to perform a synchronous command


    //Add a PVMFStatus* to the end of the command param vec to hold the result.
    PVMFStatus status;
    PVMECommandParamUnion param;
    param.pOsclAny_value = (OsclAny*) & status;
    aParamVector->push_back(param);

    //push the command across the thread boundary
    PVCommandId id = 0;
    PVMECommand* cmd = OSCL_NEW(PVMECommand, (aCmdType, id, NULL, aParamVector));
    if (aUuid)
        cmd->SetUuid(*aUuid);
    iThreadSafeQueue.AddToQueue(cmd);

    //block and wait for completion by PVME thread.
    iOOTSyncCommandSem.Wait();
    return status;
}

void PVMetadataEngine::OOTSyncCommandComplete(PVMECommand& aCmd, PVMFStatus aStatus)
{
    //Called in PVME thread to complete an out-of-thread synchronous command

    //Put the result status into the last element of the command param vector.
    PVMFStatus* status = (PVMFStatus*)(aCmd.GetParam(aCmd.iParamVector.size() - 1).pOsclAny_value);
    OSCL_ASSERT(status);
    *status = aStatus;

    //Signal the calling thread.
    iOOTSyncCommandSem.Signal();
}



PVCommandId PVMetadataEngine::AddCommandToQueue(int32 aCmdType, OsclAny* aContextData, Oscl_Vector < PVMECommandParamUnion,
        OsclMemAllocator > * aParamVector, const PVUuid* aUuid, bool aAPICommand, PVCommandId* aId)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::AddCommandToQueue() In CmdType %d, CmdId %d", aCmdType, iCommandId));

    PVCommandId commandId;
    if (aId)
    {
        //This command is being transferred from the thread-safe queue to the
        //internal queue, in pvme thread context.
        //The input ID is the one that was returned to the
        //caller, so use that ID instead of generating a new one.
        commandId = *aId;
    }
    else
    {
        //Generate the next command ID, being careful to avoid thread contention
        //for "iCommandId".
        iCommandIdMut.Lock();
        commandId = iCommandId;
        ++iCommandId;
        if (iCommandId == 0x7FFFFFFF)
        {
            iCommandId = 0;
        }
        iCommandIdMut.Unlock();

        //If this is from outside pvme thread context, then push the command across the
        //thread boundary.
        if (!iThreadSafeQueue.IsInThread())
        {
            PVMECommand* cmd = OSCL_NEW(PVMECommand, (aCmdType, commandId, aContextData, aParamVector, aAPICommand));
            if (aUuid)
                cmd->SetUuid(*aUuid);
            iThreadSafeQueue.AddToQueue(cmd, (ThreadSafeQueueId*)&commandId);
            return commandId;
        }
    }

    PVMECommand cmd(aCmdType, commandId, aContextData, aParamVector, aAPICommand);
    if (aUuid)
    {
        cmd.SetUuid(*aUuid);
    }

    int32 leavecode = 0;
    OSCL_TRY(leavecode, iPendingCmds.push(cmd));
    OSCL_FIRST_CATCH_ANY(leavecode,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::AddCommandToQueue() Adding command to pending command list did a leave!"));
                         OSCL_ASSERT(false);
                         return -1;);

    if (aCmdType == PVME_COMMAND_ERROR_HANDLING)
    {
        SetPVMEState(PVME_INTERNAL_STATE_ERROR);
        SendInformationalEvent(PVMFInfoErrorHandlingStart, NULL);
    }

    RunIfNotReady();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "PVMetadataEngine::AddCommandToQueue() Type=%d ID=%d APIcmd=%d Tick=%d",
                     aCmdType, cmd.GetCmdId(), aAPICommand, OsclTickCount::TickCount()));


    return cmd.GetCmdId();
}

void PVMetadataEngine::PVMECommandCompleted(PVCommandId aId, OsclAny* aContext, PVMFStatus aStatus,
        PVInterface* aExtInterface, OsclAny* aEventData, int32 aEventDataSize)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::PVMECommandCompleted() In CmdId %d, Status %d", aId, aStatus));

    // Assert if the current cmd is not saved or the cmd ID does not match
    OSCL_ASSERT(iCurrentCmd.size() == 1);
    OSCL_ASSERT(iCurrentCmd[0].GetCmdId() == aId);

    // Empty out the current cmd vector and set active if there are other pending commands
    PVMECommand completedcmd(iCurrentCmd[0]);
    iCurrentCmd.erase(iCurrentCmd.begin());
    if (!iPendingCmds.empty())
    {
        RunIfNotReady();
    }

    // Send the command completed event
    if (iCmdStatusObserver)
    {
        if (aId != -1 && completedcmd.IsAPICommand())
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::PVMECommandCompleted() Notifying PVME command as completed. CmdId %d Status %d", aId, aStatus));
            PVCmdResponse cmdcompleted(aId, aContext, aStatus, aExtInterface, aEventData, aEventDataSize);
            iCmdStatusObserver->CommandCompleted(cmdcompleted);
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::PVMECommandCompleted() aId is -1 or not an API command. CmdType %d", completedcmd.GetCmdType()));
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::PVMECommandCompleted() iCmdStatusObserver is NULL"));
    }
}

void PVMetadataEngine::SetPVMEState(PVMEState aState)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::SetPVMEState() In Current state %d, New state %d", iState, aState));
    iState = aState;
}

PVMFStatus PVMetadataEngine::DoErrorHandling()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoErrorHandling() In"));

    // move on to resetting Source Node
    if (iSourceNode)
    {
        int32 leavecode = 0;
        // PVME transists the source node only to following states
        // 1) Created
        // 2) Idle
        // 3) Initialized
        // Reset should be called only when in Initialized state
        if (iSourceNode->GetState() == EPVMFNodeInitialized)
        {
            if (iValueList != NULL)
            {
                iSourceNodeMetadataExtIF->ReleaseNodeMetadataValues(*iValueList,
                        0,
                        (iValueList->size() - 1));

                iValueList->clear();
                iValueList = NULL;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMetadataEngine::DoErrorHandling() Issue reset on Source Node"));
            // error handling code set engine state to resetting
            SetPVMEState(PVME_INTERNAL_STATE_RESETTING);

            iPVMEContext.iCmdId = -1;
            iPVMEContext.iCmdContext = NULL;
            iPVMEContext.iCmdType = PVME_CMD_SourceNodeReset;

            PVMFCommandId cmdid = -1;
            leavecode = 0;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_NOTICE,
                            (0, "PVMetadataEngine::DoErrorHandling() Source Node Reset Called Tick=%d", OsclTickCount::TickCount()));

            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngine::Source Node Reset Called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

            OSCL_TRY(leavecode, cmdid = iSourceNode->Reset(iSourceNodeSessionId, (OsclAny*) & iPVMEContext));
            OSCL_FIRST_CATCH_ANY(leavecode,

                                 PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                                 (0, "PVMetadataEngine::DoErrorHandling() Reset on iSourceNode did a leave!"));
                                 OSCL_ASSERT(false);
                                 return PVMFFailure);

            return PVMFPending;
        }
    }

    // Set the value list to NULL
    if (iValueList != NULL && iValueList->empty())
    {
        iValueList = NULL;
    }

    // now remove the source node.
    if (iSourceNode)
    {
        ReleaseSourceNodeInterfaces();
        ReleaseSourceNode(iSourceNodeUuid, iSourceNodeSessionId, iSourceNode);
    }
    // remove previous source node if any
    if (iPreviousSourceNode)
    {
        ReleaseSourceNode(iPreviousSourceNodeUuid, iPreviousSourceNodeSessionId, iPreviousSourceNode);
    }

    SetPVMEState(PVME_INTERNAL_STATE_IDLE);

    // Send the command completion if there is any command in Current command
    if (!iCurrentCmd.empty())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMetadataEngine::PVMetadataEngine() Complete the pvme command being processed"));
        if (iCommandCompleteErrMsgInErrorHandling)
        {
            PVMECommandCompleted(iCurrentCmd[0].GetCmdId(),
                                 iCurrentCmd[0].GetContext(),
                                 iCommandCompleteStatusInErrorHandling,
                                 OSCL_STATIC_CAST(PVInterface*, iCommandCompleteErrMsgInErrorHandling));
            iCommandCompleteErrMsgInErrorHandling->removeRef();
            iCommandCompleteErrMsgInErrorHandling = NULL;
        }
        else
        {
            PVMECommandCompleted(iCurrentCmd[0].GetCmdId(),
                                 iCurrentCmd[0].GetContext(),
                                 iCommandCompleteStatusInErrorHandling);
        }
    }

    // just send the error handling complete event
    SendInformationalEvent(PVMFInfoErrorHandlingComplete, NULL);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoErrorHandling() Out"));

    return PVMFSuccess;
}


PVMFStatus PVMetadataEngine::DoInit(PVMECommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoInit() In"));

    if (iState != PVME_INTERNAL_STATE_IDLE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoInit() Invalid State"));
        return PVMFErrInvalidState;
    }

    SetPVMEState(PVME_INTERNAL_STATE_INITIALIZED);

    PVMECommandCompleted(aCmd.GetCmdId(), aCmd.GetContext(), PVMFSuccess);

    return PVMFSuccess;

}

PVMFStatus PVMetadataEngine::DoReset(PVMECommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoReset() In"));

    PVMFStatus status;
    if (iState == PVME_INTERNAL_STATE_IDLE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoReset() Already in Idle State"));
        status = PVMFSuccess;
        PVMECommandCompleted(aCmd.GetCmdId(), aCmd.GetContext(), PVMFSuccess);

    }

    if (iValueList != NULL)
    {
        iSourceNodeMetadataExtIF->ReleaseNodeMetadataValues(*iValueList,
                0,
                (iValueList->size() - 1));

        iValueList->clear();
        iValueList = NULL;
    }

    if (iSourceNode != NULL && iState != PVME_INTERNAL_STATE_IDLE)
    {
        iPVMEContext.iCmdId = -1;
        iPVMEContext.iCmdContext = NULL;
        iPVMEContext.iCmdType = PVME_CMD_SourceNodeReset;
        int32 leavecode = 0;

        SetPVMEState(PVME_INTERNAL_STATE_RESETTING);
        status = PVMFPending;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                        (0, "PVMetadataEngine::Source Node Reset Called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

        OSCL_TRY(leavecode, iSourceNode->Reset(iSourceNodeSessionId, (const OsclAny*)&iPVMEContext));
        OSCL_FIRST_CATCH_ANY(leavecode,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoReset() Reset() on source node did a leave!"));
                             OSCL_ASSERT(false);
                             status = PVMFFailure);


    }
    else
    {
        status = PVMFSuccess;
    }
    return status;
}

PVMFStatus PVMetadataEngine::DoCleanUp()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoCleanUp() In"));

    if (!iPendingCmds.empty())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoCleanUp() - Clearing Out Pending Cmds"));
        iPendingCmds.pop();
    }

    if (iSourceNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoCleanUp() - Cleaning up source node"));
        ReleaseSourceNodeInterfaces();
        ReleaseSourceNode(iSourceNodeUuid, iSourceNodeSessionId, iSourceNode);
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoCleanUp() - Calling Registry Depopulate"));
    PVMERegistryPopulator::Depopulate(iPVMENodeRegistry, iPVMERecognizerRegistry);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoCleanUp() - Closing Cmd Mutex and Semaphores"));
    iCommandIdMut.Close();
    iOOTSyncCommandSem.Close();

    if (aContainer->iMode == PV_METADATA_ENGINE_THREADED_MODE)
    {
        OsclExecScheduler* sched = OsclExecScheduler::Current();
        if (sched)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoCleanUp() - Stopping Scheduler"));
            sched->StopScheduler();
            sched = NULL;
        }

    }
    return PVMFSuccess;
}

PVMFStatus PVMetadataEngine::DoSetMetadatakeys(PVMECommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoSetMetadatakeys() In"));

    if (iState != PVME_INTERNAL_STATE_IDLE && iState != PVME_INTERNAL_STATE_INITIALIZED)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSetMetadatakeys() Invalid State"));
        return PVMFErrInvalidState;
    }

    //Delete in case there is anything in the key list
    iRequestedMetadataKeyList.clear();

    PVPMetadataList* keylistptr = NULL;

    keylistptr = (PVPMetadataList*)(aCmd.GetParam(0).pOsclAny_value);

    for (uint32 reqKeys = 0; reqKeys < keylistptr->size(); reqKeys++)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoSetMetadatakeys() - Key=%s",
                        (*keylistptr)[reqKeys].get_cstr()));
        iRequestedMetadataKeyList.push_back((*keylistptr)[reqKeys].get_cstr());
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoSetMetadatakeys() Out"));
    return PVMFSuccess;
}

PVMFStatus PVMetadataEngine::DoGetMetadata(PVMECommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetMetadata() Called"));

    if (iState != PVME_INTERNAL_STATE_INITIALIZED)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoGetMetadata() Invalid State"));
        return PVMFErrInvalidState;
    }

    if (aCmd.GetParam(0).pOsclAny_value == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoGetMetadata() Passed in parameter invalid."));
        return PVMFErrArgument;
    }

    if (iValueList != NULL)
    {
        iSourceNodeMetadataExtIF->ReleaseNodeMetadataValues(*iValueList,
                0,
                (iValueList->size() - 1));

        iValueList->clear();
        iValueList = NULL;
    }

    // Save the data source
    iDataSource = (PVPlayerDataSource*)(aCmd.GetParam(0).pOsclAny_value);

    if (iSourceNode != NULL)
    {
        //If there exists a source node, reset it first.
        //Sequence of events in this case is as follows:
        //1) We wait for source node reset to complete (check HandleSourceNodeReset)
        //2) If source node reset fails, we assert. Reset cannot fail.
        //3) Once source node reset completes, we check to see if the source can be reused
        //4) If yes, we reuse it, else we simply release all its extension interface(we cannot delete it
        //in HandleSourceNodeReset, since we are essentially in the node in terms of callstack) and save
        //the node ptr as "previous node"
        //5) If this request requires us to invoke recognizers, we do that in HandleSourceNodeReset
        //6) If no recognition is reqd, we instantiate the new source node and issue Init to new source node
        //in HandleSourceNodeReset of old source node.
        iPVMEContext.iCmdId = -1;
        iPVMEContext.iCmdContext = NULL;
        iPVMEContext.iCmdType = PVME_CMD_SourceNodeReset;
        int32 leavecode = 0;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetMetadata() - Issuing SourceNode Reset"));

        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                        (0, "PVMetadataEngine::DoGetMetadata(): Source Node Reset Called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

        OSCL_TRY(leavecode, iSourceNode->Reset(iSourceNodeSessionId, (const OsclAny*)&iPVMEContext));
        OSCL_FIRST_CATCH_ANY(leavecode,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoGetMetadata() Reset() on source node did a leave!"));
                             OSCL_ASSERT(false);
                             return PVMFFailure);

    }
    //we do not have a source, create one (after recognition if needed)
    else
    {
        // Check the source format and do a recognize if unknown
        PVMFStatus retval = PVMFSuccess;
        iSourceFormatType = iDataSource->GetDataSourceFormatType();

        if (iSourceFormatType == PVMF_MIME_FORMAT_UNKNOWN)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetMetadata() - Recognizers Needed - Calling DoQuerySourceFormatType"));
            retval = DoQuerySourceFormatType(aCmd.GetCmdId(), aCmd.GetContext());
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetMetadata() - Recognizers Not Needed - Calling DoSetupSourceNode"));
            // Start the source node creation and setup sequence
            retval = DoSetupSourceNode(aCmd.GetCmdId(), aCmd.GetContext());
            if (retval != PVMFSuccess)
            {
                //if an error occured in create / threadlogon / queryinterfacesync
                //there is not much we can do with that node, so fail the getmetadata cmd.
                //DoSetupSourceNode must have done necessary cleanups and PVME Run will
                //do command complete
                return retval;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetMetadata() - Recognizers Not Needed - Calling DoSourceNodeInit"));

            retval = DoSourceNodeInit(aCmd.GetCmdId(), aCmd.GetContext());

            if (retval != PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMetadataEngine:: DoGetMetadata() DoSourceNodeInit failed, Add EH Command"));
                iCommandCompleteStatusInErrorHandling = retval;
                iCommandCompleteErrMsgInErrorHandling = NULL;
                AddCommandToQueue(PVME_COMMAND_ERROR_HANDLING, NULL, NULL, NULL, false);
            }
        }
    }

    return PVMFPending;
}

PVMFStatus PVMetadataEngine::DoQuerySourceFormatType(PVCommandId aCmdId, OsclAny* aCmdContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::DoQuerySourceFormatType() ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoQuerySourceFormatType() In"));

    // Use the recognizer if the source format type is unknown
    iPVMEContext.iCmdId = aCmdId;
    iPVMEContext.iCmdContext = aCmdContext;
    iPVMEContext.iCmdType = PVME_CMD_QUERYSOURCEFORMATTYPE;

    PVMFStatus retval = PVMFSuccess;
    int32 leavecode = 0;
    OsclAny * opaqueData = iDataSource->GetDataSourceContextData();
    PVInterface* pvInterface = OSCL_STATIC_CAST(PVInterface*, opaqueData);
    PVInterface* SourceContextData = NULL;
    PVUuid SourceContextDataUuid(PVMF_SOURCE_CONTEXT_DATA_UUID);
    PVMFCPMPluginAccessInterfaceFactory * DataStreamDataFactory = NULL;

    if (pvInterface != NULL && pvInterface->queryInterface(SourceContextDataUuid, SourceContextData))
    {
        PVMFSourceContextData * aSourceContextData = OSCL_STATIC_CAST(PVMFSourceContextData*, SourceContextData);
        PVMFSourceContextDataCommon * aSourceContextDataCommon = aSourceContextData->CommonData();
        if (aSourceContextDataCommon)
        {
            DataStreamDataFactory = aSourceContextDataCommon->iRecognizerDataStreamFactory;
        }
    }

    if (DataStreamDataFactory)
    {
        OSCL_TRY(leavecode, retval = iPVMERecognizerRegistry.QueryFormatType(DataStreamDataFactory, *this, (OsclAny*) & iPVMEContext));
        OSCL_FIRST_CATCH_ANY(leavecode,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "PVMetadataEngine:: DoQuerySourceFormatType() QueryFormatType (with DSFactory) did a leave, err=%d", leavecode));
                             return PVMFErrNotSupported;
                            );
    }
    else
    {
        OSCL_TRY(leavecode, retval = iPVMERecognizerRegistry.QueryFormatType(iDataSource->GetDataSourceURL(), *this, (OsclAny*) & iPVMEContext));
        OSCL_FIRST_CATCH_ANY(leavecode,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "PVMetadataEngine:: DoQuerySourceFormatType() QueryFormatType(with SrcURL) did a leave, err=%d", leavecode));
                             return PVMFErrNotSupported;
                            );
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoQuerySourceFormatType() Out"));
    return retval;
}

PVMetadataEngineState PVMetadataEngine::GetPVMEState(void)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::GetPVMEState() State=%d", iState));
    switch (iState)
    {
        case PVME_INTERNAL_STATE_IDLE:
        case PVME_INTERNAL_STATE_INITIALIZING:
            return PVME_STATE_IDLE;

        case PVME_INTERNAL_STATE_INITIALIZED:
        case PVME_INTERNAL_STATE_RESETTING:
            return PVME_STATE_INITIALIZED;

        case PVME_INTERNAL_STATE_HANDLINGERROR:
        case PVME_INTERNAL_STATE_ERROR:
            return PVME_STATE_ERROR;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::GetPVMEState() Unknown PVME state. Asserting"));
            OSCL_ASSERT(false);
            break;
    }
    return PVME_STATE_ERROR;
}


void PVMetadataEngine::RecognizeCompleted(PVMFFormatType aSourceFormatType, OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::RecognizeCompleted() ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

    // Save the recognized source format
    iSourceFormatType = aSourceFormatType;

    // Free pvme context after saving the cmd id and context
    PVMEContext* reccontext = (PVMEContext*)(aContext);
    OSCL_ASSERT(reccontext != NULL);
    PVCommandId cmdid = reccontext->iCmdId;
    OsclAny* cmdcontext = reccontext->iCmdContext;

    /*In case previous source node is not null, and we have called DoQuerySourceFormatType
    from HandleSourceNodeReset, we will have to set it to NULL here before setting up new source node*/
    if (iSourceNode != NULL)
    {
        //Previous source node is not required, but don't release it here just free the interface pointers
        ReleaseSourceNodeInterfaces();
        //Store the values of previous source node so that the node can be released later on
        iPreviousSourceNode = iSourceNode;
        iPreviousSourceNodeUuid = iSourceNodeUuid;
        iPreviousSourceNodeSessionId = iSourceNodeSessionId;

        iSourceNode = NULL;
    }

    // Start the source node creation and setup sequence
    PVMFStatus retval = DoSetupSourceNode(cmdid, cmdcontext);

    if (retval != PVMFSuccess)
    {
        //if an error occured in create / threadlogon / queryinterfacesync
        //there is not much we can do with that node, so fail the getmetadata cmd.
        //DoSetupSourceNode must have done necessary cleanups for the newly instantiated node
        //however to keep things simple queue EH cmd
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMetadataEngine:: RecognizeCompleted() DoSetupSourceNode failed, Add EH Command"));
        iCommandCompleteStatusInErrorHandling = retval;
        iCommandCompleteErrMsgInErrorHandling = NULL;
        AddCommandToQueue(PVME_COMMAND_ERROR_HANDLING, NULL, NULL, NULL, false);
        return;
    }

    // Send the event to notify the user of the updated format type
    int32 len = iSourceFormatType.getMIMEStrLen();
    uint8* localbuffer = (uint8*)iSourceFormatType.getMIMEStrPtr();
    SendInformationalEvent(PVMFInfoSourceFormatUpdated, NULL, NULL, localbuffer, len + 1 /*including null-terminator*/);

    retval = DoSourceNodeInit(cmdid, cmdcontext);
    if (retval != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMetadataEngine:: RecognizeCompleted() DoSourceNodeInit failed, Add EH Command"));
        iCommandCompleteStatusInErrorHandling = retval;
        iCommandCompleteErrMsgInErrorHandling = NULL;
        AddCommandToQueue(PVME_COMMAND_ERROR_HANDLING, NULL, NULL, NULL, false);
    }
}


bool PVMetadataEngine::CheckForPendingErrorHandlingCmd()
{
    //if an error handling cmd had been queued previously
    //it must be in top, since error handling cmds have the
    //highest priority and pending cmds queue is a priority
    //queue
    bool retval = false;
    if (!iPendingCmds.empty())
    {
        switch (iPendingCmds.top().GetCmdType())
        {
            case PVME_COMMAND_ERROR_HANDLING:
                retval = true;
                break;

            default:
                break;
        }
    }
    return retval;
}

PVMFStatus PVMetadataEngine::DoSetupSourceNode(PVCommandId aCmdId, OsclAny* aCmdContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::DoSetupSourceNode() Start Tick=%d", OsclTickCount::TickCount()));

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoSetupSourceNode() In"));

    OSCL_ASSERT(iDataSource != NULL);

    if (iSourceNode == NULL)
    {
        PVMFFormatType outputformattype = PVMF_MIME_FORMAT_UNKNOWN ;
        Oscl_Vector<PVUuid, OsclMemAllocator> foundUuids;
        // Query the node registry
        if (iPVMENodeRegistry.QueryRegistry(iSourceFormatType, outputformattype, foundUuids) == PVMFSuccess)
        {
            if (foundUuids.empty())
            {
                // No matching node found
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMetadataEngine::DoSetupSourceNode() Query Regsitry successful, No matching source node found."));
                return PVMFErrNotSupported;
            }
            //if this leaves it means system is out of memory, so let the error bubble up
            iSourceNode = iPVMENodeRegistry.CreateNode(foundUuids[0]);
            iSourceNodeUuid = foundUuids[0];
        }
        else
        {
            // Registry query failed
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSetupSourceNode() Registry query for source node failed"));
            return PVMFErrNotSupported;
        }
    }

    PVMFStatus status = PVMFFailure;
    PVMFNodeSessionInfo nodesessioninfo(this, this, (OsclAny*)iSourceNode, this, (OsclAny*)iSourceNode);
    status = iSourceNode->ThreadLogon();
    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSetupSourceNode() ThreadLogon() on the source node failed"));
        goto cleanupsource;
    }

    //if connect did a leave simply let the error bubble up
    iSourceNodeSessionId = iSourceNode->Connect(nodesessioninfo);
    iSourceNodePVInterfaceInit = NULL;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::QueryInterfaceSyncInitInterface called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
    status = iSourceNode->QueryInterfaceSync(iSourceNodeSessionId, PVMF_DATA_SOURCE_INIT_INTERFACE_UUID, iSourceNodePVInterfaceInit);
    if (status == PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                        (0, "PVMetadataEngine::QueryInterfaceSyncInitInterface completed ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
        iSourceNodeInitIF = (PVMFDataSourceInitializationExtensionInterface*)iSourceNodePVInterfaceInit;
        iSourceNodePVInterfaceInit = NULL;
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSetupSourceNode() QueryInterfaceSync for DataSource Init Interface failed"));
        goto cleanupsource;
    }

    iSourceNodePVInterfaceMetadataExt = NULL;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::QueryInterfaceSyncMetaData called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
    status = iSourceNode->QueryInterfaceSync(iSourceNodeSessionId, KPVMFMetadataExtensionUuid, iSourceNodePVInterfaceMetadataExt);

    if (status == PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                        (0, "PVMetadataEngine::QueryInterfaceSyncMetaData completed ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

        iSourceNodeMetadataExtIF = (PVMFMetadataExtensionInterface*)iSourceNodePVInterfaceMetadataExt;
        iSourceNodePVInterfaceMetadataExt = NULL;
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSetupSourceNode() QueryInterfaceSync for Metadata Interface failed"));
        goto cleanupsource;
    }


cleanupsource:
    if (status != PVMFSuccess)
    {
        if (iSourceNode != NULL)
        {
            ReleaseSourceNodeInterfaces();
            ReleaseSourceNode(iSourceNodeUuid, iSourceNodeSessionId, iSourceNode);
        }
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::DoSetupSourceNode() End ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoSetupSourceNode() Out"));
    return status;
}

PVMFStatus PVMetadataEngine::DoSetSourceInitializationData()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::DoSetSourceInitializationData() Start ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoSetSourceInitializationData() In"));

    OSCL_wHeapString<OsclMemAllocator> sourceURL;
    // In case the URL starts with file:// skip it
    OSCL_wStackString<8> fileScheme(_STRLIT_WCHAR("file"));
    OSCL_wStackString<8> schemeDelimiter(_STRLIT_WCHAR("://"));
    const oscl_wchar* actualURL = NULL;
    if (oscl_strncmp(fileScheme.get_cstr(), iDataSource->GetDataSourceURL().get_cstr(), 4) == 0)
    {
        actualURL = oscl_strstr(iDataSource->GetDataSourceURL().get_cstr(), schemeDelimiter.get_cstr());
        if (actualURL == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSetSourceInitializationData() Unable to skip over file://"));
            return PVMFErrArgument;
        }
        //skip over ://
        actualURL += schemeDelimiter.get_size();
        sourceURL += actualURL;
    }
    else
    {
        sourceURL += iDataSource->GetDataSourceURL().get_cstr();
    }

    // If there is source context data, clear the PLAY intent to
    // avoid consumption of rights for protected content.
    PVInterface* pvInterface = OSCL_STATIC_CAST(PVInterface*, (iDataSource->GetDataSourceContextData()));
    if (pvInterface)
    {
        PVInterface* localDataSrc = NULL;
        PVUuid localDataSrcUuid(PVMF_LOCAL_DATASOURCE_UUID);
        if (pvInterface->queryInterface(localDataSrcUuid, localDataSrc))
        {
            PVMFLocalDataSource* opaqueData =
                OSCL_STATIC_CAST(PVMFLocalDataSource*, localDataSrc);
            opaqueData->iIntent &= ~(BITMASK_PVMF_SOURCE_INTENT_PLAY);
        }
        else
        {
            PVInterface* sourceDataContext = NULL;
            PVInterface* commonDataContext = NULL;
            PVUuid sourceContextUuid(PVMF_SOURCE_CONTEXT_DATA_UUID);
            PVUuid commonContextUuid(PVMF_SOURCE_CONTEXT_DATA_COMMON_UUID);
            if (pvInterface->queryInterface(sourceContextUuid, sourceDataContext))
            {
                if (sourceDataContext->queryInterface(commonContextUuid, commonDataContext))
                {
                    PVMFSourceContextDataCommon* cContext =
                        OSCL_STATIC_CAST(PVMFSourceContextDataCommon*, commonDataContext);
                    cContext->iIntent &= ~(BITMASK_PVMF_SOURCE_INTENT_PLAY);
                }
            }
        }
    }
    PVMFStatus retval = iSourceNodeInitIF->SetSourceInitializationData(sourceURL, iSourceFormatType, iDataSource->GetDataSourceContextData(), 0);
    if (retval != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSetSourceInitializationData() SetSourceInitializationData failed"));
        return PVMFFailure;
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::DoSetSourceInitializationData() End ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
    return PVMFSuccess;
}


PVMFStatus PVMetadataEngine::DoSourceNodeInit(PVCommandId aCmdId, OsclAny* aCmdContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoSourceNodeInit() In"));

    OSCL_ASSERT(iSourceNode != NULL);

    int32 leavecode = 0;

    if (iSourceNodeInitIF)
    {
        PVMFStatus status;
        status = DoSetSourceInitializationData();
        if (status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSetupSourceNode() DoSetSourceInitializationData failed"));
            return status;
        }
    }

    // Initialize the source node
    iPVMEContext.iCmdId = aCmdId;
    iPVMEContext.iCmdContext = aCmdContext;
    iPVMEContext.iCmdType = PVME_CMD_SourceNodeInit;

    leavecode = 0;
    PVMFCommandId cmdid = -1;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::Source Node Init Called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

    OSCL_TRY(leavecode, cmdid = iSourceNode->Init(iSourceNodeSessionId, (OsclAny*) & iPVMEContext));
    OSCL_FIRST_CATCH_ANY(leavecode,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoSourceNodeInit() Init on iSourceNode did a leave!"));
                         return PVMFFailure);

    SetPVMEState(PVME_INTERNAL_STATE_INITIALIZING);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoSourceNodeInit() Out"));

    return PVMFSuccess;
}


void PVMetadataEngine::NodeCommandCompleted(const PVMFCmdResp& aResponse)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::NodeCommandCompleted() In"));

    PVMEContext* nodecontext = (PVMEContext*)(aResponse.GetContext());
    OSCL_ASSERT(nodecontext);

    switch (nodecontext->iCmdType)
    {
        case PVME_CMD_SourceNodeInit:
            HandleSourceNodeInit(*nodecontext, aResponse);
            break;
        case PVME_CMD_SourceNodeReset:
            HandleSourceNodeReset(*nodecontext, aResponse);
            break;
        case PVME_CMD_GetNodeMetadataValue:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngine::NodeCommandCompleted - GetNodeMetadataValues() Completed ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
            //Delete the old source node in case we have not reused it
            if (iPreviousSourceNode != NULL)
            {
                ReleaseSourceNode(iPreviousSourceNodeUuid, iPreviousSourceNodeSessionId, iPreviousSourceNode);
            }
            PVMECommandCompleted(nodecontext->iCmdId, (OsclAny*)nodecontext->iCmdContext, aResponse.GetCmdStatus());
        }
        break;
        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::NodeCommandCompleted() Invalid source node command type in PVME_INTERNAL_STATE_INITIALIZING. Asserting"));
            OSCL_ASSERT(false);
            break;
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::NodeCommandCompleted() Out"));
}

void PVMetadataEngine::HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleNodeInformationalEvent() In"));

    PVMFNodeInterface* nodeorigin = (PVMFNodeInterface*)(aEvent.GetContext());

    // Process the info event based on the node type reporting the event
    if (nodeorigin == iSourceNode)
    {
        HandleSourceNodeInfoEvent(aEvent);
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleNodeInformationalEvent() Out"));
}

void PVMetadataEngine::HandleSourceNodeInfoEvent(const PVMFAsyncEvent& aEvent)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleSourceNodeInfoEvent() In"));

    if (iState == PVME_INTERNAL_STATE_RESETTING)
    {
        //this means error handling, reset is still in progress
        //no need to look for info event
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMetadataEngine::HandleSourceNodeInfoEvent() pvme in resetting state no need to process the info event %d", aEvent.GetEventType()));
        return;
    }
    //Pass the informational event as is

    PVMFEventType event = aEvent.GetEventType();
    SendInformationalEvent(event, NULL, aEvent.GetEventData(), aEvent.GetLocalBuffer(), aEvent.GetLocalBufferSize());

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleSourceNodeInfoEvent() Out"));
}



void PVMetadataEngine::HandleSourceNodeInit(PVMEContext& aNodeContext, const PVMFCmdResp& aNodeResp)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleSourceNodeInit() In"));

    PVMFStatus cmdstatus = aNodeResp.GetCmdStatus();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::Source Node Init() completed - Status=%d, ClockInMS=%d", cmdstatus, OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

    switch (cmdstatus)
    {
        case PVMFSuccess:
        case PVMFErrDrmLicenseNotFound:
        case PVMFErrDrmLicenseExpired:
        case PVMFErrHTTPAuthenticationRequired:
        case PVMFErrRedirect:
        {
            SetPVMEState(PVME_INTERNAL_STATE_INITIALIZED);
            //Call GetMetadataValues
            PVMFStatus retval = DoGetMetadataValues();

            if (retval != PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMetadataEngine:: HandleSourceNodeInit() DoGetMetadataValues failed, Add EH Command"));
                iCommandCompleteStatusInErrorHandling = retval;
                iCommandCompleteErrMsgInErrorHandling = NULL;
                AddCommandToQueue(PVME_COMMAND_ERROR_HANDLING, NULL, NULL, NULL, false);
            }
        }
        break;

        default:
        {
            //Pass the error event as is
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleSourceNodeInit() PVMFErrDrmLicenseNotFound/PVMFErrHTTPAuthenticationRequired/PVMFErrRedirect"));
            SetPVMEState(PVME_INTERNAL_STATE_IDLE);
            PVMECommandCompleted(aNodeContext.iCmdId, aNodeContext.iCmdContext, cmdstatus, NULL, aNodeResp.GetEventData());

        }
        break;

    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleSourceNodeInit() Out"));

}

void PVMetadataEngine::HandleSourceNodeReset(PVMEContext& aNodeContext, const PVMFCmdResp& aNodeResp)
{
    // This method is invoked in 2 different usecases.
    // 1) PVME has recvd a GetMetadata cmd and is processing the same. As part of this it sees an
    // existing source node (from some previous GetMetadata cmd) and resets the same as the first step.
    // In this case PVME state will be PVME_STATE_INITIALIZED
    // 2) Some error occured while prcessing GetMetadata and the source node is being reset. In this case
    // PVME will be PVME_INTERNAL_STATE_RESETTING. Once source node reset completes, we have completed
    // error handling and we transiton the state to PVME_INTERNAL_STATE_IDLE

    PVMFStatus cmdstatus = aNodeResp.GetCmdStatus();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::Source Node Reset Completed Status=%d, ClockInMS=%d", aNodeResp.GetCmdStatus(), OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleSourceNodeReset() In"));

    if (cmdstatus == PVMFSuccess)
    {
        if (iSourceNode->GetState() != EPVMFNodeIdle)
        {
            // when reset completes on Source node, source node should be in Idle State,
            // If not then just assert.
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMetadataEngine::HandleSourceNodeReset() Source Node not in a idle state after reset, Asserting"));
            OSCL_ASSERT(false);
        }

        //If we are resetting PVME, then cleanup the node
        if (iState == PVME_INTERNAL_STATE_RESETTING)
        {
            //We need to delete the source node now, that cannot be done in the callback, hence we need to reschedule the AO now.
            SetPVMEState(PVME_INTERNAL_STATE_IDLE);
            RunIfNotReady();

        }
        //If PVME is in initialized state, it means that Source Node Reset was called from DoGetMetadata
        else if (iState == PVME_INTERNAL_STATE_INITIALIZED)
        {
            // Check the source format and do a recognize if unknown
            PVMFStatus retval = PVMFSuccess;

            PVMFFormatType iPreviousSourceFormatType = iSourceFormatType;
            iSourceFormatType = iDataSource->GetDataSourceFormatType();

            if (iSourceFormatType == PVMF_MIME_FORMAT_UNKNOWN)
            {
                retval = DoQuerySourceFormatType(iCurrentCmd[0].GetCmdId(), iCurrentCmd[0].GetContext());
            }
            else if (iPreviousSourceFormatType == iSourceFormatType)
            {
                //If format type is known and is the same as previous clip,
                //then simply reuse the source node by calling Init and GetMetaDataValues on source node
                //directly.
                PVMFStatus retval = DoSourceNodeInit(iCurrentCmd[0].GetCmdId(), iCurrentCmd[0].GetContext());
                if (retval != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMetadataEngine:: HandleSourceNodeReset() DoSourceNodeInit failed, Add EH Command"));
                    iCommandCompleteStatusInErrorHandling = retval;
                    iCommandCompleteErrMsgInErrorHandling = NULL;
                    AddCommandToQueue(PVME_COMMAND_ERROR_HANDLING, NULL, NULL, NULL, false);
                }
            }
            else
            {
                //Previous source node is not required, but don't release it here just free the interface pointers
                ReleaseSourceNodeInterfaces();
                //Store the values of previous source node so that the node can be released later on
                iPreviousSourceNode = iSourceNode;
                iPreviousSourceNodeUuid = iSourceNodeUuid;
                iPreviousSourceNodeSessionId = iSourceNodeSessionId;

                iSourceNode = NULL;

                // Start the source node creation and setup sequence
                retval = DoSetupSourceNode(iCurrentCmd[0].GetCmdId(), iCurrentCmd[0].GetContext());
                if (retval != PVMFSuccess)
                {
                    //if an error occured in create / threadlogon / queryinterfacesync
                    //there is not much we can do with that node, so fail the getmetadata cmd.
                    //DoSetupSourceNode must have done necessary cleanups for the newly instantiated node
                    //however we are still in the context of previous source node, we cannot release
                    //iPreviousSourceNode here. so queue EH cmd
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMetadataEngine:: HandleSourceNodeReset() DoSetupSourceNode failed, Add EH Command"));
                    iCommandCompleteStatusInErrorHandling = retval;
                    iCommandCompleteErrMsgInErrorHandling = NULL;
                    AddCommandToQueue(PVME_COMMAND_ERROR_HANDLING, NULL, NULL, NULL, false);
                }
                else
                {
                    retval = DoSourceNodeInit(iCurrentCmd[0].GetCmdId(), iCurrentCmd[0].GetContext());
                    if (retval != PVMFSuccess)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "PVMetadataEngine:: HandleSourceNodeReset() DoSourceNodeInit failed, Add EH Command"));
                        iCommandCompleteStatusInErrorHandling = retval;
                        iCommandCompleteErrMsgInErrorHandling = NULL;
                        AddCommandToQueue(PVME_COMMAND_ERROR_HANDLING, NULL, NULL, NULL, false);
                    }
                }
            }
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMetadataEngine::HandleSourceNodeReset() Reset failed on Source Node, Asserting"));
        OSCL_ASSERT(false);
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::HandleSourceNodeReset() Out"));
}

void PVMetadataEngine::HandleNodeErrorEvent(const PVMFAsyncEvent& aEvent)
{
    PVMFNodeInterface* nodeorigin = (PVMFNodeInterface*)(aEvent.GetContext());
    // Process the error event based on the node type reporting the event
    if ((nodeorigin == iSourceNode) || (nodeorigin == iPreviousSourceNode))
    {
        HandleSourceNodeErrorEvent(aEvent);
    }
    else
    {
        // Event from unknown node or component. Do nothing but log it
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::HandleNodeErrorEvent() Error event from unknown node EventType 0x%x Context 0x%x Data 0x%x",
                        aEvent.GetEventType(), aEvent.GetContext(), aEvent.GetEventData()));
    }
}

void PVMetadataEngine::HandleSourceNodeErrorEvent(const PVMFAsyncEvent& aEvent)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVPlayerEngine::HandleNodeErrorEvent() Source Node Error Event - EventType 0x%x Context 0x%x Data 0x%x",
                    aEvent.GetEventType(), aEvent.GetContext(), aEvent.GetEventData()));

    if (iState == PVME_INTERNAL_STATE_RESETTING)
    {
        //this means error handling or reset is still in progress
        //no need to look for error event
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMetadataEngine::HandleSourceNodeErrorEvent() PVME in resetting state no need to process the error event"));
        return;
    }

    bool ehPending = CheckForPendingErrorHandlingCmd();
    if (ehPending)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::HandleSourceNodeErrorEvent() EH pending - Ignoring Error Event"));
        return;
    }
    else
    {
        //We cannot simply send error event alone. We should also do EH here.
        iCommandCompleteStatusInErrorHandling = aEvent.GetEventType();
        iCommandCompleteErrMsgInErrorHandling = NULL;
        AddCommandToQueue(PVME_COMMAND_ERROR_HANDLING, NULL, NULL, NULL, false);

        //Pass the error event as is
        PVMFEventType event = aEvent.GetEventType();
        SendErrorEvent(event, NULL, aEvent.GetEventData(), aEvent.GetLocalBuffer(), aEvent.GetLocalBufferSize());
    }
}

PVMFStatus PVMetadataEngine::DoGetMetadataValues()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetMetadataValues() In"));

    iValueList = (Oscl_Vector<PvmiKvp, OsclMemAllocator>*)(iCurrentCmd[0].GetParam(1).pOsclAny_value);

    if (iRequestedMetadataKeyList.size() == 0)
    {
        OSCL_StackString<8> allmetadata(_STRLIT_CHAR("all"));
        iRequestedMetadataKeyList.push_back(allmetadata);
    }

    iPVMEContext.iCmdId = iCurrentCmd[0].GetCmdId();
    iPVMEContext.iCmdContext = iCurrentCmd[0].GetContext();
    iPVMEContext.iCmdType = PVME_CMD_GetNodeMetadataValue;

    PVMFCommandId cmdid = -1;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                    (0, "PVMetadataEngine::GetNodeMetadataValues() Called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

    cmdid = iSourceNodeMetadataExtIF->GetNodeMetadataValues(iSourceNodeSessionId,
            iRequestedMetadataKeyList,
            *iValueList,
            0,
            256,
            (OsclAny*) & iPVMEContext);

    if (cmdid == -1)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMetadataEngine::DoGetMetadataValues() GetNodeMetadataValues failed"));
        return PVMFFailure;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetMetadataValues() Out"));

    return PVMFSuccess;
}

PVMFStatus PVMetadataEngine::DoGetPVMEState(PVMECommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetPVMEState() In"));

    PVMetadataEngineState* state = (PVMetadataEngineState*)(aCmd.GetParam(0).pOsclAny_value);
    if (state == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::DoGetPVMEState() Passed in parameter invalid."));
        return PVMFErrArgument;
    }

    // Get player state using internal function
    *state = GetPVMEState();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMetadataEngine::DoGetPVMEState() Out"));
    return PVMFSuccess;
}

void PVMetadataEngine::ReleaseSourceNodeInterfaces(void)
{
    if (iSourceNode)
    {
        // Remove reference to the parser node init interface if available
        if (iSourceNodeInitIF)
        {
            iSourceNodeInitIF->removeRef();
            iSourceNodeInitIF = NULL;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMetadataEngine::ReleaseSourceNodeInterfaces() - iSourceNodeInitIF Released"));
        }

        // Remove reference to the parser node metadata interface if available
        if (iSourceNodeMetadataExtIF)
        {
            iSourceNodeMetadataExtIF->removeRef();
            iSourceNodeMetadataExtIF = NULL;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMetadataEngine::ReleaseSourceNodeInterfaces() - iSourceNodeMetadataExtIF Released"));
        }
    }
}

void PVMetadataEngine::ReleaseSourceNode(PVUuid& aUUID, PVMFSessionId& aID, PVMFNodeInterface*& aNode)
{
    //ignore return codes here since we are going to pretty much delete the node later anyway
    aNode->Disconnect(aID);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMetadataEngine::ReleaseSourceNode() - Disconnect Done"));
    aNode->ThreadLogoff();
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMetadataEngine::ReleaseSourceNode() - ThreadLogoff Done"));
    iPVMENodeRegistry.ReleaseNode(aUUID, aNode);
    aNode = NULL;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMetadataEngine::ReleaseSourceNode() - SourceNode Delete Done"));
}

void PVMetadataEngine::SendErrorEvent(PVMFEventType aEventType, PVInterface* aExtInterface, OsclAny* aEventData, uint8* aLocalBuffer, uint32 aLocalBufferSize)
{
    // Send the error event if observer has been specified
    if (iErrorEventObserver)
    {
        PVAsyncErrorEvent errorevent((PVEventType)aEventType, NULL, aExtInterface, (PVExclusivePtr)aEventData, aLocalBuffer, aLocalBufferSize);
        iErrorEventObserver->HandleErrorEvent(errorevent);
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::SendErrorEvent() iErrorEventObserver is NULL"));
    }
}

void PVMetadataEngine::SendInformationalEvent(PVMFEventType aEventType, PVInterface* aExtInterface, OsclAny* aEventData, uint8* aLocalBuffer, uint32 aLocalBufferSize)
{
    // Send the info event if observer has been specified
    if (iInfoEventObserver)
    {
        PVAsyncInformationalEvent infoevent((PVEventType)aEventType, NULL, aExtInterface, (PVExclusivePtr)aEventData, aLocalBuffer, aLocalBufferSize);
        iInfoEventObserver->HandleInformationalEvent(infoevent);
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMetadataEngine::SendInformationalEvent() iInfoEventObserver is NULL"));
    }
}

