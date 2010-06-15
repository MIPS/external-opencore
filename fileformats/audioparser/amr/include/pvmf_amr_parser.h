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
#define PVMF_AMR_PARSER_H_INCLUDED

#ifndef PVMF_FILEPARSER_INTERFACE_H_INCLUDED
#include "pvmf_fileparser_interface.h"
#endif
#ifndef PVMI_DS_BASIC_INTERFACE_H_INCLUDED
#include "pvmi_ds_basic_interface.h"
#endif

class PVLogger;

class PVMFAmrParser: public PVMFFileParserInterface
        , public PvmiBasicDataStreamObs
{
    public:

        PVMFAmrParser();
        ~PVMFAmrParser();
        void SetCallbackHandler(PVMFFileParserObserver* aParserUser);

        /* Initialize Parser by calculating duration,
         * initializing cache buffer
         */
        bool Init(PvmiDataStreamInterface *aDataStream, int64 &aDuration, uint32& aTimescale,
                  PVMFParserFlags aFlags = PVMFParserParseClipAsAlongPlay);

        // For AMR clips return number of tracks as one by default
        inline uint32 GetNumTracks()
        {
            return 1;
        };

        /* Reads frame data from the input file,
         * Input is number of frames required and
         * returns data rad in GAU structure
         */
        PVMFParserReturnCode GetNextAccessUnits(GAU* aGau);

        /* calculates seeking point offset and set pointer to that
         * returns timestamp of seeked to frame.
         */
        PVMFParserReturnCode Seek(int64 aSeekAt, int64& aSeekedTo);
        void NotifyDataAvailable(OsclSizeT bytesRead, PvmiDataStreamStatus aStatus);

        // RetrieveFileInfo - Function to return audio file info
        void RetrieveFileInfo(PVFileInfo& aFileInfo);

    private:
        /* returns Frame length
         */
        int GetFrameLength(uint8& arBuffer);

        /* calculate duration of a clip by scanning few frames
         */
        void CalculateClipDuration();

        PVMFParserReturnCode ReadData(GAU* aGau);

        /** Pointer to DS */
        PvmiDataStreamInterface* iDataStream;

        /** File format type */
        PVMFClipFormatType iClipFormat;

        /** duration of the input file */
        int64 iClipDuration;

        /** size of clip, in bytes */
        OsclOffsetT iClipSize;

        /** Start of data after magic words */
        OsclOffsetT iStartOffset;
        int32 iAmrBitrate;
        uint32 iAvgFrameLen;
        PVMFFileParserObserver* pParserUser;
        uint32 iFramesRead;
        GAU* iGau;
        int32* iFrameType;

        /** Pointer to cache buffer */
        CacheBuffer* iCache;

        PVLogger* ipLogger;
};

#endif
