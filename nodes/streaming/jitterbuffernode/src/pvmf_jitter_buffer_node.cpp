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
#ifndef PVMF_JB_JITTERBUFFERMISC_H_INCLUDED
#include "pvmf_jitter_buffer_node.h"
#endif

#ifndef PVMF_JITTER_BUFFER_COMMON_TYPES_H_INCLUDED
#include "pvmf_jitter_buffer_common_types.h"
#endif

#ifndef OSCL_EXCLUSIVE_PTR_H_INCLUDED
#include "oscl_exclusive_ptr.h"
#endif

#ifndef __MEDIA_CLOCK_CONVERTER_H
#include "media_clock_converter.h"
#endif

#ifndef PVMF_STREAMING_MANAGER_NODE_H_INCLUDED
#include "pvmf_jitter_buffer_node.h"
#endif

#ifndef PVMF_JITTER_BUFFER_FACTORY_H
#include "pvmf_jitter_buffer_factory.h"
#endif

#ifndef PVMF_SM_TUNABLES_H_INCLUDED
#include "pvmf_sm_tunables.h"
#endif

#ifndef PVMF_BASIC_ERRORINFOMESSAGE_H_INCLUDED
#include "pvmf_basic_errorinfomessage.h"
#endif

#ifndef OSCL_DLL_H_INCLUDED
#include "oscl_dll.h"
#endif

// Define entry point for this DLL
OSCL_DLL_ENTRY_POINT_DEFAULT()

//Construction and Destruction
OSCL_EXPORT_REF PVMFJitterBufferNode::PVMFJitterBufferNode(int32 aPriority,
        JitterBufferFactory* aJBFactory): PVMFNodeInterfaceImpl(aPriority, "JitterBufferNode")
{
    //Initialize capability
    iNodeCapability.iCanSupportMultipleInputPorts = true;
    iNodeCapability.iCanSupportMultipleOutputPorts = true;
    iNodeCapability.iHasMaxNumberOfPorts = false;
    iNodeCapability.iMaxNumberOfPorts = 0;//no maximum
    iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_RTP);
    iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_ASFFF);
    iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_RMFF);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_RTP);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_ASFFF);
    //Jitter buffer factory
    ipJitterBufferFactory   =   aJBFactory;

    //get logger objects
    ipDataPathLogger = PVLogger::GetLoggerObject("datapath.sourcenode.jitterbuffernode");
    ipDataPathLoggerIn = PVLogger::GetLoggerObject("datapath.sourcenode.jitterbuffernode.in");
    ipDataPathLoggerOut = PVLogger::GetLoggerObject("datapath.sourcenode.jitterbuffernode.out");
    ipDataPathLoggerFlowCtrl = PVLogger::GetLoggerObject("datapath.sourcenode.jitterbuffernode.flowctrl");

    ipClockLogger = PVLogger::GetLoggerObject("clock.jitterbuffernode");
    ipClockLoggerSessionDuration = PVLogger::GetLoggerObject("clock.streaming_manager.sessionduration");
    ipClockLoggerRebuff = PVLogger::GetLoggerObject("clock.jitterbuffernode.rebuffer");

    ipDiagnosticsLogger = PVLogger::GetLoggerObject("pvplayerdiagnostics.streamingmanager");
    ipJBEventsClockLogger = PVLogger::GetLoggerObject("jitterbuffernode.eventsclock");

    //Diagniostics related
    iDiagnosticsLogged = false;
    iNumRunL = 0;

    iPortVector.Construct(PVMF_JITTER_BUFFER_NODE_PORT_VECTOR_RESERVE);
    ResetNodeParams(false);
}

void PVMFJitterBufferNode::ResetNodeParams(bool aReleaseMemory)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::ResetNodeParams In aReleaseMemory[%d]", aReleaseMemory));
    //Session specific initializations and resetting

    oStartPending = false;
    oStopOutputPorts = true;
    iPauseTime = 0;
    ipClientPlayBackClock = NULL;
    iMediaReceiveingChannelPrepared = false;

    iBroadCastSession = false;

    iDelayEstablished = false;
    iJitterBufferState = PVMF_JITTER_BUFFER_READY;
    iJitterDelayPercent = 0;

    //Extension interface initializations
    if (ipExtensionInterface && aReleaseMemory)
    {
        ipExtensionInterface->removeRef();
    }
    ipExtensionInterface = NULL;

    //Variables to persist info passed on by the extension interface
    iRebufferingThreshold = DEFAULT_JITTER_BUFFER_UNDERFLOW_THRESHOLD_IN_MS;
    iJitterBufferDurationInMilliSeconds = DEFAULT_JITTER_BUFFER_DURATION_IN_MS;
    iMaxInactivityDurationForMediaInMs = DEFAULT_MAX_INACTIVITY_DURATION_IN_MS;
    iEstimatedServerKeepAheadInMilliSeconds = DEFAULT_ESTIMATED_SERVER_KEEPAHEAD_FOR_OOO_SYNC_IN_MS;

    iJitterBufferSz = 0;
    iMaxNumBufferResizes = DEFAULT_MAX_NUM_SOCKETMEMPOOL_RESIZES;
    iBufferResizeSize = DEFAULT_MAX_SOCKETMEMPOOL_RESIZELEN_INPUT_PORT;
    iBufferingStatusIntervalInMs =
        (PVMF_JITTER_BUFFER_BUFFERING_STATUS_EVENT_CYCLES * 1000) / PVMF_JITTER_BUFFER_BUFFERING_STATUS_EVENT_FREQUENCY;

    iDisableFireWallPackets = false;
    //iPlayingAfterSeek = false;

    //Event Notifier initialization/reseting
    iIncomingMediaInactivityDurationCallBkId = 0;
    iIncomingMediaInactivityDurationCallBkPending = false;
    iNotifyBufferingStatusCallBkId = 0;
    iNotifyBufferingStatusCallBkPending = false;


    if (aReleaseMemory)
    {
        if (ipJitterBufferMisc)
            PVMF_BASE_NODE_DELETE(ipJitterBufferMisc);
    }

    ipJitterBufferMisc = NULL;
    ipEventNotifier = NULL;


    /* Clear queued messages in ports */
    uint32 i;
    for (i = 0; i < iPortVector.size(); i++)
    {
        PVMFJitterBufferPortParams* pPortParams = NULL;
        bool bRet = getPortContainer(iPortVector[i], pPortParams);
        if (bRet)
        {
            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                pPortParams->ipJitterBuffer->ResetJitterBuffer();
            }

            pPortParams->ResetParams();
        }
        iPortVector[i]->ClearMsgQueues();
    }

    //Cleaning up of conatiner objects
    /* delete corresponding port params */

    if (aReleaseMemory)
    {
        //port vect and port params Q
        Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
        for (it = iPortParamsQueue.begin();
                it != iPortParamsQueue.end();
                it++)
        {
            PVMFJitterBufferPortParams* pPortParams = *it;

            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                if (ipJitterBufferFactory)
                {
                    IPayloadParser* payLoadParser = NULL;
                    pPortParams->ipJitterBuffer->getPayloadParser(payLoadParser);
                    PVMFJitterBufferMisc::DestroyPayloadParser(iPayloadParserRegistry, pPortParams->iMimeType.get_cstr(), payLoadParser);
                    ipJitterBufferFactory->Destroy(pPortParams->ipJitterBuffer);
                }
            }

            OSCL_DELETE(&pPortParams->irPort);
            OSCL_DELETE(pPortParams);
        }
        iPortParamsQueue.clear();
        iPortVector.clear();
        iPortVector.Reconstruct();
    }

    iPayloadParserRegistry = NULL;

    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::ResetNodeParams Out -"));
    return;
}

PVMFJitterBufferNode::~PVMFJitterBufferNode()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::~PVMFJitterBufferNode In"));
    LogSessionDiagnostics();
    ResetNodeParams();

    Cancel();
    /* thread logoff */
    if (IsAdded())
        RemoveFromScheduler();
    CleanUp();
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::~PVMFJitterBufferNode Out"));
}

void PVMFJitterBufferNode::CleanUp()    //Reverse of Construct
{
    //noop
}

///////////////////////////////////////////////////////////////////////////////
//Implementation of overrides from PVInterface
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//Checks if the instance of PVMFJitterBufferExtensionInterfaceImpl is existing
//If existing: Query from this interface if UUID mentioned is supported
//If not existing: Instantiate PVMFJitterBufferExtensionInterfaceImpl
//and query requested interface from the PVMFJitterBufferExtensionInterfaceImpl
//Return Values:true/false
//Leave Codes: OsclErrNoMemory
//Leave Condition: If instance of PVMFJitterBufferExtensionInterfaceImpl cannot
//be instantiated.
///////////////////////////////////////////////////////////////////////////////
bool PVMFJitterBufferNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::queryInterface In"));
    iface = NULL;
    if (uuid == PVUuid(PVMF_JITTERBUFFERNODE_EXTENSIONINTERFACE_UUID))
    {
        if (!ipExtensionInterface)
        {
            OsclMemAllocator alloc;
            int32 err;
            OsclAny*ptr = NULL;
            OSCL_TRY(err,
                     ptr = alloc.ALLOCATE(sizeof(PVMFJitterBufferExtensionInterfaceImpl));
                    );
            if (err != OsclErrNone || !ptr)
            {
                PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::queryInterface: Error - Out of memory"));
                OSCL_LEAVE(OsclErrNoMemory);
            }
            ipExtensionInterface =
                OSCL_PLACEMENT_NEW(ptr, PVMFJitterBufferExtensionInterfaceImpl(this));
        }
        return (ipExtensionInterface->queryInterface(uuid, iface));
    }
    else
    {
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////
//Retrives a port iterator.
//Can Leave:No
//Return values: PVMFSuccess/PVMFErrInvalidState
//PVMFSuccess - If API call is successful
////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFPortIter* PVMFJitterBufferNode::GetPorts(const PVMFPortFilter* aFilter)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::GetPorts"));
    OSCL_UNUSED_ARG(aFilter);//port filter is not implemented.
    iPortVector.Reset();
    return &iPortVector;
}

///////////////////////////////////////////////////////////////////////////////
//Retrives a port iterator.
//Can Leave:No
//Return values: PVMFSuccess/PVMFErrInvalidState
//PVMFSuccess - If API call is successful
//PVMFErrInvalidState - If API is called in the invalid state
////////////////////////////////////////////////////////////////////////////////

void PVMFJitterBufferNode::HandlePortActivity(const PVMFPortActivity& aActivity)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::PortActivity: port=0x%x, type=%d",
                         aActivity.iPort, aActivity.iType));

    PVMFJitterBufferPortParams* portParamsPtr = NULL;
    PVMFJitterBufferPort* jbPort = OSCL_STATIC_CAST(PVMFJitterBufferPort*, aActivity.iPort);
    portParamsPtr = jbPort->iPortParams;

    if (aActivity.iType != PVMF_PORT_ACTIVITY_DELETED)
    {
        if (portParamsPtr == NULL)
        {
            ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aActivity.iPort));
            PVMF_JBNODE_LOGERROR((0, "0x%x PVMFJitterBufferNode::HandlePortActivity - getPortContainer Failed", this));
            return;
        }
    }

    /*
     * A port is reporting some activity or state change.  This code
     * figures out whether we need to queue a processing event
     * for the AO, and/or report a node event to the observer.
     */

    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_CREATED:
            /*
             * Report port created info event to the node.
             */
            ReportInfoEvent(PVMFInfoPortCreated, (OsclAny*)aActivity.iPort);
            break;

        case PVMF_PORT_ACTIVITY_DELETED:
            /*
             * Report port deleted info event to the node.
             */
            ReportInfoEvent(PVMFInfoPortDeleted, (OsclAny*)aActivity.iPort);
            break;

        case PVMF_PORT_ACTIVITY_CONNECT:
            //nothing needed.
            break;

        case PVMF_PORT_ACTIVITY_DISCONNECT:
        {
            if (ipJitterBufferMisc)
            {
                LogSessionDiagnostics();
                ipJitterBufferMisc->StreamingSessionStopped();
            }
        }
        break;

        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
        {
            if (portParamsPtr->iProcessOutgoingMessages)
            {
                /*
                 * An outgoing message was queued on this port.
                 * All ports have outgoing messages
                 * in this node
                 */
                QueuePortActivity(portParamsPtr, aActivity);
            }
        }
        break;

        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
        {
            /*
             * An outgoing message was queued on this port.
             * Only input and feedback ports have incoming messages
             * in this node
             */
            int32 portTag = portParamsPtr->iTag;
            switch (portTag)
            {
                case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
                case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
                    if (portParamsPtr->iProcessIncomingMessages)
                    {
                        QueuePortActivity(portParamsPtr, aActivity);
                    }
                    break;

                default:
                    OSCL_ASSERT(false);
                    break;
            }
        }
        break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_BUSY:
        {
            int32 portTag = portParamsPtr->iTag;
            switch (portTag)
            {
                case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
                    /*
                     * We typically use incoming port's outgoing q
                     * only in case of 3GPP streaming, wherein we
                     * send firewall packets. If it is busy, it does
                     * not stop us from registering incoming data pkts.
                     * so do nothing.
                     */
                    break;

                case PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT:
                {
                    /*
                     * This implies that this output port cannot accept any more
                     * msgs on its outgoing queue. This would usually imply that
                     * the corresponding input port must stop processing messages,
                     * however in case of jitter buffer the input and output ports
                     * are separated by a huge jitter buffer. Therefore continue
                     * to process the input.
                     */
                }
                break;

                case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
                    portParamsPtr->iProcessIncomingMessages = false;
                    break;

                default:
                    OSCL_ASSERT(false);
                    break;
            }
        }
        break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_READY:
        {
            int32 portTag = portParamsPtr->iTag;
            /*
             * Outgoing queue was previously busy, but is now ready.
             * We may need to schedule new processing events depending
             * on the port type.
             */
            switch (portTag)
            {
                case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
                    /*
                     * We never did anything in PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_BUSY
                     * so do nothing
                     */
                    break;

                case PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT:
                {
                    /*
                     * This implies that this output port can accept more
                     * msgs on its outgoing queue. This implies that the corresponding
                     * input port can start processing messages again.
                     */
                    PVMFJitterBufferPort* jbPort = OSCL_STATIC_CAST(PVMFJitterBufferPort*, aActivity.iPort);
                    PVMFJitterBufferPortParams* inPortParams = jbPort->iCounterpartPortParams;
                    if (inPortParams != NULL)
                    {
                        inPortParams->iProcessIncomingMessages = true;
                    }
                    else
                    {
                        OSCL_ASSERT(false);
                    }
                }
                break;

                case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
                    portParamsPtr->iProcessIncomingMessages = true;
                    break;

                default:
                    OSCL_ASSERT(false);
                    break;
            }
            Reschedule();
        }
        break;

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_BUSY:
        {
            /*
             * The connected port has become busy (its incoming queue is
             * busy).
             */
            int32 portTag = portParamsPtr->iTag;
            switch (portTag)
            {
                case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
                    break;

                case PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT:
                {
                    /*
                     * This implies that this output port cannot send any more
                     * msgs from its outgoing queue. It should stop processing
                     * messages till the connect port is ready.
                     */
                    portParamsPtr->iProcessOutgoingMessages = false;
                }
                break;

                case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
                    portParamsPtr->iProcessOutgoingMessages = false;
                    break;

                default:
                    OSCL_ASSERT(false);
                    break;
            }
        }
        break;

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY:
        {
            /*
             * The connected port has transitioned from Busy to Ready.
             * It's time to start processing messages outgoing again.
             */
            int32 portTag = portParamsPtr->iTag;
            switch (portTag)
            {
                case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
                    break;

                case PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT:
                    /*
                     * This implies that this output port can now send
                     * msgs from its outgoing queue. It can start processing
                     * messages now.
                     */
                    portParamsPtr->iProcessOutgoingMessages = true;
                    break;

                case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
                    portParamsPtr->iProcessOutgoingMessages = true;
                    break;

                default:
                    OSCL_ASSERT(false);
                    break;
            }
            Reschedule();
        }
        break;

        default:
            break;
    }
}

