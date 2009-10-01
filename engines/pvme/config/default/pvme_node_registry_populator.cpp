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
#include "pvme_node_registry.h"

#if BUILD_MP4_FF_PARSER_NODE
#include "pvmf_mp4ffparser_factory.h"
#endif
#if BUILD_AMR_FF_PARSER_NODE
#include "pvmf_amrffparser_factory.h"
#endif
#if BUILD_AAC_FF_PARSER_NODE
#include "pvmf_aacffparser_factory.h"
#endif
#if BUILD_MP3_FF_PARSER_NODE
#include "pvmf_mp3ffparser_factory.h"
#endif
#if BUILD_WAV_FF_PARSER_NODE
#include "pvmf_wavffparser_factory.h"
#endif
#if BUILD_ASF_FF_PARSER_NODE
#include "pvmf_asfffparser_factory.h"
#endif
#include "pv_player_node_registry.h"
#if BUILD_RM_FF_PARSER_NODE
#include "pvmf_rmffparser_factory.h"
#endif
// For recognizer registry
#include "pvmf_recognizer_registry.h"

#include "pvmi_datastreamsyncinterface_ref_factory.h"

#include "pvmf_recognizer_plugin.h"
#if BUILD_ASF_FF_REC
#include "pvasfffrec_factory.h"
#endif
#if BUILD_MP3_FF_REC
#include "pvmp3ffrec_factory.h"
#endif
#if BUILD_MP4_FF_REC
#include "pvmp4ffrec_factory.h"
#endif
#if BUILD_RM_FF_REC
#include "pvrmffrec_factory.h"
#endif
#if BUILD_AAC_FF_REC
#include "pvaacffrec_factory.h"
#endif
#if BUILD_OMA1_FF_REC
#include "pvoma1ffrec_factory.h"
#endif
#if BUILD_WAV_FF_REC
#include "pvwavffrec_factory.h"
#endif
#if BUILD_AMR_FF_REC
#include "pvamrffrec_factory.h"
#endif



