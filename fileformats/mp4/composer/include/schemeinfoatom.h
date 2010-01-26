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


#ifndef __SchemeInfoAtom_H__
#define __SchemeInfoAtom_H__

#include "atomutils.h"
#include "atom.h"

class PVA_FF_OriginalFormatAtom : public PVA_FF_Atom
{
    public:
        PVA_FF_OriginalFormatAtom(uint32 originalformat);
        ~PVA_FF_OriginalFormatAtom() {}

        uint32 getOriginalFormat()
        {
            return _originalformat;
        }

        // From PVA_FF_Atom
        virtual void recomputeSize();
        // Rendering the PVA_FF_Atom in proper format (bitlengths, etc.) to an ostream
        virtual bool renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp);

    private:
        uint32 _originalformat;
};

class PVA_FF_ProtectionSchemeInfoAtom : public PVA_FF_Atom
{
    public:
        PVA_FF_ProtectionSchemeInfoAtom(uint32 originalformat);
        ~PVA_FF_ProtectionSchemeInfoAtom() {};


        // From PVA_FF_Atom
        virtual void recomputeSize();
        // Rendering the PVA_FF_Atom in proper format (bitlengths, etc.) to an ostream
        virtual bool renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp);

    private:
        PVA_FF_OriginalFormatAtom _originalFormatAtom;
};




#endif

