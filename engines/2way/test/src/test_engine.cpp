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
#include "test_engine.h"

#include "alloc_dealloc_test.h"
#include "init_test.h"
#include "init_cancel_test.h"
#include "video_only_test.h"
#include "av_test.h"
#include "receive_data_test.h"
#include "user_input_test.h"
#include "connect_test.h"
#include "connect_cancel_test.h"
#include "audio_only_test.h"
#include "av_duplicate_test.h"
#include "basic_lipsync_test.h"
#include "acceptable_formats_test.h"
#include "negotiated_formats_test.h"
#include "oscl_string_utils.h"
#include "oscl_mem_audit.h"
#include "pv_2way_mio.h"
#include "pause_resume_test.h"
#include "reconnect_test.h"
#include "error_check_audio_only_test.h"
#include "turn_on_test_buffer_alloc.h"


#include "tsc_h324m_config_interface.h"
#include "test_utility.h"
#include "test_codecs.h"


#include "pv_2way_source_and_sinks_lipsync.h"

#ifndef UT_H_INCLUDED
#include "ut.h"
#endif



#define AUD_SRC_PAUSE_DURATION  1
#define AUD_SRC_RESUME_DURATION 5
#define AUD_SNK_PAUSE_DURATION   10
#define AUD_SNK_RESUME_DURATION 4
#define VID_SRC_PAUSE_DURATION   15
#define VID_SRC_RESUME_DURATION  3
#define VID_SNK_PAUSE_DURATION  20
#define VID_SNK_RESUME_DURATION 5

#define MAX_SIP_TEST 27
#define SIP_TEST_OFFSET 200
#define SIP_TEST_MAP(x) (x+SIP_TEST_OFFSET)
#define NUM_SIP_ARGS 10

cmd_line *global_cmd_line;

//Find test range args:
//To run a range of tests by enum ID:
//  -test 17 29

char engine_test::iProfileName[32] = "";
uint32 engine_test::iMediaPorts[2] = { 0, 0 };
char engine_test::iPeerAddress[64] = "";

engine_test_suite::engine_test_suite() : test_case() {}
engine_test_suite::~engine_test_suite() {}

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

void engine_test_suite::AddOutgoingAudioCodecUsingFile(PV2WaySourceAndSinksFile* apSourceAndSinks,
        const char* const apAudSrcFormatType)
{
    // Audio Source /Outgoing
    if (oscl_strncmp(PVMF_MIME_PCM16, apAudSrcFormatType, oscl_strlen(PVMF_MIME_PCM16)) == 0)
    {
        apSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iAudioSourceRawFileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_AMR_IF2, apAudSrcFormatType, oscl_strlen(PVMF_MIME_AMR_IF2)) == 0)
    {
        apSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iAudioSourceFileSettings);
    }

}

void engine_test_suite::AddIncomingAudioCodecUsingFile(PV2WaySourceAndSinksFile* apSourceAndSinks,
        const char* const apAudSinkFormatType)
{
    // Audio Sink   /Incoming
    if (oscl_strncmp(PVMF_MIME_PCM16, apAudSinkFormatType, oscl_strlen(PVMF_MIME_PCM16)) == 0)
    {

        apSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iAudioSinkRawFileSettings);

    }
    else if (oscl_strncmp(PVMF_MIME_AMR_IF2, apAudSinkFormatType, oscl_strlen(PVMF_MIME_AMR_IF2)) == 0)
    {
        apSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iAudioSinkFileSettings);
    }

}

void engine_test_suite::AddOutgoingVideoCodecUsingFile(PV2WaySourceAndSinksFile* apSourceAndSinks,
        const char* const apVidSrcFormatType)
{
    // Video Source /Outgoing
    if (oscl_strncmp(PVMF_MIME_YUV420, apVidSrcFormatType, oscl_strlen(PVMF_MIME_YUV420)) == 0)
    {
        apSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, iCodecs.iVideoSourceYUVFileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_H2632000, apVidSrcFormatType, oscl_strlen(PVMF_MIME_H2632000)) == 0)
    {

        apSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, iCodecs.iVideoSourceH263FileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_M4V, apVidSrcFormatType, oscl_strlen(PVMF_MIME_M4V)) == 0)
    {
        apSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, iCodecs.iVideoSourceM4VFileSettings);
        apSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, iCodecs.iVideoSourceH263FileSettings);
    }
}

