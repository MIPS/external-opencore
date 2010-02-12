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
/*                            MPEG-4 Track Fragment Random Access Atom Class                             */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
*/

#ifndef TRACKFRAGMENTRANDOMACCESSATOM_H_INCLUDED
#define TRACKFRAGMENTRANDOMACCESSATOM_H_INCLUDED

#ifndef FULLATOM_H_INCLUDED
#include "fullatom.h"
#endif

#ifndef ATOMUTILS_H_INCLUDED
#include "atomutils.h"
#endif

#ifndef OSCL_INT64_UTILS_H_INCLUDED
#include "oscl_int64_utils.h"
#endif

class TFRAEntries
{
    public:
        TFRAEntries(MP4_FF_FILE *fp , uint32 version,
                    uint8 length_size_of_traf_num,
                    uint8 length_size_of_trun_num,
                    uint8 length_size_of_sample_num);

        ~TFRAEntries() {};

        uint64 getTimeStamp() const
        {
            if (_version == 1)
            {
                return _time64;
            }
            else
                return _time32;
        }

        uint32 getTimeMoofOffset() const
        {
            if (_version == 1)
            {
                return Oscl_Int64_Utils::get_uint64_lower32(_moof_offset64);
            }
            else
                return _moof_offset32;
        }

        uint32 GetTrafNumber() const
        {
            return _traf_number;
        }

        uint32 GetTrunNumber() const
        {
            return _trun_number;
        }

        uint32 GetSampleNumber() const
        {
            return _sample_number;
        }

        // Add APIs to update _time64 and _moof_offset64.
        // They're required for out-of-band updates for
        // streaming mp4 files with movie fragments
        void SetTime64(uint64 aTime)
        {
            _time64 = aTime;
        }

        void SetMoofOffset64(uint64 aMoofOffset)
        {
            _moof_offset64 = aMoofOffset;
        }


    private:
        uint32 _version;
        uint64 _time64;             //Valid when version ==1
        uint64 _moof_offset64;      //Valid when version ==1
        uint32 _time32;             //Valid when version ==0
        uint32 _moof_offset32;      //Valid when version ==0
        uint32 _traf_number;
        uint32 _trun_number;
        uint32 _sample_number;

};

class TrackFragmentRandomAccessAtom : public FullAtom
{

    public:
        TrackFragmentRandomAccessAtom(MP4_FF_FILE *fp,
                                      uint32 size,
                                      uint32 type);

        virtual ~TrackFragmentRandomAccessAtom();

        uint32 getTrackID()
        {
            return _trackId;
        }
        uint32 _entry_count;
        Oscl_Vector<TFRAEntries*, OsclMemAllocator>* getTrackFragmentRandomAccessEntries()
        {
            if (_pTFRAEntriesVec != NULL)
                return _pTFRAEntriesVec;
            return NULL;
        }
    private:
        uint32 _trackId;
        uint32 _reserved;
        uint8 _length_size_of_traf_num;
        uint8 _length_size_of_trun_num;
        uint8 _length_size_of_sample_num;

        Oscl_Vector<TFRAEntries*, OsclMemAllocator>  *_pTFRAEntriesVec;
        PVLogger *iLogger, *iStateVarLogger, *iParsedDataLogger;
};

#endif
