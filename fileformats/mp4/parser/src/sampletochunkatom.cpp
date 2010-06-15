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
/*                         MPEG-4 SampleToChunkAtom Class                        */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    This SampleSizeAtom Class contains the sample count and a table giving the
    size of each sample.
*/


#define IMPLEMENT_SampleToChunkAtom_H__

#include "sampletochunkatom.h"
#include "atomutils.h"
#include "atomdefs.h"

#define DEFAULT_MAX_NUM_SAMPLES_PER_CHUNK                20
#define DEFAULT_MAX_CHUNK_DATA_SIZE                   10240; // 10KB

// Stream-in ctor
// Create and return a new SampleToChunkAtom by reading in from an ifstream
SampleToChunkAtom::SampleToChunkAtom(MP4_FF_FILE *fp, uint32 size, uint32 type, OSCL_wString& filename, uint32 parsingMode)
        : FullAtom(fp, size, type)
{
    _pfirstChunkVec = NULL;
    _psamplesPerChunkVec = NULL;
    _psampleDescriptionIndexVec = NULL;

    _Index = 0;
    _numChunksInRun = 0;
    _majorGetIndex = 0;
    _currGetChunk = -1;
    _numGetChunksInRun = 0;
    _currGetSampleCount  = 0;
    _firstGetSampleInCurrChunk = 0;
    _numGetSamplesPerChunk = 0;
    _currGetSDI = 0;

    _majorPeekIndex = 0;
    _currPeekChunk = -1;
    _numPeekChunksInRun = 0;
    _currPeekSampleCount  = 0;
    _firstPeekSampleInCurrChunk = 0;
    _numPeekSamplesPerChunk = 0;
    _currPeekSDI = 0;

    _parsed_entry_cnt = 0;
    _fileptr = NULL;

    _stbl_buff_size = MAX_CACHED_TABLE_ENTRIES_FILE;
    _next_buff_number = 0;
    _curr_buff_number = 0;
    _curr_entry_point = 0;
    _stbl_fptr_vec = NULL;
    _parsing_mode = parsingMode;

    iLogger = PVLogger::GetLoggerObject("mp4ffparser");
    iStateVarLogger = PVLogger::GetLoggerObject("mp4ffparser_mediasamplestats");
    iParsedDataLogger = PVLogger::GetLoggerObject("mp4ffparser_parseddata");

    if (_success)
    {
        _currentChunkNumber = 0;
        _maxNumSamplesPerChunk = DEFAULT_MAX_NUM_SAMPLES_PER_CHUNK;
        _maxChunkDataSize = DEFAULT_MAX_CHUNK_DATA_SIZE;

        if (!AtomUtils::read32(fp, _entryCount))
        {
            _success = false;
        }
        PVMF_MP4FFPARSER_LOGPARSEDINFO((0, "SampleToChunkAtom::SampleToChunkAtom- _entryCount =%d", _entryCount));
        TOsclFileOffset dataSize = _size - (DEFAULT_FULL_ATOM_SIZE + 4);

        uint32 entrySize = (4 + 4 + 4);

        if ((TOsclFileOffset)(_entryCount*entrySize) > dataSize)
        {
            _success = false;
        }

        if (_success)
        {
            if (_entryCount > 0)
            {
                if (_parsing_mode)
                {
                    if ((_entryCount > _stbl_buff_size)) // cahce size is 4K so that optimization should work if entry_count is greater than 4K
                    {

                        uint32 fptrBuffSize = (_entryCount / _stbl_buff_size) + 1;

                        PV_MP4_FF_ARRAY_NEW(NULL, TOsclFileOffset, (fptrBuffSize), _stbl_fptr_vec);
                        if (_stbl_fptr_vec == NULL)
                        {
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }

                        PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_stbl_buff_size), _pfirstChunkVec);
                        if (_pfirstChunkVec == NULL)
                        {
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }

                        PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_stbl_buff_size), _psamplesPerChunkVec);
                        if (_psamplesPerChunkVec == NULL)
                        {
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }
                        PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_stbl_buff_size), _psampleDescriptionIndexVec);
                        if (_psampleDescriptionIndexVec == NULL)
                        {
                            _success = false;
                            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                            return;
                        }

                        {
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
                        }
                        TOsclFileOffset _head_offset = AtomUtils::getCurrentFilePosition(fp);
                        AtomUtils::seekFromCurrPos(fp, dataSize);
                        AtomUtils::seekFromStart(_fileptr, _head_offset);

                        ParseEntryUnit(0);
                        uint32 firstsamplenum = 0;
                        resetStateVariables(firstsamplenum);

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
                    _parsing_mode = 0;
                    _stbl_buff_size = _entryCount;
                }

                PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_entryCount), _pfirstChunkVec);
                PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_entryCount), _psamplesPerChunkVec);
                PV_MP4_FF_ARRAY_NEW(NULL, uint32, (_entryCount), _psampleDescriptionIndexVec);

                uint32 firstChunk;
                uint32 samplesPerChunk;
                uint32 sampleDescrIndex;

                uint32 offSet = 0;

                uint32 prevFirstChunk = 0;
                uint32 j = 0;

                for (uint32 i = 0; i < _entryCount; i++)
                {
                    if (!AtomUtils::read32(fp, firstChunk))
                    {
                        _success = false;
                        break;
                    }

                    if (i == 0)
                        offSet = firstChunk;

                    if (!AtomUtils::read32(fp, samplesPerChunk))
                    {
                        _success = false;
                        break;
                    }
                    if (!AtomUtils::read32(fp, sampleDescrIndex))
                    {
                        _success = false;
                        break;
                    }

                    if (firstChunk > prevFirstChunk)
                    {
                        _pfirstChunkVec[j] = (firstChunk - offSet);
                        _psamplesPerChunkVec[j] = (samplesPerChunk);
                        _psampleDescriptionIndexVec[j] = (sampleDescrIndex);
                        prevFirstChunk = firstChunk;
                        j++;
                    }
                }
                _entryCount = j;
                uint32 firstsamplenum = 0;
                resetStateVariables(firstsamplenum);
            }
            else
            {
                _pfirstChunkVec = NULL;
                _psamplesPerChunkVec = NULL;
                _psampleDescriptionIndexVec = NULL;
            }
        }

        if (!_success)
        {
            _mp4ErrorCode = READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleToChunkAtom::SampleToChunkAtom- Read SampleToChunk Atom failed %d", _mp4ErrorCode));
        }
    }
    else
    {
        if (_mp4ErrorCode != ATOM_VERSION_NOT_SUPPORTED)
        {
            _mp4ErrorCode = READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleToChunkAtom::SampleToChunkAtom- Read SampleToChunk Atom failed %d", _mp4ErrorCode));
        }
    }
}
bool SampleToChunkAtom::ParseEntryUnit(uint32 sample_cnt)
{

    uint32 prevFirstChunk = 0;


    const uint32 threshold = TABLE_ENTRIES_THRESHOLD_FILE;
    sample_cnt += threshold;

    if (sample_cnt > _entryCount)
        sample_cnt = _entryCount;

    while (_parsed_entry_cnt < sample_cnt)
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
        uint32 firstChunk;
        uint32 samplesPerChunk;
        uint32 sampleDescrIndex;

        if (!AtomUtils::read32(_fileptr, firstChunk))
        {
            _success = false;
            break;
        }
        uint32 offSet = 1;
        if (_parsed_entry_cnt == 0)
            offSet = firstChunk;

        if (!AtomUtils::read32(_fileptr, samplesPerChunk))
        {
            _success = false;
            break;
        }
        if (!AtomUtils::read32(_fileptr, sampleDescrIndex))
        {
            _success = false;
            break;
        }
        if (firstChunk > prevFirstChunk)
        {
            _pfirstChunkVec[_curr_entry_point] = (firstChunk - offSet);
            _psamplesPerChunkVec[_curr_entry_point] = (samplesPerChunk);
            _psampleDescriptionIndexVec[_curr_entry_point] = (sampleDescrIndex);
            _parsed_entry_cnt++;
            prevFirstChunk = firstChunk;
        }
    }
    return true;
}

