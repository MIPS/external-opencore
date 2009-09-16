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
#ifndef OSCL_SHARED_LIBRARY_H_INCLUDED
#include "oscl_shared_library.h"
#endif
#ifndef PVMF_SM_FSP_SHARED_LIB_INTERFACE_H_INCLUDED
#include "pvmf_sm_fsp_shared_lib_interface.h"
#endif
#ifndef PVMF_SM_RTSPT_UNICAST_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_rtspt_unicast_node_factory.h"
#endif

class RTSPTManagerPluginInterface: public OsclSharedLibraryInterface,
        public SMNodeFSPSharedLibraryInterface
{
    public:
        OsclAny* QueryLibInterface(const OsclUuid& aInterfaceId)
        {
            if (SMNODE_CREATE_FSP_LIB_INTERFACE == aInterfaceId)
            {
                return (OsclAny*)(PVMFSMRTSPTUnicastNodeFactory::CreateSMRTSPTUnicastNode);
            }
            else if (SMNODE_RELEASE_FSP_LIB_INTERFACE == aInterfaceId)
            {
                return (OsclAny*)(PVMFSMRTSPTUnicastNodeFactory::DeleteSMRTSPTUnicastNode);
            }

            return NULL;
        };

        // From OsclSharedLibraryInterface
        OsclAny* SharedLibraryLookup(const OsclUuid& aInterfaceId)
        {
            if (aInterfaceId == SMNODE_FSP_SHARED_LIBRARY_INTERFACE)
            {
                return OSCL_STATIC_CAST(SMNodeFSPSharedLibraryInterface*, this);
            }
            return NULL;
        };
};


extern "C"
{
    OSCL_EXPORT_REF OsclSharedLibraryInterface* PVGetInterface(void)
    {
        return OSCL_NEW(RTSPTManagerPluginInterface, ());
    }
    OSCL_EXPORT_REF void PVReleaseInterface(OsclSharedLibraryInterface* aInstance)
    {
        OSCL_DELETE(aInstance);
    }
}
