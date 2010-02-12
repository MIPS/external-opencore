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
#ifndef TEST_ENGINE_H_INCLUDED
#define TEST_ENGINE_H_INCLUDED

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
#include "engine_test.h"
#include "pv_2way_source_and_sinks_file.h"
#include "lipsync_dummy_output_mio.h"
#include "lipsync_dummy_input_mio.h"

#include "pv_comms_io_node_factory.h"
#include "pvmi_mio_comm_loopback_factory.h"

#include "test_codecs.h"
#include "pv_2way_source_and_sinks_dummy.h"


#ifndef PV2WAY_FILE_NAMES_H_INCLUDED
#include "pv2way_file_names.h"
#endif


class engine_test_suite : public test_case
{
    public:
        engine_test_suite();
        ~engine_test_suite();
        PV2WaySourceAndSinksFile* CreateSourceAndSinks(engine_test* apTest,
                const char* const apAudSrcFormatType,
                const char* const aAudSinkFormatType,
                const char* const apVidSrcFormatType,
                const char* const apVidSinkFormatType);
        PV2WayDummySourceAndSinks* CreateLipSyncSourceAndSinks(engine_test* apTest,
                const char* const apAudSrcFormatType,
                const char* const apAudSinkFormatType,
                const char* const apVidSrcFormatType,
                const char* const apVidSinkFormatType);
        PV2WayDummySourceAndSinks* CreateDummySourceAndSinks(engine_test* apTest,
                const char* const apAudSrcFormatType,
                const char* const apAudSinkFormatType,
                const char* const apVidSrcFormatType,
                const char* const apVidSinkFormatType);
#ifndef LIP_SYNC_TESTING
        bool proxy_tests(const bool aProxy);
        bool proxy_tests1(const bool aProxy);
        bool proxy_tests2(const bool aProxy);
        bool proxy_tests3(const bool aProxy);
        bool proxy_tests5(const bool aProxy);
        bool proxy_tests6(const bool aProxy);
        bool proxy_tests7(const bool aProxy);
#else

        bool proxy_tests4(const bool aProxy);
#endif

    private:

        void AddIncomingAudioCodecUsingFile(PV2WaySourceAndSinksFile* apSourceAndSinks,
                                            const char* const apFormatType);
        void AddOutgoingAudioCodecUsingFile(PV2WaySourceAndSinksFile* aSourceAndSinks,
                                            const char* const apFormatType);
        void AddIncomingVideoCodecUsingFile(PV2WaySourceAndSinksFile* apSourceAndSinks,
                                            const char* const apFormatType);
        void AddOutgoingVideoCodecUsingFile(PV2WaySourceAndSinksFile* apSourceAndSinks,
                                            const char* const apFormatType);

#ifdef LIP_SYNC_TESTING
        void AddLipSyncTests(const bool aProxy, int32 firstTest, int32 lastTest);
#else
        void AddMiscTests(const bool aProxy, int32 firsttest, int32 lastTest);
        void AddReceiveDataTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddReconnectTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddSetupTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddAudioTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddVideoTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddBasicAVTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddPauseResumeTests(const bool aproxy, int32 firstTest, int32 lastTest);
        void AddAcceptableFormatsTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddNegotiatedFormatsTests(const bool aProxy, int32 firstTest, int32 lastTest);
#endif
        void play_from_file_tests(const bool aProxy,
                                  const OSCL_wString& arFilename,
                                  const bool aHasAudio,
                                  const bool aHasVideo);
        TestCodecs iCodecs;
};




#endif


