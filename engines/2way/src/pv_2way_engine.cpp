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
#ifndef PV_2WAY_SDKINFO_H_INCLUDED
#include "pv_2way_sdkinfo.h"
#endif

#include "pv_2way_engine.h"
#include "pv_2way_dec_data_channel_datapath.h"
#include "pv_2way_enc_data_channel_datapath.h"
#include "pv_2way_mux_datapath.h"

#include "OMX_Core.h"
#include "pv_omxcore.h"
#include "pvmf_omx_videodec_factory.h"
#include "pvmf_omx_enc_factory.h"
#include "pvmf_omx_audiodec_factory.h"
#include "pvmf_audio_encnode_extension.h"

#include "pvmf_video.h"
#include "pv_video_encnode_extension.h"

#include "pv_engine_observer.h"
#include "pvlogger.h"
#include "oscl_dll.h"
#include "tsc_h324m_config_interface.h"
#include "pvmf_nodes_sync_control.h"
#include "pv_2way_track_info_impl.h"
#include "pvmi_config_and_capability.h"
#include "pvt_common.h"
#include "pv_proxied_interface.h"
#include "pvmf_format_type.h"




#ifdef MEM_TRACK
#include "oscl_mem.h"
#include "oscl_mem_audit.h"
#endif


// Define entry point for this DLL
OSCL_DLL_ENTRY_POINT_DEFAULT()

//Default skipMediaData params
const uint32 resume_timestamp = 0;
#define STREAMID 0
//Early and late margins for audio and video frames
#define SYNC_EARLY_MARGIN 1000000 // 10s
#define SYNC_LATE_MARGIN 1000000 // 10s

const uint32 KSamplingRate  = 8000;
const uint32 KBitsPerSample = 16;
const uint32 KNumChannels   = 1;
const uint32 KNumPCMFrames  = 2; // 10

//TEMP -RH
#define PV2WAY_UNKNOWN_PORT -1
#define PV2WAY_IN_PORT 0
#define PV2WAY_OUT_PORT 1
#define PV2WAY_IO_PORT 3

#define INVALID_TRACK_ID 255


#define CREATE_OMX_VIDEO_DEC_NODE()  PVMFOMXVideoDecNodeFactory::CreatePVMFOMXVideoDecNode()
#define DELETE_OMX_VIDEO_DEC_NODE(n) PVMFOMXVideoDecNodeFactory::DeletePVMFOMXVideoDecNode(n)

#define CREATE_OMX_ENC_NODE()  PVMFOMXEncNodeFactory::CreatePVMFOMXEncNode()
#define DELETE_OMX_ENC_NODE(n) PVMFOMXEncNodeFactory::DeletePVMFOMXEncNode(n);

#define CREATE_OMX_AUDIO_DEC_NODE() PVMFOMXAudioDecNodeFactory::CreatePVMFOMXAudioDecNode()
#define DELETE_OMX_AUDIO_DEC_NODE(n) PVMFOMXAudioDecNodeFactory::DeletePVMFOMXAudioDecNode(n)



#define FILL_FORMAT_INFO(format_type, format_info)\
GetSampleSize(format_type,&format_info.min_sample_size,&format_info.max_sample_size);\
format_info.format = format_type;

void ReleaseTPV2WayNode(OsclAny *apTPV2WayNode)
{
    TPV2WayNode* pTPV2WayNode = OSCL_STATIC_CAST(TPV2WayNode*, apTPV2WayNode);
    OSCL_DELETE(pTPV2WayNode);
}

void ReleaseTPV2WayCmdInfo(OsclAny *apTPV2WayCmdInfo)
{
    TPV2WayCmdInfo* pTPV2WayCmdInfo = OSCL_STATIC_CAST(TPV2WayCmdInfo*, apTPV2WayCmdInfo);
    OSCL_DELETE(pTPV2WayCmdInfo);
}

void ReleasePV2WayTrackInfoInterface(OsclAny *apPV2WayTrackInfoInterface)
{
    PV2WayTrackInfoInterface* pTPV2TrackInfo = OSCL_STATIC_CAST(PV2WayTrackInfoInterface*, apPV2WayTrackInfoInterface);
    OSCL_DELETE(pTPV2TrackInfo);
}


OSCL_EXPORT_REF CPV324m2Way *CPV324m2Way::NewL(PVMFNodeInterface* aTsc,
        TPVTerminalType aTerminalType,
        PVCommandStatusObserver* aCmdStatusObserver,
        PVInformationalEventObserver *aInfoEventObserver,
        PVErrorEventObserver *aErrorEventObserver)
{
    CPV324m2Way* aRet = OSCL_NEW(CPV324m2Way, ());
    if (aRet)
    {
        int32 error = Construct(aRet, aTsc, aTerminalType, aCmdStatusObserver,
                                aInfoEventObserver, aErrorEventObserver);
        if (error)
        {
            OSCL_DELETE(aRet);
            aRet = NULL;
            OSCL_LEAVE(error);
        }
    }
    else
    {
        OSCL_LEAVE(PVMFErrNoMemory);
    }

    return aRet;
}

int32 CPV324m2Way::Construct(CPV324m2Way* aRet,
                             PVMFNodeInterface* aTsc,
                             TPVTerminalType aTerminalType,
                             PVCommandStatusObserver* aCmdStatusObserver,
                             PVInformationalEventObserver *aInfoEventObserver,
                             PVErrorEventObserver *aErrorEventObserver)
{
    int32 error = 0;
    OSCL_TRY(error, aRet->ConstructL(aTsc,
                                     aTerminalType,
                                     aCmdStatusObserver,
                                     aInfoEventObserver,
                                     aErrorEventObserver));
    return error;
}

OSCL_EXPORT_REF void CPV324m2Way::Delete(CPV324m2Way *aTerminal)
{
    OSCL_DELETE(aTerminal);
}

CPV324m2Way::CPV324m2Way() :
        OsclActiveObject(OsclActiveObject::EPriorityNominal, "PV2WayEngine"),
        iState(EIdle),
        iLastState(EIdle),
        iCmdStatusObserver(NULL),
        iInfoEventObserver(NULL),
        iErrorEventObserver(NULL),
        iCommandId(0),
        iVideoEncDatapath(NULL),
        iVideoDecDatapath(NULL),
        iAudioEncDatapath(NULL),
        iAudioDecDatapath(NULL),
        iIsStackConnected(false),
        iMuxDatapath(NULL),
        iInitInfo(NULL),
        iConnectInfo(NULL),
        iDisconnectInfo(NULL),
        iResetInfo(NULL),
        iCancelInfo(NULL),
        iLogger(NULL),
        iMinIFrameRequestInterval(DEFAULT_MIN_IFRAME_REQ_INT),
        iIFrameReqTimer("IFrameReqTimer"),
        iEndSessionTimer(NULL),
        iRemoteDisconnectTimer(NULL),
        isIFrameReqTimerActive(false),
        iIncomingAudioTrackTag(INVALID_TRACK_ID),
        iIncomingVideoTrackTag(INVALID_TRACK_ID),
        iOutgoingAudioTrackTag(INVALID_TRACK_ID),
        iOutgoingVideoTrackTag(INVALID_TRACK_ID),
        iVideoEncQueryIntCmdId(-1),
        iTSCInterface(NULL),
        iTSC324mInterface(NULL),
        iPendingTscReset(-1),
        iPendingAudioEncReset(-1),
        iPendingVideoEncReset(-1),
        iReferenceCount(1),
        ipEncNodeCapabilityAndConfig(NULL),
        ipEncNodeCapConfigInterface(NULL),
        iUsingExternalVideoDecBuffers(false),
        iUsingExternalAudioDecBuffers(false)
{
    iLogger = PVLogger::GetLoggerObject("2wayEngine");
    iSyncControlPVUuid = PvmfNodesSyncControlUuid;
    iVideoEncPVUuid = PVMp4H263EncExtensionUUID;
    iCapConfigPVUuid = PVMI_CAPABILITY_AND_CONFIG_PVUUID;

    iAudioEncPVUuid = PVAudioEncExtensionUUID;

    OMX_MasterInit();

    //creating timers
    iEndSessionTimer = OSCL_NEW(OsclTimer<OsclMemAllocator>, (END_SESSION_TIMER, END_SESSION_TIMER_FREQUENCY));
    iRemoteDisconnectTimer = OSCL_NEW(OsclTimer<OsclMemAllocator>, (REMOTE_DISCONNECT_TIMER, REMOTE_DISCONNECT_TIMER_FREQUENCY));
    iReadDataLock.Create();

}

CPV324m2Way::~CPV324m2Way()
{
    LogFunction("~CPV324m2Way", START_FUNCTION);

    Oscl_Map<PVMFFormatType, CPvtMediaCapability*, OsclMemAllocator, pvmf_format_type_key_compare_class>::iterator it = iStackSupportedFormats.begin();
    while (it != iStackSupportedFormats.end())
    {
        CPvtMediaCapability* media_capability = (*it++).second;
        OSCL_DELETE(media_capability);
    }
    iStackSupportedFormats.clear();

    Cancel();
    iIncomingChannelParams.clear();
    iOutgoingChannelParams.clear();

    iIncomingAudioCodecs.clear();
    iOutgoingAudioCodecs.clear();
    iIncomingVideoCodecs.clear();
    iOutgoingVideoCodecs.clear();


    iFormatCapability.clear();
    iSinkNodeList.clear();
    ClearVideoEncoderNode();


    iReadDataLock.Lock();
    if (iVideoEncDatapath)
    {
        OSCL_DELETE(iVideoEncDatapath);
        iVideoEncDatapath = NULL;
    }

    if (iVideoDecDatapath)
    {
        OSCL_DELETE(iVideoDecDatapath);
        iVideoDecDatapath = NULL;
    }

    if (iAudioEncDatapath)
    {
        OSCL_DELETE(iAudioEncDatapath);
        iAudioEncDatapath = NULL;
    }

    if (iAudioDecDatapath)
    {
        OSCL_DELETE(iAudioDecDatapath);
        iAudioDecDatapath = NULL;
    }

    iReadDataLock.Unlock();

    if (iMuxDatapath)
    {
        OSCL_DELETE(iMuxDatapath);
        iMuxDatapath = NULL;
    }

    PVMFNodeInterface * nodeIFace = (PVMFNodeInterface *)iTscNode;
    if (nodeIFace)
    {
        OSCL_DELETE(nodeIFace);
        iTscNode.Clear();
    }

    OMX_MasterDeinit();

    if (iEndSessionTimer)
    {
        iEndSessionTimer->Clear();
        OSCL_DELETE(iEndSessionTimer);
        iEndSessionTimer = NULL;
    }

    if (iRemoteDisconnectTimer)
    {
        iRemoteDisconnectTimer->Clear();
        OSCL_DELETE(iRemoteDisconnectTimer);
        iRemoteDisconnectTimer = NULL;
    }
    iReadDataLock.Close();

    LogFunction("~CPV324m2Way", LEAVE_FUNCTION);
}

void CPV324m2Way::LogFunction(const char* aFunctionName,
                              TFunctionLocation aFunctionLocation,
                              PVMFStatus aStatus)
{
    switch (aFunctionLocation)
    {

        case START_FUNCTION:

        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "CPV324m2Way::%s\n", aFunctionName));
            break;
        }
        case LEAVE_FUNCTION:

        {
            switch (aStatus)
            {
                case PVMFSuccess:
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "CPV324m2Way::%s - done- Success\n", aFunctionName));
                    break;
                case PVMFFailure:
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "CPV324m2Way::%s - done- Failed\n", aFunctionName));
                    break;
                case PVMFNotSet:
                default:
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                    (0, "CPV324m2Way::%s - done\n", aFunctionName));
                    break;
            }
            break;
        }
        default:
            break;
    }
}


void CPV324m2Way::ClearVideoEncoderNode()
{
    LogFunction("ClearVideoEncoderNode", START_FUNCTION);
    PVMFNodeInterface * nodeIFace = (PVMFNodeInterface *)iVideoEncNode;
    if (nodeIFace)
    {
        nodeIFace->ThreadLogoff();
        if (iVideoEncNodeInterface.iInterface) iVideoEncNodeInterface.iInterface->removeRef();

        DELETE_OMX_ENC_NODE(nodeIFace);
        iVideoEncNode.Clear() ;
        iVideoEncNodeInterface.Reset();
    }
    LogFunction("ClearVideoEncoderNode", LEAVE_FUNCTION);
}

PVCommandId CPV324m2Way::GetSDKInfo(PVSDKInfo &aSDKInfo, OsclAny* aContextData)
{
    LogFunction("GetSDKInfo", START_FUNCTION);
    FillSDKInfo(aSDKInfo);

    TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_GET_SDK_INFO, aContextData);

    cmd->status = PVMFSuccess;
    Dispatch(cmd);
    LogFunction("GetSDKInfo", LEAVE_FUNCTION);
    return iCommandId++;
}

PVCommandId CPV324m2Way::GetSDKModuleInfo(PVSDKModuleInfo &aSDKModuleInfo,
        OsclAny* aContextData)
{
    OSCL_UNUSED_ARG(aSDKModuleInfo);
    LogFunction("GetSDKModuleInfo", START_FUNCTION);

    TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_GET_SDK_MODULE_INFO, aContextData);
    cmd->status = PVMFSuccess;
    Dispatch(cmd);
    LogFunction("GetSDKModuleInfo", LEAVE_FUNCTION);
    return iCommandId++;
}

void CPV324m2Way::CreateTSCNode()
{
    LogFunction("CreateTSCNode", START_FUNCTION);
    iTscNode = TPV2WayNode(OSCL_NEW(TSC_324m, (PV_LOOPBACK_MUX)));
    iTSC324mInterface = (TSC_324m *)iTscNode.iNode;
    iTSCInterface = (TSC *)iTSC324mInterface;
    ((TSC_324m*)(iTscNode.iNode))->SetTscObserver(this);
    // move this to TSC node!!
    // Create the list of stack supported formats
    GetStackSupportedFormats();
    LogFunction("CreateTSCNode", LEAVE_FUNCTION);
}

void CPV324m2Way::PreInit()
{
    LogFunction("PreInit", START_FUNCTION);

    PVMFNodeSessionInfo sessionInfo;

    switch (iState)
    {
        case EIdle:

            if (iTerminalType == PV_324M)
            {
                CreateTSCNode();
            }

            if (((PVMFNodeInterface *)iTscNode) == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::PreInit unable to allocate tsc node\n"));
                LogFunction("PreInit", LEAVE_FUNCTION, PVMFFailure);
                return;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "CPV324m2Way::PreInit created TSC Node(%x)", (PVMFNodeInterface *)iTscNode));

            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::Init Error - invalid state\n"));
            OSCL_LEAVE(PVMFErrInvalidState);
            break;
    }
    LogFunction("PreInit", LEAVE_FUNCTION);
}



PVCommandId CPV324m2Way::Init(PV2WayInitInfo& aInitInfo,
                              OsclAny* aContextData)
{
    LogFunction("Init", START_FUNCTION);

    switch (iState)
    {
        case EIdle:
        {
            if (iInitInfo)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "CPV324m2Way::Init cmd already sent out\n"));
                OSCL_LEAVE(PVMFErrBusy);
            }

            InitiateSession(iTscNode);

            ((TSC_324m*)(iTscNode.iNode))->SetMultiplexingDelayMs(0);

            SetPreferredCodecs(aInitInfo);

            SetInitInfo(aContextData);

            SetState(EInitializing);

            CheckState();
#ifdef MEM_TRACK
            printf("\nMemStats at Engine Init\n");
            MemStats();
#endif
            break;
        }

        case ESetup:
            SetInitInfo(aContextData);
            iInitInfo->status = PVMFSuccess;
            Dispatch(iInitInfo);
            iInitInfo = NULL;
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::Init Error - invalid state\n"));
            OSCL_LEAVE(PVMFErrInvalidState);
            break;
    }

    LogFunction("Init", LEAVE_FUNCTION);
    return iCommandId++;
}

void CPV324m2Way::SetResetInfo(OsclAny* aContextData)
{
    LogFunction("SetResetInfo", START_FUNCTION);
    iResetInfo = GetCmdInfoL();
    iResetInfo->type = PVT_COMMAND_RESET;
    iResetInfo->id = iCommandId;
    iResetInfo->contextData = aContextData;
    LogFunction("SetResetInfo", LEAVE_FUNCTION);
}

bool CPV324m2Way::AnyNodeStillAdded(Oscl_Vector<TPV2WayNode*, OsclMemAllocator> aNodes)
{
    LogFunction("AnyNodeStillAdded", START_FUNCTION);
    uint32 ii = 0;
    //checking if any sources or sinks still added.
    for (ii = 0; ii < aNodes.size(); ii++)
    {
        if (aNodes[ii])
        {
            LogFunction("AnyNodeStillAdded", LEAVE_FUNCTION);
            return true;
        }
    }
    LogFunction("AnyNodeStillAdded", LEAVE_FUNCTION);
    return false;
}


PVCommandId CPV324m2Way::Reset(OsclAny* aContextData)
{
    LogFunction("Reset", START_FUNCTION);
    //checking if any sources or sinks still added.
    if (AnyNodeStillAdded(iSinkNodes))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::Reset SinkNodes not removed before Reset\n"));
        LogFunction("Reset", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFFailure);
    }

    if (AnyNodeStillAdded(iSourceNodes))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::Reset SourceNodes not removed before Reset\n"));
        LogFunction("Reset", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFFailure);
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::Reset state: %d\n", iState));

    if (iResetInfo)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::Reset cmd already sent out\n"));
        LogFunction("Reset", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFErrBusy);
    }

    switch (iState)
    {
        case EIdle:
            SetResetInfo(aContextData);
            iResetInfo->status = PVMFSuccess;
            Dispatch(iResetInfo);
            iResetInfo = NULL;
            break;

        case EInitializing:
            //Notify application that init command has been cancelled.
            iInitInfo->status = PVMFErrCancelled;
            Dispatch(iInitInfo);
            iInitInfo = NULL;
            //Fall through to next case.

        case ESetup:
            SetResetInfo(aContextData);
            InitiateReset();
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::Reset - invalid state %d\n", iState));
            OSCL_LEAVE(PVMFErrInvalidState);
            break;
    }
#ifdef MEM_TRACK
    printf("\nMemStats After Engine Reset\n");
    MemStats();
#endif
    LogFunction("Reset", LEAVE_FUNCTION);
    return iCommandId++;
}

bool CPV324m2Way::ProperStateForAction(PV2WayAction aAction)
{
    LogFunction("ProperStateForAction", START_FUNCTION);
    switch (aAction)
    {
        case EA_ADD_DATA_SOURCE:
        case EA_REMOVE_DATA_SOURCE:
        case EA_ADD_DATA_SINK:
        case EA_REMOVE_DATA_SINK:
        case EA_PAUSE:
        case EA_RESUME:
        {
            switch (iState)
            {
                case EIdle:
                case EInitializing:
                case EResetting:
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "CPV324m2Way::ProperStateForAction(%d) - invalid state(%d)",
                                     aAction, iState));
                    LogFunction("ProperStateForAction", LEAVE_FUNCTION, PVMFFailure);
                    OSCL_LEAVE(PVMFErrInvalidState);
                    break;
                default:
                    //State check okay.
                    break;
            }
            LogFunction("ProperStateForAction", LEAVE_FUNCTION);
            return true;
            break;
        }
        default:
            break;
    }
    LogFunction("ProperStateForAction", LEAVE_FUNCTION);
    return false;
}

PVCommandId CPV324m2Way::AddDataSource(PVTrackId aChannelId,
                                       PVMFNodeInterface& aDataSource,
                                       OsclAny* aContextData)
{
    LogFunction("AddDataSource", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::AddDataSource aChannelId=%d, (%x, %x, %x)",
                     aChannelId, &aDataSource, 0, aContextData));

    if (!((TSC_324m *)(PVMFNodeInterface *)iTscNode.iNode)->IsEstablishedLogicalChannel(OUTGOING,
            aChannelId))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::AddDataSourceL Not an established logical channel in the stack\n"));
        LogFunction("AddDataSource", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFErrArgument);
    }

    if (ProperStateForAction(EA_ADD_DATA_SOURCE))
    {
        //As of v4, we'll need to initialize the node first before
        //querying its capabilities

        // Add the Data Source to the list of source nodes.
        PVMFNodeInterface *node = &aDataSource;
        TPV2WayNode* srcNode = OSCL_NEW(TPV2WayNode, (node));
        OSCL_TRAPSTACK_PUSH(OsclTrapItem(ReleaseTPV2WayNode, srcNode));

        InitiateSession(*srcNode);
        iSourceNodes.push_back(srcNode);

        TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_ADD_DATA_SOURCE, aChannelId, aContextData);
        cmd->status = PVMFSuccess;

        SendNodeCmdL(PV2WAY_NODE_CMD_INIT, srcNode, this, cmd);
        OSCL_TRAPSTACK_POP();
    }
    LogFunction("AddDataSource", LEAVE_FUNCTION);
    return iCommandId++;
}

void CPV324m2Way::DoAddDataSourceTscNode(CPVDatapathNode& datapathnode,
        CPV2WayDataChannelDatapath* datapath,
        TPV2WayCmdInfo *cmd)
{
    LogFunction("DoAddDataSourceTscNode", START_FUNCTION);
    //Add tsc node to datapath
    datapathnode.iNode = iTscNode;
    datapathnode.iConfigure = NULL;
    datapathnode.iCanNodePause = false;
    datapathnode.iLoggoffOnReset = false;
    datapathnode.iIgnoreNodeState = true;
    datapathnode.iInputPort.iRequestPortState = EPVMFNodeCreated;
    datapathnode.iInputPort.iCanCancelPort = true;
    datapathnode.iInputPort.iPortSetType = EAppDefined;
    datapathnode.iInputPort.iFormatType = datapath->GetFormat();
    datapathnode.iInputPort.iPortTag = cmd->iPvtCmdData;
    datapathnode.iOutputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    datapathnode.iOutputPort.iPortTag = PV2WAY_UNKNOWN_PORT;
    datapath->AddNode(datapathnode);
    LogFunction("DoAddDataSourceTscNode", LEAVE_FUNCTION);
}


void CPV324m2Way::DoAddDataSourceNode(TPV2WayNode& aNode,
                                      CPVDatapathNode& datapathnode,
                                      CPV2WayDataChannelDatapath* datapath)
{
    LogFunction("DoAddDataSourceNode", START_FUNCTION);
    //Add source node to datapath
    TPV2WayNode* srcNode = &aNode;
    datapathnode.iNode = *srcNode;
    datapathnode.iLoggoffOnReset = true;
    datapathnode.iCanNodePause = true;
    datapathnode.iIgnoreNodeState = false;
    datapathnode.iOutputPort.iRequestPortState = EPVMFNodeInitialized;
    datapathnode.iOutputPort.iPortSetType = EConnectedPortFormat;
    datapathnode.iOutputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    datapathnode.iOutputPort.iPortTag = PV2WAY_OUT_PORT;
    datapath->AddNode(datapathnode);
    LogFunction("DoAddDataSourceNode", LEAVE_FUNCTION);
}


