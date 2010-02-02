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

#if !defined(LOGICAL_CHANNEL_H)
#define LOGICAL_CHANNEL_H
#include "oscl_mem.h"
#include "adaptationlayer.h"
#include "h324utils.h"

#ifdef LIP_SYNC_TESTING
#include "lipsync_singleton_object.h"
#endif

#ifndef PVMF_RESIZABLE_SIMPLE_MEDIAMSG_H_INCLUDED
#include "pvmf_resizable_simple_mediamsg.h"
#endif

#ifndef PVMF_FORMAT_TYPE_H_INCLUDED
#include "pvmf_format_type.h"
#endif

#ifndef OSCL_MEM_MEMPOOL_H_INCLUDED
#include "oscl_mem_mempool.h"
#endif

#ifndef PVMF_MEDIA_CLOCK_H_INCLUDED
#include "pvmf_media_clock.h"
#endif

#ifndef OSCL_MAP_H_INCLUDED
#include "oscl_map.h"
#endif

#ifndef PVMF_MEDIA_DATA_H_INCLUDED

#include "pvmf_media_data.h"
#endif

#ifndef PVMF_SIMPLE_MEDIA_BUFFER_H_INCLUDED
#include "pvmf_simple_media_buffer.h"
#endif

#ifndef PVMF_NODE_INTERFACE_H_INCLUDED
#include "pvmf_node_interface.h"
#endif

#ifndef PVMF_PORT_BASE_IMPL_H_INCLUDED
#include "pvmf_port_base_impl.h"
#endif

#ifndef PVMF_MEDIA_FRAG_GROUP_H_INCLUDED
#include "pvmf_media_frag_group.h"
#endif

#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#ifndef PVMI_CONFIG_AND_CAPABILITY_H_INCLUDED
#include "pvmi_config_and_capability_utils.h"
#endif

#ifndef OSCL_TIMER_H_INCLUDED
#include "oscl_timer.h"
#endif

#define INVALID_MUX_CODE 0xFF
#define DEF_NUM_MEDIA_DATA 100
#define SKEW_CHECK_INTERVAL 2000
#define PARSING_JITTER_DURATION 200
#define PVDEBUG_LOG_BITSTREAM_PKT(iDebug, comp, pkt) \
{\
uint8* ptrbuf = pkt->GetMediaPtr();\
PVDEBUG_LOG_BITSTREAM(iDebug, (comp, ptrbuf, pkt->GetMediaSize()) );\
pkt->ClearMediaPtr();\
}
class Skew_Detection_Timer;
class Incm_Skew_Detection_Timer;
class LCMediaDataEntry
{
    public:
        LCMediaDataEntry() : next(NULL) {}
        ~LCMediaDataEntry()
        {
            mediaData.Unbind();
        }
        PVMFSharedMediaDataPtr mediaData;
        LCMediaDataEntry* next;
};

class LcnAlloc : public Oscl_DefAlloc
{
    public:
        void* allocate(const uint32 size)
        {
            void* tmp = (void*)OSCL_DEFAULT_MALLOC(size);
            OSCL_ASSERT(tmp != 0);
            return tmp;
        }
        void deallocate(void* p)
        {
            OSCL_DEFAULT_FREE(p);
        }
};

class LogicalChannelObserver
{
    public:
        virtual ~LogicalChannelObserver() {}
        virtual int32 GetTimestamp() = 0;
        virtual void LogicalChannelError(TPVDirection direction, TPVChannelId id, PVMFStatus error) = 0;
        virtual void SkewDetected(TPVChannelId lcn1, TPVChannelId lcn2, uint32 skew) = 0;
        virtual void ReceivedFormatSpecificInfo(TPVChannelId lcn, uint8* fsi, uint32 fsi_len) = 0;
        virtual void CalculateSkew(int lcn, bool CheckAudVid, int Timestamp, bool CheckEot) = 0;
};

