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
#ifndef PVMF_SM_FSP_RTSP_UNICAST_H
#include "pvmf_sm_fsp_rtsp_unicast.h"
#endif

#ifndef OSCL_EXCLUSIVE_PTR_H_INCLUDED
#include "oscl_exclusive_ptr.h"
#endif

#ifndef PVMF_SOCKET_NODE_H_INCLUDED
#include "pvmf_socket_node.h"
#endif

#ifndef PVMF_RTSP_ENGINE_NODE_FACTORY_H_INCLUDED
#include "pvrtsp_client_engine_factory.h"
#endif

#ifndef PVMF_JITTER_BUFFER_NODE_H_INCLUDED
#include "pvmf_jitter_buffer_node.h"
#endif

#ifndef PVRTSP_ENGINE_NODE_EXTENSION_INTERFACE_H_INCLUDED
#include "pvrtspenginenodeextensioninterface.h"
#endif

#ifndef PVMF_MEDIA_PRESENTATION_INFO_H_INCLUDED
#include "pvmf_media_presentation_info.h"
#endif

#ifndef PVMF_SM_FSP_BASE_METADATA_H_INCLUDED
#include "pvmf_sm_fsp_base_metadata.h"
#endif

#ifndef SDP_PARSER_H
#include "sdp_parser.h"
#endif

#ifndef OSCL_SNPRINTF_H_INCLUDED
#include "oscl_snprintf.h"
#endif

#ifndef AMR_PAYLOAD_PARSER_FACTORY_H_INCLUDED
#include "amr_payload_parser_factory.h"
#endif
#ifndef H263_PAYLOAD_PARSER_FACTORY_H_INCLUDED
#include "h263_payload_parser_factory.h"
#endif
#ifndef H264_PAYLOAD_PARSER_FACTORY_H_INCLUDED
#include "h264_payload_parser_factory.h"
#endif
#ifndef M4V_AUDIO_PAYLOAD_PARSER_FACTORY_H_INCLUDED
#include "m4v_audio_payload_parser_factory.h"
#endif
#ifndef M4V_PAYLOAD_PARSER_FACTORY_H_INCLUDED
#include "m4v_payload_parser_factory.h"
#endif
#ifndef PAYLOAD_PARSER_REGISTRY_H_INCLUDED
#include "payload_parser_registry.h"
#endif
#ifndef RFC3640_PAYLOAD_PARSER_FACTORY_H_INCLUDED
#include "rfc3640_payload_parser_factory.h"
#endif

#ifndef OSCL_MIME_STRING_UTILS_H
#include "pv_mime_string_utils.h"
#endif

#ifndef PVMI_KVP_UTIL_H_INCLUDED
#include "pvmi_kvp_util.h"
#endif

#ifndef PVMI_DRM_KVP_H_INCLUDED
#include "pvmi_drm_kvp.h"
#endif


#ifndef PVRTSP_CLIENT_ENGINE_NODE_H
#include "pvrtsp_client_engine_node.h"
#endif

#ifndef PVMF_ERRORINFOMESSAGE_EXTENSION_H_INCLUDED
#include "pvmf_errorinfomessage_extension.h"
#endif

#ifndef PVMF_SM_RTSP_UNICAST_CAPANDCONFIG_H
#include "pvmf_sm_rtsp_unicast_capandconfig.h"
#endif

#ifndef SDP_MEDIA_PARSER_REGISTRY_POPULATOR
#include "sdp_mediaparser_registry_populator.h"
#endif

#include "pvmf_rtp_jitter_buffer_factory.h"

#ifndef OSCL_DLL_H_INCLUDED
#include "oscl_dll.h"
#endif

OSCL_DLL_ENTRY_POINT_DEFAULT()
/**
///////////////////////////////////////////////////////////////////////////////
// Node Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////
*/
PVMFSMRTSPUnicastNode * PVMFSMRTSPUnicastNode::New(int32 aPriority)
{
    PVMFSMRTSPUnicastNode * rtspUnicastNode = OSCL_NEW(PVMFSMRTSPUnicastNode, (aPriority));
    if (rtspUnicastNode)
    {
        rtspUnicastNode->Construct();
    }
    return rtspUnicastNode;
}

PVMFSMRTSPUnicastNode::PVMFSMRTSPUnicastNode(int32 aPriority): PVMFSMFSPBaseNode(aPriority)
{
    iPlayListRepositioningSupported = true;
    iSelectTracksAndSendFCSRequestFlag = false;
    iJitterBufferDurationInMilliSeconds = DEFAULT_JITTER_BUFFER_DURATION_IN_MS;
    oAutoReposition = false;
    iPauseDenied    = false;
    iUseRandomUDPPort = true;
    iStartPortNum = 0;
    iSwitchSDPLocation = NULL;
    ResetNodeParams(false);
}

PVMFSMRTSPUnicastNode::~PVMFSMRTSPUnicastNode()
{
    CleanUp();
}

void PVMFSMRTSPUnicastNode::Construct()
{
    PVMFSMFSPBaseNode::Construct();
    int32 err;
    OSCL_TRY(err,
             iLogger = PVLogger::GetLoggerObject("PVMFSMRTSPUnicastNode");
             iAvailableMetadataKeys.reserve(PVMFSTREAMINGMGRNODE_NUM_METADATAKEYS);
             iAvailableMetadataKeys.clear();
             // create the payload parser registry
             PopulatePayloadParserRegistry();
             CreateChildNodes();
             QueryChildNodesExtentionInterface();
             // pass the payload parser registry on to the jitter buffer node
             PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                 getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
             OSCL_ASSERT(iJitterBufferNodeContainer);
             PVMFJitterBufferExtensionInterface* jbExtnIntf = NULL;
             if (iJitterBufferNodeContainer)
             jbExtnIntf = (PVMFJitterBufferExtensionInterface*)(iJitterBufferNodeContainer->iExtensions[0]);
             if (jbExtnIntf)
                 jbExtnIntf->setPayloadParserRegistry(PayloadParserRegistry::GetPayloadParserRegistry());
                );
    if (err != OsclErrNone)
    {
        CleanUp();
        OSCL_LEAVE(err);
    }

}

void PVMFSMRTSPUnicastNode::CreateChildNodes()
{
    /*
     * Create Socket Node
     */
    OsclExclusivePtr<PVMFNodeInterface> socketNodeAutoPtr;
    PVMFNodeInterface* iSocketNode;
    iSocketNode = OSCL_NEW(PVMFSocketNode, (OsclActiveObject::EPriorityNominal));
    socketNodeAutoPtr.set(iSocketNode);

    PVMFSMFSPChildNodeContainer sSocketNodeContainer;

    PVMFNodeSessionInfo socketNodeSession(this,
                                          this,
                                          OSCL_REINTERPRET_CAST(OsclAny*,
                                                                iSocketNode),
                                          this,
                                          OSCL_REINTERPRET_CAST(OsclAny*,
                                                                iSocketNode));

    sSocketNodeContainer.iNode = iSocketNode;
    sSocketNodeContainer.iSessionId =
        iSocketNode->Connect(socketNodeSession);
    sSocketNodeContainer.iNodeTag =
        PVMF_SM_FSP_SOCKET_NODE;
    sSocketNodeContainer.commandStartOffset =
        PVMF_SM_FSP_SOCKET_NODE_COMMAND_START;
    /* Push back the known UUID in case there are no queries */
    PVUuid uuid(PVMF_SOCKET_NODE_EXTENSION_INTERFACE_UUID);
    sSocketNodeContainer.iExtensionUuids.push_back(uuid);
    iFSPChildNodeContainerVec.push_back(sSocketNodeContainer);

    /*
     * Create Session Controller Node
     */
    OsclExclusivePtr<PVMFNodeInterface> sessionControllerAutoPtr;
    //PVMFNodeInterface* iSessionControllerNode = OSCL_NEW(PVRTSPEngineNode, (OsclActiveObject::EPriorityNominal));
    PVMFNodeInterface* iSessionControllerNode = PVMFRrtspEngineNodeFactory::CreatePVMFRtspEngineNode(OsclActiveObject::EPriorityNominal);
    sessionControllerAutoPtr.set(iSessionControllerNode);

    PVMFSMFSPChildNodeContainer sSessionControllerNodeContainer;

    PVMFNodeSessionInfo sessionControllerSession(this,
            this,
            OSCL_REINTERPRET_CAST(OsclAny*,
                                  iSessionControllerNode),
            this,
            OSCL_REINTERPRET_CAST(OsclAny*,
                                  iSessionControllerNode));

    sSessionControllerNodeContainer.iNode = iSessionControllerNode;
    sSessionControllerNodeContainer.iSessionId =
        iSessionControllerNode->Connect(sessionControllerSession);
    sSessionControllerNodeContainer.iNodeTag =
        PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE;
    sSessionControllerNodeContainer.commandStartOffset =
        PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_COMMAND_START;
    /* Push back the known UUID in case there are no queries */
    sSessionControllerNodeContainer.iExtensionUuids.push_back(KPVRTSPEngineNodeExtensionUuid);
    iFSPChildNodeContainerVec.push_back(sSessionControllerNodeContainer);


    /*
     * Create jitter buffer node
     */
    OsclExclusivePtr<PVMFNodeInterface> jitterBufferNodeAutoPtr;
    PVMFNodeInterface* iJitterBufferNode;
    iJBFactory = OSCL_NEW(RTPJitterBufferFactory, ());
    iJitterBufferNode = OSCL_NEW(PVMFJitterBufferNode, (OsclActiveObject::EPriorityNominal, iJBFactory));

    jitterBufferNodeAutoPtr.set(iJitterBufferNode);

    PVMFSMFSPChildNodeContainer sJitterBufferNodeContainer;

    PVMFNodeSessionInfo jitterBufferSession(this,
                                            this,
                                            OSCL_REINTERPRET_CAST(OsclAny*,
                                                                  iJitterBufferNode),
                                            this,
                                            OSCL_REINTERPRET_CAST(OsclAny*,
                                                                  iJitterBufferNode));

    sJitterBufferNodeContainer.iNode = iJitterBufferNode;
    sJitterBufferNodeContainer.iSessionId =
        iJitterBufferNode->Connect(jitterBufferSession);
    sJitterBufferNodeContainer.iNodeTag =
        PVMF_SM_FSP_JITTER_BUFFER_NODE;
    sJitterBufferNodeContainer.commandStartOffset =
        PVMF_SM_FSP_JITTER_BUFFER_CONTROLLER_COMMAND_START;
    /* Push back the known UUID in case there are no queries */
    sJitterBufferNodeContainer.iExtensionUuids.push_back(PVMF_JITTERBUFFERNODE_EXTENSIONINTERFACE_UUID);
    iFSPChildNodeContainerVec.push_back(sJitterBufferNodeContainer);

    sessionControllerAutoPtr.release();
    socketNodeAutoPtr.release();
    jitterBufferNodeAutoPtr.release();
}

void PVMFSMRTSPUnicastNode::DestroyChildNodes()
{
    uint32 i, j;
    for (i = 0; i < iFSPChildNodeContainerVec.size(); i++)
    {
        for (j = 0; j < iFSPChildNodeContainerVec[i].iExtensions.size(); j++)
        {
            PVInterface* extIntf = iFSPChildNodeContainerVec[i].iExtensions[j];
            extIntf->removeRef();
        }

        if (iFSPChildNodeContainerVec[i].iNodeTag == PVMF_SM_FSP_SOCKET_NODE)
        {
            OSCL_DELETE(OSCL_STATIC_CAST(PVMFSocketNode*, iFSPChildNodeContainerVec[i].iNode));
        }
        else if (iFSPChildNodeContainerVec[i].iNodeTag == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE)
        {
            PVMFRrtspEngineNodeFactory::DeletePVMFRtspEngineNode(iFSPChildNodeContainerVec[i].iNode);
        }
        else if (iFSPChildNodeContainerVec[i].iNodeTag == PVMF_SM_FSP_JITTER_BUFFER_NODE)
        {
            OSCL_DELETE(OSCL_STATIC_CAST(PVMFJitterBufferNode*, iFSPChildNodeContainerVec[i].iNode));
        }
        iFSPChildNodeContainerVec[i].iNode = NULL;
    }

    iFSPChildNodeContainerVec.clear();
}

void PVMFSMRTSPUnicastNode::PopulatePayloadParserRegistry()
{
    PayloadParserRegistry* registry =
        PayloadParserRegistry::GetPayloadParserRegistry();
    OSCL_ASSERT(registry == NULL);
    PayloadParserRegistry::Init();
    registry = PayloadParserRegistry::GetPayloadParserRegistry();

    StrPtrLen aac_latm("audio/MP4A-LATM");
    StrPtrLen amr("audio/AMR");
    StrPtrLen amrwb("audio/AMR-WB");
    StrPtrLen h263_old("video/H263-1998");
    StrPtrLen h263("video/H263-2000");
    StrPtrLen m4v("video/MP4V-ES");
    StrPtrLen h264("video/H264");
    StrPtrLen mp4a(PVMF_MIME_MPEG4_AUDIO);
    StrPtrLen rfc3640("audio/mpeg4-generic");

    IPayloadParserFactory* m4vP = OSCL_NEW(M4VPayloadParserFactory, ());
    IPayloadParserFactory* aacP = OSCL_NEW(M4VAudioPayloadParserFactory, ());
    IPayloadParserFactory* amrP = OSCL_NEW(AmrPayloadParserFactory, ());
    IPayloadParserFactory* h263P = OSCL_NEW(H263PayloadParserFactory, ());
    IPayloadParserFactory* h264P = OSCL_NEW(H264PayloadParserFactory, ());
    IPayloadParserFactory* amrwbP = OSCL_NEW(AmrPayloadParserFactory, ());
    IPayloadParserFactory* rfc3640P = OSCL_NEW(RFC3640PayloadParserFactory, ());

    registry->addPayloadParserFactoryToRegistry(m4v, m4vP);
    registry->addPayloadParserFactoryToRegistry(h264, h264P);
    registry->addPayloadParserFactoryToRegistry(aac_latm, aacP);
    registry->addPayloadParserFactoryToRegistry(mp4a, aacP);
    registry->addPayloadParserFactoryToRegistry(amr, amrP);
    registry->addPayloadParserFactoryToRegistry(amrwb, amrwbP);
    registry->addPayloadParserFactoryToRegistry(h263_old, h263P);
    registry->addPayloadParserFactoryToRegistry(h263, h263P);
    registry->addPayloadParserFactoryToRegistry(rfc3640,  rfc3640P);
}

void PVMFSMRTSPUnicastNode::DestroyPayloadParserRegistry()
{
    StrPtrLen aac_latm("audio/MP4A-LATM");
    StrPtrLen amr("audio/AMR");
    StrPtrLen amrwb("audio/AMR-WB");
    StrPtrLen h263("video/H263-2000");
    StrPtrLen m4v("video/MP4V-ES");
    StrPtrLen h264("video/H264");
    StrPtrLen rfc3640("audio/mpeg4-generic");

    PayloadParserRegistry* registry =
        PayloadParserRegistry::GetPayloadParserRegistry();
    if (registry == NULL) return;

    OsclMemoryFragment memFrag;

    memFrag.ptr = (OsclAny*)(m4v.c_str());
    memFrag.len = (uint32)m4v.size();
    OSCL_DELETE(registry->lookupPayloadParserFactory(memFrag));

    memFrag.ptr = (OsclAny*)(h264.c_str());
    memFrag.len = (uint32)h264.size();
    OSCL_DELETE(registry->lookupPayloadParserFactory(memFrag));

    memFrag.ptr = (OsclAny*)(aac_latm.c_str());
    memFrag.len = (uint32)aac_latm.size();
    OSCL_DELETE(registry->lookupPayloadParserFactory(memFrag));

    memFrag.ptr = (OsclAny*)(amr.c_str());
    memFrag.len = (uint32)amr.size();
    OSCL_DELETE(registry->lookupPayloadParserFactory(memFrag));

    memFrag.ptr = (OsclAny*)(amrwb.c_str());
    memFrag.len = (uint32)amrwb.size();
    OSCL_DELETE(registry->lookupPayloadParserFactory(memFrag));

    memFrag.ptr = (OsclAny*)(h263.c_str());
    memFrag.len = (uint32)h263.size();
    OSCL_DELETE(registry->lookupPayloadParserFactory(memFrag));

    memFrag.ptr = (OsclAny*)(rfc3640.c_str());
    memFrag.len = (uint32)rfc3640.size();
    OSCL_DELETE(registry->lookupPayloadParserFactory(memFrag));

    PayloadParserRegistry::Cleanup();
}

void PVMFSMRTSPUnicastNode::QueryChildNodesExtentionInterface()
{
    PVMFSMFSPChildNodeContainerVector::iterator it;
    for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
    {
        //we dont use dynamic cast for cross cast.. because some compilers may not support it, and to avoid
        //run time overheads.... So, we need to explicitly downcast the pvmfnodeinterface ptr to its
        //concrete implementation and then, upcast it into required PVInterface class

        PVInterface * interfacePtr = NULL;
        switch (it->iNodeTag)
        {
            case PVMF_SM_FSP_SOCKET_NODE:
            {
                PVMFSocketNode * tmpPtr = OSCL_STATIC_CAST(PVMFSocketNode *, it->iNode);
                interfacePtr = OSCL_STATIC_CAST(PVInterface*, tmpPtr);
            }
            break;
            case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE:
            {
                PVRTSPEngineNode * tmpPtr = OSCL_STATIC_CAST(PVRTSPEngineNode *, it->iNode);
                interfacePtr = OSCL_STATIC_CAST(PVInterface*, tmpPtr);
            }
            break;
            case PVMF_SM_FSP_JITTER_BUFFER_NODE:
            {
                PVMFJitterBufferNode * tmpPtr = OSCL_STATIC_CAST(PVMFJitterBufferNode *, it->iNode);
                interfacePtr = OSCL_STATIC_CAST(PVInterface*, tmpPtr);
            }
            break;
        }

        PVInterface* extensionIntfPtr = NULL;
        bool retval = interfacePtr->queryInterface(it->iExtensionUuids.front(), extensionIntfPtr);
        if (retval && extensionIntfPtr)
        {
            it->iExtensions.push_back(extensionIntfPtr);
        }
    }
}

/**
 * Called by the command handler AO to process a command from
 * the input queue.
 * Return true if a command was processed, false if the command
 * processor is busy and can't process another command now.
 */
bool PVMFSMRTSPUnicastNode::ProcessCommand(PVMFSMFSPBaseNodeCommand& aCmd)
{
    if (EPVMFNodeError == iInterfaceState)
    {
        if (iCurrErrHandlingCommand.size() > 0)
        {
            return false;
        }
        switch (aCmd.iCmd)
        {
            case PVMF_SMFSP_NODE_CANCEL_DUE_TO_ERROR:
                DoCancelAllPendingCommands(aCmd);
                break;
            case PVMF_SMFSP_NODE_RESET_DUE_TO_ERROR:
                DoResetDueToErr(aCmd);
                break;
        }

        return true;
    }
    /*
     * normally this node will not start processing one command
     * until the prior one is finished.  However, a hi priority
     * command such as Cancel must be able to interrupt a command
     * in progress.
     */
    if ((iCurrentCommand.size() > 0 && !aCmd.hipri())
            || iCancelCommand.size() > 0)
        return false;

    OSCL_ASSERT(PVMF_SMFSP_NODE_SET_DATASOURCE_RATE != aCmd.iCmd);
    switch (aCmd.iCmd)
    {
            /* node interface commands */
        case PVMF_SMFSP_NODE_QUERYINTERFACE:
            DoQueryInterface(aCmd);
            break;
        case PVMF_SMFSP_NODE_INIT:
            DoInit(aCmd);
            break;
        case PVMF_SMFSP_NODE_PREPARE:
            DoPrepare(aCmd);
            break;
        case PVMF_SMFSP_NODE_REQUESTPORT:
            DoRequestPort(aCmd);
            break;
        case PVMF_SMFSP_NODE_RELEASEPORT:
            DoReleasePort(aCmd);
            break;
        case PVMF_SMFSP_NODE_START:
            DoStart(aCmd);
            break;
        case PVMF_SMFSP_NODE_STOP:
            DoStop(aCmd);
            break;
        case PVMF_SMFSP_NODE_FLUSH:
            DoFlush(aCmd);
            break;
        case PVMF_SMFSP_NODE_PAUSE:
            if (iPauseDenied)
            {
                //Check with the jitter buffer node if the session is already expired?
                bool isSessionDurationExpired = false;
                PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
                if (jitterBufferNodeContainer)
                {
                    PVMFJitterBufferExtensionInterface* jbExtIntf =
                        OSCL_STATIC_CAST(PVMFJitterBufferExtensionInterface*, jitterBufferNodeContainer->iExtensions.front());
                    if (jbExtIntf)
                        jbExtIntf->HasSessionDurationExpired(isSessionDurationExpired);
                }

                if (isSessionDurationExpired)
                {
                    DoPause(aCmd);
                }
                else
                {
                    CommandComplete(iInputCommands, aCmd, PVMFErrNotSupported);
                }
            }
            else
            {
                DoPause(aCmd);
            }
            break;
        case PVMF_SMFSP_NODE_RESET:
            DoReset(aCmd);
            break;
        case PVMF_SMFSP_NODE_CANCELALLCOMMANDS:
            DoCancelAllCommands(aCmd);
            break;
        case PVMF_SMFSP_NODE_CANCELCOMMAND:
            DoCancelCommand(aCmd);
            break;

            /* add extention interface commands */
        case PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION:
            if (iPlayListRepositioning)
            {
                DoSetDataSourcePositionPlayList(aCmd);
            }
            else
            {
                if (!oAutoReposition)
                {
                    DoSetDataSourcePosition(aCmd);
                }
                else
                {
                    DoSetDataSourcePositionOverflow(aCmd);
                }
            }
            break;

        case PVMF_SMFSP_NODE_QUERY_DATASOURCE_POSITION:
            DoQueryDataSourcePosition(aCmd);
            break;
        case PVMF_SMFSP_NODE_SET_DATASOURCE_RATE:

            break;
        case PVMF_SMFSP_NODE_GETNODEMETADATAVALUES:
        {
            PVMFStatus status = DoGetMetadataValues(aCmd);
            if (status != PVMFPending)
            {
                CommandComplete(iInputCommands, aCmd, status);
            }
            else
            {
                MoveCmdToCurrentQueue(aCmd);
            }
        }
        break;
        case PVMF_SMFSP_NODE_CAPCONFIG_SETPARAMS:
        {
            PvmiMIOSession session;
            PvmiKvp* aParameters;
            int num_elements;
            PvmiKvp** ppRet_kvp;
            aCmd.Parse(session, aParameters, num_elements, ppRet_kvp);
            setParametersSync(NULL, aParameters, num_elements, *ppRet_kvp);
            ciObserver->SignalEvent(aCmd.iId);
        }
        break;

        /* internal commands common to all types of streaming*/
        case PVMF_SMFSP_NODE_CONSTRUCT_SESSION: //to construct the graph
        {
            PVMFStatus status = DoGraphConstruct();
            if (status != PVMFPending)
            {
                InternalCommandComplete(iInputCommands, aCmd, status);
            }
            else
            {
                MoveCmdToCurrentQueue(aCmd);
            }
        }
        break;

        /* internal commands specific to rtsp unicast streaming*/

        /* unknown commands */
        default:
            /* unknown command type */
            CommandComplete(iInputCommands, aCmd, PVMFErrNotSupported);
            break;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
//Node command servicing functions queued in input command Q by base class
///////////////////////////////////////////////////////////////////////////////
void PVMFSMRTSPUnicastNode::DoQueryInterface(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoQueryInterface - In"));

    PVUuid* uuid;
    PVInterface** ptr;
    aCmd.PVMFSMFSPBaseNodeCommandBase::Parse(uuid, ptr);

    *ptr = NULL;

    if (*uuid == PVMF_TRACK_SELECTION_INTERFACE_UUID)
    {
        PVMFTrackSelectionExtensionInterface* interimPtr =
            OSCL_STATIC_CAST(PVMFTrackSelectionExtensionInterface*, this);
        *ptr = OSCL_STATIC_CAST(PVInterface*, interimPtr);
    }
    else if (*uuid == PVMF_DATA_SOURCE_INIT_INTERFACE_UUID)
    {
        PVMFDataSourceInitializationExtensionInterface* interimPtr =
            OSCL_STATIC_CAST(PVMFDataSourceInitializationExtensionInterface*, this);
        *ptr = OSCL_STATIC_CAST(PVInterface*, interimPtr);
    }
    else if (*uuid == PvmfDataSourcePlaybackControlUuid)
    {
        PvmfDataSourcePlaybackControlInterface* interimPtr =
            OSCL_STATIC_CAST(PvmfDataSourcePlaybackControlInterface*, this);
        *ptr = OSCL_STATIC_CAST(PVInterface*, interimPtr);
    }
    else if (*uuid == KPVMFMetadataExtensionUuid)
    {
        PVMFMetadataExtensionInterface* interimPtr =
            OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, this);
        *ptr = OSCL_STATIC_CAST(PVInterface*, interimPtr);
    }
    else if (*uuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* interimPtr =
            OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        *ptr = OSCL_STATIC_CAST(PVInterface*, interimPtr);
    }
    else if (*uuid == PVMFCPMPluginLicenseInterfaceUuid)
    {
        *ptr = OSCL_STATIC_CAST(PVInterface*, iCPMLicenseInterface);
    }
    else
    {
        *ptr = NULL;
    }

    if (*ptr)
    {
        PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::QueryInterface() - CmdComplete - PVMFSuccess"));
        CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    }
    else
    {
        PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::QueryInterface() - CmdFailed - PVMFErrNotSupported"));
        CommandComplete(iInputCommands, aCmd, PVMFErrNotSupported);
    }


    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoQueryInterface - Out"));
    return;
}

void PVMFSMRTSPUnicastNode::DoInit(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoInit - In"));
    switch (iInterfaceState)
    {
        case EPVMFNodeIdle:
        {
            /*
             * At first Init, PVMFErrLicesneRequest is replied from Janus.
             * Then iCPMInitPending is set into true.
             * If second Init is called, just to check license authentication is required.
             */
            if (iCPMInitPending)
            {
                MoveCmdToCurrentQueue(aCmd);
                if (iSessionSourceInfo->iDRMProtected == true && iCPM)
                {
                    RequestUsage();
                    return;
                }
                else
                {
                    CommandComplete(iCurrentCommand,
                                    iCurrentCommand.front(),
                                    PVMFFailure,
                                    NULL, NULL, NULL);
                    return;
                }
            }
            else
            {
                /* An asynchronous method that prepare's the node for init */
                PVMFStatus status = DoPreInit(aCmd);
                if (status == PVMFSuccess)
                {
                    /*
                     * Init for streaming manager cannot be completed unless Init
                     * for all the children nodes are complete
                     */
                    PVMFSMFSPChildNodeContainerVector::iterator it;
                    for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
                    {
                        PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
                        if (internalCmd != NULL)
                        {
                            internalCmd->cmd =
                                it->commandStartOffset +
                                PVMF_SM_FSP_NODE_INTERNAL_INIT_CMD_OFFSET;
                            internalCmd->parentCmd = aCmd.iCmd;

                            OsclAny *cmdContextData =
                                OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                            PVMFNodeInterface* iNode = it->iNode;

                            iNode->Init(it->iSessionId, cmdContextData);
                            it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
                        }
                        else
                        {
                            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoInit:RequestNewInternalCmd - Failed"));
                            CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
                            return;
                        }
                    }
                    MoveCmdToCurrentQueue(aCmd);
                }
                else if (status == PVMFPending)
                {
                    MoveCmdToCurrentQueue(aCmd);
                }
                else
                {
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoInit: DoPreInit() - Failed"));
                    PVUuid eventuuid = PVMFStreamingManagerNodeEventTypeUUID;
                    int32 errcode = PVMFStreamingManagerNodeErrorParseSDPFailed;
                    CommandComplete(iInputCommands, aCmd, PVMFFailure, NULL, &eventuuid, &errcode);
                    return;
                }
            }
        }
        break;

        default:
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::DoInit Failed - Invalid State"));
            CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
        }
        break;
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoInit - Out"));
}

/**
 * Call by DoInit as a prep step
 */
PVMFStatus PVMFSMRTSPUnicastNode::DoPreInit(PVMFSMFSPBaseNodeCommand& aCmd)
{
    OSCL_UNUSED_ARG(aCmd);
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoPreInit - In"));
    PVMFStatus status = PVMFSuccess;

    if (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE)
    {
        status = ProcessSDP();
        if (status != PVMFSuccess)
        {
            PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoPreInit - Out, status=%d", status));
            return status;
        }

        // ClipEndURL found - let UI know about this
        if (iSdpInfo->getSessionInfo()->getEndOfClipAction() == LAUNCH_URL)
        {
            PVUuid eventuuid = PVMFStreamingManagerNodeEventTypeUUID;
            int32 infocode = PVMFStreamingManagerNodeInfoLaunchURL;
            ReportInfoEvent(PVMFInfoRemoteSourceNotification, (OsclAny*)(iSdpInfo->getSessionInfo()->getLaunchURL()), &eventuuid, &infocode);
        }

        PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);

        if (iSessionControllerNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return PVMFFailure;
        }

        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            (PVRTSPEngineNodeExtensionInterface*)
            (iSessionControllerNodeContainer->iExtensions[0]);

        /*
         * This vector is intentionally left uninitialized.
         * Streaming manager does not have any track selection info
         * at this stage. "SetSDPInfo" would be called again before
         * prepare complete to set up all the selected tracks. This
         * call is needed here to indicate to Session Controller node
         * that it is NOT a RTSP URL based session
         */
        Oscl_Vector<StreamInfo, OsclMemAllocator> aSelectedStream;

        status = rtspExtIntf->SetSDPInfo(iSdpInfo,
                                         aSelectedStream);
        if (status != PVMFSuccess)
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:DoPreInit - SetSDPInfo Failed"));
        }

    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoPreInit - Out"));
    return status;
}