/////////////////////////////////////////////////////
// Port Processing routines
/////////////////////////////////////////////////////
void PVMFJitterBufferNode::QueuePortActivity(PVMFJitterBufferPortParams* aPortParams,
        const PVMFPortActivity &aActivity)
{
    OSCL_UNUSED_ARG(aPortParams);
    OSCL_UNUSED_ARG(aActivity);

    /*
     * wake up the AO to process the port activity event.
     */
    Reschedule();

}

///////////////////////////////////////////////////////////////////////////////
//Extension interfaces function implementation
///////////////////////////////////////////////////////////////////////////////
void PVMFJitterBufferNode::SetRTCPIntervalInMicroSecs(uint32 aRTCPInterval)
{
    OSCL_UNUSED_ARG(aRTCPInterval);
}

bool PVMFJitterBufferNode::SetPortParams(PVMFPortInterface* aPort,
        uint32 aTimeScale,
        uint32 aBitRate,
        OsclRefCounterMemFrag& aConfig,
        bool aRateAdaptation,
        uint32 aRateAdaptationFeedBackFrequency)
{
    return SetPortParams(aPort, aTimeScale, aBitRate, aConfig, aRateAdaptation,
                         aRateAdaptationFeedBackFrequency, false);
}

bool PVMFJitterBufferNode::SetPlayRange(int32 aStartTimeInMS,
                                        int32 aStopTimeInMS,
                                        bool aPlayAfterASeek,
                                        bool aStopTimeAvailable)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetPlayRange In StartTime[%d], StopTime[%d] StopTimeValid[%d] PlayingAfterSeek[%d]", aStartTimeInMS, aStopTimeInMS, aStopTimeAvailable, aPlayAfterASeek));
    ipJitterBufferMisc->SetPlayRange(aStartTimeInMS, aStopTimeInMS, aPlayAfterASeek, aStopTimeAvailable);
    return true;
}

void PVMFJitterBufferNode::SetPlayBackThresholdInMilliSeconds(uint32 aThreshold)
{
    OSCL_UNUSED_ARG(aThreshold);
}

void PVMFJitterBufferNode::SetJitterBufferRebufferingThresholdInMilliSeconds(uint32 aThreshold)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetJitterBufferRebufferingThresholdInMilliSeconds Threshhold[%d]", aThreshold));
    if (aThreshold < iJitterBufferDurationInMilliSeconds)
    {
        iRebufferingThreshold = aThreshold;
        Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
        for (it = iPortParamsQueue.begin(); it != iPortParamsQueue.end(); it++)
        {
            PVMFJitterBufferPortParams* pPortParams = *it;
            if ((pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT))
            {
                PVMFJitterBuffer* jitterBuffer = pPortParams->ipJitterBuffer;
                if (jitterBuffer)
                {
                    jitterBuffer->SetRebufferingThresholdInMilliSeconds(aThreshold);
                }
            }
        }
    }
}

void PVMFJitterBufferNode::GetJitterBufferRebufferingThresholdInMilliSeconds(uint32& aThreshold)
{
    aThreshold = iRebufferingThreshold;
}

void PVMFJitterBufferNode::SetJitterBufferDurationInMilliSeconds(uint32 aDuration)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetJitterBufferDurationInMilliSeconds Duration [%d]", aDuration));
    uint32 duration = iJitterBufferDurationInMilliSeconds;
    if (aDuration > iRebufferingThreshold)
    {
        duration = aDuration;
        iJitterBufferDurationInMilliSeconds = duration;
    }

    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
    for (it = iPortParamsQueue.begin(); it != iPortParamsQueue.end(); it++)
    {
        PVMFJitterBufferPortParams* pPortParams  = *it;
        if ((pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT))
        {
            PVMFJitterBuffer* jitterBuffer = pPortParams->ipJitterBuffer;
            if (jitterBuffer)
            {
                jitterBuffer->SetDurationInMilliSeconds(duration);
            }
        }
    }
}

void PVMFJitterBufferNode::GetJitterBufferDurationInMilliSeconds(uint32& duration)
{
    duration = iJitterBufferDurationInMilliSeconds;
}

void PVMFJitterBufferNode::SetEarlyDecodingTimeInMilliSeconds(uint32 duration)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetEarlyDecodingTimeInMilliSeconds - Early Decoding Time [%d]", duration));
    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator iter;
    for (iter = iPortParamsQueue.begin(); iter != iPortParamsQueue.end(); iter++)
    {
        PVMFJitterBufferPortParams* pPortParams = *iter;
        if (pPortParams && (pPortParams->ipJitterBuffer) && (PVMF_JITTER_BUFFER_PORT_TYPE_INPUT == pPortParams->iTag))
        {
            pPortParams->ipJitterBuffer->SetEarlyDecodingTimeInMilliSeconds(duration);
        }
    }
}

void PVMFJitterBufferNode::SetBurstThreshold(float burstThreshold)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetBurstThreshold burstThreshold[%f]", burstThreshold));
    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator iter;
    for (iter = iPortParamsQueue.begin(); iter != iPortParamsQueue.end(); iter++)
    {
        PVMFJitterBufferPortParams* pPortParams = *iter;
        if (pPortParams && (pPortParams->ipJitterBuffer) && (PVMF_JITTER_BUFFER_PORT_TYPE_INPUT == pPortParams->iTag))
        {
            pPortParams->ipJitterBuffer->SetBurstThreshold(burstThreshold);
        }
    }
}

void PVMFJitterBufferNode::SetMaxInactivityDurationForMediaInMs(uint32 aDuration)
{
    iMaxInactivityDurationForMediaInMs = aDuration;
}

void PVMFJitterBufferNode::GetMaxInactivityDurationForMediaInMs(uint32& aDuration)
{
    aDuration = iMaxInactivityDurationForMediaInMs;
}

void PVMFJitterBufferNode::SetClientPlayBackClock(PVMFMediaClock* aClientClock)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetClientPlayBackClock %x", aClientClock));
    //remove ourself as observer of old clock, if any.
    //Todo: Repetition should make sence only after call to Reset function.
    ipClientPlayBackClock = aClientClock;
}

bool PVMFJitterBufferNode::PrepareForRepositioning(bool oUseExpectedClientClockVal ,
        uint32 aExpectedClientClockVal)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::PrepareForRepositioning oUseExpectedClientClockVal[%d], aExpectedClientClockVal[%d]", oUseExpectedClientClockVal, aExpectedClientClockVal));
    iJitterBufferState = PVMF_JITTER_BUFFER_IN_TRANSITION;
    ipJitterBufferMisc->PrepareForRepositioning(oUseExpectedClientClockVal, aExpectedClientClockVal);
    return true;
}

bool PVMFJitterBufferNode::SetPortSSRC(PVMFPortInterface* aPort, uint32 aSSRC, bool a3GPPFCSSwitch)
{
    bool retval = false;
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetPortSSRC aPort[%x], aSSRC[%d]", aPort, aSSRC));
    if (aPort)
    {
        Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::const_iterator iter;
        for (iter = iPortParamsQueue.begin(); iter != iPortParamsQueue.end() ; ++iter)
        {
            if (iter && (*iter) && (&((*iter)->irPort) == aPort))
            {
                retval = true;
                ipJitterBufferMisc->SetPortSSRC(aPort, aSSRC, a3GPPFCSSwitch);
                break;
            }
        }
    }
    return retval;
}

bool PVMFJitterBufferNode::SetPortRTPParams(PVMFPortInterface* aPort,
        bool   aSeqNumBasePresent,
        uint32 aSeqNumBase,
        bool   aRTPTimeBasePresent,
        uint32 aRTPTimeBase,
        bool   aNPTTimeBasePresent,
        uint32 aNPTInMS,
        bool oPlayAfterASeek)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetPortRTPParams In Port - 0x%x aSeqNumBasePresent[%d], aSeqNumBase[%u], aRTPTimeBasePresent[%d] aRTPTimeBase[%u] aNPTTimeBasePresent[%d] aNPTInMS[%u] oPlayAfterASeek[%d]", aPort, aSeqNumBasePresent, aSeqNumBase, aRTPTimeBasePresent, aRTPTimeBase, aNPTTimeBasePresent, aNPTInMS, oPlayAfterASeek));
    uint32 i;
    //The above method is called only during 3GPP repositioning, however, since the aPort param in the signature
    // takes care only of the input port, the output port msg queues aren't cleared.
    // As a result ClearMsgQueues need to be called explicity on all the ports.
    //The oPlayAfterASeek check is necessary since the clearing of msg queues has to be carried out only during repositioning,
    // not otherwise
    if (oPlayAfterASeek)
    {
        for (i = 0; i < iPortParamsQueue.size(); i++)
        {
            PVMFJitterBufferPortParams* pPortParams = iPortParamsQueue[i];
            pPortParams->irPort.ClearMsgQueues();
        }
    }
    for (i = 0; i < iPortParamsQueue.size(); i++)
    {
        PVMFJitterBufferPortParams* pPortParams = iPortParamsQueue[i];

        if (&pPortParams->irPort == aPort)
        {
            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                if (pPortParams->ipJitterBuffer != NULL)
                {
                    PVMFRTPInfoParams rtpInfoParams;
                    rtpInfoParams.seqNumBaseSet = aSeqNumBasePresent;
                    rtpInfoParams.seqNum = aSeqNumBase;
                    rtpInfoParams.rtpTimeBaseSet = aRTPTimeBasePresent;
                    rtpInfoParams.rtpTime = aRTPTimeBase;
                    rtpInfoParams.nptTimeBaseSet = aNPTTimeBasePresent;
                    rtpInfoParams.nptTimeInMS = aNPTInMS;
                    rtpInfoParams.rtpTimeScale = pPortParams->iTimeScale;
                    pPortParams->ipJitterBuffer->setRTPInfoParams(rtpInfoParams, oPlayAfterASeek);
                    /* In case this is after a reposition purge the jitter buffer */
                    if (oPlayAfterASeek)
                    {
                        uint32 timebase32 = 0;
                        uint32 clientClock32 = 0;
                        bool overflowFlag = false;
                        if (ipClientPlayBackClock != NULL)
                            ipClientPlayBackClock->GetCurrentTime32(clientClock32, overflowFlag, PVMF_MEDIA_CLOCK_MSEC, timebase32);
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)

                        PVMF_JBNODE_LOGCLOCK_REBUFF((0, "PVMFJitterBufferNode::setPortRTPParams - Purging Upto SeqNum =%d", aSeqNumBase));
                        PVMF_JBNODE_LOGCLOCK_REBUFF((0, "PVMFJitterBufferNode::setPortRTPParams - Before Purge - ClientClock=%d",
                                                     clientClock32));
#endif
                        pPortParams->ipJitterBuffer->PurgeElementsWithSeqNumsLessThan(aSeqNumBase,
                                clientClock32);
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
                        PVMF_JBNODE_LOGCLOCK_REBUFF((0, "PVMFJitterBufferNode::setPortRTPParams - After Purge - ClientClock=%d",
                                                     clientClock32));
#endif
                        /*
                         * Since we flushed the jitter buffer, set it to ready state,
                         * reset the delay flag
                         */
                        iDelayEstablished = false;
                        iJitterBufferState = PVMF_JITTER_BUFFER_READY;
                        //iPlayingAfterSeek = true;
                    }
                }
                return true;
            }
        }
    }
    return false;
}

bool PVMFJitterBufferNode::SetPortRTCPParams(PVMFPortInterface* aPort,
        int aNumSenders,
        uint32 aRR,
        uint32 aRS)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetPortRTCPParams aPort - [0x%x]", aPort));
    return ipJitterBufferMisc->SetPortRTCPParams(aPort, aNumSenders, aRR, aRS);
}

PVMFTimestamp PVMFJitterBufferNode::GetActualMediaDataTSAfterSeek()
{
    return ipJitterBufferMisc->GetActualMediaDataTSAfterSeek();
}

PVMFTimestamp PVMFJitterBufferNode::GetMaxMediaDataTS()
{
    return ipJitterBufferMisc->GetMaxMediaDataTS();
}

