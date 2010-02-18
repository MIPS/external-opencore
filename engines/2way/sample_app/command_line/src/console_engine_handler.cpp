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

#include "console_engine_handler.h"
#include "oscl_thread.h"
#include "oscl_mutex.h"
#include "pv_2way_engine_factory.h"
#include "pvmf_fileoutput_factory.h"
#include "pv_2way_interface.h"

#ifdef SUPPORT_ISDN
#include "pvmf_capi_isdn_node.h"
#endif

#include "tsc_h324m_config_interface.h"
#include "pvlogger.h"
#include "pvlogger_time_and_id_layout.h"
#include "pv_engine_observer.h"
#include "pv_engine_observer_message.h"
#include "pv2way_file_names.h"


#ifndef PVMF_MEDIA_INPUT_NODE_FACTORY_H_INCLUDED
#include "pvmf_media_input_node_factory.h"
#endif

#ifndef PVMI_MIO_FILEINPUT_FACTORY_H_INCLUDED
#include "pvmi_mio_fileinput_factory.h"
#endif

#ifndef PVLOGGER_CFG_FILE_PARSER_H_INCLUDED
#include "pvlogger_cfg_file_parser.h"
#endif

#ifndef TEST_UTILITY_H_HEADER
#include "test_utility.h"
#endif
#include "main.h"



// H324 configuration
#define H223_LEVEL H223_LEVEL2
#define H223_MAX_AL1_SDU_SIZE 512
#define H223_MAX_AL2_SDU_SIZE 512
#define H223_MAX_AL3_SDU_SIZE 512
#define H223_MAX_AL1_SDU_SIZE_R 512
#define H223_MAX_AL2_SDU_SIZE_R 512
#define H223_MAX_AL3_SDU_SIZE_R 512
#define H223_AL2_SN_WIDTH 1
#define H223_AL3_CFO 0 // 0 - 2
#define H223_MAX_PDU_SIZE 160
#define H324_TERMINAL_TYPE 128
#define H324_ALLOW_AUDIO_AL1 false
#define H324_ALLOW_AUDIO_AL2 true
#define H324_ALLOW_AUDIO_AL3 false
#define H324_ALLOW_VIDEO_AL1 false
#define H324_ALLOW_VIDEO_AL2 true
#define H324_ALLOW_VIDEO_AL3 true
#define H324_USE_VIDEO_AL1 true
#define H324_USE_VIDEO_AL2 true
#define H324_USE_VIDEO_AL3 true
#define H324_MAX_PDU_SIZE_R 160
#define H324_MAX_CCSRL_SDU_SIZE 256

#define LAYOUT_BUFFER_SIZE 1024
#define TEXT_FILE_APPENDER_CACHE_SIZE LAYOUT_BUFFER_SIZE * 512

#define DISCONNECT_TIME_FOR_LOOPBACK_CALL 9000000
#define RUN_IF_NOT_READY_TIME 800000
#define CONFIG_FILE_PATH _STRLIT("")

engine_handler::engine_handler() : OsclTimerObject(OsclActiveObject::EPriorityNominal, "2Way Wins Test"),
        iUseIsdn(false),
        iUseSockets(false),
        iRunOnce(false),
        iAutomatedCall(false),
        iLoopbackCall(false),
        iDisconnectCall(false),
        iDisconnected(false),
        iRemoveSourceAndSinks(false),
        iInitCalled(false),
        iTestSettings(NULL),
        iCommServer(NULL),
        iCommServerType(NO_SERVER_TYPE_SET),
        iCommServerIOControl(NULL),
        engineExited(false),
        iH324ConfigInterface(NULL),
        iStdErrAppender(NULL),
        iFileAppender(NULL),
        terminal(NULL),
        scheduler(NULL),
        iComponentInterface(NULL),
        iEnableWnsrp(false),
        iRstCmdId(-1),
        iDisCmdId(-1),
        iConnectCmdId(-1),
        iInitCmdId(-1),
        i324mIFCommandId(-1),
        iRemoveAudioSinkID(-1),
        iRemoveAudioSourceID(-1),
        iRemoveVideoSinkID(-1),
        iRemoveVideoSourceID(-1),
        iAllowVideoOverAl2(true),
        iAllowVideoOverAl3(true),
        iUseVideoOverAl2(true),
        iUseVideoOverAl3(true),
        iSourceAndSinks(NULL),
        iTwoWaySocket(this),
        iSentDisconnectCmd(false),
        iPv2wayCommConfig(NULL),
        iConfigFilePresent(false),
        iReadyToConnectCommunication(false)

