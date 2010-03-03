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


#define IMPLEMENT_TimeToSampleAtom

#include "timetosampleatom.h"
#include "atomutils.h"
#include "atomdefs.h"

// Stream-in ctor
TimeToSampleAtom::TimeToSampleAtom(MP4_FF_FILE *fp,
                                   uint32 mediaType,
                                   uint32 size,
                                   uint32 type,
                                   OSCL_wString& filename,
                                   uint32 parsingMode)
        : FullAtom(fp, size, type)
{

    _psampleCountVec = NULL;
    _psampleDeltaVec = NULL;

    _currGetSampleCount = 0;
    _currGetIndex = -1;
    _currGetTimeDelta = 0;
    _currPeekSampleCount = 0;
    _currPeekIndex = -1;
    _currPeekTimeDelta = 0;

    _mediaType = mediaType;
    _parsed_entry_cnt = 0;
    _fileptr = NULL;
    _parsing_mode = 0;
    _parsing_mode = parsingMode;

    _stbl_buff_size = MAX_CACHED_TABLE_ENTRIES_FILE;
    _next_buff_number = 0;
    _curr_buff_number = 0;
    _curr_entry_point = 0;
    _stbl_fptr_vec = NULL;

    iLogger = PVLogger::GetLoggerObject("mp4ffparser");
    iStateVarLogger = PVLogger::GetLoggerObject("mp4ffparser_mediasamplestats");
    iParsedDataLogger = PVLogger::GetLoggerObject("mp4ffparser_parseddata");


    if (_success)
    {
        if (!AtomUtils::read32(fp, _entryCount))
        {
            _success = false;
        }
        PVMF_MP4FFPARSER_LOGPARSEDINFO((0, "TimeToSampleAtom::TimeToSampleAtom- _entryCount =%d", _entryCount));
        TOsclFileOffset dataSize = _size - (DEFAULT_FULL_ATOM_SIZE + 4);

        uint32 entrySize = (4 + 4);

        if ((TOsclFileOffset)(_entryCount*entrySize) > dataSize)
        {
            _success = false;
        }

        if (_success)
        {
            if (_entryCount > 0)
            {

                if (parsingMode == 1)
                {
                    // cache size is 4K so that optimization should work if entry_count is greater than 4K
                    if ((_entryCount > _stbl_buff_size))
                    {
                        uint32 fptrBuffSize = (_entryCount / _stbl_buff_size) + 1;

                        PV_MP4_FF_ARRAY_NEW(NULL, TOsclFileOffset, (fptrBuffSize), _stbl_fptr_vec);
                        if (_stbl_fptr_vec == NULL)
                        {
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }

                        PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_stbl_buff_size), _psampleCountVec);
                        if (_psampleCountVec == NULL)
                        {
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }
                        PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_stbl_buff_size), _psampleDeltaVec);
                        if (_psampleDeltaVec == NULL)
                        {
                            PV_MP4_ARRAY_DELETE(NULL, _psampleDeltaVec);
                            _psampleDeltaVec = NULL;
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }
                        for (uint32 idx = 0; idx < _stbl_buff_size; idx++)  //initialization
                        {
                            _psampleCountVec[idx] = 0;
                            _psampleDeltaVec[idx] = 0;
                        }

                        OsclAny* ptr = (MP4_FF_FILE *)(oscl_malloc(sizeof(MP4_FF_FILE)));
                        if (ptr == NULL)
                        {
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }
                        _fileptr = OSCL_PLACEMENT_NEW(ptr, MP4_FF_FILE());
                        _fileptr->_fileServSession = fp->_fileServSession;
                        _fileptr->_pvfile.SetCPM(fp->_pvfile.GetCPM());
                        _fileptr->_pvfile.SetFileHandle(fp->_pvfile.iFileHandle);
                        if (AtomUtils::OpenMP4File(filename,
                                                   Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                                   _fileptr) != 0)
                        {
                            _success = false;
                            _mp4ErrorCode = FILE_OPEN_FAILED;
                        }

                        _fileptr->_fileSize = fp->_fileSize;

                        TOsclFileOffset _head_offset = AtomUtils::getCurrentFilePosition(fp);
                        AtomUtils::seekFromCurrPos(fp, dataSize);
                        AtomUtils::seekFromStart(_fileptr, _head_offset);
                        return;
                    }
                    else
                    {
                        _parsing_mode = 0;
                        _stbl_buff_size = _entryCount;
                    }
                }
                else
                {
                    _stbl_buff_size = _entryCount;
                }

                PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_entryCount), _psampleCountVec);
                if (_psampleCountVec == NULL)
                {
                    _success = false;
                    _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                    return;
                }
                PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_entryCount), _psampleDeltaVec);
                if (_psampleDeltaVec == NULL)
                {
                    PV_MP4_ARRAY_DELETE(NULL, _psampleDeltaVec);
                    _psampleDeltaVec = NULL;
                    _success = false;
                    _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                    return;
                }
                for (uint32 idx = 0; idx < _entryCount; idx++)  //initialization
                {
                    _psampleCountVec[idx] = 0;
                    _psampleDeltaVec[idx] = 0;
                }

                uint32 number = 0;
                uint32 delta = 0;
                for (_parsed_entry_cnt = 0; _parsed_entry_cnt < _entryCount; _parsed_entry_cnt++)
                {
                    if (!AtomUtils::read32(fp, number))
                    {
                        _success = false;
                        break;
                    }
                    if (!AtomUtils::read32(fp, delta))
                    {
                        _success = false;
                        break;
                    }
                    _psampleCountVec[_parsed_entry_cnt] = (number);
                    _psampleDeltaVec[_parsed_entry_cnt] = (delta);
                }
            }
        }

        if (!_success)
        {
            _mp4ErrorCode = READ_TIME_TO_SAMPLE_ATOM_FAILED;
        }
    }
    else
    {
        if (_mp4ErrorCode != ATOM_VERSION_NOT_SUPPORTED)
            _mp4ErrorCode = READ_TIME_TO_SAMPLE_ATOM_FAILED;
    }
}

