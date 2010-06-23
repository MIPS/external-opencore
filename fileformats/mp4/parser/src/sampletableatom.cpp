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
/*                         MPEG-4 SampleTableAtom Class                          */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    This SampleTableAtom Class contains all the time and data indexing of the
    media samples in a track.
*/


#define IMPLEMENT_SampleTableAtom

//HEADER FILES REQD FOR MULTIPLE SAMPLE RETRIEVAL API
#include "sampletableatom.h"
#include "atomutils.h"
#include "atomdefs.h"
#include "oscl_media_data.h"
#include "pv_gau.h"
#include "amrdecoderspecificinfo.h"
#include "oscl_int64_utils.h"
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
#include "oscl_tickcount.h"

#define PV_MP4_PARSER_SAMPLE_READ_DIAGLOG_THRESHOLD_IN_MS 5
#endif

// Stream-in ctor
SampleTableAtom::SampleTableAtom(MP4_FF_FILE *fp,
                                 uint32 mediaType,
                                 OSCL_wString& filename,
                                 uint32 size,
                                 uint32 type,
                                 bool oPVContentDownloadable,
                                 uint32 parsingMode,
                                 bool aOpenFileOncePerTrack)
        : Atom(fp, size, type),
        _currentPlaybackSampleTimestamp(0),
        _currentPlaybackSampleNumber(0),
        _trackStartTSOffset(0),
        _fileSize(0),
        _IsUpdateFileSize(0)
{
    _ptimeToSampleAtom      = NULL;
    _pcompositionOffsetAtom = NULL;
    _psampleDescriptionAtom = NULL;
    _psampleSizeAtom        = NULL;
    _psampleToChunkAtom     = NULL;
    _pchunkOffsetAtom       = NULL;
    _psyncSampleAtom        = NULL;

    _SDIndex = 0;

    SamplesCount = 0;

    _oMultipleSampleDescription = false;
    _numAMRFramesPerSample = 0;
    _pAMRTempBuffer = NULL;
    _oResidualSample = false;
    _remainingFramesInSample = 0;
    _amrTempBufferOffset = 0;
    _amrFrameDelta = 0;
    _pinput   = NULL;
    _commonFilePtr = NULL;
    _currChunkOffset = 0;
    iOpenFileOncePerTrack = aOpenFileOncePerTrack;
    _defaultMimeType += _STRLIT_WCHAR("UNKNOWN");
    _prevEditDuration = 0;
    _tsDelta = 0;
    _editOffset = 0;
    _editIndex = 0;
    _pEditListTimeVec = NULL;
    _pEditListDurationVec = NULL;
    _movieTimeScale = 0;
    _trackMediaTimeScale = 0;

    iLogger = PVLogger::GetLoggerObject("mp4ffparser");
    iStateVarLogger = PVLogger::GetLoggerObject("mp4ffparser_mediasamplestats");
    iParsedDataLogger = PVLogger::GetLoggerObject("mp4ffparser_parseddata");
    iDiagnosticsLogger = PVLogger::GetLoggerObject("pvplayerdiagnostics.mp4ffparser");

    _oPVContentDownloadable = oPVContentDownloadable;

    _parsingMode = parsingMode;

    if (_success)
    {
        _filename = filename;
        _pinput   = NULL;

        OsclAny*ptr = oscl_malloc(sizeof(MP4_FF_FILE));
        if (ptr == NULL)
        {
            _success = false;
            _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
            return;
        }
        _pinput = OSCL_PLACEMENT_NEW(ptr, MP4_FF_FILE());
        _pinput->_fileServSession = fp->_fileServSession;
        _pinput->_pvfile.SetCPM(fp->_pvfile.GetCPM());

        _pinput->_pvfile.SetFileHandle(fp->_pvfile.iFileHandle);
        _pinput->_fileSize = fp->_fileSize;

        if (!iOpenFileOncePerTrack)
        {
            ptr = oscl_malloc(sizeof(MP4_FF_FILE));
            if (ptr == NULL)
            {
                _success = false;
                _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                return;
            }
            _commonFilePtr = OSCL_PLACEMENT_NEW(ptr, MP4_FF_FILE(*fp));
            _commonFilePtr->_pvfile.SetFileHandle(fp->_pvfile.iFileHandle);
        }

        _currentPlaybackSampleNumber = 0; // Initializing playback start point

        _pparent = NULL;
        _success = true;

        int32 count = _size - DEFAULT_ATOM_SIZE;

        while (count > 0)
        {
            uint32 atomType = UNKNOWN_ATOM;
            uint32 atomSize = 0;

            AtomUtils::getNextAtomType(fp, atomSize, atomType);

            if (atomType == TIME_TO_SAMPLE_ATOM)
            {   //"stts"
                PV_MP4_FF_NEW(fp->auditCB, TimeToSampleAtom,
                              (fp, mediaType, atomSize,
                               atomType, filename, parsingMode),
                              _ptimeToSampleAtom);

                // Check for success
                if (!_ptimeToSampleAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _ptimeToSampleAtom->GetMP4Error();
                    return;
                }
                _ptimeToSampleAtom->setParent(this);
                count -= _ptimeToSampleAtom->getSize();
            }

            else if (atomType == COMPOSITION_OFFSET_ATOM)
            {   //"ctts"
                PV_MP4_FF_NEW(fp->auditCB, CompositionOffsetAtom,
                              (fp, mediaType, atomSize,
                               atomType, filename, parsingMode),
                              _pcompositionOffsetAtom);

                // Check for success
                if (!_pcompositionOffsetAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _pcompositionOffsetAtom->GetMP4Error();
                    return;
                }
                _pcompositionOffsetAtom->setParent(this);
                count -= _pcompositionOffsetAtom->getSize();
            }

            else if ((atomType == UUID_ATOM)
                     || (atomType == SHADOW_SYNC_SAMPLE_ATOM)
                     || (atomType == UNKNOWN_ATOM)
                     || (atomType == USER_DATA_ATOM)
                     || (atomType == DEGRADATION_PRIORITY_ATOM))
            {
                if (atomSize < DEFAULT_ATOM_SIZE)
                {
                    _success = false;
                    _mp4ErrorCode = ZERO_OR_NEGATIVE_ATOM_SIZE;
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::SampleTableAtom ZERO_OR_NEGATIVE_ATOM_SIZE"));
                    break;
                }
                if (count < (int32)atomSize)
                {
                    _success = false;
                    _mp4ErrorCode = READ_FAILED;
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::SampleTableAtom READ FAILED count<atomSize"));
                    break;
                }
                count -= atomSize;
                atomSize -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, atomSize);
            }
            else if (atomType == SAMPLE_DESCRIPTION_ATOM)
            {
                TOsclFileOffset myOffset = AtomUtils::getCurrentFilePosition(fp);
                //"stsd"
                if (_psampleDescriptionAtom == NULL)
                {
                    PV_MP4_FF_NEW(fp->auditCB, SampleDescriptionAtom, (fp, mediaType, atomSize, atomType), _psampleDescriptionAtom);

                    // Check for success
                    if (!_psampleDescriptionAtom->MP4Success())
                    {
                        _success = false;
                        _mp4ErrorCode = _psampleDescriptionAtom->GetMP4Error();
                        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::SampleTableAtom Sample Description Atom Failed"));
                        return;
                    }
                    _psampleDescriptionAtom->setParent(this);
                    count -= _psampleDescriptionAtom->getSize();

                    if (_psampleDescriptionAtom->Is3GPPAMR())
                    {
                        AMRSampleEntry *entry  = _psampleDescriptionAtom->getAMRSampleEntry();
                        AMRSpecificAtom *amrAtom = NULL;
                        _numAMRFramesPerSample = 0;

                        if (entry != NULL)
                        {
                            amrAtom = entry->getDecoderSpecificInfo();
                            if (amrAtom != NULL)
                                _numAMRFramesPerSample = amrAtom->getFramesPerSample();
                        }

                        if (_numAMRFramesPerSample == 0)
                        {
                            _success = false;
                            _mp4ErrorCode = READ_AMR_SAMPLE_ENTRY_FAILED;
                            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::SampleTableAtom Read AMR SAmple Entry Failed"));
                            return;
                        }

                        _pAMRTempBuffer = (uint8 *)(oscl_malloc(sizeof(uint8) * 512));
                    }
                    else
                    {
                        _numAMRFramesPerSample = 0;
                    }
                }
                else
                {
                    //Duplicate stsd++++
                    _oMultipleSampleDescription = true;
                }
                atomSize -= DEFAULT_ATOM_SIZE;
                atomSize += (uint32)myOffset;
                AtomUtils::seekFromStart(fp, (TOsclFileOffset)atomSize);
            }
            else if (atomType == SAMPLE_SIZE_ATOM)
            { //"stsz"
                PV_MP4_FF_NEW(fp->auditCB, SampleSizeAtom,
                              (fp, mediaType, atomSize, atomType,
                               filename, parsingMode),
                              _psampleSizeAtom);

                SamplesCount = _psampleSizeAtom->getSampleCount();
                // Check for success
                if (!_psampleSizeAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _psampleSizeAtom->GetMP4Error();
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::Sample Sized Atom Failed"));
                    return;
                }
                _psampleSizeAtom->setParent(this);
                count -= _psampleSizeAtom->getSize();
            }
            else if (atomType == SAMPLE_TO_CHUNK_ATOM)
            {
                //"stsc"
                PV_MP4_FF_NEW(fp->auditCB, SampleToChunkAtom, (fp, atomSize, atomType, filename, parsingMode), _psampleToChunkAtom);

                // Check for success
                if (!_psampleToChunkAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _psampleToChunkAtom->GetMP4Error();
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::Sample to chunk atom failed"));
                    return;
                }
                _psampleToChunkAtom->setParent(this);
                count -= _psampleToChunkAtom->getSize();
            }
            else if (atomType == CHUNK_OFFSET_ATOM)
            {
                //"stco"
                PV_MP4_FF_NEW(fp->auditCB, ChunkOffsetAtom, (fp, atomSize, atomType, filename, parsingMode, false), _pchunkOffsetAtom);

                // Check for success
                if (!_pchunkOffsetAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _pchunkOffsetAtom->GetMP4Error();
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::Chunk Offset atom failed"));
                    return;
                }
                _pchunkOffsetAtom->setParent(this);
                count -= _pchunkOffsetAtom->getSize();
            }
            else if (atomType == CHUNK_LARGE_OFFSET_ATOM)
            {
                //"co64"
                PV_MP4_FF_NEW(fp->auditCB, ChunkOffsetAtom, (fp, atomSize, atomType, filename, parsingMode, true), _pchunkOffsetAtom);

                // Check for success
                if (!_pchunkOffsetAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _pchunkOffsetAtom->GetMP4Error();
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::Chunk Offset atom failed"));
                    return;
                }
                _pchunkOffsetAtom->setParent(this);
                count -= _pchunkOffsetAtom->getSize();
            }
            else if (atomType == SYNC_SAMPLE_ATOM)
            {
                //"stss"
                PV_MP4_FF_NEW(fp->auditCB, SyncSampleAtom, (fp, atomSize, atomType), _psyncSampleAtom);

                // Check for success
                if (!_psyncSampleAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _psyncSampleAtom->GetMP4Error();
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::Sync Sample Atom Failed"));
                    return;
                }
                _psyncSampleAtom->setParent(this);
                count -= _psyncSampleAtom->getSize();
            }
            else if (atomType == AVC_SAMPLE_DEPENDENCY_TYPE_BOX)
            {
                //"sdtp"
                uint32 SamplesCount = _psampleSizeAtom->getSampleCount();

                PV_MP4_FF_NEW(fp->auditCB, AVCSampleDependencyType,
                              (fp, atomSize, atomType, SamplesCount),
                              _pavcSampleDependencyType);

                // Check for success
                if (!_pavcSampleDependencyType->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _pavcSampleDependencyType->GetMP4Error();
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::AVC Sample Dependency Type Box failed"));
                    return;
                }
                _pavcSampleDependencyType->setParent(this);
                count -= _pavcSampleDependencyType->getSize();

            }
            else if (atomType == AVC_SAMPLE_TO_GROUP_BOX)
            {
                //"sgpd"
                PV_MP4_FF_NEW(fp->auditCB, AVCSampleToGroup, (fp, atomSize, atomType), _pavcSampleToGroup);
                // Check for success
                if (!_pavcSampleToGroup->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _pavcSampleToGroup->GetMP4Error();
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::AVC Sample to group box failed"));
                    return;
                }
                _pavcSampleToGroup->setParent(this);
                count -= _pavcSampleToGroup->getSize();


            }
            else if (atomType == AVC_SAMPLE_DEPENDENCY_BOX)
            {
                uint32 SamplesCount = _psampleSizeAtom->getSampleCount();
                PV_MP4_FF_NEW(fp->auditCB, AVCSampleDependency, (fp, atomSize, atomType, SamplesCount), _pavcSampleDependency);
                // Check for success
                if (!_pavcSampleDependency->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _pavcSampleDependency->GetMP4Error();
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::AVC Sample Dependency box failed"));
                    return;
                }
                _pavcSampleDependency->setParent(this);
                count -= _pavcSampleDependency->getSize();
            }
            else
            {
                count -= atomSize;
                atomSize -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, atomSize);

            }

        }

        if (_pcompositionOffsetAtom != NULL)
        {
            if (SamplesCount != 0)
            {
                _pcompositionOffsetAtom->SetSamplesCount(SamplesCount);
            }
        }
        /*
         * We only check against sample description here. In case of
         * moof clips, if there are no samples associated with movie atom
         * then it is ok for stts, stsz, stco, and stsc to be not present.
         * It is not clear from the language of the spec whether authors
         * of such clips need to still author empty atoms in those cases.
         * Anycase it is best to be tolerant. In case the clip does indeed
         * turn out to be a poison clip (a moov clip with these atoms missing)
         * then other APIs (viz. sample get / peek etc will throw errors). So
         * we do not lose much by not checking here.
         */
        if (_psampleDescriptionAtom == NULL)
        {
            _success = false;
            _mp4ErrorCode = READ_SAMPLE_TABLE_ATOM_FAILED;
            return;
        }
        else if (_psampleDescriptionAtom->Is3GPPAMR())
        {
            uint32 sampleDelta = 0;
            _ptimeToSampleAtom->GetTimeDeltaForSampleNumber(0, sampleDelta);

            // The notion of separate decode and composition timestamps is specific to
            // video tracks. Therefore no ned to Adjust sampleDelta with CTTS value.

            if (_numAMRFramesPerSample > 0)
            {
                _amrFrameDelta = ((sampleDelta) / (_numAMRFramesPerSample));
            }
            _amrSampleSize = 0;
        }

    }
    else
    {
        _mp4ErrorCode = READ_SAMPLE_TABLE_ATOM_FAILED;
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::Read Sample Table Atom Failed"));
    }
}

