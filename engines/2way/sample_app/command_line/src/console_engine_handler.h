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
#ifndef CONSOLE_ENGINE_HANDLER_H
#define CONSOLE_ENGINE_HANDLER_H
#include "test_case.h"
#include "text_test_interpreter.h"
#include "oscl_scheduler.h"
#include "pv_2way_engine_factory.h"
#include "pvmf_fileoutput_factory.h"
#include "pv_comms_io_node_factory.h"
#include "pvmi_mio_comm_loopback_factory.h"
#include "pv_2way_interface.h"
#include "pvmi_mio_fileinput.h"

#ifdef SUPPORT_ISDN
#include "pvmf_capi_isdn_node.h"
#endif

#include "tsc_h324m_config_interface.h"
#include "pvlogger.h"
#include "pvlogger_stderr_appender.h"
#include "pvlogger_time_and_id_layout.h"
#include "pvlogger_file_appender.h"
#include "pv_engine_observer.h"
#include "pv_engine_observer_message.h"


#include "twowaysocket.h"
#include "pv_2way_console_source_and_sinks.h"
#include "testcaseparser.h"




class engine_handler : public OsclTimerObject,
        public PVCommandStatusObserver,
        public PVInformationalEventObserver,
        public PVErrorEventObserver,
        public PVMFNodeInfoEventObserver,
        public H324MConfigObserver,
        public TwoWaySocketObserver
{
    public:

        engine_handler();
        ~engine_handler();


        template<class DestructClass>
        class AppenderDestructDealloc : public OsclDestructDealloc
        {
            public:
                virtual void destruct_and_dealloc(OsclAny *ptr)
                {
                    OSCL_DELETE((DestructClass*)ptr);
                }
        };
        void start();
        void Run();


        // used by main
        void CreateComponent();
        void Init();
        void Connect();
        void Disconnect();
        bool Reset();

        bool ChangeAutomatedCallSetting()
        {
            iAutomatedCall = !iAutomatedCall;
            return iAutomatedCall;
        }

        bool ChangeLoopbackCallSetting()
        {
            iLoopbackCall = !iLoopbackCall;
            return iLoopbackCall;
        }
        void MemoryStats();




        bool SetCommServer(bool aCommServer)
        {
            if (!iCommServer)
            {
                iUseIsdn = aCommServer;
                return true;
            }
            return false;
        }
        void user_input();
        void send_end_session();
        bool ConnectSocket(bool aIsServer, int aPort, char* aIpAddr = NULL);
        // TwoWaySocketObserver virtual
        void SocketConnected(PVMFNodeInterface* aCommServer);
        void SocketDisconnected();

        void StopScheduler()
        {
            scheduler->StopScheduler();
        }

        void AllowVideoOverAl2(bool aAllow)
        {
            iAllowVideoOverAl2 = aAllow;
        }

        void AllowVideoOverAl3(bool aAllow)
        {
            iAllowVideoOverAl3 = aAllow;
        }

        void UseVideoOverAl2(bool aUse)
        {
            iUseVideoOverAl2 = aUse;
        }

        void UseVideoOverAl3(bool aUse)
        {
            iUseVideoOverAl3 = aUse;
        }

        void RemoveAudioSink();
        void RemoveAudioSource();
        void RemoveVideoSink();
        void RemoveVideoSource();
        bool ReadGCFTestFile(char * aFileName);
        bool ReadConfigFile(char* aFileName);
        void ReadProperties();
        void ConfigureTest();

    private:

        void Cleanup();
        void InitializeLogs();


        void InitTerminal();

        void HandleErrorEvent(const PVAsyncErrorEvent& /*aEvent*/)
        {
        }

        void HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent);


        void CommandCompleted(const PVCmdResponse& aResponse);
        void InitCompleted(const PVCmdResponse& aResponse);
        void ResetCompleted(const PVCmdResponse& aResponse);

        void ConnectCompleted(const PVCmdResponse& aResponse);
        void DisconnectCompleted(const PVCmdResponse& aResponse);
        void IFCommandCompleted(const PVCmdResponse& aResponse);


        void HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent);

        //H324MConfigObserver
        void H324MConfigCommandCompletedL(PVMFCmdResp& aResponse)
        {
            OSCL_UNUSED_ARG(aResponse);
        }

        void H324MConfigHandleInformationalEventL(PVMFAsyncEvent& aNotification)
        {
            OSCL_UNUSED_ARG(aNotification);
        }

        void ConfigureH324Interface();

    private:
        void StepsBeforeConnectCommunication();
        void StartSocketCommunication();

        void CreateComms();
        void DeleteCommServer();


        void SetWnsrp(bool aEnableWnsrp)
        {
            OSCL_STATIC_CAST(H324MConfigInterface*, iH324ConfigInterface)->SetWnsrp(aEnableWnsrp);
        }



        ///////////////////////////////////////////////////////////////////////
        bool iUseIsdn;
        bool iUseSockets;
        bool iRunOnce;
        bool iAutomatedCall;
        bool iLoopbackCall;
        bool iDisconnectCall;
        bool iDisconnected;
        bool iRemoveSourceAndSinks;
        bool iInitCalled;
        TestSettings *iTestSettings;
        PVMFNodeInterface* iCommServer;
        CommServerType iCommServerType;
        PV2Way324ConnectOptions iConnectOptions;
        PvmiMIOControl* iCommServerIOControl;
        PvmiMIOCommLoopbackSettings iCommSettings;
        PV2Way324InitInfo iSdkInitInfo;

        bool engineExited;
        PVInterface* iH324ConfigInterface;
        PVLoggerAppender *iStdErrAppender;
        PVLoggerAppender *iFileAppender;

        CPV2WayInterface *terminal;
        OsclExecScheduler *scheduler;
        PVInterface* iComponentInterface;
        bool iEnableWnsrp;

        PVCommandId iRstCmdId;
        PVCommandId iDisCmdId;
        PVCommandId iConnectCmdId;
        PVCommandId iInitCmdId;
        PVCommandId i324mIFCommandId;
        PVCommandId iRemoveAudioSinkID;
        PVCommandId iRemoveAudioSourceID;
        PVCommandId iRemoveVideoSinkID;
        PVCommandId iRemoveVideoSourceID;

        bool iAllowVideoOverAl2;
        bool iAllowVideoOverAl3;
        bool iUseVideoOverAl2;
        bool iUseVideoOverAl3;

        PV2WayConsoleSourceAndSinks* iSourceAndSinks;
        TwoWaySocket iTwoWaySocket;
        bool iSentDisconnectCmd;
        Pv2wayCommConfig* iPv2wayCommConfig;
        bool iConfigFilePresent;
        bool iReadyToConnectCommunication;
};

#endif