{


    iSourceAndSinks = OSCL_NEW(PV2WayConsoleSourceAndSinks, (iSdkInitInfo));
    iPv2wayCommConfig = OSCL_NEW(Pv2wayCommConfig, ());
}

engine_handler::~engine_handler()
{
    Cleanup();
    OSCL_DELETE(iSourceAndSinks);

    if (iStdErrAppender)
    {
        OSCL_DELETE(iStdErrAppender);
    }

    if (iTestSettings)
    {
        OSCL_DELETE(iTestSettings);
        iTestSettings = NULL;
    }

    if (iPv2wayCommConfig)
    {
        if (iPv2wayCommConfig->iConnectionType)
        {
            OSCL_ARRAY_DELETE(iPv2wayCommConfig->iConnectionType);
            iPv2wayCommConfig->iConnectionType = NULL;
        }

        OSCL_DELETE(iPv2wayCommConfig);
        iPv2wayCommConfig = NULL;
    }

    if (iComponentInterface)
    {
        iComponentInterface->removeRef();
        iComponentInterface = NULL;
    }

    if (terminal)
    {
        CPV2WayEngineFactory::DeleteTerminal(terminal);
        terminal = NULL;
    }
    if (iLoopbackCall)
    {
        MemoryStats();
    }
}


void engine_handler::CreateComponent()
{
    if (iH324ConfigInterface)
    {
        iH324ConfigInterface->removeRef();
        iH324ConfigInterface = NULL;
    }

    int32 error = 0;
    OSCL_TRY(error, i324mIFCommandId = terminal->QueryInterface(PVH324MConfigUuid,
                                       iH324ConfigInterface));
    if (error != 0)
    {
        PV2WayUtil::OutputInfo("Error in CreateComponent\n");
    }

}

void engine_handler::Init()
{
    iInitCalled = true;
    CreateComms();
    InitTerminal();
}

void engine_handler::InitTerminal()
{

    iSourceAndSinks->Init();


    int32 error = 0;
    OSCL_TRY(error, iInitCmdId = terminal->Init(iSdkInitInfo));
    if (error != 0)
    {
        PV2WayUtil::OutputInfo("Error in Init for terminal\n");
    }
}


void engine_handler::start()
{
    int32 error = 0;
    OSCL_TRY(error, terminal = CPV2WayEngineFactory::CreateTerminal(PV_324M,
                               (PVCommandStatusObserver *) this,
                               (PVInformationalEventObserver *) this,
                               (PVErrorEventObserver *) this));

    if (error != 0)
    {
        PV2WayUtil::OutputInfo("Error in CreateTerminal\n");
        OSCL_LEAVE(error);
    }

    InitializeLogs();
    iSourceAndSinks->SetTerminal(terminal);
    iSourceAndSinks->CreateCodecs();

    this->AddToScheduler();
    this->RunIfNotReady();

    scheduler = OsclExecScheduler::Current();
    OSCL_TRY(error, scheduler->StartScheduler());
    if (error != 0)
    {
        PV2WayUtil::OutputInfo("Error Starting Scheduler\n");
        OSCL_LEAVE(error);
    }

    this->DoCancel();
    this->RemoveFromScheduler();

    Cleanup();
}

void engine_handler::Cleanup()
{
    iSourceAndSinks->Cleanup();
    DeleteCommServer();
}

