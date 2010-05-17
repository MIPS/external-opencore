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
#ifndef PVMF_SM_FSP_REGISTRY_H
#include "pvmf_sm_fsp_registry.h"
#endif


//Verify that macros are indeed defined
#ifndef BUILD_STATIC_RTSPUNICAST
#error "Macro BUILD_RTSPUNICAST is supposed to be defined" 
#endif
#ifndef BUILD_STATIC_MSHTTP_ASF
#error "Macro BUILD_MSHTTP_ASF is supposed to be defined" 
#endif
#ifndef BUILD_STATIC_RTSPT
#error "Macro BUILD_STATIC_RTSPT is supposed to be defined" 
#endif
#ifndef BUILD_STATIC_RTSPT_REALMEDIA
#error "Macro BUILD_STATIC_RTSPT_REALMEDIA is supposed to be defined" 
#endif
#ifndef BUILD_STATIC_RTSP_UNICAST_PVR
#error "Macro BUILD_STATIC_RTSP_UNICAST_PVR is supposed to be defined" 
#endif
#ifndef BUILD_STATIC_RTSP_BROADCAST_PVR
#error "Macro BUILD_STATIC_RTSP_BROADCAST_PVR is supposed to be defined" 
#endif
#ifndef BUILD_STATIC_FILEPLAYBACK_PVR
#error "Macro BUILD_STATIC_FILEPLAYBACK_PVR is supposed to be defined" 
#endif

#if (BUILD_STATIC_RTSPUNICAST)
#ifndef PVMF_SM_RTSP_UNICAST_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_rtsp_unicast_node_factory.h"
#endif
#endif

#if (BUILD_STATIC_MSHTTP_ASF)
#ifndef PVMF_SM_MSHTTP_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_mshttp_node_factory.h"
#endif
#endif

#if (BUILD_STATIC_RTSPT)
#ifndef PVMF_SM_RTSPT_UNICAST_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_rtspt_unicast_node_factory.h"
#endif
#endif

#if (BUILD_STATIC_RTSPT_REALMEDIA)
#ifndef PVMF_SM_RTSPT_UNICAST_RM_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_rtspt_unicast_rm_node_factory.h"
#endif
#endif

#if (BUILD_STATIC_RTSP_UNICAST_PVR)
#ifndef PVMF_SM_FSP_RTSP_UNICAST_PLUS_PVR_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_fsp_rtsp_unicast_plus_pvr_node_factory.h"
#endif
#endif

#if (BUILD_STATIC_RTSP_BROADCAST_PVR)
#ifndef PVMF_SM_FSP_RTSP_BROADCAST_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_fsp_rtsp_broadcast_node_factory.h"
#endif
#endif

#if (BUILD_STATIC_FILEPLAYBACK_PVR)
#ifndef PVMF_SM_PVR_FILEPLAYBACK_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_fsp_pvr_fileplayback_node_factory.h"
#endif
#endif

#ifndef PVMF_SM_FSP_REGISTRY_POPULATOR_INTERFACE_H_INCLUDED
#include "pvmf_sm_fsp_registry_populator_interface.h"
#endif

#if USE_LOADABLE_MODULES
#include "oscl_shared_library.h"
#include "oscl_library_list.h"
#include "oscl_configfile_list.h"
#include "osclconfig_lib.h"
#include "oscl_shared_lib_interface.h"
#endif //USE_LOADABLE_MODULES

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif

#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif

PVMFSMFSPRegistry::PVMFSMFSPRegistry()
{
#if USE_LOADABLE_MODULES
    iMayLoadPluginsDynamically = true;
#else
    iMayLoadPluginsDynamically = false;
#endif
    PVMFSMFSPInfo fspInfo;
    iType.reserve(10);

#if (BUILD_STATIC_RTSPUNICAST)
    fspInfo.iSourceFormatTypes.clear();
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_RTSP_URL);
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_SDP_FILE);
    fspInfo.iSMFSPUUID = KPVMFSMRTSPUnicastNodeUuid;
    fspInfo.iSMFSPCreateFunc = PVMFSMRTSPUnicastNodeFactory::CreateSMRTSPUnicastNodeFactory;
    fspInfo.iSMFSPReleaseFunc = PVMFSMRTSPUnicastNodeFactory::DeleteSMRTSPUnicastNodeFactory;
    iType.push_back(fspInfo);
#endif

#if (BUILD_STATIC_MSHTTP_ASF)
    fspInfo.iSourceFormatTypes.clear();
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_MS_HTTP_STREAMING_URL);
    fspInfo.iSMFSPUUID = KPVMFSMMSHTTPNodeUuid;
    fspInfo.iSMFSPCreateFunc = PVMFSMMSHTTPNodeFactory::CreateSMMSHTTPNodeFactory;
    fspInfo.iSMFSPReleaseFunc = PVMFSMMSHTTPNodeFactory::DeleteSMMSHTTPNodeFactory;
    iType.push_back(fspInfo);
