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
#include "lipsync_dummy_input_mio.h"
#include "pv_mime_string_utils.h"
#include "oscl_file_server.h"
#include "oscl_file_io.h"
const uint32 Num_Audio_Bytes             = 20;
const uint32 KAudioMicroSecsPerDataEvent = 20000;
const uint32 KVideoMicroSecsPerDataEvent = 100000;
const uint32  TOTAL_BYTES_READ           = 50;
const uint32 One_Yuv_Frame_Size          = 38016;
#define InputFileName       "video_in.yuv420"

#define DUMMY_MEDIADATA_POOLNUM 11

#define VOP_START_BYTE_1 0x00
#define VOP_START_BYTE_2 0x00
#define VOP_START_BYTE_3 0x01
#define VOP_START_BYTE_4 0xB6
#define GOV_START_BYTE_4 0xB3


#define H263_START_BYTE_1 0x00
#define H263_START_BYTE_2 0x00
#define H263_START_BYTE_3 0x80

#define BYTES_FOR_MEMPOOL_STORAGE 38100
#define BYTE_1 1
#define BYTE_2 2
#define BYTE_3 3
#define BYTE_4 4
#define BYTE_8 8
#define BYTE_16 16

// Logging macros
#define LOG_STACK_TRACE(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, m)
#define LOG_DEBUG(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, m)
#define LOG_ERR(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iLogger,PVLOGMSG_ERR,m)

LipSyncDummyInputMIO::LipSyncDummyInputMIO(const DummyMIOSettings& aSettings)
        : DummyInputMIO(aSettings),
        iParams(NULL),
        iCount(0),
        iDiffVidAudTS(0),
        iSqrVidAudTS(0),
        iRtMnSq(0),
        iAudioTimeStamp(0),
        iVideoTimeStamp(0)
{
}


////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::ThreadLogon()
{
    if (!iThreadLoggedOn)
    {
        AddToScheduler();
        iLogger = PVLogger::GetLoggerObject("LipSyncDummyInputMIO");
        iThreadLoggedOn = true;
    }
}



////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::DoStop()
{
    iState = STATE_STOPPED;
    if (iCount == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "Can't calculate the value of RMS because the count value is zero. g_count=%d", iCount));
    }
    else
    {
        iRtMnSq = (int32)sqrt(iSqrVidAudTS / iCount);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "RMS value at input side . g_RtMnSq=%d", iRtMnSq));
    }

    return PVMFSuccess;
}


LipSyncDummyInputMIO::~LipSyncDummyInputMIO()
{
    // release semaphore
}


void LipSyncDummyInputMIO::AdditionalGenerateVideoFrameStep(PVMFFormatType aFormat)
{
    if (aFormat == PVMF_MIME_H2631998 ||
            aFormat == PVMF_MIME_H2632000)
    {
        iVideoTimeStamp = iTimestamp;
        iParams = ShareParams::Instance();
        iParams->iCompressed = true;
        CalculateRMSInfo(iVideoTimeStamp, iAudioTimeStamp);
    }
    else if (aFormat == PVMF_MIME_YUV420 ||
             aFormat == PVMF_MIME_RGB16)
    {
        iParams = ShareParams::Instance();
        iParams->iCompressed = false;
        iVideoTimeStamp = iTimestamp;
        CalculateRMSInfo(iVideoTimeStamp, iAudioTimeStamp);
    }
}

void LipSyncDummyInputMIO::AdditionalGenerateAudioFrameStep(PVMFFormatType aFormat)
{
    OSCL_UNUSED_ARG(aFormat);
    iAudioTimeStamp = iTimestamp;
}

void LipSyncDummyInputMIO::CalculateRMSInfo(uint32 aVideoData, uint32 aAudioData)
{
    iDiffVidAudTS = aVideoData - aAudioData;
    iSqrVidAudTS += (iDiffVidAudTS * iDiffVidAudTS);
    ++iCount;

}
