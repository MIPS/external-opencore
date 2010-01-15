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

#ifndef __PSSHAtom_H__
#define __PSSHAtom_H__

#include "extendedfullatom.h"
#include "atomutils.h"

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#define PSSH_SYSTEM_ID_LENGTH 16
const uint8 pssh_uuid[] = {0xd0, 0x8a, 0x4f, 0x18, 0x10, 0xf3, 0x4a, 0x82, 0xb6, 0xc8, 0x32, 0xd8, 0xab, 0xa1, 0x83, 0xd3};

class PVA_FF_PSSHAtom : public PVA_FF_ExtendedFullAtom
{

    public:
        PVA_FF_PSSHAtom(); // Constructor

        virtual ~PVA_FF_PSSHAtom();

        bool setSystemId(OsclMemoryFragment& aSystemId);

        bool setData(uint8* aData, uint32 aLength);


        // Rendering the PVA_FF_Atom in proper format (bitlengths, etc.) to an ostream
        virtual bool renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp);

        virtual void recomputeSize();

    protected:
        uint8 iSystemId[PSSH_SYSTEM_ID_LENGTH];
        OsclMemoryFragment iData;

};

#endif
