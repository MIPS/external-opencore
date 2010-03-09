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
#include "pv_frame_metadata_mio.h"
#include "pvlogger.h"
#include "pv_mime_string_utils.h"
#include "oscl_snprintf.h"
#include "cczoomrotationbase.h"

PVFMMIO::PVFMMIO() :
        OsclTimerObject(OsclActiveObject::EPriorityNominal, "PVFMMIO")
{
    InitData();
}


void PVFMMIO::InitData()
{
    iWriteBusy = false;
    iIsMIOConfigured = false;
    iCommandCounter = 0;
    iLogger = NULL;
    iCommandResponseQueue.reserve(5);
    iWriteResponseQueue.reserve(5);
    iObserver = NULL;
    iLogger = NULL;
    iPeer = NULL;
    iState = STATE_IDLE;
}


void PVFMMIO::ResetData()
{
    Cleanup();

    iIsMIOConfigured = false;
    iWriteBusy = false;
}


void PVFMMIO::Cleanup()
{
    while (!iCommandResponseQueue.empty())
    {
        if (iObserver)
        {
            iObserver->RequestCompleted(PVMFCmdResp(iCommandResponseQueue[0].iCmdId, iCommandResponseQueue[0].iContext, iCommandResponseQueue[0].iStatus));
        }
        iCommandResponseQueue.erase(&iCommandResponseQueue[0]);
    }

    while (!iWriteResponseQueue.empty())
    {
        if (iPeer)
        {
            iPeer->writeComplete(iWriteResponseQueue[0].iStatus, iWriteResponseQueue[0].iCmdId, (OsclAny*)iWriteResponseQueue[0].iContext);
        }
        iWriteResponseQueue.erase(&iWriteResponseQueue[0]);
    }
}


PVFMMIO::~PVFMMIO()
{
    Cleanup();
}


PVMFStatus PVFMMIO::connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::connect() called"));

    // Currently supports only one session
    OSCL_UNUSED_ARG(aSession);

    if (iObserver)
    {
        return PVMFFailure;
    }

    iObserver = aObserver;
    return PVMFSuccess;
}


PVMFStatus PVFMMIO::disconnect(PvmiMIOSession aSession)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::disconnect() called"));

    // Currently supports only one session
    OSCL_UNUSED_ARG(aSession);

    iObserver = NULL;
    return PVMFSuccess;
}


PvmiMediaTransfer* PVFMMIO::createMediaTransfer(PvmiMIOSession& aSession, PvmiKvp* read_formats, int32 read_flags,
        PvmiKvp* write_formats, int32 write_flags)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::createMediaTransfer() called"));

    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(read_formats);
    OSCL_UNUSED_ARG(read_flags);
    OSCL_UNUSED_ARG(write_formats);
    OSCL_UNUSED_ARG(write_flags);

    return (PvmiMediaTransfer*)this;
}


void PVFMMIO::QueueCommandResponse(CommandResponse& aResp)
{
    // Queue a command response and schedule processing.
    iCommandResponseQueue.push_back(aResp);

    // Cancel any timer delay so the command response will happen ASAP.
    if (IsBusy())
    {
        Cancel();
    }

    RunIfNotReady();
}

PVMFCommandId PVFMMIO::QueryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::QueryInterface() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = PVMFSuccess;
    }
    else if (aUuid == PvmiClockExtensionInterfaceUuid)
    {
        PvmiClockExtensionInterface* myInterface = OSCL_STATIC_CAST(PvmiClockExtensionInterface*, &iActiveTiming);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = PVMFSuccess;
        iActiveTiming.addRef();
    }
    else
    {
        status = PVMFFailure;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}


void PVFMMIO::deleteMediaTransfer(PvmiMIOSession& aSession, PvmiMediaTransfer* media_transfer)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::deleteMediaTransfer() called"));

    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(media_transfer);

    // This class is implementing the media transfer, so no cleanup is needed
}


