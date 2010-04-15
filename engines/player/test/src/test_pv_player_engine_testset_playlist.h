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
#ifndef TEST_PV_PLAYER_ENGINE_TESTSET_PLAYLIST_H_INCLUDED
#define TEST_PV_PLAYER_ENGINE_TESTSET_PLAYLIST_H_INCLUDED


/**
 *  @file test_pv_player_engine_testset_playlist.h
 *  @brief This file contains the class definitions for the eighth set of
 *         test cases for PVPlayerEngine which are not fully automated (requires human verification)
 *
 */

#ifndef TEST_PV_PLAYER_ENGINE_H_INCLUDED
#include "test_pv_player_engine.h"
#endif

#ifndef PV_PLAYER_DATASOURCEURL_H_INCLUDED
#include "pv_player_datasourceurl.h"
#endif

#ifndef PVMF_LOCAL_DAT_SOURCE_H_INCLUDED
#include "pvmf_local_data_source.h"
#endif

#ifndef PVMF_NODE_INTERFACE_H_INCLUDED
#include "pvmf_node_interface.h"
#endif

#ifndef PV_ENGINE_TYPES_H_INCLUDED
#include "pv_engine_types.h"
#endif

#ifndef TEST_PV_PLAYER_ENGINE_CONFIG_H_INCLUDED
#include "test_pv_player_engine_config.h"
#endif

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

#ifndef OSCL_STRING_UTILS_H_INCLUDED
#include "oscl_string_utils.h"
#endif

#ifndef PVMI_CONFIG_AND_CAPABILITY_OBSERVER_H_INCLUDED
#include "pvmi_config_and_capability_observer.h"
#endif

#ifndef PVMF_SOURCE_CONTEXT_DATA_H_INCLUDED
#include "pvmf_source_context_data.h"
#endif

#ifndef TEST_PV_PLAYER_ENGINE_TESTSET_PLAYLIST_CLIPINFO_H_INCLUDED
#include "test_pv_player_engine_testset_playlist_clipinfo.h"
#endif

typedef enum
{
    PL_CLIP_UNKNOWN_EVENT = 0,
    PL_CLIP_INIT_EVENT,
    PL_CLIP_CORRUPT_EVENT,
    PL_CLIP_START_EVENT,
    PL_CLIP_END_EVENT,
    PL_EOT_EVENT,
    PL_EOS_EVENT
} ClipTransitionEvent;

typedef enum
{
    PL_IGNORE_TIMESTAMP = 0,
    PL_GENERATE_TIMESTAMP,
    PL_COPY_TIMESTAMP
} EventTimestampMode;

typedef struct
{
    uint32 index;                       // clip index = -1 for EOS and EOT
    ClipTransitionEvent event;
    double timestamp;                   // for stats
} PlaylistClipTransitionEvent;

typedef struct
{
    uint32 total;
    uint32 unknown;
    uint32 init;
    uint32 corrupt;
    uint32 start;
    uint32 end;
    uint32 eos;
    uint32 eot;
} EventCounts;


typedef enum
{
    InvalidTest = -1,
    SkipFwdTest = 0,
    SkipFwdOneTrackTest,
    SkipFwdBeyondListTest,
    SkipFwdLastTrackTest,
    SkipAndSeekFwdNextTrackTest,
    SeekBeyondClipDurationTest,
    SkipBwdTest,
    SkipBwdOneTrackTest,
    SkipBwdBeyondListTest,
    SkipFwdAndSeekTest,
    SkipBwdAndSeekTest,
    SkipToCurrentTrackTest,
    SeekInCurrentTrackTest,
    PauseSkipToNextTrackResumeTest
} PVSkipAndSeekTest;

typedef enum
{
    GAPLESS_FORMAT_UNKNOWN = 0,
    GAPLESS_FORMAT_ITUNES_MP3,
    GAPLESS_FORMAT_LAME_MP3,
    GAPLESS_FORMAT_ITUNES_AAC,
} GaplessMetadataFormat;

int32 ReadPlaylistFromFile(const char* aFileName, char**& aPlaylist, FILE* aTestMsgOutputFile);

int32 CreatePlaylist(const char* aFileName, PVMFFormatType& aFileType, char**& aPlaylist, int32 aNumInvalidClips,
                     int32 aInvalidClipAtIndex, FILE* aTestMsgOutputFile, bool& aStaticPlaylist);

void DestroyPlaylist(char** aPlaylist, int32 iNumClipsInPlaylist);

bool AddEventToSequence(Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator>& aSequence, ClipTransitionEvent aEvent,
                        uint32 aClip_index, EventTimestampMode aTimestampMode, double aTimestamp, FILE* aTestMsgOutputFile);

void PrintEventSequence(Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator>& aSequence, FILE* aTestMsgOutputFile);

bool CheckEventSequence(Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator>& aSequence, int32 aNumClipsInPlaylist,
                        EventCounts* aPlaylistEventCounts, EventCounts* aClipEventCounts, PVSkipAndSeekTest aWhichTest,
                        FILE* aTestMsgOutputFile);

bool CompareEventCounts(EventCounts* aResultingPlaylistEventCounts, EventCounts* aResultingClipEventCounts,
                        EventCounts* aExpectedPlaylistEventCounts, EventCounts* aExpectedClipEventCounts,
                        FILE* aTestMsgOutputFile);

bool ValidatePCMOutput(OSCL_wHeapString<OsclMemAllocator> aAudioSinkFileName, uint32 aExpectedFileSize, FILE* aTestMsgOutputFile);


/*! PlaylistPlayTillEndOfListTest
 *  A test case to test playback of specified source with file output media IO node till EOS. Prints out the playback position
 *  player engine sends playback status events
 *  - Data Source: Specified by user of test case
 *  - Data Sink(s): Video[File Output MediaIO Interface Node-test_playlist_playback_%test_specific%_%SOURCEFILENAME%_video.dat]\n
 *                  Audio[File Output MediaIO Interface Node-test_playlist_playback_%test_specific%_%SOURCEFILENAME%_audio.dat]\n
 *                  Text[File Output MediaIO Interface Node-test_playlist_playback_%test_specific%_%SOURCEFILENAME%_text.dat]
 *  - Sequence:
 *             -# CreatePlayer()
 *             -# AddDataSource()
 *             -# Init()
 *             -# AddDataSink() (audio)
 *             -# Prepare()
 *             -# Start()
 *             -# Playback for 2 Seconds
 *             -# UpdateDataSource()
 *             -# UpdateDataSource()
 *             -# UpdateDataSource()
 *             -# LOG PVMFInfoEndOfClipReached for all clips
 *             -# WAIT FOR EVENT PVMFInfoEndOfData
 *             -# Stop()
 *             -# RemoveDataSink() (audio)
 *             -# Reset()
 *             -# RemoveDataSource()
 *             -# DeletePlayer()
 *
 */
