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

#include "pv_video_config_parser.h"
#include "m4v_config_parser.h"
#include "oscl_mem.h"

#include "oscl_dll.h"

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



OSCL_DLL_ENTRY_POINT_DEFAULT()

int32 GetNAL_Config(uint8** bitstream, int32* size);

OSCL_EXPORT_REF int16 pv_video_config_parser(pvVideoConfigParserInputs *aInputs, pvVideoConfigParserOutputs *aOutputs)
{
    if (aInputs->iMimeType == PVMF_MIME_M4V) //m4v
    {
        int32 width, height, display_width, display_height = 0;
        int32 profile_level = 0;
        int16 retval = 0;
        retval = iDecodeVOLHeader(aInputs->inPtr, aInputs->inBytes, &width, &height, &display_width, &display_height, &profile_level);
        if (retval != 0)
        {
            return retval;
        }
        aOutputs->width  = (uint32)display_width;
        aOutputs->height = (uint32)display_height;
        aOutputs->profile = (uint32)profile_level; // for mp4, profile/level info is packed
        aOutputs->level = 0;
    }
    else if (aInputs->iMimeType == PVMF_MIME_H2631998 ||
             aInputs->iMimeType == PVMF_MIME_H2632000)//h263
    {
        // Nothing to do for h263
        aOutputs->width  = 0;
        aOutputs->height = 0;
        aOutputs->profile = 0;
        aOutputs->level = 0;
    }
    else if (aInputs->iMimeType == PVMF_MIME_H264_VIDEO ||
             aInputs->iMimeType == PVMF_MIME_H264_VIDEO_MP4 ||
             aInputs->iMimeType == PVMF_MIME_H264_VIDEO_RAW) //avc
    {
        int32 width, height, display_width, display_height = 0;
        int32 profile_idc, level_idc = 0;
        int16 retval;

        if (aInputs->inBytes <= 0 || aInputs->inPtr == NULL)
        {
            aOutputs->width = 0;
            aOutputs->height = 0;
            aOutputs->level = 0;
            aOutputs->profile = 0;

            return 0;  // return success for now as the config info may be provided later.
        }
        // check codec info and get settings
        retval = iGetAVCConfigInfo(aInputs->inPtr,
                                   aInputs->inBytes,
                                   & width,
                                   & height,
                                   & display_width,
                                   & display_height,
                                   & profile_idc,
                                   & level_idc);
        if (retval != 0)
        {
            return retval;
        }
        aOutputs->width  = (uint32)display_width;
        aOutputs->height = (uint32)display_height;
        aOutputs->profile = (uint32)profile_idc;
        aOutputs->level = (uint32) level_idc;
    }
    else if (aInputs->iMimeType == PVMF_MIME_WMV || aInputs->iMimeType == PVMF_MIME_VC1) //wmv or vc1
    {
        uint32 dwdat;
        uint16 wdat;
        uint8 bdat;
        uint8 *pData = aInputs->inPtr;


        LoadDWORD(dwdat, pData); // Window width
        LoadDWORD(dwdat, pData); // Window height
        LoadBYTE(bdat, pData);
        LoadWORD(wdat, pData);  // Size of image info.

        // BITMAPINFOHEADER
        LoadDWORD(dwdat, pData); // Size of BMAPINFOHDR
        LoadDWORD(dwdat, pData);
        aOutputs->width = dwdat;
        LoadDWORD(dwdat, pData);
        aOutputs->height = dwdat;

        /* WMV1 and WMV2 are not supported. Rejected here. */
        /* Code to Check if comp is WMV1 then return not supported */
        pData += 4;
        LoadDWORD(dwdat, pData);

        if (dwdat == FOURCC_PRDY)
        {
            pData += aInputs->inBytes - 35; // check the last 4 bytes;
            LoadDWORD(dwdat, pData); // real FOURCC
            pData = aInputs->inPtr + 31;
        }

        uint32 NewCompression = dwdat;
        uint32 NewSeqHeader = 0;
        uint32 NewProfile = 0;
        uint32 NewFrameRate, NewBitRate;

        // in case of WMV store "Compression Type as Level"
        aOutputs->level = NewCompression;

        if (aInputs->iMimeType == PVMF_MIME_VC1)
        {
            if (NewCompression != FOURCC_WMV3 &&
                    NewCompression != FOURCC_WVC1 &&
                    NewCompression != FOURCC_WMVA)
            {
                return -1;
            }

        }
        else
        {
            if (NewCompression != FOURCC_WMV1 &&
                    NewCompression != FOURCC_WMV2 &&
                    NewCompression != FOURCC_WMV3 &&
                    NewCompression != FOURCC_WVC1 &&
                    NewCompression != FOURCC_WMVA &&
                    NewCompression != FOURCC_MP42 &&
                    NewCompression != FOURCC_MP43 &&
                    NewCompression != FOURCC_mp42 &&
                    NewCompression != FOURCC_mp43 &&
                    NewCompression != FOURCC_MP4S)
            {
                return -1;
            }
        }
        // get profile etc.
        // Check sequence header
        switch (NewCompression)
        {
            case FOURCC_WMV3:
            {
                // start fresh
                pData = aInputs->inPtr;
                pData += (11 + 40); //sizeof(BITMAPINFOHEADER); // position to sequence header

                LoadDWORD(dwdat, pData);
                NewSeqHeader = dwdat; // this is little endian read sequence header

                uint32 YUV411flag, Spriteflag;

                // For FOURCC_WMV3
                uint32 YUV411;
                uint32 SpriteMode;
                uint32 LoopFilter;
                uint32 Xintra8Switch;
                uint32 MultiresEnabled;
                uint32 X16bitXform;
                uint32 UVHpelBilinear;
                uint32 ExtendedMvMode;
                uint32 DQuantCodingOn;
                uint32 XformSwitch;
                uint32 DCTTable_MB_ENABLED;
                uint32 SequenceOverlap;
                uint32 StartCode;
                uint32 PreProcRange;
                uint32 NumBFrames;
                uint32 ExplicitSeqQuantizer;
                uint32 Use3QPDZQuantizer = 0;
                uint32 ExplicitFrameQuantizer = 0;


                bool bValidProfile = true;

                NewProfile = (NewSeqHeader & 0xC0) >> 6; // 0 - simple , 1- main, 3 - complex, 2-forbidden

                YUV411flag = (NewSeqHeader & 0x20) >> 5;
                Spriteflag = (NewSeqHeader & 0x10) >> 4;
                if ((YUV411flag != 0) || (Spriteflag != 0))
                    return -1;

                YUV411              = (uint32)YUV411flag;
                SpriteMode          = (uint32)Spriteflag;
                LoopFilter          = (NewSeqHeader & 0x800) >> 11;
                Xintra8Switch       = (NewSeqHeader & 0x400) >> 10;
                MultiresEnabled     = (NewSeqHeader & 0x200) >> 9;
                X16bitXform         = (NewSeqHeader & 0x100) >> 8;
                UVHpelBilinear      = (NewSeqHeader & 0x800000) >> 23;
                ExtendedMvMode      = (NewSeqHeader & 0x400000) >> 22;
                DQuantCodingOn      = (NewSeqHeader & 0x300000) >> 20;
                XformSwitch         = (NewSeqHeader & 0x80000) >> 19;
                DCTTable_MB_ENABLED = (NewSeqHeader & 0x40000) >> 18;
                SequenceOverlap     = (NewSeqHeader & 0x20000) >> 17;
                StartCode           = (NewSeqHeader & 0x10000) >> 16;
                PreProcRange            = (NewSeqHeader & 0x80000000) >> 31;
                NumBFrames          = (NewSeqHeader & 0x70000000) >> 28;
                ExplicitSeqQuantizer    = (NewSeqHeader & 0x8000000) >> 27;
                if (ExplicitSeqQuantizer)
                    Use3QPDZQuantizer = (NewSeqHeader & 0x4000000) >> 26;
                else
                    ExplicitFrameQuantizer = (NewSeqHeader & 0x4000000) >> 26;

                NewFrameRate = (NewSeqHeader & 0x0E) >> 1 ; // from 2 to 30 fps (in steps of 4)
                NewFrameRate = 4 * NewFrameRate + 2; // (in fps)

                NewBitRate = (((NewSeqHeader & 0xF000) >> 24) | ((NewSeqHeader & 0x01) << 8));  // from 32 to 2016 kbps in steps of 64kbps
                NewBitRate = 64 * NewBitRate + 32; // (in kbps)

                // Verify Profile
                if (!SpriteMode)
                {
                    if (NewProfile == WMV3_SIMPLE_PROFILE)
                    {
                        bValidProfile = (Xintra8Switch == 0) &&
                                        (X16bitXform == 1) &&
                                        (UVHpelBilinear == 1) &&
                                        (StartCode == 0) &&
                                        (LoopFilter == 0) &&
                                        (YUV411 == 0) &&
                                        (MultiresEnabled == 0) &&
                                        (DQuantCodingOn == 0) &&
                                        (NumBFrames == 0) &&
                                        (PreProcRange == 0);

                    }
                    else if (NewProfile == WMV3_MAIN_PROFILE)
                    {
                        bValidProfile = (Xintra8Switch == 0) &&
                                        (X16bitXform == 1);
                    }
                    else if (NewProfile == WMV3_PC_PROFILE)
                    {
                        // no feature restrictions for complex profile.
                    }

                    if (!bValidProfile)
                    {
                        return -1;
                    }
                }
                else
                {
                    if (Xintra8Switch   ||
                            DCTTable_MB_ENABLED  ||
                            YUV411 ||
                            LoopFilter ||
                            ExtendedMvMode ||
                            MultiresEnabled ||
                            UVHpelBilinear ||
                            DQuantCodingOn ||
                            XformSwitch ||
                            StartCode ||
                            PreProcRange ||
                            ExplicitSeqQuantizer ||
                            Use3QPDZQuantizer ||
                            ExplicitFrameQuantizer)
                        return -1;
                }
            }
            break;

            case FOURCC_WMVA:
            case FOURCC_WVC1:
            {
                pData = aInputs->inPtr;
                pData += (11 + 40 + ASFBINDING_SIZE); //sizeof(BITMAPINFOHEADER); // position to sequence header

                LoadDWORD(dwdat, pData);
                NewSeqHeader = dwdat; // this is little endian read sequence header

                int32 iPrefix;
                //ignore start code prefix
                iPrefix = NewSeqHeader & 0xFF;
                if (iPrefix != 0) return -1;
                iPrefix = (NewSeqHeader & 0xFF00) >> 8;
                if (iPrefix != 0) return -1;
                iPrefix = (NewSeqHeader & 0xFF0000) >> 16;
                if (iPrefix != 1) return -1;
                iPrefix = (NewSeqHeader & 0xFF000000) >> 24;
                if (iPrefix != SC_SEQ) return -1;

                LoadDWORD(dwdat, pData);
                NewSeqHeader = dwdat;

                NewProfile = (NewSeqHeader & 0xC0) >> 6;
                if (NewProfile != 3)
                    return -1;
            }
            break;

            default:

                NewProfile = 0;
                break;
        }

        aOutputs->profile = NewProfile;

    }
    else if (aInputs->iMimeType == PVMF_MIME_REAL_VIDEO) //rv
    {
        // use this as default value, let decoder discover the actual size and perform port reconfig
        aOutputs->width = 128;
        aOutputs->height = 96;
        aOutputs->profile = 0;
        aOutputs->level = 0;
    }
    else
    {
        return -1;
    }
    return 0;
}


/* This function finds a nal from the SC's, moves the bitstream pointer to the beginning of the NAL unit, returns the
    size of the NAL, and at the same time, updates the remaining size in the bitstream buffer that is passed in */
int32 GetNAL_Config(uint8** bitstream, int32* size)
{
    int i = 0;
    int j;
    uint8* nal_unit = *bitstream;
    int count = 0;

    /* find SC at the beginning of the NAL */
    while (nal_unit[i++] == 0 && i < *size)
    {
    }

    if (nal_unit[i-1] == 1)
    {
        *bitstream = nal_unit + i;
    }
    else
    {
        j = *size;
        *size = 0;
        return j;  // no SC at the beginning, not supposed to happen
    }

    j = i;

    /* found the SC at the beginning of the NAL, now find the SC at the beginning of the next NAL */
    while (i < *size)
    {
        if (count == 2 && nal_unit[i] == 0x01)
        {
            i -= 2;
            break;
        }
        else if (count == 3 && nal_unit[i] == 0x01)
        {
            i -= 3;
            break;
        }

        if (nal_unit[i])
            count = 0;
        else
            count++;
        i++;
    }

    *size -= i;
    return (i - j);
}
