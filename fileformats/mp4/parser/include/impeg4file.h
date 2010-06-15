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
/*                            MPEG-4 Mpeg4File Class                             */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    The Mpeg4File Class is the class that will construct and maintain all the
    mecessary data structures to be able to render a valid MP4 file to disk.
    Format.
*/

#ifndef IMPEG4FILE_H_INCLUDED
#define IMPEG4FILE_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif

//HEADER FILES REQD FOR MULTIPLE SAMPLE RETRIEVAL API
#ifndef OSCL_MEDIA_DATA_H_INCLUDED
#include "oscl_media_data.h"
#endif
#ifndef PV_GAU_H_INCLUDED
#include "pv_gau.h"
#endif

#ifndef TEXTSAMPLEENTRY_H_INCLUDED
#include "textsampleentry.h"
#endif
#ifndef FONTRECORD_H_INCLUDED
#include "fontrecord.h"
#endif

#ifndef ATOMDEFS_H_INCLUDED
#include "atomdefs.h"
#endif

#ifndef PV_MP4FFPARSER_CONFIG_H_INCLUDED
#include "pv_mp4ffparser_config.h"
#endif

#ifndef PVMF_META_DATA_TYPES_H_INCLUDED
#include "pvmf_meta_data_types.h"
#endif

/* CPM Related Header Files */
#ifndef CPM_H_INCLUDED
#include "cpm.h"
#endif
#include "pv_id3_parcom_types.h"

#ifndef PVMF_GAPLESS_METADATA_H_INCLUDED
#include "pvmf_gapless_metadata.h"
#endif

class OsclFileHandle;
class PvmiDataStreamObserver;
class AVCSampleEntry;

class EncryptionInfo
{
    public:
        EncryptionInfo(uint32 aAlgoId, uint8 aIVSz, const uint8* const& aKID)
                : iAlogorithmID(aAlgoId)
                , iIVSz(aIVSz)
        {
            oscl_memcpy(iKID, aKID, DEFAULT_KEY_SIZE);
        }

        uint32 GetAlgoID() const
        {
            return iAlogorithmID;
        }

        uint8 GetIVSize() const
        {
            return iIVSz;
        }

        const uint8* GetKeyID() const
        {
            return iKID;
        }

        enum
        {
            DEFAULT_KEY_SIZE = 16
        };
    private:
        uint32  iAlogorithmID;
        uint8   iIVSz;
        uint8   iKID[DEFAULT_KEY_SIZE];
};

class EntryInfo
{
    public:
        EntryInfo(uint16 aClearBytesCnt, uint32 aEncryptedBytesCnt): iClearBytes(aClearBytesCnt), iEncryptedBytes(aEncryptedBytesCnt) {}
        uint16 GetClearBytesCount() const
        {
            return iClearBytes;
        }
        uint32 GetEncryptedBytesCount() const
        {
            return iEncryptedBytes;
        }
    private:
        uint16 iClearBytes;
        uint32 iEncryptedBytes;
};

class SampleInfo
{
    public:
        SampleInfo(uint32 aSampleNumber): iSampleNumber(aSampleNumber), iNumberOfEntries(0) {}

        void SetEntryCount(uint32 aEntryCount)
        {
            iNumberOfEntries = aEntryCount;
            iEntryInfoVect.reserve(iNumberOfEntries);
            for (uint i = 0; i < iNumberOfEntries; i++)
            {
                iEntryInfoVect.push_back(NULL);
            }
        }

        void SetEntryInfo(uint32 aIndex, EntryInfo* apEntryInfo)
        {
            if (aIndex < iNumberOfEntries)
            {
                iEntryInfoVect[aIndex] = apEntryInfo;
            }
            else
            {
                OSCL_LEAVE(OsclErrArgument);
            }
        }

        EntryInfo* GetEntryInfo(uint32 aIndex) const
        {
            if (iEntryInfoVect.size() > aIndex)
                return iEntryInfoVect[aIndex];
            else
                return NULL;
        }

        uint32 GetEntriesCount() const
        {
            return iNumberOfEntries;
        }

        uint32 GetSampleNumber() const
        {
            return iSampleNumber;
        }

