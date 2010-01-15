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
/*
    This PVA_FF_MovieAtom Class is the main atom class in the MPEG-4 File that stores
    all the meta data about the MPEG-4 presentation.
*/


#define IMPLEMENT_PSSHAtom

#include "psshatom.h"
#include "a_atomdefs.h"

PVA_FF_PSSHAtom::PVA_FF_PSSHAtom()
        :   PVA_FF_ExtendedFullAtom(pssh_uuid, 0, 0)
{
    iData.len = 0;
    iData.ptr = NULL;
}

PVA_FF_PSSHAtom::~PVA_FF_PSSHAtom()
{
    if (iData.ptr != NULL)
        OSCL_FREE(iData.ptr);
}


bool PVA_FF_PSSHAtom::setSystemId(OsclMemoryFragment& aSystemId)
{
    if (aSystemId.len != PSSH_SYSTEM_ID_LENGTH)
        return false;

    oscl_memcpy(iSystemId, aSystemId.ptr, PSSH_SYSTEM_ID_LENGTH);
    return true;
}

bool PVA_FF_PSSHAtom::setData(uint8* aData, uint32 aLength)
{
    iData.len = aLength;
    if (iData.ptr != NULL)
        OSCL_FREE(iData.ptr);

    iData.ptr = NULL;
    if (iData.len > 0)
    {
        iData.ptr = (uint8*)OSCL_MALLOC(aLength);
        if (iData.ptr == NULL)
            return false;

        // make a copy of the data
        oscl_memcpy(iData.ptr, aData, aLength);
    }

    return true;
}


bool
PVA_FF_PSSHAtom::renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp)
{
    int32 rendered = 0; // Keep track of number of bytes rendered

    // Render PVA_FF_Atom type and size
    if (!renderAtomBaseMembers(fp))
    {
        return false;
    }
    rendered += getDefaultSize();

    // Render system id
    if (!PVA_FF_AtomUtils::renderByteData(fp, PSSH_SYSTEM_ID_LENGTH, iSystemId))
    {
        return false;
    }

    rendered +=  PSSH_SYSTEM_ID_LENGTH;

    // Render data length
    if (!PVA_FF_AtomUtils::render32(fp, iData.len))
    {
        return false;
    }

    rendered += 4;

    if (iData.len > 0)
        if (!PVA_FF_AtomUtils::renderByteData(fp, iData.len, (uint8*)iData.ptr))
        {
            return false;
        }

    rendered += iData.len;

    return true;
}


void
PVA_FF_PSSHAtom::recomputeSize()
{
    _size = getDefaultSize();

    // System Id
    _size += EXTENDED_ATOM_UUID_LENGTH;

    // data length
    _size += 4;

    // data
    _size += iData.len;

}