SampleToChunkAtom::~SampleToChunkAtom()
{
    if (_pfirstChunkVec != NULL)
    {
        PV_MP4_ARRAY_DELETE(NULL, _pfirstChunkVec);
    }
    if (_psamplesPerChunkVec != NULL)
    {
        PV_MP4_ARRAY_DELETE(NULL, _psamplesPerChunkVec);
    }
    if (_psampleDescriptionIndexVec != NULL)
    {
        PV_MP4_ARRAY_DELETE(NULL, _psampleDescriptionIndexVec);
    }
    if (_fileptr != NULL)
    {
        if (_fileptr->IsOpen())
        {
            AtomUtils::CloseMP4File(_fileptr);
        }

        oscl_free(_fileptr);
    }
    if (_stbl_fptr_vec != NULL)
        PV_MP4_ARRAY_DELETE(NULL, _stbl_fptr_vec);

}

// Returns the chunk number of the first chunk in run[index]
MP4_ERROR_CODE
SampleToChunkAtom::getFirstChunkAt(uint32 index, int32& pos)
{
    if (_pfirstChunkVec == NULL)
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }
    if (index < _entryCount)
    {
        if (_parsing_mode == 1)
        {
            CheckAndParseEntry(index);
        }
        pos = (_pfirstChunkVec[index%_stbl_buff_size]);
        return EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleToChunkAtom::getFirstChunkAt index = %d", index));
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }
}

