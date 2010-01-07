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
#ifndef PVMF_SOURCE_NODE_UTILS_H_INCLUDED
#define PVMF_SOURCE_NODE_UTILS_H_INCLUDED

#ifndef OSCL_REFCOUNTER_MEMFRAG_H_INCLUDED
#include "oscl_refcounter_memfrag.h"
#endif
#ifndef PVMF_FORMAT_TYPE_H_INCLUDED
#include "pvmf_format_type.h"
#endif
#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif
#ifndef PVMF_GAPLESS_METADATA_H_INCLUDED
#include "pvmf_gapless_metadata.h"
#endif
#ifndef OSCL_FILE_HANDLE_H_INCLUDED
#include "oscl_file_handle.h"
#endif
#ifndef PVMF_SOURCE_CONTEXT_DATA_H_INCLUDED
#include "pvmf_source_context_data.h"
#endif
#ifndef PVMF_LOCAL_DATA_SOURCE_H_INCLUDED
#include "pvmf_local_data_source.h"
#endif
/* class PVMFSourceClipInfo
 * Description :
 * This class is intended to collect and store Source Initialization data.
 * All source nodes supporting playlist playback, should use this class
 * for storing data related to clip information.
 *
 */

class PVMFSourceClipInfo
{
    public:
        PVMFSourceClipInfo()
        {
            iSourceURL = NULL;
            iFileHandle = NULL;
            iSourceFormatType = PVMF_MIME_FORMAT_UNKNOWN;
            iGaplessInfoAvailable = false;
            iSendBOC = false;
            iSendEOC = false;
            iHasBOCFrame = false;
            iHasEOCFrame = false;
            iIsInitialized = false;
            iFrameBOC = 0;
            iFirstFrameEOC = 0;
            iFramesToFollowEOC = 0;

            iSourceContextDataValid = false;
            iCPMSourceData.iFileHandle = NULL;
        };

        ~PVMFSourceClipInfo() {};

        OSCL_wHeapString<OsclMemAllocator>& GetSourceURL()
        {
            return iSourceURL;
        }

        PVMFFormatType& GetFormatType()
        {
            return iSourceFormatType;
        }

        OsclFileHandle* GetFileHandle()
        {
            return iFileHandle;
        }

        uint32 GetClipIndex()
        {
            return iClipIndex;
        }

        void SetSourceURL(OSCL_wHeapString<OsclMemAllocator> aSourceURL)
        {
            iSourceURL = aSourceURL;
        }

        void SetFormatType(PVMFFormatType aFormatType)
        {
            iSourceFormatType = aFormatType;
        }

        void SetFileHandle(OsclFileHandle* aFileHandle)
        {
            iFileHandle = aFileHandle;
        }

        void SetClipIndex(uint32 aClipIndex)
        {
            iClipIndex = aClipIndex;
        }

    public:
        // gapless metadata
        bool iGaplessInfoAvailable;
        PVMFGaplessMetadata iGaplessMetadata;
        bool iSendBOC;
        bool iSendEOC;
        bool iHasBOCFrame;
        bool iHasEOCFrame;
        uint64 iFrameBOC;
        uint64 iFirstFrameEOC;
        uint32 iFramesToFollowEOC;
        uint32 iIsInitialized;

        // BOC and EOC format specific info stored in a OsclRefCounterMemFrag
        OsclMemAllocDestructDealloc<uint8> iBOCFormatSpecificInfoAlloc;
        OsclRefCounterMemFrag iBOCFormatSpecificInfo;

        OsclMemAllocDestructDealloc<uint8> iEOCFormatSpecificInfoAlloc;
        OsclRefCounterMemFrag iEOCFormatSpecificInfo;

        PVMFSourceContextData iSourceContextData;
        bool iSourceContextDataValid;
        PVMFLocalDataSource iCPMSourceData;

    private:
        OSCL_wHeapString<OsclMemAllocator> iSourceURL;
        PVMFFormatType  iSourceFormatType;
        OsclFileHandle* iFileHandle;
        uint32          iClipIndex;
};


#endif //PVMF_SOURCE_NODE_UTILS_H_INCLUDED



