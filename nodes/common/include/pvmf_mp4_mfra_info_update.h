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
#ifndef PVMF_MP4FF_MFRA_INFO_UPDATE_H_INCLUDED
#define PVMF_MP4FF_MFRA_INFO_UPDATE_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif

class PVMFMP4MfraInfoUpdate
{
    public:
        // contructor
        PVMFMP4MfraInfoUpdate() : iMoofTrackID(0), iMoofIndex(0), iMoofTime(0), iMoofOffset(0)
        {
            ;
        }

        // copy constructor
        PVMFMP4MfraInfoUpdate(const PVMFMP4MfraInfoUpdate& x)
        {
            iMoofTrackID = x.iMoofTrackID;
            iMoofIndex   = x.iMoofIndex;
            iMoofTime    = x.iMoofTime;
            iMoofOffset  = x.iMoofOffset;
        }

        bool isInfoValid()
        {
            return (iMoofTrackID > 0 && iMoofOffset > 0);
        }

        void clear()
        {
            iMoofTrackID = 0;
            iMoofIndex   = 0;
            iMoofTime    = 0;
            iMoofOffset  = 0;
        }

    public:
        uint32 iMoofTrackID;
        uint32 iMoofIndex;
        uint64 iMoofTime;
        uint64 iMoofOffset;
};

#endif
