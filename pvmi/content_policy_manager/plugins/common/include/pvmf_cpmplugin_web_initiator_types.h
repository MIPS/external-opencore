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
#ifndef PVMF_CPMPLUGIN_WEB_INITIATOR_TYPES_H_INCLUDED
#define PVMF_CPMPLUGIN_WEB_INITIATOR_TYPES_H_INCLUDED

#include "oscl_base.h"
#include "oscl_string_containers.h"
#include "oscl_mem.h"

//Web-initiator types

/* This class holds data parsed from a lic-acquire web initiator. This data can be
** used for license acquisition.
*/
class PVMFCPMAcquireLicenseWebInitiatorData
{
    public:
        PVMFCPMAcquireLicenseWebInitiatorData():
                iIsWMConvertAvailable(false)
        {}

        //Content header
        OSCL_wHeapString<OsclMemAllocator>  iHeader;

        //Custom data to be sent to the server.
        OSCL_HeapString<OsclMemAllocator> iCustomData;

        //URL from where the content can be downloaded
        OSCL_HeapString<OsclMemAllocator> iStrContentURL;

        // Point to the content of <WMConvert> node.
        OSCL_HeapString<OsclMemAllocator> iStrWMConvert;

        // Flag inidicating whether the <WMConvert> node is available.
        bool iIsWMConvertAvailable;

        // <LA_URL> node in the content header.
        OSCL_wHeapString<OsclMemAllocator> iStrLAURL;

        // <LUI_URL> node (if exists) in the content header.
        OSCL_wHeapString<OsclMemAllocator> iStrLUIURL;

        // <DS_ID> node (if exists) in the content header.
        OSCL_wHeapString<OsclMemAllocator> iStrDSID;
};

#endif //PVMF_CPMPLUGIN_LICENSE_ACQUIRE_INITIATOR_TYPES_H_INCLUDED