// Returns the samples per chunk of all the chunks in run[index]
MP4_ERROR_CODE
SampleToChunkAtom::getSamplesPerChunkAt(uint32 index, int32& SamplesPerChunk)
{
    if (_psamplesPerChunkVec == NULL)
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }
    if (index < _entryCount)
    {
        if (_parsing_mode == 1)
        {
            CheckAndParseEntry(index);
        }
        SamplesPerChunk = (_psamplesPerChunkVec[index%_stbl_buff_size]);
        return EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleToChunkAtom::getSamplesPerChunkAt index = %d", index));
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

}

// Returns the samples description index for the samples in all the chunks in run[index]
MP4_ERROR_CODE
SampleToChunkAtom::getSDIndex(uint32& SDIndex) const
{
    if (_psampleDescriptionIndexVec == NULL)
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    if (_Index < _entryCount)
    {
        SDIndex = (_psampleDescriptionIndexVec[_Index%_stbl_buff_size]);
        return EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getSDIndex"));
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }
}

// Returns the chunk number for the given sample number
MP4_ERROR_CODE
SampleToChunkAtom::getChunkNumberForSampleGet(uint32 sampleNum, uint32& ChunkNumber)
{
    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    if (_parsing_mode == 1)
    {
        CheckAndParseEntry(_majorGetIndex);
    }

    if (sampleNum < _currGetSampleCount)
    {
        ChunkNumber = _currGetChunk;
        return EVERYTHING_FINE;
    }
    else if (_numGetChunksInRun > 1)
    {
        _firstGetSampleInCurrChunk = _currGetSampleCount;
        _currGetSampleCount += _numGetSamplesPerChunk;
        _currGetChunk++;

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _firstGetSampleInCurrChunk =%d", _firstGetSampleInCurrChunk));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _currGetSampleCount =%d", _currGetSampleCount));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _currGetChunk =%d", _currGetChunk));

        // to handle special case, every sample is a chunk
        if (_entryCount > 1)
        {
            _numGetChunksInRun--;
        }

        if (sampleNum < _currGetSampleCount)
        {
            ChunkNumber = _currGetChunk;
            return EVERYTHING_FINE;
        }
        else
        {
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getChunkNumberForSampleGet sampleNum= %d", sampleNum));
            return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
        }
    }
    else if (_numGetChunksInRun <= 1)
    {
        if (_majorGetIndex < (int32)(_entryCount - 1))
        {
            uint32 prevFirstChunk = _pfirstChunkVec[_majorGetIndex%_stbl_buff_size];
            _numGetSamplesPerChunk = _psamplesPerChunkVec[_majorGetIndex%_stbl_buff_size];
            _currGetSDI = _psampleDescriptionIndexVec[_majorGetIndex%_stbl_buff_size];

            if (_parsing_mode == 1)
            {
                CheckAndParseEntry(_majorGetIndex + 1);
            }

            uint32 nextFirstChunk = _pfirstChunkVec[(_majorGetIndex+1)%_stbl_buff_size];
            _numGetChunksInRun = nextFirstChunk - prevFirstChunk;
            _numChunksInRun = _numGetChunksInRun;


            _majorGetIndex++;
            _firstGetSampleInCurrChunk = _currGetSampleCount;
            _currGetSampleCount    += _numGetSamplesPerChunk;
            _currGetChunk++;

            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _numGetChunksInRun =%d", _numGetChunksInRun));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _numGetSamplesPerChunk =%d", _numGetSamplesPerChunk));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _currGetSampleCount =%d", _currGetSampleCount));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _currGetChunk =%d", _currGetChunk));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _majorGetIndex =%d", _majorGetIndex));

            if (sampleNum < _currGetSampleCount)
            {
                ChunkNumber = _currGetChunk;
                return EVERYTHING_FINE;
            }
            else
            {
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getChunkNumberForSampleGet sampleNum= %d", sampleNum));
                return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
            }
        }
        else if (_majorGetIndex == (int32)(_entryCount - 1))
        {
            // Last run of chunks
            _numGetChunksInRun = 1;

            _numChunksInRun = _numGetChunksInRun;

            _currGetSDI = _psampleDescriptionIndexVec[_majorGetIndex%_stbl_buff_size];

            _numGetSamplesPerChunk =
                _psamplesPerChunkVec[_majorGetIndex%_stbl_buff_size];

            _firstGetSampleInCurrChunk = _currGetSampleCount;

            _currGetSampleCount += _numGetSamplesPerChunk;
            _currGetChunk++;

            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _numGetSamplesPerChunk =%d", _firstGetSampleInCurrChunk));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _firstGetSampleInCurrChunk =%d", _firstGetSampleInCurrChunk));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _currGetSampleCount =%d", _currGetSampleCount));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSampleGet- _currGetChunk =%d", _currGetChunk));

            if (sampleNum < _currGetSampleCount)
            {
                ChunkNumber = _currGetChunk;
                return EVERYTHING_FINE;
            }
            else
            {
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getChunkNumberForSampleGet sampleNum= %d", sampleNum));
                return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
            }
        }
        else
        {
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getChunkNumberForSampleGet _majorGetIndex = %d _entryCount= %d", _majorGetIndex, _entryCount));
            return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
        }
    }

    return READ_SAMPLE_TO_CHUNK_ATOM_FAILED; // Should never get here
}

