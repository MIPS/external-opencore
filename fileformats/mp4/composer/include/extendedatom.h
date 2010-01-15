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
    This PVA_FF_ExtendedAtom Class adds support for uuid atom types as per ISO 14496-12 spec.
*/

#ifndef __ExtendedAtom_H__
#define __ExtendedAtom_H__

#include "atom.h"
#include "atomutils.h"

#define EXTENDED_ATOM_UUID_LENGTH 16

class PVA_FF_ExtendedAtom : public PVA_FF_Atom
{
    public:
        PVA_FF_ExtendedAtom(const uint8 aUuid[EXTENDED_ATOM_UUID_LENGTH]); // Constructor

        virtual uint32 getDefaultSize() const;

        virtual bool renderAtomBaseMembers(MP4_AUTHOR_FF_FILE_IO_WRAP *fp) const;

    protected:
        uint8 uuid[EXTENDED_ATOM_UUID_LENGTH];
};

#endif
