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
#include "oscl_rand.h"


#include "pvmf_simple_media_buffer.h"
#include "pvmf_media_data.h"
#include "pvmf_media_cmd.h"
#include "pvmf_media_msg_format_ids.h"

#ifndef OSCL_MIME_STRING_UTILS_H
#include "pv_mime_string_utils.h"
#endif

#include "logicalchannel.h"

#ifndef PER_COPIER
#include "h245_copier.h"
#endif

#ifndef PER_DELETER
#include "h245_deleter.h"
#endif
// decreasing DEF_CHANNEL_BUFFER_SIZE_MS can result out of memory errors,
// please change also PV_MAX_VIDEO_FRAME_SIZE to lower value.
#define DEF_CHANNEL_BITRATE 64000
#define DEF_INCOMING_CHANNEL_BUFFER_SIZE_MS 2800
#define DEF_OUTGOING_CHANNEL_BUFFER_SIZE_MS 2800
#define TIMESTAMP_MAX INT_MAX
#define DEF_SEGMENTABLE_CHANNEL_OH_BPS 2000
#define H223_INCOMING_CHANNEL_NUM_MEDIA_MSG 300
#define H223_OUTGOING_CHANNEL_NUM_MEDIA_MSG 300
#define H223_INCOMING_CHANNEL_FRAGMENT_SIZE 128
#define H223_INCOMING_CHANNEL_NUM_MEDIA_DATA 300
#define H223_INCOMING_CHANNEL_NUM_FRAGS_IN_MEDIA_DATA (3*1024/128)
#define PV2WAY_BPS_TO_BYTES_PER_MSEC_RIGHT_SHIFT 13
#define H223_LCN_IN_TIMESTAMP_BASE 40
#define MAX_VIDEO_FRAME_PARSE_SIZE (45*1024)
#define MAX_VIDEO_FRAMES 30
#define PVVIDEOPARSER_MEDIADATA_SIZE 128
#define MAX_MEMPOOL_BUFFER_NUM_LIMIT 5



#define H263_START_BYTE_1 0x00
#define H263_START_BYTE_2 0x00
#define H263_START_BYTE_3 0x80
#define H263_START_BYTE_3_MASK 0xFC


#define VOP_START_BYTE_1 0x00
#define VOP_START_BYTE_2 0x00
#define VOP_START_BYTE_3 0x01
#define VOP_START_BYTE_4 0xB6
#define VOP_START_BYTE_5 0xB0

#define H264_START_BYTE_1 0x00
#define H264_START_BYTE_2 0x00
#define H264_START_BYTE_3 0x00
#define H264_LAST_START_BYTE 0x01

// audio timestamp smoothing constants
static const uint16 AMR_FRAME_SIZE_MS = 20; // amr frame size in milliseconds
static const uint16 AMR_JITTER = 80; // maximum jitter allocated in timestamps
static const uint16 AMR_QUEUE_SIZE = 5; // number of frames used to smoothen timestamps
static const uint16 AMR_SID_SIZE = 6; // amr SID_UPDATE frame size in bytes
static const uint16 AMR_SID_UPDATE = 8; // sid update should happen in every 8 frame (3GPP TS 26.093)
static const uint16 AMR_NODATA_SIZE = 1; // amr SID_UPDATE frame size in bytes
static const uint32 AMR_TIMER_FREQUENCY = AMR_FRAME_SIZE_MS * AMR_SID_UPDATE * 1000; // dtx timer frequency in microseconds
static const int32 AMR_TIMER_ID = 0; // dtx timer id
static const uint8 AMR_NO_DATA = 15; // NO_DATA frame type

#ifdef LIP_SYNC_TESTING
/***********************Outgoing side********************/
uint32  g_Checkcount = 0;
int g_CheckSampleTime = -1;
uint32  g_Incmtsmsec = 0;
bool g_OnlyOneTime = true;
/*****************************************************************/
//Incoming side
uint32 g_IncmAudioTS = 0;
uint32  g_IncmVideoTS = 0;
uint32 g_TotalCountIncm = 0;
int g_DiffIncmVidAudTS = 0;
int g_iSqrIncCalVidAudTS = 0;
int  g_IncRtMnSqCalc = 0;
/**********************************************************************/
int  g_RecTimeStamp = 0;

#endif

H223LogicalChannel::H223LogicalChannel(TPVChannelId num,
                                       bool segmentable,
                                       OsclSharedPtr<AdaptationLayer>& al,
                                       PS_DataType data_type,
                                       LogicalChannelObserver* observer,
                                       uint32 bitrate,
                                       uint32 sample_interval,
                                       uint32 num_media_data)
        : PvmfPortBaseImpl(num, this),
        lcn(num),
        next(NULL),
        iAl(al),
        iBitrate(0),
        iSampleInterval(0),
        iObserver(observer),
        iFormatSpecificInfo(NULL),
        iFormatSpecificInfoLen(0),
        iLogger(NULL),
        iDataType(NULL),
        iAudioLatency(0),
        iVideoLatency(0)
{
    iSegmentable = segmentable;
    iIncomingSkew = 0;
    iLastSduTimestamp = 0;
    iSampleInterval = sample_interval;
    uint32 bitrate_overhead = IsSegmentable() ? DEF_SEGMENTABLE_CHANNEL_OH_BPS :
                              (uint32)((2000 / sample_interval + 1) >> 1) * (al->GetHdrSz() + al->GetTrlrSz());
    iBitrate = bitrate + bitrate_overhead;
    iNumMediaData = num_media_data;
    iMaxSduSize = (uint16)(iAl->GetSduSize() - iAl->GetHdrSz() - iAl->GetTrlrSz());
    if (data_type)
    {
        iDataType = Copy_DataType(data_type);
    }
    iMediaType = GetFormatType();
    iPaused = false;
    iClock = NULL;
}

H223LogicalChannel::~H223LogicalChannel()
{
    if (iDataType)
    {
        Delete_DataType(iDataType);
        OSCL_DEFAULT_FREE(iDataType);
        iDataType = NULL;
    }
    if (iFormatSpecificInfo)
    {
        oscl_free(iFormatSpecificInfo);
        iFormatSpecificInfo = NULL;
        iFormatSpecificInfoLen = 0;
    }

    // we need to clear the activity handler, since otherwise the PvmfPortBaseImpl destructor
    // ends up calling back onto our HandlePortActivity method, which no longer exists because
    // this objects's destructor has already been called.
    SetActivityHandler(NULL);
}

void H223LogicalChannel::Init()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223LogicalChannel::Init"));
}

PVMFStatus H223LogicalChannel::SetFormatSpecificInfo(uint8* info, uint32 info_len)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223LogicalChannel(%d)::SetFormatSpecificInfo info_len=%d, info=%x", lcn, info_len, info));
    if (iFormatSpecificInfo)
    {
        oscl_free(iFormatSpecificInfo);
        iFormatSpecificInfo = NULL;
        iFormatSpecificInfoLen = 0;
    }

    if (info == NULL || info_len == 0)
        return PVMFSuccess;

    iFormatSpecificInfo = (uint8*)oscl_malloc(info_len);
    oscl_memcpy(iFormatSpecificInfo, info, info_len);
    iFormatSpecificInfoLen = info_len;
    return PVMFSuccess;
}

const uint8* H223LogicalChannel::GetFormatSpecificInfo(uint32* info_len)
{
    if (info_len == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223LogicalChannel(%d)::GetFormatSpecificInfo ERROR info_len==NULL", lcn));
        return NULL;
    }
    *info_len = iFormatSpecificInfoLen;
    return iFormatSpecificInfo;
}


OSCL_EXPORT_REF void H223LogicalChannel::QueryInterface(const PVUuid& aUuid, OsclAny*& aPtr)
{
    aPtr = NULL;
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        aPtr = (PvmiCapabilityAndConfig*)this;
    }
    else if (aUuid == PVH324MLogicalChannelInfoUuid)
    {
        aPtr = (LogicalChannelInfo*)this;
    }
    else
    {
        OSCL_LEAVE(OsclErrNotSupported);
    }
}

void H223LogicalChannel::Pause()
{
    if (iPaused)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223LogicalChannel(d%)::Pause - Already paused", lcn));
        return;
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223LogicalChannel(d%)::Pause", lcn));

    iPaused = true;
    // flush any pending media data
    Flush();

}

void H223LogicalChannel::Resume()
{
    iPaused = false;
}

H223OutgoingChannel::H223OutgoingChannel(TPVChannelId num,
        bool segmentable,
        OsclSharedPtr<AdaptationLayer>& al,
        PS_DataType data_type,
        LogicalChannelObserver* observer,
        uint32 bitrate,
        uint32 sample_interval,
        uint32 num_media_data)
        : H223LogicalChannel(num, segmentable, al, data_type, observer, bitrate, sample_interval, num_media_data),
        iMediaMsgMemoryPool(NULL),
        iMediaDataEntryAlloc(NULL),
        iMediaFragGroupAlloc(NULL),
        iPduPktMemPool(NULL),
        iMediaDataAlloc(&iMemAlloc)
{
    iLogger = PVLogger::GetLoggerObject("3g324m.h223.H223OutgoingChannel");
    iOutgoingVideoLogger = PVLogger::GetLoggerObject("datapath.outgoing.video.h223.lcn");
    iOutgoingAudioLogger = PVLogger::GetLoggerObject("datapath.outgoing.audio.h223.lcn");

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel::H223OutgoingChannel(%d) - num(%d),segmentable(%d)", lcn , num, segmentable));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel::H223OutgoingChannel(%d) - AL SDU size(%d), hdr(%d), trlr(%d)", lcn , iAl->GetSduSize(), iAl->GetHdrSz(), iAl->GetTrlrSz()));
    ResetStats();
    lastMediaData = NULL;

    iSetBufferMediaMs = 0;
    iSetBufferMediaBytes = 0;
    iBufferMediaMs = 0;
    iBufferMediaBytes = 0;
    iCurPduTimestamp = 0;
    iNumPendingPdus = 0;
    iTsmsec = 0;
    iOnlyOnce = false;
#ifdef LIP_SYNC_TESTING
    iAudioOutTS = 0;
    iVideoOutTS = 0;
    iDiffVideoAudioTS = 0;
    iSqrCalVidAudTS = 0;
    iRtMnSqCalc = 0;
    iTotalCountOut = 0;
    iParams = ShareParams::Instance();
#endif
    iMuxingStarted = false;
    iWaitForRandomAccessPoint = false;
    iBufferSizeMs = DEF_OUTGOING_CHANNEL_BUFFER_SIZE_MS;
    PVLOGGER_LOG_USE_ONLY(
        iFsiTestIndex = 0;
    )

}

H223OutgoingChannel::~H223OutgoingChannel()
{
    if (iDataType)
    {
        Delete_DataType(iDataType);
        OSCL_DEFAULT_FREE(iDataType);
        iDataType = NULL;
    }
    Flush();
    if (iMediaFragGroupAlloc)
    {
        iMediaFragGroupAlloc->removeRef();
    }
    if (iPduPktMemPool)
    {
        OSCL_DELETE(iPduPktMemPool);
    }
    if (iMediaDataEntryAlloc)
    {
        OSCL_DELETE(iMediaDataEntryAlloc);
    }
    if (iMediaMsgMemoryPool)
    {
        OSCL_DELETE(iMediaMsgMemoryPool);
    }


}

void H223OutgoingChannel::Init()
{
    H223LogicalChannel::Init();
    iMediaMsgMemoryPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (H223_OUTGOING_CHANNEL_NUM_MEDIA_MSG));
    iMediaMsgMemoryPool->enablenullpointerreturn();
    iMediaDataEntryAlloc = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (iNumMediaData, sizeof(LCMediaDataEntry)));
    iMediaDataEntryAlloc->enablenullpointerreturn();
    iPduPktMemPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (iNumMediaData));
    iPduPktMemPool->enablenullpointerreturn();
    iMediaFragGroupAlloc = OSCL_NEW(PVMFMediaFragGroupCombinedAlloc<OsclMemAllocator>, (iNumMediaData, 10, iPduPktMemPool));
    iMediaFragGroupAlloc->create();
}