// Returns the chunk number for the given sample number
MP4_ERROR_CODE
SampleToChunkAtom::getChunkNumberForSample(uint32 sampleNum, uint32& ChunkNumber)
{
    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    uint32 sampleCount = 0;

    for (uint32 i = 0; i < _entryCount; i++)
    {
        uint32 chunkNum = 0;
        uint32 samplesPerChunkInRun = 0;

        if (_parsing_mode == 1)
        {
            CheckAndParseEntry(i);
        }
        chunkNum = _pfirstChunkVec[i%_stbl_buff_size];
        samplesPerChunkInRun = _psamplesPerChunkVec[i%_stbl_buff_size];

        if ((i + 1) < _entryCount)
        {
            if (_parsing_mode == 1)
            {
                CheckAndParseEntry(i + 1);
            }

            uint32 nextChunkNum = _pfirstChunkVec[(int32)((i+1)%_stbl_buff_size)];
            uint32 numChunksInRun = nextChunkNum - chunkNum;
            uint32 count = sampleCount + samplesPerChunkInRun * numChunksInRun;

            if (count < sampleNum)
            { // Haven't found chunk yet - running count still less than sampleNum
                sampleCount = count;
                continue;
            }
            else
            { // Found run of chunks in which sample lies - now find actual chunk
                for (int32 j = 0; j < (int32)numChunksInRun; j++)
                {
                    sampleCount += samplesPerChunkInRun; //  samples for jth chunk
                    if (sampleNum < sampleCount)
                    { // Found specific chunk
                        _Index = i;
                        ChunkNumber = chunkNum + j; // Return jth chunk in run
                        return EVERYTHING_FINE;
                    }
                }
            }
        }
        else
        { // Last run of chunks - simply find specific chunk
            int k = 0;
            while (sampleNum >= sampleCount)
            {
                // Continue until find chunk number - we do not know how many chunks are in
                // this final run so keep going til we find a number
                sampleCount += samplesPerChunkInRun;
                if (sampleNum < sampleCount)
                {
                    // Found specific chunk
                    _Index = i;
                    ChunkNumber =  chunkNum + k; // Return ith chunk in run
                    // Since we do not actually know how many chunk are in this last run,
                    // the chunkNum that is returned may not be a valid chunk!
                    // This is handled in the exception handling in the chunkOffset atom
                    return EVERYTHING_FINE;
                }
                k++;
            }
        }
    }
    return READ_SAMPLE_TO_CHUNK_ATOM_FAILED; // Should never get here
}