class H223LogicalChannel : public PvmfPortBaseImpl,
        public PVMFPortActivityHandler,
        public PvmiCapabilityAndConfig,
        public virtual LogicalChannelInfo
{
    public:
        H223LogicalChannel(TPVChannelId num,
                           bool segmentable,
                           OsclSharedPtr<AdaptationLayer>& al,
                           PS_DataType data_type,
                           LogicalChannelObserver* observer,
                           uint32 bitrate,
                           uint32 sample_interval,
                           uint32 num_media_data);
        virtual ~H223LogicalChannel();

        /* allocate resources in this function */
        virtual void Init() = 0;
        virtual TPVDirection GetDirection() = 0;

        // LogicalChannelInfo virtuals
        TPVChannelId GetLogicalChannelNumber()
        {
            return lcn;
        }

        uint32 GetSduSize()
        {
            return iAl->GetSduSize();
        }

        bool IsSegmentable()
        {
            return iSegmentable;
        }

        uint32 GetBitrate()
        {
            return iBitrate;
        }

        uint32 GetSampleInterval()
        {
            return iSampleInterval;
        }

        const uint8* GetFormatSpecificInfo(uint32* format_specific_info_len);
        PVMFTimestamp GetLastSduTimestamp()
        {
            return iLastSduTimestamp;
        }

        PVMFFormatType GetFormatType()
        {
            PVCodecType_t codec_type = GetCodecType(iDataType);
            return PVCodecTypeToPVMFFormatType(codec_type);
        }

        OsclAny SetNext(H223LogicalChannel* lcn_next)
        {
            next = lcn_next;
        }
        H223LogicalChannel* GetNext()
        {
            return next;
        }

        OsclSharedPtr<AdaptationLayer>GetAl()
        {
            return iAl;
        }

        OsclAny SetSampleInterval(uint16 sample_interval)
        {
            iSampleInterval = sample_interval;
        }

        /* Flushes the pending AL SDU data */
        virtual OsclAny Flush() = 0;

        virtual OsclAny ResetStats() = 0;
        virtual OsclAny LogStats() = 0;

        // Functions to pause and resume
        virtual void Pause();
        virtual void Resume();

        // Set format specific information
        PVMFStatus SetFormatSpecificInfo(uint8* info, uint32 info_len);

        void SetClock(PVMFMediaClock* aClock)
        {
            iClock = aClock;
        }

        //CapabilityAndConfig virtuals
        OSCL_IMPORT_REF void QueryInterface(const PVUuid& aUuid, OsclAny*& aPtr);

        /* PVMFPortActivityHandler virtuals */
        void HandlePortActivity(const PVMFPortActivity &aActivity)
        {
            OSCL_UNUSED_ARG(aActivity);
        }
        PVMFStatus setConfigParametersSync(PvmiKvp* selectedKvp, PvmiCapabilityAndConfig* aConfig, PVMFFormatType lcn_format_type = PVMF_MIME_FORMAT_UNKNOWN, bool aTryTwice = false);



        //Set the audio/video MIO latencies for the respective channel
        void SetAudioLatency(int32 aAudioLatency)
        {
            iAudioLatency = aAudioLatency;
        }
        void SetVideoLatency(int32 aVideoLatency)
        {
            iVideoLatency = aVideoLatency;
        }

    protected:
        TPVChannelId lcn;
        bool iSegmentable;
        H223LogicalChannel* next;
        OsclSharedPtr<AdaptationLayer> iAl;
        uint32 iBitrate;
        uint32 iSampleInterval;
        LogicalChannelObserver* iObserver;
        uint8* iFormatSpecificInfo;
        uint32 iFormatSpecificInfoLen;
        PVLogger* iLogger;
        uint32 iIncomingSkew;
        PVMFTimestamp iLastSduTimestamp;
        PS_DataType iDataType;
        OsclMemAllocator iKvpMemAlloc;
        uint32 iNumMediaData;
        uint32 iMaxSduSize;
        uint32 iDatapathLatency;
        PVMFFormatType iMediaType;
        bool iPaused;
        PVMFMediaClock* iClock;
        int32 iAudioLatency;
        int32 iVideoLatency;
};

