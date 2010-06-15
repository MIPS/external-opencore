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
#ifndef _PIFFBOXES_H_
#define _PIFFBOXES_H_

#ifndef ATOMDEFS_H_INCLUDED
#include "atomdefs.h"
#endif

#ifndef FULLATOM_H_INCLUDED
#include"fullatom.h"
#endif

#ifndef IMPEG4FILE_H_INCLUDED
#include "impeg4file.h"
#endif

static const uint8 PLAYREADY_SYSTEM_ID[] = {0x9A, 0x04, 0xF0, 0x79, 0x98, 0x40, 0x42, 0x86, 0xAB, 0x92, 0xE6, 0x5B, 0xE0, 0x88, 0x5F, 0x95};
const uint32 SYSTEM_ID_SIZE = 16;

/**
ProtectionSystemSpecificHeaderBox :
This box contains a header needed by a Content Protection System to play back
the content. The header‘s format is specified by the System it is targeted to,
and is considered opaque from FF perspective
*/

class ProtectionSystemSpecificHeaderBox: public ExtendedFullAtom
{
    public:
        ProtectionSystemSpecificHeaderBox(MP4_FF_FILE*const & apFilePtr, uint32 aSize, uint32 aType, const uint8*const& apUUid);
        PIFF_PROTECTION_SYSTEM GetPIFFProtectionSystem() const
        {
            if (AtomUtils::IsHexUInt8StrEqual(iSystemID, PLAYREADY_SYSTEM_ID, SYSTEM_ID_SIZE))
            {
                return PIFF_PROTECTION_SYSTEM_PLAYREADY;
            }
            else
            {
                return PIFF_PROTECTION_SYSTEM_UNKNOWN;
            }
        }

        uint32 GetDataSize() const
        {
            return iDataSize;
        }

        const uint8* GetData() const
        {
            return ipData;
        }

        ~ProtectionSystemSpecificHeaderBox()
        {
            if (ipData)
                PV_MP4_ARRAY_FREE(NULL, ipData);
        }
    private:
        //We do not see a use case to clone obj of ProtectionSystemSpecificHeaderBox
        //or get it assigned to another ProtectionSystemSpecificHeaderBox obj
        //so making copy ctor and assignment oper private
        ProtectionSystemSpecificHeaderBox(const ProtectionSystemSpecificHeaderBox&);
        ProtectionSystemSpecificHeaderBox& operator=(const ProtectionSystemSpecificHeaderBox&);

        uint8   iSystemID[SYSTEM_ID_SIZE];
        uint32  iDataSize;
        uint8*  ipData;
};

class TrackEncryptionBox: public ExtendedFullAtom
{
    public:
        TrackEncryptionBox(MP4_FF_FILE* const& apFilePtr, //Pointer to the file pointing to the beginnning of ProtectionSystemSpecificHeaderBox
                           uint32 aSize,       //Size of ProtectionSystemSpecificHeaderBox
                           uint32 aType, const uint8*const& apUUid, //Type of the atom <used by the base class to validate if the box being instantiated
                           uint32 aTrackId = 0);
        uint32 GetAlgoID() const
        {
            if (ipEncryptionInfo)
            {
                return ipEncryptionInfo->GetAlgoID();
            }
            return 0;
        }

        uint8 GetIVSize() const
        {
            if (ipEncryptionInfo)
            {
                return ipEncryptionInfo->GetIVSize();
            }
            return 0;
        }

        const uint8* GetKeyID() const
        {
            if (ipEncryptionInfo)
            {
                return ipEncryptionInfo->GetKeyID();
            }
            return NULL;    //could consider adding assertion for ipEncryptionInfo
        }

        const EncryptionInfo* GetEncryptionInfo() const
        {
            return ipEncryptionInfo;
        }

        ~TrackEncryptionBox()
        {
            PV_MP4_FF_DELETE(NULL, EncryptionInfo, ipEncryptionInfo);
        }

        uint32 GetTrackId() const
        {
            return iTrackId;
        }

        void SetTrackId(uint32 aTrackId)
        {
            iTrackId = aTrackId;
        }

    private:

        //We do not see a use case to clone obj of TrackEncryptionBox
        //or get it assigned to another TrackEncryptionBox obj
        //so making copy ctor and assignment oper private
        TrackEncryptionBox(const TrackEncryptionBox&);
        TrackEncryptionBox& operator=(const TrackEncryptionBox&);