PVMFCommandId PVFMMIO::Init(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::Init() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;

    switch (iState)
    {
        case STATE_LOGGED_ON:
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

PVMFCommandId PVFMMIO::Reset(const OsclAny* aContext)
{
    ResetData();

    PVMFCommandId cmdid = iCommandCounter++;
    PVMFStatus status = PVMFSuccess;

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId PVFMMIO::Start(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::Start() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;

    switch (iState)
    {
        case STATE_INITIALIZED:
        case STATE_PAUSED:
            iState = STATE_STARTED;
            status = PVMFSuccess;
            if (iPeer && iWriteBusy)
            {
                iWriteBusy = false;
                iPeer->statusUpdate(PVMI_MEDIAXFER_STATUS_WRITE);
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

PVMFCommandId PVFMMIO::Pause(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::Pause() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;

    switch (iState)
    {
        case STATE_STARTED:
            iState = STATE_PAUSED;
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


PVMFCommandId PVFMMIO::Flush(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::Flush() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;

    switch (iState)
    {
        case STATE_STARTED:
            iState = STATE_INITIALIZED;
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


PVMFCommandId PVFMMIO::DiscardData(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::DiscardData() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    // This component doesn't buffer data, so there's nothing needed here.
    PVMFStatus status = PVMFSuccess;

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFCommandId PVFMMIO::DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::DiscardData(timestamp,context) called"));

    OSCL_UNUSED_ARG(aTimestamp);
    return DiscardData(aContext);
}


PVMFCommandId PVFMMIO::Stop(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::Stop() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;

    switch (iState)
    {
        case STATE_STARTED:
        case STATE_PAUSED:
            iState = STATE_INITIALIZED;
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

PVMFCommandId PVFMMIO::CancelAllCommands(const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::CancelAllCommands() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    // Commands are executed immediately upon being received, so it isn't really possible to cancel them.

    PVMFStatus status = PVMFSuccess;

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

PVMFCommandId PVFMMIO::CancelCommand(PVMFCommandId aCmdId, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::CancelCommand() called"));

    PVMFCommandId cmdid = iCommandCounter++;

    // Commands are executed immediately upon being received, so it isn't really possible to cancel them.

    // See if the response is still queued.
    PVMFStatus status = PVMFFailure;
    for (uint32 i = 0; i < iCommandResponseQueue.size(); i++)
    {
        if (iCommandResponseQueue[i].iCmdId == aCmdId)
        {
            status = PVMFSuccess;
            break;
        }
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}

void PVFMMIO::ThreadLogon()
{
    if (iState == STATE_IDLE)
    {
        iLogger = PVLogger::GetLoggerObject("PVFMMIO");
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::ThreadLogon() called"));
        AddToScheduler();
        iState = STATE_LOGGED_ON;
    }
}


void PVFMMIO::ThreadLogoff()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::ThreadLogoff() called"));

    if (iState != STATE_IDLE)
    {
        RemoveFromScheduler();
        iLogger = NULL;
        iState = STATE_IDLE;
    }
}


void PVFMMIO::setPeer(PvmiMediaTransfer* aPeer)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::setPeer() called"));

    // Set the observer
    iPeer = aPeer;
}


void PVFMMIO::useMemoryAllocators(OsclMemAllocator* write_alloc)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::useMemoryAllocators() called"));

    OSCL_UNUSED_ARG(write_alloc);

    // Not supported.
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMMIO::useMemoryAllocators() NOT SUPPORTED"));
}

void PVFMMIO::writeComplete(PVMFStatus aStatus, PVMFCommandId write_cmd_id, OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::writeComplete() called"));

    OSCL_UNUSED_ARG(aStatus);
    OSCL_UNUSED_ARG(write_cmd_id);
    OSCL_UNUSED_ARG(aContext);

    // Won't be called since this component is a sink.
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMMIO::writeComplete() Should not be called since this MIO is a sink"));
}


PVMFCommandId PVFMMIO::readAsync(uint8* data, uint32 max_data_len, OsclAny* aContext, int32* formats, uint16 num_formats)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::readAsync() called"));

    OSCL_UNUSED_ARG(data);
    OSCL_UNUSED_ARG(max_data_len);
    OSCL_UNUSED_ARG(aContext);
    OSCL_UNUSED_ARG(formats);
    OSCL_UNUSED_ARG(num_formats);

    // Read not supported.
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMMIO::readAsync() NOT SUPPORTED"));
    OsclError::Leave(OsclErrNotSupported);
    return -1;
}


void PVFMMIO::readComplete(PVMFStatus aStatus, PVMFCommandId read_cmd_id, int32 format_index,
                           const PvmiMediaXferHeader& data_header_info, OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::readComplete() called"));

    OSCL_UNUSED_ARG(aStatus);
    OSCL_UNUSED_ARG(read_cmd_id);
    OSCL_UNUSED_ARG(format_index);
    OSCL_UNUSED_ARG(data_header_info);
    OSCL_UNUSED_ARG(aContext);

    // Won't be called since this component is a sink.
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMMIO::readComplete() Should not be called since this MIO is a sink"));
}


void PVFMMIO::statusUpdate(uint32 status_flags)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::statusUpdate() called"));

    OSCL_UNUSED_ARG(status_flags);

    // Won't be called since this component is a sink.
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMMIO::statusUpdate() Should not be called since this MIO is a sink"));
}


void PVFMMIO::cancelCommand(PVMFCommandId  command_id)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::cancelCommand() called"));

    // The purpose of this API is to cancel a writeAsync command and report completion ASAP.

    // In this implementation, the write commands are executed immediately when received so it isn't
    // really possible to cancel. Just report completion immediately.

    for (uint32 i = 0; i < iWriteResponseQueue.size(); i++)
    {
        if (iWriteResponseQueue[i].iCmdId == command_id)
        {
            //report completion
            if (iPeer)
            {
                iPeer->writeComplete(iWriteResponseQueue[i].iStatus, iWriteResponseQueue[i].iCmdId, (OsclAny*)iWriteResponseQueue[i].iContext);
            }
            iWriteResponseQueue.erase(&iWriteResponseQueue[i]);
            break;
        }
    }
}


void PVFMMIO::cancelAllCommands()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::cancelAllCommands() called"));

    // The purpose of this API is to cancel all writeAsync commands and report completion ASAP.

    // In this implementaiton, the write commands are executed immediately when received so it isn't
    // really possible to cancel. Just report completion immediately.

    for (uint32 i = 0; i < iWriteResponseQueue.size(); i++)
    {
        //report completion
        if (iPeer)
        {
            iPeer->writeComplete(iWriteResponseQueue[i].iStatus, iWriteResponseQueue[i].iCmdId, (OsclAny*)iWriteResponseQueue[i].iContext);
        }
        iWriteResponseQueue.erase(&iWriteResponseQueue[i]);
    }
}

PVMFStatus PVFMMIO::releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::releaseParameters() called"));

    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(num_elements);

    // Release parameters that were allocated by this component.
    if (aParameters)
    {
        oscl_free(aParameters);
        return PVMFSuccess;
    }
    return PVMFFailure;
}

PVMFStatus PVFMMIO::verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMMIO::verifyParametersSync() called"));

    OSCL_UNUSED_ARG(aSession);

    // Go through each parameter
    for (int32 paramind = 0; paramind < num_elements; ++paramind)
    {
        // Retrieve the first component from the key string
        char* compstr = NULL;
        pv_mime_string_extract_type(0, aParameters[paramind].key, compstr);

        if (pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf/media/format-type")) == 0)
        {
            Oscl_Vector<PVMFFormatType, OsclMemAllocator>::iterator it;
            for (it = iInputFormatCapability.begin(); it != iInputFormatCapability.end(); it++)
            {
                if (pv_mime_strcmp(aParameters[paramind].value.pChar_value, it->getMIMEStrPtr()) == 0)
                {
                    return PVMFSuccess;
                }
            }
            // Not found on the list of supported input formats
            return PVMFErrNotSupported;
        }
    }
    // For all other parameters return success.
    return PVMFSuccess;
}


//
// For active timing support
//
PVMFStatus PVFMMIOActiveTimingSupport::SetClock(PVMFMediaClock *clockVal)
{
    iClock = clockVal;
    return PVMFSuccess;
}


void PVFMMIOActiveTimingSupport::addRef()
{
}


void PVFMMIOActiveTimingSupport::removeRef()
{
}


bool PVFMMIOActiveTimingSupport::queryInterface(const PVUuid& aUuid, PVInterface*& aInterface)
{
    aInterface = NULL;
    PVUuid uuid;
    if (PvmiClockExtensionInterfaceUuid == aUuid)
    {
        PvmiClockExtensionInterface* myInterface = OSCL_STATIC_CAST(PvmiClockExtensionInterface*, this);
        aInterface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        return true;
    }
    return false;
}


//
// Private section
//
void PVFMMIO::Run()
{
    // Send async command responses
    while (!iCommandResponseQueue.empty())
    {
        if (iObserver)
        {
            iObserver->RequestCompleted(PVMFCmdResp(iCommandResponseQueue[0].iCmdId, iCommandResponseQueue[0].iContext, iCommandResponseQueue[0].iStatus));
        }
        iCommandResponseQueue.erase(&iCommandResponseQueue[0]);
    }

    // Send async write completion
    while (!iWriteResponseQueue.empty())
    {
        // Report write complete
        if (iPeer)
        {
            iPeer->writeComplete(iWriteResponseQueue[0].iStatus, iWriteResponseQueue[0].iCmdId, (OsclAny*)iWriteResponseQueue[0].iContext);
        }
        iWriteResponseQueue.erase(&iWriteResponseQueue[0]);
    }
}