void H223OutgoingChannel::BufferMedia(uint16 aMs)
{
    if (iBufferSizeMs && aMs > iBufferSizeMs)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::BufferMedia ERROR buffer interval=%d > buffer size=%d", lcn , aMs, iBufferSizeMs));
    }
    iSetBufferMediaMs = iBufferMediaMs = aMs;
    iSetBufferMediaBytes = iBufferMediaBytes = ((iBufferSizeMs * iBitrate + 4000) / 8000);
    //PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,"H223OutgoingChannel(%d)::BufferMedia ms=%d,bytes=%d",lcn, iBufferMediaMs,iBufferMediaBytes));
}

void H223OutgoingChannel::Resume()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::Resume", lcn));
    H223LogicalChannel::Resume();
    iPaused = false;
    // start muxing on a random access point
    //iWaitForRandomAccessPoint=true;
}

void H223OutgoingChannel::SetBufferSizeMs(uint32 buffer_size_ms)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingLogicalChannel(%d)::SetBufferSizeMs buffer_size_ms=%d", lcn, buffer_size_ms));
    iBufferSizeMs = buffer_size_ms;
}



PVMFStatus H223OutgoingChannel::PutData(PVMFSharedMediaMsgPtr media_msg)
{
    PVMFStatus ret = PVMFSuccess;
    PVMFSharedMediaDataPtr mediaData;
    convertToPVMFMediaData(mediaData, media_msg);

    PV_STAT_SET_TIME(iStartTime, iNumPacketsIn)
    PV_STAT_INCR(iNumPacketsIn, 1)

    /* zero through 255 is reserved for media data */
    if (media_msg->getFormatID() >= PVMF_MEDIA_CMD_FORMAT_IDS_START)
    {
        return PVMFSuccess;
    }

    PV_STAT_INCR(iNumBytesIn, (mediaData->getFilledSize()))
    TimeValue timenow;
    if (iMediaType.isCompressed() && iMediaType.isAudio())
    {
        PVMF_OUTGOING_AUDIO_LOGDATATRAFFIC((0, "Outgoing audio frames received. Stats: Entry time=%u, ts=%u, lcn=%d, size=%d", timenow.to_msec(), mediaData->getTimestamp(), lcn, mediaData->getFilledSize()));
    }
    else if (iMediaType.isCompressed() && iMediaType.isVideo())
    {
        PVMF_OUTGOING_VIDEO_LOGDATATRAFFIC((0, "Outgoing video frames received.Stats: Entry time=%u, ts=%u, lcn=%d, size=%d", timenow.to_msec(), mediaData->getTimestamp(), lcn, mediaData->getFilledSize()));
    }

    // Check for FSI so that it can be send with data
    if (mediaData->getFormatSpecificInfo(iFsiFrag) && iFsiFrag.getMemFragPtr() && iFsiFrag.getMemFragSize())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PutData Received Format Specific Info, len=%d", lcn, iFsiFrag.getMemFragSize()));
        PVLOGGER_LOG_USE_ONLY(

            TestFsi(iFsiFrag);
        )
    }

    if (IsSegmentable() && iWaitForRandomAccessPoint)
    {
        if ((mediaData->getMarkerInfo()&PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT) == 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PutData Not random access point.  Dropping media data.", lcn));
            return PVMFErrInvalidState;
        }
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PutData Found random access point.", lcn));
        iWaitForRandomAccessPoint = false;
    }
    else if (iNumPendingPdus == (iNumMediaData - 1))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PutData - ERROR Overflow, iNumPendingPdus=%u", lcn, iNumPendingPdus));
        return PVMFErrOverflow;
    }

    uint32 num_frags_required = 0;
    uint32 frag_num = 0;
    for (frag_num = 0; frag_num < mediaData->getNumFragments(); frag_num++)
    {
        OsclRefCounterMemFrag memfrag;
        mediaData->getMediaFragment(frag_num, memfrag);
        if (memfrag.getMemFragSize() > iMaxSduSize)
        {
            num_frags_required++;
        }
    }

    if ((mediaData->getNumFragments() + num_frags_required + iNumPendingPdus) >= (iNumMediaData - 1))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PutData - ERROR Overflow, iNumPendingPdus=%u, num_frags_required=%d,iNumMediaData=%d", lcn, iNumPendingPdus, num_frags_required, iNumMediaData));
        Flush();
        /* Start re-buffering */
        BufferMedia((uint16)iSetBufferMediaMs);
        return PVMFErrOverflow;
    }

    /* Fragment the sdu if needed */
    PVMFSharedMediaDataPtr& fragmentedMediaData = mediaData;
    if (num_frags_required)
    {
        if (true != FragmentPacket(mediaData, fragmentedMediaData))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PutData - Memory allocation failure on Fragment\n", lcn));
            return PVMFErrOverflow;
        }
    }

    uint32 sdu_size = 0;
    if (iCurPdu.GetRep())
    {
        sdu_size = iCurPdu->getFilledSize() - iAl->GetHdrSz();
        /* Is the timestamp different ? */
        if (iCurPduTimestamp != fragmentedMediaData->getTimestamp() || !IsSegmentable())
        {
            if (PVMFSuccess != CompletePdu())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PutData - Memory allocation failure on CompletePdu\n", lcn));
                return PVMFErrOverflow;
            }
            if (iMediaType.isCompressed() && iMediaType.isAudio())
            {
                PVMF_OUTGOING_AUDIO_LOGDATATRAFFIC((0, "Stats of the outgoing audio SDU are: timestamp=%u, size=%d", iCurPduTimestamp, sdu_size));
            }
            else if (iMediaType.isCompressed() && iMediaType.isVideo())
            {
                PVMF_OUTGOING_VIDEO_LOGDATATRAFFIC((0, "Stats of the outgoing video SDU are: timestamp=%u, size=%d", iCurPduTimestamp, sdu_size));
            }
            sdu_size = 0;
        }
    }
    if (sdu_size == 0)
    {
        //PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,"H223OutgoingChannel(%d)::PutData Sdu size == 0", lcn));
        iCurPdu = StartAlPdu();
        if (!iCurPdu)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "H223OutgoingChannel(%d)::PutData - Memory allocation failure on StartAlPdu\n", lcn));
            return PVMFErrOverflow;
        }

        if (iFsiFrag.getMemFragSize())
        {
            iCurPdu->appendMediaFragment(iFsiFrag);
            // reset the FSI frag
            OsclRefCounterMemFrag frag;
            iFsiFrag = frag;
        }
    }

    for (frag_num = 0; frag_num < fragmentedMediaData->getNumFragments(); frag_num++)
    {
        OsclRefCounterMemFrag frag;
        fragmentedMediaData->getMediaFragment(frag_num, frag);
        OSCL_ASSERT(frag.getMemFragSize() <= iMaxSduSize);

        if (sdu_size &&
                ((sdu_size + frag.getMemFragSize() > iMaxSduSize) || !IsSegmentable()))
        {
            if (PVMFSuccess != CompletePdu())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PutData - Memory allocation failure on CompletePdu\n", lcn));
                return PVMFErrOverflow;
            }
            if (iMediaType.isCompressed() && iMediaType.isAudio())
            {
                PVMF_OUTGOING_AUDIO_LOGDATATRAFFIC((0, "Stats of the outgoing audio SDU are: timestamp=%u, size=%d", iCurPduTimestamp, sdu_size));
            }
            else if (iMediaType.isCompressed() && iMediaType.isVideo())
            {
                PVMF_OUTGOING_VIDEO_LOGDATATRAFFIC((0, "Stats of the outgoing video SDU are: timestamp=%u, size=%d", iCurPduTimestamp, sdu_size));
            }
            sdu_size = 0;

            iCurPdu = StartAlPdu();

            if (!iCurPdu)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "H223OutgoingChannel(%d)::PutData - Memory allocation failure on StartAlPdu\n", lcn));
                return PVMFErrOverflow;
            }
        }

        iCurPdu->appendMediaFragment(frag);
        sdu_size += frag.getMemFragSize();
        iCurPduTimestamp = fragmentedMediaData->getTimestamp();

    }

    return ret;
}

bool H223OutgoingChannel::FragmentPacket(PVMFSharedMediaDataPtr& aMediaData, PVMFSharedMediaDataPtr& aFragmentedMediaData)
{
    OsclRefCounterMemFrag memfrag;
    OsclSharedPtr<PVMFMediaDataImpl> newpack;
    newpack = iMediaFragGroupAlloc->allocate();
    if (!newpack.GetRep())
    {
        return false;
    }

    int32 pkt_size = 0;
    PVMFTimestamp timestamp = aMediaData->getTimestamp();
    for (uint32 frag_num = 0; frag_num < aMediaData->getNumFragments(); frag_num++)
    {
        aMediaData->getMediaFragment(frag_num, memfrag);
        pkt_size = memfrag.getMemFragSize();
        if ((unsigned)pkt_size <= iMaxSduSize)
        {
            newpack->appendMediaFragment(memfrag);
        }
        else  /* Need to fragment it */
        {
            uint8* pos = (uint8*)memfrag.getMemFragPtr();
            int32 trim_frag_sz = iMaxSduSize;
            while (pkt_size)
            {
                trim_frag_sz = ((unsigned)pkt_size > iMaxSduSize) ? iMaxSduSize : pkt_size;
                pkt_size -= trim_frag_sz;
                OsclRefCounterMemFrag trim_frag(memfrag);
                trim_frag.getMemFrag().ptr = pos;
                trim_frag.getMemFrag().len = trim_frag_sz;
                newpack->appendMediaFragment(trim_frag);
                pos += trim_frag_sz;
            }
        }
    }
    aFragmentedMediaData = PVMFMediaData::createMediaData(newpack, iMediaMsgMemoryPool);
    if (aFragmentedMediaData.GetRep())
    {
        aFragmentedMediaData->setTimestamp(timestamp);
        return true;
    }
    return false;
}

OsclSharedPtr<PVMFMediaDataImpl> H223OutgoingChannel::StartAlPdu()
{
    PV_STAT_INCR(iNumSdusIn, 1)

    // allocate packet
    OsclSharedPtr<PVMFMediaDataImpl> pdu = iMediaFragGroupAlloc->allocate();
    if (pdu)
    {
        // Add header
        PVMFStatus status = iAl->StartPacket(pdu);
        if (status != PVMFSuccess)
        {
            pdu.Unbind();
            return pdu;
        }
        iNumPendingPdus++;
    }

    return pdu;
}

PVMFStatus H223OutgoingChannel::CompletePdu()
{

    PVLOGGER_LOG_USE_ONLY(
        if (iNumPacketsIn < 4)
{
    for (uint16 index = 0; index < iCurPdu->getNumFragments(); index++)
        {
            OsclRefCounterMemFrag memFrag;
            iCurPdu->getMediaFragment(index, memFrag);
            TestFsi(memFrag);
        }
    }
    )

    PVMFStatus status = iAl->CompletePacket(iCurPdu);
    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::CompletePdu Memory allocation failed, CompletePacket status=%d", lcn, status));
        return status;
    }


#ifdef LIP_SYNC_TESTING
    if (iParams->iCompressed == false)
    {
        AppendLipSyncTS();
    }
#endif

    // Add it to the outgoing queue
    status = AppendOutgoingPkt(iCurPdu, iCurPduTimestamp);
    if (status != PVMFSuccess)
    {
        return status;
    }
    iCurPdu.Unbind();
    iCurPduTimestamp = 0;
    return PVMFSuccess;
}

PVMFStatus H223OutgoingChannel::AppendOutgoingPkt(OsclSharedPtr<PVMFMediaDataImpl>& pdu,
        PVMFTimestamp timestamp,
        OsclRefCounterMemFrag* fsi)
{
    PVMFSharedMediaDataPtr mediaData = PVMFMediaData::createMediaData(pdu, iMediaMsgMemoryPool);
    if (mediaData.GetRep() == NULL)
    {
        return PVMFErrNoMemory;
    }

    mediaData->setTimestamp(timestamp);
    if (fsi)
    {
        mediaData->setFormatSpecificInfo(*fsi);
    }
    void* memory_for_entry = iMediaDataEntryAlloc->allocate(sizeof(LCMediaDataEntry));
    if (!memory_for_entry)
    {
        // if couldn't allocate memory - leave
        return PVMFErrNoMemory;
    }
    LCMediaDataEntry* entry = OSCL_PLACEMENT_NEW(memory_for_entry, LCMediaDataEntry());
    entry->mediaData = mediaData;

    LCMediaDataEntry* first = entry;
    PVMFTimestamp lastTS = timestamp;
    if (lastMediaData)
    {
        first = lastMediaData->next;
        lastMediaData->next = entry;
        lastTS = lastMediaData->mediaData->getTimestamp();
    }
    lastMediaData = entry;
    entry->next = first;

    /* Adjust buffering parameters */
    if (iBufferMediaMs)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::AppendOutgoingPkt last ts=%d,cur ts=%d", lcn, lastTS, timestamp));
        /* Compute delta_t from last media data */
        int32 delta_t = timestamp - lastTS;
        if (delta_t < 0)
            delta_t += TIMESTAMP_MAX;
        iBufferMediaMs -= delta_t;
        iBufferMediaBytes -= mediaData->getFilledSize();
        if (iBufferMediaMs <= 0 || iBufferMediaBytes <= 0)
        {
            iBufferMediaMs = 0;
            iBufferMediaBytes = 0;
        }
    }
    return PVMFSuccess;
}