void engine_test_suite::AddIncomingVideoCodecUsingFile(PV2WaySourceAndSinksFile* apSourceAndSinks,
        const char* const apVidSinkFormatType)
{
    // Video Sink   /Incoming
    if (oscl_strncmp(PVMF_MIME_YUV420, apVidSinkFormatType, oscl_strlen(PVMF_MIME_YUV420)) == 0)
    {
        apSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkYUVFileSettings);
    }
    else if (oscl_strncmp(PVMF_MIME_H2632000, apVidSinkFormatType, oscl_strlen(PVMF_MIME_H2632000)) == 0)
    {

        apSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkH263FileSettings);

    }
    else if (oscl_strncmp(PVMF_MIME_M4V, apVidSinkFormatType, oscl_strlen(PVMF_MIME_M4V)) == 0)
    {
        apSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkM4VFileSettings);
        apSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkH263FileSettings);
    }
}

PV2WaySourceAndSinksFile* engine_test_suite::CreateSourceAndSinks(engine_test* apTest,
        const char* const apAudSrcFormatType,
        const char* const apAudSinkFormatType,
        const char* const apVidSrcFormatType,
        const char* const apVidSinkFormatType)
{
    // this function sets up the source and sinks class given some format types.
    // note- this function adds specific codecs based on the format.
    // a totally different set of codecs could be used.  (using a different codecs input)
    PV2WaySourceAndSinksFile* pSourceAndSinks = OSCL_NEW(PV2WaySourceAndSinksFile,
            (apTest->GetSdkInfo()));
    AddOutgoingAudioCodecUsingFile(pSourceAndSinks, apAudSrcFormatType);

    AddIncomingAudioCodecUsingFile(pSourceAndSinks, apAudSinkFormatType);

    AddOutgoingVideoCodecUsingFile(pSourceAndSinks, apVidSrcFormatType);

    AddIncomingVideoCodecUsingFile(pSourceAndSinks, apVidSinkFormatType);

    return pSourceAndSinks;
}

PV2WayDummySourceAndSinks* engine_test_suite::CreateLipSyncSourceAndSinks(engine_test* apTest,
        const char* const apAudSrcFormatType,
        const char* const apAudSinkFormatType,
        const char* const apVidSrcFormatType,
        const char* const apVidSinkFormatType)
{
    iCodecs.iLipSyncAudioSinkSettings.iDummyMIOObserver = OSCL_REINTERPRET_CAST(DummyMIOObserver*,
            (basic_lipsync_test*)apTest);
    iCodecs.iLipSyncVideoSinkSettings.iDummyMIOObserver = OSCL_REINTERPRET_CAST(DummyMIOObserver*,
            (basic_lipsync_test*)apTest);
    //The Lipsync tests need to be setup separately because they use dummy input & output MIOs
    //For lipsync tests #33 and onwards
    PV2WayDummySourceAndSinks* pSourceAndSinks = OSCL_NEW(PV2WayLipSyncSourceAndSinks,
            (apTest->GetSdkInfo()));
    //Setup outgoing audio
    pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iLipSyncAudioSourceSettings);
    //setup incoming audio
    pSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iLipSyncAudioSinkSettings);
    //setup outgoing video
    pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, iCodecs.iLipSyncVideoSourceSettings);
    //setup incoming video
    pSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iLipSyncVideoSinkSettings);

    return pSourceAndSinks;
}