// Destructor
SampleTableAtom::~SampleTableAtom()
{
    // Clean up member atoms
    if (_ptimeToSampleAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, TimeToSampleAtom, _ptimeToSampleAtom);
    }

    if (_pcompositionOffsetAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, CompositionOffsetAtom, _pcompositionOffsetAtom);
    }

    if (_psampleDescriptionAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, SampleDescriptionAtom, _psampleDescriptionAtom);
    }

    if (_psampleSizeAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, SampleSizeAtom, _psampleSizeAtom);
    }

    if (_psampleToChunkAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, SampleToChunkAtom, _psampleToChunkAtom);
    }

    if (_pchunkOffsetAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, ChunkOffsetAtom, _pchunkOffsetAtom);
    }

    if (_psyncSampleAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, SyncSampleAtom, _psyncSampleAtom);
    }

    if (iOpenFileOncePerTrack)
    {
        if (_pinput != NULL)
        {
            AtomUtils::CloseMP4File(_pinput);
            oscl_free(_pinput);
        }
    }
    else
    {
        if (_pinput != NULL)
        {
            oscl_free(_pinput);
        }
        if (_commonFilePtr != NULL)
        {
            oscl_free(_commonFilePtr);
        }
    }
    if (_pAMRTempBuffer != NULL)
    {
        oscl_free(_pAMRTempBuffer);
    }
}


// Returns the specific sample with number 'sampleNum'
MP4_ERROR_CODE
SampleTableAtom::getSample(uint32 sampleNum, uint8 *buf, uint32 &size, uint32 &index, TOsclFileOffset &SampleOffset)
{
    if ((_psampleSizeAtom    == NULL) ||
            (_psampleToChunkAtom == NULL) ||
            (_pchunkOffsetAtom   == NULL))
    {
        size = 0;
        return DEFAULT_ERROR;
    }

    // return NULL if sampleNum is past end of stream
    uint32 numSamples = _psampleSizeAtom->getSampleCount();
    if (sampleNum >= numSamples)
    {
        size = 0;
        return END_OF_TRACK; // Past end of samples
    }

    // Check sample size from SampleSizeAtom
    size = 0;
    uint32 sampleSize = 0;
    MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(sampleNum, sampleSize);
    if ((errCode != EVERYTHING_FINE) || (size == 0))
    {
        return errCode;
    }

    // Find chunk
    uint32 chunk = 0;
    errCode = _psampleToChunkAtom->getChunkNumberForSampleGet(sampleNum, chunk);
    if (errCode != EVERYTHING_FINE)
    {
        return errCode;
    }
    // Find first sample in that chunk
    uint32 first = 0;
    errCode = _psampleToChunkAtom->getFirstSampleNumInChunkGet(first);
    if (errCode != EVERYTHING_FINE)
    {
        return errCode;
    }

    //set the sample description index
    errCode = _psampleToChunkAtom->getSDIndexGet(_SDIndex);
    if (errCode != EVERYTHING_FINE)
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getSample - Read Sample Description Atom Failed"));
        return READ_SAMPLE_DESCRIPTION_ATOM_FAILED;
    }
    _SDIndex -= 1;
    index = _SDIndex;

    if (sampleSize == 0)
    {   //shortcut
        return EVERYTHING_FINE;
    }

    // Find chunk offset to file
    uint64 offset;
    errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);
    //uint32 offset to int32, possible error for large files
    if (errCode != EVERYTHING_FINE)
    {
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getSample - Read Chunk Offset Atom Failed"));
        return READ_CHUNK_OFFSET_ATOM_FAILED;
    }

    // Need to add up all the sizes from the first sample in this run up to the
    // the requested sample (but not including it)
    int32 sampleSizeOffset = 0;
    uint32 tempSize = 0;
    for (uint32 i = first; i < sampleNum; i++)
    {
        // Check sample size from SampleSizeAtom
        MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
        // Check for error condition
        if (errCode == DEFAULT_ERROR)
        {
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getSample - Read Sample Sized Atom Failed"));
            return READ_SAMPLE_SIZE_ATOM_FAILED;
        }

        sampleSizeOffset += tempSize;
    }

    // Find actual file offset to sample
    TOsclFileOffset sampleFileOffset = (TOsclFileOffset)offset + sampleSizeOffset;

    SampleOffset = sampleFileOffset;

    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getSample- sampleFileOffset =%d", sampleFileOffset));
    // Open file
    if (!_pinput->IsOpen())
    {
        if (iOpenFileOncePerTrack)
        {
            // Check if file is already open
            // Opens file in binary mode with read sharing permissions
            // Return Error in case the file open fails.
            if (AtomUtils::OpenMP4File(_filename,
                                       Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                       _pinput) != 0)
            {
                return FILE_OPEN_FAILED;
            }
        }
        else
        {
            _pinput->_fileSize = _commonFilePtr->_fileSize;
            _pinput->_pvfile.Copy(_commonFilePtr->_pvfile);
            AtomUtils::Flush(_pinput);
            AtomUtils::seekFromStart(_pinput, 0);
        }
        if (!_IsUpdateFileSize)
        {
            if (AtomUtils::getCurrentFileSize(_pinput, _fileSize) == false)
            {
                return READ_FAILED;
            }
        }
    }
    if ((TOsclFileOffset)(sampleFileOffset + sampleSize) > _fileSize)
    {
        return INSUFFICIENT_DATA;
    }

    if (iOpenFileOncePerTrack)
    {
        if (_oPVContentDownloadable)
        {
            if (sampleNum == 0)
            {
                AtomUtils::seekFromStart(_pinput, sampleFileOffset);
            }
        }
        else
        {
            AtomUtils::seekFromStart(_pinput, sampleFileOffset);
        }
    }
    else
    {
        AtomUtils::seekFromStart(_pinput, sampleFileOffset);
    }

    // Read byte data into buffer
    if (AtomUtils::readByteData(_pinput, sampleSize, buf))
    {
        size = sampleSize;
        return EVERYTHING_FINE;
    }
    else
    {
        return DEFAULT_ERROR;
    }
}

MP4_ERROR_CODE SampleTableAtom::getKeyMediaSampleNumAt(uint32 aKeySampleNum,
        GAU    *pgau)
{
    uint32 n = 1;
    if (_psyncSampleAtom == NULL)
    {
        //all samples are key frames
        _currentPlaybackSampleNumber = aKeySampleNum;
    }
    else
    {
        uint32 totalNumSyncSamples = getSyncSampleAtom()->getEntryCount();
        if (aKeySampleNum >= totalNumSyncSamples)
        {
            aKeySampleNum = 0;
        }
        MP4_ERROR_CODE  errCode =
            getSyncSampleAtom()->getSampleNumberAt(aKeySampleNum, _currentPlaybackSampleNumber);
        if (errCode != EVERYTHING_FINE)
        {
            return errCode;
        }
    }
    int32 retValA = _ptimeToSampleAtom->ResetStateVariables(_currentPlaybackSampleNumber);
    if (retValA == DEFAULT_ERROR)
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _ptimeToSampleAtom->ResetStateVariables();
        return READ_FAILED;
    }
    int32 retValB = _psampleToChunkAtom->resetStateVariables(_currentPlaybackSampleNumber);
    if (retValB == DEFAULT_ERROR)
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _psampleToChunkAtom->resetStateVariables();
        return READ_FAILED;
    }
    if (NULL != _pcompositionOffsetAtom)
    {
        int32 retValC = _pcompositionOffsetAtom->ResetStateVariables(_currentPlaybackSampleNumber);
        if (DEFAULT_ERROR == retValC)
        {
            _currentPlaybackSampleNumber = 0;
            _currentPlaybackSampleTimestamp = 0;
            _pcompositionOffsetAtom->ResetStateVariables();
            return READ_FAILED;
        }
    }
    if (_currentPlaybackSampleNumber > 0)
    {
        getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
    }
    else
    {
        _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
    }
    //Reset the file pointer to the correct location. In case of non-interleaved
    //pv content, we do not seek for every media sample.
    if (_oPVContentDownloadable)
    {
        uint32 sampleSize = 0;
        _psampleSizeAtom->getSampleSizeAt(_currentPlaybackSampleNumber, sampleSize);

        // Find chunk
        uint32 chunk = 0;
        MP4_ERROR_CODE errCode =
            _psampleToChunkAtom->getChunkNumberForSample(_currentPlaybackSampleNumber, chunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Find first sample in that chunk
        uint32 first = 0;
        errCode = _psampleToChunkAtom->getFirstSampleNumInChunk(chunk, first);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Find chunk offset to file
        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);

        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Need to add up all the sizes from the first sample in this run up to the
        // the requested sample (but not including it)
        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;
        for (uint32 i = first; i < (uint32)_currentPlaybackSampleNumber; i++)
        {
            // Check sample size from SampleSizeAtom
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
            // Check for error condition
            if (errCode == DEFAULT_ERROR)
            {
                _currentPlaybackSampleNumber = 0;
                return (READ_FAILED);
            }
            sampleSizeOffset += tempSize;
        }

        // Find actual file offset to sample
        TOsclFileOffset sampleFileOffset = (TOsclFileOffset)offset + sampleSizeOffset;

        if (!_pinput->IsOpen())
        {
            if (iOpenFileOncePerTrack)
            {
                // Check if file is already open
                // Opens file in binary mode with read sharing permissions
                // Return Error in case the file open fails.
                if (AtomUtils::OpenMP4File(_filename,
                                           Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                           _pinput) != 0)
                {
                    return FILE_OPEN_FAILED;
                }
            }
            else
            {
                _pinput->_fileSize = _commonFilePtr->_fileSize;
                _pinput->_pvfile.Copy(_commonFilePtr->_pvfile);
                AtomUtils::Flush(_pinput);
                AtomUtils::seekFromStart(_pinput, 0);
            }

            if (!_IsUpdateFileSize)
            {
                if (AtomUtils::getCurrentFileSize(_pinput, _fileSize) == false)
                {
                    _currentPlaybackSampleNumber = 0;
                    return (READ_FAILED);
                }
            }
        }

        if ((TOsclFileOffset)(sampleFileOffset + sampleSize) > _fileSize)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }
        AtomUtils::seekFromStart(_pinput, sampleFileOffset);
    }

    {
        uint32 chunk = 0;
        MP4_ERROR_CODE errCode = getMutableSampleToChunkAtom().
                                 getChunkNumberForSampleGet(_currentPlaybackSampleNumber, chunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 FirstSampleNumInChunk = 0;
        errCode = getSampleToChunkAtom().getFirstSampleNumInChunkGet(FirstSampleNumInChunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 offsetIntoRunOfChunks =
            (_currentPlaybackSampleNumber - FirstSampleNumInChunk);

        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);

        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;

        _currChunkOffset = 0;
        for (uint32 i = FirstSampleNumInChunk; i < FirstSampleNumInChunk +
                offsetIntoRunOfChunks; i++)
        {
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
            if (errCode == DEFAULT_ERROR)
            {
                _currentPlaybackSampleNumber = 0;
                return READ_FAILED;
            }
            sampleSizeOffset += tempSize;
        }
        _currChunkOffset = sampleSizeOffset;
    }

    return getNextNSamples(_currentPlaybackSampleNumber, &n, pgau);;
}