bool H223OutgoingChannel::GetNextPacket(PVMFSharedMediaDataPtr& aMediaData, PVMFStatus aStatus)
{

    if (!iMuxingStarted && aStatus == PVMFSuccess)
    {
        iMuxingStarted = true;
    }

    if (lastMediaData == NULL)
    {
        return false;
    }
    if ((aStatus == PVMFSuccess) && iPaused)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::GetNextPacket Logical channel paused.", lcn));
        return false;

    }
    if ((aStatus == PVMFSuccess) && iBufferMediaMs && iBufferMediaBytes)
    {
        /* Still buffering */
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::GetNextPacket Buffering ms left=%d", lcn, iBufferMediaMs));
        return false;
    }

    LCMediaDataEntry* first = lastMediaData->next;
    aMediaData = first->mediaData;

    if (first == lastMediaData)
    {
        lastMediaData = NULL;
    }
    else
    {
        lastMediaData->next = first->next;

    }


    first->~LCMediaDataEntry();
    iMediaDataEntryAlloc->deallocate(first);


    OSCL_ASSERT(iNumPendingPdus);
    iNumPendingPdus--;
    return true;
}

OsclAny H223OutgoingChannel::ReleasePacket(PVMFSharedMediaDataPtr& aMediaData)
{
    OsclSharedPtr<PVMFMediaDataImpl> aMediaDataImpl;
    aMediaData->getMediaDataImpl(aMediaDataImpl);
    aMediaDataImpl->clearMediaFragments();
}

OsclAny H223OutgoingChannel::Flush()
{
    //PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,"H223OutgoingChannel(%d)::Flush\n", lcn));

    PVMFSharedMediaDataPtr aMediaData;

    // clear messages in input queue
    ClearMsgQueues();
    // go through pending queue
    while (GetNextPacket(aMediaData, PVMFErrCancelled))
    {
        PV_STAT_INCR(iNumBytesFlushed, aMediaData->getFilledSize())
        OsclSharedPtr<PVMFMediaDataImpl> aMediaDataImpl;
        aMediaData->getMediaDataImpl(aMediaDataImpl);
        aMediaDataImpl->clearMediaFragments();
    }
    if (iCurPdu.GetRep())
    {
        iCurPdu->clearMediaFragments();
        iCurPdu.Unbind();
    }
    iCurPduTimestamp = 0;
    iNumPendingPdus = 0;
}
OsclAny H223OutgoingChannel::ResetStats()
{
    iNumPacketsIn = 0;
    iNumSdusIn = 0;
    iNumBytesIn = 0;
    iNumSdusDropped = 0;
    iNumSdusOut = 0;
    iNumBytesOut = 0;
    iMaxPacketMuxTime = 0;
    iMaxSduMuxTime = 0;
    iNumFlush = 0;
    iNumBytesFlushed = 0;
}

OsclAny H223OutgoingChannel::LogStats()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Outgoing Logical Channel %d Statistics:\n", lcn));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Adaptation layer header bytes -  %d\n", iAl->GetHdrSz()));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Adaptation layer trailer bytes -  %d\n", iAl->GetTrlrSz()));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num packets received - %d\n", iNumPacketsIn));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num sdus received - %d\n", iNumSdusIn));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num bytes received - %d\n", iNumBytesIn));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num sdus dropped - %d\n", iNumSdusDropped));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num sdus output - %d\n", iNumSdusOut));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num bytes output - %d\n", iNumBytesOut));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Max packet mux time - %d\n", iMaxPacketMuxTime));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Max sdu mux time - %d\n", iMaxSduMuxTime));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num flush - %d\n", iNumFlush));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num bytes flushed - %d\n", iNumBytesFlushed));
}

OSCL_EXPORT_REF PVMFStatus H223OutgoingChannel::Connect(PVMFPortInterface* aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::Connect, aPort=%x", lcn, aPort));

    if (iConnectedPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::Connect Error: Already connected", lcn));
        return PVMFFailure;
    }

    PvmiCapabilityAndConfig* config = NULL;

    OsclAny* tempInterface = NULL;
    aPort->QueryInterface(PVMI_CAPABILITY_AND_CONFIG_PVUUID, tempInterface);
    config = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, tempInterface);
    if (!config)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::Connect: Error - Peer port does not support capability interface", lcn));
        return PVMFFailure;
    }

    PVMFStatus status = NegotiateInputSettings(config);

    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::Connect: Error - Settings negotiation failed. status=%d", lcn, status));
        return status;
    }

    //Automatically connect the peer.
    if ((status = aPort->PeerConnect(this)) != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::Connect: Error - Peer Connect failed. status=%d", lcn, status));
        return status;
    }

    iConnectedPort = aPort;

    PortActivity(PVMF_PORT_ACTIVITY_CONNECT);
    return PVMFSuccess;
}
OSCL_EXPORT_REF PVMFStatus H223OutgoingChannel::PeerConnect(PVMFPortInterface* aPort)
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::PeerConnect aPort=0x%x", lcn, this, aPort));
    if (!aPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "0x%x H223OutgoingChannel(%d)::PeerConnect: Error - Connecting to invalid port", lcn, this));
        return PVMFErrArgument;
    }
    if (iConnectedPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "0x%x H223OutgoingChannel(%d)::PeerConnect: Error - Already connected", lcn, this));
        return PVMFFailure;
    }

    OsclAny* config = NULL;
    aPort->QueryInterface(PVMI_CAPABILITY_AND_CONFIG_PVUUID, config);
    if (!config)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223OutgoingChannel(%d)::PeerConnect: Error - Peer port does not support capability interface", lcn));
        return PVMFFailure;
    }


    PVMFStatus status = PVMFSuccess;

    status = NegotiateInputSettings((PvmiCapabilityAndConfig*)config);

    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223OutgoingChannel(%d)::PeerConnect: Error - Settings negotiation failed. status=%d", lcn, status));
        // Ignore errors for now
        status = PVMFSuccess;
    }


    iConnectedPort = aPort;
    PortActivity(PVMF_PORT_ACTIVITY_CONNECT);

    return status;
}

PVMFStatus H223OutgoingChannel::NegotiateInputSettings(PvmiCapabilityAndConfig* aConfig)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::NegotiateInputSettings, aConfig=%x", lcn, aConfig));

    PvmiKvp* kvp = NULL;
    int numParams = 0;
    int32 i = 0;

    // Get supported output formats from peer
    PVMFStatus status = aConfig->getParametersSync(NULL,
                        OSCL_CONST_CAST(char*, OUTPUT_FORMATS_CAP_QUERY),
                        kvp, numParams, NULL);
    if (status != PVMFSuccess || numParams == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223OutgoingChannel(%d)::NegotiateInputSettings, Error:  getParametersSync failed.  status=%d", lcn, status));
        return status;
    }

    PvmiKvp* selectedKvp = NULL;
    PVCodecType_t codec_type = GetCodecType(iDataType);
    PVMFFormatType lcn_format_type = PVCodecTypeToPVMFFormatType(codec_type);

    for (i = 0; i < numParams && !selectedKvp; i++)
    {
        if (lcn_format_type == kvp[i].value.pChar_value)
            selectedKvp = &(kvp[i]);
    }

    if (!selectedKvp)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223OutgoingChannel(%d)::NegotiateInputSettings, Error:  Input format not supported by peer", lcn));
        return PVMFFailure;
    }
    if (PVMFSuccess != setConfigParametersSync(selectedKvp, aConfig))
    {
        return PVMFFailure;
    }

    aConfig->releaseParameters(NULL, kvp, numParams);
    kvp = NULL;
    numParams = 0;
    return PVMFSuccess;
}
////////////////////////////////////////////////////////////////////////////
//                  PvmiCapabilityAndConfig
////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223OutgoingChannel::setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
{
    // Not supported
    OSCL_UNUSED_ARG(aObserver);
    OSCL_LEAVE(OsclErrNotSupported);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus H223OutgoingChannel::getParametersSync(PvmiMIOSession session,
        PvmiKeyType identifier,
        PvmiKvp*& parameters,
        int& num_parameter_elements,
        PvmiCapabilityContext context)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::getParametersSync", lcn));
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);

    parameters = NULL;
    num_parameter_elements = 0;

    if (pv_mime_strcmp(identifier, INPUT_FORMATS_CAP_QUERY) == 0)
    {
        num_parameter_elements = 1;
        PVMFStatus status = AllocateKvp(iKvpMemAlloc, parameters,
                                        OSCL_CONST_CAST(char*, INPUT_FORMATS_VALTYPE),
                                        num_parameter_elements);
        if (status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223OutgoingChannel(%d)::getParametersSync: Error - AllocateKvp failed. status=%d", lcn, status));
            return status;
        }
        PVCodecType_t codec_type = GetCodecType(iDataType);

        PVMFFormatType format = PVCodecTypeToPVMFFormatType(codec_type).getMIMEStrPtr();
        if (format == PVMF_MIME_AMR_IF2)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_AMR_IF2;
        else if (format == PVMF_MIME_PCM16)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_PCM16;
        else if (format ==  PVMF_MIME_YUV420)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_YUV420;
        else if (format ==  PVMF_MIME_M4V)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_M4V;
        else if (format ==  PVMF_MIME_H264_VIDEO_RAW)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_H264_VIDEO_RAW;
        else if (format ==  PVMF_MIME_H2632000)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_H2632000;
        else if (format ==  PVMF_MIME_H2631998)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_H2631998;
        else
            parameters[0].value.pChar_value = (char*) PVMF_MIME_FORMAT_UNKNOWN;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus H223OutgoingChannel::releaseParameters(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(num_elements);

    if (parameters)
    {
        iKvpMemAlloc.deallocate((OsclAny*)parameters);
        return PVMFSuccess;
    }
    else
    {
        return PVMFFailure;
    }
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223OutgoingChannel::createContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223OutgoingChannel::setContextParameters(PvmiMIOSession session,
        PvmiCapabilityContext& context,
        PvmiKvp* parameters, int num_parameter_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_parameter_elements);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223OutgoingChannel::DeleteContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223OutgoingChannel::setParametersSync(PvmiMIOSession session, PvmiKvp* parameters,
        int num_elements, PvmiKvp*& ret_kvp)
{
    OSCL_UNUSED_ARG(session);
    PVMFStatus status = PVMFSuccess;
    ret_kvp = NULL;

    for (int32 i = 0; i < num_elements; i++)
    {
        status = VerifyAndSetParameter(&(parameters[i]), true);
        if (status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223OutgoingChannel(%d)::setParametersSync: Error - VerifiyAndSetParameter failed on parameter #%d", lcn, i));
            ret_kvp = &(parameters[i]);
            OSCL_LEAVE(OsclErrArgument);
        }
    }
}

PVMFStatus H223OutgoingChannel::VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam)
{
    OSCL_UNUSED_ARG(aSetParam);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223OutgoingChannel(%d)::VerifyAndSetParameter: aKvp=0x%x, aSetParam=%d", lcn, aKvp, aSetParam));

    if (!aKvp)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223OutgoingChannel(%d)::VerifyAndSetParameter: Error - Invalid key-value pair", lcn));
        return PVMFFailure;
    }

    if (iDataType == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223OutgoingChannel(%d)::VerifyAndSetParameter: Error - DataType == NULL", lcn));
        return PVMFErrNotSupported;
    }

    if (pv_mime_strcmp(aKvp->key, INPUT_FORMATS_VALTYPE) == 0)
    {
        PVCodecType_t codec_type = GetCodecType(iDataType);
        PVMFFormatType lcn_format_type = PVCodecTypeToPVMFFormatType(codec_type);
        if (pv_mime_strcmp(lcn_format_type.getMIMEStrPtr(), aKvp->value.pChar_value) != 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223OutgoingChannel(%d)::VerifyAndSetParameter: Error - Input format %s not supported", lcn, aKvp->value.pChar_value));
            return PVMFErrNotSupported;
        }
    }
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId H223OutgoingChannel::setParametersAsync(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements,
        PvmiKvp*& ret_kvp,
        OsclAny* context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_elements);
    OSCL_UNUSED_ARG(ret_kvp);
    OSCL_UNUSED_ARG(context);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF uint32 H223OutgoingChannel::getCapabilityMetric(PvmiMIOSession session)
{
    OSCL_UNUSED_ARG(session);
    return 1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus H223OutgoingChannel::verifyParametersSync(PvmiMIOSession session,
        PvmiKvp* parameters, int num_elements)
{
    OSCL_UNUSED_ARG(session);

    PVMFStatus status = PVMFSuccess;
    for (int32 i = 0; (i < num_elements) && (status == PVMFSuccess); i++)
        status = VerifyAndSetParameter(&(parameters[i]), true);

    return status;
}

void H223OutgoingChannel::HandlePortActivity(const PVMFPortActivity &aActivity)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::HandlePortActivity(%d)", lcn, aActivity.iType));

    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_CREATED:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity created", lcn));
            return;
        case PVMF_PORT_ACTIVITY_DELETED:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity deleted", lcn));
            return;
        case PVMF_PORT_ACTIVITY_CONNECT:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity connect", lcn));
            return;
        case PVMF_PORT_ACTIVITY_DISCONNECT:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity disconnect", lcn));
            return;
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity outgoing msg", lcn));
            return;
        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity incoming msg", lcn));
            break;
        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_BUSY:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity outgoing queue busy", lcn));
            return;
        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_READY:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity outgoing queue ready", lcn));
            return;
        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_BUSY:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity connected port busy", lcn));
            return;
        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223OutgoingChannel(%d)::HandlePortActivity connected port ready", lcn));
            break;
        case PVMF_PORT_ACTIVITY_ERROR:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223OutgoingChannel(%d)::HandlePortActivity error", lcn));
            return;
    }

    PVMFStatus aStatus;
    PVMFSharedMediaMsgPtr aMsg;
    while (IncomingMsgQueueSize())
    {
        aStatus = DequeueIncomingMsg(aMsg);
        if (aStatus == PVMFSuccess)
        {

            DetectSkewInd(aMsg);

            PutData(aMsg);
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223OutgoingChannel(%d)::HandlePortActivity Failed to DeQueue incoming message: %d", lcn, aStatus));
            break;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::HandlePortActivity - out", lcn));
}

PVLOGGER_LOG_USE_ONLY(
    PVMFStatus H223OutgoingChannel::TestFsi(OsclRefCounterMemFrag aMemFrag)
{
    // get fsi
    uint8* pFsi = NULL;
    uint32 fsiLen = ::GetFormatSpecificInfo(iDataType, pFsi);

    if (iFsiTestIndex >= fsiLen)
    {
        return PVMFFailure;
    }

    // get ptr and size
    uint32 memFragSize = aMemFrag.getMemFragSize();

    // check if this is valid data
    if (memFragSize <= 1)
    {
        return PVMFFailure;
    }

    uint8* pMemFrag = OSCL_STATIC_CAST(uint8*, aMemFrag.getMemFragPtr());

    uint16 compSize = OSCL_STATIC_CAST(uint16, OSCL_MIN(fsiLen - iFsiTestIndex, memFragSize));

    // compare and print result if error found
    if (fsiLen && oscl_memcmp(pMemFrag, &pFsi[iFsiTestIndex], compSize))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223OutgoingChannel(%d)::TestFsi Format Specific Info conflict", lcn));
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::TestFsi FSI BS", lcn));
        for (int ii = 0; ii < compSize; ii++)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::TestFsi %#x %#x", lcn, pFsi[iFsiTestIndex + ii], pMemFrag[ii]));
        }

    }
    iFsiTestIndex += compSize;
    return PVMFSuccess;
}
)

