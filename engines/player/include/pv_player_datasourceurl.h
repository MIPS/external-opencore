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
#ifndef PV_PLAYER_DATASOURCEURL_H_INCLUDED
#define PV_PLAYER_DATASOURCEURL_H_INCLUDED

#ifndef PV_PLAYER_DATASOURCE_H_INCLUDED
#include "pv_player_datasource.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif

#ifndef OSCL_VECTOR_H_INCLUDED
#include "oscl_vector.h"
#endif

class PVPlayerDataSourceURL : public PVPlayerDataSource
{
    public:
        PVPlayerDataSourceURL()
        {
            OneClip src;
            iClipList.push_back(src);
            iCurrentClip = 0;
        }

        ~PVPlayerDataSourceURL()
        {}

        PVPDataSourceType GetDataSourceType()
        {
            return PVP_DATASRCTYPE_URL;
        }

        PVMFFormatType GetDataSourceFormatType()
        {
            return iClipList[iCurrentClip].iFormatType;
        }

        OSCL_wString& GetDataSourceURL()
        {
            return iClipList[iCurrentClip].iURL;
        }

        OsclAny* GetDataSourceContextData()
        {
            return iClipList[iCurrentClip].iContext;
        }

        PVMFNodeInterface* GetDataSourceNodeInterface()
        {
            return NULL;
        }

        void SetDataSourceFormatType(PVMFFormatType aFormatType)
        {
            iClipList[iCurrentClip].iFormatType = aFormatType;
        }

        void SetDataSourceURL(const OSCL_wString& aURL)
        {
            iClipList[iCurrentClip].iURL = aURL;
        }

        void SetDataSourceContextData(const OsclAny* aContext)
        {
            iClipList[iCurrentClip].iContext = (OsclAny*)aContext;
        }

        bool SetAlternateSourceFormatType(PVMFFormatType aFormatType)
        {
            int32 err;
            OSCL_TRY(err, iClipList[iCurrentClip].iAlternateFormatTypeVec.push_back(aFormatType););
            if (err != OsclErrNone)
            {
                return false;
            }
            return true;
        }

        uint32 GetNumAlternateSourceFormatTypes()
        {
            return (iClipList[iCurrentClip].iAlternateFormatTypeVec.size());
        }

        bool GetAlternateSourceFormatType(PVMFFormatType& aFormatType,
                                          uint32 aIndex)
        {
            if (aIndex < iClipList[iCurrentClip].iAlternateFormatTypeVec.size())
            {
                aFormatType = iClipList[iCurrentClip].iAlternateFormatTypeVec[aIndex];
                return true;
            }
            return false;
        }

        uint32 GetNumClips()
        {
            return iClipList.size();
        }
        uint32 ExtendClipList()
        {
            OneClip src;
            iClipList.push_back(src);
            iCurrentClip = iClipList.size() - 1;
            return iCurrentClip;
        }
        uint32 SetCurrentClip(uint32 aIndex)
        {
            if (aIndex < iClipList.size())
                iCurrentClip = aIndex;
            return iCurrentClip;
        }
        uint32 GetCurrentClip()
        {
            return iCurrentClip;
        }

    private:
        class OneClip
        {
            public:
                OneClip() : iFormatType(PVMF_MIME_FORMAT_UNKNOWN), iURL(NULL), iContext(NULL)
                {}
                PVMFFormatType iFormatType;
                OSCL_wHeapString<OsclMemAllocator> iURL;
                OsclAny* iContext;
                Oscl_Vector<PVMFFormatType, OsclMemAllocator> iAlternateFormatTypeVec;
        };
        uint32 iCurrentClip;
        Oscl_Vector<OneClip, OsclMemAllocator> iClipList;
};

#endif // PV_PLAYER_DATASOURCEURL_H_INCLUDED