void CPV324m2Way::DoAddVideoEncNode(CPVDatapathNode& datapathnode,
                                    CPV2WayDataChannelDatapath* datapath)
{
    LogFunction("DoAddVideoEncNode", START_FUNCTION);
    //Add video enc node to datapath
    datapathnode.iNode = iVideoEncNode;
    datapathnode.iConfigure = this;
    datapathnode.iConfigTime = EConfigBeforeInit;
    datapathnode.iCanNodePause = true;
    datapathnode.iLoggoffOnReset = false;
    datapathnode.iIgnoreNodeState = false;
    datapathnode.iInputPort.iRequestPortState = EPVMFNodeInitialized;
    datapathnode.iInputPort.iPortSetType = EUserDefined;
    datapathnode.iInputPort.iFormatType = PVMF_MIME_YUV420;
    datapathnode.iInputPort.iPortTag = PV2WAY_IN_PORT;
    datapathnode.iOutputPort.iRequestPortState = EPVMFNodeInitialized;
    datapathnode.iOutputPort.iPortSetType = EConnectedPortFormat;
    datapathnode.iOutputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    datapathnode.iOutputPort.iPortTag = PV2WAY_OUT_PORT;
    datapath->AddNode(datapathnode);
    LogFunction("DoAddVideoEncNode", LEAVE_FUNCTION);
}

void CPV324m2Way::DoAddAudioEncNode(CPVDatapathNode& datapathnode,
                                    CPV2WayDataChannelDatapath* datapath)
{
    LogFunction("DoAddAudioEncNode", START_FUNCTION);
    //Add audio enc node to datapath
    datapathnode.iNode = iAudioEncNode;
    datapathnode.iConfigure = this;
    datapathnode.iConfigTime = EConfigBeforeInit;
    datapathnode.iCanNodePause = true;
    datapathnode.iLoggoffOnReset = false;
    datapathnode.iIgnoreNodeState = false;
    datapathnode.iInputPort.iRequestPortState = EPVMFNodeInitialized;
    datapathnode.iInputPort.iPortSetType = EUserDefined;
    datapathnode.iInputPort.iFormatType = PVMF_MIME_PCM16;
    datapathnode.iInputPort.iPortTag = PV2WAY_IN_PORT;
    datapathnode.iOutputPort.iRequestPortState = EPVMFNodeInitialized;
    datapathnode.iOutputPort.iPortSetType = EConnectedPortFormat;
    datapathnode.iOutputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    datapathnode.iOutputPort.iPortTag = PV2WAY_OUT_PORT;
    datapath->AddNode(datapathnode);
    LogFunction("DoAddAudioEncNode", LEAVE_FUNCTION);
}


bool CPV324m2Way::IsFormatSupported(Oscl_Vector<PVMFFormatType, OsclMemAllocator> aCapability,
                                    CPV2WayDataChannelDatapath& aDatapath)
{
    bool formatSupported = false;
    for (uint i = 0; i < aCapability.size(); ++i)
    {
        if (aDatapath.GetSourceSinkFormat() == aCapability[i])
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "CPV324m2Way::IsFormatSupported - format %s is supported\n",
                             (aCapability[i]).getMIMEStrPtr()));
            formatSupported = true;
            break;
        }
    }
    return formatSupported;
}


void CPV324m2Way::CreateVideoOutgoingDatapath(TPV2WayCmdInfo* aCmd,
        TPV2WayNode& aNode,
        CPVDatapathNode aDatapathnode,
        CPV2WayDataChannelDatapath* apDatapath)
{
    LogFunction("CreateVideoOutgoingDatapath", START_FUNCTION);
    // video media type
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                    (0, "CPV324m2Way::CreateVideoOutgoingDatapath channel id =%d\n",
                     aCmd->iPvtCmdData));
    apDatapath->SetChannelId(aCmd->iPvtCmdData);

    //Add source node to datapath
    aDatapathnode.iConfigure = NULL;
    DoAddDataSourceNode(aNode, aDatapathnode, apDatapath);

    if (!apDatapath->GetSourceSinkFormat().isCompressed())
    {
        //Add video enc node to datapath
        DoAddVideoEncNode(aDatapathnode, apDatapath);
    }
    //Add tsc node to datapath
    DoAddDataSourceTscNode(aDatapathnode, apDatapath, aCmd);

    apDatapath->SetCmd(aCmd);
    LogFunction("CreateVideoOutgoingDatapath", LEAVE_FUNCTION);
}

void CPV324m2Way::CreateAudioOutgoingDatapath(TPV2WayCmdInfo* aCmd,
        TPV2WayNode& aNode,
        CPVDatapathNode aDatapathnode,
        CPV2WayDataChannelDatapath* apDatapath)
{
    LogFunction("CreateAudioOutgoingDatapath", START_FUNCTION);
    // video media type
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                    (0, "CPV324m2Way::CreateAudioOutgoingDatapath channel id =%d\n",
                     aCmd->iPvtCmdData));
    apDatapath->SetChannelId(aCmd->iPvtCmdData);

    //Add source node to datapath
    aDatapathnode.iConfigure = NULL;
    aDatapathnode.iCanNodePause = true;
    DoAddDataSourceNode(aNode, aDatapathnode, apDatapath);

    if (!apDatapath->GetSourceSinkFormat().isCompressed())
    {
        //Add audio enc node to datapath
        DoAddAudioEncNode(aDatapathnode, apDatapath);
    }

    //Add tsc node to datapath
    DoAddDataSourceTscNode(aDatapathnode, apDatapath, aCmd);

    apDatapath->SetCmd(aCmd);
    LogFunction("CreateAudioOutgoingDatapath", LEAVE_FUNCTION);
}

void CPV324m2Way::DoAddDataSource(TPV2WayNode& aNode,
                                  const PVMFCmdResp& aResponse)
{
    LogFunction("DoAddDataSource", START_FUNCTION);
    TPV2WayNode* srcNode = &aNode;
    PVMFNodeInterface *node = srcNode->iNode;
    PVMFNodeCapability capability;
    CPVDatapathNode datapathnode;
    CPV2WayNodeContextData *data = (CPV2WayNodeContextData *) aResponse.GetContext();
    TPV2WayCmdInfo *cmd = (TPV2WayCmdInfo *)data->iContextData;

    cmd->status = aResponse.GetCmdStatus();

    if (node->GetCapability(capability) != PVMFSuccess ||
            !capability.iOutputFormatCapability.size())
    {
        OSCL_DELETE(srcNode);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::DoAddDataSource - unable to get capability\n"));
        OSCL_LEAVE(PVMFFailure);
    }

    CPV2WayDataChannelDatapath* datapath = GetDataPath(OUTGOING, capability.iOutputFormatCapability[0]);
    if (!datapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::DoAddDataSource - no datapath\n"));
        OSCL_LEAVE(PVMFFailure);
    }


    if (!IsFormatSupported(capability.iOutputFormatCapability, *datapath))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::DoAddDataSource capability is not supported\n"));
        OSCL_LEAVE(PVMFErrNotSupported);
    }

    if (datapath->GetState() != EClosed)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::DoAddDataSource invalid state (not closed)\n"));
        OSCL_LEAVE(PVMFErrInvalidState);
    }

    if (datapath->GetSourceSinkFormat().isVideo())
    {
        CreateVideoOutgoingDatapath(cmd, aNode, datapathnode, datapath);
    }

    else if (datapath->GetSourceSinkFormat().isAudio())
    {
        CreateAudioOutgoingDatapath(cmd, aNode, datapathnode, datapath);
    }

    LogFunction("DoAddDataSource", LEAVE_FUNCTION);
}

PVCommandId CPV324m2Way::DoRemoveDataSourceSink(PVMFNodeInterface& aEndPt,
        OsclAny* aContextData)
{
    LogFunction("DoRemoveDataSourceSink", START_FUNCTION);
    CPV2WayDataChannelDatapath *datapath = GetDataPath(aEndPt);
    TPV2WayCmdInfo *cmd = GetCmdInfoL();

    if (IsEncDatapath(datapath))
    {
        cmd->type = PVT_COMMAND_REMOVE_DATA_SOURCE;
    }
    else if (IsDecDatapath(datapath))
    {
        cmd->type = PVT_COMMAND_REMOVE_DATA_SINK;
    }
    else
    {
        // Just remove the node from sink and source nodes list if still in the list

        TPV2WayNode* node = RemoveTPV2WayNode(iSinkNodes, &aEndPt);

        if (!node)
        {
            // Not there in sink node list . Check in source nodes
            node = RemoveTPV2WayNode(iSourceNodes, &aEndPt);
        }

        if (node)
        {
            //Successfully found and removed the node from  sink or source nodes ,so delete it.
            OSCL_DELETE(node);
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::RemoveDataSinkSource unknown node!\n"));
        LogFunction("DoRemoveDataSourceSink", LEAVE_FUNCTION, PVMFErrArgument);
        OSCL_LEAVE(PVMFErrArgument);
    }

    switch (datapath->GetState())
    {
        case EClosing:
            //Close command already in progress
            if (datapath->GetCmdInfo())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "CPV324m2Way::RemoveDataSourceL cmd already sent out\n"));
                LogFunction("DoRemoveDataSourceSink", LEAVE_FUNCTION, PVMFErrBusy);
                OSCL_LEAVE(PVMFErrBusy);
            }
            //Already closing because of error or remote close
            // fall-through to next state
        case EOpened:
        case EOpening:
        case EPaused:
        case EPausing:
        case EUnpausing:
        {
            cmd->id = iCommandId;
            cmd->contextData = aContextData;
            datapath->SetCmd(cmd);
        }
        break;
        case EClosed:
            // Remove the node if exists in sink or source even data path is closed
            break;

        default:

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::DoRemoveDataSourceSink - invalid path state\n"));
            LogFunction("DoRemoveDataSourceSink", LEAVE_FUNCTION, PVMFErrInvalidState);
            OSCL_LEAVE(PVMFErrInvalidState);
            break;
    }

    TPV2WayNode* node = 0;

    if (cmd->type == PVT_COMMAND_REMOVE_DATA_SINK)
    {
        node = RemoveTPV2WayNode(iSinkNodes, &aEndPt);
    }
    else if (cmd->type == PVT_COMMAND_REMOVE_DATA_SOURCE)
    {
        node = RemoveTPV2WayNode(iSourceNodes, &aEndPt);
    }
    OSCL_DELETE(node);
    LogFunction("DoRemoveDataSourceSink", LEAVE_FUNCTION);
    return iCommandId++;
}

PVCommandId CPV324m2Way::RemoveDataSource(PVMFNodeInterface& aDataSource,
        OsclAny* aContextData)
{
    LogFunction("RemoveDataSource", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::RemoveDataSource(%x, %x, %x)",
                     &aDataSource, 0, aContextData));

    PVCommandId id = 0;
    if (ProperStateForAction(EA_REMOVE_DATA_SOURCE))
    {
        id = DoRemoveDataSourceSink(aDataSource, aContextData);
    }
    LogFunction("RemoveDataSource", LEAVE_FUNCTION);
    return id;
}


PVCommandId CPV324m2Way::AddDataSink(PVTrackId aChannelId,
                                     PVMFNodeInterface& aDataSink,
                                     OsclAny* aContextData)
{
    LogFunction("AddDataSink", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::AddDataSinkL(%x, %d, %x)", &aDataSink, 0,
                     aContextData));
    if (ProperStateForAction(EA_ADD_DATA_SINK))
    {
        //As of v4, we'll need to initialize the node first before
        //querying its capabilities.
        TPV2WayNode* sinkNode = OSCL_NEW(TPV2WayNode, (&aDataSink));
        OSCL_TRAPSTACK_PUSH(OsclTrapItem(ReleaseTPV2WayNode, sinkNode));
        InitiateSession(*sinkNode);
        iSinkNodes.push_back(sinkNode);
        SupportedSinkNodeInterfaces(sinkNode);
        TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_ADD_DATA_SINK, aChannelId, aContextData);
        SendNodeCmdL(PV2WAY_NODE_CMD_INIT, sinkNode, this, cmd);
        OSCL_TRAPSTACK_POP();
    }
    LogFunction("AddDataSink", LEAVE_FUNCTION);
    return iCommandId++;
}

void CPV324m2Way::DoAddDataSinkTscNode(CPVDatapathNode& datapathnode,
                                       CPV2WayDataChannelDatapath* datapath,
                                       TPV2WayCmdInfo *cmd)
{
    LogFunction("DoAddDataSinkTscNode", START_FUNCTION);
    //Add tsc node to datapath
    datapathnode.iNode = iTscNode;
    datapathnode.iConfigure = NULL;
    datapathnode.iCanNodePause = false;
    datapathnode.iIgnoreNodeState = true;
    datapathnode.iOutputPort.iRequestPortState = EPVMFNodeCreated;
    datapathnode.iOutputPort.iCanCancelPort = true;
    datapathnode.iOutputPort.iPortSetType = EAppDefined;
    datapathnode.iOutputPort.iFormatType = datapath->GetFormat();
    datapathnode.iOutputPort.iPortTag = -cmd->iPvtCmdData;
    // Need to put in the LC number here
    //datapathnode.iOutputPort.iPortTag = GetStackNodePortTag(EPV2WayAudioOut);
    datapath->AddNode(datapathnode);
    LogFunction("DoAddDataSinkTscNode", LEAVE_FUNCTION);
}



void CPV324m2Way::DoAddDataSinkNodeForH263_M4V(TPV2WayNode& aNode,
        CPVDatapathNode& datapathnode,
        CPV2WayDataChannelDatapath* datapath)
{
    LogFunction("DoAddDataSinkNodeForH263_M4V", START_FUNCTION);
    //Add sink node to datapath
    TPV2WayNode* sinkNode = &aNode;
    datapathnode.iNode.iNode = sinkNode->iNode;
    datapathnode.iNode.iSessionId = sinkNode->iSessionId;
    datapathnode.iConfigure = NULL;
    datapathnode.iCanNodePause = true;
    datapathnode.iLoggoffOnReset = true;
    datapathnode.iIgnoreNodeState = false;
    datapathnode.iInputPort.iRequestPortState = EPVMFNodeInitialized;
    datapathnode.iInputPort.iPortSetType = EConnectedPortFormat;
    datapathnode.iInputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    datapathnode.iInputPort.iPortTag = PV2WAY_IN_PORT;
    datapathnode.iOutputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    datapathnode.iOutputPort.iPortTag = PV2WAY_UNKNOWN_PORT;
    datapath->AddNode(datapathnode);
    LogFunction("DoAddDataSinkNodeForH263_M4V", LEAVE_FUNCTION);
}

void CPV324m2Way::DoAddDataSinkNodeForAVC(TPV2WayNode& arNode,
        CPVDatapathNode& arDatapathnode,
        CPV2WayDataChannelDatapath* apDatapath)
{
    LogFunction("DoAddDataSinkNodeForAVC", START_FUNCTION);
    //Add sink node to datapath
    TPV2WayNode* pSinkNode = &arNode;
    arDatapathnode.iNode.iNode = pSinkNode->iNode;
    arDatapathnode.iNode.iSessionId = pSinkNode->iSessionId;
    arDatapathnode.iConfigure = NULL;
    arDatapathnode.iCanNodePause = true;
    arDatapathnode.iLoggoffOnReset = true;
    arDatapathnode.iIgnoreNodeState = false;
    arDatapathnode.iInputPort.iRequestPortState = EPVMFNodeInitialized;
    arDatapathnode.iInputPort.iPortSetType = EConnectedPortFormat;
    arDatapathnode.iInputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    arDatapathnode.iInputPort.iPortTag = PV2WAY_IN_PORT;
    arDatapathnode.iOutputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    arDatapathnode.iOutputPort.iPortTag = PV2WAY_UNKNOWN_PORT;
    apDatapath->AddNode(arDatapathnode);
    LogFunction("DoAddDataSinkNodeForAVC", LEAVE_FUNCTION);
}


void CPV324m2Way::DoAddDataSinkGeneric(TPV2WayNode& aNode,
                                       CPVDatapathNode& datapathnode,
                                       CPV2WayDataChannelDatapath* apDatapath)
{
    LogFunction("DoAddDataSinkGeneric", START_FUNCTION);
    //Add sink node to datapath
    TPV2WayNode* sinkNode = &aNode;
    datapathnode.iNode = *sinkNode;
    datapathnode.iConfigure = NULL;
    datapathnode.iCanNodePause = true;
    datapathnode.iLoggoffOnReset = true;
    datapathnode.iIgnoreNodeState = false;
    datapathnode.iInputPort.iRequestPortState = EPVMFNodeInitialized;
    datapathnode.iInputPort.iPortSetType = EConnectedPortFormat;
    datapathnode.iInputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    datapathnode.iInputPort.iPortTag = PV2WAY_IN_PORT;
    datapathnode.iOutputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    datapathnode.iOutputPort.iPortTag = PV2WAY_UNKNOWN_PORT;
    apDatapath->AddNode(datapathnode);
    LogFunction("DoAddDataSinkGeneric", LEAVE_FUNCTION);
}

void CPV324m2Way::DoAddVideoDecNode(CPVDatapathNode& datapathnode,
                                    CPV2WayDataChannelDatapath* apDatapath)
{
    LogFunction("DoAddVideoDecNode", START_FUNCTION);
    //Add video dec node to datapath
    if (iVideoDecNode)
    {
        datapathnode.iNode = iVideoDecNode;
        datapathnode.iConfigure = this;
        datapathnode.iConfigTime = EConfigBeforePrepare;
        datapathnode.iCanNodePause = true;
        datapathnode.iIgnoreNodeState = false;
        datapathnode.iInputPort.iRequestPortState = EPVMFNodeInitialized;
        datapathnode.iInputPort.iPortSetType = EConnectedPortFormat;
        datapathnode.iInputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
        datapathnode.iInputPort.iPortTag = PV2WAY_IN_PORT;
        datapathnode.iOutputPort.iRequestPortState = EPVMFNodeInitialized;
        datapathnode.iOutputPort.iPortSetType = EUserDefined;
        datapathnode.iOutputPort.iFormatType = PVMF_MIME_YUV420;
        datapathnode.iOutputPort.iPortTag = PV2WAY_OUT_PORT;
        apDatapath->AddNode(datapathnode);
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                        (0, "CPV324m2Way::DoAddVideoDecNode No video dec node to add.\n"));
    }
    LogFunction("DoAddVideoDecNode", LEAVE_FUNCTION);
}

void CPV324m2Way::DoAddAudioDecNode(CPVDatapathNode& datapathnode,
                                    CPV2WayDataChannelDatapath* apDatapath)
{
    LogFunction("DoAddAudioDecNode", START_FUNCTION);
    //Add audio dec node to datapath
    if (iAudioDecNode)
    {
        datapathnode.iNode = iAudioDecNode;
        datapathnode.iConfigure = this;
        datapathnode.iConfigTime = EConfigBeforePrepare;
        datapathnode.iCanNodePause = true;
        datapathnode.iIgnoreNodeState = false;
        datapathnode.iInputPort.iRequestPortState = EPVMFNodeInitialized;
        datapathnode.iInputPort.iPortSetType = EConnectedPortFormat;
        datapathnode.iInputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
        datapathnode.iInputPort.iPortTag = PV2WAY_IN_PORT;
        datapathnode.iOutputPort.iRequestPortState = EPVMFNodeInitialized;
        datapathnode.iOutputPort.iPortSetType = EUserDefined;
        datapathnode.iOutputPort.iFormatType = PVMF_MIME_PCM16;
        datapathnode.iOutputPort.iPortTag = PV2WAY_OUT_PORT;
        apDatapath->AddNode(datapathnode);
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                        (0, "CPV324m2Way::DoAddAudioDecNode No audio dec node to add.\n"));
    }
    LogFunction("DoAddAudioDecNode", LEAVE_FUNCTION);
}


void CPV324m2Way::CreateVideoIncomingDatapath(TPV2WayCmdInfo* aCmd,
        TPV2WayNode& aNode,
        CPVDatapathNode aDatapathnode,
        CPV2WayDataChannelDatapath* apDatapath)
{
    LogFunction("CreateVideoIncomingDatapath", START_FUNCTION);
    // video media type
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                    (0, "CPV324m2Way::CreateVideoIncomingDatapath channel id =%d\n",
                     aCmd->iPvtCmdData));
    apDatapath->SetChannelId(aCmd->iPvtCmdData);


    //Add tsc node to datapath
    DoAddDataSinkTscNode(aDatapathnode, apDatapath, aCmd);
    if (apDatapath->GetSourceSinkFormat().isCompressed())
    {
        //Add sink node to datapath
        if (apDatapath->GetSourceSinkFormat() != PVMF_MIME_H264_VIDEO_RAW)
        {
            DoAddDataSinkNodeForH263_M4V(aNode, aDatapathnode, apDatapath);
        }
        else
        {
            DoAddDataSinkNodeForAVC(aNode, aDatapathnode, apDatapath);
        }
    }
    else // uncompressed (eg YUV420)
    {
        //Add video dec node to datapath
        DoAddVideoDecNode(aDatapathnode, apDatapath);

        //Add sink node to datapath
        DoAddDataSinkGeneric(aNode, aDatapathnode, apDatapath);
    }

    apDatapath->SetCmd(aCmd);
    LogFunction("CreateVideoIncomingDatapath", LEAVE_FUNCTION);
}


void CPV324m2Way::CreateAudioIncomingDatapath(TPV2WayCmdInfo* aCmd,
        TPV2WayNode& aNode,
        CPVDatapathNode aDatapathnode,
        CPV2WayDataChannelDatapath* apDatapath)
{
    LogFunction("CreateAudioIncomingDatapath", START_FUNCTION);
    // video media type
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                    (0, "CPV324m2Way::CreateAudioIncomingDatapath channel id =%d\n",
                     aCmd->iPvtCmdData));
    apDatapath->SetChannelId(aCmd->iPvtCmdData);

    //Add tsc node to datapath
    DoAddDataSinkTscNode(aDatapathnode, apDatapath, aCmd);


    if (!apDatapath->GetSourceSinkFormat().isCompressed())
    {
        //Add audio dec node to datapath
        DoAddAudioDecNode(aDatapathnode, apDatapath);
    }

    //Add sink node to datapath
    DoAddDataSinkGeneric(aNode, aDatapathnode, apDatapath);

    apDatapath->SetCmd(aCmd);
    LogFunction("CreateAudioIncomingDatapath", LEAVE_FUNCTION);
}

void CPV324m2Way::DoAddDataSink(TPV2WayNode& aNode,
                                const PVMFCmdResp& aResponse)
{
    LogFunction("DoAddDataSink", START_FUNCTION);
    TPV2WayNode* sinkNode = &aNode;
    PVMFNodeCapability capability;
    PVMFNodeInterface *node = sinkNode->iNode;
    CPVDatapathNode datapathnode;
    CPV2WayNodeContextData *data = (CPV2WayNodeContextData *) aResponse.GetContext();
    TPV2WayCmdInfo *cmd = (TPV2WayCmdInfo *)data->iContextData;
    cmd->status = aResponse.GetCmdStatus();

    if (node->GetCapability(capability) != PVMFSuccess)
    {
        OSCL_DELETE(sinkNode);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::AddDataSinkL - unable to get capability\n"));
        OSCL_LEAVE(PVMFFailure);
    }

    CPV2WayDataChannelDatapath* datapath = GetDataPath(INCOMING,
                                           capability.iInputFormatCapability[0]);
    if (!datapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::DoAddDataSink - no datapath\n"));
        OSCL_LEAVE(PVMFFailure);
    }

    if (!IsFormatSupported(capability.iInputFormatCapability, *datapath))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::DoAddDataSink capability is not supported\n"));
        OSCL_LEAVE(PVMFErrNotSupported);
    }

    if (datapath->GetState() != EClosed)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::DoAddDataSource invalid state (not closed)\n"));
        OSCL_LEAVE(PVMFErrInvalidState);
    }

    if (datapath->GetSourceSinkFormat().isVideo())
    {
        CreateVideoIncomingDatapath(cmd, aNode, datapathnode, datapath);
    }

    else if (datapath->GetSourceSinkFormat().isAudio())
    {
        CreateAudioIncomingDatapath(cmd, aNode, datapathnode, datapath);
    }

    LogFunction("DoAddDataSink", LEAVE_FUNCTION);
}