class pvplayer_async_test_playlist_playback : public pvplayer_async_test_base
{
    public:
        pvplayer_async_test_playlist_playback(PVPlayerAsyncTestParam aTestParam,
                                              bool aTestUpdateDataSource = false,
                                              bool aInvalidClipTest = false,
                                              int32 aNumInvalidClips = -1,
                                              int32 aInvalidClipAtIndex = -1,
                                              bool aMetadataTest = false,
                                              bool aUnknownMimeTypeTest = false,
                                              bool aInvalidMimeTypeTest = false,
                                              bool aReplaceTrackTest = false,
                                              GaplessMetadataFormat aGaplessFormat = GAPLESS_FORMAT_UNKNOWN):
                pvplayer_async_test_base(aTestParam)
                , iPlayer(NULL)
                , iDataSource(NULL)
                , iExtendedDataSource(NULL)
                , iDataSinkAudio(NULL)
                , iMIOFileOutAudio(NULL)
                , iIONodeAudio(NULL)
                , iDataSinkVideo(NULL)
                , iMIOFileOutVideo(NULL)
                , iIONodeVideo(NULL)
                , iCurrentCmdId(0)
                , iSourceContextData(NULL)
                , iNumEOSReceived(0)
                , iNumClipsAdded(0)
                , iNumInvalidClips(0)
                , iInvalidClipAtIndex(-1)
                , iCorruptedClipsIdentified(0)
                , iMetadataTest(aMetadataTest)
                , iNumValues(0)
                , iCurrentPlaybackClip(-1)
                , iCurrentInitializedClip(-1)
                , iInitComplete(false)
                , iUnknownMimeTypeTest(aUnknownMimeTypeTest)
                , iInvalidMimeTypeTest(aInvalidMimeTypeTest)
                , iAlternateMimeType(PVMF_MIME_FORMAT_UNKNOWN)
                , iClipsWithInvalidMimeType(0)
                , iTestUpdateDataSource(aTestUpdateDataSource)
                , iPlaylist(NULL)
                , iNumClipsInPlaylist(0)
                , iStaticPlaylist(true)
                , iNumClipsInitialized(0)
                , iNumClipsStarted(0)
                , iRetrieveCurrentPlayingMetadata(false)
                , iRetrieveCurrentInitializedMetadata(false)
                , iPrintCurrentPlayingMetadata(false)
                , iPrintCurrentInitializedMetadata(false)
                , iReplaceTrackTest(aReplaceTrackTest)
                , iReplaceClip(false)
                , iSkipGetMetadata(false)
                , iWhichTest(InvalidTest)
                , iReferencePCMFileSize(0)
                , iGaplessFormat(aGaplessFormat)
        {
            if (aGaplessFormat != GAPLESS_FORMAT_UNKNOWN)
            {
                // this is PCM validation with default playlist
                iSinkFileNameSubString = _STRLIT_WCHAR("gapless_validate_");
                switch (iGaplessFormat)
                {
                    case GAPLESS_FORMAT_LAME_MP3:
                        iTestCaseName = _STRLIT_CHAR("Validate Gapless LAME MP3 Playlist");
                        iFileName = DEFAULT_GAPLESS_LAME_MP3_PLAYLIST;
                        iReferencePCMFileSize = DEFAULT_GAPLESS_LAME_MP3_PLAYLIST_PCM_OUTPUT_SIZE;
                        iSinkFileNameSubString += _STRLIT_WCHAR("LAME_mp3_playlist");
                        break;

                    case GAPLESS_FORMAT_ITUNES_AAC:
                        iTestCaseName = _STRLIT_CHAR("Validate Gapless iTunes AAC Playlist");
                        iFileName = DEFAULT_GAPLESS_ITUNES_AAC_PLAYLIST;
                        iReferencePCMFileSize = DEFAULT_GAPLESS_ITUNES_AAC_PLAYLIST_PCM_OUTPUT_SIZE;
                        iSinkFileNameSubString += _STRLIT_WCHAR("iTunes_aac_playlist");
                        break;

                    default:
                        iGaplessFormat = GAPLESS_FORMAT_ITUNES_MP3;
                        // falling through
                    case GAPLESS_FORMAT_ITUNES_MP3:
                        iTestCaseName = _STRLIT_CHAR("Validate Gapless iTunes MP3 Playlist");
                        iFileName = DEFAULT_GAPLESS_ITUNES_MP3_PLAYLIST;
                        iReferencePCMFileSize = DEFAULT_GAPLESS_ITUNES_MP3_PLAYLIST_PCM_OUTPUT_SIZE;
                        iSinkFileNameSubString += _STRLIT_WCHAR("iTunes_mp3_playlist");
                        break;
                }
            }
            else
            {
                // this is playback test
                iSinkFileNameSubString = _STRLIT_WCHAR("playback_");

                if (aInvalidClipTest)
                {
                    // maximum of 4 invalid clips can be inserted
                    iNumInvalidClips = (aNumInvalidClips > PLAYLIST_MAX_NUM_INVALID_CLIPS) ? PLAYLIST_MAX_NUM_INVALID_CLIPS : aNumInvalidClips;
                    iInvalidClipAtIndex = aInvalidClipAtIndex;
                    if (0 == iInvalidClipAtIndex)
                    {
                        iTestCaseName = _STRLIT_CHAR("Playback playlist... (Invalid clip at begining of playlist)");
                        iSinkFileNameSubString += _STRLIT_WCHAR("invld_clp_bgn_");
                    }
                    else
                    {
                        iTestCaseName = _STRLIT_CHAR("Playback playlist... (Invalid clip in middle of playlist)");
                        iSinkFileNameSubString += _STRLIT_WCHAR("invld_clp_mdl_");
                    }
                }
                else if (iMetadataTest)
                {
                    iTestCaseName = _STRLIT_CHAR("Playlist retrieve metadata...");
                    iSinkFileNameSubString += _STRLIT_WCHAR("mtdt_");
                }
                else if (iUnknownMimeTypeTest)
                {
                    iTestCaseName = _STRLIT_CHAR("Playback playlist (mimetype unknown for all clips)");
                    iSinkFileNameSubString += _STRLIT_WCHAR("unkn_mime_all_");
                }
                else if (iInvalidMimeTypeTest)
                {
                    iTestCaseName = _STRLIT_CHAR("Playback playlist (mimetype invalid for few clips)");
                    iSinkFileNameSubString += _STRLIT_WCHAR("invld_mime_few_");
                }
                else if (iReplaceTrackTest)
                {
                    iTestCaseName = _STRLIT_CHAR("Playlist replace clips in list...");
                    iSinkFileNameSubString += _STRLIT_WCHAR("rplc_");
                }
                else
                {
                    iTestCaseName = _STRLIT_CHAR("Playback playlist...");
                }

                if (iTestUpdateDataSource)
                {
                    iTestCaseName += _STRLIT_CHAR(" with partial playlist");
                    iSinkFileNameSubString += _STRLIT_WCHAR("ptl_");
                }
                else
                {
                    iTestCaseName += _STRLIT_CHAR(" with full playlist");
                    iSinkFileNameSubString += _STRLIT_WCHAR("fll_");
                }
                // Treating MP4 as default
                if (iFileType == PVMF_MIME_MP3FF)
                {
                    iSinkFileNameSubString += _STRLIT_WCHAR("mp3");

                    if (iInvalidMimeTypeTest)
                    {
                        iAlternateMimeType = PVMF_MIME_MPEG4FF;
                    }
                }
                else if (iFileType == PVMF_MIME_ASFFF)
                {
                    iSinkFileNameSubString += _STRLIT_WCHAR("wma");

                    if (iInvalidMimeTypeTest)
                    {
                        iAlternateMimeType = PVMF_MIME_MP3FF;
                    }
                }
                else //(iFileType == PVMF_MIME_MP4FF)
                {
                    iSinkFileNameSubString += _STRLIT_WCHAR("mp4");

                    if (iInvalidMimeTypeTest)
                    {
                        iAlternateMimeType = PVMF_MIME_MP3FF;
                    }
                }

                if (iUnknownMimeTypeTest)
                {
                    iAlternateMimeType = PVMF_MIME_FORMAT_UNKNOWN;
                }
            }
            oscl_memset(&iResultingPlaylistEventCounts, 0, sizeof(EventCounts));
            oscl_memset(&iResultingClipEventCounts, 0, sizeof(EventCounts) * MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING);
            oscl_memset(&iExpectedPlaylistEventCounts, 0, sizeof(EventCounts));
            oscl_memset(&iExpectedClipEventCounts, 0, sizeof(EventCounts) * PLAYLIST_MAX_NUMCLIPS);

            iNumClipsInPlaylist = CreatePlaylist(iFileName, iFileType, iPlaylist, iNumInvalidClips, iInvalidClipAtIndex, iTestMsgOutputFile, iStaticPlaylist);

            // if using static/default playlist, we know how many events to expect per clip and per playlist based on the testcase
            // default playlist has PLAYLIST_MAX_NUMCLIPS (= 5) clips
            // for InvalidMimeTypeTest, only the first clip will play
            //
            // for InvalidClipTest, only (PLAYLIST_MAX_NUMCLIPS - iNumInvalidClips) clips will play
            // the invalid clips starts iInvalidClipAtIndex
            //
            // for all the other tests, we expect all clips to play, one after another, to completion
            // the behavior does not change with full or partial playlist
            if (iStaticPlaylist)
            {
                if (iInvalidMimeTypeTest)
                {
                    // init_0, start_0, end_0, EOS
                    iExpectedPlaylistEventCounts.init = 1;
                    iExpectedPlaylistEventCounts.start = 1;
                    iExpectedPlaylistEventCounts.end = 1;
                    iExpectedPlaylistEventCounts.eos = 1;
                    iExpectedPlaylistEventCounts.total = 4;

                    iExpectedClipEventCounts[0].init = 1;
                    iExpectedClipEventCounts[0].start = 1;
                    iExpectedClipEventCounts[0].end = 1;
                    iExpectedClipEventCounts[0].total = 3;
                }
                else if (aInvalidClipTest)
                {
                    // middle of clip
                    // init_0, start_0, init_1, end_0, start_1, corrupt_2, corrupt_3, init_4, end_1, start_4, end_4, EOS
                    // beginning of clip
                    // corrupt_0, corrupt_1, corrupt_2, init_3, start_3, init_4, end_3, start_4, end_4, EOS
                    iExpectedPlaylistEventCounts.init = PLAYLIST_MAX_NUMCLIPS - iNumInvalidClips;
                    iExpectedPlaylistEventCounts.start = PLAYLIST_MAX_NUMCLIPS - iNumInvalidClips;
                    iExpectedPlaylistEventCounts.end = PLAYLIST_MAX_NUMCLIPS - iNumInvalidClips;
                    iExpectedPlaylistEventCounts.corrupt = iNumInvalidClips;
                    iExpectedPlaylistEventCounts.eos = 1;
                    iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                         iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.corrupt +
                                                         iExpectedPlaylistEventCounts.eos;



                    int32 firstInvalid = iInvalidClipAtIndex;
                    int32 lastInvalid = iInvalidClipAtIndex + iNumInvalidClips - 1;
                    for (int i = 0; i < PLAYLIST_MAX_NUMCLIPS; i++)
                    {
                        if ((i >= firstInvalid) && (i <= lastInvalid))
                        {
                            iExpectedClipEventCounts[i].corrupt = 1;
                            iExpectedClipEventCounts[i].total = 1;
                        }
                        else
                        {
                            iExpectedClipEventCounts[i].init = 1;
                            iExpectedClipEventCounts[i].start = 1;
                            iExpectedClipEventCounts[i].end = 1;
                            iExpectedClipEventCounts[i].total = 3;
                        }
                    }
                }
                else
                {
                    // init_0, start_0, init_1, end_0, start_1, init_2, end_1, start 2, init_3, end_2, start_3, init_4, end_3, start 4, end_4, EOS
                    iExpectedPlaylistEventCounts.init = PLAYLIST_MAX_NUMCLIPS;
                    iExpectedPlaylistEventCounts.start = PLAYLIST_MAX_NUMCLIPS;
                    iExpectedPlaylistEventCounts.end = PLAYLIST_MAX_NUMCLIPS;
                    iExpectedPlaylistEventCounts.eos = 1;
                    iExpectedPlaylistEventCounts.total = PLAYLIST_MAX_NUMCLIPS * 3 + 1;

                    for (int i = 0; i < PLAYLIST_MAX_NUMCLIPS; i++)
                    {
                        iExpectedClipEventCounts[i].init = 1;
                        iExpectedClipEventCounts[i].start = 1;
                        iExpectedClipEventCounts[i].end = 1;
                        iExpectedClipEventCounts[i].total = 3;
                    }
                }
            }
            for (int j = 0; j < PLAYLIST_MAX_NUMCLIPS; j++)
            {
                iSessionDuration[j] = 0;
            }
        }