    private:
        uint32 iSampleNumber;
        uint32 iNumberOfEntries;
        Oscl_Vector<EntryInfo*, OsclMemAllocator> iEntryInfoVect;
};

class PVPIFFProtectedSampleDecryptionInfo
{
    public:
        PVPIFFProtectedSampleDecryptionInfo(const EncryptionInfo* aSampleEncryptionInfo, const uint8* aInitializationVector, const SampleInfo* apSampleInfo)
                : ipInitializationVector(aInitializationVector)
                , ipSampleInfo(apSampleInfo)
                , ipSampleEncryptionInfo(aSampleEncryptionInfo) {}

        const uint8* GetKeyUsedForEncryption() const
        {
            if (ipSampleEncryptionInfo)
            {
                return ipSampleEncryptionInfo->GetKeyID();
            }
            return NULL;
        }

        uint8 GetInitializationVector(const uint8*& apInitializationVector)
        {
            if (ipSampleEncryptionInfo)
            {
                apInitializationVector = ipInitializationVector;
                return ipSampleEncryptionInfo->GetIVSize();
            }
            return 0;
        }

        bool GetSampleEntriesCnt(uint32& aEntryCount) const
        {
            if (ipSampleInfo)
            {
                aEntryCount = ipSampleInfo->GetEntriesCount();
                return true;
            }
            aEntryCount = 0;
            return false;
        }

        bool GetEntryInfo(uint32 aIndex, EntryInfo*& aEntryInfo) const
        {
            if (ipSampleInfo)
            {
                aEntryInfo = ipSampleInfo->GetEntryInfo(aIndex);
                return true;
            }
            else
            {
                return false;
            }
        }

        uint32 GetAlgoUsedForEncryption() const
        {
            if (ipSampleEncryptionInfo)
            {
                return ipSampleEncryptionInfo->GetAlgoID();
            }
            return SAMPLE_NOT_ENCRYPTED;
        }

        enum
        {
            SAMPLE_NOT_ENCRYPTED = 0
        };
    private:
        const uint8* ipInitializationVector;
        const SampleInfo* ipSampleInfo; //A sample is composed of set of entries
        const EncryptionInfo* ipSampleEncryptionInfo;
};

enum PIFF_PROTECTION_SYSTEM
{
    PIFF_PROTECTION_SYSTEM_PLAYREADY = 0,
    PIFF_PROTECTION_SYSTEM_UNKNOWN
};

/*------------- Interface of Class Mpeg4 File ----------------*/
class IMpeg4File : public ISucceedFail
{
    public:
        virtual ~IMpeg4File() {} // So the Mpeg4File destructor gets called when delete the interface

        // MEDIA DATA APIS

        /* Returns media samples for the requested tracks
           id:  The track ID of the track from which the method is to retrieve the samples.
           buf: A pointer to the buffer into which to place the sample.
           size:    The size of the data buffer
           index:   An output parameter which is the index of the sample entry to which the returned sample refers.
           return:  The size in bytes of the data placed into the provided buffer.  If the buffer is not large enough, the return value is the negative of the size that is needed.
        */
        virtual int32 getNextMediaSample(uint32 id, uint8 *buf, uint32 &size, uint32 &index, TOsclFileOffset &SampleOffset) = 0;

        virtual int32 getMediaSample(uint32 id, uint32 sampleNumber, uint8 *buf, uint32 &size, uint32 &index, TOsclFileOffset &SampleOffset) = 0;

        virtual int32 getOffsetByTime(uint32 id, uint64 ts, TOsclFileOffset* sampleFileOffset, uint32 jitterbuffersize) = 0;

        virtual int32 updateFileSize(TOsclFileOffset filesize) = 0;

        virtual MP4_ERROR_CODE getKeyMediaSampleNumAt(uint32 aTrackId,
                uint32 aKeySampleNum,
                GAU    *pgau, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>* = NULL) = 0;
        virtual int32 getPrevKeyMediaSample(uint64 inputtimestamp,
                                            uint32 &aKeySampleNum,
                                            uint32 id,
                                            uint32 *n,
                                            GAU    *pgau, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>* = NULL) = 0;
        virtual int32 getNextKeyMediaSample(uint32 &aKeySampleNum,
                                            uint32 id,
                                            uint32 *n,
                                            GAU    *pgau, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>* = NULL) = 0;