PVCommandId CPV324m2Way::RemoveDataSink(PVMFNodeInterface& aDataSink,
                                        OsclAny* aContextData)
{
    LogFunction("RemoveDataSink", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::RemoveDataSink(%x, %x, %x)", 0, 0,
                     aContextData));


    if (iClock.GetState() == PVMFMediaClock::RUNNING)
    {
        // stop clock and cleanup
        iClock.Stop();
    }
    if (iMuxClock.GetState() == PVMFMediaClock::RUNNING)
    {
        // stop clock and cleanup
        iMuxClock.Stop();
    }

    // Before removing the sink node, remove reference for PvmfNodesSyncControlInterface
    for (uint32 ii = 0; ii < iSinkNodeList.size(); ii++)
    {
        if (iSinkNodeList[ii].iNodeInterface.iInterface)
        {
            iSinkNodeList[ii].iNodeInterface.iInterface->removeRef();
            iSinkNodeList[ii].iNodeInterface.iInterface = NULL;
        }
    }

    PVCommandId id = 0;
    if (ProperStateForAction(EA_REMOVE_DATA_SINK))
    {
        id = DoRemoveDataSourceSink(aDataSink, aContextData);
    }
    LogFunction("RemoveDataSink", LEAVE_FUNCTION);

    return id;
}

void CPV324m2Way::StartClock()
{
    LogFunction("StartClock", START_FUNCTION);
    /* set clock to 0 and start */
    uint32 startTime = 0;
    bool overflowFlag = false;

    if (!iClock.SetStartTime32(startTime, PVMF_MEDIA_CLOCK_MSEC, overflowFlag))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::Connect: unable to set clock time\n"));
        LogFunction("StartClock", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFFailure);
    }
    iClock.Start();
    if (iMuxClock.GetState() != PVMFMediaClock::RUNNING)
    {
        StartTscClock();
    }
    LogFunction("StartClock", LEAVE_FUNCTION);

}

void CPV324m2Way::StartTscClock()
{
    LogFunction("StartTscClock", START_FUNCTION);
    /* set clock to 0 and start */
    uint32 startTime = 0;
    bool overflowFlag = false;


    if (iMuxClock.GetState() == PVMFMediaClock::RUNNING)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_WARNING,
                        (0, "CPV324m2Way::StartTscClock: clock already started\n"));
    }
    else if (!iAudioDecDatapath && !iVideoDecDatapath)
    {

        if (!iMuxClock.SetStartTime32(startTime, PVMF_MEDIA_CLOCK_MSEC, overflowFlag))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::StartTscClock: unable to set clock time\n"));
            LogFunction("StartTscClock", LEAVE_FUNCTION, PVMFFailure);
            OSCL_LEAVE(PVMFFailure);
        }
        iMuxClock.Start();
        ((TSC_324m*)(iTscNode.iNode))->SetClock(&iMuxClock);
    }

    LogFunction("StartTscClock", LEAVE_FUNCTION);
}

PVCommandId CPV324m2Way::Connect(const PV2WayConnectOptions& aOptions,
                                 PVMFNodeInterface* aCommServer,
                                 OsclAny* aContextData)
{
    LogFunction("Connect", START_FUNCTION);

    CPVDatapathNode* node = OSCL_NEW(CPVDatapathNode, ());
    OsclError::PushL(node);

    // validate aCommServer
    if (aCommServer == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::ConnectL comm server is null\n"));
        LogFunction("Connect", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFErrArgument);
    }

    if (iConnectInfo)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::ConnectL cmd already sent out\n"));
        LogFunction("Connect", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFErrBusy);
    }

    switch (iState)
    {
        case ESetup:
            iConnectInfo = GetCmdInfoL();

            iLoopbackMode = aOptions.iLoopbackMode;
            ((TSC_324m *)(PVMFNodeInterface *)iTscNode.iNode)->SetLoopbackMode(iLoopbackMode);
            ((TSC_324m *)(PVMFNodeInterface *)iTscNode.iNode)->SetEndSessionTimeout(((PV2Way324ConnectOptions *)(&aOptions))->iDisconnectTimeoutInterval);

            // Store reference to comm server
            iCommNode = TPV2WayNode(aCommServer);
            InitiateSession(iCommNode);

            //Add tsc node to datapath
            node->iNode = iTscNode;
            node->iConfigure = this;
            node->iIgnoreNodeState = false;
            node->iConfigTime = EConfigBeforeStart;
            node->iOutputPort.iRequestPortState = EPVMFNodeInitialized;
            node->iOutputPort.iPortSetType = EUserDefined;
            node->iOutputPort.iFormatType = PVMF_MIME_H223;
            //node->iOutputPort.iPortType = EPVIOPort;
            node->iOutputPort.iPortTag = PV_MULTIPLEXED;
            iMuxDatapath->AddNode(*node);

            //Add rcomm node to datapath
            node->iNode = iCommNode;
            node->iLoggoffOnReset = true;
            node->iConfigure = NULL;
            node->iIgnoreNodeState = false;
            node->iLoggoffOnReset = false;
            node->iInputPort.iRequestPortState = EPVMFNodeInitialized;
            node->iInputPort.iPortSetType = EUserDefined;
            node->iInputPort.iFormatType = PVMF_MIME_H223;
            node->iInputPort.iPortTag = PV2WAY_IO_PORT;
            //node->iInputPort.iProperty.iPortType = EPVIOPort;
            node->iOutputPort.iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
            //node.iOutputPort.iPortType = EPVInvalidPortType;
            node->iOutputPort.iPortTag = PV2WAY_UNKNOWN_PORT;
            iMuxDatapath->AddNode(*node);

            iConnectInfo->type = PVT_COMMAND_CONNECT;
            iConnectInfo->id = iCommandId;
            iConnectInfo->contextData = aContextData;
            SetState(EConnecting);

            iMuxDatapath->Open();
            break;

        case EConnected:
            iConnectInfo = GetCmdInfoL();
            iConnectInfo->type = PVT_COMMAND_CONNECT;
            iConnectInfo->status = PVMFSuccess;
            iConnectInfo->id = iCommandId;
            iConnectInfo->contextData = aContextData;
            Dispatch(iConnectInfo);
            iConnectInfo = NULL;
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::ConnectL - invalid state(%d)", iState));
            LogFunction("Connect", LEAVE_FUNCTION, PVMFFailure);
            OSCL_LEAVE(PVMFErrInvalidState);
            break;
    }

    /*
        // start enc datapaths that are already created
        if (iAudioEncDatapath->GetState() != EClosed)
        {
            iAudioEncDatapath->CheckOpen();
        }
        if (iVideoEncDatapath->GetState() != EClosed)
        {
            iVideoEncDatapath->CheckOpen();
        }
        */
    OsclError::PopDealloc();
    LogFunction("Connect", LEAVE_FUNCTION);

    return iCommandId++;
}

PVCommandId CPV324m2Way::Disconnect(OsclAny* aContextData)
{
    LogFunction("Disconnect", START_FUNCTION);

    if (iDisconnectInfo)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::Disconnect cmd already sent out\n"));
        LogFunction("Disconnect", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFErrBusy);
    }

    switch (iState)
    {
        case EConnecting:
            //Connect in progress, notify application that it has been cancelled.
            iConnectInfo->status = PVMFErrCancelled;
            Dispatch(iConnectInfo);
            iConnectInfo = NULL;
            //Fall through to next case

        case EConnected:

            iTSC324mInterface->EndSessionCommand();

            iEndSessionTimer->SetObserver(this);
            iEndSessionTimer->Request(END_SESSION_TIMER_ID, END_SESSION_TIMER_ID,
                                      END_SESSION_TIMER_VALUE, this);

            iDisconnectInfo = GetCmdInfoL();

            iDisconnectInfo->type = PVT_COMMAND_DISCONNECT;
            iDisconnectInfo->contextData = aContextData;
            iDisconnectInfo->id = iCommandId;

            //We wait to InitiateDisconnect() till iEndSessionTimer timer expires
            break;

        case EDisconnecting:
            //If at this point, then remote disconnect is in progress, just treat as user disconnect.
            iDisconnectInfo = GetCmdInfoL();

            iDisconnectInfo->type = PVT_COMMAND_DISCONNECT;
            iDisconnectInfo->contextData = aContextData;
            iDisconnectInfo->id = iCommandId;
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::Disconnect - invalid state(%d)", iState));
            LogFunction("Disconnect", LEAVE_FUNCTION, PVMFFailure);
            OSCL_LEAVE(PVMFErrInvalidState);
            break;
    }
    LogFunction("Disconnect", LEAVE_FUNCTION);

    return iCommandId++;
}

void CPV324m2Way::InitiateDisconnect()
{
    LogFunction("InitiateDisconnect", START_FUNCTION);
    SetState(EDisconnecting);
    CheckState();
    LogFunction("InitiateDisconnect", LEAVE_FUNCTION);
}

void CPV324m2Way::InitiateReset()
{
    LogFunction("InitiateReset", START_FUNCTION);
    SetState(EResetting);

    if (isIFrameReqTimerActive)
    {
        iIFrameReqTimer.Cancel(IFRAME_REQ_TIMERID);
        isIFrameReqTimerActive = false;
    }

    if ((iAudioDecDatapath != NULL) && (iAudioDecDatapath->GetState() != EClosed))
    {
        iAudioDecDatapath->SetCmd(NULL);
    }

    if ((iAudioEncDatapath != NULL) && (iAudioEncDatapath->GetState() != EClosed))
    {
        iAudioEncDatapath->SetCmd(NULL);
    }

    if ((iVideoDecDatapath != NULL) && (iVideoDecDatapath->GetState() != EClosed))
    {
        iVideoDecDatapath->SetCmd(NULL);
    }

    if ((iVideoEncDatapath != NULL) && (iVideoEncDatapath->GetState() != EClosed))
    {
        iVideoEncDatapath->SetCmd(NULL);
    }

    CheckState();
    LogFunction("InitiateReset", LEAVE_FUNCTION);
}

void CPV324m2Way::CheckState()
{
    LogFunction("CheckState", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::CheckState state %d\n", iState));
    switch (iState)
    {
        case EInitializing:
            CheckInit();
            break;

        case EConnecting:
            CheckConnect();
            break;

        case EDisconnecting:
            CheckDisconnect();
            break;

        case EResetting:
            CheckReset();
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                            (0, "CPV324m2Way::CheckState warning: static state!\n"));
            break;
    }
    LogFunction("CheckState", LEAVE_FUNCTION);
}

void CPV324m2Way::CheckInit()
{
    LogFunction("CheckInit", START_FUNCTION);
//  PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0,"CPV324m2Way::CheckInit state %d, video enc node state %d, interface state %d\n", iState, ((PVMFNodeInterface *)iVideoEncNode)->GetState(), iVideoEncNodeInterface.iState));
    int32 error;

    if (((PVMFNodeInterface *)iTscNode)->GetState() == EPVMFNodeIdle)
    {
        OSCL_TRY(error, SendNodeCmdL(PV2WAY_NODE_CMD_INIT, &iTscNode, this));
        OSCL_FIRST_CATCH_ANY(error,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::CheckInit unable to init tsc node!\n"));
                             SetState(EResetting);
                             CheckState();
                             LogFunction("CheckInit", LEAVE_FUNCTION, PVMFFailure);
                             return;);
    }
    if (((PVMFNodeInterface *)iTscNode)->GetState() == EPVMFNodeInitialized)
    {

        SetState(ESetup);

        iInitInfo->status = PVMFSuccess;
        Dispatch(iInitInfo);
        iInitInfo = NULL;
    }
    LogFunction("CheckInit", LEAVE_FUNCTION);
}

void CPV324m2Way::CheckConnect()
{
    LogFunction("CheckConnect", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::CheckConnect state %d, comm state %d, tsc state %d\n",
                     iState, ((PVMFNodeInterface *)iCommNode)->GetState(),
                     ((PVMFNodeInterface *)iTscNode)->GetState()));

    if ((iMuxDatapath->GetState() == EOpened) && iIsStackConnected)
    {
        /* Increase video encoder bitrate if required */
        //  PVVideoEncExtensionInterface *ptr = (PVVideoEncExtensionInterface *) iVideoEncNodeInterface.iInterface;
        //  ptr->SetOutputBitRate(0, VIDEO_ENCODER_BITRATE);
        SetState(EConnected);

        iConnectInfo->status = PVMFSuccess;
        Dispatch(iConnectInfo);
        iConnectInfo = NULL;
    }
    LogFunction("CheckConnect", LEAVE_FUNCTION);
}


void CPV324m2Way::CheckDisconnect()
{
    LogFunction("CheckDisconnect", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::CheckDisconnect state %d, mux datapath state %d, stack connected %d\n",
                     iState, iMuxDatapath->GetState(), iIsStackConnected));
    if ((iMuxDatapath->GetState() == EClosed) &&
            !iIsStackConnected)
    {
        SetState(ESetup);

        //Connect failed
        if (iConnectInfo)
        {
            iConnectInfo->status = PVMFFailure;
            Dispatch(iConnectInfo);
            iConnectInfo = NULL;
        }
        //Else command cancelled
        else if (iCancelInfo)
        {
            iCancelInfo->status = PVMFSuccess;
            Dispatch(iCancelInfo);
            iCancelInfo = NULL;
        }
        //Else local disconnect
        else if (iDisconnectInfo)
        {
            iDisconnectInfo->status = PVMFSuccess;
            Dispatch(iDisconnectInfo);
            iDisconnectInfo = NULL;
        }
        //Else remote disconnect
        else
        {
            TPV2WayEventInfo* event = NULL;
            if (!GetEventInfo(event))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::CheckDisconnect unable to notify app!\n"));
                LogFunction("CheckDisconnect", LEAVE_FUNCTION, PVMFFailure);
                return;
            }
            event->type = PVT_INDICATION_DISCONNECT;
            Dispatch(event);
        }
    }
    else
    {
        iMuxDatapath->Close();
    }
    LogFunction("CheckDisconnect", LEAVE_FUNCTION);
}

void CPV324m2Way::CheckReset()
{
    LogFunction("CheckReset", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::CheckReset state %d \n", iState));
    int32 error = 0;


    if ((iAudioEncDatapath != NULL) && (iAudioEncDatapath->GetState() == EClosed) &&
            (iAudioDecDatapath != NULL) && (iAudioDecDatapath->GetState() == EClosed) &&
            (iVideoEncDatapath != NULL) && (iVideoEncDatapath->GetState() == EClosed) &&
            (iVideoDecDatapath != NULL) && (iVideoDecDatapath->GetState() == EClosed))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::CheckReset state %d, AD state %d, VD state %d, AE state %d, VE state %d\n",
                         iState,
                         iAudioDecDatapath->GetState(),
                         iVideoDecDatapath->GetState(),
                         iAudioEncDatapath->GetState(),
                         iVideoEncDatapath->GetState()));

        TPVMFNodeInterfaceState vidEncState;

        if (iVideoEncNode != NULL)
        {
            vidEncState = ((PVMFNodeInterface *)iVideoEncNode)->GetState() ;
            if ((vidEncState == EPVMFNodeInitialized) || (vidEncState == EPVMFNodeError))
            {
                OSCL_TRY(error, SendNodeCmdL(PV2WAY_NODE_CMD_RESET, &iVideoEncNode, this));
                OSCL_FIRST_CATCH_ANY(error,
                                     PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                                     (0, "CPV324m2Way::CheckReset unable to reset video encoder node!\n"));
                                     LogFunction("CheckReset", LEAVE_FUNCTION, PVMFFailure);
                                     return;);
            }
        }

        if (iAudioEncNode != NULL)
        {
            if ((iAudioEncNode.iNode->GetState() == EPVMFNodeInitialized) ||
                    (iAudioEncNode.iNode->GetState() == EPVMFNodeError))
            {
                OSCL_TRY(error, SendNodeCmdL(PV2WAY_NODE_CMD_RESET, &iAudioEncNode, this));
                OSCL_FIRST_CATCH_ANY(error,
                                     PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                                     (0, "CPV324m2Way::CheckReset unable to reset audio encoder node!\n"));
                                     LogFunction("CheckReset", LEAVE_FUNCTION, PVMFFailure);
                                     return;);
            }
        }
    }

    TPVMFNodeInterfaceState tscState = ((PVMFNodeInterface *)iTscNode)->GetState() ;

    if ((tscState == EPVMFNodeInitialized) ||
            (tscState == EPVMFNodeError))
    {
        OSCL_TRY(error, SendNodeCmdL(PV2WAY_NODE_CMD_RESET, &iTscNode, this));
        OSCL_FIRST_CATCH_ANY(error,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::CheckReset unable to reset tsc node!\n"));
                             LogFunction("CheckReset", LEAVE_FUNCTION, PVMFFailure);
                             return;);
    }
    bool aFlag = false;
    if ((tscState == EPVMFNodeIdle) &&
            (iVideoEncNodeInterface.iState != PV2WayNodeInterface::QueryInterface))
    {
        if (iVideoEncNode.iNode != NULL)
            aFlag = IsNodeReset(*(iVideoEncNode.iNode));
        else
            aFlag = true;

    }

    if (aFlag == true)
    {
        iIncomingChannelParams.clear();
        iOutgoingChannelParams.clear();

        SetState(EIdle);

        //Init failed
        if (iInitInfo)
        {
            iInitInfo->status = PVMFFailure;
            Dispatch(iInitInfo);
            iInitInfo = NULL;
        }
        //Else command cancelled
        else if (iCancelInfo)
        {
            iCancelInfo->status = PVMFSuccess;
            Dispatch(iCancelInfo);
            iCancelInfo = NULL;
        }
        //Else local reset
        else
        {
            iResetInfo->status = PVMFSuccess;
            Dispatch(iResetInfo);
            iResetInfo = NULL;
        }
    }

    LogFunction("CheckReset", LEAVE_FUNCTION);
}


void CPV324m2Way::RemoveAudioDecPath()
{
    LogFunction("RemoveAudioDecPath", START_FUNCTION);
    iReadDataLock.Lock();
    if (iAudioDecDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::RemoveAudioDecPath audio dec path state %d\n",
                         iAudioDecDatapath->GetState()));
        if (iAudioDecDatapath->GetState() == EClosed)
        {
            iAudioDecDatapath->ResetDatapath();
            iAudioSinkNode.Clear();
        }
    }
    iReadDataLock.Unlock();
    LogFunction("RemoveAudioDecPath", LEAVE_FUNCTION);
}

void CPV324m2Way::RemoveAudioEncPath()
{
    LogFunction("RemoveAudioEncPath", START_FUNCTION);
    iReadDataLock.Lock();
    if (iAudioEncDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::RemoveAudioEncPath audio enc path state %d\n",
                         iAudioEncDatapath->GetState()));
        if (iAudioEncDatapath->GetState() == EClosed)
        {
            iAudioEncDatapath->SetSourceInputPort(NULL);
            iAudioEncDatapath->ResetDatapath();
            iAudioSrcNode.Clear();
        }
    }
    iReadDataLock.Unlock();
    LogFunction("RemoveAudioEncPath", LEAVE_FUNCTION);
}

void CPV324m2Way::RemoveVideoDecPath()
{
    LogFunction("RemoveVideoDecPath", START_FUNCTION);
    iReadDataLock.Lock();
    if (iVideoDecDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::RemoveVideoDecPath video dec path state %d\n",
                         iVideoDecDatapath->GetState()));
        if (iVideoDecDatapath->GetState() == EClosed)
        {
            iVideoDecDatapath->ResetDatapath();
        }
    }

    iReadDataLock.Unlock();
    LogFunction("RemoveVideoDecPath", LEAVE_FUNCTION);
}

void CPV324m2Way::RemoveVideoEncPath()
{
    LogFunction("RemoveVideoEncPath", START_FUNCTION);
    iReadDataLock.Lock();

    if (iVideoEncDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::RemoveVideoEncPath video enc path state %d\n",
                         iVideoEncDatapath->GetState()));
        if (iVideoEncDatapath->GetState() == EClosed)
        {
            //Video encoder will be deleted at reset time.

            iVideoEncDatapath->ResetDatapath();
        }
    }
    iReadDataLock.Unlock();
    LogFunction("RemoveVideoEncPath", LEAVE_FUNCTION);
}

void CPV324m2Way::HandleCommNodeCmd(PV2WayNodeCmdType aType,
                                    const PVMFCmdResp& aResponse)
{
    LogFunction("HandleCommNodeCmd", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::HandleCommNodeCmd type %d\n", aType));

    switch (aType)
    {
        case PV2WAY_NODE_CMD_INIT:
            if (aResponse.GetCmdStatus() != PVMFSuccess)
            {
                SetState(EResetting);
            }

            CheckState();
            break;

        case PV2WAY_NODE_CMD_RESET:
            CheckState();
            break;

        default:
            break;
    }
    LogFunction("HandleCommNodeCmd", LEAVE_FUNCTION);
}

void CPV324m2Way::HandleTscNodeCmd(PV2WayNodeCmdType aType,
                                   const PVMFCmdResp& aResponse)
{
    LogFunction("HandleTscNodeCmd", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::HandleTscNodeCmd type %d\n", aType));

    switch (aType)
    {
        case PV2WAY_NODE_CMD_INIT:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                ((TSC_324m *)((PVMFNodeInterface *)iTscNode))->SetIncomingChannelConfig(iIncomingChannelParams);
                ((TSC_324m *)((PVMFNodeInterface *)iTscNode))->SetOutgoingChannelConfig(iOutgoingChannelParams);
            }
            else
            {
                SetState(EResetting);
            }

            CheckState();
            break;

        case PV2WAY_NODE_CMD_RESET:
            CheckState();
            break;

        default:
            break;
    }
    LogFunction("HandleTscNodeCmd", LEAVE_FUNCTION);
}


void CPV324m2Way::HandleVideoDecNodeCmd(PV2WayNodeCmdType aType,
                                        const PVMFCmdResp& aResponse)
{
    OSCL_UNUSED_ARG(aType);
    OSCL_UNUSED_ARG(aResponse);
    LogFunction("HandleVideoDecNodeCmd", START_FUNCTION);

    if (iVideoDecDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::HandleVideoDecNodeCmd type %d, video dec path state %d\n",
                         aType, iVideoDecDatapath->GetState()));
    }

    LogFunction("HandleVideoDecNodeCmd", LEAVE_FUNCTION);
}