void engine_handler::DeleteCommServer()
{
    if (iCommServer)
    {
        switch (iCommServerType)
        {
            case PVMF_CAPI_ISDN_COMM_SERVER:
                // there is no corresponding delete for PVMFCapiIsdnNode
                OSCL_DELETE(iCommServer);
                break;
            case PVMF_LOOPBACK_COMM_SERVER:
                PVCommsIONodeFactory::Delete(iCommServer);
                if (iCommServerIOControl)
                {
                    PvmiMIOCommLoopbackFactory::Delete(iCommServerIOControl);
                    iCommServerIOControl = NULL;
                }
                break;
            case SOCKET_COMM_SERVER:
                iTwoWaySocket.DeleteCommServer();
                break;

            case NO_SERVER_TYPE_SET: // do the same with none set and some weird value
            default:
                OSCL_DELETE(iCommServer);
                break;
        }
        iCommServer = NULL;
    }
    iCommServerType = NO_SERVER_TYPE_SET;
}

void engine_handler::InitializeLogs()
{
    OSCL_HeapString<OsclMemAllocator> cfgfilename(PVLOG_PREPEND_CFG_FILENAME);
    cfgfilename += PVLOG_CFG_FILENAME;
    OSCL_HeapString<OsclMemAllocator> logfilename(PVLOG_PREPEND_OUT_FILENAME);
    logfilename += PVLOG_OUT_FILENAME;
    if (true == PVLoggerCfgFileParser::Parse(cfgfilename.get_str(), logfilename.get_str()))
        return;

    PVLoggerCfgFileParser::SetupLogAppender(PVLoggerCfgFileParser::ePVLOG_APPENDER_FILE, logfilename.get_str());

    // disable most logging from the rcomm server, it is too much
    terminal->SetLogLevel("mpvrcommserver", PVLOGMSG_EMERG, false);
    terminal->SetLogLevel("PvmfSyncUtil", PVLOGMSG_EMERG, false);
    terminal->SetLogLevel("PvmfSyncUtilDataQueue", PVLOGMSG_EMERG, false);
}


void engine_handler::Connect()
{

    if (iUseIsdn || iUseSockets)
    {
        iConnectOptions.iLoopbackMode = PV_LOOPBACK_NONE;
    }
    else
    {
        iConnectOptions.iLoopbackMode = PV_LOOPBACK_MUX;
    }

    int32 error = 0;
    OSCL_TRY(error, iConnectCmdId = terminal->Connect(iConnectOptions, iCommServer));
    if (error != 0)
    {
        PV2WayUtil::OutputInfo("Error in Connect!\n");
    }
}

bool engine_handler::ConnectSocket(bool aIsServer, int aPort, char* aIpAddr)
{
    /*Below if condition should be enabled only if there is no prog argument and config file is not present.*/
    if (!iConfigFilePresent && !iRunOnce)
    {
        iRunOnce = true;
        if (!iReadyToConnectCommunication)
        {
            StepsBeforeConnectCommunication();
            if (aIsServer)
            {

                iPv2wayCommConfig->iConnectionType = OSCL_ARRAY_NEW(char, oscl_strlen("SERVER") + 1);
                oscl_strncpy(iPv2wayCommConfig->iConnectionType, "SERVER", oscl_strlen("SERVER") + 1);
            }
            else
            {
                iPv2wayCommConfig->iConnectionType = OSCL_ARRAY_NEW(char, oscl_strlen("CLIENT") + 1);
                oscl_strncpy(iPv2wayCommConfig->iConnectionType, "CLIENT", oscl_strlen("CLIENT") + 1);

            }
            iPv2wayCommConfig->iAddr = aIpAddr;
            iPv2wayCommConfig->iPort = aPort;


        }

    }
    iUseSockets = true;
    return iTwoWaySocket.ConnectSocket(aIsServer, aPort, aIpAddr);

}

void engine_handler::SocketConnected(PVMFNodeInterface* aCommServer)
{
    iCommServer = aCommServer;
    iCommServerType = SOCKET_COMM_SERVER;


    InitTerminal();
}

void engine_handler::SocketDisconnected()
{
    PV2WayUtil::OutputInfo("SocketDisconnected\n");
    Disconnect();
}