MP4_ERROR_CODE
SampleTableAtom::getPrevKeyMediaSample(uint64 inputtimestamp, uint32 &aKeySampleNum, uint32 *n, GAU    *pgau)
{
    if (_ptimeToSampleAtom  == NULL)
    {
        return DEFAULT_ERROR;
    }

    // Get sample number at timestamp
    _ptimeToSampleAtom->GetSampleNumberFromTimestamp(inputtimestamp, _currentPlaybackSampleNumber);
    // Go for composition offset adjustment.
    _currentPlaybackSampleNumber =
        getSampleNumberAdjustedWithCTTS(inputtimestamp, _currentPlaybackSampleNumber);

    // Check if sample is an I-frame.  If not, need to find the sample number of the
    // first I-frame sample that follows ts
    // Need to check syncSampleAtom for this - if not present, all samples are synch samples
    // (i.e. all audio samples are synch samples)
    if (_psyncSampleAtom  != NULL)
    {
        MP4_ERROR_CODE  errCode =
            getSyncSampleAtom()->getSyncSampleBefore(_currentPlaybackSampleNumber, _currentPlaybackSampleNumber);
        if (errCode != EVERYTHING_FINE)
        {
            return errCode;
        }
    }

    aKeySampleNum = _currentPlaybackSampleNumber;

    // Increment ts for current sample in this random access:No more correct in case of CTTS presence
    if (_currentPlaybackSampleNumber != 0)
    {
        // ts for sample 0 is 0
        getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
    }
    else
    {
        _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);

    }
    int32 retValA = _ptimeToSampleAtom->ResetStateVariables(_currentPlaybackSampleNumber);
    if (retValA == DEFAULT_ERROR)
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _ptimeToSampleAtom->ResetStateVariables();
        return EVERYTHING_FINE;
    }
    int32 retValB = _psampleToChunkAtom->resetStateVariables(_currentPlaybackSampleNumber);
    if (retValB == DEFAULT_ERROR)
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _psampleToChunkAtom->resetStateVariables();
        return EVERYTHING_FINE;
    }
    if (NULL != _pcompositionOffsetAtom)
    {
        int32 retValC = _pcompositionOffsetAtom->ResetStateVariables(_currentPlaybackSampleNumber);
        if (DEFAULT_ERROR == retValC)
        {
            _currentPlaybackSampleNumber = 0;
            _currentPlaybackSampleTimestamp = 0;
            _pcompositionOffsetAtom->ResetStateVariables();
            return EVERYTHING_FINE;
        }
    }
    if (_currentPlaybackSampleNumber > 0)
    {

        getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
    }
    else
    {
        _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
    }
    //Reset the file pointer to the correct location. In case of non-interleaved
    //pv content, we do not seek for every media sample.
    if (_oPVContentDownloadable)
    {
        uint32 sampleSize = 0;
        _psampleSizeAtom->getSampleSizeAt(_currentPlaybackSampleNumber, sampleSize);

        // Find chunk
        uint32 chunk = 0;
        MP4_ERROR_CODE errCode =
            _psampleToChunkAtom->getChunkNumberForSample(_currentPlaybackSampleNumber, chunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Find first sample in that chunk
        uint32 first = 0;
        errCode = _psampleToChunkAtom->getFirstSampleNumInChunk(chunk, first);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Find chunk offset to file
        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);

        //uint32 offset to int32, possible error for large files
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Need to add up all the sizes from the first sample in this run up to the
        // the requested sample (but not including it)
        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;
        for (uint32 i = first; i < (uint32)_currentPlaybackSampleNumber; i++)
        {
            // Check sample size from SampleSizeAtom
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
            // Check for error condition
            if (errCode != EVERYTHING_FINE)
            {
                _currentPlaybackSampleNumber = 0;
                return (READ_FAILED);
            }
            sampleSizeOffset += tempSize;
        }

        // Find actual file offset to sample
        TOsclFileOffset sampleFileOffset = (TOsclFileOffset)offset + sampleSizeOffset;

        if (!_pinput->IsOpen())
        {
            if (iOpenFileOncePerTrack)
            {
                // Check if file is already open
                // Opens file in binary mode with read sharing permissions
                // Return Error in case the file open fails.
                if (AtomUtils::OpenMP4File(_filename,
                                           Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                           _pinput) != 0)
                {
                    return FILE_OPEN_FAILED;
                }
            }
            else
            {
                _pinput->_fileSize = _commonFilePtr->_fileSize;
                _pinput->_pvfile.Copy(_commonFilePtr->_pvfile);
                AtomUtils::Flush(_pinput);
                AtomUtils::seekFromStart(_pinput, 0);
            }
            if (!_IsUpdateFileSize)
            {
                if (AtomUtils::getCurrentFileSize(_pinput, _fileSize) == false)
                {
                    _currentPlaybackSampleNumber = 0;
                    return (READ_FAILED);
                }
            }
        }

        if ((TOsclFileOffset)(sampleFileOffset + sampleSize) > _fileSize)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }
        AtomUtils::seekFromStart(_pinput, sampleFileOffset);
    }

    {
        uint32 chunk = 0;
        MP4_ERROR_CODE errCode = getMutableSampleToChunkAtom().
                                 getChunkNumberForSampleGet(_currentPlaybackSampleNumber, chunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 FirstSampleNumInChunk = 0;
        errCode = getSampleToChunkAtom().getFirstSampleNumInChunkGet(FirstSampleNumInChunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 offsetIntoRunOfChunks =
            (_currentPlaybackSampleNumber - FirstSampleNumInChunk);

        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);

        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;

        _currChunkOffset = 0;
        for (uint32 i = FirstSampleNumInChunk; i < FirstSampleNumInChunk +
                offsetIntoRunOfChunks; i++)
        {
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
            if (errCode != EVERYTHING_FINE)
            {
                _currentPlaybackSampleNumber = 0;
                return READ_FAILED;
            }
            sampleSizeOffset += tempSize;
        }
        _currChunkOffset = sampleSizeOffset;
    }
    return getNextNSamples(_currentPlaybackSampleNumber, n, pgau);;
}

MP4_ERROR_CODE
SampleTableAtom::getNextKeyMediaSample(uint32 &aKeySampleNum, uint32 *n, GAU  *pgau)
{
    if (_ptimeToSampleAtom  == NULL)
    {
        return DEFAULT_ERROR;
    }

    // Check if sample is an I-frame.  If not, need to find the sample number of the
    // first I-frame sample that follows ts
    // Need to check syncSampleAtom for this - if not present, all samples are synch samples
    // (i.e. all audio samples are synch samples)
    MP4_ERROR_CODE  errCode = EVERYTHING_FINE;
    if (_psyncSampleAtom != NULL)
    {
        errCode = getSyncSampleAtom()->getSyncSampleFollowing(_currentPlaybackSampleNumber, _currentPlaybackSampleNumber);
    }
    if (errCode != EVERYTHING_FINE)
    {
        return END_OF_TRACK;
    }

    aKeySampleNum = _currentPlaybackSampleNumber;

    // Increment ts for current sample in this random access: No more correct incase CTTS exits
    if (_currentPlaybackSampleNumber != 0)
    {
        // ts for sample 0 is 0

        getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
    }
    else
    {
        _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
    }

    int32 retValA = _ptimeToSampleAtom->ResetStateVariables(_currentPlaybackSampleNumber);
    if (retValA == DEFAULT_ERROR)
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _ptimeToSampleAtom->ResetStateVariables();
        return EVERYTHING_FINE;
    }
    int32 retValB = _psampleToChunkAtom->resetStateVariables(_currentPlaybackSampleNumber);
    if (retValB == DEFAULT_ERROR)
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _psampleToChunkAtom->resetStateVariables();
        return EVERYTHING_FINE;
    }
    if (NULL != _pcompositionOffsetAtom)
    {
        int32 retValC = _pcompositionOffsetAtom->ResetStateVariables(_currentPlaybackSampleNumber);
        if (DEFAULT_ERROR == retValC)
        {
            _currentPlaybackSampleNumber = 0;
            _currentPlaybackSampleTimestamp = 0;
            _pcompositionOffsetAtom->ResetStateVariables();
            return EVERYTHING_FINE;
        }
    }

    if (_currentPlaybackSampleNumber > 0)
    {

        getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
    }
    else
    {
        _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
    }
    //Reset the file pointer to the correct location. In case of non-interleaved
    //pv content, we do not seek for every media sample.
    if (_oPVContentDownloadable)
    {
        uint32 sampleSize = 0 ;
        _psampleSizeAtom->getSampleSizeAt(_currentPlaybackSampleNumber, sampleSize);

        // Find chunk
        uint32 chunk = 0;
        MP4_ERROR_CODE errCode =
            _psampleToChunkAtom->getChunkNumberForSample(_currentPlaybackSampleNumber, chunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Find first sample in that chunk
        uint32 first = 0;
        errCode = _psampleToChunkAtom->getFirstSampleNumInChunk(chunk, first);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }


        // Find chunk offset to file
        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);

        //uint32 offset to int32, possible error for large files
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Need to add up all the sizes from the first sample in this run up to the
        // the requested sample (but not including it)
        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;
        for (uint32 i = first; i < (uint32)_currentPlaybackSampleNumber; i++)
        {
            // Check sample size from SampleSizeAtom
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
            // Check for error condition
            if (errCode != EVERYTHING_FINE)
            {
                _currentPlaybackSampleNumber = 0;
                return (READ_FAILED);
            }
            sampleSizeOffset += tempSize;
        }

        // Find actual file offset to sample
        TOsclFileOffset sampleFileOffset = (TOsclFileOffset)offset + sampleSizeOffset;

        if (!_pinput->IsOpen())
        {
            if (iOpenFileOncePerTrack)
            {
                // Check if file is already open
                // Opens file in binary mode with read sharing permissions
                // Return Error in case the file open fails.
                if (AtomUtils::OpenMP4File(_filename,
                                           Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                           _pinput) != 0)
                {
                    return FILE_OPEN_FAILED;
                }
            }
            else
            {
                _pinput->_fileSize = _commonFilePtr->_fileSize;
                _pinput->_pvfile.Copy(_commonFilePtr->_pvfile);
                AtomUtils::Flush(_pinput);
                AtomUtils::seekFromStart(_pinput, 0);
            }
            if (!_IsUpdateFileSize)
            {
                if (AtomUtils::getCurrentFileSize(_pinput, _fileSize) == false)
                {
                    _currentPlaybackSampleNumber = 0;
                    return (READ_FAILED);
                }
            }
        }

        if ((TOsclFileOffset)(sampleFileOffset + sampleSize) > _fileSize)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }
        AtomUtils::seekFromStart(_pinput, sampleFileOffset);
    }

    {
        uint32 chunk = 0;
        MP4_ERROR_CODE errCode = getMutableSampleToChunkAtom().
                                 getChunkNumberForSampleGet(_currentPlaybackSampleNumber, chunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 FirstSampleNumInChunk = 0;
        errCode = getSampleToChunkAtom().getFirstSampleNumInChunkGet(FirstSampleNumInChunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 offsetIntoRunOfChunks =
            (_currentPlaybackSampleNumber - FirstSampleNumInChunk);

        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);

        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return READ_FAILED;
        }

        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;

        _currChunkOffset = 0;
        for (uint32 i = FirstSampleNumInChunk; i < FirstSampleNumInChunk +
                offsetIntoRunOfChunks; i++)
        {
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
            if (errCode != EVERYTHING_FINE)
            {
                _currentPlaybackSampleNumber = 0;
                return READ_FAILED;
            }
            sampleSizeOffset += tempSize;
        }
        _currChunkOffset = sampleSizeOffset;
    }

    return getNextNSamples(_currentPlaybackSampleNumber, n, pgau);;
}

// Returns next video frame
// Can optimize this by having the getNext()... use the seekg pointer to maintain the
// location where to read from
MP4_ERROR_CODE
SampleTableAtom::getNextSample(uint8 *buf, uint32 &size, uint32 &index, TOsclFileOffset &SampleOffset)
{
    int8 aFrameSizes[16] = {12, 13, 15, 17, 19, 20, 26, 31,
                            5,  0,  0,  0,  0,  0,  0,  0
                           };

    if (_ptimeToSampleAtom == NULL)
    {
        return READ_TIME_TO_SAMPLE_ATOM_FAILED;
    }

    if (!_IsUpdateFileSize)
    {
        if (_pinput->IsOpen())
        {
            if (AtomUtils::getCurrentFileSize(_pinput, _fileSize) == false)
            {
                return READ_FAILED;
            }
        }
    }

    uint32 tsDelta = 0;

    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getNextSample- _currentPlaybackSampleNumber =%d", _currentPlaybackSampleNumber));
    if (_currentPlaybackSampleNumber != 0)
    {
        MP4_ERROR_CODE errCode = _ptimeToSampleAtom->GetTimeDeltaForSampleNumberGet(_currentPlaybackSampleNumber, tsDelta);

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getNextSample- tsDelta =%d", tsDelta));

        if (errCode != EVERYTHING_FINE)
        {
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextSample - Read Time to Sample Atom Failed"));
            return (READ_TIME_TO_SAMPLE_ATOM_FAILED);
        }

        _currentPlaybackSampleTimestamp += (tsDelta + getCttsOffsetForSampleNumberGet(_currentPlaybackSampleNumber));

    }
    else
    {
        // Add TS offset to sample timestamps
        _currentPlaybackSampleTimestamp = 0;
    }
    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getNextSample- _currentPlaybackSampleTimestamp =%d", _currentPlaybackSampleTimestamp));
    if (_psampleDescriptionAtom->Is3GPPAMR())
    {
        if (_amrSampleSize > 0)
        {
            uint8 frame_type = *(_pAMRTempBuffer + _amrTempBufferOffset);

            frame_type = (uint8)((frame_type >> 3) & 0x0F);

            _amrTempBufferOffset += 1;
            _amrSampleSize -= 1;

            if ((frame_type > 9) && (frame_type != 15))
            {//in fact, this should be "INVALID_AMR_FRAME_TYPE"
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextSample - Read AMR SAmple Entry Failed"));
                return (READ_AMR_SAMPLE_ENTRY_FAILED);
            }

            uint32 frame_size = aFrameSizes[(uint16)frame_type];
            index = frame_type;

            if (frame_size > size)
            {
                size = frame_size;
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextSample - Insufficient Buffer Size"));
                return (INSUFFICIENT_BUFFER_SIZE);
            }

            oscl_memcpy((void *)(buf),
                        (void *)(_pAMRTempBuffer + _amrTempBufferOffset),
                        frame_size);

            _amrTempBufferOffset += frame_size;
            _amrSampleSize       -= frame_size;

            size = frame_size;
            return EVERYTHING_FINE;
        }
        else
        {
            _amrTempBufferOffset = 0;

            _amrSampleSize = 512;
            MP4_ERROR_CODE retval = getSample(_currentPlaybackSampleNumber, _pAMRTempBuffer, _amrSampleSize, index, SampleOffset);

            if (retval != EVERYTHING_FINE)
            {
                // Reset Time Stamp
                _currentPlaybackSampleTimestamp -= (tsDelta + getCttsOffsetForSampleNumber(_currentPlaybackSampleNumber));

                return retval;
            }
            _currentPlaybackSampleNumber++;

            _amrFrameTimeStamp = _currentPlaybackSampleTimestamp;

            uint8 frame_type = *(_pAMRTempBuffer + _amrTempBufferOffset);

            frame_type = (uint8)((frame_type >> 3) & 0x0F);

            _amrTempBufferOffset += 1;
            _amrSampleSize -= 1;

            if ((frame_type > 9) && (frame_type != 15))
            {//in fact, this should be "INVALID_AMR_FRAME_TYPE"
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextSample - Read AMR SAmple Entry Failed"));
                return (READ_AMR_SAMPLE_ENTRY_FAILED);
            }

            uint32 frame_size = aFrameSizes[(uint16)frame_type];

            index = frame_type;

            if (frame_size > size)
            {
                size = frame_size;
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextSample - Insufficient Buffer Size"));
                return (INSUFFICIENT_BUFFER_SIZE);
            }

            oscl_memcpy((void *)(buf),
                        (void *)(_pAMRTempBuffer + _amrTempBufferOffset),
                        frame_size);

            _amrTempBufferOffset += frame_size;
            _amrSampleSize       -= frame_size;

            size = frame_size;
            return EVERYTHING_FINE;
        }
    }
    else
    {
        MP4_ERROR_CODE retVal = getSample(_currentPlaybackSampleNumber, buf, size, index, SampleOffset);

        if (retVal == EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber++;
        }
        else
        {
            // Reset Time Stamp
            _currentPlaybackSampleTimestamp -= (tsDelta + getCttsOffsetForSampleNumber(_currentPlaybackSampleNumber));
        }

        return retVal;
    }
}

