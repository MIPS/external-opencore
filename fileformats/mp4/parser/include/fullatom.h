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
/*********************************************************************************/
/*     -------------------------------------------------------------------       */
/*                            MPEG-4 FullAtom Class                              */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    This FullAtom Class is the base class for some Atoms in the MPEG-4 File
    Format.
*/


#ifndef FULLATOM_H_INCLUDED
#define FULLATOM_H_INCLUDED

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

#ifndef ATOM_H_INCLUDED
#include "atom.h"
#endif

#ifndef ATOMUTILS_H_INCLUDED
#include "atomutils.h"
#endif

const uint32 DEFAULT_FULL_ATOM_SIZE = 12; // (8 bytes from Atom + 1 for
//  version and 3 for flags)

class FullAtom : public Atom
{

    public:
        FullAtom(uint32 type, uint8 version, uint32 flags); // Constructor
        FullAtom(MP4_FF_FILE *fp);
        FullAtom(MP4_FF_FILE *fp, uint32 size, uint32 type);
        OSCL_IMPORT_REF virtual ~FullAtom();

        // No "set" methods as they get set directly in the constructor
        uint8 getVersion() const
        {
            return _version;
        }
        uint32 getFlags() const
        {
            return _flags;
        }

        OSCL_IMPORT_REF virtual uint32 getDefaultSize() const;

    private:
        uint8 _version; // 1 (8bits)
        uint32 _flags; // 3 (24bits) -- Will need to crop when writing to stream
};

//ExtendedFullAtom should be subobject of the specialized atoms of kind UUID only
class ExtendedFullAtom: public ExtendedAtom
{
    public:
    public:
        ExtendedFullAtom(MP4_FF_FILE* const& aFilePtr, uint32 aSize, uint32 aType, const uint8* const& aUserTypeUUID);

        virtual ~ExtendedFullAtom() {}

        virtual uint32 getDefaultSize() const
        {
            return (ExtendedAtom::getDefaultSize() + SIZE_OF_VERS_FLD_IN_BYTES + SIZE_OF_FLAGS_FLD_IN_BYTES);
        }

        virtual uint8 getVersion() const
        {
            return _version;
        }

        virtual uint32 getFlags() const
        {
            return _flags;
        }

    private:
        uint8 _version; // 1 (8bits)
        uint32 _flags; // 3 (24bits)
        enum
        {
            SIZE_OF_VERS_FLD_IN_BYTES = 1,
            SIZE_OF_FLAGS_FLD_IN_BYTES = 3
        };
};

#endif // FULLATOM_H_INCLUDED