        ~pvplayer_async_test_playlist_playback()
        {
            // free filename buffer memory if not using static playlist
            if (NULL != iPlaylist)
            {
                DestroyPlaylist(iPlaylist, iNumClipsInPlaylist);
                oscl_free(iPlaylist);
                iPlaylist = NULL;
            }

            while (!iResultingEvents.empty())
            {
                PlaylistClipTransitionEvent* entry = iResultingEvents.front();
                iResultingEvents.erase(iResultingEvents.begin());
                oscl_free(entry);
            }
        }

        void StartTest();
        void Run();

        void CommandCompleted(const PVCmdResponse& aResponse);
        void HandleErrorEvent(const PVAsyncErrorEvent& aEvent);
        void HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent);
        void PrintMetadataInfo(PVPMetadataList aMetadataKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator> aMetadataValueList, int32 aClipIndex,
                               bool aUpdateSessionDuration = false);
        void GetClipDuration();
        void ValidateEvents();

        PVTestState iState;

        PVPlayerInterface* iPlayer;
        PVPlayerDataSourceURL* iDataSource;
        PVPlayerDataSourceURL* iExtendedDataSource;

        PVPlayerDataSink* iDataSinkAudio;
        PvmiMIOControl* iMIOFileOutAudio;
        PVMFNodeInterface* iIONodeAudio;

        PVPlayerDataSink* iDataSinkVideo;
        PvmiMIOControl* iMIOFileOutVideo;
        PVMFNodeInterface* iIONodeVideo;

        PVCommandId iCurrentCmdId;
        PVMFSourceContextData* iSourceContextData;
        uint32 iNumEOSReceived;
        uint32 iNumClipsAdded;
        OSCL_wHeapString<OsclMemAllocator> iFileNameWStr;
        oscl_wchar iTmpWCharBuffer[512];
        int32 iNumInvalidClips;
        int32 iInvalidClipAtIndex;
        int32 iCorruptedClipsIdentified;

        // metadata related
        bool iMetadataTest;
        PVPMetadataList iCurrentPlayingMetadataKeyList;
        Oscl_Vector<PvmiKvp, OsclMemAllocator> iCurrentPlayingMetadataValueList;
        PVPMetadataList iCurrentInitializedMetadataKeyList;
        Oscl_Vector<PvmiKvp, OsclMemAllocator> iCurrentInitializedMetadataValueList;

        int32 iNumValues;
        uint32 iSessionDuration[PLAYLIST_MAX_NUMCLIPS];
        int32 iCurrentPlaybackClip;
        int32 iCurrentInitializedClip;
        bool iInitComplete;
        // test case related
        bool iUnknownMimeTypeTest;
        bool iInvalidMimeTypeTest;

        PVMFFormatType iAlternateMimeType;
        int32 iClipsWithInvalidMimeType;

        bool iTestUpdateDataSource;
        OSCL_wHeapString<OsclMemAllocator> iSinkFileNameSubString;
        char** iPlaylist;
        int32 iNumClipsInPlaylist;
        bool iStaticPlaylist;
        int32 iNumClipsInitialized;
        int32 iNumClipsStarted;
        bool iRetrieveCurrentPlayingMetadata;
        bool iRetrieveCurrentInitializedMetadata;
        bool iPrintCurrentPlayingMetadata;
        bool iPrintCurrentInitializedMetadata;

        bool iReplaceTrackTest;
        bool iReplaceClip;
        bool iSkipGetMetadata;
        PVSkipAndSeekTest iWhichTest;

        // event tracking
        Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator> iResultingEvents;
        EventCounts iResultingPlaylistEventCounts;
        EventCounts iResultingClipEventCounts[MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING];
        EventCounts iExpectedPlaylistEventCounts;
        EventCounts iExpectedClipEventCounts[PLAYLIST_MAX_NUMCLIPS];

        uint32 iReferencePCMFileSize;
        GaplessMetadataFormat iGaplessFormat;
        OSCL_wHeapString<OsclMemAllocator> iAudioSinkFileName;
        OSCL_wHeapString<OsclMemAllocator> iVideoSinkFileName;
};