/* For outgoing A/V/C ( to be muxed) */
class H223OutgoingChannel : public H223LogicalChannel


{


    public:
        H223OutgoingChannel(TPVChannelId num,
                            bool segmentable,
                            OsclSharedPtr<AdaptationLayer>& al,
                            PS_DataType data_type,
                            LogicalChannelObserver* observer,
                            uint32 bitrate,
                            uint32 sample_interval,
                            uint32 num_media_data);
        ~H223OutgoingChannel();

        void Init();

        TPVDirection GetDirection()
        {
            return OUTGOING;
        }
        bool GetNextPacket(PVMFSharedMediaDataPtr& aMediaData, PVMFStatus aStatus);

        OsclAny ReleasePacket(PVMFSharedMediaDataPtr& aMediaData);

        OsclAny Flush();

        OsclAny ResetStats();
        OsclAny LogStats();
        OsclAny SetSkewReference(LogicalChannelInfo* reference_channel)
        {
            iSkewReferenceChannel = reference_channel;
        }

        void BufferMedia(uint16 aMs);
        void SetBufferSizeMs(uint32 buffer_size_ms);
        Skew_Detection_Timer* inputTimer;

#ifdef LIP_SYNC_TESTING
        void StartTimer();
        bool iDisconnected;
        void TimerCallback();
        int  CheckValue();
        void AppendLipSyncTS();
        uint32 iAudioOutTS;
        uint32 iVideoOutTS;
        int32 iDiffVideoAudioTS;
        int32 iSqrCalVidAudTS ;
        int32  iRtMnSqCalc ;
        uint32 iTotalCountOut;
        ShareParams *iParams;

#endif



        void DetectSkewInd(PVMFSharedMediaMsgPtr aMsg);
        void CalcRMSInfo(uint32 VideoData, uint32 AudioData);
        uint32 GetNumBytesTransferred()
        {
            return iNumBytesIn;
        }
        // Functions to pause and resume the output of data from the logical channel to the mux
        void Resume();

        OSCL_IMPORT_REF PVMFStatus Connect(PVMFPortInterface* aPort);
        OSCL_IMPORT_REF virtual PVMFStatus PeerConnect(PVMFPortInterface* aPort);