void CPV324m2Way::HandleVideoEncNodeCmd(PV2WayNodeCmdType aType,
                                        const PVMFCmdResp& aResponse)
{
    LogFunction("HandleVideoEncNodeCmd", START_FUNCTION);
    if (iVideoEncDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::HandleVideoEncNodeCmd type %d, video enc path state %d\n",
                         aType, iVideoEncDatapath->GetState()));
    }

    switch (aType)
    {
        case PV2WAY_NODE_CMD_QUERY_INTERFACE:

            if (aResponse.GetCmdId() == iVideoEncQueryIntCmdId)
            {
                iVideoEncQueryIntCmdId = -1;
                if (aResponse.GetCmdStatus() == PVMFSuccess)
                {
                    iVideoEncNodeInterface.iState = PV2WayNodeInterface::HasInterface;
                }
                else
                {
                    iVideoEncNodeInterface.iState = PV2WayNodeInterface::NoInterface;
                    SetState(EResetting);
                }

            }
            else if (aResponse.GetCmdId() == iOmxEncQueryIntCmdId)
            {
                ipEncNodeCapabilityAndConfig = (PvmiCapabilityAndConfig*)ipEncNodeCapConfigInterface;
                ipEncNodeCapConfigInterface = NULL;
            }
            CheckState();
            break;

        case PV2WAY_NODE_CMD_INIT:
            CheckState();
            break;

        case PV2WAY_NODE_CMD_RESET:
            CheckState();
            break;

        default:
            break;
    }
    LogFunction("HandleVideoEncNodeCmd", LEAVE_FUNCTION);
}

void CPV324m2Way::HandleSinkNodeCmd(PV2WayNodeCmdType aType,
                                    const PVMFCmdResp& aResponse,
                                    TPV2WayNode* aNode)
{
    LogFunction("HandleSinkNodeCmd", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::HandleSinkNodeCmd type %d\n", aType));
    switch (aType)
    {
        case PV2WAY_NODE_CMD_INIT:
            if (aResponse.GetCmdStatus() != PVMFSuccess)
            {
                SetState(EResetting);
                CheckState();
            }
            else
            {
                DoAddDataSink(*aNode, aResponse);
            }
            break;
        case PV2WAY_NODE_CMD_QUERY_INTERFACE:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                for (uint32 ii = 0; ii < iSinkNodeList.size(); ii++)
                {
                    if ((aNode == iSinkNodeList[ii].iSinkNode) &&
                            (aResponse.GetCmdId() == iSinkNodeList[ii].iNodeInterface.iId))
                    {
                        iSinkNodeList[ii].iNodeInterface.iInterface =
                            iClockSyncInterface.iInterface;
                        iClockSyncInterface.Reset();
                        if (iSinkNodeList[ii].iNodeInterface.iInterface != NULL)
                        {
                            iSinkNodeList[ii].iNodeInterface.iState =
                                PV2WayNodeInterface::HasInterface;
                            PvmfNodesSyncControlInterface* ptr = NULL;
                            ptr = OSCL_STATIC_CAST(PvmfNodesSyncControlInterface*,
                                                   iSinkNodeList[ii].iNodeInterface.iInterface);
                            ptr->SetClock(&iClock);
                        }
                        break;
                    }
                }
            }
            else
            {
                SetState(EResetting);
                CheckState();
            }
            break;
        case PV2WAY_NODE_CMD_SKIP_MEDIA_DATA:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                            (0, "CPV324m2Way::HandleSinkNodeCmd type %d, SkipComplete for Node %x ",
                             aType, aNode->iNode));
            if (iAudioDecDatapath) iAudioDecDatapath->SkipComplete(aNode);
            if (iVideoDecDatapath) iVideoDecDatapath->SkipComplete(aNode);
            RunIfNotReady();
            break;
        default:
            break;
    }
    LogFunction("HandleSinkNodeCmd", LEAVE_FUNCTION);
}

void CPV324m2Way::SupportedSinkNodeInterfaces(TPV2WayNode* aNode)
{
    LogFunction("SupportedSinkNodeInterfaces", START_FUNCTION);

    int32 error;

    //Currently this only checks if the sink node support PvmfSyncNodeControlInterface

    TPV2WayNodeQueryInterfaceParams queryParam;
    queryParam.iUuid = (PVUuid *) & iSyncControlPVUuid;
    SinkNodeIFList sinkNode;
    sinkNode.iSinkNode = aNode;
    queryParam.iInterfacePtr = &iClockSyncInterface.iInterface;
    OSCL_TRY(error, sinkNode.iNodeInterface.iId =
                 SendNodeCmdL(PV2WAY_NODE_CMD_QUERY_INTERFACE, aNode, this, &queryParam));

    iSinkNodeList.push_back(sinkNode);

    OSCL_FIRST_CATCH_ANY(error,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                         (0, "CPV324m2Way::SupportedSinkNodeInterfaces unable to query for MediaOutputNode extension interface!\n"));
                         SetState(EResetting);
                         CheckState();
                         LogFunction("SupportedSinkNodeInterfaces", LEAVE_FUNCTION, PVMFFailure);
                         return;);
    LogFunction("SupportedSinkNodeInterfaces", LEAVE_FUNCTION);
}


void CPV324m2Way::HandleAudioEncNodeCmd(PV2WayNodeCmdType aType,
                                        const PVMFCmdResp& aResponse)
{
    LogFunction("HandleAudioEncNodeCmd", START_FUNCTION);
    if (iAudioEncDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::HandleAudioEncNodeCmd type %d, audio enc path state %d\n",
                         aType, iAudioEncDatapath->GetState()));
    }
    OSCL_UNUSED_ARG(aResponse);
    OSCL_UNUSED_ARG(aType);

    switch (aType)
    {
        case PV2WAY_NODE_CMD_QUERY_INTERFACE:

            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iAudioEncNodeInterface.iState = PV2WayNodeInterface::HasInterface;

            }
            else
            {
                iAudioEncNodeInterface.iState = PV2WayNodeInterface::NoInterface;
                SetState(EResetting);
            }

            CheckState();
            break;

        case PV2WAY_NODE_CMD_INIT:
            CheckState();
            break;

        case PV2WAY_NODE_CMD_RESET:
            CheckState();
            break;

        default:
            break;
    }
    LogFunction("HandleAudioEncNodeCmd", LEAVE_FUNCTION);
}

void CPV324m2Way::GenerateIFrame(PVMFPortInterface *aPort)
{
    LogFunction("GenerateIFrame", START_FUNCTION);
    if (iVideoEncDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::GenerateIFrame, vid enc path state %d\n",
                         iVideoEncDatapath->GetState()));

        if (iVideoEncNodeInterface.iInterface &&
                (iVideoEncDatapath->IsPortInDatapath(aPort)) &&
                (iVideoEncDatapath->GetState() == EOpened))
        {
            if (!((PVVideoEncExtensionInterface *) iVideoEncNodeInterface.iInterface)->RequestIFrame())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::GenerateIFrame - unable to generate iframe\n"));
            }
        }
        if (!iVideoEncNodeInterface.iInterface)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                            (0, "CPV324m2Way::GenerateIFrame - iVideoEncNodeInterface.iInterface is NULL\n"));
        }
    }
    LogFunction("GenerateIFrame", LEAVE_FUNCTION);
}

void CPV324m2Way::RequestRemoteIFrame(PVMFPortInterface *aPort)
{
    LogFunction("RequestRemoteIFrame", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                    (0, "CPV324m2Way::RequestRemoteIFrame, timer active %d\n", isIFrameReqTimerActive));
    TSC_324m *nodeIface = (TSC_324m *)((PVMFNodeInterface *)iTscNode);
    if (nodeIface &&
            !isIFrameReqTimerActive &&
            (nodeIface->RequestFrameUpdate(aPort) == EPVT_Success))
    {
        //Still need to actually send an iframe request!!!!
        iIFrameReqTimer.Request(IFRAME_REQ_TIMERID, (int32)this,
                                iMinIFrameRequestInterval, this);
        isIFrameReqTimerActive = true;
    }
    LogFunction("RequestRemoteIFrame", LEAVE_FUNCTION);
}

PVCommandId CPV324m2Way::GetState(PV2WayState& aState,
                                  OsclAny* aContextData)
{
    LogFunction("GetState", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::GetPV2WayState %d\n", iState));

    TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_GET_PV2WAY_STATE, aContextData);
    cmd->status = PVMFSuccess;

    aState = iState;

    Dispatch(cmd);

    LogFunction("GetState", LEAVE_FUNCTION);
    return iCommandId++;
}


PVCommandId CPV324m2Way::SetLatencyQualityTradeoff(PVMFNodeInterface& aTrack,
        int32 aTradeoff,
        OsclAny* aContextData)
{
    LogFunction("SetLatencyQualityTradeoff", START_FUNCTION);
    OSCL_UNUSED_ARG(aTrack);
    OSCL_UNUSED_ARG(aTradeoff);
    OSCL_UNUSED_ARG(aContextData);
    LogFunction("SetLatencyQualityTradeoff", LEAVE_FUNCTION);
    return iCommandId++;
}

CPV2WayDataChannelDatapath* CPV324m2Way::GetDataPath(PV2WayDirection aDirection,
        PVTrackId aTrackId)
{
    LogFunction("GetDataPath", START_FUNCTION);
    CPV2WayDataChannelDatapath* datapath = NULL;
    if (aDirection == OUTGOING)
    {
        if (iAudioEncDatapath && (aTrackId == iAudioEncDatapath->GetChannelId()))
        {
            datapath = iAudioEncDatapath;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::GetDataPath Audio Enc path "));
        }
        else if (iVideoEncDatapath && (aTrackId == iVideoEncDatapath->GetChannelId()))
        {
            datapath = iVideoEncDatapath;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::GetDataPath Video Enc path "));
        }
    }
    else if (aDirection == INCOMING)
    {

        if (iAudioDecDatapath && (aTrackId == iAudioDecDatapath->GetChannelId()))
        {
            datapath = iAudioDecDatapath;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::GetDataPath Audio Dec path "));
        }
        else if (iVideoDecDatapath && (aTrackId == iVideoDecDatapath->GetChannelId()))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::GetDataPath Video Dec path "));
            datapath = iVideoDecDatapath;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::GetDataPath - invalid datapath, aDirection(%d) aTrackId(%d)",
                         aDirection, aTrackId));
        LogFunction("GetDataPath", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFErrArgument);
    }
    LogFunction("GetDataPath", LEAVE_FUNCTION);
    return datapath;
}

CPV2WayDataChannelDatapath* CPV324m2Way::GetDataPath(PV2WayDirection aDirection,
        PVMFFormatType aFormatType)
{
    LogFunction("GetDataPath", START_FUNCTION);
    if (aFormatType.isAudio())
    {
        return GetDataPath(aDirection, PV_AUDIO);
    }
    else if (aFormatType.isVideo())
    {
        return GetDataPath(aDirection, PV_VIDEO);
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::GetDataPath unknown media_type %s\n",
                         aFormatType.getMIMEStrPtr()));
    }
    LogFunction("GetDataPath", LEAVE_FUNCTION);
    return NULL;
}

CPV2WayDataChannelDatapath* CPV324m2Way::GetDataPath(PV2WayDirection aDirection,
        PV2WayMediaType aMediaType)
{
    LogFunction("GetDataPath", START_FUNCTION);
    CPV2WayDataChannelDatapath* datapath = NULL;
    if (aDirection == OUTGOING)
    {
        if (iAudioEncDatapath && PV_AUDIO == aMediaType)
        {
            datapath = iAudioEncDatapath;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::GetDataPath Audio Enc path "));
        }
        else if (iVideoEncDatapath && PV_VIDEO == aMediaType)
        {
            datapath = iVideoEncDatapath;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::GetDataPath Video Enc path "));
        }
    }
    else if (aDirection == INCOMING)
    {

        if (iAudioDecDatapath &&  PV_AUDIO == aMediaType)
        {
            datapath = iAudioDecDatapath;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::GetDataPath Audio Dec path "));
        }
        else if (iVideoDecDatapath && PV_VIDEO == aMediaType)
        {
            datapath = iVideoDecDatapath;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::GetDataPath Video Dec path "));
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::GetDataPath unknown datapath: media_type %d, direction %d\n",
                         aMediaType, aDirection));
    }
    LogFunction("GetDataPath", LEAVE_FUNCTION);
    return datapath;
}

CPV2WayDataChannelDatapath* CPV324m2Way::GetDataPath(PVMFNodeInterface& aEndPt)
{
    CPV2WayDataChannelDatapath* datapath = NULL;
    if ((iVideoEncDatapath != NULL) && iVideoEncDatapath->IsNodeInDatapath(&aEndPt))
    {
        datapath = iVideoEncDatapath;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::GetDataPath Video Enc path "));
    }
    else if ((iAudioEncDatapath != NULL) && iAudioEncDatapath->IsNodeInDatapath(&aEndPt))
    {
        datapath = iAudioEncDatapath;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::GetDataPath Audio Enc path "));
    }
    else if ((iVideoDecDatapath != NULL) && iVideoDecDatapath->IsNodeInDatapath(&aEndPt))
    {
        datapath = iVideoDecDatapath;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::GetDataPath Video Dec path "));
    }
    else if ((iAudioDecDatapath != NULL) && iAudioDecDatapath->IsNodeInDatapath(&aEndPt))
    {
        datapath = iAudioDecDatapath;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::GetDataPath Audio Dec path "));
    }
    return datapath;
}

bool CPV324m2Way::IsEncDatapath(CPV2WayDataChannelDatapath *apDatapath)
{
    if (apDatapath == iVideoEncDatapath || apDatapath == iAudioEncDatapath)
        return true;
    return false;
}

bool CPV324m2Way::IsDecDatapath(CPV2WayDataChannelDatapath *apDatapath)
{
    if (apDatapath == iVideoDecDatapath || apDatapath == iAudioDecDatapath)
        return true;
    return false;
}

bool CPV324m2Way::IsVideoDatapath(CPV2WayDataChannelDatapath *apDatapath)
{
    if (apDatapath == iVideoEncDatapath || apDatapath == iVideoDecDatapath)
        return true;
    return false;
}

bool CPV324m2Way::IsAudioDatapath(CPV2WayDataChannelDatapath *apDatapath)
{
    if (apDatapath == iAudioEncDatapath || apDatapath == iAudioDecDatapath)
        return true;
    return false;
}

TPV2WayCmdInfo * CPV324m2Way::GetAndSetCmdInfo(TPV2WayCommandType aType,
        PV2WayDirection aDirection,
        PVTrackId aTrackId,
        OsclAny* aContextData)
{
    LogFunction("GetAndSetCmdInfo", START_FUNCTION);
    TPV2WayCmdInfo *cmd = GetCmdInfoL();
    cmd->id = iCommandId;
    cmd->type = aType;
    cmd->iPvtCmdData = aTrackId;
    cmd->iPvtCmdDataExt = aDirection;
    cmd->contextData = aContextData;
    LogFunction("GetAndSetCmdInfo", LEAVE_FUNCTION);
    return cmd;
}

TPV2WayCmdInfo * CPV324m2Way::GetAndSetCmdInfo(TPV2WayCommandType aType,
        PVTrackId aTrackId,
        OsclAny* apContextData)
{
    LogFunction("GetAndSetCmdInfo", START_FUNCTION);
    TPV2WayCmdInfo *cmd = GetCmdInfoL();
    cmd->id = iCommandId;
    cmd->type = aType;
    cmd->iPvtCmdData = aTrackId;
    cmd->contextData = apContextData;
    LogFunction("GetAndSetCmdInfo", LEAVE_FUNCTION);
    return cmd;
}

TPV2WayCmdInfo * CPV324m2Way::GetAndSetCmdInfo(TPV2WayCommandType aType,
        OsclAny* apContextData)
{
    LogFunction("GetAndSetCmdInfo", START_FUNCTION);
    TPV2WayCmdInfo *cmd = GetCmdInfoL();
    cmd->id = iCommandId;
    cmd->type = aType;
    cmd->contextData = apContextData;
    LogFunction("GetAndSetCmdInfo", LEAVE_FUNCTION);
    return cmd;
}

void CPV324m2Way::SetInitInfo(OsclAny* aContextData)
{
    LogFunction("SetInitInfo", START_FUNCTION);
    iInitInfo = GetCmdInfoL();
    iInitInfo->type = PVT_COMMAND_INIT;
    iInitInfo->contextData = aContextData;
    iInitInfo->id = iCommandId;
    LogFunction("SetInitInfo", LEAVE_FUNCTION);
}

void CPV324m2Way::SetCancelInfo(OsclAny* aContextData)
{
    LogFunction("SetCancelInfo", START_FUNCTION);
    iCancelInfo = GetCmdInfoL();
    iCancelInfo->type = PVT_COMMAND_CANCEL_ALL_COMMANDS;
    iCancelInfo->id = iCommandId;
    iCancelInfo->contextData = aContextData;
    LogFunction("SetCancelInfo", LEAVE_FUNCTION);
}

PVCommandId CPV324m2Way::Pause(PV2WayDirection aDirection,
                               PVTrackId aTrackId,
                               OsclAny* aContextData)
{
    LogFunction("Pause", START_FUNCTION);

    CPV2WayDataChannelDatapath *datapath = NULL;
    TPV2WayCmdInfo *cmd;

    if (ProperStateForAction(EA_PAUSE))
    {
        datapath = GetDataPath(aDirection, aTrackId);

        switch (datapath->GetState())
        {
            case EOpened:
                cmd = GetAndSetCmdInfo(PVT_COMMAND_PAUSE, aDirection, aTrackId, aContextData);
                datapath->SetCmd(cmd);
                break;

            case EPaused:
                cmd = GetAndSetCmdInfo(PVT_COMMAND_PAUSE, aDirection, aTrackId, aContextData);
                cmd->status = PVMFSuccess;
                Dispatch(cmd);
                break;

            case EPausing:
                return datapath->GetCmdInfo()->id;

            default:
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::PauseL - invalid path state\n"));
                OSCL_LEAVE(PVMFErrInvalidState);
                break;
        }
    }
    LogFunction("Pause", LEAVE_FUNCTION);

    return iCommandId++;
}


PVCommandId CPV324m2Way::Resume(PV2WayDirection aDirection,
                                PVTrackId aTrackId,
                                OsclAny* aContextData)
{
    LogFunction("Resume", START_FUNCTION);

    CPV2WayDataChannelDatapath *datapath = NULL;
    TPV2WayCmdInfo *cmd;

    if (ProperStateForAction(EA_RESUME))
    {

        datapath = GetDataPath(aDirection, aTrackId);

        switch (datapath->GetState())
        {
            case EPaused:
                cmd = GetAndSetCmdInfo(PVT_COMMAND_RESUME, aDirection, aTrackId, aContextData);
                datapath->SetCmd(cmd);
                break;

            case EOpened:
                cmd = GetAndSetCmdInfo(PVT_COMMAND_RESUME, aDirection, aTrackId, aContextData);
                cmd->status = PVMFSuccess;
                Dispatch(cmd);
                break;

            case EUnpausing:
                return datapath->GetCmdInfo()->id;

            default:
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::ResumeL - invalid path state\n"));
                OSCL_LEAVE(PVMFErrInvalidState);
                break;
        }

    }
    LogFunction("Resume", LEAVE_FUNCTION);

    return iCommandId++;
}

PVCommandId CPV324m2Way::SetLogAppender(const char * aTag,
                                        OsclSharedPtr<PVLoggerAppender>& aAppender,
                                        OsclAny* aContextData)
{
    LogFunction("SetLogAppender", START_FUNCTION);


    PVLogger *logger = PVLogger::GetLoggerObject(aTag);
    logger->AddAppender(aAppender);

    // print sdk info
    PVSDKInfo sdkinfo;
    FillSDKInfo(sdkinfo);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "PV RELEASE LABEL = %s", sdkinfo.iLabel.get_cstr()));

    TPV2WayCmdInfo *cmd = GetCmdInfoL();
    cmd->type = PVT_COMMAND_SET_LOG_APPENDER;
    cmd->id = iCommandId;
    cmd->contextData = aContextData;
    cmd->status = PVMFSuccess;

    Dispatch(cmd);
    LogFunction("SetLogAppender", LEAVE_FUNCTION);
    return iCommandId++;
}

PVCommandId CPV324m2Way::RemoveLogAppender(const char * aTag,
        OsclSharedPtr<PVLoggerAppender>& aAppender,
        OsclAny* aContextData)
{
    LogFunction("RemoveLogAppender", START_FUNCTION);

    TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_REMOVE_LOG_APPENDER, aContextData);
    cmd->status = PVMFSuccess;

    PVLogger *logger = PVLogger::GetLoggerObject(aTag);
    logger->RemoveAppender(aAppender);

    Dispatch(cmd);
    LogFunction("RemoveLogAppender", LEAVE_FUNCTION);
    return iCommandId++;
}

PVCommandId CPV324m2Way::SetLogLevel(const char * aTag,
                                     int32 aLevel,
                                     bool aSetSubtree,
                                     OsclAny* aContextData)
{
    LogFunction("SetLogLevel", START_FUNCTION);
    OSCL_UNUSED_ARG(aSetSubtree);

    TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_SET_LOG_LEVEL, aContextData);
    cmd->status = PVMFSuccess;

    PVLogger *logger = PVLogger::GetLoggerObject(aTag);
    logger->SetLogLevel(aLevel);

    Dispatch(cmd);
    LogFunction("SetLogLevel", LEAVE_FUNCTION);
    return iCommandId++;
}

PVCommandId CPV324m2Way::GetLogLevel(const char * aTag,
                                     int32& aLogInfo,
                                     OsclAny* aContextData)
{
    LogFunction("GetLogLevel", START_FUNCTION);

    TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_GET_LOG_LEVEL, aContextData);
    cmd->status = PVMFSuccess;

    PVLogger *logger = PVLogger::GetLoggerObject(aTag);
    aLogInfo = logger->GetLogLevel();

    Dispatch(cmd);
    LogFunction("GetLogLevel", LEAVE_FUNCTION);
    return iCommandId++;
}


