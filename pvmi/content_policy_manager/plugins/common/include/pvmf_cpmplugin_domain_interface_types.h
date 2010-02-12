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
#ifndef PVMF_CPMPLUGIN_DOMAIN_INTERFACE_TYPES_H_INCLUDED
#define PVMF_CPMPLUGIN_DOMAIN_INTERFACE_TYPES_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_TYPES_H_INCLUDED
#include "oscl_types.h"
#endif
#ifndef OSCL_VECTOR_H_INCLUDED
#include "oscl_vector.h"
#endif
#ifndef OSCL_STRING_H_INCLUDED
#include "oscl_string.h"
#endif
#ifndef PV_UUID_H_INCLUDED
#include "pv_uuid.h"
#endif

class PVMFCPMDomainId
{
    public:
        PVMFCPMDomainId()
                : iServiceId(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
                , iAccountId(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
                , iRevision(0)
        {}
        PVMFCPMDomainId(const PVMFCPMDomainId& aVal)
        {
            iServiceId = aVal.iServiceId;
            iAccountId = aVal.iAccountId;
            iRevision = aVal.iRevision;
        }
        PVMFCPMDomainId & operator=(const PVMFCPMDomainId& aSrc)
        {
            if (&aSrc != this)
            {
                iServiceId = aSrc.iServiceId;
                iAccountId = aSrc.iAccountId;
                iRevision = aSrc.iRevision;
            }
            return *this;
        };

        PVUuid iServiceId;
        PVUuid iAccountId;
        uint32 iRevision;
};

//Data associated with a Domain Join request
class PVMFCPMDomainJoinData
{
    public:
        PVMFCPMDomainJoinData()
                : iFlags(0)
        {}

        PVMFCPMDomainJoinData & operator=(const PVMFCPMDomainJoinData& aSrc)
        {
            if (&aSrc != this)
            {
                iDomainUrl     = aSrc.iDomainUrl;
                iFlags         = aSrc.iFlags;
                iDomainId      = aSrc.iDomainId;
                iFriendlyName  = aSrc.iFriendlyName;
                iCustomData    = aSrc.iCustomData;
            }
            return *this;
        };

        OSCL_HeapString<OsclMemAllocator> iDomainUrl;    //the domain server URL

        uint32 iFlags;              //Flag that indicates the type of custom data.

        PVMFCPMDomainId iDomainId;  //Domain ID to be registered with the server.

        OSCL_HeapString<OsclMemAllocator> iFriendlyName; //Friendly name.

        //Custom data to be sent to the server.
        //The format of the custom data is based on the value of iFlags. It may be blank.
        OSCL_HeapString<OsclMemAllocator> iCustomData;
};

//Data associated with a Domain Leave request
class PVMFCPMDomainLeaveData
{
    public:
        PVMFCPMDomainLeaveData()
                : iFlags(0)
        {}

        OSCL_HeapString<OsclMemAllocator> iDomainUrl;    //Domain URL.

        uint32 iFlags;              //Flag that indicates the type of custom data.

        PVMFCPMDomainId iDomainId;  //Domain ID to be unregistered with the server.

        //Custom data to be sent to the server.
        //The format of the custom data is based on the value of iFlags. It may be blank.
        OSCL_HeapString<OsclMemAllocator> iCustomData;
};

//Data output by GetDomain request.
class PVMFCPMDomainCertData
{
    public:
        PVMFCPMDomainId iDomainId;//Domain ID

        OSCL_HeapString<OsclMemAllocator> iUrl;//a URL in the domain cert.  Not used currently.

        PVMFCPMDomainCertData & operator=(const PVMFCPMDomainCertData& aSrc)
        {
            if (&aSrc != this)
            {
                iDomainId = aSrc.iDomainId;
                iUrl = aSrc.iUrl;
            }
            return *this;
        }
};


#endif //PVMF_CPMPLUGIN_DOMAIN_INTERFACE_TYPES_H_INCLUDED

