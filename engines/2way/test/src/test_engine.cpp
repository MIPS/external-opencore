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
#include "test_engine.h"

#include "alloc_dealloc_test.h"
#include "init_test.h"
#include "init_cancel_test.h"
#ifndef NO_2WAY_324
#include "video_only_test.h"
#include "av_test.h"
#include "user_input_test.h"
#include "connect_test.h"
#include "connect_cancel_test.h"
#include "audio_only_test.h"
#include "av_duplicate_test.h"
#include "basic_lipsync_test.h"
#include "acceptable_formats_test.h"
#include "negotiated_formats_test.h"
#include "pvmf_fileoutput_factory.h"
#endif
#include "oscl_string_utils.h"
#include "oscl_mem_audit.h"
#include "pv_2way_mio.h"


#include "tsc_h324m_config_interface.h"
#ifndef TEST_ENGINE_UTILITY_H_HEADER
#include "test_engine_utility.h"
#endif
#include "test_codecs.h"

#include "pv_logger_impl.h"

#define AUDIO_FIRST 0
#define VIDEO_FIRST 1

#define MAX_SIP_TEST 27
#define SIP_TEST_OFFSET 200
#define SIP_TEST_MAP(x) (x+SIP_TEST_OFFSET)
#define NUM_SIP_ARGS 10
#ifdef LIP_SYNC_TESTING
#define AUDIO_SAMPLE_RATE 20
#define VIDEO_FRAME_RATE 10
#define NUMB_AUD_FRAMES  5
#endif

int start_test();

cmd_line *global_cmd_line;

//Find test range args:
//To run a range of tests by enum ID:
//  -test 17 29

char engine_test::iProfileName[32] = "";
uint32 engine_test::iMediaPorts[2] = { 0, 0 };
char engine_test::iPeerAddress[64] = "";

engine_test_suite::engine_test_suite() : test_case()
{
}

engine_test_suite::~engine_test_suite()
{
}


bool inRange(uint32 firstTest, uint32 lastTest)
{
    bool inR = false;
    static uint32 current = 0;
    if (firstTest <= current &&
            lastTest >= current)
    {
        inR = true;
    }
    current++;
    return inR;
}

PV2WaySourceAndSinksFile* engine_test_suite::CreateSourceAndSinks(engine_test* test,
        const char* const aAudSrcFormatType,
        const char* const aAudSinkFormatType,
        const char* const aVidSrcFormatType,
        const char* const aVidSinkFormatType)
{
    // this function sets up the source and sinks class given some format types.
    // note- this function adds specific codecs based on the format.
    // a totally different set of codecs could be used.  (using a different codecs input)
    PV2WaySourceAndSinksFile* sourceAndSinks = OSCL_NEW(PV2WaySourceAndSinksFile,
            (test->GetSdkInfo()));
    // Audio Source /Outgoing
    if (oscl_strncmp(PVMF_MIME_PCM16, aAudSrcFormatType, oscl_strlen(PVMF_MIME_PCM16)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iAudioSourceRawFileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_AMR_IF2, aAudSrcFormatType, oscl_strlen(PVMF_MIME_AMR_IF2)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iAudioSourceFileSettings);
    }

    // Audio Sink   /Incoming
    if (oscl_strncmp(PVMF_MIME_PCM16, aAudSinkFormatType, oscl_strlen(PVMF_MIME_PCM16)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, codecs.iAudioSinkRawFileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_AMR_IF2, aAudSinkFormatType, oscl_strlen(PVMF_MIME_AMR_IF2)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, codecs.iAudioSinkFileSettings);
    }

    // Video Source /Outgoing
    if (oscl_strncmp(PVMF_MIME_YUV420, aVidSrcFormatType, oscl_strlen(PVMF_MIME_YUV420)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, codecs.iVideoSourceYUVFileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_H2632000, aVidSrcFormatType, oscl_strlen(PVMF_MIME_H2632000)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, codecs.iVideoSourceH263FileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_M4V, aVidSrcFormatType, oscl_strlen(PVMF_MIME_M4V)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, codecs.iVideoSourceM4VFileSettings);
    }

    // Video Sink   /Incoming
    if (oscl_strncmp(PVMF_MIME_YUV420, aVidSinkFormatType, oscl_strlen(PVMF_MIME_YUV420)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, codecs.iVideoSinkYUVFileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_H2632000, aVidSinkFormatType, oscl_strlen(PVMF_MIME_H2632000)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, codecs.iVideoSinkH263FileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_M4V, aVidSinkFormatType, oscl_strlen(PVMF_MIME_M4V)) == 0)
    {
        sourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, codecs.iVideoSinkM4VFileSettings);
    }

    return sourceAndSinks;
}

