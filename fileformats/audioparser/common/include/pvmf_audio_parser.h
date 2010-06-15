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
#ifndef PVMF_AUDIO_PARSER_H_INCLUDED
#define PVMF_AUDIO_PARSER_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef PVMI_DS_BASIC_INTERFACE_H_INCLUDED
#include "pvmi_ds_basic_interface.h"
#endif

#define CACHE_SIZE 8192   // size of 8KB

const int MAX_GAU_BUNDLE = 100;

typedef enum
{
    PVMF_UNKNOWN = 0,
    PVMF_AMR_NB = 1,
    PVMF_AMR_WB = 2,
    PVMF_WAV_PCM = 3,
    PVMF_WAV_ALAW = 4,
    PVMF_WAV_MULAW = 5
} PVMFClipFormatType;

typedef enum
{
    PVMFParserParseClipAsAlongPlay = 0,
    PVMFParserParseClipUpfront

} PVMFParserFlags;

typedef enum
{
    PVMFParserEverythingFine       = 0,
    PVMFParserPending              = 1,
    PVMFParserInSufficientData     = -1,
    PVMFParserMetaDataRetrievalErr = -2,
    PVMFParserSampleRetrievalErr   = -3,
    PVMFParserMemoryErr            = -4,
    PVMFParserIOErr                = -5,
    PVMFParserInSufficientBuffer   = -6,
    PVMFParserDefaultErr           = -7
} PVMFParserReturnCode;

typedef enum
{
    PVMF_READ_SUCCESS        =  0,
    PVMF_READ_FAILURE        = -1,
    PVMF_BUFFER_OVERFLOW     = -2,
    PVMF_INSUFFICIENT_DATA   = -3,
    PVMF_INSUFFICIENT_BUFFER = -4,
    PVMF_BUSY                = -5,
    PVMF_REQ_PENDING         = -6
} PVMFBufferStatus;

typedef struct
{
    uint32 len;
    uint64 ts;
} MediaMetaInfo;

typedef struct
{
    uint8* MediaBuffer;
    uint32 BufferLen;
    uint32 NumberOfFrames;
    MediaMetaInfo info[MAX_GAU_BUNDLE];
} GAU;

struct PVAudioFileInfo
{
    int32 SamplingRate;
    int32 NoOfChannels;
    int32 BitRate;
    int32 AudioFmt;
    uint64 TrackDuration;
    uint64 TrackTimescale;
    bool LittleEndian; // Extra Info for WAV Format

    PVAudioFileInfo()
    {
        SamplingRate = 0;
        NoOfChannels = 0;
        BitRate = 0;
        AudioFmt = 0;
        TrackDuration = 0;
        TrackTimescale = 0;
        LittleEndian = false;
    };
};

struct PVVideoFileInfo
{
    int32 Width;
    int32 Height;
    int32 VideoFmt;
    uint64 TrackDuration;
    uint64 TrackTimescale;

    PVVideoFileInfo()
    {
        Width = 0;
        Height = 0;
        VideoFmt = 0;
        TrackDuration = 0;
        TrackTimescale = 0;
    };
};

struct PVFileInfo
{
    PVAudioFileInfo AudioTrkInfo;
    PVVideoFileInfo VideoTrkInfo;
    bool AudioPresent;
    bool VideoPresent;

    PVFileInfo()
    {
        AudioPresent = false;
        VideoPresent = false;
    };
};

class PvmiDataStreamInterface;
class PVLogger;

class CacheBuffer
{
    public:
        CacheBuffer(PvmiDataStreamInterface* apDataStream);
        ~CacheBuffer();

        /* seek pointer in the buffer, aSeekAt offset is relative to the start of the file.
         * if aSeekAt offset is beyond  buffer then it refills it first.
         */
        bool Seek(OsclOffsetT aSeekAt);

        PVMFBufferStatus GetBuffData(uint8*& apBuffer, OsclSizeT& aSize);
        void setBytesUsed(uint32 aUsedBytes);

        void SetRefillRequired(bool aFlag);

        // for testing async purpose only
        OsclOffsetT iTestDSPos;

    private:
        /** Pointer to data stream */
        PvmiDataStreamInterface* ipDataStream;

        /** Count of current bytes read in buffer */
        uint32 iBytesUsed;

        /** Max bytes available in buffer for read */
        uint32 iMaxBytes;

        /** pointer to start of buffer */
        uint8* ipBuffer;

        /** moving pointer to buffer */
        uint8* ipBufferPosition;

        /** refill flag */
        bool iRefill;

        PVMFBufferStatus Fill();
        PVMFBufferStatus Refill();


        bool IsRefillRequired();

        PVLogger* ipLogger;
};

#endif