/*Reset PlayBack from Beginning*/
void SampleTableAtom :: resetPlayBack()
{
    _currentPlaybackSampleNumber    = 0;
    _currentPlaybackSampleTimestamp = 0;
    _oResidualSample = false;
    _remainingFramesInSample = 0;
    _amrTempBufferOffset = 0;
    _ptimeToSampleAtom->ResetStateVariables();
    if (NULL != _pcompositionOffsetAtom)
    {
        _pcompositionOffsetAtom->ResetStateVariables();
    }
    if (NULL != _psampleToChunkAtom)
    {
        _psampleToChunkAtom->resetStateVariables();
    }
    _currChunkOffset = 0;
    _editIndex = 0;
    _prevEditDuration = 0;
    _tsDelta = 0;
    _editOffset = 0;
}

void SampleTableAtom ::resetTrackToEOT()
{
    if (_psampleSizeAtom)
    {
        _currentPlaybackSampleNumber  = getSampleSizeAtom().getSampleCount();
    }
}

uint64 SampleTableAtom::queryRepositionTime(uint64 time, bool oDependsOn, bool bBeforeRequestedTime)
{
    uint64 trueTS = 0;
    uint32 currPlaybackSampleNum = 0;
    uint64 currPlaybackSampleTS = 0;
    MP4_ERROR_CODE retValA = EVERYTHING_FINE;

    if ((_psampleSizeAtom    == NULL) ||
            (_psampleToChunkAtom == NULL) ||
            (_ptimeToSampleAtom  == NULL) ||
            (_pchunkOffsetAtom   == NULL))
    {
        return 0;
    }

    if (time > _trackStartTSOffset)
    {
        retValA = getTimeToSampleAtom().
                  GetSampleNumberFromTimestamp((time - _trackStartTSOffset), currPlaybackSampleNum);
        // Go for composition offset adjustment.
        currPlaybackSampleNum =
            getSampleNumberAdjustedWithCTTS(time , currPlaybackSampleNum);
    }
    else
    {
        trueTS = 0;
        return (trueTS);
    }

    if (retValA == DEFAULT_ERROR)
    {
        // SHOULD COME HERE ONLY IN CASE OF AUDIO & TEXT AND WE WANT TO SET THE
        // SAMPLE NUMBER IN THAT CASE TO THE LAST SAMPLE IN THE FILE
        if (getSampleSizeAtom().getSampleCount() > OSCL_STATIC_CAST(uint32, 0))
        {
            currPlaybackSampleNum =
                (getSampleSizeAtom().getSampleCount() - 1);

            // This is to ensure that _currentPlaybackSampleTimestamp is set to the previous
            // sample, and not to the current sample, as this sample is yet to be read.
            if (currPlaybackSampleNum > 0)
            {
                getTimestampForSampleNumber((currPlaybackSampleNum - 1), currPlaybackSampleTS);
            }
            else //
            {
                currPlaybackSampleTS += getCttsOffsetForSampleNumber(0);
            }
        }
        else
        {
            currPlaybackSampleNum = 0;
            currPlaybackSampleTS = 0;
        }
        trueTS =  currPlaybackSampleTS;
        return (trueTS);

    }

    //for video reset, we need to locate I frame
    if (_psampleDescriptionAtom->getMediaType() == MEDIA_TYPE_VISUAL)
    {
        //get the nearest I frame in front only for Base layer
        if (!oDependsOn)
        {

            if (getSyncSampleAtom() != NULL)
            {
                if (getSyncSampleAtom()->getEntryCount() != 0)
                {
                    if (bBeforeRequestedTime == true)
                    {
                        getSyncSampleAtom()->getSyncSampleBefore(currPlaybackSampleNum, currPlaybackSampleNum);
                    }
                    else
                    {
                        MP4_ERROR_CODE errCode =
                            getSyncSampleAtom()->getSyncSampleFollowing(currPlaybackSampleNum, currPlaybackSampleNum);
                        if (errCode != EVERYTHING_FINE)
                        {
                            currPlaybackSampleNum = 0;
                        }
                    }
                }
                else
                {
                    // ZERO SYNC SAMPLE ENTRIES - ERROR CONDITION - RESET TO THR BEGINNING OF THE
                    // CLIP
                    currPlaybackSampleNum = 0;
                    trueTS = 0;
                    return (trueTS);
                }
            }
            else
            {
                // NO SYNC SAMPLE ATOM - ERROR CONDITION - RESET TO THR BEGINNING OF THE
                // CLIP
                currPlaybackSampleNum = 0;
                trueTS = 0;
                return (trueTS);
            }
            //get the timestamp based on the sample number
        }
        if (currPlaybackSampleNum != 0)
        { // ts for sample 0 is 0: No more correct incase CTTS exists
            getTimestampForSampleNumber(currPlaybackSampleNum, currPlaybackSampleTS);
        }
        else
        {
            currPlaybackSampleTS = getCttsOffsetForSampleNumber(0);
        }
    }
    else if (_psampleDescriptionAtom->getMediaType() == MEDIA_TYPE_AUDIO)
    {


        if (currPlaybackSampleNum > 0)
        {

            getTimestampForSampleNumber(currPlaybackSampleNum, currPlaybackSampleTS);

            if (oDependsOn)
            {

                /*
                 * This check is required to ensure that audio track is positioned
                 * ahead of the intra frame. This is 'cos the getSampleNumberFromTimestamp
                 * returns the sample just before the passed time - This inturn is required
                 * for video tracks where in the positioning has to be the closest intra frame
                 * in the past.
                 */

                if (currPlaybackSampleTS < time)
                {
                    if ((currPlaybackSampleNum + 1) < getSampleSizeAtom().getSampleCount())
                    {

                        getTimestampForSampleNumber(currPlaybackSampleNum, currPlaybackSampleTS);
                    }
                }
            }
        }
        else
        {
            currPlaybackSampleTS = getCttsOffsetForSampleNumber(0);
        }
    }
    trueTS = currPlaybackSampleTS;
    return trueTS;
}

uint64 SampleTableAtom::resetPlayBackbyTime(uint64 time, bool oDependsOn)
{
    uint64 ret = resetTrackByTime(time, oDependsOn);
    getEditListEntryForTimeStamp(_currentPlaybackSampleTimestamp);
    return ret;
}

uint64 SampleTableAtom::resetTrackByTime(uint64 time, bool oDependsOn)
{
    int64 trueTS = 0;
    int32 retValA = 0 , retValB = 0;

    if ((_psampleSizeAtom    == NULL) ||
            (_psampleToChunkAtom == NULL) ||
            (_ptimeToSampleAtom  == NULL) ||
            (_pchunkOffsetAtom   == NULL))
    {
        return 0;
    }

    _oResidualSample = false;
    _remainingFramesInSample = 0;
    _amrTempBufferOffset = 0;

    if (time > 0)
    {
        uint32 _tempSampleNumber = 0;
        getTimeToSampleAtom().GetSampleNumberFromTimestamp(time, _tempSampleNumber);
        _tempSampleNumber =
            getSampleNumberAdjustedWithCTTS(time, _tempSampleNumber);

        if (_currentPlaybackSampleNumber > _tempSampleNumber)
        {
            // In case of Backward Repos,the flag _SkipOldEntry in samplesizeatom.cpp is set to true to prevent
            // old entries from getting used.
            getSampleSizeAtom()._SkipOldEntry = true;
        }
        retValA = getTimeToSampleAtom().
                  GetSampleNumberFromTimestamp(time, _currentPlaybackSampleNumber);

        // Go for composition offset adjustment.
        _currentPlaybackSampleNumber =
            getSampleNumberAdjustedWithCTTS(time, _currentPlaybackSampleNumber);

    }
    else
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _currChunkOffset = 0;
        trueTS = 0;
        _ptimeToSampleAtom->ResetStateVariables();
        if (NULL != _pcompositionOffsetAtom)
        {
            _pcompositionOffsetAtom->ResetStateVariables();
        }
        _psampleToChunkAtom->resetStateVariables();
        return (trueTS);
    }

    if (retValA == DEFAULT_ERROR)
    {
        // SHOULD COME HERE ONLY IN CASE OF AUDIO & TEXT AND WE WANT TO SET THE
        // SAMPLE NUMBER IN THAT CASE TO THE LAST SAMPLE IN THE FILE
        if (getSampleSizeAtom().getSampleCount() > OSCL_STATIC_CAST(uint32, 0))
        {
            _currentPlaybackSampleNumber =
                (getSampleSizeAtom().getSampleCount());

            retValA = _ptimeToSampleAtom->ResetStateVariables(_currentPlaybackSampleNumber);
            if (retValA == DEFAULT_ERROR)
            {
                _currentPlaybackSampleNumber = 0;
                _currentPlaybackSampleTimestamp = 0;
                _ptimeToSampleAtom->ResetStateVariables();
                return 0;
            }
            int32 retValB = _psampleToChunkAtom->resetStateVariables(_currentPlaybackSampleNumber);
            if (retValB == DEFAULT_ERROR)
            {
                _currentPlaybackSampleNumber = 0;
                _currentPlaybackSampleTimestamp = 0;
                _psampleToChunkAtom->resetStateVariables();
                return 0;
            }
            if (NULL != _pcompositionOffsetAtom)
            {
                int32 retValC = _pcompositionOffsetAtom->ResetStateVariables(_currentPlaybackSampleNumber);
                if (DEFAULT_ERROR == retValC)
                {
                    _currentPlaybackSampleNumber = 0;
                    _currentPlaybackSampleTimestamp = 0;
                    _pcompositionOffsetAtom->ResetStateVariables();
                    return 0;
                }
            }
            if (_currentPlaybackSampleNumber > 0)
            {
                getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
            }
            else
            {
                _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
            }
        }
        else
        {
            _currentPlaybackSampleNumber = 0;
            _currentPlaybackSampleTimestamp = 0;
        }
        trueTS =  _currentPlaybackSampleTimestamp;
        goto check_for_file_pointer_reset;
    }

    //for video reset, we need to locate I frame
    if (_psampleDescriptionAtom->getMediaType() == MEDIA_TYPE_VISUAL)
    {
        //get the nearest I frame in front only for Base layer
        if (!oDependsOn)
        {
            if (getSyncSampleAtom() != NULL)
            {
                if (getSyncSampleAtom()->getEntryCount() != 0)
                {
                    getSyncSampleAtom()->getSyncSampleBefore(_currentPlaybackSampleNumber, _currentPlaybackSampleNumber);
                }
                else
                {

                    // ZERO SYNC SAMPLE ENTRIES - ERROR CONDITION - RESET TO THR BEGINNING OF THE
                    // CLIP
                    _currentPlaybackSampleNumber = 0;
                    _currentPlaybackSampleTimestamp = 0;
                    _currChunkOffset = 0;
                    trueTS = 0;
                    retValA = _ptimeToSampleAtom->ResetStateVariables(_currentPlaybackSampleNumber);
                    if (retValA == DEFAULT_ERROR)
                    {
                        _currentPlaybackSampleNumber = 0;
                        _ptimeToSampleAtom->ResetStateVariables();
                        return 0;
                    }

                    int32 retValB = _psampleToChunkAtom->resetStateVariables(_currentPlaybackSampleNumber);
                    if (retValB == DEFAULT_ERROR)
                    {
                        _currentPlaybackSampleNumber = 0;
                        _psampleToChunkAtom->resetStateVariables();
                        return 0;
                    }

                    if (NULL != _pcompositionOffsetAtom)
                    {
                        int32 retValC = _pcompositionOffsetAtom->ResetStateVariables(_currentPlaybackSampleNumber);
                        if (DEFAULT_ERROR == retValC)
                        {
                            _currentPlaybackSampleNumber = 0;
                            _pcompositionOffsetAtom->ResetStateVariables();
                            return 0;
                        }
                    }
                    return (trueTS);
                }
            }
            else
            {
                // NO SYNC SAMPLE ATOM - ERROR CONDITION - RESET TO THR BEGINNING OF THE
                // CLIP
                _currentPlaybackSampleNumber = 0;
                _currentPlaybackSampleTimestamp = 0;
                _currChunkOffset = 0;
                trueTS = 0;
                _ptimeToSampleAtom->ResetStateVariables();
                if (NULL != _pcompositionOffsetAtom)
                {
                    _pcompositionOffsetAtom->ResetStateVariables();
                }
                _psampleToChunkAtom->resetStateVariables();
                return (trueTS);
            }
        }
        else
        {
            const uint32 totalnumSamples = getSampleSizeAtom().getSampleCount();
            if ((uint32)_currentPlaybackSampleNumber < totalnumSamples)
            {
                // FOR ENHANCE LAYER GET THE SAMPLE CLOSEST TO THE CORRESPONDING BASE LAYER
                // AND INCREMENT IT BY ONE, AS THE SAMPLE CLOSEST ALWAYS RETURNS ONE PREVIOUS
                _currentPlaybackSampleNumber++;
            }
        }
        //get the timestamp based on the sample number
        if (_currentPlaybackSampleNumber != 0)
        { // ts for sample 0 is 0

            getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
        }
        else
        {
            _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
        }
    }
    else if (_psampleDescriptionAtom->getMediaType() == MEDIA_TYPE_AUDIO)
    {
        if (_currentPlaybackSampleNumber > 0)
        {

            getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);

            if (oDependsOn)
            {
                /*
                 * This check is required to ensure that audio tracks are positioned
                 * ahead of the intra frame. This is 'cos the getSampleNumberFromTimestamp
                 * returns the sample just before the passed time - This inturn is required
                 * for video tracks where in the positioning has to be the closest intra frame
                 * in the past.
                 */
                if (_currentPlaybackSampleTimestamp < (uint32)time)
                {
                    if ((_currentPlaybackSampleNumber + 1) < getSampleSizeAtom().getSampleCount())
                    {
                        _currentPlaybackSampleNumber++;

                        getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
                    }
                }
            }
        }
        else
        {
            _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
        }
    }
    else
    {
        if (_currentPlaybackSampleNumber > 0)
        {

            getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
        }
        else
        {
            _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
        }
    }

    trueTS = _currentPlaybackSampleTimestamp;

    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::resetPlayBackbyTime- _currentPlaybackSampleTimestamp =%d", _currentPlaybackSampleTimestamp));
    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::resetPlayBackbyTime- _currentPlaybackSampleNumber =%d", _currentPlaybackSampleNumber));
    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::resetPlayBackbyTime- trueTS =%d", trueTS));

    retValA = _ptimeToSampleAtom->ResetStateVariables(_currentPlaybackSampleNumber);
    if (retValA == DEFAULT_ERROR)
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _ptimeToSampleAtom->ResetStateVariables();
        return 0;
    }
    retValB = _psampleToChunkAtom->resetStateVariables(_currentPlaybackSampleNumber);
    if (retValB == DEFAULT_ERROR)
    {
        _currentPlaybackSampleNumber = 0;
        _currentPlaybackSampleTimestamp = 0;
        _psampleToChunkAtom->resetStateVariables();
        return 0;
    }
    if (NULL != _pcompositionOffsetAtom)
    {
        if (DEFAULT_ERROR == _pcompositionOffsetAtom->ResetStateVariables(_currentPlaybackSampleNumber))
        {
            _currentPlaybackSampleNumber = 0;
            _currentPlaybackSampleTimestamp = 0;
            _pcompositionOffsetAtom->ResetStateVariables();
            return 0;
        }
    }

    {
        uint32 chunk = 0;
        MP4_ERROR_CODE errCode = getMutableSampleToChunkAtom().
                                 getChunkNumberForSampleGet(_currentPlaybackSampleNumber, chunk);
        if (errCode != EVERYTHING_FINE)
        {
            return errCode;
        }

        uint32 FirstSampleNumInChunk = 0;
        errCode = getSampleToChunkAtom().getFirstSampleNumInChunkGet(FirstSampleNumInChunk);
        if (errCode != EVERYTHING_FINE)
        {
            return errCode;
        }

        uint32 offsetIntoRunOfChunks =
            (_currentPlaybackSampleNumber - FirstSampleNumInChunk);

        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);

        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            trueTS = 0;
            return (trueTS);
        }

        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;

        _currChunkOffset = 0;
        for (uint32 i = FirstSampleNumInChunk; i < FirstSampleNumInChunk +
                offsetIntoRunOfChunks; i++)
        {
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
            if (errCode == DEFAULT_ERROR)
            {
                _currentPlaybackSampleNumber = 0;
                trueTS = 0;
                return (trueTS);
            }
            sampleSizeOffset += tempSize;
        }
        _currChunkOffset = sampleSizeOffset;
    }

