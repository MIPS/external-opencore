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

#define IMPLEMENT_ExtendedFullAtom

#include "extendedfullatom.h"
#include "a_atomdefs.h"

PVA_FF_ExtendedFullAtom::PVA_FF_ExtendedFullAtom(const uint8 aUuid[16], uint8 version, uint32 flags)
        :   PVA_FF_ExtendedAtom(aUuid)
{
    _version = version;
    _flags = flags;
}


bool
PVA_FF_ExtendedFullAtom::renderAtomBaseMembers(MP4_AUTHOR_FF_FILE_IO_WRAP *fp) const
{
    // Render PVA_FF_Atom type and size
    if (!PVA_FF_ExtendedAtom::renderAtomBaseMembers(fp))
    {
        return false;
    }

    // Render Version
    if (!PVA_FF_AtomUtils::render8(fp, getVersion()))
    {
        return false;
    }

    // Render flags
    if (!PVA_FF_AtomUtils::render24(fp, getFlags()))
    {
        return false;
    }

    return true;
}

uint32 PVA_FF_ExtendedFullAtom::getDefaultSize() const
{
    // default size plus version and flags info
    return PVA_FF_ExtendedAtom::getDefaultSize() + 4;
}

