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
/*                        MPEG-4 CompositionOffsetAtom Class                          */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/

/*
This atom gives difference between decoding time and composition time on each
sample basis. This atom is optional and must be present only if Decoding time
and Composition Time differ for any samples. As understood that Decoding time
is always less than composition time, the offsets are termed as unsigned
numbers such.

*/


#define IMPLEMENT_CompositionOffsetAtom

#include "compositionoffsetatom.h"
#include "atomutils.h"

#define CTTS_MIN_SAMPLE_TABLE_SIZE 4096
#define MT_SAMPLECOUNT_INCREMENT 100 //this has to be atleast 3
#define NUMBER_OF_SAMPLE_POPULATES_PER_RUNL 50
#define ENABLE_MT_LOGIC_ON_CTTS_ENTRY_COUNT_VALUE 512

// Stream-in ctor
CompositionOffsetAtom::CompositionOffsetAtom(MP4_FF_FILE *fp,
        uint32 mediaType,
        uint32 size,
        uint32 type,
        OSCL_wString& filename,
        uint32 parsingMode):
        FullAtom(fp, size, type),
        OsclTimerObject(OsclActiveObject::EPriorityNominal, "CompositionOffsetAtom")
{
    _psampleCountVec = NULL;
    _psampleOffsetVec = NULL;
    MT_SampleCount  = NULL;
    MT_EntryCount  = NULL;
    iMarkerTableCreation = false;
    MT_Table_Size = 0;

    _currGetSampleCount = 0;
    _currGetIndex = -1;
    _currGetTimeOffset = 0;
    _currPeekSampleCount = 0;
    _currPeekIndex = -1;
    _currPeekTimeOffset = 0;

    MT_Counter = 1;
    addSampleCount = 0;
    prevSampleCount = 0;
    entrycountTraversed = 0;
    refSample = MT_SAMPLECOUNT_INCREMENT;
    MT_j = 1;

    _mediaType = mediaType;
    _parsed_entry_cnt = 0;
    _fileptr = NULL;
    _parsing_mode = 0;
    _parsing_mode = parsingMode;


    _stbl_buff_size = CTTS_MIN_SAMPLE_TABLE_SIZE;
    _next_buff_number = 0;
    _curr_buff_number = 0;
    _curr_entry_point = 0;
    _stbl_fptr_vec = NULL;

    iLogger = PVLogger::GetLoggerObject("mp4ffparser");
    iStateVarLogger = PVLogger::GetLoggerObject("mp4ffparser_mediasamplestats");
    iParsedDataLogger = PVLogger::GetLoggerObject("mp4ffparser_parseddata");

    iMarkerTableCreation = false;

    /* Add this AO to the scheduler */
    if (OsclExecScheduler::Current() != NULL)
    {
        if (!IsAdded())
        {
            AddToScheduler();
        }
    }

    if (_success)
    {
        if (!AtomUtils::read32(fp, _entryCount))
        {
            _success = false;
        }

        PVMF_MP4FFPARSER_LOGPARSEDINFO((0, "CompositionOffsetAtom::CompositionOffsetAtom- _entryCount =%d", _entryCount));
        uint32 dataSize = _size - (DEFAULT_FULL_ATOM_SIZE + 4);

        uint32 entrySize = (4 + 4);

        if ((_entryCount*entrySize) > dataSize)
        {
            _success = false;
        }

        if (_success)
        {
            if (_entryCount > 0)
            {
                if (parsingMode == 1)
                {
                    // cache size is 4K so that optimization
                    // should work if entry_count is greater than 4K
                    if (_entryCount > _stbl_buff_size)
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


                        PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_stbl_buff_size), _psampleOffsetVec);
                        if (_psampleOffsetVec == NULL)
                        {
                            PV_MP4_ARRAY_DELETE(NULL, _psampleOffsetVec);
                            _psampleOffsetVec = NULL;
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }

                        for (uint32 idx = 0; idx < _stbl_buff_size; idx++)  //initialization
                        {
                            _psampleCountVec[idx] = 0;
                            _psampleOffsetVec[idx] = 0;
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
                        AtomUtils::seekFromCurrPos(fp, (TOsclFileOffset)dataSize);
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

                PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_entryCount), _psampleOffsetVec);
                if (_psampleOffsetVec == NULL)
                {
                    PV_MP4_ARRAY_DELETE(NULL, _psampleOffsetVec);
                    _psampleOffsetVec = NULL;
                    _success = false;
                    _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                    return;
                }

                for (uint32 idx = 0; idx < _entryCount; idx++)  //initialization
                {
                    _psampleCountVec[idx] = 0;
                    _psampleOffsetVec[idx] = 0;
                }

                uint32 number = 0;
                uint32 offset = 0;
                for (_parsed_entry_cnt = 0; _parsed_entry_cnt < _entryCount; _parsed_entry_cnt++)
                {
                    if (!AtomUtils::read32(fp, number))
                    {
                        _success = false;
                        break;
                    }
                    if (!AtomUtils::read32(fp, offset))
                    {
                        _success = false;
                        break;
                    }
                    _psampleCountVec[_parsed_entry_cnt] = (number);
                    _psampleOffsetVec[_parsed_entry_cnt] = (offset);
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


void CompositionOffsetAtom::SetSamplesCount(uint32 aSamplesCount)
{
    _iTotalNumSamplesInTrack = aSamplesCount;

    if (_entryCount > ENABLE_MT_LOGIC_ON_CTTS_ENTRY_COUNT_VALUE)
    {
        //Make this AO active so Run() will be called when scheduler is started
        if (OsclExecScheduler::Current() != NULL)
        {
            RunIfNotReady();
        }
    }
}

bool CompositionOffsetAtom::ParseEntryUnit(uint32 entry_cnt)
{
    const uint32 threshold = 1024;
    entry_cnt += threshold;

    if (entry_cnt > _entryCount)
    {
        entry_cnt = _entryCount;
    }

    uint32 number = 0;
    uint32 offset = 0;
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

        if (!AtomUtils::read32(_fileptr, offset))
        {
            return false;
        }
        _psampleCountVec[_curr_entry_point] = (number);
        _psampleOffsetVec[_curr_entry_point] = (offset);
        _parsed_entry_cnt++;
    }
    return true;
}

// Destructor
CompositionOffsetAtom::~CompositionOffsetAtom()
{
    if (_psampleCountVec != NULL)
        PV_MP4_ARRAY_DELETE(NULL, _psampleCountVec);

    if (_psampleOffsetVec != NULL)
        PV_MP4_ARRAY_DELETE(NULL, _psampleOffsetVec);

    if (_stbl_fptr_vec != NULL)
        PV_MP4_ARRAY_DELETE(NULL, _stbl_fptr_vec);

    DeleteMarkerTable();

    if (_fileptr != NULL)
    {
        if (_fileptr->IsOpen())
        {
            AtomUtils::CloseMP4File(_fileptr);
        }
        oscl_free(_fileptr);
    }

    if (IsAdded())
    {
        RemoveFromScheduler();
    }

}

// Return the number of samples  at index
MP4_ERROR_CODE CompositionOffsetAtom::GetSampleCountAt(uint32 aIndex, uint32& aSampleCount)
{
    MP4_ERROR_CODE retval = DEFAULT_ERROR;
    if ((aIndex < _entryCount) && _psampleCountVec)
    {
        if (_parsing_mode == 1)
            CheckAndParseEntry(aIndex);

        aSampleCount = _psampleCountVec[aIndex%_stbl_buff_size];
        retval = EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>CompositionOffsetAtom::getSampleCountAt aIndex = %d", aIndex));
    }
    return retval;
}

MP4_ERROR_CODE
CompositionOffsetAtom::GetSampleOffsetAt(uint32 aIndex, uint32& aSampleOffset)
{
    MP4_ERROR_CODE retval = DEFAULT_ERROR;
    if ((aIndex < _entryCount) && _psampleOffsetVec)
    {
        if (_parsing_mode == 1)
            CheckAndParseEntry(aIndex);

        aSampleOffset = _psampleOffsetVec[aIndex%_stbl_buff_size];
        retval = EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>CompositionOffsetAtom::getSampleOffsetAt aIndex = %d", aIndex));
    }
    return retval;
}

// Return sample offset at index
MP4_ERROR_CODE CompositionOffsetAtom::GetTimeOffsetForSampleNumberPeek(uint32 aSampleNumber, uint32& aTimeOffset)
{
    MP4_ERROR_CODE retval = DEFAULT_ERROR;
    // It is assumed that sample 0 has a ts of 0 - i.e. the first
    // entry in the table starts with the delta between sample 1 and sample 0
    if ((_psampleOffsetVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount == 0))
    {
        return retval;
    }

    // note that aSampleNumber is a zero based index while _currGetSampleCount is 1 based index
    if (aSampleNumber < _currPeekSampleCount)
    {
        aTimeOffset = _currPeekTimeOffset;
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
            _currPeekTimeOffset    = _psampleOffsetVec[_currPeekIndex%_stbl_buff_size];
        }
        while (_currPeekSampleCount == 0);

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimeOffsetForSampleNumberPeek- _currPeekIndex =%d", _currPeekIndex));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimeOffsetForSampleNumberPeek- _currPeekSampleCount =%d", _currPeekSampleCount));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimeOffsetForSampleNumberPeek- _currPeekTimeOffset =%d", _currPeekTimeOffset));

        if (aSampleNumber < _currPeekSampleCount)
        {
            aTimeOffset = _currPeekTimeOffset;
            retval = EVERYTHING_FINE;
        }
        else
        {
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>CompositionOffsetAtom::getTimeOffsetForaSampleNumberberPeek aSampleNumber = %d", aSampleNumber));
        }
    }
    return retval;
}