        // Implement pure virtuals from PvmiCapabilityAndConfig interface
        OSCL_IMPORT_REF void setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver);
        OSCL_IMPORT_REF PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier,
                PvmiKvp*& aParameters, int& num_parameter_elements,
                PvmiCapabilityContext aContext);
        OSCL_IMPORT_REF PVMFStatus releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);
        OSCL_IMPORT_REF void createContext(PvmiMIOSession aSession, PvmiCapabilityContext& aContext);
        OSCL_IMPORT_REF void setContextParameters(PvmiMIOSession aSession, PvmiCapabilityContext& aContext,
                PvmiKvp* aParameters, int num_parameter_elements);
        OSCL_IMPORT_REF void DeleteContext(PvmiMIOSession aSession, PvmiCapabilityContext& aContext);
        OSCL_IMPORT_REF void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                               int num_elements, PvmiKvp * & aRet_kvp);
        OSCL_IMPORT_REF PVMFCommandId setParametersAsync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                int num_elements, PvmiKvp*& aRet_kvp, OsclAny* context = NULL);
        OSCL_IMPORT_REF uint32 getCapabilityMetric(PvmiMIOSession aSession);
        OSCL_IMPORT_REF PVMFStatus verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);
        /* PVMFPortActivityHandler virtuals */
        void HandlePortActivity(const PVMFPortActivity &aActivity);
    protected:
        virtual PVMFStatus PutData(PVMFSharedMediaMsgPtr media_msg);

        bool FragmentPacket(PVMFSharedMediaDataPtr& aMediaData, PVMFSharedMediaDataPtr& newpack);

        OsclSharedPtr<PVMFMediaDataImpl> StartAlPdu();

        PVMFStatus CompletePdu();

        PVMFStatus AppendOutgoingPkt(OsclSharedPtr<PVMFMediaDataImpl>& pdu, PVMFTimestamp timestamp,
                                     OsclRefCounterMemFrag* fsi = NULL);

        OsclAny ResetSkewParameters();


        PVMFStatus VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam);

        PVMFStatus NegotiateInputSettings(PvmiCapabilityAndConfig* config);

        PVLOGGER_LOG_USE_ONLY(
            PVMFStatus TestFsi(OsclRefCounterMemFrag aMemFrag);
            uint32 iFsiTestIndex;
        )

        OsclMemPoolFixedChunkAllocator* iMediaMsgMemoryPool;
        OsclMemPoolFixedChunkAllocator* iMediaDataEntryAlloc;
        LCMediaDataEntry* lastMediaData;
        PVMFMediaFragGroupCombinedAlloc<OsclMemAllocator>* iMediaFragGroupAlloc;

        OsclMemPoolFixedChunkAllocator* iPduPktMemPool;
        OsclMemAllocator iMemAlloc;
        PVMFSimpleMediaBufferCombinedAlloc iMediaDataAlloc;

        OsclSharedPtr<PVMFMediaDataImpl> iCurPdu;
        TimeValue iCreateTime;
        TimeValue iStartTime;
        uint32 iNumPacketsIn;
        uint32 iNumSdusIn;
        uint32 iNumSdusDropped;
        uint32 iNumBytesIn;
        uint32 iNumSdusOut;
        uint32 iNumBytesOut;
        uint32 iMaxPacketMuxTime;
        uint32 iMaxSduMuxTime;
        uint32 iNumFlush;
        uint32 iNumBytesFlushed;
        PVMFSharedMediaDataPtr ihdrTSData;
        // skew related
        LogicalChannelInfo* iSkewReferenceChannel;
        int32 iSetBufferMediaMs;
        int32 iSetBufferMediaBytes;
        int32 iBufferMediaMs;
        int32 iBufferMediaBytes;
        bool iMuxingStarted;
        PVMFTimestamp iCurPduTimestamp;
        uint32 iNumPendingPdus;
        uint32 iTsmsec;
        bool iOnlyOnce;
        PVLogger* iOutgoingAudioLogger;
        PVLogger* iOutgoingVideoLogger;
        bool iWaitForRandomAccessPoint;
        uint32 iBufferSizeMs;
        OsclRefCounterMemFrag iFsiFrag;
};


class H223OutgoingControlChannel : public H223OutgoingChannel
{
    public:
        H223OutgoingControlChannel(OsclSharedPtr<AdaptationLayer>& al,
                                   PS_DataType data_type,
                                   LogicalChannelObserver* observer,
                                   uint32 bitrate,
                                   uint32 sample_interval,
                                   uint32 num_media_data)
                : H223OutgoingChannel(0, SEGMENTABLE, al, data_type, observer, bitrate, sample_interval, num_media_data)
        {
        }

        PVMFStatus PutData(PVMFSharedMediaMsgPtr aMsg);
        OSCL_IMPORT_REF PVMFStatus PeerConnect(PVMFPortInterface* aPort);

};

#define NUM_INCOMING_SDU_BUFFERS 8
/* For incoming (from the remote terminal) A/V/C */
class H223IncomingChannel : public H223LogicalChannel, public OsclTimerObserver
{
    public:
        H223IncomingChannel(TPVChannelId num,
                            bool segmentable,
                            OsclSharedPtr<AdaptationLayer>& al,
                            PS_DataType data_type,
                            LogicalChannelObserver* observer,
                            uint32 bitrate,
                            uint32 sample_interval,
                            uint32 num_media_data);
        ~H223IncomingChannel();
        void Init();

        // from OsclTimerObserver
        void TimeoutOccurred(int32 aTimerID, int32 aTimeoutInfo);