check_for_file_pointer_reset:
    //Reset the file pointer to the correct location. In case of non-interleaved
    //pv content, we do not seek for every media sample.
    if (_oPVContentDownloadable)
    {
        uint32 sampleSize = 0;
        _psampleSizeAtom->getSampleSizeAt(_currentPlaybackSampleNumber, sampleSize);

        // Find chunk
        uint32 chunk = 0;
        MP4_ERROR_CODE errCode =
            _psampleToChunkAtom->getChunkNumberForSample(_currentPlaybackSampleNumber, chunk);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }

        // Find first sample in that chunk
        uint32 first = 0;
        errCode = _psampleToChunkAtom->getFirstSampleNumInChunk(chunk, first);
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            return (READ_FAILED);
        }


        // Find chunk offset to file
        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);

        //uint32 offset to int32, possible error for large files
        if (errCode != EVERYTHING_FINE)
        {
            _currentPlaybackSampleNumber = 0;
            trueTS = 0;
            return (trueTS);
        }

        // Need to add up all the sizes from the first sample in this run up to the
        // the requested sample (but not including it)
        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;
        for (uint32 i = first; i < (uint32)_currentPlaybackSampleNumber; i++)
        {
            // Check sample size from SampleSizeAtom
            tempSize = 0;
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);
            // Check for error condition
            if (errCode != EVERYTHING_FINE)
            {
                _currentPlaybackSampleNumber = 0;
                trueTS = 0;
                return (trueTS);
            }
            sampleSizeOffset += tempSize;
        }

        // Find actual file offset to sample
        TOsclFileOffset sampleFileOffset = (TOsclFileOffset)offset + sampleSizeOffset;

        if (!_pinput->IsOpen())
        {
            if (iOpenFileOncePerTrack)
            {
                // Check if file is already open
                // Opens file in binary mode with read sharing permissions
                // Return Error in case the file open fails.
                if (AtomUtils::OpenMP4File(_filename,
                                           Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                           _pinput) != 0)
                {
                    return FILE_OPEN_FAILED;
                }
            }
            else
            {
                _pinput->_fileSize = _commonFilePtr->_fileSize;
                _pinput->_pvfile.Copy(_commonFilePtr->_pvfile);
                AtomUtils::Flush(_pinput);
                AtomUtils::seekFromStart(_pinput, 0);
            }
            if (!_IsUpdateFileSize)
            {
                if (AtomUtils::getCurrentFileSize(_pinput, _fileSize) == false)
                {
                    _currentPlaybackSampleNumber = 0;
                    trueTS = 0;
                    return (trueTS);
                }
            }
        }

        if ((TOsclFileOffset)(sampleFileOffset + sampleSize) > _fileSize)
        {
            _currentPlaybackSampleNumber = 0;
            trueTS = 0;
            return (trueTS);
        }
        AtomUtils::seekFromStart(_pinput, sampleFileOffset);
    }

    return trueTS;
}


//querySyncFrameBeforeTime
MP4_ERROR_CODE SampleTableAtom::IsResetNeeded(uint64 time)
{
    if (_psyncSampleAtom != NULL)
    {//not all I frames
        if (time > _currentPlaybackSampleTimestamp)
        {//forward
            uint32 nextSyncNum = 0;
            MP4_ERROR_CODE errCode = _psyncSampleAtom->getSyncSampleFollowing(_currentPlaybackSampleNumber, nextSyncNum);
            uint64 tsSampleNum = 0;
            getTimestampForSampleNumber(nextSyncNum, tsSampleNum);
            if ((errCode != EVERYTHING_FINE)
                    || (time < tsSampleNum))
            {//1. past the last sync frame, no reset is needed
                //2. the sync frame is after the repositioning time, no reset is needed
                return errCode;
            }
        }
    }
    return EVERYTHING_FINE;
}

int32 SampleTableAtom::getTimestampForRandomAccessPoints(uint32 *num, uint64 * tsBuf, uint32* numBuf, TOsclFileOffset* offsetBuf)
{
    if (_psyncSampleAtom == NULL)
    {
        if (_psampleSizeAtom != NULL)
        {
            *num = _psampleSizeAtom->getSampleCount();
        }
        return 2;   //success : every sample is a random access point
    }

    if (_ptimeToSampleAtom == NULL)
    {
        return 0;   //fail
    }

    uint32 tmp = _psyncSampleAtom->getEntryCount();

    if (*num == 0)
    {
        *num = tmp;
        return 1;   //success. This is only the query mode.
    }
    if (*num > tmp)
        *num = tmp;

    oscl_memcpy(numBuf, _psyncSampleAtom->getSampleNumberVector(),
                sizeof(uint32 *)*(*num));

    for (uint32 i = 0; i < *num; i++)
    {//it may crash if this buffer is not big enough
        getTimestampForSampleNumber(numBuf[i] - 1, tsBuf[i]);
        // Timestamps returned here should be presentation timestamps (must include
        // the ctts offset. Calling getTimestampForSampleNumber will take care of it.
        numBuf[i] = numBuf[i] - 1;
        TOsclFileOffset offset = 0;
        if (offsetBuf != NULL)
        {
            if (getOffsetByTime(tsBuf[i], &offset) != DEFAULT_ERROR)
                offsetBuf[i] = offset;
        }
    }
    return  1;  //success
}


int32 SampleTableAtom::getTimestampForRandomAccessPointsBeforeAfter(uint64 ts, uint64 *tsBuf, uint32* numBuf, uint32& numsamplestoget, uint32 howManyKeySamples)
{
    if (_psyncSampleAtom == NULL)
    {
        return 2;   //success : every sample is a random access point
    }

    if (_ptimeToSampleAtom == NULL)
    {
        return 0;   //fail
    }


    uint32 numSyncSamples = _psyncSampleAtom->getEntryCount();
    uint32 SampleNumber = 0;
    _ptimeToSampleAtom->GetSampleNumberFromTimestamp(ts, SampleNumber);

    // Go for composition offset adjustment.
    SampleNumber =
        getSampleNumberAdjustedWithCTTS(ts, SampleNumber);

    uint32 SampleNumberForTS =  0;
    MP4_ERROR_CODE errCode = _psyncSampleAtom->getSyncSampleBefore(SampleNumber, SampleNumberForTS);
    if (errCode != EVERYTHING_FINE)
    {
        return 0;   //fail
    }
    uint32 startIdx = 0, endIdx = 0, k = 0, idx = 0;
    for (idx = 0; idx < numSyncSamples; idx++)
    {
        uint32 tempSampleNum = 0;
        MP4_ERROR_CODE errCode = _psyncSampleAtom->getSampleNumberAt(idx, tempSampleNum);
        if ((errCode != EVERYTHING_FINE) && (tempSampleNum == SampleNumberForTS))
        {
            startIdx = idx + 1;
            endIdx = numSyncSamples;
            break;
        }
    }

    if ((startIdx + howManyKeySamples) <= numSyncSamples)
        endIdx = startIdx + howManyKeySamples;

    if (startIdx >= howManyKeySamples)
        startIdx -= howManyKeySamples;
    else
        startIdx = 0;

    idx = 0;
    for (idx = startIdx; idx < endIdx; idx++)
    {
        uint32 keySampleNum = 0;
        MP4_ERROR_CODE  errCode = _psyncSampleAtom->getSampleNumberAt(idx, keySampleNum);
        uint64 keySampleTS = 0;
        if (errCode == EVERYTHING_FINE)
        {
            // Timestamps returned here should be presentation timestamps (must include
            // the ctts offset. Calling getTimestampForSampleNumber will take care of it.
            errCode = getTimestampForSampleNumber(keySampleNum, keySampleTS);
            if (errCode == EVERYTHING_FINE)
            {
                numBuf[k] = keySampleNum;
                tsBuf[k] = keySampleTS;
                k++;
            }
        }
    }
    numsamplestoget = k;

    return  1;  //success
}

MP4_ERROR_CODE SampleTableAtom::getTimestampForSampleNumber(uint32 sampleNumber, uint64& aTimeStamp)
{
    if ((NULL != _ptimeToSampleAtom) && (NULL != _pcompositionOffsetAtom))
    {
        uint64 tts = 0;
        uint32 ctots = 0;
        MP4_ERROR_CODE errCode =
            _ptimeToSampleAtom->GetTimestampForSampleNumber(sampleNumber, tts);
        if (errCode == EVERYTHING_FINE)
        {
            errCode = _pcompositionOffsetAtom->GetTimeOffsetForSampleNumber(sampleNumber, ctots);
            if (errCode == EVERYTHING_FINE)
            {
                aTimeStamp = tts + ctots;
                return EVERYTHING_FINE;
            }
            else
            {
                return errCode;
            }
        }
        else
        {
            return errCode;
        }
    }

    else if (NULL != _ptimeToSampleAtom)
    {
        uint64 tts = 0;
        _ptimeToSampleAtom->GetTimestampForSampleNumber(sampleNumber, tts);
        aTimeStamp = tts;
        return EVERYTHING_FINE;
    }

    // It is not permitted that _pcompositionOffsetAtom without _ptimeToSampleAtom. Thus for any other
    // combination return 0.
    else
    {
        aTimeStamp = 0;
        return EVERYTHING_FINE;
    }
}

int32 SampleTableAtom::getCttsOffsetForSampleNumberPeek(uint32 sampleNumber)
{
    uint32 tempCompositionOffset = 0;
    // compositionOffsetAtom adjustment if compositionOffsetAtom exits
    if (NULL != _pcompositionOffsetAtom)
    {
        MP4_ERROR_CODE errCode = _pcompositionOffsetAtom->GetTimeOffsetForSampleNumberPeek(sampleNumber, tempCompositionOffset);
        if (errCode == EVERYTHING_FINE)
        {
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getCttsOffsetForSampleNumberPeek- CTTS(%d) = %d", sampleNumber, tempCompositionOffset));
            return tempCompositionOffset;
        }
        else
        {
            // It's not an issue if the CTTS is not present or not parsed correctly.
            return 0;
        }
    }
    return 0;
}

// The difference between getCttsOffsetForSampleNumber and getCttsOffsetForSampleNumberGet
// is that the getCttsOffsetForSampleNumberGet is meant to be caled in a consequtive manner,
// However getCttsOffsetForSampleNumber could be called anywhere without past reference.
int32 SampleTableAtom::getCttsOffsetForSampleNumber(uint32 sampleNumber)
{
    uint32 tempCompositionOffset = 0;
    // compositionOffsetAtom adjustment if compositionOffsetAtom exits
    if (NULL != _pcompositionOffsetAtom)
    {
        MP4_ERROR_CODE errCode = _pcompositionOffsetAtom->GetTimeOffsetForSampleNumber(sampleNumber, tempCompositionOffset);
        if (DEFAULT_ERROR != errCode)
        {
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getCttsOffsetForSampleNumber- CTTS(%d) = %d", sampleNumber, tempCompositionOffset));
            return tempCompositionOffset;
        }
        else
        {
            // It's not an issue if the CTTS is not present or not parsed correctly.
            return 0;
        }
    }
    return 0;
}

int32 SampleTableAtom::getCttsOffsetForSampleNumberGet(uint32 sampleNumber)
{
    uint32 tempCompositionOffset = 0;
    // compositionOffsetAtom adjustment if compositionOffsetAtom exits
    if (NULL != _pcompositionOffsetAtom)
    {
        MP4_ERROR_CODE errCode = _pcompositionOffsetAtom->GetTimeOffsetForSampleNumberGet(sampleNumber, tempCompositionOffset);
        if (EVERYTHING_FINE == errCode)
        {
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getCttsOffsetForSampleNumberGet- CTTS(%d) = %d", sampleNumber, tempCompositionOffset));
            return tempCompositionOffset;
        }
        else
        {
            // It's not an issue if the CTTS is not present or not parsed correctly.
            return 0;
        }
    }
    return 0;
}


