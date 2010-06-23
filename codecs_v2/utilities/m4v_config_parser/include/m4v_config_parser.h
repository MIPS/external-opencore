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
#ifndef PV_M4V_CONFIG_PARSER_H_INCLUDED
#define PV_M4V_CONFIG_PARSER_H_INCLUDED

#include "oscl_base.h"
#include "oscl_types.h"

#define USE_LATER 1  // for some code that will be needed in the future

#define MP4_INVALID_VOL_PARAM -1
#define SHORT_HEADER_MODE -4

#define M4V_MAX_LAYER 1

#define VISUAL_OBJECT_SEQUENCE_START_CODE   0x01B0
#define VISUAL_OBJECT_SEQUENCE_END_CODE     0x01B1
#define VISUAL_OBJECT_START_CODE   0x01B5
#define VO_START_CODE           0x8
#define VO_HEADER_LENGTH        32
#define VOL_START_CODE 0x12
#define VOL_START_CODE_LENGTH 28

#define GROUP_START_CODE    0x01B3
#define GROUP_START_CODE_LENGTH  32

#define VOP_ID_CODE_LENGTH      5
#define VOP_TEMP_REF_CODE_LENGTH    16

#define USER_DATA_START_CODE        0x01B2
#define USER_DATA_START_CODE_LENGTH 32

#define SHORT_VIDEO_START_MARKER        0x20
#define SHORT_VIDEO_START_MARKER_LENGTH  22

#define H263_DEFAULT_WIDTH  352
#define H263_DEFAULT_HEIGHT 288

/** Targeted profile and level to encode. */
enum M4VProfileLevel
{
    /* Non-scalable profile */
    SIMPLE_LEVEL0 = 0,
    SIMPLE_LEVEL1,
    SIMPLE_LEVEL2,
    SIMPLE_LEVEL3,
    SIMPLE_LEVEL4A,
    SIMPLE_LEVEL5,
    CORE_LEVEL1,
    CORE_LEVEL2,

    /* Scalable profile */
    SIMPLE_SCALABLE_LEVEL0,
    SIMPLE_SCALABLE_LEVEL1,
    SIMPLE_SCALABLE_LEVEL2,

    CORE_SCALABLE_LEVEL1,
    CORE_SCALABLE_LEVEL2,
    CORE_SCALABLE_LEVEL3,

    ADV_SIMPLE_LEVEL0,
    ADV_SIMPLE_LEVEL1,
    ADV_SIMPLE_LEVEL2,
    ADV_SIMPLE_LEVEL3,
    ADV_SIMPLE_LEVEL4,
    ADV_SIMPLE_LEVEL5
};

typedef struct
{
    uint8 *data;
    uint32 numBytes;
    uint32 bytePos;
    uint32 bitBuf;
    uint32 dataBitPos;
    uint32  bitPos;
} mp4StreamType;

/** This structure contains M4V config info */
typedef struct M4VConfigInfo
{
    /** Specifies the width in pixels of the encoded frames. IFrameWidth[0] is for
    base layer and iFrameWidth[1] is for enhanced layer. */
    int                 iFrameWidth[M4V_MAX_LAYER];

    /** Specifies the height in pixels of the encoded frames. IFrameHeight[0] is for
    base layer and iFrameHeight[1] is for enhanced layer. */
    int                 iFrameHeight[M4V_MAX_LAYER];


    /** Specifies the profile level value as parsed from stream */
    int32               iProfilelevelValue;


    /** Specifies the profile and level used to encode the bitstream. When present,
    other settings will be checked against the range allowable by this target profile
    and level. Fail may be returned from the Initialize call. */
    M4VProfileLevel  iProfileLevel;


    /** Specify short header mode in MPEG4 */
    bool                iShortHeader;

    /** Specifies whethere data partitioning mode is used or not. Has no meaning if encoding H.263 or short header */
    bool                iDataPartitioning;


    /** Specifies whether Resync markers are used or not Has no meaning if iDataPartitioning is on */
    bool                iResyncMarker;


    /** Specifies whether RVLC (reversible VLC) is to be used or not.
    */
    bool                iRVLCEnable;

    /** Sets the number of ticks per second used for timing information encoded in MPEG4 bitstream.*/
    int32                 iTimeIncRes;


} M4VConfigInfo;

