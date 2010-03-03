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
#include "pvmf_omx_basedec_node.h"

#define CONFIG_SIZE_AND_VERSION(param) \
        param.nSize=sizeof(param); \
        param.nVersion.s.nVersionMajor = SPECVERSIONMAJOR; \
        param.nVersion.s.nVersionMinor = SPECVERSIONMINOR; \
        param.nVersion.s.nRevision = SPECREVISION; \
        param.nVersion.s.nStep = SPECSTEP;

#define PVOMXBASEDEC_MEDIADATA_CHUNKSIZE 128


// OMX CALLBACKS
// 1) AO OMX component running in the same thread as the OMX node
//  In this case, the callbacks can be called directly from the component
//  The callback: OMX Component->CallbackEventHandler->EventHandlerProcessing
//  The callback can perform do Rechedule

// 2) Multithreaded component
//  In this case, the callback is made using the threadsafe callback (TSCB) AO
//  Component thread : OMX Component->CallbackEventHandler->TSCB(ReceiveEvent) => event is queued
//  Node thread      : dequeue event => TSCB(ProcessEvent)->ProcessCallbackEventHandler->EventHandlerProcessing



// callback for Event Handler - in multithreaded case, event is queued to be processed later
//  in AO case, event is processed immediately by calling EventHandlerProcessing
OMX_ERRORTYPE CallbackEventHandler(OMX_OUT OMX_HANDLETYPE aComponent,
                                   OMX_OUT OMX_PTR aAppData,
                                   OMX_OUT OMX_EVENTTYPE aEvent,
                                   OMX_OUT OMX_U32 aData1,
                                   OMX_OUT OMX_U32 aData2,
                                   OMX_OUT OMX_PTR aEventData)
{

    PVMFOMXBaseDecNode *Node = (PVMFOMXBaseDecNode *) aAppData;

    if (Node->IsComponentMultiThreaded())
    {

        // CALL the AO API (a semaphore will block the thread if the queue is full)
        Node->iThreadSafeHandlerEventHandler->ReceiveEvent(aComponent, aAppData, aEvent, aData1, aData2, aEventData);
        return OMX_ErrorNone;
    }
    else
    {

        OMX_ERRORTYPE status;
        status = Node->EventHandlerProcessing(aComponent, aAppData, aEvent, aData1, aData2, aEventData);
        return status;
    }

}

// callback for EmptyBufferDone - in multithreaded case, event is queued to be processed later
//  in AO case, event is processed immediately by calling EmptyBufferDoneProcessing
OMX_ERRORTYPE CallbackEmptyBufferDone(OMX_OUT OMX_HANDLETYPE aComponent,
                                      OMX_OUT OMX_PTR aAppData,
                                      OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer)
{

    PVMFOMXBaseDecNode *Node = (PVMFOMXBaseDecNode *) aAppData;
    if (Node->IsComponentMultiThreaded())
    {
        // CALL the callback AO API (a semaphore will block the thread if the queue is full)
        Node->iThreadSafeHandlerEmptyBufferDone->ReceiveEvent(aComponent, aAppData, aBuffer);
        return OMX_ErrorNone;
    }
    else
    {
        OMX_ERRORTYPE status;
        status = Node->EmptyBufferDoneProcessing(aComponent, aAppData, aBuffer);
        return status;
    }

}

// callback for FillBufferDone - in multithreaded case, event is queued to be processed later
//  in AO case, event is processed immediately by calling FillBufferDoneProcessing
OMX_ERRORTYPE CallbackFillBufferDone(OMX_OUT OMX_HANDLETYPE aComponent,
                                     OMX_OUT OMX_PTR aAppData,
                                     OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer)
{
    PVMFOMXBaseDecNode *Node = (PVMFOMXBaseDecNode *) aAppData;
    if (Node->IsComponentMultiThreaded())
    {
        // CALL the callback AO API (a semaphore will block the thread if the queue is full)
        Node->iThreadSafeHandlerFillBufferDone->ReceiveEvent(aComponent, aAppData, aBuffer);
        return OMX_ErrorNone;
    }
    else
    {
        OMX_ERRORTYPE status;
        status = Node->FillBufferDoneProcessing(aComponent, aAppData, aBuffer);
        return status;
    }

}


/////////////////////////////////////////////////////////////////////////////
// Class Destructor
/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFOMXBaseDecNode::~PVMFOMXBaseDecNode()
{
    LogDiagnostics();

    //Clearup decoder
    DeleteOMXBaseDecoder();

    // Cleanup callback AOs and Mempools
    if (iThreadSafeHandlerEventHandler)
    {
        OSCL_DELETE(iThreadSafeHandlerEventHandler);
        iThreadSafeHandlerEventHandler = NULL;
    }
    if (iThreadSafeHandlerEmptyBufferDone)
    {
        OSCL_DELETE(iThreadSafeHandlerEmptyBufferDone);
        iThreadSafeHandlerEmptyBufferDone = NULL;
    }
    if (iThreadSafeHandlerFillBufferDone)
    {
        OSCL_DELETE(iThreadSafeHandlerFillBufferDone);
        iThreadSafeHandlerFillBufferDone = NULL;
    }

    if (iOutputMediaDataMemPool)
    {
        iOutputMediaDataMemPool->removeRef();
        iOutputMediaDataMemPool = NULL;
    }

    if (iInputMediaDataMemPool)
    {
        iInputMediaDataMemPool->removeRef();
        iInputMediaDataMemPool = NULL;
    }

    if (iOutBufMemoryPool)
    {
        iOutBufMemoryPool->removeRef();
        iOutBufMemoryPool = NULL;
    }
    if (iInBufMemoryPool)
    {
        iInBufMemoryPool->removeRef();
        iInBufMemoryPool = NULL;
    }

    if (in_ctrl_struct_ptr)
    {
        oscl_free(in_ctrl_struct_ptr);
        in_ctrl_struct_ptr = NULL;
    }



    if (out_ctrl_struct_ptr)
    {
        oscl_free(out_ctrl_struct_ptr);
        out_ctrl_struct_ptr = NULL;
    }

    if (iTrackUnderVerificationConfig)
    {
        oscl_free(iTrackUnderVerificationConfig);
        iTrackUnderVerificationConfig = NULL;
        iTrackUnderVerificationConfigSize = 0;
    }

    iOMXPreferredComponentOrderVec.clear();

    iLogger = NULL;

    //Thread logoff
    if (IsAdded())
    {
        RemoveFromScheduler();
        iIsAdded = false;
    }

    //Release Input buffer
    iDataIn.Unbind();
}

OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::HandleExtensionAPICommands()
{
    PVMFStatus status = PVMFFailure;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::HandleExtensionAPICommands() In", iName.Str()));
    switch (iCurrentCommand.iCmd)
    {

        case PVMF_GENERIC_NODE_GETNODEMETADATAKEYS:
            status = DoGetNodeMetadataKey();
            break;

        case PVMF_GENERIC_NODE_GETNODEMETADATAVALUES:
            status = DoGetNodeMetadataValue();
            break;

        default: //unknown command, do an assert here
            //and complete the command with PVMFErrNotSupported
            status = PVMFErrNotSupported;
            OSCL_ASSERT(false);
            break;
    }

    return status;
}

OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::CancelCurrentCommand()
{
    // Cancel DoFlush here and return success.
    if (IsFlushPending())
    {
        CommandComplete(iCurrentCommand, PVMFErrCancelled);
        return PVMFSuccess;
    }

    if (PVMF_GENERIC_NODE_RESET  == iCurrentCommand.iCmd)
    {
        if (iResetInProgress && !iResetMsgSent)
        {
            // if reset is started but reset msg has not been sent, we can cancel reset
            // as if nothing happened. Otherwise, the callback will set the flag back to false
            iResetInProgress = false;
        }
    }

    /* The pending commands DoPrepare, DoStart, DoPause, DoStop and DoReset
     * would be canceled in HandleComponentStateChange.
     * So return pending for now.
     */
    return PVMFPending;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::SetDecoderNodeConfiguration(PVMFOMXBaseDecNodeConfig& aNodeConfig)
{
    iNodeConfig = aNodeConfig;
    return PVMFSuccess;
}


/////////////////////////////////////////////////////////////////////////////
// Class Constructor
/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFOMXBaseDecNode::PVMFOMXBaseDecNode(int32 aPriority, const char aAOName[]) :
        PVMFNodeInterfaceImpl(aPriority, aAOName),
        iInPort(NULL),
        iOutPort(NULL),
        iOutBufMemoryPool(NULL),
        iOutputMediaDataMemPool(NULL),
        iInputMediaDataMemPool(NULL),
        iOMXComponentOutputBufferSize(0),
        iOutputAllocSize(0),
        iNumOutstandingOutputBuffers(0),
        iCodecSeqNum(0),
        iInPacketSeqNum(0),
        iProcessingState(EPVMFOMXBaseDecNodeProcessingState_Idle),
        iOMXDecoder(NULL),
        iSendBOS(false),
        iStreamID(0),
        iTSOfFirstDataMsgAfterBOS(0),
        iCurrentClipId(0),
        iSeqNum(0),
        iSeqNum_In(0),
        iIsAdded(true),
        iLogger(NULL),
        iDataPathLogger(NULL),
        iClockLogger(NULL),
        iExtensionRefCount(0),
        iEndOfDataReached(false),
        iEndOfDataTimestamp(0),
        iBOCReceived(false),
        iEOCReceived(false),
        iDiagnosticsLogger(NULL),
        iDiagnosticsLogged(false),
        iAvgBitrateValue(0),
        iResetInProgress(false),
        iResetMsgSent(false),
        iStopInResetMsgSent(false),
        iCompactFSISettingSucceeded(false),
        iIsVC1(false),
        iIsVC1AdvancedProfile(false),
        iSilenceInsertionFlag(true)
{
    iThreadSafeHandlerEventHandler = NULL;
    iThreadSafeHandlerEmptyBufferDone = NULL;
    iThreadSafeHandlerFillBufferDone = NULL;

    iInBufMemoryPool = NULL;
    iOutBufMemoryPool = NULL;

    in_ctrl_struct_ptr = NULL;

    out_ctrl_struct_ptr = NULL;


    ipExternalOutputBufferAllocatorInterface = NULL;

    // init to some value
    iOMXComponentOutputBufferSize = 0;
    iNumOutputBuffers = 0;
    iOutputBufferAlignment = 0;
    iOMXComponentInputBufferSize = 0;
    iNumInputBuffers = 0;
    iInputBufferAlignment = 0;
    iNumOutstandingInputBuffers = 0;

    iDoNotSendOutputBuffersDownstreamFlag = false;

    iIsConfigDataProcessingCompletionNeeded = false;
    iIsThereMoreConfigDataToBeSent = false;
    iConfigDataBytesProcessed = 0; // init this variable that may be needed for SPS/PPS processing
    iConfigDataBuffersOutstanding = 0;

    iOutputBuffersFreed = true;// buffers have not been created yet, so they can be considered freed
    iInputBuffersFreed = true;

    // dynamic port reconfig init vars
    iSecondPortReportedChange = false;
    iDynamicReconfigInProgress = false;
    iPauseCommandWasSentToComponent = false;
    iStopCommandWasSentToComponent = false;

    // capability related, set to default values
    SetDefaultCapabilityFlags();

    // EOS flag init
    iIsEOSSentToComponent = false;
    iIsEOSReceivedFromComponent = false;

    // reset repositioning related flags
    iIsRepositioningRequestSentToComponent = false;
    iIsRepositionDoneReceivedFromComponent = false;
    iIsOutputPortFlushed = false;
    iIsInputPortFlushed = false;

    // init state of component
    iCurrentDecoderState = OMX_StateInvalid;

    iOutTimeStamp = 0;
    iSampleDurationVec.reserve(PVMF_OMX_DEC_NODE_PORT_VECTOR_RESERVE);
    iTimestampVec.reserve(PVMF_OMX_DEC_NODE_PORT_VECTOR_RESERVE);

    //if timescale value is 1 000 000 - it means that
    //timestamp is expressed in units of 1/10^6 (i.e. microseconds)

    iTimeScale = 1000000;
    iInTimeScale = 1000;
    iOutTimeScale = 1000;


    iInputTimestampClock.set_timescale(iInTimeScale); // keep the timescale set to input timestamp


    // counts output frames (for logging)
    iFrameCounter = 0;
    iInputBufferUnderConstruction = NULL; // for partial frame assembly
    iFirstPieceOfPartialFrame = true;
    iObtainNewInputBuffer = true;
    iFirstDataMsgAfterBOS = true;
    iKeepDroppingMsgsUntilMarkerBit = false;
    sendFsi = true;

    iTrackUnderVerificationConfigSize = 0;
    iTrackUnderVerificationConfig = NULL;
    iOMXPreferredComponentOrderVec.clear();

    iComputeSamplesPerFrame = true;

    //key frame detection
    iCheckForKeyFrame = true;
    iSkipFrameCount = 0;
    iSkippingNonKeyFrames = true;
    iPVVideoFrameParserInput.SeqHeaderInfo = NULL;



}

/////////////////////////////////////////////////////////////////////////////
// Local Run Routine
/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::Run()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::Run() In", iName.Str()));

    // if reset is in progress, call DoReset again until Reset Msg is sent
    if ((iResetInProgress == true) &&
            (iResetMsgSent == false) &&
            (PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd)
       )
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - Calling DoReset", iName.Str()));

        PVMFStatus status = PVMFFailure;
        status = DoReset();

        if (status != PVMFPending)
        {
            CommandComplete(iCurrentCommand, status);
        }
        return; // don't do anything else
    }
    //Check for NODE commands...
    if (!iInputCommands.empty())
    {
        if (ProcessCommand())
        {
            if (iInterfaceState != EPVMFNodeCreated
                    && (!iInputCommands.empty() || (iInPort && (iInPort->IncomingMsgQueueSize() > 0)) ||
                        (iDataIn.GetRep() != NULL) || (iDynamicReconfigInProgress == true) ||
                        ((iNumOutstandingOutputBuffers < iNumOutputBuffers) &&
                         (iProcessingState == EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode))
                       ))
            {
                // reschedule if more data is available, or if port reconfig needs to be finished (even if there is no new data)
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - rescheduling after process command", iName.Str()));
                Reschedule();
            }
            return;
        }

        if (!iInputCommands.empty())
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - rescheduling to process more commands", iName.Str()));
            Reschedule();
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - Input commands empty", iName.Str()));
    }

    // prevent further processing of input/output if the node is not in the Started state (i.e. component is in executing state)
    if ((IsCommandInProgress(iCancelCommand)) || (iInterfaceState != EPVMFNodeStarted))
    {
        // rescheduling because of input data will be handled in Command Processing Part
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - Node not in Started state yet", iName.Str()));
        return;
    }


    // Process port activity, push out all outgoing messages
    if (iOutPort)
    {
        while (iOutPort->OutgoingMsgQueueSize())
        {
            // if port is busy it is going to wakeup from port ready event
            if (!ProcessOutgoingMsg(iOutPort))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - Outgoing Port Busy, cannot send more msgs", iName.Str()));
                break;
            }
        }
    }
    int loopCount = 0;
#if (PVLOGGER_INST_LEVEL >= PVLOGMSG_INST_PROF)
    uint32 startticks = OsclTickCount::TickCount();
    uint32 starttime = OsclTickCount::TicksToMsec(startticks);
#endif
    do // Try to consume all the data from the Input port
    {
        // Process input data message:
        // However, do not accept any input if
        // a) EOS needs to be processed
        // b) if 1 BOS has been processed and repositioning request (port flush) has been sent already, or
        // c) if repositioning is in progress
        // If any of the above conditions are met - potentially getting & processing another BOS or EOS may be problematic
        if (iInPort && (iInPort->IncomingMsgQueueSize() > 0) && (iDataIn.GetRep() == NULL) &&
                (!iEndOfDataReached) &&
                (!iDynamicReconfigInProgress) &&
                (!iIsRepositioningRequestSentToComponent)
           )

        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - Getting more input", iName.Str()));
            if (!ProcessIncomingMsg(iInPort))
            {
                //Re-schedule
                Reschedule();
                return;
            }
        }


        if (iSendBOS)
        {

            // this routine may be re-entered multiple times in multiple Run's before the component goes through cycle execute->idle->execute
            if (!HandleRepositioning())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - Repositioning not done yet", iName.Str()));

                return;
            }

            SendBeginOfMediaStreamCommand();


        }
        // If in init or ready to decode state, process data in the input port if there is input available and input buffers are present
        // (note: at EOS, iDataIn will not be available)
        // Note: as long as there is more config data to be sent, iDataIn will be != NULL

        if ((iDataIn.GetRep() != NULL) ||
                ((iNumOutstandingOutputBuffers < iNumOutputBuffers) &&
                 (iProcessingState == EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode) &&
                 (iResetMsgSent == false)) ||
                ((iDynamicReconfigInProgress == true) && (iResetMsgSent == false))

           )
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "%s::Run() - Calling HandleProcessingState", iName.Str()));

            // input data is available, that means there is input data to be decoded
            if (HandleProcessingState() != PVMFSuccess)
            {
                // If HandleProcessingState does not return Success, we must wait for an event
                // no point in  rescheduling
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "%s::Run() - HandleProcessingState did not return Success", iName.Str()));

                return;
            }
        }

        loopCount++;
        // loop if there is more input data available to send more data to omx component in one run
        // Do not loop if EOS is pending, config data needs to be processed first, or if reset is pending
        // (dynamic port reconfig states take care of the HandleProcessingState exit status above)
    }
    while (iInPort &&
            (((iInPort->IncomingMsgQueueSize() > 0) || (iDataIn.GetRep() != NULL)) && (iNumOutstandingInputBuffers < iNumInputBuffers))
            && (!iEndOfDataReached)
            && (iResetMsgSent == false)
          );
#if (PVLOGGER_INST_LEVEL >= PVLOGMSG_INST_PROF)
    uint32 endticks = OsclTickCount::TickCount();
    uint32 endtime = OsclTickCount::TicksToMsec(endticks);
    uint32 timeinloop;
    timeinloop = (endtime - starttime);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iRunlLogger, PVLOGMSG_INFO,
                    (0, "%s::Run() - LoopCount = %d, Time spent in loop(in ms) = %d, iNumOutstandingInputBuffers = %d, iNumOutstandingOutputBuffers = %d ",
                     iName.Str(), loopCount, timeinloop, iNumOutstandingInputBuffers, iNumOutstandingOutputBuffers));
#endif

    // EOS processing:
    // first send an empty buffer to OMX component and mark the EOS flag
    // wait for the OMX component to send async event to indicate that it has reached this EOS buffer
    // then, create and send the EOS message downstream

    if (iEndOfDataReached && !iDynamicReconfigInProgress)
    {

        // if EOS was not sent yet and we have an available input buffer, send EOS buffer to component
        if (!iIsEOSSentToComponent && (iNumOutstandingInputBuffers < iNumInputBuffers))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "%s::Run() - Sending EOS marked buffer To Component ", iName.Str()));

            iIsEOSSentToComponent = true;

            // if the component is not yet initialized or if it's in the middle of port reconfig,
            // don't send EOS buffer to component. It does not care. Just set the flag as if we received
            // EOS from the component to enable sending EOS downstream
            if (iProcessingState != EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode)
            {

                iIsEOSReceivedFromComponent = true;
            }
            else if (!SendEOSBufferToOMXComponent())

            {
                // for some reason, Component can't receive the EOS buffer
                // it could be that it is not initialized yet (because EOS could be the first msg). In this case,
                // send the EOS downstream anyway
                iIsEOSReceivedFromComponent = true;
            }
        }

        // DV: we must wait for event (acknowledgment from component)
        // before sending EOS downstream. This is because OMX Component will send
        // the EOS event only after processing remaining buffers

        if (iIsEOSReceivedFromComponent)
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "%s::Run() - Received EOS from component, Sending EOS msg downstream ", iName.Str()));

            if (iOutPort && iOutPort->IsOutgoingQueueBusy())
            {
                // note: we already tried to empty the outgoing q. If it's still busy,
                // it means that output port is busy. Just return and wait for the port to become free.
                // this will wake up the node and it will send out a msg from the q etc.
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "%s::Run() - - EOS cannot be sent downstream, outgoing queue busy - wait", iName.Str()));
                return;
            }

            if (SendEndOfTrackCommand()) // this will only q the EOS
            {
                // EOS send downstream OK, so reset the flag
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "%s::Run() - EOS was queued to be sent downstream", iName.Str()));

                iEndOfDataReached = false; // to resume normal processing, reset the flags
                iIsEOSSentToComponent = false;
                iIsEOSReceivedFromComponent = false;

                Reschedule(); // Run again to send out the EOS msg from the outgoing q, and resume
                // normal processing
                ReportInfoEvent(PVMFInfoEndOfData);
            }
        }
        else
        {
            // keep sending output buffers, it's possible that the component needs to flush output
            //  data at the end
            while (iNumOutstandingOutputBuffers < iNumOutputBuffers)
            {
                if (!SendOutputBufferToOMXComponent())
                    break;
            }
        }

    }


    //Check for flash command complition...
    if (iInPort && iOutPort && IsFlushPending() &&
            (iInPort->IncomingMsgQueueSize() == 0) &&
            (iOutPort->OutgoingMsgQueueSize() == 0) &&
            (iDataIn.GetRep() == NULL))
    {
        //flush command is complited
        //Debug check-- all the port queues should be empty at this point.

        OSCL_ASSERT(iInPort->IncomingMsgQueueSize() == 0 && iOutPort->OutgoingMsgQueueSize() == 0);

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "%s::Run() - Flush pending", iName.Str()));
        iEndOfDataReached = false;
        iIsEOSSentToComponent = false;
        iIsEOSReceivedFromComponent = false;


        //Flush is complete.  Go to initialized state.
        SetState(EPVMFNodePrepared);
        //resume port input so the ports can be re-started.
        iInPort->ResumeInput();
        iOutPort->ResumeInput();
        CommandComplete(iCurrentCommand, PVMFSuccess);
        Reschedule();
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::Run() Out", iName.Str()));
}

/////////////////////////////////////////////////////////////////////////////
// This routine will process incomming message from the port
/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::ProcessIncomingMsg(PVMFPortInterface* aPort)
{
    //Called by the AO to process one buffer off the port's
    //incoming data queue.  This routine will dequeue and
    //dispatch the data.

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "0x%x %s::ProcessIncomingMsg: aPort=0x%x", this, iName.Str(), aPort));

    PVMFStatus status = PVMFFailure;

#ifdef SIMULATE_BOS

    if (((PVMFOMXDecPort*)aPort)->iNumFramesConsumed == 6))
    {

        PVMFSharedMediaCmdPtr BOSCmdPtr = PVMFMediaCmd::createMediaCmd();

        // Set the format ID to BOS
        BOSCmdPtr->setFormatID(PVMF_MEDIA_CMD_BOS_FORMAT_ID);

        // Set the timestamp
        BOSCmdPtr->setTimestamp(201);
        BOSCmdPtr->setStreamID(0);

        // Convert to media message and send it out
        PVMFSharedMediaMsgPtr mediaMsgOut;
        convertToPVMFMediaCmdMsg(mediaMsgOut, BOSCmdPtr);

        //store the stream id and time stamp of bos message
        iStreamID = mediaMsgOut->getStreamID();

        iSendBOS = true;

        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
        return true;

    }
#endif
#ifdef SIMULATE_PREMATURE_EOS
    if (((PVMFOMXDecPort*)aPort)->iNumFramesConsumed == 5)
    {
        PVMFSharedMediaCmdPtr EOSCmdPtr = PVMFMediaCmd::createMediaCmd();

        // Set the format ID to EOS
        EOSCmdPtr->setFormatID(PVMF_MEDIA_CMD_EOS_FORMAT_ID);

        // Set the timestamp
        EOSCmdPtr->setTimestamp(200);

        // Convert to media message and send it out
        PVMFSharedMediaMsgPtr mediaMsgOut;
        convertToPVMFMediaCmdMsg(mediaMsgOut, EOSCmdPtr);

        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::ProcessIncomingMsg: SIMULATED EOS", iName.Str()));

        // Set EOS flag
        iEndOfDataReached = true;
        // Save the timestamp for the EOS cmd
        iEndOfDataTimestamp = mediaMsgOut->getTimestamp();

        return true;
    }

