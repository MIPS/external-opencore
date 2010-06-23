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


#define IMPLEMENT_Mpeg4File

#include "oscl_int64_utils.h"

#ifndef MPEG4FILE_H_INCLUDED
#include "mpeg4file.h"
#endif

#include "atomdefs.h"
#include "atomutils.h"
#include "filetypeatom.h"
#include "oscl_utf8conv.h"
#include "oscl_string.h"
#include "oscl_snprintf.h"
#include "amrdecoderspecificinfo.h"
#include "h263decoderspecificinfo.h"
#include "media_clock_converter.h"
#include "pvmi_kvp_util.h"
#include "oscl_exclusive_ptr.h"
#include "pv_mime_string_utils.h"



#define MILLISECOND_TIMESCALE (1000)
#define PVMF_MP4_MAX_UINT32   (0xffffffffU)

typedef Oscl_Vector<TrackAtom*, OsclMemAllocator> trackAtomVecType;
typedef Oscl_Vector<MovieFragmentAtom*, OsclMemAllocator> movieFragmentAtomVecType;
typedef Oscl_Vector<MP4_FF_FILE*, OsclMemAllocator> moofFragmentAtomVecType;
typedef Oscl_Vector<TOsclFileOffset, OsclMemAllocator> movieFragmentOffsetVecType;
typedef Oscl_Vector<TrackDurationInfo*, OsclMemAllocator> trackDurationInfoVecType;
typedef Oscl_Vector<MovieFragmentRandomAccessAtom*, OsclMemAllocator> movieFragmentRandomAccessAtomVecType;

// Stream-in Constructor
Mpeg4File::Mpeg4File(MP4_FF_FILE *fp,
                     OSCL_wString& filename,
                     uint32 parsingMode,
                     bool aOpenFileOncePerTrack)
{

    _pmovieAtom = NULL;
    _puserDataAtom = NULL;
    _pFileTypeAtom = NULL;
    _pMovieFragmentAtom = NULL;
    _mp4ErrorCode = EVERYTHING_FINE;
    _isMovieFragmentsPresent = false;
    _pointerMovieAtomEnd = 0;
    _movieFragmentFilePtr = NULL;
    _pMovieFragmentAtomVec = NULL;
    _pMfraOffsetAtom = NULL;
    _pMovieFragmentRandomAccessAtomVec = NULL;
    _pTrackExtendsAtomVec = NULL;
    _pMoofOffsetVec = NULL;
    _pFileSessionsForMediaDataRetrievalVec = NULL;
    _ptrMoofEnds = 0;
    _parsing_mode = parsingMode;
    _pTrackDurationContainer = NULL;
    oMfraFound = false;
    _oVideoTrackPresent = false;
    parseMoofCompletely = true;
    isResetPlayBackCalled = false;
    _moofFragmentIdx = NULL;

    PV_MP4_FF_NEW(fp->auditCB, TrackEncryptionBoxContainer, (), ipTrckEncryptnBxCntr);

    parseMoofCompletely = true;
    moofParsingCompleted = true;
    moofSize = 0;
    moofType = UNKNOWN_ATOM;
    moofCount = 0;
    moofPtrPos = 0;
    currMoofNum = 0;
    countOfTrunsParsed = 0;
    _fp = NULL;

    _pID3Parser = NULL;

    numAlbum = 0;
    numArtist = 0;
    numYear = 0;
    numTitle = 0;
    numComment = 0;
    numAuthor = 0;
    numGenre = 0;
    numCopyright = 0;
    numRating = 0;
    numDescription = 0;
    numDate = 0;
    numLyricist = 0;
    numComposer = 0;
    numVersion = 0;
    iTotalMoofAtmsCnt = 0xFFFFFFFF;
    // Create miscellaneous vector of atoms
    PV_MP4_FF_NEW(fp->auditCB, trackAtomVecType, (), _pTrackAtomVec);
    PV_MP4_FF_NEW(fp->auditCB, movieFragmentAtomVecType, (), _pMovieFragmentAtomVec);
    PV_MP4_FF_NEW(fp->auditCB, movieFragmentOffsetVecType, (), _pMoofOffsetVec);
    PV_MP4_FF_NEW(fp->auditCB, moofFragmentAtomVecType, (), _pFileSessionsForMediaDataRetrievalVec);
    PV_MP4_FF_NEW(fp->auditCB, movieFragmentRandomAccessAtomVecType, (), _pMovieFragmentRandomAccessAtomVec);


    iLogger = PVLogger::GetLoggerObject("mp4ffparser");
    iStateVarLogger = PVLogger::GetLoggerObject("mp4ffparser_mediasamplestats");
    iParsedDataLogger = PVLogger::GetLoggerObject("mp4ffparser_parseddata");

    _success = true; // Initial state

    TOsclFileOffset fileSize;
    TOsclFileOffset filePointer;
    filePointer = AtomUtils::getCurrentFilePosition(fp);
    AtomUtils::getCurrentFileSize(fp, fileSize);

    _oPVContent = false;
    _oPVContentDownloadable = false;
    _commonFilePtr = NULL;

    _fileSize = fileSize;

    if (!aOpenFileOncePerTrack)
    {
        OsclAny*ptr = oscl_malloc(sizeof(MP4_FF_FILE));
        _fp = OSCL_PLACEMENT_NEW(ptr, MP4_FF_FILE());
        _fp->_pvfile.Copy(fp->_pvfile);
    }
    TOsclFileOffset count = (fileSize - filePointer);// -DEFAULT_ATOM_SIZE;

    //top level moov, mdat, udat
    while (count > 0)
    {
        // Read in atoms until reach end of file
        //there is a case that next atom is valid, but not in top level
        //so only check top level now
        uint32 atomType = UNKNOWN_ATOM;
        uint32 atomSize = 0;

        AtomUtils::getNextAtomType(fp, atomSize, atomType);


        if ((atomType == SKIP_ATOM)
                || (atomType == FREE_SPACE_ATOM)
                || (atomType == UUID_ATOM)
                || (atomType == UNKNOWN_ATOM)
                || (atomType == MEDIA_DATA_ATOM)
                || (atomType == META_DATA_ATOM))
        {
            if (atomSize == 1)
            {
                uint64 largeSize = 0;
                AtomUtils::read64(fp, largeSize);
                uint32 size =
                    Oscl_Int64_Utils::get_uint64_lower32(largeSize);
                count -= size;
                size -= 8; //for large size
                size -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, size);
            }
            else
            {
                if (atomSize < DEFAULT_ATOM_SIZE)
                {
                    _success = false;
                    _mp4ErrorCode = ZERO_OR_NEGATIVE_ATOM_SIZE;
                    break;
                }
                if (count < (int32)atomSize)
                {
                    _success = false;
                    _mp4ErrorCode = READ_FAILED;
                    break;
                }
                count -= atomSize;
                atomSize -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, atomSize);
            }
        }
        else if (atomType == USER_DATA_ATOM)
        {
            //"udta"
            // Check for 'pvmm' to see if it is "our" 'udta' atom

            uint32 isPVMMAtom = AtomUtils::peekNextNthBytes(fp, 2);

            if (isPVMMAtom == PVUSER_DATA_ATOM)
            {
                if (_puserDataAtom == NULL)
                {
                    PV_MP4_FF_NEW(fp->auditCB, UserDataAtom, (fp, atomSize, atomType), _puserDataAtom);
                    _oPVContent = true;
                    uint32 contentType = getContentType();
                    if (contentType == DEFAULT_AUTHORING_MODE)
                    {
                        _oPVContentDownloadable = true;
                    }
                    count -= _puserDataAtom->getSize();
                }
                else
                {
                    _success = false;
                    _mp4ErrorCode = READ_USER_DATA_ATOM_FAILED;
                    break;
                }
            }
            else
            {
                // Skip third party user data atom
                if (atomSize < DEFAULT_ATOM_SIZE)
                {
                    _success = false;
                    _mp4ErrorCode = ZERO_OR_NEGATIVE_ATOM_SIZE;
                    break;
                }
                if (count < (int32)atomSize)
                {
                    _success = false;
                    _mp4ErrorCode = READ_FAILED;
                    break;
                }
                count -= atomSize;
                atomSize -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, atomSize);
            }


        }
        else if (atomType == FILE_TYPE_ATOM)
        {
            if (_pFileTypeAtom == NULL)
            {
                //"ftyp"
                PV_MP4_FF_NEW(fp->auditCB, FileTypeAtom, (fp, atomSize, atomType), _pFileTypeAtom);

                if (!_pFileTypeAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = READ_FILE_TYPE_ATOM_FAILED;
                    break;
                }

                uint32 majorBrand = _pFileTypeAtom->getMajorBrand();
                uint32  majorBrandInfo = ENoFileType;
                uint32  compatibleBrandInfo = ENoFileType;


                switch (majorBrand)
                {
                    case BRAND_3GPP4:
                        majorBrandInfo |= E3GP4;
                        break;

                    case BRAND_3GPP5:
                        majorBrandInfo |= E3GP5;
                        break;

                    case MOBILE_MP4:
                        majorBrandInfo |= EMMP4;
                        break;

                    case BRAND_MP41:
                        majorBrandInfo |= EMP41;
                        break;

                    case BRAND_MP42:
                        majorBrandInfo |= EMP42;
                        break;

                    case BRAND_ISOM:
                        majorBrandInfo |= EISOM;
                        break;

                    default:
                        majorBrandInfo |= EUNKNOWN_TYPE;
                        break;
                }

                Oscl_Vector<uint32, OsclMemAllocator> *compatibleBrandArray =
                    _pFileTypeAtom->getCompatibleBrand();

                if (compatibleBrandArray != NULL)
                {
                    for (uint32 i = 0; i < compatibleBrandArray->size(); i++)
                    {
                        uint32 compatibleBrand = (*compatibleBrandArray)[i];

                        switch (compatibleBrand)
                        {
                            case BRAND_3GPP4:
                                compatibleBrandInfo |= E3GP4;
                                break;

                            case BRAND_3GPP5:
                                compatibleBrandInfo |= E3GP5;
                                break;

                            case MOBILE_MP4:
                                compatibleBrandInfo |= EMMP4;
                                break;

                            case BRAND_MP41:
                                compatibleBrandInfo |= EMP41;
                                break;

                            case BRAND_MP42:
                                compatibleBrandInfo |= EMP42;
                                break;

                            case BRAND_ISOM:
                                compatibleBrandInfo |= EISOM;
                                break;

                            default:
                                compatibleBrandInfo |= EUNKNOWN_TYPE;
                                break;
                        }
                    }
                }
                count -= _pFileTypeAtom->getSize();
            }
            else
            {
                //multiple "ftyp" atom not allowed.skipping
                count -= atomSize;
                atomSize -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, atomSize);
            }
        }
        else if (atomType == MOVIE_ATOM)
        {
            //"moov"
            if (_pmovieAtom == NULL)
            {
                // Only 1 movie atom allowed!
                PV_MP4_FF_NEW(fp->auditCB, MovieAtom,
                              (fp,
                               filename,
                               atomSize,
                               atomType,
                               ipTrckEncryptnBxCntr,
                               _oPVContent,
                               _oPVContentDownloadable,
                               parsingMode,
                               aOpenFileOncePerTrack),
                              _pmovieAtom);

                if (!_pmovieAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = _pmovieAtom->GetMP4Error();
                    break;
                }
                _isMovieFragmentsPresent = _pmovieAtom->IsMovieFragmentPresent();

                PV_MP4_FF_NEW(fp->auditCB, TrackIndexVecType, (), _moofFragmentIdx);
                populateTrackDurationVec();

                _pTrackExtendsAtomVec = _pmovieAtom->getTrackExtendsAtomVec();

                if (_isMovieFragmentsPresent)
                {
                    atomSize -= DEFAULT_ATOM_SIZE;
                    _pointerMovieAtomEnd =  AtomUtils::getCurrentFilePosition(fp);
                    _ptrMoofEnds = _pointerMovieAtomEnd;

                    if (_pMoofOffsetVec->size() > 0)
                    {
                        if (_pointerMovieAtomEnd > (*_pMoofOffsetVec)[0])
                        {
                            _ptrMoofEnds = (*_pMoofOffsetVec)[0];
                        }
                    }
                    OsclAny*ptr = oscl_malloc(sizeof(MP4_FF_FILE));
                    if (ptr == NULL)
                    {
                        _success = false;
                        _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                        return;
                    }
                    _movieFragmentFilePtr = OSCL_PLACEMENT_NEW(ptr, MP4_FF_FILE());
                    _movieFragmentFilePtr->_fileServSession = fp->_fileServSession;
                    _movieFragmentFilePtr->_pvfile.SetCPM(fp->_pvfile.GetCPM());
                    _movieFragmentFilePtr->_pvfile.SetFileHandle(fp->_pvfile.iFileHandle);

                    if (AtomUtils::OpenMP4File(filename,
                                               Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                               _movieFragmentFilePtr) != 0)
                    {
                        _success = false;
                        _mp4ErrorCode = FILE_OPEN_FAILED;
                    }
                    _movieFragmentFilePtr->_fileSize = fp->_fileSize;
                    OpenSessionForMoofMediaDataRetrieval(fp, filename);
                }
                count -= _pmovieAtom->getSize();
                _scalability = _pmovieAtom->getScalability();
                _fileType    = _pmovieAtom->getFileType();

                if (parsingMode != 0)
                {
                    if (_isMovieFragmentsPresent)
                    {
                        parseMFRA();

                        if (!oMfraFound)
                            continue;
                    }
                    break;
                }
            }
            else
            { //after the change above, we will never hit here.
                _success = false;
                _mp4ErrorCode = DUPLICATE_MOVIE_ATOMS;
                break;
            }
        }
        else if (atomType == MOVIE_FRAGMENT_ATOM)
        {

            TOsclFileOffset moofStartOffset = AtomUtils::getCurrentFilePosition(fp);
            moofStartOffset -= DEFAULT_ATOM_SIZE;
            if (parsingMode != 0)
            {
                AtomUtils::seekFromStart(fp, moofStartOffset);
                break;
            }
            _pMoofOffsetVec->push_back(moofStartOffset);

            if (_pmovieAtom == NULL)
            {
                count -= atomSize;
                atomSize -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, atomSize);
            }
            else
            {
                MovieFragmentAtom *pMovieFragmentAtom = NULL;
                PV_MP4_FF_NEW(fp->auditCB, MovieFragmentAtom, (fp, _pFileSessionsForMediaDataRetrievalVec, atomSize, atomType, _pTrackDurationContainer, _pTrackExtendsAtomVec, parseMoofCompletely, moofParsingCompleted, countOfTrunsParsed, ipTrckEncryptnBxCntr), pMovieFragmentAtom);

                if (!pMovieFragmentAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = pMovieFragmentAtom->GetMP4Error();
                    break;
                }
                pMovieFragmentAtom->setParent(this);
                count -= pMovieFragmentAtom->getSize();
                _ptrMoofEnds = AtomUtils::getCurrentFilePosition(fp);
                _pMovieFragmentAtomVec->push_back(pMovieFragmentAtom);
            }
        }
        else if (atomType == MOVIE_FRAGMENT_RANDOM_ACCESS_ATOM)
        {
            if (_pmovieAtom == NULL)
            {
                count -= atomSize;
                atomSize -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, atomSize);
            }
            else
            {
                MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = NULL;
                PV_MP4_FF_NEW(fp->auditCB, MovieFragmentRandomAccessAtom, (fp, atomSize, atomType), pMovieFragmentRandomAccessAtom);
                if (!pMovieFragmentRandomAccessAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = pMovieFragmentRandomAccessAtom->GetMP4Error();
                    PV_MP4_FF_DELETE(NULL, MovieFragmentRandomAccessAtom, pMovieFragmentRandomAccessAtom);
                    break;
                }
                pMovieFragmentRandomAccessAtom->setParent(this);
                count -= pMovieFragmentRandomAccessAtom->getSize();
                _pMovieFragmentRandomAccessAtomVec->push_back(pMovieFragmentRandomAccessAtom);
                oMfraFound = true;

                // Exit the loop at this point since the movie atom has already been parsed
                if ((_pmovieAtom != NULL) && (_parsing_mode != 0))
                    break;
            }
        }
        else
        {
            if (count > 0)
            {
                _mp4ErrorCode = READ_UNKNOWN_ATOM;
                _success = false;
            }
            break;
        }
    }

    if (_success)
    {
        // Check that the movie atom was in fact read in
        if (_pmovieAtom == NULL)
        {
            _success = false;
            _mp4ErrorCode = NO_MOVIE_ATOM_PRESENT;
        }
        else
        {
            // CHECK IF THERE ARE ANY VALID MEDIA TRACKS IN THE FILE
            int32 numMediaTracks = getNumTracks();
            if (numMediaTracks == 0)
            {
                _success = false;
                _mp4ErrorCode = NO_META_DATA_FOR_MEDIA_TRACKS;
            }
            if (_success)
            {
                uint32 bufferCapacity = AtomUtils::getFileBufferingCapacity(fp);
                if (0 != bufferCapacity)
                {
                    // progressive playback
                    TOsclFileOffset* offsetList = (TOsclFileOffset *)oscl_malloc(sizeof(TOsclFileOffset) * numMediaTracks);
                    if (NULL == offsetList)
                    {
                        _success = false;
                        _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
                    }
                    else
                    {
                        // get the list of track ids
                        uint32* idList = (uint32 *)oscl_malloc(sizeof(uint32) * numMediaTracks);
                        _pmovieAtom->getTrackIDList(idList, numMediaTracks);

                        // get the first sample file offset of each track
                        for (int32 i = 0; i < numMediaTracks; i++)
                        {
                            int32 retVal = getOffsetByTime(idList[i], 0, &offsetList[i], 0);
                            if (DEFAULT_ERROR == retVal)
                            {
                                _success = false;
                                _mp4ErrorCode = retVal;
                                break;
                            }
                        }
                        // check if any of the two offsets are too far apart
                        // to coexist in the cache at the same time
                        if (_success)
                        {
                            uint32 largest = 0, temp = 0;
                            for (int i = 0; i < numMediaTracks; i++)
                            {
                                for (int j = 0; j < numMediaTracks; j++)
                                {
                                    // same as abs()
                                    if (offsetList[i] > offsetList[j])
                                    {
                                        temp = (uint32)(offsetList[i] - offsetList[j]);
                                    }
                                    else
                                    {
                                        temp = (uint32)(offsetList[j] - offsetList[i]);
                                    }

                                    if (temp > largest)
                                    {
                                        largest = temp;
                                    }
                                }
                            }

                            if (largest > bufferCapacity)
                            {
                                // the samples are not interleaved properly
                                // this clip is not authored for progressive playback
                                _success = false;
                                _mp4ErrorCode = INSUFFICIENT_BUFFER_SIZE;
                            }
                        }

                        oscl_free(idList);
                    }

                    oscl_free(offsetList);
                }
            }
        }
    }


    // Check for any atoms that may have read past the EOF that were not
    // already caught by any earlier error handling

    if (filePointer > fileSize)
    {
        _mp4ErrorCode = READ_FAILED; // Read past EOF
        _success = false;
    }

    //populate metadata only if parsing was successful
    if (_success)
    {
        // skip ID3 tag parsing for progressive playback for now
        uint32 bufferCapacity = AtomUtils::getFileBufferingCapacity(fp);
        if (0 == bufferCapacity)
        {
            //Check to see if ID3V2 was present. In case it was present, ID3Parcom would have
            //traversed ID3V1 as well. In case it was not there, ID3V2Atom would not have been created
            //in MetaData Atom and hence parse ID3V1 now
            if (!IsID3V2Present())
            {
                PV_MP4_FF_NEW(fp->auditCB, PVID3ParCom, (), _pID3Parser);
                parseID3Header(fp);
            }
        }
        //Populate the title vector with all the title metadata values.
        if (!populateMetadataVectors())
        {
            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "Mpeg4File::populateMetadataVector() Failed"));
        }
    }
}


PVMFStatus Mpeg4File::populateMetadataVectors()
{
    if ((!populateTitleVector()) || (!populateAuthorVector() ||
                                     (!populateAlbumVector()) ||
                                     (!populateArtistVector()) || (!populateGenreVector()) ||
                                     (!populateYearVector()) || (!populateCopyrightVector()) ||
                                     (!populateCommentVector()) || (!populateDescriptionVector()) ||
                                     (!populateDateVector()) || (!populateLyricistVector()) ||
                                     (!populateRatingVector()) || (!populateComposerVector()) ||
                                     (!populateVersionVector()))
       )
    {
        return PVMFFailure;
    }

    return PVMFSuccess;
}

uint32 Mpeg4File::getNumTitle()
{
    uint32 numTitle = 0;
    MP4FFParserOriginalCharEnc chartype = ORIGINAL_CHAR_TYPE_UNKNOWN;
    numTitle = getNumAssetInfoTitleAtoms();
    if (getPVTitle(chartype).get_size() > 0)
    {
        numTitle++;
    }
    if (getITunesTitle().get_size() > 0)
    {
        numTitle++;
    }

    return numTitle;
}

//This function populates the Title Vector with values from Asset Info, Itunes, FullMusic and PV Proprietary Atoms.
PVMFStatus Mpeg4File::populateTitleVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    int32 numAssetInfoTitle = getNumAssetInfoTitleAtoms();
    numTitle = numAssetInfoTitle;

    for (int32 i = 0; i < numAssetInfoTitle; i++)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoTitleNotice(charType, i);

        titleValues.push_front(valuestring);
        iTitleLangCode.push_front(getAssetInfoTitleLangCode(i));
        iTitleCharType.push_front(charType);
    }
    if (getPVTitle(charType).get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getPVTitle(charType);
        titleValues.push_front(valuestring);
        iTitleLangCode.push_front(0);
        iTitleCharType.push_front(charType);
        numTitle++;
    }
    if (getITunesTitle().get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesTitle();
        titleValues.push_front(valuestring);
        iTitleLangCode.push_front(0);
        iTitleCharType.push_front(ORIGINAL_CHAR_TYPE_UNKNOWN);
        numTitle++;
    }

    return PVMFSuccess;
}

// This function returns the titles based on index value, to the parser node.
PVMFStatus Mpeg4File::getTitle(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType)
{
    if (index < titleValues.size())
    {
        aVal = titleValues[index].get_cstr();
        aLangCode = iTitleLangCode[index];
        aCharEncType = iTitleCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

//This function populates the Author Vector with values from Asset Info and PV Proprietary Atoms.
PVMFStatus Mpeg4File::populateAuthorVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    int32 numAssetInfoAuthor = getNumAssetInfoAuthorAtoms();
    numAuthor = numAssetInfoAuthor;
    for (int32 i = 0; i < numAssetInfoAuthor; i++)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoAuthorNotice(charType, i);
        authorValues.push_front(valuestring);
        iAuthorLangCode.push_front(getAssetInfoAuthorLangCode(i));
        iAuthorCharType.push_front(charType);
    }
    if (getPVAuthor(charType).get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getPVAuthor(charType);
        authorValues.push_front(valuestring);
        iAuthorLangCode.push_front(0);
        iAuthorCharType.push_front(charType);
        numAuthor++;
    }
    return PVMFSuccess;
}

// This function returns the Author based on index value, to the parser node.
PVMFStatus Mpeg4File::getAuthor(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType)
{
    if (index < authorValues.size())
    {
        aVal = authorValues[index].get_cstr();
        aLangCode = iAuthorLangCode[index];
        aCharEncType = iAuthorCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

uint32 Mpeg4File::getNumAlbum()
{
    uint32 numAlbum = 0;
    numAlbum = getNumAssetInfoAlbumAtoms();
    if (getITunesAlbum().get_size() > 0)
    {
        numAlbum++;
    }

    return numAlbum;
}

//This function populates the Album Vector with values from Asset Info, Itunes, FullMusic Atoms.
PVMFStatus Mpeg4File::populateAlbumVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    int32 numAssetInfoAlbum = getNumAssetInfoAlbumAtoms();
    numAlbum = numAssetInfoAlbum;
    for (int32 i = 0; i < numAssetInfoAlbum; i++)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoAlbumNotice(charType, i);
        albumValues.push_front(valuestring);
        iAlbumLangCode.push_front(getAssetInfoAlbumLangCode(i));
        iAlbumCharType.push_front(charType);
    }
    if (getITunesAlbum().get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesAlbum();
        albumValues.push_front(valuestring);
        iAlbumLangCode.push_front(0);
        iAlbumCharType.push_front(ORIGINAL_CHAR_TYPE_UNKNOWN);
        numAlbum++;
    }

    return PVMFSuccess;
}

// This function returns the Album based on index value, to the parser node.
PVMFStatus Mpeg4File::getAlbum(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType)
{
    if (index < albumValues.size())
    {
        aVal = albumValues[index].get_cstr();
        aLangCode = iAlbumLangCode[index];
        aCharEncType = iAlbumCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

uint32 Mpeg4File::getNumArtist()
{
    uint32 numArtist = 0;
    numArtist = getNumAssetInfoPerformerAtoms();

    if (getITunesArtist().get_size() > 0)
    {
        numArtist++;
    }
    if (getITunesAlbumArtist().get_size() > 0) //AlbumArtist
    {
        numArtist++;
    }

    return numArtist;
}


//This function populates the Artist Vector with values from Asset Info, Itunes, FullMusic Atoms.
PVMFStatus Mpeg4File::populateArtistVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    int32 numAssetInfoPerformer = getNumAssetInfoPerformerAtoms();
    numArtist = numAssetInfoPerformer;
    for (int32 i = 0; i < numAssetInfoPerformer; i++)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoPerformerNotice(charType, i);
        artistValues.push_front(valuestring);
        iArtistLangCode.push_front(getAssetInfoPerformerLangCode(i));
        iArtistCharType.push_front(charType);
    }
    if (getITunesArtist().get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesArtist();
        artistValues.push_front(valuestring);
        iArtistLangCode.push_front(0);
        iArtistCharType.push_front(ORIGINAL_CHAR_TYPE_UNKNOWN);
        numArtist++;
    }
    if (getITunesAlbumArtist().get_size() > 0) //AlbumArtist
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesAlbumArtist();
        artistValues.push_front(valuestring);
        iArtistLangCode.push_front(0);
        iArtistCharType.push_front(ORIGINAL_CHAR_TYPE_UNKNOWN);
        numArtist++;
    }

    return PVMFSuccess;
}

// This function returns the Artists based on index value, to the parser node.
PVMFStatus Mpeg4File::getArtist(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType)
{
    if (index < artistValues.size())
    {
        aVal = artistValues[index].get_cstr();
        aLangCode = iArtistLangCode[index];
        aCharEncType = iArtistCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}


uint32 Mpeg4File::getNumGenre()
{
    uint32 numGenre = 0;
    numGenre = getNumAssetInfoGenreAtoms();

    if (getITunesGnreString().get_size() > 0)
    {
        numGenre++;
    }
    int16 gnreId = getITunesGnreID();
    if ((gnreId >= 0) && gnreId <= MAX_GENRE_ID_IN_ID3_TABLE) //Total GenreIDs present in ID3 Genre table is 147
    {
        numGenre++;
    }
    return numGenre;
}

//This function populates the Genre Vector with values from Asset Info, Itunes, FullMusic Atoms.
PVMFStatus Mpeg4File::populateGenreVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    int32 numAssetInfoGenre = getNumAssetInfoGenreAtoms();
    numGenre = numAssetInfoGenre;
    for (int32 i = 0; i < numAssetInfoGenre; i++)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoGenreNotice(charType, i);
        genreValues.push_front(valuestring);
        iGenreLangCode.push_front(getAssetInfoGenreLangCode(i));
        iGenreCharType.push_front(charType);
    }
    if (getITunesGnreString().get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesGnreString();
        genreValues.push_front(valuestring);
        iGenreLangCode.push_front(0);
        iGenreCharType.push_front(ORIGINAL_CHAR_TYPE_UNKNOWN);
        numGenre++;
    }
    int16 gnreId = getITunesGnreID();
    if ((gnreId >= 0) && gnreId <= MAX_GENRE_ID_IN_ID3_TABLE) //Total GenreIDs present in ID3 Genre table is 147
    {
        oscl_wchar genreStr[256];
        oscl_UTF8ToUnicode((char*)ID3V1_GENRE[gnreId], 64, genreStr, 64);
        OSCL_wHeapString<OsclMemAllocator> valuestring(genreStr);
        genreValues.push_front(valuestring);
        iGenreLangCode.push_front(0);
        iGenreCharType.push_front(ORIGINAL_CHAR_TYPE_UNKNOWN);
        numGenre++;
    }
    return PVMFSuccess;
}

