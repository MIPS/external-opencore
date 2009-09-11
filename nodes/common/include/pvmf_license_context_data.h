/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
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
#ifndef PVMF_LICENSE_CONTEXT_DATA_H_INCLUDED
#define PVMF_LICENSE_CONTEXT_DATA_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_STRING_H_INCLUDED
#include "oscl_string.h"
#endif
#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif
#ifndef PV_UUID_H_INCLUDED
#include "pv_uuid.h"
#endif
#ifndef PV_INTERFACE_H_INCLUDED
#include "pv_interface.h"
#endif

#define PVMF_LICENSE_CONTEXT_DATA_UUID PVUuid(0x13f2d930,0x8f58,0x11de,0x8a,0x39,0x08,0x00,0x20,0x0c,0x9a,0x66)
#define PVMF_DOMAIN_LICENSE_CONTEXT_DATA_UUID PVUuid(0x574a8890,0x8f58,0x11de,0x8a,0x39,0x08,0x00,0x20,0x0c,0x9a,0x66)

class PVMFDomainLicenseDataSource : public PVInterface
{
    public:
        PVMFDomainLicenseDataSource()
        {
            iRefCounter = 0;
        };

        PVMFDomainLicenseDataSource(const PVMFDomainLicenseDataSource& aSrc) : PVInterface(aSrc)
        {
            iRefCounter = 0;
            MyCopy(aSrc);
        };

        PVMFDomainLicenseDataSource& operator=(const PVMFDomainLicenseDataSource& aSrc)
        {
            if (&aSrc != this)
            {
                MyCopy(aSrc);
            }
            return *this;
        };

        /* From PVInterface */
        void addRef()
        {
            iRefCounter++;
        }
        void removeRef()
        {
            iRefCounter--;
        }
        bool queryInterface(const PVUuid& uuid, PVInterface*& iface)
        {
            if (uuid == PVUuid(PVMF_DOMAIN_LICENSE_CONTEXT_DATA_UUID))
            {
                iface = this;
                return true;
            }
            else
            {
                iface = NULL;
                return false;
            }
        }
        void setDomainServiceId(uint8 b0, uint8 b1, uint8 b2, uint8 b3
                                , uint8 b4, uint8 b5, uint8 b6, uint8 b7, uint8 b8
                                , uint8 b9, uint8 b10, uint8 b11, uint8 b12
                                , uint8 b13, uint8 b14, uint8 b15)
        {
            uint8* pos = (uint8*) & iServiceId;
            *pos++ = b0;
            *pos++ = b1;
            *pos++ = b2;
            *pos++ = b3;
            *pos++ = b4;
            *pos++ = b5;
            *pos++ = b6;
            *pos++ = b7;
            *pos++ = b8;
            *pos++ = b9;
            *pos++ = b10;
            *pos++ = b11;
            *pos++ = b12;
            *pos++ = b13;
            *pos++ = b14;
            *pos++ = b15;
        }

        PVUuid iServiceId;
    private:
        void MyCopy(const PVMFDomainLicenseDataSource& aSrc)
        {
            iServiceId = aSrc.iServiceId;
        };

        int32 iRefCounter;
};

class PVMFLicenseContextData : public PVInterface
{
    public:
        PVMFLicenseContextData()
        {
            iRefCounter = 0;
            iDomainLicenseContextValid = false;
        };

        PVMFLicenseContextData(const PVMFLicenseContextData& aSrc) : PVInterface(aSrc)
        {
            iRefCounter = 0;
            MyCopy(aSrc);
        };

        PVMFLicenseContextData& operator=(const PVMFLicenseContextData& aSrc)
        {
            if (&aSrc != this)
            {
                MyCopy(aSrc);
            }
            return *this;
        };

        /* From PVInterface */
        void addRef()
        {
            iRefCounter++;
        }
        void removeRef()
        {
            iRefCounter--;
        }
        bool queryInterface(const PVUuid& uuid, PVInterface*& iface)
        {
            if (uuid == PVUuid(PVMF_LICENSE_CONTEXT_DATA_UUID))
            {
                iface = this;
                return true;
            }
            else if (uuid == PVUuid(PVMF_DOMAIN_LICENSE_CONTEXT_DATA_UUID))
            {
                if (iDomainLicenseContextValid == true)
                {
                    iface = &iPVMFDomainLicenseDataSource;
                    return true;
                }
            }
            iface = NULL;
            return false;
        }

        void EnableDomainLicenseContext()
        {
            iDomainLicenseContextValid = true;
        }
        void DisableDomainLicenseContext()
        {
            iDomainLicenseContextValid = false;
        }
        PVMFDomainLicenseDataSource* DomainLicense()
        {
            return iDomainLicenseContextValid ? &iPVMFDomainLicenseDataSource : NULL;
        }
    private:
        int32 iRefCounter;
        bool iDomainLicenseContextValid;

        PVMFDomainLicenseDataSource iPVMFDomainLicenseDataSource;

        void MyCopy(const PVMFLicenseContextData& aSrc)
        {
            iDomainLicenseContextValid = aSrc.iDomainLicenseContextValid;

            iPVMFDomainLicenseDataSource = aSrc.iPVMFDomainLicenseDataSource;
        };
};

#endif //PVMF_LICENSE_CONTEXT_DATA_H_INCLUDED