#endif



    PVMFSharedMediaMsgPtr msg;

    status = aPort->DequeueIncomingMsg(msg);
    if (status != PVMFSuccess)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                    (0, "0x%x %s::ProcessIncomingMsg: Error - DequeueIncomingMsg failed", this, iName.Str()));
        return false;
    }

    if (msg->getFormatID() == PVMF_MEDIA_CMD_BOS_FORMAT_ID)
    {
        //store the stream id and time stamp of bos message
        iStreamID = msg->getStreamID();
        iCurrentClipId = msg->getClipID();
        iSendBOS = true;

        // if new BOS arrives, and
        //if we're in the middle of a partial frame assembly
        // abandon it and start fresh
        if (iObtainNewInputBuffer == false)
        {
            if (iInputBufferUnderConstruction != NULL)
            {
                if (iInBufMemoryPool != NULL)
                {
                    iInBufMemoryPool->deallocate((OsclAny *)(iInputBufferUnderConstruction->pMemPoolEntry));
                }
                iInputBufferUnderConstruction = NULL;
            }
            iObtainNewInputBuffer = true;

        }

        // needed to init the sequence numbers and timestamp for partial frame assembly
        iFirstDataMsgAfterBOS = true;
        iKeepDroppingMsgsUntilMarkerBit = false;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::ProcessIncomingMsg: Received BOS stream %d, clipId %d", iName.Str(), iStreamID, iCurrentClipId));
        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
        return true;
    }
    else if (msg->getFormatID() == PVMF_MEDIA_CMD_EOS_FORMAT_ID)
    {
        // Set EOS flag
        iEndOfDataReached = true;
        // Save the timestamp for the EOS cmd
        iEndOfDataTimestamp = msg->getTimestamp();
        iCurrentClipId = msg->getClipID();

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::ProcessIncomingMsg: Received EOS clipId %d", iName.Str(), iCurrentClipId));

        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
        return true; // do not do conversion into media data, just set the flag and leave
    }

    convertToPVMFMediaData(iDataIn, msg);

    // Check if sample duration is available
    if (iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_DURATION_AVAILABLE_BIT)
    {
        // get the duration
        iSampleDurationVec.push_front(iDataIn->getDuration());
        iTimestampVec.push_front(iDataIn->getTimestamp());
    }


    if (iFirstDataMsgAfterBOS)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iDataPathLogger, PVLOGMSG_INFO,
                    (0, "%s::ProcessIncomingMsg: iTSOfFirstDataMsgAfterBOS = %d", iName.Str(), msg->getTimestamp()));
        iTSOfFirstDataMsgAfterBOS = msg->getTimestamp();
        iInputTimestampClock.set_clock(iTSOfFirstDataMsgAfterBOS, 0);
    }

    iCurrFragNum = 0; // for new message, reset the fragment counter
    iIsNewDataFragment = true;

    ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::ProcessIncomingMsg() Received %d frames", iName.Str(), ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed));

    //return true if we processed an activity...
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// This routine will process outgoing message by sending it into output the port
/////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::ProcessOutgoingMsg(PVMFPortInterface* aPort)
{
    //Called by the AO to process one message off the outgoing
    //message queue for the given port.  This routine will
    //try to send the data to the connected port.

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "0x%x %s::ProcessOutgoingMsg: aPort=0x%x", this, iName.Str(), aPort));

    PVMFStatus status = aPort->Send();
    if (status == PVMFErrBusy)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "0x%x %s::ProcessOutgoingMsg: Connected port goes into busy state", this, iName.Str()));
    }

    //Report any unexpected failure in port processing...
    //(the InvalidState error happens when port input is suspended,
    //so don't report it.)
    if (status != PVMFErrBusy
            && status != PVMFSuccess
            && status != PVMFErrInvalidState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "0x%x %s::Run: Error - ProcessPortActivity failed. port=0x%x, type=%d",
                         this, iName.Str(), iOutPort, PVMF_PORT_ACTIVITY_OUTGOING_MSG));
        ReportErrorEvent(PVMFErrPortProcessing);
    }

    //return true if we processed an activity...
    return (status != PVMFErrBusy);
}

/////////////////////////////////////////////////////////////////////////////
// This routine will process received data usign State Machine
/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXBaseDecNode::HandleProcessingState()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::HandleProcessingState() In", iName.Str()));

    PVMFStatus status = PVMFSuccess;

    switch (iProcessingState)
    {
        case EPVMFOMXBaseDecNodeProcessingState_InitDecoder:
        {
            // do init only if input data is available
            if (iDataIn.GetRep() != NULL)
            {
                // send output buffers to enable the component to start processing
                while (iNumOutstandingOutputBuffers < iNumOutputBuffers)
                {
                    // grab buffer header from the mempool if possible, and send to component
                    if (!SendOutputBufferToOMXComponent())

                        break;

                }

                // try to send config buffers to the component.
                // this should succeed, but for a large number of SPS/PPS in AVC, we may
                // run out of buffers and would need to try this several times.
                status = InitDecoder(iDataIn);
                if (status != PVMFSuccess)
                {
                    if (status != PVMFErrNoMemory)
                    {
                        // Decoder initialization failed. Fatal error
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::HandleProcessingState() Decoder initialization failed", iName.Str()));
                        ReportErrorEvent(PVMFErrResourceConfiguration);
                        ChangeNodeState(EPVMFNodeError);
                        status = PVMFFailure;
                        break;
                    }
                    else
                    {
                        // we just ran out of input buffers.
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::HandleProcessingState() Decoder initialization - ran out of input buffers. Continue when buffers are available", iName.Str()));
                        status = PVMFPending;
                        break;
                    }
                }

                // if a callback already happened, continue to decoding. If not, wait
                // it is also possible that port settings changed event may occur.
                //DV: temp
                //if(iProcessingState != EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode)
                //  iProcessingState = EPVMFOMXBaseDecNodeProcessingState_WaitForInitCompletion;

                iProcessingState = EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode;

                // spin once to send output buffers
                Reschedule();
                status = PVMFSuccess; // allow rescheduling
            }
            break;
        }

        case EPVMFOMXBaseDecNodeProcessingState_WaitForInitCompletion:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() WaitForInitCompletion -> wait for config buffer to return", iName.Str()));


            status = PVMFPending; // prevent rescheduling
            break;
        }
        // The FOLLOWING 4 states handle Dynamic Port Reconfiguration
        case EPVMFOMXBaseDecNodeProcessingState_PortReconfig:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() Port Reconfiguration -> Sending Flush Command", iName.Str()));


            // Collect all buffers first (before starting the portDisable command)
            // FIRST send a flush command. This will return all buffers from the component. Any outstanding buffers are in MIO
            // Then wait for all buffers to come back from MIO. If we haven't sent port disable, we'll be able to process
            // other commands in the copmponent (such as pause, stop etc.)
            OMX_ERRORTYPE err = OMX_ErrorNone;
            OMX_STATETYPE sState;

            // first check the state (if executing or paused, continue)
            err = OMX_GetState(iOMXDecoder, &sState);
            if (err != OMX_ErrorNone)
            {
                //Error condition report
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::HandleProcessingState (): PortReconfig Can't get State of decoder - trying to send port flush request!", iName.Str()));

                sState = OMX_StateInvalid;
                ReportErrorEvent(PVMFErrResourceConfiguration);
                ChangeNodeState(EPVMFNodeError);
                status = PVMFFailure;
                break;
            }

            if (((sState != OMX_StateExecuting) && (sState != OMX_StatePause)) || iStopCommandWasSentToComponent)
            {

                // possibly as a consequence of a previously queued cmd to go to Idle state?
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::HandleProcessingState (): PortReconfig: Component State is not executing or paused, do not proceed with port flush", iName.Str()));


            }
            else
            {

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::HandleProcessingState (): PortReconfig Sending Flush command to component", iName.Str()));


                // the port will now start returning outstanding buffers
                // set the flag to prevent output from going downstream (in case of output port being reconfigd)
                // set the flag to prevent input from being saved and returned to component (in case of input port being reconfigd)
                // set the state to wait for port saying it is disabled
                if (iPortIndexForDynamicReconfig == iOutputPortIndex)
                {
                    iDoNotSendOutputBuffersDownstreamFlag = true;
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::HandleProcessingState() Port Reconfiguration -> Output Port", iName.Str()));

                }
                else if (iPortIndexForDynamicReconfig == iInputPortIndex)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::HandleProcessingState() Port Reconfiguration -> Input Port", iName.Str()));
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::HandleProcessingState() Port Reconfiguration -> UNKNOWN PORT", iName.Str()));

                    sState = OMX_StateInvalid;
                    ReportErrorEvent(PVMFErrResourceConfiguration);
                    ChangeNodeState(EPVMFNodeError);
                    status = PVMFFailure;
                    break;

                }

                // send command to flush appropriate port
                err = OMX_SendCommand(iOMXDecoder, OMX_CommandFlush, iPortIndexForDynamicReconfig, NULL);
                if (err != OMX_ErrorNone)
                {
                    //Error condition report
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::HandleProcessingState (): PortReconfig : Can't send flush command !", iName.Str()));

                    sState = OMX_StateInvalid;
                    ReportErrorEvent(PVMFErrResourceConfiguration);
                    ChangeNodeState(EPVMFNodeError);
                    status = PVMFFailure;
                    break;
                }



            }

            // now sit back and wait for buffers to return
            // if there is a pause/stop cmd in the meanwhile, component will process it
            // and the node will end up in pause/stop state (so this internal state does not matter)
            iProcessingState = EPVMFOMXBaseDecNodeProcessingState_WaitForBufferReturn;


            // fall through to the next case to check if all buffers are already back



        }

        case EPVMFOMXBaseDecNodeProcessingState_WaitForBufferReturn:
        {
            // as buffers are coming back, Run may be called, wait until all buffers are back, then Free them all
            OMX_ERRORTYPE err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() Port Reconfiguration -> WaitForBufferReturn ", iName.Str()));
            // check if it's output port being reconfigured
            if (iPortIndexForDynamicReconfig == iOutputPortIndex)
            {
                // if all buffers have returned, free them
                if (iNumOutstandingOutputBuffers == 0)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::HandleProcessingState() Port Reconfiguration -> all output buffers are back, free them", iName.Str()));

                    // port reconfiguration is required. Only one port at a time is disabled and then re-enabled after buffer resizing
                    err = OMX_SendCommand(iOMXDecoder, OMX_CommandPortDisable, iPortIndexForDynamicReconfig, NULL);
                    if (OMX_ErrorNone != err)
                    {
                        //Error condition report
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::HandleProcessingState (): Port Reconfiguration : Can't send port disable command !", iName.Str()));

                        SetState(EPVMFNodeError);
                        ReportErrorEvent(PVMFFailure);
                        return PVMFFailure;
                    }


                    if (false == iOutputBuffersFreed)
                    {
                        if (!FreeBuffersFromComponent(iOutBufMemoryPool, // allocator
                                                      iOutputAllocSize,  // size to allocate from pool (hdr only or hdr+ buffer)
                                                      iNumOutputBuffers, // number of buffers
                                                      iOutputPortIndex, // port idx
                                                      false // this is not input
                                                     ))
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                            (0, "%s::HandleProcessingState() Port Reconfiguration -> Cannot free output buffers ", iName.Str()));

                            SetState(EPVMFNodeError);
                            ReportErrorEvent(PVMFErrNoMemory);
                            return PVMFErrNoMemory;
                        }
                    }
                    // if the callback (that port is disabled) has not arrived yet, wait for it
                    // if it has arrived, it will set the state to PortReEnable
                    if (iProcessingState != EPVMFOMXBaseDecNodeProcessingState_PortReEnable)
                        iProcessingState = EPVMFOMXBaseDecNodeProcessingState_WaitForPortDisable;

                    status = PVMFSuccess; // allow rescheduling of the node potentially
                }
                else
                    status = PVMFPending; // must wait for buffers to come back. No point in automatic rescheduling
                // but each buffer will reschedule the node when it comes in
            }
            else
            {
                // this is input port

                // if all buffers have returned, free them
                if (iNumOutstandingInputBuffers == 0)
                {

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::HandleProcessingState() Port Reconfiguration -> all input buffers are back, free them", iName.Str()));

                    // port reconfiguration is required. Only one port at a time is disabled and then re-enabled after buffer resizing
                    err = OMX_SendCommand(iOMXDecoder, OMX_CommandPortDisable, iPortIndexForDynamicReconfig, NULL);
                    if (OMX_ErrorNone != err)
                    {
                        //Error condition report
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::HandleProcessingState (): Port Reconfiguration : Can't send port disable command !", iName.Str()));

                        SetState(EPVMFNodeError);
                        ReportErrorEvent(PVMFFailure);
                        return PVMFFailure;
                    }

                    if (false == iInputBuffersFreed)
                    {
                        if (!FreeBuffersFromComponent(iInBufMemoryPool, // allocator
                                                      iInputAllocSize,   // size to allocate from pool (hdr only or hdr+ buffer)
                                                      iNumInputBuffers, // number of buffers
                                                      iInputPortIndex, // port idx
                                                      true // this is input
                                                     ))
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                            (0, "%s::HandleProcessingState() Port Reconfiguration -> Cannot free input buffers ", iName.Str()));

                            SetState(EPVMFNodeError);
                            ReportErrorEvent(PVMFErrNoMemory);
                            return PVMFErrNoMemory;

                        }
                    }
                    // if the callback (that port is disabled) has not arrived yet, wait for it
                    // if it has arrived, it will set the state to PortReEnable
                    if (iProcessingState != EPVMFOMXBaseDecNodeProcessingState_PortReEnable)
                        iProcessingState = EPVMFOMXBaseDecNodeProcessingState_WaitForPortDisable;

                    status = PVMFSuccess; // allow rescheduling of the node
                }
                else
                    status = PVMFPending; // must wait for buffers to come back. No point in automatic
                // rescheduling. Each buffer will reschedule the node
                // when it comes in
            }


            // the state will be changed to PortReEnable once we get confirmation that Port was actually disabled
            break;
        }

        case EPVMFOMXBaseDecNodeProcessingState_WaitForPortDisable:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() Port Reconfiguration -> wait for port disable callback", iName.Str()));
            // do nothing. Just wait for the port to become disabled (we'll get event from component, which will
            // transition the state to PortReEnable
            status = PVMFPending; // prevent Rescheduling the node
            break;
        }

        case EPVMFOMXBaseDecNodeProcessingState_PortReEnable:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() Port Reconfiguration -> Sending reenable port command", iName.Str()));

            status = HandlePortReEnable();
            break;
        }

        case EPVMFOMXBaseDecNodeProcessingState_WaitForPortEnable:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() Port Reconfiguration -> wait for port enable callback", iName.Str()));
            // do nothing. Just wait for the port to become enabled (we'll get event from component, which will
            // transition the state to ReadyToDecode
            status = PVMFPending; // prevent ReScheduling
            break;
        }

        // NORMAL DATA FLOW STATE:
        case EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() Ready To Decode start", iName.Str()));
            // In normal data flow and decoding state
            // Send all available output buffers to the decoder

            while (iNumOutstandingOutputBuffers < iNumOutputBuffers)
            {
                // grab buffer header from the mempool if possible, and send to component
                if (!SendOutputBufferToOMXComponent())

                    break;

            }

            if ((iNumOutstandingInputBuffers < iNumInputBuffers) && (iDataIn.GetRep() != NULL))
            {
                // try to get an input buffer header
                // and send the input data over to the component
                SendInputBufferToOMXComponent();
            }

            status = PVMFSuccess;
            break;


        }
        case EPVMFOMXBaseDecNodeProcessingState_Stopping:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() Stopping -> wait for Component to move from Executing->Idle", iName.Str()));


            status = PVMFPending; // prevent rescheduling
            break;
        }

        case EPVMFOMXBaseDecNodeProcessingState_Pausing:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleProcessingState() Pausing -> wait for Component to move from Executing->Pause", iName.Str()));


            status = PVMFPending; // prevent rescheduling
            break;
        }


        case EPVMFOMXBaseDecNodeProcessingState_WaitForOutgoingQueue:
            status = PVMFPending;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::HandleProcessingState() Do nothing since waiting for output port queue to become available", iName.Str()));
            break;

        default:
            break;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::HandleProcessingState() Out", iName.Str()));

    return status;

}
/////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::SendOutputBufferToOMXComponent()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendOutputBufferToOMXComponent() In", iName.Str()));


    OutputBufCtrlStruct *output_buf = NULL;
    OsclAny *pB = NULL;
    int32 errcode = OsclErrNone;
    uint32 ii;

    // try to get output buffer header from the mempool
    OSCL_TRY(errcode, pB = (OsclAny *) iOutBufMemoryPool->allocate(iOutputAllocSize));

    if (OsclErrNone != errcode)
    {
        if (OsclErrNoResources == errcode)
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                            PVLOGMSG_DEBUG, (0, "%s::SendOutputBufferToOMXComponent() No more output buffers in the mempool", iName.Str()));

            iOutBufMemoryPool->notifyfreechunkavailable(*this, (OsclAny *) iOutBufMemoryPool); // To signal when next deallocate() is called on mempool

            return false;
        }
        else
        {
            // Memory allocation for the pool failed
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::SendOutputBufferToOMXComponent() Output mempool error", iName.Str()));


            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return false;
        }

    }


    //for every allocated buffer, make sure you notify when buffer is released. Keep track of allocated buffers
    // use mempool as context to recognize which buffer (input or output) was returned
    iOutBufMemoryPool->notifyfreechunkavailable(*this, (OsclAny *)iOutBufMemoryPool);
    iNumOutstandingOutputBuffers++;

    for (ii = 0; ii < iNumOutputBuffers; ii++)
    {
        if (pB == out_ctrl_struct_ptr[ii].pMemPoolEntry)
        {
            output_buf = &out_ctrl_struct_ptr[ii];
            break;
        }
    }

    if ((ii == iNumOutputBuffers) || (output_buf == NULL))
        return false;


    output_buf->pBufHdr->nFilledLen = 0; // make sure you tell OMX component buffer is empty
    output_buf->pBufHdr->nOffset = 0;
    output_buf->pBufHdr->pAppPrivate = output_buf; // set pAppPrivate to be pointer to output_buf
    // (this is context for future release of this buffer to the mempool)
    // this was done during buffer creation, but still repeat just in case

    output_buf->pBufHdr->nFlags = 0; // zero out the flags

    OMX_ERRORTYPE err = OMX_ErrorNone;

    err = OMX_FillThisBuffer(iOMXDecoder, output_buf->pBufHdr);
    if (OMX_ErrorNone != err)
    {
        //Error condition report
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::SendOutputBufferToOMXComponent(): Error sending output buffer to the OMX component!", iName.Str()));

        iOutBufMemoryPool->deallocate(output_buf->pMemPoolEntry);
        return false;
    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendOutputBufferToOMXComponent() Out", iName.Str()));

    return true;
}
////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::CheckComponentForMultRoles(OMX_STRING aCompName, OMX_STRING aRole)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // find out how many roles the component supports
    OMX_U32 NumRoles;
    err = OMX_MasterGetRolesOfComponent(aCompName, &NumRoles, NULL);
    if (err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::CheckComponentForMultRoles() Problem getting component roles", iName.Str()));

        return false;
    }

    // if the component supports multiple roles, call OMX_SetParameter
    if (NumRoles > 1)
    {
        OMX_PARAM_COMPONENTROLETYPE RoleParam;
        CONFIG_SIZE_AND_VERSION(RoleParam);
        oscl_strncpy((OMX_STRING)RoleParam.cRole, aRole, OMX_MAX_STRINGNAME_SIZE);
        err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamStandardComponentRole, &RoleParam);
        if (err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::CheckComponentForMultRoles() Problem setting component role", iName.Str()));

            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::CheckComponentCapabilities(PVMFFormatType* aFormat, OMX_PTR aOutputParameters)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // GET CAPABILITY FLAGS FROM PV COMPONENT, IF this fails, use defaults
    PV_OMXComponentCapabilityFlagsType Cap_flags;
    err = OMX_GetParameter(iOMXDecoder, (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX, &Cap_flags);
    if (err != OMX_ErrorNone)
    {
        SetDefaultCapabilityFlags();
    }
    else
    {
        iIsOMXComponentMultiThreaded = (OMX_TRUE == Cap_flags.iIsOMXComponentMultiThreaded) ? true : false;
        iOMXComponentSupportsExternalInputBufferAlloc = (OMX_TRUE == Cap_flags.iOMXComponentSupportsExternalInputBufferAlloc) ? true : false;
        iOMXComponentSupportsExternalOutputBufferAlloc = (OMX_TRUE == Cap_flags.iOMXComponentSupportsExternalOutputBufferAlloc) ? true : false;
        iOMXComponentSupportsMovableInputBuffers = (OMX_TRUE == Cap_flags.iOMXComponentSupportsMovableInputBuffers) ? true : false;
        iOMXComponentSupportsPartialFrames = (OMX_TRUE == Cap_flags.iOMXComponentSupportsPartialFrames) ? true : false;
        iOMXComponentUsesNALStartCodes = (OMX_TRUE == Cap_flags.iOMXComponentUsesNALStartCodes) ? true : false;
        iOMXComponentCanHandleIncompleteFrames = (OMX_TRUE == Cap_flags.iOMXComponentCanHandleIncompleteFrames) ? true : false;
        iOMXComponentUsesFullAVCFrames = (OMX_TRUE == Cap_flags.iOMXComponentUsesFullAVCFrames) ? true : false;
        iOMXComponentUsesInterleaved2BNALSizes = (OMX_TRUE == Cap_flags.iOMXComponentUsesInterleaved2BNALSizes) ? true : false;
        iOMXComponentUsesInterleaved4BNALSizes = (OMX_TRUE == Cap_flags.iOMXComponentUsesInterleaved4BNALSizes) ? true : false;
    }

    // do some sanity checking

    if ((*aFormat != PVMF_MIME_H264_VIDEO) && (*aFormat != PVMF_MIME_H264_VIDEO_MP4) && (*aFormat != PVMF_MIME_H264_VIDEO_RAW))
    {
        iOMXComponentUsesNALStartCodes = false;
        iOMXComponentUsesFullAVCFrames = false;
        iOMXComponentUsesInterleaved2BNALSizes = false;
        iOMXComponentUsesInterleaved4BNALSizes = false;
    }

    if ((*aFormat == PVMF_MIME_H264_VIDEO) && !iOMXComponentCanHandleIncompleteFrames && !iOMXComponentUsesFullAVCFrames)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::CheckComponentCapabilities() Component cannot support streaming", iName.Str()));

        return false;
    }

    if (iOMXComponentUsesInterleaved2BNALSizes && iOMXComponentUsesInterleaved4BNALSizes)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::CheckComponentCapabilities() Invalid PV_Capability parameters.  Cannot use both interleaved 2 byte NAL sizes and interleaved 4 byte NAL sizes", iName.Str()));

        return false;
    }

    if ((iOMXComponentUsesInterleaved2BNALSizes || iOMXComponentUsesInterleaved4BNALSizes) && iOMXComponentUsesNALStartCodes)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::CheckComponentCapabilities() Invalid PV_Capability parameters.  Cannot use both NAL start codes and interleaved NAL sizes", iName.Str()));

        return false;
    }

    if (iOMXComponentUsesFullAVCFrames)
    {
        iNALCount = 0;
        oscl_memset(iNALSizeArray, 0, sizeof(uint32) * MAX_NAL_PER_FRAME); // 100 is max number of NALs
    }

    // make sure that copying is used where necessary
    if (!iOMXComponentSupportsPartialFrames
            || (iOMXComponentUsesNALStartCodes && (*aFormat != PVMF_MIME_H264_VIDEO_RAW))
            || iOMXComponentUsesInterleaved2BNALSizes
            || iOMXComponentUsesInterleaved4BNALSizes
            || iOMXComponentUsesFullAVCFrames)
    {
        iOMXComponentSupportsMovableInputBuffers = false;
    }

    // find out about parameters

    if (!NegotiateComponentParameters(aOutputParameters))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::CheckComponentCapabilities() Cannot get component parameters", iName.Str()));

        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::SetDefaultCapabilityFlags()
{

    iIsOMXComponentMultiThreaded = true;

    iOMXComponentSupportsExternalOutputBufferAlloc = false;
    iOMXComponentSupportsExternalInputBufferAlloc = false;
    iOMXComponentSupportsMovableInputBuffers = false;
    iOMXComponentUsesNALStartCodes = true;
    iOMXComponentSupportsPartialFrames = false;
    iOMXComponentCanHandleIncompleteFrames = false;
    iOMXComponentUsesFullAVCFrames = false;
    iOMXComponentUsesInterleaved2BNALSizes = false;
    iOMXComponentUsesInterleaved4BNALSizes = false;

    return true;
}



bool PVMFOMXBaseDecNode::SendEOSBufferToOMXComponent()
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendEOSBufferToOMXComponent() In", iName.Str()));


    // first of all, check if the component is running. EOS could be sent prior to component/decoder
    // even being initialized

    // returning false will ensure that the EOS will be sent downstream anyway without waiting for the
    // Component to respond
    if (iCurrentDecoderState != OMX_StateExecuting)
        return false;

    // get an input buffer. Without a buffer, no point in proceeding
    InputBufCtrlStruct *input_buf = NULL;
    OsclAny *pB = NULL;
    int32 errcode = OsclErrNone;

    // we already checked that the number of buffers is OK, so we don't expect problems
    // try to get input buffer header
    OSCL_TRY(errcode, pB = (OsclAny *) iInBufMemoryPool->allocate(iInputAllocSize));
    if (OsclErrNone != errcode)
    {
        if (OsclErrNoResources == errcode)
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                            PVLOGMSG_DEBUG, (0, "%s::SendEOSBufferToOMXComponent() No more buffers in the mempool - unexpected", iName.Str()));

            iInBufMemoryPool->notifyfreechunkavailable(*this, (OsclAny*) iInBufMemoryPool); // To signal when next deallocate() is called on mempool

            return false;
        }
        else
        {
            // Memory allocation for the pool failed
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::SendEOSBufferToOMXComponent() Input mempool error", iName.Str()));


            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return false;
        }

    }

    // keep track of buffers. When buffer is deallocated/released, the counter will be decremented
    iInBufMemoryPool->notifyfreechunkavailable(*this, (OsclAny*) iInBufMemoryPool);
    iNumOutstandingInputBuffers++;

    uint32 ii;
    for (ii = 0; ii < iNumInputBuffers; ii++)
    {
        if (pB == in_ctrl_struct_ptr[ii].pMemPoolEntry)
        {
            // found a match
            input_buf = &in_ctrl_struct_ptr[ii];
            break;
        }
    }

    if ((ii == iNumInputBuffers) || (input_buf == NULL))
        return false;

    // in this case, no need to use input msg refcounter. Make sure its unbound
    (input_buf->pMediaData).Unbind();

    // THIS IS AN EMPTY BUFFER. FLAGS ARE THE ONLY IMPORTANT THING
    input_buf->pBufHdr->nFilledLen = 0;
    input_buf->pBufHdr->nOffset = 0;

    iInputTimestampClock.update_clock(iEndOfDataTimestamp); // this will also take into consideration the rollover
    // convert TS in input timescale into OMX_TICKS
    iOMXTicksTimestamp = ConvertTimestampIntoOMXTicks(iInputTimestampClock);
    input_buf->pBufHdr->nTimeStamp = iOMXTicksTimestamp;

    // set ptr to input_buf structure for Context (for when the buffer is returned)
    input_buf->pBufHdr->pAppPrivate = (OMX_PTR) input_buf;

    // do not use Mark here (but init to NULL to prevent problems)
    input_buf->pBufHdr->hMarkTargetComponent = NULL;
    input_buf->pBufHdr->pMarkData = NULL;


    // init buffer flags
    input_buf->pBufHdr->nFlags = 0;

    input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
    // most importantly, set the EOS flag:
    input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_EOS;

    OMX_ERRORTYPE Err = OMX_ErrorNone;

    // send buffer to component
    Err = OMX_EmptyThisBuffer(iOMXDecoder, input_buf->pBufHdr);
    if (OMX_ErrorNone != Err)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::SendEOSBufferToOMXComponent() Error sending EOS marked input buffer to OMX component", iName.Str()));

        // Deallocate the mem pool incase sending buffer to component failed,
        // as there will not be any EmptyBufferDone callback for this buffer
        iInBufMemoryPool->deallocate((OsclAny *) input_buf->pMemPoolEntry);

        return false;
    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendEOSBufferToOMXComponent() Out", iName.Str()));

    return true;

}