// This function returns the Genres based on index value, to the parser node.
PVMFStatus Mpeg4File::getGenre(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType)
{
    if (index < genreValues.size())
    {
        aVal = genreValues[index].get_cstr();
        aLangCode = iGenreLangCode[index];
        aCharEncType = iGenreCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}


//This function populates the Copyright Vector with values from Asset Info, Itunes and PV Proprietary Atoms.
PVMFStatus Mpeg4File::populateCopyrightVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    int32 numAssetInfoCopyright = getNumCopyRightAtoms();
    numCopyright = numAssetInfoCopyright;
    for (int32 i = 0; i < numAssetInfoCopyright; i++)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getCopyRightString(charType, i);
        copyrightValues.push_front(valuestring);
        iCopyrightLangCode.push_front(getCopyRightLanguageCode(i));
        iCopyrightCharType.push_front(charType);
    }
    if (getPVCopyright(charType).get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getPVCopyright(charType);
        copyrightValues.push_front(valuestring);
        iCopyrightLangCode.push_front(0);
        iCopyrightCharType.push_front(charType);
        numCopyright++;
    }
    if (getITunesCopyright().get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesCopyright();
        copyrightValues.push_front(valuestring);
        iCopyrightLangCode.push_front(0);
        iCopyrightCharType.push_front(ORIGINAL_CHAR_TYPE_UNKNOWN);
        numCopyright++;
    }
    return PVMFSuccess;
}

// This function returns the Copyrights based on index value, to the parser node.
PVMFStatus Mpeg4File::getCopyright(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType)
{
    if (index < copyrightValues.size())
    {
        aVal = copyrightValues[index].get_cstr();
        aLangCode = iCopyrightLangCode[index];
        aCharEncType = iCopyrightCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

//This function populates the Comment Vector with values from Itunes, FullMusic Atoms.
PVMFStatus Mpeg4File::populateCommentVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    numComment = 0;

    if (getITunesComment().get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesComment();
        commentValues.push_front(valuestring);
        iCommentLangCode.push_front(0);
        iCommentCharType.push_front(charType);
        numComment++;
    }
    return PVMFSuccess;
}

// This function returns the Comments based on index value, to the parser node.
PVMFStatus Mpeg4File::getComment(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc& aCharEncType)
{
    if (index < commentValues.size())
    {
        aVal = commentValues[index].get_cstr();
        aLangCode = iCommentLangCode[index];
        aCharEncType = iCommentCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

//This function populates the Description Vector with values from Asset Info, Itunes and PV Proprietary Atoms.
PVMFStatus Mpeg4File::populateDescriptionVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    int32 numAssetInfoDescription = getNumAssetInfoDescAtoms();
    numDescription = numAssetInfoDescription;
    for (int32 i = 0; i < numAssetInfoDescription; i++)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoDescNotice(charType, i);
        descriptionValues.push_front(valuestring);
        iDescriptionLangCode.push_front(getAssetInfoDescLangCode(i));
        iDescriptionCharType.push_front(charType);
    }
    if (getPVDescription(charType).get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getPVDescription(charType);
        descriptionValues.push_front(valuestring);
        iDescriptionLangCode.push_front(0);
        iDescriptionCharType.push_front(charType);
        numDescription++;
    }

    if (getITunesDescription().get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesDescription();
        descriptionValues.push_front(valuestring);
        iDescriptionLangCode.push_front(0);
        iDescriptionCharType.push_front(ORIGINAL_CHAR_TYPE_UNKNOWN);
        numDescription++;
    }
    return PVMFSuccess;
}

// This function returns the Descriptions based on index value, to the parser node.
PVMFStatus Mpeg4File::getDescription(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc&
                                     aCharEncType)
{
    if (index < descriptionValues.size())
    {
        aVal = descriptionValues[index].get_cstr();
        aLangCode = iDescriptionLangCode[index];
        aCharEncType = iDescriptionCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

//This function populates the Rating Vector with values from Asset Info and PV Proprietary Atoms.
PVMFStatus Mpeg4File::populateRatingVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    int32 numAssetInfoRating = getNumAssetInfoRatingAtoms();
    numRating = numAssetInfoRating;
    if (numAssetInfoRating > 0)
    {
        for (int32 i = 0; i < numAssetInfoRating; i++)
        {
            OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoRatingNotice(charType, i);
            ratingValues.push_front(valuestring);
            iRatingLangCode.push_front(getAssetInfoRatingLangCode(i));
            iRatingCharType.push_front(charType);
        }
    }
    if (getPVRating(charType).get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getPVRating(charType);
        ratingValues.push_front(valuestring);
        iRatingLangCode.push_front(0);
        iRatingCharType.push_front(charType);
        numRating++;
    }
    return PVMFSuccess;
}

// This function returns the Ratings based on index value, to the parser node.
PVMFStatus Mpeg4File::getRating(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc&
                                aCharEncType)
{
    if (index < ratingValues.size())
    {
        aVal = ratingValues[index].get_cstr();
        aLangCode = iRatingLangCode[index];
        aCharEncType = iRatingCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

//This function populates the Date Vector
PVMFStatus Mpeg4File::populateDateVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    numDate = 0;
    const OSCL_wHeapString<OsclMemAllocator> creationDate = getCreationDate(charType);
    if (creationDate.get_size() > 0)
    {
        dateValues.push_front(creationDate);
        iDateCharType.push_front(charType);
        numDate++;
    }
    return PVMFSuccess;
}

// This function returns the Dates based on index value, to the parser node.
PVMFStatus Mpeg4File::getDate(uint32 index, OSCL_wString& aVal, MP4FFParserOriginalCharEnc&
                              aCharEncType)
{
    if (index < dateValues.size())
    {
        aVal = dateValues[index].get_cstr();
        aCharEncType = iDateCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

//This function populates the Lyricist Vector
PVMFStatus Mpeg4File::populateLyricistVector()
{
    numLyricist = 0;
    return PVMFSuccess;
}

// This function returns the Lyricists based on index value, to the parser node.
PVMFStatus Mpeg4File::getLyricist(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc&
                                  aCharEncType)
{
    if (index < lyricistValues.size())
    {
        aVal = lyricistValues[index].get_cstr();
        aLangCode = iLyricistLangCode[index];
        aCharEncType = iLyricistCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

//This function populates the Composer Vector
PVMFStatus Mpeg4File::populateComposerVector()
{
    numComposer = 0;
    return PVMFSuccess;
}

// This function returns the Composers based on index value, to the parser node.
PVMFStatus Mpeg4File::getComposer(uint32 index, OSCL_wString& aVal, uint16& aLangCode, MP4FFParserOriginalCharEnc&
                                  aCharEncType)
{
    if (index < composerValues.size())
    {
        aVal = composerValues[index].get_cstr();
        aLangCode = iComposerLangCode[index];
        aCharEncType = iComposerCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

//This function populates the Version Vector
PVMFStatus Mpeg4File::populateVersionVector()
{
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    numVersion = 0;
    if (getPVVersion(charType).get_size() > 0)
    {
        OSCL_wHeapString<OsclMemAllocator> valuestring = getPVVersion(charType);
        versionValues.push_front(valuestring);
        iVersionCharType.push_front(charType);
        numVersion++;
    }
    return PVMFSuccess;
}

// This function returns the Versions based on index value, to the parser node.
PVMFStatus Mpeg4File::getVersion(uint32 index, OSCL_wString& aVal, MP4FFParserOriginalCharEnc&
                                 aCharEncType)
{
    if (index < versionValues.size())
    {
        aVal = versionValues[index].get_cstr();
        aCharEncType = iVersionCharType[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

uint32 Mpeg4File::getNumYear()
{
    uint32 numYear = 0;
    numYear = getNumAssetInfoRecordingYearAtoms();
    if (getITunesYear().get_size() > 0)
    {
        numYear++;
    }
    return numYear;
}


//This function populates the Year Vector with values from Asset Info, Itunes, FullMusic and PV Proprietary Atoms.
PVMFStatus Mpeg4File::populateYearVector()
{
    int32 numAssetInfoRecordingYear = getNumAssetInfoRecordingYearAtoms();
    numYear = numAssetInfoRecordingYear;
    for (int32 i = 0; i < numAssetInfoRecordingYear; i++)
    {
        uint16 valuestring = getAssetInfoRecordingYear(i);
        yearValues.push_front(valuestring);
    }
    if (getITunesYear().get_size() > 0)
    {
        uint32 value, i;
        OSCL_wHeapString<OsclMemAllocator> values1 = getITunesYear();
        char valuestring[256];
        oscl_UnicodeToUTF8(values1.get_cstr(), values1.get_size(), valuestring, 256);
        i = PV_atoi(valuestring, 'd', value);
        yearValues.push_front(value);
        numYear++;
    }
    return PVMFSuccess;
}

// This function returns the Years based on index value, to the parser node.
PVMFStatus Mpeg4File::getYear(uint32 index, uint32& aVal)
{
    if (index < yearValues.size())
    {
        aVal = yearValues[index];
        return PVMFSuccess;
    }
    return PVMFErrArgument;
}

OSCL_wString& Mpeg4File::getPVTitle(MP4FFParserOriginalCharEnc &charType)
{
    PVUserDataAtom *patom = NULL;
    if (_puserDataAtom != NULL)
    {
        patom =
            (PVUserDataAtom*) _puserDataAtom->getAtomOfType(FourCharConstToUint32('p', 'v', 'm', 'm'));
    }
    else
    {
        return _emptyString;
    }

    if (patom != NULL)
    {
        return patom->getPVTitle(charType);
    }
    else
    {
        return _emptyString;
    }
}

OSCL_wString& Mpeg4File::getPVAuthor(MP4FFParserOriginalCharEnc &charType)
{
    PVUserDataAtom *patom = NULL;
    if (_puserDataAtom != NULL)
    {
        patom =
            (PVUserDataAtom*) _puserDataAtom->getAtomOfType(FourCharConstToUint32('p', 'v', 'm', 'm'));
    }
    else
    {
        return _emptyString;
    }

    if (patom != NULL)
    {
        return patom->getPVAuthor(charType);
    }
    else
    {
        return _emptyString;
    }
}

OSCL_wString& Mpeg4File::getPVVersion(MP4FFParserOriginalCharEnc &charType)
{
    OSCL_UNUSED_ARG(charType);

    PVUserDataAtom *patom = NULL;
    if (_puserDataAtom != NULL)
    {
        patom =
            (PVUserDataAtom*) _puserDataAtom->getAtomOfType(FourCharConstToUint32('p', 'v', 'm', 'm'));
    }
    else
    {
        return _emptyString;
    }

    if (patom != NULL)
    {
        return patom->getPVVersion();
    }
    else
    {
        return _emptyString;
    }
}

OSCL_wString& Mpeg4File::getPVCopyright(MP4FFParserOriginalCharEnc &charType)
{
    PVUserDataAtom *patom = NULL;
    if (_puserDataAtom != NULL)
    {
        patom =
            (PVUserDataAtom*) _puserDataAtom->getAtomOfType(FourCharConstToUint32('p', 'v', 'm', 'm'));
    }
    else
    {
        return _emptyString;
    }

    if (patom != NULL)
    {
        return patom->getPVCopyright(charType);
    }
    else
    {
        return _emptyString;
    }
}

OSCL_wString& Mpeg4File::getPVDescription(MP4FFParserOriginalCharEnc &charType)
{
    PVUserDataAtom *patom = NULL;
    if (_puserDataAtom != NULL)
    {
        patom =
            (PVUserDataAtom*) _puserDataAtom->getAtomOfType(FourCharConstToUint32('p', 'v', 'm', 'm'));
    }
    else
    {
        return _emptyString;
    }

    if (patom != NULL)
    {
        return patom->getPVDescription(charType);
    }
    else
    {
        return _emptyString;
    }
}

OSCL_wString& Mpeg4File::getPVRating(MP4FFParserOriginalCharEnc &charType)
{
    PVUserDataAtom *patom = NULL;
    if (_puserDataAtom != NULL)
    {
        patom =
            (PVUserDataAtom*) _puserDataAtom->getAtomOfType(FourCharConstToUint32('p', 'v', 'm', 'm'));
    }
    else
    {
        return _emptyString;
    }

    if (patom != NULL)
    {
        return patom->getPVRating(charType);
    }
    else
    {
        return _emptyString;
    }
}

const OSCL_wString& Mpeg4File::getCreationDate(MP4FFParserOriginalCharEnc &charType)
{
    PVUserDataAtom *patom = NULL;
    if (_puserDataAtom != NULL)
    {
        patom =
            (PVUserDataAtom*) _puserDataAtom->getAtomOfType(FourCharConstToUint32('p', 'v', 'm', 'm'));
        if (patom != NULL)
        {
            return patom->getPVCreationDate(charType);
        }
        else
        {
            return _emptyString;
        }
    }
    else if (_pmovieAtom != NULL)
    {
        return (_pmovieAtom->getCreationDate());
    }
    else
    {
        return _emptyString;
    }
}

// Destructor
Mpeg4File::~Mpeg4File()
{

    uint32 i = 0;
    PV_MP4_FF_DELETE(NULL, TrackEncryptionBoxContainer, ipTrckEncryptnBxCntr);
    // Clean up atoms
    if (_pmovieAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, MovieAtom, _pmovieAtom);
    }

    //Delete all the track atoms in the vec
    for (i = 0; i < _pTrackAtomVec->size(); i++)
    {
        PV_MP4_FF_DELETE(NULL, TrackAtom, (*_pTrackAtomVec)[i]);
    }

    // Delete the vectors themselves
    PV_MP4_FF_TEMPLATED_DELETE(NULL, trackAtomVecType, Oscl_Vector, _pTrackAtomVec);

    if (_moofFragmentIdx != NULL)
    {
        for (i = 0; i < _moofFragmentIdx->size(); i++)
        {
            PV_MP4_FF_DELETE(NULL, MOOFIndex , (*_moofFragmentIdx)[i]);
            (*_moofFragmentIdx)[i] = NULL;

        }
        PV_MP4_FF_TEMPLATED_DELETE(NULL, TrackIndexVecType, Oscl_Vector, _moofFragmentIdx);

        _moofFragmentIdx = NULL;
    }
    titleValues.destroy();
    iTitleLangCode.destroy();
    iTitleCharType.destroy();

    authorValues.destroy();
    iAuthorLangCode.destroy();
    iAuthorCharType.destroy();

    albumValues.destroy();
    iAlbumLangCode.destroy();
    iAlbumCharType.destroy();

    artistValues.destroy();
    iArtistLangCode.destroy();
    iArtistCharType.destroy();

    genreValues.destroy();
    iGenreLangCode.destroy();
    iGenreCharType.destroy();

    yearValues.destroy();

    copyrightValues.destroy();
    iCopyrightLangCode.destroy();
    iCopyrightCharType.destroy();

    commentValues.destroy();
    iCommentLangCode.destroy();
    iCommentCharType.destroy();

    descriptionValues.destroy();
    iDescriptionLangCode.destroy();
    iDescriptionCharType.destroy();

    ratingValues.destroy();
    iRatingLangCode.destroy();
    iRatingCharType.destroy()   ;

    //delete all movie fragments
    for (i = 0; i < _pMovieFragmentAtomVec->size(); i++)
    {
        PV_MP4_FF_DELETE(NULL, MovieFragmentAtom, (*_pMovieFragmentAtomVec)[i]);
    }
    PV_MP4_FF_TEMPLATED_DELETE(NULL, movieFragmentAtomVecType, Oscl_Vector, _pMovieFragmentAtomVec);
    //delete all movie fragments randomm access box
    for (i = 0; i < _pMovieFragmentRandomAccessAtomVec->size(); i++)
    {
        PV_MP4_FF_DELETE(NULL, MovieFragmentRandomAccessAtom, (*_pMovieFragmentRandomAccessAtomVec)[i]);
    }
    // Delete the vectors themselves
    PV_MP4_FF_TEMPLATED_DELETE(NULL, movieFragmentRandomAccessAtomVecType, Oscl_Vector, _pMovieFragmentRandomAccessAtomVec);

    if (_pMoofOffsetVec != NULL)
        PV_MP4_FF_TEMPLATED_DELETE(NULL, movieFragmentOffsetVecType, Oscl_Vector, _pMoofOffsetVec);


    if (_pFileSessionsForMediaDataRetrievalVec != NULL)
    {
        for (i = 0; i < _pFileSessionsForMediaDataRetrievalVec->size(); i++)
        {
            MP4_FF_FILE *temp = (*_pFileSessionsForMediaDataRetrievalVec)[i];
            if (temp->IsOpen())
            {
                AtomUtils::CloseMP4File(temp);
            }
            oscl_free(temp);
        }

        PV_MP4_FF_TEMPLATED_DELETE(NULL, moofFragmentAtomVecType, Oscl_Vector, _pFileSessionsForMediaDataRetrievalVec);
        _pFileSessionsForMediaDataRetrievalVec = NULL;
    }

    if (_pMfraOffsetAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, MfraOffsetAtom, _pMfraOffsetAtom);
    }

    if (_pTrackDurationContainer != NULL)
    {
        for (i = 0; i < _pTrackDurationContainer->_pTrackdurationInfoVec->size(); i++)
        {
            PV_MP4_FF_DELETE(NULL, TrackDurationInfo, (*_pTrackDurationContainer->_pTrackdurationInfoVec)[i]);
        }
        PV_MP4_FF_TEMPLATED_DELETE(NULL, trackDurationInfoVecType, Oscl_Vector, _pTrackDurationContainer->_pTrackdurationInfoVec);

        PV_MP4_FF_DELETE(NULL, TrackDurationContainer, _pTrackDurationContainer);
    }

    // Delete user data if present
    if (_puserDataAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, UserDataAtom, _puserDataAtom);
    }

    if (_pFileTypeAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, FileTypeAtom, _pFileTypeAtom);
    }

    if (_movieFragmentFilePtr != NULL)
    {
        if (_movieFragmentFilePtr->IsOpen())
        {
            AtomUtils::CloseMP4File(_movieFragmentFilePtr);
        }
        oscl_free(_movieFragmentFilePtr);
    }
    if (_pID3Parser)
    {
        PV_MP4_FF_DELETE(null, PVID3ParCom, _pID3Parser);
        _pID3Parser = NULL;
    }
    if (_fp != NULL)
    {
        if (_fp->IsOpen())
            AtomUtils::CloseMP4File(_fp);
        oscl_free(_fp);
    }

}


uint64 Mpeg4File::getMovieDuration() const
{
    uint64 overallMovieDuration = 0;
    uint32 id = 0;
    if (_isMovieFragmentsPresent)
    {
        overallMovieDuration = _pmovieAtom->getMovieFragmentDuration();
        if (overallMovieDuration != 0)
        {
            return overallMovieDuration;
        }
        else if (_parsing_mode == 0)
        {
            uint numTracks = _pmovieAtom->getNumTracks();
            uint32 *trackList  = (uint32 *) oscl_malloc(sizeof(uint32) * numTracks);
            if (! trackList)
                return 0;   // malloc failure
            _pmovieAtom->getTrackWholeIDList(trackList);
            uint64 prevtrackDuration = 0, trackduration = 0;
            for (uint32 i = 0; i < numTracks; i++)
            {
                TrackDurationInfo* pTrackDurationInfo = (*_pTrackDurationContainer->_pTrackdurationInfoVec)[i];
                trackduration = pTrackDurationInfo->trackDuration;
                if (prevtrackDuration > trackduration)
                {
                    trackduration = prevtrackDuration;
                }
                else
                {
                    prevtrackDuration = trackduration;
                    id = trackList[i];
                }
            }
            overallMovieDuration = trackduration;

            TrackAtom *trackAtom = NULL;
            uint32 mediaTimeScale = 0xFFFFFFFE;

            if (_pmovieAtom != NULL)
            {
                trackAtom = _pmovieAtom->getTrackForID(id);
            }
            if (trackAtom != NULL)
            {
                mediaTimeScale = trackAtom->getMediaTimescale();
                if (mediaTimeScale == 0)
                {
                    // unlikely : getMediaTimescale can return 0
                    mediaTimeScale = 0xFFFFFFFE;
                }
            }

            overallMovieDuration  = (overallMovieDuration / (uint64)mediaTimeScale) * (uint64)getMovieTimescale();
            oscl_free(trackList);
            return overallMovieDuration;
        }
        else
        {
            return overallMovieDuration;
        }
    }
    else if (_pmovieAtom != NULL)
    {
        // Get the overall duration of the Mpeg-4 presentation
        return _pmovieAtom->getDuration();
    }
    return 0;
}

uint64 Mpeg4File::getMovieFragmentDuration() const
{
    if (_pmovieAtom != NULL)
    {
        return _pmovieAtom->getMovieFragmentDuration();
    }
    else
        return 0;
}

MP4_ERROR_CODE Mpeg4File::getTimestampForSampleNumber(uint32 id, uint32 sampleNumber, uint64& aTimeStamp)
{
    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(id);

        if (trackAtom != NULL)
        {
            return trackAtom->getTimestampForSampleNumber(sampleNumber, aTimeStamp);
        }
        else
        {
            return READ_FAILED;
        }
    }
    else
    {
        return READ_FAILED;
    }
}

MP4_ERROR_CODE Mpeg4File::getSampleSizeAt(uint32 id, int32 sampleNum, uint32& aSampleSize)
{
    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(id);

        if (trackAtom != NULL)
        {
            return (trackAtom->getSampleSizeAt(sampleNum, aSampleSize));
        }
        else
        {
            return DEFAULT_ERROR;
        }
    }
    else
    {
        return DEFAULT_ERROR;
    }
}

uint64 Mpeg4File::getTrackMediaDurationForMovie(uint32 id)
{
    TrackAtom *trackAtom;
    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }
    if (trackAtom != NULL)
    {
        return trackAtom->getTrackDuration();
    }
    else
    {
        return 0;
    }

}
// From TrackHeader
uint64 Mpeg4File::getTrackDuration(uint32 id)
{
    TrackAtom *trackAtom;
    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }
    if (_isMovieFragmentsPresent)
    {
        if (_parsing_mode)
            return _pmovieAtom->getMovieFragmentDuration();
        else
        {
            int32 numTracks = _pmovieAtom->getNumTracks();
            uint32 *trackList  = (uint32 *) oscl_malloc(sizeof(uint32) * numTracks);
            if (!trackList)
                return 0;   // malloc failed
            _pmovieAtom->getTrackWholeIDList(trackList);
            uint64 trackduration = 0;
            for (int32 i = 0; i < numTracks; i++)
            {
                if (trackList[i] == id)
                {
                    TrackDurationInfo* pTrackDurationInfo = (*_pTrackDurationContainer->_pTrackdurationInfoVec)[i];
                    oscl_free(trackList);
                    return trackduration = pTrackDurationInfo->trackDuration;
                }
            }
            oscl_free(trackList);
        }
    }
    if (trackAtom != NULL)
    {
        return trackAtom->getTrackDuration();
    }
    else
    {
        return 0;
    }
}

// From TrackReference
uint32  Mpeg4File::trackDependsOn(uint32 id)
{
    TrackAtom *trackAtom;
    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->dependsOn();
    }
    else
    {
        return 0;
    }

}

// From MediaHeader
uint64 Mpeg4File::getTrackMediaDuration(uint32 id)
{
    TrackAtom *trackAtom;
    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }
    if (_isMovieFragmentsPresent)
    {
        if (_parsing_mode)
            return _pmovieAtom->getMovieFragmentDuration();
        else
        {
            int numTracks = _pmovieAtom->getNumTracks();
            uint32 *trackList  = (uint32 *) oscl_malloc(sizeof(uint32) * numTracks);
            if (!trackList)
                return 0;   // malloc failed
            _pmovieAtom->getTrackWholeIDList(trackList);
            uint64 trackduration = 0;
            for (int32 i = 0; i < numTracks; i++)
            {
                if (trackList[i] == id)
                {
                    TrackDurationInfo* pTrackDurationInfo = (*_pTrackDurationContainer->_pTrackdurationInfoVec)[i];
                    oscl_free(trackList);
                    trackduration = pTrackDurationInfo->trackDuration;
                    return trackduration;
                }
            }
            oscl_free(trackList);
        }
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getMediaDuration();
    }
    else
    {
        return 0;
    }
}

uint32 Mpeg4File::getTrackMediaTimescale(uint32 id)
{
    TrackAtom *trackAtom;
    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        // RETURN UNDEFINED VALUE
        return (0xFFFFFFFF);
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getMediaTimescale();
    }
    else
    {
        // RETURN UNDEFINED VALUE
        return (0xFFFFFFFF);
    }
}

uint16 Mpeg4File::getTrackLangCode(uint32 id)
{

    TrackAtom *trackAtom;
    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        // RETURN UNDEFINED VALUE
        return (0xFFFF);
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getLanguageCode();
    }
    else
    {
        // RETURN UNDEFINED VALUE
        return (0xFFFF);
    }
}

// From Handler
uint32 Mpeg4File::getTrackMediaType(uint32 id)
{
    TrackAtom *trackAtom;
    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        // RETURN UNDEFINED VALUE
        return (0xFFFFFFFF);
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getMediaType();
    }
    else
    {
        // RETURN UNDEFINED VALUE
        return (0xFFFFFFFF);
    }

}

// From SampleDescription
int32 Mpeg4File::getTrackNumSampleEntries(uint32 id)
{
    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getNumSampleEntries();
    }
    else
    {
        return 0;
    }
}

// From DecoderConfigDescriptor
DecoderSpecificInfo *Mpeg4File::getTrackDecoderSpecificInfo(uint32 id)
{
    TrackAtom *trackAtom;
    if (_pmovieAtom != NULL)
    {
        trackAtom = _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return NULL;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getDecoderSpecificInfo();
    }
    else
    {
        return NULL;
    }

}

// From DecoderConfigDescriptor
DecoderSpecificInfo *
Mpeg4File::getTrackDecoderSpecificInfoAtSDI(uint32 trackID, uint32 index)
{
    if (_pmovieAtom != NULL)
    {
        return (_pmovieAtom->getTrackDecoderSpecificInfoAtSDI(trackID, index));
    }
    else
    {
        return NULL;
    }
}

uint8 *Mpeg4File::getTrackDecoderSpecificInfoContent(uint32 id)
{
    DecoderSpecificInfo *decoderSpecificInfo;
    decoderSpecificInfo = getTrackDecoderSpecificInfo(id);

    if (decoderSpecificInfo != NULL)
    {
        return decoderSpecificInfo->getInfo();
    }
    else
    {
        return NULL;
    }
}

uint32 Mpeg4File::getTrackDecoderSpecificInfoSize(uint32 id)
{
    DecoderSpecificInfo *decoderSpecificInfo;
    decoderSpecificInfo = getTrackDecoderSpecificInfo(id);

    if (decoderSpecificInfo != NULL)
    {
        return decoderSpecificInfo->getInfoSize();
    }
    else
    {
        return 0;
    }
}


void Mpeg4File::getTrackMIMEType(uint32 id, OSCL_String& aMimeType) // Based on OTI value
{
    TrackAtom *trackAtom = NULL;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(id);
    }

    if (trackAtom != NULL)
    {
        trackAtom->getMIMEType(aMimeType);
    }
}


int32 Mpeg4File::getTrackMaxBufferSizeDB(uint32 id)
{
    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getMaxBufferSizeDB();
    }
    else
    {
        return 0;
    }
}

int32 Mpeg4File::getTrackAverageBitrate(uint32 id)
{
    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getAverageBitrate();
    }
    else
    {
        return 0;
    }
}

// PASP Box
//Hspacing
uint32 Mpeg4File::getHspacing(uint32 id)
{

    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getHspacing();
    }
    else
    {
        return 0;
    }
}

//Vspacing
uint32 Mpeg4File::getVspacing(uint32 id)
{

    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(id);
    }
    else
    {
        return 0;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getVspacing();
    }
    else
    {
        return 0;
    }
}


uint32
Mpeg4File::getMovieTimescale() const
{
    if (_pmovieAtom != NULL)
    {
        // Set the overall timescale of the Mpeg-4 presentation
        return _pmovieAtom->getTimeScale();
    }
    else
    {
        // RETURN UNDEFINED VALUE
        return (0xFFFFFFFF);
    }
}

/* ======================================================================== */
bool
Mpeg4File::IsMobileMP4()
{
    bool oMMP4 = false;

    if (_pFileTypeAtom != NULL)
    {
        uint32 majorBrand = _pFileTypeAtom->getMajorBrand();

        if (majorBrand != MOBILE_MP4)
        {
            Oscl_Vector<uint32, OsclMemAllocator> *_compatibleBrand =
                _pFileTypeAtom->getCompatibleBrand();
            if (_compatibleBrand != NULL)
            {
                for (uint32 i = 0; i < _compatibleBrand->size(); i++)
                {
                    uint32 brand = (*_compatibleBrand)[i];

                    if (brand == MOBILE_MP4)
                    {
                        oMMP4 = true;
                    }
                }
            }
            else
            {
                return false;
            }
        }
        else
        {
            oMMP4 = true;
        }
    }
    else
    {
        return false;
    }

    if (oMMP4 == true)
    {
        if (!(_pmovieAtom->checkMMP4()))
        {
            return false;
        }
    }

    return (oMMP4);
}