        EncryptionInfo* ipEncryptionInfo;

        uint32 iTrackId; //Makes sense to persist the trackid for which the box is created
};

class TrackEncryptionBoxContainer
{
    public:
        const TrackEncryptionBox* GetTrackEncryptionBox(uint32 aTrackId) const;
        void PushTrackEncryptionBox(TrackEncryptionBox * apTrackEncryptionBox);
    private:
        Oscl_Vector<TrackEncryptionBox*, OsclMemAllocator> iTrckEncryptnBoxCntnr;
};

class SampleEncryptionBox: public ExtendedFullAtom
{
    public:
        SampleEncryptionBox(MP4_FF_FILE* const& apFilePtr, //Pointer to the file pointing to the beginnning of ProtectionSystemSpecificHeaderBox
                            uint32 aSize,                //Size of ProtectionSystemSpecificHeaderBox
                            uint32 aType,
                            const uint8*const& apUUid,
                            const TrackEncryptionBox* apTrackEncryptionBox = NULL);
        uint32 GetAlgoID() const
        {
            if (ipEncryptionInfo)
            {
                return ipEncryptionInfo->GetAlgoID();
            }
            return 0;
        }

        uint8 GetIVSize() const
        {
            if (ipEncryptionInfo)
            {
                return ipEncryptionInfo->GetIVSize();
            }
            return 0;
        }

        const uint8* GetKeyID() const
        {
            if (ipEncryptionInfo)
            {
                return ipEncryptionInfo->GetKeyID();
            }
            return NULL;    //could consider adding assertion for ipEncryptionInfo
        }

        uint8* GetInitializationVector(uint32 aSampleCount)
        {
            return iIVVect[iSampleCount];
        }

        ~SampleEncryptionBox()
        {
            if (iEncryptionInfoOwned)
            {
                // Remove the const cast from ipEncryptionInfo since some compilers prevent
                // deleting a pointer to a const object.
                PV_MP4_FF_DELETE(NULL, EncryptionInfo, OSCL_CONST_CAST(EncryptionInfo*, ipEncryptionInfo));
            }
            int32 initVectSize =  iIVVect.size();
            for (int i = initVectSize - 1; i >= 0; i--)
            {
                if (iIVVect[i])
                {
                    PV_MP4_ARRAY_FREE(NULL, iIVVect[i]);
                    iIVVect[i] = NULL;
                }
            }
            iIVVect.clear();
            const int32 sampleInfoVectSz = iSampleInfoVect.size();
            for (int ii = sampleInfoVectSz - 1; ii >= 0; ii--)
            {
                if (iSampleInfoVect[ii])
                {
                    const uint32 sampleEntriesCnt = iSampleInfoVect[ii]->GetEntriesCount();
                    for (uint jj = 0; jj < sampleEntriesCnt; jj++)
                    {
                        PV_MP4_FF_DELETE(NULL, EntryInfo, iSampleInfoVect[ii]->GetEntryInfo(jj));
                        iSampleInfoVect[ii]->SetEntryInfo(jj, NULL);

                    }
                    PV_MP4_FF_DELETE(NULL, SampleInfo, iSampleInfoVect[ii]);
                    iSampleInfoVect[ii] = NULL;
                }
            }
            iSampleInfoVect.clear();
        }

        MP4_ERROR_CODE GetSamplesDecryptionInfo(uint32 aStartSampleNum, uint32 aNumberOfSamples, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>*  apSampleDecryptionInfoVect);

    private:
        //We do not see a use case to clone obj of SampleEncryptionBox
        //or get it assigned to another SampleEncryptionBox obj
        //so making copy ctor and assignment oper private
        SampleEncryptionBox(const SampleEncryptionBox&);
        SampleEncryptionBox& operator=(const SampleEncryptionBox&);

        void PopulateIVVectAndSampleInfo(MP4_FF_FILE* const& apFilePtr);
        SampleInfo* GetSampleInfo(uint32 aSampleNum);

        void ConstructL(MP4_FF_FILE* const& apFilePtr, uint32 aSize);

        const EncryptionInfo*   ipEncryptionInfo;
        bool    iEncryptionInfoOwned;
        uint32  iSampleCount;
        uint32 iSampleInfoCnt;
        Oscl_Vector<SampleInfo*, OsclMemAllocator> iSampleInfoVect;
        Oscl_Vector<uint8*, OsclMemAllocator> iIVVect;
};

#endif