PVMFStatus PVMFSMRTSPUnicastNode::ProcessSDP()
{
    PVMFStatus status;
    OsclRefCounterMemFrag iSDPText;

    if (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL)
    {
        PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);

        if (iSessionControllerNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return PVMFFailure;
        }

        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            (PVRTSPEngineNodeExtensionInterface*)
            (iSessionControllerNodeContainer->iExtensions[0]);

        status = rtspExtIntf->GetSDP(iSDPText);

        if (status != PVMFSuccess)
        {
            return status;
        }
    }
    else if (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE)
    {
        /* Parse SDP file contents into a buffer */
        Oscl_FileServer fileServ;
        Oscl_File osclFile;
        fileServ.Connect();
        PVMFSourceContextData* atempData = (PVMFSourceContextData*)iSessionSourceInfo->_sourceData;
        if (atempData && atempData->CommonData() && atempData->CommonData()->iFileHandle)
        {
            OsclFileHandle* tempHandle = atempData ->CommonData()->iFileHandle;
            osclFile.SetFileHandle(tempHandle);
        }

        if (osclFile.Open(iSessionSourceInfo->_sessionURL.get_cstr(),
                          Oscl_File::MODE_READ,
                          fileServ) != 0)
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ProcessSDP - Unable to open SDP file"));
            return PVMFFailure;
        }

        /* Get File Size */
        osclFile.Seek(0, Oscl_File::SEEKEND);
        int32 fileSize = (TOsclFileOffsetInt32)osclFile.Tell();
        osclFile.Seek(0, Oscl_File::SEEKSET);

        if (fileSize <= 0)
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ProcessSDP - Corrupt SDP file"));
            return PVMFFailure;
        }

        OsclMemAllocDestructDealloc<uint8> my_alloc;
        OsclRefCounter* my_refcnt;
        uint aligned_refcnt_size =
            oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));
        /*
         * To acct for null char, as SDP buffer is treated akin to a string by the
         * SDP parser lib.
         */
        uint allocsize = oscl_mem_aligned_size(aligned_refcnt_size + fileSize + 2);
        uint8* my_ptr = GetMemoryChunk(my_alloc, allocsize);
        if (!my_ptr)
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ProcessSDP - Unable to process SDP file"));
            return PVMFFailure;
        }

        my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
        my_ptr += aligned_refcnt_size;

        OsclMemoryFragment memfrag;
        memfrag.len = fileSize;
        memfrag.ptr = my_ptr;

        OsclRefCounterMemFrag tmpRefcntMemFrag(memfrag, my_refcnt, memfrag.len);
        iSDPText = tmpRefcntMemFrag;

        osclFile.Read(memfrag.ptr, 1, fileSize);

        osclFile.Close();
        fileServ.Close();
    }

    PVMFSMSharedPtrAlloc<SDPInfo> sdpAlloc;
    SDPInfo* sdpInfo = sdpAlloc.allocate();

    SDP_Parser *sdpParser;

    SDPMediaParserRegistry* sdpParserReg = SDPMediaParserRegistryPopulater::PopulateRegistry();
    sdpParser = OSCL_NEW(SDP_Parser, (sdpParserReg));

    int32 sdpRetVal =
        sdpParser->parseSDP((const char*)(iSDPText.getMemFragPtr()),
                            iSDPText.getMemFragSize(),
                            sdpInfo);

    // save the SDP file name - the packet source node will need this
    sdpInfo->setSDPFilename(iSessionSourceInfo->_sessionURL);

    OSCL_DELETE(sdpParser);
    SDPMediaParserRegistryPopulater::CleanupRegistry(sdpParserReg);

    OsclRefCounterSA< PVMFSMSharedPtrAlloc<SDPInfo> > *refcnt =
        new OsclRefCounterSA< PVMFSMSharedPtrAlloc<SDPInfo> >(sdpInfo);

    OsclSharedPtr<SDPInfo> sharedSDPInfo(sdpInfo, refcnt);

    if (sdpRetVal != SDP_SUCCESS)
    {
        return PVMFFailure;
    }

    iSdpInfo = sharedSDPInfo;

    return PVMFSuccess;
}

//Compute Jitter buffer mem pool size
uint32 PVMFSMRTSPUnicastNode::GetJitterBufferMemPoolSize(PVMFJitterBufferNodePortTag aJBNodePortTag, PVMFRTSPTrackInfo& aRTSPTrackInfo)
{
    uint32 sizeInBytes = 0;
    uint32 byteRate = (aRTSPTrackInfo.bitRate) / 8;
    uint32 overhead = (byteRate * PVMF_JITTER_BUFFER_NODE_MEM_POOL_OVERHEAD) / 100;
    uint32 jitterBufferDuration;

    PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer = getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (jitterBufferNodeContainer == NULL)
    {
        OSCL_ASSERT(false);
        return false;
    }

    PVMFJitterBufferExtensionInterface* jbExtIntf = OSCL_STATIC_CAST(PVMFJitterBufferExtensionInterface*, jitterBufferNodeContainer->iExtensions[0]);
    if (jbExtIntf == NULL)
    {
        OSCL_ASSERT(false);
        return sizeInBytes;
    }

    jbExtIntf->getJitterBufferDurationInMilliSeconds(jitterBufferDuration);
    uint32 durationInSec = jitterBufferDuration / 1000;
    switch (aJBNodePortTag)
    {
        case PVMF_JITTER_BUFFER_PORT_TYPE_INPUT:
        {
            if (durationInSec > 0)
            {
                sizeInBytes = ((byteRate + overhead) * durationInSec);
                if (sizeInBytes < MIN_RTP_SOCKET_MEM_POOL_SIZE_IN_BYTES)
                {
                    sizeInBytes = MIN_RTP_SOCKET_MEM_POOL_SIZE_IN_BYTES;
                }
                sizeInBytes += (2 * MAX_SOCKET_BUFFER_SIZE);        //eq. to SNODE_UDP_MULTI_MAX_BYTES_PER_RECV + MAX_UDP_PACKET_SIZE used in socket node
            }

        }
        break;
        case PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK:
        {
            if (durationInSec > 0)
            {
                sizeInBytes = MIN_RTP_SOCKET_MEM_POOL_SIZE_IN_BYTES;
                sizeInBytes += (2 * MAX_SOCKET_BUFFER_SIZE);        //eq. to SNODE_UDP_MULTI_MAX_BYTES_PER_RECV + MAX_UDP_PACKET_SIZE used in socket node
            }
        }
        break;
        default:
            sizeInBytes = 0;
    }
    return sizeInBytes;
}

/**
 * Called as a pre step for prepare
 */
PVMFStatus PVMFSMRTSPUnicastNode::DoGraphConstruct()
{
    /*
     * Session source info must have been set
     */
    if (iSessionSourceInfo->_sessionType == PVMF_MIME_FORMAT_UNKNOWN)
    {
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::GraphConstruct - Invalid Session Type %s", iSessionSourceInfo->_sessionType.getMIMEStrPtr()));
        return PVMFFailure;
    }
    if (!PopulateTrackInfoVec())
    {
        return PVMFFailure;
    }

    if (ConstructGraphFor3GPPUDPStreaming())
    {
        if (iTotalNumRequestPortsComplete == iNumRequestPortsPending)
            return PVMFSuccess;
        else
            return PVMFPending;

    }
    else
        return PVMFFailure;
}

bool PVMFSMRTSPUnicastNode::PopulateTrackInfoVec()
{
    if (iSelectedMediaPresetationInfo.getNumTracks() == 0)
    {
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:PopulateTrackInfoVec - Selected Track List Empty"));
        return false;
    }

    /*
     * Get selected tracks
     */

    int32 numTracks = iSdpInfo->getNumMediaObjects();

    if (numTracks > 0)
    {
        for (int32 i = 0; i < numTracks; i++)
        {
            /*
             * Get the vector of mediaInfo as there can
             * alternates for each track
             */
            Oscl_Vector<mediaInfo*, SDPParserAlloc> mediaInfoVec =
                iSdpInfo->getMediaInfo(i);

            uint32 minfoVecLen = mediaInfoVec.size();
            for (uint32 j = 0; j < minfoVecLen; j++)
            {
                mediaInfo* mInfo = mediaInfoVec[j];

                if (mInfo == NULL)
                {
                    return false;
                }

                if (mInfo->getSelect())
                {
                    PVMFRTSPTrackInfo trackInfo;
                    trackInfo.iTransportType += _STRLIT_CHAR("RTP");
                    trackInfo.trackID = mInfo->getMediaInfoID();

                    Oscl_Vector<PayloadSpecificInfoTypeBase*, SDPParserAlloc> payloadVector;
                    payloadVector = mInfo->getPayloadSpecificInfoVector();

                    if (payloadVector.size() == 0)
                    {
                        return false;
                    }
                    /*
                     * There can be multiple payloads per media segment.
                     * We only support one for now, so
                     * use just the first payload
                     */
                    PayloadSpecificInfoTypeBase* payloadInfo = payloadVector[0];
                    trackInfo.trackTimeScale = payloadInfo->sampleRate;

                    // set config for later
                    int32 configSize = payloadInfo->configSize;
                    OsclAny* config = payloadInfo->configHeader.GetRep();

                    const char* mimeType = mInfo->getMIMEType();
                    trackInfo.iMimeType += mimeType;

                    trackInfo.portTag = mInfo->getMediaInfoID();
                    trackInfo.bitRate = mInfo->getBitrate();
                    if (mInfo->getReportFrequency() > 0)
                    {
                        trackInfo.iRateAdaptation = true;
                        trackInfo.iRateAdaptationFeedBackFrequency =
                            mInfo->getReportFrequency();
                    }

                    if ((mInfo->getRTCPReceiverBitRate() >= 0) &&
                            (mInfo->getRTCPSenderBitRate() >= 0))
                    {
                        trackInfo.iRR = mInfo->getRTCPReceiverBitRate();
                        trackInfo.iRS = mInfo->getRTCPSenderBitRate();
                        trackInfo.iRTCPBwSpecified = true;
                    }

                    if ((configSize > 0) && (config != NULL))
                    {
                        OsclMemAllocDestructDealloc<uint8> my_alloc;
                        OsclRefCounter* my_refcnt;
                        uint aligned_refcnt_size =
                            oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));

                        uint8* my_ptr = GetMemoryChunk(my_alloc, aligned_refcnt_size + configSize);
                        if (!my_ptr)
                            return false;

                        my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
                        my_ptr += aligned_refcnt_size;

                        OsclMemoryFragment memfrag;
                        memfrag.len = (uint32)configSize;
                        memfrag.ptr = my_ptr;

                        oscl_memcpy((void*)(memfrag.ptr), (const void*)config, memfrag.len);

                        OsclRefCounterMemFrag tmpRefcntMemFrag(memfrag, my_refcnt, memfrag.len);
                        trackInfo.iTrackConfig = tmpRefcntMemFrag;
                    }
                    iTrackInfoVec.push_back(trackInfo);
                }
            }
        }
    }
    else
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:PopulateTrackInfoVec - Selected Track List Empty"));
        return false;
    }

    return true;
}

bool PVMFSMRTSPUnicastNode::ConstructGraphFor3GPPUDPStreaming()
{
    uint32 numPortsRequested = 0;

    /*
     * Reserve UDP socket pairs
     */
    if (!ReserveSockets())
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ConstructGraphFor3GPPUDPStreaming - ReserveSockets() Failed"));
        return false;
    }

    /*
     * Request data UDP ports
     */
    if (!RequestNetworkNodePorts(PVMF_SOCKET_NODE_PORT_TYPE_SOURCE,
                                 numPortsRequested))
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ConstructGraphFor3GPPUDPStreaming - RequestNetworkNodePorts(PVMF_SOCKET_NODE_PORT_TYPE_SOURCE) Failed"));
        return false;
    }
    iNumRequestPortsPending += numPortsRequested;

    /*
     * Request feedback (RTCP) UDP ports
     */
    if (!RequestNetworkNodePorts(PVMF_SOCKET_NODE_PORT_TYPE_SINK,
                                 numPortsRequested))
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ConstructGraphFor3GPPUDPStreaming - RequestNetworkNodePorts(PVMF_SOCKET_NODE_PORT_TYPE_SINK) Failed"));
        return false;
    }
    iNumRequestPortsPending += numPortsRequested;

    if (!RequestJitterBufferPorts(PVMF_JITTER_BUFFER_PORT_TYPE_INPUT,
                                  numPortsRequested))
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ConstructGraphFor3GPPUDPStreaming - RequestJitterBufferPorts(PVMF_JITTER_BUFFER_PORT_TYPE_INPUT) Failed"));
        return false;
    }
    iNumRequestPortsPending += numPortsRequested;

    if (!RequestJitterBufferPorts(PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT,
                                  numPortsRequested))
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ConstructGraphFor3GPPUDPStreaming - RequestJitterBufferPorts(PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT) Failed"));
        return false;
    }
    iNumRequestPortsPending += numPortsRequested;

    if (!RequestJitterBufferPorts(PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK,
                                  numPortsRequested))
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ConstructGraphFor3GPPUDPStreaming - RequestJitterBufferPorts(PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK) Failed"));
        return false;
    }
    iNumRequestPortsPending += numPortsRequested;

    return true;
}

bool PVMFSMRTSPUnicastNode::ReserveSockets()
{
    uint32 sockid = 0;
    char portConfigBuf[64];
    oscl_memset((OsclAny*)portConfigBuf, 0, 64);
    oscl_snprintf(portConfigBuf, 64, "%d", sockid);
    OSCL_StackString<128> portConfig("UDP");
    portConfig += _STRLIT_CHAR("/remote_address=0.0.0.0");
    portConfig += _STRLIT_CHAR(";client_port=");
    portConfig += portConfigBuf;

    PVMFSMFSPChildNodeContainer* iSocketNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_SOCKET_NODE);
    if (iSocketNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return false;
    }
    PVMFSocketNodeExtensionInterface* snExtIntf =
        (PVMFSocketNodeExtensionInterface*)
        (iSocketNodeContainer->iExtensions[0]);

    PVMFRTSPTrackInfoVector::iterator it;
    if (iUseRandomUDPPort)
    {
        iStartPortNum = 0;
        TimeValue current_time;
        current_time.set_to_current_time();
        uint32 my_seed = current_time.get_sec();

        OsclRand random_num;
        random_num.Seed(my_seed);
        int32 first = random_num.Rand();
        uint32 myport = (first & 0x1FFF) + 0x2000;  //start from 8192
        iStartPortNum = (myport >> 1) << 1;  //start from even
    }

    for (it = iTrackInfoVec.begin();
            it != iTrackInfoVec.end();
            it++)
    {
        OSCL_StackString<128> portConfigWithMime;
        portConfigWithMime += portConfig;
        portConfigWithMime += _STRLIT_CHAR(";mime=");
        portConfigWithMime += it->iMimeType;

        PVMFStatus status = snExtIntf->AllocateConsecutivePorts(&portConfigWithMime,
                            it->iRTPSocketID,
                            it->iRTCPSocketID, iStartPortNum);
        if (status != PVMFSuccess)
        {
            return false;
        }
    }
    return true;
}

bool PVMFSMRTSPUnicastNode::RequestNetworkNodePorts(int32 portTag,
        uint32& numPortsRequested)
{
    numPortsRequested = 0;

    PVMFSMFSPChildNodeContainer* nodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_SOCKET_NODE);

    if (nodeContainer == NULL)
    {
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:RequestNetworkNodePorts - getChildNodeContainer Failed"));
        return false;
    }

    for (uint32 i = 0; i < iTrackInfoVec.size(); i++)
    {
        PVMFRTSPTrackInfo trackInfo = iTrackInfoVec[i];

        PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
        if (internalCmd != NULL)
        {
            internalCmd->cmd =
                nodeContainer->commandStartOffset +
                PVMF_SM_FSP_NODE_INTERNAL_REQUEST_PORT_OFFSET;
            internalCmd->parentCmd = PVMF_SMFSP_NODE_CONSTRUCT_SESSION;
            internalCmd->portContext.trackID = trackInfo.trackID;
            internalCmd->portContext.portTag = portTag;

            OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

            PVMFNodeInterface* iNode = nodeContainer->iNode;

            uint32 sockid = 0;
            bool oRTCP = false;

            if (portTag == PVMF_SOCKET_NODE_PORT_TYPE_SOURCE)
            {
                sockid = trackInfo.iRTPSocketID;
            }
            else if (portTag == PVMF_SOCKET_NODE_PORT_TYPE_SINK)
            {
                sockid = trackInfo.iRTCPSocketID;
                oRTCP = true;
            }

            char portConfigBuf[64];
            oscl_memset((OsclAny*)portConfigBuf, 0, 64);
            oscl_snprintf(portConfigBuf, 64, "%d", sockid);
            OSCL_StackString<128> portConfig("UDP");
            portConfig += _STRLIT_CHAR("/remote_address=0.0.0.0");
            portConfig += _STRLIT_CHAR(";client_port=");
            portConfig += portConfigBuf;
            portConfig += _STRLIT_CHAR(";mime=");
            portConfig += trackInfo.iMimeType.get_cstr();
            if (oRTCP == true)
            {
                portConfig += _STRLIT_CHAR("/rtcp");
            }
            else
            {
                portConfig += _STRLIT_CHAR("/rtp");
            }

            iNode->RequestPort(nodeContainer->iSessionId,
                               internalCmd->portContext.portTag,
                               &portConfig,
                               cmdContextData);
            numPortsRequested++;
            nodeContainer->iNumRequestPortsPending++;
            nodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
        }
        else
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:RequestNetworkNodePorts - RequestNewInternalCmd Failed"));
            return false;
        }
    }
    return true;
}

bool PVMFSMRTSPUnicastNode::RequestJitterBufferPorts(int32 portType,
        uint32 &numPortsRequested)
{
    PVMFSMFSPChildNodeContainer* nodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);

    if (nodeContainer == NULL)
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:RequestJitterBufferPorts - getChildNodeContainer Failed"));
        return false;
    }

    numPortsRequested = 0;
    /*
     * Request port - all jitter buffer input ports
     * are even numbered and output and rtcp ports are odd numbered
     */
    int32 portTagStart = portType;

    if ((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL)
            || (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE)
       )
    {
        for (uint32 i = 0; i < iTrackInfoVec.size(); i++)
        {
            PVMFRTSPTrackInfo trackInfo = iTrackInfoVec[i];

            PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
            if (internalCmd != NULL)
            {
                internalCmd->cmd =
                    nodeContainer->commandStartOffset +
                    PVMF_SM_FSP_NODE_INTERNAL_REQUEST_PORT_OFFSET;
                internalCmd->parentCmd = PVMF_SMFSP_NODE_CONSTRUCT_SESSION;
                internalCmd->portContext.trackID = trackInfo.trackID;
                internalCmd->portContext.portTag = portType;

                OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                PVMFNodeInterface* iNode = nodeContainer->iNode;

                OSCL_StackString<32> portConfig = trackInfo.iTransportType;
                portConfig += _STRLIT_CHAR("/");
                portConfig += trackInfo.iMimeType;
                iNode->RequestPort(nodeContainer->iSessionId,
                                   portTagStart,
                                   &(portConfig),
                                   cmdContextData);
                numPortsRequested++;
                nodeContainer->iNumRequestPortsPending++;
                nodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
            }
            else
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:RequestJitterBufferPorts - RequestNewInternalCmd Failed"));
                return false;
            }
            portTagStart += 3;
        }
        return true;
    }
    return false;
}

void PVMFSMRTSPUnicastNode::DoPrepare(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoPrepare - In"));
    switch (iInterfaceState)
    {
        case EPVMFNodeInitialized:
        {
            if (iGraphConstructComplete)
            {
                /*
                 * Connect the graph here. This is needed since we would send firewall packets
                 * as part of Prepare.
                 */
                if (GraphConnect() == false)
                {
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoPrepare - GraphConnect Failed"));
                    SetState(EPVMFNodeError);
                    PVUuid eventuuid = PVMFStreamingManagerNodeEventTypeUUID;
                    int32 errcode = PVMFStreamingManagerNodeGraphConnectFailed;
                    CommandComplete(iInputCommands, aCmd, PVMFFailure, NULL, &eventuuid, &errcode);
                    return;
                }

                /*
                 * Prepare for streaming manager cannot be completed unless Prepare
                 * for all the children nodes are complete
                 */
                PVMFSMFSPChildNodeContainerVector::iterator it;
                for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
                {
                    PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
                    if (internalCmd != NULL)
                    {
                        internalCmd->cmd =
                            it->commandStartOffset +
                            PVMF_SM_FSP_NODE_INTERNAL_PREPARE_CMD_OFFSET;
                        internalCmd->parentCmd = aCmd.iCmd;

                        OsclAny *cmdContextData =
                            OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                        PVMFNodeInterface* iNode = it->iNode;

                        iNode->Prepare(it->iSessionId, cmdContextData);
                        it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
                    }
                    else
                    {
                        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoPrepare:RequestNewInternalCmd - Failed"));
                        CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
                        return;
                    }
                }

                MoveCmdToCurrentQueue(aCmd);
            }
            else
            {
                /* Graph construction not complete, so cant prep */
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoPrepare Failed - Incomplete Graph"));
                CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
            }
        }
        break;

        default:
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoPrepare Failed - Invalid State"));
            CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
            break;
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoPrepare - Out"));
}

bool PVMFSMRTSPUnicastNode::GraphConnect()
{
    if (iGraphConnectComplete == false)
    {
        /*
         * Go over the track list and connect:
         * network_node_port -> jitter_buffer_node_input_port;
         */
        PVMFStatus status;
        for (uint32 i = 0; i < iTrackInfoVec.size(); i++)
        {
            PVMFRTSPTrackInfo trackInfo = iTrackInfoVec[i];

            if ((trackInfo.iNetworkNodePort == NULL) ||
                    (trackInfo.iNetworkNodeRTCPPort == NULL) ||
                    (trackInfo.iJitterBufferInputPort == NULL) ||
                    (trackInfo.iJitterBufferOutputPort == NULL) ||
                    (trackInfo.iJitterBufferRTCPPort == NULL))
            {
                PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:GraphConnectFor3GPPUDPStreaming - Invalid Ports"));
                return false;
            }

            PVMF_SM_RTSP_LOGINFO((0, "PVMFSM:GraphConnect - Track MimeType %s", trackInfo.iMimeType.get_cstr()));

            /* connect network_node_port <-> jitter_buffer_node_input_port */
            status = ConnectPortPairs(trackInfo.iJitterBufferInputPort,
                                      trackInfo.iNetworkNodePort);

            PVMF_SM_RTSP_LOGINFO((0, "PVMFSM:GraphConnectFor3GPPUDPStreaming - Connected Network - JB Input"));
            PVMF_SM_RTSP_LOGINFO((0, "PVMFSM:GraphConnectFor3GPPUDPStreaming - NetworkPort=0x%x - JBInputPort=0x%x", trackInfo.iNetworkNodePort, trackInfo.iJitterBufferInputPort));

            if (status != PVMFSuccess)
            {
                return false;
            }

            /*
             * connect network_rtcp_port <-> jitter_buffer_rtcp_port
             */
            status = ConnectPortPairs(trackInfo.iJitterBufferRTCPPort,
                                      trackInfo.iNetworkNodeRTCPPort);

            PVMF_SM_RTSP_LOGINFO((0, "PVMFSM:GraphConnectFor3GPPUDPStreaming - NetworkRTCPPort - JBRTCPPort"));
            PVMF_SM_RTSP_LOGINFO((0, "PVMFSM:GraphConnectFor3GPPUDPStreaming - NetworkRTCPPort=0x%x - JBRTCPPort=0x%x", trackInfo.iNetworkNodeRTCPPort, trackInfo.iJitterBufferRTCPPort));

            if (status != PVMFSuccess)
            {
                return false;
            }
        }
        iGraphConnectComplete = true;
    }
    return true;
}

PVMFStatus PVMFSMRTSPUnicastNode::ConnectPortPairs(PVMFPortInterface* aPort1,
        PVMFPortInterface* aPort2)
{
    PVMFStatus status;

    status = aPort1->Connect(aPort2);

    if (status != PVMFSuccess)
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ConnectPortPairs - Connect Failed"));
        return status;
    }

    return status;
}

void PVMFSMRTSPUnicastNode::DoRequestPort(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoRequestPort - In"));
    /*
     * This node supports port request only after the graph
     * has been fully constructed
     */
    if (iGraphConstructComplete)
    {
        /*
         * retrieve port tag
         */
        OSCL_String* mimetype;
        int32 tag;
        aCmd.PVMFSMFSPBaseNodeCommandBase::Parse(tag, mimetype);
        /*
         * Do not Allocate a new port. RTSP unicast node treats the output
         * port from the jitter buffer as its own output port. Find the jitter
         * buffer output port corresponding to the input mimetype and hand the
         * same out
         */
        PVMFRTSPTrackInfo* trackInfo = FindTrackInfo(tag);

        PVUuid eventuuid = PVMFStreamingManagerNodeEventTypeUUID;
        int32 errcode = PVMFStreamingManagerNodeErrorInvalidRequestPortTag;

        if (trackInfo == NULL)
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::DoRequestPort: FindTrackInfo failed"));
            CommandComplete(iInputCommands, aCmd, PVMFErrArgument, NULL, &eventuuid, &errcode);
            return;
        }
        if (trackInfo->iJitterBufferOutputPort == NULL)
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::DoRequestPort: iJitterBufferOutputPort NULL"));
            CommandComplete(iInputCommands, aCmd, PVMFFailure, NULL, &eventuuid, &errcode);
            return;
        }
        PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::DoRequestPort() - CmdComplete - PVMFSuccess"));
        /*
         * Return the port pointer to the caller.
         */
        CommandComplete(iInputCommands,
                        aCmd,
                        PVMFSuccess,
                        (OsclAny*)(trackInfo->iJitterBufferOutputPort));

        PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::DoRequestPort Success"));
    }
    else
    {
        PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::RequestPort() - CmdFailed - PVMFErrInvalidState"));
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::DoRequestPort Failed - InvalidState"));
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoRequestPort - Out"));
}