bool TimeToSampleAtom::ParseEntryUnit(uint32 entry_cnt)
{

    const uint32 threshold = 1024;
    entry_cnt += threshold;

    if (entry_cnt > _entryCount)
        entry_cnt = _entryCount;

    uint32 number, delta;
    while (_parsed_entry_cnt < entry_cnt)
    {
        _curr_entry_point = _parsed_entry_cnt % _stbl_buff_size;
        _curr_buff_number = _parsed_entry_cnt / _stbl_buff_size;

        if (_curr_buff_number  == _next_buff_number)
        {
            TOsclFileOffset currFilePointer = AtomUtils::getCurrentFilePosition(_fileptr);
            _stbl_fptr_vec[_curr_buff_number] = currFilePointer;
            _next_buff_number++;
        }

        if (!_curr_entry_point)
        {
            TOsclFileOffset currFilePointer = _stbl_fptr_vec[_curr_buff_number];
            AtomUtils::seekFromStart(_fileptr, currFilePointer);
        }

        if (!AtomUtils::read32(_fileptr, number))
        {
            return false;
        }
        if (!AtomUtils::read32(_fileptr, delta))
        {
            return false;
        }
        _psampleCountVec[_curr_entry_point] = (number);
        _psampleDeltaVec[_curr_entry_point] = (delta);
        _parsed_entry_cnt++;
    }
    return true;
}

// Destructor
TimeToSampleAtom::~TimeToSampleAtom()
{
    if (_psampleCountVec != NULL)
        PV_MP4_ARRAY_DELETE(NULL, _psampleCountVec);

    if (_psampleDeltaVec != NULL)
        PV_MP4_ARRAY_DELETE(NULL, _psampleDeltaVec);

    if (_stbl_fptr_vec != NULL)
        PV_MP4_ARRAY_DELETE(NULL, _stbl_fptr_vec);

    if (_fileptr != NULL)
    {
        if (_fileptr->IsOpen())
        {
            AtomUtils::CloseMP4File(_fileptr);
        }
        oscl_free(_fileptr);
    }
}

MP4_ERROR_CODE TimeToSampleAtom::GetSampleCountAt(uint32 aIndex, uint32& aCount)
{
    MP4_ERROR_CODE retval = READ_FAILED;
    if ((aIndex < _entryCount) && _psampleCountVec)
    {
        if (_parsing_mode == 1)
            CheckAndParseEntry(aIndex);
        aCount = _psampleCountVec[aIndex%_stbl_buff_size];
        retval = EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>TimeToSampleAtom::getSampleCountAt aIndex = %d", aIndex));
    }
    return retval;
}

