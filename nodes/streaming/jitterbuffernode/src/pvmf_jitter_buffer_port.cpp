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
/**
 * @file pvmf_jitter_buffer_port.cpp
 * @brief PVMF Jitter Buffer Port implementation
 */

#ifndef PVMF_JITTER_BUFFER_PORT_H_INCLUDED
#include "pvmf_jitter_buffer_port.h"
#endif
#ifndef OSCL_MEM_BASIC_FUNCTIONS_H
#include "oscl_mem_basic_functions.h"
#endif
#ifndef OSCL_MIME_STRING_UTILS_H
#include "pv_mime_string_utils.h"
#endif
#ifndef PVMF_NODE_INTERFACE_H_INCLUDED
#include "pvmf_node_interface.h"
#endif
#ifndef PVMF_JITTER_BUFFER_H_INCLUDED
#include "pvmf_jitter_buffer.h"
#endif
#ifndef PVMF_RTCP_TIMER_H_INCLUDED
#include "pvmf_rtcp_timer.h"
#endif
#ifndef PVMF_STREAMING_MEM_CONFIG_H_INCLUDED
#include "pvmf_streaming_mem_config.h"
#endif
#ifndef PVMF_JITTER_BUFFER_NODE_H_INCLUDED
#include "pvmf_jitter_buffer_node.h"
#endif

#define PVMF_JB_PORT_OVERRIDE 1

////////////////////////////////////////////////////////////////////////////
PVMFJitterBufferPort::PVMFJitterBufferPort(int32 aTag, PVMFJitterBufferNode& aNode, const char*name)
        : PvmfPortBaseImpl(aTag, &aNode, name)
        , iFormat(PVMF_MIME_FORMAT_UNKNOWN)
        , irJitterBufferNode(aNode)
{
    Construct();
}

////////////////////////////////////////////////////////////////////////////
PVMFJitterBufferPort::PVMFJitterBufferPort(int32 aTag, PVMFJitterBufferNode& aNode
        , uint32 aInCapacity
        , uint32 aInReserve
        , uint32 aInThreshold
        , uint32 aOutCapacity
        , uint32 aOutReserve
        , uint32 aOutThreshold, const char*name)
        : PvmfPortBaseImpl(aTag, &aNode, aInCapacity, aInReserve, aInThreshold, aOutCapacity, aOutReserve, aOutThreshold, name)
        , iFormat(PVMF_MIME_FORMAT_UNKNOWN)
        , irJitterBufferNode(aNode)
{
    Construct();
}