#ifdef LIP_SYNC_TESTING
PV2WayDummySourceAndSinks* engine_test_suite::CreateLipSyncSourceAndSinks(engine_test* test,
        const char* const aAudSrcFormatType,
        const char* const aAudSinkFormatType,
        const char* const aVidSrcFormatType,
        const char* const aVidSinkFormatType)
{
    //The Lipsync tests need to be setup separately because they use dummy input & output MIOs
    //For lipsync tests #33 and onwards
    PV2WayDummySourceAndSinks* sourceAndSinks = OSCL_NEW(PV2WayDummySourceAndSinks,
            (test->GetSdkInfo()));
    //Setup outgoing audio
    sourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iDummyAudioSourceSettings);
    //setup incoming audio
    sourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, codecs.iDummyAudioSinkSettings);
    //setup outgoing video
    sourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, codecs.iDummyVideoSourceSettings);
    //setup incoming video
    sourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, codecs.iDummyVideoSinkSettings);
    return sourceAndSinks;
}
#endif
#ifndef LIP_SYNC_TESTING
// 32 to 40
void engine_test_suite::AddAcceptableFormatsTests(const bool aProxy,
        int32 firstTest,
        int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        // in AMR_IF2, PCM
        //      YUV
        // out AMR_IF2
        //      YUV
        // expect: AMR, YUV
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, codecs.iAudioSinkRawFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);

    }

    if (inRange(firstTest, lastTest))
    {
        // in AMR_IF2, PCM
        //      YUV
        // out PCM
        //      YUV
        // expected: AMR, YUV
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, codecs.iAudioSinkRawFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }

    // result is PCM, YUV420, PCM, YUV420
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV, H263
        // out PCM
        //      YUV
        // expected: PCM, H263
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        iSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, codecs.iVideoSinkYUVFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }
    /*
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM , YUV
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iAudioSourceFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in   AMR
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM, YUV
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iAudioSourceFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }

    // when init is done - check values
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM , YUV
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iAudioSourceFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }*/

    /////////////////////// video configurations //////////////////////////////////
    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    YUV
        // out PCM
        //    YUV, H263
        // expected: PCM, YUV
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, codecs.iVideoSourceYUVFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    H263
        // out PCM
        //    H263
        // expected: PCM, H263
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_H2632000);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_H2632000);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    YUV
        // out PCM
        //    H263
        // expected: PCM, YUV
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    H263
        // out PCM
        //    YUV
        // expected: PCM, YUV
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_H2632000);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    M4V, H263
        // out PCM
        //    M4V
        // expected: PCM, M4V
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_M4V, PVMF_MIME_M4V);
        iSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, codecs.iVideoSinkH263FileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_M4V);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_M4V);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    M4V
        // out PCM
        //    M4V
        // expected: PCM, M4V
        acceptable_formats_test* temp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_M4V, PVMF_MIME_M4V);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_M4V);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_M4V);
        adopt_test_case(temp);
    }

}


// 32 to 40
void engine_test_suite::AddNegotiatedFormatsTests(const bool aProxy,
        int32 firstTest,
        int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        // in AMR_IF2, PCM
        //      YUV
        // out AMR_IF2
        //      YUV
        // expect: AMR, YUV
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, codecs.iAudioSinkRawFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);

    }

    if (inRange(firstTest, lastTest))
    {
        // in AMR_IF2, PCM
        //      YUV
        // out PCM
        //      YUV
        // expected: AMR, YUV
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, codecs.iAudioSinkRawFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }

    // result is PCM, YUV420, PCM, YUV420
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV, H263
        // out PCM
        //      YUV
        // expected: PCM, H263
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        iSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, codecs.iVideoSinkYUVFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }
    /*
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM , YUV
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iAudioSourceFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in   AMR
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM, YUV
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iAudioSourceFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }


    // when init is done - check values
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM , YUV
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, codecs.iAudioSourceFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }
    */

    /////////////////////// video configurations //////////////////////////////////
    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    YUV
        // out PCM
        //    YUV, H263
        // expected: PCM, YUV
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        iSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, codecs.iVideoSourceYUVFileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    H263
        // out PCM
        //    H263
        // expected: PCM, H263
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_H2632000);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_H2632000);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    YUV
        // out PCM
        //    H263
        // expected: PCM, YUV
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    H263
        // out PCM
        //    YUV
        // expected: PCM, YUV
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_H2632000);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    M4V, H263
        // out PCM
        //    M4V
        // expected: PCM, M4V
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_M4V, PVMF_MIME_M4V);
        iSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, codecs.iVideoSinkH263FileSettings);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_M4V);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_M4V);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    M4V
        // out PCM
        //    M4V
        // expected: PCM, M4V
        negotiated_formats_test* temp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_M4V, PVMF_MIME_M4V);
        temp->AddSourceAndSinks(iSourceAndSinks);
        temp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        temp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_M4V);
        temp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_M4V);
        adopt_test_case(temp);
    }


}