uint8
Mpeg4File::parseBufferAndGetNumAMRFrames(uint8* buffer, uint32 size)
{
    uint32 inputBufferSize = size;
    uint8* inputPtr = buffer;
    uint8 numArmFrames = 0;

    if (((int32)(size) <= 0) ||
            (buffer == NULL))
    {
        return 0;
    }

    uint8 aFrameSizes[16] = {12, 13, 15, 17, 19, 20, 26, 31,
                             5,  0,  0,  0,  0,  0,  0,  0
                            };

    while (inputBufferSize > 0)
    {
        uint8 toc_byte = *(inputPtr);

        uint8 frame_type = (uint8)((toc_byte >> 3) & 0x0F);

        inputPtr        += 1;
        inputBufferSize -= 1;

        if ((frame_type > 8) && (frame_type != 15))
        {
            return 0;
        }

        numArmFrames++;
        inputPtr        += aFrameSizes[(uint16)frame_type];
        inputBufferSize -= aFrameSizes[(uint16)frame_type];
    }
    return (numArmFrames);
}


uint32 Mpeg4File::getTrackLevelOMA2DRMInfoSize(uint32 trackID)
{
    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(trackID);
    }
    else
    {
        return 0;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getTrackLevelOMA2DRMInfoSize();
    }
    else
    {
        return 0;
    }
}

uint8* Mpeg4File::getTrackLevelOMA2DRMInfo(uint32 trackID)
{
    TrackAtom *trackAtom;

    if (_pmovieAtom != NULL)
    {
        trackAtom =  _pmovieAtom->getTrackForID(trackID);
    }
    else
    {
        return NULL;
    }

    if (trackAtom != NULL)
    {
        return trackAtom->getTrackLevelOMA2DRMInfo();
    }
    else
    {
        return NULL;
    }
}

PIFF_PROTECTION_SYSTEM Mpeg4File::GetPIFFProtectionSystem()
{
    if (_pmovieAtom != NULL)
    {
        return _pmovieAtom->GetPIFFProtectionSystem();
    }
    else
    {
        return PIFF_PROTECTION_SYSTEM_UNKNOWN;
    }
}

MP4_ERROR_CODE Mpeg4File::GetPIFFProtectionSystemSpecificData(const uint8*& aOpaqueData, uint32& aDataSize)
{
    if (_pmovieAtom != NULL)
    {
        return _pmovieAtom->GetPIFFProtectionSystemSpecificData(aOpaqueData, aDataSize);
    }
    else
    {
        return READ_FAILED;
    }
}

MP4_ERROR_CODE
Mpeg4File::RequestReadCapacityNotification(PvmiDataStreamObserver& aObserver,
        TOsclFileOffset aFileOffset,
        OsclAny* aContextData)
{
    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "Mpeg4File::RequestReadCapacityNotification In Offset %d", aFileOffset));
    TOsclFileOffset capacity = 0;
    if (_commonFilePtr != NULL)
    {
        TOsclFileOffset currSize = 0;
        GetCurrentFileSize(currSize);
        if (aFileOffset > currSize)
        {
            TOsclFileOffset currPos = AtomUtils::getCurrentFilePosition(_commonFilePtr);
            if (aFileOffset > currPos)
            {
                capacity = (aFileOffset - currPos);
                bool retVal =
                    _commonFilePtr->_pvfile.RequestReadCapacityNotification(aObserver,
                            capacity,
                            aContextData);
                if (retVal)
                {
                    return EVERYTHING_FINE;
                }
                else
                {
                    return DEFAULT_ERROR;
                }
            }
        }
        return SUFFICIENT_DATA_IN_FILE;
    }
    return DEFAULT_ERROR;
}


MP4_ERROR_CODE
Mpeg4File::GetCurrentFileSize(TOsclFileOffset& aFileSize)
{
    aFileSize = 0;
    if (AtomUtils::getCurrentFileSize(_commonFilePtr, aFileSize) == true)
    {
        return EVERYTHING_FINE;
    }
    if (_commonFilePtr == NULL && _fileSize != 0)
    {
        aFileSize = _fileSize;
        return EVERYTHING_FINE;
    }
    return DEFAULT_ERROR;
}

int32 Mpeg4File::getNextBundledAccessUnits(const uint32 trackID,
        uint32 *n,
        GAU    *pgau,
        Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>*  apSampleDecryptionInfoVect)
{
    uint32 samplesTobeRead;
    samplesTobeRead = *n;
    uint32 totalSampleRead = 0;
    if (getNumTracks() == 0)
    {
        return -1;
    }
    uint32 moofIdx = getIndexForTrackID(trackID);

    if (_pmovieAtom != NULL)
    {
        int32 ret = _pmovieAtom->getNextBundledAccessUnits(trackID, n, pgau);

        if (ret == END_OF_TRACK)
        {
            if (!_isMovieFragmentsPresent)
                return ret;

            totalSampleRead += *n;
            bool oAllMoofExhausted = false;
            bool oAllMoofParsed = false;

            if (_parsing_mode == 0)
            {
                if (_pMovieFragmentAtomVec != NULL && _isMovieFragmentsPresent)
                {
                    if (samplesTobeRead >= *n)
                        *n = samplesTobeRead - *n;
                }
                else
                    return ret;

                int32 return1 = 0;
                while (_movieFragmentIdx[moofIdx] < _pMovieFragmentAtomVec->size())
                {
                    uint32 movieFragmentIdx = _movieFragmentIdx[moofIdx];
                    MovieFragmentAtom *pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[movieFragmentIdx];
                    if (pMovieFragmentAtom != NULL)
                    {
                        if ((uint32)pMovieFragmentAtom->getSequenceNumber() == _movieFragmentSeqIdx[moofIdx])
                        {
                            TrackFragmentAtom *trackfragment = pMovieFragmentAtom->getTrackFragmentforID(trackID);
                            if (trackfragment != NULL)
                            {
                                if (trackfragment->getTrackId() == trackID)
                                {
                                    return1 = pMovieFragmentAtom->getNextBundledAccessUnits(trackID, n, totalSampleRead, pgau, apSampleDecryptionInfoVect);
                                    totalSampleRead += *n;
                                    if (return1 != END_OF_TRACK)
                                    {
                                        *n = totalSampleRead;
                                        return return1;
                                    }
                                    else
                                    {
                                        if (samplesTobeRead >= *n)
                                        {
                                            samplesTobeRead = samplesTobeRead - *n;
                                            *n = samplesTobeRead;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    _movieFragmentIdx[moofIdx]++;
                    _movieFragmentSeqIdx[moofIdx]++;
                }
                if (return1 == END_OF_TRACK)
                {
                    *n = totalSampleRead;
                    _movieFragmentIdx[moofIdx] = 0;
                    return return1;
                }
            }
            else
            {
                int32 return1 = 0;
                while (!oAllMoofExhausted)
                {
                    if (oAllMoofParsed && (_pMovieFragmentAtomVec->size() < _movieFragmentIdx[moofIdx]))
                    {
                        oAllMoofExhausted = true;
                        *n = 0;
                        break;
                    }

                    while (!oAllMoofParsed)
                    {
                        if (moofParsingCompleted | isResetPlayBackCalled)
                        {
                            uint32 moofIndex = 0;
                            bool moofToBeParsed = false;
                            if (_pMovieFragmentAtomVec->size() > _movieFragmentIdx[moofIdx])
                            {
                                MovieFragmentAtom *pMovieFragmentAtom = NULL;
                                uint32 idx = _movieFragmentIdx[moofIdx];
                                pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[_movieFragmentIdx[moofIdx]];
                                if (pMovieFragmentAtom == NULL)
                                {
                                    isResetPlayBackCalled = true;
                                    moofToBeParsed = true;
                                    moofIndex = _movieFragmentIdx[moofIdx];
                                }
                                else if (isResetPlayBackCalled)
                                {
                                    uint32 seqNum = pMovieFragmentAtom->getSequenceNumber();
                                    if (seqNum == _movieFragmentSeqIdx[moofIdx])
                                    {
                                        TrackFragmentAtom *trackfragment = pMovieFragmentAtom->getTrackFragmentforID(trackID);
                                        if (trackfragment != NULL)
                                        {
                                            if (trackfragment->getTrackId() == trackID)
                                            {

                                                if (return1 != END_OF_TRACK)
                                                {
                                                    if (samplesTobeRead >= *n)
                                                        *n = samplesTobeRead - *n;
                                                }
                                                return1 = pMovieFragmentAtom->getNextBundledAccessUnits(trackID, n, totalSampleRead, pgau, apSampleDecryptionInfoVect);
                                                totalSampleRead += *n;
                                                if (return1 != END_OF_TRACK)
                                                {
                                                    *n = totalSampleRead;
                                                    return return1;
                                                }
                                            }
                                        }
                                        _movieFragmentSeqIdx[moofIdx]++;
                                        if (samplesTobeRead >= *n)
                                        {
                                            samplesTobeRead = samplesTobeRead - *n;
                                            *n = samplesTobeRead;
                                            if (_movieFragmentIdx[moofIdx] < _pMovieFragmentAtomVec->size())
                                            {
                                                _movieFragmentIdx[moofIdx]++;

                                                if (_pMovieFragmentAtomVec->size() == _movieFragmentIdx[moofIdx])
                                                {
                                                    uint32 i = idx + 1;
                                                    while (i < _pMovieFragmentAtomVec->size())
                                                    {
                                                        idx++;
                                                        i++;
                                                    }
                                                    TOsclFileOffset moof_start_offset = (*_pMoofOffsetVec)[idx];
                                                    AtomUtils::seekFromStart(_movieFragmentFilePtr, moof_start_offset);
                                                    uint32 atomType = UNKNOWN_ATOM;
                                                    uint32 atomSize = 0;
                                                    AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);
                                                    if (atomType == MOVIE_FRAGMENT_ATOM)
                                                    {
                                                        atomSize -= DEFAULT_ATOM_SIZE;
                                                        AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                                        _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                                    }

                                                }


                                            }
                                        }
                                        continue;
                                    }
                                    else
                                    {
                                        isResetPlayBackCalled = false;
                                    }

                                    // if moofs are already parsed, so go to the end of MOOF Vector.

                                    TOsclFileOffset currFilePos = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                    if (currFilePos < _ptrMoofEnds)
                                    {
                                        TOsclFileOffset offset = (_ptrMoofEnds - currFilePos);
                                        AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, offset);
                                    }
                                    else if (currFilePos == _ptrMoofEnds)
                                    {
                                        // no need to seek the File Pointer
                                    }
                                    else
                                    {
                                        AtomUtils::seekFromStart(_movieFragmentFilePtr, _ptrMoofEnds);
                                    }

                                    uint32 i = idx + 1;
                                    while (i < _pMovieFragmentAtomVec->size())
                                    {
                                        idx++;
                                        pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[idx];
                                        if (pMovieFragmentAtom == NULL)
                                        {
                                            //isResetPlayBackCalled = true;
                                            moofToBeParsed = true;
                                            moofIndex = idx;
                                            break;
                                        }
                                        pMovieFragmentAtom->resetPlayback();
                                        i++;
                                    }

                                    TOsclFileOffset moof_start_offset = (*_pMoofOffsetVec)[_movieFragmentIdx[moofIdx]];
                                    uint32 bufCap = AtomUtils::getFileBufferingCapacity(_movieFragmentFilePtr);
                                    if (bufCap)
                                    {
                                        TOsclFileOffset bufStart = 0, bufEnd = 0;
                                        _movieFragmentFilePtr->_pvfile.GetCurrentByteRange(bufStart, bufEnd);
                                        if ((moof_start_offset < bufStart) || ((moof_start_offset + DEFAULT_ATOM_SIZE) > bufEnd))
                                        {
                                            return INSUFFICIENT_DATA;
                                        }
                                    }
                                    AtomUtils::seekFromStart(_movieFragmentFilePtr, moof_start_offset);
                                    uint32 atomType = UNKNOWN_ATOM;
                                    uint32 atomSize = 0;

                                    AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);

                                    if (atomType == MOVIE_FRAGMENT_ATOM)
                                    {
                                        atomSize -= DEFAULT_ATOM_SIZE;
                                        AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                        _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                    }
                                }
                            }
                            else
                            {
                                isResetPlayBackCalled = false;
                            }


                            TOsclFileOffset fileSize = 0;
                            AtomUtils::getCurrentFileSize(_movieFragmentFilePtr, fileSize);
                            TOsclFileOffset currFilePos = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                            if (currFilePos < _ptrMoofEnds)
                            {
                                TOsclFileOffset offset = (_ptrMoofEnds - currFilePos);
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, offset);
                            }
                            else
                            {
                                AtomUtils::seekFromStart(_movieFragmentFilePtr, _ptrMoofEnds);
                            }
                            TOsclFileOffset filePointer = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                            TOsclFileOffset count = (fileSize - filePointer);// -DEFAULT_ATOM_SIZE;
                            while (count > 0)
                            {
                                TOsclFileOffset currPos = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                uint32 bufCap = AtomUtils::getFileBufferingCapacity(_movieFragmentFilePtr);
                                if (bufCap)
                                {
                                    TOsclFileOffset bufStart = 0, bufEnd = 0;
                                    _movieFragmentFilePtr->_pvfile.GetCurrentByteRange(bufStart, bufEnd);
                                    fileSize = bufEnd + 1;
                                    if ((currPos < bufStart) || ((currPos + DEFAULT_ATOM_SIZE) > bufEnd))
                                    {
                                        return INSUFFICIENT_DATA;
                                    }
                                }
                                uint32 atomType = UNKNOWN_ATOM;
                                uint32 atomSize = 0;

                                AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);

                                if ((currPos + (TOsclFileOffset)atomSize) > fileSize)
                                {
                                    AtomUtils::seekFromStart(_movieFragmentFilePtr, currPos);
                                    if (_movieFragmentIdx[moofIdx] < _pMovieFragmentAtomVec->size())
                                    {
                                        // dont report insufficient data as we still have a moof/moofs to
                                        // retrieve data. So just go and retrieve the data.

                                        break;
                                    }
                                    else
                                    {
                                        // We have run out of MOOF atoms so report insufficient data.
                                        if (totalSampleRead == 0)
                                        {
                                            return INSUFFICIENT_DATA;
                                        }
                                        else
                                        {
                                            return EVERYTHING_FINE;
                                        }
                                    }
                                }


                                if (atomType == MOVIE_FRAGMENT_ATOM)
                                {
                                    TOsclFileOffset moofStartOffset = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                    moofStartOffset -= DEFAULT_ATOM_SIZE;
                                    moofSize = atomSize;
                                    moofType = atomType;
                                    moofCount = count;
                                    _ptrMoofEnds = moofStartOffset + atomSize;

                                    PV_MP4_FF_NEW(_movieFragmentFilePtr->auditCB, MovieFragmentAtom, (_movieFragmentFilePtr, _pFileSessionsForMediaDataRetrievalVec, atomSize, atomType, _pTrackDurationContainer, _pTrackExtendsAtomVec, parseMoofCompletely, moofParsingCompleted, countOfTrunsParsed, ipTrckEncryptnBxCntr), _pMovieFragmentAtom);

                                    moofSize = atomSize;
                                    moofPtrPos = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);

                                    currMoofNum = _pMovieFragmentAtom->getSequenceNumber();
                                    if (moofToBeParsed)
                                    {
                                        (*_pMovieFragmentAtomVec)[moofIndex] = _pMovieFragmentAtom;
                                        (*_pMoofOffsetVec)[moofIndex] = moofStartOffset;
                                    }
                                    else
                                    {
                                        _pMoofOffsetVec->push_back(moofStartOffset);
                                        _pMovieFragmentAtomVec->push_back(_pMovieFragmentAtom);


                                    }

                                    if (moofParsingCompleted)
                                    {
                                        if (!_pMovieFragmentAtom->MP4Success())
                                        {
                                            _success = false;
                                            _mp4ErrorCode = _pMovieFragmentAtom->GetMP4Error();
                                            oAllMoofExhausted = true;
                                            break;
                                        }
                                        _pMovieFragmentAtom->setParent(this);
                                        count -= _pMovieFragmentAtom->getSize();
                                        break;
                                    }

                                    break;
                                }
                                else if (atomType == MEDIA_DATA_ATOM)
                                {
                                    if (atomSize == 1)
                                    {
                                        uint64 largeSize = 0;
                                        AtomUtils::read64(_movieFragmentFilePtr, largeSize);
                                        uint32 size =
                                            Oscl_Int64_Utils::get_uint64_lower32(largeSize);
                                        count -= size;
                                        size -= 8; //for large size
                                        atomSize = size;
                                    }
                                    else
                                    {
                                        if (atomSize < DEFAULT_ATOM_SIZE)
                                        {
                                            oAllMoofExhausted = true;
                                            break;
                                        }
                                        if (count < (int32)atomSize)
                                        {
                                            ret = INSUFFICIENT_DATA;
                                            oAllMoofExhausted = true;
                                            AtomUtils::seekFromStart(_movieFragmentFilePtr, currPos);
                                            break;
                                        }
                                        count -= atomSize;
                                    }
                                    atomSize -= DEFAULT_ATOM_SIZE;
                                    AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                }
                                else
                                {
                                    if (count > 0)
                                    {
                                        count -= atomSize;
                                        atomSize -= DEFAULT_ATOM_SIZE;
                                        AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                    }
                                }
                            }
                            if (count <= 0)
                            {
                                oAllMoofParsed = true;
                                break;
                            }
                            break;
                        }
                        else
                        {
                            if (currMoofNum != (uint32) _pMovieFragmentAtom->getSequenceNumber())
                            {
                                _pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[_pMovieFragmentAtomVec->size() - 1];
                            }

                            TOsclFileOffset currPos = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                            if (currPos > moofPtrPos)
                            {
                                TOsclFileOffset offset = (currPos - moofPtrPos);
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, - offset);
                            }
                            else
                            {
                                TOsclFileOffset offset = (moofPtrPos - currPos);
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, offset);
                            }

                            TOsclFileOffset filePointer = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                            uint32 bufCap = AtomUtils::getFileBufferingCapacity(_movieFragmentFilePtr);
                            if (bufCap)
                            {
                                TOsclFileOffset bufStart = 0, bufEnd = 0;
                                _movieFragmentFilePtr->_pvfile.GetCurrentByteRange(bufStart, bufEnd);
                                if ((filePointer < bufStart) || ((TOsclFileOffset)(filePointer + moofSize) > bufEnd))
                                {
                                    return INSUFFICIENT_DATA;
                                }
                            }
                            _pMovieFragmentAtom->ParseMoofAtom(_movieFragmentFilePtr, moofSize, moofType, _pTrackDurationContainer, _pTrackExtendsAtomVec, moofParsingCompleted, countOfTrunsParsed, trackID);

                            moofPtrPos = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                            if (moofParsingCompleted)
                            {
                                if (!_pMovieFragmentAtom->MP4Success())
                                {
                                    oAllMoofExhausted = true;
                                    break;
                                }
                                _pMovieFragmentAtom->setParent(this);
                                moofCount -= _pMovieFragmentAtom->getSize();
                            }

                            if (currPos > moofPtrPos)
                            {
                                TOsclFileOffset offset = (currPos - moofPtrPos);
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, offset);
                            }
                            else
                            {
                                TOsclFileOffset offset = (moofPtrPos - currPos);
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, -offset);
                            }

                            if (moofCount <= 0)
                            {
                                oAllMoofParsed = true;
                                break;
                            }
                            break;
                        }
                    }

                    if (return1 != END_OF_TRACK)
                    {
                        if (samplesTobeRead >= *n)
                            *n = samplesTobeRead - *n;
                    }

                    uint32 movieFragmentIdx = _movieFragmentIdx[moofIdx];
                    MovieFragmentAtom *pMovieFragmentAtom = NULL;

                    if (movieFragmentIdx < _pMovieFragmentAtomVec->size())
                    {
                        pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[movieFragmentIdx];
                    }
                    if (pMovieFragmentAtom != NULL)
                    {
                        uint32 seqNum = pMovieFragmentAtom->getSequenceNumber();
                        if (seqNum == _movieFragmentSeqIdx[moofIdx])
                        {
                            TrackFragmentAtom *trackfragment = pMovieFragmentAtom->getTrackFragmentforID(trackID);
                            if (trackfragment != NULL)
                            {
                                if (trackfragment->getTrackId() == trackID)
                                {
                                    return1 = pMovieFragmentAtom->getNextBundledAccessUnits(trackID, n, totalSampleRead, pgau, apSampleDecryptionInfoVect);
                                    totalSampleRead += *n;
                                    if (return1 != END_OF_TRACK)
                                    {
                                        *n = totalSampleRead;
                                        return return1;
                                    }
                                    else
                                    {
                                        _movieFragmentSeqIdx[moofIdx]++;
                                        if (samplesTobeRead >= *n)
                                        {
                                            samplesTobeRead = samplesTobeRead - *n;
                                            *n = samplesTobeRead;
                                            if (movieFragmentIdx < _pMovieFragmentAtomVec->size())
                                            {
                                                _movieFragmentIdx[moofIdx]++;
                                            }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                uint32 movieFragmentIdx2 = _movieFragmentIdx[moofIdx];

                                if (oAllMoofParsed)
                                {
                                    // look for all moofs
                                    if (movieFragmentIdx2 < _pMovieFragmentAtomVec->size())
                                    {
                                        _movieFragmentIdx[moofIdx]++;
                                        _movieFragmentSeqIdx[moofIdx]++;
                                    }
                                    else
                                    {
                                        if (!MoreMoofAtomsExpected(_movieFragmentSeqIdx[moofIdx]))
                                        {
                                            return END_OF_TRACK;
                                        }
                                        else
                                        {
                                            //We have run out of moofs and theres no data to be parsed
                                            *n = totalSampleRead;
                                            if (totalSampleRead == 0)
                                            {
                                                return INSUFFICIENT_DATA;
                                            }
                                            else
                                            {
                                                return EVERYTHING_FINE;
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    if (movieFragmentIdx2 < _pMovieFragmentAtomVec->size())
                                    {

                                        if ((movieFragmentIdx2 == (_pMovieFragmentAtomVec->size() - 1)) && moofParsingCompleted)
                                        {
                                            _movieFragmentIdx[moofIdx]++;
                                            _movieFragmentSeqIdx[moofIdx]++;
                                        }
                                        else if (movieFragmentIdx2 < (_pMovieFragmentAtomVec->size() - 1))
                                        {
                                            _movieFragmentIdx[moofIdx]++;
                                            _movieFragmentSeqIdx[moofIdx]++;

                                            *n = 0;
                                            return NO_SAMPLE_IN_CURRENT_MOOF;
                                        }


                                    }
                                }
                            }
                        }
                        else
                        {
                            uint32 movieFragmentIdx2 = _movieFragmentIdx[moofIdx];

                            if (oAllMoofParsed)
                            {
                                // look for all moofs
                                if (movieFragmentIdx2 < _pMovieFragmentAtomVec->size())
                                {
                                    _movieFragmentIdx[moofIdx]++;
                                    _movieFragmentSeqIdx[moofIdx]++;
                                }
                                else
                                {
                                    if (MoreMoofAtomsExpected(_movieFragmentSeqIdx[moofIdx]))
                                    {
                                        //We have run out of moofs and theres no data to be parsed
                                        *n = totalSampleRead;
                                        if (totalSampleRead == 0)
                                        {
                                            return INSUFFICIENT_DATA;
                                        }
                                        else
                                        {
                                            return EVERYTHING_FINE;
                                        }
                                    }
                                    else
                                    {
                                        return END_OF_TRACK;
                                    }
                                }

                            }
                            else
                            {
                                if (movieFragmentIdx2 < _pMovieFragmentAtomVec->size())
                                {
                                    if ((movieFragmentIdx2 == (_pMovieFragmentAtomVec->size() - 1)) && moofParsingCompleted)
                                    {
                                        _movieFragmentIdx[moofIdx]++;
                                        _movieFragmentSeqIdx[moofIdx]++;
                                    }
                                    else if (movieFragmentIdx2 < (_pMovieFragmentAtomVec->size() - 1))
                                    {
                                        _movieFragmentIdx[moofIdx]++;
                                        _movieFragmentSeqIdx[moofIdx]++;
                                        *n = 0;
                                        return NO_SAMPLE_IN_CURRENT_MOOF;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if (movieFragmentIdx < _pMovieFragmentAtomVec->size())
                        {
                            _movieFragmentIdx[moofIdx]++;
                            _movieFragmentSeqIdx[moofIdx]++;
                        }
                        else if (oAllMoofParsed)
                        {

                            if (MoreMoofAtomsExpected(_movieFragmentSeqIdx[moofIdx]))
                            {
                                //We have run out of moofs and theres no data to be parsed
                                *n = totalSampleRead;
                                if (totalSampleRead == 0)
                                {
                                    return INSUFFICIENT_DATA;
                                }
                                else
                                {
                                    return EVERYTHING_FINE;
                                }
                            }
                            else
                            {
                                PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "Mpeg4File::getNextBundledAccessUnits return END_OF_TRACK"));
                                return END_OF_TRACK;
                            }
                        }

                    }
                }
            }
        }
        if (END_OF_TRACK == ret)
        {
            if (MoreMoofAtomsExpected(_movieFragmentSeqIdx[moofIdx]))
            {
                //We have run out of moofs and theres no data to be parsed
                *n = totalSampleRead;
                if (totalSampleRead == 0)
                {
                    ret = INSUFFICIENT_DATA;
                }
                else
                {
                    ret = EVERYTHING_FINE;
                }
            }
            else
            {
                ret = END_OF_TRACK;
            }
        }
        return ret;
    }
    return -1;
}

MovieFragmentAtom * Mpeg4File::getMovieFragmentForTrackId(uint32 id)
{
    MovieFragmentAtom *movieFragmentAtom = NULL;
    uint32 i = 0;

    if (_pMovieFragmentAtomVec == NULL)
        return NULL;

    while (i < _pMovieFragmentAtomVec->size())
    {
        movieFragmentAtom = (*_pMovieFragmentAtomVec)[i];
        if (movieFragmentAtom != NULL)
        {
            TrackFragmentAtom *trackfragment = movieFragmentAtom->getTrackFragmentforID(id);
            if (trackfragment != NULL)
            {
                if (trackfragment->getTrackId() == id)
                {
                    return movieFragmentAtom;
                }
            }
        }
        i++;
    }
    return NULL;
}

void Mpeg4File::populateTrackDurationVec()
{
    uint64 trackDuration = 0;
    if (_pmovieAtom != NULL)
    {
        uint32 ids[256];
        uint32 size = 256;
        _pmovieAtom->getTrackIDList(ids, size);
        int32 numtracks = _pmovieAtom->getNumTracks();
        PV_MP4_FF_NEW(fp->auditCB, TrackDurationContainer, (), _pTrackDurationContainer);
        PV_MP4_FF_NEW(fp->auditCB, trackDurationInfoVecType, (), _pTrackDurationContainer->_pTrackdurationInfoVec);
        for (int32 i = 0; i < numtracks; i++)
        {
            uint32 trackID = ids[i];
            TrackDurationInfo *trackinfo = NULL;
            trackDuration = _pmovieAtom->getTrackMediaDuration(trackID);
            PV_MP4_FF_NEW(fp->auditCB, TrackDurationInfo, (trackDuration, trackID), trackinfo);
            (*_pTrackDurationContainer->_pTrackdurationInfoVec).push_back(trackinfo);

            TrackIndex *trackindx;
            PV_MP4_FF_NEW(fp->auditCB, TrackIndex , (), trackindx);
            trackindx->_trackID = trackID;
            trackindx->_Index = i;
            _moofFragmentIdx->push_back(trackindx);

            uint32 moofIdx = getIndexForTrackID(trackID);

            _movieFragmentIdx[moofIdx] = 0;
            _peekMovieFragmentIdx[moofIdx] = 0;
            _movieFragmentSeqIdx[moofIdx] = 1;
            _peekMovieFragmentSeqIdx[moofIdx] = 1;
        }
    }
}

void Mpeg4File::GetID3MetaData(PvmiKvpSharedPtrVector &id3Frames)
{
    if (_pID3Parser)
    {
        _pID3Parser->GetID3Frames(id3Frames);
    }
    else
    {
        _pmovieAtom->GetID3MetaData(id3Frames);
    }

}

void Mpeg4File::parseID3Header(MP4_FF_FILE *aFile)
{
    TOsclFileOffset curpos = AtomUtils::getCurrentFilePosition(aFile);
    AtomUtils::seekFromStart(aFile, 0);
    _pID3Parser->ParseID3Tag(&aFile->_pvfile);
    AtomUtils::seekFromStart(aFile, curpos);
}

uint32 Mpeg4File::getContentType()
{
    PVContentTypeAtom *pAtom = NULL;

    if (_puserDataAtom != NULL)
    {
        pAtom =
            (PVContentTypeAtom*) _puserDataAtom->getAtomOfType(PV_CONTENT_TYPE_ATOM);

        if (pAtom != NULL)
        {
            return pAtom->getContentType();
        }
        else
        {
            if (_oPVContent)
            {
                //Old PV Content, that doesnt have this atom
                //All such content is non-interleaved, with meta data
                //towards the very end
                return (DEFAULT_AUTHORING_MODE);
            }
        }
    }

    //Third party content
    return (0xFFFFFFFF);
}


MP4_ERROR_CODE Mpeg4File::getKeyMediaSampleNumAt(uint32 aTrackId,
        uint32 aKeySampleNum,
        GAU    *pgau, Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator>* aSampleDecryptionInfoVect)
{
    if (_pmovieAtom == NULL)
    {
        return READ_SAMPLE_TABLE_ATOM_FAILED;
    }
    MP4_ERROR_CODE ret = _pmovieAtom->getKeyMediaSampleNumAt(aTrackId, aKeySampleNum, pgau);
    if (ret == READ_FAILED)
    {
        uint32 totalSampleRead = 0;
        if (_isMovieFragmentsPresent)
        {
            uint32 n = 1;
            uint32 movieFragmentIdx = _movieFragmentIdx[getIndexForTrackID(aTrackId)];
            MovieFragmentAtom *pMovieFragmentAtom = NULL;

            if (movieFragmentIdx < _pMovieFragmentAtomVec->size())
                pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[movieFragmentIdx];

            if (pMovieFragmentAtom != NULL)
            {
                uint32 seqNum = pMovieFragmentAtom->getSequenceNumber();
                if (seqNum == _movieFragmentSeqIdx[getIndexForTrackID(aTrackId)])
                {
                    TrackFragmentAtom *trackfragment = pMovieFragmentAtom->getTrackFragmentforID(aTrackId);
                    if (trackfragment != NULL)
                    {
                        if (trackfragment->getTrackId() == aTrackId)
                        {
                            return (MP4_ERROR_CODE)pMovieFragmentAtom->getNextBundledAccessUnits(aTrackId, &n, totalSampleRead, pgau, aSampleDecryptionInfoVect);
                        }
                    }
                }
            }
        }
        return READ_FAILED;
    }
    else
    {
        return ret;
    }
}

int32 Mpeg4File::getOffsetByTime(uint32 id, uint64 ts, TOsclFileOffset* sampleFileOffset , uint32 jitterbuffertimeinmillisec)
{
    int32 ret =  DEFAULT_ERROR;
    uint32 sigmaAtomSize = 0;
    if (_pmovieAtom != NULL)
    {
        ret = _pmovieAtom->getOffsetByTime(id, ts, sampleFileOffset);
        if (ret == DEFAULT_ERROR || ret == LAST_SAMPLE_IN_MOOV)
        {
            if (_isMovieFragmentsPresent)
            {
                uint64 sigmaTrafDuration = 0;

                for (uint32 idx = 0; idx < _pMovieFragmentAtomVec->size(); idx++)
                {
                    MovieFragmentAtom *pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[idx];
                    if (pMovieFragmentAtom != NULL)
                    {
                        uint64 currTrafDuration = pMovieFragmentAtom->getCurrentTrafDuration(id);
                        if (currTrafDuration >= ts)
                        {
                            pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[idx];
                            return pMovieFragmentAtom->getOffsetByTime(id, ts, sampleFileOffset);
                        }
                        sigmaTrafDuration += currTrafDuration;
                    }
                }

                if (_parsing_mode == 1)
                {
                    if (moofParsingCompleted)
                    {
                        // do nothing
                    }
                    else
                    {
                        if ((uint32)_pMovieFragmentAtom->getSequenceNumber() == _movieFragmentSeqIdx[id])
                        {
                            AtomUtils::seekFromStart(_movieFragmentFilePtr, moofPtrPos);

                            while (!moofParsingCompleted)
                            {
                                TOsclFileOffset filePointer = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                uint32 bufCap = AtomUtils::getFileBufferingCapacity(_movieFragmentFilePtr);
                                if (bufCap)
                                {
                                    TOsclFileOffset bufStart = 0, bufEnd = 0;
                                    _movieFragmentFilePtr->_pvfile.GetCurrentByteRange(bufStart, bufEnd);
                                    if ((filePointer < bufStart) || ((TOsclFileOffset)(filePointer + moofSize) > bufEnd))
                                    {
                                        return INSUFFICIENT_DATA;
                                    }
                                }
                                _pMovieFragmentAtom->ParseMoofAtom(_movieFragmentFilePtr, moofSize, moofType, _pTrackDurationContainer, _pTrackExtendsAtomVec, moofParsingCompleted, countOfTrunsParsed, id);
                            }

                            if (moofParsingCompleted)
                            {
                                if (!_pMovieFragmentAtom->MP4Success())
                                {
                                    _success = false;
                                    _mp4ErrorCode = _pMovieFragmentAtom->GetMP4Error();
                                }
                                _pMovieFragmentAtom->setParent(this);
                                moofSize = _pMovieFragmentAtom->getSize();
                                moofCount -= _pMovieFragmentAtom->getSize();
                            }

                            uint64 currTrafDuration = _pMovieFragmentAtom->getCurrentTrafDuration(id);

                            if (currTrafDuration >= ts)
                            {
                                ret = _pMovieFragmentAtom->getOffsetByTime(id, ts, sampleFileOffset);
                                if (*sampleFileOffset == 0)
                                {
                                    // do nothing, continue parsing
                                }
                                else
                                {
                                    return ret;
                                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: ret Sample Offset=> Sample Offset= %d ret %d********@@@@@@@@@@@@@@@@", *sampleFileOffset, ret));
                                }
                            }
                            sigmaTrafDuration += currTrafDuration;
                        }
                        else
                        {
                            // This condition will only happen when the MovieFragmentAtomVec size is
                            // greater than 1.
                            uint32 i = _pMovieFragmentAtomVec->size();
                            _ptrMoofEnds = (*_pMoofOffsetVec)[i-2] + (*_pMovieFragmentAtomVec)[i-2]->getSize();
                            _pMoofOffsetVec->pop_back();
                            _pMovieFragmentAtomVec->pop_back();
                            PV_MP4_FF_DELETE(NULL, MovieFragmentAtom , (*_pMovieFragmentAtomVec)[i-1]);
                            parseMoofCompletely = true;
                            moofParsingCompleted = true;
                            moofSize = 0;
                            moofType = UNKNOWN_ATOM;
                            moofCount = 0;
                            moofPtrPos = 0;
                        }
                    }

                    TOsclFileOffset fileSize = 0;
                    TOsclFileOffset currfptr = 0;

                    AtomUtils::getCurrentFileSize(_movieFragmentFilePtr, fileSize);
                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime::FileSize %d Track ID %d ********@@@@@@@@@@@@@@@@", fileSize, id));

                    AtomUtils::seekFromStart(_movieFragmentFilePtr, _ptrMoofEnds);
                    TOsclFileOffset filePointer = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                    TOsclFileOffset count = (fileSize - filePointer);// -DEFAULT_ATOM_SIZE;
                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: TS= %d ********@@@@@@@@@@@@@@@@, count=%d, filePointer=%d", ts, count, filePointer));

                    while (count > 0)
                    {
                        uint32 atomType = UNKNOWN_ATOM;
                        uint32 atomSize = 0;
                        AtomUtils::Flush(_movieFragmentFilePtr);
                        AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);
                        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: count=%d, AtomSize=%d, atomtype=%d", count, atomSize, atomType));
                        if (atomSize < DEFAULT_ATOM_SIZE)
                        {

                            ret = DEFAULT_ERROR;
                            break;
                        }
                        sigmaAtomSize += atomSize;
                        if (atomType == MOVIE_FRAGMENT_ATOM)
                        {
                            TOsclFileOffset moofStartOffset = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                            moofStartOffset -= DEFAULT_ATOM_SIZE;
                            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: MovieFragmentAtom moofStartOffset=%d", moofStartOffset));

                            moofSize = atomSize;

                            if ((moofStartOffset + (TOsclFileOffset)atomSize) > fileSize)
                            {
                                const uint32 timeScale = _pmovieAtom->getTrackMediaTimescale(id);
                                if ((timeScale == 0) || (timeScale == 0xFFFFFFFF))
                                {
                                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: Invalid timeScale %d for Id %d", timeScale, id));
                                    return DEFAULT_ERROR;
                                }

                                uint32 trackPlayedSoFarInSec = (uint32)(ts / timeScale - jitterbuffertimeinmillisec / 1000);
                                uint32 rateOfDataUsageKbPerSec = 0;
                                if (trackPlayedSoFarInSec != 0)
                                {
                                    rateOfDataUsageKbPerSec = (uint32)(fileSize / trackPlayedSoFarInSec);
                                }
                                // estimate data for PVMF_MP4FFPARSER_PSEUDO_STREAMING_DURATION_IN_SEC
                                uint32 dataNeededAhead = (rateOfDataUsageKbPerSec * jitterbuffertimeinmillisec) / 1000;

                                *sampleFileOffset = moofStartOffset + atomSize + DEFAULT_ATOM_SIZE + dataNeededAhead;
                                PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: Insufficient data ot get Sample Offset= %d ********@@@@@@@@@@@@@@@@", *sampleFileOffset));
                                ret =  EVERYTHING_FINE;
                                break;
                            }

                            _pMoofOffsetVec->push_back(moofStartOffset);
                            parseMoofCompletely = true;

                            PV_MP4_FF_NEW(_movieFragmentFilePtr->auditCB, MovieFragmentAtom, (_movieFragmentFilePtr, _pFileSessionsForMediaDataRetrievalVec, atomSize, atomType, _pTrackDurationContainer, _pTrackExtendsAtomVec, parseMoofCompletely, moofParsingCompleted, countOfTrunsParsed, ipTrckEncryptnBxCntr), _pMovieFragmentAtom);

                            if (!_pMovieFragmentAtom->MP4Success())
                            {

                                _success = false;
                                _mp4ErrorCode = _pMovieFragmentAtom->GetMP4Error();
                                break;
                            }
                            count -= _pMovieFragmentAtom->getSize();
                            _pMovieFragmentAtom->setParent(this);

                            _pMovieFragmentAtomVec->push_back(_pMovieFragmentAtom);
                            _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);

                            const uint64 currTrafDuration = _pMovieFragmentAtom->getCurrentTrafDuration(id);

                            if (currTrafDuration >= ts)
                            {
                                ret = _pMovieFragmentAtom->getOffsetByTime(id, ts, sampleFileOffset);
                                if (*sampleFileOffset == 0)
                                {
                                    // do nothing, continue parsing
                                }
                                else
                                {
                                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: ret Sample Offset=> Sample Offset= %d ret %d********@@@@@@@@@@@@@@@@", *sampleFileOffset, ret));
                                    break;
                                }
                            }
                            sigmaTrafDuration += currTrafDuration;
                        }
                        else if (atomType == MEDIA_DATA_ATOM)
                        {

                            if (atomSize == 1)
                            {
                                uint64 largeSize = 0;
                                AtomUtils::read64(_movieFragmentFilePtr, largeSize);
                                uint32 size =
                                    Oscl_Int64_Utils::get_uint64_lower32(largeSize);
                                count -= size;
                                size -= 8; //for large size
                                size -= DEFAULT_ATOM_SIZE;
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, size);
                            }
                            else
                            {
                                currfptr = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                if ((currfptr + (TOsclFileOffset)atomSize) > fileSize)
                                {
                                    uint32 timeScale = _pmovieAtom->getTrackMediaTimescale(id);
                                    if ((timeScale == 0) || (timeScale == 0xFFFFFFFF))
                                    {
                                        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: Invalid timeScale %d for Id %d", timeScale, id));
                                        return DEFAULT_ERROR;
                                    }
                                    uint32 trackPlayedSoFarInSec = uint32(ts / timeScale - jitterbuffertimeinmillisec / 1000);
                                    uint32 rateOfDataUsageKbPerSec = 0;
                                    if (trackPlayedSoFarInSec != 0)
                                    {
                                        rateOfDataUsageKbPerSec = (uint32)(fileSize / trackPlayedSoFarInSec);
                                    }

                                    // estimate data for PVMF_MP4FFPARSER_PSEUDO_STREAMING_DURATION_IN_SEC
                                    uint32 dataNeededAhead = (rateOfDataUsageKbPerSec * jitterbuffertimeinmillisec) / 1000;
                                    *sampleFileOffset = currfptr + atomSize + moofSize + dataNeededAhead;
                                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "getOffsetByTime:: Insufficient data ot get Sample Offset= %d ********@@@@@@@@@@@@@@@@", *sampleFileOffset));
                                    ret = EVERYTHING_FINE;
                                    break;

                                }
                                count -= atomSize;
                                atomSize -= DEFAULT_ATOM_SIZE;
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                            }
                        }

                        else
                        {
                            if (count > 0)
                            {
                                count -= atomSize;
                                atomSize -= DEFAULT_ATOM_SIZE;
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                            }
                        }
                    }
                    if (sigmaTrafDuration == 0)
                    {
                        AtomUtils::getCurrentFileSize(_movieFragmentFilePtr, fileSize);
                        *sampleFileOffset = fileSize + 1000;

                        ret = EVERYTHING_FINE;
                    }
                }
            }
            else if (ret == LAST_SAMPLE_IN_MOOV)
            {
                ret = EVERYTHING_FINE;
            }

        }

        return ret;
    }
    else
    {

        return ret;
    }
}