// this method is called under certain conditions only if the node is doing partial frame assembly
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::DropCurrentBufferUnderConstruction()
{
    if (iObtainNewInputBuffer == false)
    {
        if (iInputBufferUnderConstruction != NULL)
        {
            if (iInBufMemoryPool != NULL)
            {
                iInBufMemoryPool->deallocate((OsclAny *)(iInputBufferUnderConstruction->pMemPoolEntry));
            }

            iInputBufferUnderConstruction = NULL;
        }
        iObtainNewInputBuffer = true;

    }
}

// this method is called under certain conditions only if the node is doing partial frame assembly
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::SendIncompleteBufferUnderConstruction()
{
    OMX_ERRORTYPE Err = OMX_ErrorNone;

    // this should never be the case, but check anyway
    if (iInputBufferUnderConstruction != NULL)
    {
        // mark as end of frame (the actual end piece is missing)
        iInputBufferUnderConstruction->pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::SendIncompleteBufferUnderConstruction()  - Sending Incomplete Buffer 0x%x to OMX Component MARKER field set to %x, TS=%d, Ticks=%L", iName.Str(), iInputBufferUnderConstruction->pBufHdr->pBuffer, iInputBufferUnderConstruction->pBufHdr->nFlags, iInTimestamp, iOMXTicksTimestamp));

        Err = OMX_EmptyThisBuffer(iOMXDecoder, iInputBufferUnderConstruction->pBufHdr);
        if (OMX_ErrorNone != Err)
        {
            //Error Condition, Log the error message and continue processing
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::SendIncompleteBufferUnderConstruction() Error Sending Incomplete Buffer to OMX component", iName.Str()));

            //Deallocate the memory pool entry as well
            (iInputBufferUnderConstruction->pMediaData).Unbind();
            iInBufMemoryPool->deallocate((OsclAny *)(iInputBufferUnderConstruction->pMemPoolEntry));
        }

        iInputBufferUnderConstruction = NULL;
        iObtainNewInputBuffer = true;

    }
}

OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::SendInputBufferToOMXComponent()
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendInputBufferToOMXComponent() In", iName.Str()));


    // first need to take care of  missing packets if node is assembling partial frames.
    // The action depends whether the component (I) can handle incomplete frames/NALs or (II) cannot handle incomplete frames/NALs
    if (!iOMXComponentSupportsPartialFrames)
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::SendInputBufferToOMXComponent() -- No support for partial frames", iName.Str()));
        // there are 4 cases after receiving a media msg and realizing there were missing packet(s):

        // a) TS remains the same - i.e. missing 1 or more pieces in the middle of the same frame
        //       I) basically ignore  - keep assembling the same frame  (middle will be missing)
        //      II) drop current buffer, drop msgs until next msg with marker bit arrives


        // b) TS is different than previous frame. Previous frame was sent OK (had marker bit).
        //              New frame assembly has not started yet. one or more pieces are missing from
        //              the beginning of the frame
        //       I) basically ignore - get a new buffer and start assembling new frame (beginning will be missing)
        //      II) no buffer to drop, but keep dropping msgs until next msg with marker bit arrives

        // c) TS is different than previous frame. Frame assembly has started (we were in the middle of a frame)
        //      but only 1 piece is missing => We know that the missing frame must have had the marker bit

        //       I) send out current buffer (last piece will be missing), get a new buffer and start assembling new frame (which is OK)
        //      II) just drop current buffer. Get a new buffer and start assembling new frame (no need to wait for marker bit)

        // d) TS is different than previous frame. Frame assembly has started ( we were in the middle of a frame)
        //      multiple pieces are missing => The last piece of the frame with the marker bit is missing for sure, but
        //      there could be also other frames missing or the beginning of the next frame is missing etc.

        //       I) send out current bufer (last piece will be missing). Get a new buffer and start assembling new frame (beginning COULD BE missing as well)
        //      II) drop current buffer. Keep dropping msgs until next msg with marker bit arrives


        // extract info from the media message - this is only needed to help skipping messages (no need to convert to OMX_TICKS yet)

        uint32 current_msg_seq_num = iDataIn->getSeqNum();
        uint32 current_msg_ts = iDataIn->getTimestamp();


        uint32 current_msg_marker;
        if (iSetMarkerBitForEveryFrag == true) // PV AVC case
        {
            current_msg_marker = iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_END_OF_NAL_BIT;
            current_msg_marker |= iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;
        }
        else
        {
            current_msg_marker = iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;
        }

        //Force marker bit for AMR streaming formats (marker bit may not be set even though full frames are present)
        if (iInPort && (
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB)
                ))
        {
            current_msg_marker = PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;
        }

        // check if this is the very first data msg
        if (iFirstDataMsgAfterBOS)
        {
            iFirstDataMsgAfterBOS = false;
            //init the sequence number & ts to make sure dropping logic does not kick in
            iInPacketSeqNum = current_msg_seq_num - 1;
            iInTimestamp = current_msg_ts - 10;
        }


        // first check if we need to keep dropping msgs
        if (iKeepDroppingMsgsUntilMarkerBit)
        {
            // drop this message
            iDataIn.Unbind();

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::SendInputBufferToOMXComponent() Dropping input msg with seqnum %d until marker bit", iName.Str(), current_msg_seq_num));

            //if msg has marker bit, stop dropping msgs
            if ((current_msg_marker != 0 && !iOMXComponentUsesFullAVCFrames) || // frame or NAL boundaries
                    ((current_msg_marker & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT) && iOMXComponentUsesFullAVCFrames)) // only frame boundaries
            {
                iKeepDroppingMsgsUntilMarkerBit = false;
                // also remember the sequence number & timestamp so that we have reference
                iInPacketSeqNum = current_msg_seq_num;
                iInTimestamp = current_msg_ts;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() Input msg with seqnum %d has marker bit set. Stop dropping msgs", iName.Str(), current_msg_seq_num));

            }
            return true;
        }

        // is there something missing?
        // compare current and saved sequence number - difference should be exactly 1
        //  if it is more, there is something missing
        if ((current_msg_seq_num - iInPacketSeqNum) > 1)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::SendInputBufferToOMXComponent() - MISSING PACKET DETECTED. Input msg with seqnum %d, TS=%d. Previous seqnum: %d, Previous TS: %d", iName.Str(), current_msg_seq_num, iInPacketSeqNum, current_msg_ts, iInTimestamp));

            // find out which case it is by comparing TS
            if (current_msg_ts == iInTimestamp)
            {

                // this is CASE a)
                // same ts, i.e. pieces are missing from the middle of the current frame
                if (!iOMXComponentCanHandleIncompleteFrames)
                {
                    // drop current buffer, drop msgs until you hit msg with marker bit
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - Drop current buffer under construction. Keep dropping msgs until marker bit", iName.Str()));

                    DropCurrentBufferUnderConstruction();
                    iKeepDroppingMsgsUntilMarkerBit = true;
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - Continue processing", iName.Str()));

                }
            }
            else // new ts and old ts are different
            {
                //  are we at the beginning of the new frame assembly?
                if (iObtainNewInputBuffer)
                {
                    // CASE b)
                    // i.e. we sent out previous frame, but have not started assembling a new frame. Pieces are missing from the beginning
                    if (!iOMXComponentCanHandleIncompleteFrames)
                    {
                        // there is no current buffer to drop, but drop msgs until you hit msg with marker bit
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                        (0, "%s::SendInputBufferToOMXComponent() - No current buffer under construction. Keep dropping msgs until marker bit", iName.Str()));

                        iKeepDroppingMsgsUntilMarkerBit = true;
                    }
                    else
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                        (0, "%s::SendInputBufferToOMXComponent() - Continue processing", iName.Str()));
                    }
                }
                else    // no, we are in the middle of a frame assembly, but new ts is different
                {
                    // is only 1 msg missing?
                    if ((current_msg_seq_num - iInPacketSeqNum) == 2)
                    {
                        // CASE c)
                        // only the last piece of the previous frame is missing
                        if (iOMXComponentCanHandleIncompleteFrames)
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                            (0, "%s::SendInputBufferToOMXComponent() - Send incomplete buffer under construction. Start assembling new frame", iName.Str()));

                            SendIncompleteBufferUnderConstruction();
                        }
                        else
                        {
                            // drop current frame only, but no need to wait until next marker bit.
                            // start assembling new frame
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                            (0, "%s::SendInputBufferToOMXComponent() - Drop current buffer under construction. It's OK to start assembling new frame. Only 1 packet is missing", iName.Str()));

                            DropCurrentBufferUnderConstruction();
                        }
                    }
                    else
                    {
                        // CASE d)
                        // (multiple) final piece(s) of the previous frame are missing and possibly pieces at the
                        // beginning of a new frame are also missing
                        if (iOMXComponentCanHandleIncompleteFrames)
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                            (0, "%s::SendInputBufferToOMXComponent() - Send incomplete buffer under construction. Start assembling new frame (potentially damaged)", iName.Str()));

                            SendIncompleteBufferUnderConstruction();
                        }
                        else
                        {
                            // drop current frame. start assembling new frame, but first keep dropping
                            // until you hit msg with marker bit.
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                            (0, "%s::SendInputBufferToOMXComponent() - Drop current buffer under construction. Keep dropping msgs until marker bit", iName.Str()));

                            DropCurrentBufferUnderConstruction();
                            iKeepDroppingMsgsUntilMarkerBit = true;
                        }
                    }
                }// end of if(obtainNewInputBuffer)/else
            }// end of if(curr_msg_ts == iInTimestamp)
        }//end of if(deltaseqnum>1)/else

        // check if we need to keep dropping msgs
        if (iKeepDroppingMsgsUntilMarkerBit)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::SendInputBufferToOMXComponent() Dropping input msg with seqnum %d until marker bit", iName.Str(), current_msg_seq_num));

            // drop this message
            iDataIn.Unbind();

            //if msg has marker bit, stop dropping msgs
            if ((current_msg_marker != 0 && !iOMXComponentUsesFullAVCFrames) || // frame or NAL boundaries
                    ((current_msg_marker & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT) && iOMXComponentUsesFullAVCFrames)) // only frame boundaries
            {
                iKeepDroppingMsgsUntilMarkerBit = false;
                // also remember the sequence number & timestamp so that we have reference
                iInPacketSeqNum = current_msg_seq_num;
                iInTimestamp = current_msg_ts;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() Input msg with seqnum %d has marker bit set. Stop dropping msgs", iName.Str(), current_msg_seq_num));

            }
            return true;
        }

    }// end of if/else (iOMXSUpportsPartialFrames)


    if (0 == iDataIn->getNumFragments())
    {
        //This is a corrupt media message, drop it
        iDataIn.Unbind();
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::SendInputBufferToOMXComponent() Corrupt Media Message with 0 Number of Fragments", iName.Str()));
        return false;
    }

    if ((((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW) &&
            !(iOMXComponentUsesFullAVCFrames && iOMXComponentUsesNALStartCodes) &&
            (iCurrFragNum == 0))
    {
        // parse raw h264 bytestream into NALs in separate media fragments
        if (false == ParseAndReWrapH264RAW(iDataIn))
        {
            return false;
        }
    }

    if (iNodeConfig.iKeyFrameOnlyMode || (iSkipFrameCount < iNodeConfig.iSkipNUntilKeyFrame))
    {
        if (PVMFSuccess != SkipNonKeyFrames())
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::SendInputBufferToOMXComponent() Frame detection failure - corrupt bitstream", iName.Str()));

            // Errors from corrupt bitstream are reported as info events
            ReportInfoEvent(PVMFInfoProcessingFailure, NULL);

            return false;
        }

        if (iSkippingNonKeyFrames)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::SendInputBufferToOMXComponent() Skipped non-keyframe", iName.Str()));

            return true;
        }
    }

    InputBufCtrlStruct *input_buf = NULL;
    OsclAny *pB = NULL;
    int32 errcode = OsclErrNone;
    uint32 ii;
    OMX_ERRORTYPE Err = OMX_ErrorNone;

// NOTE: a) if NAL start codes must be inserted i.e. iOMXComponentUsesNALStartCodess is TRUE, then iOMXComponentSupportsMovableInputBuffers must be set to FALSE.
//       b) if iOMXComponentSupportsPartialFrames is FALSE, then iOMXComponentSupportsMovableInputBuffers must be FALSE as well
//       c) if iOMXCOmponentSupportsPartialFrames is FALSE, and the input frame/NAL size is larger than the buffer size, the frame/NAL is discarded

    do
    {
        // do loop to loop over all fragments
        // first of all , get an input buffer. Without a buffer, no point in proceeding
        if (iObtainNewInputBuffer == true) // if partial frames are being reconstructed, we may be copying data into
            //existing buffer, so we don't need the new buffer
        {
            // try to get input buffer header

            errcode = AllocateChunkFromMemPool(pB, iInBufMemoryPool, iInputAllocSize);

            if (OsclErrNone != errcode)
            {
                if (OsclErrNoResources == errcode)
                {

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                                    PVLOGMSG_DEBUG, (0, "%s::SendInputBufferToOMXComponent() No more buffers in the mempool", iName.Str()));

                    iInBufMemoryPool->notifyfreechunkavailable(*this, (OsclAny*) iInBufMemoryPool); // To signal when next deallocate() is called on mempool

                    return false;
                }
                else
                {
                    // Memory allocation for the pool failed
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::SendInputBufferToOMXComponent() Input mempool error", iName.Str()));


                    SetState(EPVMFNodeError);
                    ReportErrorEvent(PVMFErrNoMemory);
                    return false;
                }

            }

            // keep track of buffers. When buffer is deallocated/released, the counter will be decremented
            iInBufMemoryPool->notifyfreechunkavailable(*this, (OsclAny*) iInBufMemoryPool);
            iNumOutstandingInputBuffers++;

            for (ii = 0; ii < iNumInputBuffers; ii++)
            {
                if (pB == in_ctrl_struct_ptr[ii].pMemPoolEntry)
                {
                    // found a match
                    input_buf = &in_ctrl_struct_ptr[ii];
                    break;
                }
            }

            if ((ii == iNumInputBuffers) || (input_buf == NULL))
                return false;

            // Now we have the buffer header (i.e. a buffer) to send to component:
            // Depending on OMX component capabilities, either pass the input msg fragment(s) directly
            //  into OMX component without copying (and update the input msg refcount)
            //  or memcopy the content of input msg memfrag(s) into OMX component allocated buffers
            input_buf->pBufHdr->nFilledLen = 0; // init this for now
            // save this in a class member
            iInputBufferUnderConstruction = input_buf;
            // set flags
            if (iOMXComponentSupportsPartialFrames == true)
            {
                // if partial frames can be sent, then send them
                // but we'll always need the new buffer for the new fragment
                iObtainNewInputBuffer = true;
            }
            else
            {
                // if we need to assemble partial frames, then obtain a new buffer
                // only after assembling the partial frame
                iObtainNewInputBuffer = false;
            }

            if (iOMXComponentUsesFullAVCFrames)
            {
                // reset NAL counters
                oscl_memset(iNALSizeArray, 0, sizeof(uint32) * iNALCount);
                iNALCount = 0;
            }

            iIncompleteFrame = false;
            iFirstPieceOfPartialFrame = iIsNewDataFragment;

        }
        else
        {
            input_buf = iInputBufferUnderConstruction;
        }

        // When copying content, a special case is when the input fragment is larger than the buffer and has to
        //  be fragmented here and broken over 2 or more buffers. Potential problem with available buffers etc.

        // if this is the first fragment in a new message, extract some info:
        if (iCurrFragNum == 0)
        {

            // NOTE: SeqNum differ in Codec and in Node because of the fact that
            // one msg can contain multiple fragments that are sent to the codec as
            // separate buffers. Node tracks msgs and codec tracks even separate fragments

            iCodecSeqNum += (iDataIn->getSeqNum() - iInPacketSeqNum); // increment the codec seq. # by the same
            // amount that the input seq. number increased

            iFirstDataMsgAfterBOS = false;
            iInPacketSeqNum = iDataIn->getSeqNum(); // remember input sequence number
            iInTimestamp = iDataIn->getTimestamp();

            // do the conversion into OMX_TICKS
            iInputTimestampClock.update_clock(iInTimestamp); // this will also take into consideration the timestamp rollover

            // convert TS in input timescale into OMX_TICKS
            iOMXTicksTimestamp = ConvertTimestampIntoOMXTicks(iInputTimestampClock);

            iInDuration = iDataIn->getDuration();
            iInNumFrags = iDataIn->getNumFragments();

            if (iSetMarkerBitForEveryFrag == true) // PV AVC case
            {
                iCurrentMsgMarkerBit = iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_END_OF_NAL_BIT;
                iCurrentMsgMarkerBit |= iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;
            }
            else
            {
                iCurrentMsgMarkerBit = iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;
            }

            //Force marker bit for AMR streaming formats (marker bit may not be set even though full frames are present)
            if (iInPort && (
                        (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR) ||
                        (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB)
                    ))
            {
                iCurrentMsgMarkerBit = PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;
            }


            // logging info:
            if (iDataIn->getNumFragments() > 1)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() - New msg has MULTI-FRAGMENTS", iName.Str()));
            }

            if (!(iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT) && !(iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_END_OF_NAL_BIT))
            {

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() - New msg has NO MARKER BIT", iName.Str()));
            }
        }
        else
        {
            //Increment the iInTimeStamp to point to the time stamp of next memory fragment for AAC format
            iOMXTicksTimestamp += iTimestampDeltaForMemFragment;

        }


        // get a memfrag from the message
        OsclRefCounterMemFrag frag;
        iDataIn->getMediaFragment(iCurrFragNum, frag);

        //Verify whether the memory fragment is valid or not
        if (NULL == (uint8 *)frag.getMemFragPtr())
        {
            iCurrFragNum++;
            iIsNewDataFragment = true; // done with this fragment. Get a new one

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::SendInputBufferToOMXComponent() - Invalid fragment with NULL pointer, TS=%d", iName.Str(), iInTimestamp));

            if (iCurrFragNum == iDataIn->getNumFragments())
            {
                // Return the buffer hdr already allocated to the memory pool
                iInBufMemoryPool->deallocate((OsclAny *) input_buf->pMemPoolEntry);

                iDataIn.Unbind();
                break;
            }

            //We want to continue with the same input buffer
            iObtainNewInputBuffer = false;
            continue;
        }

        // To add robustness to 3gpp playback of AVC file, we don't allow in-band SPS/PPS
        // If there's one, it's likely that this is a corrupted NAL
        if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_MP4)
        {
            uint8 *temp_nal = (uint8 *)frag.getMemFragPtr();
            uint32 nal_type = temp_nal[0] & 0x1F ;
            if (nal_type == 7 || nal_type == 8)
            {
                iCurrFragNum++;
                iIsNewDataFragment = true; // done with this fragment. Get a new one

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() - Invalid NAL Buffer of size %d, TS=%d", iName.Str(), frag.getMemFragSize(), iInTimestamp));

                if (iCurrFragNum == iDataIn->getNumFragments())
                {
                    // Return the buffer hdr already allocated to the memory pool
                    iInBufMemoryPool->deallocate((OsclAny *) input_buf->pMemPoolEntry);
                    iDataIn.Unbind();
                    break;
                }

                //We want to continue with the same input buffer
                iObtainNewInputBuffer = false;
                continue;
            }
        }


        if (iOMXComponentSupportsMovableInputBuffers)
        {
            // no copying required

            // increment the RefCounter of the message associated with the mem fragment/buffer
            // when sending this buffer to OMX component. (When getting the buffer back, the refcounter
            // will be decremented. Thus, when the last fragment is returned, the input mssage is finally released

            iDataIn.GetRefCounter()->addRef();

            // associate the buffer ctrl structure with the message ref counter and ptr
            input_buf->pMediaData = PVMFSharedMediaDataPtr(iDataIn.GetRep(), iDataIn.GetRefCounter());


            // set pointer to the data, length, offset
            input_buf->pBufHdr->pBuffer = (uint8 *)frag.getMemFragPtr();
            input_buf->pBufHdr->nFilledLen = frag.getMemFragSize();

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::SendInputBufferToOMXComponent() - Buffer 0x%x of size %d, %d frag out of tot. %d, TS=%d", iName.Str(), input_buf->pBufHdr->pBuffer, frag.getMemFragSize(), iCurrFragNum + 1, iDataIn->getNumFragments(), iInTimestamp));

            iCurrFragNum++; // increment fragment number and move on to the next
            iIsNewDataFragment = true; // update the flag

        }
        else
        {

            // in this case, no need to use input msg refcounter, each buffer fragment is copied over and treated separately
            (input_buf->pMediaData).Unbind();

            if (iFirstPieceOfPartialFrame == true)
            {
                if (iOMXComponentUsesNALStartCodes == true && (((PVMFOMXDecPort*)iInPort)->iFormat != PVMF_MIME_H264_VIDEO_RAW))
                {
                    oscl_memcpy(input_buf->pBufHdr->pBuffer + input_buf->pBufHdr->nFilledLen,
                                (void *) NAL_START_CODE,
                                NAL_START_CODE_SIZE);
                    input_buf->pBufHdr->nFilledLen += NAL_START_CODE_SIZE;
                    iFirstPieceOfPartialFrame = false;
                }
                else if (iOMXComponentUsesInterleaved2BNALSizes == true)
                {
                    uint16 nalsize = (uint16)frag.getMemFragSize();

                    oscl_memcpy(input_buf->pBufHdr->pBuffer + input_buf->pBufHdr->nFilledLen,
                                &nalsize,
                                sizeof(uint16));
                    input_buf->pBufHdr->nFilledLen += sizeof(uint16);
                    iFirstPieceOfPartialFrame = false;
                }
                else if (iOMXComponentUsesInterleaved4BNALSizes == true)
                {
                    uint32 nalsize = frag.getMemFragSize();

                    oscl_memcpy(input_buf->pBufHdr->pBuffer + input_buf->pBufHdr->nFilledLen,
                                &nalsize,
                                sizeof(uint32));
                    input_buf->pBufHdr->nFilledLen += sizeof(uint32);
                    iFirstPieceOfPartialFrame = false;
                }

                if (iIsVC1AdvancedProfile)
                {
                    uint8 vc1_frame_sc[4] = {0, 0, 1, 0xD};
                    uint8 *buffer = ((uint8 *)frag.getMemFragPtr());

                    if (buffer[0] || buffer[1] || !(buffer[2] == 1) || !(buffer[3] == 0xD))
                    {
                        /* does not have frame start code, therefore inserting it */
                        oscl_memcpy(input_buf->pBufHdr->pBuffer + input_buf->pBufHdr->nFilledLen, vc1_frame_sc, 4);

                        input_buf->pBufHdr->nFilledLen += 4;
                    }

                    iFirstPieceOfPartialFrame = false;
                }
            }

            // is this a new data fragment or are we still working on separating the old one?
            if (iIsNewDataFragment == true)
            {
                //  if fragment size is larger than the buffer size,
                //  need to break up the fragment even further into smaller chunks

                // init variables needed for fragment separation
                iCopyPosition = 0;
                iFragmentSizeRemainingToCopy  = frag.getMemFragSize();
            }

            // can the remaining fragment fit into the buffer?
            uint32 bytes_remaining_in_buffer;

            if (iOMXComponentUsesFullAVCFrames && !iOMXComponentUsesNALStartCodes && !iOMXComponentUsesInterleaved2BNALSizes && !iOMXComponentUsesInterleaved4BNALSizes)
            {
                // need to keep to account the extra data appended at the end of the buffer
                int32 temp = (input_buf->pBufHdr->nAllocLen - input_buf->pBufHdr->nFilledLen - (20 + 4 * (iNALCount + 1) + 20 + 6));//(sizeOfExtraDataStruct_NAL + sizeOfExTraData + sizeOfExtraDataStruct_terminator + padding);
                bytes_remaining_in_buffer = (uint32)((temp < 0) ? 0 : temp);
            }
            else
            {
                bytes_remaining_in_buffer = (input_buf->pBufHdr->nAllocLen - input_buf->pBufHdr->nFilledLen);
            }

            if (iFragmentSizeRemainingToCopy <= bytes_remaining_in_buffer)
            {

                oscl_memcpy(input_buf->pBufHdr->pBuffer + input_buf->pBufHdr->nFilledLen,
                            (void *)((uint8 *)frag.getMemFragPtr() + iCopyPosition),
                            iFragmentSizeRemainingToCopy);

                input_buf->pBufHdr->nFilledLen += iFragmentSizeRemainingToCopy;

                if (iOMXComponentUsesFullAVCFrames)
                {
                    iNALSizeArray[iNALCount] += iFragmentSizeRemainingToCopy;

                    if ((iCurrentMsgMarkerBit & PVMF_MEDIA_DATA_MARKER_INFO_END_OF_NAL_BIT) &&
                            (1 == iDataIn->getNumFragments()))
                    {
                        // streaming case (and 1 nal per frame file format case)
                        iNALCount++;
                        // we have a full NAL now, so insert a start code (if it needs it) for the next NAL, the next time through the loop
                        iFirstPieceOfPartialFrame = true;
                    }
                    else if (iDataIn->getNumFragments() > 1)
                    {
                        // multiple nals per frame file format case
                        iNALCount = iCurrFragNum + 1;
                        // we have a full NAL now, so insert a start code for the next NAL, the next time through the loop
                        iFirstPieceOfPartialFrame = true;
                    }
                }

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() - Copied %d bytes of fragment %d out of %d into buffer 0x%x of size %d, TS=%d ", iName.Str(), iFragmentSizeRemainingToCopy, iCurrFragNum + 1, iDataIn->getNumFragments(), input_buf->pBufHdr->pBuffer, input_buf->pBufHdr->nFilledLen, iInTimestamp));

                iCopyPosition += iFragmentSizeRemainingToCopy;
                iFragmentSizeRemainingToCopy = 0;

                iIsNewDataFragment = true; // done with this fragment. Get a new one
                iCurrFragNum++;

            }
            else
            {
                // copy as much as you can of the current fragment into the current buffer
                if (bytes_remaining_in_buffer > 0)
                {
                    oscl_memcpy(input_buf->pBufHdr->pBuffer + input_buf->pBufHdr->nFilledLen,
                                (void *)((uint8 *)frag.getMemFragPtr() + iCopyPosition),
                                bytes_remaining_in_buffer);

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - Copied %d bytes of fragment %d out of %d into buffer 0x%x of size %d, TS=%d", iName.Str(), bytes_remaining_in_buffer, iCurrFragNum + 1, iDataIn->getNumFragments(), input_buf->pBufHdr->pBuffer, input_buf->pBufHdr->nFilledLen, iInTimestamp));
                }

                input_buf->pBufHdr->nFilledLen += bytes_remaining_in_buffer;

                if (iOMXComponentUsesFullAVCFrames && (bytes_remaining_in_buffer > 0))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - Reconstructing partial frame (iOMXComponentUsesFullAVCFrames) - more data cannot fit in buffer 0x%x, TS=%d.Skipping data.", iName.Str(), input_buf->pBufHdr->pBuffer, iInTimestamp));

                    iNALSizeArray[iNALCount] += bytes_remaining_in_buffer;

                    // increment NAL count regardless if market bit is present or not since it is a fragment
                    iNALCount++;
                }

                iCopyPosition += bytes_remaining_in_buffer; // move current position within fragment forward
                iFragmentSizeRemainingToCopy -= bytes_remaining_in_buffer;
                iIncompleteFrame = true;
                iIsNewDataFragment = false; // set the flag to indicate we're still working on the "old" fragment
                if (!iOMXComponentSupportsPartialFrames)
                {
                    // if partial frames are not supported, and data cannot fit into the buffer, i.e. the buffer is full at this point
                    // simply go through remaining fragments if they exist and "drop" them
                    // i.e. send what data is alrady copied in the buffer and ingore the rest
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - Reconstructing partial frame - more data cannot fit in buffer 0x%x, TS=%d.Skipping data.", iName.Str(), input_buf->pBufHdr->pBuffer, iInTimestamp));

                    iIsNewDataFragment = true; // done with this fragment, get a new one
                    iCurrFragNum++;
                }
            }

        }

        // determine framesize for MP3
        if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MP3 && iComputeSamplesPerFrame)
        {
            if (PVMFSuccess != RetrieveMP3FrameLength(input_buf->pBufHdr->pBuffer + input_buf->pBufHdr->nOffset))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() - Error attempting to determing MP3 frame length from buffer 0x%x", iName.Str(), input_buf->pBufHdr->pBuffer));
            }
            else
                iComputeSamplesPerFrame = false;
        }


        // set buffer fields (this is the same regardless of whether the input is movable or not
        input_buf->pBufHdr->nOffset = 0;



        input_buf->pBufHdr->nTimeStamp = iOMXTicksTimestamp;

        // set ptr to input_buf structure for Context (for when the buffer is returned)
        input_buf->pBufHdr->pAppPrivate = (OMX_PTR) input_buf;

        // do not use Mark here (but init to NULL to prevent problems)
        input_buf->pBufHdr->hMarkTargetComponent = NULL;
        input_buf->pBufHdr->pMarkData = NULL;


        // init buffer flags
        input_buf->pBufHdr->nFlags = 0;

        // set marker bit on or off
        // Audio:
        // a) AAC - file playback - each fragment is a complete frame (1 msg may contain multiple fragments/frames)
        //                          each memory fragment is packed in a different input buffer marked with marker bit
        //                          and the timestamp for each memory fragment is calculated and assigned based on timestamp
        //                          of first mem fragment in a media message.
        //    AAC - streaming   - 1 msg may contain a partial frame, but LATM parser will assemble a full frame
        //                      (when LATM parser is done, we attach a marker bit to the data it produces)

        // b) AMR - file playback - each msg is N whole frames (marker bit is always set)
        //    AMR - streaming   - each msg is N whole frames (marker bit is missing from incoming msgs -set it here)

        // c) MP3 - file playback - 1 msg is N whole frames
        //
        // Video:
        // a) AVC - file playback - each fragment is a complete NAL (1 or more frags i.e. NALs per msg)
        //    AVC - streaming   - 1 msg contains 1 full NAL or a portion of a NAL
        // NAL may be broken up over multiple msgs. Frags are not allowed in streaming
        // b) M4V - file playback - each msg is 1 frame
        //    M4V - streaming   - 1 frame may be broken up into multiple messages and fragments

        // c) WMV - file playback - 1 frame is 1 msg
        //    WMV - streaming     - 1 frame may be broken up into multiple messages and fragments


        if (iSetMarkerBitForEveryFrag == true)
        {


            if (iIsNewDataFragment)
            {
                if ((iDataIn->getNumFragments() > 1))
                {
                    // if more than 1 fragment in the message and we have not broken it up
                    //(i.e. this is the last piece of a broken up piece), put marker bit on it unconditionally
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - END OF FRAGMENT - Multifragmented msg case, Buffer 0x%x MARKER bit set to 1", iName.Str(), input_buf->pBufHdr->pBuffer));

                    if (!iOMXComponentUsesFullAVCFrames)
                    {
                        // NAL mode, (uses OMX_BUFFERFLAG_ENDOFFRAME flag to mark end of NAL instead of end of frame)
                        // once NAL is complete, make sure you send it and obtain new buffer
                        input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
                        iObtainNewInputBuffer = true;
                    }
                    else if (iCurrentMsgMarkerBit & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT)
                    {
                        // frame mode (send out full AVC frames)
                        if (iCurrFragNum == iDataIn->getNumFragments())
                        {
                            input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
                            iObtainNewInputBuffer = true;
                        }
                    }
                }
                else if ((iDataIn->getNumFragments() == 1))
                {
                    // this is (the last piece of broken up by us) single-fragmented message. This can be a piece of a NAL (streaming) or a full NAL (file )
                    // apply marker bit if the message carries one
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - END OF FRAGMENT - Buffer 0x%x MARKER bit set to %d", iName.Str(), input_buf->pBufHdr->pBuffer, iCurrentMsgMarkerBit));

                    // if either PVMF_MEDIA_DATA_MARKER_INFO_END_OF_NAL_BIT or PVMF_MEDIA_DATA_MARKER_INFO_M_BIT
                    // and we're not in "frame" mode, then set OMX_BUFFERFLAG_ENDOFFRAME
                    if (iCurrentMsgMarkerBit && !iOMXComponentUsesFullAVCFrames)
                    {
                        // NAL mode, (uses OMX_BUFFERFLAG_ENDOFFRAME flag to mark end of NAL instead of end of frame)
                        // once NAL is complete, make sure you send it and obtain new buffer
                        input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
                        iObtainNewInputBuffer = true;
                    }
                    else if (iCurrentMsgMarkerBit & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT)
                    {
                        // frame mode
                        input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
                        iObtainNewInputBuffer = true;
                    }
                }
            }
            else
            {
                // we are separating fragments that are too big, i.e. bigger than
                // what 1 buffer can hold, this fragment Can NEVER have marker bit
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() - NOT END OF FRAGMENT - Buffer 0x%x MARKER bit set to 0", iName.Str(), input_buf->pBufHdr->pBuffer));

            }
        }
        else
        {
            // "normal" case, i.e. only fragments at ends of msgs may have marker bit set
            //                  fragments in the middle of a message never have marker bit set
            // there is also a (slight) possibility we broke up the fragment into more fragments
            //  because they can't fit into input buffer. In this case, make sure you apply
            //  the marker bit (if necessary) only to the very last piece of the very last fragment

            // for all other cases, clear the marker bit flag for the buffer
            if ((iCurrFragNum == iDataIn->getNumFragments()) && iIsNewDataFragment)
            {
                // if all the fragments have been exhausted, and this is the last piece
                // of the (possibly broken up) last fragment

                // use the marker bit from the end of message
                if (iCurrentMsgMarkerBit)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - END OF MESSAGE - Buffer 0x%x MARKER bit set to 1, TS=%d, Ticks=%L", iName.Str(), input_buf->pBufHdr->pBuffer, iInTimestamp, iOMXTicksTimestamp));

                    input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
                    // once frame is complete, make sure you send it and obtain new buffer

                    iObtainNewInputBuffer = true;
                }
                else
                {

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::SendInputBufferToOMXComponent() - END OF MESSAGE - Buffer 0x%x MARKER bit set to 0, TS=%d, Ticks=%L", iName.Str(), input_buf->pBufHdr->pBuffer, iInTimestamp, iOMXTicksTimestamp));
                }
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent() - NOT END OF MESSAGE - Buffer 0x%x MARKER bit set to 0, TS=%d, Ticks=%L", iName.Str(), input_buf->pBufHdr->pBuffer, iInTimestamp, iOMXTicksTimestamp));
            }


        }// end of else(setmarkerbitforeveryfrag)


        // set the key frame flag if necessary (mark every fragment that belongs to it)
        if (iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT)
            input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

        if (iObtainNewInputBuffer == true)
        {
            // if partial frames are supported, this flag will always be set
            // if partial frames are not supported, this flag will be set only
            // if the partial frame/NAL has been assembled, so we can send it

            // if incomplete frames are not supported then let go of this buffer, and move on
            if (iIncompleteFrame && !iOMXComponentCanHandleIncompleteFrames)
            {
                DropCurrentBufferUnderConstruction();
            }
            else
            {
                // append extra data for "frame" mode
                if (iOMXComponentUsesFullAVCFrames && !iOMXComponentUsesNALStartCodes && !iOMXComponentUsesInterleaved2BNALSizes && !iOMXComponentUsesInterleaved4BNALSizes)
                {
                    if (!AppendExtraDataToBuffer(input_buf, (OMX_EXTRADATATYPE) OMX_ExtraDataNALSizeArray, (uint8*) iNALSizeArray, 4 * iNALCount))
                    {
                        // if AppendExtraDataToBuffer returns false, that means there wasn't enough room to write the data, so drop the buffer
                        DropCurrentBufferUnderConstruction();
                    }
                }


                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::SendInputBufferToOMXComponent()  - Sending Buffer 0x%x to OMX Component MARKER field set to %x, TS=%d, Ticks=%L", iName.Str(), input_buf->pBufHdr->pBuffer, input_buf->pBufHdr->nFlags, iInTimestamp, iOMXTicksTimestamp));

                Err = OMX_EmptyThisBuffer(iOMXDecoder, input_buf->pBufHdr);
                if (OMX_ErrorNone != Err)
                {
                    //Error Condition, Log the error message and continue processing
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::SendInputBufferToOMXComponent() Error Sending Input Buffer to OMX Component", iName.Str()));

                    //Deallocate the memory pool entry as well
                    (input_buf->pMediaData).Unbind();
                    iInBufMemoryPool->deallocate((OsclAny *)(input_buf->pMemPoolEntry));
                }
                iInputBufferUnderConstruction = NULL; // this buffer is gone to OMX component now
            }
        }

        // if we sent all fragments to OMX component, decouple the input message from iDataIn
        // Input message is "decoupled", so that we can get a new message for processing into iDataIn
        //  However, the actual message is released completely to upstream mempool once all of its fragments
        //  are returned by the OMX component

        if (iCurrFragNum == iDataIn->getNumFragments())
        {
            iDataIn.Unbind();

        }
    }
    while (iCurrFragNum < iInNumFrags); //iDataIn->getNumFragments());

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendInputBufferToOMXComponent() Out", iName.Str()));

    return true;

}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::AppendExtraDataToBuffer(InputBufCtrlStruct* aInputBuffer,
        OMX_EXTRADATATYPE aType,
        uint8* aExtraData,
        uint8 aDataLength)