// Returns next I-frame at time ts (in milliseconds) - or the very next I-frame in the stream
int32
SampleTableAtom::getNextSampleAtTime(uint32 ts, uint8 *buf, uint32 &size, uint32 &index, TOsclFileOffset &SampleOffset)
{
    if ((_ptimeToSampleAtom  == NULL) ||
            (_psyncSampleAtom    == NULL))
    {
        size = 0;
        return 0;
    }

    // Get sample number at timestamp
    MP4_ERROR_CODE errCode = _ptimeToSampleAtom->GetSampleNumberFromTimestamp(ts, _currentPlaybackSampleNumber);

    if (errCode != EVERYTHING_FINE)
    {
        size = 0;
        return 0;
    }

    // Go for composition offset adjustment.
    _currentPlaybackSampleNumber =
        getSampleNumberAdjustedWithCTTS(ts, _currentPlaybackSampleNumber);

    // Check if sample is an I-frame.  If not, need to find the sample number of the
    // first I-frame sample that follows ts
    // Need to check syncSampleAtom for this - if not present, all samples are synch samples
    // (i.e. all audio samples are synch samples)
    errCode =
        _psyncSampleAtom->getSyncSampleFollowing(_currentPlaybackSampleNumber, _currentPlaybackSampleNumber);

    if (errCode != EVERYTHING_FINE)
    {
        size = 0;
        return 0;
    }

    // Increment ts for current sample in this random access; No more correct in case of CTTS presence
    if (_currentPlaybackSampleNumber != 0)
    {
        // ts for sample 0 is 0
        getTimestampForSampleNumber(_currentPlaybackSampleNumber, _currentPlaybackSampleTimestamp);
    }
    else
    {
        _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
    }

    return getSample(_currentPlaybackSampleNumber++, buf, size, index, SampleOffset);
}


MP4_ERROR_CODE
SampleTableAtom::getNextBundledAccessUnits(uint32 *n,
        GAU    *pgau)
{
    MP4_ERROR_CODE nReturn = END_OF_TRACK;

    if ((_psampleSizeAtom    == NULL) ||
            (_psampleToChunkAtom == NULL) ||
            (_ptimeToSampleAtom  == NULL) ||
            (_pchunkOffsetAtom   == NULL))
    {
        *n = 0;
        //treat this as end of track as far as movie atom goes
        //if there are samples in moof it is handled in mpeg4file
        return nReturn;
    }

    handleEditList(_currentPlaybackSampleTimestamp);

    uint32 numSamples = getSampleSizeAtom().getSampleCount();

    if (_currentPlaybackSampleNumber >= numSamples)
    {
        *n = 0;
        pgau->info[0].ts = 0;   //_currentPlaybackSampleTimestamp;
        nReturn = END_OF_TRACK;
        return nReturn;
    }

    // save the sample number of the first sample in the bundle
    uint32 fNum = _currentPlaybackSampleNumber;

    nReturn = getNextNSamples(_currentPlaybackSampleNumber, n, pgau);

    if (0 != *n)
    {
        // set the sample number of the first sample in the bundle
        pgau->frameNum = fNum;
    }

    if (nReturn == INSUFFICIENT_BUFFER_SIZE)
    {
        return nReturn;
    }

    /*
     * This check is intentionally not done in case of 3GPP AMR as there can
     * still be frames in the temp buffer, and they will be retrieved when
     * get N is called the next time.
     */
    if (!_psampleDescriptionAtom->Is3GPPAMR())
    {
        if (_currentPlaybackSampleNumber >= numSamples)
        {
            nReturn = END_OF_TRACK;
            return nReturn;
        }
    }

    if ((nReturn == INVALID_SAMPLE_SIZE) ||
            (nReturn == INVALID_CHUNK_OFFSET))
    {
        nReturn = READ_FAILED;
        return nReturn;
    }
    return nReturn;
}

MP4_ERROR_CODE
SampleTableAtom::peekNextBundledAccessUnits(uint32 *n,
        MediaMetaInfo *mInfo)
{
    MP4_ERROR_CODE nReturn = END_OF_TRACK;

    if ((_psampleSizeAtom    == NULL) ||
            (_psampleToChunkAtom == NULL) ||
            (_ptimeToSampleAtom  == NULL) ||
            (_pchunkOffsetAtom   == NULL))
    {
        *n = 0;
        //treat this as end of track as far as movie atom goes
        //if there are samples in moof it is handled in mpeg4file
        return nReturn;
    }

    nReturn = peekNextNSamples(_currentPlaybackSampleNumber, n, mInfo);

    if ((nReturn == INVALID_SAMPLE_SIZE) ||
            (nReturn == INVALID_CHUNK_OFFSET))
    {
        nReturn = READ_FAILED;
        return nReturn;
    }

    return nReturn;
}

MP4_ERROR_CODE
SampleTableAtom::getNextNSamples(uint32 startSampleNum,
                                 uint32 *n,
                                 GAU    *pgau)
{
    uint32 chunk = 0;
    uint32 numSamplesPerChunk = 0;
    uint32 numChunksInRun = 0, FirstSampleNumInChunk = 0;
    uint32 offsetIntoRunOfChunks = 0, samplesLeftInChunk = 0;
    uint32 numSamples = 0;
    uint64 startSampleNumTSBase = 0, currTSBase = 0;
    uint32 samplesYetToBeRead = *n;
    uint32 sampleNum = startSampleNum;
    bool oStoreOffset = false;
    uint32 sampleBeforeGet = startSampleNum;

    uint32 s = 0;
    uint32 end = 0;

    uint32 i = 0, j = 0, k = 0;

    MP4_ERROR_CODE _mp4ErrorCode = EVERYTHING_FINE;

    if (!_IsUpdateFileSize)
    {
        if (_pinput->IsOpen())
        {
            if (AtomUtils::getCurrentFileSize(_pinput, _fileSize) == false)
            {
                *n = 0;
                _mp4ErrorCode = READ_FAILED;
                return _mp4ErrorCode;
            }
        }
    }

    currTSBase = _currentPlaybackSampleTimestamp;
    startSampleNumTSBase = _currentPlaybackSampleTimestamp;
    end = pgau->buf.num_fragments;

    GAU tempGau;
    tempGau = *pgau;
    GAU* tempgauPtr = &tempGau;

    k = 0;

    uint32 totalnumSamples = getSampleSizeAtom().getSampleCount();

    if (sampleNum >= totalnumSamples)
    {
        *n = 0;
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - End Of Track"));
        _mp4ErrorCode = END_OF_TRACK;
        return (_mp4ErrorCode);
    }

#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
    uint32 totalTimeSTSC = 0;
    uint32 totalTimeSTSZ = 0;
    uint32 totalTimeSTTS = 0;
    uint32 totalTimeCTTS = 0;
    uint32 totalTimeFileSeek = 0;
    uint32 totalTimeFileRead = 0;

    uint32 currticks, StartTime, EndTime;
#endif
    uint32 totalBytesRead = 0;
    while (samplesYetToBeRead)
    {
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
        currticks = OsclTickCount::TickCount();
        StartTime = OsclTickCount::TicksToMsec(currticks);
#endif
        // Find chunk
        chunk = 0;
        _mp4ErrorCode = getMutableSampleToChunkAtom().
                        getChunkNumberForSampleGet(sampleNum, chunk);
        if (_mp4ErrorCode != EVERYTHING_FINE)
        {
            *n = 0;
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - Read Sample to Chunk Atom Failed"));
            return _mp4ErrorCode;
        }

        //This method needs to be added to sample to chunk atom
        _mp4ErrorCode =
            getSampleToChunkAtom().getNumChunksInRunofChunksGet(numChunksInRun);

        if (_mp4ErrorCode != EVERYTHING_FINE)
        {
            *n = 0;
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - Read Sample to Chunk Atom Failed"));
            return (_mp4ErrorCode);
        }

        numSamplesPerChunk = 0;
        MP4_ERROR_CODE errCode = getSampleToChunkAtom().getSamplesPerChunkCorrespondingToSampleGet(numSamplesPerChunk);
        if (errCode != EVERYTHING_FINE)
        {
            *n = 0;
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - getSampleToChunkAtom().getSamplesPerChunkCorrespondingToSampleGet() Failed"));
            return READ_FAILED;
        }

        FirstSampleNumInChunk = 0;
        errCode = getSampleToChunkAtom().getFirstSampleNumInChunkGet(FirstSampleNumInChunk);
        if (errCode != EVERYTHING_FINE)
        {
            *n = 0;
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - getSampleToChunkAtom().getFirstSampleNumInChunkGet() Failed"));
            return READ_FAILED;
        }

#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
        currticks = OsclTickCount::TickCount();
        EndTime = OsclTickCount::TicksToMsec(currticks);
        totalTimeSTSC += (EndTime - StartTime);
#endif

        offsetIntoRunOfChunks =
            (sampleNum - FirstSampleNumInChunk);

        // All chunks in a run of chunks ARE NOT CONTIGUOUS!!!!
        samplesLeftInChunk =
            ((numSamplesPerChunk) - offsetIntoRunOfChunks);

        if (samplesLeftInChunk  >  samplesYetToBeRead)
        {
            numSamples = samplesYetToBeRead;
            samplesYetToBeRead = 0;
            oStoreOffset = true;
        }
        else
        {
            samplesYetToBeRead -= samplesLeftInChunk;
            numSamples = samplesLeftInChunk;
            oStoreOffset = false;
        }

        uint32 SDIndex = 0;
        _mp4ErrorCode = getSampleToChunkAtom().getSDIndexGet(SDIndex);
        if (_mp4ErrorCode != EVERYTHING_FINE)
        {
            *n = 0;
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - Read Sample Description Atom Failed"));
            return READ_FAILED;
        }


        if (SDIndex > 0)
        {
            SDIndex -= 1;
        }


        // Find chunk offset to file
        uint64 offset = 0;
        errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);


        if (errCode != EVERYTHING_FINE)
        {
            _mp4ErrorCode =  INVALID_CHUNK_OFFSET;
            PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - Invalid Chunk Offset"));
            break;
        }

        // Need to add up all the sizes from the first sample in this run up to the
        // the requested sample (but not including it)

        uint32 sampleSizeOffset = 0;
        uint32 tempSize = 0;

        uint32 sigmaSampleSize = 0;

        for (j = sampleNum;
                j < (sampleNum + numSamples);
                j++)
        {
            // Check sample size from SampleSizeAtom
            tempSize = getSampleSizeAtom().getDefaultSampleSize();

            if (tempSize == 0)
            {
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
                currticks = OsclTickCount::TickCount();
                StartTime = OsclTickCount::TicksToMsec(currticks);
#endif
                // Need to get specific sample size
                errCode = getSampleSizeAtom().getSampleSizeAt(j, tempSize);
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
                currticks = OsclTickCount::TickCount();
                EndTime = OsclTickCount::TicksToMsec(currticks);
                totalTimeSTSZ += (EndTime - StartTime);
#endif
            }

            // Check for error condition
            if (errCode != EVERYTHING_FINE)
            {
                *n = 0;
                _mp4ErrorCode =  INVALID_SAMPLE_SIZE;
                PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - Invalid Sample Size"));
                return (_mp4ErrorCode);
            }

            sigmaSampleSize += tempSize;

            pgau->info[s].len = tempSize;

            uint32 tsDelta = 0;

#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
            currticks = OsclTickCount::TickCount();
            StartTime = OsclTickCount::TicksToMsec(currticks);
#endif
            MP4_ERROR_CODE errCode = _ptimeToSampleAtom->GetTimeDeltaForSampleNumberGet(j, tsDelta);
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
            currticks = OsclTickCount::TickCount();
            EndTime = OsclTickCount::TicksToMsec(currticks);
            totalTimeSTTS += (EndTime - StartTime);
#endif

            if (errCode != EVERYTHING_FINE)
            {
                *n = 0;
                _mp4ErrorCode = READ_FAILED;
                return (_mp4ErrorCode);
            }

            pgau->info[s].sample_info = SDIndex;

            pgau->info[s].ts_delta = tsDelta + _tsDelta;
            _tsDelta = 0;

#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
            currticks = OsclTickCount::TickCount();
            StartTime = OsclTickCount::TicksToMsec(currticks);
#endif
            pgau->info[s].ctts_offset = getCttsOffsetForSampleNumberGet(j);
            pgau->info[s].ts = currTSBase + pgau->info[s].ctts_offset;

#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
            currticks = OsclTickCount::TickCount();
            EndTime = OsclTickCount::TicksToMsec(currticks);
            totalTimeCTTS += (EndTime - StartTime);
#endif

            currTSBase += tsDelta;


            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getNextNSamples- pgau->info[%d].sample_info =%d", s, pgau->info[s].sample_info));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getNextNSamples- pgau->info[%d].ts_delta =%d", s, pgau->info[s].ts_delta));
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::getNextNSamples- pgau->info[%d].ts =%d", s, pgau->info[s].ts));


            s++;
        }

        // Find actual file offset to sample
        TOsclFileOffset sampleFileOffset = (TOsclFileOffset)offset + _currChunkOffset;

        // Open file
        if (!_pinput->IsOpen())
        {
            if (iOpenFileOncePerTrack)
            {
                // Check if file is already open
                // Opens file in binary mode with read sharing permissions
                // Return Error in case the file open fails.
                if (AtomUtils::OpenMP4File(_filename,
                                           Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                           _pinput) != 0)
                {
                    *n = 0;
                    return FILE_OPEN_FAILED;
                }
            }
            else
            {
                _pinput->_fileSize = _commonFilePtr->_fileSize;
                _pinput->_pvfile.Copy(_commonFilePtr->_pvfile);
                AtomUtils::Flush(_pinput);
                AtomUtils::seekFromStart(_pinput, 0);
            }
            if (!_IsUpdateFileSize)
            {
                if (AtomUtils::getCurrentFileSize(_pinput, _fileSize) == false)
                {
                    *n = 0;
                    _mp4ErrorCode = READ_FAILED;
                    return _mp4ErrorCode;
                }
            }
        }

        TOsclFileOffset bufStart = 0;
        TOsclFileOffset bufEnd = _fileSize;
        uint32 bufCap = AtomUtils::getFileBufferingCapacity(_pinput);
        if (bufCap)
        {
            // progressive playback
            // MBDS contains only a range of bytes
            // first byte is not always 0
            AtomUtils::getCurrentByteRange(_pinput, bufStart, bufEnd);

            // bufEnd is the offset of the last byte
            // need to turn it into a length, like _fileSize above
            bufEnd++;
        }

        uint32 start = 0;
        uint32 rewindPos = 0;
        uint32 totalFragmentLength = 0;
        for (k = start; k < end; k++)
        {
            totalFragmentLength += tempgauPtr->buf.fragments[k].len;
        }

        if ((sampleFileOffset < bufStart) || ((TOsclFileOffset)(sigmaSampleSize + sampleFileOffset) > bufEnd))
            _mp4ErrorCode = INSUFFICIENT_DATA;
        else if (totalFragmentLength < sigmaSampleSize)
            _mp4ErrorCode = INSUFFICIENT_BUFFER_SIZE;

        if (INSUFFICIENT_DATA == _mp4ErrorCode || INSUFFICIENT_BUFFER_SIZE == _mp4ErrorCode)
        {
            if ((uint32)_currentPlaybackSampleNumber != startSampleNum)
            {
                _currentPlaybackSampleNumber = startSampleNum;
                _currentPlaybackSampleTimestamp = startSampleNumTSBase;
                if (DEFAULT_ERROR == _ptimeToSampleAtom->ResetStateVariables(_currentPlaybackSampleNumber))
                {
                    _currentPlaybackSampleNumber = 0;
                    _currentPlaybackSampleTimestamp = 0;
                    _ptimeToSampleAtom->ResetStateVariables();
                    return DEFAULT_ERROR;
                }
                if (DEFAULT_ERROR == _psampleToChunkAtom->resetStateVariables(_currentPlaybackSampleNumber))
                {
                    _currentPlaybackSampleNumber = 0;
                    _currentPlaybackSampleTimestamp = 0;
                    _psampleToChunkAtom->resetStateVariables();
                    return DEFAULT_ERROR;
                }
                if (0 != _pcompositionOffsetAtom)
                {
                    int32 retValC = _pcompositionOffsetAtom->ResetStateVariables(_currentPlaybackSampleNumber);
                    if (retValC == DEFAULT_ERROR)
                    {
                        _currentPlaybackSampleNumber = 0;
                        _currentPlaybackSampleTimestamp = 0;
                        _pcompositionOffsetAtom->ResetStateVariables();
                        return DEFAULT_ERROR;
                    }
                }
                chunk = 0;
                MP4_ERROR_CODE errCode = getMutableSampleToChunkAtom().
                                         getChunkNumberForSampleGet(_currentPlaybackSampleNumber, chunk);
                if (errCode != EVERYTHING_FINE)
                {
                    *n = 0;
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - getChunkNumberForSampleGet()"));
                    return READ_FAILED;
                }


                //This method needs to be added to sample to chunk atom
                numChunksInRun = 0;
                errCode = getSampleToChunkAtom().getNumChunksInRunofChunksGet(numChunksInRun);
                if (errCode != EVERYTHING_FINE)
                {
                    *n = 0;
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - getNumChunksInRunofChunksGet()"));
                    return READ_FAILED;
                }

                FirstSampleNumInChunk = 0;
                errCode = getSampleToChunkAtom().getFirstSampleNumInChunkGet(FirstSampleNumInChunk);
                if (errCode != EVERYTHING_FINE)
                {
                    *n = 0;
                    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>SampleTableAtom::getNextNSamples - getFirstSampleNumInChunkGet()"));
                    return READ_FAILED;
                }

                offsetIntoRunOfChunks =
                    (_currentPlaybackSampleNumber - FirstSampleNumInChunk);

                sampleSizeOffset = 0;
                for (i = FirstSampleNumInChunk; i < FirstSampleNumInChunk +
                        offsetIntoRunOfChunks; i++)
                {
                    _psampleSizeAtom->getSampleSizeAt(i, tempSize);
                    sampleSizeOffset += tempSize;
                }
                _currChunkOffset = sampleSizeOffset;
            }

            if (bufCap)
            {
                // if MBDS, may need to kick of a http request
                TOsclFileOffset contentLength = AtomUtils::getContentLength(_pinput);
                if ((0 != contentLength) && (((TOsclFileOffset)sigmaSampleSize + sampleFileOffset) > contentLength))
                {
                    // do not skip beyond end of clip
                    // if content length is known
                    _mp4ErrorCode = READ_FAILED;
                }
                else
                {
                    AtomUtils::skipFromStart(_pinput, sampleFileOffset);
                }
            }

            *n = 0;
            for (uint32 i = 0; i < pgau->numMediaSamples; i++)
            {
                pgau->info[i].ts          = 0;
                pgau->info[i].sample_info = 0;
            }
            return _mp4ErrorCode;
        }

        if (oStoreOffset)
            _currChunkOffset += sigmaSampleSize;
        else
            _currChunkOffset  = 0;
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
        currticks = OsclTickCount::TickCount();
        StartTime = OsclTickCount::TicksToMsec(currticks);
