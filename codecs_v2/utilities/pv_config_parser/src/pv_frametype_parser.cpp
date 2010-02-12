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

#include "pv_frametype_parser.h"

OSCL_EXPORT_REF bool pv_frametype_parser_format_supported(PVMFFormatType aFormat)
{
    if (aFormat == PVMF_MIME_M4V || //m4v
            aFormat == PVMF_MIME_H2631998 || //h263
            aFormat == PVMF_MIME_H2632000 ||//h263
            aFormat == PVMF_MIME_H264_VIDEO || //avc
            aFormat == PVMF_MIME_H264_VIDEO_MP4 || //avc
            aFormat == PVMF_MIME_WMV) // wmv
    {
        return true;
    }
    else
    {
        return false;
    }
}

OSCL_EXPORT_REF PVMFStatus pv_detect_keyframe(pvVideoGetFrameTypeParserInputs *aInputs,
        pvVideoGetFrameTypeParserOutputs *aOutputs)
{

    DecBitstream *bitstream;

    bitstream = (DecBitstream*)(oscl_malloc(sizeof(DecBitstream)));

    PVMFStatus status = PVMFSuccess;

    uint code = 0;

    aOutputs->isKeyFrame = false;

    if (aInputs->inBufSize == 0 || aInputs->inBuf == NULL) // Check Allocation.
    {
        status = PVMFErrBadHandle;
        goto exit;
    }

    // Initialize Frame buffer
    status = PvConfigBitstreamInit(bitstream, aInputs->inBuf, aInputs->inBufSize);
    // Align Frame Buffer
    status = PvConfigBitstreamByteAlign(bitstream);

    if (aInputs->iMimeType == PVMF_MIME_M4V) //m4v
    {
        aInputs->inBytesOffset = 0;

        status = PvConfigBitstreamReadBits(bitstream, 32, &code);

        if (code != MP4_START_CODE) // Check Frame start code
        {
            status = PVMFErrCorrupt;
            goto exit;
        }

        status = PvConfigBitstreamReadBits(bitstream, 2, &code);

        aOutputs->FrameTypeInfo = (uint32) code; //vop_coding_type means:

        if (aOutputs->FrameTypeInfo == 0)
        {
            aOutputs->isKeyFrame = true;
        }
        /*
            0 0 -I VOP
            0 1 -P VOP
            1 0 -B VOP
            1 1 -S VOP
         */



    }
    else if (aInputs->iMimeType == PVMF_MIME_H2631998 ||
             aInputs->iMimeType == PVMF_MIME_H2632000)//h263
    {

        status = PvConfigBitstreamReadBits(bitstream, 22, &code);

        if (code != H263_START_CODE) // Check Frame start code
        {
            status = PVMFErrCorrupt;
            goto exit;
        }


        status = PVH263DecGetFrameType(bitstream, &code);
        aOutputs->FrameTypeInfo = (uint32) code;
        if (aOutputs->FrameTypeInfo == 0)
        {
            aOutputs->isKeyFrame = true;
        }


    }

    else if (aInputs->iMimeType == PVMF_MIME_H264_VIDEO ||
             aInputs->iMimeType == PVMF_MIME_H264_VIDEO_MP4) //avc
    {

        int32 nal_unit_type = 0;
        int32 nal_ref_idc = 0;

        status = PVAVCDecGetNALType(aInputs->inBuf, 2, &nal_unit_type, &nal_ref_idc);

        /*  nal_unit_type = 1;  Coded slice of a non-IDR picture
            nal_unit_type = 5;  Coded slice of an IDR picture
            nal_unit_type =20;  Coded slice of a non-IDR picture in scalable extension
            nal_unit_type =21;  Coded slice of a IDR picture in scalable extension
        */

        aOutputs->FrameTypeInfo = nal_unit_type;

        if (nal_unit_type == 5)
        {
            aOutputs->isKeyFrame = true;
        }

    }
    else if (aInputs->iMimeType == PVMF_MIME_WMV) //wmv
    {
        if (aInputs->SeqHeaderInfo == NULL)
        {
            OSCL_ASSERT(false);

            status = PVMFErrBadHandle;
            goto exit;
        }



        status = PVWMVDecGetFrameType(aInputs->SeqHeaderInfo, bitstream, &aOutputs->FrameTypeInfo);
        if (status != PVMFSuccess)
        {
            goto exit;
        }

        if (aOutputs->FrameTypeInfo == WMVIFRAME)
        {
            aOutputs->isKeyFrame = true;
        }


    }

    else
    {
        status = PVMFErrNotSupported;
        goto exit;
    }

exit:
    oscl_free(bitstream);
    return status;

}