        /* Returns the timestamp for the previously returned media samples from the requested track
           id:  The track ID of the track from which the method is to retrieve the sample timestamp.
           return:  The timestamp of the most recently return media sample in the "media timescale"
        */
        virtual uint64 getMediaTimestampForCurrentSample(uint32 id) = 0;


        // META DATA APIS

        // from 'ftyp' atom
        virtual uint32 getCompatibiltyMajorBrand() = 0;

        // From Movie
        virtual int32 getNumTracks() = 0;
        virtual int32 getTrackIDList(uint32 *ids, int size) = 0;
        virtual uint32 getTrackWholeIDList(uint32 *ids) = 0;

        // From MovieHeader
        virtual uint64 getMovieDuration() const = 0;
        virtual uint32 getMovieTimescale() const = 0;


        // From TrackHeader
        virtual uint64 getTrackDuration(uint32 id) = 0; // in movie timescale

        // From TrackReference
        // Returns the track ID of the track on which this track depends
        virtual uint32 trackDependsOn(uint32 id) = 0;

        // From MediaHeader
        virtual uint64 getTrackMediaDuration(uint32 id) = 0;
        virtual uint32 getTrackMediaTimescale(uint32 id) = 0;
        virtual uint16 getTrackLangCode(uint32 id) = 0;

        // From Handler
        // Returns the 4CC of the track media type (i.e. 'vide' for video)
        virtual uint32 getTrackMediaType(uint32 id) = 0;

        // From SampleDescription
        // Returns the number of sample entries stored in the sample description
        virtual int32 getTrackNumSampleEntries(uint32 id) = 0;

        // From DecoderConfigDescriptor
        virtual void getTrackMIMEType(uint32 id, OSCL_String& aMimeType) = 0; // Based on OTI and string tables

        virtual int32  getTrackMaxBufferSizeDB(uint32 id) = 0;
        virtual int32  getTrackAverageBitrate(uint32 id) = 0;

        virtual uint8 *getTrackDecoderSpecificInfoContent(uint32 id) = 0;
        virtual uint32 getTrackDecoderSpecificInfoSize(uint32 id) = 0;
        virtual DecoderSpecificInfo *getTrackDecoderSpecificInfoAtSDI(uint32 trackID, uint32 index) = 0;

        virtual MP4_ERROR_CODE getTimestampForSampleNumber(uint32 id, uint32 sampleNumber, uint64& aTimeStamp) = 0;
        virtual MP4_ERROR_CODE getSampleSizeAt(uint32 id, int32 sampleNum, uint32& aSampleSize) = 0;

        //From PASP atom
        virtual uint32 getHspacing(uint32 id) = 0;
        virtual uint32 getVspacing(uint32 id) = 0;



        // MPEG4 header retrieval methods
        virtual int32 getFileType() const = 0;
        virtual int32 getScalability() const = 0;

        virtual int32 getTimestampForRandomAccessPoints(uint32 id, uint32 *num, uint64 *tsBuf, uint32* numBuf, TOsclFileOffset* offsetBuf = NULL) = 0;
        virtual int32 getTimestampForRandomAccessPointsBeforeAfter(uint32 id, uint64 ts, uint64 *tsBuf, uint32* numBuf,
                uint32 &numsamplestoget,
                uint32 howManyKeySamples = 1) = 0;

        // Static method to read in an MP4 file from disk and return the IMpeg4File interface
        OSCL_IMPORT_REF static IMpeg4File *readMP4File(OSCL_wString& aFilename,
                PVMFCPMPluginAccessInterfaceFactory* aCPMAccessFactory,
                OsclFileHandle* aHandle = NULL,
                uint32 aParsingMode = 0,
                Oscl_FileServer* aFileServSession = NULL,
                bool aOpenFileOncePerTrack = true);

        OSCL_IMPORT_REF static void DestroyMP4FileObject(IMpeg4File* aMP4FileObject);