// Returns the sampleNum of the first sample in chunk with chunk number 'chunkNum'
// Note that since the coding of this table does not indicate the total number of
// chunks (i.e. don't know how many chunks in last run) this method may return a
// sample number that is not valid.  This should be taken care of in the exception
// handling in the chunkoffset and samplesize atoms
MP4_ERROR_CODE
SampleToChunkAtom::getFirstSampleNumInChunk(uint32 chunkNum, uint32& SampleNum)
{
    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED; // Error condition
    }

    uint32 firstChunkCurrentRun = 0; // chunk number of first chunk in this run
    uint32 firstChunkNextRun = 0; // chunk number of first chunk in next run
    uint32 firstSample = 0; // number of first sample in the run of chunks in which chunk 'chunkNum' lies
    // once we find the correct run, this value holds the sample number of the first
    // sample in chunk 'chunkNum'
    uint32 samplesInRun = 0;    // Number of samples in the entire run of chunks (not just in each chunk)

    for (uint32 i = 0; i < _entryCount; i++)
    {
        // Go through vector of first chunks in runs

        if (_parsing_mode == 1)
        {
            CheckAndParseEntry(i);
        }

        firstChunkCurrentRun = _pfirstChunkVec[i%_stbl_buff_size]; // Get first chunk number for run i

        if (chunkNum < firstChunkCurrentRun)
        {
            // Chunk is in previous run of chunks
            firstSample -= samplesInRun; // Backtrack to first sample of last run

            // Now need to find specific chunk and sample in this run
            if (_parsing_mode == 1)
            {
                CheckAndParseEntry(i - 1);
            }

            firstChunkCurrentRun = _pfirstChunkVec[(int32)((i-1)%_stbl_buff_size)];  // Backtrack to  last run
            uint32 samplesPerChunk = _psamplesPerChunkVec[(int32)((i-1)%_stbl_buff_size)];

            uint32 chunkOffset = chunkNum - firstChunkCurrentRun; // Offset from firstChunk
            uint32 sampleOffset = chunkOffset * samplesPerChunk;
            firstSample += sampleOffset;

            SampleNum = firstSample;
            return EVERYTHING_FINE;
        }
        else if (chunkNum == firstChunkCurrentRun)
        {
            // Requested chunk is first in this run
            SampleNum = firstSample; // Return first sample in this run
            return EVERYTHING_FINE;
        }
        else
        {
            // Haven't found chunk in run

            if ((i + 1) < _entryCount)
            {
                if (_parsing_mode == 1)
                {
                    CheckAndParseEntry(i + 1);
                }

                firstChunkNextRun = _pfirstChunkVec[(int32)(((i+1)%_stbl_buff_size))];  // If we are out of range of the vector
                // This means we are in the last run of chunks
                int32 numChunks = firstChunkNextRun - firstChunkCurrentRun;
                samplesInRun = _psamplesPerChunkVec[i%_stbl_buff_size] * numChunks; // Once you advance, this value maintains the
                // number of samples in the previous run
                firstSample += samplesInRun;
            }
            else
            {
                // In last run of chunks - we know the chunk is here
                // Now need to find specific chunk and sample

                firstChunkCurrentRun = _pfirstChunkVec[i%_stbl_buff_size]; // Get first chunk number for this run
                uint32 samplesPerChunk = _psamplesPerChunkVec[i%_stbl_buff_size];

                uint32 chunkOffset = chunkNum - firstChunkCurrentRun; // Offset from firstChunk
                uint32 sampleOffset = chunkOffset * samplesPerChunk;
                firstSample += sampleOffset;

                SampleNum = firstSample;
                return EVERYTHING_FINE;
            }
        }
    }

    return READ_SAMPLE_TO_CHUNK_ATOM_FAILED; // Error condition
}

MP4_ERROR_CODE
SampleToChunkAtom::getNumChunksInRunofChunks(uint32 chunk, uint32& NumChunks)
{
    if (_pfirstChunkVec == NULL)
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    if ((chunk + 1) < _entryCount)
    {
        for (uint32 i = 0; i < _entryCount; i++)
        {
            if (_parsing_mode == 1)
            {
                CheckAndParseEntry(i);
            }

            if (_pfirstChunkVec[i%_stbl_buff_size] < chunk)
            {
                continue;
            }
            else if (_pfirstChunkVec[i%_stbl_buff_size] == chunk)
            {
                uint32 chunkNum = _pfirstChunkVec[(int32)(i%_stbl_buff_size)];
                if (_parsing_mode == 1)
                {
                    CheckAndParseEntry(i + 1);
                }
                uint32 nextChunkNum = _pfirstChunkVec[(int32)((i+1)%_stbl_buff_size)];
                NumChunks = (nextChunkNum - chunkNum);
                return EVERYTHING_FINE;
            }
            else if (_pfirstChunkVec[(i%_stbl_buff_size)] > chunk)
            {
                NumChunks = (_pfirstChunkVec[(i%_stbl_buff_size)] - chunk);
                return EVERYTHING_FINE;
            }
        }
    }
    else
    {
        NumChunks = 1;
        return EVERYTHING_FINE;
    }

    return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
}

MP4_ERROR_CODE
SampleToChunkAtom::getSamplesPerChunkCorrespondingToSample(uint32 sampleNum, uint32& SamplesPerChunk)
{
    uint32 sampleCount = 0;

    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    for (uint32 i = 0; i < _entryCount; i++)
    {
        if (_parsing_mode == 1)
        {
            CheckAndParseEntry(i);
        }

        uint32 chunkNum = 0;
        uint32 samplesPerChunkInRun = 0;

        chunkNum = _pfirstChunkVec[(i%_stbl_buff_size)];
        samplesPerChunkInRun = _psamplesPerChunkVec[(i%_stbl_buff_size)];

        if ((i + 1) < _entryCount)
        {
            if (_parsing_mode == 1)
            {
                CheckAndParseEntry(i + 1);
            }

            uint32 nextChunkNum = _pfirstChunkVec[(int32)(((i+1)%_stbl_buff_size))];
            uint32 numChunksInRun = nextChunkNum - chunkNum;
            uint32 count = sampleCount + samplesPerChunkInRun * numChunksInRun;

            if (count < sampleNum)
            { // Haven't found chunk yet - running count still less than sampleNum
                sampleCount = count;
                continue;
            }
            else
            { // Found run of chunks in which sample lies - now find actual chunk
                for (int32 j = 0; j < (int32)numChunksInRun; j++)
                {
                    sampleCount += samplesPerChunkInRun; //  samples for jth chunk
                    if (sampleNum < sampleCount)
                    { // Found specific chunk
                        SamplesPerChunk = (samplesPerChunkInRun);
                        return EVERYTHING_FINE;
                    }
                }
            }
        }
        else
        {
            // Last run of chunks - simply find specific chunk
            int k = 0;
            int for_ever = 1;
            while (for_ever)
            {
                // Continue until find chunk number - we do not know how many chunks are in
                // this final run so keep going til we find a number
                sampleCount += samplesPerChunkInRun;
                if (sampleNum < sampleCount)
                { // Found specific chunk
                    SamplesPerChunk = (samplesPerChunkInRun);
                    // Since we do not actually know how many chunk are in this last run,
                    // the chunkNum that is returned may not be a valid chunk!
                    // This is handled in the exception handling in the chunkOffset atom
                    return EVERYTHING_FINE;
                }
                k++;
            }
        }
    }
    return READ_SAMPLE_TO_CHUNK_ATOM_FAILED; // Should never get here
}