/*! PlaylistSkipToLastTrackTest : @TODO : Update
 *  A test case to test playback of specified source with file output media IO node till EOS. Prints out the playback position
 *  player engine sends playback status events
 *  - Data Source: Specified by user of test case
 *  - Data Sink(s): Video[File Output MediaIO Interface Node-test_playlist_playback_%test_specific%_%SOURCEFILENAME%_video.dat]\n
 *                  Audio[File Output MediaIO Interface Node-test_playlist_playback_%test_specific%_%SOURCEFILENAME%_audio.dat]\n
 *                  Text[File Output MediaIO Interface Node-_test_playlist_playback_%test_specific%_%SOURCEFILENAME%_text.dat]
 *  - Sequence:
 *             -# CreatePlayer()
 *             -# AddDataSource()
 *             -# Init()
 *             -# AddDataSink() (audio)
 *             -# Prepare()
 *             -# Start()
 *             -# Playback for 2 Seconds
 *             -# UpdateDataSource()
 *             -# UpdateDataSource()
 *             -# UpdateDataSource()
 *             -# Wait For Track To Be Started
 *             -# @TODO ADD DETAILS HERE
 *             -# LOG PVMFInfoEndOfClipReached for all clips
 *             -# WAIT FOR EVENT PVMFInfoEndOfData
 *             -# Stop()
 *             -# RemoveDataSink() (audio)
 *             -# Reset()
 *             -# RemoveDataSource()
 *             -# DeletePlayer()
 *
 */
