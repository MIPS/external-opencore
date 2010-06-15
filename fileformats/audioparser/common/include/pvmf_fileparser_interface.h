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
// -*- c++ -*-
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

//                 F I L E    P A R S E R   I N T E R F A C E

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
#ifndef PVMF_FILEPARSER_INTERFACE_H_INCLUDED
#define PVMF_FILEPARSER_INTERFACE_H_INCLUDED

#ifndef PVMF_AUDIO_PARSER_H_INCLUDED
#include "pvmf_audio_parser.h"
#endif

class PvmiDataStreamInterface;
class PVMFFileParserObserver;

class PVMFFileParserInterface
{
    public:
        virtual ~PVMFFileParserInterface() = 0;
        /**
          * @brief performs initialization of the parser
          *
          * @param aDataStream - DataStream pointer to access clip data
          * @param aDuration - Clip duration,
          * @param aTimescale - Clip Timescale
          * @param aFlags - To specify if clip is to parse upfron or as along play
          * @returns true if the init succeeds, else false.
        */
        virtual bool Init(PvmiDataStreamInterface *aDataStream, int64 &aDuration, uint32& aTimescale,
                          PVMFParserFlags aFlags = PVMFParserParseClipAsAlongPlay) = 0;

        /**
          * @brief Function to get number of tracks in the clip
          *
          * @return Returns the number of tracks in the clip.
        */
        virtual uint32 GetNumTracks() = 0;

        /**
          * @brief Function to retrieve the file info
          *
        * @param aFileInfo - structure will contain track level and file info.
        */
        virtual void RetrieveFileInfo(PVFileInfo& aFileInfo) = 0;

        /**
          * @brief Retrieves all suported metadata in form of KVP.
          *
          * @param aValueList - list of key value pairs of all supported metadata
          * @returns Result of operation: PVMFEverythingFine if succeed otherwise PVMFParserReturnCode.
         */
        //virtual PVMFParserErrorCode GetMetaData(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList) = 0;

        /**
          * @brief Attempts to read in the number sample specified by aNumSamples in gau struct.
          *
          * @param aGau Frame information structure of type GAU
          * @returns Result of operation: PVMFParserEverythingFine if succeed otherwise PVMFParserReturnCode.
        */
        virtual PVMFParserReturnCode GetNextAccessUnits(GAU* aGau) = 0;

        /**
           * @brief Resets the parser variables to allow start of playback at a new position
           *
           * @param aSeekAt Time where to start playback from
           * @param aSeekedTo Time where actual playback started
           * @returns Result of operation: PVMFParserEverythingFine if succeed otherwise PVMFParserReturnCode.
        */
        virtual PVMFParserReturnCode Seek(int64 aSeekAt, int64& aSeekedTo) = 0;

        /**
          * @brief Sets the callback handler to allow the parser to report results from async operations
          *
          * @param aParserUser Pointer to the user of the parser
        */
        virtual void SetCallbackHandler(PVMFFileParserObserver* aParserUser) = 0;
};

#endif