void PVMFSMRTSPUnicastNode::DoReleasePort(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoReleasePort - In"));
    /*
     * Since the streaming manager does not have ports of its own,
     * a release port command typically translates to disconnecting
     * the underlying jitter buffer port.
     */
    PVMFPortInterface* port;
    aCmd.PVMFSMFSPBaseNodeCommandBase::Parse((PVMFPortInterface*&)port);

    /*
     * Find TrackInfo that corresponds to the Jitter Buffer Output port
     * on which the current relase is being called.
     */
    PVMFRTSPTrackInfoVector::iterator it;
    PVMFRTSPTrackInfo* trackInfo = NULL;

    for (it = iTrackInfoVec.begin();
            it != iTrackInfoVec.end();
            it++)
    {
        if (it->iJitterBufferOutputPort == port)
        {
            trackInfo = it;
            break;
        }
    }

    PVUuid eventuuid = PVMFStreamingManagerNodeEventTypeUUID;
    if (trackInfo == NULL)
    {
        PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::ReleasePort() - CmdFailed - PVMFErrArgument"));
        /* invalid port */
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::DoReleasePort Failed - Invalid Port"));
        int32 errcode = PVMFStreamingManagerNodeErrorInvalidPort;
        CommandComplete(iInputCommands, aCmd, PVMFErrArgument, NULL, &eventuuid, &errcode);
        return;
    }
    PVMFStatus status = it->iJitterBufferOutputPort->Disconnect();

    if (status != PVMFSuccess)
    {
        PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::DoReleasePort Success"));
        CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    }
    else
    {
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::DoReleasePort Failed"));
        CommandComplete(iInputCommands, aCmd, PVMFErrPortProcessing);
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoReleasePort - Out"));
}

void PVMFSMRTSPUnicastNode::DoStart(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoStart - In"));
    switch (iInterfaceState)
    {
        case EPVMFNodePrepared:
        {
            /*
             * Connect the graph if not already connected. Usually the graph is
             * disconnected as part of Stop. In case we are doing a start after
             * stop, we would need to connect the graph again.
             */
            if (GraphConnect() == false)
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:CompleteStart - GraphConnect Failed"));
                SetState(EPVMFNodeError);
                PVUuid eventuuid = PVMFStreamingManagerNodeEventTypeUUID;
                int32 errcode = PVMFStreamingManagerNodeGraphConnectFailed;
                CommandComplete(iInputCommands, aCmd, PVMFFailure, NULL, &eventuuid, &errcode);
                return;
            }

            bool isSessionDurationExpired = false;

            PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            if (jitterBufferNodeContainer)
            {
                PVMFJitterBufferExtensionInterface* jbExtIntf =
                    OSCL_STATIC_CAST(PVMFJitterBufferExtensionInterface*, jitterBufferNodeContainer->iExtensions.front());
                if (jbExtIntf)
                    jbExtIntf->HasSessionDurationExpired(isSessionDurationExpired);
            }

            PVMFSMFSPChildNodeContainer *sessionControllerNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
            if (sessionControllerNodeContainer)
            {
                PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                    OSCL_STATIC_CAST(PVRTSPEngineNodeExtensionInterface*, sessionControllerNodeContainer->iExtensions.front());
                if (rtspExtIntf)
                    rtspExtIntf->UpdateSessionCompletionStatus(isSessionDurationExpired);
            }

            /*
             * Start for streaming manager cannot be completed unless
             * Start for all the children nodes are complete
             */
            PVMFSMFSPChildNodeContainerVector::iterator it;
            for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
            {
                PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
                if (internalCmd != NULL)
                {
                    internalCmd->cmd =
                        it->commandStartOffset +
                        PVMF_SM_FSP_NODE_INTERNAL_START_CMD_OFFSET;
                    internalCmd->parentCmd = aCmd.iCmd;

                    OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                    PVMFNodeInterface* iNode = it->iNode;

                    iNode->Start(it->iSessionId, cmdContextData);
                    it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
                }
                else
                {
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoStart:RequestNewInternalCmd - Failed"));
                    CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
                    return;
                }
            }

            MoveCmdToCurrentQueue(aCmd);
        }
        break;

        /*
         * GraphConnect() not needed if starting from a paused state
         */
        case EPVMFNodePaused:
        {
            bool isSessionDurationExpired = false;

            PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            if (jitterBufferNodeContainer)
            {
                PVMFJitterBufferExtensionInterface* jbExtIntf =
                    OSCL_STATIC_CAST(PVMFJitterBufferExtensionInterface*, jitterBufferNodeContainer->iExtensions.front());
                if (jbExtIntf)
                    jbExtIntf->HasSessionDurationExpired(isSessionDurationExpired);
            }

            PVMFSMFSPChildNodeContainer *sessionControllerNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
            if (sessionControllerNodeContainer)
            {
                PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                    OSCL_STATIC_CAST(PVRTSPEngineNodeExtensionInterface*, sessionControllerNodeContainer->iExtensions.front());
                if (rtspExtIntf)
                    rtspExtIntf->UpdateSessionCompletionStatus(isSessionDurationExpired);
            }
            /*
             * Start for streaming manager cannot be completed unless
             * Start for all the children nodes are complete
             */
            PVMFSMFSPChildNodeContainerVector::iterator it;
            for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
            {
                if (it->iAutoPaused == false)
                {
                    PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
                    if (internalCmd != NULL)
                    {
                        internalCmd->cmd =
                            it->commandStartOffset +
                            PVMF_SM_FSP_NODE_INTERNAL_START_CMD_OFFSET;
                        internalCmd->parentCmd = aCmd.iCmd;

                        OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                        PVMFNodeInterface* iNode = it->iNode;

                        iNode->Start(it->iSessionId, cmdContextData);
                        it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
                    }
                    else
                    {
                        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoStart:RequestNewInternalCmd - Failed"));
                        CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
                        return;
                    }
                }
            }
            MoveCmdToCurrentQueue(aCmd);
        }
        break;

        case EPVMFNodeStarted:
            //Ignore start if already started
            CommandComplete(iInputCommands, aCmd, PVMFSuccess);
            break;

        default:
            CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
            break;
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoStart - Out"));
}

void PVMFSMRTSPUnicastNode::DoStop(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoStop - In"));
    iStreamID = 0;
    switch (iInterfaceState)
    {
        case EPVMFNodeStarted:
        case EPVMFNodePaused:
        {
            /*
             * Stop for streaming manager cannot be completed unless
             * Stop for all the children nodes are complete
             */
            PVMFSMFSPChildNodeContainerVector::iterator it;
            for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
            {
                PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
                if (internalCmd != NULL)
                {
                    internalCmd->cmd =
                        it->commandStartOffset +
                        PVMF_SM_FSP_NODE_INTERNAL_STOP_CMD_OFFSET;
                    internalCmd->parentCmd = aCmd.iCmd;

                    OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                    PVMFNodeInterface* iNode = it->iNode;

                    iNode->Stop(it->iSessionId, cmdContextData);
                    it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
                }
                else
                {
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoStop:RequestNewInternalCmd - Failed"));
                    CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
                    return;
                }
            }
            MoveCmdToCurrentQueue(aCmd);
        }
        break;

        default:
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::DoStop Failure - Invalid State"));
            CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
            break;
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoStop - Out"));
}

void PVMFSMRTSPUnicastNode::DoPause(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoPause - In"));
    switch (iInterfaceState)
    {
        case EPVMFNodeStarted:
        {
            bool isSessionDurationExpired = false;

            PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            if (jitterBufferNodeContainer)
            {
                PVMFJitterBufferExtensionInterface* jbExtIntf =
                    OSCL_STATIC_CAST(PVMFJitterBufferExtensionInterface*, jitterBufferNodeContainer->iExtensions.front());
                if (jbExtIntf)
                    jbExtIntf->HasSessionDurationExpired(isSessionDurationExpired);
            }

            PVMFSMFSPChildNodeContainer *sessionControllerNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
            if (sessionControllerNodeContainer)
            {
                PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                    OSCL_STATIC_CAST(PVRTSPEngineNodeExtensionInterface*, sessionControllerNodeContainer->iExtensions.front());
                if (rtspExtIntf)
                    rtspExtIntf->UpdateSessionCompletionStatus(isSessionDurationExpired);
            }

            PVMFSMFSPChildNodeContainerVector::iterator it;
            for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
            {
                /*
                 * Pause only if not already paused - could happen that
                 * some of the nodes could be paused due to flow control
                 */
                if ((it->iNode->GetState()) != EPVMFNodePaused)
                {
                    PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
                    if (internalCmd != NULL)
                    {
                        internalCmd->cmd =
                            it->commandStartOffset +
                            PVMF_SM_FSP_NODE_INTERNAL_PAUSE_CMD_OFFSET;
                        internalCmd->parentCmd = aCmd.iCmd;

                        OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                        PVMFNodeInterface* iNode = it->iNode;

                        iNode->Pause(it->iSessionId, cmdContextData);
                        it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
                    }
                    else
                    {
                        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoPause:RequestNewInternalCmd - Failed"));
                        CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
                        return;
                    }
                }
            }
            MoveCmdToCurrentQueue(aCmd);
        }
        break;
        default:
            CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
            break;
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoPause - Out"));
    return;
}

PVMFRTSPTrackInfo* PVMFSMRTSPUnicastNode::FindTrackInfo(uint32 atrackID)
{
    PVMFRTSPTrackInfoVector::iterator it;

    for (it = iTrackInfoVec.begin();
            it != iTrackInfoVec.end();
            it++)
    {
        if (it->trackID == atrackID)
        {
            return (it);
        }
    }
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//Implemenation of pure virtuals from PvmiCapabilityAndConfig interface
///////////////////////////////////////////////////////////////////////////////
void PVMFSMRTSPUnicastNode::setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
{
    ciObserver = aObserver;
}

PVMFStatus PVMFSMRTSPUnicastNode::getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier,
        PvmiKvp*& aParameters, int& aNumParamElements,
        PvmiCapabilityContext aContext)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);

    // Initialize the output parameters
    aNumParamElements = 0;
    aParameters = NULL;

    // Count the number of components and parameters in the key
    int compcount = pv_mime_string_compcnt(aIdentifier);
    // Retrieve the first component from the key string
    char* compstr = NULL;
    pv_mime_string_extract_type(0, aIdentifier, compstr);

    if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf")) < 0) || compcount < 2)
    {
        // First component should be "x-pvmf" and there must
        // be at least two components to go past x-pvmf
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFSMRTSPUnicastNode::getParametersSync() Invalid key string"));
        return PVMFErrArgument;
    }

    // Retrieve the second component from the key string
    pv_mime_string_extract_type(1, aIdentifier, compstr);

    // Check if it is key string for streaming manager
    if (pv_mime_strcmp(compstr, _STRLIT_CHAR("net")) < 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFSMRTSPUnicastNode::getParametersSync() Unsupported key"));
        return PVMFFailure;
    }


    if (compcount == 2)
    {
        // Since key is "x-pvmf/net" return all
        // nodes available at this level. Ignore attribute
        // since capability is only allowed

        // Allocate memory for the KVP list
        aParameters = (PvmiKvp*)oscl_malloc(StreamingManagerConfig_NumBaseKeys * sizeof(PvmiKvp));
        if (aParameters == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFSMRTSPUnicastNode::getParametersSync() Memory allocation for KVP failed"));
            return PVMFErrNoMemory;
        }
        oscl_memset(aParameters, 0, StreamingManagerConfig_NumBaseKeys*sizeof(PvmiKvp));
        // Allocate memory for the key strings in each KVP
        PvmiKeyType memblock = (PvmiKeyType)oscl_malloc(StreamingManagerConfig_NumBaseKeys * SMCONFIG_KEYSTRING_SIZE * sizeof(char));
        if (memblock == NULL)
        {
            oscl_free(aParameters);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFSMRTSPUnicastNode::getParametersSync() Memory allocation for key string failed"));
            return PVMFErrNoMemory;
        }
        oscl_strset(memblock, 0, StreamingManagerConfig_NumBaseKeys*SMCONFIG_KEYSTRING_SIZE*sizeof(char));
        // Assign the key string buffer to each KVP
        uint32 j;
        for (j = 0; j < StreamingManagerConfig_NumBaseKeys; ++j)
        {
            aParameters[j].key = memblock + (j * SMCONFIG_KEYSTRING_SIZE);
        }
        // Copy the requested info
        for (j = 0; j < StreamingManagerConfig_NumBaseKeys; ++j)
        {
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR("x-pvmf/net/"), 17);
            oscl_strncat(aParameters[j].key, StreamingManagerConfig_BaseKeys[j].iString, oscl_strlen(StreamingManagerConfig_BaseKeys[j].iString));
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(";type="), 6);
            switch (StreamingManagerConfig_BaseKeys[j].iType)
            {
                case PVMI_KVPTYPE_AGGREGATE:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_AGGREGATE_STRING), oscl_strlen(PVMI_KVPTYPE_AGGREGATE_STRING));
                    break;

                case PVMI_KVPTYPE_POINTER:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_POINTER_STRING), oscl_strlen(PVMI_KVPTYPE_POINTER_STRING));
                    break;

                case PVMI_KVPTYPE_VALUE:
                default:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_VALUE_STRING), oscl_strlen(PVMI_KVPTYPE_VALUE_STRING));
                    break;
            }
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(";valtype="), 9);
            switch (StreamingManagerConfig_BaseKeys[j].iValueType)
            {
                case PVMI_KVPVALTYPE_RANGE_INT32:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_RANGE_INT32_STRING), oscl_strlen(PVMI_KVPVALTYPE_RANGE_INT32_STRING));
                    break;

                case PVMI_KVPVALTYPE_KSV:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_KSV_STRING), oscl_strlen(PVMI_KVPVALTYPE_KSV_STRING));
                    break;

                case PVMI_KVPVALTYPE_CHARPTR:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_CHARPTR_STRING), oscl_strlen(PVMI_KVPVALTYPE_CHARPTR_STRING));
                    break;

                case PVMI_KVPVALTYPE_BOOL:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_BOOL_STRING), oscl_strlen(PVMI_KVPVALTYPE_BOOL_STRING));
                    break;

                case PVMI_KVPVALTYPE_UINT32:
                default:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_UINT32_STRING), oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING));
                    break;
            }
            aParameters[j].key[SMCONFIG_KEYSTRING_SIZE-1] = 0;
        }

        aNumParamElements = StreamingManagerConfig_NumBaseKeys;
    }
    else if (compcount == 3)
    {
        pv_mime_string_extract_type(2, aIdentifier, compstr);

        // Determine what is requested
        PvmiKvpAttr reqattr = GetAttrTypeFromKeyString(aIdentifier);
        if (reqattr == PVMI_KVPATTR_UNKNOWN)
        {
            reqattr = PVMI_KVPATTR_CUR;
        }
        uint i;
        for (i = 0; i < StreamingManagerConfig_NumBaseKeys; i++)
        {
            if (pv_mime_strcmp(compstr, (char*)(StreamingManagerConfig_BaseKeys[i].iString)) >= 0)
            {
                break;
            }
        }

        if (i == StreamingManagerConfig_NumBaseKeys)
        {
            // no match found
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFSMRTSPUnicastNode::getParametersSync() Unsupported key"));
            return PVMFErrNoMemory;
        }

        PVMFStatus retval = GetConfigParameter(aParameters, aNumParamElements, i, reqattr);
        if (retval != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFSMRTSPUnicastNode::getParametersSync() "
                             "Retrieving streaming manager parameter failed"));
            return retval;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFSMRTSPUnicastNode::getParametersSync() Unsupported key"));
        return PVMFErrArgument;
    }

    return PVMFSuccess;
}

PVMFStatus PVMFSMRTSPUnicastNode::GetConfigParameter(PvmiKvp*& aParameters,
        int& aNumParamElements,
        int32 aIndex, PvmiKvpAttr reqattr)
{
    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::GetConfigParameter() In"));

    aNumParamElements = 0;

    // Allocate memory for the KVP
    aParameters = (PvmiKvp*)oscl_malloc(sizeof(PvmiKvp));
    if (aParameters == NULL)
    {
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::GetConfigParameter() Memory allocation for KVP failed"));
        return PVMFErrNoMemory;
    }
    oscl_memset(aParameters, 0, sizeof(PvmiKvp));

    // Allocate memory for the key string in KVP
    PvmiKeyType memblock = (PvmiKeyType)oscl_malloc(SMCONFIG_KEYSTRING_SIZE * sizeof(char));
    if (memblock == NULL)
    {
        oscl_free(aParameters);
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::GetConfigParameter() Memory allocation for key string failed"));
        return PVMFErrNoMemory;
    }
    oscl_strset(memblock, 0, SMCONFIG_KEYSTRING_SIZE*sizeof(char));

    // Assign the key string buffer to KVP
    aParameters[0].key = memblock;

    // Copy the key string
    oscl_strncat(aParameters[0].key, _STRLIT_CHAR("x-pvmf/net/"), 17);
    oscl_strncat(aParameters[0].key, StreamingManagerConfig_BaseKeys[aIndex].iString,
                 oscl_strlen(StreamingManagerConfig_BaseKeys[aIndex].iString));
    oscl_strncat(aParameters[0].key, _STRLIT_CHAR(";type=value;valtype="), 20);
    switch (StreamingManagerConfig_BaseKeys[aIndex].iValueType)
    {
        case PVMI_KVPVALTYPE_RANGE_INT32:
            oscl_strncat(aParameters[0].key,
                         _STRLIT_CHAR(PVMI_KVPVALTYPE_RANGE_INT32_STRING),
                         oscl_strlen(PVMI_KVPVALTYPE_RANGE_INT32_STRING));
            break;

        case PVMI_KVPVALTYPE_KSV:
            oscl_strncat(aParameters[0].key,
                         _STRLIT_CHAR(PVMI_KVPVALTYPE_KSV_STRING),
                         oscl_strlen(PVMI_KVPVALTYPE_KSV_STRING));
            break;

        case PVMI_KVPVALTYPE_CHARPTR:
            oscl_strncat(aParameters[0].key,
                         _STRLIT_CHAR(PVMI_KVPVALTYPE_CHARPTR_STRING),
                         oscl_strlen(PVMI_KVPVALTYPE_CHARPTR_STRING));
            break;

        case PVMI_KVPVALTYPE_WCHARPTR:
            oscl_strncat(aParameters[0].key,
                         _STRLIT_CHAR(PVMI_KVPVALTYPE_WCHARPTR_STRING),
                         oscl_strlen(PVMI_KVPVALTYPE_WCHARPTR_STRING));
            break;

        case PVMI_KVPVALTYPE_BOOL:
            oscl_strncat(aParameters[0].key,
                         _STRLIT_CHAR(PVMI_KVPVALTYPE_BOOL_STRING),
                         oscl_strlen(PVMI_KVPVALTYPE_BOOL_STRING));
            break;

        case PVMI_KVPVALTYPE_UINT32:
        default:
            if (reqattr == PVMI_KVPATTR_CAP)
            {
                oscl_strncat(aParameters[0].key,
                             _STRLIT_CHAR(PVMI_KVPVALTYPE_RANGE_UINT32_STRING),
                             oscl_strlen(PVMI_KVPVALTYPE_RANGE_UINT32_STRING));
            }
            else
            {
                oscl_strncat(aParameters[0].key,
                             _STRLIT_CHAR(PVMI_KVPVALTYPE_UINT32_STRING),
                             oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING));
            }
            break;
    }
    aParameters[0].key[SMCONFIG_KEYSTRING_SIZE-1] = 0;

    // Copy the requested info
    switch (aIndex)
    {
        case BASEKEY_DELAY:
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
                OSCL_ASSERT(iJitterBufferNodeContainer);
                if (!iJitterBufferNodeContainer)
                    return PVMFFailure;
                PVMFJitterBufferExtensionInterface* jbExtIntf =
                    (PVMFJitterBufferExtensionInterface*)iJitterBufferNodeContainer->iExtensions[0];
                if (!jbExtIntf)
                    return PVMFFailure;
                jbExtIntf->getJitterBufferDurationInMilliSeconds(aParameters[0].value.uint32_value);
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.uint32_value = DEFAULT_JITTER_BUFFER_DURATION_IN_MS;
            }
            else
            {
                // Return capability
                range_uint32* rui32 = (range_uint32*)oscl_malloc(sizeof(range_uint32));
                if (rui32 == NULL)
                {
                    oscl_free(aParameters[0].key);
                    oscl_free(aParameters);
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::GetConfigParameter() "
                                           "Memory allocation for range uint32 failed"));
                    return PVMFErrNoMemory;
                }
                rui32->min = MIN_JITTER_BUFFER_DURATION_IN_MS;
                rui32->max = MAX_JITTER_BUFFER_DURATION_IN_MS;
                aParameters[0].value.key_specific_value = (void*)rui32;
            }
            break;
        case BASEKEY_REBUFFERING_THRESHOLD:
        {
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
                OSCL_ASSERT(iJitterBufferNodeContainer);
                if (!iJitterBufferNodeContainer)
                    return PVMFFailure;
                PVMFJitterBufferExtensionInterface* jbExtIntf =
                    (PVMFJitterBufferExtensionInterface*)iJitterBufferNodeContainer->iExtensions[0];
                OSCL_ASSERT(jbExtIntf);
                if (!jbExtIntf)
                    return PVMFFailure;
                jbExtIntf->getJitterBufferRebufferingThresholdInMilliSeconds(aParameters[0].value.uint32_value);
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.uint32_value = DEFAULT_JITTER_BUFFER_UNDERFLOW_THRESHOLD_IN_MS;
            }
            else
            {
                // Return capability
                range_uint32* rui32 = (range_uint32*)oscl_malloc(sizeof(range_uint32));
                if (rui32 == NULL)
                {
                    oscl_free(aParameters[0].key);
                    oscl_free(aParameters);
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::GetConfigParameter() "
                                           "Memory allocation for range uint32 failed"));
                    return PVMFErrNoMemory;
                }
                rui32->min = DEFAULT_JITTER_BUFFER_UNDERFLOW_THRESHOLD_IN_MS;
                rui32->max = DEFAULT_JITTER_BUFFER_UNDERFLOW_THRESHOLD_IN_MS;
                aParameters[0].value.key_specific_value = (void*)rui32;
            }
        }
        break;
        case BASEKEY_JITTERBUFFER_MAX_INACTIVITY_DURATION:
            if (reqattr == PVMI_KVPATTR_CUR)
            {
                // Return current value
                PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
                OSCL_ASSERT(iJitterBufferNodeContainer);
                if (!iJitterBufferNodeContainer)
                    return PVMFFailure;
                PVMFJitterBufferExtensionInterface* jbExtIntf =
                    (PVMFJitterBufferExtensionInterface*)iJitterBufferNodeContainer->iExtensions[0];
                OSCL_ASSERT(jbExtIntf);
                uint32 inactivityDuration = 0;
                if (jbExtIntf)
                {
                    jbExtIntf->getMaxInactivityDurationForMediaInMs(inactivityDuration);
                }
                aParameters[0].value.uint32_value = inactivityDuration;
            }
            else if (reqattr == PVMI_KVPATTR_DEF)
            {
                // Return default
                aParameters[0].value.uint32_value = DEFAULT_MAX_INACTIVITY_DURATION_IN_MS;
            }
            else
            {
                // Return capability
                range_uint32* rui32 = (range_uint32*)oscl_malloc(sizeof(range_uint32));
                if (rui32 == NULL)
                {
                    oscl_free(aParameters[0].key);
                    oscl_free(aParameters);
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::GetConfigParameter() "
                                           "Memory allocation for range uint32 failed"));
                    return PVMFErrNoMemory;
                }
                rui32->min = 0;
                rui32->max = UINT32_MAX;//max of 32 bit
                aParameters[0].value.key_specific_value = (void*)rui32;
            }
            break;
        case BASEKEY_SESSION_CONTROLLER_USER_AGENT:
            if ((reqattr == PVMI_KVPATTR_CUR) || (reqattr == PVMI_KVPATTR_DEF))
            {
                aParameters[0].value.pWChar_value = NULL;
                /* As of now just RTSP node supports an external config of user agent */
                PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
                if (iSessionControllerNodeContainer != NULL)
                {
                    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                        (PVRTSPEngineNodeExtensionInterface*)
                        (iSessionControllerNodeContainer->iExtensions[0]);

                    OSCL_wHeapString<OsclMemAllocator> userAgent;
                    if (rtspExtIntf->GetUserAgent(userAgent) == PVMFSuccess)
                    {
                        // Return current value
                        oscl_wchar* ptr = (oscl_wchar*)oscl_malloc(sizeof(oscl_wchar) * (userAgent.get_size()));
                        if (ptr)
                        {
                            oscl_memcpy(ptr, userAgent.get_cstr(), userAgent.get_size());
                            aParameters[0].value.pWChar_value = ptr;
                        }
                        else
                        {
                            oscl_free(aParameters[0].key);
                            oscl_free(aParameters);
                            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::GetConfigParameter() "
                                                   "Memory allocation for user agent failed"));
                            return PVMFErrNoMemory;
                        }
                    }
                }
            }
            else
            {
                // Return capability - no concept of capability for user agent
                // do nothing
            }
            break;
        case BASEKEY_SESSION_CONTROLLER_KEEP_ALIVE_INTERVAL:
        case BASEKEY_SESSION_CONTROLLER_KEEP_ALIVE_DURING_PLAY:
            if ((reqattr == PVMI_KVPATTR_CUR) || (reqattr == PVMI_KVPATTR_DEF))
            {
                if (aIndex == BASEKEY_SESSION_CONTROLLER_KEEP_ALIVE_INTERVAL)
                {
                    aParameters[0].value.uint32_value = PVRTSPENGINENODE_DEFAULT_KEEP_ALIVE_INTERVAL;
                }
                else
                {
                    aParameters[0].value.bool_value = false;
                }
                /* As of now just RTSP node supports an external config of user agent */
                PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
                if (iSessionControllerNodeContainer != NULL)
                {
                    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                        (PVRTSPEngineNodeExtensionInterface*)
                        (iSessionControllerNodeContainer->iExtensions[0]);
                    uint32 timeout;
                    bool okeepalivemethod;
                    bool okeepaliveinplay;
                    rtspExtIntf->GetKeepAliveMethod((int32&)timeout, okeepalivemethod, okeepaliveinplay);
                    if (aIndex == BASEKEY_SESSION_CONTROLLER_KEEP_ALIVE_INTERVAL)
                    {
                        aParameters[0].value.uint32_value = timeout;
                    }
                    else
                    {
                        aParameters[0].value.bool_value = okeepaliveinplay;
                    }
                }
            }
            else
            {
                // Return capability - no concept of capability for keep alive interval
                // do nothing
            }
            break;

        case BASEKEY_SESSION_CONTROLLER_RTSP_TIMEOUT:
        {
            if ((reqattr == PVMI_KVPATTR_CUR) || (reqattr == PVMI_KVPATTR_DEF))
            {
                /* As of now just RTSP node supports an external config of RTSP time out */
                PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
                if (iSessionControllerNodeContainer != NULL)
                {
                    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                        (PVRTSPEngineNodeExtensionInterface*)
                        (iSessionControllerNodeContainer->iExtensions[0]);
                    int32 timeout;
                    rtspExtIntf->GetRTSPTimeOut(timeout);
                    aParameters[0].value.uint32_value = OSCL_STATIC_CAST(uint32, timeout);
                }
            }
            else
            {
                // Return capability
                range_uint32* rui32 = (range_uint32*)oscl_malloc(sizeof(range_uint32));
                if (rui32 == NULL)
                {
                    oscl_free(aParameters[0].key);
                    oscl_free(aParameters);
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::GetConfigParameter() "
                                           "Memory allocation for range uint32 failed"));
                    return PVMFErrNoMemory;
                }
                rui32->min = MIN_RTSP_SERVER_INACTIVITY_TIMEOUT_IN_SEC;
                rui32->max = MAX_RTSP_SERVER_INACTIVITY_TIMEOUT_IN_SEC;
                aParameters[0].value.key_specific_value = (void*)rui32;
            }
        }
        break;
        case BASEKEY_DISABLE_FIREWALL_PACKETS:
        {
            if ((reqattr == PVMI_KVPATTR_CUR) || (reqattr == PVMI_KVPATTR_DEF))
            {
                aParameters[0].value.bool_value = false;
            }
        }
        break;
        default:
            // Invalid index
            oscl_free(aParameters[0].key);
            oscl_free(aParameters);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVPlayerEngine::DoGetPlayerParameter() Invalid index to player parameter"));
            return PVMFErrArgument;
    }

    aNumParamElements = 1;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVPlayerEngine::DoGetPlayerParameter() Out"));
    return PVMFSuccess;
}

PVMFStatus PVMFSMRTSPUnicastNode::releaseParameters(PvmiMIOSession aSession,
        PvmiKvp* aParameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(aSession);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFSMRTSPUnicastNode::releaseParameters() In"));

    if (aParameters == NULL || num_elements < 1)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFSMRTSPUnicastNode::releaseParameters() KVP list is NULL or number of elements is 0"));
        return PVMFErrArgument;
    }

    // Count the number of components and parameters in the key
    int compcount = pv_mime_string_compcnt(aParameters[0].key);
    // Retrieve the first component from the key string
    char* compstr = NULL;
    pv_mime_string_extract_type(0, aParameters[0].key, compstr);

    if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf")) < 0) || compcount < 2)
    {
        // First component should be "x-pvmf" and there must
        // be at least two components to go past x-pvmf
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFSMRTSPUnicastNode::releaseParameters() Unsupported key"));
        return PVMFErrArgument;
    }

    // Retrieve the second component from the key string
    pv_mime_string_extract_type(1, aParameters[0].key, compstr);

    // Assume all the parameters come from the same component so the base components are the same
    if (pv_mime_strcmp(compstr, _STRLIT_CHAR("net")) >= 0)
    {
        // Go through each KVP and release memory for value if allocated from heap
        for (int32 i = 0; i < num_elements; ++i)
        {
            // Next check if it is a value type that allocated memory
            PvmiKvpType kvptype = GetTypeFromKeyString(aParameters[i].key);
            if (kvptype == PVMI_KVPTYPE_VALUE || kvptype == PVMI_KVPTYPE_UNKNOWN)
            {
                PvmiKvpValueType keyvaltype = GetValTypeFromKeyString(aParameters[i].key);
                if (keyvaltype == PVMI_KVPVALTYPE_UNKNOWN)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFSMRTSPUnicastNode::releaseParameters() Valtype not specified in key string"));
                    return PVMFErrArgument;
                }

                if (keyvaltype == PVMI_KVPVALTYPE_CHARPTR && aParameters[i].value.pChar_value != NULL)
                {
                    oscl_free(aParameters[i].value.pChar_value);
                    aParameters[i].value.pChar_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_WCHARPTR && aParameters[i].value.pWChar_value != NULL)
                {
                    oscl_free(aParameters[i].value.pWChar_value);
                    aParameters[i].value.pWChar_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_CHARPTR && aParameters[i].value.pChar_value != NULL)
                {
                    oscl_free(aParameters[i].value.pChar_value);
                    aParameters[i].value.pChar_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_KSV && aParameters[i].value.key_specific_value != NULL)
                {
                    oscl_free(aParameters[i].value.key_specific_value);
                    aParameters[i].value.key_specific_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_RANGE_INT32 && aParameters[i].value.key_specific_value != NULL)
                {
                    range_int32* ri32 = (range_int32*)aParameters[i].value.key_specific_value;
                    aParameters[i].value.key_specific_value = NULL;
                    oscl_free(ri32);
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_RANGE_UINT32 && aParameters[i].value.key_specific_value != NULL)
                {
                    range_uint32* rui32 = (range_uint32*)aParameters[i].value.key_specific_value;
                    aParameters[i].value.key_specific_value = NULL;
                    oscl_free(rui32);
                }
            }
        }

        oscl_free(aParameters[0].key);

        // Free memory for the parameter list
        oscl_free(aParameters);
        aParameters = NULL;
    }
    else
    {
        // Unknown key string
        return PVMFErrArgument;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFSMRTSPUnicastNode::releaseParameters() Out"));
    return PVMFSuccess;
}


void PVMFSMRTSPUnicastNode::setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
        int num_elements, PvmiKvp* &aRet_kvp)
{
    OSCL_UNUSED_ARG(aSession);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFSMRTSPUnicastNode::setParametersSync() In"));


    // Go through each parameter
    for (int paramind = 0; paramind < num_elements; ++paramind)
    {
        // Count the number of components and parameters in the key
        int compcount = pv_mime_string_compcnt(aParameters[paramind].key);

        // Retrieve the first component from the key string
        char* compstr = NULL;
        pv_mime_string_extract_type(0, aParameters[paramind].key, compstr);

        if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf")) < 0) || compcount < 2)
        {
            // First component should be "x-pvmf" and there must
            // be at least two components to go past x-pvmf
            aRet_kvp = &aParameters[paramind];
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFSMRTSPUnicastNode::setParametersSync() Unsupported key"));
            return;
        }

        // Retrieve the second component from the key string
        pv_mime_string_extract_type(1, aParameters[paramind].key, compstr);

        // First check if it is key string for the streaming manager
        if (pv_mime_strcmp(compstr, _STRLIT_CHAR("net")) >= 0)
        {
            if (compcount == 3)
            {
                pv_mime_string_extract_type(2, aParameters[paramind].key, compstr);
                uint i;
                for (i = 0; i < StreamingManagerConfig_NumBaseKeys; i++)
                {
                    if (pv_mime_strcmp(compstr, (char*)(StreamingManagerConfig_BaseKeys[i].iString)) >= 0)
                    {
                        break;
                    }
                }

                if (StreamingManagerConfig_NumBaseKeys == i)
                {
                    // invalid third component
                    aRet_kvp = &aParameters[paramind];
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFSMRTSPUnicastNode::setParametersSync() Unsupported key"));
                    return;
                }
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFSMRTSPUnicastNode::setParametersSync() key str %s", compstr));
                // Verify and set the passed-in setting
                PVMFStatus retval = VerifyAndSetConfigParameter(i, aParameters[paramind], true);
                if (retval != PVMFSuccess)
                {
                    aRet_kvp = &aParameters[paramind];
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFSMRTSPUnicastNode::setParametersSync() Setting "
                                     "parameter %d failed", paramind));
                    return;
                }
            }
            else
            {
                // Do not support more than 3 components right now
                aRet_kvp = &aParameters[paramind];
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFSMRTSPUnicastNode::setParametersSync() Unsupported key"));
                return;
            }
        }
        else
        {
            // Unknown key string
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFSMRTSPUnicastNode::setParametersSync() Unsupported key"));
            return;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFSMRTSPUnicastNode::setParametersSync() Out"));
}


PVMFStatus PVMFSMRTSPUnicastNode::verifyParametersSync(PvmiMIOSession aSession,
        PvmiKvp* aParameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(aSession);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFSMRTSPUnicastNode::verifyParametersSync() In"));

    if (aParameters == NULL || num_elements < 1)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFSMRTSPUnicastNode::verifyParametersSync() Passed in parameter invalid"));
        return PVMFErrArgument;
    }

    // Go through each parameter and verify
    for (int32 paramind = 0; paramind < num_elements; ++paramind)
    {
        // Count the number of components and parameters in the key
        int compcount = pv_mime_string_compcnt(aParameters[paramind].key);
        // Retrieve the first component from the key string
        char* compstr = NULL;
        pv_mime_string_extract_type(0, aParameters[paramind].key, compstr);

        if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf")) < 0) || compcount < 2)
        {
            // First component should be "x-pvmf" and there must
            // be at least two components to go past x-pvmf
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVPlayerEngine::verifyParametersSync() Unsupported key"));
            return PVMFErrArgument;
        }

        // Retrieve the second component from the key string
        pv_mime_string_extract_type(1, aParameters[paramind].key, compstr);

        // First check if it is key string for this node
        if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("net")) >= 0) && (compcount == 3))
        {
            pv_mime_string_extract_type(2, aParameters[paramind].key, compstr);
            uint i;
            for (i = 0; i < StreamingManagerConfig_NumBaseKeys; i++)
            {
                if (pv_mime_strcmp(compstr, (char*)(StreamingManagerConfig_BaseKeys[i].iString)) >= 0)
                {
                    break;
                }
            }

            if (StreamingManagerConfig_NumBaseKeys == i)
            {
                return PVMFErrArgument;
            }

            // Verify the passed-in player setting
            PVMFStatus retval = VerifyAndSetConfigParameter(i, aParameters[paramind], false);
            if (retval != PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVPlayerEngine::DoCapConfigVerifyParameters() Verifying parameter %d failed", paramind));
                return retval;
            }
        }
        else
        {
            // Unknown key string
            return PVMFErrArgument;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVPlayerEngine::DoCapConfigVerifyParameters() Out"));
    return PVMFSuccess;
}