OSCL_EXPORT_REF PVMFStatus H223OutgoingControlChannel::PeerConnect(PVMFPortInterface* aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingControlChannel(%d)::PeerConnect aPort=0x%x", lcn, this, aPort));
    if (!aPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "0x%x H223OutgoingControlChannel(%d)::PeerConnect: Error - Connecting to invalid port", lcn, this));
        return PVMFErrArgument;
    }
    if (iConnectedPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "0x%x H223OutgoingControlChannel(%d)::PeerConnect: Error - Already connected", lcn, this));
        return PVMFFailure;
    }

    iConnectedPort = aPort;
    PortActivity(PVMF_PORT_ACTIVITY_CONNECT);

    return PVMFSuccess;

}
PVMFStatus H223OutgoingControlChannel::PutData(PVMFSharedMediaMsgPtr aMsg)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingControlChannel(%d)::PutData - iNumPendingPdus=%u,iNumMediaData=%u\n", lcn, iNumPendingPdus, iNumMediaData));

    PVMFSharedMediaDataPtr mediaData;
    convertToPVMFMediaData(mediaData, aMsg);

    PV_STAT_SET_TIME(iStartTime, iNumPacketsIn)
    PV_STAT_INCR(iNumPacketsIn, 1)
    PV_STAT_INCR(iNumSdusIn, 1)
    PV_STAT_INCR(iNumBytesIn, (mediaData->getFilledSize()))

    OsclSharedPtr<PVMFMediaDataImpl> pdu;
    pdu = StartAlPdu();

    if (!pdu)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingControlChannel(%d)::PutData - Memory allocation failure on iMediaFragGroupAlloc->allocate\n", lcn));
        return PVMFErrNoMemory;
    }

    TimeValue timenow;
    OsclRefCounterMemFrag frag;
    for (uint frag_num = 0; frag_num < mediaData->getNumFragments(); frag_num++)
    {
        mediaData->getMediaFragment(frag_num, frag);
        // Add data fragments
        pdu->appendMediaFragment(frag);
    }

    PVMFStatus status = iAl->CompletePacket(pdu);
    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingControlChannel(%d)::PutData - Memory allocation failure on iAl->CompletePacket()", lcn));
        return status;
    }

    OsclRefCounterMemFrag fsi;
    bool fsi_available = mediaData->getFormatSpecificInfo(fsi);
    // Add it to the outgoing queue
    if (PVMFSuccess != AppendOutgoingPkt(pdu, timenow.to_msec(), (fsi_available ? &fsi : NULL)))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingControlChannel(%d)::PutData - Memory allocation failure on AppendOutgoingPkt()", lcn));
        return PVMFErrNoMemory;
    }
    return PVMFSuccess;
}

PVMFStatus H223LogicalChannel::setConfigParametersSync(PvmiKvp* selectedKvp,
        PvmiCapabilityAndConfig* aConfig,
        PVMFFormatType lcn_format_type,
        bool aTryTwice)
{
    int32 err = 0;
    PvmiKvp* retKvp = NULL;
    if (!aTryTwice)
    {
        OSCL_TRY(err, aConfig->setParametersSync(NULL, selectedKvp, 1, retKvp););
        OSCL_FIRST_CATCH_ANY(err,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223OutgoingChannel(%d)::setConfigParametersSync, Error:  setParametersSync failed, err=%d", lcn, err));
                             return PVMFFailure;
                            );
    }
    else
    {
        int32 err = 0;
        OSCL_TRY(err, aConfig->setParametersSync(NULL, selectedKvp, 1, retKvp););
        if (err)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::setConfigParametersSync, Error:  setParametersSync failed for pChar value, trying uint32, err=%d", lcn, err));
            selectedKvp->value.pChar_value = OSCL_STATIC_CAST(mbchar*, lcn_format_type.getMIMEStrPtr());
            err = 0;
            OSCL_TRY(err, aConfig->setParametersSync(NULL, selectedKvp, 1, retKvp););
            if (err)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::setConfigParametersSync, Error:  setParametersSync failed, err=%d", lcn, err));
                return PVMFFailure;
            }
        }
    }
    return PVMFSuccess;
}
H223IncomingChannel::H223IncomingChannel(TPVChannelId num,
        bool segmentable,
        OsclSharedPtr<AdaptationLayer>& al,
        PS_DataType data_type,
        LogicalChannelObserver* observer,
        uint32 bitrate,
        uint32 sample_interval,
        uint32 num_media_data)
        : H223LogicalChannel(num, segmentable, al, data_type, observer, bitrate, sample_interval, num_media_data),
        iMediaMsgMemoryPool(NULL),
        iMediaFragGroupAlloc(NULL),
        iPduPktMemPool(NULL),
        iMediaDataAlloc(&iMemAlloc),
        iPduSize(al->GetPduSize()),
        iCurPduSize(0),
        iVideoFrameNum(0),
        iMax_Chunk_Size(0),
        ipVideoFrameReszMemPool(NULL),
        ipVideoFrameAlloc(NULL),
        ipVideoDataMemPool(NULL),
        iLastAudioTS(0),
        iAudioFrameNum(0),
        ipAudioDtxTimer(NULL),
        iBosMessageSent(false)
{
#ifdef LIP_SYNC_TESTING
    iParam = ShareParams::Instance();
#endif

    iLogger = PVLogger::GetLoggerObject("3g324m.h223.H223IncomingChannel");
    iIncomingAudioLogger = PVLogger::GetLoggerObject("datapath.incoming.audio.h223.lcn");
    iIncomingVideoLogger = PVLogger::GetLoggerObject("datapath.incoming.video.h223.lcn");
    iAlPduFragPos = NULL;

    ResetStats();


}

H223IncomingChannel::~H223IncomingChannel()
{
    // clean audio dtx timer
    if (ipAudioDtxTimer)
    {
        ipAudioDtxTimer->Cancel(AMR_TIMER_ID);
        ipAudioDtxTimer->Clear();
        OSCL_DELETE(ipAudioDtxTimer);
    }

    iAudioDataQueue.clear();

    Flush();
    OsclRefCounterMemFrag frag;
    iAlPduFrag = frag;
    if (iMediaFragGroupAlloc)
    {
        iMediaFragGroupAlloc->removeRef();
    }
    if (iPduPktMemPool)
    {
        OSCL_DELETE(iPduPktMemPool);
    }
    if (iMediaMsgMemoryPool)
    {
        OSCL_DELETE(iMediaMsgMemoryPool);
    }
    if (iMediaType.isVideo())
    {
        if (ipVideoDataMemPool)
        {
            OSCL_DELETE(ipVideoDataMemPool);
        }
        if (ipVideoFrameReszMemPool)
        {
            ipVideoFrameReszMemPool->removeRef();
            ipVideoFrameReszMemPool = NULL;
        }
        if (ipVideoFrameAlloc)
        {
            OSCL_DELETE(ipVideoFrameAlloc);
        }
    }
}