// Returns the chunk number for the given sample number
MP4_ERROR_CODE
SampleToChunkAtom::getSDIndexPeek(uint32 &SDIndex) const
{
    if (_psampleDescriptionIndexVec == NULL)
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    if (_currPeekSDI != 0)
    {
        SDIndex = (_currPeekSDI);
        return EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getSDIndexPeek _currPeekSDI = %d", _currPeekSDI));
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }
}
MP4_ERROR_CODE
SampleToChunkAtom::getSDIndexGet(uint32 &SDIndex) const
{
    if (_psampleDescriptionIndexVec == NULL)
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    if (_currGetSDI != 0)
    {
        SDIndex = (_currGetSDI);
        return EVERYTHING_FINE;
    }
    else
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getSDIndexGet _currGetSDI = %d", _currGetSDI));
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }
}


MP4_ERROR_CODE
SampleToChunkAtom::getFirstSampleNumInChunkGet(uint32& SampleNum) const
{
    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    SampleNum = (_firstGetSampleInCurrChunk);
    return EVERYTHING_FINE;
}


MP4_ERROR_CODE
SampleToChunkAtom::getChunkNumberForSamplePeek(uint32 sampleNum, uint32& ChunkNumber)
{
    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }
    if (_parsing_mode == 1)
    {
        CheckAndParseEntry(_majorPeekIndex);
    }

    if (sampleNum < _currPeekSampleCount)
    {
        ChunkNumber = _currPeekChunk;
        return EVERYTHING_FINE;
    }
    else if (_numPeekChunksInRun > 1)
    {
        _firstPeekSampleInCurrChunk = _currPeekSampleCount;
        _currPeekSampleCount += _numPeekSamplesPerChunk;
        _currPeekChunk++;

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _firstPeekSampleInCurrChunk =%d", _firstPeekSampleInCurrChunk));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _currPeekSampleCount =%d", _currPeekSampleCount));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _numPeekSamplesPerChunk =%d", _numPeekSamplesPerChunk));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _currPeekChunk =%d", _currPeekChunk));

        // to handle special case, every sample is a chunk
        if (_entryCount > 1)
        {
            _numPeekChunksInRun--;
        }

        if (sampleNum < _currPeekSampleCount)
        {
            ChunkNumber = _currPeekChunk;
            return EVERYTHING_FINE;
        }
        else
        {
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getChunkNumberForSamplePeek sampleNum = %d", sampleNum));
            return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
        }
    }
    else if (_numPeekChunksInRun <= 1)
    {
        if (_majorPeekIndex < (int32)(_entryCount - 1))
        {
            uint32 prevNextChunk = _pfirstChunkVec[_majorPeekIndex%_stbl_buff_size];
            _numPeekSamplesPerChunk = _psamplesPerChunkVec[_majorPeekIndex%_stbl_buff_size];
            _currPeekSDI = _psampleDescriptionIndexVec[_majorPeekIndex%_stbl_buff_size];

            if (_parsing_mode == 1)
            {
                CheckAndParseEntry(_majorPeekIndex + 1);
            }

            uint32 nextfirstChunk = _pfirstChunkVec[(_majorPeekIndex+1)%_stbl_buff_size];

            _numPeekChunksInRun = nextfirstChunk - prevNextChunk;

            _majorPeekIndex++;

            _firstPeekSampleInCurrChunk = _currPeekSampleCount;
            _currPeekSampleCount    += _numPeekSamplesPerChunk;
            _currPeekChunk++;

            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _numPeekChunksInRun =%d", _numPeekChunksInRun));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _numPeekSamplesPerChunk =%d", _numPeekSamplesPerChunk));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _majorPeekIndex =%d", _majorPeekIndex));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _firstPeekSampleInCurrChunk =%d", _firstPeekSampleInCurrChunk));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _currPeekSampleCount =%d", _currPeekSampleCount));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _numPeekSamplesPerChunk =%d", _numPeekSamplesPerChunk));

            if (sampleNum < _currPeekSampleCount)
            {
                ChunkNumber = _currPeekChunk;
                return EVERYTHING_FINE;
            }
            else
            {
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getChunkNumberForSamplePeek sampleNum = %d", sampleNum));
                return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
            }
        }
        else if (_majorPeekIndex == (int32)(_entryCount - 1))
        {
            _numPeekChunksInRun = 1;

            _currPeekSDI =
                _psampleDescriptionIndexVec[_majorPeekIndex%_stbl_buff_size];

            _numPeekSamplesPerChunk =
                _psamplesPerChunkVec[_majorPeekIndex%_stbl_buff_size];

            _firstPeekSampleInCurrChunk = _currPeekSampleCount;

            _currPeekSampleCount += _numPeekSamplesPerChunk;
            _currPeekChunk++;

            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _numPeekSamplesPerChunk =%d", _numPeekSamplesPerChunk));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _majorPeekIndex =%d", _majorPeekIndex));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _firstPeekSampleInCurrChunk =%d", _firstPeekSampleInCurrChunk));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _currPeekSampleCount =%d", _currPeekSampleCount));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::getChunkNumberForSamplePeek- _numPeekSamplesPerChunk =%d", _numPeekSamplesPerChunk));

            if (sampleNum < _currPeekSampleCount)
            {
                ChunkNumber = _currPeekChunk;
                return EVERYTHING_FINE;
            }
            else
            {
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR=>SampleToChunkAtom::getChunkNumberForSamplePeek sampleNum = %d", sampleNum));
                return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
            }
        }
        else
        {
            return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
        }
    }

    return READ_SAMPLE_TO_CHUNK_ATOM_FAILED; // Should never get here
}
MP4_ERROR_CODE
SampleToChunkAtom::getNumChunksInRunofChunksGet(uint32& numChunksInRun) const
{
    if (_pfirstChunkVec == NULL)
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    if (_numChunksInRun != 0)
    {
        numChunksInRun = _numChunksInRun;
        return EVERYTHING_FINE;
    }
    return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
}

