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
#define PVMF_WAV_PARSER_H_INCLUDED

#ifndef PVMF_FILEPARSER_INTERFACE_H_INCLUDED
#include "pvmf_fileparser_interface.h"
#endif
#ifndef PVMI_DS_BASIC_INTERFACE_H_INCLUDED
#include "pvmi_ds_basic_interface.h"
#endif

enum PVWavSupportedFormatType
{
    WAV_FORMAT_UNKNOWN = 0,
    WAV_FORMAT_PCM = 1,
    WAV_FORMAT_ALAW = 6,
    WAV_FORMAT_MULAW = 7
};

class PVLogger;

class PVMFWavParser: public PVMFFileParserInterface
        , public PvmiBasicDataStreamObs
{
    public:
        PVMFWavParser();
        ~PVMFWavParser();

        bool Init(PvmiDataStreamInterface *aDataStream, int64 &aDuration, uint32& aTimescale,
                  PVMFParserFlags aFlags = PVMFParserParseClipAsAlongPlay);
        // For WAV clips return number of tracks as one by default
        inline uint32 GetNumTracks()
        {
            return 1;
        };
        PVMFParserReturnCode GetNextAccessUnits(GAU* aGau);
        PVMFParserReturnCode Seek(int64 aSeekAt, int64& aSeekedTo);
        void SetCallbackHandler(PVMFFileParserObserver* aParserUser);
        void NotifyDataAvailable(OsclSizeT bytesRead, PvmiDataStreamStatus aStatus);
        // RetrieveFileInfo - Function to return audio file info
        void RetrieveFileInfo(PVFileInfo& aFileInfo);
    private:
        PvmiDataStreamInterface* iDataStream;
        PVMFFileParserObserver* pParserUser;
        OsclOffsetT iClipSize;
        PVMFClipFormatType iClipFmt;
        bool iLittleEndian;
        uint16 iBitsPerSample;
        uint16 iChannels;
        uint32 iSamplesPerSec;
        uint16 iBytesPerSample;
        OsclOffsetT iStartOffset;
        int64 iDuration;
        short* pLawTable;
        OsclOffsetT iEndDataChnkOffset;
        uint32 iFramesRead;
        GAU* iGau;
        bool iEOSReached;
        PVLogger* ipLogger;
        uint32 iBytesPerSec;
        OsclSizeT iWavDataReadPerFrame;
};

#endif