// Return the samples corresponding to the timestamp ts.  If there is not a sample
// exactly at ts, the very next sample is used.
// This atom maintains timestamp deltas between samples, i.e. delta[i] is the
// timestamp difference between sample[i] and sample[i+1].  It is implicit that
// sample[0] occurs at timestamp ts=0.
MP4_ERROR_CODE TimeToSampleAtom::GetSampleNumberFromTimestamp(uint64 aTimeStamp, uint32& aSampleNumber, bool oAlwaysRetSampleCount)
{
    MP4_ERROR_CODE retval = DEFAULT_ERROR;
    // It is assumed that sample 0 has a ts of 0 - i.e. the first
    // entry in the table starts with the delta between sample 1 and sample 0
    uint32 sampleCount = 0;
    uint64 timeCount = 0;

    if ((_psampleDeltaVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount      == 0))
    {
        return retval;
    }

    for (uint32 i = 0; i < _entryCount; i++)
    {
        if (_parsing_mode == 1)
            CheckAndParseEntry(i);

        if (aTimeStamp < timeCount)
        { // found range that the sample is in - need to backtrack
            if (_parsing_mode == 1)
                CheckAndParseEntry(i - 1);

            uint32 samples = _psampleCountVec[(i-1)%_stbl_buff_size];
            sampleCount -= samples;
            timeCount -= _psampleDeltaVec[(i-1)%_stbl_buff_size] * samples;
            while (timeCount <= aTimeStamp)
            {
                timeCount += _psampleDeltaVec[(i-1)%_stbl_buff_size];
                sampleCount += 1;
            }

            if (timeCount > aTimeStamp)
            {
                if (sampleCount > 0)
                {
                    sampleCount--;
                }
                aSampleNumber = sampleCount;
                return EVERYTHING_FINE;
            }
        }
        else if (aTimeStamp == timeCount)
        { // Found sample at aTimeStamp
            aSampleNumber = sampleCount;
            return EVERYTHING_FINE;
        }
        else
        { // Sample not yet found - advance
            uint32 samples = _psampleCountVec[i%_stbl_buff_size]; //number of samples at this index
            sampleCount += samples;
            timeCount += _psampleDeltaVec[i%_stbl_buff_size] * samples;
        }
    }

    // Timestamp is in last run of samples (or possibly beyond)
    // - need to backtrack to beginning of last run to find it
    uint32 samples = _psampleCountVec[(_entryCount-1)%_stbl_buff_size];
    uint32 delta = _psampleDeltaVec[(_entryCount-1)%_stbl_buff_size];
    sampleCount -= samples;
    timeCount -= delta * samples;

    for (uint32 count = 0; count < (samples - 1); count++)
    {
        timeCount += delta;
        sampleCount += 1;

        if (timeCount > aTimeStamp)
        {
            if (sampleCount > 0)
            {
                sampleCount--;
            }
            aSampleNumber = sampleCount;
            return EVERYTHING_FINE;
        }
        else if (timeCount == aTimeStamp)
        {
            aSampleNumber = sampleCount;
            return EVERYTHING_FINE;
        }
    }


    sampleCount += 1;
    if (aTimeStamp >= timeCount)
    {
        if (oAlwaysRetSampleCount)
        {
            aSampleNumber = sampleCount;
            return EVERYTHING_FINE;
        }
        else
        {
            if (_mediaType == MEDIA_TYPE_VISUAL)
            {
                aSampleNumber = sampleCount;
                return EVERYTHING_FINE;
            }
        }
    }

    // Went past last sample in last run of samples - not a valid timestamp
    return retval;
}

// Returns the timestamp delta (ms) for the current sample given by num.  This value
// is the difference in timestamps between the sample (num) and the previous sample
// in the track.  It is implicit that sample[0] occurs at timestamp ts=0.
MP4_ERROR_CODE TimeToSampleAtom::GetTimeDeltaForSampleNumber(uint32 aNumber, uint32& aTimeDelta)
{
    MP4_ERROR_CODE retval = READ_FAILED;
    // It is assumed that sample 0 has a ts of 0 - i.e. the first
    // entry in the table starts with the delta between sample 1 and sample 0
    if ((_psampleDeltaVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount == 0))
    {
        return retval;
    }

    if (0 == aNumber)
    {
        aTimeDelta = 0;
        return EVERYTHING_FINE;
    }

    uint32 sampleCount = 0;
    for (uint32 i = 0; i < _entryCount; i++)
    {
        if (_parsing_mode == 1)
            CheckAndParseEntry(i);

        if (aNumber <= (sampleCount + _psampleCountVec[i%_stbl_buff_size]))
        { // Sample num within current entry
            aTimeDelta = (_psampleDeltaVec[i%_stbl_buff_size]);
            retval = EVERYTHING_FINE;
            break;
        }
        else
        {
            sampleCount += _psampleCountVec[i%_stbl_buff_size];
        }
    }

    // Went past end of list - not a valid sample number
    return retval;
}

