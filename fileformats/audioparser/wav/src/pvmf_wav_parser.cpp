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
#ifndef PVMF_WAV_PARSER_H_INCLUDED
#include "pvmf_wav_parser.h"
#endif
#ifndef PVMF_FILEPARSER_OBSERVER_H_INCLUDED
#include "pvmf_fileparser_observer.h"
#endif
#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#define LOG_DEBUG_MSG(msg) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, msg)

#define BYTES_TO_RECOGNIZE 12
#define BYTES_TO_READ_FMT_CHNK 24

const short MuLawDecompressTable[256] =
{
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364, -9852, -9340, -8828, -8316,
    -7932, -7676, -7420, -7164, -6908, -6652, -6396, -6140,
    -5884, -5628, -5372, -5116, -4860, -4604, -4348, -4092,
    -3900, -3772, -3644, -3516, -3388, -3260, -3132, -3004,
    -2876, -2748, -2620, -2492, -2364, -2236, -2108, -1980,
    -1884, -1820, -1756, -1692, -1628, -1564, -1500, -1436,
    -1372, -1308, -1244, -1180, -1116, -1052,  -988,  -924,
    -876,  -844,  -812,  -780,  -748,  -716,  -684,  -652,
    -620,  -588,  -556,  -524,  -492,  -460,  -428,  -396,
    -372,  -356,  -340,  -324,  -308,  -292,  -276,  -260,
    -244,  -228,  -212,  -196,  -180,  -164,  -148,  -132,
    -120,  -112,  -104,   -96,   -88,   -80,   -72,   -64,
    -56,   -48,   -40,   -32,   -24,   -16,    -8,     0,
    32124, 31100, 30076, 29052, 28028, 27004, 25980, 24956,
    23932, 22908, 21884, 20860, 19836, 18812, 17788, 16764,
    15996, 15484, 14972, 14460, 13948, 13436, 12924, 12412,
    11900, 11388, 10876, 10364,  9852,  9340,  8828,  8316,
    7932,  7676,  7420,  7164,  6908,  6652,  6396,  6140,
    5884,  5628,  5372,  5116,  4860,  4604,  4348,  4092,
    3900,  3772,  3644,  3516,  3388,  3260,  3132,  3004,
    2876,  2748,  2620,  2492,  2364,  2236,  2108,  1980,
    1884,  1820,  1756,  1692,  1628,  1564,  1500,  1436,
    1372,  1308,  1244,  1180,  1116,  1052,   988,   924,
    876,   844,   812,   780,   748,   716,   684,   652,
    620,   588,   556,   524,   492,   460,   428,   396,
    372,   356,   340,   324,   308,   292,   276,   260,
    244,   228,   212,   196,   180,   164,   148,   132,
    120,   112,   104,    96,    88,    80,    72,    64,
    56,    48,    40,    32,    24,    16,     8,     0
};

const short ALawDecompressTable[256] =
{
    -5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736,
    -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
    -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
    -3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392,
    -22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
    -30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
    -11008, -10496, -12032, -11520, -8960, -8448, -9984, -9472,
    -15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
    -344,  -328,  -376,  -360,  -280,  -264,  -312,  -296,
    -472,  -456,  -504,  -488,  -408,  -392,  -440,  -424,
    -88,   -72,   -120,  -104,  -24,   -8,    -56,   -40,
    -216,  -200,  -248,  -232,  -152,  -136,  -184,  -168,
    -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
    -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696,
    -688,  -656,  -752,  -720,  -560,  -528,  -624,  -592,
    -944,  -912,  -1008, -976,  -816,  -784,  -880,  -848,
    5504,  5248,  6016,  5760,  4480,  4224,  4992,  4736,
    7552,  7296,  8064,  7808,  6528,  6272,  7040,  6784,
    2752,  2624,  3008,  2880,  2240,  2112,  2496,  2368,
    3776,  3648,  4032,  3904,  3264,  3136,  3520,  3392,
    22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
    30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136,
    11008, 10496, 12032, 11520, 8960,  8448,  9984,  9472,
    15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
    344,   328,   376,   360,   280,   264,   312,   296,
    472,   456,   504,   488,   408,   392,   440,   424,
    88,    72,   120,   104,    24,     8,    56,    40,
    216,   200,   248,   232,   152,   136,   184,   168,
    1376,  1312,  1504,  1440,  1120,  1056,  1248,  1184,
    1888,  1824,  2016,  1952,  1632,  1568,  1760,  1696,
    688,   656,   752,   720,   560,   528,   624,   592,
    944,   912,  1008,   976,   816,   784,   880,   848
};