PVMFStatus PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter(int index, PvmiKvp& aParameter, bool set)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() In index[%d]", index));

    // Determine the valtype
    PvmiKvpValueType keyvaltype = GetValTypeFromKeyString(aParameter.key);
    if (keyvaltype == PVMI_KVPVALTYPE_UNKNOWN)
    {
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() "
                               "Valtype in key string unknown"));
        return PVMFErrArgument;
    }

    // Verify the valtype
    if (keyvaltype != StreamingManagerConfig_BaseKeys[index].iValueType)
    {
        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() "
                               "Valtype does not match for key"));
        return PVMFErrArgument;
    }

    switch (index)
    {
        case BASEKEY_DELAY:
        {
            uint32  rebuffThreshold = 0;
            PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            OSCL_ASSERT(iJitterBufferNodeContainer);
            if (!iJitterBufferNodeContainer)
                return PVMFFailure;
            PVMFJitterBufferExtensionInterface* jbExtIntf = NULL;
            if (iJitterBufferNodeContainer)
                jbExtIntf = (PVMFJitterBufferExtensionInterface*)iJitterBufferNodeContainer->iExtensions[0];
            if (jbExtIntf)
                jbExtIntf->getJitterBufferRebufferingThresholdInMilliSeconds(rebuffThreshold);
            if (!jbExtIntf)
                return PVMFFailure;
            // Validate
            if ((aParameter.value.uint32_value < MIN_JITTER_BUFFER_DURATION_IN_MS) ||
                    (aParameter.value.uint32_value > MAX_JITTER_BUFFER_DURATION_IN_MS) ||
                    (aParameter.value.uint32_value < rebuffThreshold))
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() "
                                       "Trying to set delay to 0"));
                return PVMFErrArgument;
            }
            if (set)
            {
                // save value locally
                setJitterBufferDurationInMilliSeconds(aParameter.value.uint32_value);
                // pass the value on to the jitter buffer node
                jbExtIntf->setJitterBufferDurationInMilliSeconds(aParameter.value.uint32_value);
            }
        }
        break;
        case BASEKEY_REBUFFERING_THRESHOLD:
        {
            uint32 jbDuration = 0;
            PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            OSCL_ASSERT(iJitterBufferNodeContainer);
            if (!iJitterBufferNodeContainer)
                return PVMFFailure;
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)iJitterBufferNodeContainer->iExtensions[0];
            jbExtIntf->getJitterBufferDurationInMilliSeconds(jbDuration);
            // Validate
            if (aParameter.value.uint32_value >= jbDuration)
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() "
                                       "Trying to set rebuffering threshold greater than equal to jitter buffer duration"));
                return PVMFErrArgument;
            }
            if (set)
            {
                // pass the value on to the jitter buffer node
                jbExtIntf->setJitterBufferRebufferingThresholdInMilliSeconds(aParameter.value.uint32_value);
            }
        }
        break;
        case BASEKEY_JITTERBUFFER_MAX_INACTIVITY_DURATION:
        {
            PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            OSCL_ASSERT(iJitterBufferNodeContainer);
            if (!iJitterBufferNodeContainer)
                return PVMFFailure;
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)iJitterBufferNodeContainer->iExtensions[0];
            OSCL_ASSERT(jbExtIntf);
            if (!jbExtIntf)
                return PVMFFailure;
            if (set)
            {
                //update the maximum alloyed delay for the incoming msg at input port of JB node
                jbExtIntf->setMaxInactivityDurationForMediaInMs(aParameter.value.uint32_value);
            }
        }
        break;
        case BASEKEY_SESSION_CONTROLLER_USER_AGENT:
        {
            if (set)
            {
                // user agent update
                PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer = NULL;
                iSessionControllerNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
                OSCL_wHeapString<OsclMemAllocator> userAgent;
                OSCL_wHeapString<OsclMemAllocator> dummy;
                if (iSessionControllerNodeContainer != NULL)
                {
                    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                        (PVRTSPEngineNodeExtensionInterface*)
                        (iSessionControllerNodeContainer->iExtensions[0]);

                    userAgent = aParameter.value.pWChar_value;
                    rtspExtIntf->SetClientParameters(userAgent, dummy, dummy);
                }
                // save user-agent kvp for cpm
                if (iCPM)
                {
                    PVMFStatus status = iCPMKvpStore.addKVPString(aParameter.key, userAgent);
                    if (status != PVMFSuccess) return status;
                }

            }
        }
        break;

        case BASEKEY_SESSION_CONTROLLER_KEEP_ALIVE_DURING_PLAY:
        {
            if (set)
            {
                // keep-alive during play update
                /* As of now just RTSP node supports an external config of keep-alive during play */
                PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
                if (iSessionControllerNodeContainer != NULL)
                {
                    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                        (PVRTSPEngineNodeExtensionInterface*)
                        (iSessionControllerNodeContainer->iExtensions[0]);
                    rtspExtIntf->SetKeepAliveMethod_keep_alive_in_play(aParameter.value.bool_value);
                }
            }
        }
        break;

        case BASEKEY_SESSION_CONTROLLER_RTSP_TIMEOUT:
        {
            if (set)
            {
                PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() BASEKEY_SESSION_CONTROLLER_RTSP_TIMEOUT "));
                // RTSP response waiting time out
                PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
                if (iSessionControllerNodeContainer != NULL)
                {
                    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                        (PVRTSPEngineNodeExtensionInterface*)
                        (iSessionControllerNodeContainer->iExtensions[0]);
                    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() BASEKEY_SESSION_CONTROLLER_RTSP_TIMEOUT rtspExtIntf%x", rtspExtIntf));
                    rtspExtIntf->SetRTSPTimeOut(aParameter.value.uint32_value);
                }
                PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() BASEKEY_SESSION_CONTROLLER_RTSP_TIMEOUT Done"));
            }
        }
        break;

        case BASEKEY_DISABLE_FIREWALL_PACKETS:
        {
            if (set)
            {
                PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
                OSCL_ASSERT(iJitterBufferNodeContainer);
                if (!iJitterBufferNodeContainer)
                    return PVMFFailure;
                PVMFJitterBufferExtensionInterface* jbExtIntf =
                    (PVMFJitterBufferExtensionInterface*)iJitterBufferNodeContainer->iExtensions[0];
                OSCL_ASSERT(jbExtIntf);
                if (!jbExtIntf)
                    return PVMFFailure;
                jbExtIntf->DisableFireWallPackets();
            }
        }
        break;

        case BASEKEY_SET_UDP_MIN_MAX_PORT:
        {
            range_uint32 *maxMinUdpPort = (range_uint32 *)aParameter.value.key_specific_value;
            if (maxMinUdpPort->min > maxMinUdpPort->max)
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::VerifyAndSetConfigParameter() UDP port range not valid"));
                return PVMFErrArgument;
            }

            PVMFSMFSPChildNodeContainer* iSocketNodeContainer = getChildNodeContainer(PVMF_SM_FSP_SOCKET_NODE);
            if (iSocketNodeContainer == NULL)
            {
                OSCL_LEAVE(OsclErrBadHandle);
                return PVMFFailure;
            }
            PVMFSocketNodeExtensionInterface* socketNodeExtentionIntf =
                (PVMFSocketNodeExtensionInterface*)iSocketNodeContainer->iExtensions[0];
            if (socketNodeExtentionIntf)
            {
                socketNodeExtentionIntf->SetMaxUDPPortNum(maxMinUdpPort->max, maxMinUdpPort->min);
                iStartPortNum = maxMinUdpPort->min;
                iUseRandomUDPPort = false;
            }
        }
        break;

        default:
            return PVMFErrNotSupported;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFSMRTSPUnicastNode::VerifyAndSetPlayerParameter() Out"));

    return PVMFSuccess;
}

///////////////////////////////////////////////////////////////////////////////
// Implemenation of PVMFDataSourceInitializationExtensionInterface interface
///////////////////////////////////////////////////////////////////////////////

PVMFStatus PVMFSMRTSPUnicastNode::SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData, uint32 aClipIndex, PVMFFormatTypeDRMInfo aType)
{
    if (aClipIndex != 0)
        return PVMFErrArgument;//source playlist not support

    //To set proxy server info with the session controller node
    if (aSourceData != NULL)
    {
        PVInterface* pvInterface = OSCL_STATIC_CAST(PVInterface*, aSourceData);
        PVInterface* sourceDataContext = NULL;
        PVUuid sourceContextUuid(PVMF_SOURCE_CONTEXT_DATA_UUID);
        if (pvInterface->queryInterface(sourceContextUuid, sourceDataContext))
        {
            if (sourceDataContext)
            {
                PVInterface* streamingDataContext = NULL;
                PVUuid streamingContextUuid(PVMF_SOURCE_CONTEXT_DATA_STREAMING_UUID);

                if (sourceDataContext->queryInterface(streamingContextUuid, streamingDataContext))
                {
                    if (streamingDataContext)
                    {
                        PVMFSourceContextDataStreaming* sContext =
                            OSCL_STATIC_CAST(PVMFSourceContextDataStreaming*, streamingDataContext);
                        if (sContext->iProxyName.get_size() > 0)
                        {
                            PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer = NULL;
                            iSessionControllerNodeContainer = getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
                            if (iSessionControllerNodeContainer)
                            {
                                PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                                    (PVRTSPEngineNodeExtensionInterface*)(iSessionControllerNodeContainer->iExtensions[0]);
                                if (rtspExtIntf)
                                {   //the proxyname doesn't need to be unicode
                                    OsclMemAllocator alloc;
                                    char *buf = (char*)alloc.allocate(sContext->iProxyName.get_size() + 1);
                                    if (!buf)
                                        return PVMFErrNoMemory;
                                    uint32 size = oscl_UnicodeToUTF8(sContext->iProxyName.get_cstr(), sContext->iProxyName.get_size(), buf, sContext->iProxyName.get_size() + 1);
                                    if (size == 0)
                                    {
                                        alloc.deallocate(buf);
                                        return PVMFErrNoMemory;
                                    }

                                    OSCL_FastString myProxyName(buf, size);

                                    rtspExtIntf->SetRtspProxy(myProxyName, sContext->iProxyPort);
                                    alloc.deallocate(buf);
                                }
                            }
                        }
                    }
                }

            }
        }
    }
    //to check if we should use CPM plugin?
    if (aSourceData)
    {
        PVInterface* pvInterface =
            OSCL_STATIC_CAST(PVInterface*, aSourceData);
        PVInterface* streamingDataSrc = NULL;
        PVUuid streamingDataSrcUuid(PVMF_STREAMING_DATASOURCE_UUID);
        if (pvInterface->queryInterface(streamingDataSrcUuid, streamingDataSrc))
        {
            PVMFStreamingDataSource* opaqueData =
                OSCL_STATIC_CAST(PVMFStreamingDataSource*, streamingDataSrc);
            iPreviewMode = opaqueData->iPreviewMode;
            iUseCPMPluginRegistry = true;
            iCPMSourceData.iPreviewMode = iPreviewMode;
            iCPMSourceData.iIntent = opaqueData->iIntent;
        }
        else
        {
            PVInterface* sourceDataContext = NULL;
            PVInterface* commonDataContext = NULL;
            PVUuid sourceContextUuid(PVMF_SOURCE_CONTEXT_DATA_UUID);
            PVUuid commonContextUuid(PVMF_SOURCE_CONTEXT_DATA_COMMON_UUID);
            if (pvInterface->queryInterface(sourceContextUuid, sourceDataContext))
            {
                if (sourceDataContext->queryInterface(commonContextUuid, commonDataContext))
                {
                    PVMFSourceContextDataCommon* cContext =
                        OSCL_STATIC_CAST(PVMFSourceContextDataCommon*, commonDataContext);
                    iPreviewMode = cContext->iPreviewMode;
                    iUseCPMPluginRegistry = true;
                    PVMFSourceContextData* sContext =
                        OSCL_STATIC_CAST(PVMFSourceContextData*, sourceDataContext);
                    iSourceContextData = *sContext;
                    iSourceContextDataValid = true;
                }
            }
        }
    }

    if ((iUseCPMPluginRegistry) && (aType != PVMF_FORMAT_TYPE_CONNECT_UNPROTECTED))
    {
        /*
         * If a CPM flag is provided in the source data, then
         * create a CPM object here...
         */
        //cleanup any prior instance
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFSMRTSPUnicastNode::SetSourceInitializationData create CPM obj"));
        if (iCPM)
        {
            iCPM->ThreadLogoff();
            PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
            iCPM = NULL;
        }
        iCPM = PVMFCPMFactory::CreateContentPolicyManager(*this);
        //thread logon may leave if there are no plugins
        int32 err;
        OSCL_TRY(err, iCPM->ThreadLogon(););
        OSCL_FIRST_CATCH_ANY(err,
                             iCPM->ThreadLogoff();
                             PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
                             iCPM = NULL;
                             iUseCPMPluginRegistry = false;
                            );
    }
    else
    {
        //skip CPM if we for sure the content is unprotected
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFSMRTSPUnicastNode::SetSourceInitializationData non-create CPM obj"));
        iUseCPMPluginRegistry = false;
    }

    //to set the sessionsource info and configure session controller node [RTSP client], with the session type
    if ((aSourceFormat == PVMF_MIME_DATA_SOURCE_RTSP_URL))
    {
        PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
        if (iSessionControllerNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return PVMFFailure;
        }
        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            (PVRTSPEngineNodeExtensionInterface*)
            (iSessionControllerNodeContainer->iExtensions[0]);

        iSessionSourceInfo->_sessionType = aSourceFormat;
        iSessionSourceInfo->_sessionURL  = aSourceURL;
        rtspExtIntf->SetStreamingType(PVRTSP_3GPP_UDP);
        return (rtspExtIntf->SetSessionURL(iSessionSourceInfo->_sessionURL));
    }
    else if (aSourceFormat == PVMF_MIME_DATA_SOURCE_SDP_FILE)
    {
        PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
        if (iSessionControllerNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return PVMFFailure;
        }
        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            (PVRTSPEngineNodeExtensionInterface*)
            (iSessionControllerNodeContainer->iExtensions[0]);
        // right now, PVMF_MIME_DATA_SOURCE_SDP_FILE => non-interleaved RTSP
        rtspExtIntf->SetStreamingType(PVRTSP_3GPP_UDP);
        iSessionSourceInfo->_sessionType = aSourceFormat;
        iSessionSourceInfo->_sessionURL  = aSourceURL;
        iSessionSourceInfo->_sourceData = aSourceData;

        return PVMFSuccess;
    }
    else
        return PVMFErrNotSupported;

}

PVMFStatus PVMFSMRTSPUnicastNode::SetClientPlayBackClock(PVMFMediaClock* aClientClock)
{
    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);

    if (iJitterBufferNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return PVMFFailure;
    }

    PVMFJitterBufferExtensionInterface* jbExtIntf =
        (PVMFJitterBufferExtensionInterface*)
        (iJitterBufferNodeContainer->iExtensions[0]);

    jbExtIntf->setClientPlayBackClock(aClientClock);

    return PVMFSuccess;
}

PVMFStatus PVMFSMRTSPUnicastNode::SetEstimatedServerClock(PVMFMediaClock* aClientClock)
{
    OSCL_UNUSED_ARG(aClientClock);
    return PVMFErrNotSupported;
}
void PVMFSMRTSPUnicastNode::AudioSinkEvent(PVMFStatus aEvent, uint32 aStreamId)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_UNUSED_ARG(aStreamId);
    //ignore
}

///////////////////////////////////////////////////////////////////////////////
//Implementation of PVMFTrackSelectionExtensionInterface
///////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFSMRTSPUnicastNode::GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo)
{
    SDPInfo* sdpInfo = iSdpInfo.GetRep();

    /* Get SDP Session Info */
    sessionDescription* sessionInfo  = NULL;
    if (sdpInfo)
    {
        sessionInfo = sdpInfo->getSessionInfo();
        if (!sessionInfo)
        {
            return PVMFFailure;
        }
    }
    else
    {
        return PVMFFailure;
    }



    RtspRangeType *sessionRange = OSCL_CONST_CAST(RtspRangeType*, (sessionInfo->getRange()));

    int32 sessionStartTime = 0, sessionStopTime = 0;

    sessionRange->convertToMilliSec(sessionStartTime, sessionStopTime);

    int32 duration_msec = sessionStopTime;

    uint64 duration64;
    Oscl_Int64_Utils::set_uint64(duration64, 0, (uint32)duration_msec);

    if (sessionRange->end_is_set == true)
    {
        aInfo.setDurationValue(duration64);
        aInfo.setDurationTimeScale(1000);
    }
    else
    {
        aInfo.SetDurationAvailable(false);
    }

    aInfo.setSeekableFlag((!(sessionInfo->getRandomAccessDenied())));

    int32 numTracks = sdpInfo->getNumMediaObjects();

    SDPAltGroupType sdpAltGroupType = sessionInfo->getSDPAltGroupType();

    PVMF_TRACK_INFO_TRACK_ALTERNATE_TYPE iAltType = PVMF_TRACK_ALTERNATE_TYPE_UNDEFINED;

    if (sdpAltGroupType == SDP_ALT_GROUP_LANGUAGE)
    {
        iAltType = PVMF_TRACK_ALTERNATE_TYPE_LANGUAGE;
    }
    else if (sdpAltGroupType == SDP_ALT_GROUP_BANDWIDTH)
    {
        iAltType = PVMF_TRACK_ALTERNATE_TYPE_BANDWIDTH;
    }

    for (int32 i = 0; i < numTracks; i++)
    {
        /*
         * Get the vector of mediaInfo as there can
         * alternates for each track
         */
        Oscl_Vector<mediaInfo*, SDPParserAlloc> mediaInfoVec =
            sdpInfo->getMediaInfo(i);

        uint32 minfoVecLen = mediaInfoVec.size();

        for (uint32 j = 0; j < minfoVecLen; j++)
        {
            mediaInfo* mInfo = mediaInfoVec[j];

            if (mInfo == NULL)
            {
                return PVMFFailure;
            }

            RtspRangeType *mediaRange = mInfo->getRtspRange();

            int32 mediaStartTime = 0, mediaStopTime = 0;

            mediaRange->convertToMilliSec(mediaStartTime, mediaStopTime);
            int32 mediaDuration_ms = mediaStopTime - mediaStartTime;
            uint64 mediaDuration64;
            Oscl_Int64_Utils::set_uint64(mediaDuration64, 0, (uint32)mediaDuration_ms);

            PVMFTrackInfo trackInfo;

            Oscl_Vector<PayloadSpecificInfoTypeBase*, SDPParserAlloc> payloadVector;
            payloadVector = mInfo->getPayloadSpecificInfoVector();

            if (payloadVector.size() == 0)
            {
                return false;
            }
            /*
             * There can be multiple payloads per media segment.
             * We only support one for now, so
             * use just the first payload
             */
            PayloadSpecificInfoTypeBase* payloadInfo = payloadVector[0];

            // set config for later
            int32 configSize = payloadInfo->configSize;
            OsclAny* config = payloadInfo->configHeader.GetRep();

            OSCL_StackString<256> mimeString;
            const char* mimeType = mInfo->getMIMEType();
            mimeString += mimeType;
            trackInfo.setTrackMimeType(mimeString);

            uint32 trackID = mInfo->getMediaInfoID();

            trackInfo.setTrackID(trackID);
            trackInfo.setPortTag(trackID);

            trackInfo.setTrackBitRate(mInfo->getBitrate());

            if (mediaRange->end_is_set == true)
            {
                trackInfo.setTrackDurationValue(mediaDuration64);
            }
            else
            {
                trackInfo.SetDurationAvailable(false);
            }

            if ((configSize > 0) && (config != NULL))
            {
                OsclMemAllocDestructDealloc<uint8> my_alloc;
                OsclRefCounter* my_refcnt;
                uint aligned_refcnt_size =
                    oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));

                uint8* my_ptr = GetMemoryChunk(my_alloc, aligned_refcnt_size + configSize);
                if (!my_ptr)
                    return PVMFFailure;

                my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
                my_ptr += aligned_refcnt_size;

                OsclMemoryFragment memfrag;
                memfrag.len = (uint32)configSize;
                memfrag.ptr = my_ptr;

                oscl_memcpy((void*)(memfrag.ptr), (const void*)config, memfrag.len);

                OsclRefCounterMemFrag tmpRefcntMemFrag(memfrag, my_refcnt, memfrag.len);
                trackInfo.setTrackConfigInfo(tmpRefcntMemFrag);
            }

            int32 dependsOnTrackID = mInfo->getDependsOnTrackID();

            if (dependsOnTrackID != -1)
            {
                trackInfo.setDependsOn();
                mediaInfo* baseMediaInfo = sdpInfo->getMediaInfoBasedOnDependsOnID(dependsOnTrackID);
                if (baseMediaInfo == NULL)
                {
                    return PVMFFailure;
                }
                trackInfo.addDependsOnTrackID(baseMediaInfo->getMediaInfoID());
            }

            if (iAltType != PVMF_TRACK_ALTERNATE_TYPE_UNDEFINED)
            {
                /* Expose alternate track ids */
                trackInfo.setTrackAlternates(iAltType);
                for (uint32 k = 0; k < minfoVecLen; k++)
                {
                    mediaInfo* mInfo = mediaInfoVec[k];
                    if (mInfo == NULL)
                    {
                        return PVMFFailure;
                    }
                    uint32 altID = mInfo->getMediaInfoID();
                    if (altID != trackID)
                    {
                        trackInfo.addAlternateTrackID((int32)altID);
                    }
                }
            }
            aInfo.addTrackInfo(trackInfo);
        }
    }
    iCompleteMediaPresetationInfo = aInfo;
    return PVMFSuccess;
}

PVMFStatus PVMFSMRTSPUnicastNode::SelectTracks(PVMFMediaPresentationInfo& aInfo)
{
    SDPInfo* sdpInfo = iSdpInfo.GetRep();
    if (sdpInfo == NULL)
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:SelectTracks - SDP Not Available"));
        return PVMFErrArgument;
    }

    int32 numTracks = aInfo.getNumTracks();

    for (int32 i = 0; i < numTracks; i++)
    {
        PVMFTrackInfo* trackInfo = aInfo.getTrackInfo(i);

        uint32 trackID = trackInfo->getTrackID();

        mediaInfo* mInfo =
            sdpInfo->getMediaInfoBasedOnID(trackID);

        if (mInfo == NULL)
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:SelectTracks - Invalid SDP TrackID"));
            return PVMFErrArgument;
        }

        mInfo->setSelect();

        /* Set selected field in meta info */
        Oscl_Vector<PVMFSMTrackMetaDataInfo, OsclMemAllocator>::iterator it;
        for (it = iMetaDataInfo->iTrackMetaDataInfoVec.begin(); it != iMetaDataInfo->iTrackMetaDataInfoVec.end(); it++)
        {
            if (it->iTrackID == trackID)
            {
                it->iTrackSelected = true;
            }
        }
    }
    iSelectedMediaPresetationInfo = aInfo;
    return PVMFSuccess;
}

///////////////////////////////////////////////////////////////////////////////
//Implementation of PVMFMetadataExtensionInterface
///////////////////////////////////////////////////////////////////////////////
uint32 PVMFSMRTSPUnicastNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    //Metadata is avaialable in three forms
    //1. Metadata common to streaming of all type of payloads and FF specific metadata
    //2. Streaming specific metadata
    //3. CPM metadata
    //First two types are avaiable in iAvailableMetaDatakeys vector
    //Third type can be had from metadataextension interface
    //Base class considers count of all of these
    return PVMFSMFSPBaseNode::GetNumMetadataValuesBase(aKeyList);
}

PVMFStatus PVMFSMRTSPUnicastNode::GetRTSPPluginSpecificValues(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMFMetadataList* keylistptr = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    aCmd.PVMFSMFSPBaseNodeCommand::Parse(keylistptr, valuelistptr, starting_index, max_entries);

    // Check the parameters
    if (keylistptr == NULL || valuelistptr == NULL)
    {
        return PVMFErrArgument;
    }

    if (keylistptr->size() == 1)
    {
        if (oscl_strncmp((*keylistptr)[0].get_cstr(),
                         PVMFSTREAMINGMGRNODE_ALL_METADATA_KEY,
                         oscl_strlen(PVMFSTREAMINGMGRNODE_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &iAvailableMetadataKeys;
        }
    }
    uint32 numkeys = keylistptr->size();

    if (numkeys <= 0 || max_entries == 0)
    {
        // Don't do anything
        return PVMFErrArgument;
    }

    uint32 numvalentries = 0;
    int32 numentriesadded = 0;

    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        PvmiKvp KeyVal;
        KeyVal.key = NULL;
        KeyVal.value.pWChar_value = NULL;
        KeyVal.value.pChar_value = NULL;

        if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMFSTREAMINGMGRNODE_PAUSE_DENIED_KEY) != NULL)
        {
            // Increment the counter for the number of values found so far
            ++numvalentries;
            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                                    PVMFSTREAMINGMGRNODE_PAUSE_DENIED_KEY,
                                    iPauseDenied);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }

        /* Check if the max number of value entries were added */
        if (max_entries > 0 && numentriesadded >= max_entries)
        {
            iPVMFStreamingManagerNodeMetadataValueCount = (*valuelistptr).size();
            return PVMFSuccess;
        }

        // Add the KVP to the list if the key string was created
        if (KeyVal.key != NULL)
        {
            if (PVMFSuccess != PushKVPToMetadataValueList(valuelistptr, KeyVal))
            {
                switch (GetValTypeFromKeyString(KeyVal.key))
                {
                    case PVMI_KVPVALTYPE_CHARPTR:
                        if (KeyVal.value.pChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                            KeyVal.value.pChar_value = NULL;
                        }
                        break;

                    default:
                        // Add more case statements if other value types are returned
                        break;
                }

                OSCL_ARRAY_DELETE(KeyVal.key);
                KeyVal.key = NULL;
            }
            else
            {
                // Increment the counter for number of value entries added to the list
                ++numentriesadded;
            }

            // Check if the max number of value entries were added
            if (max_entries > 0 && numentriesadded >= max_entries)
            {
                // Maximum number of values added so break out of the loop
                break;
            }
        }
    }

    iNoOfValuesIteratedForValueVect = numvalentries;
    iNoOfValuesPushedInValueVect = numentriesadded;

    iPVMFStreamingManagerNodeMetadataValueCount = (*valuelistptr).size();
    return PVMFSuccess;
}

PVMFStatus PVMFSMRTSPUnicastNode::DoGetMetadataValues(PVMFSMFSPBaseNodeCommand& aCmd)
{
    iNoOfValuesIteratedForValueVect = 0;
    iNoOfValuesPushedInValueVect = 0;
    PVMFStatus retval = GetRTSPPluginSpecificValues(aCmd);
    if (PVMFSuccess != retval)
        return retval;
    return DoGetMetadataValuesBase(aCmd);
}

PVMFStatus PVMFSMRTSPUnicastNode::ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
        uint32 aStartingValueIndex,
        uint32 aEndValueIndex)
{
    //no allocation for any value in kvp took in derived class so just calling base class release functions
    return ReleaseNodeMetadataValuesBase(aValueList, aStartingValueIndex, aEndValueIndex);
}

void PVMFSMRTSPUnicastNode::DoSetDataSourcePosition(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMFSPBaseNodeCommand::DoSetDataSourcePosition - In"));

    iActualRepositionStartNPTInMSPtr = NULL;
    iActualMediaDataTSPtr = NULL;
    iPVMFDataSourcePositionParamsPtr = NULL;
    iJumpToIFrame = false;
    uint32 streamID = 0;

    aCmd.PVMFSMFSPBaseNodeCommand::Parse(iRepositionRequestedStartNPTInMS,
                                         iActualRepositionStartNPTInMSPtr,
                                         iActualMediaDataTSPtr,
                                         iJumpToIFrame,
                                         streamID);

    PVMF_SM_RTSP_LOG_COMMAND_REPOS((0, "PVMFSMFSPBaseNodeCommand::DoSetDataSourcePosition - TargetNPT = %d", iRepositionRequestedStartNPTInMS));

    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (iJitterBufferNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return;
    }
    PVMFJitterBufferExtensionInterface* jbExtIntf =
        (PVMFJitterBufferExtensionInterface*)
        (iJitterBufferNodeContainer->iExtensions[0]);

    *iActualRepositionStartNPTInMSPtr = 0;
    *iActualMediaDataTSPtr = 0;


    if ((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL) ||
            (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE))
    {
        if (iInterfaceState == EPVMFNodePrepared)
        {
            iStreamID = streamID;
            jbExtIntf->SendBOSMessage(iStreamID);

            bool oRandAccessDenied = true;

            sessionDescription* sessionInfo =
                iSdpInfo->getSessionInfo();
            oRandAccessDenied = sessionInfo->getRandomAccessDenied();

            if ((oRandAccessDenied == true) ||
                    (iSessionStopTimeAvailable == false) ||
                    (((int32)iRepositionRequestedStartNPTInMS < (int32)iSessionStartTime) ||
                     ((int32)iRepositionRequestedStartNPTInMS >= (int32)iSessionStopTime)))
            {
                if (iRepositionRequestedStartNPTInMS == 0)
                {
                    /* Implies that we are repositioning to zero, which is not a invalid request
                     * so dont send PVMFErrNotSupported
                     */
                }
                else
                {
                    /*
                     * Implies an open ended session or invalid request time
                     * - no pause or reposition
                     */
                    CommandComplete(iInputCommands, aCmd, PVMFErrNotSupported);
                    return;
                }
            }

            /*
             * We will always try to move the sm node to started state(and therefore do buffering etc)
             * in the impl of the SetDataSourcePosition
             * We will do this even if iRepositionRequestedStartNPTInMS is zero.
             * Reason - It might be possible that user request the streaming session from 0 npt
             * but, due to some reasons, server is able to stream from the x npt and does
             * specify it in the Range header of Play response.
             * This "x" npt need to be communicated to the engine in the completion of
             * SetDataSourcePosition command.
             */
            if ((iRepositionRequestedStartNPTInMS < iSessionStopTime) && (iRepositionRequestedStartNPTInMS >= iSessionStartTime))
            {
                // we need to use part of the logic of repositioning to start
                // streaming from a non-zero offset. Enabled only for 3gpp streaming
                iRepositioning = true;
                /* Start the nodes */
                PVMFStatus status = DoRepositioningStart3GPPStreaming();
                if (PVMFSuccess != status)
                {
                    CommandComplete(iInputCommands, aCmd, status);
                    return;
                }

                MoveCmdToCurrentQueue(aCmd);
                return;
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFSMRTSPUnicastNode::DoSetDataSourcePosition - Invalid value of iRepositionRequestedStartNPTInMS [%u] -- should have been between [%u] - [%u]", iRepositionRequestedStartNPTInMS, iSessionStartTime, iSessionStopTime));
                CommandComplete(iInputCommands, aCmd, PVMFErrArgument);
                return;
            }
        }
        else if ((iInterfaceState == EPVMFNodeStarted) || (iInterfaceState == EPVMFNodePaused))
        {
            bool oRandAccessDenied = true;

            sessionDescription* sessionInfo =
                iSdpInfo->getSessionInfo();
            oRandAccessDenied = sessionInfo->getRandomAccessDenied();


            if (!CanPerformRepositioning(oRandAccessDenied))
            {
                CommandComplete(iInputCommands, aCmd, PVMFErrNotSupported);
                return;
            }

            iStreamID = streamID;
            jbExtIntf->SendBOSMessage(iStreamID);

            iRepositioning = true;

            /* Put the jitter buffer into a state of transition */
            PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            if (iJitterBufferNodeContainer == NULL)
            {
                OSCL_LEAVE(OsclErrBadHandle);
                return;
            }
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)
                (iJitterBufferNodeContainer->iExtensions[0]);
            jbExtIntf->PrepareForRepositioning();

            /* If node is running, pause first */
            if (iInterfaceState == EPVMFNodeStarted)
            {
                if (!DoRepositioningPause3GPPStreaming())
                {
                    CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
                }
            }
            /* If already paused do not pause */
            else if (iInterfaceState == EPVMFNodePaused)
            {
                PVMFStatus status = DoRepositioningStart3GPPStreaming();
                if (PVMFSuccess != status)
                {
                    CommandComplete(iInputCommands, aCmd, status);
                }
            }
            MoveCmdToCurrentQueue(aCmd);
        }
        else
        {
            CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
            return;
        }
    }
    else
    {
        PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::SetDataSourcePosition() - Cmd Failed - PVMFErrArgument"));
        CommandComplete(iInputCommands, aCmd, PVMFErrArgument);
        return;
    }
    return;
}