PVMFStatus PVMFJitterBufferNode::SetServerInfo(PVMFJitterBufferFireWallPacketInfo& aServerInfo)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetServerInfo In"));
    if (iDisableFireWallPackets == false)
    {
        ipJitterBufferMisc->SetServerInfo(aServerInfo);
    }
    else
    {
        PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::setServerInfo: FW Pkts Disabled"));
        if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_PREPARE)
        {
            /* No firewall packet exchange - Complete Prepare */
            // Complete Prepare
            CommandComplete(iCurrentCommand, PVMFSuccess);
        }
    }
    return PVMFSuccess;
}

bool PVMFJitterBufferNode::setPortMediaParams(PVMFPortInterface* aPort,
        mediaInfo* aMediaInfo)
{
    bool retval = false;

    if (ipJitterBufferMisc)
    {
        retval = ipJitterBufferMisc->setPortMediaParams(aPort, aMediaInfo);
    }

    return retval;
}

void PVMFJitterBufferNode::setPayloadParserRegistry(PayloadParserRegistry* aRegistry)
{
    iPayloadParserRegistry = aRegistry;
}

PVMFStatus PVMFJitterBufferNode::setPortDataLogging(bool logEnable, OSCL_String* logPath)
{
    PVMFStatus status = PVMFFailure;
    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator iter;
    for (iter = iPortParamsQueue.begin(); iter != iPortParamsQueue.end(); iter++)
    {
        if (iter && (*iter) && ((*iter)->ipJitterBuffer) && (PVMF_JITTER_BUFFER_PORT_TYPE_INPUT == (*iter)->iTag))
        {
            (*iter)->ipJitterBuffer->SetDataLogging(logEnable, logPath);
        }
    }
    return status;
}

PVMFStatus PVMFJitterBufferNode::NotifyOutOfBandEOS()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::NotifyOutOfBandEOS In"));
    // Ignore Out Of Band EOS for any Non Live stream
    if (ipJitterBufferMisc && (!ipJitterBufferMisc->PlayStopTimeAvailable()))
    {
        if (iJitterBufferState != PVMF_JITTER_BUFFER_IN_TRANSITION)
        {
            ipJitterBufferMisc->SetSessionDurationExpired();
            CancelEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
            PVMF_JBNODE_LOGDATATRAFFIC((0, "PVMFJitterBufferNode::NotifyOutOfBandEOS - Out Of Band EOS Recvd"));
            PVMF_JBNODE_LOGCLOCK_SESSION_DURATION((0, "PVMFJitterBufferNode::NotifyOutOfBandEOS - Out Of Band EOS Recvd"));
        }
        else
        {
            PVMF_JBNODE_LOGDATATRAFFIC((0, "PVMFJitterBufferNode::NotifyOutOfBandEOS - Ignoring Out Of Band EOS in Transition State"));
            PVMF_JBNODE_LOGCLOCK_SESSION_DURATION((0, "PVMFJitterBufferNode::NotifyOutOfBandEOS - Ignoring Out Of Band EOS in Transition State"));
        }
    }
    else
    {
        PVMF_JBNODE_LOGDATATRAFFIC((0, "PVMFJitterBufferNode::NotifyOutOfBandEOS - Ignoring Out Of Band EOS for Non Live Stream"));
        PVMF_JBNODE_LOGCLOCK_SESSION_DURATION((0, "PVMFJitterBufferNode::NotifyOutOfBandEOS - Ignoring Out Of Band EOS for Non Live Stream"));
    }
    return PVMFSuccess;
}

PVMFStatus PVMFJitterBufferNode::SendBOSMessage(uint32 aStreamID)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SendBOSMessage In"));
    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
    for (it = iPortParamsQueue.begin();
            it != iPortParamsQueue.end();
            it++)
    {
        PVMFJitterBufferPortParams* pPortParams = *it;
        if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
        {
            if (pPortParams->ipJitterBuffer)
            {
                pPortParams->ipJitterBuffer->QueueBOSCommand(aStreamID);
            }
        }
    }
    PVMF_JBNODE_LOGDATATRAFFIC((0, "PVMFJitterBufferNode::SendBOSMessage - BOS Recvd"));
    return PVMFSuccess;
}

void PVMFJitterBufferNode::SetJitterBufferChunkAllocator(OsclMemPoolResizableAllocator*
        aDataBufferAllocator, const PVMFPortInterface* aPort)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetJitterBufferChunkAllocator -aPort 0x%x", aPort));
    PVMFJitterBufferPort* port = OSCL_STATIC_CAST(PVMFJitterBufferPort*, aPort);
    if (port->iPortParams->ipJitterBuffer)
    {
        port->iPortParams->ipJitterBuffer->SetJitterBufferChunkAllocator(aDataBufferAllocator);
    }
}

void PVMFJitterBufferNode::SetJitterBufferMemPoolInfo(const PvmfPortBaseImpl* aPort,
        uint32 aSize,
        uint32 aResizeSize,
        uint32 aMaxNumResizes,
        uint32 aExpectedNumberOfBlocksPerBuffer)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetJitterBufferMemPoolInfo Port 0x%x", aPort));
    PVMFJitterBufferPort* port = OSCL_STATIC_CAST(PVMFJitterBufferPort*, aPort);
    if (port->iPortParams->ipJitterBuffer)
    {
        port->iPortParams->ipJitterBuffer->SetJitterBufferMemPoolInfo(aSize, aResizeSize, aMaxNumResizes, aExpectedNumberOfBlocksPerBuffer);
    }
}

void PVMFJitterBufferNode::GetJitterBufferMemPoolInfo(const PvmfPortBaseImpl* aPort,
        uint32& aSize,
        uint32& aResizeSize,
        uint32& aMaxNumResizes,
        uint32& aExpectedNumberOfBlocksPerBuffer) const
{
    PVMFJitterBufferPort* port = OSCL_STATIC_CAST(PVMFJitterBufferPort*, aPort);
    if (port->iPortParams->ipJitterBuffer)
    {
        port->iPortParams->ipJitterBuffer->GetJitterBufferMemPoolInfo(aSize, aResizeSize, aMaxNumResizes, aExpectedNumberOfBlocksPerBuffer);
    }
}

void PVMFJitterBufferNode::SetSharedBufferResizeParams(uint32 maxNumResizes,
        uint32 resizeSize)
{
    // make sure we're in a state that makes sense
    OSCL_ASSERT((iInterfaceState == EPVMFNodeCreated) ||
                (iInterfaceState == EPVMFNodeIdle) ||
                (iInterfaceState == EPVMFNodeInitialized));

    iMaxNumBufferResizes = maxNumResizes;
    iBufferResizeSize = resizeSize;
}

void PVMFJitterBufferNode::GetSharedBufferResizeParams(uint32& maxNumResizes,
        uint32& resizeSize)
{
    maxNumResizes = iMaxNumBufferResizes;
    resizeSize = iBufferResizeSize;
}

bool PVMFJitterBufferNode::ClearJitterBuffer(PVMFPortInterface* aPort,
        uint32 aSeqNum)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::ClearJitterBuffer Port 0x%x aSeqNum[%d]", aPort, aSeqNum));
    /* Typically called only for HTTP streaming sessions */
    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
    for (it = iPortParamsQueue.begin(); it != iPortParamsQueue.end(); it++)
    {
        PVMFJitterBufferPortParams* pPortParams = *it;
        pPortParams->irPort.ClearMsgQueues();
    }

    for (it = iPortParamsQueue.begin(); it != iPortParamsQueue.end(); it++)
    {
        PVMFJitterBufferPortParams* pPortParams = *it;
        if (&pPortParams->irPort == aPort && (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT) && (pPortParams->ipJitterBuffer != NULL))
        {
            uint32 timebase32 = 0;
            uint32 clientClock32 = 0;
            bool overflowFlag = false;
            if (ipClientPlayBackClock != NULL)
                ipClientPlayBackClock->GetCurrentTime32(clientClock32, overflowFlag, PVMF_MEDIA_CLOCK_MSEC, timebase32);
            pPortParams->ipJitterBuffer->PurgeElementsWithSeqNumsLessThan(aSeqNum,
                    clientClock32);
            ipJitterBufferMisc->ResetSession();
            iJitterBufferState = PVMF_JITTER_BUFFER_READY;
            return true;
        }
    }
    return false;
}

void PVMFJitterBufferNode::FlushJitterBuffer()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::FlushJitterBuffer In"));
    for (uint32 i = 0; i < iPortParamsQueue.size(); i++)
    {
        PVMFJitterBufferPortParams* pPortParams = iPortParamsQueue[i];
        if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
        {
            if (pPortParams->ipJitterBuffer != NULL)
            {
                pPortParams->ipJitterBuffer->FlushJitterBuffer();
            }
        }
    }
}

void PVMFJitterBufferNode::SetInputMediaHeaderPreParsed(PVMFPortInterface* aPort,
        bool aHeaderPreParsed)
{
    PVMFJitterBufferPort *port = OSCL_STATIC_CAST(PVMFJitterBufferPort*, aPort);
    if (port)
    {
        PVMFJitterBufferPortParams* portParams = port->GetPortParams();
        if (portParams && portParams->ipJitterBuffer)
        {
            portParams->ipJitterBuffer->SetInputPacketHeaderPreparsed(aHeaderPreParsed);
        }
    }
}

PVMFStatus PVMFJitterBufferNode::HasSessionDurationExpired(bool& aExpired)
{
    aExpired = ipJitterBufferMisc->IsSessionExpired();
    PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::HasSessionDurationExpired %d", aExpired));
    return PVMFSuccess;
}

bool PVMFJitterBufferNode::PurgeElementsWithNPTLessThan(NptTimeFormat &aNPTTime)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::PurgeElementsWithNPTLessThan"));
    bool retval = false;

    if (ipJitterBufferMisc)
    {
        retval = ipJitterBufferMisc->PurgeElementsWithNPTLessThan(aNPTTime);
    }

    iJitterBufferState = PVMF_JITTER_BUFFER_READY;

    return retval;
}

void PVMFJitterBufferNode::SetBroadCastSession()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetBroadCastSession"));
    iBroadCastSession = true;
    if (ipJitterBufferMisc)
    {
        ipJitterBufferMisc->SetBroadcastSession();
    }
}

void PVMFJitterBufferNode::DisableFireWallPackets()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::DisableFireWallPackets"));
    if (ipJitterBufferMisc)
        ipJitterBufferMisc->MediaReceivingChannelPreparationRequired(false);
}

void PVMFJitterBufferNode::UpdateJitterBufferState()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::UpdateJitterBufferState"));
    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator iter;
    for (iter = iPortParamsQueue.begin(); iter != iPortParamsQueue.end(); iter ++)
    {
        PVMFJitterBufferPortParams* ptr = *iter;
        if (PVMF_JITTER_BUFFER_PORT_TYPE_INPUT == ptr->iTag)
        {
            ptr->ipJitterBuffer->SetJitterBufferState(PVMF_JITTER_BUFFER_READY);
        }
    }
    iDelayEstablished = true;
}

void PVMFJitterBufferNode::StartOutputPorts()
{
    oStopOutputPorts = false;
}

void PVMFJitterBufferNode::StopOutputPorts()
{
    oStopOutputPorts = true;
}

