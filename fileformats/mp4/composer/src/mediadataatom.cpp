/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
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
/*
    This PVA_FF_MediaDataAtom Class contains the media data.  This class can operate in
    either one of two ways - 1. it can store all it's data in memory (such as
    during the creation of PVA_FF_ObjectDescriptor streams), or 2. it can maintain all
    it's data on disk (such as during the creation ofmedia streams - i.e. audio
    and video).

    Note that during reading in this atom from a file stream, the type fp forced
    to MEDIA_DATA_ON_DISK thereby keeping all the object data in the physical
    file.
*/

#define IMPLEMENT_MediaDataAtom

#include "mediadataatom.h"
#include "atomutils.h"
#include "a_atomdefs.h"
#include "oscl_byte_order.h"
#include "oscl_bin_stream.h"

#define TEMP_TO_TARGET_FILE_COPY_BLOCK_SIZE 1024

typedef Oscl_Vector<PVA_FF_Renderable*, OsclMemAllocator> PVA_FF_RenderableVecType;
typedef Oscl_Vector<PVA_FF_TrackAtom*, OsclMemAllocator> PVA_FF_TrackAtomVecType;

// Constructor
PVA_FF_MediaDataAtom::PVA_FF_MediaDataAtom
(
    PVA_FF_UNICODE_STRING_PARAM targetFileName,
    MP4_AUTHOR_FF_FILE_HANDLE targetFileHandle,
    void  *osclFileServerSession,
    uint32 aCacheSize,
    PVA_FF_UNICODE_STRING_PARAM outputPathString,
    PVA_FF_UNICODE_STRING_PARAM postfixString,
    int32 tempFileIndex
)
        : PVA_FF_Atom(MEDIA_DATA_ATOM)
{
    _osclFileServerSession = osclFileServerSession;

    _prenderables = NULL;
    PV_MP4_FF_NEW(fp->auditCB, PVA_FF_RenderableVecType, (), _prenderables);
    PV_MP4_FF_NEW(fp->auditCB, PVA_FF_TrackAtomVecType, (), _ptrackReferencePtrVec);

    // ADDED TO CHECK FOR ANY FILE WRITE FAILURES
    _fileWriteError = false;
    _targetFileWriteError = false;
    _oIsFileOpen = false;

    _fileSize = 0;
    _fileOffsetForChunkStart = 0;

    _pofstream._filePtr = NULL;
    _ptrackReferencePtr = NULL;
    _targetFileMediaStartOffset = 0;
    _totalDataRenderedToTargetFile = 0;

    _tempFilePostfix = postfixString;
    _tempFilename = outputPathString;
    _tempFileIndex = tempFileIndex;

    _osclFileServerSession = osclFileServerSession;
    // ADDED TO CHECK FOR ANY FILE WRITE FAILURES

    if (0 < targetFileName.get_size())
    {
        _pofstream._osclFileServerSession = OSCL_STATIC_CAST(Oscl_FileServer*, _osclFileServerSession);
        int retVal = PVA_FF_AtomUtils::openFile(&_pofstream, targetFileName, Oscl_File::MODE_READWRITE | Oscl_File::MODE_BINARY, aCacheSize);
        _oIsFileOpen = true;

        if (_pofstream._filePtr == NULL)
        {
            _fileWriteError = true;
        }
        else if (retVal == 0)
        {
            _targetFileWriteError = true;
            if (_pofstream._filePtr != NULL)
            {
                PVA_FF_AtomUtils::closeFile(&_pofstream);
                _pofstream._filePtr = NULL;
            }
        }
    }

    if (targetFileHandle != NULL)
        _pofstream._filePtr = targetFileHandle;

    recomputeSize();
    // Preparing the temp output file for rendering the atom data
    prepareTempFile(aCacheSize);

}