{
    // This function is used to append AVC NAL info to the buffer using the OMX_EXTRADATA_TYPE structure, when
    // a component requires buffers with full AVC frames rather than just NALs
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::AppendExtraDataToBuffer() In", iName.Str()));


    if ((aType != OMX_ExtraDataNone) && (aExtraData != NULL) && (aInputBuffer->pBufHdr->pBuffer != NULL))
    {
        const uint32 sizeOfExtraDataStruct = 20; // 20 is the number of bytes for the OMX_OTHER_EXTRADATATYPE structure (minus the data hint member)

        OMX_OTHER_EXTRADATATYPE extra;
        OMX_OTHER_EXTRADATATYPE terminator;

        CONFIG_SIZE_AND_VERSION(extra);
        CONFIG_SIZE_AND_VERSION(terminator);

        extra.nPortIndex = iInputPortIndex;
        terminator.nPortIndex = iInputPortIndex;

        extra.eType = aType;
        extra.nSize = (sizeOfExtraDataStruct + aDataLength + 3) & ~3; // size + padding for byte alignment
        extra.nDataSize = aDataLength;

        // fill in fields for terminator
        terminator.eType = OMX_ExtraDataNone;
        terminator.nDataSize = 0;

        // make sure there is enough room in the buffer
        if (aInputBuffer->pBufHdr->nAllocLen < (aInputBuffer->pBufHdr->nFilledLen + sizeOfExtraDataStruct + aDataLength + terminator.nSize + 6))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::AppendExtraDataToBuffer()  - Error (not enough room in buffer) appending extra data to Buffer 0x%x to OMX Component, TS=%d, Ticks=%L", iName.Str(), aInputBuffer->pBufHdr->pBuffer, iInTimestamp, iOMXTicksTimestamp));

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::AppendExtraDataToBuffer() Out", iName.Str()));

            return false;
        }

        // copy extra data into buffer
        // need to align to 4 bytes
        OMX_U8* buffer = aInputBuffer->pBufHdr->pBuffer + aInputBuffer->pBufHdr->nOffset + aInputBuffer->pBufHdr->nFilledLen;
        buffer = (OMX_U8*)(((OMX_U32) buffer + 3) & ~3);

        oscl_memcpy(buffer, &extra, sizeOfExtraDataStruct);
        oscl_memcpy(buffer + sizeOfExtraDataStruct, aExtraData, aDataLength);
        buffer += extra.nSize;

        oscl_memcpy(buffer, &terminator, terminator.nSize);

        // flag buffer
        aInputBuffer->pBufHdr->nFlags |= OMX_BUFFERFLAG_EXTRADATA;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::AppendExtraDataToBuffer()  - Appending extra data to Buffer 0x%x to OMX Component, TS=%d, Ticks=%L", iName.Str(), aInputBuffer->pBufHdr->pBuffer, iInTimestamp, iOMXTicksTimestamp));

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::AppendExtraDataToBuffer() Out", iName.Str()));

        return true;
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::AppendExtraDataToBuffer() Out", iName.Str()));

        return false;
    }
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::SendConfigBufferToOMXComponent(uint8 *initbuffer, uint32 initbufsize)