// This is the most widely used API
// Returns the offset (ms) for the  sample given by num.  This is used when
// randomly accessing a frame and the timestamp has not been accumulated.
MP4_ERROR_CODE CompositionOffsetAtom::GetTimeOffsetForSampleNumberGet(uint32 aSampleNum, uint32& aTimeOffset)
{
    MP4_ERROR_CODE retval = DEFAULT_ERROR;
    if ((_psampleOffsetVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount == 0))
    {
        return retval;
    }

    // note that aSampleNum is a zero based index while _currGetSampleCount is 1 based index
    if (aSampleNum < _currGetSampleCount)
    {
        aTimeOffset = _currGetTimeOffset;
        retval = EVERYTHING_FINE;
    }
    else
    {

        do
        {
            _currGetIndex++;
            if (_parsing_mode)
                CheckAndParseEntry(_currGetIndex);
            _currGetSampleCount += _psampleCountVec[_currGetIndex%_stbl_buff_size];
            _currGetTimeOffset    = _psampleOffsetVec[_currGetIndex%_stbl_buff_size];
        }
        while (_currGetSampleCount == 0);

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimeOffsetForaSampleNumberGet- _currGetIndex =%d", _currGetIndex));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimeOffsetForaSampleNumberGet- _currGetSampleCount =%d", _currGetSampleCount));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimeOffsetForaSampleNumberGet- _currGetTimeOffset =%d", _currGetTimeOffset));

        if (aSampleNum < _currGetSampleCount)
        {
            aTimeOffset = _currGetTimeOffset;
            retval = EVERYTHING_FINE;
        }
        else
        {
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>TimeToSampleAtom::getTimeDeltaForaSampleNumberGet aSampleNum = %d", aSampleNum));
        }
    }
    return retval;
}