PVMFWavParser::PVMFWavParser()
{
    iDataStream = NULL;
    pParserUser = NULL;
    iClipSize = 0;
    iLittleEndian = false;
    iClipFmt = PVMF_UNKNOWN;
    iBitsPerSample = 16;
    iChannels = 0;
    iSamplesPerSec = 0;
    iBytesPerSample = 0;
    iStartOffset = 0;
    iDuration = 0;
    pLawTable = NULL;
    iEndDataChnkOffset = 0;
    iFramesRead = 0;
    iEOSReached = false;
    ipLogger = PVLogger::GetLoggerObject("wavparser");
    iBytesPerSec = 0;
    iWavDataReadPerFrame = 0;
}

PVMFWavParser::~PVMFWavParser()
{
    ipLogger = NULL;
}

bool PVMFWavParser::Init(PvmiDataStreamInterface *aDataStream, int64 &aDuration, uint32& aTimescale,
                         PVMFParserFlags aFlags)
{
    LOG_DEBUG_MSG((0, "PVMFWavParser::Init In..."));

    iDataStream = aDataStream;
    if (pParserUser)
        iDataStream->SetCallbackHandler(this);

    PvmiDataStreamStatus status = iDataStream->GetFileSize(iClipSize);
    if (PVDS_SUCCESS != status || 0 == iClipSize)
    {
        LOG_DEBUG_MSG((0, "WavParser Init: DS GetFileSize failed OR Not a valid Clip"));
        return false;
    }
    LOG_DEBUG_MSG((0, "WavParser Init: Valid ClipSize %d", iClipSize));

    // Now validate the clip
    uint8 buffer[BYTES_TO_RECOGNIZE + BYTES_TO_READ_FMT_CHNK];
    OsclSizeT size = BYTES_TO_RECOGNIZE;
    OsclOffsetT fileOffset = 0;
    if (PVDS_SUCCESS != iDataStream->ReadAtOffset(0, buffer, size, fileOffset))
    {
        LOG_DEBUG_MSG((0, "WavParser Init: DS ReadAtOffset failed @Offset %d", fileOffset));
        return false;
    }
    fileOffset += size;

    if ('R' == buffer[0] &&
            'I' == buffer[1] &&
            'F' == buffer[2] &&
            'F' == buffer[3])
        // Little Endian format
        iLittleEndian = true;
    else if ('R' == buffer[0] &&
             'I' == buffer[1] &&
             'F' == buffer[2] &&
             'X' == buffer[3])
        // Big Endian Format
        iLittleEndian = false;
    else
        return false;

    // Check the WAVE Tag
    if ('W' != buffer[8] ||
            'A' != buffer[9] ||
            'V' != buffer[10] ||
            'E' != buffer[11])
    {
        // InValid wav tag
        return false;
    }

    // Get the format chunk now
    bool fmtChnkFound = false;
    uint32 subChunkSize = 0;
    size = 8;
    while (!fmtChnkFound)
    {
        if (PVDS_SUCCESS != iDataStream->ReadAtOffset(0, buffer, size, fileOffset))
        {
            LOG_DEBUG_MSG((0, "WavParser Init: DS ReadAtOffset failed @Offset %d", fileOffset));
            return false;
        }
        fileOffset += size;
        subChunkSize = ((buffer[7] << 24) | (buffer[6] << 16) | (buffer[5] << 8) | (buffer[4]));
        if ('f' == buffer[0] &&
                'm' == buffer[1] &&
                't' == buffer[2] &&
                ' ' == buffer[3])
        {
            fmtChnkFound = true;
            break;
        }
        fileOffset += subChunkSize;
    }

    // We have found the fmt tag, now get the parameters
    size = BYTES_TO_READ_FMT_CHNK - 8;
    if (PVDS_SUCCESS != iDataStream->ReadAtOffset(0, buffer + 8, size, fileOffset))
    {
        LOG_DEBUG_MSG((0, "WavParser Init: DS ReadAtOffset failed @Offset %d", fileOffset));
        return false;
    }
    fileOffset += size;

    uint16 audioFmt = ((buffer[9] << 8) | (buffer[8]));
    if (WAV_FORMAT_PCM == audioFmt)
    {
        // Data is Pulse Code Modulated
        // Read the fmt specific fields, for PCM read BitsPerSample
        iBitsPerSample = ((buffer[23] << 8) | (buffer[22]));
        iClipFmt = PVMF_WAV_PCM;
    }
    else if (WAV_FORMAT_MULAW == audioFmt)
    {
        pLawTable = (short*)MuLawDecompressTable;
        iClipFmt = PVMF_WAV_MULAW;
    }
    else if (WAV_FORMAT_ALAW == audioFmt)
    {
        pLawTable = (short*)ALawDecompressTable;
        iClipFmt = PVMF_WAV_ALAW;
    }
    else
    {
        LOG_DEBUG_MSG((0, "PVMFWavParser::Init Unsupported Audio Format"));
        return false;
    }

    uint16 BlockAlign = 0;
    uint32 AvgBytesPerSec = 0;
    iChannels = ((buffer[11] << 8) | (buffer[10]));
    iSamplesPerSec = ((buffer[15] << 24) | (buffer[14] << 16) | (buffer[13] << 8) | (buffer[12]));
    AvgBytesPerSec = ((buffer[19] << 24) | (buffer[18] << 16) | (buffer[17] << 8) | (buffer[16]));
    BlockAlign = ((buffer[21] << 8) | (buffer[20]));
    iBytesPerSample = (iBitsPerSample + 7) / 8;
    LOG_DEBUG_MSG((0, "PVMFWavParser::Init Parameters Read from file: audioFmt %d, iChannels %d, iSamplesPerSec %d,\
 AvgBytesPerSec %d, BlockAlign %d, iBitsPerSample %d ", audioFmt, iChannels, iSamplesPerSec, AvgBytesPerSec, BlockAlign, iBitsPerSample));

    // Update the fileOffset based on SubChunkSize
    // For PCM subChunkSize is 16
    if (16 != subChunkSize)
        fileOffset += (subChunkSize - 16);

    // Now look for the data chunk
    bool dataChnkFound = false;
    size = 8; // Read just 8 bytes to get the data chunk and size
    subChunkSize = 0;
    while (!dataChnkFound)
    {
        if (PVDS_SUCCESS != iDataStream->ReadAtOffset(0, buffer, size, fileOffset))
        {
            LOG_DEBUG_MSG((0, "WavParser Init: DS ReadAtOffset failed @Offset %d", fileOffset));
            return false;
        }
        fileOffset += size;
        subChunkSize = (((int32)buffer[7] << 24) | ((int32)buffer[6] << 16) | ((int32)buffer[5] << 8) | (buffer[4]));

        if ('d' == buffer[0] &&
                'a' == buffer[1] &&
                't' == buffer[2] &&
                'a' == buffer[3])
        {
            dataChnkFound = true;
            break;
        }
        fileOffset += subChunkSize;
    }

    iStartOffset = fileOffset;
    iDataStream->Seek(0, iStartOffset, PVDS_SEEK_SET);

    iEndDataChnkOffset = fileOffset + subChunkSize;

    iBytesPerSec = iBytesPerSample * iSamplesPerSec * iChannels;
    aDuration = iDuration = ((iEndDataChnkOffset - iStartOffset) / iBytesPerSec) * 1000;
    aTimescale = 1000;

    return true;
}