////////////////////////////////////////////////////////////////////////////
void PVMFJitterBufferPort::Construct()
{
    iPortParams = NULL;
    iCounterpartPortParams = NULL;
    iPortCounterpart = NULL;
    ipLogger = PVLogger::GetLoggerObject("PVMFJitterBufferPort");
    oscl_memset(&iStats, 0, sizeof(PvmfPortBaseImplStats));
    /*
     * Input ports have tags: 0, 3, 6, ...
     * Output ports have tags: 1, 4, 7, ...
     * Feedback ports have tags: 2, 5, 8, ...
     */
    if (iTag % 3)
    {
        if (iTag % 3 == 1)
        {
            iPortType = PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT;
        }
        else if (iTag % 3 == 2)
        {
            iPortType = PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK;
        }
    }
    else
    {
        iPortType = PVMF_JITTER_BUFFER_PORT_TYPE_INPUT;
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFJitterBufferPort::~PVMFJitterBufferPort()
{
    Disconnect();
    ClearMsgQueues();
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferPort::Connect(PVMFPortInterface* aPort)
{
    PVMF_JBNODE_LOGINFO((0, "0x%x PVMFJitterBufferPort::Connect: aPort=0x%x", this, aPort));

    if (!aPort)
    {
        PVMF_JBNODE_LOGERROR((0, "0x%x PVMFJitterBufferPort::Connect: Error - Connecting to invalid port", this));
        return PVMFErrArgument;
    }

    if (iConnectedPort)
    {
        PVMF_JBNODE_LOGERROR((0, "0x%x PVMFJitterBufferPort::Connect: Error - Already connected", this));
        return PVMFFailure;
    }

    if ((iPortType == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT) ||
            (iPortType == PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK))
    {
        OsclAny* temp = NULL;
        aPort->QueryInterface(PVMI_CAPABILITY_AND_CONFIG_PVUUID, temp);
        PvmiCapabilityAndConfig *config = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, temp);

        if (config != NULL)
        {
            int numKvp = 0;
            PvmiKvp* kvpPtr = NULL;
            PVMFStatus status =
                config->getParametersSync(NULL, (char*)PVMI_PORT_CONFIG_INPLACE_DATA_PROCESSING_KEY, kvpPtr, numKvp, NULL);
            if (status == PVMFSuccess)
            {
                bool inPlaceDataProcessing = kvpPtr[0].value.bool_value;
                irJitterBufferNode.SetInPlaceProcessingMode(OSCL_STATIC_CAST(PVMFPortInterface*, this),
                        inPlaceDataProcessing);
            }
            config->releaseParameters(NULL, kvpPtr, numKvp);
        }
    }


    //If the ports of this node will act as the src node's port
    if ((irJitterBufferNode.iPayloadParserRegistry) && (iPortType == PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT))
    {
        OsclAny* temp = NULL;
        aPort->QueryInterface(PVMI_CAPABILITY_AND_CONFIG_PVUUID, temp);
        PvmiCapabilityAndConfig *config = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, temp);
        if ((config != NULL))
        {
            if (!(pvmiSetPortFormatSpecificInfoSync(config, PVMF_FORMAT_SPECIFIC_INFO_KEY)))
            {
                PVMF_JBNODE_LOGERROR((0, "PVMFMediaLayerPort::Connect: Error - Unable To Send Format Specific Info To Peer"));
                return PVMFFailure;
            }
            if (!(pvmiSetPortFormatSpecificInfoSync(config, PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY)))
            {
                PVMF_JBNODE_LOGERROR((0, "PVMFMediaLayerPort::Connect: Error - Unable To Send Max Num Media Msg Key To Peer"));
                return PVMFFailure;
            }
        }
    }

    //Automatically connect the peer.
    if (aPort->PeerConnect(this) != PVMFSuccess)
    {
        PVMF_JBNODE_LOGERROR((0, "0x%x PVMFJitterBufferPort::Connect: Error - Peer Connect failed", this));
        return PVMFFailure;
    }

    iConnectedPort = aPort;

    PortActivity(PVMF_PORT_ACTIVITY_CONNECT);
    return PVMFSuccess;
}


////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferPort::getParametersSync(PvmiMIOSession aSession,
        PvmiKeyType aIdentifier,
        PvmiKvp*& aParameters,
        int& num_parameter_elements,
        PvmiCapabilityContext aContext)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);

    if (irJitterBufferNode.iPayloadParserRegistry)
    {
        num_parameter_elements = 0;
        if (pv_mime_strcmp(aIdentifier, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
        {
            if (!pvmiGetPortFormatSpecificInfoSync(PVMF_FORMAT_SPECIFIC_INFO_KEY, aParameters))
            {
                return PVMFFailure;
            }
        }
        else if (pv_mime_strcmp(aIdentifier, PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY) == 0)
        {
            if (!pvmiGetPortFormatSpecificInfoSync(PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY, aParameters))
            {
                return PVMFFailure;
            }
        }
        num_parameter_elements = 1;
    }
    return PVMFErrNotSupported;
}


////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferPort::releaseParameters(PvmiMIOSession aSession,
        PvmiKvp* aParameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(aSession);

    PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferPort::releaseParameters: aSession=0x%x, aParameters=0x%x, num_elements=%d",
                         aSession, aParameters, num_elements));

    if ((num_elements != 1) ||
            (pv_mime_strcmp(aParameters->key, PVMF_JITTER_BUFFER_PORT_SPECIFIC_ALLOCATOR_VALTYPE) != 0))
    {
        PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferPort::releaseParameters: Error - Not a PvmiKvp created by this port"));
        return PVMFFailure;
    }

    OsclMemAllocator alloc;

    if (pv_mime_strcmp(aParameters->key, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
    {
        OsclMemAllocator alloc;
        alloc.deallocate((OsclAny*)(aParameters->key));
    }

    alloc.deallocate((OsclAny*)(aParameters));//need to check deallocs
    return PVMFSuccess;
}


////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFJitterBufferPort::QueueOutgoingMsg(PVMFSharedMediaMsgPtr aMsg)
{
#if PVMF_JB_PORT_OVERRIDE
    if (iPortType == PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT)
    {
        PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferPort::QueueOutgoingMsg"));
        //If port is not connected, don't accept data on the
        //outgoing queue.
        if (!iConnectedPort)
        {
            PVMF_JBNODE_LOGERROR((0, "PVMFJitterBufferPort::QueueOutgoingMsg: Error - Port not connected"));
            return PVMFFailure;
        }

        PvmfPortBaseImpl* cpPort = OSCL_STATIC_CAST(PvmfPortBaseImpl*, iConnectedPort);

        // Connected Port incoming Queue is in busy / flushing state.  Do not accept more outgoing messages
        // until the queue is not busy, i.e. queue size drops below specified threshold or FlushComplete
        // is called.
        if (cpPort->iIncomingQueue.iBusy)
        {
            PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferPort::QueueOutgoingMsg: Connected Port Incoming queue in busy / flushing state"));
            return PVMFErrBusy;
        }

        // In case there are data pending in iOutgoingQueue, we should try sending them first.??????

        // Add message to outgoing queue and notify the node of the activity
        // There is no need to trap the push_back, since it cannot leave in this usage
        // Reason being that we do a reserve in the constructor and we do not let the
        // port queues grow indefinitely (we either a connected port busy or outgoing Q busy
        // before we reach the reserved limit
        PVMFStatus status = cpPort->Receive(aMsg);

        if (status != PVMFSuccess)
        {
            OSCL_ASSERT(false);
        }

        // Outgoing queue size is at capacity and goes into busy state. The owner node is
        // notified of this transition into busy state.
        if (cpPort->isIncomingFull())
        {
            PVMF_JBNODE_LOGINFO((0, "PVMFJitterBufferPort::QueueOutgoingMsg: Connected Port incoming queue is full. Goes into busy state"));
            cpPort->iIncomingQueue.iBusy = true;
            PvmfPortBaseImpl::PortActivity(PVMF_PORT_ACTIVITY_CONNECTED_PORT_BUSY);
        }
        return PVMFSuccess;
    }
    else
    {
        return (PvmfPortBaseImpl::QueueOutgoingMsg(aMsg));
    }
#else
    return (PvmfPortBaseImpl::QueueOutgoingMsg(aMsg));
#endif
}

bool PVMFJitterBufferPort::IsOutgoingQueueBusy()
{
#if PVMF_JB_PORT_OVERRIDE
    if (iPortType == PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT)
    {
        if (iConnectedPort != NULL)
        {
            PvmfPortBaseImpl* cpPort = OSCL_STATIC_CAST(PvmfPortBaseImpl*, iConnectedPort);
            return (cpPort->iIncomingQueue.iBusy);
        }
    }
    return (PvmfPortBaseImpl::IsOutgoingQueueBusy());;
#else
    return (PvmfPortBaseImpl::IsOutgoingQueueBusy());
#endif
}


bool PVMFJitterBufferPort::pvmiSetPortFormatSpecificInfoSync(PvmiCapabilityAndConfig *aPort,
        const char* aFormatValType)
{
    /*
     * Create PvmiKvp for capability settings
     */
    if (pv_mime_strcmp(aFormatValType, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
    {
        OsclMemAllocator alloc;
        PvmiKvp kvp;
        kvp.key = NULL;
        kvp.length = oscl_strlen(aFormatValType) + 1; // +1 for \0
        kvp.key = (PvmiKeyType)alloc.ALLOCATE(kvp.length);
        if (kvp.key == NULL)
        {
            return false;
        }
        oscl_strncpy(kvp.key, aFormatValType, kvp.length);
        OsclRefCounterMemFrag trackConfig;

        if ((!iPortParams->ipJitterBuffer) && (iPortParams->iTag != PVMF_JITTER_BUFFER_PORT_TYPE_INPUT))
        {
            PVMFJitterBufferPortParams* portParams = NULL;
            if (LocateInputPort(portParams, iPortParams))
            {
                if (portParams && portParams->ipJitterBuffer)
                {
                    portParams->ipJitterBuffer->GetTrackConfig(trackConfig);
                }
            }
        }
        else
        {
            iPortParams->ipJitterBuffer->GetTrackConfig(trackConfig);
        }

        if (trackConfig.getMemFragSize() == 0)
        {
            kvp.value.key_specific_value = 0;
            kvp.capacity = 0;
        }
        else
        {
            kvp.value.key_specific_value = (OsclAny*)(trackConfig.getMemFragPtr());
            kvp.capacity = trackConfig.getMemFragSize();
        }
        PvmiKvp* retKvp = NULL; // for return value
        int32 err;
        OSCL_TRY(err, aPort->setParametersSync(NULL, &kvp, 1, retKvp););
        /* ignore the error for now */
        alloc.deallocate((OsclAny*)(kvp.key));
        return true;
    }
    else if (pv_mime_strcmp(aFormatValType, PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY) == 0)
    {
        OsclMemAllocator alloc;
        PvmiKvp kvp;
        kvp.key = NULL;
        kvp.length = oscl_strlen(aFormatValType) + 1; // +1 for \0
        kvp.key = (PvmiKeyType)alloc.ALLOCATE(kvp.length);
        if (kvp.key == NULL)
        {
            return false;
        }
        oscl_strncpy(kvp.key, aFormatValType, kvp.length);

        kvp.value.uint32_value = MEDIALAYERNODE_MAXNUM_MEDIA_DATA; //need to get from JB
        PvmiKvp* retKvp = NULL; // for return value
        int32 err;
        OSCL_TRY(err, aPort->setParametersSync(NULL, &kvp, 1, retKvp););
        /* ignore the error for now */
        alloc.deallocate((OsclAny*)(kvp.key));
        return true;
    }
    return false;
}

bool
PVMFJitterBufferPort::pvmiGetPortFormatSpecificInfoSync(const char* aFormatValType,
        PvmiKvp*& aKvp) const
{
    if (pv_mime_strcmp(aFormatValType, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
    {
        OsclMemAllocator alloc;
        aKvp->key = NULL;
        aKvp->length = oscl_strlen(aFormatValType) + 1; // +1 for \0
        aKvp->key = (PvmiKeyType)alloc.ALLOCATE(aKvp->length);
        if (aKvp->key == NULL)
        {
            return false;
        }
        oscl_strncpy(aKvp->key, aFormatValType, aKvp->length);

        OsclRefCounterMemFrag trackConfig;
        if ((!iPortParams->ipJitterBuffer) && (iPortParams->iTag != PVMF_JITTER_BUFFER_PORT_TYPE_INPUT))
        {
            PVMFJitterBufferPortParams* portParams = NULL;
            LocateInputPort(portParams, iPortParams);
            if (portParams && portParams->ipJitterBuffer)
            {
                portParams->ipJitterBuffer->GetTrackConfig(trackConfig);
            }
        }
        else
        {
            iPortParams->ipJitterBuffer->GetTrackConfig(trackConfig);
        }

        if (trackConfig.getMemFragSize() == 0)
        {
            aKvp->value.key_specific_value = 0;
            aKvp->capacity = 0;
        }
        else
        {
            aKvp->value.key_specific_value = (OsclAny*)(trackConfig.getMemFragPtr());
            aKvp->capacity = trackConfig.getMemFragSize();
        }
        return true;
    }
    else if (pv_mime_strcmp(aFormatValType, PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY) == 0)
    {
        OsclMemAllocator alloc;
        aKvp->key = NULL;
        aKvp->length = oscl_strlen(aFormatValType) + 1; // +1 for \0
        aKvp->key = (PvmiKeyType)alloc.ALLOCATE(aKvp->length);
        if (aKvp->key == NULL)
        {
            return false;
        }
        oscl_strncpy(aKvp->key, aFormatValType, aKvp->length);
        aKvp->value.uint32_value = MEDIALAYERNODE_MAXNUM_MEDIA_DATA; //need to get from the JB
        return true;
    }
    return false;
}


bool PVMFJitterBufferPort::LocateInputPort(PVMFJitterBufferPortParams*& aInputPortParamsPtr, PVMFJitterBufferPortParams* aReferencePortParamsPtr) const
{
    const uint32 referencePortId = aReferencePortParamsPtr->iId;

    /* input port id must be outputPortId - 1 */


    Oscl_Vector<PVMFJitterBufferPortParams*, OsclMemAllocator>::const_iterator it;

    for (it = irJitterBufferNode.iPortParamsQueue.begin(); it != irJitterBufferNode.iPortParamsQueue.end(); ++it)
    {
        PVMFJitterBufferPortParams* portParams = *it;
        if (((portParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT) &&
                ((int32)portParams->iId == (int32)referencePortId - 1)) ||
                ((portParams->iTag == PVMF_JITTER_BUFFER_PORT_TYPE_INPUT) &&
                 ((int32)portParams->iId == (int32)referencePortId - 2))
           )
        {
            aInputPortParamsPtr = portParams;
            return true;
        }
    }
    return false;
}