int32 Mpeg4File::getTimestampForRandomAccessPoints(uint32 id, uint32 *num, uint64 *tsBuf, uint32* numBuf, TOsclFileOffset *offsetBuf)
{
    if (_pmovieAtom == NULL)
        return 0;

    uint32 requestedSamples = *num, delta = 0, returnedSampleFromMoov = 0;
    uint32 ret =  _pmovieAtom->getTimestampForRandomAccessPoints(id, num, tsBuf, numBuf, offsetBuf);
    if (ret == 1)
    {
        returnedSampleFromMoov = *num;
        if (requestedSamples != 0)
        {
            if (requestedSamples == returnedSampleFromMoov)
                return ret;

            if (requestedSamples > returnedSampleFromMoov)
            {
                delta = requestedSamples - returnedSampleFromMoov;
            }
        }

    }
    else
        delta = *num;

    if (_isMovieFragmentsPresent)
    {
        if (_pMovieFragmentRandomAccessAtomVec != NULL)
        { // Only one mfra possible in a clip so this loop will run only once
            for (uint32 idx = 0; idx < _pMovieFragmentRandomAccessAtomVec->size(); idx++)
            {
                MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = (*_pMovieFragmentRandomAccessAtomVec)[idx];
                ret = pMovieFragmentRandomAccessAtom->getTimestampForRandomAccessPoints(id, &delta, tsBuf, numBuf, offsetBuf, returnedSampleFromMoov);
                *num = delta;
                return ret;
            }

        }
    }
    return ret;
}

int32 Mpeg4File::getTimestampForRandomAccessPointsBeforeAfter(uint32 id, uint64 ts, uint64 *tsBuf, uint32* numBuf,
        uint32& numsamplestoget,
        uint32 howManyKeySamples)
{
    if (_pmovieAtom == NULL)
        return 0;

    int32 ret = _pmovieAtom->getTimestampForRandomAccessPointsBeforeAfter(id, ts, tsBuf, numBuf, numsamplestoget, howManyKeySamples);
    if (ret != 1)
    {
        if (_isMovieFragmentsPresent)
        {
            if (_pMovieFragmentRandomAccessAtomVec != NULL)
            { // Only one mfra possible in a clip so this loop will run only once
                for (uint32 idx = 0; idx < _pMovieFragmentRandomAccessAtomVec->size(); idx++)
                {
                    MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = (*_pMovieFragmentRandomAccessAtomVec)[idx];
                    ret = pMovieFragmentRandomAccessAtom->getTimestampForRandomAccessPointsBeforeAfter(id, ts, tsBuf, numBuf, numsamplestoget, howManyKeySamples);
                    return ret;
                }

            }
        }
    }
    return ret;
}

void Mpeg4File::resetAllMovieFragments()
{
    uint64 trackDuration = 0;

    if (_isMovieFragmentsPresent)
    {
        if (_pMovieFragmentAtomVec != NULL)
        {
            int numTracks = _pmovieAtom->getNumTracks();
            uint32 *trackList  = (uint32 *) oscl_malloc(sizeof(uint32) * numTracks);
            if (!trackList)
                return;       // malloc failed

            _pmovieAtom->getTrackWholeIDList(trackList);
            for (int32 i = 0; i < numTracks; i++)
            {
                uint32 trackID = trackList[i];
                uint32 moofIdx = getIndexForTrackID(trackID);
                _movieFragmentIdx[moofIdx] = 0;
                _peekMovieFragmentIdx[moofIdx] = 0;
                _movieFragmentSeqIdx[moofIdx] = 1;
                _peekMovieFragmentSeqIdx[moofIdx] = 1;
                TrackDurationInfo *trackinfo = NULL;
                if (_pTrackDurationContainer != NULL)
                {
                    TrackDurationInfo *pTrackDurationInfo = (*_pTrackDurationContainer->_pTrackdurationInfoVec)[i];
                    if (pTrackDurationInfo != NULL)
                    {
                        PV_MP4_FF_DELETE(NULL, TrackDurationInfo, pTrackDurationInfo);
                        pTrackDurationInfo = NULL;
                    }
                }
                trackDuration = _pmovieAtom->getTrackMediaDuration(trackID);
                PV_MP4_FF_NEW(fp->auditCB, TrackDurationInfo, (trackDuration, trackID), trackinfo);
                (*_pTrackDurationContainer->_pTrackdurationInfoVec)[i] = trackinfo;
            }
            oscl_free(trackList);
            for (uint32 idx = 0; idx < _pMovieFragmentAtomVec->size(); idx++)
            {
                MovieFragmentAtom *pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[idx];
                if (pMovieFragmentAtom != NULL)
                    pMovieFragmentAtom->resetPlayback();

            }
        }
    }
}

void Mpeg4File::OpenSessionForMoofMediaDataRetrieval(MP4_FF_FILE *fp, OSCL_wString& filename)
{
    //Open seperate sesssion for each track, required specially incase of Moof PS
    for (int32 i = 0; i < _pmovieAtom->getNumTracks(); i++)
    {
        OsclAny*ptr1 = oscl_malloc(sizeof(MP4_FF_FILE));
        if (ptr1 == NULL)
            return;

        MP4_FF_FILE *temp = OSCL_PLACEMENT_NEW(ptr1, MP4_FF_FILE());
        temp->_fileServSession = fp->_fileServSession;
        temp->_pvfile.SetCPM(fp->_pvfile.GetCPM());
        temp->_pvfile.SetFileHandle(fp->_pvfile.iFileHandle);
        if (AtomUtils::OpenMP4File(filename,
                                   Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                   temp) != 0)
        {
            _success = false;
            _mp4ErrorCode = FILE_OPEN_FAILED;
        }
        temp->_fileSize = fp->_fileSize;
        _pFileSessionsForMediaDataRetrievalVec->push_back(temp);
    }
}