void H223IncomingChannel::Init()
{
    OsclSharedPtr<PVMFMediaDataImpl> tempImpl;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::Init", lcn));
    H223LogicalChannel::Init();

    int bitrate = (GetBitrate() > 0) ? GetBitrate() : DEF_CHANNEL_BITRATE;
    int mem_size = (DEF_INCOMING_CHANNEL_BUFFER_SIZE_MS * bitrate) >> PV2WAY_BPS_TO_BYTES_PER_MSEC_RIGHT_SHIFT;
    int num_fragments = (mem_size / H223_INCOMING_CHANNEL_FRAGMENT_SIZE) + 1;
    iMediaMsgMemoryPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (H223_INCOMING_CHANNEL_NUM_MEDIA_MSG));
    if (iMediaMsgMemoryPool == NULL)
    {
        OSCL_LEAVE(PVMFErrNoMemory);
    }
    iMediaMsgMemoryPool->enablenullpointerreturn();

    iPduPktMemPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (H223_INCOMING_CHANNEL_NUM_MEDIA_DATA));
    if (iPduPktMemPool == NULL)
    {
        OSCL_LEAVE(PVMFErrNoMemory);
    }
    iPduPktMemPool->enablenullpointerreturn();

    iMediaFragGroupAlloc = OSCL_NEW(PVMFMediaFragGroupCombinedAlloc<OsclMemAllocator>, (H223_INCOMING_CHANNEL_NUM_MEDIA_DATA, H223_INCOMING_CHANNEL_NUM_FRAGS_IN_MEDIA_DATA, iPduPktMemPool));
    if (iMediaFragGroupAlloc == NULL)
    {
        OSCL_LEAVE(PVMFErrNoMemory);
    }
    iMediaFragGroupAlloc->create();

    iMemFragmentAlloc.SetLeaveOnAllocFailure(false);
    iMemFragmentAlloc.size((uint16)num_fragments, (uint16)H223_INCOMING_CHANNEL_FRAGMENT_SIZE);
    if (iMediaType.isVideo())
    {
        ipVideoDataMemPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (MAX_VIDEO_FRAMES, PVVIDEOPARSER_MEDIADATA_SIZE));
        if (NULL == ipVideoDataMemPool)
        {
            OSCL_LEAVE(PVMFErrNoMemory);
        }

        ipVideoFrameReszMemPool = OSCL_NEW(OsclMemPoolResizableAllocator, (MAX_VIDEO_FRAME_PARSE_SIZE, MAX_MEMPOOL_BUFFER_NUM_LIMIT));
        if (NULL == ipVideoFrameReszMemPool)
        {
            OSCL_LEAVE(PVMFErrNoMemory);
        }
        ipVideoFrameAlloc  = OSCL_NEW(PVMFResizableSimpleMediaMsgAlloc, (ipVideoFrameReszMemPool));
        if (NULL == ipVideoFrameAlloc)
        {
            OSCL_LEAVE(PVMFErrNoMemory);
        }
        tempImpl = ipVideoFrameAlloc->allocate(MAX_VIDEO_FRAME_PARSE_SIZE);
        if (!tempImpl)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::Init(), tempImpl Allocation failed\n", lcn));
            OSCL_LEAVE(PVMFErrNoMemory);
        }

        iVideoFrame = PVMFMediaData::createMediaData(tempImpl, ipVideoDataMemPool);
        if (iVideoFrame.GetRep() == NULL)
        {
            OSCL_LEAVE(PVMFErrNoMemory);
        }
    }
    else if (iMediaType.isAudio())
    {
        PVMFStatus err = PVMFNotSet;
        typedef OsclTimer<OsclMemAllocator> timerType;
        OSCL_TRY(err, ipAudioDtxTimer = OSCL_NEW(timerType, ("IncomingChannelDTXTimer")););
        // only log the error, we can do without the timer if
        OSCL_FIRST_CATCH_ANY(err,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223IncomingChannel(%d)::Init(), Error:  IncomingChannelDTXTimer allocation failed, err=%d", lcn, err));
                            );
        if (ipAudioDtxTimer)
        {
            ipAudioDtxTimer->SetExactFrequency(AMR_TIMER_FREQUENCY);
            ipAudioDtxTimer->SetObserver(this);
        }
    }

    ResetAlPdu();
    AllocateAlPdu();
    iCurTimestamp = 0;
    iNumSdusIn = 0;
    iNumPdusIn = 0;
}

OsclAny H223IncomingChannel::ResetAlPdu()
{
    iAlPduFragPos = NULL;
    iCurPduSize = 0;
    OsclRefCounterMemFrag frag;
    iAlPduFrag = frag;
    iAlPduMediaData.Unbind();
}

OsclAny H223IncomingChannel::AllocateAlPdu()
{
    iAlPduMediaData = iMediaFragGroupAlloc->allocate();
    if (iAlPduMediaData.GetRep() == NULL)
    {
        return;
    }
    AppendAlPduFrag();
}

OsclAny H223IncomingChannel::AppendAlPduFrag()
{
    OsclRefCounterMemFrag ref_counter_mem_frag = iMemFragmentAlloc.get();
    if (ref_counter_mem_frag.getMemFragPtr() == NULL)
    {
        return;
    }
    ref_counter_mem_frag.getMemFrag().len = 0;
    iAlPduFrag = ref_counter_mem_frag;
    iAlPduFragPos = (uint8*)iAlPduFrag.getMemFragPtr();
}

uint32 H223IncomingChannel::CopyAlPduData(uint8* buf, uint16 len)
{
    int32 remaining = len;
    uint32 copied = 0;
    do
    {
        copied = CopyToCurrentFrag(buf, (uint16)remaining);
        buf += copied;
        remaining -= copied;
    }
    while (remaining && copied);
    return (uint32)(len - remaining);
}

uint32 H223IncomingChannel::CopyToCurrentFrag(uint8* buf, uint16 len)
{
    if (iAlPduMediaData.GetRep() == NULL)
    {
        AllocateAlPdu();
    }
    else if (iAlPduFragPos == NULL)
    {
        AppendAlPduFrag();
    }
    if (iAlPduFragPos == NULL)
    {
        return 0;
    }

    uint32 space_in_current_frag = ((uint8*)iAlPduFrag.getMemFragPtr() + iAlPduFrag.getCapacity() - iAlPduFragPos);
    OSCL_ASSERT(space_in_current_frag > 0);
    uint32 num_bytes_copied = (len > space_in_current_frag) ? space_in_current_frag : len;
    oscl_memcpy(iAlPduFragPos, buf, num_bytes_copied);
    iAlPduFrag.getMemFrag().len += num_bytes_copied;
    iAlPduFragPos += num_bytes_copied;
    space_in_current_frag -= num_bytes_copied;
    if (space_in_current_frag == 0)
    {
        iAlPduMediaData->appendMediaFragment(iAlPduFrag);
        iAlPduFragPos = NULL;
    }
    PVLOGGER_LOG_USE_ONLY(
        if (iAlPduMediaData->getFilledSize() > iPduSize)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::CopyToCurrentFrag WARNING current pdu size=%d > iPduSize=%d", lcn, iAlPduMediaData->getFilledSize(), iPduSize));
    }
    );
    return num_bytes_copied;
}

void H223IncomingChannel::PreAlPduData()

{
    if (iPaused)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::PreAlPduData Logical channel paused.  Dropping media data.", lcn));
        return;
    }
}

PVMFStatus H223IncomingChannel::AlPduData(uint8* buf, uint16 len)
{
    PV_STAT_INCR(iNumBytesIn, len)
    PV_STAT_SET_TIME(iStartTime, iNumBytesIn)

    PreAlPduData();

    if (iAlPduMediaData.GetRep() == NULL || iAlPduMediaData->getFilledSize() == 0)
    {
        if (lcn != 0)
        {
            UpdateCurrentTimestamp();
        }
    }
    uint32 copied = CopyAlPduData(buf, len);
    return (copied == len) ? PVMFSuccess : PVMFFailure;
}

PVMFStatus H223IncomingChannel::AlDispatch()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::AlDispatch, iCurPduSize=%d, sn=%d", lcn, iCurPduSize, iNumSdusIn));
    IncomingALPduInfo info;

    if (iPaused)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::AlDispatch Logical channel paused.", lcn));
        return PVMFFailure;
    }

    /*  Nothing to dispatch */
    if (!iAlPduMediaData.GetRep())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::AlDispatch Nothing to dispatch.", lcn));
        ResetAlPdu();
        return PVMFSuccess;
    }

    /*  Add pending fragment to the media message */
    if (iAlPduFragPos)
    {
        iAlPduMediaData->appendMediaFragment(iAlPduFrag);
    }

    PVMFFormatType mediaType = GetFormatType();
#ifdef LIP_SYNC_TESTING
    if ((mediaType.isVideo() && iParam->iCompressed == false))
    {
        ExtractTimestamp();
    }
#endif

    /* Parse AL headers etc */
    iAl->ParsePacket(iAlPduMediaData, info);
    int32 len = info.sdu_size;
    if (len <= 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::AlDispatch Empty SDU len=%d", lcn, len));
        ResetAlPdu();
        return PVMFErrCorrupt;
    }

    PV_STAT_INCR_COND(iNumCrcErrors, 1, info.crc_error)
    PV_STAT_INCR_COND(iNumSeqNumErrors, 1, info.seq_num_error)
    PVMFStatus status = PVMFSuccess;

    // set the errors flag
    uint32 errorsFlag = 0;
    if (info.crc_error)
    {
        errorsFlag |= PVMF_MEDIA_DATA_BIT_ERRORS;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::AlDispatch CRC error size=%d", lcn, len));
        status =  PVMFErrCorrupt;
    }
    else
    {
        PV_STAT_INCR(iNumSdusIn, 1)
    }
    if (info.seq_num_error)
    {
        errorsFlag |= PVMF_MEDIA_DATA_PACKET_LOSS;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::AlDispatch Sequence number error size=%d", lcn, len));
        status = PVMFErrCorrupt;
    }

    OsclSharedPtr<PVMFMediaData> aMediaData = PVMFMediaData::createMediaData(iAlPduMediaData, iMediaMsgMemoryPool);
    if (aMediaData.GetRep() == NULL)
    {
        return PVMFErrNoMemory;
    }

    aMediaData->setTimestamp(iCurTimestamp);
    aMediaData->setSeqNum(info.seq_num);
    iAlPduMediaData->setErrorsFlag(errorsFlag);
    status = DispatchOutgoingMsg(aMediaData);

    ResetAlPdu();
    AllocateAlPdu();
    return status;
}

OsclAny H223IncomingChannel::Flush()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::Flush()", lcn));

    PV_STAT_INCR(iNumBytesFlushed, (iAlPduFragPos - (uint8*)iAlPduFrag.getMemFragPtr()))
    PV_STAT_INCR_COND(iNumAbort, 1, (iAlPduFragPos - (uint8*)iAlPduFrag.getMemFragPtr()))
    iVideoFrame.Unbind();
    ResetAlPdu();
    iAudioDataQueue.clear();
}

PVMFStatus H223IncomingChannel::Connect(PVMFPortInterface* aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::Connect", lcn));
    if (iConnectedPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::Connect Error: Already connected", lcn));
        return PVMFFailure;
    }


    PvmiCapabilityAndConfig* config = NULL;
    OsclAny * tempInterface = NULL;
    aPort->QueryInterface(PVMI_CAPABILITY_AND_CONFIG_PVUUID, tempInterface);
    config = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, tempInterface);
    if (!config)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::Connect: Error - Peer port does not support capability interface", lcn));
        return PVMFFailure;
    }

    PVMFStatus status = NegotiateOutputSettings(config);

    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::Connect: Error - Settings negotiation failed. status=%d", lcn, status));
        return status;
    }

    if (config != NULL)
    {
        if (!(pvmiSetPortFormatSpecificInfoSync(OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, config), PVMF_FORMAT_SPECIFIC_INFO_KEY)))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO
                            , (0, "H223IncomingChannel(%d)::Connect: Error - Unable To Send Format Specific Info To Peer", lcn));
            return PVMFFailure;
        }
    }
    //Automatically connect the peer.
    if ((status = aPort->PeerConnect(this)) != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::Connect: Error - Peer Connect failed. status=%d", lcn, status));
        return status;
    }


    iConnectedPort = aPort;


    //Check the BOS command status
    status = SendBeginOfStreamMediaCommand();

    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::Connect: Failed to send BOS message status=%d", lcn, status));
        return status;
    }

    PortActivity(PVMF_PORT_ACTIVITY_CONNECT);

    return PVMFSuccess;
}

void H223IncomingChannel::Pause()
{
    if (iPaused)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "H223IncomingChannel(%d)::Pause - Already paused", lcn));
        return;
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "H223IncomingChannel(%d)::Pause", lcn));
    H223LogicalChannel::Pause();
    iBosMessageSent = false;
}

void H223IncomingChannel::Resume()
{
    if (!iPaused)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "H223IncomingChannel(%d)::Resume - Already resumed", lcn));
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "H223IncomingChannel(%d)::Resume", lcn));
    H223LogicalChannel::Resume();
    SendBeginOfStreamMediaCommand();
}

OsclAny H223IncomingChannel::ResetStats()
{
    iNumSdusIn = 0;
    iNumBytesIn = 0;
    iSduSizeExceededCnt = 0;
    iNumCrcErrors = 0;
    iNumSeqNumErrors = 0;
    iNumBytesFlushed = 0;
    iNumAbort = 0;
}

OsclAny H223IncomingChannel::LogStats()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Incoming Logical Channel %d Statistics:\n", lcn));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num sdus received - %d\n", iNumSdusIn));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num bytes received - %d\n", iNumBytesIn));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num times sdu size exceeded - %d\n", iSduSizeExceededCnt));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num sdus with CRC errors - %d\n", iNumCrcErrors));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num sdus with sequence number errors - %d\n", iNumSeqNumErrors));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num sdus aborted - %d\n", iNumAbort));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "Num bytes aborted - %d\n", iNumBytesFlushed));
}