#define GetUnalignedWord( pb, w ) \
            (w) = ((uint16) *(pb + 1) << 8) + *pb;

#define GetUnalignedDword( pb, dw ) \
            (dw) = ((uint32) *(pb + 3) << 24) + \
                   ((uint32) *(pb + 2) << 16) + \
                   ((uint16) *(pb + 1) << 8) + *pb;

#define GetUnalignedWordEx( pb, w )     GetUnalignedWord( pb, w ); (pb) += sizeof(uint16);
#define GetUnalignedDwordEx( pb, dw )   GetUnalignedDword( pb, dw ); (pb) += sizeof(uint32);
#define GetUnalignedQwordEx( pb, qw )   GetUnalignedQword( pb, qw ); (pb) += sizeof(uint64);

#define LoadBYTE( b, p )    b = *(uint8 *)p;  p += sizeof( uint8 )

#define LoadWORD( w, p )    GetUnalignedWordEx( p, w )
#define LoadDWORD( dw, p )  GetUnalignedDwordEx( p, dw )
#define LoadQWORD( qw, p )  GetUnalignedQwordEx( p, qw )

#ifndef MAKEFOURCC_WMC
#define MAKEFOURCC_WMC(ch0, ch1, ch2, ch3) \
        ((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |   \
        ((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define mmioFOURCC_WMC(ch0, ch1, ch2, ch3)  MAKEFOURCC_WMC(ch0, ch1, ch2, ch3)
#endif

#define FOURCC_WMV1     mmioFOURCC_WMC('W','M','V','1')
#define FOURCC_WMV2     mmioFOURCC_WMC('W','M','V','2')
#define FOURCC_WMV3     mmioFOURCC_WMC('W','M','V','3')
#define FOURCC_WMVA     mmioFOURCC_WMC('W','M','V','A')
#define FOURCC_WVC1     mmioFOURCC_WMC('W','V','C','1')
//For WMVA
#define ASFBINDING_SIZE                   1   // size of ASFBINDING is 1 byte
#define FOURCC_MP42     mmioFOURCC_WMC('M','P','4','2')
#define FOURCC_MP43     mmioFOURCC_WMC('M','P','4','3')
#define FOURCC_mp42     mmioFOURCC_WMC('m','p','4','2')
#define FOURCC_mp43     mmioFOURCC_WMC('m','p','4','3')
#define FOURCC_MP4S     mmioFOURCC_WMC('M','P','4','S')

// For PlayReady
#define FOURCC_PRDY     mmioFOURCC_WMC('P','R','D','Y')

//For WMV3
enum { NOT_WMV3 = -1, WMV3_SIMPLE_PROFILE, WMV3_MAIN_PROFILE, WMV3_PC_PROFILE, WMV3_SCREEN };

//For WMVA
#define ASFBINDING_SIZE                   1   // size of ASFBINDING is 1 byte
#define SC_SEQ          0x0F
#define SC_ENTRY        0x0E


OSCL_EXPORT_REF bool pv_get_wmv_seq_hdr_info(uint8 *bitstream, int size, WmvSeqheaderInfo *wmvseqhdrinfo)
{
    uint32 dwdat;
    uint8 bdat;
    uint8 *pData = bitstream;



    pData += 27;
    LoadDWORD(dwdat, pData);

    if (dwdat == FOURCC_PRDY)
    {
        pData += size - 35; // check the last 4 bytes;
        LoadDWORD(dwdat, pData); // real FOURCC
        pData = bitstream + 31;
    }

    uint32 NewCompression = dwdat;
    uint32 NewSeqHeader = 0;
    uint32 NewProfile = 0;


    // Check sequence header
    switch (NewCompression)
    {
        case FOURCC_WMV3:
        {
            // start fresh
            pData = bitstream;
            pData += (11 + 40); //sizeof(BITMAPINFOHEADER); // position to sequence header

            LoadDWORD(dwdat, pData);
            NewSeqHeader = dwdat; // this is little endian read sequence header


            // For FOURCC_WMV3
            uint32 PreProcRange;
            uint32 NumBFrames;
            uint32 FrameInterpFlag;

            NewProfile = (NewSeqHeader & 0xC0) >> 6; // 0 - simple , 1- main, 3 - complex, 2-forbidden
            PreProcRange        = (NewSeqHeader & 0x80000000) >> 31;
            NumBFrames          = (NewSeqHeader & 0x70000000) >> 28;
            FrameInterpFlag     = (NewSeqHeader & 0x02000000) >> 25;

            // record relevant information for this compression
            wmvseqhdrinfo->PreProcRange = PreProcRange;
            wmvseqhdrinfo->NumBFrames = NumBFrames;
            wmvseqhdrinfo->FrameInterpFlag = FrameInterpFlag;

        }
        break;

        case FOURCC_WMVA:
        case FOURCC_WVC1:
        {
            pData = bitstream;
            pData += (11 + 40 + ASFBINDING_SIZE); //sizeof(BITMAPINFOHEADER); // position to sequence header

            //ignore start code prefix
            // if there are problems with it - they were caught earlier (0,0,1,SEQ_SC)
            LoadDWORD(dwdat, pData);


            uint8 InterlacedSource;


            // this is little endian read sequence header
            LoadDWORD(dwdat, pData);
            NewSeqHeader = dwdat;

            NewProfile = (NewSeqHeader & 0xC0) >> 6;


            LoadBYTE(bdat, pData); // skip 1 byte
            LoadBYTE(bdat, pData);

            InterlacedSource = (bdat & 0x40) >> 6;

            // record relevant information about this compression
            wmvseqhdrinfo->InterlacedSource = (uint32) InterlacedSource;

        }
        break;

        case FOURCC_WMV2:
        case FOURCC_WMV1:
            NewProfile = 0;
            break;

        default:

            NewProfile = 0;
            return false;
    }

    wmvseqhdrinfo->CompressionType = NewCompression;
    wmvseqhdrinfo->Profile = NewProfile;

    return true;
}

/**======================================================================== */
/* @Function : PVAVCGetNALType()           */
/* @Date     : 11/4/2003             */
/* @Purpose  : Sniff NAL type from the bitstream        */
/* @In/out   :                */
/* @Return   : PVMFSuccess if succeed, PVMFFailure if fail.     */
/* @Modified :                */
/* ======================================================================== */
PVMFStatus PVAVCDecGetNALType(uint8 *bitstream, int size,
                              int32 *nal_type, int32 *nal_ref_idc)
{
    int32 forbidden_zero_bit;
    if (size > 0)
    {
        forbidden_zero_bit = bitstream[0] >> 7;
        if (forbidden_zero_bit != 0)
            return PVMFErrCorrupt;
        *nal_ref_idc = (bitstream[0] & 0x60) >> 5;
        *nal_type = bitstream[0] & 0x1F;
        return PVMFSuccess;
    }

    return PVMFFailure;
}

/** ======================================================================= */
/* @Function : PVH263DecGetFrameType()             */
/* @Date     : 05/2/2009             */
/* @Purpose  : Sniff PLUSTYPE or extended PLUSTYPE type from the bitstream */
/* @In/out   :                                                             */
/*  @Input    : Pointer to type DecBitstream *bitstream :                   */
/*  @           Structure of bitstram buffer.                               */
/*  @OutPut   : Pointer to PTYPE or FPTYPE : uint code                      */
/*                                                              */
/* @Return   : PVMFSuccess if succeed, PVMFFailure if fail.     */
/* @Modified :                */
/* ======================================================================== */

PVMFStatus PVH263DecGetFrameType(DecBitstream *bitstream, uint* code)
{

    PVMFStatus status = PVMFSuccess;

    // Skip reading 8 bit TR field
    PvConfigBitstreamFlushBits(bitstream, 8); // advance by a byte

    // Read first 8 bit  of PTYPE

    status = PvConfigBitstreamReadBits(bitstream, 8, code);

    *code = *code & 0x7; //  Extract only 6-8 Bits out of it.

    if (*code == 0x7) // If  extended PLUSTYPE is indicated.
    {
        status = PvConfigBitstreamReadBits(bitstream, 3, code); // Read UFEP

        if (*code == 0x0) // only mandatary part of  PLUSTYPE
        {
            status = PvConfigBitstreamReadBits(bitstream, 3, code);
        }
        else if (*code == 0x1) // opional + mandatary part of PLUSTYPE
        {

            PvConfigBitstreamFlushBits(bitstream, 18); // advance by 26 bits ,ie, 5(from 9-13 bits of PTYPE)
            // + 3(UFEP) + 18(OPPTYPE)
            // Read first 3 bits of MPPTYPE
            status = PvConfigBitstreamReadBits(bitstream, 3, code);
        }

        else
        {
            return  PVMFErrCorrupt; // Reserved Bit fields

        }
    }
    else  // If  extended PLUSTYPE not indicated.
    {
        status = PvConfigBitstreamRead1Bit(bitstream, code);
    }
    return status;

}

/** ======================================================================= */
/* @Function : PVWMVDecGetFrameType()             */
/* Date     : 05/2/2009             */
/* @Purpose  : Sniff PTYPE or FPTYPE type from the bitstream    */
/* @In/out   :                                                             */
/*    @Input  :WmvSeqheaderInfo  : Sequence information like                */
/*    @         Profile,interlaceflag information,                          */
/*    @         pointer to type AVCDecBitstream *bitstream :                */
/*    @         Structure of bitstram buffer.                               */
/*    @OutPut :Pointer to PTYPE or FPTYPE : uint32*code                       */
/*                                                              */
/* @Return   : PVMFSuccess if succeed, PVMFFailure if fail.     */
/* @Modified :                */
/* ======================================================================== */

// Look-up tables for frame types
// Width Of PTYPE in advance profile is variable
/**********************************
 PTYPE VLC Picture Type
 110x      I
 0xxx      P
 10xx      B
 1110      BI
 1111     Skipped
*********************************/
static const uint32 FrameTypeAdvancedProgressiveTable[16] = {WMVPFRAME, //0000
        WMVPFRAME, //0001
        WMVPFRAME, //0010
        WMVPFRAME, //0011
        WMVPFRAME, //0100
        WMVPFRAME, //0101
        WMVPFRAME, //0110
        WMVPFRAME, //0111
        WMVBFRAME, //1000
        WMVBFRAME, //1001
        WMVBFRAME, //1010
        WMVBFRAME, //1011
        WMVIFRAME, //1100
        WMVIFRAME, //1101
        WMVBIFRAME, //1110
        WMVSKIPPED //1111
                                                            };

/***************************************
                 FPTYPE First Field Picture Type Second Field Picture Type
                 000         I                             I
                 001         I                             P
                 010         P                             I
                 011         P                             P
                 100         B                             B
                 101         B                             BI
                 110         BI                             B
                 111         BI                             BI
***********************************************/
static const uint32 FrameTypeAdvancedInterlacedTable[8] = { WMVIFRAME, // 000
        WMVIPFRAME, // 001
        WMVPIFRAME, // 010
        WMVPFRAME, //011
        WMVBFRAME, //100
        WMVBBIFRAME, //101
        WMVBIBFRAME, //110
        WMVBIFRAME, //111
                                                          };

PVMFStatus PVWMVDecGetFrameType(WmvSeqheaderInfo  *SeqheaderInfo,
                                DecBitstream *bitstream, uint32* aFrameType)

{
    uint32 FrameCodingMode = 0xFF;
    uint code = 0;

    PVMFStatus status = PVMFSuccess;
    *aFrameType = WMVUNKNOWNFRAME;

    if ((SeqheaderInfo->CompressionType != FOURCC_WMV3) &&
            (SeqheaderInfo->CompressionType != FOURCC_WMV2) &&
            (SeqheaderInfo->CompressionType != FOURCC_WMV1) &&
            (SeqheaderInfo->CompressionType != FOURCC_WMVA) &&
            (SeqheaderInfo->CompressionType != FOURCC_WVC1))
    {
        return PVMFErrNotSupported;
    }


    if ((SeqheaderInfo->CompressionType == FOURCC_WMVA) || (SeqheaderInfo->CompressionType == FOURCC_WVC1))  // Advance Profile
    {
        if (bitstream->data_end_pos < 5)
        {
            // there has to be at least 5 bytes of data
            return PVMFErrCorrupt;
        }

        // check if the start code is present (by peeking into the bitstream)
        status = PvConfigBitstreamShowBits(bitstream, 32, &code);

        if (code == WMV_FRM_START_CODE) // Check Frame start code
        {
            // skip the start code by actually reading
            status = PvConfigBitstreamReadBits(bitstream, 32, &code);
        }

        if (SeqheaderInfo->InterlacedSource)
        {
            status = PvConfigBitstreamReadBits(bitstream, 1, &code); // Read Frame Coding Mode

            if (0 == code)
                FrameCodingMode = 0;  // PROGRESSIVE
            else
            {
                status = PvConfigBitstreamReadBits(bitstream, 1, &code);
                if (0 == code)
                    FrameCodingMode = 1; // INTERLACEFRAME;
                else
                    FrameCodingMode = 2; // INTERLACEFIELD
            }
        }
        else
            FrameCodingMode = 0; // PROGRESSIVE

        switch (FrameCodingMode)
        {
            case 0: //  Progressive Frame
            case 0x1: //Frame Interlace
            {

                code = 0;
                status = PvConfigBitstreamReadBits(bitstream, 4, &code);
                code &= 0xF;

                // read from look-up table
                *aFrameType = FrameTypeAdvancedProgressiveTable[code];

            }
            break;

            case 0x2:  // Field Interlace
            {

                code = 0;
                status = PvConfigBitstreamReadBits(bitstream, 3, &code);
                code &= 0x7;

                // read from look-up table
                *aFrameType = FrameTypeAdvancedInterlacedTable[code];


            }

            break;
            default:
                return PVMFErrCorrupt;
        }
    }
    else if (SeqheaderInfo->CompressionType == FOURCC_WMV3)
    {
        if (bitstream->data_end_pos < 1)
        {
            // there has to be at least 1 bytes of data
            return PVMFErrCorrupt;
        }

        if (SeqheaderInfo->FrameInterpFlag)
        {
            status = PvConfigBitstreamReadBits(bitstream, 1, &code); // skip interpfrm
        }
        status = PvConfigBitstreamReadBits(bitstream, 2, &code); // skip frmcnt

        if (SeqheaderInfo->PreProcRange) // we are already doing WMV3
        {
            status = PvConfigBitstreamReadBits(bitstream, 1, &code); // skip preprocfrm
        }

        code = 0;
        status = PvConfigBitstreamReadBits(bitstream, 1, &code);
        if (0 != code)
            // 1
            *aFrameType = WMVPFRAME;
        else
        {
            // 0
            if (SeqheaderInfo->NumBFrames > 0) // if main profile and B frame is there
            {
                /**************************************************
                Main Profile Picture Type VLC if MAXBFRAMES > 0
                PTYPE VLC Picture Type
                  1      P
                  01  I
                  00  B or BI
                *************************************************/
                code = 0;
                status = PvConfigBitstreamReadBits(bitstream, 1, &code);

                if (0 != code) // 01 I
                    *aFrameType = WMVIFRAME;

                else // 00 B or BI
                    *aFrameType = WMVBFRAME; // though could be WMVBIFRAME as well

            }
            else  // Simple and Main profile and if there is no B frame
            {
                /**************************************************
                    Simple/Main Profile Picture Type FLC if MAXBFRAMES = 0
                    PTYPE VLC Picture Type
                    1      P
                    0      I

                    *************************************************/
                *aFrameType = WMVIFRAME;
            }
        }
    }
    else if (SeqheaderInfo->CompressionType == FOURCC_WMV2)
    {
        if (bitstream->data_end_pos < 1)
        {
            // there has to be at least 1 bytes of data
            return PVMFErrCorrupt;
        }

        // this is FOURCC_WMV2
        code = 0;
        status = PvConfigBitstreamReadBits(bitstream, 1, &code);
        if (0 != code)
            // 1
            *aFrameType = WMVPFRAME;
        else
            *aFrameType = WMVIFRAME;

    }
    else
        // this is FOURCC_WMV1
    {
        code = 0;
        status = PvConfigBitstreamReadBits(bitstream, 2, &code);
        if (0 == code)
            // 1
            *aFrameType = WMVIFRAME;
        else
            *aFrameType = WMVPFRAME;
    }

    return status;
}