void PVMFSMRTSPUnicastNode::DoQueryDataSourcePosition(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoQueryDataSourcePosition - In"));

    PVMFTimestamp repositionrequestedstartnptinms = 0;
    PVMFTimestamp* actualrepositionstartnptinmsptr = NULL;
    bool seektosyncpoint = false;

    aCmd.PVMFSMFSPBaseNodeCommand::Parse(repositionrequestedstartnptinms,
                                         actualrepositionstartnptinmsptr,
                                         seektosyncpoint);

    if (actualrepositionstartnptinmsptr == NULL)
    {
        CommandComplete(iInputCommands, aCmd, PVMFErrArgument);
        return;
    }
    *actualrepositionstartnptinmsptr = 0;

    // This query is not supported for streaming sessions
    CommandComplete(iInputCommands, aCmd, PVMFErrNotSupported);
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoQueryDataSourcePosition - Out"));
    return;
}

PVMFStatus PVMFSMRTSPUnicastNode::ComputeSkipTimeStamp(PVMFTimestamp aTargetNPT,
        PVMFTimestamp aActualNPT,
        PVMFTimestamp aActualMediaDataTS,
        PVMFTimestamp& aSkipTimeStamp,
        PVMFTimestamp& aStartNPT)
{
    //for RTSP streaming we always start playback from aActualNPT
    //by defintion aActualMediaDataTS is the timestamp that corresponds
    //to aActualNPT
    OSCL_UNUSED_ARG(aTargetNPT);
    OSCL_UNUSED_ARG(aSkipTimeStamp);
    OSCL_UNUSED_ARG(aStartNPT);
    aSkipTimeStamp = aActualMediaDataTS;
    aStartNPT = aActualNPT;
    return PVMFSuccess;
}

///////////////////////////////////////////////////////////////////////////////
//Implementation of the virtual function declared in PVMFNodeCmdStatusObserver
///////////////////////////////////////////////////////////////////////////////
void PVMFSMRTSPUnicastNode::NodeCommandCompleted(const PVMFCmdResp& aResponse)
{
    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::NodeCommandCompleted"));
    bool performErrHandling = false;
    HandleChildNodeCommandCompletion(aResponse, performErrHandling);
    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::NodeCommandCompleted - performErrHandling[%d]", performErrHandling));

    if (performErrHandling == true)
    {
        HandleError(aResponse);
    }

    return;
}

/**
retval: true - perform error handling based on response (if needed)
retval: false - do not perform error handling.Concrete implemenbtation of the FSP will take care of error handling
*/
void PVMFSMRTSPUnicastNode::HandleChildNodeCommandCompletion(const PVMFCmdResp& aResponse, bool& aPerformErrHandling)
{
    aPerformErrHandling = true;

    PVMFSMFSPCommandContext *cmdContextData =
        OSCL_REINTERPRET_CAST(PVMFSMFSPCommandContext*, aResponse.GetContext());

    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode:HandleChildNodeCommandCompletion: %d", cmdContextData->cmd));

    if ((cmdContextData->cmd >=
            PVMF_SM_FSP_SOCKET_NODE_COMMAND_START) &&
            (cmdContextData->cmd <
             PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_COMMAND_START))

    {
        HandleSocketNodeCommandCompleted(aResponse, aPerformErrHandling);
    }
    else if ((cmdContextData->cmd >=
              PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_COMMAND_START) &&
             (cmdContextData->cmd <
              PVMF_SM_FSP_JITTER_BUFFER_CONTROLLER_COMMAND_START))

    {
        HandleRTSPSessionControllerCommandCompleted(aResponse, aPerformErrHandling);
    }
    else if ((cmdContextData->cmd >=
              PVMF_SM_FSP_JITTER_BUFFER_CONTROLLER_COMMAND_START) &&
             (cmdContextData->cmd <
              PVMF_SM_FSP_MEDIA_LAYER_COMMAND_START))

    {
        HandleJitterBufferCommandCompleted(aResponse, aPerformErrHandling);
    }
    else
    {
        OSCL_ASSERT(false);
    }

}

void PVMFSMRTSPUnicastNode::HandleSocketNodeCommandCompleted(const PVMFCmdResp& aResponse, bool& aPerformErrHandling)
{
    aPerformErrHandling = false;

    PVMFSMFSPChildNodeContainer* iSocketNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_SOCKET_NODE);
    if (iSocketNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return;
    }

    PVMFSMFSPCommandContext *cmdContextData =
        OSCL_REINTERPRET_CAST(PVMFSMFSPCommandContext*, aResponse.GetContext());
    cmdContextData->oFree = true;

    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleSocketNodeCommandCompleted In - cmd [%d] iSocketNodeContainer->iNodeCmdState [%d] iInterfaceState[%d]", cmdContextData->cmd, iSocketNodeContainer->iNodeCmdState, iInterfaceState));

    //RTSPUNICAST plugin uses sync version of QueryInterface to get xtension interface of its various child nodes
    //So, command completion of async version of QueryInterface from child node not expected.
    OSCL_ASSERT(cmdContextData->cmd != PVMF_SM_FSP_SOCKET_NODE_QUERY_INTERFACE);



    if (iSocketNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_PENDING)
    {
        if (cmdContextData->cmd == PVMF_SM_FSP_SOCKET_NODE_REQUEST_PORT)
        {
            //This is last of the request ports
            OSCL_ASSERT(iSocketNodeContainer->iNumRequestPortsPending > 0);
            if (--iSocketNodeContainer->iNumRequestPortsPending == 0)
            {
                iSocketNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
            }
        }
        else
        {
            iSocketNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
        }
    }
    else if (iSocketNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_CANCEL_PENDING)
    {
        if ((cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCELALLCOMMANDS) || (cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCELCOMMAND) || (cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCEL_DUE_TO_ERROR))
        {
            iSocketNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
        }
        else
        {
            PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleSocketNodeCommandCompleted cmd completion for cmd other than cancel during cancellation"));

            //if cancel is pending and if the parent cmd is not cancel then this is
            //is most likely the cmd that is being cancelled.
            //we ignore cmd completes from child nodes if cancel is pending
            //we simply wait on cancel complete and cancel the pending cmd
            return;
        }
    }
    else if (iSocketNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_IDLE)
    {
        PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleSocketNodeCommandCompleted Nodecontainer state IDLE already!!!!!"));
        /*
         * This is to handle a usecase where a node reports cmd complete for cancelall first
         * and then reports cmd complete on the cmd that was meant to be cancelled.
         * There are two possible scenarios that could arise based on this:
         * i) SM node has reported cmd complete on both canceall and the cmd meant to be cancelled
         * to engine by the time cmd complete on the cmd that was meant to be cancelled arrives
         * from the child node. In this case iNodeCmdState would be PVMFSMFSP_NODE_CMD_NO_PENDING.
         * ii) SM node is still waiting on some other child nodes to complete cancelall.
         * In this case iNodeCmdState would be PVMFSMFSP_NODE_CMD_IDLE.
         * In either case iNodeCmdState cannot be PVMFSMFSP_NODE_CMD_PENDING or PVMFSMFSP_NODE_CMD_IDLE
         * (recall that we call ResetNodeContainerCmdState  prior to issuing cancelall)
         * Or this is the case of node reporting cmd complete multiple times for a cmd, which
         * also can be ignored
         */
        return;
    }

    if (EPVMFNodeError == iInterfaceState)//If interface is in err state, let the err handler do processing
    {
        aPerformErrHandling = true;
        return;
    }

    if (aResponse.GetCmdStatus() != PVMFSuccess)
    {
        if (aResponse.GetCmdStatus() != PVMFErrCancelled)
        {
            aPerformErrHandling = true;
        }

        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::HandleSocketNodeCommandCompleted - Command failed - context=0x%x, status=0x%x", aResponse.GetContext(), aResponse.GetCmdStatus()));
        if (IsBusy())
        {
            Cancel();
            RunIfNotReady();
        }
        return;
    }

    switch (cmdContextData->cmd)
    {
        case PVMF_SM_FSP_SOCKET_NODE_INIT:
        {
            CompleteInit();
        }
        break;

        case PVMF_SM_FSP_SOCKET_NODE_PREPARE:
        {
            CompletePrepare();
        }
        break;

        case PVMF_SM_FSP_SOCKET_NODE_START:
        {
            CompleteStart();
        }
        break;

        case PVMF_SM_FSP_SOCKET_NODE_STOP:
        {
            CompleteStop();
        }
        break;

        case PVMF_SM_FSP_SOCKET_NODE_FLUSH:
        {
            CompleteFlush();
        }
        break;

        case PVMF_SM_FSP_SOCKET_NODE_PAUSE:
        {
            CompletePause();
        }
        break;

        case PVMF_SM_FSP_SOCKET_NODE_RESET:
        {
            CompleteReset();
        }
        break;

        case PVMF_SM_FSP_SOCKET_NODE_REQUEST_PORT:
        {
            PVMFPortInterface* port =
                (PVMFPortInterface*)aResponse.GetEventData();

            /*
             * Save the port in TrackInfo
             */
            PVMFRTSPTrackInfo* trackInfo =
                FindTrackInfo(cmdContextData->portContext.trackID);
            OSCL_ASSERT(trackInfo);
            if (!trackInfo)
                return;
            if (cmdContextData->portContext.portTag ==
                    PVMF_SOCKET_NODE_PORT_TYPE_SOURCE)
            {
                if (trackInfo)
                    trackInfo->iNetworkNodePort = port;
                iSocketNodeContainer->iOutputPorts.push_back(port);
            }
            else if (cmdContextData->portContext.portTag ==
                     PVMF_SOCKET_NODE_PORT_TYPE_SINK)
            {
                if (trackInfo)
                    trackInfo->iNetworkNodeRTCPPort = port;
                iSocketNodeContainer->iInputPorts.push_back(port);
            }
            CompleteGraphConstruct();
        }
        break;
        case PVMF_SM_FSP_SOCKET_NODE_CANCEL_ALL_COMMANDS:
        {
            CompleteChildNodesCmdCancellation();
        }
        break;

        default:
            break;
    }
    return;
}

void PVMFSMRTSPUnicastNode::HandleRTSPSessionControllerCommandCompleted(const PVMFCmdResp& aResponse,
        bool& aPerformErrHandling)
{
    aPerformErrHandling = false;

    PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
    if (iSessionControllerNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return;
    }

    PVMFSMFSPCommandContext *cmdContextData =
        OSCL_REINTERPRET_CAST(PVMFSMFSPCommandContext*, aResponse.GetContext());
    cmdContextData->oFree = true;
    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleRTSPSessionControllerCommandCompleted In - cmd [%d] iSessionControllerNodeContainer->iNodeCmdState [%d] iInterfaceState[%d]", cmdContextData->cmd, iSessionControllerNodeContainer->iNodeCmdState, iInterfaceState));

    //RTSPPLUSUNICAST plugin uses sync version of QueryInterface to get xtension interface of its various child nodes
    //So, command completion of async version of QueryInterface from child node not expected.
    OSCL_ASSERT(cmdContextData->cmd != PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_QUERY_INTERFACE);

    if (iSessionControllerNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_PENDING)
    {
        if (cmdContextData->cmd == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_REQUEST_PORT)
        {
            //This is last of the request ports
            OSCL_ASSERT(iSessionControllerNodeContainer->iNumRequestPortsPending > 0);
            if (--iSessionControllerNodeContainer->iNumRequestPortsPending == 0)
            {
                iSessionControllerNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
            }
        }
        else
        {
            iSessionControllerNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
        }
    }
    else if (iSessionControllerNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_CANCEL_PENDING)
    {
        if ((cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCELALLCOMMANDS) || (cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCELCOMMAND) || (cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCEL_DUE_TO_ERROR))
        {
            iSessionControllerNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
        }
        else
        {
            PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleRTSPSessionControllerCommandCompleted cmd completion for cmd other than cancel during cancellation"));

            //if cancel is pending and if the parent cmd is not cancel then this is
            //is most likely the cmd that is being cancelled.
            //we ignore cmd completes from child nodes if cancel is pending
            //we simply wait on cancel complete and cancel the pending cmd
            return;
        }
    }
    else if (iSessionControllerNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_IDLE)
    {
        PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleRTSPSessionControllerCommandCompleted Container in IDLE state already"));
        /*
         * This is to handle a usecase where a node reports cmd complete for cancelall first
         * and then reports cmd complete on the cmd that was meant to be cancelled.
         * There are two possible scenarios that could arise based on this:
         * i) SM node has reported cmd complete on both canceall and the cmd meant to be cancelled
         * to engine by the time cmd complete on the cmd that was meant to be cancelled arrives
         * from the child node. In this case iNodeCmdState would be PVMFSMFSP_NODE_CMD_NO_PENDING.
         * ii) SM node is still waiting on some other child nodes to complete cancelall.
         * In this case iNodeCmdState would be PVMFSMFSP_NODE_CMD_IDLE.
         * In either case iNodeCmdState cannot be PVMFSMFSP_NODE_CMD_PENDING or PVMFSMFSP_NODE_CMD_IDLE
         * (recall that we call ResetNodeContainerCmdState  prior to issuing cancelall)
         * Or this is the case of node reporting cmd complete multiple times for a cmd, which
         * also can be ignored
         */
        return;
    }

    if (EPVMFNodeError == iInterfaceState)//If interface is in err state, let the err handler do processing
    {
        aPerformErrHandling = true;
        return;
    }
    if (aResponse.GetCmdStatus() != PVMFSuccess)
    {
        if (aResponse.GetCmdStatus() != PVMFErrCancelled)
        {
            aPerformErrHandling = true;
        }
        if ((cmdContextData->cmd == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_PAUSE) ||
                (cmdContextData->cmd == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_STOP))
        {
            /*
             * Check if it is a pause/stop  failure - suppress pause/stop failures if they
             * happen after a session is complete
             */
            PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            if (iJitterBufferNodeContainer == NULL)
            {
                OSCL_LEAVE(OsclErrBadHandle);
                return;
            }
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)
                (iJitterBufferNodeContainer->iExtensions[0]);
            bool oSessionExpired = false;
            jbExtIntf->HasSessionDurationExpired(oSessionExpired);
            if (oSessionExpired == true)
            {
                aPerformErrHandling = false;
            }
        }
        if (cmdContextData->cmd == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_PLAYLIST_PLAY)
        {
            PVInterface* localPVInterface = aResponse.GetEventExtensionInterface();
            if (NULL != localPVInterface)
            {
                PVMFErrorInfoMessageInterface *localErrorInfoMessage = OSCL_REINTERPRET_CAST(PVMFErrorInfoMessageInterface*, localPVInterface);
                int32 localErrorCode = 0;
                PVUuid localUuid;
                localErrorInfoMessage->GetCodeUUID(localErrorCode, localUuid);
                // 202 means switch SDP available, but track selection needs to be done
                if (PVMRTSPClientEngineInfoRTSPPartialSuccessCode202 == localErrorCode)
                {
                    // Set a flag that would trigger track selection and a new FCS request to be sent out.
                    // The status of this flag is checked once switch SDP is retrieved from the entity body.
                    iSelectTracksAndSendFCSRequestFlag = true;
                    iPlaylistPlayInProgress = true;
                    return;
                }
                else if ((PVMFRTSPClientEngineNodeErrorRTSPErrorCode404 == localErrorCode) ||
                         (PVMFRTSPClientEngineNodeErrorRTSPErrorCode415 == localErrorCode) ||
                         (PVMFRTSPClientEngineNodeErrorRTSPErrorCode457 == localErrorCode) ||
                         (PVMFRTSPClientEngineNodeErrorRTSPErrorCode551 == localErrorCode))
                {
                    aPerformErrHandling = false;
                    // Reset iSelectTracksAndSendFCSRequestFlag and iPlaylistPlayInProgress
                    iSelectTracksAndSendFCSRequestFlag = false;
                    iPlaylistPlayInProgress = false;
                    iSessionControllerNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
                    BypassError();
                    ReportInfoEvent(PVMFInfoPlayListClipTransition, NULL, &localUuid, &localErrorCode);
                    return;
                }

            }
        }
        /* if a failure has been overridden just fall thru */
        if (aPerformErrHandling == true)
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::HandleRTSPSessionControllerCommandCompleted - Command failed - context=0x%x, status=0x%x", aResponse.GetContext(), aResponse.GetCmdStatus()));
            if (IsBusy())
            {
                Cancel();
                RunIfNotReady();
            }
            return;
        }
    }

    switch (cmdContextData->cmd)
    {
        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_INIT:
            CompleteInit();
            break;

        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_PREPARE:
        {
            /* Complete set up of feedback channels */
            CompleteFeedBackPortsSetup();
            /*
             * Send start complete params to child nodes
             * viz. SSRC etc
             */
            SendSessionControlPrepareCompleteParams();
            CompletePrepare();
        }
        break;

        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_START:
        {
            /*
             * Send start complete params to child nodes
             * viz. actual play range, rtp info params etc
             */

            SendSessionControlStartCompleteParams();
            CompleteStart();
        }
        break;

        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_STOP:
        {
            CompleteStop();
        }
        break;

        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_FLUSH:
            CompleteFlush();
            break;

        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_PAUSE:
        {
            CompletePause();
        }
        break;

        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_RESET:
            CompleteReset();
            break;

        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_REQUEST_PORT:
        {
            /*
             * Save the port in TrackInfo
             */
            PVMFRTSPTrackInfo* trackInfo =
                FindTrackInfo(cmdContextData->portContext.trackID);
            OSCL_ASSERT(trackInfo);

            PVMFPortInterface* port =
                (PVMFPortInterface*)aResponse.GetEventData();

            if (cmdContextData->portContext.portTag ==
                    PVMF_RTSP_NODE_PORT_TYPE_OUTPUT)
            {
                if (trackInfo)
                    trackInfo->iSessionControllerOutputPort = port;
                iSessionControllerNodeContainer->iOutputPorts.push_back(port);
            }
            else if (cmdContextData->portContext.portTag ==
                     PVMF_RTSP_NODE_PORT_TYPE_INPUT)
            {
                iSessionControllerNodeContainer->iInputPorts.push_back(port);
            }
            else if (cmdContextData->portContext.portTag ==
                     PVMF_RTSP_NODE_PORT_TYPE_INPUT_OUTPUT)
            {
                if (trackInfo)
                    trackInfo->iSessionControllerFeedbackPort = port;
                iSessionControllerNodeContainer->iFeedBackPorts.push_back(port);
            }
            CompleteGraphConstruct();
        }
        break;
        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_PLAYLIST_PLAY:
        {
            // In case of 3GPP FCS with switch SDP available or in case of 202 response processing, all the information
            // needed to transition to the next clip is available in the FCS response.
            // There is no need to wait for an entity body (and thus the event PVMFInfoPlayListClipTransition).
            if (iPVMFDataSourcePositionParamsPtr->iMode == PVMF_SET_DATA_SOURCE_POSITION_MODE_3GPP_FCS)
            {
                // If switch SDP is not available locally and not processing 202 response,
                // wait for entity body processing.
                if (!iPVMFDataSourcePositionParamsPtr->iSDPAvailable && !iSelectTracksAndSendFCSRequestFlag)
                {
                    iPlaylistPlayInProgress = true;
                }
                else
                {
                    // Save switch SDP info
                    iSdpInfo = iSwitchSDPInfo;

                    // Reset iSelectTracksAndSendFCSRequestFlag
                    iSelectTracksAndSendFCSRequestFlag = false;
                }

                // Check to see if tracks were removed
                if (iRemovedTrackIDVector.size())
                {
                    // If tracks were removed, need to send EOS on those ports
                    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
                    OSCL_ASSERT(iJitterBufferNodeContainer);
                    if (!iJitterBufferNodeContainer)
                        return;
                    PVMFJitterBufferExtensionInterface* jbExtIntf =
                        (PVMFJitterBufferExtensionInterface*)iJitterBufferNodeContainer->iExtensions[0];
                    OSCL_ASSERT(jbExtIntf);
                    if (!jbExtIntf)
                        return;
                    jbExtIntf->SendEOSMessage(iRemovedTrackIDVector);
                }

                // Send the PVMFInfoPlayListSwitch info event to the engine
                ReportInfoEvent(PVMFInfoPlayListSwitch);

                // Update RTP Info received in server response
                if (iPVMFDataSourcePositionParamsPtr->iSDPAvailable)
                {
                    UpdateRTPInfoAndFlushJitterBuffer();
                    iSessionControllerNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
                    CompleteStart();
                }
            }

            else
            {
                iSessionControllerNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
                CompleteStart();
            }
        }
        break;

        case PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_CANCEL_ALL_COMMANDS:
        {
            CompleteChildNodesCmdCancellation();
        }
        break;

        default:
            break;
    }
    return;
}

void PVMFSMRTSPUnicastNode::HandleJitterBufferCommandCompleted(const PVMFCmdResp& aResponse, bool& aPerformErrHandling)
{
    aPerformErrHandling = false;
    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (iJitterBufferNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return;
    }

    PVMFSMFSPCommandContext *cmdContextData =
        OSCL_REINTERPRET_CAST(PVMFSMFSPCommandContext*, aResponse.GetContext());
    cmdContextData->oFree = true;

    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleJitterBufferCommandCompleted In - cmd [%d] iJitterBufferNodeContainer->iNodeCmdState [%d] iInterfaceState[%d]", cmdContextData->cmd, iJitterBufferNodeContainer->iNodeCmdState, iInterfaceState));

    OSCL_ASSERT(cmdContextData->cmd != PVMF_SM_FSP_JITTER_BUFFER_QUERY_UUID);
    OSCL_ASSERT(cmdContextData->cmd != PVMF_SM_FSP_JITTER_BUFFER_QUERY_INTERFACE);


    if (iJitterBufferNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_PENDING)
    {
        if (cmdContextData->cmd == PVMF_SM_FSP_JITTER_BUFFER_REQUEST_PORT)
        {
            //This is last of the request ports
            OSCL_ASSERT(iJitterBufferNodeContainer->iNumRequestPortsPending > 0);
            if (--iJitterBufferNodeContainer->iNumRequestPortsPending == 0)
            {
                iJitterBufferNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
            }
        }
        else
        {
            iJitterBufferNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
        }
    }
    else if (iJitterBufferNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_CANCEL_PENDING)
    {
        if ((cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCELALLCOMMANDS) || (cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCELCOMMAND) || (cmdContextData->parentCmd == PVMF_SMFSP_NODE_CANCEL_DUE_TO_ERROR))
        {
            iJitterBufferNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
        }
        else
        {
            PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleJitterBufferCommandCompleted cmd completion for cmd other than cancel during cancellation"));
            //if cancel is pending and if the parent cmd is not cancel then this is
            //is most likely the cmd that is being cancelled.
            //we ignore cmd completes from child nodes if cancel is pending
            //we simply wait on cancel complete and cancel the pending cmd
            return;
        }
    }
    else if (iJitterBufferNodeContainer->iNodeCmdState == PVMFSMFSP_NODE_CMD_IDLE)
    {
        PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::HandleJitterBufferCommandCompleted cmd container in IDLE state already"));
        /*
         * This is to handle a usecase where a node reports cmd complete for cancelall first
         * and then reports cmd complete on the cmd that was meant to be cancelled.
         * There are two possible scenarios that could arise based on this:
         * i) SM node has reported cmd complete on both canceall and the cmd meant to be cancelled
         * to engine by the time cmd complete on the cmd that was meant to be cancelled arrives
         * from the child node. In this case iNodeCmdState would be PVMFSMFSP_NODE_CMD_NO_PENDING.
         * ii) SM node is still waiting on some other child nodes to complete cancelall.
         * In this case iNodeCmdState would be PVMFSMFSP_NODE_CMD_IDLE.
         * In either case iNodeCmdState cannot be PVMFSMFSP_NODE_CMD_PENDING or PVMFSMFSP_NODE_CMD_IDLE
         * (recall that we call ResetNodeContainerCmdState  prior to issuing cancelall)
         * Or this is the case of node reporting cmd complete multiple times for a cmd, which
         * also can be ignored
         */
        return;
    }

    if (EPVMFNodeError == iInterfaceState)//If interface is in err state, let the err handler do processing
    {
        aPerformErrHandling = true;
        return;
    }

    if (aResponse.GetCmdStatus() != PVMFSuccess)
    {
        if (aResponse.GetCmdStatus() != PVMFErrCancelled)
        {
            aPerformErrHandling = true;
        }

        PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::HandleJitterBufferCommandCompleted - Command failed - context=0x%x, status=0x%x", aResponse.GetContext(), aResponse.GetCmdStatus()));
        if (IsBusy())
        {
            Cancel();
            RunIfNotReady();
        }
        return;
    }

    switch (cmdContextData->cmd)
    {
        case PVMF_SM_FSP_JITTER_BUFFER_INIT:
        {
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)
                (iJitterBufferNodeContainer->iExtensions[0]);
            if (jbExtIntf == NULL)
            {
                OSCL_LEAVE(OsclErrBadHandle);
                return;
            }
            bool disableFireWallPackets = true;
            if ((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL) ||
                    (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE))
            {


                //do not disable fw pkts run time
                //apps can still disable it compile time or using KVP
                //for UDP sessions ofcourse
                disableFireWallPackets = false;

            }
            CompleteInit();
        }
        break;

        case PVMF_SM_FSP_JITTER_BUFFER_PREPARE:
            CompletePrepare();
            break;

        case PVMF_SM_FSP_JITTER_BUFFER_START:
        {
            /* If start has been cancelled wait for cancel success */
            if (aResponse.GetCmdStatus() != PVMFErrCancelled)
            {
                //Though start of Jb is complete i.e delay has been established. But JB node will be able to sent out the
                //packets from it with proper timestamps only when session controller start completes and configures the
                //JB node properly with playrange, rtp info etc.
                //So let us wait for SM node start complete which will be complete once all child nodes(including session controller) start complete.
                PVMFJitterBufferExtensionInterface* jbExtIntf =
                    (PVMFJitterBufferExtensionInterface*)(iJitterBufferNodeContainer->iExtensions[0]);
                jbExtIntf->StopOutputPorts();
                CompleteStart();
            }
        }
        break;

        case PVMF_SM_FSP_JITTER_BUFFER_STOP:
        {
            CompleteStop();
        }
        break;

        case PVMF_SM_FSP_JITTER_BUFFER_FLUSH:
            CompleteFlush();
            break;

        case PVMF_SM_FSP_JITTER_BUFFER_PAUSE:
            CompletePause();
            break;

        case PVMF_SM_FSP_JITTER_BUFFER_RESET:
            CompleteReset();
            break;

        case PVMF_SM_FSP_JITTER_BUFFER_REQUEST_PORT:
        {
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)
                (iJitterBufferNodeContainer->iExtensions[0]);

            if (jbExtIntf == NULL)
            {
                OSCL_LEAVE(OsclErrBadHandle);
                return;
            }

            /*
             * Save the port in TrackInfo
             */
            PVMFRTSPTrackInfo* trackInfo =
                FindTrackInfo(cmdContextData->portContext.trackID);

            PVMFPortInterface* port =
                (PVMFPortInterface*)aResponse.GetEventData();

            OSCL_ASSERT(trackInfo && port);
            if (!trackInfo || ! port)
            {
                return;
            }

            uint32 bitrate = 0;

            if (cmdContextData->portContext.portTag ==
                    PVMF_JITTER_BUFFER_PORT_TYPE_INPUT)
            {
                if (trackInfo)
                {
                    bitrate = trackInfo->bitRate;
                    trackInfo->iJitterBufferInputPort = port;
                }
                mediaInfo* mInfo = NULL;
                SDPInfo* sdpInfo = iSdpInfo.GetRep();
                if (sdpInfo == NULL)
                {
                    OSCL_LEAVE(OsclErrBadHandle);
                    return;
                }
                if (trackInfo)
                    mInfo = sdpInfo->getMediaInfoBasedOnID(trackInfo->trackID);
                jbExtIntf->setPortMediaParams(port, mInfo);
                iJitterBufferNodeContainer->iInputPorts.push_back(port);
            }
            else if (cmdContextData->portContext.portTag ==
                     PVMF_JITTER_BUFFER_PORT_TYPE_OUTPUT)
            {
                if (trackInfo)
                    trackInfo->iJitterBufferOutputPort = port;
                iJitterBufferNodeContainer->iOutputPorts.push_back(port);
            }
            else if (cmdContextData->portContext.portTag ==
                     PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK)
            {
                iJitterBufferNodeContainer->iFeedBackPorts.push_back(port);
                if (trackInfo)
                {
                    trackInfo->iJitterBufferRTCPPort = port;
                    if (trackInfo->iRTCPBwSpecified)
                    {
                        jbExtIntf->setPortRTCPParams(port, iTrackInfoVec.size(), trackInfo->iRR, trackInfo->iRS);
                    }
                }

            }
            jbExtIntf->setPortParams(port,
                                     trackInfo->trackTimeScale,
                                     bitrate,
                                     trackInfo->iTrackConfig,
                                     trackInfo->iRateAdaptation,
                                     trackInfo->iRateAdaptationFeedBackFrequency);
            CompleteGraphConstruct();
        }
        break;

        case PVMF_SM_FSP_JITTER_BUFFER_CANCEL_ALL_COMMANDS:
        {
            CompleteChildNodesCmdCancellation();
        }
        break;

        default:
            break;
    }
    return;
}

