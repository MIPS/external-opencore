/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
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
#include "lipsync_dummy_output_mio.h"
#include "pv_mime_string_utils.h"

const uint32 KNumMsgBeforeFlowControl = 25;


#define LIPSYNCDUMMYOUTPUTMIO_LOGERROR(m)       PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_ERR,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGWARNING(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_WARNING,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGINFO(m)        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_INFO,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG(m)       PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_DEBUG,m);


LipSyncDummyOutputMIO::LipSyncDummyOutputMIO(const LipSyncDummyMIOSettings& aSettings)
        : OsclActiveObject(OsclActiveObject::EPriorityNominal, aSettings.iMediaFormat.isAudio() ? "LipSyncDummyAudioOutputMIO" : "LipSyncDummyVideoOutputMIO")
{
    iNumChannels = 0;//no default
    iSamplingRate = 0;//no default
    iBitsPerSample = 0;//no default
    iAudioFormat = PVMF_MIME_FORMAT_UNKNOWN;//no default
    iSamplingRateSet = false;
    iNumChannelsSet = false;
    iBitsPerSampleSet = false;
    iObserver = NULL;
    iCommandCounter = 0;
    iLogger = NULL;
    iCommandPendingQueue.reserve(1);
    iCommandResponseQueue.reserve(5);
    iPeer = NULL;
    iState = STATE_IDLE;
    iIsMIOConfigured = false;
    iSettings = aSettings;
    iIsAudioMIO = false;
    iIsVideoMIO = false;
    iIsCompressed = false;

}
void LipSyncDummyOutputMIO::ResetData()
//reset all data from this session so that a new clip can be played.
{
    Cleanup();

    //reset all the audio parameters.
    iAudioFormatString = "";
    iAudioFormat = PVMF_MIME_FORMAT_UNKNOWN;//no default
    iSamplingRateSet = false;
    iNumChannelsSet = false;
    iBitsPerSampleSet = false;
    iIsMIOConfigured = false;
}

PVMFCommandId LipSyncDummyOutputMIO::Reset(const OsclAny* aContext)
{
    // Reset all data from this session
    ResetData();

    // TEMP to properly behave asynchronously
    PVMFCommandId cmdid = iCommandCounter++;
    CommandResponse resp(PVMFSuccess, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

void LipSyncDummyOutputMIO::Cleanup()
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

LipSyncDummyOutputMIO::~LipSyncDummyOutputMIO()
{
    Cleanup();
    // release semaphore
}

PVMFStatus LipSyncDummyOutputMIO::connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver)
{
    OSCL_UNUSED_ARG(aSession);

    // Currently supports only one session
    if (iObserver)
    {
        LIPSYNCDUMMYOUTPUTMIO_LOGERROR((0, "LipSyncDummyOutputMIO::connect Session already exists. Can only support one session"));
        return PVMFErrAlreadyExists;
    }

    iObserver = aObserver;

    iParams = ShareParams::Instance();
    iParams->iObserver = this;
    return PVMFSuccess;
}


PVMFStatus LipSyncDummyOutputMIO::disconnect(PvmiMIOSession aSession)
{
    OSCL_UNUSED_ARG(aSession);

    // Currently supports only one session
    if (iObserver == NULL)
    {
        LIPSYNCDUMMYOUTPUTMIO_LOGERROR((0, "LipSyncDummyOutputMIO::disconnect No session to disconnect"));
        return PVMFErrArgument;
    }
    iObserver = NULL;
    return PVMFSuccess;
}


PvmiMediaTransfer* LipSyncDummyOutputMIO::createMediaTransfer(
    PvmiMIOSession& aSession,
    PvmiKvp* read_formats, int32 read_flags,
    PvmiKvp* write_formats, int32 write_flags)
{
    return (PvmiMediaTransfer*)this;
}


void LipSyncDummyOutputMIO::QueueCommandResponse(CommandResponse& aResp)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::QueueCommandResponse(), aResp.CmdId is %d ", aResp.iCmdId));
    //queue a command response and schedule processing.
    iCommandResponseQueue.push_back(aResp);

    //schedule the AO.
    if (!IsBusy())
        PendForExec();
    if (IsAdded() && iStatus == OSCL_REQUEST_PENDING)
        PendComplete(OSCL_REQUEST_ERR_NONE);
}