uint32 Mpeg4File::resetPlayback(uint32 time, uint16 numTracks, uint32 *trackList, bool bResetToIFrame)
{
    OSCL_UNUSED_ARG(numTracks);

    if (0 == time)
    {

        resetPlayback();
        resetAllMovieFragments();
        return 0;
    }

    uint32 modifiedTimeStamp = time;

    bool oMoofFound = false;

    uint64 convertedTS = 0, returnedTS = 0;
    uint32 timestamp = 0;

    TOsclFileOffset moof_offset = 0;
    uint32 traf_number = 0, trun_number = 0, sample_num = 0;

    const uint32& trackID = *trackList; //  numTracks is the track index in trackList
    uint32 moofIdx = getIndexForTrackID(trackID);
    if (getTrackMediaType(trackID) == MEDIA_TYPE_VISUAL)
    {
        _oVideoTrackPresent = true;
    }

    if (_isMovieFragmentsPresent)
    {
        if (_pMovieFragmentAtomVec->size() > 1)
        {
            // The boolean is used to reset all MOOFs to start after reposition. This should
            // be true only when number of MOOFs in MOOF vector queue is more than one.
            isResetPlayBackCalled = true;
        }
    }
    if (getTrackMediaType(trackID) == MEDIA_TYPE_VISUAL)
    {
        if (repositionFromMoof(time, trackID))
        {
            //moof
            modifiedTimeStamp = time;

            // convert modifiedTimeStamp (which is in ms) to the appropriate
            // media time scale
            MediaClockConverter mcc1(1000);
            mcc1.set_clock(modifiedTimeStamp, 0);
            convertedTS = mcc1.get_converted_ts64(getTrackMediaTimescale(trackID));
            if (oMfraFound)
            {
                for (uint32 idx = 0; idx < _pMovieFragmentRandomAccessAtomVec->size(); idx++)
                {
                    MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = (*_pMovieFragmentRandomAccessAtomVec)[idx];
                    uint32 ret = pMovieFragmentRandomAccessAtom->getSyncSampleInfoClosestToTime(trackID, convertedTS, moof_offset, traf_number, trun_number, sample_num);
                    if (ret == 0)
                    {
                        if (moofParsingCompleted)
                        {
                            // do nothing
                        }
                        else
                        {
                            uint32 i = _pMovieFragmentAtomVec->size();
                            _pMoofOffsetVec->pop_back();
                            _pMovieFragmentAtomVec->pop_back();
                            PV_MP4_FF_DELETE(NULL, MovieFragmentAtom , (*_pMovieFragmentAtomVec)[i-1]);
                            parseMoofCompletely = true;
                            moofParsingCompleted = true;
                            moofSize = 0;
                            moofType = UNKNOWN_ATOM;
                            moofCount = 0;
                            moofPtrPos = 0;
                        }

                        for (uint32 idx = 0; idx < _pMoofOffsetVec->size(); idx++)
                        {
                            TOsclFileOffset moof_start_offset = (*_pMoofOffsetVec)[idx];
                            if (moof_start_offset == moof_offset && (*_pMovieFragmentAtomVec)[idx])
                            {
                                _movieFragmentIdx[moofIdx] = idx;
                                _peekMovieFragmentIdx[moofIdx] = idx;
                                _movieFragmentSeqIdx[moofIdx] = (*_pMovieFragmentAtomVec)[idx]->getSequenceNumber();
                                _peekMovieFragmentSeqIdx[moofIdx] = _movieFragmentSeqIdx[moofIdx];
                                _pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[idx];
                                currMoofNum = _pMovieFragmentAtom->getSequenceNumber();
                                oMoofFound = true;

                                AtomUtils::seekFromStart(_movieFragmentFilePtr, moof_offset);
                                uint32 atomType = UNKNOWN_ATOM;
                                uint32 atomSize = 0;
                                AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);
                                if (atomType == MOVIE_FRAGMENT_ATOM)
                                {
                                    atomSize -= DEFAULT_ATOM_SIZE;
                                    AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                    _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                }
                                break;
                            }
                        }

                        if (_parsing_mode == 1)
                        {
                            if (!oMoofFound)
                            {
                                TOsclFileOffset fileSize = 0;
                                _ptrMoofEnds = moof_offset;
                                AtomUtils::getCurrentFileSize(_movieFragmentFilePtr, fileSize);
                                AtomUtils::seekFromStart(_movieFragmentFilePtr, _ptrMoofEnds);
                                TOsclFileOffset filePointer = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                TOsclFileOffset count = (fileSize - filePointer);// -DEFAULT_ATOM_SIZE;

                                while (count > 0)
                                {
                                    uint32 atomType = UNKNOWN_ATOM;
                                    uint32 atomSize = 0;
                                    AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);
                                    if (atomType == MOVIE_FRAGMENT_ATOM)
                                    {
                                        parseMoofCompletely = true;

                                        TOsclFileOffset moofStartOffset = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                        moofStartOffset -= DEFAULT_ATOM_SIZE;

                                        if (moofParsingCompleted)
                                        {
                                            // do nothing
                                        }
                                        else
                                        {
                                            uint32 i = _pMovieFragmentAtomVec->size();
                                            _pMoofOffsetVec->pop_back();
                                            _pMovieFragmentAtomVec->pop_back();
                                            PV_MP4_FF_DELETE(NULL, MovieFragmentAtom , (*_pMovieFragmentAtomVec)[i-1]);
                                            parseMoofCompletely = true;
                                            moofParsingCompleted = true;
                                            moofSize = 0;
                                            moofType = UNKNOWN_ATOM;
                                            moofCount = 0;
                                            moofPtrPos = 0;
                                        }

                                        PV_MP4_FF_NEW(_movieFragmentFilePtr->auditCB, MovieFragmentAtom, (_movieFragmentFilePtr, _pFileSessionsForMediaDataRetrievalVec, atomSize, atomType, _pTrackDurationContainer, _pTrackExtendsAtomVec, parseMoofCompletely, moofParsingCompleted, countOfTrunsParsed, ipTrckEncryptnBxCntr), _pMovieFragmentAtom);

                                        if (!_pMovieFragmentAtom->MP4Success())
                                        {
                                            _success = false;
                                            _mp4ErrorCode = _pMovieFragmentAtom->GetMP4Error();
                                            break;
                                        }

                                        _pMovieFragmentAtom->setParent(this);
                                        count -= _pMovieFragmentAtom->getSize();

                                        uint32 i = _pMovieFragmentAtomVec->size();

                                        MovieFragmentAtom *pMovieFragmentAtom = NULL;
                                        uint32 prevMoofSeqNum = 0;

                                        if (i > 0)
                                        {
                                            pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[i-1];

                                            if (pMovieFragmentAtom != NULL)
                                                prevMoofSeqNum = (*_pMovieFragmentAtomVec)[i-1]->getSequenceNumber();
                                        }

                                        currMoofNum = _pMovieFragmentAtom->getSequenceNumber();

                                        for (uint32 idx = prevMoofSeqNum; idx < currMoofNum - 1; idx++)
                                        {
                                            _pMovieFragmentAtomVec->push_back(NULL);
                                            _pMoofOffsetVec->push_back(0);
                                        }
                                        if (currMoofNum > i)
                                        {
                                            _pMoofOffsetVec->push_back(moofStartOffset);
                                            _pMovieFragmentAtomVec->push_back(_pMovieFragmentAtom);
                                        }
                                        else if ((*_pMovieFragmentAtomVec)[currMoofNum-1] == NULL)
                                        {
                                            (*_pMovieFragmentAtomVec)[currMoofNum-1] = _pMovieFragmentAtom;
                                            (*_pMoofOffsetVec)[currMoofNum-1] = moofStartOffset;
                                        }
                                        else
                                        {
                                            PV_MP4_FF_DELETE(_movieFragmentFilePtr->auditCB, MovieFragmentAtom, _pMovieFragmentAtom);
                                            _pMovieFragmentAtom = NULL;
                                            break;

                                        }
                                        _movieFragmentSeqIdx[moofIdx] = currMoofNum;
                                        _movieFragmentIdx[moofIdx] = currMoofNum - 1;
                                        _peekMovieFragmentIdx[moofIdx] = currMoofNum - 1;
                                        _peekMovieFragmentSeqIdx[moofIdx] = currMoofNum;

                                        oMoofFound = true;

                                        _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                        break;
                                    }
                                    else if (atomType == MEDIA_DATA_ATOM)
                                    {
                                        if (atomSize == 1)
                                        {
                                            uint64 largeSize = 0;
                                            AtomUtils::read64(_movieFragmentFilePtr, largeSize);
                                            uint32 size =
                                                Oscl_Int64_Utils::get_uint64_lower32(largeSize);
                                            count -= size;
                                            size -= 8; //for large size
                                            size -= DEFAULT_ATOM_SIZE;
                                            AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, size);
                                        }
                                        else
                                        {
                                            if (atomSize < DEFAULT_ATOM_SIZE)
                                            {
                                                _success = false;
                                                _mp4ErrorCode = ZERO_OR_NEGATIVE_ATOM_SIZE;
                                                break;
                                            }
                                            if (count < (int32)atomSize)
                                            {
                                                _success = false;
                                                _mp4ErrorCode = READ_FAILED;
                                                break;
                                            }
                                            count -= atomSize;
                                            atomSize -= DEFAULT_ATOM_SIZE;
                                            AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                        }
                                    }

                                    else
                                    {
                                        if (count > 0)
                                        {
                                            count -= atomSize;
                                            atomSize -= DEFAULT_ATOM_SIZE;
                                            AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                        }

                                    }
                                }
                            }

                        }

                        if (_pmovieAtom != NULL)
                            _pmovieAtom->resetTrackToEOT();

                        if (_pMovieFragmentAtom != NULL)
                            returnedTS = _pMovieFragmentAtom->resetPlayback(trackID, convertedTS, traf_number, trun_number, sample_num);
                    }
                    else
                    {
                        // Not a valid tfra entries, cannot reposition.
                        return 0;
                    }
                }
            }
            else
                return 0;

            // convert returnedTS (which is in media time scale) to the ms
            MediaClockConverter mcc(getTrackMediaTimescale(trackID));
            mcc.set_clock(returnedTS, 0);
            timestamp = mcc.get_converted_ts(1000);

            if (timestamp <= modifiedTimeStamp)
            {
                modifiedTimeStamp = timestamp;
            }
        }
        else
        {
            if (_isMovieFragmentsPresent)
            {
                if (_pMovieFragmentAtomVec->size() > 0)
                {
                    if (moofParsingCompleted)
                    {
                        // do nothing
                    }
                    else
                    {
                        uint32 i = _pMovieFragmentAtomVec->size();
                        _pMoofOffsetVec->pop_back();
                        _pMovieFragmentAtomVec->pop_back();
                        PV_MP4_FF_DELETE(NULL, MovieFragmentAtom , (*_pMovieFragmentAtomVec)[i-1]);
                        parseMoofCompletely = true;
                        moofParsingCompleted = true;
                        moofSize = 0;
                        moofType = UNKNOWN_ATOM;
                        moofCount = 0;
                        moofPtrPos = 0;
                    }
                }
            }

            //movie
            if (_pmovieAtom != NULL)
            {
                resetAllMovieFragments();
                uint32 trackVideo = trackID;
                uint16 numTrackForVideo = 1;
                modifiedTimeStamp =  _pmovieAtom->resetPlayback(modifiedTimeStamp, numTrackForVideo, &trackVideo, bResetToIFrame);
            }
        }
    }

    uint32 retVal = modifiedTimeStamp;
    if ((getTrackMediaType(trackID) == MEDIA_TYPE_AUDIO) ||
            (getTrackMediaType(trackID) == MEDIA_TYPE_TEXT))
    {
        if (repositionFromMoof(time, trackID))
        {
            oMoofFound = false;
            //moof
            // convert modifiedTimeStamp (which is in ms) to the appropriate
            // media time scale
            MediaClockConverter mcc1(1000);
            mcc1.set_clock(modifiedTimeStamp, 0);
            convertedTS = mcc1.get_converted_ts64(getTrackMediaTimescale(trackID));
            if (oMfraFound)
            {
                for (uint32 idx = 0; idx < _pMovieFragmentRandomAccessAtomVec->size(); idx++)
                {
                    MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = (*_pMovieFragmentRandomAccessAtomVec)[idx];
                    uint32 ret = pMovieFragmentRandomAccessAtom->getSyncSampleInfoClosestToTime(trackID, convertedTS, moof_offset, traf_number, trun_number, sample_num);
                    if (ret == 0)
                    {
                        if (moofParsingCompleted)
                        {
                            // do nothing
                        }
                        else
                        {
                            uint32 i = _pMovieFragmentAtomVec->size();
                            _pMoofOffsetVec->pop_back();
                            _pMovieFragmentAtomVec->pop_back();
                            PV_MP4_FF_DELETE(NULL, MovieFragmentAtom , (*_pMovieFragmentAtomVec)[i-1]);
                            parseMoofCompletely = true;
                            moofParsingCompleted = true;
                            moofSize = 0;
                            moofType = UNKNOWN_ATOM;
                            moofCount = 0;
                            moofPtrPos = 0;
                        }
                        //
                        for (idx = 0; idx < _pMoofOffsetVec->size(); idx++)
                        {
                            TOsclFileOffset moof_start_offset = (*_pMoofOffsetVec)[idx];
                            if (moof_start_offset == moof_offset)
                            {
                                _movieFragmentIdx[moofIdx] = idx;
                                _peekMovieFragmentIdx[moofIdx] = idx;
                                _movieFragmentSeqIdx[moofIdx] = (*_pMovieFragmentAtomVec)[idx]->getSequenceNumber();
                                _peekMovieFragmentSeqIdx[moofIdx] = _movieFragmentSeqIdx[moofIdx];
                                _pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[idx];
                                currMoofNum = _pMovieFragmentAtom->getSequenceNumber();
                                oMoofFound = true;

                                AtomUtils::seekFromStart(_movieFragmentFilePtr, moof_offset);
                                uint32 atomType = UNKNOWN_ATOM;
                                uint32 atomSize = 0;
                                AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);
                                if (atomType == MOVIE_FRAGMENT_ATOM)
                                {
                                    atomSize -= DEFAULT_ATOM_SIZE;
                                    AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                    _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                }
                                break;
                            }
                        }
                        //
                    }
                    else
                    {
                        // Not a valid tfra entries, cannot reposition.
                        return 0;
                    }
                }
                if (_parsing_mode == 1 && !oMoofFound)
                {

                    if (!oMoofFound)
                    {
                        _ptrMoofEnds = moof_offset;
                        TOsclFileOffset fileSize = 0;
                        AtomUtils::getCurrentFileSize(_movieFragmentFilePtr, fileSize);
                        AtomUtils::seekFromStart(_movieFragmentFilePtr, _ptrMoofEnds);
                        TOsclFileOffset filePointer = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                        TOsclFileOffset count = (fileSize - filePointer);// -DEFAULT_ATOM_SIZE;

                        while (count > 0)
                        {
                            uint32 atomType = UNKNOWN_ATOM;
                            uint32 atomSize = 0;
                            AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);
                            if (atomType == MOVIE_FRAGMENT_ATOM)
                            {
                                TOsclFileOffset moofStartOffset = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                moofStartOffset -= DEFAULT_ATOM_SIZE;
                                parseMoofCompletely = true;

                                if (moofParsingCompleted)
                                {
                                    // do nothing
                                }
                                else
                                {
                                    uint32 i = _pMovieFragmentAtomVec->size();
                                    _pMoofOffsetVec->pop_back();
                                    _pMovieFragmentAtomVec->pop_back();
                                    PV_MP4_FF_DELETE(NULL, MovieFragmentAtom , (*_pMovieFragmentAtomVec)[i-1]);
                                    parseMoofCompletely = true;
                                    moofParsingCompleted = true;
                                    moofSize = 0;
                                    moofType = UNKNOWN_ATOM;
                                    moofCount = 0;
                                    moofPtrPos = 0;
                                }

                                PV_MP4_FF_NEW(_movieFragmentFilePtr->auditCB, MovieFragmentAtom, (_movieFragmentFilePtr, _pFileSessionsForMediaDataRetrievalVec, atomSize, atomType, _pTrackDurationContainer, _pTrackExtendsAtomVec, parseMoofCompletely, moofParsingCompleted, countOfTrunsParsed, ipTrckEncryptnBxCntr), _pMovieFragmentAtom);

                                if (!_pMovieFragmentAtom->MP4Success())
                                {
                                    _success = false;
                                    _mp4ErrorCode = _pMovieFragmentAtom->GetMP4Error();
                                    break;
                                }

                                _pMovieFragmentAtom->setParent(this);
                                count -= _pMovieFragmentAtom->getSize();
                                uint32 i = _pMovieFragmentAtomVec->size();

                                MovieFragmentAtom *pMovieFragmentAtom = NULL;
                                uint32 prevMoofSeqNum = 0;

                                if (i > 0)
                                {
                                    pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[i-1];

                                    if (pMovieFragmentAtom != NULL)
                                        prevMoofSeqNum = (*_pMovieFragmentAtomVec)[i-1]->getSequenceNumber();
                                }
                                currMoofNum = _pMovieFragmentAtom->getSequenceNumber();

                                for (uint32 idx = prevMoofSeqNum; idx < currMoofNum - 1; idx++)
                                {
                                    _pMovieFragmentAtomVec->push_back(NULL);
                                    _pMoofOffsetVec->push_back(0);
                                }

                                if (currMoofNum > i)
                                {
                                    _pMoofOffsetVec->push_back(moofStartOffset);
                                    _pMovieFragmentAtomVec->push_back(_pMovieFragmentAtom);
                                }
                                else if ((*_pMovieFragmentAtomVec)[currMoofNum-1] == NULL)
                                {
                                    (*_pMovieFragmentAtomVec)[currMoofNum-1] = _pMovieFragmentAtom;
                                    (*_pMoofOffsetVec)[currMoofNum-1] = moofStartOffset;
                                }
                                else
                                {
                                    PV_MP4_FF_DELETE(_movieFragmentFilePtr->auditCB, MovieFragmentAtom, _pMovieFragmentAtom);
                                    _pMovieFragmentAtom = NULL;
                                    break;

                                }
                                if (oMfraFound)
                                {
                                    currMoofNum = _pMovieFragmentAtom->getSequenceNumber();
                                    _movieFragmentIdx[moofIdx] = currMoofNum - 1 ;
                                    _peekMovieFragmentIdx[moofIdx] = currMoofNum - 1;
                                    _movieFragmentSeqIdx[moofIdx] = currMoofNum;
                                    _peekMovieFragmentSeqIdx[moofIdx] = _movieFragmentSeqIdx[moofIdx];
                                    oMoofFound = true;
                                    if (!_oVideoTrackPresent)
                                    {
                                        _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                    }
                                    break;
                                }
                                uint64 currTrafDuration = _pMovieFragmentAtom->getCurrentTrafDuration(trackID);
                                if (currTrafDuration >= modifiedTimeStamp)
                                {
                                    currMoofNum = _pMovieFragmentAtom->getSequenceNumber();
                                    _movieFragmentIdx[moofIdx] = currMoofNum - 1;
                                    _peekMovieFragmentIdx[moofIdx] = currMoofNum - 1;
                                    _movieFragmentSeqIdx[moofIdx] = currMoofNum;
                                    _peekMovieFragmentSeqIdx[moofIdx] = _movieFragmentSeqIdx[moofIdx];
                                    oMoofFound = true;
                                    if (!_oVideoTrackPresent)
                                    {
                                        _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                    }
                                    break;
                                }

                            }
                            else if (atomType == MEDIA_DATA_ATOM)
                            {
                                if (atomSize == 1)
                                {
                                    uint64 largeSize = 0;
                                    AtomUtils::read64(_movieFragmentFilePtr, largeSize);
                                    uint32 size =
                                        Oscl_Int64_Utils::get_uint64_lower32(largeSize);
                                    count -= size;
                                    size -= 8; //for large size
                                    size -= DEFAULT_ATOM_SIZE;
                                    AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, size);
                                }
                                else
                                {
                                    if (atomSize < DEFAULT_ATOM_SIZE)
                                    {
                                        _success = false;
                                        _mp4ErrorCode = ZERO_OR_NEGATIVE_ATOM_SIZE;
                                        break;
                                    }
                                    if (count < (int32)atomSize)
                                    {
                                        _success = false;
                                        _mp4ErrorCode = READ_FAILED;
                                        break;
                                    }
                                    count -= atomSize;
                                    atomSize -= DEFAULT_ATOM_SIZE;
                                    AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                }
                            }

                            else
                            {
                                if (count > 0)
                                {
                                    count -= atomSize;
                                    atomSize -= DEFAULT_ATOM_SIZE;
                                    AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                }

                            }
                        }
                    }


                }

                if (_pmovieAtom != NULL)
                    _pmovieAtom->resetTrackToEOT();

                if (_pMovieFragmentAtom != NULL)
                    returnedTS = _pMovieFragmentAtom->resetPlayback(trackID, convertedTS, traf_number, trun_number, sample_num);
            }
            else
                return 0;

            // convert returnedTS (which is in media time scale) to the ms
            MediaClockConverter mcc(getTrackMediaTimescale(trackID));
            mcc.set_clock(returnedTS, 0);
            timestamp = mcc.get_converted_ts(1000);


            if (timestamp <= modifiedTimeStamp)
            {
                modifiedTimeStamp = timestamp;
            }
            retVal = modifiedTimeStamp;

        }
        else
        {
            if (_isMovieFragmentsPresent)
            {
                if (_pMovieFragmentAtomVec->size() > 0)
                {
                    if (moofParsingCompleted)
                    {
                        // do nothing
                    }
                    else
                    {
                        uint32 i = _pMovieFragmentAtomVec->size();
                        _pMoofOffsetVec->pop_back();
                        _pMovieFragmentAtomVec->pop_back();
                        PV_MP4_FF_DELETE(NULL, MovieFragmentAtom , (*_pMovieFragmentAtomVec)[i-1]);
                        parseMoofCompletely = true;
                        moofParsingCompleted = true;
                        moofSize = 0;
                        moofType = UNKNOWN_ATOM;
                        moofCount = 0;
                        moofPtrPos = 0;
                    }
                }
            }

            //movie
            if (_pmovieAtom != NULL)
            {
                resetAllMovieFragments();
                uint32 trackAudio = trackID;
                uint16 numTrackforAudio = 1;
                retVal = _pmovieAtom->resetPlayback(modifiedTimeStamp, numTrackforAudio, &trackAudio
                                                    , bResetToIFrame);
            }
        }

    }
    return retVal;

}

uint32 Mpeg4File::queryRepositionTime(uint32 time,
                                      uint16 numTracks,
                                      uint32 *trackList,
                                      bool bResetToIFrame,
                                      bool bBeforeRequestedTime)
{

    uint32 i = 0;
    uint32 ret = 0;
    uint32 modifiedTimeStamp = time;
    uint32 trackID = 0;
    uint32 trackIds[256];

    bool oVideoTrackFound = false;
    int j = 1;
    for (i = 0; i < numTracks; i++)
    {
        trackID = trackList[i];
        if (getTrackMediaType(trackID) == MEDIA_TYPE_VISUAL)
        {
            trackIds[0] = trackList[i];
            oVideoTrackFound = true;
        }
        else
        {
            trackIds[j++] = trackList[i];
        }
    }

    uint64 convertedTS = 0;
    uint64 returnedTS = 0;
    uint32 timestamp = 0;

    for (i = 0; i < numTracks; i++)
    {
        trackID = trackIds[i];

        if (!oVideoTrackFound)
            trackID = trackList[i];

        if (getTrackMediaType(trackID) == MEDIA_TYPE_VISUAL)
        {
            if (repositionFromMoof(time, trackID))
            {
                //moof
                modifiedTimeStamp = time;

                // convert modifiedTimeStamp (which is in ms) to the appropriate
                // media time scale
                MediaClockConverter mcc1(1000);
                mcc1.set_clock(modifiedTimeStamp, 0);
                convertedTS = mcc1.get_converted_ts64(getTrackMediaTimescale(trackID));
                if (oMfraFound)
                {
                    oMfraFound = true;
                    for (uint32 idx = 0; idx < _pMovieFragmentRandomAccessAtomVec->size(); idx++)
                    {

                        MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = (*_pMovieFragmentRandomAccessAtomVec)[idx];
                        returnedTS = pMovieFragmentRandomAccessAtom->queryRepositionTime(trackID, convertedTS, bResetToIFrame,
                                     bBeforeRequestedTime);
                        if (returnedTS != 0)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    oMfraFound = false;
                    if (_parsing_mode == 1)
                    {
                        //Todo: Should have some sentinel for the invalid ts..
                        //PVMF_MP4_MAX_UINT32 seems to be valid ts.
                        return PVMF_MP4_MAX_UINT32;
                    }
                }

                // convert returnedTS (which is in media time scale) to the ms
                MediaClockConverter mcc(getTrackMediaTimescale(trackID));
                mcc.update_clock(returnedTS);
                timestamp = mcc.get_converted_ts(1000);

                modifiedTimeStamp = timestamp;

                ret = modifiedTimeStamp;

            }
            else
            {
                //movie
                if (_pmovieAtom != NULL)
                {
                    modifiedTimeStamp =  _pmovieAtom->queryRepositionTime(time,
                                         numTracks,
                                         trackList,
                                         bResetToIFrame,
                                         bBeforeRequestedTime);
                    ret = modifiedTimeStamp;
                }
            }
        }

        if ((getTrackMediaType(trackID) == MEDIA_TYPE_AUDIO) ||
                (getTrackMediaType(trackID) == MEDIA_TYPE_TEXT))
        {
            if (repositionFromMoof(time, trackID))
            {
                //moof
                MediaClockConverter mcc1(1000);
                mcc1.update_clock(modifiedTimeStamp);
                convertedTS = mcc1.get_converted_ts(getTrackMediaTimescale(trackID));

                for (uint32 idx = 0; idx < _pMovieFragmentRandomAccessAtomVec->size(); idx++)
                {

                    MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = (*_pMovieFragmentRandomAccessAtomVec)[idx];
                    returnedTS = pMovieFragmentRandomAccessAtom->queryRepositionTime(trackID, convertedTS, bResetToIFrame,
                                 bBeforeRequestedTime);
                    if (returnedTS != 0)
                    {
                        break;
                    }
                }
                // convert returnedTS (which is in media time scale) to the ms
                MediaClockConverter mcc(getTrackMediaTimescale(trackID));
                mcc.update_clock(returnedTS);
                timestamp = mcc.get_converted_ts(1000);

                if (!oVideoTrackFound)
                {
                    if (getTrackMediaType(trackID) == MEDIA_TYPE_AUDIO)
                    {
                        modifiedTimeStamp = timestamp;
                    }
                    else if (getTrackMediaType(trackID) == MEDIA_TYPE_TEXT && numTracks == 1)
                    {
                        modifiedTimeStamp = timestamp;
                    }
                }

                return modifiedTimeStamp;

            }
            else
            {
                //movie
                if (_pmovieAtom != NULL)
                {
                    modifiedTimeStamp =   _pmovieAtom->queryRepositionTime(modifiedTimeStamp,
                                          numTracks,
                                          trackList,
                                          bResetToIFrame,
                                          bBeforeRequestedTime);

                    if (!oVideoTrackFound)
                    {
                        if (getTrackMediaType(trackID) == MEDIA_TYPE_AUDIO)
                        {
                            ret = modifiedTimeStamp;
                        }
                        else if (getTrackMediaType(trackID) == MEDIA_TYPE_TEXT && numTracks == 1)
                        {
                            ret = modifiedTimeStamp;
                        }
                    }
                }

            }

        }
    }
    return ret;
}

int32 Mpeg4File::parseMFRA()
{
    uint32 ret = 0;
    TOsclFileOffset fileSize = 0;
    uint32 MfraStartOffset = 0;

    // save the start search point
    TOsclFileOffset startpoint =  AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);

    AtomUtils::getCurrentFileSize(_movieFragmentFilePtr, fileSize);
    AtomUtils::seekFromStart(_movieFragmentFilePtr, fileSize);
    AtomUtils::rewindFilePointerByN(_movieFragmentFilePtr, 16);

    uint32 atomType = UNKNOWN_ATOM;
    uint32 atomSize = 0;
    AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);

    TOsclFileOffset curr = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);

    if (atomType == MOVIE_FRAGMENT_RANDOM_ACCESS_OFFSET_ATOM)
    {
        if (_pMfraOffsetAtom == NULL)
        {
            PV_MP4_FF_NEW(fp->auditCB, MfraOffsetAtom, (_movieFragmentFilePtr, atomSize, atomType), _pMfraOffsetAtom);
            if (!_pMfraOffsetAtom->MP4Success())
            {
                _success = false;
                _mp4ErrorCode = READ_MOVIE_FRAGMENT_RANDOM_ACCESS_OFFSET_FAILED;
                return _mp4ErrorCode;
            }
            MfraStartOffset = _pMfraOffsetAtom->getSizeStoredInmfro();
        }
    }
    AtomUtils::rewindFilePointerByN(_movieFragmentFilePtr, MfraStartOffset);
    AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);
    curr = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);

    if (atomType == MOVIE_FRAGMENT_RANDOM_ACCESS_ATOM)
    {
        if (_pMovieFragmentRandomAccessAtomVec->size() == 0)
        {
            MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = NULL;
            PV_MP4_FF_NEW(fp->auditCB, MovieFragmentRandomAccessAtom, (_movieFragmentFilePtr, atomSize, atomType), pMovieFragmentRandomAccessAtom);

            if (!pMovieFragmentRandomAccessAtom->MP4Success())
            {
                PV_MP4_FF_DELETE(NULL, MovieFragmentRandomAccessAtom, pMovieFragmentRandomAccessAtom);
                _success = false;
                _mp4ErrorCode = pMovieFragmentRandomAccessAtom->GetMP4Error();
                return _mp4ErrorCode ;
            }
            pMovieFragmentRandomAccessAtom->setParent(this);
            _pMovieFragmentRandomAccessAtomVec->push_back(pMovieFragmentRandomAccessAtom);
            oMfraFound = true;
        }
    }

    // return to the start point when the mfra atom is not found
    if (!oMfraFound)
        AtomUtils::seekFromStart(_movieFragmentFilePtr, startpoint);

    return ret;

}


int32 Mpeg4File::peekNextBundledAccessUnits(const uint32 trackID,
        uint32 *n,
        MediaMetaInfo *mInfo)
{
    // IF THERE ARE NO MEDIA TRACKS, RETURN READ ERROR
    uint32 samplesTobeRead;
    samplesTobeRead = *n;
    uint32 totalSampleRead = 0;
    uint32 moofIdx = getIndexForTrackID(trackID);
    if (getNumTracks() == 0)
    {
        return -1;
    }
    if (_pmovieAtom != NULL)
    {
        uint32 ret = (_pmovieAtom->peekNextBundledAccessUnits(trackID, n, mInfo));

        if (ret == END_OF_TRACK)
        {
            if (!_isMovieFragmentsPresent)
                return ret;

            bool oAllMoofExhausted = false;

            totalSampleRead += *n;

            if (totalSampleRead == samplesTobeRead)
            {
                *n = totalSampleRead;
                return EVERYTHING_FINE;
            }

            if (_pMovieFragmentAtomVec != NULL)
            {
                if (samplesTobeRead >= *n)
                    *n = samplesTobeRead - *n;
                if (*n == 0)
                    *n = samplesTobeRead;
            }
            else
                return ret;

            // Align the peek to the get indexes
            _peekMovieFragmentIdx[moofIdx] = _movieFragmentIdx[moofIdx];
            _peekMovieFragmentSeqIdx[moofIdx] = _movieFragmentSeqIdx[moofIdx];

            if (_parsing_mode == 0)
            {
                int32 return1 = 0;
                while (_peekMovieFragmentIdx[moofIdx] < _pMovieFragmentAtomVec->size())
                {
                    uint32 peekMovieFragmentIdx = _peekMovieFragmentIdx[moofIdx];
                    MovieFragmentAtom *pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[peekMovieFragmentIdx];
                    if (pMovieFragmentAtom != NULL)
                    {
                        if ((uint32)pMovieFragmentAtom->getSequenceNumber() == _peekMovieFragmentSeqIdx[moofIdx])
                        {
                            TrackFragmentAtom *trackfragment = pMovieFragmentAtom->getTrackFragmentforID(trackID);
                            if (trackfragment != NULL)
                            {
                                if (trackfragment->getTrackId() == trackID)
                                {
                                    return1 = pMovieFragmentAtom->peekNextBundledAccessUnits(trackID, n, totalSampleRead, mInfo);
                                    totalSampleRead += *n;
                                    if (return1 != END_OF_TRACK)
                                    {
                                        *n = totalSampleRead;
                                        return return1;
                                    }
                                    else
                                    {
                                        if (samplesTobeRead >= *n)
                                        {
                                            samplesTobeRead = samplesTobeRead - *n;
                                            *n = samplesTobeRead;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    _peekMovieFragmentIdx[moofIdx]++;
                    _peekMovieFragmentSeqIdx[moofIdx]++;
                }
                if (return1 == END_OF_TRACK)
                {
                    *n = totalSampleRead;
                    if (!MoreMoofAtomsExpected(_movieFragmentSeqIdx[moofIdx]))
                    {
                        _peekMovieFragmentIdx[moofIdx] = 0;
                        _peekMovieFragmentSeqIdx[moofIdx] = 1;
                    }
                    else
                    {
                        //We have run out of moofs and theres no data to be parsed
                        if (*n == 0)
                        {
                            return1 = INSUFFICIENT_DATA;
                        }
                        else
                        {
                            *n = totalSampleRead;
                            return1 = EVERYTHING_FINE;
                        }
                    }
                    return return1;
                }
            }
            else
            {

                while (!oAllMoofExhausted)
                {
                    uint32 moofIndex = 0;
                    bool moofToBeParsed = false;

                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "1.) moofIndex=%d, moofToBeParsed=%d, sizeAtomVec=%d",
                            moofIndex, moofToBeParsed, _pMovieFragmentAtomVec->size()));

                    if (_pMovieFragmentAtomVec->size() > _peekMovieFragmentIdx[moofIdx])
                    {
                        MovieFragmentAtom *pMovieFragmentAtom = NULL;
                        pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[_peekMovieFragmentIdx[moofIdx]];
                        if (pMovieFragmentAtom == NULL)
                        {
                            moofToBeParsed = true;
                            moofIndex = _peekMovieFragmentIdx[moofIdx];
                        }
                    }


                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "2.) moofIndex=%d, moofToBeParsed=%d, sizeAtomVec=%d",
                            moofIndex, moofToBeParsed, _pMovieFragmentAtomVec->size()));

                    if ((_pMovieFragmentAtomVec->size() <= _peekMovieFragmentIdx[moofIdx]) || moofToBeParsed)
                    {
                        TOsclFileOffset fileSize = 0;
                        AtomUtils::getCurrentFileSize(_movieFragmentFilePtr, fileSize);
                        AtomUtils::seekFromStart(_movieFragmentFilePtr, _ptrMoofEnds);
                        TOsclFileOffset filePointer = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                        TOsclFileOffset count = fileSize - filePointer;// -DEFAULT_ATOM_SIZE;

//                        PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "filePointer=%d, fileSize=%d, count=%d",
//                              (uint32)filePointer, (uint32)fileSize, (uint32)count));

                        while (count > 0)
                        {
                            /////////////////////////////////////////
                            TOsclFileOffset filePointer = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                            count = (fileSize - filePointer);// -DEFAULT_ATOM_SIZE;
                            uint32 bufCap = AtomUtils::getFileBufferingCapacity(_movieFragmentFilePtr);
                            if (bufCap)
                            {
                                TOsclFileOffset bufStart = 0, bufEnd = 0;
                                _movieFragmentFilePtr->_pvfile.GetCurrentByteRange(bufStart, bufEnd);
                                fileSize = bufEnd + 1;
                                if ((filePointer < bufStart) || ((filePointer + DEFAULT_ATOM_SIZE) > bufEnd))
                                {
                                    return INSUFFICIENT_DATA;
                                }
                            }
                            uint32 atomType = UNKNOWN_ATOM;
                            uint32 atomSize = 0;

                            TOsclFileOffset currPos = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                            AtomUtils::getNextAtomType(_movieFragmentFilePtr, atomSize, atomType);

                            PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "atomType=%d, atomSize=%d, currPos=%d",
                                    atomType, atomSize, currPos));

                            if ((currPos + (TOsclFileOffset)atomSize) > fileSize)
                            {
                                AtomUtils::seekFromStart(_movieFragmentFilePtr, currPos);
                                if (*n == 0)
                                {
                                    return  INSUFFICIENT_DATA;
                                }
                                else
                                {
                                    *n = totalSampleRead;
                                    return EVERYTHING_FINE;
                                }
                            }
                            if (atomType == MOVIE_FRAGMENT_ATOM)
                            {
                                TOsclFileOffset moofStartOffset = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                moofStartOffset -= DEFAULT_ATOM_SIZE;

                                PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "atomType=MovieFrgment, moofStart=%d", moofStartOffset));

                                parseMoofCompletely = true;

                                PV_MP4_FF_NEW(_movieFragmentFilePtr->auditCB, MovieFragmentAtom, (_movieFragmentFilePtr, _pFileSessionsForMediaDataRetrievalVec, atomSize, atomType, _pTrackDurationContainer, _pTrackExtendsAtomVec, parseMoofCompletely, moofParsingCompleted, countOfTrunsParsed, ipTrckEncryptnBxCntr), _pMovieFragmentAtom);

                                if (!_pMovieFragmentAtom->MP4Success())
                                {
                                    _success = false;
                                    _mp4ErrorCode = _pMovieFragmentAtom->GetMP4Error();
                                    oAllMoofExhausted = true;
                                    break;
                                }
                                _pMovieFragmentAtom->setParent(this);
                                count -= _pMovieFragmentAtom->getSize();
                                if (moofToBeParsed)
                                {
                                    (*_pMovieFragmentAtomVec)[moofIndex] = _pMovieFragmentAtom;
                                    (*_pMoofOffsetVec)[moofIndex] = moofStartOffset;
                                }
                                else
                                {
                                    _pMoofOffsetVec->push_back(moofStartOffset);
                                    _pMovieFragmentAtomVec->push_back(_pMovieFragmentAtom);
                                }
                                _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);

                                break;
                            }
                            else if (atomType == MEDIA_DATA_ATOM)
                            {
                                if (atomSize == 1)
                                {
                                    uint64 largeSize = 0;
                                    AtomUtils::read64(_movieFragmentFilePtr, largeSize);
                                    uint32 size =
                                        Oscl_Int64_Utils::get_uint64_lower32(largeSize);
                                    count -= size;
                                    size -= 8; //for large size
                                    atomSize = size;
                                }
                                else
                                {
                                    if (atomSize < DEFAULT_ATOM_SIZE)
                                    {
                                        _success = false;
                                        oAllMoofExhausted = true;
                                        _mp4ErrorCode = ZERO_OR_NEGATIVE_ATOM_SIZE;
                                        break;
                                    }
                                    if (count < (int32)atomSize)
                                    {
                                        _success = false;
                                        oAllMoofExhausted = true;
                                        _mp4ErrorCode = INSUFFICIENT_DATA;
                                        AtomUtils::seekFromStart(_movieFragmentFilePtr, currPos);
                                        ret = _mp4ErrorCode;
                                        break;
                                    }
                                    count -= atomSize;
                                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "atomType=MediaData"));
                                }
                                atomSize -= DEFAULT_ATOM_SIZE;
                                AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);

                            }

                            else
                            {
                                if (count > 0)
                                {
                                    count -= atomSize;
                                    atomSize -= DEFAULT_ATOM_SIZE;
                                    AtomUtils::seekFromCurrPos(_movieFragmentFilePtr, atomSize);
                                    _ptrMoofEnds = AtomUtils::getCurrentFilePosition(_movieFragmentFilePtr);
                                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "atomType=other, atomSize=%d, count=%d",
                                            atomSize, count));
                                }
                                break;
                            }
                        }
                        if (count <= 0)
                        {
                            oAllMoofExhausted = true;
                            if (_pMovieFragmentAtomVec->size() < _peekMovieFragmentIdx[moofIdx])
                                break;
                        }
                    }

                    int32 return1 = 0;
                    MovieFragmentAtom *pMovieFragmentAtom = NULL;
                    uint32 movieFragmentIdx = _peekMovieFragmentIdx[moofIdx];

                    if (movieFragmentIdx < _pMovieFragmentAtomVec->size())
                        pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[movieFragmentIdx];
                    else
                        continue;

                    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES(
                        (0, "peekMvFgIdx=%d, peekSeqNum=%d, *n=%d, oAllMoofExhausted=%d, movieFragment=0x%x",
                         _peekMovieFragmentIdx[moofIdx], _peekMovieFragmentSeqIdx[moofIdx], *n, oAllMoofExhausted,
                         pMovieFragmentAtom));


                    if (pMovieFragmentAtom != NULL)
                    {
                        uint32 seqNum = pMovieFragmentAtom->getSequenceNumber();
                        if (seqNum == _peekMovieFragmentSeqIdx[moofIdx])
                        {
                            TrackFragmentAtom *trackfragment = pMovieFragmentAtom->getTrackFragmentforID(trackID);
                            if (trackfragment != NULL)
                            {
                                if (trackfragment->getTrackId() == trackID)
                                {
                                    return1 = pMovieFragmentAtom->peekNextBundledAccessUnits(trackID, n, totalSampleRead, mInfo);
                                    totalSampleRead += *n;
                                    if (return1 != END_OF_TRACK)
                                    {
                                        *n = totalSampleRead;
                                        return return1;
                                    }
                                    else
                                    {
                                        _peekMovieFragmentSeqIdx[moofIdx]++;
                                        if (samplesTobeRead >= *n)
                                        {
                                            samplesTobeRead = samplesTobeRead - *n;
                                            *n = samplesTobeRead;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                _peekMovieFragmentIdx[moofIdx]++;
                                _peekMovieFragmentSeqIdx[moofIdx]++;
                                /**n = 0;
                                return NO_SAMPLE_IN_CURRENT_MOOF;*/
                                continue;
                            }
                        }
                    }
                    _peekMovieFragmentIdx[moofIdx]++;

                }
            }
        }
        if (END_OF_TRACK == ret)
        {
            if (MoreMoofAtomsExpected(_movieFragmentSeqIdx[moofIdx]))
            {
                //We have run out of moofs and theres no data to be parsed
                if (*n == 0)
                {
                    ret = INSUFFICIENT_DATA;
                }
                else
                {
                    *n = totalSampleRead;
                    ret = EVERYTHING_FINE;
                }
            }
        }
        return ret;
    }
    else
    {
        return -1;
    }
}