PVCommandId CPV324m2Way::QueryInterface(const PVUuid& aUuid,
                                        PVInterface*& aInterfacePtr,
                                        OsclAny* aContextData)
{
    LogFunction("QueryInterface", START_FUNCTION);

    TPV2WayNodeQueryInterfaceParams queryParam;
    queryParam.iUuid = (PVUuid*) & aUuid;
    queryParam.iInterfacePtr = &aInterfacePtr;

    TPV2WayCmdInfo *cmd = GetAndSetCmdInfo(PVT_COMMAND_QUERY_INTERFACE, aContextData);
    cmd->status = PVMFPending;
    aInterfacePtr = NULL;

    if (aUuid == PVH324MConfigUuid && ((PVMFNodeInterface *)iTscNode))
    {
        int32 error = 0;
        OSCL_TRY(error, SendNodeCmdL(PV2WAY_NODE_CMD_QUERY_INTERFACE,
                                     &iTscNode, this, &queryParam, cmd));
        OSCL_FIRST_CATCH_ANY(error,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::QueryInterface failed!\n"));
                             cmd->status = PVMFFailure;
                             Dispatch(cmd););
        cmd->status = PVMFSuccess;
    }
    else if (aUuid == PVMp4H263EncExtensionUUID &&
             ((PVMFNodeInterface *)iVideoEncNode))
    {
        int32 error = 0;
        OSCL_TRY(error, SendNodeCmdL(PV2WAY_NODE_CMD_QUERY_INTERFACE, &iVideoEncNode,
                                     this, &queryParam, cmd));
        OSCL_FIRST_CATCH_ANY(error,
                             cmd->status = PVMFFailure;
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::QueryInterface unable to query for video encoder interface!\n")););
    }
    else if (aUuid == PV2WayTestEncExtensionUUID)
    {
        PV2WayTestExtensionInterface* myInterface = OSCL_STATIC_CAST(PV2WayTestExtensionInterface*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        aInterfacePtr->addRef();
        cmd->status = PVMFSuccess;
    }
    else if (aUuid == PVUidProxiedInterface)
    {
        // this is needed for the callback for any extension interfaces for this class directly
        PV2WayTestExtensionInterface* myInterface = OSCL_STATIC_CAST(PV2WayTestExtensionInterface*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        aInterfacePtr->addRef();
        cmd->status = PVMFSuccess;
    }
    else
    {
        aInterfacePtr = NULL;
        cmd->status = PVMFErrNotSupported;
    }
    if (cmd->status != PVMFPending)
        Dispatch(cmd);
    LogFunction("QueryInterface", LEAVE_FUNCTION);

    return iCommandId++;
}



PVCommandId CPV324m2Way::CancelAllCommands(OsclAny* aContextData)
{
    LogFunction("CancelAllCommands", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::CancelAllCommands state %d\n", iState));

    if (iCancelInfo)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::CancelAllCommands, cancel in progress!\n"));
        OSCL_LEAVE(PVMFErrBusy);
    }

    switch (iState)
    {
        case EInitializing:
            SetCancelInfo(aContextData);
            SetState(EResetting);

            if (!iInitInfo)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::CancelAllCommands, no init info!\n"));
            }
            else
            {
                iInitInfo->status = PVMFErrCancelled;
                Dispatch(iInitInfo);
                iInitInfo = NULL;
            }

            CheckState();
            break;

        case EConnecting:
            SetCancelInfo(aContextData);
            SetState(EDisconnecting);

            if (!iConnectInfo)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::CancelAllCommands, no connect info!\n"));
            }
            else
            {
                iConnectInfo->status = PVMFErrCancelled;
                Dispatch(iConnectInfo);
                iConnectInfo = NULL;
            }

            CheckState();
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::CancelAllCommands invalid node state\n"));
            LogFunction("CancelAllCommands", LEAVE_FUNCTION, PVMFErrInvalidState);
            OSCL_LEAVE(PVMFErrInvalidState);
            break;
    }
    LogFunction("CancelAllCommands", LEAVE_FUNCTION);

    return iCommandId++;
}







void CPV324m2Way::ConstructL(PVMFNodeInterface* aTsc,
                             TPVTerminalType aType,
                             PVCommandStatusObserver* aCmdStatusObserver,
                             PVInformationalEventObserver *aInfoEventObserver,
                             PVErrorEventObserver *aErrorEventObserver)
{
    OSCL_UNUSED_ARG(aTsc);
    LogFunction("ConstructL", START_FUNCTION);

    iTerminalType = aType;

    /* Initialize the clock */
    if (!iClock.SetClockTimebase(iTickCountTimeBase))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::ConstructL: unable to initialize clock\n"));
        OSCL_LEAVE(PVMFFailure);
    }
    if (!iMuxClock.SetClockTimebase(iTickCountTimeBase))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::ConstructL: unable to initialize clock\n"));
        OSCL_LEAVE(PVMFFailure);
    }

    FormatCapabilityInfo inFormat;
    inFormat.dir = INCOMING;

    /* Add user input types */
    inFormat.format = PVMF_MIME_USERINPUT_DTMF;
    iIncomingUserInputFormats.push_back(inFormat);
    inFormat.format = PVMF_MIME_USERINPUT_IA5_STRING;
    iIncomingUserInputFormats.push_back(inFormat);
    inFormat.format = PVMF_MIME_USERINPUT_BASIC_STRING;
    iIncomingUserInputFormats.push_back(inFormat);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::ConstructL (%x, %x, %x, %x)",
                     aTsc, aCmdStatusObserver,
                     aInfoEventObserver, aErrorEventObserver));
    iMuxDatapath = CPV2WayMuxDatapath::NewL(iLogger, PVMF_MIME_H223, this);


    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "Full pv_2way_engine\n"));

    SetDefaults();
    iCmdStatusObserver = aCmdStatusObserver;
    iInfoEventObserver = aInfoEventObserver;
    iErrorEventObserver = aErrorEventObserver;

    iPendingNotifications.reserve(MAX_PENDING_2WAY_COMMANDS + MAX_PENDING_2WAY_EVENTS);
    iPendingNodeCmdInfo.reserve(MAX_PENDING_2WAY_NODE_COMMANDS);
    iIncomingChannelParams.reserve(MAX_LOGICAL_CHANNEL_PARAMS);
    iOutgoingChannelParams.reserve(MAX_LOGICAL_CHANNEL_PARAMS);

    int32 i;
    for (i = 0; i < MAX_PENDING_2WAY_COMMANDS; i++)
    {
        iFreeCmdInfo.push_back(&iCmdInfo[i]);
    }

    for (i = 0; i < MAX_PENDING_2WAY_EVENTS; i++)
    {
        iFreeEventInfo.push_back(&iEventInfo[i]);
    }

    for (i = 0; i < MAX_PENDING_2WAY_NODE_COMMANDS; i++)
    {
        iFreeNodeCmdInfo.push_back(&iNodeCmdInfo[i]);
    }

    iSourceNodes.reserve(3);
    iSinkNodes.reserve(3);

    AddToScheduler();

    PreInit();
    LogFunction("ConstructL", LEAVE_FUNCTION);
}

void CPV324m2Way::SetDefaults()
{
    LogFunction("SetDefaults", START_FUNCTION);
    uint32 i = 0;
    SetState(EIdle);

    if (iVideoDecNode.iNode)
    {
        iVideoDecNode.iNode->ThreadLogoff();

        DELETE_OMX_VIDEO_DEC_NODE(iVideoDecNode.iNode);

        iVideoDecNode.Clear();
    }

    if (iAudioDecNode.iNode)
    {
        iAudioDecNode.iNode->ThreadLogoff();
        DELETE_OMX_AUDIO_DEC_NODE(iAudioDecNode.iNode);

        iAudioDecNode.Clear();
    }

    if (iTscNode.iNode)
    {
        iTscNode.iNode->ThreadLogoff();
    }

    if (iAudioEncNode.iNode)
    {
        iAudioEncNode.iNode->ThreadLogoff();
        if (iAudioEncNodeInterface.iInterface)
            iAudioEncNodeInterface.iInterface->removeRef();
        DELETE_OMX_ENC_NODE(iAudioEncNode.iNode);
        iAudioEncNode.Clear() ;
        iAudioEncNodeInterface.Reset();
    }

    ClearVideoEncoderNode();

    if (iCommNode.iNode)
    {
        iCommNode.iNode->ThreadLogoff();
        iCommNode.Clear();
    }

    iFormatCapability.clear();
    iIncomingAudioCodecs.clear();
    iOutgoingAudioCodecs.clear();
    iIncomingVideoCodecs.clear();
    iOutgoingVideoCodecs.clear();

    /* Stop the clock */
    iClock.Stop();
    iMuxClock.Stop();

    /* Delete the list of sink/source nodes */
    for (i = 0; i < iSourceNodes.size(); i++)
    {
        TPV2WayNode* lNode = iSourceNodes[i];
        OSCL_DELETE(lNode);
        iSourceNodes[i] = 0;
    }
    iSourceNodes.clear();
    iSourceNodes.destroy();

    for (i = 0; i < iSinkNodes.size(); i++)
    {
        TPV2WayNode* lNode = iSinkNodes[i] ;
        OSCL_DELETE(lNode);
        iSinkNodes[i] = 0;
    }
    iSinkNodes.clear();
    iSinkNodes.destroy();

    LogFunction("SetDefaults", LEAVE_FUNCTION);
}

void CPV324m2Way::DoCancel()
{

}

void CPV324m2Way::Run()
{
    LogFunction("Run", START_FUNCTION);
    int32 error = 0;
    TPV2WayCmdInfo* cmd = NULL;
    TPV2WayEventInfo* event = NULL;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                    (0, "CPV324m2Way::Run state %d, number of notifications pending %d\n",
                     iState, iPendingNotifications.size()));

    while (!iPendingNotifications.empty())
    {
        if (iPendingNotifications[0]->notificationType ==
                TPV2WayNotificationInfo::EPV2WayCommandType)
        {
            cmd = (TPV2WayCmdInfo *) iPendingNotifications[0];

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                            (0, "CPV324m2Way::Run Calling CommandCompleted CmdType %d CmdId %d CmdStatus %d\n",
                             cmd->type, cmd->id, cmd->status));

            if (cmd->status != PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                                (0, "CPV324m2Way::Dispatch Command failed\n"));
            }
            switch (cmd->type)
            {
                case PVT_COMMAND_INIT:
                    if ((cmd->status != PVMFSuccess) &&
                            (cmd->status != PVMFErrCancelled))
                    {
                        SetDefaults();
                    }
                    break;

                case PVT_COMMAND_RESET:
                    RemoveAudioEncPath();
                    RemoveVideoEncPath();
                    RemoveAudioDecPath();
                    RemoveVideoDecPath();
                    SetDefaults();
                    break;

                case PVT_COMMAND_CONNECT:
                    if ((cmd->status != PVMFSuccess) &&
                            (cmd->status != PVMFErrCancelled))
                    {
                        iMuxDatapath->ResetDatapath();
                        iCommNode.iNode->ThreadLogoff();
                        iCommNode.Clear();
                    }
                    break;

                case PVT_COMMAND_PAUSE:
                case PVT_COMMAND_RESUME:
                {
                    PV2WayDirection direction = OSCL_STATIC_CAST(PV2WayDirection, cmd->iPvtCmdDataExt);
                    PVMFStatus status = PVMFSuccess;
                    bool pause = (cmd->type == PVT_COMMAND_PAUSE);
                    TPVChannelId channelId = OSCL_STATIC_CAST(TPVChannelId, cmd->iPvtCmdData);

                    if (direction == OUTGOING)  //If outgoing channel
                    {
                        status = iTSC324mInterface->SetLogicalChannelPause(channelId, direction, pause);
                        if (status != PVMFSuccess)
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                                            (0, "CPV324m2Way::Run Failed to set Pause to %d in Channel Id %d", pause, channelId));
                        }
                    }
                    if (status == PVMFSuccess)
                    {
                        // send pause indication to observer
                        event = GetEventInfoL();
                        if (!event)
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                            (0, "CPV324m2Way::Run() unable to notify app about pause state!\n"));
                            LogFunction("Run", LEAVE_FUNCTION, PVMFFailure);
                            return;
                        }
                        event->type = (pause) ? PVT_INDICATION_PAUSE_TRACK : PVT_INDICATION_RESUME_TRACK;

                        oscl_memset(event->localBuffer, 0, PV_COMMON_ASYNC_EVENT_LOCAL_BUF_SIZE);
                        // direction is set to first byte inside indications local buffer
                        // channel id is set to second and third byte
                        event->localBuffer[0] = (uint8)(direction & 0xFF);
                        event->localBuffer[1] = (uint8)((channelId >> 8) & 0xFF);
                        event->localBuffer[2] = (uint8)(channelId & 0xFF);
                        event->localBufferSize = 3;
                        Dispatch(event);
                    }
                }
                break;

                case PVT_COMMAND_DISCONNECT:
                    iMuxDatapath->ResetDatapath();
                    iCommNode.iNode->ThreadLogoff();
                    iCommNode.Clear();
                    break;
                case PVT_COMMAND_ADD_DATA_SOURCE:
                    if (cmd->status == PVMFSuccess)
                    {
                        if (iAudioEncDatapath && (iAudioEncDatapath->GetState() == EOpened) &&
                                iVideoEncDatapath && (iVideoEncDatapath->GetState() == EOpened))
                        {
                            TSC_324m *tsc_node = (TSC_324m *)(iTscNode.iNode);
                            tsc_node->SetSkewReference(iVideoEncDatapath->GetTSCPort(),
                                                       iAudioEncDatapath->GetTSCPort());
                        }
                    }
                    else
                    {
                        if (cmd->status != PVMFErrCancelled)
                        {
                            RemoveAudioEncPath();
                            RemoveVideoEncPath();
                        }
                    }
                    break;

                case PVT_COMMAND_ADD_DATA_SINK:
                {
                    CPV2WayDecDataChannelDatapath* datapath = NULL;
                    if (iAudioDecDatapath && (cmd->iPvtCmdData == iAudioDecDatapath->GetChannelId()))
                    {
                        datapath = iAudioDecDatapath;
                    }
                    else if (iVideoDecDatapath && (cmd->iPvtCmdData == iVideoDecDatapath->GetChannelId()))
                    {
                        datapath = iVideoDecDatapath;
                    }
                    if (datapath == NULL)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "CPV324m2Way::Run ERROR Failed to lookup dec datapath.\n"));
                        break;
                    }
                    if ((cmd->status != PVMFSuccess) &&
                            (cmd->status != PVMFErrCancelled))
                    {
                        if (datapath == iAudioDecDatapath)
                            RemoveAudioDecPath();
                        else if (datapath == iVideoDecDatapath)
                            RemoveVideoDecPath();
                        break;
                    }
                    if (iClock.GetState() != PVMFMediaClock::RUNNING)
                    {
                        // start clock close to when we expect to get data
                        StartClock();
                    }

                    if (!datapath->IsSkipComplete())
                    {
                        LogFunction("Run", LEAVE_FUNCTION);
                        return;
                    }
                }
                break;

                case PVT_COMMAND_REMOVE_DATA_SOURCE:
                    RemoveAudioEncPath();
                    RemoveVideoEncPath();
                    break;

                case PVT_COMMAND_REMOVE_DATA_SINK:
                    RemoveAudioDecPath();
                    RemoveVideoDecPath();
                    break;

                case PVT_COMMAND_CANCEL_ALL_COMMANDS:
                    //Init cancelled
                    if (iState == EIdle)
                    {
                        SetDefaults();
                    }
                    if (iState == ESetup)
                    {
                        if (iMuxDatapath->GetState() == EClosed)
                        {
                            iMuxDatapath->ResetDatapath();
                            iCommNode.iNode->ThreadLogoff();
                            iCommNode.Clear();
                        }
                    }
                    break;

                case PVT_ESTABLISH_CHANNEL:
                {

                    PV2WayChannelData* pChannelData = (PV2WayChannelData*)cmd->contextData;
                    // call EstablishChannel using cmd->ContextData
                    if (pChannelData)
                    {
                        int err = 0;
                        if (pChannelData->iDir == INCOMING)
                        {
                            OSCL_TRY(err, StartTscClock(););
                            OSCL_FIRST_CATCH_ANY(err, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                                 (0, "CPV324m2Way::Run ERROR calling StartTscClock: %d .\n", err)););
                        }

                        OSCL_TRY(err, EstablishChannel(pChannelData->iDir, pChannelData->iId,
                                                       pChannelData->iCodec, pChannelData->iFormatSpecificInfo,
                                                       pChannelData->iFormatSpecificInfoLen));

                        OSCL_FIRST_CATCH_ANY(err, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::Run ERROR EstablishChannel: %d .\n", err)););
                        pChannelData->removeRef();
                        pChannelData = NULL;
                    }
                    else
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "CPV324m2Way::Run ERROR EstablishChannel called without channel information.\n"));
                    }
                    break;
                }
                case PVT_REQUEST_FRAME_UPDATE:
                {
                    PVMFPortInterface* pPort = (PVMFPortInterface*)cmd->contextData;
                    if (pPort)
                    {
                        GenerateIFrame(pPort);
                    }
                    else
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "CPV324m2Way::Run ERROR GenerateIFrame called without port.\n"));
                    }

                    break;
                }
                case PVT_CHANNEL_CLOSED:
                {
                    PV2WayChannelData* pChannelData = (PV2WayChannelData*)cmd->contextData;
                    // call FinishChannelClosed using cmd->ContextData
                    if (pChannelData)
                    {
                        int32 err = 0;
                        OSCL_TRY(err, FinishChannelClosed(pChannelData->iDir, pChannelData->iId,
                                                          pChannelData->iCodec));
                        OSCL_FIRST_CATCH_ANY(err, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::Run ERROR FinishChannelClosed: %d .\n", err)););
                        pChannelData->removeRef();
                        pChannelData = NULL;
                    }
                    else
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "CPV324m2Way::Run ERROR ChannelClosed called without channel information.\n"));
                    }
                    break;
                }
                default:
                    break;
            }

            if (cmd->type != PVT_ESTABLISH_CHANNEL &&
                    cmd->type != PVT_REQUEST_FRAME_UPDATE &&
                    cmd->type != PVT_CHANNEL_CLOSED)
            {
                // only send back response for things we want the app to be notified about
                PVCmdResponse resp(cmd->id, cmd->contextData, cmd->status, NULL, 0);
                OSCL_TRY(error, iCmdStatusObserver->CommandCompleted(resp));
                OSCL_FIRST_CATCH_ANY(error, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                                     PVLOGMSG_ERR, (0, "CPV324m2Way::Run CommandCompletedL leave %d\n",
                                                    error)));
            }

            iPendingNotifications.erase(iPendingNotifications.begin());
            FreeCmdInfo(cmd);

        }
        else if (iPendingNotifications[0]->notificationType ==
                 TPV2WayNotificationInfo::EPV2WayEventType)
        {
            event = (TPV2WayEventInfo *) iPendingNotifications[0];

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                            (0, "CPV324m2Way::Run Calling HandleInformationalEventL EventType %d\n",
                             event->type));

            switch (event->type)
            {
                case PVT_INDICATION_CLOSE_TRACK:
                {
                    TPVChannelId* id = (TPVChannelId*)(event->localBuffer + 4);
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                                    (0, "CPV324m2Way::Run PVT_INDICATION_CLOSE_TRACK direction %d, channel id=%d",
                                     event->localBuffer[0], *id));
                    if (event->localBuffer[0] == INCOMING)
                    {
                        if (iAudioDecDatapath && (*id == iAudioDecDatapath->GetChannelId()))
                        {
                            RemoveAudioDecPath();
                        }
                        else if (iVideoDecDatapath && (*id == iVideoDecDatapath->GetChannelId()))
                        {
                            RemoveVideoDecPath();
                        }
                    }
                    else if (event->localBuffer[0] == OUTGOING)
                    {
                        if (iAudioEncDatapath && (*id == iAudioEncDatapath->GetChannelId()))
                        {
                            RemoveAudioEncPath();
                        }
                        else if (iVideoEncDatapath && (*id == iVideoEncDatapath->GetChannelId()))
                        {
                            RemoveVideoEncPath();
                        }
                    }

                    CheckState();
                }
                break;

                case PVT_INDICATION_DISCONNECT:
                    iMuxDatapath->ResetDatapath();
                    iCommNode.iNode->ThreadLogoff();
                    iCommNode.Clear();
                    break;

                default:
                    break;
            }

            {
                PVAsyncInformationalEvent infoEvent(event->type, event->exclusivePtr,
                                                    &event->localBuffer[0], event->localBufferSize);
                OSCL_TRY(error, iInfoEventObserver->HandleInformationalEvent(infoEvent));
                OSCL_FIRST_CATCH_ANY(error, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                                     PVLOGMSG_ERR, (0, "CPV324m2Way::Run HandleInformationalEventL leave %d\n",
                                                    error)));
            }

            iPendingNotifications.erase(iPendingNotifications.begin());
            FreeEventInfo(event);
        }
    }
    LogFunction("Run", LEAVE_FUNCTION);
}

void CPV324m2Way::Dispatch(TPV2WayCmdInfo* aCmdInfo)
{
    LogFunction("Dispatch(TPV2WayCmdInfo*)", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                    (0, "CPV324m2Way::Dispatch Appending command to queue CmdType %d CmdId %d CmdStatus %d\n",
                     aCmdInfo->type, aCmdInfo->id, aCmdInfo->status));
    if (aCmdInfo->status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::Dispatch Command failed\n"));
    }
    int32 err = 0;
    OSCL_TRY(err, iPendingNotifications.push_back(aCmdInfo););
    if (err)
    {
        OSCL_DELETE(aCmdInfo);
        LogFunction("Dispatch(TPV2WayCmdInfo*)", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFErrNoMemory);
    }
    RunIfNotReady();
    LogFunction("Dispatch(TPV2WayCmdInfo*)", LEAVE_FUNCTION);
}

void CPV324m2Way::Dispatch(TPV2WayEventInfo* aEventInfo)
{
    LogFunction("Dispatch(TPV2WayEventInfo*)", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                    (0, "CPV324m2Way::Dispatch Appending event to queue event type %d\n",
                     aEventInfo->type));

    int32 err = 0;
    OSCL_TRY(err, iPendingNotifications.push_back(aEventInfo););
    if (err)
    {
        OSCL_DELETE(aEventInfo);
        LogFunction("Dispatch(TPV2WayEventInfo*)", LEAVE_FUNCTION, PVMFFailure);
        OSCL_LEAVE(PVMFErrNoMemory);
    }
    RunIfNotReady();
    LogFunction("Dispatch(TPV2WayEventInfo*)", LEAVE_FUNCTION);
}


bool CPV324m2Way::IsNodeInList(Oscl_Vector<TPV2WayNode*, OsclMemAllocator>& aList,
                               PVMFNodeInterface* aNode)
{
    LogFunction("IsNodeInList", START_FUNCTION);
    for (uint32 i = 0; i < aList.size(); i++)
    {
        TPV2WayNode* lNode = aList[i];
        if (lNode && lNode->iNode == aNode)
        {
            LogFunction("IsNodeInList", LEAVE_FUNCTION, PVMFSuccess);
            return true;
        }
    }
    LogFunction("IsNodeInList", LEAVE_FUNCTION, PVMFFailure);
    return false;
}

bool CPV324m2Way::IsSourceNode(PVMFNodeInterface* apNode)
{
    return IsNodeInList(iSourceNodes, apNode);
}

bool CPV324m2Way::IsSinkNode(PVMFNodeInterface* apNode)
{
    return IsNodeInList(iSinkNodes, apNode);
}

TPV2WayNode* CPV324m2Way::GetTPV2WayNode(Oscl_Vector<TPV2WayNode*, OsclMemAllocator>& aList,
        PVMFNodeInterface* aNode)
{
    LogFunction("GetTPV2WayNode", START_FUNCTION);
    for (uint32 i = 0; i < aList.size(); i++)
    {
        TPV2WayNode* lNode = aList[i];
        if (lNode && lNode->iNode == aNode)
        {
            LogFunction("GetTPV2WayNode", LEAVE_FUNCTION, PVMFSuccess);
            return lNode;
        }
    }
    LogFunction("GetTPV2WayNode", LEAVE_FUNCTION, PVMFFailure);
    return NULL;
}

TPV2WayNode* CPV324m2Way::RemoveTPV2WayNode(Oscl_Vector<TPV2WayNode*, OsclMemAllocator>& aList,
        PVMFNodeInterface* aNode)
{
    LogFunction("RemoveTPV2WayNode", START_FUNCTION);
    for (uint32 i = 0; i < aList.size(); i++)
    {
        TPV2WayNode* lNode = aList[i];
        if (lNode && lNode->iNode == aNode)
        {
            aList[i] = 0;
            LogFunction("RemoveTPV2WayNode", LEAVE_FUNCTION, PVMFSuccess);
            return lNode;
        }
    }
    LogFunction("RemoveTPV2WayNode", LEAVE_FUNCTION, PVMFFailure);
    return NULL;
}

