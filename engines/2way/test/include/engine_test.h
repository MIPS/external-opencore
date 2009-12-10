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
#ifndef ENGINE_TEST_H_INCLUDED
#define ENGINE_TEST_H_INCLUDED

#include "oscl_error.h"
#include "oscl_timer.h"
#include "oscl_mem.h"
#include "oscl_scheduler.h"
#include "oscl_utf8conv.h"
#include "pvlogger.h"
#include "pvlogger_stderr_appender.h"
#include "pvlogger_file_appender.h"
#include "pvlogger_time_and_id_layout.h"
#include "test_case.h"
#include "text_test_interpreter.h"
#include "pv_2way_interface.h"
#include "pv_2way_engine_factory.h"
#include "pv_2way_proxy_factory.h"
#include "pvmf_media_input_node_factory.h"
#include "pv_media_output_node_factory.h"
#include "pvmi_media_io_fileoutput.h"
#include "pvmi_mio_fileinput_factory.h"
#include "pv_engine_observer.h"
#include "pv_engine_observer_message.h"
#include "tsc_h324m_config_interface.h"

#include "pv_comms_io_node_factory.h"
#include "pvmi_mio_comm_loopback_factory.h"

#ifndef PV_2WAY_SOURCE_AND_SINKS_BASE_H_INCLUDED
#include "pv_2way_source_and_sinks_base.h"
#endif

#ifndef PV2WAY_FILE_NAMES_H_INCLUDED
#include "pv2way_file_names.h"
#endif

#ifndef TEST_ENGINE_UTILITY_H_HEADER
#include "test_utility.h"
#endif


class engine_test : public test_case,
        public OsclActiveObject,
        public OsclTimerObserver,
        public PVCommandStatusObserver,
        public PVInformationalEventObserver,
        public PVErrorEventObserver

