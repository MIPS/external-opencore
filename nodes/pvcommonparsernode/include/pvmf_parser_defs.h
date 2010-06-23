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
#ifndef PVMF_PARSER_DEFS_H_INCLUDED
#define PVMF_PARSER_DEFS_H_INCLUDED

typedef enum
{
    PVMF_PARSER_NODE_PORT_TYPE_OUTPUT = 1
} PVMFParserNodePortType;

// Capability mime strings
#define PVMF_COMMONPARSER_PORT_OUTPUT_FORMATS "x-pvmf/port/formattype"
#define PVMF_COMMONPARSER_PORT_OUTPUT_FORMATS_VALTYPE "x-pvmf/port/formattype;valtype=char*"

// Metadata supported by the node
static const char PVMETADATA_TRACKINFO_AUDIO_FORMAT_KEY[] = "track-info/audio/format";
static const char PVMF_COMMON_PARSER_NODE_RANDOM_ACCESS_DENIED_KEY[] = "random-access-denied";

// Tuneables for different fileformats
#define PVMF_NUM_FRAMES_AMR 10
#define PVMF_MAX_FRAME_SIZE_AMR_NB 32
#define PVMF_MAX_FRAME_SIZE_AMR_WB 61

#define PVMF_NUM_FRAMES_WAV 1102

#endif