#endif
        if (iOpenFileOncePerTrack)
        {
            if (_oPVContentDownloadable)
            {
                if (_currentPlaybackSampleNumber == 0)
                {
                    AtomUtils::seekFromStart(_pinput, sampleFileOffset);
                }
            }
            else
            {
                AtomUtils::seekFromStart(_pinput, sampleFileOffset);
            }
        }
        else
            AtomUtils::seekFromStart(_pinput, sampleFileOffset);

#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
        currticks = OsclTickCount::TickCount();
        EndTime = OsclTickCount::TicksToMsec(currticks);
        totalTimeFileSeek += (EndTime - StartTime);

        currticks = OsclTickCount::TickCount();
        StartTime = OsclTickCount::TicksToMsec(currticks);
#endif
        start = 0;
        rewindPos = 0;
        for (k = start; k < end; k++)
        {
            uint32 tmpSize =
                (tempgauPtr->buf.fragments[k].len > sigmaSampleSize) ? sigmaSampleSize : tempgauPtr->buf.fragments[k].len;
            if (tmpSize)
            {
                if (!AtomUtils::readByteData(_pinput, tmpSize,
                                             (uint8 *)(tempgauPtr->buf.fragments[k].ptr)))
                {
                    *n = 0;
                    _mp4ErrorCode =  READ_FAILED;
                    return (_mp4ErrorCode);
                }
                tempgauPtr->buf.fragments[k].len -= tmpSize;

                uint8* fragment_ptr = NULL;
                fragment_ptr = (uint8 *)(tempgauPtr->buf.fragments[k].ptr);
                fragment_ptr += tmpSize;
                tempgauPtr->buf.fragments[k].ptr = fragment_ptr;

                sigmaSampleSize -= tmpSize;
                totalBytesRead += tmpSize;

            }
            rewindPos += tmpSize;

            if (sigmaSampleSize == 0)
            {
                break;
            }
        }
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
        currticks = OsclTickCount::TickCount();
        EndTime = OsclTickCount::TicksToMsec(currticks);
        totalTimeFileRead += (EndTime - StartTime);
#endif

        sampleNum = sampleNum + numSamples;

        /*
         * This check is required to protect the FF lib from context
         * switching.
         */
        if (_currentPlaybackSampleNumber == sampleBeforeGet)
        {
            _currentPlaybackSampleNumber += numSamples;
            sampleBeforeGet += numSamples;
        }
        else
        {
            break;
        }

        if (_currentPlaybackSampleNumber >= totalnumSamples)
        {
            _mp4ErrorCode = END_OF_TRACK;
            break;
        }
    }

    if (_currentPlaybackSampleNumber == sampleBeforeGet)
    {
        _currentPlaybackSampleTimestamp = currTSBase;
        *n = (*n - samplesYetToBeRead);
#if (PVLOGGER_INST_LEVEL > PVLOGMSG_INST_LLDBG)
        if (totalTimeSTSC > PV_MP4_PARSER_SAMPLE_READ_DIAGLOG_THRESHOLD_IN_MS)
        {
            PVMF_MP4FFPARSER_LOGDIAGNOSTICS((0, "PVMFMP4FFParserNode - SampleTableAtom::totalTimeSTSZ - NPT=%d, N=%d, Time=%d",
                                             currTSBase,
                                             *n,
                                             totalTimeSTSC));
        }
        if (totalTimeSTSZ > PV_MP4_PARSER_SAMPLE_READ_DIAGLOG_THRESHOLD_IN_MS)
        {
            PVMF_MP4FFPARSER_LOGDIAGNOSTICS((0, "SampleTableAtom::totalTimeSTSZ - NPT=%d, N=%d, Time=%d",
                                             currTSBase,
                                             *n,
                                             totalTimeSTSZ));
        }
        if (totalTimeSTTS > PV_MP4_PARSER_SAMPLE_READ_DIAGLOG_THRESHOLD_IN_MS)
        {
            PVMF_MP4FFPARSER_LOGDIAGNOSTICS((0, "SampleTableAtom::totalTimeSTTS - NPT=%d, N=%d, Time=%d",
                                             currTSBase,
                                             *n,
                                             totalTimeSTTS));
        }
        if (totalTimeCTTS > PV_MP4_PARSER_SAMPLE_READ_DIAGLOG_THRESHOLD_IN_MS)
        {
            PVMF_MP4FFPARSER_LOGDIAGNOSTICS((0, "SampleTableAtom::totalTimeCTTS - NPT=%d, N=%d, Time=%d",
                                             currTSBase,
                                             *n,
                                             totalTimeCTTS));
        }
        if (totalTimeFileSeek > PV_MP4_PARSER_SAMPLE_READ_DIAGLOG_THRESHOLD_IN_MS)
        {
            PVMF_MP4FFPARSER_LOGDIAGNOSTICS((0, "SampleTableAtom::totalTimeFileSeek - NPT=%d, N=%d, Time=%d",
                                             currTSBase,
                                             *n,
                                             totalTimeFileSeek));
        }
        if (totalTimeFileRead > PV_MP4_PARSER_SAMPLE_READ_DIAGLOG_THRESHOLD_IN_MS)
        {
            PVMF_MP4FFPARSER_LOGDIAGNOSTICS((0, "SampleTableAtom::totalTimeFileRead - NPT=%d, N=%d, BytesRead=%d, Time=%d",
                                             currTSBase,
                                             *n,
                                             totalBytesRead,
                                             totalTimeFileRead));
        }
#endif
    }
    else
    {
        if (_currentPlaybackSampleNumber > 0)
        {
            getTimestampForSampleNumber((_currentPlaybackSampleNumber - 1), _currentPlaybackSampleTimestamp);
        }
        else
        {
            _currentPlaybackSampleTimestamp = getCttsOffsetForSampleNumber(0);
        }

        *n = 0;
        if (_mp4ErrorCode != INSUFFICIENT_BUFFER_SIZE)
        {
            _mp4ErrorCode = READ_FAILED;
        }
    }
    return (_mp4ErrorCode);
}

MP4_ERROR_CODE
SampleTableAtom::peekNextNSamples(uint32 startSampleNum,
                                  uint32 *n,
                                  MediaMetaInfo    *mInfo)
{//this API is only called in peekNextBundledAccessUnits(), which already check _psampleSizeAtom is NULL


    uint64 currTSBase = _currentPlaybackSampleTimestamp;
    int32 i = 0;
    uint32 sampleNum = startSampleNum;
    int32 samplesToBePeeked = *n;

    MP4_ERROR_CODE  _mp4ErrorCode = EVERYTHING_FINE;

    if (_psampleToChunkAtom->getCurrPeekSampleCount() !=
            _currentPlaybackSampleNumber)
    {
        _psampleToChunkAtom->resetPeekwithGet();
    }

    if (_ptimeToSampleAtom->GetCurrPeekSampleCount() !=
            _currentPlaybackSampleNumber)
    {
        _ptimeToSampleAtom->ResetPeekwithGet();
    }



    // return NULL if sampleNum is past end of stream
    uint32 numSamples = _psampleSizeAtom->getSampleCount();

    if (startSampleNum + samplesToBePeeked >= numSamples)
    {
        _mp4ErrorCode = END_OF_TRACK;
        if (startSampleNum >= numSamples)
        {
            *n = 0;
            return (_mp4ErrorCode); // Past end of samples
        }
        samplesToBePeeked = numSamples - startSampleNum;
        *n = samplesToBePeeked;
    }

    if ((_IsUpdateFileSize)
            && (_psampleDescriptionAtom->getMediaType() != MEDIA_TYPE_TEXT))
    {
        //1. check if the data is available for PS
        //!!!!!!!!!!!!!!!!ASSUME the sample arranged in time order in the file!!!!!!!!
        // Find chunk
        uint32 lastSampleNum = startSampleNum + samplesToBePeeked - 1;

        uint32 chunk = 0;
        _mp4ErrorCode = _psampleToChunkAtom->getChunkNumberForSamplePeek(sampleNum, chunk);

        if (_mp4ErrorCode != EVERYTHING_FINE)
        {
            *n = 0;
            return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
        }
        // Find first sample in that chunk
        uint32 first = 0;
        _mp4ErrorCode = _psampleToChunkAtom->getFirstSampleNumInChunkPeek(first);
        if (_mp4ErrorCode != EVERYTHING_FINE)
        {
            *n = 0;
            return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
        }

        // Find chunk offset to file
        uint64 sampleSizeOffset = 0;
        _mp4ErrorCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, sampleSizeOffset);
        if (_mp4ErrorCode != EVERYTHING_FINE)
        {
            *n = 0;
            return READ_FAILED;
        }

        uint32 tempSize = 0;
        for (uint32 i = first; i < lastSampleNum; i++)
        {
            // Check sample size from SampleSizeAtom
            MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(i, tempSize);

            // Check for error condition
            if (errCode != EVERYTHING_FINE)
            {
                *n = 0;
                return READ_SAMPLE_SIZE_ATOM_FAILED;
            }

            sampleSizeOffset += tempSize;
        }
        if ((TOsclFileOffset)sampleSizeOffset > _fileSize)
        {
            *n = 0;
            return READ_FAILED;
        }
    }

    for (i = 0; i < samplesToBePeeked; i++)
    {
        // Find chunk
        uint32 chunk = 0;
        _mp4ErrorCode = _psampleToChunkAtom->getChunkNumberForSamplePeek(sampleNum, chunk);

        if (_mp4ErrorCode == READ_SAMPLE_TO_CHUNK_ATOM_FAILED)
        {
            *n = (*n - samplesToBePeeked);
            return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
        }

        uint32 SDIndex = 0;
        _mp4ErrorCode = _psampleToChunkAtom->getSDIndexPeek(SDIndex);

        if (_mp4ErrorCode != EVERYTHING_FINE)
        {
            *n = (*n - samplesToBePeeked);
            return READ_SAMPLE_TO_CHUNK_ATOM_FAILED;
        }

        if (SDIndex > 0)
        {
            SDIndex -= 1;
        }
        uint32  tempSize = 0;
        MP4_ERROR_CODE errCode = _psampleSizeAtom->getSampleSizeAt(sampleNum, tempSize);

        // Check for error condition
        if (errCode != EVERYTHING_FINE)
        {
            *n = (*n - samplesToBePeeked);
            return INVALID_SAMPLE_SIZE;
        }

        uint32 tsDelta = 0;
        errCode = _ptimeToSampleAtom->GetTimeDeltaForSampleNumberPeek(sampleNum, tsDelta);

        if (errCode != EVERYTHING_FINE)
        {
            *n = 0;
            return READ_TIME_TO_SAMPLE_ATOM_FAILED;
        }


        mInfo[i].sample_info = SDIndex;

        //SET THE META INFO HERE
        mInfo[i].len = tempSize;
        mInfo[i].ts_delta = tsDelta + _tsDelta;
        mInfo[i].ctts_offset = getCttsOffsetForSampleNumberPeek(sampleNum);
        mInfo[i].ts = currTSBase + mInfo[i].ctts_offset;
        currTSBase += tsDelta;

        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::peekNextNSamples- mInfo[%d].len =%d", i, mInfo[i].len));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::peekNextNSamples- mInfo[%d].ts_delta =%d", i, mInfo[i].ts_delta));
        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "SampleTableAtom::peekNextNSamples- mInfo[%d].ts =%d", i, mInfo[i].ts));

        sampleNum ++;
    }
    return (_mp4ErrorCode);
}

