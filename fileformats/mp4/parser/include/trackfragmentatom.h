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
/*                            MPEG-4 Track Fragment Atom Class                             */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
*/


#ifndef TRACKFRAGMENTATOM_H_INCLUDED
#define TRACKFRAGMENTATOM_H_INCLUDED

#ifndef ATOM_H_INCLUDED
#include "atom.h"
#endif

#ifndef TRACKFRAGMENTHEADERATOM_H_INCLUDED
#include "trackfragmentheaderatom.h"
#endif

#ifndef TRACKFRAGMENTRUNATOM_H_INCLUDED
#include "trackfragmentrunatom.h"
#endif

#ifndef PV_GAU_H_INCLUDED
#include "pv_gau.h"
#endif

#ifndef TRACKEXTENDSATOM_H_INCLUDED
#include "trackextendsatom.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef _PIFFBOXES_H_
#include "piffboxes.h"
#endif

class TrackDurationInfo : public HeapBase
{
    public:
        TrackDurationInfo(uint32 td, uint32 id)
        {
            trackDuration = td;
            trackId = id;
        }

        TrackDurationInfo(uint64 td, uint32 id)
        {
            trackDuration = td;
            trackId = id;
        }

        ~TrackDurationInfo() {}
        uint64 trackDuration;
        uint32 trackId;
    private:
};

class TrackDurationContainer : public HeapBase
{

    public:
        TrackDurationContainer()
        {
            _pTrackdurationInfoVec = NULL;
        }
        ~TrackDurationContainer() {};

        int32 getNumTrackInfoVec()
        {
            if (_pTrackdurationInfoVec != NULL)
                return _pTrackdurationInfoVec->size();
            return 0;
        }
        TrackDurationInfo *getTrackdurationInfoAt(int32 index)
        {
            if (index >= 0 && index < (int32)_pTrackdurationInfoVec->size())
                return (*_pTrackdurationInfoVec)[index];
            else
                return NULL;
        }

        void updateTrackDurationForTrackId(int32 id, uint64 duration);

        Oscl_Vector<TrackDurationInfo*, OsclMemAllocator> *_pTrackdurationInfoVec;

    private:

};

class TrackFragmentAtom : public Atom
{

    public:

        TrackFragmentAtom(MP4_FF_FILE *fp,
                          MP4_FF_FILE *Moof_fp,
                          uint32 &size,
                          uint32 type,
                          TOsclFileOffset movieFragmentCurrentOffset,
                          TOsclFileOffset movieFragmentBaseOffset,
                          uint32 moof_size,
                          TrackDurationContainer *trackDurationContainer,
                          Oscl_Vector<TrackExtendsAtom*, OsclMemAllocator> *trackExtendAtomVec,
                          bool &parseTrafCompletely,
                          bool &trafParsingCompleted,
                          uint32 &countOfTrunsParsed,
                          const TrackEncryptionBoxContainer* apTrackEncryptionBoxCntr);

        virtual ~TrackFragmentAtom();

        void ParseTrafAtom(MP4_FF_FILE *fp,
                           uint32 &size,
                           uint32 type,
                           TOsclFileOffset movieFragmentCurrentOffset,
                           TOsclFileOffset movieFragmentBaseOffset,
                           uint32 moofSize,
                           TrackDurationContainer *trackDurationContainer,
                           Oscl_Vector<TrackExtendsAtom*, OsclMemAllocator> *trackExtendAtomVec,
                           bool &trafParsingCompleted,
                           uint32 &countOfTrunsParsed);

        uint32 getTrackId()
        {
            if (_pTrackFragmentHeaderAtom != NULL)
            {
                return _pTrackFragmentHeaderAtom->getTrackId();
            }
            return 0;
        }
        uint32 getSampleCount() const;
        Oscl_Vector<TFrunSampleTable*, OsclMemAllocator>* getSampleTable() const;
        uint64 getBaseDataOffset() const;
        uint32 getSampleDescriptionIndex() const;
        uint32 getDefaultSampleDuration() const;
        uint32 getDefaultSampleSize() const;
        uint32 getDefaultSampleFlags() const;
        TrackFragmentRunAtom *getTrackFragmentRunForSampleNum(uint32 samplenum, uint32 &samplecount) const;
        int32 getNextNSamples(uint32 startSampleNum, uint32 *n, uint32 totalSampleRead, GAU    *pgau, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>*  apSampleDecryptionInfoVect);
        int32 getNextBundledAccessUnits(uint32 *n, uint32 totalSampleRead, GAU *pgau, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>*  apSampleDecryptionInfoVect);
        int32 peekNextNSamples(uint32 startSampleNum, uint32 *n, uint32 totalSampleRead, MediaMetaInfo *mInfo);
        int32 peekNextBundledAccessUnits(uint32 *n, uint32 totalSampleRead, MediaMetaInfo *mInfo);
        uint32 getTotalNumSampleInTraf() const;
        uint32 getSampleNumberFromTimestamp(uint64 time) const;
        MP4_ERROR_CODE getTimestampForSampleNumber(uint32 sampleNumber, uint64& aTimeStamp) const;
        uint64 getCurrentTrafDuration();
        int32 getOffsetByTime(uint32 id, uint64 ts, TOsclFileOffset* sampleFileOffset);
        void resetPeekwithGet();
        void resetPlayback();
        uint64 resetPlayback(uint64 time);
        uint64 resetPlayback(uint64 time, uint32 trun_number, uint32 sample_num);
        uint32 _trackFragmentEndOffset;
        uint32 trackId;

    private:
        SampleEncryptionBox* ipSampleEncryptionBox;
        TrackFragmentHeaderAtom * _pTrackFragmentHeaderAtom;
        TrackFragmentRunAtom *_pTrackFragmentRunAtom;
        Oscl_Vector<TrackFragmentRunAtom*, OsclMemAllocator> *_pTrackFragmentRunAtomVec;
        uint64 _currentPlaybackSampleTimestamp;
        uint32 _currentTrackFragmentRunSampleNumber;
        uint32 _peekPlaybackSampleNumber;
        MP4_FF_FILE *_pinput;
        MP4_FF_FILE *_commonFilePtr;
        uint64 _startTrackFragmentTSOffset;
        TOsclFileOffset _fileSize;
        uint32 _movieFragmentOffset;
        uint32 _prevSampleOffset;
        PVLogger *iLogger, *iStateVarLogger, *iParsedDataLogger;
        uint64 _trackEndDuration;
        Oscl_Vector<uint32, OsclMemAllocator> *_pFragmentptrOffsetVec;
        uint32 _cnt;
        uint32 _default_duration;
        bool _use_default_duratoin;
        TrackDurationContainer *_pTrackDurationContainer;

        uint32 tf_flags;
        TOsclFileOffset trun_offset;
        bool trunParsingCompleted;
        const TrackEncryptionBoxContainer* ipTrackEncryptionBoxCntr;
};

#endif