MP4_ERROR_CODE CompositionOffsetAtom::GetTimeOffsetFromMT(uint32 aSampleNum, uint32 aCurrEC, uint32 aCurrSampleCount, uint32& aTimeOffset)
{
    MP4_ERROR_CODE retval = DEFAULT_ERROR;
    if ((_psampleOffsetVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount == 0))
    {
        return retval;
    }

    if (aSampleNum < aCurrSampleCount)
    { // Sample num within current entry
        if (_parsing_mode == 1)
            CheckAndParseEntry(aCurrEC);

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimestampForaSampleNumber- Time StampOffset = %d", _psampleOffsetVec[aCurrEC%_stbl_buff_size]));
        aTimeOffset = _psampleOffsetVec[aCurrEC%_stbl_buff_size];
        retval = EVERYTHING_FINE;
    }
    else
    {

        for (uint32 i = aCurrEC + 1; i < _entryCount; i++)
        {
            if (_parsing_mode == 1)
                CheckAndParseEntry(i);


            if (aSampleNum < (aCurrSampleCount + _psampleCountVec[i%_stbl_buff_size]))
            {
                PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimestampForaSampleNumber- Time StampOffset = %d", _psampleOffsetVec[i%_stbl_buff_size]));
                aTimeOffset = _psampleOffsetVec[i%_stbl_buff_size];
                retval = EVERYTHING_FINE;
            }
            else
            {
                aCurrSampleCount += _psampleCountVec[i%_stbl_buff_size];
            }
        }
    }

    // Went past end of list - not a valid sample number
    return retval;
}