PVMFParserReturnCode PVMFWavParser::GetNextAccessUnits(GAU* aGau)
{
    LOG_DEBUG_MSG((0, "PVMFWavParser::GetNextAccessUnits In"));

    PvmiDataStreamStatus status = PVDS_FAILURE;
    PVMFParserReturnCode retStatus = PVMFParserEverythingFine;
    // Check if the buffer provided is big enough to hold the data
    OsclSizeT sizeInBytes = aGau->NumberOfFrames * iBytesPerSample * iChannels;

    if (sizeInBytes > aGau->BufferLen)
        return PVMFParserInSufficientBuffer;

    OsclOffsetT filePos = iDataStream->GetCurrentPointerPosition(0);
    if ((filePos + (OsclOffsetT)aGau->BufferLen) > iEndDataChnkOffset)
    {
        sizeInBytes = iEndDataChnkOffset - filePos;
        // last segment to read
        iEOSReached = true;
    }

    LOG_DEBUG_MSG((0, "PVMFWavParser: Bytes to be read %d", sizeInBytes));
    status = iDataStream->Read(0, aGau->MediaBuffer, sizeInBytes);

    if (PVDS_SUCCESS == status)
    {
        iWavDataReadPerFrame = sizeInBytes;
        aGau->info[0].len = sizeInBytes;
        uint32 wavDataReadinMsec = (sizeInBytes * 1000) / iBytesPerSec;
        aGau->info[0].ts = (iFramesRead * wavDataReadinMsec);
        iFramesRead++;
        if (pLawTable)
        {
            uint8* srcBuf = &(aGau->MediaBuffer[sizeInBytes-1]);
            short* destBuf = &(((short*)aGau->MediaBuffer)[sizeInBytes-1]);
            for (uint32 ii = sizeInBytes; ii > 0; ii--)
                *destBuf-- = pLawTable[*srcBuf--];
        }
        if (iEOSReached)
            retStatus = PVMFParserInSufficientData;
    }
    else if (PVDS_PENDING == status)
    {
        LOG_DEBUG_MSG((0, "DS returned Pending, means it need more data to process further read, wait for callback"));
        // Check if there is an observer of this class
        if (NULL != pParserUser)
        {
            iGau = aGau;
            return PVMFParserPending;
        }
        // No observer, break out of the loop and just return success with already read samples
        LOG_DEBUG_MSG((0, "No observer, just return success with already read samples"));
    }
    else if (PVDS_END_OF_STREAM == status)
    {
        retStatus = PVMFParserInSufficientData;
    }
    else
    {
        retStatus = PVMFParserDefaultErr;
    }
    aGau->BufferLen = sizeInBytes;
    aGau->NumberOfFrames = 1; // All samples combined in 1 frame

    LOG_DEBUG_MSG((0, "PVMFWavParser::GetNextAccessUnits Out, bytesRead %d, FramesRead %d", sizeInBytes, iFramesRead));

    return retStatus;
}

