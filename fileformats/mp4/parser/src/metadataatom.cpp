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
#include "metadataatom.h"
#include "atomdefs.h"


MetaDataAtom::MetaDataAtom(MP4_FF_FILE *fp, uint32 size, uint32 type): Atom(fp, size, type)
{
    _success = true;

    // User ilst Data
    _pITunesILSTAtom = NULL;
    _pHdlrAtom = NULL;
    _pid3v2Atom = NULL;

    uint32 _count = _size - getDefaultSize();

    uint32 data_32_hdlr = 0;
    iLogger = PVLogger::GetLoggerObject("mp4ffparser");

    // Skip first 4 bytes.
    if (!AtomUtils::read32(fp, data_32_hdlr))
    {
        _success = false;
        _mp4ErrorCode = READ_META_DATA_FAILED;
        PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>MetaDataAtom::MetaDataAtom READ_META_DATA_FAILED   if(!AtomUtils::read32(fp,data_32_hdlr)))"));
        return;
    }
    _count -= 4;

    while (_count > 0)
    {
        uint32 atomType = UNKNOWN_ATOM;
        uint32 atomSize = 0;

        TOsclFileOffset currPtr = AtomUtils::getCurrentFilePosition(fp);
        AtomUtils::getNextAtomType(fp, atomSize, atomType);

        if ((atomType == FREE_SPACE_ATOM) || (atomType == UNKNOWN_ATOM))
        {
            //skip the atom
            if (atomSize < DEFAULT_ATOM_SIZE)
            {
                _success = false;
                _mp4ErrorCode = ZERO_OR_NEGATIVE_ATOM_SIZE;
                break;
            }
            if (_count < atomSize)
            {
                AtomUtils::seekFromStart(fp, currPtr);
                AtomUtils::seekFromCurrPos(fp, (TOsclFileOffset)_count);
                _count = 0;
                return;
            }

            _count -= atomSize;
            atomSize -= DEFAULT_ATOM_SIZE;
            AtomUtils::seekFromCurrPos(fp, atomSize);
        }
        else if (atomType == HANDLER_ATOM)
        {
            PV_MP4_FF_NEW(fp->auditCB, HandlerAtom, (fp, atomSize, atomType), _pHdlrAtom);

            if (!_pHdlrAtom->MP4Success())
            {
                AtomUtils::seekFromStart(fp, currPtr);
                AtomUtils::seekFromCurrPos(fp, atomSize);
                PV_MP4_FF_DELETE(NULL, HandlerAtom, _pHdlrAtom);
                _pHdlrAtom = NULL;
                _count -= atomSize;
            }
            else
                _count -= _pHdlrAtom->getSize();

        }
        else if (atomType == ID3V2_ATOM)
        {
            PV_MP4_FF_NEW(fp->auditCB, ID3V2Atom, (fp, atomSize, atomType), _pid3v2Atom);//id32

            // Error checking
            if (!_pid3v2Atom->MP4Success())
            {
                AtomUtils::seekFromStart(fp, currPtr);
                AtomUtils::seekFromCurrPos(fp, atomSize);
                PV_MP4_FF_DELETE(NULL, ID3V2Atom, _pid3v2Atom);
                _pid3v2Atom = NULL;
                _count -= atomSize;
            }
            else
            {
                _count -= _pid3v2Atom->getSize();
            }
        }
        // Read the ilst Atom
        else if (atomType == ITUNES_ILST_ATOM)
        {
            PV_MP4_FF_NEW(fp->auditCB, ITunesILSTAtom, (fp, atomSize, atomType), _pITunesILSTAtom);

            if (!_pITunesILSTAtom->MP4Success())
            {
                AtomUtils::seekFromStart(fp, currPtr);
                AtomUtils::seekFromCurrPos(fp, atomSize);
                PV_MP4_FF_DELETE(NULL, ITunesILSTAtom, _pITunesILSTAtom);
                _pITunesILSTAtom = NULL;
                _count -= atomSize;
            }
            else
                _count -= _pITunesILSTAtom->getSize();
        }
    }
}

MetaDataAtom::~MetaDataAtom()
{
    if (_pHdlrAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, HandlerAtom, _pHdlrAtom);
    }
    if (_pITunesILSTAtom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, ITunesILSTAtom, _pITunesILSTAtom);
    }
    if (_pid3v2Atom != NULL)
    {
        PV_MP4_FF_DELETE(NULL, ID3V2Atom, _pid3v2Atom);
    }

}


ID3V2Atom::ID3V2Atom(MP4_FF_FILE *fp, uint32 size, uint32 type)
        : FullAtom(fp, size, type)
{

    if (_success)
    {
        if (!AtomUtils::read16(fp, _language))
        {
            _success = false;
            _mp4ErrorCode = READ_ID3V2_ATOM_FAILED;
            return;
        }
        else
        {
            PV_MP4_FF_NEW(fp->auditCB, PVID3ParCom, (), _pID3Parser);

            if (_pID3Parser)
            {
                if (_pID3Parser->ParseID3Tag(&fp->_pvfile) != PVMFSuccess)
                {
                    _success = false;
                    _mp4ErrorCode = READ_ID3V2_ATOM_FAILED;
                    return;
                }
            }
        }
    }
    else
    {
        if (_mp4ErrorCode != ATOM_VERSION_NOT_SUPPORTED)
            _mp4ErrorCode = READ_ID3V2_ATOM_FAILED;
    }

}


ID3V2Atom::~ID3V2Atom()
{
    if (_pID3Parser)
    {
        PV_MP4_FF_DELETE(null, PVID3ParCom, _pID3Parser);
        _pID3Parser = NULL;
    }
}


