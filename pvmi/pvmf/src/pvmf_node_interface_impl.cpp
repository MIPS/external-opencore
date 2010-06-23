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
#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#include "pvmf_node_interface_impl.h"
#endif
#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif
#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif
#ifndef PVMF_MEDIA_CMD_H_INCLUDED
#include "pvmf_media_cmd.h"
#endif
#include "pvmf_media_msg_format_ids.h"

#define PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_STACK_TRACE,m);
#define PVMF_NODEINTERFACE_IMPL_INFO(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_INFO,m);

OSCL_EXPORT_REF PVMFNodeInterfaceImpl::PVMFNodeInterfaceImpl(int32 aPriority, const char aNodeName[])
        : OsclActiveObject(aPriority, aNodeName),
        iStreamID(0),
        iExtensionRefCount(0),
        iLogger(NULL)
{
    // set the node name
    iNodeName.Set(aNodeName);
    // use node's logger string
    iLogger = PVLogger::GetLoggerObject((char*)iNodeName.Str());

    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::PVMFNodeInterfaceImpl() In", iNodeName.Str()));

#if !(OSCL_BYPASS_MEMMGT)
    iAuditCB.pAudit = NULL;

    // set audit object
    iAuditCB.pAudit = OsclMemGlobalAuditObject::getGlobalMemAuditObject();
    if (true == iAuditCB.pAudit->MM_AddTag(aNodeName))
    {
        iAuditCB.pStatsNode = iAuditCB.pAudit->MM_GetExistingTag(aNodeName);
    }
    else
    {
        PVMF_NODEINTERFACE_IMPL_INFO((0, "%s::Constructor() MemoryAuditing Tag addition failed", iNodeName.Str()));
    }
#endif
    //intialize node state
    iInterfaceState = EPVMFNodeCreated;

    iSessions.reserve(PVMF_NODE_DEFAULT_SESSION_RESERVE);

    int32 err = OsclErrNone;
    OSCL_TRY(err,
             iInputCommands.Construct(PVMF_NODE_COMMAND_ID_START,
                                      PVMF_NODE_VECTOR_RESERVE););
    if (err != OsclErrNone)
    {
        iInputCommands.clear();
    }
}

OSCL_EXPORT_REF PVMFSessionId PVMFNodeInterfaceImpl::Connect(const PVMFNodeSessionInfo &aSession)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Connect() In", iNodeName.Str()));
    PVMFNodeSession session;
    session.iId = iSessions.size();
    session.iInfo = aSession;
    iSessions.push_back(session);
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Connect() Out", iNodeName.Str()));
    return session.iId;
}

OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::Disconnect(PVMFSessionId aSessionId)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Disconnect() In", iNodeName.Str()));
    for (uint32 i = 0; i < iSessions.size(); i++)
    {
        if (iSessions[i].iId == aSessionId)
        {
            iSessions.erase(&iSessions[i]);
            PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Disconnect() Out Success", iNodeName.Str()));
            return PVMFSuccess;
        }
    }
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Disconnect() Failed", iNodeName.Str()));
    return PVMFFailure;
}


OSCL_EXPORT_REF PVMFNodeInterfaceImpl::~PVMFNodeInterfaceImpl()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s ~PVMFNodeInterfaceImpl() In", iNodeName.Str()));
    iSessions.clear();

    // Make sure the extension ref counter is 0
    OSCL_ASSERT(iExtensionRefCount == 0);

    //Cleanup commands
    //The command queue is self-deleting, but we want to
    //notify the observer of unprocessed commands.
    PVMFNodeCommand cmd;

    while (!iInputCommands.empty())
    {
        iInputCommands.GetFrontAndErase(cmd);
        CommandComplete(cmd, PVMFFailure);
    }

    if (IsCommandInProgress(iCancelCommand))
        CommandComplete(iCancelCommand, PVMFFailure);

    if (IsCommandInProgress(iCurrentCommand))
        CommandComplete(iCurrentCommand, PVMFFailure);

    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s ~PVMFNodeInterfaceImpl() Out", iNodeName.Str()));
    iLogger = NULL;
}

OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::ThreadLogon()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ThreadLogon() In", iNodeName.Str()));

    if (iInterfaceState != EPVMFNodeCreated)
    {
        PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ThreadLogon() Failed", iNodeName.Str()));
        return PVMFErrInvalidState;
    }

    if (!IsAdded())
    {
        AddToScheduler();
    }

    SetState(EPVMFNodeIdle);
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ThreadLogon() Out", iNodeName.Str()));
    return PVMFSuccess;
}


OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::ThreadLogoff()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ThreadLogoff() In", iNodeName.Str()));

    if (iInterfaceState != EPVMFNodeIdle)
    {
        PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ThreadLogoff() Failed", iNodeName.Str()));
        return PVMFErrInvalidState;
    }

    if (IsAdded())
    {
        RemoveFromScheduler();
    }

    SetState(EPVMFNodeCreated);
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ThreadLogoff() Out", iNodeName.Str()));
    return PVMFSuccess;
}


OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::GetCapability(PVMFNodeCapability& aNodeCapability)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::GetCapability() In", iNodeName.Str()));
    aNodeCapability = iNodeCapability;
    return PVMFSuccess;
}


OSCL_EXPORT_REF PVMFPortIter* PVMFNodeInterfaceImpl::GetPorts(const PVMFPortFilter* aFilter)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::GetPorts()", iNodeName.Str()));
    OSCL_UNUSED_ARG(aFilter);
    return NULL;
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::QueryInterface(PVMFSessionId aSessionId,
        const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::QueryInterface", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_QUERYINTERFACE, aUuid, aInterfacePtr, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::RequestPort(PVMFSessionId aSessionId,
        int32 aPortTag,
        const PvmfMimeString* aPortConfig,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::RequestPort", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_REQUESTPORT, aPortTag, aPortConfig, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::ReleasePort(PVMFSessionId aSessionId,
        PVMFPortInterface& aPort,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ReleasePort", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_RELEASEPORT, aPort, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::Init(PVMFSessionId aSessionId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Init", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_INIT, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::Prepare(PVMFSessionId aSessionId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Prepare", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_PREPARE, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::Start(PVMFSessionId aSessionId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Start", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_START, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::Stop(PVMFSessionId aSessionId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Stop", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_STOP, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::Flush(PVMFSessionId aSessionId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Flush", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_FLUSH, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::Pause(PVMFSessionId aSessionId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Pause", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_PAUSE, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::Reset(PVMFSessionId aSessionId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::Reset", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_RESET, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::CancelAllCommands(PVMFSessionId aSessionId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::CancelAllCommands", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_CANCELALLCOMMANDS, aContext);

    // Complete all the commands in the input queue here itself
    // so that the queue is available for pushing more commands.
    // Anything that is pending would be completed in DoCancelAllCommands.

    PVMFNodeCommand tempcmd;
    while (iInputCommands.size() > 0)
    {
        iInputCommands.GetFrontAndErase(tempcmd);
        CommandComplete(tempcmd, PVMFErrCancelled);
    }

    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::CancelCommand(PVMFSessionId aSessionId,
        PVMFCommandId aCmdId,
        const OsclAny* aContext)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::CancelCommand", iNodeName.Str()));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommandBase::Construct(aSessionId, PVMF_GENERIC_NODE_CANCELCOMMAND, aCmdId, aContext);
    return QueueCommandL(cmd);
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::HandlePortActivity(const PVMFPortActivity& aActivity)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::HandlePortActivity: port=0x%x, type=%d In",
                                           iNodeName.Str(), aActivity.iPort, aActivity.iType));

    /*
     * A port is reporting some activity or state change.  This code
     * figures out whether we need to queue a processing event
     * for the AO, and/or report a node event to the observer.
     */
    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_CREATED:
            ReportInfoEvent(PVMFInfoPortCreated, (OsclAny*)aActivity.iPort);
            break;
        case PVMF_PORT_ACTIVITY_DELETED:
            ReportInfoEvent(PVMFInfoPortDeleted, (OsclAny*)aActivity.iPort);
            break;
        case PVMF_PORT_ACTIVITY_CONNECT:
        case PVMF_PORT_ACTIVITY_DISCONNECT:
            break;
        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
            if (aActivity.iPort)
                OSCL_ASSERT(false);
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
            if (aActivity.iPort)
                OSCL_ASSERT(false);
            break;
        default:
            break;
    }
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::HandlePortActivity Out"));
}

OSCL_EXPORT_REF PVMFCommandId PVMFNodeInterfaceImpl::QueueCommandL(PVMFNodeCommand& aCmd)
{
    if (IsAdded())
    {
        PVMFCommandId id;
        id = iInputCommands.AddL(aCmd);
        // make sure that this command is executed in next node execution
        // lets reschedule the node here.
        Reschedule();
        return id;
    }
    OSCL_LEAVE(OsclErrInvalidState);
    return PVMF_GENERIC_NODE_COMMAND_INVALID;
}

OSCL_EXPORT_REF TPVMFNodeInterfaceState PVMFNodeInterfaceImpl::GetState()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::GetState() In", iNodeName.Str()));
    return iInterfaceState;
}