void engine_handler::Disconnect()
{
    int32 error = 0;
    if (!iSentDisconnectCmd)
    {
        OSCL_TRY(error, iDisCmdId = terminal->Disconnect());
    }
    if (error != 0)
    {
        PV2WayUtil::OutputInfo("Error in Disconnect!\n");
    }
    else
    {
        iSentDisconnectCmd = true;

    }

}

void engine_handler::RemoveAudioSink()
{
    iRemoveAudioSinkID = iSourceAndSinks->RemoveAudioSink();
}

void engine_handler::RemoveAudioSource()
{
    iRemoveAudioSourceID = iSourceAndSinks->RemoveAudioSource();
}

void engine_handler::RemoveVideoSink()
{
    iRemoveVideoSinkID = iSourceAndSinks->RemoveVideoSink();
}

void engine_handler::RemoveVideoSource()
{
    iRemoveVideoSourceID = iSourceAndSinks->RemoveVideoSource();
}


bool engine_handler::Reset()
{
    int32 error = 0;
    if (false == iDisconnected)
    {
        PV2WayUtil::OutputInfo("\nError:Reset can not ne done before disconnect complete!\n");
        return true;
    }
    if (iH324ConfigInterface)
    {
        iH324ConfigInterface->removeRef();
        iH324ConfigInterface = NULL;
    }
    OSCL_TRY(error, iRstCmdId = terminal->Reset());
    if (error)
    {
        PV2WayUtil::OutputInfo("\n Error in reset()\n");
        OSCL_ASSERT(0);
    }
    return true;
}


void engine_handler::user_input()
{
    const char* userinputstring = "this is a test of user input indication encoded as alphanumeric string.this is a test of user input indication encoded as alphanumeric string.this is a test of user input indication encoded as alphanumeric string.this is a test of user input indication encoded as alphanumeric string.";
    CPVUserInputAlphanumeric ui((uint8*)userinputstring, oscl_strlen(userinputstring) + 1);
}

void engine_handler::send_end_session()
{
    if (iH324ConfigInterface)
        OSCL_STATIC_CAST(H324MConfigInterface*, iH324ConfigInterface)->SendEndSession();
}



void engine_handler::Run()
{
    ConfigureTest();
    RunIfNotReady(RUN_IF_NOT_READY_TIME);
}