class pvplayer_async_test_playlist_seek_skip : public pvplayer_async_test_base
{
    public:
        pvplayer_async_test_playlist_seek_skip(PVPlayerAsyncTestParam aTestParam,
                                               PVSkipAndSeekTest aWhichTest,
                                               bool aTestUpdateDataSource = false,
                                               int32 aForwardSkip = -1,
                                               int32 aBackwardSkip = -1,
                                               bool aSkipAndSeek = false,
                                               int32 aTargetNPT = -1):
                pvplayer_async_test_base(aTestParam)
                , iPlayer(NULL)
                , iDataSource(NULL)
                , iExtendedDataSource(NULL)
                , iDataSinkAudio(NULL)
                , iIONodeAudio(NULL)
                , iMIOFileOutAudio(NULL)
                , iDataSinkVideo(NULL)
                , iIONodeVideo(NULL)
                , iMIOFileOutVideo(NULL)
                , iCurrentCmdId(0)
                , iSourceContextData(NULL)
                , iNumEOSReceived(0)
                , iNumClipsAdded(0)
                , iSkipAndSeekTest(aSkipAndSeek)
                , iTargetNPT(0)
                , iForwardSkip(aForwardSkip)
                , iBackwardSkip(aBackwardSkip)
                , iForwardAndBackwardSkip(false)
                , iWhichTest(aWhichTest)
                , iCurrentPlaybackClip(-1)
                , iCurrentInitializedClip(-1)
                , iPendingRepos(false)
                , iTestUpdateDataSource(aTestUpdateDataSource)
                , iPlaylist(NULL)
                , iNumClipsInPlaylist(0)
                , iStaticPlaylist(true)
                , iNumClipsInitialized(0)
                , iSkippedToClip(-1)
                , iNumClipsStarted(0)
                , iSetEOT(false)
                , iUpdateDataSource(false)
                , iSeekInCurrentTrack(false)
        {
            iSinkFileNameSubString = _STRLIT_WCHAR("_");

            iNumClipsInPlaylist = CreatePlaylist(iFileName, iFileType, iPlaylist, 0, -1, iTestMsgOutputFile, iStaticPlaylist);

            // when testing UpdateDataSource (aka starting with partial list, where to skip may be set at run time
            switch (iWhichTest)
            {
                case SkipFwdAndSeekTest:
                    iTargetNPT = aTargetNPT;
                    iForwardSkip = aForwardSkip;
                    iTestCaseName = _STRLIT_CHAR("Skip forward to Nth Track and start playback from M msec");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_fwd_ntrk_strt_msec_");
                    break;
                case SkipFwdTest:
                    iForwardSkip = aForwardSkip;
                    iTestCaseName = _STRLIT_CHAR("Skip forward to Nth Track");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_fwd_ntrk_");
                    break;
                case SkipFwdOneTrackTest:
                    iTestCaseName = _STRLIT_CHAR("Skip forward to Next Track Setting EOT");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_fwd_nxt_trk_");
                    break;
                case SkipToCurrentTrackTest:
                    iTestCaseName = _STRLIT_CHAR("Skip forward to current Track");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_to_curr_trk_");
                    break;
                case SkipFwdBeyondListTest:
                    iForwardSkip = iNumClipsInPlaylist + 5;
                    iTestCaseName = _STRLIT_CHAR("Skip forward to track beyond playlist");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_fwd_bynd_plst_");
                    break;
                case SkipFwdLastTrackTest:
                    iForwardSkip = iNumClipsInPlaylist - 1;
                    iTestCaseName = _STRLIT_CHAR("Skip forward last track in playlist");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_fwd_lst_trk_");
                    break;
                case SkipBwdOneTrackTest:
                    iTestCaseName = _STRLIT_CHAR("Skip backward One track in playlist");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_bk_1_trk_");
                    break;
                case SkipBwdBeyondListTest:
                    iBackwardSkip = -100;
                    iTestCaseName = _STRLIT_CHAR("Skip backward to track beyond playlist");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_bk_bynd_plst");
                    break;
                case SkipBwdAndSeekTest:
                    iTargetNPT = aTargetNPT;
                    iBackwardSkip = aBackwardSkip;
                    iTestCaseName = _STRLIT_CHAR("Skip backward to Nth Track and start playback from M msec");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_bk_ntrk_strt_msec_");
                    break;
                case SkipBwdTest:
                    iBackwardSkip = aBackwardSkip;
                    iTestCaseName = _STRLIT_CHAR("Skip backward to Nth Track");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_bk_ntrk_");
                    break;
                case SeekBeyondClipDurationTest:
                    iTestCaseName = _STRLIT_CHAR("Skip beyond clip duration...");
                    iSinkFileNameSubString += _STRLIT_WCHAR("skp_bynd_clp_drtn_");
                    break;
                case SeekInCurrentTrackTest:
                    iSeekInCurrentTrack = true;
                    iTestCaseName = _STRLIT_CHAR("Seek in current track test...");
                    iSinkFileNameSubString += _STRLIT_WCHAR("sk_crnt_trk_");
                    break;
                case PauseSkipToNextTrackResumeTest:
                    iTestCaseName = _STRLIT_CHAR("Pause, skip to next track, and Resume test...");
                    iSinkFileNameSubString += _STRLIT_WCHAR("pause_skp_fwd_1_trk_resume_");
                    break;
                default:
                    iTestCaseName = _STRLIT_CHAR("Invalid Test Case ID");
                    break;
            }

            if (iTestUpdateDataSource)
            {
                iTestCaseName += _STRLIT_CHAR(" with partial playlist");
                iSinkFileNameSubString += _STRLIT_WCHAR("ptl_");
            }
            else
            {
                iTestCaseName += _STRLIT_CHAR(" with full playlist");
                iSinkFileNameSubString += _STRLIT_WCHAR("fll_");
            }
            // Treating MP4 as default
            if (iFileType == PVMF_MIME_MP3FF)
            {
                iSinkFileNameSubString += _STRLIT_WCHAR("mp3");
            }
            else if (iFileType == PVMF_MIME_ASFFF)
            {
                iSinkFileNameSubString += _STRLIT_WCHAR("wma");
            }
            else //(iFileType == PVMF_MIME_MP4FF)
            {
                iSinkFileNameSubString += _STRLIT_WCHAR("mp4");
            }

            if (iForwardSkip > 0 && iBackwardSkip > 0)
            {
                iForwardAndBackwardSkip = true;
            }
            if (iSkipAndSeekTest)
            {
                iTargetNPT = aTargetNPT;
            }

            oscl_memset(&iResultingPlaylistEventCounts, 0, sizeof(EventCounts));
            oscl_memset(&iResultingClipEventCounts, 0, sizeof(EventCounts) * MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING);
            oscl_memset(&iExpectedPlaylistEventCounts, 0, sizeof(EventCounts));
            oscl_memset(&iExpectedClipEventCounts, 0, sizeof(EventCounts) * PLAYLIST_MAX_NUMCLIPS);

            // if using static/default playlist, we know how many events to expect per clip and per playlist based on the testcase
            // default playlist has PLAYLIST_MAX_NUMCLIPS (= 5) clips
            // behavior may be different with full or partial playlist
            if (iStaticPlaylist)
            {
                switch (iWhichTest)
                {
                    case SkipFwdLastTrackTest:
                        if (iTestUpdateDataSource)
                        {
                            // partial
                            // init_0, start_0, init_1, start_1, init_2, end_1, start 2, init_3, end_2, start_3, init_4, end_3, start 4, end_4, EOS
                            iExpectedPlaylistEventCounts.init = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.start = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.end = PLAYLIST_MAX_NUMCLIPS - 1;
                            iExpectedPlaylistEventCounts.eos = 1;
                            iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                                 iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.eos;

                            iExpectedClipEventCounts[0].init = 1;
                            iExpectedClipEventCounts[0].start = 1;
                            iExpectedClipEventCounts[0].total = 2;

                            for (int i = 1; i < PLAYLIST_MAX_NUMCLIPS; i++)
                            {
                                iExpectedClipEventCounts[i].init = 1;
                                iExpectedClipEventCounts[i].start = 1;
                                iExpectedClipEventCounts[i].end = 1;
                                iExpectedClipEventCounts[i].total = 3;
                            }
                        }
                        else
                        {
                            // full
                            // init_0, start_0, init_4, start_4, end_4, EOS
                            iExpectedPlaylistEventCounts.init = 2;
                            iExpectedPlaylistEventCounts.start = 2;
                            iExpectedPlaylistEventCounts.end = 1;
                            iExpectedPlaylistEventCounts.eos = 1;
                            iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                                 iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.eos;

                            iExpectedClipEventCounts[0].init = 1;
                            iExpectedClipEventCounts[0].start = 1;
                            iExpectedClipEventCounts[0].total = 2;

                            iExpectedClipEventCounts[4].init = 1;
                            iExpectedClipEventCounts[4].start = 1;
                            iExpectedClipEventCounts[4].end = 1;
                            iExpectedClipEventCounts[4].total = 3;
                        }
                        break;
                    case SkipFwdBeyondListTest:
                        if (iTestUpdateDataSource)
                        {
                            // partial
                            // init_0, start_0, init_1
                            iExpectedPlaylistEventCounts.init = 2;
                            iExpectedPlaylistEventCounts.start = 1;
                            iExpectedPlaylistEventCounts.total = 3;

                            iExpectedClipEventCounts[0].init = 1;
                            iExpectedClipEventCounts[0].start = 1;
                            iExpectedClipEventCounts[0].total = 2;

                            iExpectedClipEventCounts[1].init = 1;
                            iExpectedClipEventCounts[1].total = 1;
                        }
                        else
                        {
                            // full
                            // init_0, start_0
                            iExpectedPlaylistEventCounts.init = 1;
                            iExpectedPlaylistEventCounts.start = 1;
                            iExpectedPlaylistEventCounts.total = 2;

                            iExpectedClipEventCounts[0].init = 1;
                            iExpectedClipEventCounts[0].start = 1;
                            iExpectedClipEventCounts[0].total = 2;
                        }
                        break;
                    case SkipFwdOneTrackTest:
                    case PauseSkipToNextTrackResumeTest:
                        // same for full and partial
                        // init_0, start_0, init_1, start_1, init_2, end_1, start_2, EOT
                        iExpectedPlaylistEventCounts.init = 3;
                        iExpectedPlaylistEventCounts.start = 3;
                        iExpectedPlaylistEventCounts.end = 1;
                        iExpectedPlaylistEventCounts.eot = 1;
                        iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                             iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.eot;


                        iExpectedClipEventCounts[0].init = 1;
                        iExpectedClipEventCounts[0].start = 1;
                        iExpectedClipEventCounts[0].total = 2;

                        iExpectedClipEventCounts[1].init = 1;
                        iExpectedClipEventCounts[1].start = 1;
                        iExpectedClipEventCounts[1].end = 1;
                        iExpectedClipEventCounts[1].total = 3;

                        iExpectedClipEventCounts[2].init = 1;
                        iExpectedClipEventCounts[2].start = 1;
                        iExpectedClipEventCounts[2].total = 2;
                        break;
                    case SkipBwdOneTrackTest:
                    {
                        // same for full and partial
                        // init_0, start_0, init_1, end_0, start_1, init_2, init_0, start_0, init_1, end_0, start_1, init_2, end_1,
                        //   start 2, init_3, end_2, start_3, init_4, end_3, start 4, end_4, EOS
                        iExpectedPlaylistEventCounts.init = PLAYLIST_MAX_NUMCLIPS + 3;
                        iExpectedPlaylistEventCounts.start = PLAYLIST_MAX_NUMCLIPS + 2;
                        iExpectedPlaylistEventCounts.end = PLAYLIST_MAX_NUMCLIPS + 1;
                        iExpectedPlaylistEventCounts.eos = 1;
                        iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                             iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.eos;

                        iExpectedClipEventCounts[0].init = 2;
                        iExpectedClipEventCounts[0].start = 2;
                        iExpectedClipEventCounts[0].end = 2;
                        iExpectedClipEventCounts[0].total = 6;

                        iExpectedClipEventCounts[1].init = 2;
                        iExpectedClipEventCounts[1].start = 2;
                        iExpectedClipEventCounts[1].end = 1;
                        iExpectedClipEventCounts[1].total = 5;

                        iExpectedClipEventCounts[2].init = 2;
                        iExpectedClipEventCounts[2].start = 1;
                        iExpectedClipEventCounts[2].end = 1;
                        iExpectedClipEventCounts[2].total = 4;

                        for (int i = 3; i < PLAYLIST_MAX_NUMCLIPS; i++)
                        {
                            iExpectedClipEventCounts[i].init = 1;
                            iExpectedClipEventCounts[i].start = 1;
                            iExpectedClipEventCounts[i].end = 1;
                            iExpectedClipEventCounts[i].total = 3;
                        }
                    }
                    break;
                    case SkipFwdAndSeekTest:
                        if (iTestUpdateDataSource)
                        {
                            // partial
                            // init_0, start_0, init_1, start_1, init_2, end_1, start 2, init_3, end_2, start_3, init_4, end_3, start 4, end_4, EOS
                            iExpectedPlaylistEventCounts.init = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.start = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.end = PLAYLIST_MAX_NUMCLIPS - 1;
                            iExpectedPlaylistEventCounts.eos = 1;
                            iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                                 iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.eos;

                            iExpectedClipEventCounts[0].init = 1;
                            iExpectedClipEventCounts[0].start = 1;
                            iExpectedClipEventCounts[0].total = 2;

                            for (int i = 1; i < PLAYLIST_MAX_NUMCLIPS; i++)
                            {
                                iExpectedClipEventCounts[i].init = 1;
                                iExpectedClipEventCounts[i].start = 1;
                                iExpectedClipEventCounts[i].end = 1;
                                iExpectedClipEventCounts[i].total = 3;
                            }
                        }
                        else
                        {
                            // full
                            // init_0, start_0, init_3, start_3, init_4, end_3, start_4, end_4, EOS
                            iExpectedPlaylistEventCounts.init = 3;
                            iExpectedPlaylistEventCounts.start = 3;
                            iExpectedPlaylistEventCounts.end = 2;
                            iExpectedPlaylistEventCounts.eos = 1;
                            iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                                 iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.eos;

                            iExpectedClipEventCounts[0].init = 1;
                            iExpectedClipEventCounts[0].start = 1;
                            iExpectedClipEventCounts[0].total = 2;

                            for (int i = 3; i < PLAYLIST_MAX_NUMCLIPS; i++)
                            {
                                iExpectedClipEventCounts[i].init = 1;
                                iExpectedClipEventCounts[i].start = 1;
                                iExpectedClipEventCounts[i].end = 1;
                                iExpectedClipEventCounts[i].total = 3;
                            }

                        }
                        break;
                    case SkipToCurrentTrackTest:
                        if (iTestUpdateDataSource)
                        {
                            // partial
                            // init_0, start_0, init_1, init_0, init_1, end_0, start_1, init_2, end_1, start 2, init_3, end_2,
                            //   start_3, init_4, end_3, start 4, end_4, EOS
                            iExpectedPlaylistEventCounts.init = PLAYLIST_MAX_NUMCLIPS + 2;
                            iExpectedPlaylistEventCounts.start = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.end = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.eos = 1;
                            iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                                 iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.eos;

                            for (int i = 0; i < PLAYLIST_MAX_NUMCLIPS; i++)
                            {
                                iExpectedClipEventCounts[i].init = 1;
                                iExpectedClipEventCounts[i].start = 1;
                                iExpectedClipEventCounts[i].end = 1;
                                iExpectedClipEventCounts[i].total = 3;
                            }
                            iExpectedClipEventCounts[0].init += 1;
                            iExpectedClipEventCounts[0].total += 1;

                            iExpectedClipEventCounts[1].init += 1;
                            iExpectedClipEventCounts[1].total += 1;
                        }
                        else
                        {
                            // full
                            // init_0, start_0, init_1, end_0, start_1, init_2, end_1, start 2, init_3, end_2,
                            //   start_3, init_4, end_3, start 4, end_4, EOS
                            iExpectedPlaylistEventCounts.init = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.start = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.end = PLAYLIST_MAX_NUMCLIPS;
                            iExpectedPlaylistEventCounts.eos = 1;
                            iExpectedPlaylistEventCounts.total = iExpectedPlaylistEventCounts.init + iExpectedPlaylistEventCounts.start +
                                                                 iExpectedPlaylistEventCounts.end + iExpectedPlaylistEventCounts.eos;

                            for (int i = 0; i < PLAYLIST_MAX_NUMCLIPS; i++)
                            {
                                iExpectedClipEventCounts[i].init = 1;
                                iExpectedClipEventCounts[i].start = 1;
                                iExpectedClipEventCounts[i].end = 1;
                                iExpectedClipEventCounts[i].total = 3;
                            }
                        }
                        break;
                    case SeekInCurrentTrackTest:
                    case SeekBeyondClipDurationTest:
                    {
                        // init_0, start_0, init_1, end_0, start_1, init_2, end_1, start 2, init_3, end_2, start_3, init_4, end_3, start 4, end_4, EOS
                        iExpectedPlaylistEventCounts.init = PLAYLIST_MAX_NUMCLIPS;
                        iExpectedPlaylistEventCounts.start = PLAYLIST_MAX_NUMCLIPS;
                        iExpectedPlaylistEventCounts.end = PLAYLIST_MAX_NUMCLIPS;
                        iExpectedPlaylistEventCounts.eos = 1;
                        iExpectedPlaylistEventCounts.total = PLAYLIST_MAX_NUMCLIPS * 3 + 1;

                        for (int j = 0; j < PLAYLIST_MAX_NUMCLIPS; j++)
                        {
                            iExpectedClipEventCounts[j].init = 1;
                            iExpectedClipEventCounts[j].start = 1;
                            iExpectedClipEventCounts[j].end = 1;
                            iExpectedClipEventCounts[j].total = 3;
                        }
                    }
                    break;
                    default:
                        break;
                } // end switch
            } // end if static playlist

            for (int j = 0; j < PLAYLIST_MAX_NUMCLIPS; j++)
            {
                iSessionDuration[j] = 0;
            }
        }