OSCL_EXPORT_REF bool PVMFNodeInterfaceImpl::IsCommandInProgress(PVMFNodeCommand& aCmd)
{
    // If the command is in progress, it would have some valid command ID
    return (PVMF_GENERIC_NODE_COMMAND_INVALID != aCmd.iCmd);
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::SetState(TPVMFNodeInterfaceState aNewState)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::SetState() In", iNodeName.Str()));
    iInterfaceState = aNewState;
    ReportInfoEvent(PVMFInfoStateChanged, (OsclAny*)aNewState);
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::ReportCmdCompleteEvent(PVMFSessionId aSession, PVMFCmdResp &aResponse)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ReportCmdCompleteEvent() CmdId %d, CmdStatus %d", iNodeName.Str(), aResponse.GetCmdId(), aResponse.GetCmdStatus()));
    for (uint32 i = 0; i < iSessions.size(); i++)
    {
        if (iSessions[i].iId == aSession)
        {
            if (iSessions[i].iInfo.iCmdStatusObserver)
                iSessions[i].iInfo.iCmdStatusObserver->NodeCommandCompleted(aResponse);
            break;
        }
    }
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::ReportErrorEvent(PVMFEventType aEventType, OsclAny* aEventData, PVInterface*aExtMsg, int32* aEventCode)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ReportErrorEvent() EventType %d", iNodeName.Str(), aEventType));
    for (uint32 i = 0; i < iSessions.size(); i++)
    {
        PVMFAsyncEvent resp(PVMFErrorEvent
                            , aEventType
                            , iSessions[i].iInfo.iErrorContext
                            , aExtMsg
                            , aEventData);
        if (iSessions[i].iInfo.iErrorObserver)
            iSessions[i].iInfo.iErrorObserver->HandleNodeErrorEvent(resp);
    }
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::ReportErrorEvent(PVMFAsyncEvent &aEvent)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ReportErrorEvent() EventType %d", iNodeName.Str(), aEvent.GetEventType()));
    for (uint32 i = 0; i < iSessions.size(); i++)
    {
        PVMFAsyncEvent resp(PVMFErrorEvent
                            , aEvent.GetEventType()
                            , iSessions[i].iInfo.iErrorContext
                            , aEvent.GetEventExtensionInterface()
                            , aEvent.GetEventData()
                            , aEvent.GetLocalBuffer()
                            , aEvent.GetLocalBufferSize());
        if (iSessions[i].iInfo.iErrorObserver)
            iSessions[i].iInfo.iErrorObserver->HandleNodeErrorEvent(resp);
    }
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::ReportInfoEvent(PVMFEventType aEventType, OsclAny* aEventData, PVInterface*aExtMsg)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ReportInfoEvent() EventType %d", iNodeName.Str(), aEventType));
    for (uint32 i = 0; i < iSessions.size(); i++)
    {
        PVMFAsyncEvent resp(PVMFInfoEvent
                            , aEventType
                            , iSessions[i].iInfo.iInfoContext
                            , aExtMsg
                            , aEventData);
        if (iSessions[i].iInfo.iInfoObserver)
            iSessions[i].iInfo.iInfoObserver->HandleNodeInformationalEvent(resp);
    }
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::ReportInfoEvent(PVMFAsyncEvent &aEvent)
{
    for (uint32 i = 0; i < iSessions.size(); i++)
    {
        PVMFAsyncEvent resp(PVMFInfoEvent
                            , aEvent.GetEventType()
                            , iSessions[i].iInfo.iInfoContext
                            , aEvent.GetEventExtensionInterface()
                            , aEvent.GetEventData()
                            , aEvent.GetLocalBuffer()
                            , aEvent.GetLocalBufferSize());
        if (iSessions[i].iInfo.iInfoObserver)
            iSessions[i].iInfo.iInfoObserver->HandleNodeInformationalEvent(resp);
    }
}

OSCL_EXPORT_REF bool PVMFNodeInterfaceImpl::SendEndOfTrackCommand(PVMFPortInterface* aPort, int32 aStreamID, PVMFTimestamp aTimestamp, uint32 aSeqNum, uint32 aClipIndex, uint32 aDuration)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::SendEndOfTrackCommand StreamID %d In ClipIndex %d", iNodeName.Str(), iStreamID, aClipIndex));

    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();
    // set format id
    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_EOS_FORMAT_ID);
    // set stream id
    sharedMediaCmdPtr->setStreamID(aStreamID);
    // Set timestamp
    sharedMediaCmdPtr->setTimestamp(aTimestamp);
    // set sequence number
    sharedMediaCmdPtr->setSeqNum(aSeqNum);
    // Set current playback clip id
    sharedMediaCmdPtr->setClipID(aClipIndex);

    //EOS timestamp(aTrackPortInfo.iTimestamp)is considered while deciding the iResumeTimeStamp in the mediaoutput node
    //therefore its length should also be considered while making decision to forward or drop the packet
    //at the mediaoutput node.
    if (PVMF_DEFAULT_TRACK_DURATION != aDuration)
    {
        sharedMediaCmdPtr->setDuration(aDuration);
    }

    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);

    if (aPort->QueueOutgoingMsg(mediaMsgOut) != PVMFSuccess)
    {
        // Output queue is busy, so wait for the output queue being ready
        // reschedule the node so that the eos is sent in next ao-cycle
        Reschedule();
        PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::SendEndOfTrackCommand (port busy) StreamID %d Failed", iNodeName.Str(), iStreamID));
        return false;
    }
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::SendEndOfTrackCommand StreamID %d Out", iNodeName.Str(), iStreamID));
    return true;
}

