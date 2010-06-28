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
#ifndef PVMF_AMR_PARSER_H_INCLUDED
#include "pvmf_amr_parser.h"
#endif
#ifndef PVMF_FILEPARSER_OBSERVER_H_INCLUDED
#include "pvmf_fileparser_observer.h"
#endif
#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#define LOG_DEBUG_MSG(msg) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, msg)

#define TIME_STAMP_PER_FRAME 20
#define AMR_NB_START_OFFSET 6
#define AMR_WB_START_OFFSET 9

const int BYTES_TO_RECOGNIZE = 32;
const int FRAMES_TO_SCAN = 100; // scan 2 secs of data

#define FOURCC(a, b, c, d) ((uint32)(uint8)(a) | ((uint32)(uint8)(b) << 8) | \
                ((uint32)(uint8)(c) << 16) | ((uint32)(uint8)(d) << 24))

/* Table containing the sum of the frame type byte     */
/* and the number of speech bytes for each codec mode  */
/* for IETF input format                                */
static const int32 IetfDecInputBytes[] =
{
    13, /* 4.75 */
    14, /* 5.15 */
    16, /* 5.90 */
    18, /* 6.70 */
    20, /* 7.40 */
    21, /* 7.95 */
    27, /* 10.2 */
    32, /* 12.2 */
    6, /* GsmAmr comfort noise */
    7, /* Gsm-Efr comfort noise */
    6, /* IS-641 comfort noise */
    6, /* Pdc-Efr comfort noise */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1  /* No transmission */
};

/* Table containing the number of speech bytes for each codec mode  */
/* for IF2 input format                                             */
static const int32 If2DecInputBytes[] =
{
    13, /* 4.75 */
    14, /* 5.15 */
    16, /* 5.90 */
    18, /* 6.70 */
    19, /* 7.40 */
    21, /* 7.95 */
    26, /* 10.2 */
    31, /* 12.2 */
    6, /* GsmAmr comfort noise */
    6, /* Gsm-Efr comfort noise */
    6, /* IS-641 comfort noise */
    6, /* Pdc-Efr comfort noise */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1  /* No transmission */
};

static const int32 IetfWBDecInputBytes[] =
{
    18, /* 6.60 */
    24, /* 8.85 */
    33, /* 12.65 */
    37, /* 14.25 */
    41, /* 15.85 */
    47, /* 18.25 */
    51, /* 19.85 */
    59, /* 23.05 */
    61, /* 23.85 */
    6,  /* SID */
    1,  /* For future use : Set to 1 to skip the frame type byte */
    1,  /* For future use : Set to 1 to skip the frame type byte */
    1,  /* For future use : Set to 1 to skip the frame type byte */
    1,  /* For future use : Set to 1 to skip the frame type byte */
    1,  /* Speech lost */
    1   /* No data/transmission */
};

PVMFAmrParser::PVMFAmrParser()
{
    iDataStream   = NULL;
    iClipFormat   = PVMF_UNKNOWN;
    iClipDuration = -1;
    iClipSize     = -1;
    iStartOffset  = -1;
    iAmrBitrate   = -1;
    iAvgFrameLen  =  0;
    iFrameType    = NULL;
    iFramesRead   =  0;
    pParserUser   = NULL;
    iCache        = NULL;
    ipLogger = PVLogger::GetLoggerObject("amrparser");
}

PVMFAmrParser::~PVMFAmrParser()
{
    ipLogger = NULL;

    delete iCache;
    pParserUser = NULL;
}