// Destructor
PVA_FF_MediaDataAtom::~PVA_FF_MediaDataAtom()
{
    if (_pofstream._filePtr != NULL && true == _oIsFileOpen)
    {
        PVA_FF_AtomUtils::closeFile(&_pofstream);
        _pofstream._filePtr = NULL;
    }

    // PVA_FF_TrackAtom *_ptrackReferencePtr - is taken care of by the movie atom
    // Delete vector<PVA_FF_Renderable*> *_prenderables
    if (_prenderables != NULL)
    {
        for (uint32 i = 0; i < _prenderables->size(); i++)
        {
            if ((*_prenderables)[i] != NULL)
            {
                OSCL_DELETE((*_prenderables)[i]);
                //PV_MP4_FF_DELETE(NULL,PVA_FF_Renderable,(*_prenderables)[i]);
                (*_prenderables)[i] = NULL;
            }
        }
        PV_MP4_FF_TEMPLATED_DELETE(NULL, PVA_FF_RenderableVecType, Oscl_Vector, _prenderables);
        _prenderables = NULL;
    }

    //Contents of this array are deleted in movie atom
    //OSCL_DELETE(_ptrackReferencePtrVec);

    PV_MP4_FF_TEMPLATED_DELETE(NULL, PVA_FF_TrackAtomVecType, Oscl_Vector, _ptrackReferencePtrVec);

    Oscl_FileServer fileServ;
    fileServ.Connect();
    fileServ.Oscl_DeleteFile(_tempFilename.get_cstr());
    fileServ.Close();
}

// Create the atom temp file and the corresponding ofstream
void
PVA_FF_MediaDataAtom::prepareTempFile(uint32 aCacheSize)
{
    if (0 != _pofstream._filePtr || true == _fileWriteError)
        return;

    // 05/31/01 Generate temporary files into output path (the actual mp4 location)
    // _tempFilename already contains the output path ("drive:\\...\\...\\")
    _tempFilename += _STRLIT("temp");
    // Assign the rest of the temp filename - index plus suffix
    _tempFilename += (uint16)(_tempFileIndex++);
    _tempFilename += _STRLIT("_");
    _tempFilename += _tempFilePostfix;
    _tempFilename += _STRLIT(".mdat");

    _pofstream._osclFileServerSession = OSCL_STATIC_CAST(Oscl_FileServer*, _osclFileServerSession);

    PVA_FF_AtomUtils::openFile(&_pofstream, _tempFilename, Oscl_File::MODE_READWRITE | Oscl_File::MODE_BINARY, aCacheSize);

    if (_pofstream._filePtr == NULL)
    {
        _fileWriteError = true;
    }
    else
    {
        _oIsFileOpen = true;
    }

    renderAtomBaseMembers(&_pofstream); // render base members to media data atom file

    _fileOffsetForChunkStart = getDefaultSize();
    _fileSize = getDefaultSize();
}

bool
PVA_FF_MediaDataAtom::prepareTargetFile(uint32 mediaStartOffset)
{
    if (0 == _pofstream._filePtr || true == _fileWriteError)
        return false;

    if (0 < mediaStartOffset)
    {
        // Write zeros to accomodate the user data upfront
        uint8* tempBuffer = NULL;
        PV_MP4_FF_ARRAY_NEW(NULL, uint8, mediaStartOffset, tempBuffer);

        oscl_memset(tempBuffer, 0, mediaStartOffset);

        if (!(PVA_FF_AtomUtils::renderByteData(&_pofstream, mediaStartOffset, tempBuffer)))
        {
            PV_MP4_ARRAY_DELETE(NULL, tempBuffer);
            return false;
        }
        PV_MP4_ARRAY_DELETE(NULL, tempBuffer);
    }

    // Render the atoms base members to the media data atom file
    renderAtomBaseMembers(&_pofstream);

    _fileOffsetForChunkStart = getDefaultSize();
    _fileSize = getDefaultSize();
    _targetFileMediaStartOffset = mediaStartOffset;

    return true;
}


