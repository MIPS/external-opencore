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
/**
 * @file pvmi_io_interface_node.cpp
 * @brief
 */
#include "pv_media_output_node.h"

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFNodeInterface* PVMediaOutputNodeFactory::CreateMediaOutputNode(
    PvmiMIOControl* aMIOControl)
{
    return PVMediaOutputNode::Create(aMIOControl);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMediaOutputNodeFactory::DeleteMediaOutputNode(PVMFNodeInterface* aNode)
{
    PVMediaOutputNode::Release(aNode);
}

////////////////////////////////////////////////////////////////////////////
PVMFNodeInterface* PVMediaOutputNode::Create(PvmiMIOControl* aIOInterfacePtr)
{
    PVMediaOutputNode* node = OSCL_NEW(PVMediaOutputNode, ());
    if (node)
    {
        OSCL_TRAPSTACK_PUSH(node);
        node->ConstructL(aIOInterfacePtr);
        OSCL_TRAPSTACK_POP();
    }
    return (PVMFNodeInterface*)node;
}

////////////////////////////////////////////////////////////////////////////
void PVMediaOutputNode::Release(PVMFNodeInterface* aNode)
{
    OSCL_DELETE(((PVMediaOutputNode*)aNode));
}

////////////////////////////////////////////////////////////////////////////
PVMediaOutputNode::~PVMediaOutputNode()
{
    LogDiagnostics();

    Cancel();
    if (IsAdded())
        RemoveFromScheduler();

    if (iMIOControl)
    {
        //call disconnect to make sure that there are no
        //callback once the object has been destroyed
        iMIOControl->disconnect(iMIOSession);
        //ignore any returned errors.
        iMIOControl->ThreadLogoff();
    }

    //if any MIO commands are outstanding, there will be
    //a crash when they callback-- so panic here instead.
    if (IsCommandInProgress(iCancelCommand)
            || iMediaIORequest != ENone)
        OSCL_ASSERT(0);//OsclError::Panic("PVMOUT",PVMoutPanic_OutstandingMIO_Command);

    //Cleanup allocated ports
    while (!iInPortVector.empty())
        iInPortVector.Erase(&iInPortVector.front());
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::ThreadLogon()
{
    PVMFStatus status = PVMFNodeInterfaceImpl::ThreadLogon();

    if (PVMFSuccess == status)
    {
        if (iMIOControl)
        {
            iMIOControl->ThreadLogon();
            iMediaIOState = STATE_LOGGED_ON;
        }

        if (iMIOControl->connect(iMIOSession, this) != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMediaOutputNode::ThreadLogon: Error - iMIOControl->connect failed"));
            status = PVMFFailure;
        }
    }
    return status;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::ThreadLogoff()
{
    PVMFStatus status = PVMFNodeInterfaceImpl::ThreadLogoff();
    if (PVMFSuccess == status)
    {
        if (iMIOControl)
        {
            // Currently we do not distinguish between these states
            // in how we drive the MIO. In the future, we will be
            // able to independently reset/disconnect MIOs.
            //
            // The MIO reset is called here instead of the internal
            // reset because there is currently no processing there.
            // This is to reduce risk to existing MIOs.
            //
            // It can be moved to the internal node reset in the future.
            status = iMIOControl->disconnect(iMIOSession);
            //ignore any returned errors.
            iMIOControl->ThreadLogoff();
            iMediaIOState = STATE_IDLE;
        }
    }
    return status;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::GetCapability(PVMFNodeCapability& aNodeCapability)
{
    if (!iMIOConfig)
        return PVMFFailure;

    aNodeCapability.iCanSupportMultipleInputPorts = false;
    aNodeCapability.iCanSupportMultipleOutputPorts = false;
    aNodeCapability.iHasMaxNumberOfPorts = true;
    aNodeCapability.iMaxNumberOfPorts = 1;

    PvmiKvp* kvp ;
    int numParams ;
    int32 err ;
    int32 i ;
    PVMFStatus status;

    // Get input formats capability from media IO
    kvp = NULL;
    numParams = 0;
    status = iMIOConfig->getParametersSync(NULL, (char*)INPUT_FORMATS_CAP_QUERY, kvp, numParams, NULL);
    if (status == PVMFSuccess)
    {
        OSCL_TRY(err,
                 for (i = 0; i < numParams; i++)
                 aNodeCapability.iInputFormatCapability.push_back(PVMFFormatType(kvp[i].value.pChar_value));
                );
        if (kvp)
            iMIOConfig->releaseParameters(0, kvp, numParams);
    }
    //else ignore errors.

    // Get output formats capability from media IO
    kvp = NULL;
    numParams = 0;
    status = iMIOConfig->getParametersSync(NULL, (char*)OUTPUT_FORMATS_CAP_QUERY, kvp, numParams, NULL);
    if (status == PVMFSuccess)
    {
        OSCL_TRY(err,
                 for (i = 0; i < numParams; i++)
                 aNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(kvp[i].value.pChar_value));
                );
        if (kvp)
            iMIOConfig->releaseParameters(0, kvp, numParams);
    }
    //else ignore errors.

    if (aNodeCapability.iInputFormatCapability.empty() && aNodeCapability.iOutputFormatCapability.empty())
        return PVMFFailure;

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFPortIter* PVMediaOutputNode::GetPorts(const PVMFPortFilter* aFilter)
{
    OSCL_UNUSED_ARG(aFilter);//port filter is not implemented.
    iInPortVector.Reset();
    return &iInPortVector;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMediaOutputNode::addRef()
{
    ++iExtensionRefCount;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMediaOutputNode::removeRef()
{
    if (iExtensionRefCount > 0)
        --iExtensionRefCount;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMediaOutputNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    if (uuid == PvmfNodesSyncControlUuid)
    {
        PvmfNodesSyncControlInterface* myInterface = OSCL_STATIC_CAST(PvmfNodesSyncControlInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else
    {
        iface = NULL;
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::SetClock(PVMFMediaClock* aClock)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::SetClock: aClock=0x%x", aClock));

    //remove any old clock
    if (iClock)
    {
        if (iMIOClockExtension)
            iMIOClockExtension->SetClock(NULL);
        for (uint32 i = 0; i < iInPortVector.size(); i++)
            iInPortVector[i]->SetClock(NULL);
    }

    iClock = aClock;
    for (uint32 i = 0; i < iInPortVector.size(); i++)
    {
        iInPortVector[i]->SetClock(aClock);
        iInPortVector[i]->ChangeClockRate(iClockRate);
    }

    //pass the clock to the optional MIO clock interface
    if (iMIOClockExtension)
    {
        iMIOClockExtension->SetClock(aClock);
    }
    else
    {
#if (PVMF_MEDIA_OUTPUT_NODE_ENABLE_AV_SYNC)
        //turn on sync params
        for (uint32 i = 0; i < iInPortVector.size(); i++)
        {
            iInPortVector[i]->EnableMediaSync();
            iInPortVector[i]->SetMargins(iEarlyMargin, iLateMargin);
        }
#endif
    }
    return PVMFSuccess;
}

///////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::ChangeClockRate(int32 aRate)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::ChangeClockRate: aRate=%d", aRate));

    iClockRate = aRate;
    for (uint32 i = 0; i < iInPortVector.size() ; i++)
        iInPortVector[i]->ChangeClockRate(aRate);

    // For now support all rates.
    // In future, need to check with underlying media IO component

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::SetMargins(int32 aEarlyMargin, int32 aLateMargin)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::SetMargins: aEarlyMargin=%d, aLateMargin=%d", aEarlyMargin, aLateMargin));

    //save the margins
    iEarlyMargin = aEarlyMargin;
    iLateMargin = aLateMargin;

    //pass the margins to the ports
    for (uint32 i = 0; i < iInPortVector.size() ; i++)
        iInPortVector[i]->SetMargins(aEarlyMargin, aLateMargin);

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMediaOutputNode::ClockStarted()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::ClockStarted"));

    //notify the ports
    for (uint32 i = 0; i < iInPortVector.size() ; i++)
        iInPortVector[i]->ClockStarted();

}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMediaOutputNode::ClockStopped()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::ClockStopped"));

    //notify the ports
    for (uint32 i = 0; i < iInPortVector.size() ; i++)
        iInPortVector[i]->ClockStopped();
}


////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId PVMediaOutputNode::SkipMediaData(PVMFSessionId s,
        PVMFTimestamp aResumeTimestamp,
        uint32 aStreamID,
        bool aPlayBackPositionContinuous,
        OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::SkipMediaData() called "));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_INFO,
                    (0, "PVMediaOutputNode::SkipMediaData() called - Mime=%s", iSinkFormatString.get_str()));
    PVMFNodeCommand cmd;
    cmd.Construct(s,
                  PVMF_MEDIAOUTPUTNODE_SKIPMEDIADATA,
                  aResumeTimestamp,
                  aStreamID,
                  aPlayBackPositionContinuous,
                  aContext);
    return QueueCommandL(cmd);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMediaOutputNode::RequestCompleted(const PVMFCmdResp& aResponse)
//callback from the MIO module.
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::RequestCompleted: Cmd ID=%d", aResponse.GetCmdId()));

    //look for cancel completion.
    if (iMediaIOCancelPending
            && aResponse.GetCmdId() == iMediaIOCancelCmdId)
    {
        iMediaIOCancelPending = false;

        OSCL_ASSERT(IsCommandInProgress(iCancelCommand));

        //Current cancel command is now complete.
        CommandComplete(iCancelCommand, PVMFSuccess);
    }
    //look for non-cancel completion
    else if (iMediaIORequest != ENone
             && aResponse.GetCmdId() == iMediaIOCmdId)
    {
        OSCL_ASSERT(IsCommandInProgress(iCurrentCommand));

        switch (iMediaIORequest)
        {
            case EQueryCapability:
                iMIOConfig = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, iMIOConfigPVI);
                iMIOConfigPVI = NULL;
                if (aResponse.GetCmdStatus() != PVMFSuccess)
                    iEventCode = PVMFMoutNodeErr_MediaIOQueryCapConfigInterface;
                CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
                break;

            case EQueryClockExtension:
                //ignore any error from this query since the interface is optional.
                iMediaIORequest = ENone;
                iMIOClockExtension = OSCL_STATIC_CAST(PvmiClockExtensionInterface*, iMIOClockExtensionPVI);
                iMIOClockExtensionPVI = NULL;
                //re-do the clock setting call since iMIOClockExtension may have changed.
                if (aResponse.GetCmdStatus() == PVMFSuccess
                        && iMIOClockExtension)
                {
                    SetClock(iClock);
                }
                //To continue the Node Init, query for the
                //capability & config interface
                {
                    PVMFStatus status = SendMioRequest(EQueryCapability);
                    if (status == PVMFPending)
                        return;//wait on response
                    else
                        CommandComplete(iCurrentCommand, status);
                }
                break;

            case EInit:
                if (aResponse.GetCmdStatus() != PVMFSuccess)
                    iEventCode = PVMFMoutNodeErr_MediaIOInit;
                else
                    iMediaIOState = STATE_INITIALIZED;
                CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
                break;

            case EStart:
                if (aResponse.GetCmdStatus() != PVMFSuccess)
                    iEventCode = PVMFMoutNodeErr_MediaIOStart;
                else
                    iMediaIOState = STATE_STARTED;
                CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
                break;

            case EPause:
                if (aResponse.GetCmdStatus() != PVMFSuccess)
                    iEventCode = PVMFMoutNodeErr_MediaIOPause;
                else
                    iMediaIOState = STATE_PAUSED;
                CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
                break;

            case EStop:
                if (aResponse.GetCmdStatus() != PVMFSuccess)
                    iEventCode = PVMFMoutNodeErr_MediaIOStop;
                else
                    iMediaIOState = STATE_INITIALIZED;
                CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
                break;

            case EDiscard:
            {
                if (aResponse.GetCmdStatus() != PVMFSuccess)
                {
                    iEventCode = PVMFMoutNodeErr_MediaIODiscardData;
                    CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
                }
                else
                {
                    iMediaIORequest = ENone;
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_INFO,
                                    (0, "PVMediaOutputNode::ResuestCompleted - EDiscard success - Mime=%s",
                                     iSinkFormatString.get_str()));
                    //attempt to complete skip media data
                    CompleteSkipMediaData();
                }
            }
            break;

            case EReset:
            {
                if (aResponse.GetCmdStatus() != PVMFSuccess)
                {
                    iEventCode = PVMFMoutNodeErr_MediaIOReset;
                }
                else
                {
                    iMediaIOState = STATE_LOGGED_ON;
                }
                CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
            }
            break;

            default:
                OSCL_ASSERT(false);
                CommandComplete(iCurrentCommand, PVMFFailure);
                break;
        }

    }
    else
    {
        //unexpected response.
        LOGERROR((0, "PVMediaOutputNode:RequestComplete Warning! Unexpected command ID %d"
                  , aResponse.GetCmdId()));
    }
}

OSCL_EXPORT_REF void PVMediaOutputNode::ReportErrorEvent(PVMFEventType aEventType, PVInterface* aExtMsg)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::ReportErrorEvent Type %d", aEventType));
    PVMFNodeInterface::ReportErrorEvent(aEventType, NULL, aExtMsg);
}
OSCL_EXPORT_REF void PVMediaOutputNode::ReportInfoEvent(PVMFEventType aEventType, PVInterface* aExtMsg)
{
    OSCL_UNUSED_ARG(aExtMsg);
    if (PVMFMIOConfigurationComplete == aEventType)
    {
        LOGINFO((0, "PVMediaOutputNode::ReportInfoEvent PVMFMIOConfigurationComplete received"));
        for (uint32 i = 0; i < iInPortVector.size(); i++)
        {
            iInPortVector[i]->SetMIOComponentConfigStatus(true);
        }
    }
}

////////////////////////////////////////////////////////////////////////////
PVMediaOutputNode::PVMediaOutputNode()
        : PVMFNodeInterfaceImpl(OsclActiveObject::EPriorityNominal, "PVMediaOutputNode")
        , iEventUuid(PVMFMediaOutputNodeEventTypesUUID)
        , iMIOControl(NULL)
        , iMIOSession(NULL)
        , iMIOConfig(NULL)
        , iMIOConfigPVI(NULL)
        , iMediaIOState(STATE_IDLE)
        , iClock(NULL)
        , iEarlyMargin(DEFAULT_EARLY_MARGIN)
        , iLateMargin(DEFAULT_LATE_MARGIN)
        , iDiagnosticsLogger(NULL)
        , iDiagnosticsLogged(false)
        , iReposLogger(NULL)
        , iRecentBOSStreamID(0)
{
}

////////////////////////////////////////////////////////////////////////////
void PVMediaOutputNode::ConstructL(PvmiMIOControl* aIOInterfacePtr)
{
    iMIOControl = aIOInterfacePtr;
    iInPortVector.Construct(0);//reserve zero
    iMediaIORequest = ENone;
    iMediaIOCancelPending = false;
    iMIOClockExtension = NULL;
    iMIOClockExtensionPVI = NULL;
    iClockRate = 100000;

    iReposLogger = PVLogger::GetLoggerObject("pvplayerrepos.mionode");
    iDiagnosticsLogger = PVLogger::GetLoggerObject("pvplayerdiagnostics.mionode");

}

//The various command handlers call this when a command is complete.
void PVMediaOutputNode::CommandComplete(PVMFNodeCommand& aCmd, PVMFStatus aStatus, OsclAny*aEventData)
{
    if (aStatus == PVMFSuccess || aCmd.iCmd == PVMF_GENERIC_NODE_QUERYINTERFACE) //(mg) treat QueryIF failures as info, not errors
    {
        LOGINFO((0, "PVMediaOutputNode:CommandComplete Id %d Cmd %d Status %d Context %d EVData %d EVCode %d"
                 , aCmd.iId, aCmd.iCmd, aStatus, aCmd.iContext, aEventData, iEventCode));
    }
    else
    {
        LOGERROR((0, "PVMediaOutputNode:CommandComplete Error! Id %d Cmd %d Status %d Context %d EVData %d EVCode %d"
                  , aCmd.iId, aCmd.iCmd, aStatus, aCmd.iContext, aEventData, iEventCode));
    }

    // Perform additional functionality
    if (aStatus == PVMFSuccess)
    {
        switch (aCmd.iCmd)
        {
            case PVMF_GENERIC_NODE_START:
            {
                for (uint32 i = 0; i < iInPortVector.size(); i++)
                    iInPortVector[i]->NodeStarted();
            }
            break;
            case PVMF_GENERIC_NODE_FLUSH:
                SetState(EPVMFNodePrepared);
                //resume port input so the ports can be re-started.
                {
                    for (uint32 i = 0; i < iInPortVector.size(); i++)
                        iInPortVector[i]->ResumeInput();
                }
                break;
            default:
                break;
        }
    }

    //Reset the media I/O request
    iMediaIORequest = ENone;

    {
        PVMFStatus eventCode = iEventCode;

        //Reset the event code after using it.
        iEventCode = PVMFMoutNodeErr_First;

        // Now call BaseNode CommandComplete
        if (PVMFMoutNodeErr_First != eventCode)
        {
            PVMFNodeInterfaceImpl::CommandComplete(aCmd, aStatus, NULL, aEventData, &iEventUuid, &eventCode);
        }
        else
        {
            PVMFNodeInterfaceImpl::CommandComplete(aCmd, aStatus, NULL, aEventData);
        }
    }

    // Reschedule if there are more commands and node isn't logged off
    if (!iInputCommands.empty())
        Reschedule();
}


////////////////////////////////////////////////////////////////////////////
void PVMediaOutputNode::Run()
{
    //Process async node commands.
    if (!iInputCommands.empty())
    {
        ProcessCommand();
    }

    //Check for completion of a flush command...
    if (PVMF_GENERIC_NODE_FLUSH == iCurrentCommand.iCmd
            && PortQueuesEmpty())
    {
        //Flush is complete.
        CommandComplete(iCurrentCommand, PVMFSuccess);
    }
}

bool PVMediaOutputNode::PortQueuesEmpty()
{
    uint32 i;
    for (i = 0; i < iInPortVector.size(); i++)
    {
        if (iInPortVector[i]->IncomingMsgQueueSize() > 0 ||
                iInPortVector[i]->OutgoingMsgQueueSize())
        {
            return false;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoQueryInterface()
{
    PVUuid* uuid;
    PVInterface** ptr;
    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, ptr);
    if (uuid && ptr)
    {
        if (queryInterface(*uuid, *ptr))
        {
            (*ptr)->addRef();
            return PVMFSuccess;
        }
    }
    // Interface not supported
    *ptr = NULL;
    return PVMFErrNotSupported;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::DoRequestPort"));
    //This node supports port request from any state

    //retrieve port tag & mimetype
    int32 tag;
    OSCL_String* mimetype;
    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, mimetype);

    switch (tag)
    {
        case PVMF_MEDIAIO_NODE_INPUT_PORT_TAG:
        {
            //Allocate a new port
            OsclAny *ptr = NULL;
            int32 err;
            OSCL_TRY(err, ptr = iInPortVector.Allocate(););
            if (err != OsclErrNone || !ptr)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::DoRequestPort: Error - iInPortVector Out of memory"));
                return PVMFErrNoMemory;
            }
            PVMediaOutputNodePort *port = OSCL_PLACEMENT_NEW(ptr, PVMediaOutputNodePort(this));

            //Add the port to the port vector.
            OSCL_TRY(err, iInPortVector.AddL(port););
            if (err != OsclErrNone)
            {
                iInPortVector.DestructAndDealloc(port);
                return PVMFErrNoMemory;
            }

            //set the format from the mimestring, if provided
            if (mimetype)
            {
                PVMFStatus status = port->Configure(*mimetype);
                if (status != PVMFSuccess)
                {
                    //bad format!
                    iInPortVector.Erase(&port);
                    iInPortVector.DestructAndDealloc(port);
                    return PVMFErrArgument;
                }
            }

            iSinkFormatString = *mimetype;

            //pass the current clock settings to the port.
            SetClock(iClock);

            //cast the port to the generic interface before returning.
            aPort = port;
            return PVMFSuccess;
        }
        break;

        default:
        {
            //bad port tag
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMediaOutputNode::DoRequestPort: Error - Invalid port tag"));
            return PVMFFailure;
        }
        break;
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoInit()
{

    if (EPVMFNodeInitialized == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMediaOutputNode::DoInit() already in Initialized state"));
        return PVMFSuccess;
    }

    if (!iMIOControl)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMediaOutputNode::DoInit: Error - iMIOControl is NULL"));
        iEventCode = PVMFMoutNodeErr_MediaIONotExist;
        return PVMFFailure;
    }

    //Query for MIO interfaces.
    return SendMioRequest(EQueryClockExtension);
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoPrepare()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::DoPrepare"));

    if (EPVMFNodePrepared == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMediaOutputNode::DoPrepare() already in Prepared state"));
        return PVMFSuccess;
    }

    return SendMioRequest(EInit);
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoStart()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::DoStart"));

    if (EPVMFNodeStarted == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMediaOutputNode::DoStart() already in Start state"));
        return PVMFSuccess;
    }

    iDiagnosticsLogged = false;
    iInPortVector[0]->iFramesDropped = 0;
    iInPortVector[0]->iTotalFrames = 0;

    iInPortVector[0]->ResetConsecutiveFramesDropped();

    //Start the MIO

    return SendMioRequest(EStart);
}


////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoStop()
{
    //clear the message queues of any unprocessed data now.
    uint32 i;
    for (i = 0; i < iInPortVector.size(); i++)
    {
        iInPortVector[i]->ClearMsgQueues();
    }

    LogDiagnostics();

    if (iMediaIOState == STATE_STARTED || iMediaIOState == STATE_PAUSED)
    {
        //Stop the MIO component only if MIOs are in Paused or Started states.
        return SendMioRequest(EStop);
    }
    else
    {
        // no stop needed if MIOs are not in started or paused states. return success.
        return PVMFSuccess;
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoFlush()
{
    //Disable the input.
    if (iInPortVector.size() > 0)
    {
        for (uint32 i = 0; i < iInPortVector.size(); i++)
            iInPortVector[i]->SuspendInput();
    }

    //wait for all data to be consumed
    return PVMFPending;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoPause()
{
    if (EPVMFNodePaused == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMediaOutputNode::DoPause() already in Paused state"));
        return PVMFSuccess;
    }

    return SendMioRequest(EPause);
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoSkipMediaData()
{
    //Here is what we do during skip:
    //1) We pass the skiptimestamp to ports. This is to make sure that
    //they can start tossing out data ASAP
    //2) We send DiscardData to media output components. Failure to
    //queue discarddata on mediaoutput comps is considered fatal failure
    //and will result in skip failure on media output node
    //3) Then we wait for all discard data to complete and all ports to
    //report BOS. Ports call "ReportBOS" when they recv BOS.

    PVMFTimestamp resumeTimestamp;
    bool playbackpositioncontinuous;
    uint32 streamID;
    iCurrentCommand.Parse(resumeTimestamp,
                          playbackpositioncontinuous,
                          streamID);
    iRecentBOSStreamID = streamID;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_INFO,
                    (0, "PVMediaOutputNode::DoSkipMediaData - Mime=%s, SkipTS=%d, StreamID=%d, SFR=%d",
                     iSinkFormatString.get_str(),
                     resumeTimestamp,
                     iRecentBOSStreamID,
                     playbackpositioncontinuous));

    //although we treat inport as a vector, we still
    //assume just one media output comp per node
    if (iInPortVector.size() > 0)
    {
        iInPortVector[0]->SetSkipTimeStamp(resumeTimestamp, iRecentBOSStreamID);
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_ERR,
                        (0, "PVMediaOutputNode::DoSkipMediaData - No Input Ports - Mime=%s", iSinkFormatString.get_str()));
        return PVMFErrInvalidState;
    }

    PVMFStatus status = PVMFFailure;
    if (playbackpositioncontinuous == true)
    {
        //source node has not be involved in determining this skip
        //boundaries. in this case if an EOS has been sent to the mio
        //component, then do not call discard data
        //assume that there is only one port per mio node and one
        //mio comp per node.
        //also do NOT ever call discard data in case of compressed mio
        if (iInPortVector[0]->isUnCompressedMIO == true)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_INFO,
                            (0, "PVMediaOutputNode::DoSkipMediaData - SFR - Calling DiscardData - Mime=%s", iSinkFormatString.get_str()));
            status = SendMioRequest(EDiscard);
            if (status != PVMFPending)
            {
                iMediaIORequest = ENone;
                return status;
            }
        }
        else
        {
            // For Compressed MIO, we need not call discard data and should report Skip Media Complete
            // right away.
            return PVMFSuccess;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_INFO,
                        (0, "PVMediaOutputNode::DoSkipMediaData - Calling DiscardData - Mime=%s", iSinkFormatString.get_str()));
        status = SendMioRequest(EDiscard);
        if (status != PVMFPending)
        {
            iMediaIORequest = ENone;
            return status;
        }
    }
    //wait on discard
    return status;
}

void PVMediaOutputNode::CompleteSkipMediaData()
{
    if (PVMF_MEDIAOUTPUTNODE_SKIPMEDIADATA == iCurrentCommand.iCmd)
    {
        if (iMediaIORequest == ENone)
        {
            //implies that discard is complete
            if (CheckForBOS() == PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_INFO,
                                (0, "PVMediaOutputNode::CompleteSkipMediaData - SkipComplete - Mime=%s", iSinkFormatString.get_str()));
                CommandComplete(iCurrentCommand, PVMFSuccess);
                for (uint32 i = 0; i < iInPortVector.size(); i++)
                {
                    //clear out the bos stream id vec on all ports so that they is no confusion later on
                    iInPortVector[i]->ClearPreviousBOSStreamIDs(iRecentBOSStreamID);
                }
                return;
            }
        }
        //else continue waiting on the discard to complete.
    }
    else
    {
        //this means that either skipmediadata cmd is yet to be issued or processed
        //this should be a problem since ports record the BOS stream ids which would
        //be checked later
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_ERR,
                        (0, "PVMediaOutputNode::CompleteSkipMediaData - Waiting On SkipCmd To Be Issued/Processed - Mime=%s", iSinkFormatString.get_str()));
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMediaOutputNode::CompleteSkipMediaData - Waiting On SkipCmd To Be Issued/Processed - Mime=%s", iSinkFormatString.get_str()));
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoReleasePort()
{
    //This node supports release port from any state
    PVMFPortInterface* p;
    iCurrentCommand.PVMFNodeCommandBase::Parse(p);
    //search the input port vector
    {
        PVMediaOutputNodePort* port = (PVMediaOutputNodePort*)p;
        PVMediaOutputNodePort** portPtr = iInPortVector.FindByValue(port);
        if (portPtr)
        {
            (*portPtr)->Disconnect();
            iInPortVector.Erase(portPtr);
            return PVMFSuccess;
        }
    }
    iEventCode = PVMFMoutNodeErr_PortNotExist;
    return PVMFFailure;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::DoReset()
{

    LogDiagnostics();

    // delete all ports and notify observer.
    while (!iInPortVector.empty())
    {
        iInPortVector.front()->Disconnect();
        iInPortVector.Erase(&iInPortVector.front());
    }

    // restore original port vector reserve.
    iInPortVector.Reconstruct();

    if ((iInterfaceState == EPVMFNodeIdle) ||
            (iInterfaceState == EPVMFNodeCreated))
    {
        // node is either in Created or Idle state, no need to call Reset on MIO. MIO has not been
        // connected yet so no need to send a asynchronous command to MIO.
        return PVMFSuccess;
    }
    else
    {
        return SendMioRequest(EReset);
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::CancelMioRequest()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::CancelMioRequest In"));

    OSCL_ASSERT(iMediaIORequest != ENone);

    OSCL_ASSERT(!iMediaIOCancelPending);

    //Issue the cancel to the MIO.
    iMediaIOCancelPending = true;
    int32 err;
    OSCL_TRY(err, iMediaIOCancelCmdId = iMIOControl->CancelCommand(iMediaIOCmdId););
    if (err != OsclErrNone)
    {
        iEventCode = PVMFMoutNodeErr_MediaIOCancelCommand;
        iMediaIOCancelPending = false;
        LOGINFOHI((0, "PVMediaOutputNode::CancelMioRequest: CancelCommand on MIO failed"));
        return PVMFFailure;
    }
    LOGINFOHI((0, "PVMediaOutputNode::CancelMioRequest: Cancel Command Issued to MIO component, waiting on response..."));
    return PVMFPending;//wait on request to cancel.
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMediaOutputNode::SendMioRequest(EMioRequest aRequest)
{
    //Make an asynchronous request to MIO component
    //and save the request in iMediaIORequest.


    //there should not be a MIO command in progress..
    OSCL_ASSERT(iMediaIORequest == ENone);


    //save media io request.
    iMediaIORequest = aRequest;

    PVMFStatus status = PVMFPending;
    int32 err = OsclErrGeneral;

    //Issue the command to the MIO.
    switch (aRequest)
    {
        case EQueryCapability:
        {
            iMIOConfigPVI = NULL;
            OSCL_TRY(err,
                     iMediaIOCmdId = iMIOControl->QueryInterface(PVMI_CAPABILITY_AND_CONFIG_PVUUID,
                                     iMIOConfigPVI, NULL);
                    );

            if (err != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::SendMioRequest: Error - iMIOControl->QueryInterface(cap & config) failed"));
                iEventCode = PVMFMoutNodeErr_MediaIOQueryCapConfigInterface;
                status = PVMFFailure;
            }
        }
        break;

        case EQueryClockExtension:
        {
            iMIOClockExtensionPVI = NULL;
            OSCL_TRY(err,
                     iMediaIOCmdId = iMIOControl->QueryInterface(PvmiClockExtensionInterfaceUuid,
                                     iMIOClockExtensionPVI, NULL);
                    );

            if (err != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::SendMioRequest: Error iMIOControl->QueryInterface(clock ext) failed"));
                //this interface is optional so ignore the error
                status = PVMFSuccess;
            }
        }
        break;

        case EInit:
        {
            PvmiMediaTransfer* mediaTransfer = NULL;
            if (iInPortVector.size() > 0)
            {
                mediaTransfer = iInPortVector[0]->getMediaTransfer();
            }
            if (mediaTransfer != NULL)
            {
                OSCL_TRY(err, iMediaIOCmdId = iMIOControl->Init(););
            }
            if ((err != OsclErrNone))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::SendMioRequest: Error - iMIOControl->Init failed"));
                iEventCode = PVMFMoutNodeErr_MediaIOInit;
                status = PVMFFailure;
            }
        }
        break;

        case EStart:
        {
            PvmiMediaTransfer* mediaTransfer = NULL;
            if (iInPortVector.size() > 0)
            {
                mediaTransfer = iInPortVector[0]->getMediaTransfer();
            }
            if (mediaTransfer != NULL)
            {
                OSCL_TRY(err, iMediaIOCmdId = iMIOControl->Start(););
            }
            if (err != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::SendMioRequest: Error - iMIOControl->Start failed"));
                iEventCode = PVMFMoutNodeErr_MediaIOStart;
                status = PVMFFailure;
            }
        }
        break;

        case EPause:
        {
            PvmiMediaTransfer* mediaTransfer = NULL;
            if (iInPortVector.size() > 0)
            {
                mediaTransfer = iInPortVector[0]->getMediaTransfer();
            }
            if (mediaTransfer != NULL)
            {
                OSCL_TRY(err, iMediaIOCmdId = iMIOControl->Pause(););
            }
            if (err != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::SendMioRequest: Error - iMIOControl->Pause failed"));
                iEventCode = PVMFMoutNodeErr_MediaIOPause;
                status = PVMFFailure;
            }
        }
        break;

        case EStop:
        {
            PvmiMediaTransfer* mediaTransfer = NULL;
            if (iInPortVector.size() > 0)
            {
                mediaTransfer = iInPortVector[0]->getMediaTransfer();
            }
            else
            {
                /*There can be cases where stop is called after ports have been released and in such cases
                we succeed stop as no-op assuming a subsequent reset will be called which would
                guarantee proper reset of mio comp. Reset the media I/O request and return Success */
                iMediaIORequest = ENone;
                return PVMFSuccess;
            }
            if (mediaTransfer != NULL)
            {
                OSCL_TRY(err, iMediaIOCmdId = iMIOControl->Stop(););
            }
            if (err != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::SendMioRequest: Error - iMIOControl->Stop failed"));
                iEventCode = PVMFMoutNodeErr_MediaIOStop;
                status = PVMFFailure;
            }
        }
        break;

        case EDiscard:
        {
            PvmiMediaTransfer* mediaTransfer = NULL;
            if (iInPortVector.size() > 0)
            {
                mediaTransfer = iInPortVector[0]->getMediaTransfer();
            }
            if (mediaTransfer != NULL)
            {
                PVMFTimestamp resumeTimestamp;
                bool playbackpositioncontinuous;
                uint32 streamId;
                iCurrentCommand.Parse(resumeTimestamp, playbackpositioncontinuous, streamId);
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_INFO,
                                (0, "PVMediaOutputNode::SendMioRequest(EDiscard): skipTimestamp=%d", resumeTimestamp));
                OSCL_TRY(err, iMediaIOCmdId = iMIOControl->DiscardData(resumeTimestamp););
            }
            if (err != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::SendMioRequest: Error - iMIOControl->DiscardData failed"));
                iEventCode = PVMFMoutNodeErr_MediaIODiscardData;
                status = PVMFFailure;
            }
        }
        break;

        case EReset:
        {
            OSCL_TRY(err, iMediaIOCmdId = iMIOControl->Reset(););
            if (err != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMediaOutputNode::SendMioRequest: Error - iMIOControl->Reset failed"));
                iEventCode = PVMFMoutNodeErr_MediaIOReset;
                status = PVMFFailure;
            }
        }
        break;

        default:
            OSCL_ASSERT(false);//unrecognized command.
            status = PVMFFailure;
            break;
    }

    if (status == PVMFPending)
    {
        LOGINFOHI((0, "PVMediaOutputNode:SendMIORequest: Command Issued to MIO component, waiting on response..."));
    }

    return status;
}




/////////////////////////////////////////////////////
// Event reporting routines.
/////////////////////////////////////////////////////

void PVMediaOutputNode::ReportErrorEvent(PVMFEventType aEventType, OsclAny* aEventData, PVMFStatus aEventCode)
{
    LOGERROR((0, "PVMediaOutputNode:NodeErrorEvent Type %d EVData %d EVCode %d"
              , aEventType, aEventData, aEventCode));

    //create the extension message if any.
    if (aEventCode != PVMFMoutNodeErr_First)
    {
        PVMFBasicErrorInfoMessage* eventmsg = OSCL_NEW(PVMFBasicErrorInfoMessage, (aEventCode, iEventUuid, NULL));
        PVMFAsyncEvent asyncevent(PVMFErrorEvent, aEventType, NULL, OSCL_STATIC_CAST(PVInterface*, eventmsg), aEventData, NULL, 0);
        PVMFNodeInterfaceImpl::ReportErrorEvent(asyncevent);
        eventmsg->removeRef();
    }
    else
        PVMFNodeInterfaceImpl::ReportErrorEvent(aEventType, aEventData);
}

void PVMediaOutputNode::ReportInfoEvent(PVMFEventType aEventType, OsclAny* aEventData, PVMFStatus aEventCode)
{
    LOGINFO((0, "PVMediaOutputNode:NodeInfoEvent Type %d EVData %d EVCode %d"
             , aEventType, aEventData, aEventCode));

    //create the extension message if any.
    if (aEventCode != PVMFMoutNodeErr_First)
    {
        PVMFBasicErrorInfoMessage* eventmsg = OSCL_NEW(PVMFBasicErrorInfoMessage, (aEventCode, iEventUuid, NULL));
        PVMFAsyncEvent asyncevent(PVMFErrorEvent, aEventType, NULL, OSCL_STATIC_CAST(PVInterface*, eventmsg), aEventData, NULL, 0);
        PVMFNodeInterfaceImpl::ReportInfoEvent(asyncevent);
        eventmsg->removeRef();
    }
    else
        PVMFNodeInterfaceImpl::ReportInfoEvent(aEventType, aEventData);
}

OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements)
{
    OSCL_UNUSED_ARG(aSession);
    if (iMIOConfig)
    {
        return (iMIOConfig->verifyParametersSync(iMIOSession, aParameters, num_elements));
    }
    else
    {
        return PVMFFailure;
    }
}

OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::getParametersSync(PvmiMIOSession aSession,
        PvmiKeyType aIdentifier,
        PvmiKvp*& aParameters,
        int& aNumParamElements,
        PvmiCapabilityContext aContext)
{
    OSCL_UNUSED_ARG(aSession);
    return (iMIOConfig->getParametersSync(iMIOSession, aIdentifier, aParameters, aNumParamElements, aContext));
}

OSCL_EXPORT_REF PVMFStatus PVMediaOutputNode::releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements)
{
    OSCL_UNUSED_ARG(aSession);
    return (iMIOConfig->releaseParameters(iMIOSession, aParameters, num_elements));
}

void PVMediaOutputNode::ReportBOS()
{
    CompleteSkipMediaData();
}

PVMFStatus PVMediaOutputNode::CheckForBOS()
{
    Oscl_Vector<uint32, OsclMemAllocator>::iterator it;
    for (it = iInPortVector[0]->iBOSStreamIDVec.begin();
            it != iInPortVector[0]->iBOSStreamIDVec.end();
            it++)
    {
        if (*it == iRecentBOSStreamID)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iReposLogger, PVLOGMSG_INFO,
                            (0, "PVMediaOutputNode::CheckForBOS - BOS Found - Mime=%s, BOSStreamID=%d",
                             iSinkFormatString.get_str(), iRecentBOSStreamID));
            //we have recvd BOS
            return PVMFSuccess;
        }
    }
    return PVMFPending;
}