void PVMERegistryPopulator::RegisterAllNodes(PVPlayerNodeRegistryInterface* aRegistry, OsclAny*& aContext)
{
    OSCL_UNUSED_ARG(aContext);

    PVPlayerNodeInfo nodeinfo;

#if BUILD_MP4_FF_PARSER_NODE
    //For PVMFMP4FFParserNode
    nodeinfo.iInputTypes.clear();
    nodeinfo.iInputTypes.push_back(PVMFFormatType(PVMF_MIME_MPEG4FF));
    nodeinfo.iNodeUUID = KPVMFMP4FFParserNodeUuid;
    nodeinfo.iOutputType.clear();
    nodeinfo.iOutputType.push_back(PVMFFormatType(PVMF_MIME_FORMAT_UNKNOWN));
    nodeinfo.iNodeCreateFunc = PVMFMP4FFParserNodeFactory::CreatePVMFMP4FFParserNode;
    nodeinfo.iNodeReleaseFunc = PVMFMP4FFParserNodeFactory::DeletePVMFMP4FFParserNode;
    aRegistry->RegisterNode(nodeinfo);
#endif
#if BUILD_AMR_FF_PARSER_NODE
    //For PVMFAMRFFParserNode
    nodeinfo.iInputTypes.clear();
    nodeinfo.iInputTypes.push_back(PVMFFormatType(PVMF_MIME_AMRFF));
    nodeinfo.iNodeUUID = KPVMFAmrFFParserNodeUuid;
    nodeinfo.iOutputType.clear();
    nodeinfo.iOutputType.push_back(PVMFFormatType(PVMF_MIME_FORMAT_UNKNOWN));
    nodeinfo.iNodeCreateFunc = PVMFAMRFFParserNodeFactory::CreatePVMFAMRFFParserNode;
    nodeinfo.iNodeReleaseFunc = PVMFAMRFFParserNodeFactory::DeletePVMFAMRFFParserNode;
    aRegistry->RegisterNode(nodeinfo);
#endif
#if BUILD_AAC_FF_PARSER_NODE
    //For PVMFAACFFParserNode
    nodeinfo.iInputTypes.clear();
    nodeinfo.iInputTypes.push_back(PVMFFormatType(PVMF_MIME_AACFF));
    nodeinfo.iInputTypes.push_back(PVMFFormatType(PVMF_MIME_RAWAAC));
    nodeinfo.iNodeUUID = KPVMFAacFFParserNodeUuid;
    nodeinfo.iOutputType.clear();
    nodeinfo.iOutputType.push_back(PVMFFormatType(PVMF_MIME_FORMAT_UNKNOWN));
    nodeinfo.iNodeCreateFunc = PVMFAACFFParserNodeFactory::CreatePVMFAACFFParserNode;
    nodeinfo.iNodeReleaseFunc = PVMFAACFFParserNodeFactory::DeletePVMFAACFFParserNode;
    aRegistry->RegisterNode(nodeinfo);
#endif
#if BUILD_MP3_FF_PARSER_NODE
    //For PVMFMP3FFParserNode
    nodeinfo.iInputTypes.clear();
    nodeinfo.iInputTypes.push_back(PVMFFormatType(PVMF_MIME_MP3FF));
    nodeinfo.iNodeUUID = KPVMFMP3FFParserNodeUuid;
    nodeinfo.iOutputType.clear();
    nodeinfo.iOutputType.push_back(PVMFFormatType(PVMF_MIME_FORMAT_UNKNOWN));
    nodeinfo.iNodeCreateFunc = PVMFMP3FFParserNodeFactory::CreatePVMFMP3FFParserNode;
    nodeinfo.iNodeReleaseFunc = PVMFMP3FFParserNodeFactory::DeletePVMFMP3FFParserNode;
    aRegistry->RegisterNode(nodeinfo);
#endif
#if BUILD_WAV_FF_PARSER_NODE
    //For PVMFWAVFFParserNode
    nodeinfo.iInputTypes.clear();
    nodeinfo.iInputTypes.push_back(PVMFFormatType(PVMF_MIME_WAVFF));
    nodeinfo.iNodeUUID = KPVMFWavFFParserNodeUuid;
    nodeinfo.iOutputType.clear();
    nodeinfo.iOutputType.push_back(PVMFFormatType(PVMF_MIME_FORMAT_UNKNOWN));
    nodeinfo.iNodeCreateFunc = PVMFWAVFFParserNodeFactory::CreatePVMFWAVFFParserNode;
    nodeinfo.iNodeReleaseFunc = PVMFWAVFFParserNodeFactory::DeletePVMFWAVFFParserNode;
    aRegistry->RegisterNode(nodeinfo);
#endif
#if BUILD_ASF_FF_PARSER_NODE
    //For PVMFASFParserNode
    nodeinfo.iInputTypes.clear();
    nodeinfo.iInputTypes.push_back(PVMFFormatType(PVMF_MIME_ASFFF));
    nodeinfo.iNodeUUID = KPVMFASFFFParserNodeUuid;
    nodeinfo.iOutputType.clear();
    nodeinfo.iOutputType.push_back(PVMFFormatType(PVMF_MIME_FORMAT_UNKNOWN));
    nodeinfo.iNodeCreateFunc = PVMFASFParserNodeFactory::CreatePVMFASFParserNode;
    nodeinfo.iNodeReleaseFunc = PVMFASFParserNodeFactory::DeletePVMFASFParserNode;
    aRegistry->RegisterNode(nodeinfo);
#endif
#if BUILD_RM_FF_PARSER_NODE
    //For PVMFRMFFParserNode
    nodeinfo.iInputTypes.clear();
    nodeinfo.iInputTypes.push_back(PVMFFormatType(PVMF_MIME_RMFF));
    nodeinfo.iNodeUUID = KPVMFRMFFParserNodeUuid;
    nodeinfo.iOutputType.clear();
    nodeinfo.iOutputType.push_back(PVMFFormatType(PVMF_MIME_FORMAT_UNKNOWN));
    nodeinfo.iNodeCreateFunc = PVMFRMFFParserNodeFactory::CreatePVMFRMFFParserNode;
    nodeinfo.iNodeReleaseFunc = PVMFRMFFParserNodeFactory::DeletePVMFRMFFParserNode;
    aRegistry->RegisterNode(nodeinfo);
#endif
}

void PVMERegistryPopulator::UnregisterAllNodes(PVPlayerNodeRegistryInterface* aRegistry, OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aRegistry);
    OSCL_UNUSED_ARG(aContext);
    //nothing needed currently.
}