MP4_ERROR_CODE SampleTableAtom::getOffsetByTime(uint64 ts, TOsclFileOffset* sampleFileOffset)
{

    if ((_psampleSizeAtom    == NULL) ||
            (_psampleToChunkAtom == NULL) ||
            (_ptimeToSampleAtom == NULL) ||
            (_pchunkOffsetAtom   == NULL))
    {
        return DEFAULT_ERROR;
    }
    uint32 sampleNum = 0;
    if (_ptimeToSampleAtom->GetSampleNumberFromTimestamp(ts, sampleNum, true) != EVERYTHING_FINE)
    {
        return DEFAULT_ERROR;
    }

    // Go for composition offset adjustment.
    sampleNum =
        getSampleNumberAdjustedWithCTTS(ts, sampleNum);

    // return NULL if sampleNum is past end of stream
    uint32 numSamples = _psampleSizeAtom->getSampleCount();

    if (numSamples == 0)
    {
        return DEFAULT_ERROR;
    }

    if (sampleNum >= numSamples)
    {
        sampleNum = numSamples - 1;
    }

    uint32 sampleSizeOffset = 0;
    uint64 tsOfSample = 0;
    getTimestampForSampleNumber(sampleNum, tsOfSample);
    if (ts != tsOfSample)
    {
        if ((_psampleSizeAtom->getSampleSizeAt(sampleNum, sampleSizeOffset) != EVERYTHING_FINE) || (sampleSizeOffset == 0))
        {
            return DEFAULT_ERROR;
        }
    }

    // Find chunk
    uint32 chunk = 0;
    MP4_ERROR_CODE errCode = getMutableSampleToChunkAtom().getChunkNumberForSample(sampleNum, chunk);
    if (errCode != EVERYTHING_FINE)
    {
        return errCode;
    }

    // Find first sample in that chunk
    uint32 first = 0;
    errCode = _psampleToChunkAtom->getFirstSampleNumInChunk(chunk, first);
    if (errCode != EVERYTHING_FINE)
    {
        return (READ_FAILED);
    }


    // Find chunk offset to file
    uint64 offset = 0;
    errCode = _pchunkOffsetAtom->getChunkOffsetAt(chunk, offset);
    if (errCode != EVERYTHING_FINE)
    {
        return errCode;
    }

    // Need to add up all the sizes from the first sample in this run up to the
    // the requested sample (but not including it)
    uint32 tempSize = 0;


    for (uint32 i = first; i < sampleNum; i++)
    {
        // Check sample size from SampleSizeAtom
        if ((_psampleSizeAtom->getSampleSizeAt(i, tempSize) != EVERYTHING_FINE) || (tempSize == 0))
        {
            return DEFAULT_ERROR;
        }

        sampleSizeOffset += tempSize;
    }

    // Find actual file offset to sample
    *sampleFileOffset = (TOsclFileOffset)offset + sampleSizeOffset;
    getSampleSizeAtom()._SkipOldEntry = true;
    if (sampleNum == (numSamples - 1))
    {
        return LAST_SAMPLE_IN_MOOV;
    }
    else
    {
        return EVERYTHING_FINE;
    }
}

MP4_ERROR_CODE SampleTableAtom::updateFileSize(TOsclFileOffset filesize)
{
    _fileSize = filesize;
    _IsUpdateFileSize = 1;
    if (_pinput->IsOpen())
    {
        if (AtomUtils::Flush(_pinput))
        {
            return DEFAULT_ERROR;   //error
        }
    }
    return EVERYTHING_FINE;
}

MP4_ERROR_CODE
SampleTableAtom::getMaxTrackTimeStamp(TOsclFileOffset fileSize, uint64& timeStamp)
{
    timeStamp = 0;

    /*
     * Get the chunk that is closest to "fileSize"
     */
    int32 chunk;

    MP4_ERROR_CODE retVal =
        _pchunkOffsetAtom->getChunkClosestToOffset(fileSize, chunk);

    if (retVal == EVERYTHING_FINE)
    {
        /*
         * Get chunk offset for the chunk
         */
        uint64 chunkOffset = 0;
        _pchunkOffsetAtom->getChunkOffsetAt(chunk, chunkOffset);

        /*
         * Find the first sample in chunk
         */
        uint32 FirstSampleNumInChunk = 0;
        MP4_ERROR_CODE errCode = _psampleToChunkAtom->getFirstSampleNumInChunk(chunk, FirstSampleNumInChunk);
        if (errCode != EVERYTHING_FINE)
        {
            return (READ_FAILED);
        }

        /*
         * Get number of samples in this chunk
         */
        uint32 numSamplesPerChunk = 0;
        errCode =
            getSampleToChunkAtom().getSamplesPerChunkCorrespondingToSampleGet(numSamplesPerChunk);
        if (errCode != EVERYTHING_FINE)
        {
            return READ_FAILED;
        }

        /*
         * Get the sample closest to the specified fileSize.
         * Need to add up all the sizes from the first sample in this run
         * up to the the closest sample
         */
        uint32 tempSize = 0;
        uint32 sampleNum = FirstSampleNumInChunk ? (FirstSampleNumInChunk - 1) : 0;

        do
        {
            // Check sample size from SampleSizeAtom
            tempSize = getSampleSizeAtom().getDefaultSampleSize();

            if (tempSize == 0)
            {
                // Need to get specific sample size
                MP4_ERROR_CODE errCode = getSampleSizeAtom().getSampleSizeAt(sampleNum, tempSize);
                if (errCode != EVERYTHING_FINE)
                {
                    sampleNum++;
                    return READ_FAILED;
                }
            }
            sampleNum++;
            chunkOffset += tempSize;

            if ((TOsclFileOffset)chunkOffset > fileSize)
            {
                sampleNum--;
                break;
            }
        }
        while (sampleNum < (FirstSampleNumInChunk + numSamplesPerChunk - 1));

        /*
         * Get time stamp corresponding to sampleNum
         */
        uint64 ts = 0;
        errCode = getTimestampForSampleNumber(sampleNum, ts);
        if (errCode != EVERYTHING_FINE)
        {
            return errCode;
        }
        timeStamp = ts + getCttsOffsetForSampleNumber(sampleNum);
    }
    return (EVERYTHING_FINE);
}

MP4_ERROR_CODE
SampleTableAtom::getSampleNumberClosestToTimeStamp(uint32 &sampleNumber,
        uint64 timeStamp,
        TOsclFileOffset sampleOffset)
{
    sampleNumber = 0;

    if ((_psampleSizeAtom    == NULL) ||
            (_psampleToChunkAtom == NULL) ||
            (_ptimeToSampleAtom  == NULL) ||
            (_pchunkOffsetAtom   == NULL))
    {
        return (READ_FAILED);
    }

    uint32 totalNumSamplesInTrack = getSampleSizeAtom().getSampleCount();

    if (totalNumSamplesInTrack > 0)
    {
        if (timeStamp > _trackStartTSOffset)
        {
            getTimeToSampleAtom().GetSampleNumberFromTimestamp((timeStamp - _trackStartTSOffset), sampleNumber, true);
            // Go for composition offset adjustment.
            sampleNumber =
                getSampleNumberAdjustedWithCTTS((timeStamp - _trackStartTSOffset), sampleNumber);

            if ((sampleNumber + sampleOffset) >= totalNumSamplesInTrack)
            {
                sampleNumber = (totalNumSamplesInTrack - 1);
                return (END_OF_TRACK);
            }
        }
        else
        {
            sampleNumber = 0;
        }
        sampleNumber += (uint32)sampleOffset;
        return (EVERYTHING_FINE);
    }
    else
    {
        return (READ_FAILED);
    }
}


// This function takes the presentation time stamp and the sample number calculated
// referring stts api getSampleNumberFromTimestamp.
// We look for all the samples which has (decode time  + ctts >= presentation timestamp)
// before this sample and return the most nearest to presentation timestamp.
int32 SampleTableAtom::getSampleNumberAdjustedWithCTTS(uint64 aTs, int32 aSampleNumber)
{
    uint64 minimumTimestamp   = 0; //stores minimum ts encountered
    int32  minimumTsFrameNum  = 0; //stores frame num of minimum ts
    int32  curSampleParsed   = 0; //stores number of sample currently parsed
    uint64 curSampleTs       = 0; //stores timestamp of current sample

    // CTTS not present do nothing.
    if (NULL == _pcompositionOffsetAtom)
    {
        return aSampleNumber;
    }

    //initially we beleive that sample number that we have received is
    //minimum sample number available and ts corresponding to this sample number
    //is the minimum sample time(including its ctts offset as well)

    minimumTsFrameNum = aSampleNumber;
    // getTimestampForSampleNumber has CTTS already adjusted
    getTimestampForSampleNumber(minimumTsFrameNum, minimumTimestamp);

    //assign curSampleParsed the value of current minimumTsFrameNum
    curSampleParsed = minimumTsFrameNum;

    for (int loopIndex = 0; loopIndex < BACK_TRAVERSE_FRAME_COUNT; loopIndex++)
    {
        if (0 == curSampleParsed)
        {
            // For loop termination
            return minimumTsFrameNum;
        }
        else
        {
            //get the timestamp of the current sample
            getTimestampForSampleNumber(curSampleParsed, curSampleTs);

            //if curSampleTs = aTs then this is the sample we needed so return
            if (curSampleTs == aTs)
            {
                return curSampleParsed;
            }

            //if curSampleTs > aTs and is less than minimumTimestamp
            //then this may be the minimum sample that we need to start with
            if ((curSampleTs > aTs) && (curSampleTs < minimumTimestamp))
            {
                minimumTsFrameNum = curSampleParsed;
                minimumTimestamp = curSampleTs;
            }

            //decrement the curSampleParsed to move back to the previous sample
            curSampleParsed--;
        }
    }
    return minimumTsFrameNum;
}

// Returns the timestamp (in milliseconds) for the last sample returned
// This is mainly to be used when seeking in the bitstream - you request a frame at timestamp
// X, but the actual frame you get is Y, this method returns the timestamp for Y so you know which
// audio sample to request.
uint64 SampleTableAtom::getTimestampForCurrentSample()
{
    return _currentPlaybackSampleTimestamp;
}

// adjusting playabck according to entries stored in 'elst' atom
void SampleTableAtom::handleEditList(uint64 &ts)
{
    if ((_pEditListTimeVec == NULL) || (_pEditListDurationVec == NULL))
        return;

    if ((ts < _prevEditDuration) || (_editIndex >= _pEditListTimeVec->size()))
        return;

    if ((*_pEditListTimeVec)[_editIndex] == -1)
    {
        MediaClockConverter mcc(_movieTimeScale, (*_pEditListDurationVec)[_editIndex]);
        uint64 timeStamp = mcc.get_converted_ts64(_trackMediaTimeScale);
        ts = timeStamp;
        _prevEditDuration += timeStamp;
        _tsDelta = (uint32)timeStamp;
        _editIndex++;
    }
    else
    {
        if ((ts >= _prevEditDuration) && (_editIndex < (_pEditListTimeVec->size() - 1)))
        {
            ts = resetTrackByTime(_editOffset + (*_pEditListTimeVec)[_editIndex], false);
            _editOffset = 0;
            MediaClockConverter mcc(_movieTimeScale, (*_pEditListDurationVec)[_editIndex]);
            _prevEditDuration += mcc.get_converted_ts64(_trackMediaTimeScale);
            _editIndex++;
        }
    }
}

// adjusting playabck during seek according to entries stored in 'elst' atom
void SampleTableAtom::getEditListEntryForTimeStamp(uint64 &ts)
{
    if ((_pEditListTimeVec == NULL) || (_pEditListDurationVec == NULL))
        return;

    _prevEditDuration = 0, _editIndex = 0, _editOffset = 0, _tsDelta = 0;
    for (uint32 i = 0; i < _pEditListTimeVec->size(); i++)
    {
        MediaClockConverter mcc(_movieTimeScale, (*_pEditListDurationVec)[i]);
        uint64 duration = mcc.get_converted_ts64(_trackMediaTimeScale);
        int32 editStartTime = (*_pEditListTimeVec)[i];
        if ((int64)ts >= editStartTime && ts <= (editStartTime + duration + _prevEditDuration))
        {
            _editOffset = (ts - editStartTime);
            _editIndex = i;
            break;
        }
        else if (editStartTime == -1)
        {
            if ((ts >= _prevEditDuration && ts <= (_prevEditDuration + duration)) ||
                    (ts <= duration))
            {
                _editIndex = i;
                break;
            }
            else
            {
                _editIndex = i;
            }
        }
        _prevEditDuration += duration;
        _tsDelta += (uint32)_prevEditDuration;
    }
}

void SampleTableAtom::setEditListInfo(Oscl_Vector<int32, OsclMemAllocator> *aEditListTime,
                                      Oscl_Vector<uint32, OsclMemAllocator> *aEditListDuration,
                                      uint32 aMovieTimeScale, uint32 aTrackMediaTS)
{
    _pEditListTimeVec = aEditListTime;
    _pEditListDurationVec = aEditListDuration;
    _movieTimeScale = aMovieTimeScale;
    _trackMediaTimeScale = aTrackMediaTS;
}

