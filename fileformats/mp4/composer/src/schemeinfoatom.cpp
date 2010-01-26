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


#define IMPLEMENT_SchemeInfoAtom

#include "schemeinfoatom.h"
#include "a_atomdefs.h"

PVA_FF_OriginalFormatAtom::PVA_FF_OriginalFormatAtom(uint32 originalformat)
        :   PVA_FF_Atom(ORIGINAL_FORMAT_ATOM), _originalformat(originalformat)
{
    recomputeSize();
}

bool
PVA_FF_OriginalFormatAtom::renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp)
{
    int32 rendered = 0; // Keep track of number of bytes rendered

    // Render PVA_FF_Atom type and size
    if (!renderAtomBaseMembers(fp))
    {
        return false;
    }
    rendered += getDefaultSize();

    // Render original format
    if (!PVA_FF_AtomUtils::render32(fp, _originalformat))
    {
        return false;
    }

    rendered += 4;

    return true;
}


void
PVA_FF_OriginalFormatAtom::recomputeSize()
{
    _size = getDefaultSize();

    // Original format
    _size += 4;
}


PVA_FF_ProtectionSchemeInfoAtom::PVA_FF_ProtectionSchemeInfoAtom(uint32 originalformat)
        :   PVA_FF_Atom(PROTECTION_SCHEME_INFO_ATOM), _originalFormatAtom(originalformat)
{
    recomputeSize();
}

bool
PVA_FF_ProtectionSchemeInfoAtom::renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp)
{
    int32 rendered = 0; // Keep track of number of bytes rendered

    // Render PVA_FF_Atom type and size
    if (!renderAtomBaseMembers(fp))
    {
        return false;
    }
    rendered += getDefaultSize();

    if (! _originalFormatAtom.renderToFileStream(fp))
    {
        return false;
    }

    rendered += _originalFormatAtom.getSize();

    return true;
}


void
PVA_FF_ProtectionSchemeInfoAtom::recomputeSize()
{
    _size = getDefaultSize();

    // Original format
    _size += _originalFormatAtom.getSize();
}


