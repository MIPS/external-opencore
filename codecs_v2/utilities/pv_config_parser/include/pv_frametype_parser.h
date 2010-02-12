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
#ifndef PV_VIDEO_FRAMETYPE_PARSER_H_INCLUDED
#define PV_VIDEO_FRAMETYPE_PARSER_H_INCLUDED

#include "oscl_base.h"
#include "oscl_types.h"
#include "pvmf_format_type.h"
#include "pv_config_bitstream.h"

//!  Definitions of macros included here .
//!  These are mainly mask values for different Frame types for
//!  different Video stream types , likes , MP4,H.263 ,  H.264,  WMV .
/*********************************************************************************/

//! MP4

#define   MP4_START_CODE  0x000001b6 // Frame Start Code

//! H.263

#define   H263_START_CODE 0x00000020 //0x00000020 // Frame Start code

//! WMV

#define WMV_SC_START_CODE  0x0000010F  // Sequence Start Code

#define WMV_FRM_START_CODE 0x0000010D  // Frame  Start Code

#define MAIN_PROFILE      0x1
#define SIMPLE_PROFILE    0x0
#define ADVANCE_PROFILE   0x3

#define WMV_NUMFCMBITS 0x2

#define WMV_ADVNUMPTYPEBITS 0x4

#define WMV_ADVNUMFPTYPEBITS 0x3

typedef enum
{
    WMVIFRAME = 0,
    WMVPFRAME,
    WMVBFRAME,
    WMVBIFRAME,
    WMVSKIPPED,
    // interlaced types in which fields differ
    WMVIPFRAME,
    WMVPIFRAME,
    WMVBBIFRAME,
    WMVBIBFRAME,
    WMVUNKNOWNFRAME
} WMVFrameType;

#define  KEY_FRAMETYPE_MP4 0x2


/*********************************************************************************/

//! Definitions of  structures used

/*********************************************************************************/

typedef struct
{
    uint32 CompressionType;
    uint32 Profile;
    uint32 InterlacedSource; // relevant for WMVA
    uint32 FrameInterpFlag;  // relevant for WMV3
    uint32 PreProcRange;      // relevant for WMV3
    uint32 NumBFrames;        // relevant for WMV3
} WmvSeqheaderInfo;

typedef struct pvVideoParserInput
{
    uint8 *inBuf;                 //! Pointer to input Video Stream,ie, MP4,H.263, H.264,WMV
    uint32 inBufSize;             //! Size of input pointer
    uint32 inBytesOffset;         //! pointer offset in Number of bytes . is it Required ??
    PVMFFormatType iMimeType;     //! Type of Input Video stream like H.263
    WmvSeqheaderInfo *SeqHeaderInfo;

} pvVideoGetFrameTypeParserInputs;

typedef struct
{
    uint32 FrameTypeInfo;         //!  Returns  various Frame Types,like I,P ,B for Input Video
    bool isKeyFrame;
    //!  BitStream like H.263

} pvVideoGetFrameTypeParserOutputs;

/*********************************************************************************/
//! This function returns true or false depending on whether or not the specified format is supported by
//! the frametype parser
/**
  \param Input arugments :  PVMFFormatType
  \Returns         :        boolean
  */

OSCL_IMPORT_REF bool pv_frametype_parser_format_supported(PVMFFormatType aFormat);

//! This is external interface of functions for extracting diffrent frame type information like
//! I , P , B for input video  bitstream like H.263
//! This interface takes
/**
  \param Input arugments :  pvVideoGetFrameTypeParserInputs and pvVideoGetFrameTypeParserOutputs
                            as described above
  \Returns         :        type as  sucess or fails indication.
  \calls           :  PVMFStatus PVAVCDecGetNALType(),PVMFStatus PVWMVDecGetFrameType()
  */
OSCL_IMPORT_REF PVMFStatus pv_detect_keyframe(pvVideoGetFrameTypeParserInputs *aInputs,
        pvVideoGetFrameTypeParserOutputs *aOutputs);

//! These are internal interface of this module.

/*********************************************************************************/
//! This function fills in the WMV parameters based on configuration data
//! the frametype parser
/**
  \param Input arugment :  config_data - pointer to config data pointer
  \param Input argument :  config_data_len - length of config data
  \param Output          :  wmvSeqheaderInfo pointer to WmvSeqheaderInfo structure that is to be filled in
  \Returns         :        boolean
  */

OSCL_IMPORT_REF bool pv_get_wmv_seq_hdr_info(uint8 *bitstream, int size, WmvSeqheaderInfo *wmvseqhdrinfo);

/**
    This function sniffs the nal_unit_type such that users can call corresponding APIs.
    \param "bitstream" "Pointer to the beginning of a NAL unit (start with forbidden_zero_bit, etc.)."
    \param "size"  "size of the bitstream (NumBytesInNALunit + 1)."
    \param "nal_unit_type" "Pointer to the return value of nal unit type."
    \return "DEC_SUCCESS if success, DEC_FAIL otherwise."
    */
PVMFStatus PVAVCDecGetNALType(uint8 *bitstream, int size, int32 *nal_type, int32 *nal_ref_idc);


/**
    This function sniffs the PTYPE or FPTYPE such that users can call corresponding APIs.
 \param "WmvSeqheaderInfo"  pass informations like profile , interlaceflag information
         such as Simple , main or Advance
    \param "bitstream" "Pointer to the beginning of a NAL unit (start with forbidden_zero_bit, etc.)."
    \param "code" "Pointer to the return value of PTYPE or FPTYPE."
    \return "DEC_SUCCESS if success, DEC_FAIL otherwise."
    */

//!PVMFStatus PVWMVDecGetFrameType(uint32 NewProfile,DecBitstream *bitstream,uint*code);

PVMFStatus PVWMVDecGetFrameType(WmvSeqheaderInfo  *SeqheaderInfo,
                                DecBitstream *bitstream, uint32* aFrameType);

/**
    This function sniffs the PLUSTYPE or EXTENDED PLUSTYPE such that users can call corresponding APIs.
 \param "bitstream" "Pointer to the beginning of a NAL unit (start with forbidden_zero_bit, etc.)."
    \param "code" "Pointer to the return value of PLUSTYPE or EXTENDED PLUSTYPE."
    \return "DEC_SUCCESS if success, DEC_FAIL otherwise."
    */

PVMFStatus PVH263DecGetFrameType(DecBitstream *bitstream, uint* code);


#endif //PV_VIDEO_GETFRAMETYPE_PARSER_H_INCLUDED