#endif

#if (BUILD_STATIC_RTSP_UNICAST_PVR)
    fspInfo.iSourceFormatTypes.clear();
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_SDP_PVR_FCS_FILE);
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_RTSP_PVR_FCS_URL);
    fspInfo.iSMFSPUUID = KPVMFSMPVRRTSPUnicastPlusPVRNodeUuid;
    fspInfo.iSMFSPCreateFunc = PVMFSMRTSPUnicastPlusPVRNodeFactory::CreatePVMFSMRTSPUnicastPlusPVRNode;
    fspInfo.iSMFSPReleaseFunc = PVMFSMRTSPUnicastPlusPVRNodeFactory::DeletePVMFSMRTSPUnicastPlusPVRNode;
    iType.push_back(fspInfo);
#endif

#if (BUILD_STATIC_FILEPLAYBACK_PVR)
    fspInfo.iSourceFormatTypes.clear();
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_PVRFF);
    fspInfo.iSMFSPUUID = KPVMFSMPVRFilePlaybackNodeUuid;
    fspInfo.iSMFSPCreateFunc = PVMFSMPVRFilePlaybackNodeFactory::CreateSMPVRFilePlaybackNodeFactory;
    fspInfo.iSMFSPReleaseFunc = PVMFSMPVRFilePlaybackNodeFactory::DeleteSMPVRFilePlaybackNodeFactory;
    iType.push_back(fspInfo);
#endif

#if (BUILD_STATIC_RTSP_BROADCAST_PVR)
    fspInfo.iSourceFormatTypes.clear();
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_SDP_BROADCAST);
    fspInfo.iSMFSPUUID = KPVMFSMRTSPBroadcastNodeUuid;
    fspInfo.iSMFSPCreateFunc = PVMFSMRTSPBroadcastNodeFactory::CreateSMRTSPBroadcastNode;
    fspInfo.iSMFSPReleaseFunc = PVMFSMRTSPBroadcastNodeFactory::DeleteSMRTSPBroadcastNode;
    iType.push_back(fspInfo);
#endif

#if (BUILD_STATIC_RTSPT)
    fspInfo.iSourceFormatTypes.clear();
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_RTSP_TUNNELLING);
    fspInfo.iSMFSPUUID = KPVMFSMRTSPTUnicastNodeUuid;
    fspInfo.iSMFSPCreateFunc = PVMFSMRTSPTUnicastNodeFactory::CreateSMRTSPTUnicastNode;
    fspInfo.iSMFSPReleaseFunc = PVMFSMRTSPTUnicastNodeFactory::DeleteSMRTSPTUnicastNode;
    iType.push_back(fspInfo);
#endif

#if (BUILD_STATIC_RTSPT_REALMEDIA)
    fspInfo.iSourceFormatTypes.clear();
    fspInfo.iSourceFormatTypes.push_back(PVMF_MIME_DATA_SOURCE_REAL_HTTP_CLOAKING_URL);
    fspInfo.iSMFSPUUID = KPVMFSMRTSPTForUnicastRMNodeUuid;
    fspInfo.iSMFSPCreateFunc = PVMFSMRTSPTUnicastForRMNodeFactory::CreateSMRTSPTUnicastForRMNode;
    fspInfo.iSMFSPReleaseFunc = PVMFSMRTSPTUnicastForRMNodeFactory::DeleteSMRTSPTUnicastForRMNode;
    iType.push_back(fspInfo);
#endif
}

PVMFSMFSPRegistry::~PVMFSMFSPRegistry()
{
    RemoveLoadableModules();
    iType.clear();
}

PVMFStatus PVMFSMFSPRegistry::QueryRegistry(PVMFFormatType& aInputType, Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids)
{
    PVMFStatus status = PVMFFailure;
    if (CheckPluginAvailability(aInputType, aUuids))
    {
        status = PVMFSuccess;
    }
    else
    {
        if (iMayLoadPluginsDynamically)
        {
            const uint32 typeVectSz = iType.size();
            AddLoadableModules();
            if (CheckPluginAvailability(aInputType, aUuids, typeVectSz))
            {
                status = PVMFSuccess;
            }
        }
    }
    return status;
}

PVMFSMFSPBaseNode* PVMFSMFSPRegistry::CreateSMFSP(PVUuid& aUuid)
{
    bool iFoundFlag = false;
    uint32 FSPSearchCount = 0;

    while (FSPSearchCount < iType.size())
    {
        //Search if the UUID's will match
        if (iType[FSPSearchCount].iSMFSPUUID == aUuid)
        {
            //Since the UUID's match set the flag to true
            iFoundFlag = true;
            break;
        }

        FSPSearchCount++;

    }

    if (iFoundFlag)
    {
        //Call the appropriate Node creation function & return Node pointer
        return (*(iType[FSPSearchCount].iSMFSPCreateFunc))(OsclActiveObject::EPriorityNominal);
    }
    else
    {
        return NULL;
    }
}

