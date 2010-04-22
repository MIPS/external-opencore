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
#include "piffboxes.h"
#endif

#ifndef ATOMUTILS_H_INCLUDED
#include "atomutils.h"
#endif

//ProtectionSystemSpecificHeaderBox
ProtectionSystemSpecificHeaderBox::ProtectionSystemSpecificHeaderBox(MP4_FF_FILE* const& apFilePtr, uint32 aSize, uint32 aType, const uint8* const& apUUid)
        : ExtendedFullAtom(apFilePtr, aSize, aType, apUUid)
        , iDataSize(0), ipData(NULL)
{
    oscl_memset(iSystemID, 0, SYSTEM_ID_SIZE);

    if (_success)
    {
        _success = false;
        _mp4ErrorCode = READ_FAILED;

        //Get System ID and Opaque DataSize
        if (AtomUtils::readByteData(apFilePtr, SYSTEM_ID_SIZE, iSystemID) && AtomUtils::read32(apFilePtr, iDataSize))
        {
            if (!AtomUtils::IsHexUInt8StrEqual(iSystemID, PLAYREADY_SYSTEM_ID, SYSTEM_ID_SIZE))
            {
                _mp4ErrorCode = PSSH_BOX_UNRECOGNISED_SYSTEM_ID;
                return;
            }
            //Read the Opaque Data
            PV_MP4_FF_ARRAY_MALLOC(apFilePtr->auditCB, uint8, iDataSize, ipData);
            if (ipData)
            {
                AtomUtils::readByteData(apFilePtr, iDataSize, ipData);
                _success = true;
                _mp4ErrorCode = EVERYTHING_FINE;
            }
        }
    }
}

//TrackEncryptionBox
TrackEncryptionBox::TrackEncryptionBox(MP4_FF_FILE* const& apFilePtr,
                                       uint32 aSize,
                                       uint32 aType, const uint8*const& apUUid, uint32 aTrackId)
        : ExtendedFullAtom(apFilePtr, aSize, aType, apUUid)
        , iTrackId(aTrackId)
{
    ipEncryptionInfo = NULL;

    if (_success)
    {
        _success = false;
        _mp4ErrorCode = READ_FAILED;

        //Get Algo Id and IV size
        uint32 algoIdIVSz = 0;
        if (AtomUtils::read32(apFilePtr, algoIdIVSz))
        {
            const uint32 algoId = ((algoIdIVSz >> 8) & 0x00FFFFFF);
            const uint32 iVSz = OSCL_STATIC_CAST(uint8, (algoIdIVSz & 0x000000FF));

            //Read the KID
            uint8 buffer[EncryptionInfo::DEFAULT_KEY_SIZE];
            if (AtomUtils::readByteData(apFilePtr, EncryptionInfo::DEFAULT_KEY_SIZE, buffer))
            {
                //Intantiate the EncryptionInfo
                PV_MP4_FF_NEW(apFilePtr->auditCB, EncryptionInfo, (algoId, iVSz, buffer), ipEncryptionInfo);
                if (ipEncryptionInfo)
                {
                    _success = true;
                    _mp4ErrorCode = EVERYTHING_FINE;
                }
            }
        }
    }
}

//SampleEncryptionBox
SampleEncryptionBox::SampleEncryptionBox(MP4_FF_FILE* const& apFilePtr,
        uint32 aSize,
        uint32 aType,
        const uint8*const& apUUid,
        const TrackEncryptionBox* apTrackEncryptionBox)
        : ExtendedFullAtom(apFilePtr, aSize, aType, apUUid)
        , ipEncryptionInfo(NULL)
        , iEncryptionInfoOwned(false)
        , iSampleCount(0)
        , iSampleInfoCnt(0)
{
    if (apTrackEncryptionBox)
    {
        ipEncryptionInfo = apTrackEncryptionBox->GetEncryptionInfo();
    }

    if (_success)
    {
        if (getFlags() & 0x000001)
        {
            //Get Algo Id and IV size
            uint32 algoIdIVSz = 0;
            if (AtomUtils::read32(apFilePtr, algoIdIVSz))
            {
                const uint32 algoId = ((algoIdIVSz >> 8) & 0x00FFFFFF);
                const uint32 iVSz = OSCL_STATIC_CAST(uint8, (algoIdIVSz & 0x000000FF));

                //Read the KID
                uint8 buffer[EncryptionInfo::DEFAULT_KEY_SIZE];
                if (AtomUtils::readByteData(apFilePtr, EncryptionInfo::DEFAULT_KEY_SIZE, buffer))
                {
                    //Intantiate the EncryptionInfo
                    PV_MP4_FF_NEW(apFilePtr->auditCB, EncryptionInfo, (algoId, iVSz, buffer), ipEncryptionInfo);
                    if (ipEncryptionInfo)
                    {
                        iEncryptionInfoOwned = true;
                    }
                }
            }
        }
        //Read the sample count
        if (AtomUtils::read32(apFilePtr, iSampleCount))
        {
            iIVVect.reserve(iSampleCount);
            _success = true;
            _mp4ErrorCode = EVERYTHING_FINE;
        }
        if (_success)
        {
            PopulateIVVectAndSampleInfo(apFilePtr);
        }
    }
}

