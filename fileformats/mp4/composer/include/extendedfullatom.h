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

#ifndef __ExtendedFullAtom_H__
#define __ExtendedFullAtom_H__

#include "extendedatom.h"
#include "atomutils.h"

class PVA_FF_ExtendedFullAtom : public PVA_FF_ExtendedAtom
{

    public:
        PVA_FF_ExtendedFullAtom(const uint8 aUuid[EXTENDED_ATOM_UUID_LENGTH],
                                uint8 version, uint32 flags); // Constructor

        // No "set" methods as they get set directly in the constructor
        uint8 getVersion() const
        {
            return _version;
        }

        uint32 getFlags() const
        {
            return _flags;
        }

    protected:
        virtual uint32 getDefaultSize() const;

        virtual bool renderAtomBaseMembers(MP4_AUTHOR_FF_FILE_IO_WRAP *fp) const;

    private:
        uint8 _version; // 1 (8bits)
        uint32 _flags; // 3 (24bits) -- Will need to crop when writing to stream
};

#endif