uint32
PVA_FF_MediaDataAtom::prepareTargetFileForFragments(uint32 mediaStartOffset)
{
    _targetFileMediaStartOffset = mediaStartOffset;
    PVA_FF_AtomUtils::seekFromStart(&_pofstream, _targetFileMediaStartOffset);

    renderAtomBaseMembers(&_pofstream);
    _fileOffsetForChunkStart = getDefaultSize();
    _fileSize = getDefaultSize();

    return _fileOffsetForChunkStart;
}

bool
PVA_FF_MediaDataAtom::closeTargetFile()
{
    if (0 == _pofstream._filePtr || true == _fileWriteError)
        return false;

    // Get current position of put pointer
    _totalDataRenderedToTargetFile =
        PVA_FF_AtomUtils::getCurrentFilePosition(&_pofstream);

    // Go to the beginning of the media data
    PVA_FF_AtomUtils::seekFromStart(&_pofstream, _targetFileMediaStartOffset);

    // Update size field
    if (false == PVA_FF_AtomUtils::render32(&_pofstream, getSize()))
        return false;

    // Return the _pofstream's pointer to start
    PVA_FF_AtomUtils::seekFromStart(&_pofstream, 0);

    _fileOffsetForChunkStart =
        _targetFileMediaStartOffset + getDefaultSize();

    return true;
}

Oscl_File*
PVA_FF_MediaDataAtom::getTargetFilePtr()
{
    return (_pofstream._filePtr);
}

// Adds more data to the atom then update the atom size field (first 4 bytes)
bool
PVA_FF_MediaDataAtom::addRawSample(void *psample, uint32 length)
{
    if (true == _fileWriteError || 0 == _pofstream._filePtr)
        return false;

    _fileWriteError = (false == PVA_FF_AtomUtils::renderByteData(&_pofstream, length, (uint8*)psample));
    _fileSize += length;                        // update the size of the atom
    recomputeSize();                            // update the size of the atom

    return (false == _fileWriteError);
}

bool PVA_FF_MediaDataAtom::addRawSample(Oscl_Vector <OsclMemoryFragment, OsclMemAllocator>& fragmentList,
                                        uint32 length, int32 mediaType, PVA_FF_MP4_CODEC_TYPE codecType)
{
    if (true == _fileWriteError || 0 == _pofstream._filePtr)
        return false;

    bool retVal = true;

    if (MEDIA_TYPE_VISUAL == (uint32)mediaType && PVA_FF_MP4_CODEC_TYPE_AVC_VIDEO == codecType)
    {
        uint32 nalLength = 0;
        OsclBinIStreamBigEndian stream;
        for (uint32 ii = 0; ii < fragmentList.size(); ++ii)
        {
            // read NAL length in Big Endian format
            stream.Attach((OsclAny*) &(fragmentList[ii].len), 4);
            stream >> nalLength;

            // compose nal length in two bytes
            if (false == PVA_FF_AtomUtils::renderByteData(&_pofstream, 4, (uint8 *) & nalLength))
            {
                _fileWriteError = true;
                retVal = false;
            }

            // write NAL uint
            if (false == PVA_FF_AtomUtils::renderByteData(&_pofstream, fragmentList[ii].len, (uint8 *)fragmentList[ii].ptr))
            {
                _fileWriteError = true;
                retVal = false;
            }
        }
    }
    else
    {
        for (uint32 ii = 0; ii < fragmentList.size(); ++ii)
        {
            if (false == PVA_FF_AtomUtils::renderByteData(&_pofstream, fragmentList[ii].len, (uint8 *)fragmentList[ii].ptr))
            {
                _fileWriteError = true;
                retVal = false;
            }
        }
    }

    _fileSize += length;                          // update the size of the atom
    recomputeSize();                              // update the size of the atom

    return retVal;
}

int32
PVA_FF_MediaDataAtom::addRenderableSample(PVA_FF_Renderable *psample)
{
    uint32 length = psample->getSize();
    psample->renderToFileStream(&_pofstream);
    _fileSize += length;

    recomputeSize();
    return length;
}


// Allocates in-memory space for the media data
void
PVA_FF_MediaDataAtom::reserveBuffer(int32 size)
{
    OSCL_UNUSED_ARG(size);
}