void PVMFSMRTSPUnicastNode::CompleteInit()
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::CompleteInit - In"));
    if (CheckChildrenNodesInit())
    {
        if (!iCurrentCommand.empty() && iCancelCommand.empty())
        {
            PVMFSMFSPBaseNodeCommand& aCmd = iCurrentCommand.front();
            if (aCmd.iCmd == PVMF_SMFSP_NODE_INIT)
            {
                PVMFStatus status = PVMFSuccess;

                if ((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL))
                {
                    status = ProcessSDP();
                }

                if (status == PVMFSuccess)
                {
                    status = InitMetaData();
                    if (status == PVMFSuccess)
                    {
                        if (iSessionSourceInfo->iDRMProtected == true)
                        {
                            PopulateDRMInfo();
                            //This clip is protected clip
                            if (iCPM)
                            {
                                /*
                                 * Go thru CPM commands before parsing the file in case
                                 * of a new source file.
                                 * - Init CPM
                                 * - Open Session
                                 * - Register Content
                                 * - Get Content Type
                                 * - Approve Usage
                                 * In case the source file has already been parsed skip to
                                 * - Approve Usage
                                 */
                                InitCPM();
                                return;
                            }
                            else
                            {
                                PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::Init() - CmdComplete - PVMFErrArgument"));
                                CommandComplete(iCurrentCommand,
                                                iCurrentCommand.front(),
                                                PVMFErrArgument,
                                                NULL, NULL, NULL);
                                return;
                            }
                        }
                    }
                    else
                    {
                        PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::CompleteInit - InitMetaData fail"));
                    }

                    PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::Init() - CmdComplete - PVMFSuccess"));
                    //Init is completed at unprotected clip
                    SetState(EPVMFNodeInitialized);
                    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::CompleteInit Success"));
                    CommandComplete(iCurrentCommand, aCmd, PVMFSuccess);
                }
                else
                {
                    PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::Init() - Cmd Failed - PVMFStreamingManagerNodeErrorParseSDPFailed"));
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::CompleteInit Failure"));
                    PVUuid eventuuid = PVMFStreamingManagerNodeEventTypeUUID;
                    int32 errcode = PVMFStreamingManagerNodeErrorParseSDPFailed;
                    CommandComplete(iCurrentCommand, aCmd, status, NULL, &eventuuid, &errcode);
                }
            }
        }
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFPVMFSMRTSPUnicastNode::CompleteInit - Out"));
    return;
}

bool PVMFSMRTSPUnicastNode::CheckChildrenNodesInit()
{
    for (uint32 i = 0; i < iFSPChildNodeContainerVec.size(); i++)
    {
        if (iFSPChildNodeContainerVec[i].iNodeCmdState != PVMFSMFSP_NODE_CMD_IDLE)
        {
            return false;
        }
    }
    return true;
}

PVMFStatus PVMFSMRTSPUnicastNode::InitMetaData()
{
    // Clear out the existing key list
    iAvailableMetadataKeys.clear();

    // Get the SDP info
    SDPInfo* sdpInfo = iSdpInfo.GetRep();
    if (sdpInfo == NULL)
    {
        return PVMFErrInvalidState;
    }
    // Get Asset Info
    sessionDescription* sessionInfo = sdpInfo->getSessionInfo();
    if (sessionInfo != NULL)
    {
        iMetaDataInfo->iRandomAccessDenied = sessionInfo->getRandomAccessDenied();

        AssetInfoType assetInfo = sessionInfo->getAssetInfo();

        iMetaDataInfo->iTitlePresent = assetInfo.oTitlePresent;
        iMetaDataInfo->iDescriptionPresent = assetInfo.oDescriptionPresent;
        iMetaDataInfo->iCopyRightPresent = assetInfo.oCopyRightPresent;
        iMetaDataInfo->iPerformerPresent = assetInfo.oPerformerPresent;
        iMetaDataInfo->iAuthorPresent = assetInfo.oAuthorPresent;
        iMetaDataInfo->iGenrePresent = assetInfo.oGenrePresent;
        iMetaDataInfo->iRatingPresent = assetInfo.oRatingPresent;
        iMetaDataInfo->iClassificationPresent = assetInfo.oClassificationPresent;
        iMetaDataInfo->iKeyWordsPresent = assetInfo.oKeyWordsPresent;
        iMetaDataInfo->iLocationPresent = assetInfo.oLocationPresent;
        iMetaDataInfo->iAlbumPresent = assetInfo.oAlbumPresent;

        if (iMetaDataInfo->iTitlePresent)
        {
            iMetaDataInfo->iTitle = assetInfo.Box[AssetInfoType::TITLE];
        }
        if (iMetaDataInfo->iDescriptionPresent)
        {
            iMetaDataInfo->iDescription = assetInfo.Box[AssetInfoType::DESCRIPTION];
        }
        if (iMetaDataInfo->iCopyRightPresent)
        {
            iMetaDataInfo->iCopyright = assetInfo.Box[AssetInfoType::COPYRIGHT];
        }
        if (iMetaDataInfo->iPerformerPresent)
        {
            iMetaDataInfo->iPerformer = assetInfo.Box[AssetInfoType::PERFORMER];
        }
        if (iMetaDataInfo->iAuthorPresent)
        {
            iMetaDataInfo->iAuthor = assetInfo.Box[AssetInfoType::AUTHOR];
        }
        if (iMetaDataInfo->iRatingPresent)
        {
            iMetaDataInfo->iRating = assetInfo.Box[AssetInfoType::RATING];
        }
        if (iMetaDataInfo->iClassificationPresent)
        {
            iMetaDataInfo->iClassification = assetInfo.Box[AssetInfoType::CLASSIFICATION];
        }
        if (iMetaDataInfo->iKeyWordsPresent)
        {
            iMetaDataInfo->iNumKeyWords = assetInfo.iNumKeyWords;
            for (uint32 i = 0; i < iMetaDataInfo->iNumKeyWords; i++)
            {
                iMetaDataInfo->iKeyWords[i] = assetInfo.KeyWords[i];
            }
        }
        if (iMetaDataInfo->iLocationPresent)
        {
            iMetaDataInfo->iLocationStruct = assetInfo.iLocationStruct;
        }
        if (iMetaDataInfo->iAlbumPresent)
        {
            iMetaDataInfo->iAlbum = assetInfo.Box[AssetInfoType::ALBUM];
        }
        if (assetInfo.iRecordingYear)
        {
            iMetaDataInfo->iYear = assetInfo.iRecordingYear;
        }

        RtspRangeType *sessionRange = OSCL_CONST_CAST(RtspRangeType*, (sessionInfo->getRange()));
        if (sessionRange->end_is_set == true)
        {
            iMetaDataInfo->iSessionDurationAvailable = true;

            int32 sessionStartTime = 0, sessionStopTime = 0;
            sessionRange->convertToMilliSec(sessionStartTime, sessionStopTime);
            uint32 duration = 0;
            if (sessionStopTime > sessionStartTime && sessionStartTime >= 0)
            {
                duration = (uint32)(sessionStopTime);
            }
            Oscl_Int64_Utils::set_uint64(iMetaDataInfo->iSessionDuration, 0, duration);
            iMetaDataInfo->iSessionDurationTimeScale = 1000;
        }

        iPauseDenied = iMetaDataInfo->iSessionDurationAvailable ? false : true;

    }

    iMetaDataInfo->iNumTracks = sdpInfo->getNumMediaObjects();

    for (uint32 i = 0; i < iMetaDataInfo->iNumTracks; i++)
    {
        Oscl_Vector<mediaInfo*, SDPParserAlloc> mediaInfoVec = sdpInfo->getMediaInfo(i);
        for (uint32 j = 0; j < mediaInfoVec.size(); ++j)
        {
            mediaInfo* mInfo = mediaInfoVec[j];
            if (mInfo != NULL)
            {
                PVMFSMTrackMetaDataInfo trackMetaDataInfo;

                trackMetaDataInfo.iTrackID = mInfo->getMediaInfoID();
                const char* mimeType = mInfo->getMIMEType();
                OSCL_StackString<32> realAudio(_STRLIT_CHAR("realaudio"));
                OSCL_StackString<32> realVideo(_STRLIT_CHAR("realvideo"));
                OSCL_StackString<32> h263(_STRLIT_CHAR("H263"));
                if (oscl_strstr(mimeType, realAudio.get_cstr()) != NULL)
                {
                    trackMetaDataInfo.iMimeType = PVMF_MIME_REAL_AUDIO;
                }
                else if (oscl_strstr(mimeType, realVideo.get_cstr()) != NULL)
                {
                    trackMetaDataInfo.iMimeType = PVMF_MIME_REAL_VIDEO;
                }
                else
                {
                    trackMetaDataInfo.iMimeType += mimeType;
                }

                Oscl_Vector<PayloadSpecificInfoTypeBase*, SDPParserAlloc> payloadVector;
                payloadVector = mInfo->getPayloadSpecificInfoVector();
                if (payloadVector.size() != 0)
                {
                    /*
                     * There can be multiple payloads per media segment.
                     * We only support one for now, so
                     * use just the first payload
                     */
                    PayloadSpecificInfoTypeBase* payloadInfo = payloadVector[0];
                    if (oscl_strstr(mimeType, h263.get_cstr()) != NULL)
                    {
                        H263PayloadSpecificInfoType* h263PayloadInfo =
                            OSCL_STATIC_CAST(H263PayloadSpecificInfoType*, payloadInfo);
                        trackMetaDataInfo.iTrackWidth = h263PayloadInfo->getFrameWidth();
                        trackMetaDataInfo.iTrackHeight = h263PayloadInfo->getFrameHeight();
                    }

                    int32 configSize = payloadInfo->configSize;
                    OsclAny* config = payloadInfo->configHeader.GetRep();

                    if ((configSize > 0) && (config != NULL))
                    {
                        OsclMemAllocDestructDealloc<uint8> my_alloc;
                        OsclRefCounter* my_refcnt;
                        uint aligned_refcnt_size =
                            oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));

                        uint8* my_ptr = GetMemoryChunk(my_alloc, aligned_refcnt_size + configSize);
                        if (!my_ptr)
                            return PVMFErrResource;

                        my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
                        my_ptr += aligned_refcnt_size;

                        OsclMemoryFragment memfrag;
                        memfrag.len = (uint32)configSize;
                        memfrag.ptr = my_ptr;

                        oscl_memcpy((void*)(memfrag.ptr), (const void*)config, memfrag.len);

                        OsclRefCounterMemFrag tmpRefcntMemFrag(memfrag, my_refcnt, memfrag.len);
                        trackMetaDataInfo.iCodecSpecificInfo = tmpRefcntMemFrag;
                    }
                }

                trackMetaDataInfo.iTrackBitRate = (uint32)(mInfo->getBitrate());

                RtspRangeType *mediaRange = mInfo->getRtspRange();
                if (mediaRange->end_is_set == true)
                {
                    int32 mediaStartTime = 0, mediaStopTime = 0;
                    mediaRange->convertToMilliSec(mediaStartTime, mediaStopTime);
                    uint32 trackduration = 0;
                    if (mediaStopTime > mediaStartTime && mediaStartTime >= 0)
                    {
                        trackduration = (uint32)(mediaStopTime - mediaStartTime);
                    }
                    uint64 trackduration64 = 0;
                    Oscl_Int64_Utils::set_uint64(trackduration64, 0, trackduration);
                    trackMetaDataInfo.iTrackDuration = trackduration64;
                    trackMetaDataInfo.iTrackDurationTimeScale = 1000;
                    trackMetaDataInfo.iTrackDurationAvailable = true;
                }
                else
                {
                    trackMetaDataInfo.iTrackDurationAvailable = false;
                }
                iMetaDataInfo->iTrackMetaDataInfoVec.push_back(trackMetaDataInfo);
            }
        }
    }

    PVMFStatus status = PopulateAvailableMetadataKeys();
    if (PVMFSuccess != status)
    {
        return status;
    }
    iMetaDataInfo->iMetadataAvailable = true;
    return PVMFSuccess;
}

PVMFStatus PVMFSMRTSPUnicastNode::PopulateAvailableMetadataKeys()
{
    int32 leavecode = OsclErrNone;
    OSCL_TRY(leavecode,
             PVMFSMFSPBaseNode::PopulateAvailableMetadataKeys();

             //Add feature specific streaming metadata keys
             // Create the parameter string for the index range
             if (iMetaDataInfo->iNumTracks > 0)
{
    char indexparam[18];
        oscl_snprintf(indexparam, 18, ";index=0...%d", (iMetaDataInfo->iNumTracks - 1));
        indexparam[17] = NULL_TERM_CHAR;

        iAvailableMetadataKeys.push_front(PVMFSTREAMINGMGRNODE_TRACKINFO_BITRATE_KEY);
        iAvailableMetadataKeys[0] += indexparam;
    }
    iAvailableMetadataKeys.push_back(PVMFSTREAMINGMGRNODE_PAUSE_DENIED_KEY);
            );
    if (leavecode != OsclErrNone)
        return leavecode;
    else
        return PVMFSuccess;
}

void PVMFSMRTSPUnicastNode::CompletePrepare()
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::CompletePrepare - In"));
    if ((CheckChildrenNodesPrepare()) && (iGraphConstructComplete))
    {
        if (!iCurrentCommand.empty() && iCancelCommand.empty())
        {
            PVMFSMFSPBaseNodeCommand& aCmd = iCurrentCommand.front();
            if (aCmd.iCmd == PVMF_SMFSP_NODE_PREPARE)
            {
                SetState(EPVMFNodePrepared);
                PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::Prepare() - CmdComplete - PVMFSuccess"));
                CommandComplete(iCurrentCommand, aCmd, PVMFSuccess);
            }
        }
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFPVMFSMRTSPUnicastNode::CompletePrepare - Out"));
    return;
}

bool PVMFSMRTSPUnicastNode::CheckChildrenNodesPrepare()
{
    for (uint32 i = 0; i < iFSPChildNodeContainerVec.size(); i++)
    {
        if (iFSPChildNodeContainerVec[i].iNodeCmdState != PVMFSMFSP_NODE_CMD_IDLE)
        {
            return false;
        }
    }
    return true;
}

void PVMFSMRTSPUnicastNode::CompleteStart()
{
    int32 localMode = 0;

    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::CompleteStart - In"));
    if (CheckChildrenNodesStart())
    {
        if (!iCurrentCommand.empty() && iCancelCommand.empty())
        {
            //Start of all the child nodes completed.
            //Let us start the output ports of the JB node.
            PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            OSCL_ASSERT(iJitterBufferNodeContainer);
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)(iJitterBufferNodeContainer->iExtensions[0]);
            jbExtIntf->StartOutputPorts();
            PVMFSMFSPBaseNodeCommand& aCmd = iCurrentCommand.front();
            if ((aCmd.iCmd == PVMF_SMFSP_NODE_START) ||
                    (aCmd.iCmd == PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION))
            {
                if (iRepositioning)
                {
                    PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::SetDataSourcePosition() - CmdComplete - PMVFSuccess"));
                    iRepositioning = false;
                    iPlayListRepositioning = false;

                    if ((localMode == 0) || (localMode == -1))
                    {
                        GetActualMediaTSAfterSeek();
                    }
                    iPVMFDataSourcePositionParamsPtr = NULL;
                }
                if ((localMode == 0) || (localMode == -1))
                {
                    SetState(EPVMFNodeStarted);
                    if (IsAdded())
                    {
                        /* wakeup the AO */
                        RunIfNotReady();
                    }
                    PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::Start() - CmdComplete - PMVFSuccess"));
                }
                CommandComplete(iCurrentCommand, aCmd, PVMFSuccess);
            }
        }
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::CompleteStart - Out"));
    return;
}

bool PVMFSMRTSPUnicastNode::CheckChildrenNodesStart()
{
    for (uint32 i = 0; i < iFSPChildNodeContainerVec.size(); i++)
    {
        if (iFSPChildNodeContainerVec[i].iNodeCmdState == PVMFSMFSP_NODE_CMD_PENDING)
        {
            return false;
        }
    }
    return true;
}

void PVMFSMRTSPUnicastNode::GetActualMediaTSAfterSeek()
{
    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (iJitterBufferNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return;
    }
    PVMFJitterBufferExtensionInterface* jbExtIntf =
        (PVMFJitterBufferExtensionInterface*)
        (iJitterBufferNodeContainer->iExtensions[0]);

    iActualMediaDataTS = jbExtIntf->getActualMediaDataTSAfterSeek();
    if (iActualMediaDataTSPtr != NULL)
    {
        *iActualMediaDataTSPtr = iActualMediaDataTS;
        PVMF_SM_RTSP_LOG_COMMAND_REPOS((0, "PVMFPVMFSMRTSPUnicastNode::GetActualMediaTSAfterSeek - TargetNPT = %d, ActualNPT=%d, ActualMediaDataTS=%d",
                                        iRepositionRequestedStartNPTInMS, *iActualRepositionStartNPTInMSPtr, *iActualMediaDataTSPtr));
    }
}

void PVMFSMRTSPUnicastNode::CompleteStop()
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::CompleteStop - In"));
    if (CheckChildrenNodesStop())
    {
        if (!iCurrentCommand.empty() && iCancelCommand.empty())
        {
            PVMFSMFSPBaseNodeCommand& aCmd = iCurrentCommand.front();
            if (aCmd.iCmd == PVMF_SMFSP_NODE_STOP)
            {
                /* transition to Prepared state */
                ResetStopCompleteParams();
                SetState(EPVMFNodePrepared);
                PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFSMRTSPUnicastNode::Stop() - CmdComplete - PVMFSuccess"));
                CommandComplete(iCurrentCommand, aCmd, PVMFSuccess);
            }
        }
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::CompleteStop - Out"));
    return;
}

void PVMFSMRTSPUnicastNode::ResetStopCompleteParams()
{
    iPlaylistPlayInProgress = false;
    iRepositionRequestedStartNPTInMS = 0;
}

void PVMFSMRTSPUnicastNode::CompletePause()
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFPVMFSMRTSPUnicastNode::CompletePause - In"));
    if (CheckChildrenNodesPause())
    {
        SetState(EPVMFNodePaused);
        if (iRepositioning)
        {
            OSCL_ASSERT((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL) ||
                        (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE));
            /*
             * Pause request generated by a reposition command
             * complete. Issue a start.
             */
            if (iPVMFDataSourcePositionParamsPtr == NULL)
            {
                DoRepositioningStart3GPPStreaming();
            }
        }
        else
        {
            if (!iCurrentCommand.empty() && iCancelCommand.empty())
            {
                PVMFSMFSPBaseNodeCommand& aCmd = iCurrentCommand.front();
                PVMF_SM_RTSP_LOG_COMMAND_SEQ((0, "PVMFPVMFSMRTSPUnicastNode::Pause() - CmdComplete - PVMFSuccess"));
                CommandComplete(iCurrentCommand, aCmd, PVMFSuccess);
            }
        }

    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFPVMFSMRTSPUnicastNode::CompletePause - Out"));
    return;
}

bool PVMFSMRTSPUnicastNode::CheckChildrenNodesPause()
{
    for (uint32 i = 0; i < iFSPChildNodeContainerVec.size(); i++)
    {
        if (iFSPChildNodeContainerVec[i].iNodeCmdState != PVMFSMFSP_NODE_CMD_IDLE
                && iFSPChildNodeContainerVec[i].iNodeCmdState != PVMFSMFSP_NODE_CMD_CANCEL_PENDING)
        {
            return false;
        }
    }
    return true;
}

/*
 * Called by the call back routine whenever a "RequestPort" call
 * completes successfully.
 */
void PVMFSMRTSPUnicastNode::CompleteGraphConstruct()
{
    iTotalNumRequestPortsComplete++;
    /*
     * Once all port requests are complete, connect the graph
     */
    if (iTotalNumRequestPortsComplete == iNumRequestPortsPending)
    {
        PVMFSMFSPBaseNodeCommand& aCmd = iCurrentCommand.front();
        if (!SendSessionSourceInfoToSessionController())
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:CompleteGraphConstruct - SendSessionSourceInfoToSessionController Failed"));
            InternalCommandComplete(iCurrentCommand, aCmd, PVMFFailure);
        }
        else
        {
            iGraphConstructComplete = true;
            PVMFSMFSPChildNodeContainer* socketNodeContainer = getChildNodeContainer(PVMF_SM_FSP_SOCKET_NODE);
            PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer = getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            if ((socketNodeContainer == NULL) || (jitterBufferNodeContainer == NULL))
            {
                OSCL_ASSERT(false);
                return;
            }

            PVMFJitterBufferExtensionInterface* jbExtIntf = OSCL_STATIC_CAST(PVMFJitterBufferExtensionInterface*, jitterBufferNodeContainer->iExtensions[0]);
            PVMFSocketNodeExtensionInterface* socketExtnIntf =  OSCL_STATIC_CAST(PVMFSocketNodeExtensionInterface*, socketNodeContainer->iExtensions[0]);

            OSCL_ASSERT(jbExtIntf && socketExtnIntf);
            if (!jbExtIntf || !socketExtnIntf)
                return;

            //For RTSP based streamings, we want the mempools to get resized
            //For input port, get the value from Jitter Buffer Node
            uint32 resizeSizeInputPort = 0;
            uint32 numResizeInputPort = 0;
            jbExtIntf->GetSharedBufferResizeParams(numResizeInputPort, resizeSizeInputPort);
            //For feedback port we use the const initialized to a configurable macro
            const uint32 resizeSizeFeedbackPort = DEFAULT_MAX_SOCKETMEMPOOL_RESIZELEN_FEEDBACK_PORT;
            const uint32 numResizeFeedbackPort = 1;

            //For every track
            //Decide the size of the shared buffer to be allocated at socket node for receiving
            //RTP packets [Input port]
            //Receiving the response of RTCP reports send by JB [Feedback Port]

            for (uint32 i = 0; i < iTrackInfoVec.size(); i++)
            {
                PVMFRTSPTrackInfo trackInfo = iTrackInfoVec[i];
                uint32 bufferSzForRTPPackets = GetJitterBufferMemPoolSize(PVMF_JITTER_BUFFER_PORT_TYPE_INPUT, iTrackInfoVec[i]);
                uint32 bufferSzForRTCPResponse = GetJitterBufferMemPoolSize(PVMF_JITTER_BUFFER_PORT_TYPE_FEEDBACK, iTrackInfoVec[i]);

                //Set with the socket node
                OsclMemPoolResizableAllocator* socketAlloc = socketExtnIntf->CreateSharedBuffer(trackInfo.iNetworkNodePort, bufferSzForRTPPackets, DEFAULT_NUM_MEDIA_MSGS_IN_JITTER_BUFFER, resizeSizeInputPort, numResizeInputPort);
                if (socketAlloc == NULL)
                {
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:CompleteGraphConstruct - CreateSharedBuffer Failed"));
                    InternalCommandComplete(iCurrentCommand, aCmd, PVMFFailure);
                    return;
                }
                jbExtIntf->SetJitterBufferChunkAllocator(socketAlloc, trackInfo.iJitterBufferInputPort);
                const uint32 numMemChunksForRTCPMemPool = bufferSzForRTCPResponse / MAX_SOCKET_BUFFER_SIZE;
                socketExtnIntf->CreateSharedBuffer(trackInfo.iNetworkNodeRTCPPort, bufferSzForRTCPResponse, numMemChunksForRTCPMemPool, resizeSizeFeedbackPort, numResizeFeedbackPort);
            }
            InternalCommandComplete(iCurrentCommand, aCmd, PVMFSuccess);
        }
    }
}

/*
 * Called when all port requests are complete, in order to send the
 * UDP port information to RTSP
 */
bool PVMFSMRTSPUnicastNode::SendSessionSourceInfoToSessionController()
{
    if (((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL) ||
            (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE)))
    {
        PVMFSMFSPChildNodeContainer* iSocketNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_SOCKET_NODE);
        if (iSocketNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return false;
        }

        PVMFSocketNode* socketNode =
            (PVMFSocketNode*)(iSocketNodeContainer->iNode);

        PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);

        if (iSessionControllerNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return false;
        }

        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            (PVRTSPEngineNodeExtensionInterface*)
            (iSessionControllerNodeContainer->iExtensions[0]);

        Oscl_Vector<StreamInfo, OsclMemAllocator> aSelectedStream;

        for (uint32 i = 0; i < iTrackInfoVec.size(); i++)
        {
            PVMFRTSPTrackInfo trackInfo = iTrackInfoVec[i];

            OsclNetworkAddress localAdd;
            OsclNetworkAddress remoteAdd;
            StreamInfo sInfo;

            sInfo.iSDPStreamId = trackInfo.trackID;


            if (trackInfo.iNetworkNodePort == NULL)
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:SendSessionSourceInfoToSessionController - Invalid Port"));
                return false;
            }

            socketNode->GetPortConfig(*trackInfo.iNetworkNodePort,
                                      localAdd,
                                      remoteAdd);

            sInfo.iCliRTPPort = localAdd.port;

            socketNode->GetPortConfig(*trackInfo.iNetworkNodeRTCPPort,
                                      localAdd,
                                      remoteAdd);

            sInfo.iCliRTCPPort = localAdd.port;


            /* Set Rate Adaptation parameters */
            sInfo.b3gppAdaptationIsSet = false;
            if (trackInfo.iRateAdaptation)
            {
                sInfo.b3gppAdaptationIsSet = true;
                /* Compute buffer size based on bitrate and jitter duration*/
                uint32 sizeInBytes = MIN_RTP_SOCKET_MEM_POOL_SIZE_IN_BYTES;
                if (((int32)iJitterBufferDurationInMilliSeconds > 0) &&
                        ((int32)trackInfo.bitRate > 0))
                {
                    uint32 byteRate = trackInfo.bitRate / 8;
                    uint32 overhead = (byteRate * PVMF_JITTER_BUFFER_NODE_MEM_POOL_OVERHEAD) / 100;
                    uint32 durationInSec = iJitterBufferDurationInMilliSeconds / 1000;
                    sizeInBytes = ((byteRate + overhead) * durationInSec);
                    if (sizeInBytes < MIN_RTP_SOCKET_MEM_POOL_SIZE_IN_BYTES)
                    {
                        sizeInBytes = MIN_RTP_SOCKET_MEM_POOL_SIZE_IN_BYTES;
                    }
                    sizeInBytes += 2 * MAX_SOCKET_BUFFER_SIZE;
                }
                sInfo.iBufSize = sizeInBytes;
                sInfo.iTargetTime = iJitterBufferDurationInMilliSeconds;
            }
            aSelectedStream.push_back(sInfo);
        }

        if (rtspExtIntf->SetSDPInfo(iSdpInfo,
                                    aSelectedStream) != PVMFSuccess)
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:SendSessionSourceInfoToSessionController - SetSDPInfo Failed"));
            return false;
        }

        /* Set play range from SDP */
        sessionDescription* sessionInfo = iSdpInfo->getSessionInfo();
        RtspRangeType *rtspRange = OSCL_CONST_CAST(RtspRangeType*, (sessionInfo->getRange()));
        rtspRange->convertToMilliSec((int32&)iSessionStartTime, (int32&)iSessionStopTime);

        if (rtspRange->end_is_set == false)
        {
            iSessionStopTime = 0xFFFFFFFF;
            iSessionStopTimeAvailable = false;
        }

        if ((rtspRange->format != RtspRangeType::INVALID_RANGE) &&
                (rtspRange->start_is_set != false))
        {
            if (rtspExtIntf->SetRequestPlayRange(*rtspRange) != PVMFSuccess)
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:SendRequestPlayRangeToSessionController - SetRequestPlayRange Failed"));
                return false;
            }
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
//Implemenation of pure virtuals from PVMFSMFSPBaseNode
///////////////////////////////////////////////////////////////////////////////
bool PVMFSMRTSPUnicastNode::IsFSPInternalCmd(PVMFCommandId aId)
{
    OSCL_UNUSED_ARG(aId);
    return false;
}

void PVMFSMRTSPUnicastNode::addRef()
{
}

void PVMFSMRTSPUnicastNode::removeRef()
{
}

void PVMFSMRTSPUnicastNode::setJitterBufferDurationInMilliSeconds(uint32 duration)
{
    iJitterBufferDurationInMilliSeconds = duration;
}

PVMFStatus PVMFSMRTSPUnicastNode::DoRepositioningStart3GPPStreaming()
{
    PVMFStatus status = SetRTSPPlaybackRange();
    if (PVMFSuccess != status)
    {
        return status;
    }

    bool isSessionDurationExpired = false;

    PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (jitterBufferNodeContainer)
    {
        PVMFJitterBufferExtensionInterface* jbExtIntf =
            OSCL_STATIC_CAST(PVMFJitterBufferExtensionInterface*, jitterBufferNodeContainer->iExtensions.front());
        if (jbExtIntf)
            jbExtIntf->HasSessionDurationExpired(isSessionDurationExpired);
    }

    PVMFSMFSPChildNodeContainer *sessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
    if (sessionControllerNodeContainer)
    {
        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            OSCL_STATIC_CAST(PVRTSPEngineNodeExtensionInterface*, sessionControllerNodeContainer->iExtensions.front());
        if (rtspExtIntf)
            rtspExtIntf->UpdateSessionCompletionStatus(isSessionDurationExpired);
    }

    PVMFSMFSPChildNodeContainerVector::iterator it;
    for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
    {
        PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
        if (internalCmd != NULL)
        {
            internalCmd->cmd =
                it->commandStartOffset +
                PVMF_SM_FSP_NODE_INTERNAL_START_CMD_OFFSET;
            internalCmd->parentCmd = PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION;

            OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

            PVMFNodeInterface* iNode = it->iNode;

            iNode->Start(it->iSessionId, cmdContextData);
            it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
        }
        else
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoRepositioningStart:RequestNewInternalCmd - Failed"));
            status = PVMFErrNoMemory;
        }
    }
    return status;
}

PVMFStatus PVMFSMRTSPUnicastNode::SetRTSPPlaybackRange()
{
    PVMFStatus status = PVMFSuccess;

    /* Set Requested Play Range */
    PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);

    if (iSessionControllerNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return false;
    }

    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
        (PVRTSPEngineNodeExtensionInterface*)
        (iSessionControllerNodeContainer->iExtensions[0]);

    RtspRangeType rtspRange;
    rtspRange.format = RtspRangeType::NPT_RANGE;
    rtspRange.start_is_set = true;
    rtspRange.npt_start.npt_format = NptTimeFormat::NPT_SEC;
    rtspRange.npt_start.npt_sec.sec = iRepositionRequestedStartNPTInMS / 1000;
    rtspRange.npt_start.npt_sec.milli_sec =
        (iRepositionRequestedStartNPTInMS - ((iRepositionRequestedStartNPTInMS / 1000) * 1000));
    rtspRange.end_is_set = iSessionStopTimeAvailable;
    if (true == iSessionStopTimeAvailable)
    {
        rtspRange.npt_end.npt_format = NptTimeFormat::NPT_SEC;
        rtspRange.npt_end.npt_sec.sec = iSessionStopTime / 1000;
        rtspRange.npt_end.npt_sec.milli_sec =
            (iSessionStopTime - ((iSessionStopTime / 1000) * 1000));
    }
    status = rtspExtIntf->SetRequestPlayRange(rtspRange);
    if (PVMFSuccess != status)
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:SetRTSPPlaybackRange() - SetRequestPlayRange Failed"));
    }

    return status;
}