///////////////////////////////////////////////////////////////////////////////
//Used for the implementation of extension interface functions
///////////////////////////////////////////////////////////////////////////////
bool
PVMFJitterBufferNode::SetPortParams(PVMFPortInterface* aPort,
                                    uint32 aTimeScale,
                                    uint32 aBitRate,
                                    OsclRefCounterMemFrag& aConfig,
                                    bool aRateAdaptation,
                                    uint32 aRateAdaptationFeedBackFrequency,
                                    uint aMaxNumBuffResizes, uint aBuffResizeSize)
{
    return SetPortParams(aPort, aTimeScale, aBitRate, aConfig, aRateAdaptation,
                         aRateAdaptationFeedBackFrequency, true,
                         aMaxNumBuffResizes, aBuffResizeSize);
}
bool
PVMFJitterBufferNode::SetPortParams(PVMFPortInterface* aPort,
                                    uint32 aTimeScale,
                                    uint32 aBitRate,
                                    OsclRefCounterMemFrag& aConfig,
                                    bool aRateAdaptation,
                                    uint32 aRateAdaptationFeedBackFrequency,
                                    bool aUserSpecifiedBuffParams,
                                    uint aMaxNumBuffResizes, uint aBuffResizeSize)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::SetPortParams"));
    OSCL_UNUSED_ARG(aUserSpecifiedBuffParams);
    uint32 ii;
    for (ii = 0; ii < iPortParamsQueue.size(); ii++)
    {
        PVMFJitterBufferPortParams* pPortParams = iPortParamsQueue[ii];

        if (&pPortParams->irPort == aPort)
        {
            pPortParams->iTimeScale = aTimeScale;
            pPortParams->iMediaClockConverter.set_timescale(aTimeScale);
            pPortParams->iBitrate = aBitRate;
            if (pPortParams->ipJitterBuffer)
            {
                pPortParams->ipJitterBuffer->SetTrackConfig(aConfig);
                pPortParams->ipJitterBuffer->SetTimeScale(aTimeScale);
                pPortParams->ipJitterBuffer->SetMediaClockConverter(&pPortParams->iMediaClockConverter);
            }


            /* Compute buffer size based on bitrate and jitter duration*/
            uint32 sizeInBytes = 0;
            if (((int32)iJitterBufferDurationInMilliSeconds > 0) &&
                    ((int32)aBitRate > 0))
            {
                uint32 byteRate = aBitRate / 8;
                uint32 overhead = (byteRate * PVMF_JITTER_BUFFER_NODE_MEM_POOL_OVERHEAD) / 100;
                uint32 durationInSec = iJitterBufferDurationInMilliSeconds / 1000;
                sizeInBytes = ((byteRate + overhead) * durationInSec);
                if (sizeInBytes < MIN_RTP_SOCKET_MEM_POOL_SIZE_IN_BYTES)
                {
                    sizeInBytes = MIN_RTP_SOCKET_MEM_POOL_SIZE_IN_BYTES;
                }
                sizeInBytes += (2 * MAX_SOCKET_BUFFER_SIZE);
            }

            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                PVMFJitterBuffer* jitterBuffer = NULL;
                jitterBuffer = pPortParams->ipJitterBuffer;

                if (jitterBuffer)
                {
                    pPortParams->ipJitterBuffer->SetJitterBufferMemPoolInfo(sizeInBytes, aBuffResizeSize, aMaxNumBuffResizes, 3000);
                }

                if (ipJitterBufferMisc)
                    ipJitterBufferMisc->SetRateAdaptationInfo(&pPortParams->irPort, aRateAdaptation, aRateAdaptationFeedBackFrequency, sizeInBytes);
            }

            iPortParamsQueue[ii] = pPortParams;
            return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
//OsclActiveObject Implementation
///////////////////////////////////////////////////////////////////////////////
/**
  * This AO handles both API commands and port activity.
  * The AO will either process one command or service one connected
  * port per call.  It will re-schedule itself and run continuously
  * until it runs out of things to do.
  */
void PVMFJitterBufferNode::Run()
{
    iNumRunL++;
    /*
     * Process commands.
     */
    if (!iInputCommands.empty())
    {
        if (ProcessCommand())
        {
            /*
             * note: need to check the state before re-scheduling
             * since the node could have been reset in the ProcessCommand
             * call.
             */
            if (iInterfaceState != EPVMFNodeCreated)
            {
                Reschedule();
            }
            return;
        }
    }

    /*
     * Process port activity
     */
    if (((iInterfaceState == EPVMFNodeInitialized) ||
            (iInterfaceState == EPVMFNodePrepared) ||
            (iInterfaceState == EPVMFNodeStarted)  ||
            (iInterfaceState == EPVMFNodePaused)) ||
            IsFlushPending())
    {
        uint32 i;
        for (i = 0; i < iPortVector.size(); i++)
        {
            PVMFJitterBufferPortParams* portContainerPtr =
                iPortVector[i]->iPortParams;

            if (portContainerPtr == NULL)
            {
                if (!getPortContainer(iPortVector[i], portContainerPtr))
                {
                    PVMF_JBNODE_LOGERROR((0, "0x%x PVMFJitterBufferNode::Run: Error - getPortContainer failed", this));
                    return;
                }
                iPortVector[i]->iPortParams = portContainerPtr;
            }

            ProcessPortActivity(portContainerPtr);
        }

        if (CheckForPortRescheduling())
        {
            /*
             * Re-schedule since there is atleast one port that needs processing
             */
            Reschedule();

            return;
        }
    }

    /*
     * If we get here we did not process any ports or commands.
     * Check for completion of a flush command...
     */
    if (IsFlushPending() && (!CheckForPortActivityQueues()))
    {
        uint32 i;
        /*
         * Debug check-- all the port queues should be empty at
         * this point.
         */
        for (i = 0; i < iPortVector.size(); i++)
        {
            if (iPortVector[i]->IncomingMsgQueueSize() > 0 ||
                    iPortVector[i]->OutgoingMsgQueueSize() > 0)
            {
                OSCL_ASSERT(false);
            }
        }
        /*
         * Flush is complete.  Go to prepared state.
         */
        SetState(EPVMFNodePrepared);
        /*
         * resume port input so the ports can be re-started.
         */
        for (i = 0; i < iPortVector.size(); i++)
        {
            iPortVector[i]->ResumeInput();
        }
        CommandComplete(iCurrentCommand, PVMFSuccess);
        Reschedule();

    }
    return;
}

void PVMFJitterBufferNode::DoCancel()
{
    /*
     * the base class cancel operation is sufficient.
     */
    OsclActiveObject::DoCancel();
}

bool PVMFJitterBufferNode::ProcessPortActivity(PVMFJitterBufferPortParams* aPortParams)
{
    if (!aPortParams)
    {
        return false;
    }

    PVMFStatus status = PVMFSuccess;
    switch (aPortParams->iTag)
    {
        case PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT:
        {
            if ((aPortParams->iProcessOutgoingMessages) &&
                    (aPortParams->irPort.OutgoingMsgQueueSize() > 0))
            {
                status = ProcessOutgoingMsg(aPortParams);
            }
            /*
             * Send data out of jitter buffer as long as there's:
             *  - more data to send
             *  - outgoing queue isn't in a Busy state.
             *  - ports are not paused
             */
            PVMFJitterBufferPort* outPort = OSCL_STATIC_CAST(PVMFJitterBufferPort*, &aPortParams->irPort);
            PVMFJitterBufferPortParams* inPortParamsPtr = outPort->iCounterpartPortParams;
            if (aPortParams->iProcessOutgoingMessages)
            {
                if ((oStopOutputPorts == false) && (inPortParamsPtr->iCanReceivePktFromJB))
                {
                    SendData(OSCL_STATIC_CAST(PVMFPortInterface*, &inPortParamsPtr->irPort));
                }
            }
        }
        break;

        case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
        {
            PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::ProcessPortActivity: input port- aPortParams->iProcessIncomingMessages %d aPortParams->iPort->IncomingMsgQueueSize()  %d" ,
                                 aPortParams->iProcessIncomingMessages, aPortParams->irPort.IncomingMsgQueueSize()));
            if ((aPortParams->iProcessIncomingMessages) &&
                    (aPortParams->irPort.IncomingMsgQueueSize() > 0))
            {
                status = ProcessIncomingMsg(aPortParams);
            }
            if ((aPortParams->iProcessOutgoingMessages) &&
                    (aPortParams->irPort.OutgoingMsgQueueSize() > 0))
            {
                status = ProcessOutgoingMsg(aPortParams);
            }
        }
        break;

        case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
        {
            PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::ProcessPortActivity: - aPortParams->iProcessIncomingMessages %d aPortParams->iPort->IncomingMsgQueueSize()  %d" ,
                                 aPortParams->iProcessIncomingMessages, aPortParams->irPort.IncomingMsgQueueSize()));

            if ((aPortParams->iProcessIncomingMessages) &&
                    (aPortParams->irPort.IncomingMsgQueueSize() > 0))
            {
                status = ProcessIncomingMsg(aPortParams);
            }
            if ((aPortParams->iProcessOutgoingMessages) &&
                    (aPortParams->irPort.OutgoingMsgQueueSize() > 0))
            {
                status = ProcessOutgoingMsg(aPortParams);
            }
        }
        break;

        default:
            break;
    }

    /*
     * Report any unexpected failure in port processing...
     * (the InvalidState error happens when port input is suspended,
     * so don't report it.)
     */
    if (status != PVMFErrBusy
            && status != PVMFSuccess
            && status != PVMFErrInvalidState)
    {
        PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ProcessPortActivity: Error - ProcessPortActivity failed. port=0x%x",
                              &aPortParams->irPort));
        ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(&aPortParams->irPort));
    }

    /*
     * return true if we processed an activity...
     */
    return (status != PVMFErrBusy);
}

///////////////////////////////////////////////////////////////////////////////
//Processing of incoming msg
///////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferNode::ProcessIncomingMsg(PVMFJitterBufferPortParams* aPortParams)
{
    PVUuid eventuuid = PVMFJitterBufferNodeEventTypeUUID;
    PVMFPortInterface* aPort = &aPortParams->irPort;

    PVMF_JBNODE_LOGDATATRAFFIC_IN((0, "PVMFJitterBufferNode::ProcessIncomingMsg: %s Tag %d", aPortParams->iMimeType.get_cstr(), aPortParams->iTag));

    aPortParams->iNumMediaMsgsRecvd++;

    switch (aPortParams->iTag)
    {
        case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
        {

            if (aPortParams->iMonitorForRemoteActivity == true)
            {
                CancelEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
                RequestEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
            }
            /* Parse packet header - mainly to retrieve time stamp */
            PVMFJitterBuffer* jitterBuffer = aPortParams->ipJitterBuffer;
            if (jitterBuffer == NULL)
            {
                PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ProcessIncomingMsg: findJitterBuffer failed"));
                int32 errcode = PVMFJitterBufferNodeUnableToRegisterIncomingPacket;
                ReportErrorEvent(PVMFErrArgument, (OsclAny*)(aPort), &eventuuid, &errcode);
                return PVMFErrArgument;
            }

            /*
             * Incoming message recvd on the input port.
             * Dequeue the message
             */
            PVMFSharedMediaMsgPtr msg;
            PVMFStatus status = aPort->DequeueIncomingMsg(msg);
            if (status != PVMFSuccess)
            {
                ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aPort));
                PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ProcessIncomingMsg: Error - INPUT PORT - DequeueIncomingMsg failed"));
                return status;
            }

            PVMFJitterBufferRegisterMediaMsgStatus regStatus = jitterBuffer->RegisterMediaMsg(msg);
            switch (regStatus)
            {

                case PVMF_JB_REGISTER_MEDIA_MSG_SUCCESS:
                {
                    PVMF_JBNODE_LOGDATATRAFFIC_IN((0, "PVMFJitterBufferNode::ProcessIncomingMsg: Packet registered successfully Mime %s", aPortParams->iMimeType.get_cstr()));
                }
                break;
                case PVMF_JB_REGISTER_MEDIA_MSG_FAILURE_JB_FULL:
                {
                    aPortParams->iProcessIncomingMessages = false;
                    jitterBuffer->NotifyFreeSpaceAvailable();
                    int32 infocode = PVMFJitterBufferNodeJitterBufferFull;
                    ReportInfoEvent(PVMFInfoOverflow, (OsclAny*)(aPort), &eventuuid, &infocode);
                    PVMF_JBNODE_LOGDATATRAFFIC_IN((0, "PVMFJitterBufferNode::ProcessIncomingMsg: Jitter Buffer full"));
                    PVMF_JBNODE_LOGDATATRAFFIC_FLOWCTRL_E((0, "PVMFJitterBufferNode::ProcessIncomingMsg: Jitter Buffer full"));
                    return PVMFErrBusy;
                }
                break;
                case PVMF_JB_REGISTER_MEDIA_MSG_FAILURE_INSUFFICIENT_MEMORY_FOR_PACKETIZATION:
                {
                    aPortParams->iProcessIncomingMessages = false;
                    jitterBuffer->NotifyFreeSpaceAvailable();
                    return PVMFErrBusy;
                }
                break;
                case PVMF_JB_REGISTER_MEDIA_MSG_ERR_CORRUPT_PACKET:
                {
                    PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ProcessIncomingMsg: unable to register packet"));
                    int32 errcode = PVMFJitterBufferNodeUnableToRegisterIncomingPacket;
                    ReportErrorEvent(PVMFErrArgument, (OsclAny*)(aPort), &eventuuid, &errcode);
                    return PVMFErrArgument;
                }
                case PVMF_JB_REGISTER_MEDIA_MSG_ERR_EOS_SIGNALLED:
                {
                    PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ProcessIncomingMsg: data received after signalling EOS"));
                }
                break;
                default:
                {
                    PVMF_JBNODE_LOGDATATRAFFIC_IN((0, "PVMFJitterBufferNode::ProcessIncomingMsg: Packet could not be registered Register packet returned status %d", regStatus));
                    PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ProcessIncomingMsg: Packet could not be registered Register packet returned status %d", regStatus));
                }
            }
            SendData(aPort);
        }
        break;

        case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
        {
            /*
            * Incoming RTCP reports - recvd on the input port.
            * Dequeue the message - Need to fully implement
            * RTCP
            */
            PVMFSharedMediaMsgPtr msg;
            PVMFStatus status = aPort->DequeueIncomingMsg(msg);
            if (status != PVMFSuccess)
            {
                ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aPort));
                PVMF_JBNODE_LOGERROR((0, "0x%x PVMFJitterBufferNode::ProcessIncomingMsg: Error - FB PORT - DequeueIncomingMsg failed", this));
                return status;
            }
            status = ipJitterBufferMisc->ProcessFeedbackMessage(*aPortParams, msg);
            PVMF_JBNODE_LOGDATATRAFFIC_IN((0, "PVMFJitterBufferNode::ProcessIncomingMsg: Feedback Packet registered with status code status %d", status));
        }
        break;

        default:
        {
            ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aPort));
            PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ProcessIncomingMsg - Invalid Port Tag"));
            return PVMFFailure;
        }
    }
    return (PVMFSuccess);
}

///////////////////////////////////////////////////////////////////////////////
//Processing of outgoing msg
///////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferNode::ProcessOutgoingMsg(PVMFJitterBufferPortParams* aPortParams)
{
    PVMFPortInterface* aPort = &aPortParams->irPort;
    /*
     * Called by the AO to process one message off the outgoing
     * message queue for the given port.  This routine will
     * try to send the data to the connected port.
     */
    PVMF_JBNODE_LOGINFO((0, "0x%x PVMFJitterBufferNode::ProcessOutgoingMsg: aPort=0x%x", this, aPort));

    /*
     * If connected port is busy, the outgoing message cannot be process. It will be
     * queued to be processed again after receiving PORT_ACTIVITY_CONNECTED_PORT_READY
     * from this port.
     */
    if (aPort->IsConnectedPortBusy())
    {
        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "0x%x PVMFJitterBufferNode::ProcessOutgoingMsg: Connected port is busy", this));
        aPortParams->iProcessOutgoingMessages = false;
        return PVMFErrBusy;
    }

    PVMFStatus status = PVMFSuccess;

    status = aPort->Send();
    if (status == PVMFErrBusy)
    {
        PVMF_JBNODE_LOGDATATRAFFIC((0, "PVMFJitterBufferNode::ProcessOutgoingMsg: Connected port goes into busy state"));
        aPortParams->iProcessOutgoingMessages = false;
    }
    else
    {
        aPortParams->iNumMediaMsgsSent++;
    }
    return status;
}