MP4_ERROR_CODE
SampleToChunkAtom::getSamplesPerChunkCorrespondingToSampleGet(uint32& SamplesPerChunk) const
{

    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    if (_numGetSamplesPerChunk != 0)
    {
        SamplesPerChunk = _numGetSamplesPerChunk;
        return EVERYTHING_FINE;
    }

    return READ_SAMPLE_TO_CHUNK_ATOM_FAILED; // Should never get here
}
MP4_ERROR_CODE
SampleToChunkAtom::getFirstSampleNumInChunkPeek(uint32& SampleNum) const
{
    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
    }

    SampleNum = (_firstPeekSampleInCurrChunk);
    return EVERYTHING_FINE;
}

int32
SampleToChunkAtom::resetStateVariables()
{
    _majorGetIndex = 0;
    _currGetChunk = -1;
    _numGetChunksInRun = 0;
    _currGetSampleCount  = 0;
    _firstGetSampleInCurrChunk = 0;
    _numGetSamplesPerChunk = 0;
    _currGetSDI = 0;

    _majorPeekIndex = 0;
    _currPeekChunk = -1;
    _numPeekChunksInRun = 0;
    _currPeekSampleCount  = 0;
    _firstPeekSampleInCurrChunk = 0;
    _numPeekSamplesPerChunk = 0;
    _currPeekSDI = 0;

    return (EVERYTHING_FINE);
}