// from PVMFNodeCmdEventObserver
void CPV324m2Way::NodeCommandCompleted(const PVMFCmdResp& aResponse)
{
    LogFunction("NodeCommandCompleted", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::NodeCommandCompleted status %d, context %x\n",
                     aResponse.GetCmdStatus(), aResponse.GetContext()));

    if (aResponse.GetCmdStatus() != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::NodeCommandCompleted Command failed\n"));
    }
    CPV2WayNodeContextData *data = (CPV2WayNodeContextData *) aResponse.GetContext();
    TPV2WayNodeCmdInfo *info = FindPendingNodeCmd(data->iNode, aResponse.GetCmdId());

    data->iObserver->CommandHandler(info->type, aResponse);

    // check if node cmd response requires engine cmd response
    if (info->engineCmdInfo != NULL)
    {
        info->engineCmdInfo->status = aResponse.GetCmdStatus();
        Dispatch(info->engineCmdInfo);
    }

    //Remove the command from the pending list.
    RemovePendingNodeCmd(data->iNode, aResponse.GetCmdId());
    LogFunction("NodeCommandCompleted", LEAVE_FUNCTION);
}

// from PVMFNodeInfoEventObserver
void CPV324m2Way::HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    LogFunction("HandleNodeInformationalEvent", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::HandleNodeInformationalEvent type %d\n",
                     aEvent.GetEventType()));

    if (iTscNode == aEvent.GetContext())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::HandleNodeInformationalEvent tsc node\n"));
        uint8 *buf = aEvent.GetLocalBuffer();
        switch (buf[0])
        {
            case PV_H324COMPONENT_H223:
                switch (buf[2])
                {
                    case INCOMING:
                    {
                        TPVChannelId id = CHANNEL_ID_UNKNOWN;
                        if (buf[1] == PV_MUX_COMPONENT_LOGICAL_CHANNEL)
                        {
                            id = *((TPVChannelId*)(buf + 4));

                            // See if error is in video datapath
                            if (id == iVideoDecDatapath->GetChannelId())
                            {
                                // request for I-frame
                                RequestRemoteIFrame(iVideoDecDatapath->GetTSCPort());
                            }
                        }
                    }
                    break;

                    case OUTGOING:
                        GenerateIFrame(iVideoEncDatapath->GetTSCPort());
                        break;

                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }
    else if (iCommNode == aEvent.GetContext())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::HandleNodeInformationalEvent comm node\n"));
    }
    else if (iVideoDecNode == aEvent.GetContext())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::HandleNodeInformationalEvent video dec node\n"));
        if (PVMFPvmiBufferAllocatorNotAcquired == aEvent.GetEventType())
        {
            // do something
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                            (0, "CPV324m2Way::HandleNodeInformationalEvent using local buffer for video decoder\n"));
            // now let app know somehow
            iUsingExternalVideoDecBuffers = false;
        }
        if (PVMFPvmiBufferAlloctorAcquired == aEvent.GetEventType())
        {
            // do something
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "CPV324m2Way::HandleNodeInformationalEvent using *External* buffer for video decoder\n"));
            // now let app know somehow
            iUsingExternalVideoDecBuffers = true;
        }
    }
    else if (iVideoEncNode == aEvent.GetContext())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::HandleNodeInformationalEvent video encoder node\n"));
    }
    else
    {
        PVMFNodeInterface * node = (PVMFNodeInterface *) aEvent.GetContext();
        CPV2WayDataChannelDatapath* datapath = NULL;
        if (node)
        {
            datapath = GetDataPath(*node);
        }
        if (iAudioEncDatapath == datapath || iVideoEncDatapath == datapath)
        {
            // nothing to do
        }
        else if (iAudioDecDatapath == datapath)
        {
            PVMFEventType event = aEvent.GetEventType();
            if (event == PVMFInfoStartOfData)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "CPV324m2Way::HandleNodeInformationalEvent audio dec datapath PVMFInfoStartOfData received, Clock started\n"));
            }
            else if (event == PVMFInfoStateChanged)
            {
                // Pause/resume has to be done here and not in run as decoder needs to be started before
                // channel is resumed. Otherwise the sending of bos message will fail.

                // interface state is written into pointer???
                // so pointer int* needs to casted to int
                int nodeInterfaceState = OSCL_STATIC_CAST(int, OSCL_STATIC_CAST(int*,
                                         aEvent.GetEventData()));
                bool pause = (nodeInterfaceState == EPVMFNodePaused);
                if (iTSC324mInterface && (pause || (nodeInterfaceState == EPVMFNodeStarted)))
                {
                    iTSC324mInterface->SetLogicalChannelPause(iAudioDecDatapath->GetChannelId(),
                            INCOMING, pause);
                }
            }
            else if (event == PVMFPvmiBufferAllocatorNotAcquired)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_WARNING,
                                (0, "CPV324m2Way::HandleNodeInformationalEvent using local buffer for audio decoder\n"));
                iUsingExternalAudioDecBuffers = false;
            }
            else if (event == PVMFPvmiBufferAlloctorAcquired)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "CPV324m2Way::HandleNodeInformationalEvent using *External* buffer for audio decoder\n"));
                iUsingExternalAudioDecBuffers = true;
            }
        }
        else if (iVideoDecDatapath == datapath)
        {
            PVMFEventType event = aEvent.GetEventType();
            if (event == PVMFInfoStartOfData)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "CPV324m2Way::HandleNodeInformationalEvent video dec datapath PVMFInfoStartOfData received, Clock started\n"));
            }
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::HandleNodeInformationalEvent unknown node!"));
        }
    }

    LogFunction("HandleNodeInformationalEvent", LEAVE_FUNCTION);
}

// from PVMFNodeErrorEventObserver
void CPV324m2Way::HandleNodeErrorEvent(const PVMFAsyncEvent& aEvent)
{
    LogFunction("HandleNodeErrorEvent", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::HandleNodeErrorEvent type %d\n", aEvent.GetEventType()));

    if (iTscNode == aEvent.GetContext())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::HandleNodeErrorEvent tsc node\n"));
        switch (iState)
        {
            case EDisconnecting:
                CheckState();
                break;

            case EConnecting:
            case EConnected:
                //InitiateDisconnect();
                break;

            default:
                break;
        }
    }
    else if (iCommNode == aEvent.GetContext())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::HandleNodeErrorEvent comm node\n"));
        switch (iState)
        {
            case EDisconnecting:
                CheckState();
                break;

            case EConnecting:
            case EConnected:
                InitiateDisconnect();
                break;

            default:
                break;
        }
    }
    else
    {
        PVMFNodeInterface * node = (PVMFNodeInterface *) aEvent.GetContext();
        CPV2WayDataChannelDatapath* datapath = NULL;
        if (node)
        {
            datapath = GetDataPath(*node);
        }
        if (iVideoEncDatapath == datapath || iVideoDecDatapath == datapath)
        {
            datapath->SetCmd(NULL);
        }
        else if (iAudioEncDatapath == datapath)
        {
            iAudioEncDatapath->UseFilePlayPort(false);
            iAudioEncDatapath->SetSourceInputPort(NULL);

            iAudioEncDatapath->SetCmd(NULL);
        }
        else if (iAudioDecDatapath == datapath)
        {
            iAudioDecDatapath->SetCmd(NULL);
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::HandleNodeErrorEvent unknown node!\n"));
        }
    }
    LogFunction("HandleNodeErrorEvent", LEAVE_FUNCTION);
}

void CPV324m2Way::CommandHandler(PV2WayNodeCmdType aType,
                                 const PVMFCmdResp& aResponse)
{
    LogFunction("CommandHandler", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::CommandHandler, state %d, type %d\n", iState, aType));

    CPV2WayNodeContextData *data = (CPV2WayNodeContextData *) aResponse.GetContext();

    if (data->iNode == iCommNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::CommandHandler comm node\n"));
        HandleCommNodeCmd(aType, aResponse);
    }
    else if (data->iNode == iTscNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::CommandHandler TSC node\n"));
        HandleTscNodeCmd(aType, aResponse);
    }
    else if (data->iNode == iVideoDecNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::CommandHandler video decoder node\n"));
        HandleVideoDecNodeCmd(aType, aResponse);
    }
    else if (data->iNode == iVideoEncNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::CommandHandler video encoder node\n"));
        HandleVideoEncNodeCmd(aType, aResponse);
    }
    else if (IsSourceNode(data->iNode))
    {
        /* Do Add Data Source Here */
        TPV2WayNode *source_node = GetTPV2WayNode(iSourceNodes, data->iNode);
        OSCL_ASSERT(source_node);
        DoAddDataSource(*source_node, aResponse);
    }
    else if (IsSinkNode(data->iNode))
    {
        TPV2WayNode *sink_node = GetTPV2WayNode(iSinkNodes, data->iNode);
        OSCL_ASSERT(sink_node);
        HandleSinkNodeCmd(aType, aResponse, sink_node);
    }
    else if (data->iNode == iAudioEncNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::CommandHandler audio encoder node\n"));
        HandleAudioEncNodeCmd(aType, aResponse);
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::CommandHandler unknown node!"));
    }
    LogFunction("CommandHandler", LEAVE_FUNCTION);
}

PVMFStatus CPV324m2Way::ConfigureNode(CPVDatapathNode *aNode)
{
    LogFunction("ConfigureNode", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::ConfigureNode, state %d\n", iState));

    PVMFNodeInterface *node = aNode->iNode.iNode;

    if (node == iTscNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::ConfigureNode configuring tsc node\n"));
        LogFunction("ConfigureNode", LEAVE_FUNCTION, PVMFSuccess);
        return PVMFSuccess;
    }
    else if (node == iCommNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::ConfigureNode configuring comm node\n"));
        LogFunction("ConfigureNode", LEAVE_FUNCTION, PVMFSuccess);
        return PVMFSuccess;
    }
    else if (node == iVideoEncNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "CPV324m2Way::ConfigureNode configuring video enc node\n"));

        PVMFStatus status = ConfigureVideoEncoderNode();

        LogFunction("ConfigureNode", LEAVE_FUNCTION, status);
        return status;
    }
    else if (node == iVideoDecNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::ConfigureNode configuring video dec node\n"));

        LogFunction("ConfigureNode", LEAVE_FUNCTION, PVMFSuccess);
        return PVMFSuccess;
    }
    else if (node == iAudioEncNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::ConfigureNode configuring audio enc node\n"));
        //PVMFPortProperty prop;
        PVAudioEncExtensionInterface *ptr =
            (PVAudioEncExtensionInterface *) iAudioEncNodeInterface.iInterface;
        if (aNode->iOutputPort.iPortPair->iDestPort.GetStatus() != EHasPort)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "CPV324m2Way::ConfigureNode waiting for tsc port to determine audio codec type.\n"));
            LogFunction("ConfigureNode", LEAVE_FUNCTION, PVMFPending);
            return PVMFPending;
        }

        //aNode->iOutputPort.iPortPair->iDestPort.GetPort()->Query(prop);

        //ptr->SetOutputFormat(PVMF_AMR_IF2);
        //ptr->SetInputSamplingRate(KSamplingRate);
        //ptr->SetInputBitsPerSample(KBitsPerSample);
        //ptr->SetInputNumChannels(KNumChannels);
        ptr->SetOutputBitRate(GSM_AMR_12_2);
        ptr->SetMaxNumOutputFramesPerBuffer(KNumPCMFrames);

        LogFunction("ConfigureNode", LEAVE_FUNCTION, PVMFSuccess);
        return PVMFSuccess;

    }
    else if (node == iAudioDecNode.iNode)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::ConfigureNode configuring audio dec node\n"));
        LogFunction("ConfigureNode", LEAVE_FUNCTION, PVMFSuccess);
        return PVMFSuccess;
    }

    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::ConfigureNode unknown node\n"));
        LogFunction("ConfigureNode", LEAVE_FUNCTION, PVMFFailure);
        return PVMFFailure;
    }
    LogFunction("ConfigureNode", LEAVE_FUNCTION, PVMFFailure);
    return PVMFFailure;
}

// Implementations of TSC Observer virtuals
void CPV324m2Way::ConnectComplete(PVMFStatus status)
{
    LogFunction("ConnectComplete", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::ConnectComplete, state %d, status %d\n",
                     iState, status));
    if (status == PVMFSuccess)
    {
        iIsStackConnected = true;
    }
    else
    {
        iIsStackConnected = false;
        SetState(EDisconnecting);
    }

    CheckState();
    LogFunction("ConnectComplete", LEAVE_FUNCTION);
}

void CPV324m2Way::CloseDatapathTSCPorts()
{
    if (iAudioDecDatapath)
    {
        iAudioDecDatapath->TSCPortClosed();
    }
    if (iAudioEncDatapath)
    {
        iAudioEncDatapath->TSCPortClosed();
    }
    if (iVideoDecDatapath)
    {
        iVideoDecDatapath->TSCPortClosed();
    }
    if (iVideoEncDatapath)
    {
        iVideoEncDatapath->TSCPortClosed();
    }
}

void CPV324m2Way::InternalError()
{
    LogFunction("InternalError", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                    (0, "CPV324m2Way::InternalError, state %d\n", iState));

    switch (iState)
    {
        case EDisconnecting:
        case EConnecting:
        case EConnected:
            CloseDatapathTSCPorts();

            break;

        default:
            break;
    }
    switch (iState)
    {
        case EDisconnecting:
            CheckState();
            break;

        case EConnecting:
        case EConnected:
            InitiateDisconnect();
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::InternalError invalid state\n"));
            break;
    }
    LogFunction("InternalError", LEAVE_FUNCTION);
}

void CPV324m2Way::DisconnectRequestReceived()
{
    LogFunction("DisconnectRequestReceived", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::DisconnectRequestReceived state %d\n", iState));

    iIsStackConnected = false;
    if (iDisconnectInfo)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::DisconnectRequestReceived Doing nothing as  disconnect is in progress\n"));
    }
    else
    {

        switch (iState)
        {
            case EDisconnecting:
                CloseDatapathTSCPorts();

                CheckState();
                break;

            case EConnecting:
            case EConnected:
                CloseDatapathTSCPorts();

                iRemoteDisconnectTimer->SetObserver(this);
                iRemoteDisconnectTimer->Request(REMOTE_DISCONNECT_TIMER_ID, REMOTE_DISCONNECT_TIMER_ID,
                                                REMOTE_DISCONNECT_TIMER_VALUE, this);


                //We do InitiateDisconnect() once above timer expires
                break;

            default:
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::DisconnectRequestReceived invalid state\n"));
                break;
        }
    }
    LogFunction("DisconnectRequestReceived", LEAVE_FUNCTION);
}



PVMFStatus CPV324m2Way::EstablishChannel(TPVDirection aDir,
        TPVChannelId aId,
        PVCodecType_t aCodec,
        uint8* apFormatSpecificInfo,
        uint32 aFormatSpecificInfoLen)
{
    LogFunction("EstablishChannel", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::EstablishChannel aDir=%d, channel id=%d, codec %d\n",
                     aDir, aId, aCodec));

    if (PV_CODEC_TYPE_NONE == aCodec)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::EstablishChannel codec type none\n"));
        LogFunction("EstablishChannel", LEAVE_FUNCTION);
        return EPVT_Success;
    }

    PV2WayMediaType media_type = ::GetMediaType(aCodec);

    // aFormatType is the Codec that the establishing channel wants
    PVMFFormatType formatType = PVCodecTypeToPVMFFormatType(aCodec);
    PVMFFormatType appFormatType = PVMF_MIME_FORMAT_UNKNOWN;


    ////////////////////////////////////////////////////////////////

    Oscl_Map < PVMFFormatType, FormatCapabilityInfo, OsclMemAllocator,
    pvmf_format_type_key_compare_class > * codec_list = NULL;
    Oscl_Map < PVMFFormatType, PVMFFormatType, OsclMemAllocator,
    pvmf_format_type_key_compare_class > * app_format_for_engine_format = NULL;

    iReadDataLock.Lock();

    CPV2WayDataChannelDatapath* datapath = NULL;

    if (aDir == INCOMING)
    {
        if (media_type == PV_AUDIO)
        {
            if (!(iAudioDecDatapath))
            {
                iAudioDecDatapath = CPV2WayDecDataChannelDatapath::NewL(iLogger,
                                    formatType, this);

            }
            datapath = iAudioDecDatapath;
            codec_list = &iIncomingAudioCodecs;
        }
        else if (media_type == PV_VIDEO)
        {
            if (!(iVideoDecDatapath))
            {
                iVideoDecDatapath = CPV2WayDecDataChannelDatapath::NewL(iLogger, formatType, this);
            }
            datapath = iVideoDecDatapath;
            codec_list = &iIncomingVideoCodecs;
        }
        app_format_for_engine_format = &iAppFormatForEngineFormatIncoming;
    }

    else
    {
        if (media_type == PV_AUDIO)
        {
            if (!(iAudioEncDatapath))
            {
                iAudioEncDatapath = CPV2WayEncDataChannelDatapath::NewL(iLogger,
                                    formatType, this);
            }

            datapath = iAudioEncDatapath;
            codec_list = &iOutgoingAudioCodecs;
        }
        else if (media_type == PV_VIDEO)
        {
            if (!(iVideoEncDatapath))
            {
                iVideoEncDatapath = CPV2WayEncDataChannelDatapath::NewL(iLogger, formatType, this);
            }

            datapath = iVideoEncDatapath;
            codec_list = &iOutgoingVideoCodecs;
        }
        app_format_for_engine_format = &iAppFormatForEngineFormatOutgoing;
    }

    Oscl_Map < PVMFFormatType, FormatCapabilityInfo, OsclMemAllocator,
    pvmf_format_type_key_compare_class >::iterator it = codec_list->find(formatType);

    if (it == codec_list->end())
    {
        iReadDataLock.Unlock();
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::EstablishChannel Failed to lookup codec=%d\n", aCodec));
        return PVMFFailure;
    }


    if ((*it).second.iPriority == ENG)
    {
        // Set the app format to the stored raw format type
        appFormatType = (*app_format_for_engine_format)[formatType];

        if (aDir == INCOMING)
        {
            // since the app will use raw format we need a decoder.
            if (media_type == PV_AUDIO)
            {
                AddAudioDecoderNode();
            }
            else if (media_type == PV_VIDEO)
            {
                AddVideoDecoderNode();
            }
        }
        else
        {
            if (media_type == PV_AUDIO)
            {
                AddAudioEncoderNode();
            }
            else if (media_type == PV_VIDEO)
            {
                AddVideoEncoderNode();
            }
        }
    }
    else
    {
        // Set the app format to the compressed type
        appFormatType = formatType;
    }
    ////////////////////////////////////////////////////////////////

    datapath->SetFormat(formatType);
    datapath->SetSourceSinkFormat(appFormatType);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                    (0, "CPV324m2Way::EstablishChannel setting codec=%d formatType:%s appFormat: %s\n", aCodec,
                     formatType.getMIMEStrPtr(), appFormatType.getMIMEStrPtr()));

    TPV2WayEventInfo* event = NULL;
    if (!GetEventInfo(event))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::EstablishChannel Memory allocation failed!\n"));
        return PVMFErrNoMemory;
    }
    // Send the informational event to the app
    event->localBuffer[0] = (uint8) media_type;
    // bytes 1,2,3 are unused
    *((TPVChannelId*)(event->localBuffer + 4)) = aId;
    event->localBufferSize = 8;

    PVEventType eventType = (aDir == INCOMING) ? PVT_INDICATION_INCOMING_TRACK : PVT_INDICATION_OUTGOING_TRACK;
    PVUuid puuid = PV2WayTrackInfoInterfaceUUID;

    PV2WayTrackInfoInterface* pTrackInfo = OSCL_NEW(PV2WayTrackInfoImpl,
                                           (appFormatType,
                                            apFormatSpecificInfo,
                                            aFormatSpecificInfoLen,
                                            eventType, puuid));
    OSCL_TRAPSTACK_PUSH(OsclTrapItem(ReleasePV2WayTrackInfoInterface, pTrackInfo));
    PVAsyncInformationalEvent infoEvent(eventType, NULL,
                                        OSCL_STATIC_CAST(PVInterface*, pTrackInfo), NULL,
                                        event->localBuffer, event->localBufferSize);
    if (iInfoEventObserver != NULL)
    {
        iInfoEventObserver->HandleInformationalEvent(infoEvent);
    }
    OSCL_TRAPSTACK_POP();
    pTrackInfo->removeRef();
    iReadDataLock.Unlock();

    LogFunction("EstablishChannel", LEAVE_FUNCTION);
    return EPVT_Success;
}

void CPV324m2Way::OutgoingChannelEstablished(TPVChannelId aId,
        PVCodecType_t aCodec,
        uint8* aFormatSpecificInfo,
        uint32 aFormatSpecificInfoLen)
{
    LogFunction("OutgoingChannelEstablished", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::OutgoingChannelEstablished id=%d, codec=%d, fsi=%x, fsi_len=%d",
                     aId, aCodec, aFormatSpecificInfo, aFormatSpecificInfoLen));
    TPV2WayCmdInfo* nodeEvent = GetCmdInfoL();
    nodeEvent->type = PVT_ESTABLISH_CHANNEL;
    PV2WayChannelData* pChannelData = OSCL_NEW(PV2WayChannelData, (OUTGOING,
                                      aId, aCodec, aFormatSpecificInfo, aFormatSpecificInfoLen));
    nodeEvent->contextData = pChannelData;
    Dispatch(nodeEvent);
    LogFunction("OutgoingChannelEstablished", LEAVE_FUNCTION);
}

OSCL_EXPORT_REF TPVStatusCode CPV324m2Way::IncomingChannel(TPVChannelId aId,
        PVCodecType_t aCodec,
        uint8* aFormatSpecificInfo,
        uint32 aFormatSpecificInfoLen)
{
    LogFunction("IncomingChannel", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::IncomingChannel channel id=%d, codec %d\n",
                     aId, aCodec));
    TPV2WayCmdInfo* nodeEvent = GetCmdInfoL();
    nodeEvent->type = PVT_ESTABLISH_CHANNEL;
    PV2WayChannelData* pChannelData = OSCL_NEW(PV2WayChannelData, (INCOMING,
                                      aId, aCodec, aFormatSpecificInfo, aFormatSpecificInfoLen));
    nodeEvent->contextData = pChannelData;
    Dispatch(nodeEvent);
    LogFunction("IncomingChannel", LEAVE_FUNCTION);
    return PVMFSuccess;
}

bool CPV324m2Way::GetEventInfo(TPV2WayEventInfo*& aprEvent)
{
    LogFunction("GetEventInfo", START_FUNCTION);
    int32 error = 0;
    OSCL_TRY(error, aprEvent = GetEventInfoL());
    OSCL_FIRST_CATCH_ANY(error,
                         return false);
    LogFunction("GetEventInfo", LEAVE_FUNCTION);
    return true;
}