{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendConfigBufferToOMXComponent() In", iName.Str()));


    // first of all , get an input buffer. Without a buffer, no point in proceeding
    InputBufCtrlStruct *input_buf = NULL;
    OsclAny *pB = NULL;
    int32 errcode = OsclErrNone;

    // try to get input buffer header
    OSCL_TRY(errcode, pB = (OsclAny *) iInBufMemoryPool->allocate(iInputAllocSize));
    if (OsclErrNone != errcode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::SendConfigBufferToOMXComponent() Input buffer mempool out of buffers -unexpected at init - but possible for multiple SPS PPS NAL", iName.Str()));
        // if this happens - we'll wait for buffers to come back and send more config data later

        return PVMFErrNoMemory;
    }

    // Got a buffer OK
    // keep track of buffers. When buffer is deallocated/released, the counter will be decremented
    iInBufMemoryPool->notifyfreechunkavailable(*this, (OsclAny*) iInBufMemoryPool);
    iNumOutstandingInputBuffers++;

    uint32 ii;
    for (ii = 0; ii < iNumInputBuffers; ii++)
    {
        if (pB == in_ctrl_struct_ptr[ii].pMemPoolEntry)
        {
            // found a match
            input_buf = &in_ctrl_struct_ptr[ii];
            break;
        }
    }

    if ((ii == iNumInputBuffers) || (input_buf == NULL))
        return PVMFFailure; // something is wrong - this should not happen

    input_buf->pBufHdr->nFilledLen = 0; //init this to 0

    // Now we have the buffer header (i.e. a buffer) to send to component:
    // Depending on OMX component capabilities, either pass the input msg fragment(s) directly
    //  into OMX component without copying (and update the input msg refcount)
    //  or memcopy the content of input msg memfrag(s) into OMX component allocated buffers

    // When copying content, a special case is when the input fragment is larger than the buffer and has to
    //  be fragmented here and broken over 2 or more buffers. Potential problem with available buffers etc.

    iCodecSeqNum += (iDataIn->getSeqNum() - iInPacketSeqNum); // increment the codec seq. # by the same
    // amount that the input seq. number increased

    iInPacketSeqNum = iDataIn->getSeqNum(); // remember input sequence number
    iInTimestamp = iDataIn->getTimestamp();

    iInDuration = iDataIn->getDuration();


    if (!(iDataIn->getMarkerInfo() & PVMF_MEDIA_DATA_MARKER_INFO_M_BIT))
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::SendConfigBufferToOMXComponent() - New msg has NO MARKER BIT", iName.Str()));
    }

    if (iOMXComponentSupportsMovableInputBuffers)
    {
        // no copying required

        // increment the RefCounter of the message associated with the mem fragment/buffer
        // when sending this buffer to OMX component. (When getting the buffer back, the refcounter
        // will be decremented. Thus, when the last fragment is returned, the input mssage is finally released

        iDataIn.GetRefCounter()->addRef();

        // associate the buffer ctrl structure with the message ref counter and ptr
        input_buf->pMediaData = PVMFSharedMediaDataPtr(iDataIn.GetRep(), iDataIn.GetRefCounter());

        // set pointer to the data, length, offset
        if ((((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW) && !iOMXComponentUsesNALStartCodes && !iOMXComponentUsesFullAVCFrames)
        {
            // remove start codes
            uint8* temp_ptr = initbuffer;
            uint32 remaining_size = initbufsize;
            uint32 length = 0;

            length = GetNAL_OMXNode(&temp_ptr, &remaining_size);

            input_buf->pBufHdr->pBuffer = temp_ptr;
            input_buf->pBufHdr->nFilledLen = length;
        }
        else
        {
            input_buf->pBufHdr->pBuffer = initbuffer;
            input_buf->pBufHdr->nFilledLen = initbufsize;
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::SendConfigBufferToOMXComponent() - Config Buffer 0x%x of size %d", iName.Str(), initbuffer, initbufsize));

    }
    else
    {

        // in this case, no need to use input msg refcounter, each buffer fragment is copied over and treated separately
        (input_buf->pMediaData).Unbind();

        // we assume the buffer is large enough to fit the config data
        if ((((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_H264_VIDEO_RAW) && !iOMXComponentUsesNALStartCodes && !iOMXComponentUsesFullAVCFrames)
        {
            // skip start codes
            uint8* temp_ptr = initbuffer;
            uint32 remaining_size = initbufsize;

            iCopyPosition = (uint32)temp_ptr;

            // get length of NAL
            iFragmentSizeRemainingToCopy = GetNAL_OMXNode(&temp_ptr, &remaining_size);

            // find offset to beginning of NAL
            iCopyPosition = (uint32)temp_ptr - iCopyPosition;
        }
        else
        {
            iCopyPosition = 0;
            iFragmentSizeRemainingToCopy  = initbufsize;
        }

        if (iOMXComponentUsesNALStartCodes == true && (((PVMFOMXDecPort*)iInPort)->iFormat != PVMF_MIME_H264_VIDEO_RAW))
        {
            oscl_memcpy(input_buf->pBufHdr->pBuffer,
                        (void *) NAL_START_CODE,
                        NAL_START_CODE_SIZE);
            input_buf->pBufHdr->nFilledLen += NAL_START_CODE_SIZE;
        }
        else if (iOMXComponentUsesInterleaved2BNALSizes == true)
        {
            uint16 tempSize = (uint16)initbufsize;

            oscl_memcpy(input_buf->pBufHdr->pBuffer,
                        &tempSize,
                        sizeof(uint16));
            input_buf->pBufHdr->nFilledLen += sizeof(uint16);
        }
        else if (iOMXComponentUsesInterleaved4BNALSizes == true)
        {
            oscl_memcpy(input_buf->pBufHdr->pBuffer,
                        &initbufsize,
                        sizeof(uint32));
            input_buf->pBufHdr->nFilledLen += sizeof(uint32);
        }

        // can the remaining fragment fit into the buffer?
        uint32 bytes_remaining_in_buffer = (input_buf->pBufHdr->nAllocLen - input_buf->pBufHdr->nFilledLen);

        if (iFragmentSizeRemainingToCopy <= bytes_remaining_in_buffer)
        {
            oscl_memcpy(input_buf->pBufHdr->pBuffer + input_buf->pBufHdr->nFilledLen,
                        (void *)(initbuffer + iCopyPosition),
                        iFragmentSizeRemainingToCopy);

            input_buf->pBufHdr->nFilledLen += iFragmentSizeRemainingToCopy;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::SendConfigBufferToOMXComponent() - Copied %d bytes into buffer 0x%x of size %d", iName.Str(), iFragmentSizeRemainingToCopy, input_buf->pBufHdr->pBuffer, input_buf->pBufHdr->nFilledLen));

            iCopyPosition += iFragmentSizeRemainingToCopy;
            iFragmentSizeRemainingToCopy = 0;
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::SendConfigBufferToOMXComponent() Config buffer too large problem -unexpected at init", iName.Str()));

            return PVMFFailure;
        }

    }


    // set buffer fields (this is the same regardless of whether the input is movable or not
    input_buf->pBufHdr->nOffset = 0;

    iInputTimestampClock.update_clock(iInTimestamp); // this will also take into consideration the timestamp rollover
    iOMXTicksTimestamp = ConvertTimestampIntoOMXTicks(iInputTimestampClock); // make a conversion into OMX ticks
    input_buf->pBufHdr->nTimeStamp = iOMXTicksTimestamp;

    // set ptr to input_buf structure for Context (for when the buffer is returned)
    input_buf->pBufHdr->pAppPrivate = (OMX_PTR) input_buf;

    // do not use Mark here (but init to NULL to prevent problems)
    input_buf->pBufHdr->hMarkTargetComponent = NULL;
    input_buf->pBufHdr->pMarkData = NULL;


    // init buffer flags
    input_buf->pBufHdr->nFlags = 0;

    // set marker bit on

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendConfigBufferToOMXComponent() - END OF FRAGMENT - Buffer 0x%x MARKER bit set to 1", iName.Str(), input_buf->pBufHdr->pBuffer));

    input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

    // set buffer flag indicating buffer contains codec config data
    input_buf->pBufHdr->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;

    OMX_ERRORTYPE Err = OMX_ErrorNone;

    Err = OMX_EmptyThisBuffer(iOMXDecoder, input_buf->pBufHdr);
    if (OMX_ErrorNone != Err)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::SendConfigBufferToOMXComponent() Config buffer too large problem -unexpected at init", iName.Str()));

        //Deallocate the memory pool entry as well
        (input_buf->pMediaData).Unbind();
        iInBufMemoryPool->deallocate((OsclAny *)(input_buf->pMemPoolEntry));

        return PVMFFailure;
    }

    return PVMFSuccess;

}



/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::CreateOutMemPool(uint32 num_buffers)
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::CreateOutMemPool() start", iName.Str()));
    // In the case OMX component wants to allocate its own buffers,
    // mempool only contains a dummy value - so that the mempool mechanisms are used to keep track of free/used buffers
    // In case OMX component uses pre-allocated buffers (here),
    // mempool allocates the actual data buffers

    // OutputBufCtrlStruct are allocated separately as an array (one ctrl structure for each buffer)

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::CreateOutMemPool() Allocating output buffer mempool", iName.Str()));

    iOutputAllocSize = oscl_mem_aligned_size((uint32)sizeof(uint32));

    if (iOMXComponentSupportsExternalOutputBufferAlloc)
    {
        // In case of an external output buffer allocator interface, output buffer memory will be allocated
        // outside the node and hence iOutputAllocSize need not be incremented here

        if (NULL == ipExternalOutputBufferAllocatorInterface)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::CreateOutMemPool() Allocating output buffers of size %d as well", iName.Str(), iOMXComponentOutputBufferSize));
            //pre-negotiated output buffer size
            iOutputAllocSize = iOMXComponentOutputBufferSize;
        }
    }

    // for media data wrapper
    if (iOutputMediaDataMemPool)
    {
        iOutputMediaDataMemPool->removeRef();
        iOutputMediaDataMemPool = NULL;
    }

    if (iOutBufMemoryPool)
    {
        iOutBufMemoryPool->removeRef();
        iOutBufMemoryPool = NULL;
    }

    int32 leavecode = OsclErrNone;
    OSCL_TRY(leavecode, iOutBufMemoryPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (num_buffers, iOutputAllocSize, NULL, iOutputBufferAlignment)););
    if (leavecode || iOutBufMemoryPool == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                        PVLOGMSG_ERR, (0, "%s::CreateOutMemPool() Memory pool structure for output buffers failed to allocate", iName.Str()));
        return false;
    }

    // init the counter
    iNumOutstandingOutputBuffers = 0;

    // allocate mempool for media data message wrapper
    leavecode = OsclErrNone;
    OSCL_TRY(leavecode, iOutputMediaDataMemPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (num_buffers, PVOMXBASEDEC_MEDIADATA_CHUNKSIZE)));
    if (leavecode || iOutputMediaDataMemPool == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::CreateOutMemPool() Output Media Data Buffer pool for output buffers failed to allocate", iName.Str()));
        return false;
    }

    // create or re-create out ctrl structures
    if (out_ctrl_struct_ptr)
    {
        oscl_free(out_ctrl_struct_ptr);
        out_ctrl_struct_ptr = NULL;
    }


    out_ctrl_struct_ptr = (OutputBufCtrlStruct *) oscl_malloc(iNumOutputBuffers * sizeof(OutputBufCtrlStruct));

    if (out_ctrl_struct_ptr == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::CreateOutMemPool() Error - Cannot create out_ctrl_struct_ptr == NULL"));

        return false;
    }



    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::CreateOutMemPool() done", iName.Str()));
    return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Creates memory pool for input buffer management ///////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::CreateInputMemPool(uint32 num_buffers)
{
    // 3 cases in order of preference and simplicity

    // Case 1 (buffers allocated upstream - no memcpy needed):
    //  PV OMX Component - We use buffers allocated outside the OMX node (i.e. allocated upstream)
    // Mempool contains either dummy values (if buffers are allocated in the component) or actual data buffers

    // InputCtrlStruct ptrs are allocated separately (one ctrl struct per buffer)
    // and contain ptrs to OMX buffer headers and PMVFMediaData ptrs - to keep track of when to unbind input msgs

    // NOTE:    in this case, (iOMXCOmponentSupportsMovableBuffers = TRUE) - when providing input buffers to OMX component,
    //          OMX_UseBuffer calls will provide some initial pointers and sizes of buffers, but these
    //          are dummy values. Actual buffer pointers and filled sizes will be obtained from the input msg fragments.
    //          The PV OMX component will use the buffers even if the ptrs differ from the ones during initialization
    //          3rd party OMX components can also use this case if they are capable of ignoring the actual buffer pointers in
    //          buffer header field (i.e. if after OMX_UseBuffer(...) call, they allow the ptr to actual buffer data to change at a later time

    // CASE 2 ( Data buffers allocated in the node - memcpy needed)
    //          use_buffer omx call is used -
    //          since 3rd party OMX component can use buffers allocated outside OMX component.
    //          Actual data buffers must still be filled using memcpy since movable buffers are
    //          not supported (having movable buffers means being able to change pBuffer ptr in the omx buffer header
    //          structure dynamically (i.e. after initialization with OMX_UseBuffer call is complete)

    //      Mempool contains actual data buffers (alignment is observed)
    //      InputBufCtrlStructures (ptrs to buffer headers, PVMFMediaData ptrs to keep track of when to unbind input msgs) +
    //
    //          NOTE: Data must be copied from input message into the local buffer before the buffer is given to the OMX component

    // CASE 3  (Data buffers allocated in the OMX component - memcpy needed)
    //          If 3rd party OMX component must allocate its own buffers
    //          Mempool only contains dummy values
    //          InputBufCtrlStructures (ptrs to buffer headers + PMVFMediaData ptrs to keep track of when to unbind input msgs)
    //          NOTE: Data must be copied from input message into the local buffer before the buffer is given to the OMX component (like in case 2)

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::CreateInputMemPool() start ", iName.Str()));

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::CreateInputMemPool() allocating buffer header pointers and shared media data ptrs ", iName.Str()));



    iInputAllocSize = oscl_mem_aligned_size((uint32) sizeof(uint32));

    // Need to allocate buffers in the node either if component supports external buffers buffers
    // but they are not "movable"

    if ((iOMXComponentSupportsExternalInputBufferAlloc && !iOMXComponentSupportsMovableInputBuffers))
    {
        //pre-negotiated input buffer size
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::CreateOutMemPool() Allocating input buffers of size %d as well", iName.Str(), iOMXComponentInputBufferSize));

        iInputAllocSize = iOMXComponentInputBufferSize;
    }

    // for media data wrapper
    if (iInputMediaDataMemPool)
    {
        iInputMediaDataMemPool->removeRef();
        iInputMediaDataMemPool = NULL;
    }
    if (iInBufMemoryPool)
    {
        iInBufMemoryPool->removeRef();
        iInBufMemoryPool = NULL;
    }

    int32 leavecode = OsclErrNone;
    OSCL_TRY(leavecode, iInBufMemoryPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (num_buffers, iInputAllocSize, NULL, iInputBufferAlignment)););
    if (leavecode || iInBufMemoryPool == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                        PVLOGMSG_ERR, (0, "%s::CreateInputMemPool() Memory pool structure for input buffers failed to allocate", iName.Str()));
        return false;
    }

    // init the counter
    iNumOutstandingInputBuffers = 0;

    // allocate mempool for media data message wrapper
    leavecode = OsclErrNone;
    OSCL_TRY(leavecode, iInputMediaDataMemPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (num_buffers, PVOMXBASEDEC_MEDIADATA_CHUNKSIZE)));
    if (leavecode || iInputMediaDataMemPool == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::CreateInputMemPool() Media Data Buffer pool for input buffers failed to allocate", iName.Str()));
        return false;
    }

    // create input ctrl structures
    if (in_ctrl_struct_ptr)
    {
        oscl_free(in_ctrl_struct_ptr);
        in_ctrl_struct_ptr = NULL;
    }

    in_ctrl_struct_ptr = (InputBufCtrlStruct *) oscl_malloc(iNumInputBuffers * sizeof(InputBufCtrlStruct));

    if (in_ctrl_struct_ptr == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::CreateInputMemPool() Error - Cannot create in_ctrl_struct_ptr == NULL"));


        return false;
    }




    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::CreateInputMemPool() done", iName.Str()));
    return true;
}
////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::ProvideBuffersToComponent(OsclMemPoolFixedChunkAllocator *aMemPool, // allocator
        uint32 aAllocSize,   // size to allocate from mempool (data buffer or dummy )
        uint32 aNumBuffers,    // number of buffers
        uint32 aActualBufferSize, // actual buffer size
        uint32 aPortIndex,      // port idx
        bool aUseBufferOK,      // can component use OMX_UseBuffer or should it use OMX_AllocateBuffer
        bool    aIsThisInputBuffer      // is this input or output
                                                                  )
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::ProvideBuffersToComponent() enter", iName.Str()));

    uint32 ii = 0;
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // Now, go through all buffers and tell component to
    // either use a buffer, or to allocate its own buffer
    for (ii = 0; ii < aNumBuffers; ii++)
    {

        OsclAny **pmempoolentry = aIsThisInputBuffer ? &((in_ctrl_struct_ptr[ii]).pMemPoolEntry) : &((out_ctrl_struct_ptr[ii]).pMemPoolEntry) ;

        int32 errcode = OsclErrNone;
        // get the address where the buf hdr ptr will be stored
        errcode = AllocateChunkFromMemPool(*pmempoolentry , aMemPool, aAllocSize);
        if ((OsclErrNone != errcode) || (*pmempoolentry == NULL))
        {
            if (OsclErrNoResources == errcode)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::ProvideBuffersToComponent ->allocate() failed for no mempool chunk available", iName.Str()));
            }
            else
            {
                // General error
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::ProvideBuffersToComponent ->allocate() failed due to some general error", iName.Str()));

                ReportErrorEvent(PVMFFailure);
                ChangeNodeState(EPVMFNodeError);
            }


            return false;
        }

        if (aUseBufferOK)
        {
            // Buffers are allocated outside OMX component.

            // In case of input buffers, InputBufCtrlStruct - the omx buffer header pointer is followed by a MediaDataSharedPtr
            //  which is used to ensure proper unbinding of the input messages. The buffer itself is either:
            //      a) Movable buffers supported - input buffer is allocated upstream (and the ptr to the buffer
            //          is a dummy pointer to which the component does not pay attention - PV OMX component)
            //      b) No-movable buffers supported - buffers are allocated as part of the memory pool

            uint8 *pB = NULL;


            // in case of input buffers, initialize also MediaDataSharedPtr structure
            if (aIsThisInputBuffer)
            {

                InputBufCtrlStruct *temp = (InputBufCtrlStruct *) & in_ctrl_struct_ptr[ii];
                //oscl_memset(&(temp->pMediaData), 0, sizeof(PVMFSharedMediaDataPtr));
                // initialize the structure
                OSCL_PLACEMENT_NEW(&(temp->pMediaData), PVMFSharedMediaDataPtr(NULL, NULL));

                //
                pB = (uint8 *) in_ctrl_struct_ptr[ii].pMemPoolEntry;

                err = OMX_UseBuffer(iOMXDecoder,    // hComponent
                                    &(temp->pBufHdr),       // address where ptr to buffer header will be stored
                                    aPortIndex,             // port index (for port for which buffer is provided)
                                    temp,                   // App. private data = pointer to ctrl structure associated with the buffer
                                    //              to have a context when component returns with a callback (i.e. to know
                                    //              to free a buffer into the memory pool
                                    (OMX_U32)aActualBufferSize,     // buffer size
                                    pB);                        // buffer data ptr




            }
            else
            {
                if (ipExternalOutputBufferAllocatorInterface)
                {
                    // Actual buffer memory will be allocated outside the node from
                    // an external output buffer allocator interface

                    uint8 *pB = (uint8*) ipFixedSizeBufferAlloc->allocate();
                    if (NULL == pB)
                    {
                        //  error
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::ProvideBuffersToComponent ->allocate() failed due to some general error", iName.Str()));
                        ReportErrorEvent(PVMFFailure);
                        ChangeNodeState(EPVMFNodeError);
                        return false;
                    }

                    OutputBufCtrlStruct *temp = (OutputBufCtrlStruct *) & out_ctrl_struct_ptr[ii];

                    err = OMX_UseBuffer(iOMXDecoder,    // hComponent
                                        &(temp->pBufHdr),       // address where ptr to buffer header will be stored
                                        aPortIndex,             // port index (for port for which buffer is provided)
                                        temp,                   // App. private data = pointer to ctrl structure associated with the buffer
                                        //              to have a context when component returns with a callback (i.e. to know
                                        //              what to free etc.
                                        (OMX_U32)aActualBufferSize,     // buffer size
                                        pB);                        // buffer data ptr




                }
                else
                {
                    OutputBufCtrlStruct *temp = (OutputBufCtrlStruct *) & out_ctrl_struct_ptr[ii];

                    pB = (uint8 *) out_ctrl_struct_ptr[ii].pMemPoolEntry;


                    err = OMX_UseBuffer(iOMXDecoder,    // hComponent
                                        &(temp->pBufHdr),       // address where ptr to buffer header will be stored
                                        aPortIndex,             // port index (for port for which buffer is provided)
                                        temp,                   // App. private data = pointer to ctrl structure associated with the buffer
                                        //              to have a context when component returns with a callback (i.e. to know
                                        //              what to free etc.
                                        (OMX_U32)aActualBufferSize,     // buffer size
                                        pB);                        // buffer data ptr



                }

            }


        }
        else
        {
            // the component must allocate its own buffers.
            if (aIsThisInputBuffer)
            {

                InputBufCtrlStruct *temp = (InputBufCtrlStruct *) & in_ctrl_struct_ptr[ii];
                // make sure to init all this to NULL
                //oscl_memset(&(temp->pMediaData), 0, sizeof(PVMFSharedMediaDataPtr));
                OSCL_PLACEMENT_NEW(&(temp->pMediaData), PVMFSharedMediaDataPtr(NULL, NULL));

                err = OMX_AllocateBuffer(iOMXDecoder,
                                         &(temp->pBufHdr),
                                         aPortIndex,
                                         temp,                   // App. private data = pointer to ctrl structure associated with the buffer
                                         (OMX_U32)aActualBufferSize);



            }
            else
            {
                OutputBufCtrlStruct *temp = (OutputBufCtrlStruct *) & out_ctrl_struct_ptr[ii];
                err = OMX_AllocateBuffer(iOMXDecoder,
                                         &(temp->pBufHdr),
                                         aPortIndex,
                                         temp,                   // App. private data = pointer to ctrl structure associated with the buffer
                                         (OMX_U32)aActualBufferSize);



            }

        }

        if (err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::ProvideBuffersToComponent() Problem using/allocating a buffer", iName.Str()));


            return false;
        }

    }

    if (aIsThisInputBuffer)
    {
        for (ii = 0; ii < aNumBuffers; ii++)
        {
            // after initializing the buffer hdr ptrs, return them
            // to the mempool
            aMemPool->deallocate((in_ctrl_struct_ptr[ii]).pMemPoolEntry);
        }
    }
    else
    {
        for (ii = 0; ii < aNumBuffers; ii++)
        {
            // after initializing the buffer hdr ptrs, return them
            // to the mempool
            aMemPool->deallocate((out_ctrl_struct_ptr[ii]).pMemPoolEntry);
        }
    }


    // set the flags
    if (aIsThisInputBuffer)
    {
        iInputBuffersFreed = false;
    }
    else
    {
        iOutputBuffersFreed = false;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::ProvideBuffersToComponent() done", iName.Str()));
    return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::FreeBuffersFromComponent(OsclMemPoolFixedChunkAllocator *aMemPool, // allocator
        uint32 aAllocSize,   // size to allocate from pool (hdr only or hdr+ buffer)
        uint32 aNumBuffers,    // number of buffers
        uint32 aPortIndex,      // port idx
        bool    aIsThisInputBuffer      // is this input or output
                                                 )
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::FreeBuffersToComponent() enter", iName.Str()));

    uint32 ii = 0;
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // Now, go through all buffers and tell component to free them
    for (ii = 0; ii < aNumBuffers; ii++)
    {

        OsclAny **pmempoolentry = aIsThisInputBuffer ? &((in_ctrl_struct_ptr[ii]).pMemPoolEntry) : &((out_ctrl_struct_ptr[ii]).pMemPoolEntry) ;

        int32 errcode = OsclErrNone;
        // get the address where the buf hdr ptr will be stored

        errcode = AllocateChunkFromMemPool(*pmempoolentry, aMemPool, aAllocSize);
        if ((OsclErrNone != errcode) || (*pmempoolentry == NULL))
        {
            if (OsclErrNoResources == errcode)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::FreeBuffersFromComponent ->allocate() failed for no mempool chunk available", iName.Str()));
            }
            else
            {
                // General error
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::FreeBuffersFromComponent ->allocate() failed due to some general error", iName.Str()));

                ReportErrorEvent(PVMFFailure);
                ChangeNodeState(EPVMFNodeError);
            }

            return false;
        }
        // to maintain correct count
        aMemPool->notifyfreechunkavailable((*this), (OsclAny*) aMemPool);

        if (aIsThisInputBuffer)
        {

            iNumOutstandingInputBuffers++;
            // get the buf hdr pointer
            InputBufCtrlStruct *temp = (InputBufCtrlStruct *) & in_ctrl_struct_ptr[ii];
            err = OMX_FreeBuffer(iOMXDecoder,
                                 aPortIndex,
                                 temp->pBufHdr);

        }
        else
        {
            if (ipExternalOutputBufferAllocatorInterface)
            {
                //Deallocate the output buffer memory that was allocated outside the node
                //using an external output buffer allocator interface

                iNumOutstandingOutputBuffers++;
                OutputBufCtrlStruct *temp = (OutputBufCtrlStruct *) & out_ctrl_struct_ptr[ii];
                ipFixedSizeBufferAlloc->deallocate((OsclAny*) temp->pBufHdr->pBuffer);

                err = OMX_FreeBuffer(iOMXDecoder,
                                     aPortIndex,
                                     temp->pBufHdr);
            }
            else
            {
                iNumOutstandingOutputBuffers++;
                OutputBufCtrlStruct *temp = (OutputBufCtrlStruct *) & out_ctrl_struct_ptr[ii];
                err = OMX_FreeBuffer(iOMXDecoder,
                                     aPortIndex,
                                     temp->pBufHdr);
            }
        }

        if (err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::FreeBuffersFromComponent() Problem freeing a buffer", iName.Str()));

            return false;
        }

    }


    if (aIsThisInputBuffer)
    {
        for (ii = 0; ii < aNumBuffers; ii++)
        {
            // after initializing the buffer hdr ptrs, return them
            // to the mempool
            aMemPool->deallocate((in_ctrl_struct_ptr[ii]).pMemPoolEntry);
        }
    }
    else
    {
        for (ii = 0; ii < aNumBuffers; ii++)
        {
            // after initializing the buffer hdr ptrs, return them
            // to the mempool
            aMemPool->deallocate((out_ctrl_struct_ptr[ii]).pMemPoolEntry);
        }
    }


    // mark buffers as freed (so as not to do it twice)
    if (aIsThisInputBuffer)
    {

        if (in_ctrl_struct_ptr)
        {
            oscl_free(in_ctrl_struct_ptr);
            in_ctrl_struct_ptr = NULL;
        }



        iInputBuffersFreed = true;
    }
    else
    {
        if (out_ctrl_struct_ptr)
        {
            oscl_free(out_ctrl_struct_ptr);
            out_ctrl_struct_ptr = NULL;
        }


        iOutputBuffersFreed = true;

        if (ipExternalOutputBufferAllocatorInterface)
        {
            ipExternalOutputBufferAllocatorInterface->removeRef();
            ipExternalOutputBufferAllocatorInterface = NULL;
        }
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::FreeBuffersFromComponent() done", iName.Str()));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// This function handles the event of OMX component state change
/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::HandleComponentStateChange(OMX_U32 decoder_state)
{
    switch (decoder_state)
    {
        case OMX_StateIdle:
        {
            iCurrentDecoderState = OMX_StateIdle;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleComponentStateChange: OMX_StateIdle reached", iName.Str()));

            //  this state can be reached either going from OMX_Loaded->OMX_Idle (preparing)
            //  or going from OMX_Executing->OMX_Idle (stopping)

            // reset the flag requiring config data processing by the component (even if it has been set previously) -
            // when we next send config data (if a data format requires it) - we may set this flag to true
            iIsConfigDataProcessingCompletionNeeded = false;
            iIsThereMoreConfigDataToBeSent = false;
            iConfigDataBuffersOutstanding = 0; // no need to keep track of this any more
            iConfigDataBytesProcessed = 0;

            if (PVMF_GENERIC_NODE_PREPARE == iCurrentCommand.iCmd)
            {
                iProcessingState = EPVMFOMXBaseDecNodeProcessingState_InitDecoder;
                SetState(EPVMFNodePrepared);
                //Complete the pending command and reschedule
                CommandComplete(iCurrentCommand, PVMFSuccess);
                Reschedule();
            }
            else if (PVMF_GENERIC_NODE_STOP == iCurrentCommand.iCmd)
            {
                // if we are stopped, we won't start until the node gets DoStart command.
                //  in this case, we are ready to start sending buffers
                if (iProcessingState == EPVMFOMXBaseDecNodeProcessingState_Stopping)
                    iProcessingState = EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode;
                // if the processing state was not stopping, leave the state as it was (continue port reconfiguration)
                SetState(EPVMFNodePrepared);
                iStopCommandWasSentToComponent = false;
                //Complete the pending command and reschedule
                CommandComplete(iCurrentCommand, PVMFSuccess);
                Reschedule();
            }
            else if (PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd)
            {
                // State change to Idle was initiated due to Reset. First need to reach idle, and then loaded
                // Once Idle is reached, we need to initiate idle->loaded transition
                iStopInResetMsgSent = false;
                Reschedule();
            }
            break;
        }//end of case OMX_StateIdle

        case OMX_StateExecuting:
        {
            iCurrentDecoderState = OMX_StateExecuting;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleComponentStateChange: OMX_StateExecuting reached", iName.Str()));

            // this state can be reached going from OMX_Idle -> OMX_Executing (preparing)
            //  or going from OMX_Pause -> OMX_Executing (coming from pause)
            //  either way, this is a response to "DoStart" command

            if (PVMF_GENERIC_NODE_START == iCurrentCommand.iCmd)
            {
                SetState(EPVMFNodeStarted);
                //Complete the pending command and reschedule
                CommandComplete(iCurrentCommand, PVMFSuccess);
                Reschedule();
            }

            break;
        }//end of case OMX_StateExecuting

        case OMX_StatePause:
        {
            iCurrentDecoderState = OMX_StatePause;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleComponentStateChange: OMX_StatePause reached", iName.Str()));

            // if we are paused, we won't start until the node gets DoStart command.
            //  in this case, we are ready to start sending buffers
            // if the processing state was not pausing, leave the state as it was (continue port reconfiguration after going out of pause)

            if (iProcessingState == EPVMFOMXBaseDecNodeProcessingState_Pausing)
            {
                iProcessingState = EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode;
                // we need to send more config buffers
                if (iIsThereMoreConfigDataToBeSent)
                {
                    iProcessingState = EPVMFOMXBaseDecNodeProcessingState_InitDecoder;
                }
            }

            //  This state can be reached going from OMX_Executing-> OMX_Pause
            if (PVMF_GENERIC_NODE_PAUSE == iCurrentCommand.iCmd)
            {


                SetState(EPVMFNodePaused);
                iPauseCommandWasSentToComponent = false;
                //Complete the pending command and reschedule
                CommandComplete(iCurrentCommand, PVMFSuccess);
                Reschedule();
            }

            break;
        }//end of case OMX_StatePause

        case OMX_StateLoaded:
        {
            iCurrentDecoderState = OMX_StateLoaded;

            //  this state can be reached only going from OMX_Idle ->OMX_Loaded (stopped to reset)
            //

            // reset the flag requiring config data processing by the component (even if it has been set previously) -
            // when we next send config data (if a data format requires it) - we may set this flag to true
            iIsConfigDataProcessingCompletionNeeded = false;
            iIsThereMoreConfigDataToBeSent = false;
            iConfigDataBuffersOutstanding = 0; // no need to keep track of this any more
            iConfigDataBytesProcessed = 0;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleComponentStateChange: OMX_StateLoaded reached", iName.Str()));
            //Check if command's response is pending
            if (PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd)
            {

                // move this here
                if (iInPort)
                {
                    OSCL_DELETE(((PVMFOMXDecPort*)iInPort));
                    iInPort = NULL;
                }

                if (iOutPort)
                {
                    OSCL_DELETE(((PVMFOMXDecPort*)iOutPort));
                    iOutPort = NULL;
                }

                iDataIn.Unbind();

                // Reset the metadata key list
                iAvailableMetadataKeys.clear();


                iEndOfDataReached = false;
                iIsEOSSentToComponent = false;
                iIsEOSReceivedFromComponent = false;

                if (iOMXComponentUsesFullAVCFrames)
                {
                    iNALCount = 0;
                    oscl_memset(iNALSizeArray, 0, sizeof(uint32) * MAX_NAL_PER_FRAME); // 100 is max number of NALs
                }

                // reset dynamic port reconfig flags - no point continuing with port reconfig
                // if we start again - we'll have to do prepare and send new config etc.
                iSecondPortReportedChange = false;
                iDynamicReconfigInProgress = false;


                iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Idle;
                //logoff & go back to Created state.
                SetState(EPVMFNodeIdle);
                CommandComplete(iCurrentCommand, PVMFSuccess);
                iResetInProgress = false;
                iResetMsgSent = false;
            }

            break;
        }//end of case OMX_StateLoaded

        case OMX_StateInvalid:
        default:
        {
            iCurrentDecoderState = OMX_StateInvalid;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::HandleComponentStateChange: OMX_StateInvalid reached", iName.Str()));

            if (iOMXDecoder == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::HandleComponentStateChange: cleanup already done. Do nothing", iName.Str()));
                return;
            }
            //Clearup decoder
            DeleteOMXBaseDecoder();
            //Stop using OMX component
            iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Idle;

            if (PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd)
            {
                //delete all ports and notify observer.
                if (iInPort)
                {
                    OSCL_DELETE(((PVMFOMXDecPort*)iInPort));
                    iInPort = NULL;
                }

                if (iOutPort)
                {
                    OSCL_DELETE(((PVMFOMXDecPort*)iOutPort));
                    iOutPort = NULL;
                }

                iDataIn.Unbind();

                // Reset the metadata key list
                iAvailableMetadataKeys.clear();

                iEndOfDataReached = false;
                iIsEOSSentToComponent = false;
                iIsEOSReceivedFromComponent = false;

                if (iOMXComponentUsesFullAVCFrames)
                {
                    iNALCount = 0;
                    oscl_memset(iNALSizeArray, 0, sizeof(uint32) * MAX_NAL_PER_FRAME); // 100 is max number of NALs
                }

                // reset dynamic port reconfig flags - no point continuing with port reconfig
                // if we start again - we'll have to do prepare and send new config etc.
                iSecondPortReportedChange = false;
                iDynamicReconfigInProgress = false;

                // logoff & go back to Created state.
                SetState(EPVMFNodeIdle);
                CommandComplete(iCurrentCommand, PVMFSuccess);
                iResetInProgress = false;
                iResetMsgSent = false;
            }
            else if (PVMF_GENERIC_NODE_COMMAND_INVALID != iCurrentCommand.iCmd)
            {
                SetState(EPVMFNodeError);
                CommandComplete(iCurrentCommand, PVMFErrResource);
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::HandleComponentStateChange: ERROR state transition event while OMX client does NOT have any pending state transition request", iName.Str()));
                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrResourceConfiguration);
            }

            break;
        }//end of case OMX_StateInvalid

    }//end of switch(decoder_state)

    // OMX does not support cancelling the command.
    // Once initiated, an async command to the OMX component will complete.
    // So if the node is waiting for any pending cancel, cancel it now with Failure.

    if (IsCommandInProgress(iCancelCommand))
    {
        CommandComplete(iCancelCommand, PVMFFailure);
        return;
    }
}