int32
SampleToChunkAtom::resetStateVariables(uint32 sampleNum)
{
    _majorGetIndex = 0;
    _currGetChunk = -1;
    _numGetChunksInRun = 0;
    _currGetSampleCount  = 0;
    _firstGetSampleInCurrChunk = 0;
    _numGetSamplesPerChunk = 0;
    _currGetSDI = 0;

    _majorPeekIndex = 0;
    _currPeekChunk = -1;
    _numPeekChunksInRun = 0;
    _currPeekSampleCount  = 0;
    _firstPeekSampleInCurrChunk = 0;
    _numPeekSamplesPerChunk = 0;
    _currPeekSDI = 0;

    if ((_pfirstChunkVec == NULL) ||
            (_psamplesPerChunkVec == NULL))
    {
        return DEFAULT_ERROR;
    }

    uint32 sampleCount = 0;

    for (uint32 i = 0; i < _entryCount; i++)
    {
        uint32 chunkNum = 0;
        uint32 samplesPerChunkInRun = 0;

        if (_parsing_mode == 1)
        {
            CheckAndParseEntry(i + 1);
        }

        chunkNum = _pfirstChunkVec[i%_stbl_buff_size];
        samplesPerChunkInRun = _psamplesPerChunkVec[i%_stbl_buff_size];

        if ((i + 1) < _entryCount)
        {
            uint32 nextChunkNum = _pfirstChunkVec[(int32)(i+1)%_stbl_buff_size];
            uint32 numChunksInRun = nextChunkNum - chunkNum;
            uint32 count = sampleCount + samplesPerChunkInRun * numChunksInRun;

            if (count < sampleNum)
            {
                // Haven't found chunk yet - running count still less than sampleNum
                sampleCount = count;
                continue;
            }
            else
            {
                _numGetChunksInRun = numChunksInRun;

                // Found run of chunks in which sample lies - now find actual chunk
                for (int32 j = 0; j < (int32)numChunksInRun; j++)
                {
                    //  samples for jth chunk
                    _firstGetSampleInCurrChunk = sampleCount;
                    _numGetSamplesPerChunk = samplesPerChunkInRun;
                    sampleCount += samplesPerChunkInRun;

                    if (sampleNum < sampleCount)
                    {
                        _majorGetIndex = i;
                        _numChunksInRun = numChunksInRun;
                        _currGetSampleCount = sampleCount;
                        _currGetChunk =  chunkNum + j;
                        _currGetSDI =
                            _psampleDescriptionIndexVec[_majorGetIndex%_stbl_buff_size];

                        goto END_OF_RESET;
                    }
                    _numGetChunksInRun--;
                }
            }
        }
        else
        {
            // Last run of chunks - simply find specific chunk
            int k = 0;
            while (sampleNum >= sampleCount)
            {
                // Continue until find chunk number - we do not know how many chunks are in
                // this final run so keep going til we find a number
                _firstGetSampleInCurrChunk = sampleCount;
                _numGetSamplesPerChunk = samplesPerChunkInRun;

                sampleCount += samplesPerChunkInRun;

                if (sampleNum < sampleCount)
                {
                    // Found specific chunk
                    _majorGetIndex = i;
                    _numGetChunksInRun = 1;
                    _numChunksInRun = _numGetChunksInRun;
                    _currGetSampleCount = sampleCount;
                    _currGetChunk =  chunkNum + k;
                    _currGetSDI =
                        _psampleDescriptionIndexVec[_majorGetIndex%_stbl_buff_size];

                    goto END_OF_RESET;
                    // Return ith chunk in run
                    // Since we do not actually know how many chunk are in this last run,
                    // the chunkNum that is returned may not be a valid chunk!
                    // This is handled in the exception handling in the chunkOffset atom
                }
                k++;
            }
        }
    }

    return DEFAULT_ERROR; // Should never get here

END_OF_RESET:
    {
        if (_majorGetIndex < (int32)(_entryCount - 1))
        {
            _majorGetIndex++;
        }
        _majorPeekIndex = _majorGetIndex;
        _currPeekChunk = _currGetChunk;
        _numPeekChunksInRun = _numGetChunksInRun;
        _currPeekSampleCount  = _currGetSampleCount;
        _firstPeekSampleInCurrChunk = _firstGetSampleInCurrChunk;
        _numPeekSamplesPerChunk = _numGetSamplesPerChunk;
        _currPeekSDI = _currGetSDI;

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::resetStateVariables- _majorPeekIndex = _majorGetIndex =%d", _majorPeekIndex));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::resetStateVariables- _currPeekChunk = _currGetChunk =%d", _currPeekChunk));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::resetStateVariables- _numPeekChunksInRun = _numGetChunksInRun = %d", _numPeekChunksInRun));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::resetStateVariables- _currPeekSampleCount = _currGetSampleCount = %d", _currPeekSampleCount));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::resetStateVariables- _firstPeekSampleInCurrChunk = _firstGetSampleInCurrChunk = %d", _firstPeekSampleInCurrChunk));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::resetStateVariables- _numPeekSamplesPerChunk = _numGetSamplesPerChunk = %d", _numPeekSamplesPerChunk));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleToChunkAtom::resetStateVariables- _numPeekSamplesPerChunk = _currGetSDI = %d", _currPeekSDI));

        return (EVERYTHING_FINE);
    }
}

int32 SampleToChunkAtom::resetPeekwithGet()
{
    _majorPeekIndex = _majorGetIndex;
    _currPeekChunk = _currGetChunk;
    _numPeekChunksInRun = _numGetChunksInRun;
    _currPeekSampleCount  = _currGetSampleCount;
    _firstPeekSampleInCurrChunk = _firstGetSampleInCurrChunk;
    _numPeekSamplesPerChunk = _numGetSamplesPerChunk;
    _currPeekSDI = _currGetSDI;
    return (EVERYTHING_FINE);
}
void SampleToChunkAtom::CheckAndParseEntry(uint32 i)
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