PVMFStatus
PVMFJitterBufferNode::SendData(PVMFPortInterface* aPort)
{
    PVMFJitterBufferPort* jbPort =
        OSCL_STATIC_CAST(PVMFJitterBufferPort*, aPort);

    PVMFPortInterface* outputPort = jbPort->iPortCounterpart;
    PVMFJitterBufferPortParams* portParamsPtr = jbPort->iPortParams;
    PVMFJitterBufferPortParams* outPortParamsPtr = jbPort->iCounterpartPortParams;
    PVMFJitterBuffer* jitterBuffer = portParamsPtr->ipJitterBuffer;

    PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData In %s", outPortParamsPtr->iMimeType.get_cstr())) ;

    if (!(portParamsPtr->iCanReceivePktFromJB))
    {
        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData - Cant retrieve pkt from JB"));
        return PVMFFailure;
    }

    if (outPortParamsPtr->irPort.IsOutgoingQueueBusy())
    {
        outPortParamsPtr->iProcessOutgoingMessages = false;
        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData - Port found to be busy %s", outPortParamsPtr->iMimeType.get_cstr())) ;
        return PVMFErrBusy;
    }

    if (oStopOutputPorts)
    {
        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData from Mime[%s] Output ports Stopped--", jbPort->iPortParams->iMimeType.get_cstr()));
        return PVMFSuccess;
    }

    PVMFSharedMediaMsgPtr mediaOutMsg;
    bool cmdPacket = false;
    PVMFStatus status = jbPort->iPortParams->ipJitterBuffer->RetrievePacket(mediaOutMsg, cmdPacket);
    while (PVMFSuccess == status)
    {
        if (!cmdPacket)
        {
            //media msg
            outPortParamsPtr->iLastMsgTimeStamp = mediaOutMsg->getTimestamp();
        }

        status = outputPort->QueueOutgoingMsg(mediaOutMsg);

        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData from Mime[%s] MediaMsg SeqNum[%d], Ts[%d]", jbPort->iPortParams->iMimeType.get_cstr(), mediaOutMsg->getSeqNum(), mediaOutMsg->getTimestamp()));

        if (outPortParamsPtr->irPort.IsOutgoingQueueBusy())
        {
            outPortParamsPtr->iProcessOutgoingMessages = false;
            PVMFJitterBufferStats stats = jbPort->iPortParams->ipJitterBuffer->getJitterBufferStats();
            PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "Send Data Mime %s stats.currentOccupancy[%d], stats.maxSeqNumRegistered[%d], stats.lastRetrievedSeqNum[%d] stats.maxTimeStampRetrievedWithoutRTPOffset[%d] SendOut Pkt[%d]", jbPort->iPortParams->iMimeType.get_cstr(), stats.currentOccupancy, stats.maxSeqNumRegistered, stats.lastRetrievedSeqNum, stats.maxTimeStampRetrievedWithoutRTPOffset, outPortParamsPtr->iNumMediaMsgsSent));
            return PVMFErrBusy;
        }
        status = jbPort->iPortParams->ipJitterBuffer->RetrievePacket(mediaOutMsg, cmdPacket);
    }

    if (PVMFErrNotReady == status)
    {
        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData from Mime[%s] JB not ready", jbPort->iPortParams->iMimeType.get_cstr()));


        uint32 currentTime32 = 0;
        uint32 currentTimeBase32 = 0;
        bool overflowFlag = false;
        ipJitterBufferMisc->GetEstimatedServerClock().GetCurrentTime32(currentTime32,
                overflowFlag,
                PVMF_MEDIA_CLOCK_MSEC,
                currentTimeBase32);


        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData - Estimated serv clock %d", currentTime32));
        currentTime32 = 0;
        currentTimeBase32 = 0;
        ipClientPlayBackClock->GetCurrentTime32(currentTime32,
                                                overflowFlag,
                                                PVMF_MEDIA_CLOCK_MSEC,
                                                currentTimeBase32);
        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData - Client serv clock %d", currentTime32));


        PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::SendData - Occupancy is %d delayestablish %d", jbPort->iPortParams->ipJitterBuffer->getJitterBufferStats().currentOccupancy, iDelayEstablished));

        portParamsPtr->iCanReceivePktFromJB = false;
        jitterBuffer->NotifyCanRetrievePacket();
    }

    return status;
}

bool PVMFJitterBufferNode::CheckForPortRescheduling()
{
    //This method is only called from JB Node AO's Run.
    //Purpose of this method is to determine whether the node
    //needs scheduling based on any outstanding port activities
    //Here is the scheduling criteria for different port types:
    //a) PVMF_JITTER_BUFFER_PORT_TYPE_INPUT - If there are incoming
    //msgs waiting in incoming msg queue then node needs scheduling,
    //as long oProcessIncomingMessages is true. This boolean stays true
    //as long we can register packets in JB. If JB is full this boolean
    //is made false (when CheckForSpaceInJitterBuffer() returns false)
    //and is once again made true in JitterBufferFreeSpaceAvailable() callback.
    //We also use the input port briefly as a bidirectional port in case of
    //RTSP streaming to do firewall packet exchange. So if there are outgoing
    //msgs and oProcessOutgoingMessages is true then node needs scheduling.
    //b) PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT - As long as:
    //  - there are msgs in outgoing queue
    //  - oProcessOutgoingMessages is true
    //  - and as long as there is data in JB and we are not in buffering
    //then node needs scheduling.
    //c) PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK - As long as:
    //  - there are msgs in incoming queue and oProcessIncomingMessages is true
    //  - there are msgs in outgoing queue and oProcessOutgoingMessages is true
    uint32 i;
    for (i = 0; i < iPortVector.size(); i++)
    {
        PVMFJitterBufferPortParams* portContainerPtr = iPortVector[i]->iPortParams;
        if (portContainerPtr == NULL)
        {
            PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::CheckForPortRescheduling: Error - GetPortContainer failed"));
            return false;
        }
        if (portContainerPtr->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
        {
            if (portContainerPtr->irPort.IncomingMsgQueueSize() > 0)
            {
                if (portContainerPtr->iProcessIncomingMessages)
                {
                    /*
                     * Found a port that has outstanding activity and
                     * is not busy.
                     */
                    return true;
                }
            }
            if (portContainerPtr->irPort.OutgoingMsgQueueSize() > 0)
            {
                if (portContainerPtr->iProcessOutgoingMessages)
                {
                    /*
                     * Found a port that has outstanding activity and
                     * is not busy.
                     */
                    return true;
                }
            }
        }
        else if (portContainerPtr->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT)
        {
            PVMFJitterBufferPort* jbPort =
                OSCL_STATIC_CAST(PVMFJitterBufferPort*, &portContainerPtr->irPort);
            PVMFJitterBufferPortParams* inPortParamsPtr = jbPort->iCounterpartPortParams;
            if ((portContainerPtr->irPort.OutgoingMsgQueueSize() > 0) ||
                    (inPortParamsPtr->iCanReceivePktFromJB))
            {
                if ((portContainerPtr->iProcessOutgoingMessages) && (oStopOutputPorts == false))
                {
                    /*
                     * Found a port that has outstanding activity and
                     * is not busy.
                     */
                    return true;
                }
            }
        }
        else if (portContainerPtr->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK)
        {
            if (portContainerPtr->irPort.IncomingMsgQueueSize() > 0)
            {
                if (portContainerPtr->iProcessIncomingMessages)
                {
                    /*
                     * Found a port that has outstanding activity and
                     * is not busy.
                     */
                    return true;
                }
            }
            if (portContainerPtr->irPort.OutgoingMsgQueueSize() > 0)
            {
                if (portContainerPtr->iProcessOutgoingMessages)
                {
                    /*
                     * Found a port that has outstanding activity and
                     * is not busy.
                     */
                    return true;
                }
            }
        }
    }
    /*
     * No port processing needed - either all port activity queues are empty
     * or the ports are backed up due to flow control.
     */
    return false;
}

bool PVMFJitterBufferNode::CheckForPortActivityQueues()
{
    uint32 i;
    for (i = 0; i < iPortVector.size(); i++)
    {
        PVMFJitterBufferPortParams* portContainerPtr = NULL;

        if (!getPortContainer(iPortVector[i], portContainerPtr))
        {
            PVMF_JBNODE_LOGERROR((0, "0x%x PVMFJitterBufferNode::CheckForPortActivityQueues: Error - GetPortContainer failed", this));
            return false;
        }

        if ((portContainerPtr->irPort.IncomingMsgQueueSize() > 0) ||
                (portContainerPtr->irPort.OutgoingMsgQueueSize() > 0))
        {
            /*
             * Found a port that still has an outstanding activity.
             */
            return true;
        }
    }

    return false;
}

void PVMFJitterBufferNode::CommandComplete(PVMFNodeCommand& aCmd, PVMFStatus aStatus,
        PVInterface* aExtMsg, OsclAny* aEventData, PVUuid* aEventUUID,
        int32* aEventCode, int32 aEventDataLen)
{
    PVMF_JBNODE_LOGINFO((0, "JitterBufferNode:CommandComplete Id %d Cmd %d Status %d Context %d Data %d"
                         , aCmd.iId, aCmd.iCmd, aStatus, aCmd.iContext, aEventData));

    PVMFNodeInterfaceImpl::CommandComplete(aCmd, aStatus, aExtMsg, aEventData, aEventUUID, aEventCode, aEventDataLen);
    /*
     * Transition to error state in case of select errors only, viz.
     * PVMFFailure, PVMFErrNoMemory, PVMFErrNoResources
     * Any other status implies that the node is probably in a recoverable
     * state
     */
    if ((aStatus == PVMFFailure) ||
            (aStatus == PVMFErrNoMemory) ||
            (aStatus == PVMFErrNoResources))
    {
        SetState(EPVMFNodeError);
    }
}

///////////////////////////////////////////////////////////////////////////////
//Called by the command handler AO to do the Query Interface.
///////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferNode::DoQueryInterface()
{
    //This node supports Query Interface from any state
    PVUuid* uuid;
    PVInterface** ptr;
    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, ptr);

    if (*uuid == PVUuid(PVMF_JITTERBUFFERNODE_EXTENSIONINTERFACE_UUID))
    {
        if (!ipExtensionInterface)
        {
            OsclMemAllocator alloc;
            int32 err;
            OsclAny*ptr = NULL;
            OSCL_TRY(err,
                     ptr = alloc.ALLOCATE(sizeof(PVMFJitterBufferExtensionInterfaceImpl));
                    );
            if (err != OsclErrNone || !ptr)
            {
                PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::DoQueryInterface: Error - Out of memory"));
                return PVMFErrNoMemory;
            }
            ipExtensionInterface =
                OSCL_PLACEMENT_NEW(ptr, PVMFJitterBufferExtensionInterfaceImpl(this));
        }
        if (ipExtensionInterface->queryInterface(*uuid, *ptr))
        {
            return PVMFSuccess;
        }
        else
        {
            return PVMFErrNotSupported;
        }
    }
    else
    {
        // not supported
        *ptr = NULL;
        return PVMFErrNotSupported;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Called by the command handler AO to do the port request
// Decides the type of requested port
// Reserve space in port vector for the new port
// Instantiate the port and push it in the port vector
// Populate portparms for tag (port type), jitterbuffer, port, iId,
//
///////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    // This node supports port request from any state
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::DoRequestPort"));

    // retrieve port tag
    int32 tag;
    OSCL_String* mimetype;
    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, mimetype);

    PVMFJitterBufferNodePortTag jitterbufferPortTag = PVMF_JITTER_BUFFER_PORT_TYPE_INPUT;

    if (tag % 3)
    {
        if (tag % 3 == 1)
        {
            jitterbufferPortTag = PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT;
        }
        else if (tag % 3 == 2)
        {
            jitterbufferPortTag = PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK;
        }
    }
    else
    {
        jitterbufferPortTag = PVMF_JITTER_BUFFER_PORT_TYPE_INPUT;
    }

    // Input ports have tags: 0, 3, 6, ...
    // Output ports have tags: 1, 4, 7, ...
    // Feedback ports have tags: 2, 5, 8, ...

    //set port name for datapath logging.
    OSCL_StackString<20> portname;
    switch (jitterbufferPortTag)
    {
        case PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT:
            portname = "JitterBufOut";
            break;
        case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
            //don't log this port for now...
            //portname="JitterBufFeedback";
            break;
        case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
            //don't log this port for now...
            //portname="JitterBufIn";
            break;
        default:
            // avoid compiler warning
            break;
    }

    // Allocate a new port
    OsclAny *ptr = AllocatePort();
    if (!ptr)
    {
        PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::DoRequestPort: Error - iPortVector Out of memory"));
        return PVMFErrNoMemory;
    }

    OsclExclusivePtr<PVMFJitterBufferPort> portAutoPtr;
    PVMFJitterBufferPort* port = NULL;

    OsclExclusivePtr<PVMFJitterBuffer> jitterBufferAutoPtr;

    // create base port with default settings
    port = OSCL_PLACEMENT_NEW(ptr, PVMFJitterBufferPort(tag, *this, portname.get_str()));
    portAutoPtr.set(port);

    /* Add the port to the port vector. */
    if (!PushPortToVect(port))
    {
        return PVMFErrNoMemory;
    }

    PVMFJitterBufferPortParams* pPortParams = PVMF_BASE_NODE_NEW(PVMFJitterBufferPortParams, (*port));
    pPortParams->iTag = jitterbufferPortTag;
    PVMFJitterBuffer* jbPtr = NULL;
    pPortParams->ipJitterBuffer = NULL;
    pPortParams->iId = tag;
    if (mimetype != NULL)
    {
        pPortParams->iMimeType = mimetype->get_str();
    }

    // create jitter buffer if input port
    if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
    {
        PVMFJitterBufferConstructParams jbConstructParams(ipJitterBufferMisc->GetEstimatedServerClock(), *ipClientPlayBackClock, pPortParams->iMimeType, *ipJitterBufferMisc->GetEventNotifier(), iDelayEstablished, iJitterDelayPercent, iJitterBufferState, this, port);
        jbPtr = ipJitterBufferFactory->Create(jbConstructParams);
        if (jbPtr)
            jbPtr->SetDurationInMilliSeconds(iJitterBufferDurationInMilliSeconds);
        jitterBufferAutoPtr.set(jbPtr);
        pPortParams->ipJitterBuffer = jbPtr;
        if (iBroadCastSession == true)
        {
            jbPtr->SetBroadCastSession();
        }
        jbPtr->setPayloadParser(PVMFJitterBufferMisc::CreatePayloadParser(iPayloadParserRegistry, pPortParams->iMimeType.get_cstr()));
    }

    if (!PushPortParamsToQ(pPortParams))
    {
        PVMF_JBNODE_LOGERROR((0, "0x%x PVMFJitterBufferNode::DoRequestPort: Error - iPortParamsQueue.push_back() failed", this));
        return PVMFErrNoMemory;
    }


    // Update the iPortParams for all existing ports since adding a new Port Parameters element might
    // have caused reallocation of the vector elements.
    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
    for (it = iPortParamsQueue.begin();
            it != iPortParamsQueue.end();
            it++)
    {
        PVMFJitterBufferPortParams* portParametersPtr = *it;
        PVMFJitterBufferPort* portPtr = OSCL_REINTERPRET_CAST(PVMFJitterBufferPort*, &portParametersPtr->irPort);
        portPtr->iPortParams = portParametersPtr;

        // Update also the port counterpart and port counterpart parameters
        PVMFPortInterface* cpPort = getPortCounterpart(portPtr);
        if (cpPort != NULL)
        {
            portPtr->iPortCounterpart = (PVMFJitterBufferPort*)cpPort;
            PVMFJitterBufferPortParams* cpPortContainerPtr = NULL;
            if (getPortContainer(portPtr->iPortCounterpart, cpPortContainerPtr))
            {
                portPtr->iCounterpartPortParams = cpPortContainerPtr;
            }
            else
            {
                PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::DoRequestPort: getPortContainer for port counterpart failed"));
                return PVMFFailure;
            }
        }

    }

    portAutoPtr.release();
    jitterBufferAutoPtr.release();


    /* Return the port pointer to the caller. */
    aPort = port;
    return PVMFSuccess;
}

