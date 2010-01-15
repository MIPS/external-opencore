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

#define IMPLEMENT_ExtendedAtom

#include "extendedatom.h"
#include "a_atomdefs.h"

PVA_FF_ExtendedAtom::PVA_FF_ExtendedAtom(const uint8 aUuid[EXTENDED_ATOM_UUID_LENGTH])
        :   PVA_FF_Atom(UUID_ATOM)
{
    oscl_memcpy(uuid, aUuid, EXTENDED_ATOM_UUID_LENGTH);
}


bool
PVA_FF_ExtendedAtom::renderAtomBaseMembers(MP4_AUTHOR_FF_FILE_IO_WRAP *fp) const
{
    // Render PVA_FF_Atom type and size
    if (!PVA_FF_Atom::renderAtomBaseMembers(fp))
    {
        return false;
    }

    if (!PVA_FF_AtomUtils::renderByteData(fp, EXTENDED_ATOM_UUID_LENGTH, (uint8*)uuid))
    {
        return false;
    }

    return true;
}

uint32 PVA_FF_ExtendedAtom::getDefaultSize() const
{
    return PVA_FF_Atom::getDefaultSize() + EXTENDED_ATOM_UUID_LENGTH;
}