/////////////////////////////////////////////////////////////////////////////
////////////////////// CALLBACK PROCESSING FOR EMPTY BUFFER DONE - input buffer was consumed
/////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE PVMFOMXBaseDecNode::EmptyBufferDoneProcessing(OMX_OUT OMX_HANDLETYPE aComponent,
        OMX_OUT OMX_PTR aAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer)
{
    OSCL_UNUSED_ARG(aComponent);
    OSCL_UNUSED_ARG(aAppData);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::EmptyBufferDoneProcessing: In", iName.Str()));

    OSCL_ASSERT((void*) aComponent == (void*) iOMXDecoder); // component should match the component
    OSCL_ASSERT(aAppData == (OMX_PTR)(this));       // AppData should represent this node ptr

    // first, get the buffer "context", i.e. pointer to application private data that contains the
    // address of the mempool buffer (so that it can be released)
    InputBufCtrlStruct *pContext = (InputBufCtrlStruct *)(aBuffer->pAppPrivate);

    if (iConfigDataBuffersOutstanding > 0)
    {
        iConfigDataBuffersOutstanding--;

        // it's possible there are many config buffers. We shouldn't re-enable processing until
        // all config buffers have been sent and processed
        if ((iConfigDataBuffersOutstanding == 0) && (!iIsThereMoreConfigDataToBeSent))
        {
            // reset the flag requiring config data processing by the component
            iIsConfigDataProcessingCompletionNeeded = false;
            iConfigDataBytesProcessed = 0;

        }
    }

    // if a buffer is not empty, log a msg, but release anyway
    if (aBuffer->nFilledLen > 0)
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::EmptyBufferDoneProcessing: Input buffer returned non-empty with %d bytes still in it", iName.Str(), aBuffer->nFilledLen));


    }


    // input buffer is to be released,
    // refcount needs to be decremented (possibly - the input msg associated with the buffer will be unbound)
    // NOTE: in case of "moveable" input buffers (passed into component without copying), unbinding decrements a refcount which eventually results
    //          in input message being released back to upstream mempool once all its fragments are returned
    //      in case of input buffers passed into component by copying, unbinding has no effect
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::EmptyBufferDoneProcessing: Release input buffer with Ticks=%d (with %d refcount remaining of input message)", iName.Str(), aBuffer->nTimeStamp, (pContext->pMediaData).get_count() - 1));


    (pContext->pMediaData).Unbind();


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::EmptyBufferDoneProcessing: Release input buffer %x back to mempool - pointing to buffer %x", iName.Str(), pContext, aBuffer->pBuffer));

    iInBufMemoryPool->deallocate((OsclAny *) pContext->pMemPoolEntry);


    // the OMX spec says that no error is to be returned
    return OMX_ErrorNone;

}



/////////////////////////////////////////////////////////////////////////////
////////////////////// CALLBACK PROCESSING FOR FILL BUFFER DONE - output buffer is ready
/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF OMX_ERRORTYPE PVMFOMXBaseDecNode::FillBufferDoneProcessing(OMX_OUT OMX_HANDLETYPE aComponent,
        OMX_OUT OMX_PTR aAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer)
{
    OSCL_UNUSED_ARG(aComponent);
    OSCL_UNUSED_ARG(aAppData);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::FillBufferDoneProcessing: In", iName.Str()));

    OSCL_ASSERT((void*) aComponent == (void*) iOMXDecoder); // component should match the component
    OSCL_ASSERT(aAppData == (OMX_PTR)(this));       // AppData should represent this node ptr

    // first, get the buffer "context", i.e. pointer to application private data that contains the
    // address of the mempool buffer (so that it can be released)
    OutputBufCtrlStruct *pContext = (OutputBufCtrlStruct*) aBuffer->pAppPrivate;


    // check for EOS flag
    if ((aBuffer->nFlags & OMX_BUFFERFLAG_EOS))
    {
        // EOS received - enable sending EOS msg
        iIsEOSReceivedFromComponent = true;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::FillBufferDoneProcessing: Output buffer has EOS set", iName.Str()));
    }

    // if a buffer is empty, or if it should not be sent downstream (say, due to state change)
    // release the buffer back to the pool
    if ((aBuffer->nFilledLen == 0) || (iDoNotSendOutputBuffersDownstreamFlag == true))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::FillBufferDoneProcessing: Release output buffer %x pointing to buffer %x back to mempool - buffer empty or not to be sent downstream", iName.Str(), pContext, pContext->pMemPoolEntry));

        iOutBufMemoryPool->deallocate(pContext->pMemPoolEntry);

    }
    else
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::FillBufferDoneProcessing: Output frame %d received", iName.Str(), iFrameCounter++));

        // get pointer to actual buffer data
        uint8 *pBufdata = ((uint8*) aBuffer->pBuffer);
        // move the data pointer based on offset info
        pBufdata += aBuffer->nOffset;

        iOutTimeStamp = ConvertOMXTicksIntoTimestamp(aBuffer->nTimeStamp);


        ipPrivateData = (OsclAny *) aBuffer->pPlatformPrivate; // record the pointer

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::FillBufferDoneProcessing: Wrapping buffer %x of size %d", iName.Str(), pBufdata, aBuffer->nFilledLen));
        // wrap the buffer into the MediaDataImpl wrapper, and queue it for sending downstream
        // wrapping will create a refcounter. When refcounter goes to 0 i.e. when media data
        // is released in downstream components, the custom deallocator will automatically release the buffer back to the
        //  mempool. To do that, the deallocator needs to have info about Context
        // NOTE: we had to wait until now to wrap the buffer data because we only know
        //          now where the actual data is located (based on buffer offset)
        OsclSharedPtr<PVMFMediaDataImpl> MediaDataOut = WrapOutputBuffer(pBufdata, (uint32)(aBuffer->nFilledLen), pContext->pMemPoolEntry);

        // if you can't get the MediaDataOut, release the buffer back to the pool
        if (MediaDataOut.GetRep() == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::FillBufferDoneProcessing: Problem wrapping buffer %x of size %d - releasing the buffer", iName.Str(), pBufdata, aBuffer->nFilledLen));

            iOutBufMemoryPool->deallocate(pContext->pMemPoolEntry);
        }
        else
        {

            // if there's a problem queuing output buffer, MediaDataOut will expire at end of scope and
            // release buffer back to the pool, (this should not be the case)
            if (QueueOutputBuffer(MediaDataOut, aBuffer->nFilledLen))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::FillBufferDoneProcessing: Buffer %x of size %d queued - reschedule the node to send out", iName.Str(), pBufdata, aBuffer->nFilledLen));

                // if queing went OK,
                // re-schedule the node so that outgoing queue can be emptied (unless the outgoing port is busy)
                if ((iOutPort) && !(iOutPort->IsConnectedPortBusy()))
                    Reschedule();
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::FillBufferDoneProcessing: Problem queing buffer %x of size %d - releasing the buffer", iName.Str(), pBufdata, aBuffer->nFilledLen));
            }


        }

    }
    // the OMX spec says that no error is to be returned
    return OMX_ErrorNone;

}