bool CPV324m2Way::ChannelClosed(CPV2WayDataChannelDatapath* apDatapath,
                                TPVChannelId aId)
{
    LogFunction("ChannelClosed", START_FUNCTION);
    bool track_closed = false;
    if (apDatapath)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                        (0, "CPV324m2Way::ChannelClosed path state %d, id %d\n",
                         apDatapath->GetState(), apDatapath->GetChannelId()));
        if (apDatapath->GetChannelId() == CHANNEL_ID_UNKNOWN)
        {
            track_closed = true;
        }
        else if (aId == apDatapath->GetChannelId())
        {
            switch (apDatapath->GetState())
            {
                case EClosing:
                    break;
                case EClosed:
                    track_closed = true;
                    break;
                default:
                    apDatapath->SetCmd(NULL);
                    break;
            }
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                            (0, "CPV324m2Way::ChannelClosed ERROR Channel id mismatch id=%d, datapath id=%d\n",
                             aId, apDatapath->GetChannelId()));
        }
    }
    LogFunction("ChannelClosed", LEAVE_FUNCTION);
    return track_closed;
}

void CPV324m2Way::SendClosingTrackIndication(TPVDirection aDirection,
        TPVChannelId aId)
{
    TPV2WayEventInfo* event = NULL;
    // Send the closing track indication
    if (!GetEventInfo(event))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::SendClosingTrackIndication unable to allocate memory\n"));
        LogFunction("SendClosingTrackIndication", LEAVE_FUNCTION, PVMFFailure);
        return;
    }

    event->type = PVT_INDICATION_CLOSING_TRACK;
    event->localBufferSize = 8;
    event->localBuffer[0] = (uint8)aDirection;
    // bytes 1,2,3 are unused
    *((TPVChannelId*)(event->localBuffer + 4)) = aId;
    Dispatch(event);
}

void CPV324m2Way::SendClosedTrackIndication(TPVDirection aDirection,
        TPVChannelId aId)
{
    TPV2WayEventInfo* event = NULL;
    // Send the closing track indication
    if (!GetEventInfo(event))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::FinishChannelClosed unable to allocate memory\n"));
        LogFunction("SendClosedTrackIndication", LEAVE_FUNCTION, PVMFFailure);
        return;
    }
    event->type = PVT_INDICATION_CLOSE_TRACK;
    event->localBufferSize = 8;
    event->localBuffer[0] = (uint8)aDirection;
    // bytes 1,2,3 are unused
    *((TPVChannelId*)(event->localBuffer + 4)) = aId;
    Dispatch(event);
}

void CPV324m2Way::ChannelClosed(TPVDirection aDirection,
                                TPVChannelId aId,
                                PVCodecType_t aCodec,
                                PVMFStatus aStatus)
{
    OSCL_UNUSED_ARG(aStatus);
    LogFunction("ChannelClosed", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::ChannelClosed id %d, codec %d, direction %d\n",
                     aId, aCodec, aDirection));
    TPV2WayCmdInfo* nodeEvent = GetCmdInfoL();
    nodeEvent->type = PVT_CHANNEL_CLOSED;
    PV2WayChannelData* pChannelData = OSCL_NEW(PV2WayChannelData, (aDirection,
                                      aId, aCodec, NULL, 0));
    nodeEvent->contextData = pChannelData;
    Dispatch(nodeEvent);
    LogFunction("ChannelClosed", LEAVE_FUNCTION);
}

void CPV324m2Way::FinishChannelClosed(TPVDirection aDirection,
                                      TPVChannelId aId,
                                      PVCodecType_t aCodec)
{
    LogFunction("FinishChannelClosed", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "CPV324m2Way::FinishChannelClosed id %d, codec %d, direction %d\n",
                     aId, aCodec, aDirection));

    SendClosingTrackIndication(aDirection, aId);

    CPV2WayDataChannelDatapath* datapath = GetDataPath(aDirection,
                                           ::GetMediaType(aCodec));
    if (datapath)
    {
        if (!ChannelClosed(datapath, aId))
        {
            LogFunction("ChannelClosed", LEAVE_FUNCTION, PVMFFailure);
            return;
        }
    }

    SendClosedTrackIndication(aDirection, aId);

    LogFunction("FinishChannelClosed", LEAVE_FUNCTION);
}

void CPV324m2Way::RequestFrameUpdate(PVMFPortInterface* aPort)
{
    LogFunction("RequestFrameUpdate", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::RequestFrameUpdate\n"));
    if (iVideoEncDatapath)
    {
        TPV2WayCmdInfo* nodeEvent = GetCmdInfoL();
        nodeEvent->type = PVT_REQUEST_FRAME_UPDATE;
        nodeEvent->contextData = aPort;
        Dispatch(nodeEvent);
    }
    LogFunction("RequestFrameUpdate", LEAVE_FUNCTION);
}


void  CPV324m2Way::TimeoutOccurred(int32 timerID,
                                   int32 timeoutInfo)
{
    OSCL_UNUSED_ARG(timeoutInfo);
    LogFunction("TimeoutOccurred", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::TimeoutOccurred id %d, info %d\n", timerID, timeoutInfo));

    if (timerID == IFRAME_REQ_TIMERID)
    {
        isIFrameReqTimerActive = false;
    }
    else if ((timerID == END_SESSION_TIMER_ID) || (timerID == REMOTE_DISCONNECT_TIMER_ID))
    {
        // Cancel out both timers if any one expires, as both do InitiateDisconnect()
        if (iEndSessionTimer)
        {
            iEndSessionTimer->Cancel(END_SESSION_TIMER_ID);
        }

        if (iRemoteDisconnectTimer)
        {
            iRemoteDisconnectTimer->Cancel(REMOTE_DISCONNECT_TIMER_ID);
        }

        InitiateDisconnect();
    }
    LogFunction("TimeoutOccurred", LEAVE_FUNCTION);

}

TPV2WayCmdInfo *CPV324m2Way::GetCmdInfoL()
{
    LogFunction("GetCmdInfoL", START_FUNCTION);
    if (iFreeCmdInfo.empty())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::GetFreeCmdInfo unable to allocate cmd info!\n"));
        OSCL_LEAVE(PVMFErrNoMemory);
    }
    else
    {
        TPV2WayCmdInfo *cmd = (TPV2WayCmdInfo *)iFreeCmdInfo[0];
        iFreeCmdInfo.erase(iFreeCmdInfo.begin());
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::GetCmdInfoL - done\n"));
        return cmd;
    }

    LogFunction("GetCmdInfoL", LEAVE_FUNCTION);
    return NULL;
}

void CPV324m2Way::FreeCmdInfo(TPV2WayCmdInfo *info)
{
    LogFunction("FreeCmdInfo", START_FUNCTION);
    info->Clear();
    iFreeCmdInfo.push_back(info);
    LogFunction("FreeCmdInfo", LEAVE_FUNCTION);
}

TPV2WayEventInfo *CPV324m2Way::GetEventInfoL()
{
    LogFunction("GetEventInfoL", START_FUNCTION);
    if (iFreeEventInfo.empty())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::GetFreeEventInfo unable to allocate event info!\n"));
        OSCL_LEAVE(PVMFErrNoMemory);
    }
    else
    {
        TPV2WayEventInfo *cmd = (TPV2WayEventInfo *)iFreeEventInfo[0];
        iFreeEventInfo.erase(iFreeEventInfo.begin());
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::GetEventInfoL - done\n"));
        return cmd;
    }

    LogFunction("GetEventInfoL", LEAVE_FUNCTION);
    return NULL;
}

void CPV324m2Way::FreeEventInfo(TPV2WayEventInfo *info)
{
    LogFunction("FreeEventInfo", START_FUNCTION);
    info->Clear();
    iFreeEventInfo.push_back(info);
    LogFunction("FreeEventInfo", LEAVE_FUNCTION);
}

PVMFCommandId CPV324m2Way::SendNodeCmdL(PV2WayNodeCmdType aCmd,
                                        TPV2WayNode *aNode,
                                        MPV2WayNodeCommandObserver *aObserver,
                                        void *aParam,
                                        TPV2WayCmdInfo *a2WayCmdInfo)
{
    LogFunction("SendNodeCmdL", START_FUNCTION);
    int32 error = 0;
    PVMFCommandId id = 0;
    TPV2WayNodeCmdInfo *info;
    PVMFNodeInterface * nodeIFace = (PVMFNodeInterface *)aNode->iNode;
    PvmfNodesSyncControlInterface* ptr = NULL;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::SendNodeCmdL state %d, cmd %d, session %d\n",
                     iState, aCmd, aNode->iSessionId));

    if (aNode == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::SendNodeCmdL node ptr is null!\n"));
        OSCL_LEAVE(PVMFErrArgument);
    }

    if (iFreeNodeCmdInfo.empty())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::SendNodeCmdL unable to allocate node command info!\n"));
        OSCL_LEAVE(PVMFErrNoMemory);
    }

    info = (TPV2WayNodeCmdInfo *)iFreeNodeCmdInfo[0];
    iFreeNodeCmdInfo.erase(iFreeNodeCmdInfo.begin());

    info->type = aCmd;
    info->context.iObserver = aObserver;
    info->context.iNode = nodeIFace;
    info->engineCmdInfo = a2WayCmdInfo;

    PVMFSessionId sessionId = aNode->GetSessionId();

    switch (aCmd)
    {
        case PV2WAY_NODE_CMD_QUERY_INTERFACE:
            if (aParam != NULL)
            {
                TPV2WayNodeQueryInterfaceParams *queryParam =
                    (TPV2WayNodeQueryInterfaceParams *) aParam;
                OSCL_TRY(error, id = nodeIFace->QueryInterface(sessionId,
                                     *queryParam->iUuid, *queryParam->iInterfacePtr,
                                     (OsclAny *) & info->context));
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::SendNodeCmdL RequestPort param is null!\n"));
                error = PVMFErrArgument;
            }
            break;

        case PV2WAY_NODE_CMD_INIT:
            info->context.iContextData = aParam;
            OSCL_TRY(error, id = nodeIFace->Init(sessionId,
                                                 (OsclAny *) & info->context));
            break;

        case PV2WAY_NODE_CMD_REQUESTPORT:
            if (aParam != NULL)
            {
                OSCL_HeapString<OsclMemAllocator> mimeType;
                TPV2WayNodeRequestPortParams *params = (TPV2WayNodeRequestPortParams *) aParam;
                mimeType = params->format.getMIMEStrPtr();
                //Get mime string from format type
                OSCL_TRY(error, id = nodeIFace->RequestPort(sessionId,
                                     params->portTag, &mimeType, (OsclAny *) & info->context));
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::SendNodeCmdL RequestPort param is null!\n"));
                error = PVMFErrArgument;
            }
            break;

        case PV2WAY_NODE_CMD_PREPARE:
            OSCL_TRY(error, id = nodeIFace->Prepare(sessionId,
                                                    (OsclAny *) & info->context));
            break;

        case PV2WAY_NODE_CMD_START:
            OSCL_TRY(error, id = nodeIFace->Start(sessionId,
                                                  (OsclAny *) & info->context));
            break;

        case PV2WAY_NODE_CMD_PAUSE:
            OSCL_TRY(error, id = nodeIFace->Pause(sessionId,
                                                  (OsclAny *) & info->context));
            break;

        case PV2WAY_NODE_CMD_STOP:
            OSCL_TRY(error, id = nodeIFace->Stop(sessionId,
                                                 (OsclAny *) & info->context));
            break;

        case PV2WAY_NODE_CMD_RELEASEPORT:
            if (aParam != NULL)
            {
                OSCL_TRY(error, id = nodeIFace->ReleasePort(sessionId,
                                     *((PVMFPortInterface *) aParam), (OsclAny *) & info->context));
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::SendNodeCmdL ReleasePort param is null!\n"));
                error = PVMFErrArgument;
            }
            break;

        case PV2WAY_NODE_CMD_RESET:
            OSCL_TRY(error, id = nodeIFace->Reset(sessionId,
                                                  (OsclAny *) & info->context));
            break;

        case PV2WAY_NODE_CMD_CANCELCMD:
            if (aParam != NULL)
            {
                OSCL_TRY(error, id = nodeIFace->CancelCommand(sessionId,
                                     *((PVMFCommandId *) aParam), (OsclAny *) & info->context));
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "CPV324m2Way::SendNodeCmdL CancelCommand param is null!\n"));
                error = PVMFErrArgument;
            }

            //Remove commands on pending list
            if (!error)
            {
                RemovePendingNodeCmd((PVMFNodeInterface *)aNode,
                                     *((PVMFCommandId *) aParam));
            }
            break;

        case PV2WAY_NODE_CMD_CANCELALL:
            OSCL_TRY(error, id = nodeIFace->CancelAllCommands(sessionId,
                                 (OsclAny *) & info->context));

            //Remove commands on pending list
            if (!error)
            {
                RemovePendingNodeCmd((PVMFNodeInterface *)aNode, 0, true);
            }
            break;

        case PV2WAY_NODE_CMD_SKIP_MEDIA_DATA:
        {
            for (uint32 ii = 0; ii < iSinkNodeList.size(); ii++)
            {
                if ((aNode == iSinkNodeList[ii].iSinkNode)
                        && (iSinkNodeList[ii].iNodeInterface.iState ==
                            PV2WayNodeInterface::HasInterface))
                {
                    ptr = (PvmfNodesSyncControlInterface*)
                          iSinkNodeList[ii].iNodeInterface.iInterface;
                    if (ptr != NULL)
                    {
                        //Pause the clock, since this gives a chance to register
                        // the clock observer notifications
                        bool pauseAndResume = false;

                        if (iClock.GetState() == PVMFMediaClock::RUNNING)
                        {
                            pauseAndResume = true;
                            iClock.Pause();
                        }
                        bool pauseAndResumeMux = false;
                        if (iMuxClock.GetState() == PVMFMediaClock::RUNNING)
                        {
                            pauseAndResumeMux = true;
                            iMuxClock.Pause();
                        }
                        id = SkipMediaData(*ptr, aNode, *info);
                        if (pauseAndResume)
                        {
                            //Re-start the clock, since by now, the sink node and MIO component
                            // would've registered itself as the clock observer
                            // but only if the clock was already started.
                            iClock.Start();
                        }
                        if (pauseAndResumeMux)
                        {
                            iMuxClock.Start();
                        }
                    }
                    else
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "CPV324m2Way::SendNodeCmdL SkipMediaData param is null!\n"));
                        error = PVMFErrArgument;
                    }
                    break;
                }
            }
        }
        break;
        case PV2WAY_NODE_CMD_INVALID:
        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::SendNodeCmdL invalid command!\n"));
            OSCL_LEAVE(PVMFErrArgument);
            break;
    }

    if (error)
    {
        info->Clear();
        iFreeNodeCmdInfo.push_back(info);
        OSCL_LEAVE(error);
    }

    info->id = id;

    iPendingNodeCmdInfo.push_back(info);
    LogFunction("SendNodeCmdL", LEAVE_FUNCTION);
    return id;
}

int32 CPV324m2Way::SkipMediaData(PvmfNodesSyncControlInterface& aNodeSyncCtrl,
                                 TPV2WayNode* aNode,
                                 TPV2WayNodeCmdInfo& ainfo)
{
    LogFunction("SkipMediaData", START_FUNCTION);
    int32 error = 0;
    int32 id = 0;
    aNodeSyncCtrl.SetClock(&iClock);
    aNodeSyncCtrl.SetMargins(SYNC_EARLY_MARGIN, SYNC_LATE_MARGIN);
    OSCL_TRY(error, id =
                 aNodeSyncCtrl.SkipMediaData(aNode->iSessionId,
                                             resume_timestamp, STREAMID, false,
                                             (OsclAny *) & ainfo.context));
    LogFunction("SkipMediaData", LEAVE_FUNCTION);
    return id;
}

TPV2WayNodeCmdInfo *CPV324m2Way::FindPendingNodeCmd(PVMFNodeInterface *aNode,
        PVMFCommandId aId)
{
    LogFunction("FindPendingNodeCmd", START_FUNCTION);
    for (uint32 i = 0; i < iPendingNodeCmdInfo.size(); i++)
    {
        if ((iPendingNodeCmdInfo[i]->context.iNode == aNode) &&
                (iPendingNodeCmdInfo[i]->id == aId))
        {
            LogFunction("FindPendingNodeCmd", LEAVE_FUNCTION, PVMFSuccess);
            return iPendingNodeCmdInfo[i];
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                    (0, "CPV324m2Way::FindPendingNodeCmd unable to find command, node %x, id %d!\n", aNode, aId));
    LogFunction("FindPendingNodeCmd", LEAVE_FUNCTION, PVMFFailure);
    return NULL;
}

void CPV324m2Way::RemovePendingNodeCmd(PVMFNodeInterface *aNode,
                                       PVMFCommandId aId,
                                       bool aAllCmds)
{
    LogFunction("RemovePendingNodeCmd", START_FUNCTION);
    TPV2WayNodeCmdInfo **info = NULL;

    info = iPendingNodeCmdInfo.begin();
    while (info != iPendingNodeCmdInfo.end())
    {
        if (((*info)->context.iNode == aNode) &&
                (aAllCmds || ((*info)->id == aId)))
        {
            (*info)->Clear();
            iFreeNodeCmdInfo.push_back(*info);
            iPendingNodeCmdInfo.erase(info);
            info = iPendingNodeCmdInfo.begin();
            continue;
        }

        info++;
    }

    LogFunction("RemovePendingNodeCmd", LEAVE_FUNCTION);
}


void CPV324m2Way::FillSDKInfo(PVSDKInfo &aSDKInfo)
{
    aSDKInfo.iLabel = PV2WAY_ENGINE_SDKINFO_LABEL;
    aSDKInfo.iDate = PV2WAY_ENGINE_SDKINFO_DATE;
}

bool CPV324m2Way::CheckMandatoryCodecs(const PVMFFormatType *aMandatoryList,
                                       uint32 aMandatorySize,
                                       Oscl_Vector<PVMFFormatType, OsclMemAllocator> &aCodecList)
{
    LogFunction("CheckMandatoryCodecs", START_FUNCTION);
    uint32 i, j;
    bool found;

    if (aCodecList.empty())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                        (0, "CPV324m2Way::CheckMandatoryCodecs empty codecs list, use default\n"));
        return true;
    }

    for (i = 0; i < aMandatorySize; i++)
    {
        found = false;
        for (j = 0; j < aCodecList.size(); j++)
        {
            if (aMandatoryList[i] == aCodecList[j])
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::CheckMandatoryCodecs %s not found!",
                             (aMandatoryList[i]).getMIMEStrPtr()));
            return false;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                    (0, "CPV324m2Way::CheckMandatoryCodecs all codecs found\n"));
    LogFunction("CheckMandatoryCodecs", LEAVE_FUNCTION);
    return true;
}

void CPV324m2Way::InitiateSession(TPV2WayNode& aNode)
{
    LogFunction("InitiateSession", START_FUNCTION);
    PVMFNodeInterface * nodeIFace = (PVMFNodeInterface *)aNode ;
    PVMFNodeSessionInfo session(this, this, aNode, this, aNode);
    aNode.iSessionId =  nodeIFace->Connect(session);
    nodeIFace->ThreadLogon();
    LogFunction("InitiateSession", LEAVE_FUNCTION);
}

bool CPV324m2Way::IsNodeReset(PVMFNodeInterface& aNode)
{
    TPVMFNodeInterfaceState node_state = aNode.GetState();

    if (node_state == EPVMFNodeCreated || node_state == EPVMFNodeIdle)
        return true;
    return false;
}

///////////////////////////////////////////////////////////////////
// CPV324m2Way::SelectPreferredCodecs
// This function takes the selection of codecs from the MIOs/app
// and compares them to the set of codecs the 2way engine supports.
// it adds the appropriate codecs (the same codec if it finds an exact match,
// the corresponding codec if it finds a converter) to the iIncomingVideoCodecs,
// iOutgoingAudioCodecs, etc.
///////////////////////////////////////////////////////////////////
void CPV324m2Way::SelectPreferredCodecs(TPVDirection aDir,
                                        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aAppAudioFormats,
                                        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aAppVideoFormats)
{
    LogFunction("SelectPreferredCodecs", START_FUNCTION);
    // Iterate over formats supported by the stack
    Oscl_Map < PVMFFormatType, CPvtMediaCapability*, OsclMemAllocator,
    pvmf_format_type_key_compare_class >::iterator it = iStackSupportedFormats.begin();
    // go through the stack's supported codecs (those that it can process the protocol for)
    while (it != iStackSupportedFormats.end())
    {
        CPvtMediaCapability* media_capability = (*it++).second;
        PVMFFormatType stackFormatType = media_capability->GetFormatType();
        // Is stack preferred format present in application formats ?
        const char* format_str = NULL;
        PVMFFormatType format = FindFormatType(stackFormatType,
                                               aAppAudioFormats, aAppVideoFormats);
        if (format != PVMF_MIME_FORMAT_UNKNOWN)
        {
            // the stack-supported format is found within the MIO-specified codecs
            format_str = format.getMIMEStrPtr();
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::SelectPreferredCodecs ***** format was found: %s ******",
                             stackFormatType.getMIMEStrPtr()));

            // no conversion necessary- stack supports format given by application/MIOs
            DoSelectFormat(aDir, stackFormatType, format_str, APP);
        }
        else
        {
            PV2WayMediaType media_type = ::GetMediaType(PVMFFormatTypeToPVCodecType(stackFormatType));
            const char* can_convert_format = NULL;

            if (media_type == PV_AUDIO)
            {
                // can the engine convert between the stack preferred codec and any of the application
                // formats?
                can_convert_format = CanConvertFormat(aDir, stackFormatType, aAppAudioFormats);
            }
            else if (media_type == PV_VIDEO)
            {
                can_convert_format = CanConvertFormat(aDir, stackFormatType, aAppVideoFormats);
            }

            if (can_convert_format)
            {
                // Engine can convert the format using a conversion node
                // so select it in a list
                DoSelectFormat(aDir, stackFormatType, format_str, ENG, can_convert_format);
            }
            else
            {
                // Check if it is a mandatory codec - if it is and it got
                // here then it isn't supported but it needs to be
                if (media_capability->IsMandatory())
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "CPV324m2Way::SelectPreferredCodecs, ERROR Mandatory stack codec=%s not supported",
                                     stackFormatType.getMIMEStrPtr()));
                    OSCL_LEAVE(PVMFErrResource);
                }
            }
        }
    }
    LogFunction("SelectPreferredCodecs", LEAVE_FUNCTION);
}