void engine_test_suite::AddAudioTests(const bool aProxy,
                                      int32 firstTest,
                                      int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp,
                PVMF_MIME_AMR_IF2, PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
}

void engine_test_suite::AddVideoTests(const bool aProxy,
                                      int32 firstTest,
                                      int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_M4V, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_M4V);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

}

void engine_test_suite::AddBasicAVTests(const bool aProxy,
                                        int32 firstTest,
                                        int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_H2632000, PVMF_MIME_H2632000);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }


    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_M4V, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_M4V);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

}

// 0 through 6
void engine_test_suite::AddSetupTests(const bool aProxy,
                                      int32 firstTest,
                                      int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        // alloc_dealloc_test
        test_base* temp = OSCL_NEW(alloc_dealloc_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
    if (inRange(firstTest, lastTest))
    {
        // init_test
        test_base* temp = OSCL_NEW(init_test, (aProxy, 1));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // init_test
        test_base* temp = OSCL_NEW(init_test, (aProxy, 2));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // init_cancel_test
        test_base* temp = OSCL_NEW(init_cancel_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // connect_test
        test_base* temp = OSCL_NEW(connect_test, (aProxy, 1));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // connect_test
        test_base* temp = OSCL_NEW(connect_test, (aProxy, 1, true));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

    if (inRange(firstTest, lastTest))
    {
        // connect_cancel_test
        test_base* temp = OSCL_NEW(connect_cancel_test, (aProxy));
        PV2WaySourceAndSinksFile* iSourceAndSinks = CreateSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }
}
#endif
#ifdef LIP_SYNC_TESTING
void engine_test_suite::AddLipSyncTests(const bool aProxy, int32 firstTest, int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* temp = OSCL_NEW(basic_lipsync_test, (aProxy));
        codecs.iDummyAudioSourceSettings.iMediaFormat = PVMF_MIME_AMR_IF2;
        codecs.iDummyAudioSinkSettings.iMediaFormat = PVMF_MIME_AMR_IF2;
        codecs.iDummyVideoSourceSettings.iMediaFormat = PVMF_MIME_H2632000;
        codecs.iDummyVideoSinkSettings.iMediaFormat = PVMF_MIME_H2632000;
        codecs.iDummyAudioSinkSettings.iDummyMIOObserver = OSCL_REINTERPRET_CAST(LipSyncDummyMIOObserver*, (basic_lipsync_test*)temp);
        codecs.iDummyVideoSinkSettings.iDummyMIOObserver = OSCL_REINTERPRET_CAST(LipSyncDummyMIOObserver*, (basic_lipsync_test*)temp);
        //spacing of 20ms between 2 consecutive audio frames
        codecs.iDummyAudioSourceSettings.iAudioFrameRate = AUDIO_SAMPLE_RATE;
        //spacing of 100ms between 2 consecutive video frames
        codecs.iDummyVideoSourceSettings.iVideoFrameRate = VIDEO_FRAME_RATE;
        codecs.iDummyAudioSourceSettings.iNumofAudioFrame = NUMB_AUD_FRAMES;
        PV2WayDummySourceAndSinks* iSourceAndSinks = CreateLipSyncSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_H2632000, PVMF_MIME_H2632000);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }


    if (inRange(firstTest, lastTest))
    {
        //iIsLipSyncTest = true;
        test_base* temp = OSCL_NEW(basic_lipsync_test, (aProxy));
        codecs.iDummyAudioSourceSettings.iMediaFormat = PVMF_MIME_AMR_IF2;
        codecs.iDummyAudioSinkSettings.iMediaFormat = PVMF_MIME_AMR_IF2;
        codecs.iDummyVideoSourceSettings.iMediaFormat = PVMF_MIME_YUV420;
        codecs.iDummyVideoSinkSettings.iMediaFormat = PVMF_MIME_YUV420;
        codecs.iDummyAudioSinkSettings.iDummyMIOObserver = OSCL_REINTERPRET_CAST(LipSyncDummyMIOObserver*, (basic_lipsync_test*)temp);
        codecs.iDummyVideoSinkSettings.iDummyMIOObserver = OSCL_REINTERPRET_CAST(LipSyncDummyMIOObserver*, (basic_lipsync_test*)temp);
        codecs.iDummyAudioSourceSettings.iNumofAudioFrame =  NUMB_AUD_FRAMES;
        codecs.iDummyAudioSourceSettings.iAudioFrameRate =  AUDIO_SAMPLE_RATE;
        codecs.iDummyVideoSourceSettings.iVideoFrameRate =  VIDEO_FRAME_RATE;
        PV2WayDummySourceAndSinks* iSourceAndSinks = CreateLipSyncSourceAndSinks(temp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        temp->AddSourceAndSinks(iSourceAndSinks);
        adopt_test_case(temp);
    }

}

#endif
#ifndef LIP_SYNC_TESTING
bool engine_test_suite::proxy_tests1(const bool aProxy)
{

    int32 firstTest = 0;
    int32 lastTest = MAX_324_TEST;
    bool alltests = false;


    FindTestRange(global_cmd_line, firstTest, lastTest, PV2WayUtil::GetFileHandle());

    //Basic 2way tests
    PV2WayUtil::OutputInfo("Setup and Audio engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!codecs.setvalues())
    {
        PV2WayUtil::OutputInfo("ERROR! Could not locate all input files.\n");
        return false;
    }
    AddSetupTests(aProxy, firstTest, lastTest);
    AddAudioTests(aProxy, firstTest, lastTest);

    return true;
}

bool engine_test_suite::proxy_tests2(const bool aProxy)
{

    int32 firstTest = 0;
    int32 lastTest = MAX_324_TEST;
    bool alltests = false;


    FindTestRange(global_cmd_line, firstTest, lastTest, PV2WayUtil::GetFileHandle());

    //Basic 2way tests
    PV2WayUtil::OutputInfo("Video and BasicAV engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!codecs.setvalues())
    {
        PV2WayUtil::OutputInfo("ERROR! Could not locate all input files.\n");
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////
    //          VIDEO TESTS
    AddVideoTests(aProxy, firstTest, lastTest);

    ///////////////////////////////////////////////////////////////////////////////
    //          AUDIO-VIDEO TESTS

    //          AUDIO-VIDEO TESTS: MULTIPLE INPUTS
    // these check for what is selected at app
    AddBasicAVTests(aProxy, firstTest, lastTest);
    return true;
}
bool engine_test_suite::proxy_tests3(const bool aProxy)
{

    int32 firstTest = 0;
    int32 lastTest = MAX_324_TEST;
    bool alltests = false;

    FindTestRange(global_cmd_line, firstTest, lastTest, PV2WayUtil::GetFileHandle());

    //Basic 2way tests
    PV2WayUtil::OutputInfo("AcceptableFormats and NegotiatedFormats engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!codecs.setvalues())
    {
        PV2WayUtil::OutputInfo("ERROR! Could not locate all input files.\n");
        return false;
    }

    AddAcceptableFormatsTests(aProxy, firstTest, lastTest);


    // 33 to 40
    // these check for what is selected within the engine datapath

    // temporarily comment out while fixing
    AddNegotiatedFormatsTests(aProxy, firstTest, lastTest);

    return true;
}

bool engine_test_suite::proxy_tests(const bool aProxy)
{
    proxy_tests1(aProxy);
    proxy_tests2(aProxy);
    proxy_tests3(aProxy);
    return true;
}
#else
bool engine_test_suite::proxy_tests4(const bool aProxy)
{

    int32 firstTest = 0;
    int32 lastTest = MAX_324_TEST;
    bool alltests = false;

    FindTestRange(global_cmd_line, firstTest, lastTest, PV2WayUtil::GetFileHandle());

    //Basic 2way tests
    PV2WayUtil::OutputInfo("LipSync engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!codecs.setvalues())
    {
        PV2WayUtil::OutputInfo("ERROR! Could not locate all input files.\n");
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////
    ///////     LIP-SYNC TESTS                    ///////////////////////////////

    PV2WayUtil::OutputInfo("Add LIP-SYNC tests\n");
    AddLipSyncTests(aProxy, firstTest, lastTest);

    ///////////////////////////////////////////////////////////////////////////
    return true;
}
#endif
int test_wrapper()
{
    int result;

    /*!

      Step 1b: Initialization
      Includes initializing OsclErrorTrap, OsclScheduler
    */
    OsclErrorTrap::Init();
    OsclScheduler::Init("PV2WayEngineFactory");

    result = start_test();

    /*!

      Step 12f: Cleanup
      Includes cleanup OsclErrorTrap, OsclScheduler
    */
    OsclScheduler::Cleanup();
    OsclErrorTrap::Cleanup();

    return result;
}


int local_main(FILE* filehandle, cmd_line *command_line)
{
    OSCL_UNUSED_ARG(command_line);
    int result;
    global_cmd_line = command_line;

    PV2WayUtil::SetFileHandle(filehandle);
    PV2WayUtil::OutputInfo("Test Program for PV Engine class.\n");

    CPV2WayEngineFactory::Init();
#ifndef OSCL_BYPASS_MEMMGT
#ifndef NDEBUG
#ifdef MEM_AUDIT_2WAY
    OsclAuditCB auditCB;
    OsclMemInit(auditCB);
    auditCB.pAudit->MM_SetMode(auditCB.pAudit->MM_GetMode() |
                               MM_AUDIT_VALIDATE_ON_FREE_FLAG | MM_AUDIT_ALLOC_NODE_ENABLE_FLAG);
#endif
#endif
#endif
    result = test_wrapper();

    PVLogger::Cleanup();
#ifndef OSCL_BYPASS_MEMMGT
#ifndef NDEBUG
#ifdef MEM_AUDIT_2WAY
    //Check for memory leaks before cleaning up OsclMem.

    uint32 leaks = 0;

    if (auditCB.pAudit)
    {
        MM_Stats_t* stats = auditCB.pAudit->MM_GetStats("");
        if (stats)
        {
            PV2WayUtil::OutputInfo("Memory Stats:\n");
            PV2WayUtil::OutputInfo("  peakNumAllocs %d\n", stats->peakNumAllocs);
            PV2WayUtil::OutputInfo("  peakNumBytes %d\n", stats->peakNumBytes);
            PV2WayUtil::OutputInfo("  numAllocFails %d\n", stats->numAllocFails);
            if (stats->numAllocs)
            {
                PV2WayUtil::OutputInfo("  ERROR: Memory Leaks! numAllocs %d, numBytes %d\n", stats->numAllocs, stats->numBytes);
            }
        }
        leaks = auditCB.pAudit->MM_GetNumAllocNodes();
        if (leaks != 0)
        {
            result = 1;
            PV2WayUtil::OutputInfo("ERROR: %d Memory leaks detected!\n", leaks);
            MM_AllocQueryInfo*info = auditCB.pAudit->MM_CreateAllocNodeInfo(leaks);
            uint32 leakinfo = auditCB.pAudit->MM_GetAllocNodeInfo(info, leaks, 0);
            if (leakinfo != leaks)
            {
                PV2WayUtil::OutputInfo("ERROR: Leak info is incomplete.\n");
            }
            for (uint32 i = 0; i < leakinfo; i++)
            {
                PV2WayUtil::OutputInfo("Leak Info:\n");
                PV2WayUtil::OutputInfo("  allocNum %d\n", info[i].allocNum);
                PV2WayUtil::OutputInfo("  fileName %s\n", info[i].fileName);
                PV2WayUtil::OutputInfo("  lineNo %d\n", info[i].lineNo);
                PV2WayUtil::OutputInfo("  size %d\n", info[i].size);
                PV2WayUtil::OutputInfo("  pMemBlock 0x%x\n", info[i].pMemBlock);
                PV2WayUtil::OutputInfo("  tag %s\n", info[i].tag);
            }
            auditCB.pAudit->MM_ReleaseAllocNodeInfo(info);
        }
    }
#endif
#endif
#endif
    /*!

      Step 12: Cleanup
      Step 12g: Cleanup PVLogger, etc
    */
    CPV2WayEngineFactory::Cleanup();

    return (result);
}
#ifndef LIP_SYNC_TESTING
int start_test1(test_result *aTestResult)
{
    int32 leave = 0;
    bool result = 0;
    engine_test_suite* engine_tests = NULL;
    engine_tests = OSCL_NEW(engine_test_suite, ());
    if (engine_tests)
    {
        // setting iProxy
        if (!engine_tests->proxy_tests1(true))
        {
            PV2WayUtil::OutputInfo("ERROR - unable to setup tests\n");
        }
        OSCL_TRY(leave, engine_tests->run_test());

        if (leave != 0)
            PV2WayUtil::OutputInfo("Leave %d\n", leave);

        const test_result the_result = engine_tests->last_result();
        result = (the_result.success_count() != the_result.total_test_count());
        aTestResult->add_result(engine_tests->last_result());
    }
    OSCL_DELETE(engine_tests);
    return result;
}
int start_test2(test_result *aTestResult)
{
    int32 leave = 0;
    int result = 0;
    engine_test_suite* engine_tests = NULL;
    engine_tests = OSCL_NEW(engine_test_suite, ());
    if (engine_tests)
    {
        // setting iProxy
        if (!engine_tests->proxy_tests2(true))
        {
            PV2WayUtil::OutputInfo("ERROR - unable to setup tests\n");
        }
        OSCL_TRY(leave, engine_tests->run_test());
        if (leave != 0)
            PV2WayUtil::OutputInfo("Leave %d\n", leave);

        const test_result the_result = engine_tests->last_result();
        result = (the_result.success_count() != the_result.total_test_count());
        aTestResult->add_result(engine_tests->last_result());
    }
    OSCL_DELETE(engine_tests);
    return result;
}
int start_test3(test_result *aTestResult)
{
    int32 leave = 0;
    int result = 0;
    engine_test_suite* engine_tests = NULL;
    engine_tests = OSCL_NEW(engine_test_suite, ());
    if (engine_tests)
    {
        if (!engine_tests->proxy_tests3(true))
        {
            PV2WayUtil::OutputInfo("ERROR - unable to setup tests\n");
        }
        OSCL_TRY(leave, engine_tests->run_test());
        if (leave != 0)
            PV2WayUtil::OutputInfo("Leave %d\n", leave);

        const test_result the_result = engine_tests->last_result();
        result = (the_result.success_count() != the_result.total_test_count());
        aTestResult->add_result(engine_tests->last_result());
    }
    OSCL_DELETE(engine_tests);
    return result;
}
#else
int start_test4(test_result *aTestResult)
{
    int32 leave = 0;
    int result = 0;
    engine_test_suite* engine_tests = NULL;
    engine_tests = OSCL_NEW(engine_test_suite, ());
    if (engine_tests)
    {
        // setting iProxy
        if (!engine_tests->proxy_tests4(true))
        {
            PV2WayUtil::OutputInfo("ERROR - unable to setup tests\n");
        }
        OSCL_TRY(leave, engine_tests->run_test());
        if (leave != 0)
            PV2WayUtil::OutputInfo("Leave %d\n", leave);

        const test_result the_result = engine_tests->last_result();
        result = (the_result.success_count() != the_result.total_test_count());
        aTestResult->add_result(engine_tests->last_result());
    }
    OSCL_DELETE(engine_tests);
    return result;
}
#endif
int start_test()
{
    int result = 0;
    int temp = 0;

    OSCL_HeapString<OsclMemAllocator> xmlresultsfile;
    FindXmlResultsFile(global_cmd_line, xmlresultsfile, PV2WayUtil::GetFileHandle());
    WriteInitialXmlSummary(xmlresultsfile, PV2WayUtil::GetFileHandle());

    //This result pointer will be passed to all test suites
    //and it will be use to append all the results
    test_result *TestResult = OSCL_NEW(test_result, ());
    //this will clear all the private members of test_result
    TestResult->delete_contents();
#ifndef LIP_SYNC_TESTING
    temp = start_test3(TestResult);
    if (temp != 0)
        result = temp;
    temp = start_test1(TestResult);
    if (temp != 0)
        result = temp;
    temp = start_test2(TestResult);
    if (temp != 0)
        result = temp;
#else
    temp = start_test4(TestResult);
    if (temp != 0)
        result = temp;
#endif
    WriteFinalXmlSummary(xmlresultsfile, *TestResult, PV2WayUtil::GetFileHandle());
    text_test_interpreter interp;
    _STRING rs = interp.interpretation(*TestResult);
    PV2WayUtil::OutputInfo(rs.c_str());
    OSCL_DELETE(TestResult);
    return result;
}

#if (LINUX_MAIN==1)

int main(int argc, char *argv[])
{
    local_main(stdout, NULL);
    return 0;
}

#endif