OsclAny* PVMFJitterBufferNode::AllocatePort()
{
    OsclAny *ptr = NULL;
    int32 err;
    OSCL_TRY(err, ptr = iPortVector.Allocate(););
    if (err != OsclErrNone)
    {
        ptr = NULL;
    }
    return ptr;
}

bool PVMFJitterBufferNode::PushPortToVect(PVMFJitterBufferPort*& aPort)
{
    int32 err;
    OSCL_TRY(err, iPortVector.AddL(aPort););
    if (err != OsclErrNone)
    {
        return false;
    }
    return true;
}

bool PVMFJitterBufferNode::PushPortParamsToQ(PVMFJitterBufferPortParams*& aPortParams)
{
    int32 err;
    OSCL_TRY(err, iPortParamsQueue.push_back(aPortParams););
    if (err != OsclErrNone)
    {
        return false;
    }
    return true;
}

/**
 * Called by the command handler AO to do the port release
 */
PVMFStatus PVMFJitterBufferNode::DoReleasePort()
{
    /*This node supports release port from any state*/

    /* Find the port in the port vector */
    ResetNodeParams();

    PVMFPortInterface* p = NULL;
    iCurrentCommand.PVMFNodeCommandBase::Parse(p);

    PVMFJitterBufferPort* port = (PVMFJitterBufferPort*)p;

    PVMFJitterBufferPort** portPtr = iPortVector.FindByValue(port);
    if (portPtr)
    {
        /* delete corresponding port params */
        Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;

        for (it = iPortParamsQueue.begin();
                it != iPortParamsQueue.end();
                it++)
        {
            PVMFJitterBufferPortParams* pPortParams = *it;
            if (&pPortParams->irPort == iPortVector.front())
            {
                if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
                {
                    IPayloadParser* payloadParser = NULL;
                    pPortParams->ipJitterBuffer->getPayloadParser(payloadParser);
                    PVMFJitterBufferMisc::DestroyPayloadParser(iPayloadParserRegistry, pPortParams->iMimeType.get_cstr(), payloadParser);
                    ipJitterBufferFactory->Destroy(pPortParams->ipJitterBuffer);
                }
                iPortParamsQueue.erase(it);
                break;
            }
        }
        /* delete the port */
        iPortVector.Erase(portPtr);

        return PVMFSuccess;
    }
    else
    {
        /* port not found */
        return PVMFErrArgument;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Called by the command handler AO to initialize the node
// Decides the type of requested port
// Reserve space in port vector for the new port
// Instantiate the port and push it in the port vector
// Populate portparms for tag (port type), jitterbuffer, port, iId,
//
///////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferNode::DoInit()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::DoInit"));

    if (ipJitterBufferMisc)
    {
        ipJitterBufferMisc->Reset();
        OSCL_DELETE(ipJitterBufferMisc);
        ipJitterBufferMisc = NULL;
    }
    ipJitterBufferMisc = PVMFJitterBufferMisc::New(this, *ipClientPlayBackClock, iPortParamsQueue);
    if (ipJitterBufferMisc)
    {
        ipEventNotifier = ipJitterBufferMisc->GetEventNotifier();
        if (iBroadCastSession == true)
            ipJitterBufferMisc->SetBroadcastSession();
    }

    return PVMFSuccess;
}

/**
 * Called by the command handler AO to do the node Prepare
 */
PVMFStatus PVMFJitterBufferNode::DoPrepare()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::DoPrepare"));
    uint32 i;
    for (i = 0; i < iPortVector.size(); i++)
    {
        PVMFJitterBufferPortParams* portContainerPtr1 = NULL;
        if (getPortContainer(iPortVector[i], portContainerPtr1))
        {
            iPortVector[i]->iPortParams = portContainerPtr1;
        }
        else
        {
            PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::DoPrepare: getPortContainer - Self"));
            return PVMFFailure;
        }
        PVMFPortInterface* cpPort = getPortCounterpart(iPortVector[i]);
        if (cpPort != NULL)
        {
            iPortVector[i]->iPortCounterpart = (PVMFJitterBufferPort*)cpPort;
            PVMFJitterBufferPortParams* portContainerPtr2 = NULL;
            if (getPortContainer(iPortVector[i]->iPortCounterpart, portContainerPtr2))
            {
                iPortVector[i]->iCounterpartPortParams = portContainerPtr2;
            }
            else
            {
                PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::DoPrepare: getPortContainer - Counterpart"));
                return PVMFFailure;
            }
        }
    }

    ipJitterBufferMisc->Prepare();
    PVMFStatus status = ipJitterBufferMisc->PrepareMediaReceivingChannel();
    if (PVMFSuccess == status)
    {
        PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::DoPrepare: FW Pkts Disabled"));
        return PVMFCmdCompleted; // Prepare is already completed
    }

    return status;
}

/**
 * Called by the command handler AO to do the node Start
 */
PVMFStatus PVMFJitterBufferNode::DoStart()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::DoStart"));

    ipJitterBufferMisc->StreamingSessionStarted();
    /* Diagnostic logging */
    iDiagnosticsLogged = false;
    iMediaReceiveingChannelPrepared = true;

    if (iInterfaceState == EPVMFNodePaused)
    {
        uint32 currticks = OsclTickCount::TickCount();
        uint32 startTime = OsclTickCount::TicksToMsec(currticks);
        uint32 diff = (startTime - iPauseTime);
        if (diff > PVMF_JITTER_BUFFER_NODE_FIREWALL_PKT_DEFAULT_PAUSE_DURATION_IN_MS)
        {
            if (PVMFPending == ipJitterBufferMisc->PrepareMediaReceivingChannel())
            {
                iMediaReceiveingChannelPrepared = false;
            }
        }
    }

    if (!ipJitterBufferMisc->IsSessionExpired())
    {
        RequestEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
        /* Enable remote activity monitoring */
        Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
        for (it = iPortParamsQueue.begin(); it != iPortParamsQueue.end(); it++)
        {
            PVMFJitterBufferPortParams* pPortParams = *it;
            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                pPortParams->iMonitorForRemoteActivity = true;
            }
        }
    }

    /* If auto paused, implies jitter buffer is not empty */
    if ((iDelayEstablished == false) ||
            (iJitterBufferState == PVMF_JITTER_BUFFER_IN_TRANSITION))
    {

        Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator iter;
        for (iter = iPortParamsQueue.begin(); iter != iPortParamsQueue.end(); iter++)
        {
            PVMFJitterBufferPortParams* pPortParams = *iter;
            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                pPortParams->ipJitterBuffer->NotifyCanRetrievePacket();
            }
        }
        /*
         * Move start to current msg queue where it would stay
         * jitter buffer is full.
         */
        oStartPending = true;
        ReportInfoEvent(PVMFInfoBufferingStart);
        RequestEventCallBack(JB_NOTIFY_REPORT_BUFFERING_STATUS);
        return PVMFPending;
    }
    else
    {
        if (false == iMediaReceiveingChannelPrepared)
        {
            oStartPending = true;
            return PVMFPending;
        }
        else
        {
            /* Just resuming from a paused state with enough data in jitter buffer */
            oStartPending = false;
            /* Enable Output Ports */
            StartOutputPorts();
            return PVMFSuccess;
        }
    }
}

void PVMFJitterBufferNode::CompleteStart()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::CompleteStart"));
    PVMF_JBNODE_LOGDATATRAFFIC((0, "PVMFJitterBufferNode::CompleteStart"));

    if (!iMediaReceiveingChannelPrepared)
        return;

    if (iJitterBufferState == PVMF_JITTER_BUFFER_READY)
    {
        /* transition to Started */
        oStartPending = false;
        /* Enable Output Ports */
        StartOutputPorts();
        CommandComplete(iCurrentCommand, PVMFSuccess);
    }
    else
    {
        SetState(EPVMFNodeError);
        CommandComplete(iCurrentCommand, PVMFErrInvalidState);
    }
}

PVMFStatus PVMFJitterBufferNode::DoStop()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::DoStop"));
    LogSessionDiagnostics();
    PVMFStatus aStatus = PVMFSuccess;


    if (ipJitterBufferMisc)
        ipJitterBufferMisc->StreamingSessionStopped();

    /* Clear queued messages in ports */
    for (uint32 i = 0; i < iPortVector.size(); i++)
    {
        PVMFJitterBufferPortParams* pPortParams = NULL;
        bool bRet = getPortContainer(iPortVector[i], pPortParams);
        if (bRet)
        {
            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                pPortParams->ipJitterBuffer->ResetJitterBuffer();
            }
            pPortParams->ResetParams();
        }
        iPortVector[i]->ClearMsgQueues();
    }

    if (aStatus == PVMFSuccess)
    {
        /* Reset State Variables */
        iDelayEstablished = false;
        if (ipJitterBufferMisc)
            ipJitterBufferMisc->SetSessionDurationExpired();
        oStopOutputPorts = true;
        oStartPending = false;
        iJitterBufferState = PVMF_JITTER_BUFFER_READY;
        iJitterDelayPercent = 0;
    }
    return aStatus;
}

/**
 * Called by the command handler AO to do the node Pause
 */
PVMFStatus PVMFJitterBufferNode::DoPause()
{
    iPauseTime = 0;

    uint32 currticks = OsclTickCount::TickCount();
    iPauseTime = OsclTickCount::TicksToMsec(currticks);
    ipJitterBufferMisc->StreamingSessionPaused();
    StopOutputPorts();
    CancelEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
    CancelEventCallBack(JB_NOTIFY_REPORT_BUFFERING_STATUS);
    PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::DoPause Success"));
    return PVMFSuccess;
}

/**
 * Called by the command handler AO to do the node Reset.
 */
PVMFStatus PVMFJitterBufferNode::DoReset()
{
    PVMF_JBNODE_LOGERROR((0, "JitterBufferNode:DoReset %d", iInterfaceState));
    LogSessionDiagnostics();
    ResetNodeParams();
    return PVMFSuccess;
}

/**
 * Called by the command handler AO to do the Cancel All
 */
PVMFStatus PVMFJitterBufferNode::CancelCurrentCommand()
{
    if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_PREPARE)
    {
        // Cancel Prepare
        ipJitterBufferMisc->CancelMediaReceivingChannelPreparation();
    }
    else if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_START)
    {
        // Cancel Start
        if (ipJitterBufferMisc)
            ipJitterBufferMisc->Reset();

        oStartPending = false;
    }

    CommandComplete(iCurrentCommand, PVMFErrCancelled);

    uint32 i;
    for (i = 0; i < iPortVector.size(); i++)
    {
        PVMFJitterBufferPortParams* pPortParams = NULL;
        bool bRet = getPortContainer(iPortVector[i], pPortParams);
        if (bRet)
        {
            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                pPortParams->ipJitterBuffer->ResetJitterBuffer();
            }
            pPortParams->ResetParams();
        }
        iPortVector[i]->ClearMsgQueues();
    }
    return PVMFSuccess;
}

PVMFStatus PVMFJitterBufferNode::HandleExtensionAPICommands()
{
    return PVMFErrNotSupported;
}

