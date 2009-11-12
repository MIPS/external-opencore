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
    This PVA_FF_MediaDataAtom Class contains the media data.
*/


#ifndef __MediaDataAtom_H__
#define __MediaDataAtom_H__

#include "atom.h"
#include "a_atomdefs.h"
#include "renderable.h"
#include "trackatom.h"

#include <stdio.h>

class PVA_FF_MediaDataAtom : public PVA_FF_Atom
{

    public:
        PVA_FF_MediaDataAtom(PVA_FF_UNICODE_STRING_PARAM targetFileName,
                             MP4_AUTHOR_FF_FILE_HANDLE targetFileHandle,
                             void* osclFileServerSession = NULL,
                             uint32 aCacheSize = 0,
                             PVA_FF_UNICODE_STRING_PARAM outputPathString = PVA_FF_UNICODE_HEAP_STRING(_STRLIT_WCHAR("")),
                             PVA_FF_UNICODE_STRING_PARAM postfixString = PVA_FF_UNICODE_HEAP_STRING(_STRLIT_WCHAR("")),
                             int32 tempFileIndex = -1); // Constructor

        virtual ~PVA_FF_MediaDataAtom();

        // Add actual media sample to atom and return the size of the data
        bool addRawSample(void *psample, uint32 length = 0);
        // accepts memory fragments
        bool addRawSample(Oscl_Vector <OsclMemoryFragment, OsclMemAllocator>& fragmentList,
                          uint32 length, int32 mediaType, PVA_FF_MP4_CODEC_TYPE codecType);

        int32 addRenderableSample(PVA_FF_Renderable *psample); // Adds to vector of renderables

        // Get and set the file offset where the chunks (data) start
        void setFileOffsetForChunkStart(uint32 offset)
        {
            _fileOffsetForChunkStart = offset;
        }
        uint32 getFileOffsetForChunkStart() const
        {
            return _fileOffsetForChunkStart;
        }

        // The trackReferencePtr keeps a ptr to the actual track that contains
        // the meta data for this media data atom
        void setTrackReferencePtr(PVA_FF_TrackAtom *ta)
        {
            _ptrackReferencePtr = ta;
            _ptrackReferencePtrVec->push_back(ta);
        }
        virtual void *getTrackReferencePtr()
        {
            return (void*)_ptrackReferencePtr;
        }

        virtual Oscl_Vector<PVA_FF_TrackAtom*, OsclMemAllocator> *getTrackReferencePtrVec()
        {
            return _ptrackReferencePtrVec;
        }
        const Oscl_Vector<PVA_FF_Renderable*, OsclMemAllocator> &getRenderables()
        {
            return *_prenderables;
        }
        void reserveBuffer(int32 size);

        // These are for FINAL rendering once the file data is complete
        // Rendering the PVA_FF_Atom in proper format (bitlengths, etc.) to an ostream
        virtual bool renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp);

        bool _targetFileWriteError;

        uint32 getMediaDataSize();

        bool prepareTargetFile(uint32 mediaOffset);
        uint32 prepareTargetFileForFragments(uint32 mediaStartOffset);
        bool closeTargetFile();

        uint32 getTotalDataRenderedToTargetFile() const
        {
            return _totalDataRenderedToTargetFile;
        }

        Oscl_File* getTargetFilePtr();

    private:
        virtual void recomputeSize();
        void prepareTempFile(uint32 aCache = 0);

        uint32 _fileSize; // Size (in bytes) of actual media data stored within the atom
        uint32 _fileOffsetForChunkStart;
        PVA_FF_TrackAtom *_ptrackReferencePtr;

        Oscl_Vector<PVA_FF_TrackAtom*, OsclMemAllocator> *_ptrackReferencePtrVec;
        PVA_FF_UNICODE_HEAP_STRING      _tempFilename;
        MP4_AUTHOR_FF_FILE_IO_WRAP      _pofstream; // Pointer to file stream that is temp storing the actual media data

        int32 _tempFileIndex;

        // 03/21/01 Add postfix string to handle multiple instances of the output filter,
        // the temporary file names will be different for every instances
        PVA_FF_UNICODE_HEAP_STRING _tempFilePostfix;

        Oscl_Vector<PVA_FF_Renderable*, OsclMemAllocator> *_prenderables;
        bool _fileWriteError;

        void*  _osclFileServerSession;
        uint32 _targetFileMediaStartOffset;
        uint32  _totalDataRenderedToTargetFile;
        bool _oIsFileOpen;
};



#endif

