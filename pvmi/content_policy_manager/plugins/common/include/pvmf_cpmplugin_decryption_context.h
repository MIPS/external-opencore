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
#ifndef PVMF_CPMPLUGIN_DECRYPTION_CONTEXT_H_INCLUDED
#define PVMF_CPMPLUGIN_DECRYPTION_CONTEXT_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_TYPES_H_INCLUDED
#include "oscl_types.h"
#endif
#ifndef PV_UUID_H_INCLUDED
#include "pv_uuid.h"
#endif
#ifndef PV_INTERFACE_H_INCLUDED
#include "pv_interface.h"
#endif

#define PVMFCPMPluginWMDRMDecryptContextUuid PVUuid(0x19c4fdb8,0x2165,0x4f10,0xa5,0x53,0x72,0x76,0xd7,0xc6,0xb1,0x2a)

/**
 * This class is used to pass decryption context data to CPM Plugins
 */
class PVMFCPMPluginWMDRMDecryptContext : public PVInterface
{
    public:
        PVMFCPMPluginWMDRMDecryptContext(): ipKID(NULL)
                , ipInitializationVector(NULL)
                , iInitializationVectorSize(0)
        {
            iRef = 0;
            Oscl_Int64_Utils::set_uint64(iSampleID, 0, 0);
            iMediaObjectStartOffset = 0;
        }

        PVMFCPMPluginWMDRMDecryptContext(const uint8* apKID, const uint8* apInitializationVector,
                                         uint32 aInitializationVectorSize)
                : iMediaObjectStartOffset(0)
                , iRef(0)
                , ipKID(apKID), ipInitializationVector(apInitializationVector)
                , iInitializationVectorSize(aInitializationVectorSize)
        {
            Oscl_Int64_Utils::set_uint64(iSampleID, 0, 0);
        }

        virtual ~PVMFCPMPluginWMDRMDecryptContext()
        {
        };

        void addRef()
        {
            iRef++;
        };

        void removeRef()
        {
            iRef--;
        };

        bool queryInterface(const PVUuid& uuid,
                            PVInterface*& iface)
        {
            iface = NULL;
            if (uuid == PVMFCPMPluginWMDRMDecryptContextUuid)
            {
                iface = OSCL_STATIC_CAST(PVInterface*, this);
                return true;
            }
            return false;
        };

        const uint8* GetKID() const
        {
            return ipKID;
        }

        const uint8* GetInitializationVector() const
        {
            return  ipInitializationVector;
        }

        uint32 GetInitializationVectorSize()
        {
            return  iInitializationVectorSize;
        }
        uint64 iSampleID;
        uint32 iMediaObjectStartOffset;
    private:
        uint32 iRef;
        const uint8* ipKID;
        const uint8* ipInitializationVector;
        uint32 iInitializationVectorSize;
};

#endif //PVMF_CPMPLUGIN_DECRYPTION_CONTEXT_H_INCLUDED