OSCL_EXPORT_REF bool PVMFNodeInterfaceImpl::SendBeginOfMediaStreamCommand(PVMFPortInterface* aPort, int32 aStreamID, PVMFTimestamp aTimestamp, uint32 aSeqNum, uint32 aClipIndex)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::SendBeginOfMediaStreamCommand StreamID %d ClipIndex %d In", iNodeName.Str(), iStreamID, aClipIndex));
    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();
    // set format id
    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_BOS_FORMAT_ID);
    // set timestamp
    sharedMediaCmdPtr->setTimestamp(aTimestamp);
    // set sequence number
    sharedMediaCmdPtr->setSeqNum(aSeqNum);
    // Set current playback clip id
    sharedMediaCmdPtr->setClipID(aClipIndex);

    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);
    // set stream id
    mediaMsgOut->setStreamID(aStreamID);

    if (aPort->QueueOutgoingMsg(mediaMsgOut) != PVMFSuccess)
    {
        // Output queue is busy, so wait for the output queue being ready
        // reschedule the node so that the bos is sent in next ao-cycle
        Reschedule();
        PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::SendBeginOfMediaStreamCommand (port busy) StreamID %d Failed", iNodeName.Str(), iStreamID));
        return false;
    }
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::SendBeginOfMediaStreamCommand StreamID %d Out", iNodeName.Str(), iStreamID));
    return true;
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::CommandComplete(PVMFNodeCommand& aCmd, PVMFStatus aStatus,
        PVInterface* aExtMsg, OsclAny* aEventData, PVUuid* aEventUUID, int32* aEventCode, int32 aEventDataLen)
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s:CommandComplete Id %d Cmd %d Status %d Context %d Data %d"
                                           , iNodeName.Str(), aCmd.iId, aCmd.iCmd, aStatus, aCmd.iContext, aEventData));

    PVInterface* extif = NULL;
    PVMFBasicErrorInfoMessage* errormsg = NULL;
    if (aEventUUID && aEventCode)
    {
        int32 leavecode = 0;
        OSCL_TRY(leavecode, errormsg = PVMF_BASE_NODE_NEW(PVMFBasicErrorInfoMessage, (*aEventCode, *aEventUUID, NULL)));
        if (leavecode == 0)
        {
            extif = OSCL_STATIC_CAST(PVInterface*, errormsg);
        }
    }
    else if (aExtMsg)
    {
        extif = aExtMsg;
    }

    //create response
    PVMFCmdResp resp(aCmd.iId, aCmd.iContext, aStatus, extif, aEventData);
    // set Event data lenght, if any
    if (aEventDataLen != 0)
    {
        resp.SetEventDataLen(aEventDataLen);
    }
    PVMFSessionId session = aCmd.iSession;

    if (aStatus == PVMFSuccess)
    {
        // change node's state if command was success.
        switch (aCmd.iCmd)
        {
            case PVMF_GENERIC_NODE_INIT:
                SetState(EPVMFNodeInitialized);
                break;
            case PVMF_GENERIC_NODE_PREPARE:
                SetState(EPVMFNodePrepared);
                break;
            case PVMF_GENERIC_NODE_START:
                SetState(EPVMFNodeStarted);
                break;
            case PVMF_GENERIC_NODE_STOP:
                SetState(EPVMFNodePrepared);
                break;
            case PVMF_GENERIC_NODE_PAUSE:
                SetState(EPVMFNodePaused);
                break;
            case PVMF_GENERIC_NODE_RESET:
                SetState(EPVMFNodeIdle);
                break;
            default:
                break;
        }
    }
    else
    {
        // Log that the command was failed
        PVMF_NODEINTERFACE_IMPL_INFO((0, "%s::CommandComplete cmd %d Failed with err %d", iNodeName.Str(), aCmd.iCmd, aStatus));
    }

    //Report completion to the session observer.
    ReportCmdCompleteEvent(session, resp);

    if (errormsg)
    {
        errormsg->removeRef();
    }

    //command completion reported, destroy the command.
    aCmd.Destroy();

}