bool PVMFSMFSPRegistry::ReleaseSMFSP(PVUuid& aUuid, PVMFSMFSPBaseNode *aSMFSP)
{
    bool iFoundFlag = false;
    uint32 FSPSearchCount = 0;

    while (FSPSearchCount < iType.size())
    {
        //Search if the UUID's will match
        if (iType[FSPSearchCount].iSMFSPUUID == aUuid)
        {
            //Since the UUID's match set the flag to true
            iFoundFlag = true;
            break;
        }

        FSPSearchCount++;

    }

    if (iFoundFlag)
    {
        //Call the appropriate Node creation function
        bool del_stat = (*(iType[FSPSearchCount].iSMFSPReleaseFunc))(aSMFSP);
        return del_stat;
    }
    return false;
}

bool PVMFSMFSPRegistry::CheckPluginAvailability(PVMFFormatType& aInputType, Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids, uint32 aIndex)
{
    uint32 searchIndex = aIndex;
    bool matchfound = false;

    // Find all nodes that support the specified src format
    while (searchIndex < iType.size())
    {
        uint32 inputsearchcount = 0;
        while (inputsearchcount < iType[searchIndex].iSourceFormatTypes.size())
        {
            // Check if the input format matches
            if (iType[searchIndex].iSourceFormatTypes[inputsearchcount] == aInputType)
            {
                // Set the the input flag to true since we found the match in the search
                matchfound = true;
                break;
            }
            inputsearchcount++;
        }
        if (matchfound)
            break;
        searchIndex++;
    }

    if (matchfound)
    {
        aUuids.push_back(iType[searchIndex].iSMFSPUUID);
    }
    return matchfound;
}

void PVMFSMFSPRegistry::AddLoadableModules()
{

#if USE_LOADABLE_MODULES
    OsclConfigFileList aCfgList;
    // collects all config files from the project specified directory
    if (NULL != PV_DYNAMIC_LOADING_CONFIG_FILE_PATH)
    {
        OSCL_HeapString<OsclMemAllocator> configFilePath = PV_DYNAMIC_LOADING_CONFIG_FILE_PATH;
        aCfgList.Populate(configFilePath);
    }
    // populate libraries from all config files
    for (uint k = 0; k < aCfgList.Size(); k++)
    {
        OsclLibraryList libList;
        libList.Populate(PVMF_SM_FSP_REGISTRY_POPULATOR_INTERFACE, aCfgList.GetConfigfileAt(k));

        for (uint32 i = 0; i < libList.Size(); i++)
        {
            OsclSharedLibrary* lib = OSCL_NEW(OsclSharedLibrary, ());
            if (lib->LoadLib(libList.GetLibraryPathAt(i)) == OsclLibSuccess)
            {
                OsclAny* interfacePtr = NULL;
                OsclLibStatus result = lib->QueryInterface(PVMF_SM_FSP_REGISTRY_POPULATOR_INTERFACE, (OsclAny*&)interfacePtr);
                if (result == OsclLibSuccess && interfacePtr != NULL)
                {
                    struct PVSMFSPSharedLibInfo *libInfo =
                        OSCL_STATIC_CAST(PVSMFSPSharedLibInfo*, oscl_malloc(sizeof(struct PVSMFSPSharedLibInfo)));
                    if (NULL != libInfo)
                    {
                        libInfo->iLib = lib;
                        PVMFSMFSPRegistryPopulatorInterface* fspLibIntPtr = OSCL_DYNAMIC_CAST(PVMFSMFSPRegistryPopulatorInterface*, interfacePtr);
                        libInfo->iFSPLibIfacePtr = fspLibIntPtr;
                        fspLibIntPtr->Register(this);

                        // save for depopulation later
                        iFSPLibInfoList.push_front(libInfo);
                        continue;
                    }
                }
            }
            lib->Close();
            OSCL_DELETE(lib);
        }
    }
    iMayLoadPluginsDynamically = false;
#endif
}


void PVMFSMFSPRegistry::RemoveLoadableModules()
{

#if USE_LOADABLE_MODULES
    // remove all dynamic nodes now
    // unregister node one by one
    while (!iFSPLibInfoList.empty())
    {
        struct PVSMFSPSharedLibInfo *libInfo = iFSPLibInfoList.front();
        iFSPLibInfoList.erase(iFSPLibInfoList.begin());

        OsclSharedLibrary* lib = libInfo->iLib;
        PVMFSMFSPRegistryPopulatorInterface* nodeIntPtr = libInfo->iFSPLibIfacePtr;
        oscl_free(libInfo);

        nodeIntPtr->Unregister(this);

        lib->Close();
        OSCL_DELETE(lib);
    }
    iMayLoadPluginsDynamically = true;
#endif

}