        TPVDirection GetDirection()
        {
            return INCOMING;
        }

        virtual PVMFStatus PutData(PVMFSharedMediaMsgPtr aMsg)
        {
            OSCL_UNUSED_ARG(aMsg);
            return PVMFErrNotSupported;
        }

        PVMFStatus GetData(PVMFSharedMediaMsgPtr aMsg)
        {
            OSCL_UNUSED_ARG(aMsg);
            return PVMFErrNotSupported;
        }

        /* Dispaches packets to bound PAcketInput */
        PVMFStatus AlPduData(uint8* buf, uint16 len);

        PVMFStatus AlDispatch();
        OsclAny ResetAlPdu();
        OsclAny AllocateAlPdu();
        OsclAny AppendAlPduFrag();
        uint32 CopyAlPduData(uint8* buf, uint16 len);
        uint32 CopyToCurrentFrag(uint8* buf, uint16 len);

        Incm_Skew_Detection_Timer* inputTimerIncm;

#ifdef LIP_SYNC_TESTING
        bool iIncmDisconnected;
        void StartTimer();
        void TimerCallback();
        ShareParams* iParam;
        void CalculateRMSInfo(uint32 VideoData, uint32 AudioData);
        void DetectFrameBoundary(uint8* buffer, uint32 aTimestamp);
        void ExtractTimestamp();
#endif

        OsclAny Flush();

        OsclAny ResetStats();
        OsclAny LogStats();

        uint32 GetNumSdusIn()
        {
            return iNumSdusIn;
        }
        // overload Connect to send out format specific info if available
        PVMFStatus Connect(PVMFPortInterface* aPort);

        // H223LogicalChannel
        void Pause();
        void Resume();

