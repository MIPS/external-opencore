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
/*********************************************************************************/
/*     -------------------------------------------------------------------       */
/*                        MPEG-4 TimeToSampleAtom Class                          */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    This TimeToSampleAtom Class contains a compact version of a table that allows
    indexing from decoding to sample number.
*/


#ifndef TIMETOSAMPLEATOM_H_INCLUDED
#define TIMETOSAMPLEATOM_H_INCLUDED

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

#ifndef FULLATOM_H_INCLUDED
#include "fullatom.h"
#endif

class TimeToSampleAtom : public FullAtom
{

    public:
        TimeToSampleAtom(MP4_FF_FILE *fp,
                         uint32 mediaType,
                         uint32 size,
                         uint32 type,
                         OSCL_wString& filename,
                         uint32 parsingMode = 0);
        virtual ~TimeToSampleAtom();

        // Member gets and sets
        uint32 getEntryCount() const
        {
            return _entryCount;
        }

        MP4_ERROR_CODE GetSampleCountAt(uint32 aIndex, uint32& aCount);
        MP4_ERROR_CODE GetSampleNumberFromTimestamp(uint64 ts, uint32& aSampleNumber, bool oAlwaysRetSampleCount = false);
        MP4_ERROR_CODE GetTimeDeltaForSampleNumber(uint32 aNumber, uint32& aTimeDelta);
        MP4_ERROR_CODE GetTimestampForSampleNumber(uint32 aSampleNumber, uint64& aTimestamp);
        MP4_ERROR_CODE GetTimeDeltaForSampleNumberPeek(uint32 aSampleNumber, uint32& aTimeDelta);
        MP4_ERROR_CODE GetTimeDeltaForSampleNumberGet(uint32 aSampleNumber, uint32& aTimeDelta);

        MP4_ERROR_CODE ResetStateVariables();
        MP4_ERROR_CODE ResetStateVariables(uint32 sampleNum);
        MP4_ERROR_CODE ResetPeekwithGet();

        uint32 GetCurrPeekSampleCount() const
        {
            return _currPeekSampleCount;
        }

    private:
        bool ParseEntryUnit(uint32 entry_cnt);
        void CheckAndParseEntry(uint32 i);
        uint32 _entryCount;

        uint32 *_psampleCountVec;
        uint32 *_psampleDeltaVec;

        uint32 _mediaType;

        // For visual samples
        uint32 _currentTimestamp;

        MP4_FF_FILE *_fileptr;

        MP4_FF_FILE *_curr_fptr;
        TOsclFileOffset *_stbl_fptr_vec;
        uint32 _stbl_buff_size;
        uint32 _curr_entry_point;
        uint32 _curr_buff_number;
        uint32 _next_buff_number;

        uint32  _parsed_entry_cnt;

        uint32 _currGetSampleCount;
        int32 _currGetIndex;
        uint32 _currGetTimeDelta;
        uint32 _currPeekSampleCount;
        int32 _currPeekIndex;
        uint32 _currPeekTimeDelta;
        uint32 _parsing_mode;
        PVLogger *iLogger, *iStateVarLogger, *iParsedDataLogger;
};

#endif  // TIMETOSAMPLEATOM_H_INCLUDED