        ~pvplayer_async_test_playlist_seek_skip()
        {
            // free filename buffer memory if not using static playlist
            if (NULL != iPlaylist)
            {
                DestroyPlaylist(iPlaylist, iNumClipsInPlaylist);
                oscl_free(iPlaylist);
                iPlaylist = NULL;
            }

            while (!iResultingEvents.empty())
            {
                PlaylistClipTransitionEvent* entry = iResultingEvents.front();
                iResultingEvents.erase(iResultingEvents.begin());
                oscl_free(entry);
            }
        }

        int32 AddDataSource();
        void StartTest();
        void Run();

        void CommandCompleted(const PVCmdResponse& aResponse);
        void HandleErrorEvent(const PVAsyncErrorEvent& aEvent);
        void HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent);

        void ValidateEvents();

        PVTestState iState;

        PVPlayerInterface* iPlayer;
        PVPlayerDataSourceURL* iDataSource;
        PVPlayerDataSourceURL* iExtendedDataSource;

        PVPlayerDataSink* iDataSinkAudio;
        PVMFNodeInterface* iIONodeAudio;
        PvmiMIOControl* iMIOFileOutAudio;
        PVPlayerDataSink* iDataSinkVideo;
        PVMFNodeInterface* iIONodeVideo;
        PvmiMIOControl* iMIOFileOutVideo;

        PVCommandId iCurrentCmdId;
        PVMFSourceContextData* iSourceContextData;
        uint32 iNumEOSReceived;
        uint32 iNumClipsAdded;
        OSCL_wHeapString<OsclMemAllocator> iFileNameWStr;
        oscl_wchar iTmpWCharBuffer[512];

        bool iSkipAndSeekTest;
        uint32 iTargetNPT;
        int32 iForwardSkip;
        int32 iBackwardSkip;
        bool iForwardAndBackwardSkip;
        PVSkipAndSeekTest iWhichTest;

        PVPMetadataList iCurrentInitializedMetadataKeyList;
        Oscl_Vector<PvmiKvp, OsclMemAllocator> iCurrentInitializedMetadataValueList;
        int32 iNumValues;
        uint32 iSessionDuration[PLAYLIST_MAX_NUMCLIPS];
        int32 iCurrentPlaybackClip;
        int32 iCurrentInitializedClip;
        bool iPendingRepos;

        bool iTestUpdateDataSource;
        OSCL_wHeapString<OsclMemAllocator> iSinkFileNameSubString;

        char** iPlaylist;
        int32 iNumClipsInPlaylist;
        bool iStaticPlaylist;
        int32 iNumClipsInitialized;
        int32 iSkippedToClip;
        int32 iNumClipsStarted;
        bool iSetEOT;
        bool iUpdateDataSource;
        bool iSeekInCurrentTrack;

        // event tracking
        Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator> iResultingEvents;
        EventCounts iResultingPlaylistEventCounts;
        EventCounts iResultingClipEventCounts[MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING];
        EventCounts iExpectedPlaylistEventCounts;
        EventCounts iExpectedClipEventCounts[PLAYLIST_MAX_NUMCLIPS];
};