void PVMediaOutputNode::LogDiagnostics()
{
    if (iDiagnosticsLogged == false)
    {
        iDiagnosticsLogged = true;

        if (!iInPortVector.empty())
        {
            LOGDIAGNOSTICS((0, "PVMediaOutputNode:LogDiagnostics Mime %s, FramesDropped/TotalFrames %d/%d"
                            , iSinkFormatString.get_str(), iInPortVector[0]->iFramesDropped, iInPortVector[0]->iTotalFrames));
        }
    }
}

PVMFStatus PVMediaOutputNode::HandleExtensionAPICommands()
{
    PVMFStatus status = PVMFFailure;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMediaOutputNode::HandleExtensionAPICommands - command=%d", iCurrentCommand.iCmd));

    switch (iCurrentCommand.iCmd)
    {

        case PVMF_MEDIAOUTPUTNODE_SKIPMEDIADATA:
            status = DoSkipMediaData();
            break;
        default:
            //Do an assert as there shouldn't be any unidentified command
            status = PVMFErrNotSupported;
            OSCL_ASSERT(false);
            break;
    }

    return status;
}

PVMFStatus PVMediaOutputNode::CancelCurrentCommand()
{

    //cancel any SkipMediaData in progress on the ports.
    if (iCurrentCommand.iCmd == PVMF_MEDIAOUTPUTNODE_SKIPMEDIADATA)
    {
        for (uint32 i = 0; i < iInPortVector.size(); i++)
        {
            iInPortVector[i]->CancelSkip();
        }
    }

    //Check if MIO request needs to be cancelled
    bool pendingmiocancel = false;
    if (iMediaIORequest != ENone)
    {
        if (PVMFPending == CancelMioRequest())
        {
            pendingmiocancel = true;
        }
    }

    if (pendingmiocancel)
    {
        // Wait for MIO cancel to complete before completing CancelCommand
        return PVMFPending;
    }
    else
    {
        //report report success
        return PVMFSuccess;
    }

    // Cancel DoFlush here and return success.
    if (IsFlushPending())
    {
        CommandComplete(iCurrentCommand, PVMFErrCancelled);
    }

    return PVMFSuccess;
}