OSCL_EXPORT_REF bool PVMFNodeInterfaceImpl::ProcessCommand()
{
    PVMFNodeCommand cmd = iInputCommands.front();

    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::ProcessCommand Cmd %d In", iNodeName.Str(), cmd.iCmd));

    //normally this node will not start processing one command
    //until the prior one is finished.  However, a hi priority
    //command such as Cancel must be able to interrupt a command
    //in progress.

    if (IsCommandInProgress(iCurrentCommand) && !cmd.hipri() && PVMF_GENERIC_NODE_CANCEL_GET_LICENSE != cmd.iCmd)
        return false;

    OsclAny* eventData = NULL;
    PVMFStatus status = PVMFErrInvalidState;

    // Pop the first command from the input queue to CancelCommand if its cancel or cancelall command.
    if (cmd.hipri())
    {
        iInputCommands.GetFrontAndErase(iCancelCommand);

        switch (iCancelCommand.iCmd)
        {
            case PVMF_GENERIC_NODE_CANCELALLCOMMANDS:
                status = DoCancelAllCommands();
                break;

            case PVMF_GENERIC_NODE_CANCELCOMMAND:
                status = DoCancelCommand();
                break;

            default://unknown high priority command, do an assert here
                //and complete the command with PVMFErrNotSupported
                status = PVMFErrNotSupported;
                OSCL_ASSERT(false);
                break;
        }

        if ((status != PVMFPending) && (status != PVMFCmdCompleted))
        {
            CommandComplete(iCancelCommand, status);
        }
    }
    else
    {
        // Pop the front command from InputCommand queue to CurrentCommand
        iInputCommands.GetFrontAndErase(iCurrentCommand);

        switch (iCurrentCommand.iCmd)
        {
            case PVMF_GENERIC_NODE_QUERYINTERFACE:
                status = DoQueryInterface();
                break;

            case PVMF_GENERIC_NODE_REQUESTPORT:
            {
                PVMFPortInterface* port = NULL;
                status = DoRequestPort(port);
                if (status == PVMFSuccess)
                {
                    eventData = (OsclAny*)port;
                }
            }
            break;

            case PVMF_GENERIC_NODE_RELEASEPORT:
                status = DoReleasePort();
                break;

            case PVMF_GENERIC_NODE_INIT:
            {
                if (iInterfaceState == EPVMFNodeIdle ||
                        iInterfaceState == EPVMFNodeInitialized)
                {
                    status = DoInit();
                }
            }
            break;

            case PVMF_GENERIC_NODE_PREPARE:
            {
                if (iInterfaceState == EPVMFNodeInitialized ||
                        iInterfaceState == EPVMFNodePrepared)
                {
                    status = DoPrepare();
                }
            }
            break;

            case PVMF_GENERIC_NODE_START:
            {
                if (iInterfaceState == EPVMFNodePrepared ||
                        iInterfaceState == EPVMFNodePaused ||
                        iInterfaceState == EPVMFNodeStarted)
                {
                    // if node is started, it needs to be scheduled to run
                    // so that it can start sending out/accepting data.
                    status = DoStart();
                }
            }
            break;

            case PVMF_GENERIC_NODE_STOP:
            {
                if (iInterfaceState == EPVMFNodeStarted ||
                        iInterfaceState == EPVMFNodePaused ||
                        iInterfaceState == EPVMFNodePrepared)
                {
                    status = DoStop();
                }
            }
            break;

            case PVMF_GENERIC_NODE_FLUSH:
            {
                if (iInterfaceState == EPVMFNodeStarted ||
                        iInterfaceState == EPVMFNodePaused)
                {
                    status = DoFlush();
                }
            }
            break;

            case PVMF_GENERIC_NODE_PAUSE:
            {
                if (iInterfaceState == EPVMFNodeStarted ||
                        iInterfaceState == EPVMFNodePaused)
                {
                    status = DoPause();
                }
            }
            break;

            case PVMF_GENERIC_NODE_RESET:
                status = DoReset();
                break;

            default://unknown command type, possibly and extension api command
                status = HandleExtensionAPICommands();
                break;
        }

        if ((status != PVMFPending) && (status != PVMFCmdCompleted))
        {
            CommandComplete(iCurrentCommand, status, NULL, eventData);
        }
    }
    if (status != PVMFPending)
    {
        // node needs to be rescheduled, if there's any command
        // pending to be executed
        if (iInputCommands.size() > 0)
        {
            Reschedule();
        }
    }
    return true;
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::Reschedule()
{
    if (IsAdded())
    {
        // reschedule only if it has been added to scheduler.
        RunIfNotReady();
    }
}

// Command Handler for Prepare
OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::DoPrepare()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::DoPrepare()", iNodeName.Str()));
    SetState(EPVMFNodePrepared);
    return PVMFSuccess;
}