uint32 Mpeg4File::getSampleCountInTrack(uint32 id)
{
    uint32 nTotalSamples = 0;
    if (_pmovieAtom != NULL)
    {
        nTotalSamples = (_pmovieAtom->getSampleCountInTrack(id));
        if (!_isMovieFragmentsPresent)
            return nTotalSamples;

        if (_parsing_mode == 0)
        {
            if (_pMovieFragmentAtomVec->size() > 0)
            {
                for (uint32 idx = 0; idx < _pMovieFragmentAtomVec->size(); idx++)
                {
                    MovieFragmentAtom *pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[idx];
                    if (pMovieFragmentAtom != NULL)
                    {
                        nTotalSamples += pMovieFragmentAtom->getTotalSampleInTraf(id);
                    }
                }
                return nTotalSamples;
            }
        }
        return nTotalSamples;
    }
    return 0;
}


bool Mpeg4File::IsTFRAPresentForTrack(uint32 TrackId, bool oVideoAudioTextTrack)
{
    if (_pMovieFragmentRandomAccessAtomVec != NULL)
    {
        for (uint32 idx = 0; idx < _pMovieFragmentRandomAccessAtomVec->size(); idx++)
        {

            MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = (*_pMovieFragmentRandomAccessAtomVec)[idx];
            return pMovieFragmentRandomAccessAtom->IsTFRAPresentForTrack(TrackId, oVideoAudioTextTrack);
        }
    }
    return false;
}


/*
This function has been modified to check the entry count in TFRA for all tracks are equal.
The code change is under macro DISABLE_REPOS_ON_CLIPS_HAVING_UNEQUAL_TFRA_ENTRY_COUNT
*/
bool Mpeg4File::IsTFRAPresentForAllTrack(uint32 numTracks, uint32 *trackList)
{

#if (DISABLE_REPOS_ON_CLIPS_HAVING_UNEQUAL_TFRA_ENTRY_COUNT)
    bool oVideoAudioTextTrack  = false;
// This flag will be true for Video in AVT,VT & AV and for Audio in AT.
// Based on this flag IsTFRAPresentForTrack() functions behaviour is changing
// We are comparing the entry count for all the tracks to entry count of V in case of Vonly,AVT,VT & AV clips
// and in case of Aonly & AT clips, entry count for all the tracks is compared with entry count of audio. For Tonly
// clips, entry count for text track is compared with its own entry count.

    // Support for clips having Video track.
    for (int32 i = 0; i < numTracks; i++)
    {
        uint32 trackID = trackList[i];
        if (getTrackMediaType(trackID) == MEDIA_TYPE_VISUAL)
        {
            oVideoAudioTextTrack  = true;
            if (IsTFRAPresentForTrack(trackID, oVideoAudioTextTrack) == false)
            {
                return false;
            }
            break;
        }
    }
    // Support for clips having Audio track and no Video track.
    if (!oVideoAudioTextTrack)
    {
        for (int32 i = 0; i < numTracks; i++)
        {
            uint32 trackID = trackList[i];
            if (getTrackMediaType(trackID) == MEDIA_TYPE_AUDIO)
            {
                oVideoAudioTextTrack = true;
                if (IsTFRAPresentForTrack(trackID, oVideoAudioTextTrack) == false)
                {
                    return false;
                }
                break;
            }
        }
    }
    // Support for clips having only Text track.
    if (!oVideoAudioTextTrack && numTracks == 1)
    {
        for (uint32 i = 0; i < numTracks; i++)
        {
            uint32 trackID = trackList[i];
            if (getTrackMediaType(trackID) == MEDIA_TYPE_TEXT)
            {
                oVideoAudioTextTrack = true;
                if (IsTFRAPresentForTrack(trackID, oVideoAudioTextTrack) == false)
                {
                    return false;
                }
                break;
            }
        }
    }
#endif // DISABLE_REPOS_ON_CLIPS_HAVING_UNEQUAL_TFRA_ENTRY_COUNT
    for (uint32 idx = 0; idx < numTracks; idx++)
    {
        uint32 trackID = trackList[idx];
        // second argument is false always
        if (IsTFRAPresentForTrack(trackID, false) == false)
        {
            return false;
        }
    }
    return true;
}

void Mpeg4File::resetPlayback()
{
    if (_pmovieAtom == NULL)
        return;

    isResetPlayBackCalled = true;

    _pmovieAtom->resetPlayback();

    if (_isMovieFragmentsPresent)
    {
        if (_pMovieFragmentAtomVec != NULL)
        {
            int numTracks = _pmovieAtom->getNumTracks();
            uint32 *trackList  = (uint32 *) oscl_malloc(sizeof(uint32) * numTracks);
            if (!trackList)
                return;       // malloc failed

            _pmovieAtom->getTrackWholeIDList(trackList);
            for (int i = 0; i < numTracks; i++)
            {
                uint32 trackID = trackList[i];
                uint32 moofIdx = getIndexForTrackID(trackID);
                _peekMovieFragmentIdx[moofIdx] = 0;
                _movieFragmentIdx[moofIdx] = 0;
                _movieFragmentSeqIdx[moofIdx] = 1;
                _peekMovieFragmentSeqIdx[moofIdx] = 1;
            }
            currMoofNum = 1;
            oscl_free(trackList);

            for (uint32 idx = 0; idx < _pMovieFragmentAtomVec->size(); idx++)
            {
                MovieFragmentAtom *pMovieFragmentAtom = (*_pMovieFragmentAtomVec)[idx];
                if (pMovieFragmentAtom != NULL)
                    pMovieFragmentAtom->resetPlayback();
            }

            if (moofParsingCompleted)
            {
            }
            else
            {
                _ptrMoofEnds = (*_pMoofOffsetVec)[_pMoofOffsetVec->size()-1];
                uint32 i = _pMovieFragmentAtomVec->size();
                _pMoofOffsetVec->pop_back();
                _pMovieFragmentAtomVec->pop_back();
                PV_MP4_FF_DELETE(NULL, MovieFragmentAtom , (*_pMovieFragmentAtomVec)[i-1]);
                parseMoofCompletely = true;
                moofParsingCompleted = true;
                moofSize = 0;
                moofType = UNKNOWN_ATOM;
                moofCount = 0;
                moofPtrPos = 0;
            }

        }
    }
}

bool Mpeg4File::repositionFromMoof(uint32 time, uint32 trackID)
{
    const uint64 trackDuration = getTrackMediaDurationForMovie(trackID);
    MediaClockConverter mcc1(1000);
    mcc1.set_clock(time, 0);
    uint64 convertedTime = mcc1.get_converted_ts64(getTrackMediaTimescale(trackID));

    if (_isMovieFragmentsPresent)
    {
        if (IsTFRAPresentForTrack(trackID, false) == false)
        {
            return false;
        }
        if (convertedTime >= trackDuration)
        {
            return true; //repos in moof
        }
    }
    return false; //repos in moov
}

MP4_ERROR_CODE Mpeg4File::CancelNotificationSync()
{
    PVMF_MP4FFPARSER_LOGMEDIASAMPELSTATEVARIABLES((0, "Mpeg4File::CancelNotificationSync"));

    bool retVal = _commonFilePtr->_pvfile.CancelNotificationSync();

    if (retVal)
    {
        return EVERYTHING_FINE;
    }
    else
    {
        return DEFAULT_ERROR;
    }
}

bool Mpeg4File::CreateDataStreamSessionForExternalDownload(OSCL_wString& aFilename,
        PVMFCPMPluginAccessInterfaceFactory* aCPMAccessFactory,
        OsclFileHandle* aHandle,
        Oscl_FileServer* aFileServSession)
{
    OsclAny*ptr = oscl_malloc(sizeof(MP4_FF_FILE));
    if (ptr == NULL)
    {
        _success = false;
        _mp4ErrorCode = MEMORY_ALLOCATION_FAILED;
        return false;
    }
    _commonFilePtr = OSCL_PLACEMENT_NEW(ptr, MP4_FF_FILE());

    if (_commonFilePtr != NULL)
    {
        _commonFilePtr->_fileServSession = aFileServSession;
        _commonFilePtr->_pvfile.SetCPM(aCPMAccessFactory);
        _commonFilePtr->_pvfile.SetFileHandle(aHandle);

        if (AtomUtils::OpenMP4File(aFilename,
                                   Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                   _commonFilePtr) != 0)
        {
            return false;
        }

        TOsclFileOffset fileSize;
        AtomUtils::getCurrentFileSize(_commonFilePtr, fileSize);
        _commonFilePtr->_fileSize = (int32)fileSize;
    }
    return true;
}

void Mpeg4File::DestroyDataStreamForExternalDownload()
{
    if (_commonFilePtr != NULL)
    {
        if (_commonFilePtr->IsOpen())
        {
            AtomUtils::CloseMP4File(_commonFilePtr);
        }
        oscl_free(_commonFilePtr);
        _commonFilePtr = NULL;
    }
}

PVMFStatus Mpeg4File::InitMetaData(PVMFMetadataList* aAvailableMetadataKeys)
{
    MP4FFParserOriginalCharEnc charType;
    // Populate the available metadata keys based on what's available in the MP4 file

    int32 iNumTracks = getNumTracks();
    uint32 iIdList[16];

    if (iNumTracks != getTrackIDList(iIdList, iNumTracks))
    {
        return PVMFFailure;
    }
    for (int32 i = iNumTracks - 1; i >= 0; i--)
    {
        //track id is a one based index
        char indexparam[18];
        oscl_snprintf(indexparam, 18, ";index=%d", i);
        indexparam[17] = '\0';

        uint32 trackID = iIdList[i];

        OSCL_HeapString<OsclMemAllocator> trackMIMEType;

        getTrackMIMEType(trackID, (OSCL_String&)trackMIMEType);

        if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000))) == 0)
        {
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_VIDEO_PROFILE_KEY, indexparam);
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_VIDEO_LEVEL_KEY, indexparam);
        }

        if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0))
        {
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_FRAME_RATE_KEY, indexparam);
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_VIDEO_FORMAT_KEY, indexparam);
        }

        if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_MPEG4_AUDIO, oscl_strlen(PVMF_MIME_MPEG4_AUDIO)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR, oscl_strlen(PVMF_MIME_AMR)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR_IETF, oscl_strlen(PVMF_MIME_AMR_IETF)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMRWB_IETF, oscl_strlen(PVMF_MIME_AMRWB_IETF)) == 0))
        {
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_AUDIO_FORMAT_KEY, indexparam);
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_AUDIO_NUMCHANNELS_KEY, indexparam);
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_SAMPLERATE_KEY, indexparam);
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_AUDIO_BITS_PER_SAMPLE_KEY, indexparam);
        }
    }

    if (numAuthor > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_AUTHOR_KEY);
    }

    //Common Keys
    if (numAlbum > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_ALBUM_KEY);
    }
    if (numComment > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_COMMENT_KEY);
    }
    if (numGenre > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_GENRE_KEY);
    }
    if (numTitle > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TITLE_KEY);
    }
    if (numCopyright > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_COPYRIGHT_KEY);
    }
    if (numYear > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_YEAR_KEY);
    }
    if (numArtist > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_ARTIST_KEY);
    }
    if (numDescription > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_DESCRIPTION_KEY);
    }

    if (numRating > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_RATING_KEY);
    }


    if (getNumAssetInfoLocationAtoms() > 0)
    {
        uint32 numLocations = getNumAssetInfoLocationAtoms();
        if (numLocations > 0)
        {
            //PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_LOCATION_KEY);
            // Create the parameter string for the index range
            char indexparam[18];
            oscl_snprintf(indexparam, 18, ";index=0...%d", (numLocations - 1));
            indexparam[17] = '\0';

            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_LOCATION_KEY, indexparam);
        }

    }
    if (getNumAssetInfoKeyWordAtoms() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_KEYWORD_KEY);
    }
    if (getNumAssetInfoClassificationAtoms() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_CLASSIFICATION_KEY);
    }
    if (getCompatibiltyMajorBrand() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_MAJORBRAND_KEY);
    }

    if (getCompatibiltyList() != NULL)
    {
        if (getCompatibiltyList()->size() > 0)
        {
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_COMPATIBLEBRAND_KEY);
        }
    }

    if (getPVVersion(charType).get_size() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_VERSION_KEY);
    }

    if (getCreationDate(charType).get_size() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_DATE_KEY);
    }

    if (getMovieDuration() > (uint64)0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_DURATION_KEY);

    }

    if (getITunesBeatsPerMinute() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TEMPO_KEY);
    }

    if (getITunesCDIdentifierData(0).get_size() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_CDDBID_KEY);
    }
    if (getITunesGroupData().get_size() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_GROUPING_KEY);
    }
    if (getITunesImageData() != NULL)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_GRAPHICS_KEY);
    }
    if (getITunesLyrics().get_size() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_LYRICS_KEY);
    }
    if (getITunesNormalizationData().get_size() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_FREEFORMDATA_KEY);
    }
    if (getITunesThisDiskNo() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_DISKDATA_KEY);
    }
    if (getITunesThisTrackNo() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKDATA_KEY);
    }
    if (getITunesTool().get_size() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TOOL_KEY);
    }
    if (getITunesWriter().get_size() > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_WRITER_KEY);
    }
    if (IsITunesCompilationPart() != false)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_COMPILATION_KEY);
    }

    PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_CLIP_TYPE_KEY);

    PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_RANDOM_ACCESS_DENIED_KEY);

    PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_IS_MOOF_KEY);

    int32 numtracks = getNumTracks();
    if (numtracks > 0)
    {
        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_NUMTRACKS_KEY);

        //Create the parameter string for the index range
        char indexparam[18];
        oscl_snprintf(indexparam, 18, ";index=0...%d", (numtracks - 1));
        indexparam[17] = '\0';

        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_TYPE_KEY, indexparam);

        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_TRACKID_KEY, indexparam);

        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_DURATION_KEY, indexparam);

        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_BITRATE_KEY, indexparam);

        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_SAMPLECOUNT_KEY, indexparam);

        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_SELECTED_KEY, indexparam);

        if (getITunesThisTrackNo() > 0)
        {
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_TRACK_NUMBER_KEY, indexparam);
        }

        PushToAvailableMetadataKeysList(aAvailableMetadataKeys, PVMP4METADATA_TRACKINFO_NUM_KEY_SAMPLES_KEY, indexparam);
    }

    //There may be some keys which are present in ID3 but not in MP4 FF lib. Need to add them as well.
    PvmiKvpSharedPtrVector _pID3Framevector;
    GetID3MetaData(_pID3Framevector);
    uint32 num_frames = _pID3Framevector.size();
    //Ignoring the Keys from ID3 list which are already pushed
    bool isKeyAlreadyPushed = false;
    for (uint32 j = 0; j < num_frames; j++)
    {
        uint32 totalKeysPushed = aAvailableMetadataKeys->size();
        for (uint32 k = 0; k < totalKeysPushed; k++)
        {
            if ((oscl_strstr(_pID3Framevector[j]->key, (*aAvailableMetadataKeys)[k].get_cstr())) != NULL)
            {
                isKeyAlreadyPushed = true;
                break;
            }
        }
        if (!isKeyAlreadyPushed)
            PushToAvailableMetadataKeysList(aAvailableMetadataKeys, _pID3Framevector[j]->key);

        isKeyAlreadyPushed = false;
    }
    return PVMFSuccess;
}



void Mpeg4File::PushToAvailableMetadataKeysList(PVMFMetadataList* aAvailableMetadataKeys, const char* aKeystr, char* aOptionalParam)
{
    if (aKeystr == NULL)
    {
        return;
    }

    if (aOptionalParam)
    {
        aAvailableMetadataKeys->push_front(aKeystr);
        aAvailableMetadataKeys[0][0] += aOptionalParam;
    }

    else
    {
        aAvailableMetadataKeys->push_front(aKeystr);
    }
}


PVMFStatus Mpeg4File::GetIndexParamValues(const char* aString, uint32& aStartIndex, uint32& aEndIndex)
{
    // This parses a string of the form "index=N1...N2" and extracts the integers N1 and N2.
    // If string is of the format "index=N1" then N2=N1

    if (aString == NULL)
    {
        return PVMFErrArgument;
    }

    // Go to end of "index="
    char* n1string = (char*)aString + 6;

    PV_atoi(n1string, 'd', oscl_strlen(n1string), aStartIndex);

    const char* n2string = oscl_strstr(aString, _STRLIT_CHAR("..."));

    if (n2string == NULL)
    {
        aEndIndex = aStartIndex;
    }
    else
    {
        // Go to end of "index=N1..."
        n2string += 3;

        PV_atoi(n2string, 'd', oscl_strlen(n2string), aEndIndex);
    }

    return PVMFSuccess;
}

uint32 Mpeg4File::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    uint32 numvalentries = 0;
    if (aKeyList.size() == 0)
    {
        return numvalentries;
    }

    int32 iNumTracks = getNumTracks();
    uint32 iIdList[16];
    if (iNumTracks != getTrackIDList(iIdList, iNumTracks))
    {
        return numvalentries;
    }
    // Retrieve the track ID list
    OsclExclusiveArrayPtr<uint32> trackidlistexclusiveptr;
    uint32* trackidlist = NULL;
    uint32 numTracks = (uint32)(iNumTracks);
    PVMFStatus status = CreateNewArray(&trackidlist, numTracks);
    if (PVMFErrNoMemory == status)
    {
        OSCL_LEAVE(PVMFErrNoMemory);
    }
    oscl_memset(trackidlist, 0, sizeof(uint32)*(numTracks));
    getTrackIDList(trackidlist, numTracks);
    trackidlistexclusiveptr.set(trackidlist);


    //Count ID3 specific values
    PvmiKvpSharedPtrVector _pID3Framevector;
    GetID3MetaData(_pID3Framevector);
    numvalentries += _pID3Framevector.size();

    uint32 numkeys = aKeyList.size();
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_TOOL_KEY) == 0)
        {
            // Tool
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_WRITER_KEY) == 0)
        {
            // Writer
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_GROUPING_KEY) == 0)
        {
            // Grouping
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKDATA_KEY) == 0)
        {
            // Trackdata
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_COMPILATION_KEY) == 0) && (IsITunesCompilationPart() == true))
        {
            //Compilation
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_TEMPO_KEY) == 0)
        {
            // Tempo
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_DISKDATA_KEY) == 0)
        {
            // Disk data
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_LYRICS_KEY) == 0)
        {
            // Lyrics
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_FREEFORMDATA_KEY) == 0)
        {
            // Free form data
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_CDDBID_KEY) == 0)
        {
            // CD Identifier
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }

        if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_AUTHOR_KEY) == 0) &&
                (numAuthor > 0))
        {
            // Author
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numAuthor;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_ALBUM_KEY) == 0) &&
                 (numAlbum > 0))
        {
            // Album
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numAlbum;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_CLIP_TYPE_KEY) == 0)
        {
            // clip-type
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_COMMENT_KEY) == 0) &&
                 (numComment > 0))
        {
            // Comment
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numComment;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_GRAPHICS_KEY) == 0)
        {
            // Graphic
            // Increment the counter for the number of values found so far
            if (getITunesImageData() != NULL)
            {
                ++numvalentries;
            }

        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_RANDOM_ACCESS_DENIED_KEY) == 0)
        {
            /*
             * Random Access
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_IS_MOOF_KEY) == 0)
        {
            /*
             * is-moof
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;
        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_LOCATION_KEY) == 0)
        {
            /*
             * location
             * Determine the index requested. Default to all pictures */

            uint32 NumLocations = getNumAssetInfoLocationAtoms();

            if (!NumLocations)
                break;

            uint32 startindex = 0;
            uint32 endindex = (uint32)(NumLocations - 1);

            /* Check if the index parameter is present */
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                /* Retrieve the index values */
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            /* Validate the indices */
            if (startindex > endindex || startindex >= (uint32)NumLocations || endindex >= (uint32)NumLocations)
            {
                break;
            }
            /* Return a KVP for each index */
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;
                /* Increment the counter for the number of values found so far */
                ++numvalentries;
            }
        }

        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_ARTIST_KEY) == 0) &&
                 (numArtist > 0))
        {
            // Artist
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numArtist;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_GENRE_KEY) == 0) &&
                 (numGenre > 0))
        {
            // Genre
            numvalentries += numGenre;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_KEYWORD_KEY) == 0)
        {
            int32 numAssetInfoKeyword = getNumAssetInfoKeyWordAtoms();
            for (int32 idx = 0; idx < numAssetInfoKeyword; idx++)
            {
                int32 AssetInfoKeywordCount = getAssetInfoNumKeyWords(idx);
                for (int32 idy = 0; idy < AssetInfoKeywordCount; idy++)
                {

                    // Increment the counter for the number of values found so far
                    ++numvalentries;
                }
            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_CLASSIFICATION_KEY) == 0)
        {

            int32 numAssetInfoClassification = getNumAssetInfoClassificationAtoms();
            // classification
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numAssetInfoClassification;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_MAJORBRAND_KEY) == 0)
        {
            // MAJOR BRAND
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_COMPATIBLEBRAND_KEY) == 0)
        {
            // COMPATIBLE BRAND
            // Increment the counter for the number of values found so far
            Oscl_Vector<uint32, OsclMemAllocator> *Compatiblebrand_Vec = getCompatibiltyList();
            if (Compatiblebrand_Vec)
            {
                numvalentries += Compatiblebrand_Vec->size();
            }
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_TITLE_KEY) == 0) &&
                 (numTitle > 0))
        {
            // Title
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numTitle;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_DESCRIPTION_KEY) == 0)  &&
                 (numDescription > 0))
        {
            // Description
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numDescription;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_RATING_KEY) == 0) &&
                 (numRating > 0))
        {
            // Rating
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numRating;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_COPYRIGHT_KEY) == 0) &&
                 (numCopyright > 0))
        {
            // Copyright
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numCopyright;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_VERSION_KEY) == 0 &&
                 numVersion > 0)
        {
            // Version
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numVersion;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_DATE_KEY) == 0 &&
                 numDate > 0)
        {
            // Date
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numDate;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_DURATION_KEY) == 0)
        {
            // Movie Duration
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_NUMTRACKS_KEY) == 0 &&
                 getNumTracks() > 0)
        {
            // Number of tracks
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_YEAR_KEY) == 0) &&
                 (numYear > 0))
        {
            // year
            // Increment the counter for the number of values found so far
            numvalentries = numvalentries + numYear;
        }
        else if ((oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_PROFILE_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_LEVEL_KEY) != NULL))
        {
            // Determine the index requested.
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)(numtracks) || endindex >= (uint32)(numtracks))
            {
                break;
            }

            uint32 iIdList[16];
            getTrackIDList(iIdList, (numtracks < 16) ? numtracks : 16);
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                uint32 trackID = iIdList[i];

                OSCL_HeapString<OsclMemAllocator> trackMIMEType;
                getTrackMIMEType(trackID, trackMIMEType);

                if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_FRAME_RATE_KEY) != NULL)
        {
            // frame-rate
            // Determine the index requested.
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)(numtracks) || endindex >= (uint32)(numtracks))
            {
                break;
            }

            uint32 iIdList[16];
            getTrackIDList(iIdList, (numtracks < 16) ? numtracks : 16);
            // Go through all tracks
            for (uint32 i = startindex; i <= endindex; i++)
            {
                uint32 trackID = iIdList[i];

                OSCL_HeapString<OsclMemAllocator> trackMIMEType;

                getTrackMIMEType(trackID, trackMIMEType);

                if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0) ||
                        (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0) ||
                        (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0))
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;
                }
            }
        }
        else if ((oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_TYPE_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_TRACKID_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_DURATION_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_BITRATE_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_SELECTED_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_WIDTH_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_HEIGHT_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_NUM_KEY_SAMPLES_KEY) != NULL))
        {
            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Increment the counter for the number of values found so far
            numvalentries += (endindex + 1 - startindex);
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_TRACK_NUMBER_KEY) != NULL)
        {
            uint32 numCDTrackNumber = 0;

            if (getITunesThisTrackNo() > 0)
                numCDTrackNumber++;


            if (numCDTrackNumber > 0)
            {
                // Track Number

                // Determine the index requested. Default to all tracks
                // Check if the file has at least one track
                int32 numtracks = getNumTracks();
                if (numtracks <= 0)
                {
                    break;
                }
                uint32 startindex = 0;
                uint32 endindex = (uint32)numtracks - 1;
                // Check if the index parameter is present
                const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
                if (indexstr != NULL)
                {
                    // Retrieve the index values
                    GetIndexParamValues(indexstr, startindex, endindex);
                }
                // Validate the indices
                if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
                {
                    break;
                }

                // Increment the counter for the number of values found so far
                numvalentries += (endindex + 1 - startindex);
                numvalentries = numCDTrackNumber * numvalentries;
            }
        }
        else if ((oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_FORMAT_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_FORMAT_KEY) != NULL))
        {
            // Audio or video track format
            // Set index for track type
            uint32 tracktype = 0; // 0 unknown, 1 video, 2 audio
            if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_FORMAT_KEY) != NULL)
            {
                tracktype = 1;
            }
            else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_FORMAT_KEY) != NULL)
            {
                tracktype = 2;
            }

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                OSCL_HeapString<OsclMemAllocator> trackMIMEType;

                getTrackMIMEType(trackidlist[i], trackMIMEType);

                if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0)
                {
                    if (tracktype == 1)
                    {
                        ++numvalentries;
                    }
                }
                else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0)
                {
                    if (tracktype == 1)
                    {
                        ++numvalentries;
                    }
                }
                else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0)
                {
                    if (tracktype == 1)
                    {
                        ++numvalentries;
                    }
                }
                else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_MPEG4_AUDIO, oscl_strlen(PVMF_MIME_MPEG4_AUDIO)) == 0)
                {
                    if (tracktype == 2)
                    {
                        ++numvalentries;
                    }
                }
                else if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR_IETF, oscl_strlen(PVMF_MIME_AMR_IETF)) == 0) ||
                         (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMRWB_IETF, oscl_strlen(PVMF_MIME_AMRWB_IETF)) == 0))
                {
                    if (tracktype == 2)
                    {
                        ++numvalentries;
                    }
                }
            }
        }
        else if ((oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_NUMCHANNELS_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_BITS_PER_SAMPLE_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_SAMPLERATE_KEY) != NULL))
        {
            // Sampling rate is only for video tracks
            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                if (getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_AUDIO)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_SAMPLECOUNT_KEY) != NULL)
        {
            // Sample count

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                // Increment the counter for the number of values found so far
                numvalentries++;
            }
        }
    }
    return numvalentries;
}