MuxSduData::MuxSduData()
{
    size = 0;
    cur_frag_num = 0;
    cur_pos = 0;
}
PVMFStatus H223IncomingChannel::NegotiateOutputSettings(PvmiCapabilityAndConfig* aConfig)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::NegotiateInputSettings, aConfig=%x", lcn, aConfig));

    PvmiKvp* kvp = NULL;
    int numParams = 0;
    int32 i = 0;

    // Get supported input formats from peer
    PVMFStatus status = aConfig->getParametersSync(NULL,
                        OSCL_CONST_CAST(char*, INPUT_FORMATS_CAP_QUERY),
                        kvp, numParams, NULL);

    if (status != PVMFSuccess)
    {
        status = aConfig->getParametersSync(NULL,
                                            OSCL_CONST_CAST(char*, "x-pvmf/video/decode/input_formats"),
                                            kvp, numParams, NULL);
    }

    if (status != PVMFSuccess || numParams == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::NegotiateInputSettings, Error:  getParametersSync failed.  status=%d", lcn, status));

        return PVMFSuccess;
    }

    PvmiKvp* selectedKvp = NULL;
    PVCodecType_t codec_type = GetCodecType(iDataType);
    PVMFFormatType lcn_format_type = PVCodecTypeToPVMFFormatType(codec_type);

    for (i = 0; i < numParams && !selectedKvp; i++)
    {
        if (lcn_format_type == kvp[i].value.pChar_value)
            selectedKvp = &(kvp[i]);
    }

    if (!selectedKvp)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::NegotiateInputSettings, Error:  Input format not supported by peer", lcn));
        return PVMFFailure;
    }

    /* This is for the silly problem of MIO components having the convention
       of returning uint32 for a query and requiring pChar for a setting
       we don't know if we are talking to an MIO or a decoder node
       (which will want a uint32), so we try both.  Try the pchar
       first, because if its expecting pchar and gets uint32, it will
       crash.
    */
    if (PVMFSuccess != setConfigParametersSync(selectedKvp, aConfig, lcn_format_type, true))
        return PVMFFailure;

    aConfig->releaseParameters(NULL, kvp, numParams);
    kvp = NULL;
    numParams = 0;

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
//                  PvmiCapabilityAndConfig
////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223IncomingChannel::setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
{
    // Not supported
    OSCL_UNUSED_ARG(aObserver);
    OSCL_LEAVE(OsclErrNotSupported);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus H223IncomingChannel::getParametersSync(PvmiMIOSession session,
        PvmiKeyType identifier,
        PvmiKvp*& parameters,
        int& num_parameter_elements,
        PvmiCapabilityContext context)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::getParametersSync", lcn));
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);

    parameters = NULL;
    num_parameter_elements = 0;


    if (pv_mime_strcmp(identifier, OUTPUT_FORMATS_CAP_QUERY) == 0)
    {
        num_parameter_elements = 1;
        PVMFStatus status = AllocateKvp(iKvpMemAlloc, parameters,
                                        OSCL_CONST_CAST(char*, OUTPUT_FORMATS_VALTYPE),
                                        num_parameter_elements);
        if (status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::getParametersSync: Error - AllocateKvp failed. status=%d", lcn, status));
            return status;
        }
        PVCodecType_t codec_type = GetCodecType(iDataType);
        PVMFFormatType format = PVCodecTypeToPVMFFormatType(codec_type).getMIMEStrPtr();
        if (format == PVMF_MIME_AMR_IF2)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_AMR_IF2;
        else if (format == PVMF_MIME_PCM16)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_PCM16;
        else if (format ==  PVMF_MIME_YUV420)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_YUV420;
        else if (format ==  PVMF_MIME_M4V)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_M4V;
        else if (format ==  PVMF_MIME_H264_VIDEO_RAW)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_H264_VIDEO_RAW;
        else if (format ==  PVMF_MIME_H2632000)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_H2632000;
        else if (format ==  PVMF_MIME_H2631998)
            parameters[0].value.pChar_value = (char*) PVMF_MIME_H2631998;
        else
            parameters[0].value.pChar_value = (char*) PVMF_MIME_FORMAT_UNKNOWN;

    }



    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus H223IncomingChannel::releaseParameters(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(num_elements);

    if (parameters)
    {
        iKvpMemAlloc.deallocate((OsclAny*)parameters);
        return PVMFSuccess;
    }
    else
    {
        return PVMFFailure;
    }
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223IncomingChannel::createContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223IncomingChannel::setContextParameters(PvmiMIOSession session,
        PvmiCapabilityContext& context,
        PvmiKvp* parameters, int num_parameter_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_parameter_elements);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223IncomingChannel::DeleteContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void H223IncomingChannel::setParametersSync(PvmiMIOSession session, PvmiKvp* parameters,
        int num_elements, PvmiKvp*& ret_kvp)
{
    OSCL_UNUSED_ARG(session);
    PVMFStatus status = PVMFSuccess;
    ret_kvp = NULL;

    for (int32 i = 0; i < num_elements; i++)
    {
        status = VerifyAndSetParameter(&(parameters[i]), true);
        if (status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::setParametersSync: Error - VerifiyAndSetParameter failed on parameter #%d", lcn, i));
            ret_kvp = &(parameters[i]);
            /* Silently ignore unrecognized codecs untill CapEx is supported by peer */
            //OSCL_LEAVE(OsclErrArgument);
        }
    }
}

PVMFStatus H223IncomingChannel::VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam)
{
    OSCL_UNUSED_ARG(aSetParam);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::VerifyAndSetParameter: aKvp=0x%x, aSetParam=%d", lcn, aKvp, aSetParam));

    if (!aKvp)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::VerifyAndSetParameter: Error - Invalid key-value pair", lcn));
        return PVMFFailure;
    }

    if (iDataType == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::VerifyAndSetParameter: Error - DataType == NULL", lcn));
        return PVMFErrNotSupported;
    }

    if (pv_mime_strcmp(aKvp->key, OUTPUT_FORMATS_VALTYPE) == 0)
    {
        PVCodecType_t codec_type = GetCodecType(iDataType);
        PVMFFormatType lcn_format_type = PVCodecTypeToPVMFFormatType(codec_type);
        if (lcn_format_type != aKvp->value.pChar_value)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::VerifyAndSetParameter: Error - Output format %s not supported", lcn, aKvp->value.pChar_value));
            return PVMFErrNotSupported;
        }
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId H223IncomingChannel::setParametersAsync(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements,
        PvmiKvp*& ret_kvp,
        OsclAny* context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_elements);
    OSCL_UNUSED_ARG(ret_kvp);
    OSCL_UNUSED_ARG(context);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF uint32 H223IncomingChannel::getCapabilityMetric(PvmiMIOSession session)
{
    OSCL_UNUSED_ARG(session);
    return 1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus H223IncomingChannel::verifyParametersSync(PvmiMIOSession session,
        PvmiKvp* parameters, int num_elements)
{
    OSCL_UNUSED_ARG(session);

    PVMFStatus status = PVMFSuccess;
    for (int32 i = 0; (i < num_elements) && (status == PVMFSuccess); i++)
        status = VerifyAndSetParameter(&(parameters[i]), true);

    return status;
}

void H223IncomingChannel::HandlePortActivity(const PVMFPortActivity &aActivity)
{
    if (aActivity.iType != PVMF_PORT_ACTIVITY_OUTGOING_MSG &&
            aActivity.iType != PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::HandlePortActivity Unhandled port activity: %d", lcn, aActivity.iType));
        return;
    }
    PVMFStatus aStatus;
    PVMFSharedMediaMsgPtr aMsg;
    while (OutgoingMsgQueueSize())
    {
        aStatus = Send();
        if (aStatus != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::HandlePortActivity Failed to DeQueue incoming message: %d", lcn, aStatus));
            break;
        }
    }
}

PVMFStatus H223IncomingChannel::SendBeginOfStreamMediaCommand()
{
    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();
    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_BOS_FORMAT_ID);

    sharedMediaCmdPtr->setTimestamp(iCurTimestamp);

    uint32 seqNum = 0;
    uint32 streamID = 0;
    sharedMediaCmdPtr->setSeqNum(seqNum);

    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);
    mediaMsgOut->setStreamID(streamID);

    PVMFStatus status = QueueOutgoingMsg(mediaMsgOut);
    if (status != PVMFSuccess)
    {
        // Output queue is busy, so wait for the output queue being ready
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                        (0, "H223IncomingChannel(%d)::SendBeginOfMediaStreamCommand: Outgoing queue busy. ", lcn));
        return status;
    }
    iBosMessageSent = true;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::SendBeginOfMediaStreamCommand() BOS Sent StreamId %d ", lcn, streamID));
    return status;
}

/* *****************************************************************************************************
*  Function : DetectSkewInd()
*  Date     : 6/15/2009
*  Purpose  : The purpose of this function is to calculate skew (difference b/w audio and video TS)
*              on stack outgoing side.Audio and Video both sending TS through observer and we are calculating
                actual skew in cpvh223multiplex.cpp. Function name CalculateSkew()
*  INPUT    : aMsg
*  Return   : NONE
*
*
* *************************************************************************************************/
void H223OutgoingChannel::DetectSkewInd(PVMFSharedMediaMsgPtr aMsg)
{

    PVMFFormatType mediaType = GetFormatType();
    uint32 audio = 0;
    uint32 video = 0;
    if (mediaType.isCompressed() && (mediaType.isAudio() || mediaType.isVideo()))
    {
        if (mediaType.isCompressed() && mediaType.isAudio())
        {
            audio = (uint32)aMsg->getTimestamp();
            iObserver->CalculateSkew(lcn, 0, audio, 0);

        }
        else if (mediaType.isCompressed() && mediaType.isVideo())
        {
            video = (uint32)aMsg->getTimestamp();
            iObserver->CalculateSkew(lcn, 1, video, 0);
            CalcRMSInfo(video, audio);

        }
    }
}
/* *****************************************************************************************************
*  Function : CalcRMSInfo()
*  Date     : 6/15/2009
*  Purpose  : The purpose of this function is to Calculate skew on stack outgoing side.In this function we are
               reciving the video and audio TS. The main thing is that we are calculting the skew, when we
               recieves the video TS with last Audio TS.This function is also for lip-sync calculation
*  INPUT    : VideoData,AudioData
*  Return   : NONE
*
*
* *************************************************************************************************/
void H223OutgoingChannel::CalcRMSInfo(uint32 aVideoData, uint32 aAudioData)
{

    if (iOnlyOnce == false)
    {

        uint32 tick_ct_val = OsclTickCount::TickCount();
        iTsmsec = OsclTickCount::TicksToMsec(tick_ct_val);
        iOnlyOnce = true;
    }

    int Diff = aVideoData - iTsmsec;
    if (Diff >= 60000)
    {
        /*End Of TIMER EOT=1*/

        iObserver->CalculateSkew(lcn, 0, 0, 1);
    }



}


#ifdef LIP_SYNC_TESTING

/* *****************************************************************************************************
*  Function : CalculateRMSInfo()
*  Date     : 6/15/2009
*  Purpose  : The purpose of this function is to Calculate skew on stack incoming side.In this function we are
               reciving the video and audio TS. The main thing is that we are calculting the skew, when we
               recieves the video TS with last Audio TS.
*  INPUT    : VideoData,AudioData
*  Return   : NONE
*
*
* *************************************************************************************************/
void H223IncomingChannel::CalculateRMSInfo(uint32 aVideoData, uint32 aAudioData)
{
    g_DiffIncmVidAudTS = aVideoData - aAudioData;
    g_iSqrIncCalVidAudTS += (g_DiffIncmVidAudTS * g_DiffIncmVidAudTS);

    if (g_OnlyOneTime)
    {
        uint32 tick_ct_val;
        tick_ct_val = OsclTickCount::TickCount();
        g_Incmtsmsec = OsclTickCount::TicksToMsec(tick_ct_val);
        g_OnlyOneTime = 0;
    }



    int Diff = aVideoData - g_Incmtsmsec;
    if (Diff >= 60000)
    {
        if (g_TotalCountIncm == 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "Not be able to calculate RMS because Total count value is zero  TotalCount=%d", lcn, g_TotalCountIncm));

        }
        else
        {
            g_IncRtMnSqCalc = (int32)sqrt(g_iSqrIncCalVidAudTS / g_TotalCountIncm);
        }

    }
    g_TotalCountIncm++;

}