// gapless testcases
/*!
 *  A test case to test the normal engine sequence of playing default source with gapless metadata till end of clip
 *  - Data Source: Passed in parameter
 *  - Data Sink(s): Video[FileOutputNode-test_player_validate_gapless_[SRCFILENAME]_video.dat]\n
 *                  Audio[FileOutputNode-test_player_validate_gapless_[SRCFILENAME]_audio.dat]\n
 *  - Sequence
 *             -# CreatePlayer()
 *             -# AddDataSource()
 *             -# Init()
 *             -# AddDataSink() (video)
 *             -# AddDataSink() (audio)
 *             -# AddDataSink() (text)
 *             -# Prepare()
 *             -# Start()
 *             -# WAIT FOR EOS OR 180 SEC TIMEOUT
 *             -# Stop()
 *             -# RemoveDataSink() (video)
 *             -# RemoveDataSink() (audio)
 *             -# RemoveDataSink() (text)
 *             -# Reset()
 *             -# RemoveDataSource()
 *             -# DeletePlayer()
 *
 */
class pvplayer_async_test_validate_gapless : public pvplayer_async_test_base
{
    public:
        pvplayer_async_test_validate_gapless(PVPlayerAsyncTestParam aTestParam, GaplessMetadataFormat aGaplessFormat):
                pvplayer_async_test_base(aTestParam)
                , iPlayer(NULL)
                , iDataSource(NULL)
                , iDataSinkVideo(NULL)
                , iIONodeVideo(NULL)
                , iMIOFileOutVideo(NULL)
                , iDataSinkAudio(NULL)
                , iIONodeAudio(NULL)
                , iMIOFileOutAudio(NULL)
                , iDataSinkText(NULL)
                , iIONodeText(NULL)
                , iMIOFileOutText(NULL)
                , iCurrentCmdId(0)
                , iGaplessFormat(GAPLESS_FORMAT_ITUNES_MP3)
                , iReferencePCMFileSize(0)
                , iNewFileName(NULL)
        {
            iGaplessFormat = aGaplessFormat;
            iTempFileName = SOURCENAME_PREPEND_STRING;

            switch (iGaplessFormat)
            {
                case GAPLESS_FORMAT_LAME_MP3:
                    iTestCaseName = _STRLIT_CHAR("Validate Gapless LAME MP3");
                    iTempFileName += DEFAULT_GAPLESS_LAME_MP3_CLIP;
                    iFileType = PVMF_MIME_MP3FF;
                    iReferencePCMFileSize = DEFAULT_GAPLESS_LAME_MP3_PCM_OUTPUT_SIZE;
                    iSinkFileNameSubString += _STRLIT_WCHAR("LAME_mp3");
                    break;

                case GAPLESS_FORMAT_ITUNES_AAC:
                    iTestCaseName = _STRLIT_CHAR("Validate Gapless iTunes AAC");
                    iTempFileName += DEFAULT_GAPLESS_ITUNES_AAC_CLIP;
                    iFileType = PVMF_MIME_MPEG4FF;
                    iReferencePCMFileSize = DEFAULT_GAPLESS_ITUNES_AAC_PCM_OUTPUT_SIZE;
                    iSinkFileNameSubString += _STRLIT_WCHAR("iTunes_aac");
                    break;

                default:
                    iGaplessFormat = GAPLESS_FORMAT_ITUNES_MP3;
                    // falling through
                case GAPLESS_FORMAT_ITUNES_MP3:
                    iTestCaseName = _STRLIT_CHAR("Validate Gapless iTunes MP3");
                    iTempFileName += DEFAULT_GAPLESS_ITUNES_MP3_CLIP;
                    iFileType = PVMF_MIME_MP3FF;
                    iReferencePCMFileSize = DEFAULT_GAPLESS_ITUNES_MP3_PCM_OUTPUT_SIZE;
                    iSinkFileNameSubString += _STRLIT_WCHAR("iTunes_mp3");
                    break;
            }
            iFileName = iTempFileName.get_str();

            // if there is a path delimiter,
            // need to replace with OSCL_FILE_CHAR_PATH_DELIMITER
            iNewFileName = (char *)oscl_malloc(oscl_strlen(iFileName) + 1);
            uint32 i = 0;
            for (i = 0; i < oscl_strlen(iFileName); i++)
            {
                // look for backward slash and forward slash
                if ((iFileName[i] == 0x5C) || (iFileName[i] == 0x2F))
                {
                    iNewFileName[i] = OSCL_FILE_CHAR_PATH_DELIMITER[0];
                }
                else
                {
                    iNewFileName[i] = iFileName[i];
                }
            }
            iNewFileName[i] = 0x00;
        }

