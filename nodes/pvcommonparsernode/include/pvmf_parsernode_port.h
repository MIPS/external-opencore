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
#ifndef PVMF_PARSERNODE_PORT_H_INCLUDED
#define PVMF_PARSERNODE_PORT_H_INCLUED

#ifndef PVMF_PORT_BASE_IMPL_H_INCLUDED
#include "pvmf_port_base_impl.h"
#endif
#ifndef OSCL_MEM_MEMPOOL_H_INCLUDED
#include "oscl_mem_mempool.h"
#endif
#ifndef PVMI_CONFIG_AND_CAPABILITY_UTILS_H_INCLUDED
#include "pvmi_config_and_capability_utils.h"
#endif

class PVMFParserNodeImpl;
class MediaClockConverter;
class PVMFResizableSimpleMediaMsgAlloc;

class PVMFParserNodePort: public PvmfPortBaseImpl
        , PvmiCapabilityAndConfigPortFormatImpl
{
    public:
        PVMFParserNodePort(int32 aPortTag
                           , PVMFPortActivityHandler* aNode
                           , const char* aName = NULL);
        ~PVMFParserNodePort();

        void QueryInterface(const PVUuid& aUuid, OsclAny*& aPtr);
        PVMFStatus Connect(PVMFPortInterface* aPort);

        // From PvmiCapabilityAndConfigPortFormatImpl
        bool IsFormatSupported(PVMFFormatType aFormat);
        void FormatUpdated();

    private:
        PVMFParserNodeImpl* iParserNode;
};

class PVMFParserNodeTrackPortInfo: public OsclMemPoolFixedChunkAllocatorObserver
        , public OsclMemPoolResizableAllocatorObserver
{
    public:
        PVMFParserNodeTrackPortInfo()
        {
            iTrackId = 0;
            iSeqNum = 0;
            iTimestamp = 0;
            iSampleNPT = 0;
            iSampleDur = 0;
            iNumSamples = 1;
            iBufferlen = 0;
            iSendBos = true;
            iSendEos = false;
            iEosSent = false;
            iOutgoingQueueBusy = false;
            iProcessOutgoingMsg = false;
            iFirstFrameAfterSeek = false;
            ipMimeString = NULL;
            ipNode = NULL;
            ipPort = NULL;
            ipClockConv = NULL;
            ipResizeableMemPoolAllocator = NULL;
            ipResizeableSimpleMediaMsg = NULL;
        }

        PVMFParserNodeTrackPortInfo(const PVMFParserNodeTrackPortInfo& aSrc)
                : OsclMemPoolFixedChunkAllocatorObserver(aSrc)
                , OsclMemPoolResizableAllocatorObserver(aSrc)
        {
            iTrackId = aSrc.iTrackId;
            iSeqNum = aSrc.iSeqNum;
            iTimestamp = aSrc.iTimestamp;
            iSampleNPT = aSrc.iSampleNPT;
            iSampleDur = aSrc.iSampleDur;
            iNumSamples = aSrc.iNumSamples;
            iBufferlen = aSrc.iBufferlen;
            iSendBos = aSrc.iSendBos;
            iSendEos = aSrc.iSendEos;
            iEosSent = aSrc.iEosSent;
            iOutgoingQueueBusy = aSrc.iOutgoingQueueBusy;
            iProcessOutgoingMsg = aSrc.iProcessOutgoingMsg;
            iFirstFrameAfterSeek = aSrc.iFirstFrameAfterSeek;
            ipMimeString = aSrc.ipMimeString;
            ipNode = aSrc.ipNode;
            ipPort = aSrc.ipPort;
            ipClockConv = aSrc.ipClockConv;
            ipResizeableMemPoolAllocator = aSrc.ipResizeableMemPoolAllocator;
            ipResizeableSimpleMediaMsg = aSrc.ipResizeableSimpleMediaMsg;
        }

        // From OsclMemPoolFixedChunkAllocatorObserver
        void freechunkavailable(OsclAny* aContextData)
        {
            OSCL_UNUSED_ARG(aContextData);
            if (ipNode)
            {
                ipNode->RunIfNotReady();
            }
        }

        // From OsclMemPoolResizableAllocatorObserver
        void freeblockavailable(OsclAny* aContextData)
        {
            OSCL_UNUSED_ARG(aContextData);
            if (ipNode)
            {
                ipNode->RunIfNotReady();
            }
        }

        // Reset all parameters
        void reset()
        {
            iSeqNum = 0;
            iTimestamp = 0;
            iSampleNPT = 0;
            iSampleDur = 0;
            iNumSamples = 1;
            iBufferlen = 0;
            iSendBos = true;
            iSendEos = false;
            iEosSent = false;
            iOutgoingQueueBusy = false;
            iProcessOutgoingMsg = false;
            iFirstFrameAfterSeek = false;
        }

        uint32 iTrackId;
        uint32 iSeqNum;
        PVMFTimestamp iTimestamp;
        PVMFTimestamp iSampleNPT;
        uint64 iSampleDur;
        uint32 iNumSamples;
        uint32 iBufferlen;
        bool iSendBos;
        bool iSendEos;
        bool iEosSent;
        bool iOutgoingQueueBusy;
        bool iProcessOutgoingMsg;
        bool iFirstFrameAfterSeek;
        char* ipMimeString;
        PVMFNodeInterfaceImpl* ipNode;
        PVMFPortInterface* ipPort;
        MediaClockConverter* ipClockConv;
        OsclMemPoolResizableAllocator* ipResizeableMemPoolAllocator;
        PVMFResizableSimpleMediaMsgAlloc* ipResizeableSimpleMediaMsg;
};


#endif