{
    public:
        engine_test(bool aUseProxy = false,
                    int aMaxRuns = 1) :
                OsclActiveObject(OsclActiveObject::EPriorityNominal, "Test Engine"),
                iAudioSourceAdded(false),
                iAudioAddSourceId(0),
                iAudioSrcChannelId(NULL),
                iAudioRemoveSourceId(0),
                iAudioPauseSourceId(0),
                iAudioResumeSourceId(0),
                iAudioSinkAdded(false),
                iAudioAddSinkId(0),
                iAudioSnkChannelId(NULL),
                iAudioAddSink2Id(0),
                iAudioRemoveSinkId(0),
                iAudioPauseSinkId(0),
                iAudioResumeSinkId(0),
                iVideoSourceAdded(false),
                iVideoSrcChannelId(NULL),
                iVideoAddSourceId(0),
                iVideoRemoveSourceId(0),
                iVideoPauseSourceId(0),
                iVideoResumeSourceId(0),
                iVideoSinkAdded(false),
                iVideoSnkChannelId(NULL),
                iVideoAddSinkId(0),
                iVideoAddSink2Id(0),
                iVideoRemoveSinkId(0),
                iVideoPauseSinkId(0),
                iVideoResumeSinkId(0),
                iUseProxy(aUseProxy),
                iMaxRuns(aMaxRuns),
                iCurrentRun(0),
                iCommServer(NULL),
                iCommServerIOControl(NULL),
                iGetSessionParamsId(0),
                iDuplicatesStarted(false),
                terminal(NULL),
                scheduler(NULL),
                timer(NULL)
        {
            iConnectOptions.iLoopbackMode = PV_LOOPBACK_COMM; //PV_LOOPBACK_MUX;
            iRstCmdId = 0;
            iDisCmdId = 0;
            iConnectCmdId = 0;
            iInitCmdId = 0;
            iCommsAddSourceId = 0;
        }

        virtual ~engine_test()
        {
        }

        virtual void test() = 0;

        virtual void Run() = 0;

        virtual void DoCancel() = 0;

        void HandleErrorEvent(const PVAsyncErrorEvent& /*aEvent*/)
        {
        }

        virtual void HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent) = 0;

        virtual void CommandCompleted(const PVCmdResponse& aResponse) = 0;

        virtual void TimeoutOccurred(int32 timerID, int32 timeoutInfo) = 0;

        PV2Way324InitInfo& GetSdkInfo()
        {
            return iSdkInitInfo;
        }

        static char iProfileName[32];
        static char iPeerAddress[64];
        static uint32 iMediaPorts[2];

    protected:

        /*!

          Step 12: Cleanup
          Step 12b: Disconnect
          Disconnect the terminal.
        */
        virtual void disconnect()
        {
            int error = 0;
            OSCL_TRY(error, iDisCmdId = terminal->Disconnect());
            if (error)
            {
                reset();
            }
        }

        virtual void cleanup()
        {
            destroy_comm();
        }

        virtual void reset();


        virtual void connect()
        {
            int error = 0;
            /*!

              Step 8: Connect terminal
              @param connection options, communication node
            */
            OSCL_TRY(error, iConnectCmdId = terminal->Connect(iConnectOptions, iCommServer));
            if (error)
            {
                reset();
            }
        }

        virtual void printFormatString(PVMFFormatType aFormatType)
        {
            PV2WayUtil::OutputInfo("%s", aFormatType.getMIMEStrPtr());
        }

        bool check_audio_started()
        {
            return (iAudioSourceAdded && iAudioSinkAdded);
        }
        bool check_audio_stopped()
        {
            return (!iAudioSourceAdded && !iAudioSinkAdded);
        }
        bool check_video_started()
        {
            return (iVideoSourceAdded && iVideoSinkAdded);
        }
        bool check_video_stopped()
        {
            return (!iVideoSourceAdded && !iVideoSinkAdded);
        }


        void create_comm();
        void destroy_comm();

        PVCommandId iRstCmdId, iDisCmdId, iConnectCmdId, iInitCmdId;

        PVCommandId iCommsAddSourceId;

        bool iAudioSourceAdded;
        PVCommandId iAudioAddSourceId;
        TPVChannelId* iAudioSrcChannelId;
        PVCommandId iAudioAddSource2Id;
        PVCommandId iAudioRemoveSourceId;
        PVCommandId iAudioPauseSourceId;
        PVCommandId iAudioResumeSourceId;

        bool iAudioSinkAdded;
        PVCommandId iAudioAddSinkId;
        TPVChannelId* iAudioSnkChannelId;
        PVCommandId iAudioAddSink2Id;
        PVCommandId iAudioRemoveSinkId;
        PVCommandId iAudioPauseSinkId;
        PVCommandId iAudioResumeSinkId;

        bool iVideoSourceAdded;
        TPVChannelId* iVideoSrcChannelId;
        PVCommandId iVideoAddSourceId;
        PVCommandId iVideoAddSource2Id;
        PVCommandId iVideoRemoveSourceId;
        PVCommandId iVideoPauseSourceId;
        PVCommandId iVideoResumeSourceId;

        bool iVideoSinkAdded;
        TPVChannelId* iVideoSnkChannelId;
        PVCommandId iVideoAddSinkId;
        PVCommandId iVideoAddSink2Id;
        PVCommandId iVideoRemoveSinkId;
        PVCommandId iVideoPauseSinkId;
        PVCommandId iVideoResumeSinkId;


        bool iUseProxy;
        int iMaxRuns;
        int iCurrentRun;
        PVMFNodeInterface* iCommServer;
        PvmiMIOControl* iCommServerIOControl;
        PvmiMIOCommLoopbackSettings iCommSettings;
        PV2Way324ConnectOptions iConnectOptions;
        //CPV2WaySIPConnectInfo iSIPConnectOptions;
        PV2Way324InitInfo iSdkInitInfo;
        PVCommandId iGetSessionParamsId;
        bool iDuplicatesStarted;
        CPV2WayInterface *terminal;
        OsclExecScheduler *scheduler;
        OsclTimer<OsclMemAllocator> *timer;
};




#endif


