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

//                 F I L E    P A R S E R  O B S E R V E R

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
#ifndef PVMF_FILEPARSER_OBSERVER_H_INCLUDED
#define PVMF_FILEPARSER_OBSERVER_H_INCLUDED

#ifndef PVMF_AUDIO_PARSER_H_INCLUDED
#include "pvmf_audio_parser.h"
#endif

class PVMFFileParserObserver
{
    public:
        virtual ~PVMFFileParserObserver() = 0;
        /**
          * @brief Function to handle callbacks from the parser
          * @param aGau - GAU structure to hold the sample requestedby observer
          * @param aCode - Result of operation: PVMFParserEverythingFine if succeed otherwise PVMFParserReturnCode.
          *
        */
        virtual void CallbackHandler(GAU* aGau, PVMFParserReturnCode& aCode) = 0;
};

#endif