bool PVMFSMRTSPUnicastNode::DoRepositioningPause3GPPStreaming()
{
    PVMFSMFSPChildNodeContainerVector::iterator it;

    bool isSessionDurationExpired = false;

    PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (jitterBufferNodeContainer)
    {
        PVMFJitterBufferExtensionInterface* jbExtIntf =
            OSCL_STATIC_CAST(PVMFJitterBufferExtensionInterface*, jitterBufferNodeContainer->iExtensions.front());
        if (jbExtIntf)
            jbExtIntf->HasSessionDurationExpired(isSessionDurationExpired);
    }

    PVMFSMFSPChildNodeContainer *sessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
    if (sessionControllerNodeContainer)
    {
        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            OSCL_STATIC_CAST(PVRTSPEngineNodeExtensionInterface*, sessionControllerNodeContainer->iExtensions.front());
        if (rtspExtIntf)
            rtspExtIntf->UpdateSessionCompletionStatus(isSessionDurationExpired);
    }

    for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
    {
        int32 nodeTag = it->iNodeTag;
        //if pv playlist rp, don't pause
        if ((nodeTag == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE) ||
                (nodeTag == PVMF_SM_FSP_JITTER_BUFFER_NODE) ||
                (nodeTag == PVMF_SM_FSP_SOCKET_NODE))
        {
            PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
            if (internalCmd != NULL)
            {
                internalCmd->cmd =
                    it->commandStartOffset +
                    PVMF_SM_FSP_NODE_INTERNAL_PAUSE_CMD_OFFSET;
                internalCmd->parentCmd = PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION;

                OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                PVMFNodeInterface* iNode = it->iNode;

                iNode->Pause(it->iSessionId, cmdContextData);
                it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
            }
            else
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoRepositioningPause:RequestNewInternalCmd - Failed"));
                return false;
            }
        }
    }
    return true;
}

bool PVMFSMRTSPUnicastNode::CompleteFeedBackPortsSetup()
{
    PVMFSMFSPChildNodeContainer* iSocketNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_SOCKET_NODE);
    if (iSocketNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return false;
    }

    PVMFSocketNode* socketNode =
        (PVMFSocketNode*)(iSocketNodeContainer->iNode);

    PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);

    if (iSessionControllerNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return false;
    }

    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
        (PVRTSPEngineNodeExtensionInterface*)
        (iSessionControllerNodeContainer->iExtensions[0]);

    Oscl_Vector<StreamInfo, OsclMemAllocator> aSelectedStream;

    if (rtspExtIntf->GetStreamInfo(aSelectedStream) != PVMFSuccess)
    {
        OSCL_LEAVE(OsclErrGeneral);
    }

    for (uint32 i = 0; i < aSelectedStream.size(); i++)
    {
        StreamInfo streamInfo = aSelectedStream[i];

        PVMFRTSPTrackInfo* trackInfo = FindTrackInfo(streamInfo.iSDPStreamId);

        if (trackInfo == NULL)
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:CompleteFeedBackPortsSetup - FindTrackInfo Failed"));
            return false;
        }

        if (trackInfo->iNetworkNodeRTCPPort == NULL)
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:CompleteFeedBackPortsSetup - Invalid RTCP Port"));
            return false;
        }

        OsclNetworkAddress localAddRTCP;
        OsclNetworkAddress remoteAddRTCP;

        localAddRTCP.port = streamInfo.iCliRTCPPort;
        remoteAddRTCP.port = streamInfo.iSerRTCPPort;
        remoteAddRTCP.ipAddr = streamInfo.iSerIpAddr;

        socketNode->SetPortConfig(*(trackInfo->iNetworkNodeRTCPPort),
                                  localAddRTCP,
                                  remoteAddRTCP);

        if (trackInfo->iNetworkNodePort == NULL)
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:CompleteFeedBackPortsSetup - Invalid RTP Port"));
            return false;
        }

        OsclNetworkAddress localAddRTP;
        OsclNetworkAddress remoteAddRTP;

        localAddRTP.port = streamInfo.iCliRTPPort;
        remoteAddRTP.port = streamInfo.iSerRTPPort;
        remoteAddRTP.ipAddr = streamInfo.iSerIpAddr;

        socketNode->SetPortConfig(*(trackInfo->iNetworkNodePort),
                                  localAddRTP,
                                  remoteAddRTP);

    }

    return true;
}

bool
PVMFSMRTSPUnicastNode::SendSessionControlPrepareCompleteParams()
{
    if ((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL) ||
            (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE))
    {
        PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
        if (iSessionControllerNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return false;
        }
        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            (PVRTSPEngineNodeExtensionInterface*)
            (iSessionControllerNodeContainer->iExtensions[0]);

        PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
        if (iJitterBufferNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return false;
        }
        PVMFJitterBufferExtensionInterface* jbExtIntf =
            (PVMFJitterBufferExtensionInterface*)
            (iJitterBufferNodeContainer->iExtensions[0]);

        Oscl_Vector<StreamInfo, OsclMemAllocator> aSelectedStream;

        if (rtspExtIntf->GetStreamInfo(aSelectedStream) != PVMFSuccess)
        {
            OSCL_LEAVE(OsclErrGeneral);
        }

        for (uint32 i = 0; i < aSelectedStream.size(); i++)
        {
            StreamInfo streamInfo = aSelectedStream[i];

            PVMFRTSPTrackInfo* trackInfo = FindTrackInfo(streamInfo.iSDPStreamId);

            if (trackInfo == NULL)
            {
                PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:SendSessionControlPrepareCompleteParams - FindTrackInfo Failed"));
                return false;
            }

            if (trackInfo->iJitterBufferInputPort == NULL)
            {
                PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:SendSessionControlPrepareCompleteParams - Invalid Port"));
                return false;
            }

            if (streamInfo.ssrcIsSet)
            {
                jbExtIntf->setPortSSRC(trackInfo->iJitterBufferInputPort,
                                       streamInfo.iSSRC);
            }
        }

        /* Set server info */
        PVRTSPEngineNodeServerInfo rtspServerInfo;
        PVMFJitterBufferFireWallPacketInfo fireWallPktInfo;

        rtspExtIntf->GetServerInfo(rtspServerInfo);

        if (rtspServerInfo.iIsPVServer)
        {
            fireWallPktInfo.iFormat = PVMF_JB_FW_PKT_FORMAT_PV;
        }
        fireWallPktInfo.iServerRoundTripDelayInMS = rtspServerInfo.iRoundTripDelayInMS;

        if (0 == fireWallPktInfo.iServerRoundTripDelayInMS)
        {
            fireWallPktInfo.iServerRoundTripDelayInMS = PVMF_JITTER_BUFFER_NODE_FIREWALL_PKT_DEFAULT_SERVER_RESPONSE_TIMEOUT_IN_MS;
        }

        fireWallPktInfo.iNumAttempts = PVMF_JITTER_BUFFER_NODE_DEFAULT_FIREWALL_PKT_ATTEMPTS;

        jbExtIntf->setServerInfo(fireWallPktInfo);
    }
    return true;
}

bool PVMFSMRTSPUnicastNode::SendSessionControlStartCompleteParams()
{
    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (iJitterBufferNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return false;
    }
    PVMFJitterBufferExtensionInterface* jbExtIntf =
        (PVMFJitterBufferExtensionInterface*)
        (iJitterBufferNodeContainer->iExtensions[0]);

    bool end_is_set = true;
    int32 startTime = 0;
    int32 stopTime  = 0;

    if ((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL) ||
            (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE))
    {
        PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
        if (iSessionControllerNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return false;
        }
        PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
            (PVRTSPEngineNodeExtensionInterface*)
            (iSessionControllerNodeContainer->iExtensions[0]);

        /* Get Actual Play Range */
        RtspRangeType rangeType;
        if (rtspExtIntf->GetActualPlayRange(rangeType) != PVMFSuccess)
        {
            return false;
        }

        rangeType.convertToMilliSec(startTime, stopTime);

        /* Use from SDP if not set */
        end_is_set = rangeType.end_is_set;
        if (end_is_set == false)
        {
            stopTime = iSessionStopTime;
        }

        if (iRepositioning)
        {
            iActualRepositionStartNPTInMS = startTime;
            if (iActualRepositionStartNPTInMSPtr != NULL)
            {
                *iActualRepositionStartNPTInMSPtr = startTime;
            }
        }

        Oscl_Vector<StreamInfo, OsclMemAllocator> aSelectedStream;

        if (rtspExtIntf->GetStreamInfo(aSelectedStream) != PVMFSuccess)
        {
            OSCL_LEAVE(OsclErrGeneral);
        }

        for (uint32 i = 0; i < aSelectedStream.size(); i++)
        {
            StreamInfo streamInfo = aSelectedStream[i];

            PVMFRTSPTrackInfo* trackInfo = FindTrackInfo(streamInfo.iSDPStreamId);

            if (trackInfo == NULL)
            {
                PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:SendStartCompleteSessionControlParams - FindTrackInfo Failed"));
                return false;
            }

            if (trackInfo->iJitterBufferInputPort == NULL)
            {
                PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:SendStartCompleteSessionControlParams - Invalid Port"));
                return false;
            }

            if (streamInfo.seqIsSet != true)
            {
                streamInfo.seqIsSet = false;
                streamInfo.seq = 0;
            }
            if (streamInfo.rtptimeIsSet != true)
            {
                streamInfo.rtptimeIsSet = false;
                streamInfo.rtptime = 0;
            }
            jbExtIntf->setPortRTPParams(trackInfo->iJitterBufferInputPort,
                                        streamInfo.seqIsSet,
                                        streamInfo.seq,
                                        streamInfo.rtptimeIsSet,
                                        streamInfo.rtptime,
                                        rangeType.start_is_set,
                                        startTime,
                                        iRepositioning);

        }
    }


    /* Send actual stop time to Jitter Buffer */
    if (jbExtIntf->setPlayRange(startTime,
                                stopTime,
                                iRepositioning,
                                end_is_set) != true)
    {
        return false;
    }

    return true;
}

void PVMFSMRTSPUnicastNode::HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    if (SupressInfoEvent())
    {
        PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode:HandleNodeInformationalEvent Supress info event %u", aEvent.GetEventType()));
        return;
    }

    PVMFAsyncEvent event = OSCL_CONST_CAST(PVMFAsyncEvent, aEvent);
    PVMFEventType infoEvent = aEvent.GetEventType();
    if (infoEvent == PVMFInfoEndOfData)
    {
        /* Notify jitter buffer */
        PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
        if (iJitterBufferNodeContainer == NULL)
        {
            OSCL_LEAVE(OsclErrBadHandle);
            return;
        }
        PVMFJitterBufferExtensionInterface* jbExtIntf =
            (PVMFJitterBufferExtensionInterface*)
            (iJitterBufferNodeContainer->iExtensions[0]);
        jbExtIntf->NotifyOutOfBandEOS();
    }
    else if (infoEvent == PVMFInfoPlayListClipTransition)
    {
        if (iPlaylistPlayInProgress)
        {
            iPlaylistPlayInProgress = false;
            PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
            if (iSessionControllerNodeContainer == NULL)
            {
                OSCL_LEAVE(OsclErrBadHandle);
                return;
            }

            // The following code is executed in case of 3GPP FCS without SDP available. The
            // PVMFInfoPlayListClipTransition event is called once SDP text is received
            // in the server response.
            // RTP info in server response is updated first.
            // New SDP information is processed next.


            /*****
             * i. Retrieve SDP text
             */
            PVMFStatus status;                                     // saves return value from rtspExtIntf->GetSDP() call
            OsclRefCounterMemFrag iSDPText;                        // passed by reference to rtspExtIntf->GetSDP() call - saves SDP text
            PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                (PVRTSPEngineNodeExtensionInterface*)
                (iSessionControllerNodeContainer->iExtensions[0]); // pointer to the RTSP extension interface object

            // Call to get switch SDP text
            status = rtspExtIntf->GetSDP(iSDPText);
            if (status != PVMFSuccess)
            {
                PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastPlusPVRNode:HandleNodeInformationalEvent Unable to get new SDP %d", status));
                return;
            }

            /*****
             * ii. Parse SDP
             */
            PVMFSMSharedPtrAlloc<SDPInfo> sdpAlloc; // SDPInfo allocator object
            SDP_Parser *sdpParser;                  // Pointer to SDP parser object
            SDPMediaParserRegistry* sdpParserReg = SDPMediaParserRegistryPopulater::PopulateRegistry(); // Pointer to SDP parser registry object

            // Create SDPInfo object and save
            SDPInfo* sdpInfo = sdpAlloc.allocate();

            // Create SDP parser and save
            sdpParser = OSCL_NEW(SDP_Parser, (sdpParserReg));

            // Parse switch SDP text
            int32 sdpRetVal = sdpParser->parseSDP((const char*)(iSDPText.getMemFragPtr()),
                                                  iSDPText.getMemFragSize(),
                                                  sdpInfo);

            // Cleanup parser, registry
            OSCL_DELETE(sdpParser);
            SDPMediaParserRegistryPopulater::CleanupRegistry(sdpParserReg);

            // If SDP parsing fails, log failure (if logging enabled) and return
            if (sdpRetVal != SDP_SUCCESS)
            {
                PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastPlusPVRNode:HandleNodeInformationalEvent SDP parser error %d", sdpRetVal));
                return;
            }

            OsclRefCounterSA< PVMFSMSharedPtrAlloc<SDPInfo> > *refcnt =
                new OsclRefCounterSA< PVMFSMSharedPtrAlloc<SDPInfo> >(sdpInfo);

            OsclSharedPtr<SDPInfo> sharedSDPInfo(sdpInfo, refcnt);

            // In case of a 200 response, simply save the SDP and pass it to the RTSP node
            if (!iSelectTracksAndSendFCSRequestFlag)
            {
                // Save switch SDP info
                iSwitchSDPInfo = sharedSDPInfo;

                /*****
                 * iii. Update RTSP node with new SDP info
                 */
                // First populate presentation info
                PVMFMediaPresentationInfo aMediaInfo;
                PopulatePresentationInfoFromSwitchSDP(aMediaInfo);

                Oscl_Vector<StreamInfo, OsclMemAllocator> aSwitchSelectedStream;
                if (SelectTracksFromSwitchSDP(aSwitchSelectedStream, aMediaInfo) == PVMFFailure)
                {
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastPlusPVRNode:HandleNodeInformationalEvent:SelectTracksFromSwitchSDP - Failed"));
                    return;
                }
                // Send new SDP info to rtsp node
                status = rtspExtIntf->SetSDPInfo(iSwitchSDPInfo, aSwitchSelectedStream);
                if (status != PVMFSuccess)
                {
                    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastPlusPVRNode:HandleNodeInformationalEvent Unable to pass new SDP info to RTSP node %d", status));
                    return;
                }
                else
                {
                    UpdateRTPInfoAndFlushJitterBuffer();
                    iSessionControllerNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
                    CompleteStart();
                    // Save switch SDP as current SDP for future switches
                    iSdpInfo = iSwitchSDPInfo;
                }
            }
            // In case of 202 response processing, need to do track selection and
            // send out a new FCS request.
            else
            {
                // Save switch SDP info
                iSwitchSDPInfo = sharedSDPInfo;

                /******
                 * iii. Track selection
                 */
                // First populate presentation info
                PVMFMediaPresentationInfo aMediaInfo;
                PopulatePresentationInfoFromSwitchSDP(aMediaInfo);

                Oscl_Vector<StreamInfo, OsclMemAllocator> aSwitchSelectedStream;
                if (SelectTracksFromSwitchSDP(aSwitchSelectedStream, aMediaInfo) == PVMFFailure)
                {
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastPlusPVRNode:HandleNodeInformationalEvent:SelectTracksFromSwitchSDP - Failed"));
                    return;
                }

                /******
                 * iv. Send out another FCS request to the RTSP engine. Set the aSDPAvailable flag to true.
                 */
                rtspExtIntf->SetSwitchSDPInfo(iSwitchSDPInfo, aSwitchSelectedStream, true);

                for (uint32 i = 0; i < iFSPChildNodeContainerVec.size(); i++)
                {
                    if (iFSPChildNodeContainerVec[i].iNodeTag == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE)
                    {
                        PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
                        if (internalCmd != NULL)
                        {
                            internalCmd->cmd =
                                iFSPChildNodeContainerVec[i].commandStartOffset +
                                PVMF_SM_FSP_NODE_INTERNAL_PLAYLIST_PLAY_CMD_OFFSET;
                            internalCmd->parentCmd = PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION;

                            OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                            /* Set Requested Play Range */
                            RtspRangeType rtspRange;
                            rtspRange.format = RtspRangeType::PLAYLIST_TIME_RANGE;
                            rtspRange.iPlaylistUrl[0] = '\0';
                            rtspRange.start_is_set = true;
                            rtspRange.playlist_start.iClipIndex = iPVMFDataSourcePositionParamsPtr->iPlayElementIndex;
                            rtspRange.npt_start.npt_sec.sec = iPVMFDataSourcePositionParamsPtr->iTargetNPT / 1000;
                            rtspRange.npt_start.npt_sec.milli_sec =
                                (iPVMFDataSourcePositionParamsPtr->iTargetNPT - ((iPVMFDataSourcePositionParamsPtr->iTargetNPT / 1000) * 1000));
                            rtspRange.end_is_set = false;
                            PVEffectiveTime effectiveTime = (PVEffectiveTime)iPVMFDataSourcePositionParamsPtr->iMode;

                            // Set playlist uri, if any, before calling playlist play
                            rtspExtIntf->SetPlaylistUri(iPVMFDataSourcePositionParamsPtr->iPlaylistUri);
                            rtspExtIntf->PlaylistPlay(iFSPChildNodeContainerVec[i].iSessionId,
                                                      rtspRange,
                                                      effectiveTime,
                                                      cmdContextData);
                            iFSPChildNodeContainerVec[i].iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
                        }
                        else
                        {
                            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastPlusPVRNode:HandleNodeInformationalEvent:RequestNewInternalCmd - Failed"));
                            return;
                        }
                    }
                }
            }

            iSessionControllerNodeContainer->iNodeCmdState = PVMFSMFSP_NODE_CMD_IDLE;
            CompleteStart();

        }

        PVMFNodeInterface::ReportInfoEvent(event);
    }
    else if (infoEvent == PVMFInfoSourceOverflow)
    {
        /* Make note of the overflow */
        oAutoReposition = true;
        PVMFNodeInterface::ReportInfoEvent(event);
    }
    else
    {
        /* Just pass the info event up */
        PVMFNodeInterface::ReportInfoEvent(event);
    }
}
bool PVMFSMRTSPUnicastNode::CanPerformRepositioning(bool aRandAccessDenied)
{

    if ((aRandAccessDenied == true) ||
            (iSessionStopTimeAvailable == false) ||
            (((int32)iRepositionRequestedStartNPTInMS < (int32)iSessionStartTime) ||
             ((int32)iRepositionRequestedStartNPTInMS > (int32)iSessionStopTime)))
    {
        /*
         * Implies an open ended session or invalid request time
         * - no pause or reposition
         */
        return false;

    }
    return true;

}

bool
PVMFSMRTSPUnicastNode::CheckChildrenNodesStop()
{

    {
        for (uint32 i = 0; i < iFSPChildNodeContainerVec.size(); i++)
        {
            if (iFSPChildNodeContainerVec[i].iNodeCmdState != PVMFSMFSP_NODE_CMD_IDLE)
            {
                return false;
            }
        }
    }
    return true;
}

void PVMFSMRTSPUnicastNode::CleanUp()
{
    DestroyChildNodes();
    DestroyPayloadParserRegistry();
    ResetNodeParams();
    iLogger = NULL;
    // Cleanup any old SDP file paths
    if (iSwitchSDPLocation != NULL)
    {
        OSCL_FREE(iSwitchSDPLocation);
        iSwitchSDPLocation = NULL;
    }

}

void PVMFSMRTSPUnicastNode::PopulateDRMInfo()
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::PopulateDRMInfo() In"));
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::PopulateDRMInfo() - CPM not supported yet"));
}

void PVMFSMRTSPUnicastNode::ResetNodeParams(bool aReleaseMemmory)
{
    iTrackInfoVec.clear();
    iSdpInfo.Unbind();
    PVMFSMFSPBaseNode::ResetNodeParams(aReleaseMemmory);
}

void PVMFSMRTSPUnicastNode::GetMaxMediaTS()
{
    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);

    if (iJitterBufferNodeContainer == NULL) OSCL_LEAVE(OsclErrBadHandle);
    PVMFJitterBufferExtensionInterface* jbExtIntf =
        (PVMFJitterBufferExtensionInterface*)
        (iJitterBufferNodeContainer->iExtensions[0]);

    if ((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL) ||
            (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE))
    {
        iActualMediaDataTS = jbExtIntf->getMaxMediaDataTS();
        if (iActualMediaDataTSPtr != NULL)
        {
            *iActualMediaDataTSPtr = iActualMediaDataTS;
            PVMF_SM_RTSP_LOG_COMMAND_REPOS((0, "PVMFStreamingManagerNode::GetActualMediaTSAfterSeek - TargetNPT = %d, ActualNPT=%d, ActualMediaDataTS=%d",
                                            iRepositionRequestedStartNPTInMS, *iActualRepositionStartNPTInMSPtr, *iActualMediaDataTSPtr));
        }
        if (iPVMFDataSourcePositionParamsPtr != NULL)
        {
            iPVMFDataSourcePositionParamsPtr->iActualMediaDataTS = iActualMediaDataTS;
            PVMF_SM_RTSP_LOG_COMMAND_REPOS((0, "PVMFStreamingManagerNode::GetActualMediaTSAfterSeek - ActualMediaDataTS=%d",
                                            iPVMFDataSourcePositionParamsPtr->iActualMediaDataTS));
        }
    }
}


void PVMFSMRTSPUnicastNode::DoSetDataSourcePositionOverflow(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFStreamingManagerNode::DoSetDataSourcePositionOverflow - In"));

    iActualRepositionStartNPTInMSPtr = NULL;
    iActualMediaDataTSPtr = NULL;
    iPVMFDataSourcePositionParamsPtr = NULL;
    iJumpToIFrame = false;
    PVMFTimestamp aTargetNPT;
    bool aSeekToSyncPoint;
    aCmd.PVMFSMFSPBaseNodeCommand::Parse(aTargetNPT,
                                         iActualRepositionStartNPTInMSPtr,
                                         iActualMediaDataTSPtr,
                                         aSeekToSyncPoint,
                                         iStreamID);

    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (iJitterBufferNodeContainer == NULL) OSCL_LEAVE(OsclErrBadHandle);
    PVMFJitterBufferExtensionInterface* jbExtIntf =
        (PVMFJitterBufferExtensionInterface*)
        (iJitterBufferNodeContainer->iExtensions[0]);

    jbExtIntf->SendBOSMessage(iStreamID);
    oAutoReposition = false;

    if (iInterfaceState == EPVMFNodeStarted)
    {
        bool oRandAccessDenied = false;

        if ((iSessionSourceInfo->_sessionType != PVMF_MIME_DATA_SOURCE_RTSP_URL) &&
                (iSessionSourceInfo->_sessionType != PVMF_MIME_DATA_SOURCE_SDP_FILE))
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:DoSetDataSourcePositionOverflow - Invalid Session Type"));
            oRandAccessDenied = true;
        }

        if (oRandAccessDenied == true)
        {
            /*
            * Implies an open ended session or invalid request time
            * - no pause or reposition
                */
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:DoSetDataSourcePositionOverflow - Invalid Args"));
            CommandComplete(iInputCommands, aCmd, PVMFFailure);
            return;
        }

        /* Put the jitter buffer into a state of transition - only if the playlist switch mode is 0*/

        PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
            getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
        if (iJitterBufferNodeContainer == NULL) OSCL_LEAVE(OsclErrBadHandle);
        PVMFJitterBufferExtensionInterface* jbExtIntf =
            (PVMFJitterBufferExtensionInterface*)
            (iJitterBufferNodeContainer->iExtensions[0]);
        jbExtIntf->PrepareForRepositioning();
        if ((iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_RTSP_URL) ||
                (iSessionSourceInfo->_sessionType == PVMF_MIME_DATA_SOURCE_SDP_FILE))
        {
            // Purge JB at this point
            GetMaxMediaTS();
            *iActualMediaDataTSPtr = iActualMediaDataTS;
            PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            if (iJitterBufferNodeContainer == NULL) OSCL_LEAVE(OsclErrBadHandle);
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)
                (iJitterBufferNodeContainer->iExtensions[0]);

            // Convert iActualMediaDataTS to NPT time format
            NptTimeFormat npt_start;
            npt_start.npt_format = NptTimeFormat::NPT_SEC;
            npt_start.npt_sec.sec = iActualMediaDataTS / 1000;
            npt_start.npt_sec.milli_sec = iActualMediaDataTS - 1000 * (iActualMediaDataTS / 1000);

            jbExtIntf->PurgeElementsWithNPTLessThan(npt_start);
            CommandComplete(iInputCommands, aCmd, PVMFSuccess);
        }
        else
        {
            PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:DoSetDataSourcePositionOverflow - Invalid Session Type"));
            CommandComplete(iInputCommands, aCmd, PVMFFailure);
        }
    }
    else
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:DoSetDataSourcePositionOverflow - Invalid State"));
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
    }
    return;
}

PVMFStatus PVMFSMRTSPUnicastNode::NotifyTargetPositionSync(PVMFTimestamp aTargetNPT)
{
    PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);

    if (iSessionControllerNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return PVMFFailure;
    }


    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
        (PVRTSPEngineNodeExtensionInterface*)
        (iSessionControllerNodeContainer->iExtensions[0]);

    if (rtspExtIntf == NULL)
    {
        return PVMFFailure;
    }

    RtspRangeType rtspRange;
    rtspRange.format = RtspRangeType::NPT_RANGE;
    rtspRange.start_is_set = true;
    rtspRange.npt_start.npt_format = NptTimeFormat::NPT_SEC;
    rtspRange.npt_start.npt_sec.sec = aTargetNPT / 1000;
    rtspRange.npt_start.npt_sec.milli_sec =
        (aTargetNPT - ((aTargetNPT / 1000) * 1000));

    /* Set end time from SDP, if available */
    if (iSdpInfo)
    {
        sessionDescription* sessionInfo = iSdpInfo->getSessionInfo();
        int32 sessionStartTime = 0, sessionStopTime = 0;
        RtspRangeType *rtspRangeForEndTime = OSCL_CONST_CAST(RtspRangeType*, (sessionInfo->getRange()));
        rtspRangeForEndTime->convertToMilliSec((int32&)sessionStartTime, (int32&)sessionStopTime);
        // Pass the stop time only if it is available
        if (rtspRangeForEndTime->end_is_set == true)
        {
            rtspRange.end_is_set = true;
            rtspRange.npt_end.npt_format = NptTimeFormat::NPT_SEC;
            rtspRange.npt_end.npt_sec.sec = sessionStopTime / 1000;
            rtspRange.npt_end.npt_sec.milli_sec =
                (sessionStopTime - ((sessionStopTime / 1000) * 1000));
        }
    }

    return rtspExtIntf->SetRequestPlayRange(rtspRange);
}


///////////////////////////////////////////////////////////////////////////////
//Node state
//EPVMFNodePrepared:
//              - Send BOS notification to JB node
//              - Validate requested npt w.r.t session info
//                  - if requested npt is within session range and is != session stop time
//                          - Provide RTSP extension interface with RtspRangeType obj, with
//                            rtspRange.playlist_start.iClipIndex = iPVMFDataSourcePositionParamsPtr->iPlayElementIndex;
//                            rtspRange.npt_start.npt_sec.sec = iPVMFDataSourcePositionParamsPtr->iTargetNPT/1000;
//                            rtspRange.end_is_set = false;
//
//                  -else
//                          - available media data Ts after seek from JB node and set it to *iActualMediaDataTSPtr
//                          - Report cmd status PVMFSuccess to observer
//EPVMFNodeStarted
//              -   Make sure session is either PVMF_MIME_DATA_SOURCE_RTSP_URL or PVMF_MIME_DATA_SOURCE_SDP_FILE. For invalid
//                  session, complete command with status PVMFFailure
//              -   Prepare JB for repositioning.
//              -   Provide RTSP extension interface with RtspRangeType obj, with
//                  rtspRange.playlist_start.iClipIndex = iPVMFDataSourcePositionParamsPtr->iPlayElementIndex;
//                  rtspRange.npt_start.npt_sec.sec = iPVMFDataSourcePositionParamsPtr->iTargetNPT/1000;
//                  rtspRange.end_is_set = false;
//              -   Set playlisturi with RTSP node and call playlistplay
//
//EPVMFNodePaused
//              -   Make sure session is either PVMF_MIME_DATA_SOURCE_RTSP_URL or PVMF_MIME_DATA_SOURCE_SDP_FILE. For invalid
//                  session, complete command with status PVMFFailure
//              -   Prepare JB for repositioning.
//              -   Provide RTSP extension interface with RtspRangeType obj, with
//                  rtspRange.playlist_start.iClipIndex = iPVMFDataSourcePositionParamsPtr->iPlayElementIndex;
//                  rtspRange.npt_start.npt_sec.sec = iPVMFDataSourcePositionParamsPtr->iTargetNPT/1000;
//                  rtspRange.end_is_set = false;
//              -   Call playlistplay on RTSP extension interface.
//Other state:
//              - Report invalid state
//
///////////////////////////////////////////////////////////////////////////////
void PVMFSMRTSPUnicastNode::DoSetDataSourcePositionPlayList(PVMFSMFSPBaseNodeCommand& aCmd)
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMRTSPUnicastNode::DoSetDataSourcePositionPlayList - In"));

    iActualRepositionStartNPTInMSPtr = NULL;
    iActualMediaDataTSPtr = NULL;
    iPVMFDataSourcePositionParamsPtr = NULL;
    iJumpToIFrame = false;

    aCmd.PVMFSMFSPBaseNodeCommand::Parse(iPVMFDataSourcePositionParamsPtr);

    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (iJitterBufferNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return;
    }
    PVMFJitterBufferExtensionInterface* jbExtIntf =
        (PVMFJitterBufferExtensionInterface*)
        (iJitterBufferNodeContainer->iExtensions[0]);

    // Save the playlist switch mode
    iPlaylistSwitchMode = iPVMFDataSourcePositionParamsPtr->iMode;

    iStreamID = iPVMFDataSourcePositionParamsPtr->iStreamID;
    if (iPVMFDataSourcePositionParamsPtr->iMode == PVMF_SET_DATA_SOURCE_POSITION_MODE_3GPP_FCS)
    {
        jbExtIntf->SendBOSMessage(iStreamID);
    }

    if (iInterfaceState == EPVMFNodePrepared)
    {
        /*
        * SetDataSource from a prepared state could mean two things:
        *  - In Play-Stop-Play usecase engine does a SetDataSourcePosition
        *    to get the start media TS to set its playback clock
        *  - Engine is trying to do a play with a non-zero start offset
            */
        if (iPVMFDataSourcePositionParamsPtr->iTargetNPT < iSessionStopTime)
        {
            if (iPVMFDataSourcePositionParamsPtr->iTargetNPT != iSessionStartTime)
            {
                /* Set Requested Play Range */
                PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
                    getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
                if (iSessionControllerNodeContainer == NULL)
                {
                    OSCL_LEAVE(OsclErrBadHandle);
                    return;
                }
                PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
                    (PVRTSPEngineNodeExtensionInterface*)
                    (iSessionControllerNodeContainer->iExtensions[0]);

                RtspRangeType rtspRange;
                rtspRange.format = RtspRangeType::PLAYLIST_TIME_RANGE;
                rtspRange.start_is_set = true;
                rtspRange.playlist_start.iClipIndex = iPVMFDataSourcePositionParamsPtr->iPlayElementIndex;
                rtspRange.npt_start.npt_sec.sec = iPVMFDataSourcePositionParamsPtr->iTargetNPT / 1000;
                rtspRange.npt_start.npt_sec.milli_sec =
                    (iPVMFDataSourcePositionParamsPtr->iTargetNPT - ((iPVMFDataSourcePositionParamsPtr->iTargetNPT / 1000) * 1000));
                rtspRange.end_is_set = false;

                if (rtspExtIntf->SetRequestPlayRange(rtspRange) != PVMFSuccess)
                {
                    PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoSetDataSourcePositionPlayList - SetRequestPlayRange Failed"));
                    CommandComplete(iInputCommands, aCmd, PVMFFailure);
                    return;
                }

            }
        }
        GetActualMediaTSAfterSeek();
        CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    }
    else if ((iInterfaceState == EPVMFNodeStarted) || (iInterfaceState == EPVMFNodePaused))
    {
        iRepositioning = true;

        /* Put the jitter buffer into a state of transition - only if the playlist switch mode is 0*/
        if ((iPVMFDataSourcePositionParamsPtr->iMode == PVMF_SET_DATA_SOURCE_POSITION_MODE_3GPP_FCS) ||
                (iPVMFDataSourcePositionParamsPtr->iMode == PVMF_SET_DATA_SOURCE_POSITION_MODE_NOW) ||
                (iPVMFDataSourcePositionParamsPtr->iMode == PVMF_SET_DATA_SOURCE_POSITION_MODE_UNKNOWN))
        {
            PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
                getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
            if (iJitterBufferNodeContainer == NULL)
            {
                OSCL_LEAVE(OsclErrBadHandle);
                return;
            }
            PVMFJitterBufferExtensionInterface* jbExtIntf =
                (PVMFJitterBufferExtensionInterface*)
                (iJitterBufferNodeContainer->iExtensions[0]);
            jbExtIntf->PrepareForPlaylistSwitch();
        }
        /* If node is running, pause first */
        if (iInterfaceState == EPVMFNodeStarted)
        {
            if (!DoRepositioningStart3GPPPlayListStreamingDuringPlay())
            {
                PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:DoSetDataSourcePositionPlayList - No Memory"));
                CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
            }
        }
        /* If already paused do not pause */
        else if (iInterfaceState == EPVMFNodePaused)
        {
            /* Start the nodes */
            if (!DoRepositioningStart3GPPPlayListStreaming())
            {
                PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:DoSetDataSourcePositionPlayList - No Memory"));
                CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
            }
        }
        MoveCmdToCurrentQueue(aCmd);
    }
    else
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:DoSetDataSourcePositionPlayList - Invalid State"));
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
    }
    return;
}