        ~pvplayer_async_test_validate_gapless()
        {
            if (NULL != iNewFileName)
            {
                oscl_free(iNewFileName);
                iNewFileName = NULL;
            }
        }

        void StartTest();
        void Run();

        void CommandCompleted(const PVCmdResponse& aResponse);
        void HandleErrorEvent(const PVAsyncErrorEvent& aEvent);
        void HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent);

        enum PVTestState
        {
            STATE_CREATE,
            STATE_ADDDATASOURCE,
            STATE_INIT,
            STATE_ADDDATASINK_VIDEO,
            STATE_ADDDATASINK_AUDIO,
            STATE_PREPARE,
            STATE_START,
            STATE_PAUSE,
            STATE_EOSNOTREACHED,
            STATE_STOP,
            STATE_REMOVEDATASINK_VIDEO,
            STATE_REMOVEDATASINK_AUDIO,
            STATE_RESET,
            STATE_RESUME,
            STATE_REMOVEDATASOURCE,
            STATE_CLEANUPANDCOMPLETE
        };

        PVTestState iState;

        PVPlayerInterface* iPlayer;
        PVPlayerDataSourceURL* iDataSource;
        PVPlayerDataSink* iDataSinkVideo;
        PVMFNodeInterface* iIONodeVideo;
        PvmiMIOControl* iMIOFileOutVideo;
        PVPlayerDataSink* iDataSinkAudio;
        PVMFNodeInterface* iIONodeAudio;
        PvmiMIOControl* iMIOFileOutAudio;
        PVPlayerDataSink* iDataSinkText;
        PVMFNodeInterface* iIONodeText;
        PvmiMIOControl* iMIOFileOutText;
        PVCommandId iCurrentCmdId;

        GaplessMetadataFormat iGaplessFormat;
        OSCL_wHeapString<OsclMemAllocator> iGaplessWFileName;
        oscl_wchar iTmpWCharBuffer[512];
        OSCL_wHeapString<OsclMemAllocator> iAudioSinkFileName;
        uint32 iReferencePCMFileSize;
        char* iNewFileName;
        OSCL_wHeapString<OsclMemAllocator> iSinkFileNameSubString;
        OSCL_HeapString<OsclMemAllocator> iTempFileName;
};




#endif //TEST_PV_PLAYER_ENGINE_TESTSET_PLAYLIST_H_INCLUDED