/** This structure contains AVC config info */
typedef struct AVCConfigInfo
{
    //SPS
    int32               nProfile;
    int32               nLevel;
    uint32              nDisplayWidth;
    uint32              nDisplayHeight;
    uint32              nWidth;
    uint32              nHeight;
    uint32              nRefFrames;  // (num_ref_frames)
    bool                bFrameMBsOnly; // (frame_mbs_only_flag)

    //PPS
    bool                bEnableFMO;  //( if num_slice_groups_minus1 > 0)
    bool                bEnableRS; // (redundant_pic_cnt_present_flag)
    bool                bEntropyCodingCABAC; // (entropy_coding_mode_flag)
    bool                bMBAFF;  // (pic_order_present_flag)
    int32               nRefIdxl0ActiveMinus1;  // (num_ref_idx_l0_active_minus1)
    int32               nRefIdxl1ActiveMinus1; // (num_ref_idx_l1_active_minus1)
    bool                bWeightedPPrediction;  // (weighted_pred_flag)
    int32               nWeightedBipredictionMode;  // (weighted_bipred_idc)
    bool                bconstIpred;  // (constrained_intra_pred_flag)
    bool                bDirect8x8Inference;  // (transform_8x8_mode_flag)

    int32               nNumSliceGroups;  // num_slice_groups_minus1
    int32               nSliceGroupMapType; // slice_group_map_type

    bool                bConstrained_set3_flag;
    uint32              nMaxFrameNum;       //from log2_max_frame_num_minus4. Total Max Frames
    int32               pic_init_qp;        //pic_init_qp_minus26
    bool                bDblkFilterFlag;    //deblocking_filter_control_present_flag

} AVCConfigInfo;


int16 ShowBits(
    mp4StreamType *pStream,
    uint8 ucNBits,
    uint32 *pulOutData
);

int16 FlushBits(
    mp4StreamType *pStream,
    uint8 ucNBits
);

int16 ReadBits(
    mp4StreamType *pStream,
    uint8 ucNBits,
    uint32 *pulOutData
);

int16 ByteAlign(
    mp4StreamType *pStream
);

OSCL_IMPORT_REF int16 ParseM4VFSI(uint8* buffer, uint32 length,  M4VConfigInfo* iM4VConfigInfo);
OSCL_IMPORT_REF int16 ParseAVCFSI(uint8* buffer, uint32 length,  AVCConfigInfo* iAVCConfigInfo);

OSCL_IMPORT_REF int16 iDecodeVOLHeader(uint8 *buffer, int32 length, int32 *width, int32 *height, int32 *, int32 *, int32 *profilelevel);
OSCL_IMPORT_REF int16 iGetM4VConfigInfo(uint8 *buffer, int32 length, int32 *width, int32 *height, int32 *, int32 *);
int16 DecodeUserData(mp4StreamType *pStream);
OSCL_IMPORT_REF int16 iDecodeShortHeader(mp4StreamType *psBits, int32 *width, int32 *height, int32 *, int32 *);
OSCL_IMPORT_REF int16 iGetAVCConfigInfo(uint8 *buffer, int32 length, int32 *width, int32 *height, int32 *, int32 *, int32 *profile, int32 *level);

int32 FindNAL(uint8** nal_pnt, uint8* buffer, int32 length);
int16 DecodeSPS(mp4StreamType *psBits, AVCConfigInfo* iAVCConfigInfo);
#if USE_LATER
int32 DecodeHRD(mp4StreamType *psBits);
int32 DecodeVUI(mp4StreamType *psBits);
#endif
int32 DecodePPS(mp4StreamType *psBits, AVCConfigInfo* iAVCConfigInfo);

void ue_v(mp4StreamType *psBits, uint32 *codeNum);
void se_v(mp4StreamType *psBits, int32 *value);
void Parser_EBSPtoRBSP(uint8 *nal_unit, int32 *size);
bool getNALType(uint8 *bitstream, int size, int32 *nal_type);

#endif //PV_M4V_CONFIG_PARSER_H_INCLUDED

