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
#ifndef PV2WAY_FILE_NAMES_H_INCLUDED
#define PV2WAY_FILE_NAMES_H_INCLUDED



#define AUDIO_SAMPLING_FREQUENCY 8000
#define VIDEO_TIMESCALE 1000
#define VIDEO_FRAMEHEIGHT 144
#define VIDEO_FRAMEWIDTH 176
#define VIDEO_FRAMERATE 5

#define TEST_COMPRESSED
#ifdef TEST_COMPRESSED
#define LAYOUT_BUFFER_SIZE 1024
#define AUDIO_SAMPLING_FREQUENCY 8000
#define VIDEO_TIMESCALE 1000
#define VIDEO_FRAMEHEIGHT 144
#define VIDEO_FRAMEWIDTH 176
#define VIDEO_FRAMERATE 5


#else
#define LAYOUT_BUFFER_SIZE 1024
#define AUDIO_SAMPLING_FREQUENCY 8000
#define VIDEO_TIMESCALE 1000
#define VIDEO_FRAMEHEIGHT 144
#define VIDEO_FRAMEWIDTH 176
#define VIDEO_FRAMERATE 5

#define AUDIO_SOURCE_FILENAME _STRLIT("audio_in.pcm16")
#define AUDIO_SINK_FILENAME _STRLIT("audio_out.pcm16")
#define VIDEO_SOURCE_FILENAME _STRLIT("video_in.yuv420")
#define VIDEO_SINK_FILENAME _STRLIT("video_out.yuv420")
#endif


#define RX_LOGGER_TAG _STRLIT_CHAR("pvcommionode.rx.bin")
#define TX_LOGGER_TAG _STRLIT_CHAR("pvcommionode.tx.bin")
#define PVSIP2WAY_PROFILE _STRLIT_CHAR("pvSIP2Way")
#define PVSIPDEMO_ADDRESS _STRLIT_CHAR("sip:pvSIPDemo@")
#define PVSIPDEMO2_ADDRESS _STRLIT_CHAR("sip:pvSIPDemo2@")
#define PVSIP_DEFAULT_REALM _STRLIT_CHAR("pvrealm")



//#if OSCL_HAS_SYMBIAN_SUPPORT
#define AUDIO_SINK_FILENAME _STRLIT("audio_out.if2")
#define TEST_RX_LOG_FILENAME _STRLIT("commrx.bin")
#define TEST_TX_LOG_FILENAME _STRLIT("commtx.bin")
#define AUDIO_SOURCE3_FILENAME _STRLIT("audio_in.amr")
#define AUDIO_SOURCE_RAW_FILENAME _STRLIT("audio_in.pcm16")
#define VIDEO_SOURCE_YUV_FILENAME _STRLIT("video_in.yuv420")
#define VIDEO_SOURCE_H263_FILENAME _STRLIT("video_in.h263")
#define VIDEO_SOURCE_M4V_FILENAME _STRLIT("video_in.m4v")
#define AUDIO_SOURCE_FILENAME _STRLIT("audio_in.if2")
#define AUDIO_SINK_RAW_FILENAME _STRLIT("audio_out.pcm16")
#define VIDEO_SINK_YUV_FILENAME _STRLIT("video_out.yuv420")
#define VIDEO_SINK_H263_FILENAME _STRLIT("video_out.h263")
#define VIDEO_SINK_M4V_FILENAME _STRLIT("video_out.m4v")
#define VIDEO_SOURCE_FILENAME _STRLIT("video_in.h263")
#define VIDEO_SINK_FILENAME _STRLIT("video_out.h263")


#define AUDIO_SINK2_FILENAME _STRLIT("audio_ietf_out.dat")
#define VIDEO_PREVIEW_FILENAME _STRLIT("video_preview_out.dat")
#define RECORDED_CALL_FILENAME _STRLIT("recorded_call.mp4")
#define AUDIO_ONLY_PLAY_FILENAME _STRLIT("pv-amr-122_novisual.3gp")
#define AUDIO_H263_PLAY_FILENAME _STRLIT("pv-amr-122_h263-64.3gp")
#define AUDIO_MPEG4_PLAY_FILENAME _STRLIT("pv2-amr122_mpeg4-rvlcs-64.3gp")
#define H263_ONLY_PLAY_FILENAME _STRLIT("pv-noaudio_h263-64.3gp")
#define MPEG4_ONLY_PLAY_FILENAME _STRLIT("pv2-noaudio_mpeg4-rvlcs-64.3gp")
#define SQCIF_PLAY_FILENAME _STRLIT("sqcif1.3gp")
#define QVGA_PLAY_FILENAME _STRLIT("qvga.3gp")

#endif