PV2WayDummySourceAndSinks* engine_test_suite::CreateDummySourceAndSinks(engine_test* apTest,
        const char* const apAudSrcFormatType,
        const char* const apAudSinkFormatType,
        const char* const apVidSrcFormatType,
        const char* const apVidSinkFormatType)
{
    iCodecs.iLipSyncAudioSinkSettings.iDummyMIOObserver = OSCL_REINTERPRET_CAST(DummyMIOObserver*,
            (basic_lipsync_test*)apTest);
    iCodecs.iLipSyncVideoSinkSettings.iDummyMIOObserver = OSCL_REINTERPRET_CAST(DummyMIOObserver*,
            (basic_lipsync_test*)apTest);
    //The Lipsync tests need to be setup separately because they use dummy input & output MIOs
    //For lipsync tests #33 and onwards
    PV2WayDummySourceAndSinks* pSourceAndSinks = OSCL_NEW(PV2WayDummySourceAndSinks,
            (apTest->GetSdkInfo()));
    //Setup outgoing audio
    pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iLipSyncAudioSourceSettings);
    //setup incoming audio
    pSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iLipSyncAudioSinkSettings);
    //setup outgoing video
    pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, iCodecs.iLipSyncVideoSourceSettings);
    //setup incoming video
    pSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iLipSyncVideoSinkSettings);

    return pSourceAndSinks;
}


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
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iAudioSinkRawFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in AMR_IF2, PCM
        //      YUV
        // out PCM
        //      YUV
        // expected: AMR, YUV
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iAudioSinkRawFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    // result is PCM, YUV420, PCM, YUV420
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV, H263
        // out PCM
        //      YUV
        // expected: PCM, H263
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkYUVFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM , YUV
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iAudioSourceFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in   AMR
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM, YUV
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iAudioSourceFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    // when init is done - check values
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM , YUV
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iAudioSourceFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    /////////////////////// video configurations //////////////////////////////////
    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    YUV
        // out PCM
        //    YUV, H263
        // expected: PCM, YUV
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, iCodecs.iVideoSourceYUVFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    H263
        // out PCM
        //    H263
        // expected: PCM, H263
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_H2632000);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_H2632000);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    YUV
        // out PCM
        //    H263
        // expected: PCM, YUV
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    H263
        // out PCM
        //    YUV
        // expected: PCM, YUV
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_H2632000);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    M4V, H263
        // out PCM
        //    M4V
        // expected: PCM, M4V
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_M4V, PVMF_MIME_M4V);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkH263FileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_M4V);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_M4V);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    M4V
        // out PCM
        //    M4V
        // expected: PCM, M4V
        acceptable_formats_test* pTemp = OSCL_NEW(acceptable_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_M4V, PVMF_MIME_M4V);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_M4V);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_M4V);
        adopt_test_case(pTemp);
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
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iAudioSinkRawFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);

    }

    if (inRange(firstTest, lastTest))
    {
        // in AMR_IF2, PCM
        //      YUV
        // out PCM
        //      YUV
        // expected: AMR, YUV
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iAudioSinkRawFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    // result is PCM, YUV420, PCM, YUV420
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV, H263
        // out PCM
        //      YUV
        // expected: PCM, H263
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkYUVFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM , YUV
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iAudioSourceFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in   AMR
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM, YUV
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iAudioSourceFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }


    // when init is done - check values
    if (inRange(firstTest, lastTest))
    {
        // in   PCM
        //      YUV
        // out PCM , AMR
        //      YUV
        // expected: PCM , YUV
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_AUDIO, iCodecs.iAudioSourceFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_AMR_IF2);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    /////////////////////// video configurations //////////////////////////////////
    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    YUV
        // out PCM
        //    YUV, H263
        // expected: PCM, YUV
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(OUTGOING, PV_VIDEO, iCodecs.iVideoSourceYUVFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);

        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    H263
        // out PCM
        //    H263
        // expected: PCM, H263
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_H2632000);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_H2632000);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    YUV
        // out PCM
        //    H263
        // expected: PCM, YUV
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_YUV420);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_H2632000);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    H263
        // out PCM
        //    YUV
        // expected: PCM, YUV
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_H2632000);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_YUV420);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    M4V, H263
        // out PCM
        //    M4V
        // expected: PCM, M4V
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_M4V, PVMF_MIME_M4V);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkH263FileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_M4V);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_M4V);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // in PCM
        //    M4V
        // out PCM
        //    M4V
        // expected: PCM, M4V
        negotiated_formats_test* pTemp = OSCL_NEW(negotiated_formats_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_M4V, PVMF_MIME_M4V);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        pTemp->AddExpectedFormat(INCOMING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(OUTGOING, PV_AUDIO, PVMF_MIME_PCM16);
        pTemp->AddExpectedFormat(INCOMING, PV_VIDEO, PVMF_MIME_M4V);
        pTemp->AddExpectedFormat(OUTGOING, PV_VIDEO, PVMF_MIME_M4V);
        adopt_test_case(pTemp);
    }


}

