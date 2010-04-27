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
#ifndef OSCL_SHARED_LIBRARY_H_INCLUDED
#include "oscl_shared_library.h"
#endif

#ifndef PVMF_SM_FSP_REGISTRY_POPULATOR_INTERFACE_H_INCLUDED
#include "pvmf_sm_fsp_registry_populator_interface.h"
#endif

#ifndef PVMF_SM_FSP_REGISTRY_INTERFACE_H_INCLUDED
#include "pvmf_sm_fsp_registry_interface.h"
#endif

#ifndef PVMF_SM_RTSP_UNICAST_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_rtsp_unicast_node_factory.h"
#endif

#ifndef PVMF_SM_FSP_SHARED_LIB_INTERFACE_H_INCLUDED
#include "pvmf_sm_fsp_shared_lib_interface.h"
#endif

#ifndef PVMF_SM_FSP_BASE_IMPL_H
#include "pvmf_sm_fsp_base_impl.h"
#endif

#ifdef SM_FSP_LIB_NAME
#error "Multiple definition of this macro is not intended"
#endif

#define SM_FSP_LIB_NAME "libpvrtspunicast_streaming"

#define LIB_NAME_MAX_LENGTH 64

class PVMFSMPluginLibManager
{
    public:
        static PVMFSMFSPBaseNode* LoadLibAndCreatePlugin(int32 aPriority);
        static bool DeletePluginAndUnloadLib(PVMFSMFSPBaseNode* aFSP);
};

typedef PVMFSMFSPBaseNode*(* LPFN_SM_FSP_CREATE_FUNC)(int32);
typedef bool (* LPFN_SM_FSP_RELEASE_FUNC)(PVMFSMFSPBaseNode *);

PVMFSMFSPBaseNode* PVMFSMPluginLibManager::LoadLibAndCreatePlugin(int32 aPriority)
{
    OsclSharedLibrary* aSharedLibrary = NULL;
    OSCL_StackString<LIB_NAME_MAX_LENGTH> libname(SM_FSP_LIB_NAME);

    // Need to load the library for the node
    aSharedLibrary = OSCL_NEW(OsclSharedLibrary, ());

    OsclLibStatus result = aSharedLibrary->LoadLib(libname);
    if (OsclLibSuccess != result) return NULL;
    aSharedLibrary->AddRef();

    // Query for create function
    OsclAny* interfacePtr = NULL;
    aSharedLibrary->QueryInterface(SMNODE_FSP_SHARED_LIBRARY_INTERFACE, (OsclAny*&)interfacePtr);
    if (NULL == interfacePtr) return NULL;

    SMNodeFSPSharedLibraryInterface* libIntPtr = OSCL_DYNAMIC_CAST(SMNodeFSPSharedLibraryInterface*, interfacePtr);

    OsclAny* createFuncTemp = libIntPtr->QueryLibInterface(SMNODE_CREATE_FSP_LIB_INTERFACE);
    if (!createFuncTemp) return NULL;

    LPFN_SM_FSP_CREATE_FUNC libCreateFunc = OSCL_DYNAMIC_CAST(PVMFSMFSPBaseNode * (*)(int32), createFuncTemp);

    if (NULL != libCreateFunc)
    {
        // call the real node factory function
        PVMFSMFSPBaseNode* plugin = (*(libCreateFunc))(aPriority);
        if (NULL != plugin)
        {
            plugin->SetSharedLibraryPtr(aSharedLibrary);
            return plugin;
        }
    }

    aSharedLibrary->RemoveRef();
    if (OsclLibSuccess == aSharedLibrary->Close())
    {
        // Close will unload the library if refcount is 0
        OSCL_DELETE(aSharedLibrary);
    }
    return NULL;

}

bool PVMFSMPluginLibManager::DeletePluginAndUnloadLib(PVMFSMFSPBaseNode* aFSP)
{
    if (NULL == aFSP) return false;

    // Retrieve shared library pointer
    OsclSharedLibrary* aSharedLibrary = aFSP->GetSharedLibraryPtr();

    bool bStatus = false;
    if (NULL != aSharedLibrary)
    {
        // Query for release function
        OsclAny* interfacePtr = NULL;
        aSharedLibrary->QueryInterface(SMNODE_FSP_SHARED_LIBRARY_INTERFACE, (OsclAny*&)interfacePtr);

        SMNodeFSPSharedLibraryInterface* libIntPtr = OSCL_DYNAMIC_CAST(SMNodeFSPSharedLibraryInterface*, interfacePtr);

        OsclAny* releaseFuncTemp = libIntPtr->QueryLibInterface(SMNODE_RELEASE_FSP_LIB_INTERFACE);
        if (!releaseFuncTemp) return false;

        LPFN_SM_FSP_RELEASE_FUNC libReleaseFunc = OSCL_DYNAMIC_CAST(bool (*)(PVMFSMFSPBaseNode*), releaseFuncTemp);

        if (NULL != libReleaseFunc)
        {
            bStatus = (*(libReleaseFunc))(aFSP);
        }

        aSharedLibrary->RemoveRef();

        if (OsclLibSuccess == aSharedLibrary->Close())
        {
            // Close will unload the library if refcount is 0
            OSCL_DELETE(aSharedLibrary);
        }
    }

    return bStatus;
}

class SMNodeRTSPUnicastPluginRegistryPopulatorInterface: public OsclSharedLibraryInterface,
        public PVMFSMFSPRegistryPopulatorInterface
{
    public:
        // From OsclSharedLibraryInterface
        OsclAny* SharedLibraryLookup(const OsclUuid& aInterfaceId)
        {
            if (aInterfaceId == PVMF_SM_FSP_REGISTRY_POPULATOR_INTERFACE)
            {
                return OSCL_STATIC_CAST(PVMFSMFSPRegistryPopulatorInterface*, this);
            }
            return NULL;
        };

        void Register(PVMFFSPRegistryInterface* aRegistry)
        {
            PVMFSMFSPInfo fspInfo;
            fspInfo.iSourceFormatTypes.clear();
            fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_RTSP_URL);
            fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_SDP_FILE);
            fspInfo.iSMFSPUUID = KPVMFSMRTSPUnicastNodeUuid;
            fspInfo.iSMFSPCreateFunc = PVMFSMPluginLibManager::LoadLibAndCreatePlugin;
            fspInfo.iSMFSPReleaseFunc = PVMFSMPluginLibManager::DeletePluginAndUnloadLib;
            if (aRegistry) aRegistry->RegisterSMFSP(fspInfo);
        }

        void Unregister(PVMFFSPRegistryInterface* aRegistry)
        {
            OSCL_UNUSED_ARG(aRegistry);
        }
};

extern "C"
{
    OSCL_EXPORT_REF OsclSharedLibraryInterface* PVGetInterface(void)
    {
        return OSCL_NEW(SMNodeRTSPUnicastPluginRegistryPopulatorInterface, ());
    }

    OSCL_EXPORT_REF void PVReleaseInterface(OsclSharedLibraryInterface* aInstance)
    {
        OSCL_DELETE(aInstance);
    }
}
