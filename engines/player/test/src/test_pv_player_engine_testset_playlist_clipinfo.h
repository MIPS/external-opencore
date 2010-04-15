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
#ifndef TEST_PV_PLAYER_ENGINE_TESTSET_PLAYLIST_CLIPINFO_H_INCLUDED
#define TEST_PV_PLAYER_ENGINE_TESTSET_PLAYLIST_CLIPINFO_H_INCLUDED

#define PLAYLIST_MAX_NUMCLIPS 5
#define PLAYLIST_MAX_NUM_INVALID_CLIPS 5

#define MIN_VALID_CLIPS_IN_PLAYLIST 2
#define MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING 20


#define DEFAULT_GAPLESS_ITUNES_MP3_CLIP     "gapless\\album\\iTunes_mp3\\iTunes_gapless_track01.mp3"
#define DEFAULT_GAPLESS_LAME_MP3_CLIP       "gapless\\album\\LAME_mp3\\LAME_gapless_track01.mp3"
#define DEFAULT_GAPLESS_ITUNES_AAC_CLIP     "gapless\\album\\iTunes_aac\\iTunes_gapless_track01.m4a"

#define DEFAULT_GAPLESS_ITUNES_MP3_PCM_OUTPUT_SIZE  0xFF89C
#define DEFAULT_GAPLESS_LAME_MP3_PCM_OUTPUT_SIZE    0xFF89C
#define DEFAULT_GAPLESS_ITUNES_AAC_PCM_OUTPUT_SIZE  0xFF89C

#define DEFAULT_GAPLESS_ITUNES_MP3_PLAYLIST     "iTunes_mp3_gapless_playlist.txt"
#define DEFAULT_GAPLESS_LAME_MP3_PLAYLIST       "LAME_mp3_gapless_playlist.txt"
#define DEFAULT_GAPLESS_ITUNES_AAC_PLAYLIST     "iTunes_aac_gapless_playlist.txt"

#define DEFAULT_GAPLESS_ITUNES_MP3_PLAYLIST_PCM_OUTPUT_SIZE  0x43B8FC
#define DEFAULT_GAPLESS_LAME_MP3_PLAYLIST_PCM_OUTPUT_SIZE    0x43B8FC
#define DEFAULT_GAPLESS_ITUNES_AAC_PLAYLIST_PCM_OUTPUT_SIZE  0x43B8FC

#define DEFAULT_VALID_MP3_PLAYLIST     "test_valid_mp3_playlist.txt"
#define DEFAULT_VALID_MP4_PLAYLIST     "test_valid_mp4_playlist.txt"
#define DEFAULT_VALID_WMA_PLAYLIST     "test_valid_wma_playlist.txt"
#define DEFAULT_INVALID_MP3_PLAYLIST   "test_invalid_mp3_playlist.txt"
#define DEFAULT_INVALID_MP4_PLAYLIST   "test_invalid_mp4_playlist.txt"
#define DEFAULT_INVALID_WMA_PLAYLIST   "test_invalid_wma_playlist.txt"

#define PLAYLIST_READ_BUFFER_SIZE       6 * 1024

#endif