// Returns the timestamp (ms) for the current sample given by num.  This is used when
// randomly accessing a frame and the timestamp has not been accumulated. It is implicit
// that sample[0] occurs at timestamp ts=0.
MP4_ERROR_CODE TimeToSampleAtom::GetTimestampForSampleNumber(uint32 aSampleNumber, uint64& aTimestamp)
{
    MP4_ERROR_CODE retval = READ_FAILED;
    if ((_psampleDeltaVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount == 0))
    {
        return retval;
    }
    // It is assumed that sample 0 has a ts of 0 - i.e. the first
    // entry in the table starts with the delta between sample 1 and sample 0
    if (0 == aSampleNumber)
    {
        aTimestamp = 0;
        return EVERYTHING_FINE;
    }

    uint32 sampleCount = 0;
    uint64 ts = 0; // Timestamp value to return
    for (uint32 i = 0; i < _entryCount; i++)
    {
        if (_parsing_mode == 1)
            CheckAndParseEntry(i);

        if (aSampleNumber <= (sampleCount + _psampleCountVec[i%_stbl_buff_size]))
        { // Sample num within current entry
            int32 count = aSampleNumber - sampleCount;
            ts += _psampleDeltaVec[i%_stbl_buff_size] * count;
            aTimestamp = ts;
            retval = EVERYTHING_FINE;
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "TimeToSampleAtom::getTimestampForSampleNumber- Time Stamp =%d", ts));
            break;
        }
        else
        {
            sampleCount += _psampleCountVec[i%_stbl_buff_size];
            ts += (_psampleDeltaVec[i%_stbl_buff_size] * _psampleCountVec[i%_stbl_buff_size]);
        }
    }

    // Went past end of list - not a valid sample number
    return retval;
}

// Returns the timestamp delta (ms) for the current sample given by num.  This value
// is the difference in timestamps between the sample (num) and the previous sample
// in the track.  It is implicit that sample[0] occurs at timestamp ts=0.
MP4_ERROR_CODE TimeToSampleAtom::GetTimeDeltaForSampleNumberPeek(uint32 aSampleNumber, uint32& aTimeDelta)
{
    MP4_ERROR_CODE retval = READ_FAILED;
    // It is assumed that sample 0 has a ts of 0 - i.e. the first
    // entry in the table starts with the delta between sample 1 and sample 0
    if ((_psampleDeltaVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount == 0))
    {
        return retval;
    }

    // note that sampleNum is a zero based index while _currGetSampleCount is 1 based index
    if (aSampleNumber < _currPeekSampleCount)
    {
        aTimeDelta = _currPeekTimeDelta;
        retval = EVERYTHING_FINE;
    }
    else
    {
        do
        {
            _currPeekIndex++;
            if (_parsing_mode)
                CheckAndParseEntry(_currPeekIndex);

            _currPeekSampleCount += _psampleCountVec[_currPeekIndex%_stbl_buff_size];
            _currPeekTimeDelta    = _psampleDeltaVec[_currPeekIndex%_stbl_buff_size];
        }
        while (_currPeekSampleCount == 0);

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "TimeToSampleAtom::getTimeDeltaForSampleNumberPeek- _currPeekIndex =%d", _currPeekIndex));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "TimeToSampleAtom::getTimeDeltaForSampleNumberPeek- _currPeekSampleCount =%d", _currPeekSampleCount));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "TimeToSampleAtom::getTimeDeltaForSampleNumberPeek- _currPeekTimeDelta =%d", _currPeekTimeDelta));

        if (aSampleNumber < _currPeekSampleCount)
        {
            aTimeDelta = _currPeekTimeDelta;
            retval = EVERYTHING_FINE;
        }
        else
        {
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>TimeToSampleAtom::getTimeDeltaForSampleNumberPeek sampleNum = %d", aSampleNumber));
        }
    }
    return retval;
}