/* *****************************************************************************************************
*  Function : DetectFrameBoundary()
*  Date     : 6/15/2009
*  Purpose  : The purpose of this function is to Detect frame boundary that whether the format is MPEG-4 or H263,
               In this function we are sending  original VideoTS to dummy output MIO node through observer.
               The reason is we dont want to pass the same to specific decoder, as decoder will modify the TS.
               This is we are doing for calculating lip-sync skew.

*  INPUT    : buf,aTimestamp
*  Return   : NONE
*
*
* *************************************************************************************************/
void H223IncomingChannel::DetectFrameBoundary(uint8* aBuf, uint32 aTimestamp)
{
    PVMFFormatType mediaType = GetFormatType();

    /********************This is for MPEG-4 Header and H263 Header***********************************/
    if ((mediaType.isCompressed() && (mediaType.isAudio() || mediaType.isVideo())))
    {

        if (mediaType.isCompressed() && mediaType.isAudio())
        {
            g_IncmAudioTS = *(((uint32*)aBuf));

        }

        if (mediaType.isCompressed() && mediaType.isVideo())
        {
            /*This check is for VOL header*/
            if ((aBuf[0] == VOP_START_BYTE_1) &&
                    (aBuf[1] == VOP_START_BYTE_2) &&
                    (aBuf[2] == VOP_START_BYTE_3) &&
                    (aBuf[3] == VOP_START_BYTE_5))
            {

                g_IncmVideoTS = g_RecTimeStamp;
                CalculateRMSInfo(g_IncmVideoTS, g_IncmAudioTS);
                iParam->iObserver->MIOFramesTimeStamps(0, g_RecTimeStamp, aTimestamp);

            }
            /*This Check is for MP4 Header*/
            else if ((aBuf[0] == VOP_START_BYTE_1) &&
                     (aBuf[1] == VOP_START_BYTE_2) &&
                     (aBuf[2] == VOP_START_BYTE_3) &&
                     (aBuf[3] == VOP_START_BYTE_4))
            {

                g_IncmVideoTS = g_RecTimeStamp;
                CalculateRMSInfo(g_IncmVideoTS, g_IncmAudioTS);
                iParam->iObserver->MIOFramesTimeStamps(0, g_RecTimeStamp, aTimestamp);



            }
            /*This check is for H263 Header*/
            else if ((aBuf[0] == H263_START_BYTE_1) &&
                     (aBuf[1] == H263_START_BYTE_2) &&
                     (aBuf[2] == H263_START_BYTE_3))
            {

                g_IncmVideoTS = *(((uint32*)aBuf) + 1);
                CalculateRMSInfo(g_IncmVideoTS, g_IncmAudioTS);



            }

        }

    }

}
/* *****************************************************************************************************
*  Function : ExtractTimestamp()
*  Date     : 6/15/2009
*  Purpose  : The purpose of this function is to extract original TS on stack incoming side.So for each SDU's
               we are extracting original video TS.This also we are doing for uncompressed video.Later we will
               add the same functionality for uncompressed audio only.This functionality is also for lip-sync
               detection.
*  INPUT    : NONE
*  Return   : NONE
*
*
* ********************************************************************************************************/
void H223IncomingChannel::ExtractTimestamp()
{
    PVMFFormatType mediaType = GetFormatType();
    if (lcn != 0)
    {
        if (mediaType.isCompressed() && mediaType.isVideo())
        {
            OsclRefCounterMemFrag MemFrag;
            OsclRefCounterMemFrag tsFrag;
            OsclRefCounterMemFrag frag;
            uint32 FragSize = 0;
            uint8* CheckTSbuff = 0;
            for (uint32 i = 0; i < iAlPduMediaData->getNumFragments(); i++)
            {
                iAlPduMediaData->getMediaFragment(i, frag);
                uint8* buf = (uint8*)frag.getMemFragPtr();
                FragSize = frag.getMemFragSize();
                CheckTSbuff = (uint8*)(buf + FragSize);
            }
            uint8* TimeStamp = (uint8*)(CheckTSbuff - 4);
            g_RecTimeStamp = (uint32)(*((uint32*)TimeStamp));
            iAlPduMediaData->getMediaFragment(0, tsFrag);
            iAlPduMediaData->setMediaFragFilledLen(iAlPduMediaData->getNumFragments() - 1, FragSize - 4);

        }
    }

}


/* *******************************************************************************************
*  Function : AppendLipSyncTS()
*  Date     : 6/15/2009
*  Purpose  : The purpose of this function is to append four bytes(Video TS)
*               on every sdu's.Here we are doing for Video only.this is
*              we are doing for to get original Video TS at stack incoming side.
*
*  INPUT    : NONE
*  Return   : NONE
*
*
* *********************************************************************************************/
void  H223OutgoingChannel::AppendLipSyncTS()
{


    if (iMediaType.isCompressed() && iMediaType.isVideo())
    {
        PVMFSharedMediaDataPtr hdrTSData;
        OsclRefCounterMemFrag tsFrag;
        OsclSharedPtr<PVMFMediaDataImpl> addTsImpl =  iMediaDataAlloc.allocate(4);
        hdrTSData = PVMFMediaData::createMediaData(addTsImpl);

        hdrTSData->getMediaFragment(0, tsFrag);
        tsFrag.getMemFrag().len = 4;
        uint32* SendBuff = (uint32*) tsFrag.getMemFragPtr();
        SendBuff[0] = iCurPduTimestamp;
        iCurPdu->appendMediaFragment(tsFrag);

    }


}
#endif
/* ***********************************************************************************
*  Function : DispatchOutgoingMsg()
*  Date     : 9/11/2009
*  Purpose  : The purpose of this function is to dispatch Audio and Video data.
*             Basically for the first new fragment (In Video Case) we are checking
*             the frame boundary, and buffering the video fragments in iVideoFrame
*             Buffer, until we completed one full video frame.After completing one
*             Video frame, we are dispatching the same to next lower node (Decoder).
*
*
*  INPUT    : aMediaData
*  Return   : PVMFStatus
*
*
* **********************************************************************************/

PVMFStatus H223IncomingChannel::DispatchOutgoingMsg(PVMFSharedMediaDataPtr aMediaData)
{

    OsclRefCounterMemFrag memFrag;
    OsclRefCounterMemFrag curVideoFrag;
    OsclSharedPtr<PVMFMediaMsg> mediaMsg;
    PVMFStatus status = PVMFSuccess;

    if (iMediaType.isVideo())
    {

        if (iVideoFrame.GetRep() == NULL)
        {
            return PVMFFailure;
        }


        aMediaData->getMediaFragment(0, memFrag);


        OSCL_ASSERT(memFrag.getMemFragSize() > 0);

        if (CheckFrameBoundary((uint8 *)memFrag.getMemFragPtr(), memFrag.getMemFragSize(), aMediaData->getErrorsFlag()))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223IncomingChannel(%d)::::DispatchOutgoingMsg, frame found\n", lcn));
            //If buffer exists then send it.
            if (iVideoFrame.GetRep() && iVideoFrame->getFilledSize())
            {
                CallSendVideoFrame(aMediaData);
                iVideoFrame->setTimestamp(aMediaData->getTimestamp());

            }

        }

        for (uint32 i = 0; i < aMediaData->getNumFragments(); i++)
        {
            aMediaData->getMediaFragment(i, memFrag);


            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223IncomingChannel(%d)::::DispatchOutgoingMsg, frag size %d, idx %d\n", lcn, memFrag.getMemFragSize(), i));



            //Make sure it can fit in the video buffer.
            if (memFrag.getMemFragSize() > iVideoFrame->getCapacity() - iVideoFrame->getFilledSize())
            {
                CallSendVideoFrame(aMediaData);
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223IncomingChannel(%d)::::DispatchOutgoingMsg, frag cannot fit into video buffer\n", lcn));

            }

            iVideoFrame->getMediaFragment(0, curVideoFrag);
            oscl_memcpy((uint8 *)curVideoFrag.getMemFragPtr() + iVideoFrame->getFilledSize(), memFrag.getMemFragPtr(), memFrag.getMemFragSize());
            iVideoFrame->setMediaFragFilledLen(0, iVideoFrame->getFilledSize() + memFrag.getMemFragSize());
        }

    }
    else if (iMediaType.isAudio())
    {
        if (PVMFSuccess != SendAudioFrame(aMediaData))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "H223IncomingChannel(%d)::DispatchOutgoingMsg - Sending audio failed, clearing queue", lcn));
            iAudioDataQueue.clear();
        }
    }
    else if (IsConnected())
    {
        // control
        convertToPVMFMediaMsg(mediaMsg, aMediaData);
        PVMFStatus dispatch_status = QueueOutgoingMsg(mediaMsg);
        if (dispatch_status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::DispatchOutgoingMsg Failed to queue outgoing control message status=%d", lcn, dispatch_status));
            status = dispatch_status;

        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::DispatchOutgoingMsg Queue not connected", lcn));
    }

    return status;
}
/* ***************************************************************************************************
*  Function : CheckFrameBoundary()
*  Date     : 9/11/2009
*  Purpose  : The purpose of this function is to detect, wether the coming video stream
*             is H263 or MPEG-4 format, Very soon we will add the case for H264 also.
*
*
*  INPUT    : aDataPtr,aDataSize,aCrcError
*  Return   : bool
*
*
* ******************************************************************************************************/

bool H223IncomingChannel::CheckFrameBoundary(uint8* aDataPtr,
        int32 aDataSize,
        uint32 aCrcError)
{



    //If start of new frame.
    if ((aDataSize > 2) &&
            (aDataPtr[0] == H263_START_BYTE_1) &&
            (aDataPtr[1] == H263_START_BYTE_2) &&
            ((aDataPtr[2] & H263_START_BYTE_3_MASK) == H263_START_BYTE_3))
    {
        return true;
    }

    if (!aCrcError)
    {
        if (aDataSize >= 3)
        {
            if ((aDataPtr[0] == VOP_START_BYTE_1) &&
                    (aDataPtr[1] == VOP_START_BYTE_2) &&
                    (aDataPtr[2] == VOP_START_BYTE_3))
            {
                return true;

            }
        }
    }

    // H264
    if ((aDataSize > 2) &&
            (aDataPtr[0] == H264_START_BYTE_1) &&
            (aDataPtr[1] == H264_START_BYTE_2))
    {
        if (aDataPtr[2] == H264_LAST_START_BYTE)
        {
            return true;
        }
        else if (((aDataSize > 3) &&
                  (aDataPtr[2] == H264_START_BYTE_3) &&
                  (aDataPtr[3] == H264_LAST_START_BYTE)))
        {
            return true;
        }
    }

    return false;


}
/* *********************************************************************************************************
*  Function : pvmiSetPortFormatSpecificInfoSync()
*  Date     : 9/11/2009
*  Purpose  : The purpose of this function is to set fromat specific Inforamtion
*             for MPEG-4 video stream.
*
*
*  INPUT    : aPort,aFormatValType
*  Return   : bool
*
*
* **********************************************************************************************************/