PVMFPortInterface*
PVMFJitterBufferNode::getPortCounterpart(PVMFPortInterface* aPort)
{
    uint32 ii;
    /*
     * Get port params
     */
    for (ii = 0; ii < iPortParamsQueue.size(); ii++)
    {
        if (&iPortParamsQueue[ii]->irPort == aPort)
        {
            break;
        }
    }
    if (ii >= iPortParamsQueue.size())
    {
        return NULL;
    }

    PVMFJitterBufferNodePortTag tag = iPortParamsQueue[ii]->iTag;
    int32 id = iPortParamsQueue[ii]->iId;
    uint32 jj;

    /* Even numbered ports are input ports */
    if (tag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
    {
        for (jj = 0; jj < iPortParamsQueue.size(); jj++)
        {
            if ((id + 1) == iPortParamsQueue[jj]->iId)
            {
                return (&iPortParamsQueue[jj]->irPort);
            }
        }
    }
    /* odd numbered ports are output ports */
    else if (tag == PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT)
    {
        for (jj = 0; jj < iPortParamsQueue.size(); jj++)
        {
            if ((id - 1) == iPortParamsQueue[jj]->iId)
            {
                return (&iPortParamsQueue[jj]->irPort);


            }
        }
    }
    return NULL;
}




///////////////////////////////////////////////////////////////////////////////
//Jitter Buffer Extension Interface Implementation
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//OsclActiveObject
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//PVMFJitterBufferObserver Implementation
///////////////////////////////////////////////////////////////////////////////
void PVMFJitterBufferNode::JitterBufferFreeSpaceAvailable(OsclAny* aContext)
{
    PVMFPortInterface* port = OSCL_STATIC_CAST(PVMFPortInterface*, aContext);
    PVMFJitterBufferPort* jbPort = OSCL_STATIC_CAST(PVMFJitterBufferPort*, port);
    PVMFJitterBufferPortParams* portParams = jbPort->iPortParams;
    if (portParams)
        portParams->iProcessIncomingMessages = true;

    Reschedule();
}

void PVMFJitterBufferNode::ProcessJBInfoEvent(PVMFAsyncEvent& aEvent)
{
    PVMF_JBNODE_LOGCLOCK_REBUFF((0, "PVMFJitterBufferNode::ProcessJBInfoEvent: Event Type [%d]", aEvent.GetEventType()));
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::ProcessJBInfoEvent Event: Type [%d]", aEvent.GetEventType()));
    switch (aEvent.GetEventType())
    {
        case PVMFInfoUnderflow:
        {
            RequestEventCallBack(JB_NOTIFY_REPORT_BUFFERING_STATUS);
            if (oStartPending == false)
            {
                UpdateRebufferingStats(PVMFInfoUnderflow);
                ipJitterBufferMisc->StreamingSessionBufferingStart();
                ReportInfoEvent(PVMFInfoUnderflow);
                ReportInfoEvent(PVMFInfoBufferingStart);
                ReportInfoEvent(PVMFInfoBufferingStatus);
            }
        }
        break;
        case PVMFInfoDataReady:
        {
            UpdateRebufferingStats(PVMFInfoDataReady);
            ReportInfoEvent(PVMFInfoBufferingStatus);
            ReportInfoEvent(PVMFInfoDataReady);
            ReportInfoEvent(PVMFInfoBufferingComplete);
            CancelEventCallBack(JB_NOTIFY_REPORT_BUFFERING_STATUS);

            Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
            for (it = iPortParamsQueue.begin(); it != iPortParamsQueue.end(); it++)
            {
                PVMFJitterBufferPortParams* pPortParams = *it;
                if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
                {
                    pPortParams->iCanReceivePktFromJB = true;
                    pPortParams->ipJitterBuffer->CancelNotifyCanRetrievePacket();
                    PVMFJitterBufferStats stats = pPortParams->ipJitterBuffer->getJitterBufferStats();
                    PVMF_JBNODE_LOGDATATRAFFIC((0, "Mime %s stats.currentOccupancy[%d], stats.maxSeqNumRegistered[%d], stats.lastRetrievedSeqNum[%d] stats.maxTimeStampRetrievedWithoutRTPOffset[%d]", pPortParams->iMimeType.get_cstr(), stats.currentOccupancy, stats.maxSeqNumRegistered, stats.lastRetrievedSeqNum, stats.maxTimeStampRetrievedWithoutRTPOffset));
                }
            }

            if (oStartPending)
            {
                CompleteStart();
            }
            else
            {
                ipJitterBufferMisc->StreamingSessionBufferingEnd();

            }
        }
        break;
        case PVMFInfoOverflow:
        {
            ReportInfoEvent(PVMFInfoOverflow);
        }
        break;
        case PVMFJitterBufferNodeJitterBufferLowWaterMarkReached:
        {
            Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator iter;
            for (iter = iPortParamsQueue.begin(); iter != iPortParamsQueue.end(); iter++)
            {
                PVMFJitterBufferPortParams* pPortParams = *iter;
                if (pPortParams->iMonitorForRemoteActivity == false)
                {
                    pPortParams->iMonitorForRemoteActivity = true;
                    RequestEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
                }

            }
            ReportInfoEvent(PVMFJitterBufferNodeJitterBufferLowWaterMarkReached);
        }
        break;
        case PVMFJitterBufferNodeJitterBufferHighWaterMarkReached:
        {
            Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator iter;
            for (iter = iPortParamsQueue.begin(); iter != iPortParamsQueue.end(); iter++)
            {
                PVMFJitterBufferPortParams* pPortParams = *iter;
                if (pPortParams->iMonitorForRemoteActivity == true)
                {
                    pPortParams->iMonitorForRemoteActivity = false;
                    CancelEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
                }
            }
            ReportInfoEvent(PVMFJitterBufferNodeJitterBufferHighWaterMarkReached);
        }
        break;
        case PVMFJitterBufferNodeStreamThinningRecommended:
        {
            PVMFNodeInterface::ReportInfoEvent(aEvent);
        }
        break;
        case PVMFJitterBufferNodePayloadParserError:
        {
            PVMFNodeInterface::ReportInfoEvent(aEvent);
        }
        break;
        default:
        {
            //noop
        }
    }

}

void PVMFJitterBufferNode::PacketReadyToBeRetrieved(OsclAny* aContext)
{
    if (aContext)
    {
        PVMFJitterBufferPort* port = OSCL_REINTERPRET_CAST(PVMFJitterBufferPort*, aContext);
        PVMFJitterBufferPortParams* portparams = port->GetPortParams();
        if (portparams)
        {
            PVMF_JBNODE_LOGDATATRAFFIC_OUT((0, "PVMFJitterBufferNode::PacketReadyToBeRetrieved for mime type %s", portparams->iMimeType.get_cstr()));
            portparams->iCanReceivePktFromJB = true;
        }
    }
}

void PVMFJitterBufferNode::EndOfStreamSignalled(OsclAny* aContext)
{
    if (aContext)
    {
        PVLogger* ipDataPathLoggerRTCP = PVLogger::GetLoggerObject("datapath.sourcenode.jitterbuffer.rtcp");
        PVMF_JBNODE_LOG_RTCP_DATAPATH((0, "PVMFJitterBufferNode::EndOfStreamSignalled"));
        PVMFJitterBufferPort* port = OSCL_REINTERPRET_CAST(PVMFJitterBufferPort*, aContext);
        PVMFJitterBufferPortParams* portparams = port->GetPortParams();
        if (portparams)
        {
            Reschedule();
        }
    }
}

void PVMFJitterBufferNode::UpdateRebufferingStats(PVMFEventType aEventType)
{
    if (aEventType == PVMFInfoUnderflow)
    {
        Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
        for (it = iPortParamsQueue.begin();
                it != iPortParamsQueue.end();
                it++)
        {
            PVMFJitterBufferPortParams* pJitterBufferPortParams = *it;
            if (pJitterBufferPortParams->iMonitorForRemoteActivity == false)
            {
                pJitterBufferPortParams->iMonitorForRemoteActivity = true;
                CancelEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
                RequestEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
            }
        }

        PVMF_JBNODE_LOGDATATRAFFIC_FLOWCTRL_E((0, "PVMFJitterBufferNode::UpdateRebufferingStats: Sending Auto Resume"));
    }

}
///////////////////////////////////////////////////////////////////////////////
//PVMFJitterBufferMiscObserver
///////////////////////////////////////////////////////////////////////////////
void PVMFJitterBufferNode::MessageReadyToSend(PVMFPortInterface*& aPort, PVMFSharedMediaMsgPtr& aMessage)
{
    PVMF_JBNODE_LOGINFO((0, "0x%x PVMFJitterBufferNode::MessageReadyToSend: aPort=0x%x", this, aPort));

    PVMFJitterBufferPort* jitterbufferPort = OSCL_STATIC_CAST(PVMFJitterBufferPort*, aPort);

    //Kind of messages received here
    //RTCP reports
    //Firewall packets
    //We do not expect port to go in busy state at firewall port and if somehow port used for sending rtcp messages
    //is in busy state, or gets into busy state, we just ignore it and discard the message
    PVMFStatus status = PVMFSuccess;
    aPort->QueueOutgoingMsg(aMessage);
    status = aPort->Send();
    if (status == PVMFSuccess)
    {
        jitterbufferPort->iPortParams->iNumMediaMsgsSent++;
    }
    return;
}

void PVMFJitterBufferNode::MediaReceivingChannelPrepared(bool aStatus)
{
    //ignore the status param.
    OSCL_UNUSED_ARG(aStatus);
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::MediaRecvChannelPrerared In"));
    iMediaReceiveingChannelPrepared = true;
    if (PVMF_GENERIC_NODE_PREPARE == iCurrentCommand.iCmd)
    {
        // Complete Prepare
        CommandComplete(iCurrentCommand, PVMFSuccess);
    }
    if (PVMF_GENERIC_NODE_START == iCurrentCommand.iCmd)
    {
        CompleteStart();
    }
}

void PVMFJitterBufferNode::ProcessRTCPControllerEvent(PVMFAsyncEvent& aEvent)
{
    PVMFNodeInterface::ReportInfoEvent(aEvent);
}

void PVMFJitterBufferNode::SessionSessionExpired()
{

    CancelEventCallBack(JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED);
}

///////////////////////////////////////////////////////////////////////////////
//PVMFJBEventNotifierObserver implementation
///////////////////////////////////////////////////////////////////////////////
void PVMFJitterBufferNode::ProcessCallback(CLOCK_NOTIFICATION_INTF_TYPE aClockNotificationInterfaceType, uint32 aCallBkId, const OsclAny* aContext, PVMFStatus aStatus)
{
    OSCL_UNUSED_ARG(aClockNotificationInterfaceType);
    OSCL_UNUSED_ARG(aContext);
    PVMF_JBNODE_LOG_EVENTS_CLOCK((0, "PVMFJitterBufferNode::ProcessCallBack In CallBackId [%d] ", aCallBkId));

    if (PVMFSuccess == aStatus)
    {
        if (aCallBkId == iIncomingMediaInactivityDurationCallBkId)
        {
            iIncomingMediaInactivityDurationCallBkPending = false;
            HandleEvent_IncomingMediaInactivityDurationExpired();
        }
        else if (aCallBkId == iNotifyBufferingStatusCallBkId)
        {
            iNotifyBufferingStatusCallBkPending = false;
            HandleEvent_NotifyReportBufferingStatus();
        }
    }
    else
    {

        //Log it
    }
    PVMF_JBNODE_LOG_EVENTS_CLOCK((0, "PVMFJitterBufferNode::ProcessCallBack Out"));
}

void PVMFJitterBufferNode::HandleEvent_IncomingMediaInactivityDurationExpired()
{
    PVMF_JBNODE_LOG_EVENTS_CLOCK((0, "PVMFJitterBufferNode::HandleEvent_IncomingMediaInactivityDurationExpired In"));

    PVUuid eventuuid = PVMFJitterBufferNodeEventTypeUUID;
    int32 errcode = PVMFJitterBufferNodeRemoteInactivityTimerExpired;

    PVMF_JB_LOGCLOCK_SESSION_DURATION((0, "PVMFJitterBufferNode::HandleEvent_IncomingMediaInactivityDurationExpired- iCurrentCommand.iCmd[%d]", iCurrentCommand.iCmd));
    if (IsCommandInProgress(iCurrentCommand))
    {
        CommandComplete(iCurrentCommand, PVMFFailure, NULL, NULL, &eventuuid, &errcode);
    }
    else
    {
        ReportInfoEvent(PVMFErrTimeout, NULL, &eventuuid, &errcode);
        ipJitterBufferMisc->SetSessionDurationExpired();

        Reschedule();
    }

    PVMF_JBNODE_LOG_EVENTS_CLOCK((0, "PVMFJitterBufferNode::HandleEvent_IncomingMediaInactivityDurationExpired Out"));
}

void PVMFJitterBufferNode::HandleEvent_NotifyReportBufferingStatus()
{
    PVMF_JBNODE_LOG_EVENTS_CLOCK((0, "PVMFJitterBufferNode::HandleEvent_NotifyReportBufferingStatus In"));
    if (iDelayEstablished == false)
    {
        /*
         * Check to see if the session duration has expired
         */
        if (ipJitterBufferMisc->IsSessionExpired())
        {
            PVMF_JBNODE_LOGCLOCK((0, "PVMFJitterBufferNode::TimeoutOccurred - Session Duration Expired"));
            /* Force out of rebuffering */
            Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
            for (it = iPortParamsQueue.begin();
                    it != iPortParamsQueue.end();
                    it++)
            {
                PVMFJitterBufferPortParams* pPortParams = *it;
                if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
                {
                    SendData(&(pPortParams->irPort));
                }
            }
            Reschedule();
        }
        else
        {
            ReportInfoEvent(PVMFInfoBufferingStatus);
            RequestEventCallBack(JB_NOTIFY_REPORT_BUFFERING_STATUS);
        }
    }
    PVMF_JBNODE_LOG_EVENTS_CLOCK((0, "PVMFJitterBufferNode::HandleEvent_NotifyReportBufferingStatus Out"));
}

///////////////////////////////////////////////////////////////////////////////
//Utility functions
///////////////////////////////////////////////////////////////////////////////

bool
PVMFJitterBufferNode::getPortContainer(PVMFPortInterface* aPort,
                                       PVMFJitterBufferPortParams*& aPortParamsPtr)
{
    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;

    for (it = iPortParamsQueue.begin();
            it != iPortParamsQueue.end();
            it++)
    {
        PVMFJitterBufferPortParams* pPortParams = *it;
        if (&pPortParams->irPort == aPort)
        {
            aPortParamsPtr = *it;
            return true;
        }
    }
    return false;
}

bool PVMFJitterBufferNode::RequestEventCallBack(JB_NOTIFY_CALLBACK aEventType, uint32 aDelay, OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aDelay);
    OSCL_UNUSED_ARG(aContext);
    bool retval = false;
    switch (aEventType)
    {
        case JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED:
        {
            PVMFJBEventNotificationRequestInfo eventRequestInfo(CLOCK_NOTIFICATION_INTF_TYPE_NONDECREASING, this, NULL);
            retval = ipEventNotifier->RequestCallBack(eventRequestInfo, iMaxInactivityDurationForMediaInMs, iIncomingMediaInactivityDurationCallBkId);
            if (retval)
            {
                iIncomingMediaInactivityDurationCallBkPending = true;
            }
        }
        break;
        case JB_NOTIFY_REPORT_BUFFERING_STATUS:
        {
            if (iNotifyBufferingStatusCallBkPending)
            {
                CancelEventCallBack(JB_NOTIFY_REPORT_BUFFERING_STATUS);
            }
            PVMFJBEventNotificationRequestInfo eventRequestInfo(CLOCK_NOTIFICATION_INTF_TYPE_NONDECREASING, this, NULL);
            retval = ipEventNotifier->RequestCallBack(eventRequestInfo, iBufferingStatusIntervalInMs, iNotifyBufferingStatusCallBkId);
            if (retval)
            {
                iNotifyBufferingStatusCallBkPending = true;
            }
        }
        break;

        default:
        {
            //Log it
        }
    }
    return retval;
}