bool PVMFAmrParser::Init(PvmiDataStreamInterface *aDataStream, int64 &aDuration, uint32& aTimescale,
                         PVMFParserFlags aFlags)
{
    LOG_DEBUG_MSG((0, "PVMFAmrParser::Init() IN"));

    iDataStream = aDataStream;
    if (pParserUser)
        iDataStream->SetCallbackHandler(this);

    PvmiDataStreamStatus status = iDataStream->GetFileSize(iClipSize);
    if (PVDS_SUCCESS != status)
    {
        LOG_DEBUG_MSG((0, "GetFileSize Unsuccessful in PVMFAmrParser::Init()"));
        return false;
    }

    uint32 buffer[BYTES_TO_RECOGNIZE];
    OsclSizeT size = BYTES_TO_RECOGNIZE * sizeof(uint32);
    if (PVDS_SUCCESS != iDataStream->ReadAtOffset(0, buffer, size, 0))
    {
        LOG_DEBUG_MSG((0, "ReadAtOffset failed"));
        return false;
    }

    // Determine sub-format type
    if (FOURCC('#', '!', 'A', 'M') == buffer[0])
    {
        const uint8 *ptr = (uint8 *) buffer;
        if ('R' == ptr[4] && 0xa == ptr[5])
        {
            iClipFormat = PVMF_AMR_NB;
            iStartOffset = AMR_NB_START_OFFSET;
            iFrameType = const_cast<int32*>(IetfDecInputBytes);
        }
        else if (FOURCC('R', '-', 'W', 'B') == buffer[1] &&
                 0xa == ptr[8])
        {
            iClipFormat = PVMF_AMR_WB;
            iStartOffset = AMR_WB_START_OFFSET;
            iFrameType = const_cast<int32*>(IetfWBDecInputBytes);
        }
        LOG_DEBUG_MSG((0, "ClipFormat is %d", iClipFormat));
    }

    // Seek at StartOffset
    status = iDataStream->Seek(0, iStartOffset, PVDS_SEEK_SET);
    if (PVDS_SUCCESS != status)
    {
        LOG_DEBUG_MSG((0, "Seek to StartOffset failed"));
        return false;
    }

    iCache = new CacheBuffer(iDataStream);

    CalculateClipDuration();
    aDuration = iClipDuration;
    aTimescale = 1000;

    // reset cache to StartOffset for Read()
    if (!iCache->Seek(iStartOffset))
    {
        LOG_DEBUG_MSG((0, "Seek to start offset FAILED"));
        return false;
    }

    LOG_DEBUG_MSG((0, "PVMFAmrParser::Init() OUT"));
    return true;
}

void PVMFAmrParser::SetCallbackHandler(PVMFFileParserObserver* aParserUser)
{
    pParserUser = aParserUser;
    if (iDataStream)
        iDataStream->SetCallbackHandler(this);
}

int PVMFAmrParser::GetFrameLength(uint8& arBuffer)
{
    uint8 frameTypeIndex = (arBuffer >> 3) & 0xf;
    return iFrameType[frameTypeIndex];
}

void PVMFAmrParser::CalculateClipDuration()
{
    LOG_DEBUG_MSG((0, "PVMFAmrParser::CalculateClipDuration() IN"));
    int32 frame_count = 0, frame_len = 0, bytesRead = 0;
    OsclSizeT TotalBytesRead = 0, bytesToBeUsed = 0;
    uint8* buffer = NULL;
    bool scanningOver = false;

    while (PVMF_READ_SUCCESS ==  iCache->GetBuffData(buffer, bytesToBeUsed))
    {
        if (-1 == iAmrBitrate)
        {
            // Use the index to get the AMRBitrate, do this only once
            uint8 frameTypeIndex = (buffer[0] >> 3) & 0xf;
            static const int32 AMR_NB_Bitrate[8] =
            {
                4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200
            };
            static const int32 AMR_WB_Bitrate[9] =
            {
                6600, 8850, 12650, 14250, 15850, 18250, 19850, 23050, 23850
            };
            if (frameTypeIndex > 8 || (frameTypeIndex > 7 && (PVMF_AMR_NB == iClipFormat)))
                iAmrBitrate = 0;
            else
                iAmrBitrate = (PVMF_AMR_NB == iClipFormat) ? AMR_NB_Bitrate[frameTypeIndex] : AMR_WB_Bitrate[frameTypeIndex];
        }
        bytesRead = 0;
        while (1)
        {
            frame_len = GetFrameLength(buffer[0]);

            if (OsclSizeT(bytesRead + frame_len) > bytesToBeUsed)
            {
                iCache->SetRefillRequired(true);
                break;
            }
            TotalBytesRead += frame_len;
            bytesRead += frame_len;
            frame_count++;
            buffer += frame_len;
            if (frame_count >= FRAMES_TO_SCAN)
            {
                scanningOver = true;
                break;
            }
        }
        iCache->setBytesUsed(bytesRead);
        if (scanningOver)
            break;
    }
    LOG_DEBUG_MSG((0, "Frames scanned %d", frame_count));

    iAvgFrameLen = TotalBytesRead / frame_count;        // average frame length
    iClipDuration = ((iClipSize - iStartOffset) / iAvgFrameLen) * TIME_STAMP_PER_FRAME;

    LOG_DEBUG_MSG((0, "PVMFAmrParser::CalculateClipDuration() OUT"));
}

PVMFParserReturnCode PVMFAmrParser::GetNextAccessUnits(GAU* aGau)
{
    LOG_DEBUG_MSG((0, "PVMFAmrParser::GetNextAccessUnits"));
    LOG_DEBUG_MSG((0, "Frames to read are %d", aGau->NumberOfFrames));

    return ReadData(aGau);
}