MP4_ERROR_CODE CompositionOffsetAtom::GetTimeOffsetForSampleNumber(uint32 aSampleNum, uint32& aTimeOffset)
{
    if (true == iMarkerTableCreation)
    {
        uint32 currSample = 0;
        uint32 currEC = 0;
        uint32 MT_EC = aSampleNum / (MT_SAMPLECOUNT_INCREMENT - 1); //where MT_SAMPLECOUNT_INCREMENT is the granuality of sample separation in Marker Table

        if (MT_EC > ((_iTotalNumSamplesInTrack / MT_SAMPLECOUNT_INCREMENT) - 1))
        {
            //boundary check, MT_EC valid value will always be between 0 to (_samples_count/MT_SAMPLECOUNT_INCREMENT)-1
            MT_EC = (_iTotalNumSamplesInTrack  / MT_SAMPLECOUNT_INCREMENT) - 1;
        }
        //assign the last marker entry count created till now to look for sample from this location onwards instead of from start
        if (MT_EC > MT_Counter)
            MT_EC = MT_Counter;

        while ((aSampleNum < MT_SampleCount[MT_EC]) && (MT_EC > 0))
        {
            //This check was put keeping in mind that it may happen (due to rounding off error),
            //that we choose a MT_EC greater than desired. So to avoid such a scenario.
            MT_EC = MT_EC - 1;
            currSample = MT_SampleCount[MT_EC];
            currEC = MT_EntryCount[MT_EC];
        }

        currSample = MT_SampleCount[MT_EC];
        currEC = MT_EntryCount[MT_EC];

        GetTimeOffsetFromMT(aSampleNum, currEC, currSample, aTimeOffset);
        return EVERYTHING_FINE;
    }

    if (0 == _psampleOffsetVec || 0 == _psampleCountVec || 0 == _entryCount)
        return DEFAULT_ERROR;

    uint32 sampleCount = 0;

    for (uint32 i = 0; i < _entryCount; ++i)
    {
        if (1 == _parsing_mode)
            CheckAndParseEntry(i);

        if (aSampleNum >= (sampleCount + _psampleCountVec[i%_stbl_buff_size]))
        {
            sampleCount += _psampleCountVec[i%_stbl_buff_size];
            continue;
        }

        // sample at 'aSampleNum' is within current vector element

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "CompositionOffsetAtom::getTimestampForSampleaSampleNumber- Time StampOffset = %d", _psampleOffsetVec[i%_stbl_buff_size]));
        aTimeOffset = _psampleOffsetVec[i%_stbl_buff_size];
        return EVERYTHING_FINE;
    }

    return DEFAULT_ERROR;
}

MP4_ERROR_CODE CompositionOffsetAtom::ResetStateVariables()
{
    uint32 sampleNum = 0;
    return (ResetStateVariables(sampleNum));
}

MP4_ERROR_CODE CompositionOffsetAtom::ResetStateVariables(uint32 sampleNum)
{
    MP4_ERROR_CODE retval = DEFAULT_ERROR;
    _currGetSampleCount = 0;
    _currGetIndex = -1;
    _currGetTimeOffset = 0;
    _currPeekSampleCount = 0;
    _currPeekIndex = -1;
    _currPeekTimeOffset = 0;

    // It is assumed that sample 0 has a ts of 0 - i.e. the first
    // entry in the table starts with the delta between sample 1 and sample 0
    if ((_psampleOffsetVec == NULL) ||
            (_psampleCountVec == NULL) ||
            (_entryCount == 0))
    {
        return retval;
    }

    if (_parsing_mode)
    {
        if (_parsed_entry_cnt == 0)
        {
            ParseEntryUnit(sampleNum);
        }
    }

    for (uint32 i = 0; i < _entryCount; i++)
    {
        _currPeekIndex++;
        _currPeekSampleCount += _psampleCountVec[i%_stbl_buff_size];
        _currPeekTimeOffset    = _psampleOffsetVec[i%_stbl_buff_size];

        _currGetIndex++;
        _currGetSampleCount += _psampleCountVec[i%_stbl_buff_size];
        _currGetTimeOffset    = _psampleOffsetVec[i%_stbl_buff_size];

        if (sampleNum <= _currPeekSampleCount)
        {
            retval = EVERYTHING_FINE;
            break;
        }

    }

    // Went past end of list - not a valid sample number
    return retval;
}

MP4_ERROR_CODE CompositionOffsetAtom::ResetPeekwithGet()
{
    _currPeekSampleCount = _currGetSampleCount;
    _currPeekIndex = _currGetIndex;
    _currPeekTimeOffset = _currGetTimeOffset;
    return EVERYTHING_FINE;
}

void CompositionOffsetAtom::CheckAndParseEntry(uint32 i)
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