        virtual bool CreateDataStreamSessionForExternalDownload(OSCL_wString& aFilename,
                PVMFCPMPluginAccessInterfaceFactory* aCPMAccessFactory,
                OsclFileHandle* aHandle = NULL,
                Oscl_FileServer* aFileServSession = NULL) = 0;

        virtual void DestroyDataStreamForExternalDownload() = 0;

        virtual void resetPlayback() = 0;
        virtual uint32 resetPlayback(uint32 time, uint16 numTracks, uint32 *trackList, bool bResetToIFrame = true) = 0;
        virtual uint32 queryRepositionTime(uint32 time,
                                           uint16 numTracks,
                                           uint32 *trackList,
                                           bool bResetToIFrame = true,
                                           bool bBeforeRequestedTime = true) = 0;


        virtual int32 querySyncFrameBeforeTime(uint32 time, uint16 numTracks, uint32 *trackList) = 0;

        virtual int32 getNextBundledAccessUnits(const uint32 id,
                                                uint32 *n,
                                                GAU    *pgau, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo , OsclMemAllocator>* pSampleDecryptionInfoVect = NULL) = 0;

        virtual int32 peekNextBundledAccessUnits(const uint32 id,
                uint32 *n,
                MediaMetaInfo *mInfo) = 0;

        virtual uint32 getSampleCountInTrack(uint32 id) = 0;

        virtual bool IsTFRAPresentForAllTrack(uint32 numTracks, uint32 *trackList) = 0;
        virtual uint32 getNumKeyFrames(uint32 trackid) = 0;

        virtual int16 getLayer(uint32 trackid) = 0;
        virtual uint16 getAlternateGroup(uint32 trackid) = 0;

        virtual int32 getVideoFrameHeight(uint32 trackid) = 0;
        virtual int32 getVideoFrameWidth(uint32 trackid) = 0;

        virtual int32 getTextTrackWidth(uint32 trackid) = 0;
        virtual int32 getTextTrackHeight(uint32 trackid) = 0;
        virtual int32 getTextTrackXOffset(uint32 trackid) = 0;
        virtual int32 getTextTrackYOffset(uint32 trackid) = 0;
        virtual SampleEntry *getTextSampleEntryAt(uint32 trackid, uint32 index) = 0;


        OSCL_IMPORT_REF static int32 IsXXXable(OSCL_wString& filename,
                                               TOsclFileOffset &metaDataSize,
                                               int32  &oMoovBeforeMdat,
                                               uint32 *pMajorBrand,
                                               uint32 *pCompatibleBrands,
                                               Oscl_FileServer* fileServSession = NULL);


        OSCL_IMPORT_REF static int32 IsXXXable(Oscl_File* fileRef,
                                               TOsclFileOffset &metaDataSize,
                                               int32  &oMoovBeforeMdat,
                                               uint32 *pMajorBrand,
                                               uint32 *pCompatibleBrands);

        virtual bool IsMobileMP4() = 0;

        virtual int32 getNumAMRFramesPerSample(uint32 trackID) = 0;

        virtual uint8 parseBufferAndGetNumAMRFrames(uint8* buffer, uint32 size) = 0;

        virtual uint32 getNumAVCSampleEntries(uint32 trackID) = 0;

        virtual AVCSampleEntry* getAVCSampleEntry(uint32 trackID, uint32 index = 0) = 0;

        virtual uint32 getAVCNALLengthSize(uint32 trackID, uint32 index = 0) = 0;

        /*
         * @param Oscl_File* filePtr - File pointer to the MP4/3GP
         * file. Please note that the file open and close are handled by the
         * caller.
         *
         * @param TOsclFileOffset fileSize - Size of the downloaded file, thus far.
         *
         *
         * @param bool& oIsProgressiveDownloadable - Set to true if the clip is
         * is progressive dowmloadable.
         *
         * @param TOsclFileOffset& metaDataSize - If the clip is progressive
         * downloadable then this API also returns the meta data size. Player
         * needs to wait for the file to grow past the metaDataSize before
         * starting playback.This param is valid only if oIsProgressiveDownloadable
         * is set to TRUE.
         *
         * @return MP4_ERROR_CODE - EVERYTHING_FINE, if a conclusion is reached
         * either way on whether a clip is progressive downloadable or not.
         * INSUFFICIENT_DATA, if more calls to this API are needed to reach a
         * decision
         * Any other return value indicates error.
         */
        OSCL_IMPORT_REF static MP4_ERROR_CODE IsProgressiveDownloadable(Oscl_File* filePtr,
                TOsclFileOffset  fileSize,
                bool& oIsProgressiveDownloadable,
                TOsclFileOffset& metaDataSize);