// Command Handler for Start
OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::DoStart()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::DoStart()", iNodeName.Str()));
    SetState(EPVMFNodeStarted);
    return PVMFSuccess;
}

// Command Handler for Pause
OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::DoPause()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::DoPause()", iNodeName.Str()));
    SetState(EPVMFNodePaused);
    return PVMFSuccess;
}

// Command Handler for CancelAllCommands
OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::DoCancelAllCommands()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::DoCancelAllCommands()", iNodeName.Str()));

    // The queued commands in the input command queue were cancelled immediately after the CancelAllCommands
    // was queued. Now Cancel the current command if any.

    if (IsCommandInProgress(iCurrentCommand))
    {
        PVMFStatus status = CancelCurrentCommand();
        return status;
    }

    return PVMFSuccess;
}

// Command Handler for CancelCommand
OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::DoCancelCommand()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::DoCancelCommand()", iNodeName.Str()));

    // Extract the command ID from the parameters
    PVMFCommandId id;
    iCancelCommand.PVMFNodeCommandBase::Parse(id);

    // Check if the command to be cancelled is the current command
    if (iCurrentCommand.iId == id)
    {
        PVMFStatus status = CancelCurrentCommand();
        return status;
    }

    // Otherwise check if its in input command queue
    // Use a temporary variable cmd as iCurrentCommand may be in progress
    PVMFNodeCommand cmd;
    iInputCommands.GetCmdByIdAndErase(id, cmd);

    // If a valid command is found
    if (PVMF_GENERIC_NODE_COMMAND_INVALID != cmd.iCmd)
    {
        // Cancel the queued command
        CommandComplete(cmd, PVMFErrCancelled);
        return PVMFSuccess;
    }

    // The command not found in any queue.
    return PVMFErrArgument;
}

// Command Handler for Flush
OSCL_EXPORT_REF PVMFStatus PVMFNodeInterfaceImpl::DoFlush()
{
    PVMF_NODEINTERFACE_IMPL_LOGSTACKTRACE((0, "%s::DoFlush()", iNodeName.Str()));

    // The flush will remain pending until it completes.
    // The command is completed by the node in the subsequent Run.
    return PVMFPending;
}

OSCL_EXPORT_REF void PVMFNodeInterfaceImpl::DoCancel()
{
    OsclActiveObject::DoCancel();
}

OSCL_EXPORT_REF bool PVMFNodeInterfaceImpl::IsFlushPending()
{
    return (PVMF_GENERIC_NODE_FLUSH == iCurrentCommand.iCmd);
}