//////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Attach a MediaDataImpl wrapper (refcount, deallocator etc.)
/////////////////////////////// to the output buffer /////////////////////////////////////////
OSCL_EXPORT_REF OsclSharedPtr<PVMFMediaDataImpl> PVMFOMXBaseDecNode::WrapOutputBuffer(uint8 *pData, uint32 aDataLen, OsclAny *pContext)
{
    // wrap output buffer into a mediadataimpl
    uint32 aligned_class_size = oscl_mem_aligned_size(sizeof(PVMFSimpleMediaBuffer));
    uint32 aligned_cleanup_size = oscl_mem_aligned_size(sizeof(PVOMXDecBufferSharedPtrWrapperCombinedCleanupDA));
    uint32 aligned_refcnt_size = oscl_mem_aligned_size(sizeof(OsclRefCounterDA));
    uint8 *my_ptr = (uint8*) oscl_malloc(aligned_refcnt_size + aligned_cleanup_size + aligned_class_size);

    if (my_ptr == NULL)
    {
        OsclSharedPtr<PVMFMediaDataImpl> null_buff(NULL, NULL);
        return null_buff;
    }
    // create a deallocator and pass the buffer_allocator to it as well as pointer to data that needs to be returned to the mempool
    PVOMXDecBufferSharedPtrWrapperCombinedCleanupDA *cleanup_ptr =
        OSCL_PLACEMENT_NEW(my_ptr + aligned_refcnt_size, PVOMXDecBufferSharedPtrWrapperCombinedCleanupDA(iOutBufMemoryPool, pContext));

    //ModifiedPvciBufferCombinedCleanup* cleanup_ptr = OSCL_PLACEMENT_NEW(my_ptr + aligned_refcnt_size,ModifiedPvciBufferCombinedCleanup(aOutput.GetRefCounter()) );

    // create the ref counter after the cleanup object (refcount is set to 1 at creation)
    OsclRefCounterDA *my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterDA(my_ptr, cleanup_ptr));

    my_ptr += aligned_refcnt_size + aligned_cleanup_size;

    PVMFMediaDataImpl* media_data_ptr = OSCL_PLACEMENT_NEW(my_ptr, PVMFSimpleMediaBuffer((void *) pData, // ptr to data
                                        aDataLen, // capacity
                                        my_refcnt));   // ref counter

    OsclSharedPtr<PVMFMediaDataImpl> MediaDataImplOut(media_data_ptr, my_refcnt);

    MediaDataImplOut->setMediaFragFilledLen(0, aDataLen);

    return MediaDataImplOut;

}
//////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::SendBeginOfMediaStreamCommand()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendBeginOfMediaStreamCommand() In", iName.Str()));

    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();
    // Set the formatID, timestamp, sequenceNumber clipId and streamID for the media message
    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_BOS_FORMAT_ID);
    sharedMediaCmdPtr->setClipID(iCurrentClipId);
    //reset the sequence number
    uint32 seqNum = 0;
    sharedMediaCmdPtr->setSeqNum(seqNum);
    sharedMediaCmdPtr->setStreamID(iStreamID);

    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);
    if (iOutPort->QueueOutgoingMsg(mediaMsgOut) != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::SendBeginOfMediaStreamCommand() Outgoing queue busy", iName.Str()));
        return false;
    }

    iSendBOS = false;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendBeginOfMediaStreamCommand() BOS Sent StreamID %d clipId %d", iName.Str(), iStreamID, iCurrentClipId));
    return true;
}
////////////////////////////////////
bool PVMFOMXBaseDecNode::SendEndOfTrackCommand(void)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendEndOfTrackCommand() In", iName.Str()));

    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();

    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_EOS_FORMAT_ID);

    // Set the timestamp
    sharedMediaCmdPtr->setTimestamp(iEndOfDataTimestamp);
    // set the clip id
    sharedMediaCmdPtr->setClipID(iCurrentClipId);
    // Set Streamid
    sharedMediaCmdPtr->setStreamID(iStreamID);

    // Set the sequence number
    sharedMediaCmdPtr->setSeqNum(iSeqNum++);

    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);
    if (iOutPort->QueueOutgoingMsg(mediaMsgOut) != PVMFSuccess)
    {
        // this should not happen because we check for queue busy before calling this function
        return false;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::SendEndOfTrackCommand() clipId %d Out", iName.Str(), iCurrentClipId));
    return true;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoInit()
{
    return PVMFSuccess;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoPrepare()
{
    if (EPVMFNodePrepared == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXBaseDecNode::DoPrepare() already in Prepared state"));
        return PVMFSuccess;
    }

    OMX_ERRORTYPE err = OMX_ErrorNone;
    Oscl_Vector<OMX_STRING, OsclMemAllocator> roles;

    if (NULL == iInPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::DoPrepare() Input port not initialized", iName.Str()));
        return PVMFFailure;
    }

    // Check format of input data
    PVMFFormatType format = ((PVMFOMXDecPort*)iInPort)->iFormat;
    // AAC
    if (format == PVMF_MIME_MPEG4_AUDIO ||
            format == PVMF_MIME_3640 ||
            format == PVMF_MIME_LATM ||
            format == PVMF_MIME_ADIF ||
            format == PVMF_MIME_ASF_MPEG4_AUDIO ||
            format == PVMF_MIME_AAC_SIZEHDR)
    {
        roles.push_back((OMX_STRING)"audio_decoder.aac");
    }
    // AMR
    else if (format == PVMF_MIME_AMR_IF2 ||
             format == PVMF_MIME_AMR_IETF ||
             format == PVMF_MIME_AMR)
    {
        roles.push_back((OMX_STRING)"audio_decoder.amrnb");
    }
    else if (format == PVMF_MIME_AMRWB_IETF ||
             format == PVMF_MIME_AMRWB)
    {
        roles.push_back((OMX_STRING)"audio_decoder.amrwb");
    }
    else if (format == PVMF_MIME_MP3)
    {
        roles.push_back((OMX_STRING)"audio_decoder.mp3");
    }
    else if (format ==  PVMF_MIME_WMA)
    {
        roles.push_back((OMX_STRING)"audio_decoder.wma");
    }
    else if (format ==  PVMF_MIME_REAL_AUDIO)
    {
        roles.push_back((OMX_STRING)"audio_decoder.ra");
    }
    else if (format ==  PVMF_MIME_H264_VIDEO ||
             format == PVMF_MIME_H264_VIDEO_MP4 ||
             format == PVMF_MIME_H264_VIDEO_RAW)
    {
        roles.push_back((OMX_STRING)"video_decoder.avc");
    }
    else if (format ==  PVMF_MIME_M4V)
    {
        roles.push_back((OMX_STRING)"video_decoder.mpeg4");
    }
    else if (format ==  PVMF_MIME_H2631998 ||
             format == PVMF_MIME_H2632000)
    {
        roles.push_back((OMX_STRING)"video_decoder.h263");
    }
    else if (format ==  PVMF_MIME_WMV)
    {
        roles.push_back((OMX_STRING)"video_decoder.vc1");
        roles.push_back((OMX_STRING)"video_decoder.wmv");
    }
    else if (format ==  PVMF_MIME_REAL_VIDEO)
    {
        roles.push_back((OMX_STRING)"video_decoder.rv");
    }
    else
    {
        // Illegal codec specified.
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::DoPrepare() Input port format other then codec type", iName.Str()));
        return PVMFErrArgument;
    }


    /* Set callback structure */
    iCallbacks.EventHandler    = CallbackEventHandler; //event_handler;
    iCallbacks.EmptyBufferDone = CallbackEmptyBufferDone; //empty_buffer_done;
    iCallbacks.FillBufferDone  = CallbackFillBufferDone; //fill_buffer_done;

    //Buffer large enough to store any kind of output parameter struct.
    char                  outputParameters[sizeof(VideoOMXConfigParserOutputs)];
    OMXConfigParserInputs inputParameters;


    inputParameters.inPtr = (uint8*)((PVMFOMXDecPort*)iInPort)->iTrackConfig;
    inputParameters.inBytes = (int32)((PVMFOMXDecPort*)iInPort)->iTrackConfigSize;

    if (inputParameters.inBytes == 0 || inputParameters.inPtr == NULL)
    {
        // in case of following formats - config codec data is expected to
        // be present in the query. If not, config parser cannot be called

        if (format == PVMF_MIME_WMA ||
                format == PVMF_MIME_MPEG4_AUDIO ||
                format == PVMF_MIME_3640 ||
                format == PVMF_MIME_LATM ||
                format == PVMF_MIME_ADIF ||
                format == PVMF_MIME_ASF_MPEG4_AUDIO ||
                format == PVMF_MIME_AAC_SIZEHDR ||
                format == PVMF_MIME_H264_VIDEO ||
                format == PVMF_MIME_H264_VIDEO_MP4 ||
                format == PVMF_MIME_M4V ||
                format == PVMF_MIME_WMV ||
                format == PVMF_MIME_REAL_VIDEO ||
                format == PVMF_MIME_REAL_AUDIO)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::DoPrepare() Codec Config data is not present", iName.Str()));
            return PVMFErrNoResources;
        }
    }

    // in case of ASF_MPEG4 audio need to create aac config header
    if (format == PVMF_MIME_ASF_MPEG4_AUDIO)
    {

        if (!CreateAACConfigDataFromASF(inputParameters.inPtr, inputParameters.inBytes, &iAACConfigData[0], iAACConfigDataLength))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::DoPrepare() Error in AAC Codec Config data", iName.Str()));
            return PVMFErrNoResources;
        }

        inputParameters.inPtr = &iAACConfigData[0];
        inputParameters.inBytes = iAACConfigDataLength;
    }



    OMX_BOOL status = OMX_FALSE;
    Oscl_Vector<OMX_STRING, OsclMemAllocator>::iterator role;

    iIsVC1 = false;
    iIsVC1AdvancedProfile = false; /* default*/



    OMX_U32 total_num_comps = 0;
    OMX_U32 *num_comps_for_role = NULL;
    OMX_U32 num_roles = roles.size(); // this is typically 1, but can be more than 1
    OMX_STRING *CompOfRole = NULL;
    OMX_U32 cumulative_comps = 0;
    OMX_U32 ii = 0;

    bool isTrackSelectionHelperUsed = false;

    // Do we have a track selection helper list?
    if (iOMXPreferredComponentOrderVec.size() > 0)
    {
        isTrackSelectionHelperUsed = true;
        // If we have the list - no need to build it from scratch (the list of
        // components that support the role(s) was built during
        // track selection and verification for each track.
        // Also, each omx component in the list has already been verified to support the track

        total_num_comps = iOMXPreferredComponentOrderVec.size();

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "%s::DoPrepare(): USING PREFERRED LIST OF COMPONENTS FROM THE ENGINE TRACK SELECTION HELPER - There are %d component for the given format ", iName.Str(), iOMXPreferredComponentOrderVec.size()));


    }
    else // ELSE no track selection helper
    {
        // we need to build a list of components that support the role(s)

        num_comps_for_role = (OMX_U32*) oscl_malloc(num_roles * sizeof(OMX_U32));
        if (num_comps_for_role == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for num array failed"));
            return PVMFErrNoMemory;
        }

        // get the total number of components first that supports N roles
        for (ii = 0, role = roles.begin(); role != roles.end(); role++, ii++)
        {
            inputParameters.cComponentRole = *role;

            // call once to find out the number of components that can fit the given role and save
            err = OMX_MasterGetComponentsOfRole(inputParameters.cComponentRole, &num_comps_for_role[ii], NULL);
            if (OMX_ErrorNone == err)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "%s::DoPrepare(): There are %d components of role %s ", iName.Str(), num_comps_for_role[ii], *role));
                total_num_comps += num_comps_for_role[ii];
            }
        }
    }

    // Now that we know the number of omx components for the role, we need to construct the list of OMX component names

    // Are there any omx components?
    if (total_num_comps > 0)
    {
        CompOfRole = (OMX_STRING *) oscl_malloc(total_num_comps * sizeof(OMX_STRING));
        if (!CompOfRole)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "%s::DoPrepare(): unable to allocate CompOfRole", iName.Str()));
            if (num_comps_for_role)
                oscl_free(num_comps_for_role);

            return PVMFErrNoMemory;
        }
        // allocate memory for component names in one chunk
        OMX_U8 *memblockomx = (OMX_U8 *) oscl_malloc(total_num_comps * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
        if (memblockomx == NULL)
        {
            oscl_free(CompOfRole);
            if (num_comps_for_role)
                oscl_free(num_comps_for_role);

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "%s::DoPrepare(): unable to allocate CompOfRole[0]", iName.Str()));
            return PVMFErrNoMemory;
        }
        oscl_memset(memblockomx, 0, total_num_comps * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
        for (ii = 0; ii < total_num_comps; ii++)
        {
            CompOfRole[ii] = (OMX_STRING)(memblockomx + (ii * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8)));
        }


        // If this is track selection helper - copy component names from the track selection helper list
        if (isTrackSelectionHelperUsed)
        {
            ii = 0;
            Oscl_Vector<OMX_STRING, OsclMemAllocator>::iterator component_name;
            for (component_name = iOMXPreferredComponentOrderVec.begin();
                    component_name != iOMXPreferredComponentOrderVec.end();
                    component_name++)
            {
                oscl_strncpy(CompOfRole[ii++]
                             , *component_name
                             , PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
            }
        }
        else // No track selection list ...
        {
            // need to create the list by calling OMX_MasterGetComponentsOfRole
            OMX_U32 cumulative_comps = 0;
            for (ii = 0, role = roles.begin(); role != roles.end(); role++, ii++)
            {
                OMX_U32 num = 0;
                // call 2nd time to fill the component names into the array
                err = OMX_MasterGetComponentsOfRole((OMX_STRING) * role, &num, (OMX_U8 **) & CompOfRole[cumulative_comps]);
                if (OMX_ErrorNone == err)
                {
                    cumulative_comps += num_comps_for_role[ii];
                }
            }
        }

    } // end of iftotal_num_comps

    // Now, we have the complete list of omx components that support the role(s)
    // In case of multiple roles (i.e. VC1 + WMV), and in case of track selection helper list,
    // roles of components in the list can be mixed such that VC1 component can be followed by WMV, followed by VC1 etc.
    // In case of no track selection helper and multiple roles, component list that we built contain roles that
    // are ordered such that all VC1 components are listed first, followed by all WMV components etc.

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "%s::DoPrepare(): There are a total of %d components for the given format", iName.Str(), total_num_comps));


    // Go through all the components, and try to instantiate and negotiate with the component.
    // If there are multiple components that fit the role and format, and the track selection helper
    // is used, components are selected based on the order provided by the track selection helper list.
    // If there is no track selection helper, the components are listed and selected in the order they were
    // registered during OMX_MasterInit
    // If one component fails to pass config parser test, fails to be created or fails negotiation of parameters,
    // the next one in the list is selected etc. until one component is found that works OK.


    // init the role

    uint32 role_ctr = 0;

    if (!isTrackSelectionHelperUsed)
    {
        cumulative_comps = num_comps_for_role[role_ctr]; // remember in case of multiple roles
    }

    role = roles.begin();
    inputParameters.cComponentRole = *role;

    for (ii = 0; ii < total_num_comps; ii++)
    {

        inputParameters.cComponentName = CompOfRole[ii];

        // in case of multiple roles for a format - check if we need to update the role
        if (!isTrackSelectionHelperUsed)
        {
            if (ii == cumulative_comps)
            {
                // This happens if say, there are 2 roles(e.g. VC1, WMV) and 3 components for VC1 role, and 4 components for WMV role
                // Then - num_comps_for_role[0] = 3, and num_comps_for_role[1] = 4, total_num_comps = 7
                // If we've processed all components for the current role and previous roles, it's time to update the role

                // NOTE: in case of track selection helper list - this udpate would be meaningless because
                // components in the track selection helper list can have mixed roles (i.e. roles are not ordered)
                role_ctr++;
                role++;
                inputParameters.cComponentRole = *role;
                cumulative_comps += num_comps_for_role[role_ctr];
            }

            status = OMX_MasterConfigParser(&inputParameters, outputParameters);
        }
        else
        {
            // In case of track selection helper list, there is no need to check the config parser. This has already
            // been done during the formation of the list.
            // However, we do need to find out the role of the component in question.

            // assume status is false
            status = OMX_FALSE;
            // find out how many roles the component supports
            OMX_U32 NumRoles;
            err = OMX_MasterGetRolesOfComponent(inputParameters.cComponentName, &NumRoles, NULL);
            if ((err != OMX_ErrorNone) || (NumRoles == 0))
            {
                // in case of a problem, move on to the next component
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::DoPrepare() Problem getting component role for component %s", iName.Str(), inputParameters.cComponentName));

                continue;
            }

            OMX_STRING *CompRoles = NULL;
            CompRoles = (OMX_STRING *) oscl_malloc(NumRoles * sizeof(OMX_STRING *));
            if (NULL == CompRoles)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::DoPrepare() Problem getting component role for component %s", iName.Str(), inputParameters.cComponentName));
                continue;
            }

            OMX_U8 *memblockomx = NULL;

            memblockomx = (OMX_U8 *) oscl_malloc(NumRoles * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
            if (memblockomx == NULL)
            {
                oscl_free(CompRoles);

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "%s::DoPrepare(): unable to allocate CompRole[0]", iName.Str()));
                continue;
            }
            oscl_memset(memblockomx, 0, NumRoles * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
            OMX_U32 jj = 0;
            for (jj = 0; jj < NumRoles; jj++)
            {
                CompRoles[jj] = (OMX_STRING)(memblockomx + (jj * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8)));
            }

            // now call to get all the component roles
            err = OMX_MasterGetRolesOfComponent(inputParameters.cComponentName, &NumRoles, (OMX_U8 **) CompRoles);
            if ((err != OMX_ErrorNone) || (NumRoles == 0))
            {
                // in case of a problem, move on to the next component
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::DoPrepare() Problem getting component role for component %s", iName.Str(), inputParameters.cComponentName));

                oscl_free(memblockomx);
                oscl_free(CompRoles);
                continue;
            }

            // now, match the format role with the component role (there has to be at least 1 match)
            for (role = roles.begin(); role != roles.end(); role++)
            {
                for (jj = 0; jj < NumRoles; jj++)
                {
                    if (oscl_strcmp((OMX_STRING) *role, (OMX_STRING) CompRoles[jj]) == 0)
                    {
                        inputParameters.cComponentRole = *role;
                        status = OMX_TRUE;
                        break;
                    }
                }

                if (status == OMX_TRUE)
                    break;
            }

            oscl_free(memblockomx);
            oscl_free(CompRoles);

            // call the config parser to get the width/height etc.
            status = OMX_MasterConfigParser(&inputParameters, outputParameters);

        }


        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "%s::Initializing OMX component %s and decoder for role %s", iName.Str(), CompOfRole[ii], *role));

        if (status == OMX_TRUE)
        {
            // try to create component
            err = OMX_MasterGetHandle(&iOMXDecoder, (OMX_STRING) inputParameters.cComponentName, (OMX_PTR) this, (OMX_CALLBACKTYPE *) & iCallbacks);

            if ((err == OMX_ErrorNone) && (iOMXDecoder != NULL))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "%s::DoPrepare(): Got Component %s handle ", iName.Str(), inputParameters.cComponentName));
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "%s::DoPrepare(): Cannot get component %s handle, try another component if available", iName.Str(), inputParameters.cComponentName));
                status = OMX_FALSE;
                continue;
            }

            if (!CheckComponentForMultRoles(inputParameters.cComponentName, inputParameters.cComponentRole))
            {
                // error, free handle and try to find a different component
                err = OMX_MasterFreeHandle(iOMXDecoder);
                if (OMX_ErrorNone != err)
                {
                    //Error condition report
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::DoPrepare(): Can't free decoder's handle!", iName.Str()));
                }
                iOMXDecoder = NULL;
                status = OMX_FALSE;
                continue;
            }

            if (!CheckComponentCapabilities(&format, outputParameters))
            {
                // error, free handle and try to find a different component
                err = OMX_MasterFreeHandle(iOMXDecoder);
                if (OMX_ErrorNone != err)
                {
                    //Error condition report
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::DoPrepare(): Can't free decoder's handle!", iName.Str()));
                }
                iOMXDecoder = NULL;
                status = OMX_FALSE;
                continue;
            }

            // found a component, and it passed all tests.   no need to continue.
            // in case of multiple roles per Mime type (i.e. VC1 vs WMV)
            // we need to check whether the selected component supports VC1 role or the WMV role
            if (0 == oscl_strcmp(*role, "video_decoder.vc1"))
            {
                iIsVC1 = true;
            }

            break;
        }
        else
        {
            status = OMX_FALSE;
        }

    } // end of for( total_num_comps )

    // whether successful or not, need to free CompOfRoles
    if (CompOfRole)
    {
        if (CompOfRole[0])
        {
            oscl_free(CompOfRole[0]); // only the 0th block was allocated
        }
        oscl_free(CompOfRole);
    }

    if (num_comps_for_role)
        oscl_free(num_comps_for_role);



    // reset the flag requiring config data processing by the component (even if it has been set previously) -
    // when we send config data (if a data format requires it) - we may set this flag to true
    iIsConfigDataProcessingCompletionNeeded = false;
    iIsThereMoreConfigDataToBeSent = false;
    iConfigDataBuffersOutstanding = 0;
    iConfigDataBytesProcessed = 0;

    if (status == OMX_TRUE)
    {
        // check if there was a problem
        if ((err != OMX_ErrorNone) || (iOMXDecoder == NULL))
        {
            // free any memory that may have been allocated during NegotiateParameters
            if (ipExternalOutputBufferAllocatorInterface)
            {
                ipExternalOutputBufferAllocatorInterface->removeRef();
                ipExternalOutputBufferAllocatorInterface = NULL;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::Can't get handle for decoder!", iName.Str()));
            iOMXDecoder = NULL;
            return PVMFErrResource;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::No component can handle role %s !", iName.Str(), inputParameters.cComponentRole));
        iOMXDecoder = NULL;
        return PVMFErrResource;
    }

    // create active objects to handle callbacks in case of multithreaded implementation

    // NOTE: CREATE THE THREADSAFE CALLBACK AOs REGARDLESS OF WHETHER MULTITHREADED COMPONENT OR NOT
    //      If it is not multithreaded, we won't use them
    //      The Flag iIsComponentMultiThreaded decides which mechanism is used for callbacks.
    //      This flag is set by looking at component capabilities (or to true by default)

    if (iThreadSafeHandlerEventHandler)
    {
        OSCL_DELETE(iThreadSafeHandlerEventHandler);
        iThreadSafeHandlerEventHandler = NULL;
    }
    // substitute default parameters: observer(this node),queuedepth(3),nameAO for logging
    // Get the priority of the dec node, and set the threadsafe callback AO priority to 1 higher
    iThreadSafeHandlerEventHandler = OSCL_NEW(EventHandlerThreadSafeCallbackAO, (this, 10, "EventHandlerAO", Priority() + 2));

    if (iThreadSafeHandlerEmptyBufferDone)
    {
        OSCL_DELETE(iThreadSafeHandlerEmptyBufferDone);
        iThreadSafeHandlerEmptyBufferDone = NULL;
    }
    // use queue depth of iNumInputBuffers to prevent deadlock
    iThreadSafeHandlerEmptyBufferDone = OSCL_NEW(EmptyBufferDoneThreadSafeCallbackAO, (this, iNumInputBuffers, "EmptyBufferDoneAO", Priority() + 1));

    if (iThreadSafeHandlerFillBufferDone)
    {
        OSCL_DELETE(iThreadSafeHandlerFillBufferDone);
        iThreadSafeHandlerFillBufferDone = NULL;
    }
    // use queue depth of iNumOutputBuffers to prevent deadlock
    iThreadSafeHandlerFillBufferDone = OSCL_NEW(FillBufferDoneThreadSafeCallbackAO, (this, iNumOutputBuffers, "FillBufferDoneAO", Priority() + 1));

    if ((iThreadSafeHandlerEventHandler == NULL) ||
            (iThreadSafeHandlerEmptyBufferDone == NULL) ||
            (iThreadSafeHandlerFillBufferDone == NULL)
       )
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::Can't get threadsafe callbacks for decoder!", iName.Str()));
        iOMXDecoder = NULL;
    }

    // ONLY FOR AVC FILE PLAYBACK WILL 1 FRAGMENT CONTAIN ONE FULL NAL
    //This will be true for AAC and Real Audio format as well
    if ((format == PVMF_MIME_H264_VIDEO) || (format == PVMF_MIME_H264_VIDEO_MP4) || (format == PVMF_MIME_H264_VIDEO_RAW)
            || (format == PVMF_MIME_MPEG4_AUDIO) || (format == PVMF_MIME_ASF_MPEG4_AUDIO)
            || (format == PVMF_MIME_REAL_AUDIO))
    {
        // every memory fragment in case of AVC is a full NAL
        iSetMarkerBitForEveryFrag = true;
    }
    else
    {
        iSetMarkerBitForEveryFrag = false;
    }

    // FOR WMV, fill in the sequence header information that may be needed by the key-frame only mode
    if (format == PVMF_MIME_WMV)
    {

        pv_get_wmv_seq_hdr_info((uint8*)((PVMFOMXDecPort*)iInPort)->iTrackConfig, (int)((PVMFOMXDecPort*)iInPort)->iTrackConfigSize, &iWmvSeqHeaderInfo);

    }
    if (iNodeConfig.iKeyFrameOnlyMode || iNodeConfig.iSkipNUntilKeyFrame)
    {
        if (pv_frametype_parser_format_supported(((PVMFOMXDecPort*)iInPort)->iFormat))
        {
            iCheckForKeyFrame = true;
            iSkippingNonKeyFrames = true;
            iSkipFrameCount = 0;
        }
        else
        {
            // format not supported, disable
            iNodeConfig.iKeyFrameOnlyMode = false;
            iNodeConfig.iSkipNUntilKeyFrame = 0;
            iSkipFrameCount = 0;
            iCheckForKeyFrame = true;
            iSkippingNonKeyFrames = false;
        }
    }




    // Init Decoder
    iCurrentDecoderState = OMX_StateLoaded;

    /* Change state to OMX_StateIdle from OMX_StateLoaded. */
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "%s::DoPrepare(): Changing Component state Loaded -> Idle ", iName.Str()));

    err = OMX_SendCommand(iOMXDecoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoPrepare() Can't send StateSet command!", iName.Str()));
        return PVMFErrNoResources;
    }


    /* Allocate input buffers */
    if (!CreateInputMemPool(iNumInputBuffers))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoPrepare() Can't allocate mempool for input buffers!", iName.Str()));

        return PVMFErrNoResources;
    }



    if (!ProvideBuffersToComponent(iInBufMemoryPool, // allocator
                                   iInputAllocSize,  // size to allocate from pool (hdr only or hdr+ buffer)
                                   iNumInputBuffers, // number of buffers
                                   iOMXComponentInputBufferSize, // actual buffer size
                                   iInputPortIndex, // port idx
                                   iOMXComponentSupportsExternalInputBufferAlloc, // can component use OMX_UseBuffer
                                   true // this is input
                                  ))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoPrepare() Component can't use input buffers!", iName.Str()));

        return PVMFErrNoResources;
    }


    /* Allocate output buffers */
    if (!CreateOutMemPool(iNumOutputBuffers))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoPrepare() Can't allocate mempool for output buffers!", iName.Str()));

        return PVMFErrNoResources;
    }


    if (!ProvideBuffersToComponent(iOutBufMemoryPool, // allocator
                                   iOutputAllocSize,     // size to allocate from pool (hdr only or hdr+ buffer)
                                   iNumOutputBuffers, // number of buffers
                                   iOMXComponentOutputBufferSize, // actual buffer size
                                   iOutputPortIndex, // port idx
                                   iOMXComponentSupportsExternalOutputBufferAlloc, // can component use OMX_UseBuffer
                                   false // this is not input
                                  ))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoPrepare() Component can't use output buffers!", iName.Str()));

        return PVMFErrNoResources;
    }


    //this command is asynchronous, it will remain pending
    //until OMX component state transition is complete.
    return PVMFPending;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoStart()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::DoStart() In", iName.Str()));

    if (EPVMFNodeStarted == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXBaseDecNode::DoStart() already in Started state"));
        return PVMFSuccess;
    }

    iDiagnosticsLogged = false;

    PVMFStatus status = PVMFPending;

    OMX_ERRORTYPE  err;
    OMX_STATETYPE sState;

    //Get state of OpenMAX decoder
    err = OMX_GetState(iOMXDecoder, &sState);
    if (err != OMX_ErrorNone)
    {
        //Error condition report
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,

                        (0, "%s::DoStart(): Can't get State of decoder!", iName.Str()));

        sState = OMX_StateInvalid;
    }

    if ((sState == OMX_StateIdle) || (sState == OMX_StatePause))
    {
        /* Change state to OMX_StateExecuting form OMX_StateIdle. */
        // init the flag
        if (!iDynamicReconfigInProgress)
        {

            iDoNotSendOutputBuffersDownstreamFlag = false; // or if output was not being sent downstream due to state changes
            // re-enable sending output
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::DoStart() Changing Component state Idle or Paused->Executing", iName.Str()));

        err = OMX_SendCommand(iOMXDecoder, OMX_CommandStateSet, OMX_StateExecuting, NULL);
        if (err != OMX_ErrorNone)
        {
            //Error condition report
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::DoStart(): Can't send StateSet command to decoder!", iName.Str()));

            status = PVMFErrInvalidState;
        }

    }
    else
    {
        //Error condition report
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoStart(): Decoder is not in the Idle or Pause state!", iName.Str()));

        status = PVMFErrInvalidState;
    }

    return status;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoStop()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::DoStop() In", iName.Str()));

    LogDiagnostics();

    OMX_ERRORTYPE  err;
    OMX_STATETYPE sState;

    PVMFStatus status = PVMFPending;

    // Stop data source
    // This will also prevent execution of HandleProcessingState

    iDataIn.Unbind();
    // Clear queued messages in ports
    if (iInPort)
    {
        iInPort->ClearMsgQueues();
    }

    if (iOutPort)
    {
        iOutPort->ClearMsgQueues();
    }

    // Clear the data flags

    iEndOfDataReached = false;
    iIsEOSSentToComponent = false;
    iIsEOSReceivedFromComponent = false;

    iEOCReceived = false;
    iBOCReceived = false;

    iDoNotSendOutputBuffersDownstreamFlag = true; // stop sending output buffers downstream

    //if we're in the middle of a partial frame assembly
    // abandon it and start fresh
    if (iObtainNewInputBuffer == false)
    {
        if (iInputBufferUnderConstruction != NULL)
        {
            if (iInBufMemoryPool != NULL)
            {
                iInBufMemoryPool->deallocate((OsclAny *)(iInputBufferUnderConstruction->pMemPoolEntry));
            }
            iInputBufferUnderConstruction = NULL;
        }
        iObtainNewInputBuffer = true;

    }

    iFirstDataMsgAfterBOS = true;

    //Get state of OpenMAX decoder
    err = OMX_GetState(iOMXDecoder, &sState);
    if (err != OMX_ErrorNone)
    {
        //Error condition report
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoStop(): Can't get State of decoder!", iName.Str()));

        sState = OMX_StateInvalid;
    }

    if ((sState == OMX_StateExecuting) || (sState == OMX_StatePause))
    {
        /* Change state to OMX_StateIdle from OMX_StateExecuting or OMX_StatePause. */

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::DoStop() Changing Component State Executing->Idle or Pause->Idle", iName.Str()));

        err = OMX_SendCommand(iOMXDecoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        if (err != OMX_ErrorNone)
        {
            //Error condition report
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::DoStop(): Can't send StateSet command to decoder!", iName.Str()));

            return PVMFErrInvalidState;
        }

        // prevent the node from sending more buffers etc.
        // if port reconfiguration is in process, let the state remain one of the port config states
        //  if there is a start command, we can do it seemlessly (by continuing the port reconfig)
        if ((iProcessingState == EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode) ||
                (iProcessingState == EPVMFOMXBaseDecNodeProcessingState_InitDecoder))
            iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Stopping;

        // indicate that stop cmd was sent


        iStopCommandWasSentToComponent = true;
    }
    else
    {
        //Error condition report
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoStop(): Decoder is not in the Executing or Pause state!", iName.Str()));

        return PVMFErrInvalidState;
    }

    return status;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoFlush()
{

    //Notify all ports to suspend their input
    if (iInPort)
    {
        iInPort->SuspendInput();
    }
    if (iOutPort)
    {
        iOutPort->SuspendInput();
    }
    // Stop data source

    // Sending "OMX_CommandFlush" to the decoder: Not supported yet

    return PVMFPending;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoPause()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::DoPause() In", iName.Str()));

    if (EPVMFNodePaused == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXBaseDecNode::DoPause() already in Paused state"));
        return PVMFSuccess;
    }

    OMX_ERRORTYPE  err;
    OMX_STATETYPE sState;

    //Get state of OpenMAX decoder
    err = OMX_GetState(iOMXDecoder, &sState);
    if (err != OMX_ErrorNone)
    {
        //Error condition report
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoPause(): Can't get State of decoder!", iName.Str()));

        sState = OMX_StateInvalid;
    }

    if (sState == OMX_StateExecuting)
    {
        /* Change state to OMX_StatePause from OMX_StateExecuting. */
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::DoPause() Changing Component State Executing->Paused", iName.Str()));


        // prevent the node from sending more buffers etc.
        // if port reconfiguration is in process, let the state remain one of the port config states
        //  if there is a start command, we can do it seemlessly (by continuing the port reconfig)
        if ((iProcessingState == EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode) ||
                (iProcessingState == EPVMFOMXBaseDecNodeProcessingState_InitDecoder))
            iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Pausing;

        // indicate that pause cmd was sent

        iPauseCommandWasSentToComponent = true;


        err = OMX_SendCommand(iOMXDecoder, OMX_CommandStateSet, OMX_StatePause, NULL);
        if (err != OMX_ErrorNone)
        {
            //Error condition report
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::DoPause(): Can't send StateSet command to decoder!", iName.Str()));

            return PVMFErrInvalidState;
        }

    }
    else
    {
        //Error condition report
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::DoPause(): Decoder is not in the Executing state!", iName.Str()));
        return PVMFErrInvalidState;
    }

    //this command is asynchronous, return pending.
    return PVMFPending;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoReset()
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::DoReset() In", iName.Str()));

    OMX_ERRORTYPE  err;
    OMX_STATETYPE sState;

    LogDiagnostics();

    PVMFStatus status = PVMFPending;

    //Check if decoder is initilized
    if (iOMXDecoder != NULL)
    {
        //if we're in the middle of a partial frame assembly
        // abandon it and start fresh
        if (iObtainNewInputBuffer == false)
        {
            if (iInputBufferUnderConstruction != NULL)
            {
                if (iInBufMemoryPool != NULL)
                {
                    iInBufMemoryPool->deallocate((OsclAny *)(iInputBufferUnderConstruction->pMemPoolEntry));
                }
                iInputBufferUnderConstruction = NULL;
            }
            iObtainNewInputBuffer = true;
        }

        iFirstDataMsgAfterBOS = true;
        iKeepDroppingMsgsUntilMarkerBit = false;

        //Get state of OpenMAX decoder
        err = OMX_GetState(iOMXDecoder, &sState);
        if (err != OMX_ErrorNone)
        {
            sState = OMX_StateInvalid;
        }

        if (sState == OMX_StateLoaded)
        {
            // this is a value obtained by synchronous call to component. Either the component was
            // already in this state without node issuing any commands,
            // or perhaps we started the Reset, but the callback notification has not yet arrived.
            if (iResetInProgress)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::DoReset() OMX comp is in loaded state. Wait for official callback to change variables etc.", iName.Str()));
                return PVMFPending;
            }
            else
            {
                //delete all ports and notify observer.
                if (iInPort)
                {
                    OSCL_DELETE(((PVMFOMXDecPort*)iInPort));
                    iInPort = NULL;
                }

                if (iOutPort)
                {
                    OSCL_DELETE(((PVMFOMXDecPort*)iOutPort));
                    iOutPort = NULL;
                }

                iDataIn.Unbind();


                // Reset the metadata key list
                iAvailableMetadataKeys.clear();

                // clear the flags requiring codec config processing completion
                iIsConfigDataProcessingCompletionNeeded = false;
                iIsThereMoreConfigDataToBeSent = false;
                iConfigDataBytesProcessed = 0; // init this variable that may be needed for SPS/PPS processing
                iConfigDataBuffersOutstanding = 0;

                iEndOfDataReached = false;
                iIsEOSSentToComponent = false;
                iIsEOSReceivedFromComponent = false;
                iEOCReceived = false;
                iBOCReceived = false;

                if (iOMXComponentUsesFullAVCFrames)
                {
                    iNALCount = 0;
                    oscl_memset(iNALSizeArray, 0, sizeof(uint32) * MAX_NAL_PER_FRAME); // 100 is max number of NALs
                }

                // reset dynamic port reconfig flags - no point continuing with port reconfig
                // if we start again - we'll have to do prepare and send new config etc.
                iSecondPortReportedChange = false;
                iDynamicReconfigInProgress = false;

                iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Idle;
                return PVMFSuccess;
            }
        }
        else if (sState == OMX_StateIdle)
        {
            //this command is asynchronous.  move the command from
            //the input command queue to the current command, where
            //it will remain until it is completed.
            if (!iResetInProgress)
            {
                iResetInProgress = true;
            }

            // if buffers aren't all back (due to timing issues with different callback AOs
            //      state change can be reported before all buffers are returned)
            if (iNumOutstandingInputBuffers > 0 || iNumOutstandingOutputBuffers > 0)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::DoReset() Waiting for %d input and-or %d output buffers", iName.Str(), iNumOutstandingInputBuffers, iNumOutstandingOutputBuffers));

                return PVMFPending;
            }

            if (!iResetMsgSent)
            {
                // We can come here only if all buffers are already back
                // Don't repeat any of this twice.
                /* Change state to OMX_StateLoaded form OMX_StateIdle. */
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::DoReset() Changing Component State Idle->Loaded", iName.Str()));

                err = OMX_SendCommand(iOMXDecoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
                if (err != OMX_ErrorNone)
                {
                    //Error condition report
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::DoReset(): Can't send StateSet command to decoder!", iName.Str()));
                }

                iResetMsgSent = true;


                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::DoReset() freeing output buffers", iName.Str()));

                if (false == iOutputBuffersFreed)
                {
                    if (!FreeBuffersFromComponent(iOutBufMemoryPool, // allocator
                                                  iOutputAllocSize,  // size to allocate from pool (hdr only or hdr+ buffer)
                                                  iNumOutputBuffers, // number of buffers
                                                  iOutputPortIndex, // port idx
                                                  false // this is not input
                                                 ))
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::DoReset() Cannot free output buffers ", iName.Str()));

                        if (iResetInProgress)
                        {
                            iResetInProgress = false;
                            if (PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd)
                            {
                                return PVMFErrResource;
                            }
                        }

                    }

                }
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::DoReset() freeing input buffers ", iName.Str()));
                if (false == iInputBuffersFreed)
                {
                    if (!FreeBuffersFromComponent(iInBufMemoryPool, // allocator
                                                  iInputAllocSize,   // size to allocate from pool (hdr only or hdr+ buffer)
                                                  iNumInputBuffers, // number of buffers
                                                  iInputPortIndex, // port idx
                                                  true // this is input
                                                 ))
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::DoReset() Cannot free input buffers ", iName.Str()));

                        if (iResetInProgress)
                        {
                            iResetInProgress = false;
                            if (PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd)
                            {
                                return PVMFErrResource;
                            }
                        }


                    }
                }

                iEndOfDataReached = false;
                iIsEOSSentToComponent = false;
                iIsEOSReceivedFromComponent = false;

                iEOCReceived = false;
                iBOCReceived = false;



                // also, perform Port deletion when the component replies with the command
                // complete, not right here
            } // end of if(iResetMsgSent)
            return PVMFPending;
        }

        else if ((sState == OMX_StateExecuting) || (sState == OMX_StatePause))
        {
            //this command is asynchronous.  move the command from
            //the input command queue to the current command, where
            //it will remain until it is completed.
            if (!iResetInProgress)
            {
                iResetInProgress = true;
            }

            // Change state to OMX_StateIdle from OMX_StateExecuting or OMX_StatePause.

            if (!iStopInResetMsgSent)
            {
                // replicate behavior of stop cmd
                iDataIn.Unbind();
                // Clear queued messages in ports
                if (iInPort)
                {
                    iInPort->ClearMsgQueues();
                }

                if (iOutPort)
                {
                    iOutPort->ClearMsgQueues();
                }

                // Clear the data flags

                iEndOfDataReached = false;
                iIsEOSSentToComponent = false;
                iIsEOSReceivedFromComponent = false;

                iEOCReceived = false;
                iBOCReceived = false;


                iDoNotSendOutputBuffersDownstreamFlag = true; // stop sending output buffers downstream


                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "%s::DoReset() Changing Component State Executing->Idle or Pause->Idle", iName.Str()));

                err = OMX_SendCommand(iOMXDecoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
                if (err != OMX_ErrorNone)
                {
                    //Error condition report
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::DoReset(): Can't send StateSet command to decoder!", iName.Str()));

                    return PVMFErrInvalidState;
                }


                iStopInResetMsgSent = true;
                // prevent the node from sending more buffers etc.
                // if port reconfiguration is in process, let the state remain one of the port config states
                //  if there is a start command, we can do it seemlessly (by continuing the port reconfig)
                if ((iProcessingState == EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode) ||
                        (iProcessingState == EPVMFOMXBaseDecNodeProcessingState_InitDecoder))
                    iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Stopping;
            }
            return PVMFPending;
        }
        else
        {
            //Error condition report
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::DoReset(): Decoder is not in the Idle state! %d", iName.Str(), sState));
            //do it here rather than relying on DTOR to avoid node reinit problems.
            DeleteOMXBaseDecoder();
            //still return success.
        }//end of if (sState == OMX_StateLoaded)
    }//end of if (iOMXDecoder != NULL)

    //delete all ports and notify observer.
    if (iInPort)
    {
        OSCL_DELETE(((PVMFOMXDecPort*)iInPort));
        iInPort = NULL;
    }

    if (iOutPort)
    {
        OSCL_DELETE(((PVMFOMXDecPort*)iOutPort));
        iOutPort = NULL;
    }

    iDataIn.Unbind();


    iIsConfigDataProcessingCompletionNeeded = false;
    iIsThereMoreConfigDataToBeSent = false;
    iConfigDataBytesProcessed = 0; // init this variable that may be needed for SPS/PPS processing
    iConfigDataBuffersOutstanding = 0;


    // Reset the metadata key list
    iAvailableMetadataKeys.clear();

    iEndOfDataReached = false;
    iIsEOSSentToComponent = false;
    iIsEOSReceivedFromComponent = false;

    iEOCReceived = false;
    iBOCReceived = false;

    iComputeSamplesPerFrame = true;

    if (iOMXComponentUsesFullAVCFrames)
    {
        iNALCount = 0;
        oscl_memset(iNALSizeArray, 0, sizeof(uint32) * MAX_NAL_PER_FRAME); // 100 is max number of NALs
    }

    // reset dynamic port reconfig flags - no point continuing with port reconfig
    // if we start again - we'll have to do prepare and send new config etc.
    iSecondPortReportedChange = false;
    iDynamicReconfigInProgress = false;

    iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Idle;

    status = PVMFFailure;

    if (iResetInProgress)
    {
        iResetInProgress = false;
        if (PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd)
        {
            status = PVMFSuccess;
        }
    }
    else
    {
        status = PVMFSuccess;
    }
    return status;
}