void engine_test_suite::AddPauseResumeTests(const bool aProxy, int32 firstTest, int32 lastTest)
{

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(pause_resume_test, (true, AUD_SRC_PAUSE_DURATION, AUD_SRC_RESUME_DURATION,
                                    true, AUD_SNK_PAUSE_DURATION, AUD_SNK_RESUME_DURATION,
                                    true, VID_SRC_PAUSE_DURATION, VID_SRC_RESUME_DURATION,
                                    true, VID_SNK_PAUSE_DURATION, VID_SNK_RESUME_DURATION,
                                    aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(pause_resume_test, (false, AUD_SRC_PAUSE_DURATION, AUD_SRC_RESUME_DURATION,
                                    true, AUD_SNK_PAUSE_DURATION, AUD_SNK_RESUME_DURATION,
                                    false, VID_SRC_PAUSE_DURATION, VID_SRC_RESUME_DURATION,
                                    false, VID_SNK_PAUSE_DURATION, VID_SNK_RESUME_DURATION,
                                    aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(pause_resume_test, (false, AUD_SRC_PAUSE_DURATION, AUD_SRC_RESUME_DURATION,
                                    false, AUD_SNK_PAUSE_DURATION, AUD_SNK_RESUME_DURATION,
                                    true, VID_SRC_PAUSE_DURATION, VID_SRC_RESUME_DURATION,
                                    false, VID_SNK_PAUSE_DURATION, VID_SNK_RESUME_DURATION,
                                    aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(pause_resume_test, (false, AUD_SRC_PAUSE_DURATION, AUD_SRC_RESUME_DURATION,
                                    false, AUD_SNK_PAUSE_DURATION, AUD_SNK_RESUME_DURATION,
                                    false, VID_SRC_PAUSE_DURATION, VID_SRC_RESUME_DURATION,
                                    true, VID_SNK_PAUSE_DURATION, VID_SNK_RESUME_DURATION,
                                    aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
}

void engine_test_suite::AddAudioTests(const bool aProxy,
                                      int32 firstTest,
                                      int32 lastTest)
{

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }


    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp,
                PVMF_MIME_AMR_IF2, PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
}

void engine_test_suite::AddVideoTests(const bool aProxy,
                                      int32 firstTest,
                                      int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_M4V, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(video_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_M4V);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

}

void engine_test_suite::AddBasicAVTests(const bool aProxy,
                                        int32 firstTest,
                                        int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_PCM16, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_H2632000);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_H2632000, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_H2632000, PVMF_MIME_H2632000);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }


    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_PCM16,
                PVMF_MIME_AMR_IF2, PVMF_MIME_M4V, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(av_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_M4V);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
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
        test_base* pTemp = OSCL_NEW(alloc_dealloc_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
    if (inRange(firstTest, lastTest))
    {
        // init_test
        test_base* pTemp = OSCL_NEW(init_test, (aProxy, 1));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // init_test
        test_base* pTemp = OSCL_NEW(init_test, (aProxy, 2));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // init_cancel_test
        test_base* pTemp = OSCL_NEW(init_cancel_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // connect_test
        test_base* pTemp = OSCL_NEW(connect_test, (aProxy, 1));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // connect_test
        test_base* pTemp = OSCL_NEW(connect_test, (aProxy, 1, true));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        // connect_cancel_test
        test_base* pTemp = OSCL_NEW(connect_cancel_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
}

void engine_test_suite::AddReceiveDataTests(const bool aProxy, int32 firstTest, int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(receive_data_test, (aProxy, TEST_DURATION, MAX_TEST_DURATION * 20));
        iCodecs.iLipSyncVideoSourceSettings.iMediaFormat = PVMF_MIME_H2632000;
        iCodecs.iLipSyncVideoSinkSettings.iMediaFormat = PVMF_MIME_H2632000;
//        iCodecs.iLipSyncVideoSourceSettings.iMediaFormat = PVMF_MIME_YUV420;
//        iCodecs.iLipSyncVideoSinkSettings.iMediaFormat = PVMF_MIME_YUV420;
        PV2WayDummySourceAndSinks* pSourceAndSinks = CreateDummySourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);

        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(receive_data_test, (aProxy, TEST_DURATION * 5, MAX_TEST_DURATION * 20));
        iCodecs.iLipSyncVideoSourceSettings.iMediaFormat = PVMF_MIME_H2632000;
        iCodecs.iLipSyncVideoSinkSettings.iMediaFormat = PVMF_MIME_H2632000;
//        iCodecs.iLipSyncVideoSourceSettings.iMediaFormat = PVMF_MIME_YUV420;
//        iCodecs.iLipSyncVideoSinkSettings.iMediaFormat = PVMF_MIME_YUV420;
        PV2WayDummySourceAndSinks* pSourceAndSinks = CreateDummySourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);

        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(receive_data_test, (aProxy, TEST_DURATION * 10, MAX_TEST_DURATION * 20));
        iCodecs.iLipSyncVideoSourceSettings.iMediaFormat = PVMF_MIME_H2632000;
        iCodecs.iLipSyncVideoSinkSettings.iMediaFormat = PVMF_MIME_H2632000;
//        iCodecs.iLipSyncVideoSourceSettings.iMediaFormat = PVMF_MIME_YUV420;
//        iCodecs.iLipSyncVideoSinkSettings.iMediaFormat = PVMF_MIME_YUV420;
        PV2WayDummySourceAndSinks* pSourceAndSinks = CreateDummySourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);

        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

}


void engine_test_suite::AddReconnectTests(const bool aProxy, int32 firstTest, int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(reconnect_test, (aProxy, TEST_DURATION, MAX_TEST_DURATION * 20));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);

        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }
}

void engine_test_suite::AddMiscTests(const bool aProxy, int32 firstTest, int32 lastTest)
{

    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(error_check_audio_only_test, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = CreateSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

    if (inRange(firstTest, lastTest))
    {

        test_base* pTemp = OSCL_NEW(turn_on_test_buffer_alloc, (aProxy));
        PV2WaySourceAndSinksFile* pSourceAndSinks = OSCL_NEW(PV2WaySourceAndSinksFile,
                (pTemp->GetSdkInfo()));
        AddOutgoingAudioCodecUsingFile(pSourceAndSinks, PVMF_MIME_PCM16);
        AddOutgoingVideoCodecUsingFile(pSourceAndSinks, PVMF_MIME_YUV420);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_AUDIO, iCodecs.iAudioSinkBufferAllocatorFileSettings);
        pSourceAndSinks->AddPreferredCodec(INCOMING, PV_VIDEO, iCodecs.iVideoSinkBufferAllocatorFileSettings);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);


    }

}
#endif

#ifdef LIP_SYNC_TESTING
void engine_test_suite::AddLipSyncTests(const bool aProxy, int32 firstTest, int32 lastTest)
{
    if (inRange(firstTest, lastTest))
    {
        test_base* pTemp = OSCL_NEW(basic_lipsync_test, (aProxy));
        iCodecs.iLipSyncVideoSourceSettings.iMediaFormat = PVMF_MIME_H2632000;
        iCodecs.iLipSyncVideoSinkSettings.iMediaFormat = PVMF_MIME_H2632000;
        PV2WayDummySourceAndSinks* pSourceAndSinks = CreateLipSyncSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_H2632000, PVMF_MIME_H2632000);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }


    if (inRange(firstTest, lastTest))
    {
        //iIsLipSyncTest = true;
        test_base* pTemp = OSCL_NEW(basic_lipsync_test, (aProxy));
        iCodecs.iLipSyncVideoSourceSettings.iMediaFormat = PVMF_MIME_YUV420;
        iCodecs.iLipSyncVideoSinkSettings.iMediaFormat = PVMF_MIME_YUV420;
        PV2WayDummySourceAndSinks* pSourceAndSinks = CreateLipSyncSourceAndSinks(pTemp, PVMF_MIME_AMR_IF2,
                PVMF_MIME_AMR_IF2, PVMF_MIME_YUV420, PVMF_MIME_YUV420);
        pTemp->AddSourceAndSinks(pSourceAndSinks);
        adopt_test_case(pTemp);
    }

}

#endif



#ifndef LIP_SYNC_TESTING
bool engine_test_suite::proxy_tests1(const bool aProxy)
{

    uint32 firstTest = 0;
    uint32 lastTest = MAX_324_TEST;
    bool alltests = false;


    PV2WayUtil::FindTestRange(global_cmd_line, firstTest, lastTest);

    //Basic 2way tests
    PV2WayUtil::OutputInfo("Setup and Audio engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!iCodecs.SetValues())
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

    uint32 firstTest = 0;
    uint32 lastTest = MAX_324_TEST;
    bool alltests = false;


    PV2WayUtil::FindTestRange(global_cmd_line, firstTest, lastTest);

    //Basic 2way tests
    PV2WayUtil::OutputInfo("Video and BasicAV engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!iCodecs.SetValues())
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

    uint32 firstTest = 0;
    uint32 lastTest = MAX_324_TEST;
    bool alltests = false;

    PV2WayUtil::FindTestRange(global_cmd_line, firstTest, lastTest);

    //Basic 2way tests
    PV2WayUtil::OutputInfo("AcceptableFormats and NegotiatedFormats engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!iCodecs.SetValues())
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

bool engine_test_suite::proxy_tests5(const bool aProxy)
{

    uint32 firstTest = 0;
    uint32 lastTest = MAX_324_TEST;
    bool alltests = false;

    PV2WayUtil::FindTestRange(global_cmd_line, firstTest, lastTest);

    //Basic 2way tests
    PV2WayUtil::OutputInfo("Pause Resume engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!iCodecs.SetValues())
    {
        PV2WayUtil::OutputInfo("ERROR! Could not locate all input files.\n");
        return false;
    }

    //pause resume test case.
    AddPauseResumeTests(aProxy, firstTest, lastTest);

    return true;
}

bool engine_test_suite::proxy_tests6(const bool aProxy)
{

    uint32 firstTest = 0;
    uint32 lastTest = MAX_324_TEST;
    bool alltests = false;

    PV2WayUtil::FindTestRange(global_cmd_line, firstTest, lastTest);

    //Basic 2way tests
    PV2WayUtil::OutputInfo("Perf engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!iCodecs.SetValues())
    {
        PV2WayUtil::OutputInfo("ERROR! Could not locate all input files.\n");
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////
    ///////     ReceiveData tests                    ///////////////////////////////

    PV2WayUtil::OutputInfo("Add Receive data tests\n");
    AddReceiveDataTests(aProxy, firstTest, lastTest);

    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////
    ///////     Reconnect tests                    ///////////////////////////////

    PV2WayUtil::OutputInfo("Add Reconnect tests\n");
    AddReconnectTests(aProxy, firstTest, lastTest);
    return true;
}

bool engine_test_suite::proxy_tests7(const bool aProxy)
{
    uint32 firstTest = 0;
    uint32 lastTest = MAX_324_TEST;
    bool alltests = false;

    PV2WayUtil::FindTestRange(global_cmd_line, firstTest, lastTest);

    //Basic 2way tests
    PV2WayUtil::OutputInfo("Perf engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!iCodecs.SetValues())
    {
        PV2WayUtil::OutputInfo("ERROR! Could not locate all input files.\n");
        return false;
    }
    PV2WayUtil::OutputInfo("Add Misc tests\n");
    AddMiscTests(aProxy, firstTest, lastTest);
    return true;



}

bool engine_test_suite::proxy_tests(const bool aProxy)
{
    proxy_tests1(aProxy);
    proxy_tests2(aProxy);
    proxy_tests3(aProxy);
    proxy_tests5(aProxy);
    proxy_tests6(aProxy);
    proxy_tests7(aProxy);
    return true;
}
#else


bool engine_test_suite::proxy_tests4(const bool aProxy)
{

    uint32 firstTest = 0;
    uint32 lastTest = MAX_324_TEST;
    bool alltests = false;

    PV2WayUtil::FindTestRange(global_cmd_line, firstTest, lastTest);

    //Basic 2way tests
    PV2WayUtil::OutputInfo("LipSync engine tests.  First: %d Last: %d\n", firstTest, lastTest);
    if (firstTest == 0 && lastTest == MAX_324_TEST)
        alltests = true;

    if (!iCodecs.SetValues())
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


bool doTest
(
    bool (engine_test_suite::*a_fnctProxyTest)(const bool),
    test_result& a_testResult
)
{
    engine_test_suite ts;

    if (false == (ts.*a_fnctProxyTest)(true))                  // setting iProxy
        PV2WayUtil::OutputInfo("ERROR - unable to setup tests\n");

    int32 err = 0;
    OSCL_TRY(err, ts.run_test());
    if (0 != err)
        PV2WayUtil::OutputInfo("Leave %d\n", err);

    test_result tr = ts.last_result();
    a_testResult.add_result(tr);
    return (tr.success_count() != tr.total_test_count());
}


bool test_wrapper()
{
    /*!

      Step 1b: Initialization
      Includes initializing OsclErrorTrap, OsclScheduler
    */
    OsclErrorTrap::Init();
    OsclScheduler::Init("PV2WayEngineFactory");

    OSCL_HeapString<OsclMemAllocator> xmlresultsfile;
    FindXmlResultsFile(global_cmd_line, xmlresultsfile, PV2WayUtil::GetFileHandle());
    FILE* pOutputStream = 0;
    UT::CM::InitializeReporting("PV2WayEngineUnitTest", xmlresultsfile, PV2WayUtil::GetFileHandle(), pOutputStream);

    // This result will be passed to all test suites
    // and it will be use to append all the results
    test_result tr;

    typedef bool (engine_test_suite::*fnctProxyTest)(const bool);
    fnctProxyTest testcase[] =
    {
#ifndef LIP_SYNC_TESTING
        &engine_test_suite::proxy_tests1,
        &engine_test_suite::proxy_tests2,
        &engine_test_suite::proxy_tests3,
        &engine_test_suite::proxy_tests5,
        &engine_test_suite::proxy_tests6,
        &engine_test_suite::proxy_tests7,
#else
        &engine_test_suite::proxy_tests4,
#endif
    };

    const uint32 starttime = OsclTickCount::TicksToMsec(OsclTickCount::TickCount()); // get start time

    bool retval = true;
    for (size_t i = 0; i < sizeof(testcase) / sizeof(testcase[0]); ++i)
    {
        bool result = doTest(testcase[i], tr);
        if (false == result)
            retval = false;
    }
    tr.set_elapsed_time(OsclTickCount::TicksToMsec(OsclTickCount::TickCount()) - starttime);

    UT::CM::FinalizeReporting("PV2WayEngineUnitTest", xmlresultsfile, tr, PV2WayUtil::GetFileHandle(), pOutputStream);

    /*!

      Step 12f: Cleanup
      Includes cleanup OsclErrorTrap, OsclScheduler
    */
    OsclScheduler::Cleanup();
    OsclErrorTrap::Cleanup();

    return retval;
}


int local_main(FILE* filehandle, cmd_line *command_line)
{
    OSCL_UNUSED_ARG(command_line);
    int result;
    global_cmd_line = command_line;

    PV2WayUtil::SetFileHandle(filehandle);

    PV2WayUtil::OutputInfo("Test Program for PV Engine class.\n");
    /*!
       Step 1: Initialization
       Call all necessary PV SDK Initialization functions

       Step 1a: Initialization
       Includes initializing OsclBase, OsclMem and PVLogger
       File: engines\2way\src\pv_2way_engine_factory.cpp
    */
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


#if (LINUX_MAIN==1)

int main(int argc, char *argv[])
{
    local_main(stdout, NULL);
    return 0;
}

#endif