        /*
         * @param aCPMAccessFactory aCPMAccessFactory - Pointer to the datastream
         * factory.
         *
         * @param bool& oIsProgressiveDownloadable - Set to true if the clip is
         * is progressive dowmloadable.
         *
         * @param TOsclFileOffset& metaDataSize - If the clip is progressive
         * downloadable then this API also returns the meta data size. Player
         * needs to wait for the file to grow past the metaDataSize before
         * starting playback.This param is valid only if oIsProgressiveDownloadable
         * is set to TRUE.
         *
         * @return MP4_ERROR_CODE - EVERYTHING_FINE, if a conclusion is reached
         * either way on whether a clip is progressive downloadable or not.
         * INSUFFICIENT_DATA, if more calls to this API are needed to reach a
         * decision
         * Any other return value indicates error.
         */
        OSCL_IMPORT_REF static MP4_ERROR_CODE GetMetaDataSize(PVMFCPMPluginAccessInterfaceFactory* aCPMAccessFactory,
                bool& oIsProgressiveDownloadable,
                TOsclFileOffset& metaDataSize);


        /*
         * This API return the timestamp of a sample that is closest to the given
         * fileSize.
         *
         * @param trackID
         *
         * @param fileSize
         *
         * @param uint64& timeStamp
         *
         * @return EVERYTHING_FINE - In case a valid sample and corresponding time
         * stamp was located.
         * INSUFFICIENT_DATA in case the very first sample location is past the fileSize
         * NOT_SUPPORTED - in case "parsingMode" is set to 1, in "readMP4File"
         * call
         * Any other return value indicates ERROR.
         *
         */

        virtual MP4_ERROR_CODE getMaxTrackTimeStamp(uint32 trackID,
                TOsclFileOffset fileSize,
                uint64& timeStamp) = 0;

        /*
         * This API returns the closest sample number, prior to the required timestamp
         * The timestamp is assumed to be in media time scale, hence no timescale
         * conversions are performed internally.Closest frame number is returned in
         * the argument "sampleNumber", and it is offset by "sampleOffset".
         *
         * @param trackID
         *
         * @param frameNumber
         *
         * @param timeStamp
         *
         * @param frameOffset
         *
         * @return MP4_ERROR_CODE
         * In case the frameNumber closest to timestamp plus frameOffset falls
         * beyond the track boundary, frameNumber is set to total number of samples
         * per track and the return code is "END_OF_TRACK".In other error scenarios
         * (like invalid timestamps, some missing atoms etc) frameNumber is set to
         * zero, and return code is "READ_FAILED". In case of normal operation,
         * return code is "EVERYTHING_FINE"
         */
        virtual MP4_ERROR_CODE getSampleNumberClosestToTimeStamp(uint32 trackID,
                uint32 &sampleNumber,
                uint64 timeStamp,
                TOsclFileOffset sampleOffset = 0) = 0;

        /*
         * This API returns the size of the "odkm" header if present at the
         * track level. If the "odkm" header were not present at the
         * track level, this API would return ZERO. Please note that
         * the "odkm" is only present for OMA2 protected content.
         */
        virtual uint32 getTrackLevelOMA2DRMInfoSize(uint32 trackID) = 0;
        /*
         * This API returns a buffer containing the "odkm" header if present at the
         * track level. If the "odkm" header were not present at the
         * track level, this API would return NULL. The memory buffer
         * returned is owned by the file parser lib. Please note that
         * the "odkm" is only present for OMA2 protected content.
         */
        virtual uint8* getTrackLevelOMA2DRMInfo(uint32 trackID) = 0;

