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
#include "test_engine_utility.h"
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

FILE* fileoutput;
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
bool engine_test_suite::proxy_tests(const bool aProxy)
{

    int32 firstTest = 0;
    int32 lastTest = MAX_324_TEST;
    bool alltests = false;


    FindTestRange(global_cmd_line, firstTest, lastTest, fileoutput);

    //Basic 2way tests
    fprintf(fileoutput, "Basic engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!codecs.setvalues())
    {
        fprintf(fileoutput, "ERROR! Could not locate all input files.\n");
        return false;
    }

    // if we loop here we'll run out of memory
    {
#ifndef LIP_SYNC_TESTING
        fprintf(fileoutput, "Add Setup tests\n");
        AddSetupTests(aProxy, firstTest, lastTest);
        fprintf(fileoutput, "Add Audio tests\n");

        AddAudioTests(aProxy, firstTest, lastTest);

        ///////////////////////////////////////////////////////////////////////////////
        //          VIDEO TESTS
        fprintf(fileoutput, "Add Video tests\n");
        AddVideoTests(aProxy, firstTest, lastTest);

        ///////////////////////////////////////////////////////////////////////////////
        //          AUDIO-VIDEO TESTS
        fprintf(fileoutput, "Add AV tests\n");
        AddBasicAVTests(aProxy, firstTest, lastTest);

        //          AUDIO-VIDEO TESTS: MULTIPLE INPUTS


        // 33 to 40
        // these check for what is selected within the engine datapath

        // temporarily comment out while fixing
        AddNegotiatedFormatsTests(aProxy, firstTest, lastTest);
        // these check for what is selected at app
        AddAcceptableFormatsTests(aProxy, firstTest, lastTest);

#endif
        ///////////////////////////////////////////////////////////////////////////////
        ///////     LIP-SYNC TESTS                    ///////////////////////////////

        fprintf(fileoutput, "Add LIP-SYNC tests\n");
#ifdef LIP_SYNC_TESTING
        AddLipSyncTests(aProxy, firstTest, lastTest);
#endif

        ///////////////////////////////////////////////////////////////////////////

    }
    return true;
}


int test_wrapper()
{
    int result;

    OsclErrorTrap::Init();
    OsclScheduler::Init("PV2WayEngineFactory");

    result = start_test();

    OsclScheduler::Cleanup();
    OsclErrorTrap::Cleanup();

    return result;
}


int local_main(FILE* filehandle, cmd_line *command_line)
{
    OSCL_UNUSED_ARG(command_line);
    int result;
    global_cmd_line = command_line;

    fileoutput = filehandle;
    fprintf(fileoutput, "Test Program for PV Engine class.\n");

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
            fprintf(fileoutput, "Memory Stats:\n");
            fprintf(fileoutput, "  peakNumAllocs %d\n", stats->peakNumAllocs);
            fprintf(fileoutput, "  peakNumBytes %d\n", stats->peakNumBytes);
            fprintf(fileoutput, "  numAllocFails %d\n", stats->numAllocFails);
            if (stats->numAllocs)
            {
                fprintf(fileoutput, "  ERROR: Memory Leaks! numAllocs %d, numBytes %d\n", stats->numAllocs, stats->numBytes);
            }
        }
        leaks = auditCB.pAudit->MM_GetNumAllocNodes();
        if (leaks != 0)
        {
            result = 1;
            fprintf(fileoutput, "ERROR: %d Memory leaks detected!\n", leaks);
            MM_AllocQueryInfo*info = auditCB.pAudit->MM_CreateAllocNodeInfo(leaks);
            uint32 leakinfo = auditCB.pAudit->MM_GetAllocNodeInfo(info, leaks, 0);
            if (leakinfo != leaks)
            {
                fprintf(fileoutput, "ERROR: Leak info is incomplete.\n");
            }
            for (uint32 i = 0; i < leakinfo; i++)
            {
                fprintf(fileoutput, "Leak Info:\n");
                fprintf(fileoutput, "  allocNum %d\n", info[i].allocNum);
                fprintf(fileoutput, "  fileName %s\n", info[i].fileName);
                fprintf(fileoutput, "  lineNo %d\n", info[i].lineNo);
                fprintf(fileoutput, "  size %d\n", info[i].size);
                fprintf(fileoutput, "  pMemBlock 0x%x\n", info[i].pMemBlock);
                fprintf(fileoutput, "  tag %s\n", info[i].tag);
            }
            auditCB.pAudit->MM_ReleaseAllocNodeInfo(info);
        }
    }
#endif
#endif
#endif
    CPV2WayEngineFactory::Cleanup();

    return (result);
}


int start_test()
{
    int32 leave = 0;
    bool result = 0;
    // looping for testing
    uint32 ii = 0;
    //for (ii = 0; ii < 1000; ++ii)
    {
        engine_test_suite engine_tests;
        fprintf(fileoutput, "####  TEST ROUND NUMBER: %d ### \n", ii);
        // setting iProxy
        if (!engine_tests.proxy_tests(true))
        {
            fprintf(fileoutput, "ERROR - unable to setup tests\n");
        }

        OSCL_HeapString<OsclMemAllocator> xmlresultsfile;
        FindXmlResultsFile(global_cmd_line, xmlresultsfile, fileoutput);

        OSCL_TRY(leave, engine_tests.run_test());

        if (leave != 0)
            fprintf(fileoutput, "Leave %d\n", leave);

        XmlSummary(xmlresultsfile, engine_tests.last_result(), fileoutput);
        text_test_interpreter interp;
        _STRING rs = interp.interpretation(engine_tests.last_result());
        fprintf(fileoutput, rs.c_str());
        const test_result the_result = engine_tests.last_result();
        result = (the_result.success_count() != the_result.total_test_count());
    }
    return result;
}

#if (LINUX_MAIN==1)

int main(int argc, char *argv[])
{
    local_main(stdout, NULL);
    return 0;
}

#endif