void
PVA_FF_MediaDataAtom::recomputeSize()
{
    // Entire atom size fp same as atom file size
    _size = (0 == _fileSize) ? getDefaultSize() : _fileSize;
}

uint32
PVA_FF_MediaDataAtom::getMediaDataSize()
{
    recomputeSize();
    return getSize();
}

// Rendering the PVA_FF_Atom in proper format (bitlengths, etc.) to an ostream
bool
PVA_FF_MediaDataAtom::renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp)
{

    int32 rendered = 0; // Keep track of number of bytes rendered

    int32 currentPos = PVA_FF_AtomUtils::getCurrentFilePosition(&_pofstream);    // Get current position of put pointer
    PVA_FF_AtomUtils::seekFromStart(&_pofstream, 0);          // Go to the beginning of the file
    if (!PVA_FF_AtomUtils::render32(&_pofstream, getSize()))
    {
        return false;
    }
    // Update size field
    PVA_FF_AtomUtils::seekFromStart(&_pofstream, currentPos); // Return the ostream's put pointer

    // Cleanup and close temp data output file
    if (_pofstream._filePtr != NULL)
    {
        PVA_FF_AtomUtils::closeFile(&_pofstream);
        _pofstream._filePtr = NULL;
    }

    // Open the file in which this mdat atom was stored
    MP4_AUTHOR_FF_FILE_IO_WRAP mdatFilePtr;
    mdatFilePtr._filePtr = NULL;
    mdatFilePtr._osclFileServerSession = OSCL_STATIC_CAST(Oscl_FileServer*, _osclFileServerSession);
    PVA_FF_AtomUtils::openFile(&mdatFilePtr, _tempFilename, Oscl_File::MODE_READ | Oscl_File::MODE_BINARY);

    // In the case where the mdat atom fp stored on disk file,
    // the atom just gets directly copied - i.e. there fp no atom-specific
    // rendering.  We need to adjust the fileOffset by the size of the
    // atom header (based on what the header "should" be).

    // BREAKING CONST RULES!!!

    // Need to set the actual file offset where the actual chunk data begins
    // so before rendering the ChunkOffetAtom, we can shift the PVA_FF_ChunkOffsetAtom
    // table elements by this offset - the table elements are actual file offsets
    // and NOT just offsets from the first chunk (i.e. zero) and we don't really
    // know this offset until now (during rendering).
    PVA_FF_MediaDataAtom *This = const_cast<PVA_FF_MediaDataAtom*>(this);
    This->setFileOffsetForChunkStart((uint32)(PVA_FF_AtomUtils::getCurrentFilePosition(fp)) +
                                     (uint32)getDefaultSize());

    // Read in atom from separate file and copy byte-by-byte to new ofstream
    // (including the mediaDataAtom header - 4 byte size ad 4 byte type)

    uint32 readBlockSize = 0;
    uint32 tempFileSize  = getSize();

    uint8 *dataBuf = NULL;

    PV_MP4_FF_ARRAY_NEW(NULL, uint8, TEMP_TO_TARGET_FILE_COPY_BLOCK_SIZE, dataBuf);

    while (0 < tempFileSize)
    {
        readBlockSize = (tempFileSize < TEMP_TO_TARGET_FILE_COPY_BLOCK_SIZE)
                        ? tempFileSize : TEMP_TO_TARGET_FILE_COPY_BLOCK_SIZE;

        if (true != PVA_FF_AtomUtils::readByteData(&mdatFilePtr, readBlockSize, dataBuf))
        {
            _targetFileWriteError = true;
            return false;
        }

        if (true != PVA_FF_AtomUtils::renderByteData(fp, readBlockSize, dataBuf))
        {
            _targetFileWriteError = true;
            return false;
        }
        tempFileSize -= readBlockSize;
    }

    PV_MP4_FF_DELETE(NULL, uint8, dataBuf);
    rendered += _fileSize;
    PVA_FF_AtomUtils::closeFile(&mdatFilePtr);
    return true;
}