void PVMFAmrParser::NotifyDataAvailable(OsclSizeT bytesRead, PvmiDataStreamStatus aStatus)
{
    LOG_DEBUG_MSG((0, "PVMFAmrParser::NotifyDataAvailable DS returned with last read complete"));

    if (NULL == pParserUser)
    {
        // No observer for Parser so just return.
        LOG_DEBUG_MSG((0, "No observer for Parser so just return"));
        return;
    }

    LOG_DEBUG_MSG((0, "NotifyDataAvailable: Frames to be read now are %d", (iGau->NumberOfFrames - iFramesRead)));

    PVMFParserReturnCode retStatus = PVMFParserEverythingFine;
    retStatus = ReadData(iGau);
    if (PVMFParserPending == retStatus)
        // Getting a Pending event from Datastream during
        // callback should not happen, fail the Read operation
        retStatus = PVMFParserDefaultErr;

    // Data available at data stream, send the data to the user
    pParserUser->CallbackHandler(iGau, retStatus);
}

PVMFParserReturnCode PVMFAmrParser::ReadData(GAU* aGau)
{
    LOG_DEBUG_MSG((0, "PVMFAmrParser::ReadData() INReading %d frames", aGau->NumberOfFrames));
    uint32 frame_len  = 0;
    uint32 framesRead = 0;
    OsclSizeT bytesToRead    = 0;
    OsclSizeT totalBytesRead = 0;
    aGau->BufferLen = totalBytesRead;
    uint8* cache_ptr = NULL;
    PVMFBufferStatus cache_status;

    cache_status = iCache->GetBuffData(cache_ptr, bytesToRead);
    if (PVMF_INSUFFICIENT_DATA == cache_status)
    {
        LOG_DEBUG_MSG((0, "Read FAILED. Out of Data."));
        aGau->NumberOfFrames = framesRead;
        return PVMFParserDefaultErr;
    }
    else if (PVMF_REQ_PENDING == cache_status)
    {
        // Copy the GAU structure
        iGau = aGau;
        return PVMFParserPending;
    }

    if (NULL == aGau->MediaBuffer)
    {
        LOG_DEBUG_MSG((0, "Setting gau pointer."));
        aGau->MediaBuffer = cache_ptr;
    }
    LOG_DEBUG_MSG((0, "Bytes can be read are %d", bytesToRead));

    for (uint32 ii = 0; ii < aGau->NumberOfFrames; ii++)
    {
        // In AMR, first byte has frame length info

        frame_len = GetFrameLength(cache_ptr[0]);
        LOG_DEBUG_MSG((0, "Frame Length is %d", frame_len));

        if (frame_len > bytesToRead)
        {
            iCache->SetRefillRequired(true);
            break;
        }

        if (totalBytesRead + frame_len > bytesToRead)
            break;

        cache_ptr += frame_len;
        totalBytesRead += frame_len;
        ++framesRead;
        aGau->info[ii].len = frame_len;
        aGau->info[ii].ts  = (iFramesRead * TIME_STAMP_PER_FRAME);
        iFramesRead++;
    }

    iCache->setBytesUsed(totalBytesRead);

    LOG_DEBUG_MSG((0, "Frames read are %d", framesRead));
    aGau->NumberOfFrames = framesRead;
    aGau->BufferLen = totalBytesRead;

    LOG_DEBUG_MSG((0, "PVMFAmrParser::ReadData() OUT"));
    return PVMFParserEverythingFine;
}

PVMFParserReturnCode PVMFAmrParser::Seek(int64 aSeekAt, int64& aSeekedTo)
{
    LOG_DEBUG_MSG((0, "PVMFAmrParser::Seek() IN"));

    // Use the frame len to do the seek
    OsclOffsetT offset = (aSeekAt * iAvgFrameLen) / TIME_STAMP_PER_FRAME;
    uint32 numFrames = offset / iAvgFrameLen;
    offset = iStartOffset + (numFrames * iAvgFrameLen); // Offset in Sync with frame boundaries

    if (!iCache->Seek(offset))
        return PVMFParserDefaultErr;

    aSeekedTo = aSeekAt;
    // Set the FramesRead accordingly
    iFramesRead = aSeekAt / TIME_STAMP_PER_FRAME;

    LOG_DEBUG_MSG((0, "Seeked at timestamp %ld msec", aSeekedTo));
    LOG_DEBUG_MSG((0, "PVMFAmrParser::Seek() OUT"));
    return PVMFParserEverythingFine;
}

void PVMFAmrParser::RetrieveFileInfo(PVFileInfo& aFileInfo)
{
    aFileInfo.AudioTrkInfo.BitRate = iAmrBitrate;
    aFileInfo.AudioTrkInfo.AudioFmt = iClipFormat;
    aFileInfo.AudioTrkInfo.TrackDuration = iClipDuration;
    aFileInfo.AudioTrkInfo.TrackTimescale = 1000;
    aFileInfo.AudioPresent = true;
}