void SampleEncryptionBox::PopulateIVVectAndSampleInfo(MP4_FF_FILE* const& apFilePtr)
{
    bool sampleInfoAvailable = (getFlags() & 0x000002);
    for (uint32 i = 0; i < iSampleCount; i++)
    {
        const uint32 iIVSize = ipEncryptionInfo->GetIVSize();
        uint8* buffer = NULL;
        PV_MP4_FF_ARRAY_MALLOC(apFilePtr->auditCB, uint8, iIVSize, buffer);
        AtomUtils::readByteData(apFilePtr, iIVSize, buffer);
        iIVVect.push_back(buffer);
        //Check if Sample info is available
        if (sampleInfoAvailable)
        {
            uint16 sampleCount = 0;
            AtomUtils::read16(apFilePtr, sampleCount);
            SampleInfo* sampleInfo = NULL;
            PV_MP4_FF_NEW(apFilePtr->auditCB, SampleInfo, (i), sampleInfo);
            sampleInfo->SetEntryCount(sampleCount);
            for (uint16 ii = 0; ii < sampleCount ; ii++)
            {
                uint16 clearBytesCnt = 0;
                uint32 encryptedBytesCnt = 0;
                AtomUtils::read16(apFilePtr, clearBytesCnt);
                AtomUtils::read32(apFilePtr, encryptedBytesCnt);
                EntryInfo* pEntryInfo = NULL;
                PV_MP4_FF_NEW(apFilePtr->auditCB, EntryInfo, (clearBytesCnt, encryptedBytesCnt), pEntryInfo);
                sampleInfo->SetEntryInfo(ii, pEntryInfo);
            }
            iSampleInfoVect.push_back(sampleInfo);
        }
    }
    iSampleInfoCnt = iSampleInfoVect.size();
}

const TrackEncryptionBox* TrackEncryptionBoxContainer::GetTrackEncryptionBox(uint32 aTrackId) const
{
    Oscl_Vector<TrackEncryptionBox*, OsclMemAllocator>::const_iterator iter = iTrckEncryptnBoxCntnr.begin();
    while (iter != iTrckEncryptnBoxCntnr.end())
    {
        const TrackEncryptionBox* ptr = *iter;
        if (aTrackId == ptr->GetTrackId())
        {
            return *iter;
        }
    }
    return NULL;
}

void TrackEncryptionBoxContainer::PushTrackEncryptionBox(TrackEncryptionBox * apTrackEncryptionBox)
{
    iTrckEncryptnBoxCntnr.push_back(apTrackEncryptionBox);
}

MP4_ERROR_CODE SampleEncryptionBox::GetSamplesDecryptionInfo(uint32 aStartSampleNum, uint32 aNumberOfSamples, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>*  apSampleDecryptionInfoVect)
{
    if (apSampleDecryptionInfoVect && (apSampleDecryptionInfoVect->empty()) && (iIVVect.size() >= aNumberOfSamples))
    {
        for (uint32 ii = 0; ii < aNumberOfSamples; ii++)
        {
            PVPIFFProtectedSampleDecryptionInfo requiredDecryptionInfo(ipEncryptionInfo, iIVVect[aStartSampleNum + ii], GetSampleInfo(aStartSampleNum + ii));
            //Assuming both sample num and start sample num are zero based and vect is zero based
            apSampleDecryptionInfoVect->push_back(requiredDecryptionInfo);
        }
        return EVERYTHING_FINE;
    }
    else
    {
        return READ_FAILED;
    }
}

SampleInfo* SampleEncryptionBox::GetSampleInfo(uint32 aSampleNum)
{
    if (iSampleInfoVect.size() > 0)
    {
        if (iSampleInfoCnt > aSampleNum)
        {
            return iSampleInfoVect[aSampleNum];
        }
    }
    return NULL;
}