PVMFParserReturnCode PVMFWavParser::Seek(int64 aSeekAt, int64& aSeekedTo)
{
    LOG_DEBUG_MSG((0, "PVMFWavParser: Seek In, seekAt %d", aSeekAt));

    // Get the file offset toSeekTo
    OsclOffsetT offsetSeekAt = ((aSeekAt * iSamplesPerSec) / 1000) * iBytesPerSample * iChannels;
    // Now add the start offset
    offsetSeekAt += iStartOffset;

    if (offsetSeekAt > iEndDataChnkOffset)
    {
        // Seek point beyond end of data, cap it to end of data
        offsetSeekAt = iEndDataChnkOffset;
    }
    if (PVDS_SUCCESS != iDataStream->Seek(0, offsetSeekAt, PVDS_SEEK_SET))
    {
        LOG_DEBUG_MSG((0, "PVMFWavParser:Seek Failed"));
        return PVMFParserIOErr;
    }
    // Seek on Datastream succedded, means we did seeked to offsetSeekAt, which means SeekAt msecs
    aSeekedTo = aSeekAt;
    // Set the FramesRead accordingly
    if (iWavDataReadPerFrame > 0)
        iFramesRead = aSeekedTo / iWavDataReadPerFrame;
    // Reset EOS Boolean
    iEOSReached = false;

    LOG_DEBUG_MSG((0, "PVMFWavParser: Seek Out, seekedTo %d", aSeekedTo));
    return PVMFParserEverythingFine;
}

void PVMFWavParser::SetCallbackHandler(PVMFFileParserObserver* aParserUser)
{
    pParserUser = aParserUser;
    if (iDataStream)
        iDataStream->SetCallbackHandler(this);
}

void PVMFWavParser::NotifyDataAvailable(OsclSizeT bytesRead, PvmiDataStreamStatus aStatus)
{
    LOG_DEBUG_MSG((0, "PVMFWavParser::NotifyDataAvailable DS returned with last read complete"));

    if (NULL == pParserUser)
    {
        // No observer for Parser so just return.
        LOG_DEBUG_MSG((0, "No observer for Parser so just return"));
        return;
    }

    PVMFParserReturnCode retStatus = PVMFParserDefaultErr;
    if (PVDS_SUCCESS == aStatus)
    {
        iWavDataReadPerFrame = bytesRead;
        iGau->info[0].len = bytesRead;
        uint32 wavDataReadinMsec = (bytesRead * 1000) / iBytesPerSec;
        iGau->info[0].ts = (iFramesRead * wavDataReadinMsec);
        iFramesRead++;
        if (pLawTable)
        {
            uint8* srcBuf = &(iGau->MediaBuffer[bytesRead-1]);
            short* destBuf = &(((short*)iGau->MediaBuffer)[bytesRead-1]);
            for (uint32 ii = bytesRead; ii > 0; ii--)
                *destBuf-- = pLawTable[*srcBuf--];
        }
        iGau->BufferLen = bytesRead;
        iGau->NumberOfFrames = 1; // All samples combined in 1 frame

        retStatus = PVMFParserEverythingFine;
        if (iEOSReached)
            retStatus = PVMFParserInSufficientData;
    }
    // Data available at data stream, send the data to the user
    pParserUser->CallbackHandler(iGau, retStatus);
}

void PVMFWavParser::RetrieveFileInfo(PVFileInfo& aFileInfo)
{
    aFileInfo.AudioTrkInfo.SamplingRate = iSamplesPerSec;
    aFileInfo.AudioTrkInfo.NoOfChannels = iChannels;
    aFileInfo.AudioTrkInfo.BitRate = iBitsPerSample * iSamplesPerSec;
    aFileInfo.AudioTrkInfo.AudioFmt = iClipFmt;
    aFileInfo.AudioTrkInfo.TrackDuration = iDuration;
    aFileInfo.AudioTrkInfo.TrackTimescale = 1000;
    aFileInfo.AudioTrkInfo.LittleEndian = iLittleEndian;
    aFileInfo.AudioPresent = true;
}