/////////////////////////////////////////////////////////////////////////////
// Clean Up Decoder
/////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::DeleteOMXBaseDecoder()
{
    OMX_ERRORTYPE  err;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::DeleteOMXBaseDecoder() In", iName.Str()));

    if (iOMXDecoder != NULL)
    {
        /* Free Component handle. */
        err = OMX_MasterFreeHandle(iOMXDecoder);
        if (err != OMX_ErrorNone)
        {
            //Error condition report
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::DeleteOMXBaseDecoder(): Can't free decoder's handle!", iName.Str()));
        }
        iOMXDecoder = NULL;

    }//end of if (iOMXDecoder != NULL)


    return true;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::ChangeNodeState(TPVMFNodeInterfaceState aNewState)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::ChangeNodeState() Changing state from %d to %d", iName.Str(), iInterfaceState, aNewState));
    iInterfaceState = aNewState;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::freechunkavailable(OsclAny *aContext)
{

    // check context to see whether input or output buffer was returned to the mempool
    if (aContext == (OsclAny *) iInBufMemoryPool)
    {

        iNumOutstandingInputBuffers--;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::freechunkavailable() Memory chunk in INPUT mempool was deallocated, %d out of %d now available", iName.Str(), iNumInputBuffers - iNumOutstandingInputBuffers, iNumInputBuffers));

        // notification only works once.
        // If there are multiple buffers coming back in a row, make sure to set the notification
        // flag in the mempool again, so that next buffer also causes notification
        iInBufMemoryPool->notifyfreechunkavailable(*this, aContext);

    }
    else if (aContext == (OsclAny *) iOutBufMemoryPool)
    {

        iNumOutstandingOutputBuffers--;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::freechunkavailable() Memory chunk in OUTPUT mempool was deallocated, %d out of %d now available", iName.Str(), iNumOutputBuffers - iNumOutstandingOutputBuffers, iNumOutputBuffers));

        // notification only works once.
        // If there are multiple buffers coming back in a row, make sure to set the notification
        // flag in the mempool again, so that next buffer also causes notification
        iOutBufMemoryPool->notifyfreechunkavailable(*this, aContext);

    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::freechunkavailable() UNKNOWN mempool ", iName.Str()));

    }

    Reschedule();


}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::HandlePortActivity(const PVMFPortActivity &aActivity)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "0x%x PVMFOMXBaseDecNode::PortActivity: port=0x%x, type=%d",
                     this, aActivity.iPort, aActivity.iType));

    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
            //An outgoing message was queued on this port.
            //We only need to queue a port activity event on the
            //first message.  Additional events will be queued during
            //the port processing as needed.
            if (aActivity.iPort->OutgoingMsgQueueSize() == 1)
            {
                //wake up the AO to process the port activity event.
                Reschedule();
            }
            break;

        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "%s::PortActivity: IncomingMsgQueueSize=%d", iName.Str(), aActivity.iPort->IncomingMsgQueueSize()));
            if (aActivity.iPort->IncomingMsgQueueSize() == 1)
            {
                //wake up the AO to process the port activity event.
                Reschedule();
            }
            break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_READY:
            if (iProcessingState == EPVMFOMXBaseDecNodeProcessingState_WaitForOutgoingQueue)
            {
                iProcessingState = EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode;
                // if we need to complete sending config buffers
                if (iIsThereMoreConfigDataToBeSent)
                    iProcessingState = EPVMFOMXBaseDecNodeProcessingState_InitDecoder;

                Reschedule();
            }
            break;

        case PVMF_PORT_ACTIVITY_CONNECT:
            //nothing needed.
            break;

        case PVMF_PORT_ACTIVITY_DISCONNECT:
            //clear the node input data when either port is disconnected.

            iDataIn.Unbind();
            break;

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_BUSY:
            // The connected port has become busy (its incoming queue is
            // busy).
            // No action is needed here-- the port processing code
            // checks for connected port busy during data processing.
            break;

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY:
            // The connected port has transitioned from Busy to Ready to Receive.
            // It's time to start processing outgoing messages again.

            //iProcessingState should transition from WaitForOutputPort to ReadyToDecode
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "0x%x PVMFOMXBaseDecNode::PortActivity: Connected port is now ready", this));
            Reschedule();
            break;

        default:
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoQueryInterface()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::DoQueryInterface", iName.Str()));
    PVUuid* uuid;
    PVInterface** ptr;
    PVMFStatus status = PVMFSuccess;

    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, ptr);

    if (*uuid == PVUuid(PVMF_OMX_BASE_DEC_NODE_CUSTOM1_UUID))
    {
        addRef();
        *ptr = (PVMFOMXBaseDecNodeExtensionInterface*)this;
    }
    else if (*uuid == PVUuid(KPVMFMetadataExtensionUuid))
    {
        addRef();
        *ptr = (PVMFMetadataExtensionInterface*)this;
    }
    else if (*uuid == PVUuid(PVMI_CAPABILITY_AND_CONFIG_PVUUID))
    {
        addRef();
        *ptr = (PvmiCapabilityAndConfig*)this;
    }
    else
    {
        //not supported
        *ptr = NULL;
        status = PVMFFailure;
    }
    return status;
}


/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::addRef()
{
    ++iExtensionRefCount;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMFOMXBaseDecNode::removeRef()
{
    --iExtensionRefCount;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    PVUuid my_uuid(PVMF_OMX_BASE_DEC_NODE_CUSTOM1_UUID);
    if (uuid == my_uuid)
    {
        PVMFOMXBaseDecNodeExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFOMXBaseDecNodeExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
        return true;
    }
    else if (uuid == KPVMFMetadataExtensionUuid)
    {
        PVMFMetadataExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXBaseDecNode::HandleRepositioning()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::HandleRepositioning() IN", iName.Str()));


    if (iIsConfigDataProcessingCompletionNeeded)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::HandleRepositioning() Cannot perform repositioning until component processes the config data", iName.Str()));
        return false;
    }

    // 1) Send Flush command to component for both input and output ports
    // 2) "Wait" until component flushes both ports
    // 3) Resume
    OMX_ERRORTYPE  err = OMX_ErrorNone;
    OMX_STATETYPE sState = OMX_StateInvalid;


    if (!iIsRepositioningRequestSentToComponent)
    {

        // first check the state (if executing or paused, continue)
        err = OMX_GetState(iOMXDecoder, &sState);
        if (err != OMX_ErrorNone)
        {
            //Error condition report
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::HandleRepositioning(): Can't get State of decoder - trying to send reposition request!", iName.Str()));

            sState = OMX_StateInvalid;
            ReportErrorEvent(PVMFErrResourceConfiguration);
            ChangeNodeState(EPVMFNodeError);
            return false;
        }

        if ((sState != OMX_StateExecuting) && (sState != OMX_StatePause))
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::HandleRepositioning() Component State is not executing or paused, do not proceed with repositioning", iName.Str()));

            return true;

        }


        iIsRepositioningRequestSentToComponent = true; // prevent sending requests multiple times
        iIsInputPortFlushed = false;    // flag that will be set to true once component flushes the port
        iIsOutputPortFlushed = false;
        iDoNotSendOutputBuffersDownstreamFlag = true;

        // make sure that if we reposition that we wait until a keyframe if necessary
        if (iNodeConfig.iKeyFrameOnlyMode || iNodeConfig.iSkipNUntilKeyFrame)
        {
            if (pv_frametype_parser_format_supported(((PVMFOMXDecPort*)iInPort)->iFormat))
            {
                iCheckForKeyFrame = true;
                iSkipFrameCount = 0;
                iSkippingNonKeyFrames = true;
            }
            else
            {
                // format not supported, disable
                iNodeConfig.iKeyFrameOnlyMode = false;
                iNodeConfig.iSkipNUntilKeyFrame = 0;
                iSkipFrameCount = 0;
                iCheckForKeyFrame = true;
                iSkippingNonKeyFrames = false;
            }
        }


        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::HandleRepositioning() Sending Flush command to component", iName.Str()));

        // send command to flush all ports (arg is OMX_ALL)
        err = OMX_SendCommand(iOMXDecoder, OMX_CommandFlush, OMX_ALL, NULL);
        if (err != OMX_ErrorNone)
        {
            //Error condition report
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "%s::HandleRepositioning(): Can't send flush command  - trying to send reposition request!", iName.Str()));

            sState = OMX_StateInvalid;
            ReportErrorEvent(PVMFErrResourceConfiguration);
            ChangeNodeState(EPVMFNodeError);
            return false;
        }

    }

    if (iIsRepositionDoneReceivedFromComponent)
    {

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "%s::HandleRepositioning() Component has flushed both ports, and is done repositioning", iName.Str()));

        iIsRepositioningRequestSentToComponent = false; // enable sending requests again
        iIsRepositionDoneReceivedFromComponent = false;
        iIsInputPortFlushed = false;
        iIsOutputPortFlushed = false;

        iDoNotSendOutputBuffersDownstreamFlag = false;
        return true;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "%s::HandleRepositioning() Component is not yet done repositioning ", iName.Str()));

    return false;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF OMX_TICKS PVMFOMXBaseDecNode::ConvertTimestampIntoOMXTicks(const MediaClockConverter& src)
{
    // This is similar to mediaclockconverter set_value method - except without using the modulo for upper part of 64 bits

    // Timescale value cannot be zero
    OSCL_ASSERT(src.get_timescale() != 0);
    if (src.get_timescale() == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::ConvertTimestampIntoOMXTicks Input timescale is 0", iName.Str()));

        SetState(EPVMFNodeError);
        ReportErrorEvent(PVMFErrResourceConfiguration);
        return (OMX_TICKS) 0;
    }

    OSCL_ASSERT(iTimeScale != 0);
    if (0 == iTimeScale)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::ConvertTimestampIntoOMXTicks target timescale is 0", iName.Str()));

        SetState(EPVMFNodeError);
        ReportErrorEvent(PVMFErrResourceConfiguration);
        return (OMX_TICKS) 0;
    }

    uint64 value = (uint64(src.get_wrap_count())) << 32;
    value += src.get_current_timestamp();
    // rounding up
    value = (uint64(value) * iTimeScale + uint64(src.get_timescale() - 1)) / src.get_timescale();
    return (OMX_TICKS) value;


}
////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF uint32 PVMFOMXBaseDecNode::ConvertOMXTicksIntoTimestamp(const OMX_TICKS &src)
{
    // omx ticks use microsecond timescale (iTimeScale = 1000000)
    // This is similar to mediaclockconverter set_value method

    // Timescale value cannot be zero
    OSCL_ASSERT(iOutTimeScale != 0);
    if (0 == iOutTimeScale)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::ConvertOMXTicksIntoTimestamp Output timescale is 0", iName.Str()));

        SetState(EPVMFNodeError);
        ReportErrorEvent(PVMFErrResourceConfiguration);
        return (uint32) 0;
    }

    OSCL_ASSERT(iTimeScale != 0);
    if (0 == iTimeScale)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "%s::ConvertOMXTicksIntoTimestamp target timescale is 0", iName.Str()));

        SetState(EPVMFNodeError);
        ReportErrorEvent(PVMFErrResourceConfiguration);
        return (uint32) 0;
    }

    uint32 current_ts;

    uint64 value = (uint64) src;

    // rounding up
    value = (uint64(value) * iOutTimeScale + uint64(iTimeScale - 1)) / iTimeScale;

    current_ts = (uint32)(value & 0xFFFFFFFF);
    return (uint32) current_ts;

}


OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier, PvmiKvp*& aParameters, int& aNumParamElements, PvmiCapabilityContext aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::getParametersSync()", iName.Str()));
    OSCL_UNUSED_ARG(aSession);

    return DoCapConfigGetParametersSync(aIdentifier, aParameters, aNumParamElements, aContext);
}


OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int aNumElements)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::releaseParameters()", iName.Str()));
    OSCL_UNUSED_ARG(aSession);

    return DoCapConfigReleaseParameters(aParameters, aNumElements);
}


OSCL_EXPORT_REF void PVMFOMXBaseDecNode::setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int aNumElements, PvmiKvp* &aRetKVP)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::setParametersSync()", iName.Str()));
    OSCL_UNUSED_ARG(aSession);

    // Complete the request synchronously
    DoCapConfigSetParameters(aParameters, aNumElements, aRetKVP);
}

OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int aNumElements)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::verifyParametersSync()", iName.Str()));
    OSCL_UNUSED_ARG(aSession);

    return DoCapConfigVerifyParameters(aParameters, aNumElements);
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId PVMFOMXBaseDecNode::GetNodeMetadataKeys(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList, uint32 starting_index, int32 max_entries, char* query_key, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%sCommand::GetNodeMetadataKeys() called", iName.Str()));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_GETNODEMETADATAKEYS, aKeyList, starting_index, max_entries, query_key, aContext);
    return QueueCommandL(cmd);
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId PVMFOMXBaseDecNode::GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 starting_index, int32 max_entries, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%sCommand::GetNodeMetadataValue() called", iName.Str()));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_GETNODEMETADATAVALUES, aKeyList, aValueList, starting_index, max_entries, aContext);
    return QueueCommandL(cmd);
}

// From PVMFMetadataExtensionInterface
/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::SetMetadataClipIndex(uint32 aClipIndex)
{
    return (aClipIndex == 0) ? PVMFSuccess : PVMFErrArgument;
}

OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::ReleaseNodeMetadataKeys(PVMFMetadataList& , uint32 , uint32)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::ReleaseNodeMetadataKeys() called", iName.Str()));
    //nothing needed-- there's no dynamic allocation in this node's key list
    return PVMFSuccess;
}

// From PVMFMetadataExtensionInterface
/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 start, uint32 end)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "%s::ReleaseNodeMetadataValues() called", iName.Str()));

    if (aValueList.size() == 0 || start > end)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::ReleaseNodeMetadataValues() Invalid start/end index", iName.Str()));
        return PVMFErrArgument;
    }

    if (end >= aValueList.size())
    {
        end = aValueList.size() - 1;
    }

    for (uint32 i = start; i <= end; i++)
    {
        if (aValueList[i].key != NULL)
        {
            switch (GetValTypeFromKeyString(aValueList[i].key))
            {
                case PVMI_KVPVALTYPE_CHARPTR:
                    if (aValueList[i].value.pChar_value != NULL)
                    {
                        OSCL_ARRAY_DELETE(aValueList[i].value.pChar_value);
                        aValueList[i].value.pChar_value = NULL;
                    }
                    break;

                case PVMI_KVPVALTYPE_UINT32:
                case PVMI_KVPVALTYPE_UINT8:
                    // No memory to free for these valtypes
                    break;

                default:
                    // Should not get a value that wasn't created from here
                    break;
            }

            OSCL_ARRAY_DELETE(aValueList[i].key);
            aValueList[i].key = NULL;
        }
    }

    return PVMFSuccess;
}

/////////////////////////////////////////////////////////////////////////////
////////////////////// CALLBACK PROCESSING FOR EVENT HANDLER
/////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE PVMFOMXBaseDecNode::EventHandlerProcessing(OMX_OUT OMX_HANDLETYPE aComponent,
        OMX_OUT OMX_PTR aAppData,
        OMX_OUT OMX_EVENTTYPE aEvent,
        OMX_OUT OMX_U32 aData1,
        OMX_OUT OMX_U32 aData2,
        OMX_OUT OMX_PTR aEventData)
{
    OSCL_UNUSED_ARG(aComponent);
    OSCL_UNUSED_ARG(aAppData);
    OSCL_UNUSED_ARG(aEventData);

    switch (aEvent)
    {
        case OMX_EventCmdComplete:
        {

            switch (aData1)
            {
                case OMX_CommandStateSet:
                {
                    HandleComponentStateChange(aData2);
                    break;
                }
                case OMX_CommandFlush:
                {

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::EventHandlerProcessing: OMX_CommandFlush - completed on port %d", iName.Str(), aData2));

                    if (iIsRepositioningRequestSentToComponent)
                    {
                        if (aData2 == iOutputPortIndex)
                        {
                            iIsOutputPortFlushed = true;
                        }
                        else if (aData2 == iInputPortIndex)
                        {
                            iIsInputPortFlushed = true;
                        }

                        if (iIsOutputPortFlushed && iIsInputPortFlushed)
                        {
                            iIsRepositionDoneReceivedFromComponent = true;
                        }
                    }
                    Reschedule();

                }
                break;

                case OMX_CommandPortDisable:
                {
                    // if port disable command is done, we can re-allocate the buffers and re-enable the port

                    iProcessingState = EPVMFOMXBaseDecNodeProcessingState_PortReEnable;
                    iPortIndexForDynamicReconfig =  aData2;

                    Reschedule();
                    break;
                }
                case OMX_CommandPortEnable:
                    // port enable command is done. Check if the other port also reported change.
                    // If not, we can start data flow. Otherwise, must start dynamic reconfig procedure for
                    // the other port as well.
                {
                    if (iSecondPortReportedChange)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::EventHandlerProcessing: OMX_CommandPortEnable - completed on port %d, dynamic reconfiguration needed on port %d", iName.Str(), aData2, iSecondPortToReconfig));

                        iProcessingState = EPVMFOMXBaseDecNodeProcessingState_PortReconfig;
                        iPortIndexForDynamicReconfig = iSecondPortToReconfig;
                        iSecondPortReportedChange = false;
                    }
                    else
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "%s::EventHandlerProcessing: OMX_CommandPortEnable - completed on port %d, resuming normal data flow", iName.Str(), aData2));
                        iProcessingState = EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode;

                        if (iIsThereMoreConfigDataToBeSent)
                            iProcessingState = EPVMFOMXBaseDecNodeProcessingState_InitDecoder;

                        iDynamicReconfigInProgress = false;
                        // in case pause or stop command was sent to component
                        // change processing state (because the node might otherwise
                        // start sending buffers to component before pause/stop is processed)
                        if (iPauseCommandWasSentToComponent)
                        {
                            iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Pausing;
                        }
                        if (iStopCommandWasSentToComponent)
                        {
                            iProcessingState = EPVMFOMXBaseDecNodeProcessingState_Stopping;
                        }
                    }
                    Reschedule();
                    break;
                }

                case OMX_CommandMarkBuffer:
                    // nothing to do here yet;
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::EventHandlerProcessing: OMX_CommandMarkBuffer - completed - no action taken", iName.Str()));

                    break;

                default:
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "%s::EventHandlerProcessing: Unsupported event", iName.Str()));
                    break;
                }
            }//end of switch (aData1)

            break;
        }//end of case OMX_EventCmdComplete

        case OMX_EventError:
        {

            if (aData1 == (OMX_U32) OMX_ErrorStreamCorrupt)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::EventHandlerProcessing: OMX_EventError - Bitstream corrupt error", iName.Str()));
                // Errors from corrupt bitstream are reported as info events
                ReportInfoEvent(PVMFInfoProcessingFailure, NULL);

            }
            else if (aData1 == (OMX_U32) OMX_ErrorInvalidState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::EventHandlerProcessing: OMX_EventError - OMX_ErrorInvalidState", iName.Str()));

                HandleComponentStateChange(OMX_StateInvalid);
            }
            else
            {

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "%s::EventHandlerProcessing: OMX_EventError", iName.Str()));
                // for now, any error from the component will be reported as error
                ReportErrorEvent(PVMFErrProcessing, NULL, NULL);
                SetState(EPVMFNodeError);
            }
            break;

        }

        case OMX_EventBufferFlag:
        {
            // the component is reporting it encountered end of stream flag
            // we'll send eos when we get the actual last buffer with marked eos

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::EventHandlerProcessing: OMX_EventBufferFlag (EOS) flag returned from OMX component", iName.Str()));

            Reschedule();
            break;
        }//end of case OMX_EventBufferFlag

        case OMX_EventMark:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::EventHandlerProcessing: OMX_EventMark returned from OMX component - no action taken", iName.Str()));

            Reschedule();
            break;
        }//end of case OMX_EventMark

        case OMX_EventPortSettingsChanged:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::EventHandlerProcessing: OMX_EventPortSettingsChanged returned from OMX component", iName.Str()));


            // Recompute iSamplesPerFrame
            iComputeSamplesPerFrame = true;

            //first check how many ports requested dynamic port reconfiguration
            if (OMX_ALL == aData1)
            {
                if (!iDynamicReconfigInProgress)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::EventHandlerProcessing: OMX_EventPortSettingsChanged returned for both input and output port %d", iName.Str(), aData1));

                    iProcessingState = EPVMFOMXBaseDecNodeProcessingState_PortReconfig;
                    iPortIndexForDynamicReconfig = iInputPortIndex;
                    iDynamicReconfigInProgress = true;

                    iSecondPortToReconfig = iOutputPortIndex;
                    iSecondPortReportedChange = true;
                }
                else
                {
                    // This is an error case, where we received port settings changed callback for
                    // both the ports while dynamic port reconfiguration is still in progress for one of them
                    // We can handle this by setting it to happen at the other port

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::EventHandlerProcessing: OMX_EventPortSettingsChanged returned for both the port %d, dynamic reconfig already in progress for port %d", iName.Str(), aData1, iPortIndexForDynamicReconfig));

                    if (iInputPortIndex == iPortIndexForDynamicReconfig)
                    {
                        iSecondPortToReconfig = iOutputPortIndex;
                    }
                    else
                    {
                        iSecondPortToReconfig = iInputPortIndex;
                    }
                    iSecondPortReportedChange = true;
                }
            }
            else
            {
                // first check if dynamic reconfiguration is already in progress,
                // if so, wait until this is completed, and then initiate the 2nd reconfiguration
                if (iDynamicReconfigInProgress)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::EventHandlerProcessing: OMX_EventPortSettingsChanged returned for port %d, dynamic reconfig already in progress", iName.Str(), aData1));

                    iSecondPortToReconfig = aData1;
                    iSecondPortReportedChange = true;
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "%s::EventHandlerProcessing: OMX_EventPortSettingsChanged returned for port %d", iName.Str(), aData1));

                    iProcessingState = EPVMFOMXBaseDecNodeProcessingState_PortReconfig;
                    iPortIndexForDynamicReconfig = aData1;
                    iDynamicReconfigInProgress = true;
                }
            }

            Reschedule();
            break;
        }//end of case OMX_PortSettingsChanged

        case OMX_EventResourcesAcquired:        //not supported
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::EventHandlerProcessing: OMX_EventResourcesAcquired returned from OMX component - no action taken", iName.Str()));

            Reschedule();
            break;
        }

        default:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "%s::EventHandlerProcessing:  Unknown Event returned from OMX component - no action taken", iName.Str()));

            break;
        }

    }//end of switch (eEvent)



    return OMX_ErrorNone;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoReleasePort()
{
    PVMFPortInterface* p = NULL;
    iCurrentCommand.PVMFNodeCommandBase::Parse(p);
    PVMFOMXDecPort* port = (PVMFOMXDecPort*)p;

    if (port != NULL && (port == iInPort || port == iOutPort))
    {
        if (port == iInPort)
        {
            OSCL_DELETE(((PVMFOMXDecPort*)iInPort));
            iInPort = NULL;
        }
        else
        {
            OSCL_DELETE(((PVMFOMXDecPort*)iOutPort));
            iOutPort = NULL;
        }
        //delete the port.
        return PVMFSuccess;
    }
    else
    {
        //port not found.
        return PVMFFailure;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
// CAPABILITY CONFIG PRIVATE

OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoCapConfigGetParametersSync(PvmiKeyType aIdentifier, PvmiKvp*& aParameters, int& aNumParamElements, PvmiCapabilityContext aContext)
{
    OSCL_UNUSED_ARG(aParameters);
    OSCL_UNUSED_ARG(aIdentifier);
    OSCL_UNUSED_ARG(aNumParamElements);
    OSCL_UNUSED_ARG(aContext);
    return PVMFFailure;
}


OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoCapConfigReleaseParameters(PvmiKvp* aParameters, int aNumElements)
{
    OSCL_UNUSED_ARG(aParameters);
    OSCL_UNUSED_ARG(aNumElements);
    return PVMFSuccess;
}


OSCL_EXPORT_REF void PVMFOMXBaseDecNode::DoCapConfigSetParameters(PvmiKvp* aParameters, int aNumElements, PvmiKvp* &aRetKVP)
{
    OSCL_UNUSED_ARG(aParameters);
    OSCL_UNUSED_ARG(aNumElements);
    OSCL_UNUSED_ARG(aRetKVP);
}


OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::DoCapConfigVerifyParameters(PvmiKvp* aParameters, int aNumElements)
{
    OSCL_UNUSED_ARG(aParameters);
    OSCL_UNUSED_ARG(aNumElements);
    return PVMFSuccess;
}


/////////////////////////////////////////////////////////////////////////////
void PVMFOMXBaseDecNode::LogDiagnostics()
{
    if (iDiagnosticsLogged == false)
    {
        iDiagnosticsLogged = true;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iDiagnosticsLogger, PVLOGMSG_INFO, (0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iDiagnosticsLogger, PVLOGMSG_INFO, (0, "%s - Number of Frames Sent = %d", iName.Str(), iSeqNum));
        PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iDiagnosticsLogger, PVLOGMSG_INFO, (0, "%s - TS of last decoded frame = %d", iName.Str(), iOutTimeStamp));
    }
}


OSCL_EXPORT_REF OsclAny* PVMFOMXBaseDecNode::AllocateKVPKeyArray(int32& aLeaveCode, PvmiKvpValueType aValueType, int32 aNumElements)
{
    int32 leaveCode = OsclErrNone;
    OsclAny* aBuffer = NULL;
    switch (aValueType)
    {
        case PVMI_KVPVALTYPE_WCHARPTR:
            OSCL_TRY(leaveCode,
                     aBuffer = (oscl_wchar*) OSCL_ARRAY_NEW(oscl_wchar, aNumElements);
                    );
            break;

        case PVMI_KVPVALTYPE_CHARPTR:
            OSCL_TRY(leaveCode,
                     aBuffer = (char*) OSCL_ARRAY_NEW(char, aNumElements);
                    );
            break;
        case PVMI_KVPVALTYPE_UINT8PTR:
            OSCL_TRY(leaveCode,
                     aBuffer = (uint8*) OSCL_ARRAY_NEW(uint8, aNumElements);
                    );
            break;
        default:
            break;
    }
    aLeaveCode = leaveCode;
    return aBuffer;
}

OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::RetrieveMP3FrameLength(uint8 *pBuffer)
{
    OSCL_UNUSED_ARG(pBuffer);

    // stub function.  should only be called by omx audio dec node

    OSCL_ASSERT(false);

    return PVMFFailure;
}

OSCL_EXPORT_REF int32 PVMFOMXBaseDecNode::GetNAL_OMXNode(uint8** bitstream, uint32* size)
{
    // stub function
    OSCL_ASSERT(false);

    OSCL_UNUSED_ARG(bitstream);
    OSCL_UNUSED_ARG(size);

    return 0;
}

OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::ParseAndReWrapH264RAW(PVMFSharedMediaDataPtr& aMediaDataPtr)
{
    // stub function
    OSCL_ASSERT(false);

    OSCL_UNUSED_ARG(aMediaDataPtr);

    return false;
}


OSCL_EXPORT_REF bool PVMFOMXBaseDecNode::CreateAACConfigDataFromASF(uint8 *inptr, uint32 inlen, uint8 *outptr, uint32 &outlen)
{
    // stub function
    OSCL_ASSERT(false); // should never be called here - but rather from audio dec node
    OSCL_UNUSED_ARG(inptr);
    OSCL_UNUSED_ARG(inlen);
    OSCL_UNUSED_ARG(outptr);
    OSCL_UNUSED_ARG(outlen);

    return false;
}

OSCL_EXPORT_REF void PVMFOMXBaseDecNode::AllocatePvmiKey(PvmiKeyType* KvpKey, OsclMemAllocator* alloc, int32 KeyLength)
{
    *KvpKey = (PvmiKeyType)(*alloc).ALLOCATE(KeyLength);
}

OSCL_EXPORT_REF PVMFStatus PVMFOMXBaseDecNode::SkipNonKeyFrames()
{
    // this should only called by the video dec node
    iSkippingNonKeyFrames = false;

    OSCL_ASSERT(false);

    return PVMFErrNotSupported;
}