///////////////////////////////////////////////////////////////////
// CPV324m2Way::SetPreferredCodecs
// This function takes the selection of codecs from the MIOs/app
// and uses these to select the appropriate codecs for the stack node
// it also sets these as the expected codecs in the channels.
//
///////////////////////////////////////////////////////////////////
void CPV324m2Way::SetPreferredCodecs(PV2WayInitInfo& aInitInfo)
{
    LogFunction("SetPreferredCodecs", START_FUNCTION);
    ////////////////////////////////////////////////////////////////

    // given aInitInfo from app, match up with the stack preferred codecs and engine codecs
    // side effect is that iIncomingVideoCodecs, etc are set.
    SelectPreferredCodecs(OUTGOING, aInitInfo.iOutgoingAudioFormats, aInitInfo.iOutgoingVideoFormats);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::SetPreferredCodecs: Incoming\n"));
    SelectPreferredCodecs(INCOMING, aInitInfo.iIncomingAudioFormats, aInitInfo.iIncomingVideoFormats);

    // now that iIncomingVideoCodecs, etc are selected (codecs the stack, engine and app want)
    // take those values and add them to the iIncomingChannelParams, iOutgoingChannelParams
    // so the channel will know which formats are acceptable.
    H324ChannelParameters inDtmfParams(0);
    inDtmfParams.SetCodecs(iIncomingUserInputFormats);

    //Set incoming channel capabilities.
    // TBD: Incoming capabilities need to be queried from the registry and passed to the stack
    H324ChannelParameters inAudioChannelParams(MAX_AUDIO_BITRATE);
    // add iIncomingAudioCodecs members into iFormatCapability
    ConvertMapToVector(iIncomingAudioCodecs, iFormatCapability);
    inAudioChannelParams.SetCodecs(iFormatCapability);

    H324ChannelParameters inVideoChannelParams(MAX_VIDEO_BITRATE);
    ConvertMapToVector(iIncomingVideoCodecs, iFormatCapability);
    inVideoChannelParams.SetCodecs(iFormatCapability);

    iIncomingChannelParams.push_back(inDtmfParams);
    iIncomingChannelParams.push_back(inAudioChannelParams);
    iIncomingChannelParams.push_back(inVideoChannelParams);


    //Set outgoing channel capabilities.
    H324ChannelParameters outAudioChannelParams(MAX_AUDIO_BITRATE);
    ConvertMapToVector(iOutgoingAudioCodecs, iFormatCapability);
    outAudioChannelParams.SetCodecs(iFormatCapability);

    H324ChannelParameters outVideoChannelParams(MAX_VIDEO_BITRATE);
    ConvertMapToVector(iOutgoingVideoCodecs, iFormatCapability);
    outVideoChannelParams.SetCodecs(iFormatCapability);

    iOutgoingChannelParams.push_back(outAudioChannelParams);
    iOutgoingChannelParams.push_back(outVideoChannelParams);
    ////////////////////////////////////////////////////////////////
    LogFunction("SetPreferredCodecs", LEAVE_FUNCTION);

}


bool CPV324m2Way::Supports(PVMFNodeCapability &capability,
                           PVMFFormatType aFormat,
                           bool isInput/*=true*/)
{
    if (isInput)
    {
        for (uint16 i = 0; i < capability.iInputFormatCapability.size(); i++)
        {
            if (capability.iInputFormatCapability[i] == aFormat)
                return true;
        }
    }
    else
    {
        for (uint16 i = 0; i < capability.iOutputFormatCapability.size(); i++)
        {
            if (capability.iOutputFormatCapability[i] == aFormat)
                return true;
        }
    }

    return false;
}

int32 CPV324m2Way::GetStackNodePortTag(TPV2WayPortTagType tagType)
{
    switch (tagType)
    {

        case EPV2WayVideoIn:
            if (iTerminalType == PV_324M)
            {
                return PV_VIDEO;
            }
            else
            {
                return -1;
            }
            break;

        case EPV2WayAudioIn:
            if (iTerminalType == PV_324M)
            {
                return PV_AUDIO;
            }
            else
            {
                return -1;
            }
            break;

        case EPV2WayVideoOut:
            if (iTerminalType == PV_324M)
            {
                return iVideoDecDatapath->GetTSCPortTag();
            }
            else
            {
                return -1;
            }
            break;

        case EPV2WayAudioOut:
            if (iTerminalType == PV_324M)
            {
                return iAudioDecDatapath->GetTSCPortTag();
            }
            else
            {
                return -1;
            }
            break;

        default:
            break;

    }
    return -1;
}


bool CPV324m2Way::AllChannelsOpened()
{
    ////////////////////////////////////////////////////////////////

    bool retval = ((iIncomingAudioTrackTag != INVALID_TRACK_ID ||
                    !iIncomingAudioCodecs.size()) &&
                   (iIncomingVideoTrackTag != INVALID_TRACK_ID ||
                    !iIncomingVideoCodecs.size()) &&
                   (iOutgoingAudioTrackTag != INVALID_TRACK_ID ||
                    !iOutgoingAudioCodecs.size()) &&
                   (iOutgoingVideoTrackTag != INVALID_TRACK_ID ||
                    !iOutgoingVideoCodecs.size()));
    ////////////////////////////////////////////////////////////////
    return retval;
}


void CPV324m2Way::ConvertMapToVector(Oscl_Map < PVMFFormatType,
                                     FormatCapabilityInfo,
                                     OsclMemAllocator,
                                     pvmf_format_type_key_compare_class > & aCodecs,
                                     Oscl_Vector < FormatCapabilityInfo,
                                     OsclMemAllocator > & aFormatCapability)
{
    LogFunction("ConvertMapToVector", START_FUNCTION);
    // add aCodecs members into aFormatCapability
    aFormatCapability.clear();
    Oscl_Map < PVMFFormatType, FormatCapabilityInfo, OsclMemAllocator,
    pvmf_format_type_key_compare_class >::iterator it;
    it = aCodecs.begin();
    for (it = aCodecs.begin() ; it != aCodecs.end(); it++)
    {
        aFormatCapability.push_back(aCodecs[(*it).first]);
    }
    LogFunction("ConvertMapToVector", LEAVE_FUNCTION);
}


void CPV324m2Way::AddVideoEncoderNode()
{
    LogFunction("AddVideoEncoderNode", START_FUNCTION);

    if (iVideoEncNode != NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::AddVideoEncoderNode - done (not adding)\n"));
        LogFunction("AddVideoEncoderNode", LEAVE_FUNCTION, PVMFFailure);
        return;
    }
    int32 error = 0;
    iVideoEncNode = TPV2WayNode(CREATE_OMX_ENC_NODE());

    if (iVideoEncNode.iNode == NULL)
        OSCL_LEAVE(PVMFErrNoMemory);
    InitiateSession(iVideoEncNode);

    if (iVideoEncNodeInterface.iState == PV2WayNodeInterface::NoInterface)
    {
        TPV2WayNodeQueryInterfaceParams queryParam;
        queryParam.iInterfacePtr = &iVideoEncNodeInterface.iInterface;

        queryParam.iUuid = (PVUuid *) & iVideoEncPVUuid;

        OSCL_TRY(error, iVideoEncQueryIntCmdId = SendNodeCmdL(PV2WAY_NODE_CMD_QUERY_INTERFACE,
                 &iVideoEncNode, this, &queryParam));
        OSCL_FIRST_CATCH_ANY(error,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::AddVideoEncoderNode unable to query for video encoder interface!\n"));
                             SetState(EResetting);
                             CheckState();
                             LogFunction("AddVideoEncoderNode", LEAVE_FUNCTION, PVMFFailure);
                             return;);

        iVideoEncNodeInterface.iState = PV2WayNodeInterface::QueryInterface;

        queryParam.iInterfacePtr = &ipEncNodeCapConfigInterface;
        queryParam.iUuid = (PVUuid *) & iCapConfigPVUuid;

        iOmxEncQueryIntCmdId = -1;
        ipEncNodeCapConfigInterface = NULL;

        OSCL_TRY(error, iOmxEncQueryIntCmdId = SendNodeCmdL(PV2WAY_NODE_CMD_QUERY_INTERFACE,
                                               &iVideoEncNode, this, &queryParam));
        OSCL_FIRST_CATCH_ANY(error,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::AddVideoEncoderNode unable to query for video encoder c&c interface!\n"));
                             ipEncNodeCapabilityAndConfig = NULL;
                             LogFunction("AddVideoEncoderNode", LEAVE_FUNCTION, PVMFFailure);
                             return;);


    }

    LogFunction("AddVideoEncoderNode", LEAVE_FUNCTION);

}
void CPV324m2Way::AddAudioEncoderNode()
{
    LogFunction("AddAudioEncoderNode", START_FUNCTION);


    if (iAudioEncNode != NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::AddAudioEncoderNode - done (not adding)\n"));
        LogFunction("AddAudioEncoderNode", LEAVE_FUNCTION, PVMFFailure);
        return;
    }
    int32 error = 0;

    OSCL_TRY(error, iAudioEncNode = TPV2WayNode(CREATE_OMX_ENC_NODE()));
    OSCL_FIRST_CATCH_ANY(error, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,
                         iLogger, PVLOGMSG_ERR,
                         (0, "CPV324m2Way::AddAudioEncoderNode unable to allocate audio encoder node\n")));

    InitiateSession(iAudioEncNode);

    if (iAudioEncNodeInterface.iState == PV2WayNodeInterface::NoInterface)
    {
        TPV2WayNodeQueryInterfaceParams queryParam;
        queryParam.iInterfacePtr = &iAudioEncNodeInterface.iInterface;

        queryParam.iUuid = (PVUuid *) & iAudioEncPVUuid;

        OSCL_TRY(error, iAudioEncNodeInterface.iId =
                     SendNodeCmdL(PV2WAY_NODE_CMD_QUERY_INTERFACE,
                                  &iAudioEncNode, this, &queryParam));
        OSCL_FIRST_CATCH_ANY(error,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,
                                             iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::AddAudioEncoderNode unable to query for audio encoder interface!\n"));
                             SetState(EResetting);
                             CheckState();
                             LogFunction("AddAudioEncoderNode", LEAVE_FUNCTION, PVMFFailure);
                             return;);

        iAudioEncNodeInterface.iState = PV2WayNodeInterface::QueryInterface;
    }
    else if ((iAudioEncNode.iNode)->GetState() == EPVMFNodeError)
    {
        OSCL_TRY(error, SendNodeCmdL(PV2WAY_NODE_CMD_RESET, &iAudioEncNode, this));
        OSCL_FIRST_CATCH_ANY(error,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                             (0, "CPV324m2Way::AddAudioEncoderNode unable to reset audio encoder node after error!\n"));
                             LogFunction("AddAudioEncoderNode", LEAVE_FUNCTION, PVMFFailure);
                             return;);
    }


    // start enc datapaths that are already created
    if (iAudioEncDatapath->GetState() != EClosed)
    {
        iAudioEncDatapath->CheckOpen();
    }
    LogFunction("AddAudioEncoderNode", LEAVE_FUNCTION);

}
void CPV324m2Way::AddVideoDecoderNode()
{
    LogFunction("AddVideoDecoderNode", START_FUNCTION);
    if (iVideoDecNode != NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::AddVideoDecoderNode - done (not adding)\n"));
        LogFunction("AddVideoDecoderNode", LEAVE_FUNCTION, PVMFFailure);
        return;
    }
    int32 error = 0;

    OSCL_TRY(error, iVideoDecNode = TPV2WayNode(CREATE_OMX_VIDEO_DEC_NODE()););

    OSCL_FIRST_CATCH_ANY(error, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                         PVLOGMSG_ERR, (0, "CPV324m2Way::AddVideoDecoderNode unable to allocate video decoder node\n")));

    InitiateSession(iVideoDecNode);

    LogFunction("AddVideoDecoderNode", LEAVE_FUNCTION);
}

void CPV324m2Way::AddAudioDecoderNode()
{
    LogFunction("AddAudioDecoderNode", START_FUNCTION);

    if (iAudioDecNode != NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "CPV324m2Way::AddAudioDecoderNode - done (not adding)\n"));
        LogFunction("AddAudioDecoderNode", LEAVE_FUNCTION, PVMFFailure);
        return;
    }
    int32 error = 0;
    error = 0;

    OSCL_TRY(error, iAudioDecNode =
                 TPV2WayNode(CREATE_OMX_AUDIO_DEC_NODE()););

    OSCL_FIRST_CATCH_ANY(error, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger,
                         PVLOGMSG_ERR, (0, "CPV324m2Way::AddAudioDecoderNode unable to allocate audio decoder node\n")));

    InitiateSession(iAudioDecNode);
    LogFunction("AddAudioDecoderNode", LEAVE_FUNCTION);
}

PVMFStatus CPV324m2Way::ConfigureVideoEncoderNode()
{
    LogFunction("ConfigureVideoEncoderNode", START_FUNCTION);
    CPvtVideoCapability* pMediaCapability = NULL;
    Oscl_Vector<FormatCapabilityInfo, OsclMemAllocator> formats;
    PVMFVideoResolution* pVideoResolution = NULL;
    uint32 framerate = 0;
    uint32 bitrate = 0;
    uint32 fsiSize = 0;
    uint8* fsi = NULL;

    PVMFFormatType formatType = iVideoEncDatapath->GetFormat();

    // Get default settings for video encoder
    iTSC324mInterface->GetChannelFormatAndCapabilities(OUTGOING, formats);

    for (uint16 index = 0; index < formats.size(); index++)
    {
        if (formatType == formats[index].format)
        {
            bitrate = formats[index].bitrate;
            formats[index].GetFormatSpecificInfo(&fsi, fsiSize);
            pMediaCapability = OSCL_STATIC_CAST(CPvtVideoCapability*, iTSC324mInterface->GetRemoteCodecCapability(formats[index]));
            framerate = pMediaCapability->iFrameRate;
            pVideoResolution = pMediaCapability->iVideoResolution;
        }
    }

    if (!bitrate)
    {
        // makes no sense to continue if bitrate is zero
        return PVMFFailure;
    }

    PVVideoEncExtensionInterface *ptr =
        (PVVideoEncExtensionInterface *) iVideoEncNodeInterface.iInterface;

    if (ptr)
    {
        // give settings to encoder using video extension interface
        ptr->SetNumLayers(1);
        ptr->SetOutputBitRate(0, bitrate);
        ptr->SetOutputFrameSize(0, pVideoResolution->width, pVideoResolution->height);
        ptr->SetOutputFrameRate(0, OSCL_STATIC_CAST(OsclFloat, framerate));

        ptr->SetSegmentTargetSize(0, VIDEO_ENCODER_SEGMENT_SIZE);
        ptr->SetRateControlType(0, VIDEO_ENCODER_RATE_CONTROL);
        ptr->SetDataPartitioning(VIDEO_ENCODER_DATA_PARTITIONING);
        ptr->SetRVLC(VIDEO_ENCODER_RVLC);
        ptr->SetIFrameInterval(VIDEO_ENCODER_I_FRAME_INTERVAL);
    }



    if (fsiSize)
    {
        OsclMemAllocator alloc;

        PvmiKvp kvp;
        kvp.length = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY) + 1;
        kvp.key = (PvmiKeyType)alloc.ALLOCATE(kvp.length);

        oscl_strncpy(kvp.key, PVMF_FORMAT_SPECIFIC_INFO_KEY, kvp.length);

        kvp.value.key_specific_value = (OsclAny*)fsi;
        kvp.capacity = fsiSize;

        PvmiKvp* retKvp = NULL; // for return value
        int32 err1;

        OSCL_TRY(err1, ipEncNodeCapabilityAndConfig->setParametersSync(NULL, &kvp, 1, retKvp););

        alloc.deallocate((OsclAny*)(kvp.key));

        if ((retKvp != NULL) || (err1 != OsclErrNone))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                            (0, "CPV324m2Way::ConfigureNode: Error - Configuring enc node via cap-config IF failed"));


            return PVMFFailure;
        }
    }
    LogFunction("ConfigureVideoEncoderNode", LEAVE_FUNCTION);
    return PVMFSuccess;
}

#ifdef MEM_TRACK
void CPV324m2Way::MemStats()
{
#if !(OSCL_BYPASS_MEMMGT)

    OsclAuditCB auditCB;
    OsclMemInit(auditCB);
    if (auditCB.pAudit)
    {
        MM_Stats_t* stats = auditCB.pAudit->MM_GetStats("");
        if (stats)
        {
            printf("  numBytes %d\n", stats->numBytes);
            printf("  peakNumBytes %d\n", stats->peakNumBytes);
            printf("  numAllocs %d\n", stats->numAllocs);
            printf("  peakNumAllocs %d\n", stats->peakNumAllocs);
            printf("  numAllocFails %d\n", stats->numAllocFails);
            printf("  totalNumAllocs %d\n", stats->totalNumAllocs);
            printf("  totalNumBytes %d\n", stats->totalNumBytes);
        }

    }
#endif
}
#endif

/* This should be changed to query the node registry */
bool CPV324m2Way::IsSupported(const PVMFFormatType& aInputFmtType, const PVMFFormatType& aOutputFmtType)
{
    if (aInputFmtType == PVMF_MIME_AMR_IF2)
    {
        if ((aOutputFmtType == PVMF_MIME_PCM8) ||
                (aOutputFmtType == PVMF_MIME_PCM16))
        {
            return true;
        }
        return false;
    }
    else if ((aInputFmtType ==  PVMF_MIME_M4V) ||
             (aInputFmtType ==  PVMF_MIME_H2632000) ||
             (aInputFmtType ==  PVMF_MIME_H264_VIDEO_RAW))
    {
        if (aOutputFmtType == PVMF_MIME_YUV420)
        {
            return true;
        }
        return false;
    }
    else if ((aInputFmtType ==  PVMF_MIME_PCM8) ||
             (aInputFmtType ==  PVMF_MIME_PCM16))
    {
        if (aOutputFmtType == PVMF_MIME_AMR_IF2)
        {
            return true;
        }
        return false;
    }
    else if ((aInputFmtType ==  PVMF_MIME_YUV420))
    {
        if (aOutputFmtType == PVMF_MIME_M4V ||
                aOutputFmtType == PVMF_MIME_H2632000 ||
                (aOutputFmtType ==  PVMF_MIME_H264_VIDEO_RAW))
        {
            return true;
        }
        return false;
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::IsSupported() ERROR unsupported conversion: input: %s output: %s\n",
                         aInputFmtType.getMIMEStrPtr(), aOutputFmtType.getMIMEStrPtr()));
        return false;
    }
}

/* This should be changed to query the formats from the stack */
void CPV324m2Way::GetStackSupportedFormats()
{
    LogFunction("GetStackSupportedFormats", START_FUNCTION);
    iStackSupportedFormats[PVMF_MIME_AMR_IF2] = OSCL_NEW(CPvtAudioCapability, (PVMF_MIME_AMR_IF2, MAX_AMR_BITRATE));
    iStackSupportedFormats[PVMF_MIME_H264_VIDEO_RAW] = OSCL_NEW(CPvtAvcCapability, (MAX_VIDEO_BITRATE));
    iStackSupportedFormats[PVMF_MIME_M4V] = OSCL_NEW(CPvtMpeg4Capability, (MAX_VIDEO_BITRATE));
    iStackSupportedFormats[PVMF_MIME_H2632000] = OSCL_NEW(CPvtH263Capability, (MAX_VIDEO_BITRATE));
    LogFunction("GetStackSupportedFormats", LEAVE_FUNCTION);
}


PVMFFormatType CPV324m2Way::FindFormatType(PVMFFormatType aFormatType,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aAudioFormats,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aVideoFormats)
{
    LogFunction("FindFormatType", START_FUNCTION);
    uint32 i = 0;

    for (i = 0; i < aAudioFormats.size(); i++)
    {
        PVMFFormatType temp = aAudioFormats[i];
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::FindFormatType ***** aFormat %s aAudioFormat %s ******",
                         aFormatType.getMIMEStrPtr(), aAudioFormats[i].getMIMEStrPtr()));
        if (aAudioFormats[i] == aFormatType)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "CPV324m2Way::FindFormatType - done (found audio)\n"));
            LogFunction("FindFormatType", LEAVE_FUNCTION);
            return aAudioFormats[i];
        }
    }

    for (i = 0; i < aVideoFormats.size(); i++)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "CPV324m2Way::FindFormatType ***** aFormat %s aVideoFormat %s ******",
                         aFormatType.getMIMEStrPtr(), aVideoFormats[i].getMIMEStrPtr()));
        if (aVideoFormats[i] == aFormatType)
        {
            LogFunction("FindFormatType", LEAVE_FUNCTION, PVMFSuccess);
            return aVideoFormats[i];
        }
    }
    LogFunction("FindFormatType", LEAVE_FUNCTION, PVMFFailure);
    return PVMF_MIME_FORMAT_UNKNOWN;
}

const char* CPV324m2Way::CanConvertFormat(TPVDirection aDir,
        PVMFFormatType aThisFmtType,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aThatFormatList)
{
    LogFunction("CanConvertFormat", START_FUNCTION);
    PVMFFormatType aInputFmtType = PVMF_MIME_FORMAT_UNKNOWN;
    PVMFFormatType aOutputFmtType = PVMF_MIME_FORMAT_UNKNOWN;

    OSCL_ASSERT(aDir == INCOMING || aDir == OUTGOING);

    for (uint32 i = 0; i < aThatFormatList.size(); i++)
    {
        PVMFFormatType thatFmtType = aThatFormatList[i];
        aInputFmtType = (aDir == INCOMING) ? aThisFmtType : thatFmtType;
        aOutputFmtType = (aDir == INCOMING) ? thatFmtType : aThisFmtType;
        if (IsSupported(aInputFmtType, aOutputFmtType))
        {
            LogFunction("CanConvertFormat", LEAVE_FUNCTION, PVMFSuccess);
            return thatFmtType.getMIMEStrPtr();
        }
    }
    LogFunction("CanConvertFormat", LEAVE_FUNCTION, PVMFFailure);
    return NULL;
}


//////////////////////////////////////////////////////
// Function: DoSelectFormat
// DoSelectFormat will add the new format to the appropriate
// codec map (iOutgoingAudioCodecs, iIncomingVideoCodecs, etc)
// And it will add whether this codec is serviced by the APP
// or the stack node (ENG)
//
//////////////////////////////////////////////////////
void CPV324m2Way::DoSelectFormat(TPVDirection aDir,
                                 PVMFFormatType aFormatType,
                                 const char* aFormatStr,
                                 TPVPriority aPriority,
                                 PVMFFormatType aFormatTypeApp)
{
    LogFunction("DoSelectFormat", START_FUNCTION);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "CPV324m2Way::DoSelectFormat, aDir=%d, aFormatType=%s, aPriority=%d",
                     aDir, aFormatType.getMIMEStrPtr(), aPriority));


    // select the appropriate lists for the engine formats,
    // and for the app formats.
    Oscl_Map < PVMFFormatType, FormatCapabilityInfo,
    OsclMemAllocator, pvmf_format_type_key_compare_class > * the_engine_map = NULL;
    Oscl_Map < PVMFFormatType, PVMFFormatType,
    OsclMemAllocator, pvmf_format_type_key_compare_class > * the_app_map = NULL;
    PV2WayMediaType media_type = GetMediaType(PVMFFormatTypeToPVCodecType(aFormatType));
    switch (aDir)
    {
        case OUTGOING:
            the_engine_map = (media_type == PV_AUDIO) ? &iOutgoingAudioCodecs : &iOutgoingVideoCodecs;
            the_app_map = &iAppFormatForEngineFormatOutgoing;
            break;
        case INCOMING:
            the_engine_map = (media_type == PV_AUDIO) ? &iIncomingAudioCodecs : &iIncomingVideoCodecs;
            the_app_map = &iAppFormatForEngineFormatIncoming;
            break;
        default:
            LogFunction("DoSelectFormat", LEAVE_FUNCTION, PVMFFailure);
            return;
    }

    FormatCapabilityInfo format_cap_info;
    format_cap_info.dir = aDir;
    // fills format_cap with info from aFormatType
    FILL_FORMAT_INFO(aFormatType, format_cap_info);
    format_cap_info.iPriority = aPriority;

    // set the map with information about the format information
    // this is where iIncoming/Outgoing,Audio/Video Codecs gets set.
    (*the_engine_map)[aFormatType] = format_cap_info;
    // set whether app directly supports (APP) or
    // the engine can convert using a conversion node (ENG)
    (*the_app_map)[aFormatType] = aFormatTypeApp;

    LogFunction("DoSelectFormat", LEAVE_FUNCTION);

}