PVMFStatus Mpeg4File::GetMetadataValues(PVMFMetadataList& aKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                        uint32 aStartingValueIndex, int32 aMaxValueEntries, int32 &numentriesadded,
                                        uint32 &aID3ValueCount)
{
    uint32 numKeys = aKeyList.size();
    OSCL_wHeapString<OsclMemAllocator> valuestring = NULL;
    MP4FFParserOriginalCharEnc charType = ORIGINAL_CHAR_TYPE_UNKNOWN;
    uint16 iLangCode = 0;

    if (aStartingValueIndex > (numKeys - 1) || numKeys <= 0 || aMaxValueEntries == 0)
    {
        // Don't do anything
        return PVMFErrArgument;
    }

    uint32 numvalentries = 0;
    uint32 lcv = 0;

    // Retrieve the track ID list
    OsclExclusiveArrayPtr<uint32> trackidlistexclusiveptr;
    uint32* trackidlist = NULL;
    uint32 numTracks = (uint32)(getNumTracks());
    PVMFStatus status = CreateNewArray(&trackidlist, numTracks);
    if (PVMFErrNoMemory == status)
    {
        return PVMFErrNoMemory;
    }
    oscl_memset(trackidlist, 0, sizeof(uint32)*(numTracks));
    getTrackIDList(trackidlist, numTracks);
    trackidlistexclusiveptr.set(trackidlist);

    //Populate ID3 values
    PvmiKvpSharedPtrVector _pID3Framevector;
    GetID3MetaData(_pID3Framevector);
    uint32 num_frames = _pID3Framevector.size();

    for (uint32 j = 0; j < numKeys; j++)
    {
        for (uint32 i = 0; i < num_frames; i++)
        {
            if (oscl_strstr(_pID3Framevector[i]->key, aKeyList[j].get_cstr()) != NULL)
            {
                PvmiKvp *kvp = _pID3Framevector[i];
                int32 leavecode = AddToValueList(aValueList, *kvp);
                if (leavecode == 0)
                    ++numentriesadded;
                // Check if the max number of value entries were added
                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                {
                    // Maximum number of values added so break out of the loop
                    break;
                }

            }
        }
    }
    aID3ValueCount = numentriesadded;

    // Check if the max number of value entries were added
    if (aMaxValueEntries > -1 && numentriesadded >= aMaxValueEntries)
    {
        //If required number of entries have already been added
        return PVMFSuccess;
    }


    if (numvalentries >= (uint32)aMaxValueEntries && aMaxValueEntries > -1)
    {
        //If required number of entries have already been added
        return PVMFSuccess;
    }

    for (lcv = 0; lcv < numKeys; lcv++)
    {
        int32 leavecode = 0;
        PvmiKvp KeyVal;
        KeyVal.key = NULL;
        KeyVal.value.pWChar_value = NULL;
        KeyVal.value.pChar_value = NULL;
        int32 idx = 0;
        char orig_char_enc[2][7] = {"UTF-8", "UTF-16"};

        bool IsMetadataValAddedBefore = false;

        if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_AUTHOR_KEY) == 0)
        {
            // Author

            if (numAuthor > 0)
            {
                for (idx = 0; idx < (int32)numAuthor ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (!getAuthor(idx, valuestring, iLangCode, charType))
                        {
                            return PVMFFailure;
                        }


                        char lang_param[43];
                        if (iLangCode != 0)
                        {
                            int8 LangCode[4];
                            getLanguageCode(iLangCode, LangCode);
                            LangCode[3] = '\0';
                            oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                            lang_param[20] = '\0';
                        }
                        else
                        {
                            lang_param[0] = '\0';
                        }
                        KeyVal.key = NULL;
                        KeyVal.value.pWChar_value = NULL;
                        KeyVal.value.pChar_value = NULL;
                        if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                        {
                            char char_enc_param[22];
                            oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                            char_enc_param[21] = '\0';
                            oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                        }
                        PVMFStatus retval =
                            PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                    PVMP4METADATA_AUTHOR_KEY,
                                    valuestring,
                                    lang_param);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }
                        // Add the KVP to the list if the key string was created
                        if (KeyVal.key != NULL)
                        {
                            leavecode = AddToValueList(aValueList, KeyVal);
                            if (leavecode != 0)
                            {
                                if (KeyVal.value.pWChar_value != NULL)
                                {
                                    OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                    KeyVal.value.pWChar_value = NULL;
                                }

                                OSCL_ARRAY_DELETE(KeyVal.key);
                                KeyVal.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                            {

                                return PVMFSuccess;
                            }
                        }

                    }
                }

            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_TOOL_KEY) == 0)
        {

            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;

            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {

                OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesTool();
                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                            PVMP4METADATA_TOOL_KEY,
                            valuestring);

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {
                    IsMetadataValAddedBefore = false;

                }

            }

        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_WRITER_KEY) == 0)
        {
            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {


                OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesWriter();
                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                            PVMP4METADATA_WRITER_KEY,
                            valuestring);

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {
                    IsMetadataValAddedBefore = false;

                }

            }
        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_GROUPING_KEY) == 0)
        {


            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;

            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {


                OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesGroupData();
                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                            PVMP4METADATA_GROUPING_KEY,
                            valuestring);

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {
                    IsMetadataValAddedBefore = false;

                }

            }

        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKDATA_KEY) == 0)
        {

            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {

                uint32 numtracks = getITunesThisTrackNo();
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVMP4METADATA_TRACKDATA_KEY, numtracks);


                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {

                    IsMetadataValAddedBefore = false;

                }

            }

        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_COMPILATION_KEY) == 0)
        {

            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;

            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {


                bool compilationPart = IsITunesCompilationPart();
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal, PVMP4METADATA_COMPILATION_KEY, compilationPart);

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {

                    IsMetadataValAddedBefore = false;

                }

            }

        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_TEMPO_KEY) == 0)
        {

            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;

            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {

                uint32 beatsperminute = getITunesBeatsPerMinute();
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVMP4METADATA_TEMPO_KEY, beatsperminute);


                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {

                    IsMetadataValAddedBefore = false;

                }

            }

        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_GRAPHICS_KEY) == 0)
        {

            KeyVal.key = NULL;
            KeyVal.value.key_specific_value = NULL;

            // Increment the counter for the number of values found so far

            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {

                PvmfApicStruct*  imagedata = getITunesImageData();

                if (imagedata != NULL)
                {
                    PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForKSVValue(KeyVal, PVMP4METADATA_GRAPHICS_KEY, OSCL_STATIC_CAST(OsclAny*, imagedata));

                    if (retval != PVMFSuccess && retval != PVMFErrArgument)
                    {
                        break;
                    }
                    else
                    {
                        // Add the KVP to the list if the key string was created
                        if (KeyVal.key != NULL)
                        {
                            leavecode = AddToValueList(aValueList, KeyVal);
                            if (leavecode != 0)
                            {
                                if (KeyVal.value.key_specific_value)
                                {
                                    PvmfApicStruct* apicStruct =
                                        OSCL_STATIC_CAST(PvmfApicStruct*, KeyVal.value.key_specific_value);
                                    DeleteAPICStruct(apicStruct);
                                }

                                OSCL_ARRAY_DELETE(KeyVal.key);
                                KeyVal.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                            {

                                return PVMFSuccess;
                            }
                        }
                    }
                }

            }

        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_DISKDATA_KEY) == 0)
        {

            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;

            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {


                uint32 disknum = getITunesThisDiskNo();
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVMP4METADATA_DISKDATA_KEY, disknum);



                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {

                    IsMetadataValAddedBefore = false;

                }

            }

        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_FREEFORMDATA_KEY) == 0)
        {

            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;

            // Increment the counter for the number of values found so far
            ++numvalentries;
            PVMFStatus retval;
            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {
                OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesNormalizationData();
                if (valuestring.get_size() > 0)
                {
                    retval =
                        PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                PVMP4METADATA_FREEFORMDATA_KEY,
                                valuestring);
                }
                else
                {
                    OSCL_wHeapString<OsclMemAllocator> cdidentifierstring = getITunesCDIdentifierData(0);
                    retval =
                        PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                PVMP4METADATA_FREEFORMDATA_KEY,
                                cdidentifierstring);
                }
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {

                    IsMetadataValAddedBefore = false;

                }

            }

        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_IS_MOOF_KEY) == 0)
        {
            /*
             * is-moof
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;

            /* Create a value entry if past the starting index */
            if (numvalentries > (uint32)aStartingValueIndex)
            {
                bool is_movie_fragmnent_present = IsMovieFragmentsPresent();

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                            PVMP4METADATA_IS_MOOF_KEY,
                            is_movie_fragmnent_present,
                            NULL);





                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_LYRICS_KEY) == 0)
        {

            KeyVal.key = NULL;
            KeyVal.value.pWChar_value = NULL;
            KeyVal.value.pChar_value = NULL;

            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {

                OSCL_wHeapString<OsclMemAllocator> valuestring = getITunesLyrics();
                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                            PVMP4METADATA_LYRICS_KEY,
                            valuestring);

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                else
                {

                    IsMetadataValAddedBefore = false;

                }

            }

        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_ALBUM_KEY) == 0)
        {
            // Album

            if (numAlbum > 0)
            {
                for (idx = 0; idx < (int32)numAlbum ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (getAlbum(idx, valuestring, iLangCode, charType) != PVMFErrArgument)
                        {

                            char lang_param[43];
                            if (iLangCode != 0)
                            {
                                int8 LangCode[4];
                                getLanguageCode(iLangCode, LangCode);
                                LangCode[3] = '\0';
                                oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                                lang_param[20] = '\0';
                            }
                            else
                            {
                                lang_param[0] = '\0';
                            }
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                            if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                            {
                                char char_enc_param[22];
                                oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                                char_enc_param[21] = '\0';
                                oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                            }


                            PVMFStatus retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_ALBUM_KEY,
                                        valuestring,
                                        lang_param);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                            // Add the KVP to the list if the key string was created
                            if (KeyVal.key != NULL)
                            {
                                leavecode = AddToValueList(aValueList, KeyVal);
                                if (leavecode != 0)
                                {
                                    if (KeyVal.value.pWChar_value != NULL)
                                    {
                                        OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                        KeyVal.value.pWChar_value = NULL;
                                    }

                                    OSCL_ARRAY_DELETE(KeyVal.key);
                                    KeyVal.key = NULL;
                                }
                                else
                                {
                                    // Increment the value list entry counter
                                    ++numentriesadded;
                                    IsMetadataValAddedBefore = true;
                                }

                                // Check if the max number of value entries were added
                                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                                {

                                    return PVMFSuccess;
                                }
                            }
                        }

                    }
                }

            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_COMMENT_KEY) == 0)
        {
            // Comment

            if (numComment > 0)
            {
                for (idx = 0; idx < (int32)numComment ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (getComment(idx, valuestring, iLangCode, charType) != PVMFErrArgument)
                        {

                            char lang_param[43];
                            if (iLangCode != 0)
                            {
                                int8 LangCode[4];
                                getLanguageCode(iLangCode, LangCode);
                                LangCode[3] = '\0';
                                oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                                lang_param[20] = '\0';
                            }
                            else
                            {
                                lang_param[0] = '\0';
                            }
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                            if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                            {
                                char char_enc_param[22];
                                oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                                char_enc_param[21] = '\0';
                                oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                            }
                            PVMFStatus retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_COMMENT_KEY,
                                        valuestring,
                                        lang_param);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                            // Add the KVP to the list if the key string was created
                            if (KeyVal.key != NULL)
                            {
                                leavecode = AddToValueList(aValueList, KeyVal);
                                if (leavecode != 0)
                                {
                                    if (KeyVal.value.pWChar_value != NULL)
                                    {
                                        OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                        KeyVal.value.pWChar_value = NULL;
                                    }

                                    OSCL_ARRAY_DELETE(KeyVal.key);
                                    KeyVal.key = NULL;
                                }
                                else
                                {
                                    // Increment the value list entry counter
                                    ++numentriesadded;
                                    IsMetadataValAddedBefore = true;
                                }

                                // Check if the max number of value entries were added
                                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                                {

                                    return PVMFSuccess;
                                }
                            }
                        }

                    }
                }

            }
        }
        else if ((oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_LOCATION_KEY) != NULL))
        {
            /* Location */
            /* Determine the index requested. Default to all pictures */
            uint32 startindex = 0;
            int32 numLocationRecords = 0;
            numLocationRecords = getNumAssetInfoLocationAtoms();

            uint32 endindex = numLocationRecords - 1;

            /* Check if the index parameter is present */
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                /* Retrieve the index values */
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            /* Validate the indices */
            if (startindex > endindex || (int32)startindex >= numLocationRecords || (int32)endindex >= numLocationRecords)
            {
                break;
            }
            PvmfAssetInfo3GPPLocationStruct* pLocationRecord;

            /* Return a KVP for each index */
            for (uint32 cnt = startindex; (int32)cnt < numLocationRecords; cnt++)
            {
                pLocationRecord = getAssetInfoLocationStruct(cnt);
                char indexparam[29];

                oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, cnt);
                indexparam[15] = '\0';

                PvmiKvp KeyVal;
                KeyVal.key = NULL;
                /* Increment the counter for the number of values found so far */
                ++numvalentries;
                /* Add the value entry if past the starting index */
                PVMFStatus retval = PVMFErrArgument;
                if (numvalentries > aStartingValueIndex)
                {

                    retval = PVMFCreateKVPUtils::CreateKVPForKSVValue(KeyVal,
                             PVMP4METADATA_LOCATION_KEY,
                             OSCL_STATIC_CAST(OsclAny*, pLocationRecord),
                             indexparam);
                    if (retval != PVMFSuccess && retval != PVMFErrArgument)
                    {
                        break;
                    }
                }
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                if (KeyVal.key != NULL)
                {
                    PVMFStatus status = PushKVPToMetadataValueList(&aValueList, KeyVal);
                    if (status != PVMFSuccess)
                    {
                        return status;
                    }
                    // Increment the counter for number of value entries added to the list
                    ++numentriesadded;
                    IsMetadataValAddedBefore = true;

                    /* Check if the max number of value entries were added */
                    if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                    {

                        return PVMFSuccess;
                    }
                }

            }
        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_TITLE_KEY) == 0)
        {
            // Title

            if (numTitle > 0)
            {
                for (idx = 0; idx < (int32)numTitle ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (getTitle(idx, valuestring, iLangCode, charType) != PVMFErrArgument)
                        {



                            char lang_param[43];
                            if (iLangCode != 0)
                            {
                                int8 LangCode[4];
                                getLanguageCode(iLangCode, LangCode);
                                LangCode[3] = '\0';
                                oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                                lang_param[20] = '\0';
                            }
                            else
                            {
                                lang_param[0] = '\0';
                            }
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                            if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                            {
                                char char_enc_param[22];
                                oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                                char_enc_param[21] = '\0';
                                oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                            }
                            PVMFStatus retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_TITLE_KEY,
                                        valuestring,
                                        lang_param);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                            // Add the KVP to the list if the key string was created
                            if (KeyVal.key != NULL)
                            {
                                leavecode = AddToValueList(aValueList, KeyVal);
                                if (leavecode != 0)
                                {
                                    if (KeyVal.value.pWChar_value != NULL)
                                    {
                                        OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                        KeyVal.value.pWChar_value = NULL;
                                    }

                                    OSCL_ARRAY_DELETE(KeyVal.key);
                                    KeyVal.key = NULL;
                                }
                                else
                                {
                                    // Increment the value list entry counter
                                    ++numentriesadded;
                                    IsMetadataValAddedBefore = true;
                                }

                                // Check if the max number of value entries were added
                                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                                {

                                    return PVMFSuccess;
                                }
                            }
                        }

                    }
                }

            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_DESCRIPTION_KEY) == 0)
        {
            // Description

            if (numDescription > 0)
            {
                for (idx = 0; idx < (int32)numDescription ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (getDescription(idx, valuestring, iLangCode, charType) != PVMFErrArgument)
                        {


                            char lang_param[43];
                            if (iLangCode != 0)
                            {
                                int8 LangCode[4];
                                getLanguageCode(iLangCode, LangCode);
                                LangCode[3] = '\0';
                                oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                                lang_param[20] = '\0';
                            }
                            else
                            {
                                lang_param[0] = '\0';
                            }
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                            if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                            {
                                char char_enc_param[22];
                                oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                                char_enc_param[21] = '\0';
                                oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                            }
                            PVMFStatus retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_DESCRIPTION_KEY,
                                        valuestring,
                                        lang_param);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                            // Add the KVP to the list if the key string was created
                            if (KeyVal.key != NULL)
                            {
                                leavecode = AddToValueList(aValueList, KeyVal);
                                if (leavecode != 0)
                                {
                                    if (KeyVal.value.pWChar_value != NULL)
                                    {
                                        OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                        KeyVal.value.pWChar_value = NULL;
                                    }

                                    OSCL_ARRAY_DELETE(KeyVal.key);
                                    KeyVal.key = NULL;
                                }
                                else
                                {
                                    // Increment the value list entry counter
                                    ++numentriesadded;
                                    IsMetadataValAddedBefore = true;
                                }

                                // Check if the max number of value entries were added
                                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                                {

                                    return PVMFSuccess;
                                }
                            }
                        }

                    }
                }

            }
        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_RATING_KEY) == 0)
        {
            // Rating

            if (numRating > 0)
            {
                for (idx = 0; idx < (int32)numRating ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (getRating(idx, valuestring, iLangCode, charType) != PVMFErrArgument)
                        {

                            char lang_param[43];
                            if (iLangCode != 0)
                            {
                                int8 LangCode[4];
                                getLanguageCode(iLangCode, LangCode);
                                LangCode[3] = '\0';
                                oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                                lang_param[20] = '\0';
                            }
                            else
                            {
                                lang_param[0] = '\0';
                            }
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                            if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                            {
                                char char_enc_param[22];
                                oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                                char_enc_param[21] = '\0';
                                oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                            }
                            PVMFStatus retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_RATING_KEY,
                                        valuestring,
                                        lang_param);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                            // Add the KVP to the list if the key string was created
                            if (KeyVal.key != NULL)
                            {
                                leavecode = AddToValueList(aValueList, KeyVal);
                                if (leavecode != 0)
                                {
                                    if (KeyVal.value.pWChar_value != NULL)
                                    {
                                        OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                        KeyVal.value.pWChar_value = NULL;
                                    }

                                    OSCL_ARRAY_DELETE(KeyVal.key);
                                    KeyVal.key = NULL;
                                }
                                else
                                {
                                    // Increment the value list entry counter
                                    ++numentriesadded;
                                    IsMetadataValAddedBefore = true;
                                }

                                // Check if the max number of value entries were added
                                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                                {

                                    return PVMFSuccess;
                                }
                            }
                        }

                    } //End of Outer If
                }

            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_COPYRIGHT_KEY) == 0)
        {
            // Copyright

            if (numCopyright > 0)
            {
                for (idx = 0; idx < (int32)numCopyright ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (getCopyright(idx, valuestring, iLangCode, charType) != PVMFErrArgument)
                        {
                            char lang_param[43];
                            if (iLangCode != 0)
                            {
                                int8 LangCode[4];
                                getLanguageCode(iLangCode, LangCode);
                                LangCode[3] = '\0';
                                oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                                lang_param[20] = '\0';
                            }
                            else
                            {
                                lang_param[0] = '\0';
                            }
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                            if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                            {
                                char char_enc_param[22];
                                oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                                char_enc_param[21] = '\0';
                                oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                            }
                            PVMFStatus retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_COPYRIGHT_KEY,
                                        valuestring,
                                        lang_param);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                            // Add the KVP to the list if the key string was created
                            if (KeyVal.key != NULL)
                            {
                                leavecode = AddToValueList(aValueList, KeyVal);
                                if (leavecode != 0)
                                {
                                    if (KeyVal.value.pWChar_value != NULL)
                                    {
                                        OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                        KeyVal.value.pWChar_value = NULL;
                                    }

                                    OSCL_ARRAY_DELETE(KeyVal.key);
                                    KeyVal.key = NULL;
                                }
                                else
                                {
                                    // Increment the value list entry counter
                                    ++numentriesadded;
                                    IsMetadataValAddedBefore = true;
                                }

                                // Check if the max number of value entries were added
                                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                                {

                                    return PVMFSuccess;
                                }
                            }
                        }
                    }

                }

            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_ARTIST_KEY) == 0)
        {
            // Artist

            if (numArtist > 0)
            {
                for (idx = 0; idx < (int32)numArtist ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (getArtist(idx, valuestring, iLangCode, charType) != PVMFErrArgument)
                        {

                            char lang_param[43];
                            if (iLangCode != 0)
                            {
                                int8 LangCode[4];
                                getLanguageCode(iLangCode, LangCode);
                                LangCode[3] = '\0';
                                oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                                lang_param[20] = '\0';
                            }
                            else
                            {
                                lang_param[0] = '\0';
                            }
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                            if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                            {
                                char char_enc_param[22];
                                oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                                char_enc_param[21] = '\0';
                                oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                            }
                            PVMFStatus retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_ARTIST_KEY,
                                        valuestring,
                                        lang_param);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                            // Add the KVP to the list if the key string was created
                            if (KeyVal.key != NULL)
                            {
                                leavecode = AddToValueList(aValueList, KeyVal);
                                if (leavecode != 0)
                                {
                                    if (KeyVal.value.pWChar_value != NULL)
                                    {
                                        OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                        KeyVal.value.pWChar_value = NULL;
                                    }

                                    OSCL_ARRAY_DELETE(KeyVal.key);
                                    KeyVal.key = NULL;
                                }
                                else
                                {
                                    // Increment the value list entry counter
                                    ++numentriesadded;
                                    IsMetadataValAddedBefore = true;
                                }

                                // Check if the max number of value entries were added
                                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                                {

                                    return PVMFSuccess;
                                }
                            }
                        }

                    }
                }

            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_KEYWORD_KEY) == 0)
        {
            int32 numAssetInfoKeyword = getNumAssetInfoKeyWordAtoms();
            for (idx = 0; idx < numAssetInfoKeyword; idx++)
            {
                int32 AssetInfoKeywordCount = getAssetInfoNumKeyWords(idx);
                for (int32 idy = 0; idy < AssetInfoKeywordCount; idy++)
                {

                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {
                        int8 LangCode[4];
                        getLanguageCode(getAssetInfoKeyWordLangCode(idx), LangCode);
                        LangCode[3] = '\0';

                        char lang_param[21];
                        oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                        lang_param[20] = '\0';

                        OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoKeyWord(idx, idy);
                        PVMFStatus retval =
                            PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                    PVMP4METADATA_KEYWORD_KEY,
                                    valuestring,
                                    lang_param);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }
                        // Add the KVP to the list if the key string was created
                        if (KeyVal.key != NULL)
                        {
                            leavecode = AddToValueList(aValueList, KeyVal);
                            if (leavecode != 0)
                            {
                                if (KeyVal.value.pWChar_value != NULL)
                                {
                                    OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                    KeyVal.value.pWChar_value = NULL;
                                }

                                OSCL_ARRAY_DELETE(KeyVal.key);
                                KeyVal.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                            {

                                return PVMFSuccess;
                            }
                        }

                    }
                    KeyVal.key = NULL;
                    KeyVal.value.pWChar_value = NULL;
                    KeyVal.value.pChar_value = NULL;
                }
            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_GENRE_KEY) == 0)
        {
            // Genre
            PVMFStatus retval = PVMFFailure;


            if (numGenre > 0)
            {
                for (idx = 0; idx < (int32)numGenre ; idx++)
                {
                    // Create a value entry if past the starting index
                    if (numvalentries >= aStartingValueIndex)
                    {

                        if (getGenre(idx, valuestring, iLangCode, charType) != PVMFErrArgument)
                        {
                            // Increment the counter for the number of values found so far
                            ++numvalentries;

                            char lang_param[43];
                            if (iLangCode != 0)
                            {
                                int8 LangCode[4];
                                getLanguageCode(iLangCode, LangCode);
                                LangCode[3] = '\0';
                                oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                                lang_param[20] = '\0';
                            }
                            else
                            {
                                lang_param[0] = '\0';
                            }
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                            if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                            {
                                char char_enc_param[22];
                                oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                                char_enc_param[21] = '\0';
                                oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                            }
                            retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_GENRE_KEY,
                                        valuestring,
                                        lang_param);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                            // Add the KVP to the list if the key string was created
                            if (KeyVal.key != NULL)
                            {
                                leavecode = AddToValueList(aValueList, KeyVal);
                                if (leavecode != 0)
                                {
                                    if (KeyVal.value.pWChar_value != NULL)
                                    {
                                        OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                        KeyVal.value.pWChar_value = NULL;
                                    }

                                    OSCL_ARRAY_DELETE(KeyVal.key);
                                    KeyVal.key = NULL;
                                }
                                else
                                {
                                    // Increment the value list entry counter
                                    ++numentriesadded;
                                    IsMetadataValAddedBefore = true;
                                }

                                // Check if the max number of value entries were added
                                if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                                {

                                    return PVMFSuccess;
                                }
                            }
                        }
                    }
                }

            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_CLASSIFICATION_KEY) == 0)
        {
            int32 numAssetInfoClassification = getNumAssetInfoClassificationAtoms();
            for (idx = 0; idx < numAssetInfoClassification; idx++)
            {
                // Increment the counter for the number of values found so far
                ++numvalentries;

                // Create a value entry if past the starting index
                if (numvalentries > aStartingValueIndex)
                {
                    int8 LangCode[4];
                    getLanguageCode(getAssetInfoClassificationLangCode(idx), LangCode);
                    LangCode[3] = '\0';

                    char lang_param[43];
                    oscl_snprintf(lang_param, 20, ";%s%s", PVMP4METADATA_LANG_CODE, LangCode);
                    lang_param[20] = '\0';

                    OSCL_wHeapString<OsclMemAllocator> valuestring = getAssetInfoClassificationNotice(charType, idx);
                    if (charType != ORIGINAL_CHAR_TYPE_UNKNOWN)
                    {
                        char char_enc_param[22];
                        oscl_snprintf(char_enc_param, 22, ";%s%s", PVMP4METADATA_ORIG_CHAR_ENC, orig_char_enc[charType-1]);
                        char_enc_param[21] = '\0';
                        oscl_strncat(lang_param, char_enc_param, oscl_strlen(char_enc_param));
                    }

                    PVMFStatus retval =
                        PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                PVMP4METADATA_CLASSIFICATION_KEY,
                                valuestring,
                                lang_param);
                    if (retval != PVMFSuccess && retval != PVMFErrArgument)
                    {
                        break;
                    }
                    // Add the KVP to the list if the key string was created
                    if (KeyVal.key != NULL)
                    {
                        leavecode = AddToValueList(aValueList, KeyVal);
                        if (leavecode != 0)
                        {
                            if (KeyVal.value.pWChar_value != NULL)
                            {
                                OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                                KeyVal.value.pWChar_value = NULL;
                            }

                            OSCL_ARRAY_DELETE(KeyVal.key);
                            KeyVal.key = NULL;
                        }
                        else
                        {
                            // Increment the value list entry counter
                            ++numentriesadded;
                            IsMetadataValAddedBefore = true;
                        }

                        // Check if the max number of value entries were added
                        if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                        {

                            return PVMFSuccess;
                        }
                    }
                }
            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_MAJORBRAND_KEY) == 0)
        {
            // MAJOR BRAND
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {
                char BrandCode[5];
                uint32 Mbrand = getCompatibiltyMajorBrand();
                getBrand(Mbrand, BrandCode);
                BrandCode[4] = '\0';

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                            PVMP4METADATA_MAJORBRAND_KEY,
                            BrandCode);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_COMPATIBLEBRAND_KEY) == 0)
        {
            // Compatible Brand
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {
                Oscl_Vector<uint32, OsclMemAllocator> *Compatiblebrand_Vec = getCompatibiltyList();
                uint32 idy = 0;
                for (idy = 0; idy < Compatiblebrand_Vec->size() ; idy++)
                {
                    char BrandCode[5];
                    uint32 CbrandNum = (*Compatiblebrand_Vec)[idy];
                    getBrand(CbrandNum, BrandCode);
                    BrandCode[4] = '\0';
                    KeyVal.key = NULL;
                    KeyVal.value.pWChar_value = NULL;
                    KeyVal.value.pChar_value = NULL;

                    PVMFStatus retval =
                        PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                                PVMP4METADATA_COMPATIBLEBRAND_KEY,
                                BrandCode);
                    if (retval != PVMFSuccess && retval != PVMFErrArgument)
                    {
                        break;
                    }
                    // Add the KVP to the list if the key string was created
                    if (KeyVal.key != NULL)
                    {
                        leavecode = AddToValueList(aValueList, KeyVal);
                        if (leavecode != 0)
                        {
                            if (KeyVal.value.pChar_value != NULL)
                            {
                                OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                                KeyVal.value.pChar_value = NULL;
                            }

                            OSCL_ARRAY_DELETE(KeyVal.key);
                            KeyVal.key = NULL;
                        }
                        else
                        {
                            // Increment the value list entry counter
                            ++numentriesadded;
                            IsMetadataValAddedBefore = true;
                        }

                        // Check if the max number of value entries were added
                        if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                        {

                            return PVMFSuccess;
                        }
                    }
                }
            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_VERSION_KEY) == 0)
        {
            if (numVersion > 0)
            {
                for (idx = 0; idx < (int32)numVersion ; idx++)
                {
                    // Version
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {
                        if (getVersion(idx, valuestring, charType) != PVMFErrArgument)
                        {
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;

                            PVMFStatus retval =
                                PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                        PVMP4METADATA_VERSION_KEY,
                                        valuestring);
                            if (retval != PVMFSuccess && retval != PVMFErrArgument)
                            {
                                break;
                            }
                        }
                    }
                    // Add the KVP to the list if the key string was created
                    if (KeyVal.key != NULL)
                    {
                        leavecode = AddToValueList(aValueList, KeyVal);
                        if (leavecode != 0)
                        {
                            if (KeyVal.value.pChar_value != NULL)
                            {
                                OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                                KeyVal.value.pChar_value = NULL;
                            }

                            OSCL_ARRAY_DELETE(KeyVal.key);
                            KeyVal.key = NULL;
                        }
                        else
                        {
                            // Increment the value list entry counter
                            ++numentriesadded;
                            IsMetadataValAddedBefore = true;
                        }

                        // Check if the max number of value entries were added
                        if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                        {

                            return PVMFSuccess;
                        }
                    }
                }
            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_DATE_KEY) == 0)
        {
            if (numDate > 0)
            {
                for (idx = 0; idx < (int32)numDate ; idx++)
                {
                    // Date
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {
                        if (getDate(idx, valuestring, charType) != PVMFErrArgument)
                        {
                            KeyVal.key = NULL;
                            KeyVal.value.pWChar_value = NULL;
                            KeyVal.value.pChar_value = NULL;
                        }

                        PVMFStatus retval =
                            PVMFCreateKVPUtils::CreateKVPForWStringValue(KeyVal,
                                    PVMP4METADATA_DATE_KEY,
                                    valuestring);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }
                    }
                    // Add the KVP to the list if the key string was created
                    if (KeyVal.key != NULL)
                    {
                        leavecode = AddToValueList(aValueList, KeyVal);
                        if (leavecode != 0)
                        {
                            if (KeyVal.value.pChar_value != NULL)
                            {
                                OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                                KeyVal.value.pChar_value = NULL;
                            }

                            OSCL_ARRAY_DELETE(KeyVal.key);
                            KeyVal.key = NULL;
                        }
                        else
                        {
                            // Increment the value list entry counter
                            ++numentriesadded;
                            IsMetadataValAddedBefore = true;
                        }

                        // Check if the max number of value entries were added
                        if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                        {

                            return PVMFSuccess;
                        }
                    }
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_PROFILE_KEY) != NULL)
        {
            // profile
            // Determine the index requested.
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)(numtracks) || endindex >= (uint32)(numtracks))
            {
                break;
            }

            uint32 iProfile = 0;
            OSCL_HeapString<OsclMemAllocator> trackMIMEType;
            // Go through all tracks
            uint32 iIdList[16];
            getTrackIDList(iIdList, (numtracks < 16) ? numtracks : 16);
            // Go through all tracks
            for (uint32 i = startindex; i <= endindex; i++)
            {
                uint32 trackID = iIdList[i];
                getTrackMIMEType(trackID, trackMIMEType);

                if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0)
                {
                    H263DecoderSpecificInfo *ptr = (H263DecoderSpecificInfo *)getTrackDecoderSpecificInfoAtSDI(trackID, 0);
                    iProfile = ptr->getCodecProfile();
                    // Increment the counter for the number of values found so far
                    ++numvalentries;
                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, startindex);
                        indexparam[15] = '\0';

                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                                            PVMP4METADATA_TRACKINFO_VIDEO_PROFILE_KEY,
                                            iProfile,
                                            indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }
                    }
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_LEVEL_KEY) != NULL)
        {
            // level
            // Determine the index requested.
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)(numtracks) || endindex >= (uint32)(numtracks))
            {
                break;
            }

            uint32 iLevel = 0;
            OSCL_HeapString<OsclMemAllocator> trackMIMEType;
            // Go through all tracks
            uint32 iIdList[16];
            getTrackIDList(iIdList, (numtracks < 16) ? numtracks : 16);
            for (uint32 i = startindex; i <= endindex; i++)
            {
                uint32 trackID = iIdList[i];

                getTrackMIMEType(trackID, trackMIMEType);

                if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0)
                {
                    H263DecoderSpecificInfo *ptr = (H263DecoderSpecificInfo *)getTrackDecoderSpecificInfoAtSDI(trackID, 0);
                    iLevel = ptr->getCodecLevel();
                    // Increment the counter for the number of values found so far
                    ++numvalentries;
                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, startindex);
                        indexparam[15] = '\0';
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                                            PVMP4METADATA_TRACKINFO_VIDEO_LEVEL_KEY,
                                            iLevel,
                                            indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }
                    }
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_FRAME_RATE_KEY) != NULL)
        {
            // level
            // Determine the index requested.
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)(numtracks) || endindex >= (uint32)(numtracks))
            {
                break;
            }

            uint32 iIdList[16];
            getTrackIDList(iIdList, numtracks);
            for (uint32 i = startindex; i <= endindex; i++)
            {
                uint32 trackID = iIdList[i];

                OSCL_HeapString<OsclMemAllocator> trackMIMEType;

                getTrackMIMEType(trackID, trackMIMEType);

                if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0) ||
                        (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0) ||
                        (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0))
                {
                    uint64 trackduration  = getTrackMediaDuration(trackID);
                    uint32 samplecount = getSampleCountInTrack(trackID);

                    MediaClockConverter mcc(getTrackMediaTimescale(trackID));
                    mcc.set_clock(trackduration, 0);
                    uint32 TrackDurationInSec = mcc.get_converted_ts(1);
                    uint32 frame_rate = 0;

                    uint32 OverflowThreshold = PVMF_MP4_MAX_UINT32 / MILLISECOND_TIMESCALE;
                    // If overflow could not happen, we calculate it in millisecond
                    if (TrackDurationInSec < OverflowThreshold && samplecount < OverflowThreshold)
                    {
                        uint32 TrackDurationInMilliSec = mcc.get_converted_ts(MILLISECOND_TIMESCALE);
                        if (TrackDurationInMilliSec > 0)
                        {
                            frame_rate = samplecount * MILLISECOND_TIMESCALE / TrackDurationInMilliSec;
                        }
                        else
                        {
                            continue;
                        }
                    }
                    else // if overflow could happen when calculate in millisecond, we calculate it in second
                    {
                        if (TrackDurationInSec > 0)
                        {
                            frame_rate = samplecount / TrackDurationInSec;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    // Increment the counter for the number of values found so far
                    ++numvalentries;
                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                                            PVMP4METADATA_TRACKINFO_FRAME_RATE_KEY,
                                            frame_rate,
                                            indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }
                    }
                }
            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_DURATION_KEY) == 0 &&
                 getMovieDuration() > (uint64)0 && getMovieTimescale() > 0)
        {
            // Movie Duration
            // Increment the counter for the number of values found so far
            ++numvalentries;

            //When timescale is  huge say 10000000. Then duration might need 64 bits to storeits value
            //But, the existing codebase doesnt expect duarton to be big enough than to overflow 32 bits
            //Therefore, we explicitly make timescale to be millisec and convert duration in millisec
            //timescale so as our assumption of storing duration in 32  bit datatype still holds true
            if (numvalentries > aStartingValueIndex)
            {
                const uint32 timescale = MILLISECOND_TIMESCALE;
                uint64 duration64 = getMovieDuration();
                MediaClockConverter mcc(getMovieTimescale());
                mcc.set_clock(duration64, 0);
                uint32 duration = mcc.get_converted_ts(timescale);
                char timescalestr[20] = {0};
                oscl_snprintf(timescalestr, 20, ";%s%d", PVMP4METADATA_TIMESCALE, timescale);
                timescalestr[19] = '\0';
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVMP4METADATA_DURATION_KEY, duration, timescalestr);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_YEAR_KEY) == 0)
        {
            // Year
            uint32 value = 0;
            if (numYear > 0)
            {
                for (idx = 0; idx < (int32)numYear ; idx++)
                {
                    // Increment the counter for the number of values found so far
                    ++numvalentries;

                    // Create a value entry if past the starting index
                    if (numvalentries > aStartingValueIndex)
                    {

                        if (!getYear(idx, value))
                        {
                            return PVMFFailure;
                        }

                        KeyVal.key = NULL;
                        KeyVal.value.pUint32_value = NULL;

                        PVMFStatus retval =
                            PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                                    PVMP4METADATA_YEAR_KEY, value);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }
                        // Add the KVP to the list if the key string was created
                        if (KeyVal.key != NULL)
                        {
                            leavecode = AddToValueList(aValueList, KeyVal);
                            if (leavecode != 0)
                            {
                                KeyVal.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                            {

                                return PVMFSuccess;
                            }
                        }

                    }
                }

            }
        }

        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVMP4METADATA_NUMTRACKS_KEY) == 0 &&
                 getNumTracks() > 0)
        {
            // Number of tracks
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > aStartingValueIndex)
            {
                uint32 numtracks = getNumTracks();
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVMP4METADATA_NUMTRACKS_KEY, numtracks);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }

        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_TYPE_KEY) != NULL)
        {
            // Track type

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;
                trackkvp.value.pChar_value = NULL;

                char indexparam[16];
                oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                indexparam[15] = '\0';

                PVMFStatus retval = PVMFErrArgument;

                OSCL_HeapString<OsclMemAllocator> trackMIMEType;

                getTrackMIMEType(trackidlist[i], (OSCL_String&)trackMIMEType);

                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                if (numvalentries > aStartingValueIndex)
                {
                    retval =
                        PVMFCreateKVPUtils::CreateKVPForCharStringValue(trackkvp,
                                PVMP4METADATA_TRACKINFO_TYPE_KEY,
                                trackMIMEType.get_str(),
                                indexparam);
                }
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
                if (trackkvp.key != NULL)
                {
                    leavecode = AddToValueList(aValueList, trackkvp);
                    if (leavecode != 0)
                    {
                        if (trackkvp.value.pChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(trackkvp.value.pChar_value);
                            trackkvp.value.pChar_value = NULL;
                        }

                        OSCL_ARRAY_DELETE(trackkvp.key);
                        trackkvp.key = NULL;
                    }
                    else
                    {
                        // Increment the value list entry counter
                        ++numentriesadded;
                        IsMetadataValAddedBefore = true;
                    }

                    // Check if the max number of value entries were added
                    if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                    {

                        return PVMFSuccess;
                    }
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_TRACKID_KEY) != NULL)
        {
            // Track ID

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                PVMFStatus retval = PVMFErrArgument;
                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                if (numvalentries > aStartingValueIndex)
                {
                    char indexparam[16];
                    oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                    indexparam[15] = '\0';

                    retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_TRACKID_KEY, trackidlist[i], indexparam);
                }

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

                if (trackkvp.key != NULL)
                {
                    leavecode = AddToValueList(aValueList, trackkvp);
                    if (leavecode != 0)
                    {
                        OSCL_ARRAY_DELETE(trackkvp.key);
                        trackkvp.key = NULL;
                    }
                    else
                    {
                        // Increment the value list entry counter
                        ++numentriesadded;
                        IsMetadataValAddedBefore = true;
                    }

                    // Check if the max number of value entries were added
                    if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                    {

                        return PVMFSuccess;
                    }
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_DURATION_KEY) != NULL)
        {
            // Track duration
            //When timescale is  huge say 10000000. Then duration might need 64 bits to storeits value
            //But, the existing codebase doesnt expect duarton to be big enough than to overflow 32 bits
            //Therefore, we explicitly make timescale to be millisec and convert duration in millisec
            //timescale so as our assumption of storing duration in 32  bit datatype still holds true

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                PVMFStatus retval = PVMFErrArgument;
                if (numvalentries > aStartingValueIndex)
                {
                    char indextimescaleparam[36];
                    uint32 timeScale = 0;

                    if (_parsing_mode && IsMovieFragmentsPresent())
                        timeScale = getMovieTimescale();
                    else
                        timeScale = getTrackMediaTimescale(trackidlist[i]);

                    const uint32 millisecTs = MILLISECOND_TIMESCALE;
                    oscl_snprintf(indextimescaleparam, 36, ";%s%d;%s%d", PVMP4METADATA_INDEX, i, PVMP4METADATA_TIMESCALE, millisecTs);

                    indextimescaleparam[35] = '\0';

                    uint64 trackduration64 = getTrackMediaDuration(trackidlist[i]);
                    MediaClockConverter mcc(timeScale);
                    mcc.set_clock(trackduration64, 0);
                    uint32 trackduration = mcc.get_converted_ts(1000);
                    retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_DURATION_KEY, trackduration, indextimescaleparam);
                }

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

                if (trackkvp.key != NULL)
                {
                    leavecode = AddToValueList(aValueList, trackkvp);
                    if (leavecode != 0)
                    {
                        OSCL_ARRAY_DELETE(trackkvp.key);
                        trackkvp.key = NULL;
                    }
                    else
                    {
                        // Increment the value list entry counter
                        ++numentriesadded;
                        IsMetadataValAddedBefore = true;
                    }

                    // Check if the max number of value entries were added
                    if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                    {

                        return PVMFSuccess;
                    }
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_BITRATE_KEY) != NULL)
        {
            // Track bitrate

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                PVMFStatus retval = PVMFErrArgument;
                if (numvalentries > aStartingValueIndex)
                {
                    char indexparam[16];
                    oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                    indexparam[15] = '\0';

                    uint32 trackbitrate = (uint32)(getTrackAverageBitrate(trackidlist[i])); // Always returns unsigned value

                    retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_BITRATE_KEY, trackbitrate, indexparam);
                }

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

                if (trackkvp.key != NULL)
                {
                    leavecode = AddToValueList(aValueList, trackkvp);
                    if (leavecode != 0)
                    {
                        OSCL_ARRAY_DELETE(trackkvp.key);
                        trackkvp.key = NULL;
                    }
                    else
                    {
                        // Increment the value list entry counter
                        ++numentriesadded;
                        IsMetadataValAddedBefore = true;
                    }

                    // Check if the max number of value entries were added
                    if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                    {

                        return PVMFSuccess;
                    }
                }
            }
        }
        else if ((oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_TRACK_NUMBER_KEY) != NULL) &&
                 getITunesThisTrackNo() > 0)
        {
            // iTunes Current Track Number
            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;
                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                PVMFStatus retval = PVMFErrArgument;
                if (numvalentries > aStartingValueIndex)
                {
                    char indexparam[16];
                    oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                    indexparam[15] = '\0';

                    uint32 track_number = getITunesThisTrackNo(); // Always returns unsigned value

                    char cdTrackNumber[6];
                    uint16 totalTrackNumber = getITunesTotalTracks();
                    oscl_snprintf(cdTrackNumber, 6, "%d/%d", track_number, totalTrackNumber);
                    cdTrackNumber[5] = '\0';

                    retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(trackkvp, PVMP4METADATA_TRACKINFO_TRACK_NUMBER_KEY, cdTrackNumber, indexparam);
                    if ((retval != PVMFSuccess) && (retval != PVMFErrArgument))
                    {
                        break;
                    }

                    if (trackkvp.key != NULL)
                    {
                        leavecode = AddToValueList(aValueList, trackkvp);
                        if (leavecode != 0)
                        {
                            OSCL_ARRAY_DELETE(trackkvp.key);
                            trackkvp.key = NULL;
                        }
                        else
                        {
                            // Increment the value list entry counter
                            ++numentriesadded;
                            IsMetadataValAddedBefore = true;
                        }

                        // Check if the max number of value entries were added
                        if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                        {


                            return PVMFSuccess;
                        }
                    }
                }
            }
        }
        else if ((oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_FORMAT_KEY) != NULL) ||
                 (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_FORMAT_KEY) != NULL))
        {
            // Audio or video track format
            // Set index for track type
            uint32 tracktype = 0; // 0 unknown, 1 video, 2 audio
            if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_FORMAT_KEY) != NULL)
            {
                tracktype = 1;
            }
            else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_FORMAT_KEY) != NULL)
            {
                tracktype = 2;
            }

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;
                trackkvp.value.pChar_value = NULL;

                char indexparam[16];
                oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                indexparam[15] = '\0';

                PVMFStatus retval = PVMFErrArgument;
                OSCL_HeapString<OsclMemAllocator> trackMIMEType;

                getTrackMIMEType(trackidlist[i], trackMIMEType);

                if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0)
                {
                    if (tracktype == 1)
                    {
                        ++numvalentries;
                        if (numvalentries > aStartingValueIndex)
                        {
                            retval =
                                PVMFCreateKVPUtils::CreateKVPForCharStringValue(trackkvp, PVMP4METADATA_TRACKINFO_VIDEO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_M4V), indexparam);
                        }
                    }
                }
                else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0)
                {
                    if (tracktype == 1)
                    {
                        ++numvalentries;
                        if (numvalentries > aStartingValueIndex)
                        {
                            retval =
                                PVMFCreateKVPUtils::CreateKVPForCharStringValue(trackkvp, PVMP4METADATA_TRACKINFO_VIDEO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_H2631998), indexparam);
                        }
                    }
                }
                else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0)
                {
                    if (tracktype == 1)
                    {
                        ++numvalentries;
                        if (numvalentries > aStartingValueIndex)
                        {
                            retval =
                                PVMFCreateKVPUtils::CreateKVPForCharStringValue(trackkvp, PVMP4METADATA_TRACKINFO_VIDEO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_H264_VIDEO_MP4), indexparam);
                        }
                    }
                }
                else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_MPEG4_AUDIO, oscl_strlen(PVMF_MIME_MPEG4_AUDIO)) == 0)
                {
                    if (tracktype == 2)
                    {
                        ++numvalentries;
                        if (numvalentries > aStartingValueIndex)
                        {
                            retval =
                                PVMFCreateKVPUtils::CreateKVPForCharStringValue(trackkvp, PVMP4METADATA_TRACKINFO_AUDIO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_MPEG4_AUDIO), indexparam);
                        }
                    }
                }
                else if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR_IETF, oscl_strlen(PVMF_MIME_AMR_IETF)) == 0) ||
                         (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMRWB_IETF, oscl_strlen(PVMF_MIME_AMRWB_IETF)) == 0))
                {
                    if (tracktype == 2)
                    {
                        ++numvalentries;
                        if (numvalentries > aStartingValueIndex)
                        {
                            retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(trackkvp, PVMP4METADATA_TRACKINFO_AUDIO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_AMR_IETF), indexparam);
                        }
                    }
                }
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

                if (trackkvp.key != NULL)
                {
                    leavecode = AddToValueList(aValueList, trackkvp);
                    if (leavecode != 0)
                    {
                        if (trackkvp.value.pChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(trackkvp.value.pChar_value);
                            trackkvp.value.pChar_value = NULL;
                        }

                        OSCL_ARRAY_DELETE(trackkvp.key);
                        trackkvp.key = NULL;
                    }
                    else
                    {
                        // Increment the value list entry counter
                        ++numentriesadded;
                        IsMetadataValAddedBefore = true;
                    }

                    // Check if the max number of value entries were added
                    if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                    {

                        return PVMFSuccess;
                    }
                }
            }
        }

        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_SAMPLECOUNT_KEY) != NULL)
        {
            // Sample count

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                PVMFStatus retval = PVMFErrArgument;
                if (numvalentries > aStartingValueIndex)
                {
                    char indexparam[16];
                    oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                    indexparam[15] = '\0';

                    uint32 samplecount = (uint32)(getSampleCountInTrack(trackidlist[i])); // Always returns unsigned value

                    retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_SAMPLECOUNT_KEY, samplecount, indexparam);
                }

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

                if (trackkvp.key != NULL)
                {
                    leavecode = AddToValueList(aValueList, trackkvp);
                    if (leavecode != 0)
                    {
                        OSCL_ARRAY_DELETE(trackkvp.key);
                        trackkvp.key = NULL;
                    }
                    else
                    {
                        // Increment the value list entry counter
                        ++numentriesadded;
                        IsMetadataValAddedBefore = true;
                    }

                    // Check if the max number of value entries were added
                    if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                    {

                        return PVMFSuccess;
                    }
                }
            }
        }
        else if (oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_NUM_KEY_SAMPLES_KEY) != NULL)
        {
            // Num-Key-Samples

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr(aKeyList[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                PVMFStatus retval = PVMFErrArgument;
                if (numvalentries > aStartingValueIndex)
                {
                    char indexparam[16];
                    oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                    indexparam[15] = '\0';

                    uint32 keySampleCount = getNumKeyFrames(trackidlist[i]);

                    retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_NUM_KEY_SAMPLES_KEY, keySampleCount, indexparam);
                }

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

                if (trackkvp.key != NULL)
                {
                    leavecode = AddToValueList(aValueList, trackkvp);
                    if (leavecode != 0)
                    {
                        OSCL_ARRAY_DELETE(trackkvp.key);
                        trackkvp.key = NULL;
                    }
                    else
                    {
                        // Increment the value list entry counter
                        ++numentriesadded;
                        IsMetadataValAddedBefore = true;
                    }

                    // Check if the max number of value entries were added
                    if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
                    {

                        return PVMFSuccess;
                    }
                }
            }
        }

        // Add the KVP to the list if the key string was created
        if ((KeyVal.key != NULL) && (!IsMetadataValAddedBefore))
        {
            leavecode = AddToValueList(aValueList, KeyVal);
            if (leavecode != 0)
            {
                switch (GetValTypeFromKeyString(KeyVal.key))
                {
                    case PVMI_KVPVALTYPE_CHARPTR:
                        if (KeyVal.value.pChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                            KeyVal.value.pChar_value = NULL;
                        }
                        break;

                    case PVMI_KVPVALTYPE_WCHARPTR:
                        if (KeyVal.value.pWChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                            KeyVal.value.pWChar_value = NULL;
                        }
                        break;

                    default:
                        // Add more case statements if other value types are returned
                        break;
                }

                OSCL_ARRAY_DELETE(KeyVal.key);
                KeyVal.key = NULL;
            }
            else
            {
                // Increment the counter for number of value entries added to the list
                ++numentriesadded;
            }

            // Check if the max number of value entries were added
            if (aMaxValueEntries > 0 && numentriesadded >= aMaxValueEntries)
            {
                // Maximum number of values added so break out of the loop
                //return PVMFSuccess;
                break;
            }
        }
    }


    return PVMFSuccess;


}