bool
H223IncomingChannel::pvmiSetPortFormatSpecificInfoSync(PvmiCapabilityAndConfig *aPort,
        const char* aFormatValType)
{
    uint8* fsi = NULL;
    uint32 fsiLen = ::GetFormatSpecificInfo(iDataType, fsi);
    SetFormatSpecificInfo(fsi, fsiLen);

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
        if (iFormatSpecificInfoLen == 0)
        {
            kvp.value.key_specific_value = 0;
            kvp.capacity = 0;
        }
        else
        {
            kvp.value.key_specific_value = (OsclAny*)iFormatSpecificInfo;
            kvp.capacity = iFormatSpecificInfoLen;
        }

        PvmiKvp* retKvp = NULL; // for return value
        int32 err;
        OSCL_TRY(err, aPort->setParametersSync(NULL, &kvp, 1, retKvp););
        /* ignore the error for now */
        alloc.deallocate((OsclAny*)(kvp.key));
        return true;
    }
    return false;
}
/* ************************************************************************************************************
*  Function : SendVideoFrame()
*  Date     : 9/11/2009
*  Purpose  : The purpose of this function is to send one video frame to its lower (Decoder) node.
*             This function is being called from DispatchOutgoingMsg() function.
*
*
*  INPUT    : aMediaData
*  Return   : PVMFStatus
*
*
* ************************************************************************************************************/
PVMFStatus H223IncomingChannel::SendVideoFrame(PVMFSharedMediaDataPtr aMediaData)
{
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImpl;
    OsclSharedPtr<PVMFMediaDataImpl> tempImpl;
    OsclRefCounterMemFrag formatSpecificInfo;
    PVMFStatus dispatch_status = PVMFSuccess;
    OsclSharedPtr<PVMFMediaMsg> mediaMsg;
    int32 err = 0;


    iVideoFrame->getMediaDataImpl(mediaDataImpl);
    iVideoFrameNum ++;
    iVideoFrame->setSeqNum(iVideoFrameNum);
    aMediaData->getFormatSpecificInfo(formatSpecificInfo);
    iVideoFrame->setFormatSpecificInfo(formatSpecificInfo);
    mediaDataImpl->setMarkerInfo(true);

    if (IsConnected())
    {
        DispatchMessage(iVideoFrame);

#ifdef LIP_SYNC_TESTING
        g_IncmVideoTS = iVideoFrame->getTimestamp();
        CalculateRMSInfo(g_IncmVideoTS, g_IncmAudioTS);
#endif
        iVideoFrame.Unbind();
    }

    ipVideoFrameAlloc->ResizeMemoryFragment(mediaDataImpl);
    uint32 MediaMsgAllocOverhead = ipVideoFrameAlloc->GetMediaMsgAllocationOverheadBytes();
    uint32 LargestAvilableChunk = ipVideoFrameReszMemPool->getLargestContiguousFreeBlockSize();
    iMax_Chunk_Size = (LargestAvilableChunk - MediaMsgAllocOverhead);
    tempImpl = ipVideoFrameAlloc->allocate(iMax_Chunk_Size);
    if (!tempImpl)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING, (0, "H223IncomingChannel(%d)::SendVideoFrame , Max_Chunk_size Allocation failed\n", lcn));
        iVideoFrame.Unbind();

    }

    OSCL_TRY(err, iVideoFrame = PVMFMediaData::createMediaData(tempImpl, ipVideoDataMemPool););
    OSCL_FIRST_CATCH_ANY(err,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::SendVideoFrame, Error:  createMediaData failed, err=%d", lcn, err));
                         return PVMFFailure;
                        );



    return dispatch_status;
}
PVMFStatus H223IncomingChannel::CallSendVideoFrame(PVMFSharedMediaDataPtr aMediaData)
{
    PVMFStatus status = PVMFSuccess;
    int32 err = 0;
    OSCL_TRY(err, status = SendVideoFrame(aMediaData););
    OSCL_FIRST_CATCH_ANY(err,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::DispatchOutgoingMsg, Error:  SendVideoFrame failed, err=%d", lcn, err));
                         return PVMFFailure;
                        );
    return status;
}

PVMFStatus H223IncomingChannel::SendAudioFrame(PVMFSharedMediaDataPtr aMediaDataPtr)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::SendAudioFrame", lcn));

    int32 err = 0;

    if (ipAudioDtxTimer)
    {
        ipAudioDtxTimer->Cancel(AMR_TIMER_ID);
    }

    if (aMediaDataPtr)
    {
        // push a new frame to queue
        OSCL_TRY(err, iAudioDataQueue.push_back(aMediaDataPtr));
        OSCL_FIRST_CATCH_ANY(err,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "H223IncomingChannel(%d)::SendAudioFrame - Error - No Memory", lcn));
                             return PVMFErrNoMemory;
                            );
    }

    // we keep small buffer filled so we can have more accurate estimation of timestamps
    // following is based on a fact that we are always receiving data late.
    // we compare the current frame and the last frame from the buffer and select TS that is smaller

    uint16 dataQueueSize = OSCL_STATIC_CAST(uint16, iAudioDataQueue.size());
    uint16 dataQueueLimit = AMR_QUEUE_SIZE;

    PVMFSharedMediaDataPtr back = iAudioDataQueue.back();

    // check if last frame in queue is sid frame
    // if it is, send all data from queue
    if (back->getFilledSize() <= AMR_SID_SIZE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223IncomingChannel(%d)::SendAudioFrame - SID frame detected", lcn));
        dataQueueLimit = 1;
    }

    if (!aMediaDataPtr)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223IncomingChannel(%d)::SendAudioFrame - Flushing", lcn));
        dataQueueLimit = 1;
    }

    while (dataQueueSize >= dataQueueLimit)
    {
        PVMFSharedMediaDataPtr front = iAudioDataQueue.front();

        // get the smallest from ts and future timestamp
        // this will remove possible delay of current ts
        uint16 size = OSCL_STATIC_CAST(uint16, dataQueueSize * AMR_FRAME_SIZE_MS);
        PVMFTimestamp fts = front->getTimestamp();
        PVMFTimestamp bts = back->getTimestamp();

        // select the smallest time stamp as time stamp is never early
        PVMFTimestamp ts = OSCL_MIN(fts, (PVMFTimestamp)(bts - size));

        // look if last timestamp send to playback is higher than the one we have now
        if (iLastAudioTS)
        {
            ts = OSCL_MAX(ts, iLastAudioTS + AMR_FRAME_SIZE_MS);
        }

        // look that the timestamp that we have estimated is not late compared original ts.
        // we allow quite much jitter, so that we do not remove data and afterwards need to add it back.
        // it is also more likely to receive less than 50 frame/s than over 50fps.
        if (ts > (fts + AMR_JITTER))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "H223IncomingChannel(%d)::RemoveAudioFrame, size(%d) origTS(%d) TS(%d)\n", lcn, iAudioDataQueue[0]->getFilledSize(), fts, ts));
            // remove this frame
            iAudioDataQueue.erase(iAudioDataQueue.begin());
        }
        else
        {
            // send frame

            // smoothen the time stamp
            ts = ts - ts % AMR_FRAME_SIZE_MS;

            // set length, marker, timestamp for new media message
            iAudioDataQueue[0]->setMarkerInfo(PVMF_MEDIA_DATA_MARKER_INFO_M_BIT);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "H223IncomingChannel(%d)::SendAudioFrame, size(%d) origTS(%d) TS(%d)\n", lcn, iAudioDataQueue[0]->getFilledSize(), fts, ts));

            iAudioDataQueue[0]->setTimestamp(ts);
            iAudioDataQueue[0]->setSeqNum(++iAudioFrameNum);

            DispatchMessage(iAudioDataQueue[0]);

#ifdef LIP_SYNC_TESTING
            g_IncmAudioTS = ts;
            CalculateRMSInfo(g_IncmVideoTS, g_IncmAudioTS);
#endif

            // update the last timestamp variable
            iLastAudioTS = ts;

            // remove data from queue
            iAudioDataQueue.erase(iAudioDataQueue.begin());
        }
        dataQueueSize = OSCL_STATIC_CAST(uint16, iAudioDataQueue.size());
    }

    if (ipAudioDtxTimer)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223IncomingChannel(%d)::SendAudioFrame - Start new timer for ts(%d)", lcn, iCurTimestamp));
        ipAudioDtxTimer->Request(AMR_TIMER_ID, OSCL_STATIC_CAST(int32, iCurTimestamp), 1, this);
    }

    return PVMFSuccess;
}

void H223IncomingChannel::TimeoutOccurred(int32 aTimerID, int32 aTimeoutInfo)
{
    OSCL_UNUSED_ARG(aTimerID);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::TimeoutOccurred, TimerId(%d) TimeoutInfo(%d)", lcn, aTimerID, aTimeoutInfo));

    uint32 timeout = OSCL_STATIC_CAST(uint32, aTimeoutInfo);
    // if not no audio is sent during timeout we have problems receiving new data
    if (timeout == iCurTimestamp)
    {
        // if we have data in buffer
        if (iAudioDataQueue.size())
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223IncomingChannel(%d)::TimeoutOccurred - Flush audio", lcn));
            PVMFSharedMediaDataPtr mediaData;

            if (iPaused) return;

            SendAudioFrame(mediaData);
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223IncomingChannel(%d)::TimeoutOccurred - Create no data", lcn));

            if (!iPaused)
            {
                int leavecode = 0;
                PVMFSharedMediaDataPtr mediaData;
                if (CreateNoDataAudioFrame(mediaData))
                {
                    // don't let this leave as mediaData would be leaked
                    OSCL_TRY(leavecode, SendAudioFrame(mediaData););
                    OSCL_FIRST_CATCH_ANY(leavecode,
                                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223IncomingChannel(%d)::TimeoutOccurred, Error:  SendAudioFrame failed, err=%d", lcn, leavecode));
                                        );
                }
            }
        }
    }
}

void H223IncomingChannel::UpdateCurrentTimestamp()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::UpdateTimestamp", lcn));
    if (!iClock)
    {
        return;
    }

    bool overflow = false;
    PVMFTimestamp timestamp;

    // get the current time
    iClock->GetCurrentTime32(timestamp, overflow, PVMF_MEDIA_CLOCK_MSEC);

    if (overflow)
    {
        // we need to clean the queue and send a new bos message as previous clock value was higher than the new one
        // best way is to do this is to do pause/resume
        Pause();
        Resume();
    }

    // make sure that the new timestamp is not the same as previous one
    iCurTimestamp = (iCurTimestamp < timestamp) ? timestamp : iCurTimestamp + 1;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "H223IncomingChannel(%d)::UpdateCurrentTimestamp - update current ts(%u)", lcn, iCurTimestamp));
}

PVMFStatus H223IncomingChannel::DispatchMessage(PVMFSharedMediaDataPtr& arMediaDataPtr)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::DispatchMessage", lcn));

    PVMFStatus dispatchStatus = PVMFNotSet;
    OsclSharedPtr<PVMFMediaMsg> mediaMsg;

    // convert to media message
    convertToPVMFMediaMsg(mediaMsg, arMediaDataPtr);

    // send media message if bos message is already sent
    if (IsResumable())
    {
        dispatchStatus = QueueOutgoingMsg(mediaMsg);
    }
    else
    {
        dispatchStatus = PVMFErrInvalidState;
    }

    if (dispatchStatus != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                        (0, "H223IncomingChannel(%d)::DispatchMessage Failed to queue outgoing media message status=%d", lcn, dispatchStatus));
    }
    return dispatchStatus;
}

bool H223IncomingChannel::IsResumable()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H223IncomingChannel(%d)::IsResumable", lcn));
    // Bos message is needed at the beginning of stream
    // It is also needed in reposition, like in the case of pause
    // Only if bos message is sent media message can be send to port
    return (iBosMessageSent || (SendBeginOfStreamMediaCommand() == PVMFSuccess));
}

PVMFStatus H223IncomingChannel::CreateNoDataAudioFrame(PVMFSharedMediaDataPtr &arMediaDataPtr)
{
    int leavecode = 0;

    // Create new media data buffer
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImpl = iMediaFragGroupAlloc->allocate();

    // set no data frame as payload
    OsclRefCounterMemFrag memFrag;

    OSCL_TRY(leavecode, memFrag = iMemFragmentAlloc.get());
    OSCL_FIRST_CATCH_ANY(leavecode,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223IncomingChannel(%d)::TimeoutOccurred, Error: Mem frag alloc leaved, err=%d", lcn, leavecode));
                         return PVMFErrNoMemory;
                        );
    if (memFrag.getMemFragPtr() == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223IncomingChannel(%d)::TimeoutOccurred, Error: Mem frag alloc failed", lcn));
        return PVMFErrNoMemory;
    }
    *((uint8*)memFrag.getMemFragPtr()) = AMR_NO_DATA;
    memFrag.getMemFrag().len = AMR_NODATA_SIZE;

    mediaDataImpl->appendMediaFragment(memFrag); // this does not leave

    OSCL_TRY(leavecode, arMediaDataPtr = PVMFMediaData::createMediaData(mediaDataImpl, iMediaMsgMemoryPool););
    OSCL_FIRST_CATCH_ANY(leavecode,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "H223IncomingChannel(%d)::TimeoutOccurred, Error: Create Media Data, err=%d", lcn, leavecode));
                         return PVMFErrNoMemory;
                        );

    // set timestamp for no data
    UpdateCurrentTimestamp();
    arMediaDataPtr->setTimestamp(iCurTimestamp);
    return PVMFSuccess;
}