void PVMFJitterBufferNode::CancelEventCallBack(JB_NOTIFY_CALLBACK aEventType, OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    switch (aEventType)
    {
        case JB_INCOMING_MEDIA_INACTIVITY_DURATION_EXPIRED:
        {
            if (iIncomingMediaInactivityDurationCallBkPending)
            {
                PVMFJBEventNotificationRequestInfo eventRequestInfo(CLOCK_NOTIFICATION_INTF_TYPE_NONDECREASING, this, NULL);
                ipEventNotifier->CancelCallBack(eventRequestInfo, iIncomingMediaInactivityDurationCallBkId);
                iIncomingMediaInactivityDurationCallBkPending = false;
            }
        }
        break;
        case JB_NOTIFY_REPORT_BUFFERING_STATUS:
        {
            if (iNotifyBufferingStatusCallBkPending)
            {
                PVMFJBEventNotificationRequestInfo eventRequestInfo(CLOCK_NOTIFICATION_INTF_TYPE_NONDECREASING, this, NULL);
                ipEventNotifier->CancelCallBack(eventRequestInfo, iNotifyBufferingStatusCallBkId);
                iNotifyBufferingStatusCallBkPending = false;
            }
        }
        break;

        default:
        {
            //Log it
        }
    }
    return;
}

void PVMFJitterBufferNode::ReportErrorEvent(PVMFEventType aEventType,
        OsclAny* aEventData,
        PVUuid* aEventUUID,
        int32* aEventCode)
{
    PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode:NodeErrorEvent Type %d Data %d"
                          , aEventType, aEventData));

    if (aEventUUID && aEventCode)
    {
        PVMFBasicErrorInfoMessage* eventmsg;
        eventmsg = PVMF_BASE_NODE_NEW(PVMFBasicErrorInfoMessage, (*aEventCode, *aEventUUID, NULL));
        PVMFAsyncEvent asyncevent(PVMFErrorEvent,
                                  aEventType,
                                  NULL,
                                  OSCL_STATIC_CAST(PVInterface*, eventmsg),
                                  aEventData,
                                  NULL,
                                  0);
        PVMFNodeInterfaceImpl::ReportErrorEvent(asyncevent);
        eventmsg->removeRef();
    }
    else
    {
        PVMFNodeInterfaceImpl::ReportErrorEvent(aEventType, aEventData);
    }
}

void PVMFJitterBufferNode::ReportInfoEvent(PVMFEventType aEventType,
        OsclAny* aEventData,
        PVUuid* aEventUUID,
        int32* aEventCode)
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode:NodeInfoEvent Type %d Data %d"
                         , aEventType, aEventData));

    if (aEventType == PVMFInfoBufferingStatus)
    {
        PVMFAsyncEvent asyncevent(PVMFInfoEvent,
                                  aEventType,
                                  NULL,
                                  NULL,
                                  aEventData,
                                  &iJitterDelayPercent,
                                  sizeof(iJitterDelayPercent));
        PVMFNodeInterfaceImpl::ReportInfoEvent(asyncevent);
    }
    else if (aEventUUID && aEventCode)
    {
        PVMFBasicErrorInfoMessage* eventmsg;
        eventmsg = PVMF_BASE_NODE_NEW(PVMFBasicErrorInfoMessage, (*aEventCode, *aEventUUID, NULL));
        PVMFErrorInfoMessageInterface* interimPtr =
            OSCL_STATIC_CAST(PVMFErrorInfoMessageInterface*, eventmsg);
        PVMFAsyncEvent asyncevent(PVMFInfoEvent,
                                  aEventType,
                                  NULL,
                                  OSCL_STATIC_CAST(PVInterface*, interimPtr),
                                  aEventData,
                                  NULL,
                                  0);
        PVMFNodeInterfaceImpl::ReportInfoEvent(asyncevent);
        eventmsg->removeRef();
    }
    else
    {
        PVMFNodeInterfaceImpl::ReportInfoEvent(aEventType, aEventData);
    }
}

PVMFJitterBuffer*
PVMFJitterBufferNode::findJitterBuffer(PVMFPortInterface* aPort)
{
    uint32 ii;
    for (ii = 0; ii < iPortParamsQueue.size(); ii++)
    {

        if (&iPortParamsQueue[ii]->irPort == aPort)
        {
            return (iPortParamsQueue[ii]->ipJitterBuffer);

        }
    }
    return NULL;
}


void PVMFJitterBufferNode::LogSessionDiagnostics()
{
    if (iDiagnosticsLogged == false)
    {
        ipDiagnosticsLogger = PVLogger::GetLoggerObject("pvplayerdiagnostics.streamingmanager");

        LogPortDiagnostics();

        Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
        for (it = iPortParamsQueue.begin(); it != iPortParamsQueue.end(); it++)
        {
            PVMFJitterBufferPortParams* pPortParams = *it;
            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                PVMFJitterBuffer* jitterBuffer = findJitterBuffer(&pPortParams->irPort);
                if (jitterBuffer != NULL)
                {
                    PVMFJitterBufferStats jbStats = jitterBuffer->getJitterBufferStats();
                    uint32 in_wrap_count = 0;
                    uint32 max_ts_reg = jbStats.maxTimeStampRegistered;
                    pPortParams->iMediaClockConverter.set_clock(max_ts_reg, in_wrap_count);

                    in_wrap_count = 0;
                    uint32 max_ts_ret = jbStats.maxTimeStampRetrieved;
                    pPortParams->iMediaClockConverter.set_clock(max_ts_ret, in_wrap_count);

                    uint32 currentTime32 = 0;
                    uint32 currentTimeBase32 = 0;
                    bool overflowFlag = false;
                    ipJitterBufferMisc->GetEstimatedServerClock().GetCurrentTime32(currentTime32,
                            overflowFlag,
                            PVMF_MEDIA_CLOCK_MSEC,
                            currentTimeBase32);
                    uint32 bitrate32 = 0;
                    uint32 totalNumBytesRecvd = jbStats.totalNumBytesRecvd;
                    if (currentTime32 != 0)
                    {
                        bitrate32 = (totalNumBytesRecvd / currentTime32);
                    }

                    bitrate32 *= 8;

                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "JitterBuffer - TrackMime Type = %s", pPortParams->iMimeType.get_cstr()));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "Total Num Packets Recvd = %d", jbStats.totalNumPacketsReceived));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "Total Num Packets Registered Into JitterBuffer = %d", jbStats.totalNumPacketsRegistered));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "Total Num Packets Retrieved From JitterBuffer = %d", jbStats.totalNumPacketsRetrieved));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "MaxSeqNum Recvd = %d", jbStats.maxSeqNumReceived));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "MaxSeqNum Registered = %d", jbStats.maxSeqNumRegistered));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "MaxSeqNum Retrieved = %d", jbStats.lastRetrievedSeqNum));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "MaxTimeStamp Registered In MS = %d", pPortParams->iMediaClockConverter.get_converted_ts(1000)));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "MaxTimeStamp Retrieved In MS = %d", pPortParams->iMediaClockConverter.get_converted_ts(1000)));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "Total Number of Packets Lost = %d", jbStats.totalPacketsLost));
                    PVMF_JBNODE_LOGDIAGNOSTICS((0, "Estimated Bitrate = %d", bitrate32));
                }
            }
        }
        iDiagnosticsLogged = true;
    }
}

void PVMFJitterBufferNode::LogPortDiagnostics()
{
    PVLogger* ipDiagnosticsLogger = PVLogger::GetLoggerObject("pvplayerdiagnostics.streamingmanager");

    PVMF_JBNODE_LOGDIAGNOSTICS((0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
    PVMF_JBNODE_LOGDIAGNOSTICS((0, "PVMFJitterBufferNode - iNumRunL = %d", iNumRunL));

    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::iterator it;
    for (it = iPortParamsQueue.begin(); it != iPortParamsQueue.end(); it++)
    {
        PVMFJitterBufferPortParams* pPortParams = *it;
        PvmfPortBaseImpl* ptr =
            OSCL_STATIC_CAST(PvmfPortBaseImpl*, &pPortParams->irPort);
        PvmfPortBaseImplStats stats;
        ptr->GetStats(stats);

        if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
        {
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "PVMF_JITTER_BUFFER_PORT_TYPE_INPUT"));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingMsgRecv = %d", stats.iIncomingMsgRecv));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingMsgConsumed = %d", stats.iIncomingMsgConsumed));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingQueueBusy = %d", stats.iIncomingQueueBusy));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgQueued = %d", stats.iOutgoingMsgQueued));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgSent = %d", stats.iOutgoingMsgSent));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingQueueBusy = %d", stats.iOutgoingQueueBusy));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgDiscarded = %d", stats.iOutgoingMsgDiscarded));
        }
        else if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK)
        {
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK"));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingMsgRecv = %d", stats.iIncomingMsgRecv));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingMsgConsumed = %d", stats.iIncomingMsgConsumed));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingQueueBusy = %d", stats.iIncomingQueueBusy));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgQueued = %d", stats.iOutgoingMsgQueued));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgSent = %d", stats.iOutgoingMsgSent));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingQueueBusy = %d", stats.iOutgoingQueueBusy));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgDiscarded = %d", stats.iOutgoingMsgDiscarded));
        }
        else if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT)
        {
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT"));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingMsgRecv = %d", stats.iIncomingMsgRecv));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingMsgConsumed = %d", stats.iIncomingMsgConsumed));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iIncomingQueueBusy = %d", stats.iIncomingQueueBusy));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgQueued = %d", stats.iOutgoingMsgQueued));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgSent = %d", stats.iOutgoingMsgSent));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingQueueBusy = %d", stats.iOutgoingQueueBusy));
            PVMF_JBNODE_LOGDIAGNOSTICS((0, "iOutgoingMsgDiscarded = %d", stats.iOutgoingMsgDiscarded));
        }
    }
}

bool PVMFJitterBufferNode::PrepareForPlaylistSwitch()
{
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
    uint32 clientClock32 = 0;
    bool overflowFlag = false;
    ipClientPlayBackClock->GetCurrentTime32(clientClock32, overflowFlag, PVMF_MEDIA_CLOCK_MSEC);
    uint32 serverClock32 = ipJitterBufferMisc->GetEstimatedServerClockValue();
    PVMF_JBNODE_LOGCLOCK_REBUFF((0, "PVMFJitterBufferNode::PrepareForPlaylistSwitch - Before - EstServClock=%d",
                                 serverClock32));
    PVMF_JBNODE_LOGCLOCK_REBUFF((0, "PVMFJitterBufferNode::PrepareForPlaylistSwitch - Before - ClientClock=%d",
                                 clientClock32));
#endif
    iJitterBufferState = PVMF_JITTER_BUFFER_IN_TRANSITION;
    ipClientPlayBackClock->Pause();

    return true;
}

void PVMFJitterBufferNode::ClockStateUpdated()
{
    PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ClockStateUpdated - iClientPlayBackClock[%d]", ipClientPlayBackClock->GetState()));
    if (!iDelayEstablished)
    {
        // Don't let anyone start the clock while
        // we're rebuffering
        if (ipClientPlayBackClock != NULL)
        {
            if (ipClientPlayBackClock->GetState() == PVMFMediaClock::RUNNING)
            {
                PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferNode::ClockStateUpdated - Clock was started during rebuffering.  Pausing..."));
                ipClientPlayBackClock->Pause();
            }
        }
    }
}

void PVMFJitterBufferNode::NotificationsInterfaceDestroyed()
{
    //noop
}

void PVMFJitterBufferNode::MediaTrackSSRCEstablished(PVMFJitterBuffer* aJitterBuffer, uint32 aSSRC)
{
    for (uint32 ii = 0; ii < iPortVector.size(); ii++)
    {
        PVMFJitterBufferPortParams* pPortParams = NULL;
        bool bRet = getPortContainer(iPortVector[ii], pPortParams);
        if (bRet)
        {
            if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT && pPortParams->ipJitterBuffer == aJitterBuffer)
            {
                ipJitterBufferMisc->SetPortSSRC(&pPortParams->irPort, aSSRC);
                break;
            }
        }
    }
}

void PVMFJitterBufferNode::SendEOSMessage(Oscl_Vector<int32, OsclMemAllocator> aRemovedTrackIDVector)
{
    // This function is used to send EOS messages for tracks that were removed during a 3GPP FCS.
    for (uint32 ii = 0; ii < aRemovedTrackIDVector.size(); ii++)
    {
        // Calculate input tag offset. Since input ports are numbered 0, 3, 6, ..., do the following
        int32 aInputTagOffset = 3 * aRemovedTrackIDVector[ii];

        // Just to be sure, check the port's tag type
        if (iPortParamsQueue[aInputTagOffset]->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
        {
            iPortParamsQueue[aInputTagOffset]->ipJitterBuffer->ResetJitterBuffer();
            iPortParamsQueue[aInputTagOffset]->ipJitterBuffer->SetEOS(true);
        }
    }
}

void PVMFJitterBufferNode::ResetJitterBuffer()
{
    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferNode::FlushJitterBuffer In"));
    for (uint32 i = 0; i < iPortParamsQueue.size(); i++)
    {
        PVMFJitterBufferPortParams* pPortParams = iPortParamsQueue[i];
        if (pPortParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
        {
            if (pPortParams->ipJitterBuffer != NULL)
            {
                pPortParams->ipJitterBuffer->ResetJitterBuffer();
            }
        }
    }
}

void PVMFJitterBufferNode::SetClientClockToServerClock()
{
    // Bring client clock to server clock's value
    uint32 currentTime32 = 0;
    uint32 currentTimeBase32 = 0;
    bool overflowFlag = false;
    ipJitterBufferMisc->GetEstimatedServerClock().GetCurrentTime32(currentTime32,
            overflowFlag,
            PVMF_MEDIA_CLOCK_MSEC,
            currentTimeBase32);

    overflowFlag = false;
    ipClientPlayBackClock->Stop();
    ipClientPlayBackClock->SetStartTime32(currentTime32,
                                          PVMF_MEDIA_CLOCK_MSEC, overflowFlag);
}