PVMFStatus Mpeg4File::ReleaseMetadataValue(PvmiKvp& aValueKVP)
{
    if (aValueKVP.key == NULL)
    {
        return PVMFErrArgument;
    }

    switch (GetValTypeFromKeyString(aValueKVP.key))
    {
        case PVMI_KVPVALTYPE_WCHARPTR:
            if ((aValueKVP.value.pWChar_value != NULL) && (aValueKVP.length != 0))
            {
                OSCL_ARRAY_DELETE(aValueKVP.value.pWChar_value);
                aValueKVP.value.pWChar_value = NULL;

            }
            break;

        case PVMI_KVPVALTYPE_CHARPTR:
            if ((aValueKVP.value.pChar_value != NULL) && (aValueKVP.length != 0))
            {
                OSCL_ARRAY_DELETE(aValueKVP.value.pChar_value);
                aValueKVP.value.pChar_value = NULL;
            }
            break;

        case PVMI_KVPVALTYPE_UINT8PTR:
            if ((aValueKVP.value.pUint8_value != NULL) && (aValueKVP.length != 0))
            {
                OSCL_ARRAY_DELETE(aValueKVP.value.pUint8_value);
                aValueKVP.value.pUint8_value = NULL;
            }

            break;

        case PVMI_KVPVALTYPE_UINT32:
        case PVMI_KVPVALTYPE_UINT8:
        case PVMI_KVPVALTYPE_FLOAT:
        case PVMI_KVPVALTYPE_BOOL:
        case PVMI_KVPVALTYPE_KSV:
            // No memory to free for these valtypes
            break;

        default:
        {
            // Should not get a value that wasn't created from here
            OSCL_ASSERT(false);
        }
        break;
    }

    OSCL_ARRAY_DELETE(aValueKVP.key);
    aValueKVP.key = NULL;

    return PVMFSuccess;
}

int32 Mpeg4File::AddToValueList(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, PvmiKvp& aNewValue)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aValueList.push_back(aNewValue));
    return leavecode;
}

PVMFStatus Mpeg4File::PushValueToList(Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> &aRefMetaDataKeys, PVMFMetadataList *&aKeyListPtr, uint32 aLcv)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aKeyListPtr->push_back(aRefMetaDataKeys[aLcv]));
    OSCL_FIRST_CATCH_ANY(leavecode, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4Parser::PushValueToList() Memory allocation failure when copying metadata key")); return PVMFErrNoMemory);
    return PVMFSuccess;
}

PVMFStatus Mpeg4File::PushKVPToMetadataValueList(Oscl_Vector<PvmiKvp, OsclMemAllocator>* aVecPtr, PvmiKvp& aKvpVal)
{
    if (aVecPtr == NULL)
    {
        return PVMFErrArgument;
    }
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aVecPtr->push_back(aKvpVal););
    if (leavecode != 0)
    {
        OSCL_ARRAY_DELETE(aKvpVal.key);
        aKvpVal.key = NULL;
        return PVMFErrNoMemory;
    }
    return PVMFSuccess;
}

PVMFStatus Mpeg4File::CreateNewArray(uint32** aTrackidList, uint32 aNumTracks)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, *aTrackidList = OSCL_ARRAY_NEW(uint32, aNumTracks););
    OSCL_FIRST_CATCH_ANY(leavecode, return PVMFErrNoMemory;);
    return PVMFSuccess;
}

void Mpeg4File::getLanguageCode(uint16 langcode, int8 *LangCode)
{
    //ISO-639-2/T 3-char Lang Code
    oscl_memset(LangCode, 0, 4);
    LangCode[0] = OSCL_STATIC_CAST(int8, (0x60 + ((langcode >> 10) & 0x1F)));
    LangCode[1] = OSCL_STATIC_CAST(int8, (0x60 + ((langcode >> 5) & 0x1F)));
    LangCode[2] = OSCL_STATIC_CAST(int8, (0x60 + ((langcode) & 0x1F)));
}

void Mpeg4File::getBrand(uint32 aBrandVal, char *BrandVal)
{
    BrandVal[0] = OSCL_STATIC_CAST(char, ((aBrandVal >> 24)));
    BrandVal[1] = OSCL_STATIC_CAST(char, ((aBrandVal >> 16)));
    BrandVal[2] = OSCL_STATIC_CAST(char, ((aBrandVal >> 8)));
    BrandVal[3] =  OSCL_STATIC_CAST(char, aBrandVal);
}

void Mpeg4File::DeleteAPICStruct(PvmfApicStruct*& aAPICStruct)
{
    OSCL_ARRAY_DELETE(aAPICStruct->iGraphicData);
    OSCL_DELETE(aAPICStruct);
    aAPICStruct = NULL;
}

void Mpeg4File::SetMoofAtomsCnt(const uint32 aMoofAtmsCnt)
{

    iTotalMoofAtmsCnt = aMoofAtmsCnt;
}

MP4_ERROR_CODE Mpeg4File::GetUserDataAtomInfo(uint32 &atomSize, TOsclFileOffset &atomOffset)
{
    if (_pmovieAtom)
        return _pmovieAtom->GetUserDataAtomInfo(atomSize, atomOffset);

    return DEFAULT_ERROR;
}

void Mpeg4File::SetMoofInfo(uint32 aTrackId, uint32 aIndex, uint64 aMoofOffset, uint64 aMoofTimestamp)
{
    if (_isMovieFragmentsPresent)
    {
        if (_pMovieFragmentRandomAccessAtomVec != NULL)
        { // Only one mfra possible in a clip so this loop will run only once
            for (uint32 idx = 0; idx < _pMovieFragmentRandomAccessAtomVec->size(); idx++)
            {
                MovieFragmentRandomAccessAtom *pMovieFragmentRandomAccessAtom = (*_pMovieFragmentRandomAccessAtomVec)[idx];
                pMovieFragmentRandomAccessAtom->updateMfraEntry(aTrackId, aIndex, aMoofOffset, aMoofTimestamp);
            }

        }
    }
}