// Recognizers for ASF, OMA1, AAC, RM, MP3
void PVMERegistryPopulator::RegisterAllRecognizers(PVPlayerRecognizerRegistryInterface* aRegistry, OsclAny*& aContext)
{
    //Keep a list of all factories allocated by this populator for later cleanup.
    typedef Oscl_Vector<PVMFRecognizerPluginFactory*, OsclMemAllocator> nodelistType;
    nodelistType* nodeList = OSCL_NEW(nodelistType, ());
    aContext = nodeList;

    PVMFRecognizerPluginFactory* tmpfac = NULL;

#if BUILD_MP4_FF_REC
    tmpfac = OSCL_STATIC_CAST(PVMFRecognizerPluginFactory*, OSCL_NEW(PVMP4FFRecognizerFactory, ()));
    if (PVMFRecognizerRegistry::RegisterPlugin(*tmpfac) == PVMFSuccess)
    {
        aRegistry->RegisterRecognizer(tmpfac);
        nodeList->push_back(tmpfac);
    }
    else
    {
        OSCL_DELETE(((PVMP4FFRecognizerFactory*)tmpfac));
        tmpfac = NULL;
        return;
    }
#endif
#if BUILD_ASF_FF_REC
    tmpfac = OSCL_STATIC_CAST(PVMFRecognizerPluginFactory*, OSCL_NEW(PVASFFFRecognizerFactory, ()));
    if (PVMFRecognizerRegistry::RegisterPlugin(*tmpfac) == PVMFSuccess)
    {
        aRegistry->RegisterRecognizer(tmpfac);
        nodeList->push_back(tmpfac);
    }
    else
    {
        OSCL_DELETE(((PVASFFFRecognizerFactory*)tmpfac));
        tmpfac = NULL;
        return;
    }
#endif
#if BUILD_OMA1_FF_REC
    tmpfac = OSCL_STATIC_CAST(PVMFRecognizerPluginFactory*, OSCL_NEW(PVOMA1FFRecognizerFactory, ()));
    if (PVMFRecognizerRegistry::RegisterPlugin(*tmpfac) == PVMFSuccess)
    {
        aRegistry->RegisterRecognizer(tmpfac);
        nodeList->push_back(tmpfac);
    }
    else
    {
        OSCL_DELETE(((PVOMA1FFRecognizerFactory*)tmpfac));
        tmpfac = NULL;
        return;
    }
#endif
#if BUILD_AAC_FF_REC
    tmpfac = OSCL_STATIC_CAST(PVMFRecognizerPluginFactory*, OSCL_NEW(PVAACFFRecognizerFactory, ()));
    if (PVMFRecognizerRegistry::RegisterPlugin(*tmpfac) == PVMFSuccess)
    {
        aRegistry->RegisterRecognizer(tmpfac);
        nodeList->push_back(tmpfac);
    }
    else
    {
        OSCL_DELETE(((PVAACFFRecognizerFactory*)tmpfac));
        tmpfac = NULL;
        return;
    }
#endif
#if BUILD_RM_FF_REC
    tmpfac = OSCL_STATIC_CAST(PVMFRecognizerPluginFactory*, OSCL_NEW(PVRMFFRecognizerFactory, ()));
    if (PVMFRecognizerRegistry::RegisterPlugin(*tmpfac) == PVMFSuccess)
    {
        aRegistry->RegisterRecognizer(tmpfac);
        nodeList->push_back(tmpfac);
    }
    else
    {
        OSCL_DELETE(((PVRMFFRecognizerFactory*)tmpfac));
        tmpfac = NULL;
        return;
    }
#endif
#if BUILD_MP3_FF_REC
    tmpfac = OSCL_STATIC_CAST(PVMFRecognizerPluginFactory*, OSCL_NEW(PVMP3FFRecognizerFactory, ()));
    if (PVMFRecognizerRegistry::RegisterPlugin(*tmpfac) == PVMFSuccess)
    {
        aRegistry->RegisterRecognizer(tmpfac);
        nodeList->push_back(tmpfac);
    }
    else
    {
        OSCL_DELETE(((PVMP3FFRecognizerFactory*)tmpfac));
        tmpfac = NULL;
        return;
    }
#endif
#if BUILD_WAV_FF_REC
    tmpfac = OSCL_STATIC_CAST(PVMFRecognizerPluginFactory*, OSCL_NEW(PVWAVFFRecognizerFactory, ()));
    if (PVMFRecognizerRegistry::RegisterPlugin(*tmpfac) == PVMFSuccess)
    {
        aRegistry->RegisterRecognizer(tmpfac);
        nodeList->push_back(tmpfac);
    }
    else
    {
        OSCL_DELETE(((PVWAVFFRecognizerFactory*)tmpfac));
        tmpfac = NULL;
        return;
    }
#endif
#if BUILD_AMR_FF_REC
    tmpfac = OSCL_STATIC_CAST(PVMFRecognizerPluginFactory*, OSCL_NEW(PVAMRFFRecognizerFactory, ()));
    if (PVMFRecognizerRegistry::RegisterPlugin(*tmpfac) == PVMFSuccess)
    {
        aRegistry->RegisterRecognizer(tmpfac);
        nodeList->push_back(tmpfac);
    }
    else
    {
        OSCL_DELETE(((PVAMRFFRecognizerFactory*)tmpfac));
        tmpfac = NULL;
        return;
    }
#endif
}

void PVMERegistryPopulator::UnregisterAllRecognizers(PVPlayerRecognizerRegistryInterface* aRegistry, OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aRegistry);

    //find the nodes added by this populator & delete them.
    Oscl_Vector<PVMFRecognizerPluginFactory*, OsclMemAllocator>* nodeList = (Oscl_Vector<PVMFRecognizerPluginFactory*, OsclMemAllocator>*) aContext;
    if (nodeList)
    {
        PVMFRecognizerPluginFactory* tmpfac = NULL;
        while (nodeList->size())
        {
            tmpfac = nodeList->front();;
            nodeList->erase(nodeList->begin());
            PVMFRecognizerRegistry::RemovePlugin(*tmpfac);
            OSCL_DELETE(tmpfac);
        }
        OSCL_DELETE(nodeList);
    }
}