// Returns the timestamp delta (ms) for the current sample given by num.  This value
// is the difference in timestamps between the sample (num) and the previous sample
// in the track.  It is implicit that sample[0] occurs at timestamp ts=0.
MP4_ERROR_CODE TimeToSampleAtom::GetTimeDeltaForSampleNumberGet(uint32 aSampleNumber, uint32& aTimeDelta)
{
    MP4_ERROR_CODE retval = READ_FAILED;
    // It is assumed that sample 0 has a ts of 0 - i.e. the first
    // entry in the table starts with the delta between sample 1 and sample 0
    if ((NULL == _psampleDeltaVec) ||
            (NULL == _psampleCountVec) ||
            (0 == _entryCount))
    {
        return retval;
    }

    // note that sampleNum is a zero based index while _currGetSampleCount is 1 based index
    if (aSampleNumber < _currGetSampleCount)
    {
        aTimeDelta = _currGetTimeDelta;
        return EVERYTHING_FINE;
    }

    do
    {
        ++_currGetIndex;
        if (_parsing_mode)
            CheckAndParseEntry(_currGetIndex);

        _currGetSampleCount += _psampleCountVec[_currGetIndex%_stbl_buff_size];
        _currGetTimeDelta    = _psampleDeltaVec[_currGetIndex%_stbl_buff_size];
    }
    while (0 == _currGetSampleCount);

    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "TimeToSampleAtom::getTimeDeltaForSampleNumberGet- _currGetIndex =%d", _currGetIndex));
    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "TimeToSampleAtom::getTimeDeltaForSampleNumberGet- _currGetSampleCount =%d", _currGetSampleCount));
    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "TimeToSampleAtom::getTimeDeltaForSampleNumberGet- _currGetTimeDelta =%d", _currGetTimeDelta));

    if (aSampleNumber < _currGetSampleCount)
    {
        aTimeDelta = _currGetTimeDelta;
        retval = EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>TimeToSampleAtom::getTimeDeltaForSampleNumberGet sampleNum = %d", aSampleNumber));
    }
    return retval;
}

MP4_ERROR_CODE
TimeToSampleAtom::ResetStateVariables()
{
    uint32 sampleNum = 0;
    return (ResetStateVariables(sampleNum));
}

MP4_ERROR_CODE
TimeToSampleAtom::ResetStateVariables(uint32 aSampleNum)
{
    _currGetSampleCount = 0;
    _currGetIndex = -1;
    _currGetTimeDelta = 0;
    _currPeekSampleCount = 0;
    _currPeekIndex = -1;
    _currPeekTimeDelta = 0;

    // It is assumed that sample 0 has a ts of 0 - i.e. the first
    // entry in the table starts with the delta between sample 1 and sample 0
    if ((_psampleDeltaVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount == 0))
    {
        return DEFAULT_ERROR;
    }


    for (uint32 i = 0; i < _entryCount; i++)
    {
        if (_parsing_mode)
            CheckAndParseEntry(i);

        _currPeekIndex++;
        _currPeekSampleCount += _psampleCountVec[i%_stbl_buff_size];
        _currPeekTimeDelta    = _psampleDeltaVec[i%_stbl_buff_size];

        _currGetIndex++;
        _currGetSampleCount += _psampleCountVec[i%_stbl_buff_size];
        _currGetTimeDelta    = _psampleDeltaVec[i%_stbl_buff_size];

        if (aSampleNum <= _currPeekSampleCount)
        {
            return (EVERYTHING_FINE);
        }

    }

    // Went past end of list - not a valid sample number
    return DEFAULT_ERROR;
}

MP4_ERROR_CODE TimeToSampleAtom::ResetPeekwithGet()
{
    _currPeekSampleCount = _currGetSampleCount;
    _currPeekIndex = _currGetIndex ;
    _currPeekTimeDelta = _currGetTimeDelta;
    return (EVERYTHING_FINE);
}

void TimeToSampleAtom::CheckAndParseEntry(uint32 i)
{
    if (i >= _parsed_entry_cnt)
    {
        ParseEntryUnit(i);
    }
    else
    {
        uint32 entryLoc = i / _stbl_buff_size;
        if (_curr_buff_number != entryLoc)
        {
            _parsed_entry_cnt = entryLoc * _stbl_buff_size;
            while (_parsed_entry_cnt <= i)
                ParseEntryUnit(_parsed_entry_cnt);
        }
    }
}