PVMFCommandId LipSyncDummyOutputMIO::QueryUUID(const PvmfMimeString& aMimeType,
        Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids,
        bool aExactUuidsOnly,
        const OsclAny* aContext)
{
    LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG((0, "LipSyncDummyOutputMIO::QueryUUID()"));

    OSCL_UNUSED_ARG(aMimeType);
    OSCL_UNUSED_ARG(aExactUuidsOnly);

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFSuccess;
    aUuids.push_back(PVMI_CAPABILITY_AND_CONFIG_PVUUID);
    aUuids.push_back(PvmiClockExtensionInterfaceUuid);

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId LipSyncDummyOutputMIO::QueryInterface(const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG((0, "LipSyncDummyOutputMIO::QueryInterface()"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = PVMFSuccess;
    }
    else if ((aUuid == PvmiClockExtensionInterfaceUuid) && (iIsAudioMIO))
    {
        PvmiClockExtensionInterface* myInterface = OSCL_STATIC_CAST(PvmiClockExtensionInterface*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = true;
    }
    else
    {
        status = PVMFFailure;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}


bool LipSyncDummyOutputMIO::queryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr)
{
    aInterfacePtr = NULL;
    bool status = false;
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = true;
    }
    else if ((aUuid == PvmiClockExtensionInterfaceUuid) && (iIsAudioMIO))
    {
        PvmiClockExtensionInterface* myInterface = OSCL_STATIC_CAST(PvmiClockExtensionInterface*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = true;
    }
    else
    {
        status = false;
    }

    return status;
}

PVMFStatus LipSyncDummyOutputMIO::SetClock(PVMFMediaClock* aClockVal)
{
    iRenderClock = aClockVal;
    return PVMFSuccess;
}

void LipSyncDummyOutputMIO::addRef()
{
    // nothing to do...
}

void LipSyncDummyOutputMIO::removeRef()
{
    // nothing to do...
}

void LipSyncDummyOutputMIO::deleteMediaTransfer(
    PvmiMIOSession& aSession,
    PvmiMediaTransfer* media_transfer)
{
    // This class is implementing the media transfer so no cleanup is needed.
}


PVMFCommandId LipSyncDummyOutputMIO::Init(const OsclAny* aContext)
{
    LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG((0, "LipSyncDummyOutputMIO::Init()"));

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


PVMFCommandId LipSyncDummyOutputMIO::Start(const OsclAny* aContext)
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

PVMFCommandId LipSyncDummyOutputMIO::Pause(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::Pause() "));

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

PVMFCommandId LipSyncDummyOutputMIO::Flush(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::Flush() "));

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

PVMFCommandId LipSyncDummyOutputMIO::DiscardData(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::DiscardData() "));

    PVMFCommandId cmdid = iCommandCounter++;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::DiscardData, CmdId is %d ", cmdid));

    PVMFStatus status;
    status = PVMFSuccess;

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId LipSyncDummyOutputMIO::DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "LipSyncDummyOutputMIO::DiscardData() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::DiscardData with TS, CmdId is %d ", cmdid));

    CommandResponse resp(PVMFSuccess, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId LipSyncDummyOutputMIO::Stop(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::Stop() "));


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

PVMFCommandId LipSyncDummyOutputMIO::CancelAllCommands(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::CancelAllCommands() "));

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

PVMFCommandId LipSyncDummyOutputMIO::CancelCommand(PVMFCommandId aCmdId, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "LipSyncDummyOutputMIO::CancelCommand() "));

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

void LipSyncDummyOutputMIO::ThreadLogon()
{
    if (iState == STATE_IDLE)
    {
        iLogger = PVLogger::GetLoggerObject("LipSyncDummyOutputMIO");
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "LipSyncDummyOutputMIO::ThreadLogon() "));
        AddToScheduler();
        iState = STATE_LOGGED_ON;
        iIsAudioMIO = iSettings.iMediaFormat.isAudio();
        iIsVideoMIO = iSettings.iMediaFormat.isVideo();
        iIsCompressed = iSettings.iMediaFormat.isCompressed();
        iMIOObserver = iSettings.iDummyMIOObserver;
    }
}

void LipSyncDummyOutputMIO::ThreadLogoff()
{
    if (iState != STATE_IDLE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "LipSyncDummyOutputMIO::ThreadLogoff() "));
        iLogger = NULL;
        RemoveFromScheduler();
        iState = STATE_IDLE;
        //Reset any data from this session.
        ResetData();
    }
}

void LipSyncDummyOutputMIO::Run()
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

void LipSyncDummyOutputMIO::setPeer(PvmiMediaTransfer* aPeer)
{
    // Set the observer
    iPeer = aPeer;
}


void LipSyncDummyOutputMIO::useMemoryAllocators(OsclMemAllocator* write_alloc)
{
    //not supported-- ignore
}

PVMFCommandId LipSyncDummyOutputMIO::writeAsync(
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
                    (0, "LipSyncDummyOutputMIO::writeAsync() seqnum %d ts %d context %d", data_header_info.seq_num, aTimestamp, aContext));

    PVMFCommandId cmdid = iCommandCounter++;

    if (IsAdded() && !IsBusy())
        PendForExec();


    if (iMIOObserver)
    {
        uint32 origTS = 0;
        if (iIsAudioMIO && iIsCompressed)
        {
            oscl_memcpy((char *)&origTS, aData, sizeof(origTS));
            iMIOObserver->MIOFramesTSUpdate(true, (PVMFTimestamp)origTS);
        }
        else if (iIsVideoMIO && iIsCompressed)
        {
            oscl_memcpy((char *)&origTS, aData + 4, sizeof(origTS));
            iMIOObserver->MIOFramesTSUpdate(false, (PVMFTimestamp)origTS);

        }
        else if (iIsVideoMIO && !iIsCompressed)
        {

            Oscl_Map<uint32, uint32, OsclMemAllocator>::iterator it;
            it = iCompareTS.find(aTimestamp);
            if (!(it == iCompareTS.end()))
            {
                origTS = (((*it).second));
            }
            iMIOObserver->MIOFramesTSUpdate(false, (PVMFTimestamp)origTS);
        }
    }

    if (iIsAudioMIO)
    {
        uint32 timebfAdjust = 0;
        bool flag = false;
        uint32 currentTimeBase32 = 0;
        bool overflowFlag = false;
        uint32 timeVal = Oscl_Int64_Utils::get_uint64_lower32(aTimestamp);
        iRenderClock->GetCurrentTime32(timebfAdjust, flag, PVMF_MEDIA_CLOCK_MSEC, currentTimeBase32);

        // Adjust the render-clock to accurately represent this.
        PVMFMediaClockAdjustTimeStatus retVal;
        retVal = iRenderClock->AdjustClockTime32(timebfAdjust,
                 currentTimeBase32,
                 timeVal,
                 PVMF_MEDIA_CLOCK_MSEC, overflowFlag);
    }
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



void LipSyncDummyOutputMIO::writeComplete(PVMFStatus aStatus,
        PVMFCommandId write_cmd_id,
        OsclAny* aContext)
{
    //won't be called since this component is a sink.
}



PVMFCommandId LipSyncDummyOutputMIO::readAsync(
    uint8* data, uint32 max_data_len,
    OsclAny* aContext,
    int32* formats, uint16 num_formats)
{
    //read not supported.
    OsclError::Leave(OsclErrNotSupported);
    return -1;
}


void LipSyncDummyOutputMIO::readComplete(
    PVMFStatus aStatus,
    PVMFCommandId read_cmd_id,
    int32 format_index,
    const PvmiMediaXferHeader& data_header_info,
    OsclAny* aContext)
{
    //won't be called since this component is a sink.
}

void LipSyncDummyOutputMIO::statusUpdate(uint32 status_flags)
{
    //resume data transfer
    if (iPeer)
        iPeer->statusUpdate(PVMI_MEDIAXFER_STATUS_WRITE);
    RunIfNotReady();
    //won't be called since this component is a sink.
}


void LipSyncDummyOutputMIO::cancelCommand(PVMFCommandId aCmdId)
{
    //the purpose of this API is to cancel a writeAsync command and report
    //completion ASAP.
}

// lower-case cancel all commands for the node's ports only. not the engine commands
void LipSyncDummyOutputMIO::cancelAllCommands()
{
}


void LipSyncDummyOutputMIO::setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
{
    OSCL_UNUSED_ARG(aObserver);
    //not needed since this component only supports synchronous capability & config
    //APIs.
}


PVMFStatus LipSyncDummyOutputMIO::releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements)
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

void LipSyncDummyOutputMIO::createContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OsclError::Leave(OsclErrNotSupported);
}

void LipSyncDummyOutputMIO::setContextParameters(PvmiMIOSession session, PvmiCapabilityContext& context,
        PvmiKvp* parameters, int num_parameter_elements)
{
    OsclError::Leave(OsclErrNotSupported);
}


void LipSyncDummyOutputMIO::DeleteContext(PvmiMIOSession session,
        PvmiCapabilityContext& context)
{
    OsclError::Leave(OsclErrNotSupported);
}


void LipSyncDummyOutputMIO::setParametersSync(PvmiMIOSession aSession,
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
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_AUDIO_SAMPLING_RATE_KEY) == 0)
        {
            //Do nothing since this is a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_AUDIO_NUM_CHANNELS_KEY) == 0)
        {
            //Do nothing since this is a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
        {
            //We simply check it to make sure that an error isn't returned for this key.
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_FORMAT_KEY) == 0)
        {
            //Do nothing since this is a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_WIDTH_KEY) == 0)
        {
            //Do nothing since this is a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_HEIGHT_KEY) == 0)
        {
            //Do nothing since this is a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_DISPLAY_HEIGHT_KEY) == 0)
        {
            //Do nothing since this is a dummy
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_DISPLAY_WIDTH_KEY) == 0)
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


PVMFCommandId LipSyncDummyOutputMIO::setParametersAsync(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements,
        PvmiKvp*& ret_kvp,
        OsclAny* context)
{
    OsclError::Leave(OsclErrNotSupported);
    return -1;
}


uint32 LipSyncDummyOutputMIO::getCapabilityMetric(PvmiMIOSession session)
{
    return 0;
}


PVMFStatus LipSyncDummyOutputMIO::verifyParametersSync(PvmiMIOSession session,
        PvmiKvp* parameters, int num_elements)
{
    return PVMFSuccess; // no restrictions on playback settings always return OK
}


PVMFStatus LipSyncDummyOutputMIO::getParametersSync(PvmiMIOSession aSession,
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
void LipSyncDummyOutputMIO::MIOFramesTimeStamps(bool aIsAudio, uint32 aOrigTS, uint32 aRenderTS)
{
    if (aIsAudio)
    {
        /* Currently we are doing for video case TS .Need to develop audio case also */


    }
    else
    {
        iCompareTS[aRenderTS] = aOrigTS;

    }
}