void CompositionOffsetAtom::Run()
{
    // Create it for the first time
    if ((MT_SampleCount == NULL) && (MT_EntryCount == NULL))
    {
        PVMFStatus status = CreateMarkerTable();
        if (status == PVMFFailure)
        {
            OSCL_LEAVE(OsclErrNoMemory);
        }
        iMarkerTableCreation = true;
    }

    PopulateMarkerTable();

    // check for entry count being exhausted.. table iterated completely
    if ((entrycountTraversed < _entryCount) && (refSample < _iTotalNumSamplesInTrack)
            && (MT_Counter < _iTotalNumSamplesInTrack / MT_SAMPLECOUNT_INCREMENT))
    {
        RunIfNotReady();
    }

    return;

}

//Populate the Marker Table with SampleCount and EntryCount values
//for every nth sample starting from n-1, where n=MT_SAMPLECOUNT_INCREMENT.
uint32 CompositionOffsetAtom::PopulateMarkerTable()
{
    uint32 increment = MT_SAMPLECOUNT_INCREMENT;
    uint32 numPopulated = 0;

    for (uint32 i = entrycountTraversed; i < _entryCount; i++)
    {
        if (refSample < _iTotalNumSamplesInTrack)
        {
            if (i == 0)
            {
                if (_parsing_mode == 1)
                    CheckAndParseEntry(i);

                MT_SampleCount[0] = _psampleCountVec[i%_stbl_buff_size];
                prevSampleCount = MT_SampleCount[0];
                addSampleCount = MT_SampleCount[0];
            }
            else if (addSampleCount < refSample)
            {
                if (_parsing_mode == 1)
                    CheckAndParseEntry(MT_j);

                prevSampleCount = addSampleCount;
                addSampleCount += _psampleCountVec[MT_j%_stbl_buff_size];
                MT_j++;
            }
            else
            {
                entrycountTraversed = i - 1;
                i = i - 1;
                refSample += increment;
                MT_SampleCount[MT_Counter] = prevSampleCount;
                //Incase SampleCounts have same value for consecutive MT entries,
                //even the same EntryCount should be mentioned in the Marker Table.
                if (MT_SampleCount[MT_Counter] == MT_SampleCount[MT_Counter-1])
                {
                    MT_EntryCount[MT_Counter] = MT_EntryCount[MT_Counter-1];
                }
                else
                {
                    MT_EntryCount[MT_Counter] = MT_j - 2;
                }


                MT_Counter++;
                numPopulated++;
                if ((numPopulated == NUMBER_OF_SAMPLE_POPULATES_PER_RUNL) ||
                        (MT_Counter >= _iTotalNumSamplesInTrack / MT_SAMPLECOUNT_INCREMENT))
                    break;
            }
        }
        else
        {
            break;
        }

    }

    return numPopulated;

}

void CompositionOffsetAtom::DeleteMarkerTable()
{
    if (MT_SampleCount != NULL)
        PV_MP4_ARRAY_DELETE(NULL, MT_SampleCount);

    if (MT_EntryCount != NULL)
        PV_MP4_ARRAY_DELETE(NULL, MT_EntryCount);
}
// responsible for creation of data sturcture inside the table.
PVMFStatus CompositionOffsetAtom::CreateMarkerTable()
{

    MT_Table_Size = (_iTotalNumSamplesInTrack / MT_SAMPLECOUNT_INCREMENT);
    PV_MP4_FF_ARRAY_NEW(NULL, uint32, (MT_Table_Size), MT_SampleCount);

    if (MT_SampleCount == NULL)
    {
        PV_MP4_ARRAY_DELETE(NULL, MT_SampleCount);
        MT_SampleCount = NULL;
        _success = false;
        _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
        return PVMFFailure;
    }

    PV_MP4_FF_ARRAY_NEW(NULL, uint32, (MT_Table_Size), MT_EntryCount);

    if (MT_EntryCount == NULL)
    {
        PV_MP4_ARRAY_DELETE(NULL, MT_EntryCount);
        MT_EntryCount = NULL;
        PV_MP4_ARRAY_DELETE(NULL, MT_SampleCount);
        MT_SampleCount = NULL;
        _success = false;
        _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
        return PVMFFailure;
    }

    for (uint32 idx = 0; idx < MT_Table_Size; idx++)  //initialization
    {

        MT_EntryCount[idx] = 0;
        MT_SampleCount[idx] = 0;
    }

    return PVMFSuccess;
}