        // Implement pure virtuals from PvmiCapabilityAndConfig interface
        OSCL_IMPORT_REF void setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver);
        OSCL_IMPORT_REF PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier,
                PvmiKvp*& aParameters, int& num_parameter_elements,
                PvmiCapabilityContext aContext);
        OSCL_IMPORT_REF PVMFStatus releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);
        OSCL_IMPORT_REF void createContext(PvmiMIOSession aSession, PvmiCapabilityContext& aContext);
        OSCL_IMPORT_REF void setContextParameters(PvmiMIOSession aSession, PvmiCapabilityContext& aContext,
                PvmiKvp* aParameters, int num_parameter_elements);
        OSCL_IMPORT_REF void DeleteContext(PvmiMIOSession aSession, PvmiCapabilityContext& aContext);
        OSCL_IMPORT_REF void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                               int num_elements, PvmiKvp * & aRet_kvp);
        OSCL_IMPORT_REF PVMFCommandId setParametersAsync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                int num_elements, PvmiKvp*& aRet_kvp, OsclAny* context = NULL);
        OSCL_IMPORT_REF uint32 getCapabilityMetric(PvmiMIOSession aSession);
        OSCL_IMPORT_REF PVMFStatus verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);
        /* PVMFPortActivityHandler virtuals */
        void HandlePortActivity(const PVMFPortActivity &aActivity);

    protected:

        PVMFStatus DispatchOutgoingMsg(PVMFSharedMediaDataPtr aMediaData);
        bool CheckFrameBoundary(uint8* aDataPtr, int32 aDataSize, uint32 aCrcError);
        bool pvmiSetPortFormatSpecificInfoSync(PvmiCapabilityAndConfig *aPort,
                                               const char* aFormatValType);
        PVMFStatus SendVideoFrame(PVMFSharedMediaDataPtr aMediaData);
        PVMFStatus CallSendVideoFrame(PVMFSharedMediaDataPtr aMediaData);


    private:
        void PreAlPduData();
        PVMFStatus VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam);
        PVMFStatus NegotiateOutputSettings(PvmiCapabilityAndConfig* config);

        OsclAny SendFormatSpecificInfo();


        PVMFStatus SendBeginOfStreamMediaCommand();

        /**
         * Add audio media data queue and smoothens timestamps
         * and send oldest audio frame to the output port
         * If aMediaDataPtr is null queue is flushed
         *
         * @param aMediaDataPtr
         *
         * @returns PVMFStatus
         **/
        PVMFStatus SendAudioFrame(PVMFSharedMediaDataPtr aMediaDataPtr);

        /**
         * Gets current time and puts it into timestamp (iCurTimestamp)
         *
         * @returns void
         **/
        void UpdateCurrentTimestamp();

        /**
         * Dispatch media data to outgoing port if bos message is already sent.
         *
         * @param arMediaDataPtr media data
         *
         * @returns PVMFStatus PVMFSuccess if succesful
         **/
        PVMFStatus DispatchMessage(PVMFSharedMediaDataPtr& arMediaDataPtr);

        /**
         * Check if bos message is already sent.
         * Tries to send bos message if sent.
         *
         * @returns bool true if bos was sent.
         **/
        bool IsResumable();

        /**
         * Creates audio NO_DATA frame
         *
         * @param PVMFSharedMediaDataPtr media data pointer to NO_DATA frame
         *
         * @returns PVMFStatus PVMFSuccess if succesful
         **/
        PVMFStatus CreateNoDataAudioFrame(PVMFSharedMediaDataPtr &arMediaDataPtr);

        PVMFBufferPoolAllocator iMemFragmentAlloc;
        OsclMemPoolFixedChunkAllocator* iMediaMsgMemoryPool;
        PVMFMediaFragGroupCombinedAlloc<OsclMemAllocator>* iMediaFragGroupAlloc;
        OsclMemPoolFixedChunkAllocator* iPduPktMemPool;

        OsclMemAllocator iMemAlloc;
        PVMFSimpleMediaBufferCombinedAlloc iMediaDataAlloc;
        OsclSharedPtr<PVMFMediaDataImpl> iAlPduMediaData;
        uint8* iAlPduFragPos;
        OsclRefCounterMemFrag iAlPduFrag;
        uint32 iPduSize;
        uint32 iCurPduSize;
        TimeValue iCreateTime;
        TimeValue iStartTime;
        uint32 iNumPdusIn;
        uint32 iNumSdusIn;
        uint32 iNumBytesIn;
        uint32 iSduSizeExceededCnt;
        uint32 iNumCrcErrors;
        uint32 iNumSeqNumErrors;
        uint32 iNumAbort;
        uint32 iNumBytesFlushed;
        PVMFTimestamp iCurTimestamp;
        friend class TSC_324m;
        PVLogger* iIncomingAudioLogger;
        PVLogger* iIncomingVideoLogger;
        uint32 iVideoFrameNum;
        uint32 iMax_Chunk_Size;
        PVMFSharedMediaDataPtr iVideoFrame;
        OsclMemPoolResizableAllocator*  ipVideoFrameReszMemPool;
        PVMFResizableSimpleMediaMsgAlloc*  ipVideoFrameAlloc;
        OsclMemPoolFixedChunkAllocator *   ipVideoDataMemPool;
        // small queue for audio that is needed to smoothen the timestamps
        Oscl_Vector<PVMFSharedMediaDataPtr, OsclMemAllocator> iAudioDataQueue;

        // a timestamp from last audio frame sent to output port
        PVMFTimestamp iLastAudioTS;

        // frame number for audio frame
        uint32 iAudioFrameNum;

        // timer to be used to detect missing audio frames
        OsclTimer<OsclMemAllocator>* ipAudioDtxTimer;

        // bos message is needed when playing is started.
        bool iBosMessageSent;
};

class MuxSduData
{
    public:
        MuxSduData();
        OsclSharedPtr<H223OutgoingChannel> lcn;
        PVMFSharedMediaDataPtr sdu;
        uint16 size;
        uint16 cur_frag_num;
        uint16 cur_pos;
};

typedef Oscl_Vector<MuxSduData, OsclMemAllocator> MuxSduDataList;

#endif