void engine_handler::HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent)
{
    switch (aEvent.GetEventType())
    {
        case PVT_INDICATION_INCOMING_TRACK:
            if (((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0] == PV_VIDEO)
            {
                iSourceAndSinks->HandleIncomingVideo(aEvent);

            }
            else if (((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0] == PV_AUDIO)
            {
                iSourceAndSinks->HandleIncomingAudio(aEvent);
            }
            break;

        case PVT_INDICATION_OUTGOING_TRACK:
            if (((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0] == PV_VIDEO)
            {
                iSourceAndSinks->HandleOutgoingVideo(aEvent);
            }
            else if (((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0] == PV_AUDIO)
            {
                iSourceAndSinks->HandleOutgoingAudio(aEvent);
            }
            break;
        case PVT_INDICATION_DISCONNECT:
            PV2WayUtil::OutputInfo("\nRemote Disconnect\n");
            break;

        case PVT_INDICATION_CLOSING_TRACK:
            iSourceAndSinks->HandleClosingTrack(aEvent);
            if (!iDisconnectCall)
            {
                iSourceAndSinks->RemoveSourceAndSinks();
                iDisconnectCall = true;
            }

            break;
        case PVT_INDICATION_CLOSE_TRACK:
            iSourceAndSinks->HandleCloseTrack(aEvent);

            break;

        case PVT_INDICATION_INTERNAL_ERROR:
        {
            PV2WayUtil::OutputInfo("\nInternal error detected\n");
        }
        break;

        default:
            PV2WayUtil::OutputInfo("\nOther event %d\n", aEvent.GetEventType());
            break;
    }

}

void engine_handler::InitCompleted(const PVCmdResponse& aResponse)
{
    PV2WayUtil::OutputInfo("\nInit complete status %d\n", aResponse.GetCmdStatus());
    ConfigureH324Interface();

    if (aResponse.GetCmdStatus() == PVMFSuccess)
    {
        if (iLoopbackCall || (iAutomatedCall && !iUseIsdn))
        {
            Connect();
        }
    }
    if (iLoopbackCall)
    {
        MemoryStats();
    }
}

void engine_handler::ResetCompleted(const PVCmdResponse& aResponse)
{
    iSourceAndSinks->ResetCompleted();

    if (iCommServer)
    {
#ifdef SUPPORT_ISDN
        if (iUseIsdn)
        {
            ((PVMFCapiIsdnNode *) iCommServer)->CapiDisconnect();
        }
#endif
        DeleteCommServer();
    }
    PV2WayUtil::OutputInfo("\nReset complete status %d\n", aResponse.GetCmdStatus());

    Cleanup();
    StopScheduler();
    MemoryStats();
}


void engine_handler::ConnectCompleted(const PVCmdResponse& aResponse)
{
    PV2WayUtil::OutputInfo("\nConnect complete status %d\n", aResponse.GetCmdStatus());
    if (iLoopbackCall)
    {
        MemoryStats();
    }
}

void engine_handler::DisconnectCompleted(const PVCmdResponse& aResponse)
{
    PV2WayUtil::OutputInfo("\nDisconnect complete status %d\n", aResponse.GetCmdStatus());
    if (PVMFSuccess == aResponse.GetCmdStatus())
    {
        iDisconnected = true;

    }
    MemoryStats();
    Reset();
}

void engine_handler::IFCommandCompleted(const PVCmdResponse& aResponse)
{
    PV2WayUtil::OutputInfo("\nQuery Interface complete.  Response status(%d), iH324ConfigInterface(0x%p)\n", aResponse.GetCmdStatus(), iH324ConfigInterface);

    if (aResponse.GetCmdStatus() != PVMFSuccess || (iH324ConfigInterface == NULL))
    {
        PV2WayUtil::OutputInfo("\nQuery Interface failed.\n");
        return;
    }

    PVUuid uuid = PVUuidH324ComponentInterface;

    iH324ConfigInterface->queryInterface(uuid, iComponentInterface);
    if (!iComponentInterface)
    {
        PV2WayUtil::OutputInfo("\n  Query Interface status failed.\n");
        return;
    }

    // ConfigureH324Interface();
    if (iConfigFilePresent)
    {
        ReadProperties();
    }
    else
    {
        ConfigureH324Interface();
    }

    iReadyToConnectCommunication = true;

    if (iLoopbackCall)
    {
        Init();
    }
}

void engine_handler::ConfigureH324Interface()
{
    H324MConfigInterface* h324ConfigInterface = OSCL_STATIC_CAST(H324MConfigInterface*, iH324ConfigInterface);
    h324ConfigInterface->SetObserver(this);
    h324ConfigInterface->SetMultiplexLevel(H223_LEVEL);
    h324ConfigInterface->SetMaxSduSize(PVT_AL1, H223_MAX_AL1_SDU_SIZE);
    h324ConfigInterface->SetMaxSduSize(PVT_AL2, H223_MAX_AL2_SDU_SIZE);
    h324ConfigInterface->SetMaxSduSize(PVT_AL3, H223_MAX_AL3_SDU_SIZE);
    h324ConfigInterface->SetMaxSduSizeR(PVT_AL1, H223_MAX_AL1_SDU_SIZE_R);
    h324ConfigInterface->SetMaxSduSizeR(PVT_AL2, H223_MAX_AL2_SDU_SIZE_R);
    h324ConfigInterface->SetMaxSduSizeR(PVT_AL3, H223_MAX_AL3_SDU_SIZE_R);
    h324ConfigInterface->SetAl2SequenceNumbers(H223_AL2_SN_WIDTH);
    h324ConfigInterface->SetAl3ControlFieldOctets(H223_AL3_CFO);
    h324ConfigInterface->SetMaxPduSize(H223_MAX_PDU_SIZE);
    h324ConfigInterface->SetTerminalType(H324_TERMINAL_TYPE);
    h324ConfigInterface->SetALConfiguration(PV_AUDIO, PVT_AL1, H324_ALLOW_AUDIO_AL1);
    h324ConfigInterface->SetALConfiguration(PV_AUDIO, PVT_AL2, H324_ALLOW_AUDIO_AL2);
    h324ConfigInterface->SetALConfiguration(PV_AUDIO, PVT_AL3, H324_ALLOW_AUDIO_AL3);
    h324ConfigInterface->SetALConfiguration(PV_VIDEO, PVT_AL1, H324_ALLOW_VIDEO_AL1, H324_USE_VIDEO_AL1);
    h324ConfigInterface->SetALConfiguration(PV_VIDEO, PVT_AL2, iAllowVideoOverAl2,
                                            iUseVideoOverAl2);
    h324ConfigInterface->SetALConfiguration(PV_VIDEO, PVT_AL3, iAllowVideoOverAl3,
                                            iUseVideoOverAl3);
    h324ConfigInterface->SetMaxMuxPduSize(H324_MAX_PDU_SIZE_R);
    h324ConfigInterface->SetMaxMuxCcsrlSduSize(H324_MAX_CCSRL_SDU_SIZE);
}

void engine_handler::CommandCompleted(const PVCmdResponse& aResponse)
{
    PVCommandId cmdId = aResponse.GetCmdId();

    if (cmdId == iInitCmdId)
    {
        InitCompleted(aResponse);
    }
    else if (iRstCmdId == cmdId)
    {
        ResetCompleted(aResponse);
    }
    else if (iConnectCmdId == cmdId)
    {
        ConnectCompleted(aResponse);
    }
    else if (iDisCmdId == cmdId)
    {
        iSentDisconnectCmd = false;
        DisconnectCompleted(aResponse);
    }
    else if (i324mIFCommandId == cmdId)
    {
        IFCommandCompleted(aResponse);
    }
    else if (iRemoveAudioSinkID == cmdId)
    {
        PV2WayUtil::OutputInfo("\nRemoved Audio Sink.\n");
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iRemoveAudioSourceID == cmdId)
    {
        PV2WayUtil::OutputInfo("\nRemoved Audio Source.\n");
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iRemoveVideoSinkID == cmdId)
    {
        PV2WayUtil::OutputInfo("\nRemoved Video Sink.\n");
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iRemoveVideoSourceID == cmdId)
    {
        PV2WayUtil::OutputInfo("\nRemoved Video Source.\n");
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else
    {
        iSourceAndSinks->CommandCompleted(aResponse);
    }
}

void engine_handler::HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    PVMFAsyncEvent event = aEvent;

#ifdef SUPPORT_ISDN
    //ISDN node will send an unsolicited event that signals when bearer is up.
    // only used to get Capi connect event from Capi node
    if (event.IsA() == PVCapiEvent &&  event.GetEventType() == PVCapiConnected)
    {
        PV2WayUtil::OutputInfo("\nISDN Bearer is up!\n");

        if (iAutomatedCall)
        {
            Connect();
        }
    }
#endif
}


void engine_handler::CreateComms()
{
    DeleteCommServer();

    if (iUseIsdn)
    {
#ifdef SUPPORT_ISDN
        iCommServer = PVMFCapiIsdnNode::Create();
        iCommServerType = PVMF_CAPI_ISDN_COMM_SERVER;
        ((PVMFCapiIsdnNode*)iCommServer)->SetInfoEventObserver(*this, NULL);
        ((PVMFCapiIsdnNode*)iCommServer)->CapiListen(CIP_MASK_ALL_SERVICES);
        //Wait for ISDN bearer to come up before connecting stack.
#endif
    }
    if (iUseSockets)
    {
    }
    else
    {
        iCommSettings.iMediaFormat = PVMF_MIME_H223;
        iCommSettings.iTestObserver = NULL;
        iCommServerIOControl = PvmiMIOCommLoopbackFactory::Create(iCommSettings);
        bool enableBitstreamLogging = true;
        iCommServer = PVCommsIONodeFactory::Create(iCommServerIOControl, enableBitstreamLogging);
        iCommServerType = PVMF_LOOPBACK_COMM_SERVER;
    }

    return;
}

void engine_handler::MemoryStats()
{
#if !(OSCL_BYPASS_MEMMGT)

    OsclAuditCB auditCB;
    OsclMemInit(auditCB);
    if (auditCB.pAudit)
    {
        MM_Stats_t* stats = auditCB.pAudit->MM_GetStats("");
        if (stats)
        {
            PV2WayUtil::OutputInfo("\n###################Memory Stats Start#################\n");
            PV2WayUtil::OutputInfo("  numBytes %d\n", stats->numBytes);
            PV2WayUtil::OutputInfo("  peakNumBytes %d\n", stats->peakNumBytes);
            PV2WayUtil::OutputInfo("  numAllocs %d\n", stats->numAllocs);
            PV2WayUtil::OutputInfo("  peakNumAllocs %d\n", stats->peakNumAllocs);
            PV2WayUtil::OutputInfo("  numAllocFails %d\n", stats->numAllocFails);
            PV2WayUtil::OutputInfo("  totalNumAllocs %d\n", stats->totalNumAllocs);
            PV2WayUtil::OutputInfo("  totalNumBytes %d\n", stats->totalNumBytes);
            PV2WayUtil::OutputInfo("\n###################Memory Stats End###################\n");
        }

    }
#endif
}

void engine_handler::ReadProperties()
{
    int i, j;
    H324MConfigInterface* h324ConfigInterface = OSCL_STATIC_CAST(H324MConfigInterface*, iH324ConfigInterface);
    h324ConfigInterface->SetObserver(this);
    TerminalSettings* lTermSettings = &iTestSettings->iTerminalSettings;
    if (lTermSettings->iH223MuxLevel != H223_LEVEL_UNKNOWN)
    {
        h324ConfigInterface->SetMultiplexLevel(lTermSettings->iH223MuxLevel);
    }

    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 2; j++)
        {
            TPVAdaptationLayer lAdapLayer = (TPVAdaptationLayer)i;
            if (j == 0)
            {
                int lMaxSduSize = lTermSettings->iSDUSizeSettings[0].iMaxSDUSize[i];
                if (lMaxSduSize)
                {
                    h324ConfigInterface->SetMaxSduSize(lAdapLayer, lMaxSduSize);
                }
            }
            else if (j == 1)
            {
                int lMaxSduSizeR = lTermSettings->iSDUSizeSettings[1].iMaxSDUSize[i];
                if (lMaxSduSizeR)
                {
                    h324ConfigInterface->SetMaxSduSizeR(lAdapLayer, lMaxSduSizeR);
                }
            }
        }
    }
    bool set = false;
    bool use = false;
    //Set AL properties for Audio and Video
    for (i = 0; i < 3; i++)
    {
        if (lTermSettings->iAlSettings[AUDIO].iALAllow[i] != AL_SETTINGS_NOT_SET)
        {
            set = (lTermSettings->iAlSettings[AUDIO].iALAllow[i] == 0) ? false : true;
            h324ConfigInterface->SetALConfiguration(PV_AUDIO, (TPVAdaptationLayer)i, set);
        }

        if (lTermSettings->iAlSettings[VIDEO].iALAllow[i] != AL_SETTINGS_NOT_SET)
        {
            set = (lTermSettings->iAlSettings[VIDEO].iALAllow[i] == 0) ? false : true;
            if (lTermSettings->iAlSettings[VIDEO].iALUse[i] != AL_SETTINGS_NOT_SET)
            {
                use = (lTermSettings->iAlSettings[VIDEO].iALUse[i] == 0) ? false : true;
            }
            else
            {
                // if haven't set the use variable, set it to whatever set was set to
                use = set;
            }
            h324ConfigInterface->SetALConfiguration(PV_VIDEO, (TPVAdaptationLayer)i, set, use);
        }
        else if (lTermSettings->iAlSettings[VIDEO].iALUse[i] != AL_SETTINGS_NOT_SET)
        {
            use = (lTermSettings->iAlSettings[VIDEO].iALUse[i] == 0) ? false : true;
            set = use;
            h324ConfigInterface->SetALConfiguration(PV_VIDEO, (TPVAdaptationLayer)i, set, use);
        }

    }
    if (lTermSettings->iAl2SequenceNumbers != PVT_NOT_SET)
    {
        h324ConfigInterface->SetAl2SequenceNumbers(lTermSettings->iAl2SequenceNumbers);
    }
    if (lTermSettings->iAl3ControlFieldOctets != PVT_NOT_SET)
    {
        h324ConfigInterface->SetAl3ControlFieldOctets(lTermSettings->iAl3ControlFieldOctets);
    }
    h324ConfigInterface->SetTerminalType(lTermSettings->iMSD);

}

bool engine_handler::ReadGCFTestFile(char * aFileName)
{
    iTestSettings = OSCL_NEW(TestSettings, ());

    TestCaseParser parser;
    parser.ParseFile(aFileName, iPv2wayCommConfig->iTestTerminal, *iTestSettings);

    return false;

}

bool engine_handler::ReadConfigFile(char* aFileName)
{
    iPv2wayCommConfig->iAddr = NULL;
    iPv2wayCommConfig->iCommNode = NULL;
    iPv2wayCommConfig->iConnectionType = NULL;
    TestCaseParser parser;
    iConfigFilePresent = parser.ParseConfigFile(aFileName, *iPv2wayCommConfig);

    return iConfigFilePresent;
}

// have to finish these steps before connecting communication
void engine_handler::StepsBeforeConnectCommunication()
{
    CreateComponent();
}

void engine_handler::StartSocketCommunication()
{
    int32 portext = 0;

    if (iTestSettings->iName)
    {
        sscanf(iTestSettings->iName, "TestCase%d", &portext);
    }

    uint32 port = iPv2wayCommConfig->iPort + portext;

    ChangeAutomatedCallSetting();


    if (oscl_strncmp(iPv2wayCommConfig->iConnectionType, "SERVER", oscl_strlen("SERVER")) == 0)
    {
        ConnectSocket(true, port, iPv2wayCommConfig->iAddr);
    }
    else if (oscl_strncmp(iPv2wayCommConfig->iConnectionType, "CLIENT", oscl_strlen("CLIENT")) == 0)
    {
        ConnectSocket(false, port, iPv2wayCommConfig->iAddr);
    }
}

void engine_handler::ConfigureTest()
{
    // evaluate user's input.  this function will call correct function based on
    // what is entered.

    if (!iConfigFilePresent)
    {
        evaluate_command();

    }

    else
    {
        //If comm node is loopback
        if (strcmp(iPv2wayCommConfig->iCommNode, "LOOPBACK") == 0)
        {
            if (!iInitCalled)
            {
                iLoopbackCall = true;
                Init();
            }
        }
//if comm node is socket
        else if (strcmp(iPv2wayCommConfig->iCommNode, "SOCKET") == 0)
        {
            if (!iReadyToConnectCommunication)
            {
                StepsBeforeConnectCommunication();
            }
            else if (!iUseSockets)
            {
                StartSocketCommunication();
            }

        }
        if ((iSourceAndSinks->AllMIOsAdded()) && (!iDisconnectCall))
        {
            iDisconnectCall = true;
            RunIfNotReady(DISCONNECT_TIME_FOR_LOOPBACK_CALL);
        }
        else if ((iDisconnectCall) && !(iSourceAndSinks->AllMIOsRemoved()))
        {
            iRemoveSourceAndSinks = true;
            iSourceAndSinks->RemoveSourceAndSinks();
        }
        else if (!(iDisconnected) && (iSourceAndSinks->AllMIOsRemoved()) && (iRemoveSourceAndSinks))
        {
            PV2WayUtil::OutputInfo("Disconnecting from ConfigureTest.\n");
            Disconnect();
        }

    }
}


