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
#include "pv_2way_source_and_sinks_dummy.h"
#include "lipsync_dummy_output_mio.h"
#include "lipsync_dummy_input_mio.h"

#ifndef NO_2WAY_324
#include "pv_comms_io_node_factory.h"
#include "pvmi_mio_comm_loopback_factory.h"
#endif

#include "test_codecs.h"


#ifndef PV2WAY_FILE_NAMES_H_INCLUDED
#include "pv2way_file_names.h"
#endif


class engine_test_suite : public test_case
{
    public:
        engine_test_suite();
        PV2WaySourceAndSinksFile* CreateSourceAndSinks(engine_test* test,
                const char* const aAudSrcFormatType,
                const char* const aAudSinkFormatType,
                const char* const aVidSrcFormatType,
                const char* const aVidSinkFormatType);
        PV2WayDummySourceAndSinks* CreateLipSyncSourceAndSinks(engine_test* test,
                const char* const aAudSrcFormatType,
                const char* const aAudSinkFormatType,
                const char* const aVidSrcFormatType,
                const char* const aVidSinkFormatType);
        bool proxy_tests(const bool aProxy);

        ~engine_test_suite();

    private:
        TestCodecs codecs;
        void AddSetupTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddAudioTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddVideoTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddBasicAVTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddLipSyncTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddAcceptableFormatsTests(const bool aProxy, int32 firstTest, int32 lastTest);
        void AddNegotiatedFormatsTests(const bool aProxy, int32 firstTest, int32 lastTest);

        void play_from_file_tests(const bool aProxy,
                                  const OSCL_wString& aFilename,
                                  const bool aHasAudio,
                                  const bool aHasVideo);
};

class engine_timer;


class engine_timer : public OsclTimerObject
{
    public:
        engine_timer(engine_test *aObserver) : OsclTimerObject(OsclActiveObject::EPriorityNominal, "Test Engine Timer"),
                iObserver(aObserver)
        {};

        ~engine_timer()
        {
            Cancel();
        }

    protected:
        void Run()
        {
            iObserver->TimerCallback();
        }
        void DoCancel()
        {
            OsclTimerObject::DoCancel();
        };

        engine_test *iObserver;

};



#endif