        /**
         * If the file is protected as per Portable Interoperable File Format spec
         * Then, there could be multiple protection systems used to protect the file
         * GetPIFFProtectionSystemID returns the uuid of the system uniquely
         * identifying the content protection sytem
        */
        virtual PIFF_PROTECTION_SYSTEM GetPIFFProtectionSystem() = 0;

        /**
         * If the file is protected as per Portable Interoperable File Format spec
         * Then, there could be multiple protection systems used to protect the file
         * GetPIFFProtectionSystemSpecificData provides the protection system specific
         * opaque data and the data size.
        */
        virtual MP4_ERROR_CODE GetPIFFProtectionSystemSpecificData(const uint8*& aOpaqueData, uint32& aDataSize) = 0;

        /*
         * This API is used to set a callback request on the datastream interface.
         */
        virtual MP4_ERROR_CODE RequestReadCapacityNotification(PvmiDataStreamObserver& aObserver,
                TOsclFileOffset aFileOffset,
                OsclAny* aContextData = NULL) = 0;

        /*
         * This API is used to cancel the callback request on the datastream interface.
         */
        virtual MP4_ERROR_CODE CancelNotificationSync() = 0;

        /*
         * This API is used to get the current file size from the datastream interface.
         */
        virtual MP4_ERROR_CODE GetCurrentFileSize(TOsclFileOffset& aFileSize) = 0;

        virtual int32 getTrackTSStartOffset(TOsclFileOffset& aTSOffset, uint32 aTrackID) = 0;

        // ITunes Specific functions
        virtual uint16 getITunesThisTrackNo() const = 0;
        virtual OSCL_wHeapString<OsclMemAllocator> getITunesWriter() const = 0;
        virtual uint16 getITunesTotalTracks() const = 0;
        virtual PvmfApicStruct* getITunesImageData() const = 0;
        virtual uint16 getITunesThisDiskNo() const = 0;
        virtual uint16 getITunesTotalDisks() const = 0;

        virtual bool IsMovieFragmentsPresent() const = 0;
        // retrieve gapless metadata
        virtual bool getITunesGaplessMetadata(PVMFGaplessMetadata& aGaplessMetadata) const = 0;

        // Reposition Related Video Track present API
        virtual void ResetVideoTrackPresentFlag() = 0;

        //APIs to return the no. of titles and their metadata values respectively.
        virtual uint32 getNumTitle() = 0;
        virtual PVMFStatus getTitle(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType) = 0;

        //APIs to return the no. of albums and their metadata values respectively.
        virtual uint32 getNumAlbum() = 0;
        virtual PVMFStatus getAlbum(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType) = 0;

        //APIs to return the no. of artist and their metadata values respectively.
        virtual uint32 getNumArtist() = 0;
        virtual PVMFStatus getArtist(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType) = 0;

        //APIs to return the no. of genre and their metadata values respectively.
        virtual uint32 getNumGenre() = 0;
        virtual PVMFStatus getGenre(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType) = 0;
        virtual int16 getITunesGnreID() const = 0;

        //APIs to return the no. of year and their metadata values respectively.
        virtual uint32 getNumYear() = 0;
        virtual PVMFStatus getYear(uint32 index, uint32& aVal) = 0;

        //Metadata related APIs
        virtual uint32 GetNumMetadataValues(PVMFMetadataList& aKeyList) = 0;
        virtual PVMFStatus GetMetadataValues(PVMFMetadataList& aKeyList,
                                             Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                             uint32 aStartingValueIndex, int32 aMaxValueEntries,
                                             int32 &aNumentriesadded, uint32 &aID3ValueCount) = 0;
        virtual PVMFStatus ReleaseMetadataValue(PvmiKvp& aValueKVP) = 0;
        virtual PVMFStatus InitMetaData(PVMFMetadataList* aAvailableMetadataKeys) = 0;
        virtual void SetMoofAtomsCnt(const uint32 aMoofAtmsCnt) = 0;
        virtual void SetMoofInfo(uint32 aTrackId, uint32 aIndex, uint64 aMoofOffset, uint64 aMoofTimestamp) = 0;
        virtual MP4_ERROR_CODE GetUserDataAtomInfo(uint32 &atomSize, TOsclFileOffset &atomOffset) = 0;

};

#endif // IMPEG4FILE_H_INCLUDED