bool PVMFSMRTSPUnicastNode::DoRepositioningStart3GPPPlayListStreamingDuringPlay()
{
    /* Set Requested Play Range */
    PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);

    if (iSessionControllerNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return false;
    }

    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
        (PVRTSPEngineNodeExtensionInterface*)
        (iSessionControllerNodeContainer->iExtensions[0]);

    RtspRangeType rtspRange;
    rtspRange.format = RtspRangeType::PLAYLIST_TIME_RANGE;
    rtspRange.iPlaylistUrl[0] = '\0';

    rtspRange.start_is_set = true;
    rtspRange.playlist_start.iClipIndex = iPVMFDataSourcePositionParamsPtr->iPlayElementIndex;
    rtspRange.npt_start.npt_sec.sec = iPVMFDataSourcePositionParamsPtr->iTargetNPT / 1000;
    rtspRange.npt_start.npt_sec.milli_sec =
        (iPVMFDataSourcePositionParamsPtr->iTargetNPT - ((iPVMFDataSourcePositionParamsPtr->iTargetNPT / 1000) * 1000));
    rtspRange.end_is_set = false;
    PVEffectiveTime effectiveTime = (PVEffectiveTime)iPVMFDataSourcePositionParamsPtr->iMode;

    // Is switch SDP available, process it and select tracks
    Oscl_Vector<StreamInfo, OsclMemAllocator> aSwitchSelectedStream;      // Saves selected stream info
    bool aSDPAvailable = iPVMFDataSourcePositionParamsPtr->iSDPAvailable; // Flag indicating if switch SDP available
    if (aSDPAvailable)
    {
        // Cleanup any old SDP file paths
        if (iSwitchSDPLocation != NULL)
        {
            OSCL_FREE(iSwitchSDPLocation);
            iSwitchSDPLocation = NULL;
        }
        // Allocate required memory to save SDP file path
        iSwitchSDPLocation = (oscl_wchar*)OSCL_MALLOC((oscl_strlen(iPVMFDataSourcePositionParamsPtr->iPlaylistUri) + 1) * sizeof(oscl_wchar));
        if (iSwitchSDPLocation == NULL)
        {
            return false;
        }
        // First convert character array to wide character array
        oscl_UTF8ToUnicode(iPVMFDataSourcePositionParamsPtr->iPlaylistUri, oscl_strlen(iPVMFDataSourcePositionParamsPtr->iPlaylistUri) + 1, iSwitchSDPLocation, oscl_strlen(iPVMFDataSourcePositionParamsPtr->iPlaylistUri) + 1);
        // Create an OSCL_sHeapString with the file path
        OSCL_wHeapString<OsclMemAllocator> aSDPLocation(iSwitchSDPLocation);
        PVMFMediaPresentationInfo aMediaInfo;

        // Parse SDP file pointed to by aSDPLocation
        if (ParseSwitchSDP(aSDPLocation) != PVMFSuccess)
        {
            return false;
        }
        // Populate presentation info from the switch SDP. This is
        // needed to index SDP track IDs.
        // For e.g. the first track (indexed by 0) may have a track ID equal to 2.
        if (PopulatePresentationInfoFromSwitchSDP(aMediaInfo) != PVMFSuccess)
        {
            return false;
        }

        // Select tracks from the switch SDP using the current SDP as basis.
        if (SelectTracksFromSwitchSDP(aSwitchSelectedStream, aMediaInfo) == PVMFFailure)
        {
            return false;
        }
    }
    // Now pass the new SDP info down to the RTSP node. Note that this API is
    // called even when switch SDP is not available. In addition to passing
    // SDP info (when available), the availability of SDP itself is indicated to
    // to the RTSP node by this call.
    rtspExtIntf->SetSwitchSDPInfo(iSwitchSDPInfo, aSwitchSelectedStream, aSDPAvailable);

    for (uint32 i = 0; i < iFSPChildNodeContainerVec.size(); i++)
    {
        if (iFSPChildNodeContainerVec[i].iNodeTag == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE)
        {
            PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
            if (internalCmd != NULL)
            {
                internalCmd->cmd =
                    iFSPChildNodeContainerVec[i].commandStartOffset +
                    PVMF_SM_FSP_NODE_INTERNAL_PLAYLIST_PLAY_CMD_OFFSET;
                internalCmd->parentCmd = PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION;

                OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);
                // Set playlist uri, if any, before calling playlist play
                rtspExtIntf->SetPlaylistUri(iPVMFDataSourcePositionParamsPtr->iPlaylistUri);
                rtspExtIntf->PlaylistPlay(iFSPChildNodeContainerVec[i].iSessionId,
                                          rtspRange,
                                          effectiveTime,
                                          cmdContextData);
                iFSPChildNodeContainerVec[i].iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
            }
            else
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoRepositioningStart3GPPPlayListStreamingDuringPlay:RequestNewInternalCmd - Failed"));
                return false;
            }
        }
    }
    return true;
}

bool PVMFSMRTSPUnicastNode::DoRepositioningStart3GPPPlayListStreaming()
{
    /* Set Requested Play Range */
    PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);

    if (iSessionControllerNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return false;
    }

    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
        (PVRTSPEngineNodeExtensionInterface*)
        (iSessionControllerNodeContainer->iExtensions[0]);


    RtspRangeType rtspRange;
    rtspRange.format = RtspRangeType::PLAYLIST_TIME_RANGE;
    rtspRange.iPlaylistUrl[0] = '\0';

    //OSCL_StackString<256> newUrn(_STRLIT_CHAR("/public/playlist/va_playlists/pv2-aac64_mpeg4-rvlcs-64.ply"));
    //oscl_strncpy(rtspRange.iPlaylistUrl, newUrn.get_cstr(), 1024);

    rtspRange.start_is_set = true;
    rtspRange.playlist_start.iClipIndex = iPVMFDataSourcePositionParamsPtr->iPlayElementIndex;
    rtspRange.npt_start.npt_sec.sec = iPVMFDataSourcePositionParamsPtr->iTargetNPT / 1000;
    rtspRange.npt_start.npt_sec.milli_sec =
        (iPVMFDataSourcePositionParamsPtr->iTargetNPT - ((iPVMFDataSourcePositionParamsPtr->iTargetNPT / 1000) * 1000));
    rtspRange.end_is_set = false;

    PVMFSMFSPChildNodeContainerVector::iterator it;
    for (it = iFSPChildNodeContainerVec.begin(); it != iFSPChildNodeContainerVec.end(); it++)
    {
        PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
        if (internalCmd != NULL)
        {
            internalCmd->cmd =
                it->commandStartOffset +
                PVMF_SM_FSP_NODE_INTERNAL_START_CMD_OFFSET;
            internalCmd->parentCmd = PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION;

            OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

            PVMFNodeInterface* iNode = it->iNode;

            iNode->Start(it->iSessionId, cmdContextData);
            it->iNodeCmdState = PVMFSMFSP_NODE_CMD_PENDING;
        }
        else
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoRepositioningStart3GPPPlayListStreaming:RequestNewInternalCmd - Failed"));
            return false;
        }
        if (it->iNodeTag == PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE)
        {
            PVMFSMFSPCommandContext* internalCmd = RequestNewInternalCmd();
            if (internalCmd != NULL)
            {
                internalCmd->cmd =
                    it->commandStartOffset +
                    PVMF_SM_FSP_NODE_INTERNAL_PLAYLIST_PLAY_CMD_OFFSET;
                internalCmd->parentCmd = PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION;

                OsclAny *cmdContextData = OSCL_REINTERPRET_CAST(OsclAny*, internalCmd);

                rtspExtIntf->PlaylistPlay(it->iSessionId,
                                          rtspRange,
                                          PVEffectiveTime_NOW,
                                          cmdContextData);
            }
            else
            {
                PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode:DoRepositioningStart3GPPPlayListStreaming:RequestNewInternalCmd - Failed"));
                return false;
            }
        }
    }
    return true;
}

PVMFStatus PVMFSMRTSPUnicastNode::ParseSwitchSDP(OSCL_wString& aClipLocation)
{
    /**
     * Parsing SDP file.
     */
    //PVMFStatus status;
    OsclRefCounterMemFrag iSwitchSDPText;
    /* Parse SDP file contents into a buffer */
    Oscl_FileServer fileServ;
    Oscl_File osclFile;
    fileServ.Connect();

    if (osclFile.Open(aClipLocation.get_cstr(),
                      Oscl_File::MODE_READ,
                      fileServ) != 0)
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ParseSwitchSDP - Unable to open SDP file"));
        return PVMFFailure;
    }

    /* Get File Size */
    osclFile.Seek(0, Oscl_File::SEEKEND);
    int32 fileSize = (TOsclFileOffsetInt32)osclFile.Tell();
    osclFile.Seek(0, Oscl_File::SEEKSET);

    if (fileSize <= 0)
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ParseSwitchSDP - Corrupt SDP file"));
        return PVMFFailure;
    }

    OsclMemAllocDestructDealloc<uint8> my_alloc;
    OsclRefCounter* my_refcnt;
    uint aligned_refcnt_size =
        oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));
    /*
     * To acct for null char, as SDP buffer is treated akin to a string by the
     * SDP parser lib.
     */
    uint allocsize = oscl_mem_aligned_size(aligned_refcnt_size + fileSize + 2);
    uint8* my_ptr = GetMemoryChunk(my_alloc, allocsize);
    if (!my_ptr)
    {
        PVMF_SM_RTSP_LOGERROR((0, "StreamingManagerNode:ParseSwitchSDP - Unable to process SDP file"));
        return PVMFFailure;
    }

    my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
    my_ptr += aligned_refcnt_size;

    OsclMemoryFragment memfrag;
    memfrag.len = fileSize;
    memfrag.ptr = my_ptr;

    OsclRefCounterMemFrag tmpRefcntMemFrag(memfrag, my_refcnt, memfrag.len);
    iSwitchSDPText = tmpRefcntMemFrag;

    osclFile.Read(memfrag.ptr, 1, fileSize);

    osclFile.Close();
    fileServ.Close();

    PVMFSMSharedPtrAlloc<SDPInfo> sdpAlloc;
    SDPInfo* sdpInfo = sdpAlloc.allocate();

    SDP_Parser *sdpParser;

    SDPMediaParserRegistry* sdpParserReg = SDPMediaParserRegistryPopulater::PopulateRegistry();
    sdpParser = OSCL_NEW(SDP_Parser, (sdpParserReg));

    int32 sdpRetVal =
        sdpParser->parseSDP((const char*)(iSwitchSDPText.getMemFragPtr()),
                            iSwitchSDPText.getMemFragSize(),
                            sdpInfo);

    // save the SDP file name - the packet source node will need this
    sdpInfo->setSDPFilename(aClipLocation);

    OSCL_DELETE(sdpParser);
    SDPMediaParserRegistryPopulater::CleanupRegistry(sdpParserReg);

    OsclRefCounterSA< PVMFSMSharedPtrAlloc<SDPInfo> > *refcnt =
        new OsclRefCounterSA< PVMFSMSharedPtrAlloc<SDPInfo> >(sdpInfo);

    OsclSharedPtr<SDPInfo> sharedSDPInfo(sdpInfo, refcnt);

    if (sdpRetVal != SDP_SUCCESS)
    {
        return PVMFFailure;
    }

    iSwitchSDPInfo = sharedSDPInfo;

    return PVMFSuccess;
}

PVMFStatus PVMFSMRTSPUnicastNode::PopulatePresentationInfoFromSwitchSDP(PVMFMediaPresentationInfo& aMediaInfo)
{
    /**
     * Populate presentation information
     */
    if (iSwitchSDPInfo == NULL)
    {
        return PVMFFailure;
    }

    /* Get SDP Session Info */
    sessionDescription* sessionInfo = iSwitchSDPInfo->getSessionInfo();

    RtspRangeType *sessionRange = OSCL_CONST_CAST(RtspRangeType*, (sessionInfo->getRange()));

    int32 sessionStartTime = 0, sessionStopTime = 0;

    sessionRange->convertToMilliSec(sessionStartTime, sessionStopTime);

    int32 duration_msec = (sessionStopTime - sessionStartTime);

    uint64 duration64;
    Oscl_Int64_Utils::set_uint64(duration64, 0, (uint32)duration_msec);

    if (sessionRange->end_is_set == true)
    {
        aMediaInfo.setDurationValue(duration64);
        aMediaInfo.setDurationTimeScale(1000);
    }
    else
    {
        aMediaInfo.SetDurationAvailable(false);
    }

    aMediaInfo.setSeekableFlag((!(sessionInfo->getRandomAccessDenied())));

    int32 numTracks = iSwitchSDPInfo->getNumMediaObjects();

    SDPAltGroupType sdpAltGroupType = sessionInfo->getSDPAltGroupType();

    PVMF_TRACK_INFO_TRACK_ALTERNATE_TYPE iAltType = PVMF_TRACK_ALTERNATE_TYPE_UNDEFINED;

    if (sdpAltGroupType == SDP_ALT_GROUP_LANGUAGE)
    {
        iAltType = PVMF_TRACK_ALTERNATE_TYPE_LANGUAGE;
    }
    else if (sdpAltGroupType == SDP_ALT_GROUP_BANDWIDTH)
    {
        iAltType = PVMF_TRACK_ALTERNATE_TYPE_BANDWIDTH;
    }

    for (int32 i = 0; i < numTracks; i++)
    {
        // Get the vector of mediaInfo as there can alternates for each track.
        Oscl_Vector<mediaInfo*, SDPParserAlloc> mediaInfoVec =
            iSwitchSDPInfo->getMediaInfo(i);

        uint32 minfoVecLen = mediaInfoVec.size();

        for (uint32 j = 0; j < minfoVecLen; j++)
        {
            mediaInfo* mInfo = mediaInfoVec[j];

            if (mInfo == NULL)
            {
                return PVMFFailure;
            }

            RtspRangeType *mediaRange = mInfo->getRtspRange();

            int32 mediaStartTime = 0, mediaStopTime = 0;

            mediaRange->convertToMilliSec(mediaStartTime, mediaStopTime);
            int32 mediaDuration_ms = mediaStopTime - mediaStartTime;
            uint64 mediaDuration64;
            Oscl_Int64_Utils::set_uint64(mediaDuration64, 0, (uint32)mediaDuration_ms);

            PVMFTrackInfo trackInfo;

            Oscl_Vector<PayloadSpecificInfoTypeBase*, SDPParserAlloc> payloadVector;
            payloadVector = mInfo->getPayloadSpecificInfoVector();

            if (payloadVector.size() == 0)
            {
                return false;
            }
            /*
            * There can be multiple payloads per media segment.
            * We only support one for now, so
            * use just the first payload
            */
            PayloadSpecificInfoTypeBase* payloadInfo = payloadVector[0];

            // set config for later
            int32 configSize = payloadInfo->configSize;
            OsclAny* config = payloadInfo->configHeader.GetRep();

            OSCL_StackString<256> mimeString;
            const char* mimeType = mInfo->getMIMEType();
            mimeString += mimeType;
            trackInfo.setTrackMimeType(mimeString);

            uint32 trackID = mInfo->getMediaInfoID();

            trackInfo.setTrackID(trackID);
            trackInfo.setPortTag(trackID);

            trackInfo.setTrackBitRate(mInfo->getBitrate());

            if (mediaRange->end_is_set == true)
            {
                trackInfo.setTrackDurationValue(mediaDuration64);
            }
            else
            {
                trackInfo.SetDurationAvailable(false);
            }

            if ((configSize > 0) && (config != NULL))
            {
                OsclMemAllocDestructDealloc<uint8> my_alloc;
                OsclRefCounter* my_refcnt;
                uint aligned_refcnt_size =
                    oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));

                uint8* my_ptr = GetMemoryChunk(my_alloc, aligned_refcnt_size + configSize);
                if (!my_ptr)
                    return PVMFFailure;

                my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
                my_ptr += aligned_refcnt_size;

                OsclMemoryFragment memfrag;
                memfrag.len = (uint32)configSize;
                memfrag.ptr = my_ptr;

                oscl_memcpy((void*)(memfrag.ptr), (const void*)config, memfrag.len);

                OsclRefCounterMemFrag tmpRefcntMemFrag(memfrag, my_refcnt, memfrag.len);
                trackInfo.setTrackConfigInfo(tmpRefcntMemFrag);
            }

            int32 dependsOnTrackID = mInfo->getDependsOnTrackID();

            if (dependsOnTrackID != -1)
            {
                trackInfo.setDependsOn();
                mediaInfo* baseMediaInfo = iSwitchSDPInfo->getMediaInfoBasedOnDependsOnID(dependsOnTrackID);
                if (baseMediaInfo == NULL)
                {
                    return PVMFFailure;
                }
                trackInfo.addDependsOnTrackID(baseMediaInfo->getMediaInfoID());
            }

            if (iAltType != PVMF_TRACK_ALTERNATE_TYPE_UNDEFINED)
            {
                /* Expose alternate track ids */
                trackInfo.setTrackAlternates(iAltType);
                for (uint32 k = 0; k < minfoVecLen; k++)
                {
                    mediaInfo* mInfo = mediaInfoVec[k];
                    if (mInfo == NULL)
                    {
                        return PVMFFailure;
                    }
                    uint32 altID = mInfo->getMediaInfoID();
                    if (altID != trackID)
                    {
                        trackInfo.addAlternateTrackID((int32)altID);
                    }
                }
            }
            aMediaInfo.addTrackInfo(trackInfo);
        }
    }

    return PVMFSuccess;
}


/*
 * This function gets called when the server responds successfully to a 3GPP FCS
 * request. The function updates RTP info for all selected track.
 */
void PVMFSMRTSPUnicastNode::UpdateRTPInfoAndFlushJitterBuffer()
{
    PVMFSMFSPChildNodeContainer* iSessionControllerNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_RTSP_SESSION_CONTROLLER_NODE);
    if (iSessionControllerNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return;
    }

    PVMFSMFSPChildNodeContainer* iJitterBufferNodeContainer =
        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
    if (iJitterBufferNodeContainer == NULL)
    {
        OSCL_LEAVE(OsclErrBadHandle);
        return;
    }
    PVMFJitterBufferExtensionInterface* jbExtIntf =
        (PVMFJitterBufferExtensionInterface*)
        (iJitterBufferNodeContainer->iExtensions[0]);

    PVRTSPEngineNodeExtensionInterface* rtspExtIntf =
        (PVRTSPEngineNodeExtensionInterface*)
        (iSessionControllerNodeContainer->iExtensions[0]);

    Oscl_Vector<StreamInfo, OsclMemAllocator> aSelectedStream;

    // Find currently selected tracks
    if (rtspExtIntf->GetStreamInfo(aSelectedStream) != PVMFSuccess)
    {
        OSCL_LEAVE(OsclErrGeneral);
    }

    /* Get actual range information from rtsp extension interface*/
    RtspRangeType rangeType;
    int32 startTime = 0;
    int32 stopTime = 0;

    if (rtspExtIntf->GetActualPlayRange(rangeType) != PVMFSuccess)
    {
        return;
    }

    rangeType.convertToMilliSec(startTime, stopTime);

    /* Use from SDP if not set */
    bool end_is_set = rangeType.end_is_set;
    if (end_is_set == false)
    {
        stopTime = iSessionStopTime;
    }

    // Loop over all selected tracks
    for (uint32 i = 0; i < iSelectedMediaPresetationInfo.getNumTracks(); i++)
    {
        // Use the selected track info to lookup port information. This is safe since track selection
        // happens once per session and port information corresponding to the tracks selected initially
        // is re-used during FCS. This will change once we support adding new tracks during FCS.
        PVMFTrackInfo *refTrackInfo = iSelectedMediaPresetationInfo.getTrackInfo(i);
        uint32 refTrackID = refTrackInfo->getTrackID();
        PVMFRTSPTrackInfo* trackInfo = FindTrackInfo(refTrackID);

        if (trackInfo == NULL)
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::UpdateRTPInfoAndFlushJitterBuffer - FindTrackInfo Failed"));
            return;
        }
        if (trackInfo->iJitterBufferInputPort == NULL)
        {
            PVMF_SM_RTSP_LOGERROR((0, "PVMFSMRTSPUnicastNode::UpdateRTPInfoAndFlushJitterBuffer - Invalid Port"));
            return;
        }

        // Update RTP info for each track.
        // Retrieve new RTP information (in the server response) from the RTSP client engine node.
        Oscl_Vector<StreamInfo, OsclMemAllocator> aSwitchSelectedStream;
        rtspExtIntf->GetSwitchStreamInfo(aSwitchSelectedStream);

        // Wait until this information is available
        if (aSwitchSelectedStream.size() == 0)
        {
            return;
        }

        if (aSwitchSelectedStream[i].ssrcIsSet)
        {
            // Reset jitter buffer only once
            if (i == 0)
            {
                jbExtIntf->ResetJitterBuffer();
            }
            jbExtIntf->setPortSSRC(trackInfo->iJitterBufferInputPort, aSwitchSelectedStream[i].iSSRC, aSwitchSelectedStream[i].ssrcIsSet);
            PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::UpdateRTPInfoAndFlushJitterBuffer() - Updating SSRC to %d", aSwitchSelectedStream[i].iSSRC));
        }

        jbExtIntf->setPortRTPParams(trackInfo->iJitterBufferInputPort,
                                    aSwitchSelectedStream[i].seqIsSet,
                                    aSwitchSelectedStream[i].seq,
                                    aSwitchSelectedStream[i].rtptimeIsSet,
                                    aSwitchSelectedStream[i].rtptime,
                                    rangeType.start_is_set,
                                    startTime,
                                    iRepositioning);
        PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::UpdateRTPInfoAndFlushJitterBuffer() - Updating seq num to %d and rtptime to %d", aSwitchSelectedStream[i].seq, aSwitchSelectedStream[i].rtptime));
    }

    /* Send actual stop time to Jitter Buffer */
    if (jbExtIntf->setPlayRange(startTime,
                                stopTime,
                                iRepositioning,
                                end_is_set) != true)
    {
        return;
    }

    // Bring client and server clocks together
    jbExtIntf->SetClientClockToServerClock();
}

/*
 * This function selects tracks from the switch SDP by matching MIME types with the
 * tracks that are currenly selected and being streamed. The selected tracks
 * are saved in a StreamInfo vector in addition to setting the 'Select' flag in
 * their mediaInfo structures.
 */
PVMFStatus PVMFSMRTSPUnicastNode::SelectTracksFromSwitchSDP(Oscl_Vector<StreamInfo, OsclMemAllocator>& aSwitchSelectedStream, PVMFMediaPresentationInfo aMediaInfo)
{

    PVMFStatus retval = PVMFSuccess;                       // Return value
    int32 currentTrackCount = 0, newTrackCount = 0;       // Keep count of current and new track
    int32 currTrackCountDiff = 0, prevTrackCountDiff = 0; // Keep track of mismatched tracks

    // Clear removed track ID vector
    iRemovedTrackIDVector.clear();

    for (int32 ii = 0; ii < iSdpInfo->getNumMediaObjects(); ii++)
    {
        // For current session, use the selected track info to find SDP tracks
        PVMFTrackInfo *refTrackInfo = iSelectedMediaPresetationInfo.getTrackInfo(ii);
        uint32 refTrackID = refTrackInfo->getTrackID();
        // Retrieve media info from current SDP
        mediaInfo* mInfo = iSdpInfo->getMediaInfoBasedOnID(refTrackID);
        // Use only selected tracks
        if (mInfo->getSelect())
        {
            currentTrackCount++;
            // Loop over all the tracks in the new SDP to find a match
            for (int32 jj = 0; jj < iSwitchSDPInfo->getNumMediaObjects(); jj++)
            {
                // Get track info and track ID for each track in the SDP
                PVMFTrackInfo* trackInfo = aMediaInfo.getTrackInfo(jj);
                uint32 trackID = trackInfo->getTrackID();
                // Use track ID to retrieve mediaInfo
                mediaInfo* sMediaInfo = iSwitchSDPInfo->getMediaInfoBasedOnID(trackID);
                if (!oscl_strncmp(mInfo->getMIMEType(), sMediaInfo->getMIMEType(), oscl_strlen(mInfo->getMIMEType())))
                {
                    newTrackCount++;
                    // Select the media info
                    sMediaInfo->setSelect();
                    // Quit loop
                    jj = iSwitchSDPInfo->getNumMediaObjects();
                    // Create streaminfo
                    StreamInfo sInfo;
                    sInfo.iSDPStreamId = trackID;
                    aSwitchSelectedStream.push_back(sInfo);
                }
            }
            currTrackCountDiff = currentTrackCount - newTrackCount;
            // Indicates track removal
            if (currTrackCountDiff > prevTrackCountDiff)
            {
                // Save this track ID in a vector. This will be needed to send EOS for those tracks
                iRemovedTrackIDVector.push_back(ii);
                // Update previous track count difference to current
                prevTrackCountDiff = currTrackCountDiff;
            }
        }
    }

    // The engine's intervention during FCS track selection is needed except the case of a perfect match between current and switch SDP.
    // A perfect match is defined as the case where all the currently selected tracks are matched with tracks from the switch
    // SDP and there are no tracks left unselected in the switch SDP.
    if ((iSwitchSDPInfo->getNumMediaObjects() == newTrackCount) && (currentTrackCount == newTrackCount))
    {
        iPerfectMatchDuringFCSTrackSelectionFlag = true;
    }
    else
    {
        iPerfectMatchDuringFCSTrackSelectionFlag = false;
    }

    // If no track is selected, return failure
    if (newTrackCount == 0)
    {
        retval = PVMFFailure;
    }

    return retval;
}


void PVMFSMRTSPUnicastNode::BypassError()
{
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMFSPPVRBase::BypassError() - In"));
    if (CheckChildrenNodesStart())
    {
        if (!iCurrentCommand.empty() && iCancelCommand.empty())
        {
            PVMFSMFSPBaseNodeCommand aCmd = iCurrentCommand.front();
            if ((aCmd.iCmd == PVMF_SMFSP_NODE_START) ||
                    (aCmd.iCmd == PVMF_SMFSP_NODE_SET_DATASOURCE_POSITION))
            {
                if (iRepositioning)
                {
                    PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::BypassError() - InRepos"));
                    iRepositioning = false;
                    iPlayListRepositioning = false;
                    iPVMFDataSourcePositionParamsPtr = NULL;
                    PVMFSMFSPChildNodeContainer* jitterBufferNodeContainer =
                        getChildNodeContainer(PVMF_SM_FSP_JITTER_BUFFER_NODE);
                    OSCL_ASSERT(jitterBufferNodeContainer);
                    if (!jitterBufferNodeContainer)
                        return;

                    PVMFJitterBufferExtensionInterface* jbExtIntf =
                        (PVMFJitterBufferExtensionInterface*)
                        (jitterBufferNodeContainer->iExtensions[0]);

                    OSCL_ASSERT(jbExtIntf);
                    if (!jbExtIntf)
                        return;

                    /* Set jitter buffer state to ready */
                    jbExtIntf->UpdateJitterBufferState();
                }
                SetState(EPVMFNodeStarted);
                if (IsAdded())
                {
                    /* wakeup the AO */
                    RunIfNotReady();
                }
                PVMF_SM_RTSP_LOGINFO((0, "PVMFSMRTSPUnicastNode::BypassError() - InLocalMode"));
                RunIfNotReady();
                CommandComplete(iCurrentCommand, iCurrentCommand.front(), PVMFSuccess, NULL, NULL, NULL);

            }
        }
    }
    PVMF_SM_RTSP_LOGSTACKTRACE((0, "PVMFSMFSPPVRBase::BypassError() - Out"));
    return;
}
