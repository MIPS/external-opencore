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

#include "m4v_config_parser.h"
#include "oscl_mem.h"
#include "oscl_dll.h"
OSCL_DLL_ENTRY_POINT_DEFAULT()

#define PV_CLZ(A,B) while (((B) & 0x8000) == 0) {(B) <<=1; A++;}

static const uint32 mask[33] =
{
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
    0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
    0xffffffff
};

int32 LocateFrameHeader(uint8 *ptr, int32 size)
{
    int32 count = 0;
    int32 i = size;

    if (size < 1)
    {
        return 0;
    }
    while (i--)
    {
        if ((count > 1) && (*ptr == 0x01))
        {
            i += 2;
            break;
        }

        if (*ptr++)
            count = 0;
        else
            count++;
    }
    return (size - (i + 1));
}

void movePointerTo(mp4StreamType *psBits, int32 pos)
{
    uint32 byte_pos;
    if (pos < 0)
    {
        pos = 0;
    }

    byte_pos = pos >> 3;
    if (byte_pos > psBits->numBytes)
    {
        byte_pos = psBits->numBytes;
    }

    psBits->bytePos = byte_pos & -4;
    psBits->dataBitPos = psBits->bytePos << 3;
    FlushBits(psBits, ((pos & 0x7) + ((byte_pos & 0x3) << 3)));
}

int16 SearchNextM4VFrame(mp4StreamType *psBits)
{
    int16 status = 0;
    uint8 *ptr;
    int32 i;
    uint32 initial_byte_aligned_position = (psBits->dataBitPos + 7) >> 3;

    ptr = psBits->data + initial_byte_aligned_position;

    i = LocateFrameHeader(ptr, psBits->numBytes - initial_byte_aligned_position);
    if (psBits->numBytes <= initial_byte_aligned_position + i)
    {
        status = -1;
    }
    (void)movePointerTo(psBits, ((i + initial_byte_aligned_position) << 3)); /* ptr + i */
    return status;
}

int16 SearchVOLHeader(mp4StreamType *psBits)
{
    uint32 codeword = 0;
    int16 status = 0;
    do
    {
        status = SearchNextM4VFrame(psBits); /* search 0x00 0x00 0x01 */
        if (status != 0)
            return MP4_INVALID_VOL_PARAM;

        status = ReadBits(psBits, VOL_START_CODE_LENGTH, &codeword);
    }
    while ((codeword != VOL_START_CODE) && (status == 0));
    return status;
}

OSCL_EXPORT_REF int16 iGetM4VConfigInfo(uint8 *buffer, int32 length, int32 *width, int32 *height, int32 *display_width, int32 *display_height)
{
    int16 status;

    *width = *height = *display_height = *display_width = 0;

    if (length == 0)
    {
        return MP4_INVALID_VOL_PARAM;
    }
    int32 profilelevelvalue = 0; // dummy value discarded here
    status = iDecodeVOLHeader(buffer, length, width, height, display_width, display_height, &profilelevelvalue);
    return status;
}

// name: iDecodeVOLHeader
// Purpose: decode VOL header
// return:  error code
OSCL_EXPORT_REF int16 iDecodeVOLHeader(uint8 *buffer, int32 length, int32 *width, int32 *height, int32 *display_width, int32 *display_height, int32 *profilelevel)
{
    int16 status = 0;
    M4VConfigInfo iM4VConfigInfo;
    status = ParseM4VFSI(buffer, length, &iM4VConfigInfo);
    *display_width = iM4VConfigInfo.iFrameWidth[0];
    *display_height = iM4VConfigInfo.iFrameHeight[0];
    *width = (iM4VConfigInfo.iFrameWidth[0] + 15) & -16;
    *height = (iM4VConfigInfo.iFrameHeight[0] + 15) & -16;
    *profilelevel = iM4VConfigInfo.iProfilelevelValue;
    return status;
}

OSCL_EXPORT_REF int16 ParseM4VFSI(uint8* buffer, uint32 length,  M4VConfigInfo* iM4VConfigInfo)
{
    mp4StreamType* psBits, tempBits;

    int32 iWidth, iHeight;

    psBits = &tempBits;
    psBits->data = buffer;
    psBits->numBytes = length;
    psBits->bitBuf = 0;
    psBits->bitPos = 32;
    psBits->bytePos = 0;
    psBits->dataBitPos = 0;

    int16 iErrorStat;
    uint32 codeword;
    int32 time_increment_resolution, nbits_time_increment;
    uint32 video_object_layer_verid = 1; //Default
    uint32 estimation_method = 0; //Default
    int32 i, j;

    oscl_memset(iM4VConfigInfo, 0, sizeof(M4VConfigInfo));

    //Set Default values
    iM4VConfigInfo->iProfilelevelValue = 0x0000FFFF; // init to some invalid value. When this value is returned, then no profilelevel info is available
    iM4VConfigInfo->iFrameWidth[0] = H263_DEFAULT_WIDTH;
    iM4VConfigInfo->iFrameHeight[0] = H263_DEFAULT_HEIGHT;

    //visual_object_sequence_start_code
    ShowBits(psBits, 32, &codeword);

    if (codeword == VISUAL_OBJECT_SEQUENCE_START_CODE)
    {
        //DV: this is the wrong way to skip bits, use FLush or Read psBits->dataBitPos += 32;
        ReadBits(psBits, 32, &codeword); // skip 32 bits of the Start code

        ReadBits(psBits, 8, &codeword);

        /* profile_and_level_indication */
        iM4VConfigInfo->iProfilelevelValue = (int) codeword;

        switch (codeword)
        {
            case 0x08: /* SP_LVL0 */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_LEVEL0;
                break;
            }
            case 0x01: /* SP_LVL1 */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_LEVEL1;
                break;
            }
            case 0x02: /* SP_LVL2 */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_LEVEL2;
                break;
            }
            case 0x03: /* SP_LVL3 */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_LEVEL3;
                break;
            }
            case 0x04: /* SP_LVL4A */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_LEVEL4A;
                break;
            }
            case 0x05: /* SP_LVL5 */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_LEVEL5;
                break;
            }
            case 0x10: /* SIMPLE_SCALABLE_LEVEL0 */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_SCALABLE_LEVEL0;
                break;
            }
            case 0x11: /* SIMPLE_SCALABLE_LEVEL1 */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_SCALABLE_LEVEL1;
                break;
            }
            case 0x12: /* SIMPLE_SCALABLE_LEVEL2 */
            {
                iM4VConfigInfo->iProfileLevel =  SIMPLE_SCALABLE_LEVEL2;
                break;
            }
            case 0x21: /* CP_LVL1 */
            {
                iM4VConfigInfo->iProfileLevel =  CORE_LEVEL1;
                break;
            }
            case 0x22: /* CP_LVL2 */
            {
                iM4VConfigInfo->iProfileLevel =  CORE_LEVEL2;
                break;
            }
            case 0xA1: /* CORE_SCALABLE_LEVEL1 */
            {
                iM4VConfigInfo->iProfileLevel =  CORE_SCALABLE_LEVEL1;
                break;
            }
            case 0xA2: /* CORE_SCALABLE_LEVEL2 */
            {
                iM4VConfigInfo->iProfileLevel =  CORE_SCALABLE_LEVEL2;
                break;
            }
            case 0xA3: /* CORE_SCALABLE_LEVEL3 */
            {
                iM4VConfigInfo->iProfileLevel =  CORE_SCALABLE_LEVEL3;
                break;
            }
            case 0xF0: /* ADV_SIMPLE_LEVEL0 */
            {
                iM4VConfigInfo->iProfileLevel =  ADV_SIMPLE_LEVEL0;
                break;
            }
            case 0xF1: /* ADV_SIMPLE_LEVEL1 */
            {
                iM4VConfigInfo->iProfileLevel =  ADV_SIMPLE_LEVEL1;
                break;
            }
            case 0xF2: /* ADV_SIMPLE_LEVEL2 */
            {
                iM4VConfigInfo->iProfileLevel =  ADV_SIMPLE_LEVEL2;
                break;
            }
            case 0xF3: /* ADV_SIMPLE_LEVEL3 */
            {
                iM4VConfigInfo->iProfileLevel =  ADV_SIMPLE_LEVEL3;
                break;
            }
            case 0xF4: /* ADV_SIMPLE_LEVEL4 */
            {
                iM4VConfigInfo->iProfileLevel =  ADV_SIMPLE_LEVEL4;
                break;
            }
            case 0xF5: /* ADV_SIMPLE_LEVEL5 */
            {
                iM4VConfigInfo->iProfileLevel =  ADV_SIMPLE_LEVEL5;
                break;
            }
            default: // tolerate bad VOL, just set it to max one encode can encode.
            {
                iM4VConfigInfo->iProfileLevel = SIMPLE_LEVEL5;
                break;
            }
        }

        ShowBits(psBits, 32, &codeword);
        if (codeword == USER_DATA_START_CODE)
        {
            iErrorStat = DecodeUserData(psBits);
            if (iErrorStat)
            {
                return MP4_INVALID_VOL_PARAM;
            }
        }

        //visual_object_start_code
        ReadBits(psBits, 32, &codeword);
        if (codeword != VISUAL_OBJECT_START_CODE)
        {
            /* Search for VOL_HEADER */
            if (SearchVOLHeader(psBits) != 0)
                return MP4_INVALID_VOL_PARAM;
            goto decode_vol;
        }

        /*  is_visual_object_identifier            */
        ReadBits(psBits, 1, &codeword);

        if (codeword)
        {
            /* visual_object_verid                            */
            ReadBits(psBits, 4, &codeword);
            /* visual_object_priority                         */
            ReadBits(psBits, 3, &codeword);
        }
        /* visual_object_type                                 */
        ReadBits(psBits, 4, &codeword);

        if (codeword == 1)
        { /* video_signal_type */
            ReadBits(psBits, 1, &codeword);
            if (codeword == 1)
            {
                /* video_format */
                ReadBits(psBits, 3, &codeword);
                /* video_range  */
                ReadBits(psBits, 1, &codeword);
                /* color_description */
                ReadBits(psBits, 1, &codeword);
                if (codeword == 1)
                {
                    /* color_primaries */
                    ReadBits(psBits, 8, &codeword);;
                    /* transfer_characteristics */
                    ReadBits(psBits, 8, &codeword);
                    /* matrix_coefficients */
                    ReadBits(psBits, 8, &codeword);
                }
            }
        }
        else
        {
            /* Search for VOL_HEADER */
            if (SearchVOLHeader(psBits) != 0)
                return MP4_INVALID_VOL_PARAM;
            goto decode_vol;
        }
        /* next_start_code() */
        ByteAlign(psBits);

        ShowBits(psBits, 32, &codeword);
        if (codeword == USER_DATA_START_CODE)
        {
            iErrorStat = DecodeUserData(psBits);
            if (iErrorStat)
            {
                return MP4_INVALID_VOL_PARAM;
            }
        }
        ShowBits(psBits, 27, &codeword);
    }
    else
    {
        ShowBits(psBits, 27, &codeword);
    }

    if (codeword == VO_START_CODE)
    {

        ReadBits(psBits, 32, &codeword);

        /* video_object_layer_start_code                   */
        ReadBits(psBits, 28, &codeword);
        if (codeword != VOL_START_CODE)
        {
            if (psBits->dataBitPos >= (psBits->numBytes << 3))
            {
                iM4VConfigInfo->iShortHeader = 1;
                // END OF VOP
                return 0; //SUCCESS

            }
            else
            {
                /* Search for VOL_HEADER */
                if (SearchVOLHeader(psBits) != 0)
                    return MP4_INVALID_VOL_PARAM;
                goto decode_vol;
            }
        }
decode_vol:

        uint32 vol_id;

        /* vol_id (4 bits) */
        ReadBits(psBits, 4, & vol_id);

        // RandomAccessibleVOLFlag
        ReadBits(psBits, 1, &codeword);

        //Video Object Type Indication
        ReadBits(psBits, 8, &codeword);

        // is_object_layer_identifier
        ReadBits(psBits, 1, &codeword);
        if (codeword)
        {
            //video_object_layer_verid
            ReadBits(psBits, 4, &codeword);
            video_object_layer_verid = codeword;
            //video_object_layer_priority
            ReadBits(psBits, 3, &codeword);
        }

        // aspect ratio
        ReadBits(psBits, 4, &codeword);
        if (codeword == 0xF)
        {
            // Extended Parameter
            /* par_width */
            ReadBits(psBits, 8, &codeword);
            /* par_height */
            ReadBits(psBits, 8, &codeword);
        }

        //vol_control_parameters
        ReadBits(psBits, 1, &codeword);
        if (codeword)
        {
            //chroma_format
            ReadBits(psBits, 2, &codeword);
            if (codeword != 1)
            {
                return MP4_INVALID_VOL_PARAM;
            }

            //low_delay
            ReadBits(psBits, 1, &codeword);

            //vbv_parameters
            ReadBits(psBits, 1, &codeword);
            if (codeword)   /* if (vbv_parameters) {}, page 36 */
            {
                ReadBits(psBits, 15, &codeword);
                ReadBits(psBits, 1, &codeword);
                if (codeword != 1)
                {
                    return MP4_INVALID_VOL_PARAM;
                }

                ReadBits(psBits, 15, &codeword);
                ReadBits(psBits, 1, &codeword);
                if (codeword != 1)
                {
                    return MP4_INVALID_VOL_PARAM;
                }

                //15 + 1 + 3
                ReadBits(psBits, 19, &codeword);
                if (!(codeword & 0x8))
                {
                    return MP4_INVALID_VOL_PARAM;
                }

                ReadBits(psBits, 11, &codeword);
                ReadBits(psBits, 1, &codeword);
                if (codeword != 1)
                {
                    return MP4_INVALID_VOL_PARAM;
                }

                ReadBits(psBits, 15, &codeword);
                ReadBits(psBits, 1, &codeword);
                if (codeword != 1)
                {
                    return MP4_INVALID_VOL_PARAM;
                }
            }

        }

        // video_object_layer_shape
        ReadBits(psBits, 2, &codeword);
        /* video_object_layer_shape is RECTANGULAR */
        if (codeword != 0)
        {
            return MP4_INVALID_VOL_PARAM;
        }

        //Marker bit
        ReadBits(psBits, 1, &codeword);
        if (codeword != 1)
        {
            return MP4_INVALID_VOL_PARAM;
        }

        //  vop_time_increment_resolution
        ReadBits(psBits, 16, &codeword);
        time_increment_resolution = codeword;
        iM4VConfigInfo->iTimeIncRes = codeword;

        // Marker bit
        ReadBits(psBits, 1, &codeword);
        if (codeword != 1)
        {
            return MP4_INVALID_VOL_PARAM;
        }

        // fixed_vop_rate
        ReadBits(psBits, 1, &codeword);
        if (codeword && time_increment_resolution > 2)
        {
            i = time_increment_resolution - 1;
            j = 1;
            while (i >>= 1)
            {
                j++;
            }
            nbits_time_increment = j;

            ReadBits(psBits, nbits_time_increment, &codeword);
        }

        //Marker bit
        ReadBits(psBits, 1, &codeword);
        if (codeword != 1)
        {
            return MP4_INVALID_VOL_PARAM;
        }

        /* this should be 176 for QCIF */
        ReadBits(psBits, 13, &codeword);
        iM4VConfigInfo->iFrameWidth[0] = (int32)codeword;
        ReadBits(psBits, 1, &codeword);
        if (codeword != 1)
        {
            return MP4_INVALID_VOL_PARAM;
        }

        /* this should be 144 for QCIF */
        ReadBits(psBits, 13, &codeword);
        iM4VConfigInfo->iFrameHeight[0] = (int32)codeword;

        iWidth = (iM4VConfigInfo->iFrameWidth[0] + 15) & -16;
        iHeight = (iM4VConfigInfo->iFrameHeight[0] + 15) & -16;

        //Marker bit
        ReadBits(psBits, 1, &codeword);
        if (codeword != 1) return MP4_INVALID_VOL_PARAM;

        //Interlaced
        ReadBits(psBits, 1, &codeword);
        if (codeword != 0) return MP4_INVALID_VOL_PARAM;

        //obmc_disable
        ReadBits(psBits, 1, &codeword);
        if (codeword != 1) return MP4_INVALID_VOL_PARAM;

        if (video_object_layer_verid == 1)
        {
            //sprite_enable
            ReadBits(psBits, 1, &codeword);
            if (codeword != 0) return MP4_INVALID_VOL_PARAM;
        }
        else
        {
            //sprite_enable
            ReadBits(psBits, 2, &codeword);
            if (codeword != 0) return MP4_INVALID_VOL_PARAM;
        }

        //not_8_bit
        ReadBits(psBits, 1, &codeword);
        if (codeword)
        {
            //quant_precision
            ReadBits(psBits, 4, &codeword);
            //bits_per_pixel
            ReadBits(psBits, 4, &codeword);
        }

        /* video_object_layer_shape is not GRAY_SCALE  */

        //quant_type
        ReadBits(psBits, 1, &codeword);
        if (codeword != 0) //quant_type = 1
        {
            ReadBits(psBits, 1, &codeword); //load_intra_quant_mat
            if (codeword)
            {
                /* intra_quant_mat (8*64 bits) */
                i = 0;
                do
                {
                    ReadBits(psBits, 8, &codeword);
                }
                while ((codeword != 0) && (++i < 64));
            }

            ReadBits(psBits, 1, &codeword); //load_nonintra_quant_mat
            if (codeword)
            {
                /* nonintra_quant_mat (8*64 bits) */
                i = 0;
                do
                {
                    ReadBits(psBits, 8, &codeword);
                }
                while ((codeword != 0) && (++i < 64));
            }

        }

        if (video_object_layer_verid != 1)
        {
            //quarter_sample
            ReadBits(psBits, 1, &codeword);
            if (codeword) return MP4_INVALID_VOL_PARAM;
        }


        //complexity_estimation_disable
        ReadBits(psBits, 1, &codeword);
        if (!codeword)
        {
            //estimation_method
            ReadBits(psBits, 2, &codeword);
            estimation_method = codeword;

            if (estimation_method < 2)
            {
                // shape_complexity_estimation_disable
                ReadBits(psBits, 1, &codeword);
                if (codeword == 0)
                {
                    return MP4_INVALID_VOL_PARAM;
                }
                // texture_complexity_estimation_set_1_disable
                ReadBits(psBits, 1, &codeword);
                if (codeword == 0)
                {
                    ReadBits(psBits, 4, &codeword);
                }

                //Marker bit
                ReadBits(psBits, 1, &codeword);
                if (codeword != 1) return MP4_INVALID_VOL_PARAM;

                // texture_complexity_estimation_set_2_disable
                ReadBits(psBits, 1, &codeword);
                if (codeword == 0)
                {
                    ReadBits(psBits, 4, &codeword);
                }

                // motion_compensation_complexity_disable
                ReadBits(psBits, 1, &codeword);
                if (codeword == 0)
                {
                    ReadBits(psBits, 6, &codeword);
                }

                //Marker bit
                ReadBits(psBits, 1, &codeword);
                if (codeword != 1) return MP4_INVALID_VOL_PARAM;

                if (estimation_method == 1)
                {   // version2_complexity_estimation_disable
                    ReadBits(psBits, 1, &codeword);
                    if (codeword == 0)
                    {
                        return MP4_INVALID_VOL_PARAM;
                    }
                }
            }

        }

        //resync_marker_disable
        ReadBits(psBits, 1, &codeword);
        if (!codeword)
        {
            iM4VConfigInfo->iResyncMarker = 1;
        }

        //data_partitioned
        ReadBits(psBits, 1, &codeword);
        if (codeword)
        {
            iM4VConfigInfo->iDataPartitioning = true; //  DATA_PARTITIONING_MODE
            //reversible_vlc
            ReadBits(psBits, 1, &codeword);
            iM4VConfigInfo->iRVLCEnable = (bool)codeword;

        }
        else
        {
            // No data_partitioned
            iM4VConfigInfo->iDataPartitioning = false;
        }

        if (video_object_layer_verid != 1)
        {
            // newpred_enable
            ReadBits(psBits, 1, &codeword);
            if (codeword) return MP4_INVALID_VOL_PARAM;

            // reduced_resolution_vop
            ReadBits(psBits, 1, &codeword);
            if (codeword) return MP4_INVALID_VOL_PARAM;

        }

        //scalability
        ReadBits(psBits, 1, &codeword);
        if (codeword)
        {
            //hierarchy_type
            ReadBits(psBits, 1, &codeword);
            if (!codeword) return MP4_INVALID_VOL_PARAM;

            // ref_layer_id (4 bits)
            ReadBits(psBits, 4, &codeword);

            //ref_layer_sampling_direc
            ReadBits(psBits, 1, &codeword);
            if (codeword) return MP4_INVALID_VOL_PARAM;

            /* hor_sampling_factor_n (5 bits) */
            /* hor_sampling_factor_m (5 bits) */
            /* ver_sampling_factor_n (5 bits) */
            /* ver_sampling_factor_m (5 bits) */
            ReadBits(psBits, 20, &codeword);

            //enhancement_type
            ReadBits(psBits, 1, &codeword);
            if (codeword) return MP4_INVALID_VOL_PARAM;
        }

    }
    else
    {
        /* SHORT_HEADER */
        ShowBits(psBits, SHORT_VIDEO_START_MARKER_LENGTH, &codeword);
        if (codeword == SHORT_VIDEO_START_MARKER)
        {
            iM4VConfigInfo->iShortHeader = 1;
            return (iDecodeShortHeader(psBits, &iWidth, &iHeight, (int32*)&(iM4VConfigInfo->iFrameWidth[0]), (int32*)&(iM4VConfigInfo->iFrameHeight[0])));

        }
        else
        {
            {
                /* Search for VOL_HEADER */
                if (SearchVOLHeader(psBits) != 0)
                    return MP4_INVALID_VOL_PARAM;
                goto decode_vol;
            }
        }
    }

    return 0;
}

OSCL_EXPORT_REF
int16 iDecodeShortHeader(mp4StreamType *psBits,
                         int32 *width,
                         int32 *height,
                         int32 *display_width,
                         int32 *display_height)
{
    uint32 codeword;
    int32   extended_PTYPE = 0;
    int32 UFEP = 0;
    int32 custom_PFMT = 0;

    ShowBits(psBits, 22, &codeword);

    if (codeword !=  0x20)
    {
        return MP4_INVALID_VOL_PARAM;
    }
    FlushBits(psBits, 22);
    ReadBits(psBits, 8, &codeword);

    ReadBits(psBits, 1, &codeword);
    if (codeword == 0) return MP4_INVALID_VOL_PARAM;

    ReadBits(psBits, 1, &codeword);
    if (codeword == 1) return MP4_INVALID_VOL_PARAM;

    ReadBits(psBits, 1, &codeword);
    if (codeword == 1) return MP4_INVALID_VOL_PARAM;

    ReadBits(psBits, 1, &codeword);
    if (codeword == 1) return MP4_INVALID_VOL_PARAM;

    ReadBits(psBits, 1, &codeword);
    if (codeword == 1) return MP4_INVALID_VOL_PARAM;

    /* source format */
    ReadBits(psBits, 3, &codeword);
    switch (codeword)
    {
        case 1:
            *width = 128;
            *height = 96;
            break;

        case 2:
            *width = 176;
            *height = 144;
            break;

        case 3:
            *width = 352;
            *height = 288;
            break;

        case 4:
            *width = 704;
            *height = 576;
            break;

        case 5:
            *width = 1408;
            *height = 1152;
            break;

        case 7:
            extended_PTYPE = 1;
            break;
        default:
            /* Msg("H.263 source format not legal\n"); */
            return MP4_INVALID_VOL_PARAM;
    }

    if (extended_PTYPE == 0)
    {
        *display_width = *width;
        *display_height = *height;
        return 0;
    }
    /* source format */
    ReadBits(psBits, 3, &codeword);
    UFEP = codeword;
    if (UFEP == 1)
    {
        ReadBits(psBits, 3, &codeword);
        switch (codeword)
        {
            case 1:
                *width = 128;
                *height = 96;
                break;

            case 2:
                *width = 176;
                *height = 144;
                break;

            case 3:
                *width = 352;
                *height = 288;
                break;

            case 4:
                *width = 704;
                *height = 576;
                break;

            case 5:
                *width = 1408;
                *height = 1152;
                break;

            case 6:
                custom_PFMT = 1;
                break;
            default:
                /* Msg("H.263 source format not legal\n"); */
                return MP4_INVALID_VOL_PARAM;
        }
        if (custom_PFMT == 0)
        {
            *display_width = *width;
            *display_height = *height;
            return 0;
        }
        ReadBits(psBits, 1, &codeword);
        ReadBits(psBits, 1, &codeword);
        if (codeword) return MP4_INVALID_VOL_PARAM;
        ReadBits(psBits, 1, &codeword);
        if (codeword) return MP4_INVALID_VOL_PARAM;
        ReadBits(psBits, 1, &codeword);
        if (codeword) return MP4_INVALID_VOL_PARAM;
        ReadBits(psBits, 3, &codeword);
        ReadBits(psBits, 3, &codeword);
        if (codeword) return MP4_INVALID_VOL_PARAM;             /* RPS, ISD, AIV */
        ReadBits(psBits, 1, &codeword);
        ReadBits(psBits, 4, &codeword);
        if (codeword != 8) return MP4_INVALID_VOL_PARAM;
    }
    if (UFEP == 0 || UFEP == 1)
    {
        ReadBits(psBits, 3, &codeword);
        if (codeword > 1) return MP4_INVALID_VOL_PARAM;
        ReadBits(psBits, 1, &codeword);
        if (codeword) return MP4_INVALID_VOL_PARAM;
        ReadBits(psBits, 1, &codeword);
        if (codeword) return MP4_INVALID_VOL_PARAM;
        ReadBits(psBits, 1, &codeword);
        ReadBits(psBits, 3, &codeword);
        if (codeword != 1) return MP4_INVALID_VOL_PARAM;
    }
    else
    {
        return MP4_INVALID_VOL_PARAM;
    }
    ReadBits(psBits, 1, &codeword);
    if (codeword) return MP4_INVALID_VOL_PARAM; /* CPM */
    if (custom_PFMT == 1 && UFEP == 1)
    {
        ReadBits(psBits, 4, &codeword);
        if (codeword == 0) return MP4_INVALID_VOL_PARAM;
        if (codeword == 0xf)
        {
            ReadBits(psBits, 8, &codeword);
            ReadBits(psBits, 8, &codeword);
        }
        ReadBits(psBits, 9, &codeword);
        *display_width = (codeword + 1) << 2;
        *width = (*display_width + 15) & -16;
        ReadBits(psBits, 1, &codeword);
        if (codeword != 1) return MP4_INVALID_VOL_PARAM;
        ReadBits(psBits, 9, &codeword);
        if (codeword == 0) return MP4_INVALID_VOL_PARAM;
        *display_height = codeword << 2;
        *height = (*display_height + 15) & -16;
    }

    return 0;
}



int16 ShowBits(
    mp4StreamType *pStream,           /* Input Stream */
    uint8 ucNBits,          /* nr of bits to read */
    uint32 *pulOutData      /* output target */
)
{
    uint8 *bits;
    uint32 dataBitPos = pStream->dataBitPos;
    uint32 bitPos = pStream->bitPos;
    uint32 dataBytePos;

    uint i;

    if (ucNBits > (32 - bitPos))    /* not enough bits */
    {
        dataBytePos = dataBitPos >> 3; /* Byte Aligned Position */
        bitPos = dataBitPos & 7; /* update bit position */
        if (dataBytePos > pStream->numBytes - 4)
        {
            pStream->bitBuf = 0;
            for (i = 0; i < pStream->numBytes - dataBytePos; i++)
            {
                pStream->bitBuf |= pStream->data[dataBytePos+i];
                pStream->bitBuf <<= 8;
            }
            pStream->bitBuf <<= 8 * (3 - i);
        }
        else
        {
            bits = &pStream->data[dataBytePos];
            pStream->bitBuf = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
        }
        pStream->bitPos = bitPos;
    }

    bitPos += ucNBits;

    *pulOutData = (pStream->bitBuf >> (32 - bitPos)) & mask[(uint16)ucNBits];


    return 0;
}

int16 FlushBits(
    mp4StreamType *pStream,           /* Input Stream */
    uint8 ucNBits                      /* number of bits to flush */
)
{
    uint8 *bits;
    uint32 dataBitPos = pStream->dataBitPos;
    uint32 bitPos = pStream->bitPos;
    uint32 dataBytePos, byteLeft;


    if ((dataBitPos + ucNBits) > (uint32)(pStream->numBytes << 3))
        return (-2); // Buffer over run

    dataBitPos += ucNBits;
    bitPos     += ucNBits;

    if (bitPos > 32)
    {
        dataBytePos = dataBitPos >> 3;    /* Byte Aligned Position */
        byteLeft = pStream->numBytes - dataBytePos; // Byte Lefts
        bitPos = dataBitPos & 7; /* update bit position */
        bits = &pStream->data[dataBytePos];
        if (byteLeft > 3)
        {
            pStream->bitBuf = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
        }
        else
        {
            uint32 lShift = 24;
            uint32 tmpBuff = 0;
            while (byteLeft)
            {
                tmpBuff |= (*bits << lShift);
                bits++;
                byteLeft--;
                lShift -= 8;
            }
            pStream->bitBuf = tmpBuff;
        }
    }

    pStream->dataBitPos = dataBitPos;
    pStream->bitPos     = bitPos;

    return 0;
}

int16 ReadBits(
    mp4StreamType *pStream,           /* Input Stream */
    uint8 ucNBits,                     /* nr of bits to read */
    uint32 *pulOutData                 /* output target */
)
{
    uint8 *bits;
    uint32 dataBitPos = pStream->dataBitPos;
    uint32 bitPos = pStream->bitPos;
    uint32 dataBytePos, byteLeft;

    if ((dataBitPos + ucNBits) > (pStream->numBytes << 3))
    {
        *pulOutData = 0;
        return (-2); // Buffer over run
    }

    //  dataBitPos += ucNBits;

    if (ucNBits > (32 - bitPos))    /* not enough bits */
    {
        dataBytePos = dataBitPos >> 3;    /* Byte Aligned Position */
        byteLeft = pStream->numBytes - dataBytePos; // Byte Lefts
        bitPos = dataBitPos & 7; /* update bit position */
        bits = &pStream->data[dataBytePos];
        if (byteLeft > 3)
        {
            pStream->bitBuf = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
        }
        else
        {
            uint32 lShift = 24;
            uint32 tmpBuff = 0;
            while (byteLeft)
            {
                tmpBuff |= (*bits << lShift);
                bits++;
                byteLeft--;
                lShift -= 8;
            }
            pStream->bitBuf = tmpBuff;
        }
    }

    pStream->dataBitPos += ucNBits;
    pStream->bitPos      = (unsigned char)(bitPos + ucNBits);

    *pulOutData = (pStream->bitBuf >> (32 - pStream->bitPos)) & mask[(uint16)ucNBits];

    return 0;
}



int16 ByteAlign(
    mp4StreamType *pStream           /* Input Stream */
)
{
    uint8 *bits;
    uint32 dataBitPos = pStream->dataBitPos;
    uint32 bitPos = pStream->bitPos;
    uint32 dataBytePos;
    uint32 leftBits, byteLeft;

    leftBits =  8 - (dataBitPos & 0x7);
    if (leftBits == 8)
    {
        if ((dataBitPos + 8) > (uint32)(pStream->numBytes << 3))
            return (-2); // Buffer over run
        dataBitPos += 8;
        bitPos += 8;
    }
    else
    {
        dataBytePos = dataBitPos >> 3;
        dataBitPos += leftBits;
        bitPos += leftBits;
    }


    if (bitPos > 32)
    {
        dataBytePos = dataBitPos >> 3;    /* Byte Aligned Position */
        bits = &pStream->data[dataBytePos];

        byteLeft = pStream->numBytes - dataBytePos; // Byte Lefts
        if (byteLeft > 3)
        {
            pStream->bitBuf = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
        }
        else
        {
            uint32 lShift = 24;
            uint32 tmpBuff = 0;
            while (byteLeft)
            {
                tmpBuff |= (*bits << lShift);
                bits++;
                byteLeft--;
                lShift -= 8;
            }
            pStream->bitBuf = tmpBuff;
        }
    }

    pStream->dataBitPos = dataBitPos;
    pStream->bitPos     = bitPos;

    return 0;
}

int16 DecodeUserData(mp4StreamType *pStream)
{

    uint32 codeword;
    int16 iErrorStat;

    iErrorStat = ReadBits(pStream, 32, &codeword);
    if (iErrorStat) return iErrorStat;
    iErrorStat = ShowBits(pStream, 24, &codeword);
    if (iErrorStat) return iErrorStat;

    while (codeword != 1)
    {
        /* Discard user data for now. */
        iErrorStat = ReadBits(pStream, 8, &codeword);
        if (iErrorStat) return iErrorStat;
        iErrorStat = ShowBits(pStream, 24, &codeword);
        if (iErrorStat) return iErrorStat;
    }
    return 0;
}


OSCL_EXPORT_REF int16 iGetAVCConfigInfo(uint8 *buffer, int32 length, int32 *width, int32 *height, int32 *display_width, int32 *display_height, int32 *profile_idc, int32 *level_idc)
{
    int16 status;
    AVCConfigInfo iAVCConfigInfo;

    status = ParseAVCFSI(buffer, length, &iAVCConfigInfo);
    *display_width = iAVCConfigInfo.nDisplayWidth;
    *display_height = iAVCConfigInfo.nDisplayHeight;
    *width = iAVCConfigInfo.nWidth;
    *height = iAVCConfigInfo.nHeight;
    *profile_idc = iAVCConfigInfo.nProfile;
    *level_idc = iAVCConfigInfo.nLevel;

    return status;
}

OSCL_EXPORT_REF int16 ParseAVCFSI(uint8* buffer, uint32 length, AVCConfigInfo* iAVCConfigInfo)
{
    mp4StreamType psBits;
    int32 nal_length = 0, bytesLeft = 0, tmp_nal_length = 0;
    int32 i;
    uint8* temp = NULL;
    uint8* nal = NULL;
    int32 nal_type = 0;
    int32 count = 0;

    temp = (uint8 *)OSCL_MALLOC(sizeof(uint8) * length);

    // Zero initialization
    oscl_memset(iAVCConfigInfo, 0, sizeof(AVCConfigInfo));

    if (temp)
    {
        nal = temp; // Make a copy of the original pointer to be freed later
        // Successfull allocation... copy input buffer
        oscl_memcpy(nal, buffer, length);
    }
    else
    {
        // Allocation failed
        return MP4_INVALID_VOL_PARAM;
    }

    if (length < 3)
    {
        OSCL_FREE(temp);
        return MP4_INVALID_VOL_PARAM;
    }

    bytesLeft = length;

    do
    {
        if (nal[0] == 0 && nal[1] == 0)
        {
            i = 0;
            /* find SC at the beginning of the NAL */
            while ((nal[i++] == 0) && (i < bytesLeft))
            {
            }

            if (nal[i-1] == 1)
            {
                nal += i;

                nal_length = 0;
                count = 0;
                bytesLeft -= i;
                // search for the next start code
                while (nal_length < (int32)bytesLeft)
                {
                    if (count == 2 && nal[nal_length] == 0x01)
                    {
                        //000001
                        nal_length -= 2;
                        break;
                    }
                    else if (count == 3 && nal[nal_length] == 0x01)
                    {
                        //00000001
                        nal_length -= 3;
                        break;
                    }

                    if (nal[nal_length])
                        count = 0;
                    else
                        count++;
                    nal_length++;
                }

                //here we have all nals together as next nal
                bytesLeft -= nal_length;
            }
            else
            {
                OSCL_FREE(temp);
                return MP4_INVALID_VOL_PARAM;
            }
        }
        else
        {
            nal_length = (uint16)(nal[1] << 8) | nal[0];
            nal += 2;
            // robustness check
            if (nal_length > (bytesLeft - 2)) //-2 for the length bytes
            {
                OSCL_FREE(temp);
                return MP4_INVALID_VOL_PARAM;
            }
            bytesLeft -= (nal_length + 2);
        }

        tmp_nal_length = nal_length;
        Parser_EBSPtoRBSP(nal, (int32*) &tmp_nal_length);

        psBits.data = nal;
        psBits.numBytes = tmp_nal_length;
        psBits.bitBuf = 0;
        psBits.bitPos = 32;
        psBits.bytePos = 0;
        psBits.dataBitPos = 0;

        if (!getNALType(nal, tmp_nal_length, &nal_type))
        {
            OSCL_FREE(temp);
            return MP4_INVALID_VOL_PARAM;
        }

        /*
        ** Since we re-use iAVCConfigInfo, it will contain the values from the last SPS/PPS in the config info.
        ** Typically width and height of the same sequence should be the same even if they come from different SPSs.
        ** However, it is also possible for MP4 file format to have multiple track config with different width/height.
        ** In that case, we actually expect the parser node to provide all track configs at once. So, in a single mediamsg,
        ** there will be several media fragments one for each config. Each config can contain several SPSs/PPSs.
        ** So, if we assume that width/height do not change per config info, using one AVCConfigInfo memory space should be sufficient.
        */

        switch (nal_type)
        {
                /*
                ** Parsing fails if either SPS or PPS decoding fails.
                */
            case 7 : //SPS
                if (DecodeSPS(&psBits, iAVCConfigInfo))
                {
                    OSCL_FREE(temp);
                    return MP4_INVALID_VOL_PARAM;
                }
                break;

            case 8: //PPS
                if (DecodePPS(&psBits, iAVCConfigInfo))
                {
                    // SPS is missing or error in SPS decoding.
                    OSCL_FREE(temp);
                    return MP4_INVALID_VOL_PARAM;
                }
                break;
            default: /* Skip AUD/SEI or others */
                break;
        }

        nal += nal_length;

    }
    while (bytesLeft > 1);


    OSCL_FREE(temp);

    return 0; // return 0 for success
}


int16 DecodeSPS(mp4StreamType *psBits,  AVCConfigInfo* iAVCConfigInfo)
{
    uint32 temp;
    int32 temp0;
    uint left_offset, right_offset, top_offset, bottom_offset;
    uint i;

    ReadBits(psBits, 8, &temp);

    // nal_unit_type == 7 for SPS
    if ((temp & 0x1F) != 7) return MP4_INVALID_VOL_PARAM;

    ReadBits(psBits, 8, &temp);

    //profile_idc
    iAVCConfigInfo->nProfile = temp;

    //constrained_set0_flag
    ReadBits(psBits, 1, &temp);
    //constrained_set1_flag
    ReadBits(psBits, 1, &temp);
    //constrained_set2_flag
    ReadBits(psBits, 1, &temp);
    //constrained_set3_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bConstrained_set3_flag = (bool)temp;

    //reserved_zero_4bits
    ReadBits(psBits, 4, &temp);

    //level_idc
    ReadBits(psBits, 8, &temp);
    iAVCConfigInfo->nLevel = temp;
    if (temp > 51)
        return MP4_INVALID_VOL_PARAM;

    //seq_parameter_set_id
    ue_v(psBits, &temp);
    //log2_max_frame_num_minus4
    ue_v(psBits, &temp);
    iAVCConfigInfo->nMaxFrameNum = (1 << (temp + 4)) - 1;

    //pic_order_cnt_type
    ue_v(psBits, &temp);

    if (temp == 0)
    {
        //log2_max_pic_order_cnt_lsb_minus4
        ue_v(psBits, &temp);
    }
    else if (temp == 1)
    {
        //delta_pic_order_always_zero_flag
        ReadBits(psBits, 1, &temp);
        //offset_for_non_ref_pic
        se_v(psBits, &temp0);
        //offset_for_top_to_bottom_field
        se_v(psBits, &temp0);
        //num_ref_frames_in_pic_order_cnt_cycle
        ue_v(psBits, &temp);

        for (i = 0; i < temp; i++)
        {
            //offset_for_ref_frame
            se_v(psBits, &temp0);
        }
    }

    //num_ref_frames
    ue_v(psBits, &temp);
    iAVCConfigInfo->nRefFrames = temp;

    //gaps_in_frame_num_value_allowed_flag
    ReadBits(psBits, 1, &temp);

    //pic_width_in_mbs_minus1
    ue_v(psBits, &temp);
    iAVCConfigInfo->nDisplayWidth = iAVCConfigInfo->nWidth = (temp + 1) << 4;

    //pic_height_in_map_units_minus1
    ue_v(psBits, &temp);
    iAVCConfigInfo->nDisplayHeight = iAVCConfigInfo->nHeight = (temp + 1) << 4;

    //frame_mbs_only_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bFrameMBsOnly = (bool)temp;
    if (!temp)
    {
        // we do not support if frame_mb_only_flag is off
        return MP4_INVALID_VOL_PARAM;
        //ReadBits(psBits,1, &temp);
    }

    //direct_8x8_inference_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bDirect8x8Inference = (bool)temp;

    //frame_cropping_flag
    ReadBits(psBits, 1, &temp);

    if (temp)
    {
        ue_v(psBits, (uint32*)&left_offset);  //frame_crop_left_offset
        ue_v(psBits, (uint32*)&right_offset); //frame_crop_right_offset
        ue_v(psBits, (uint32*)&top_offset); //frame_crop_top_offset
        ue_v(psBits, (uint32*)&bottom_offset); //frame_crop_bottom_offset

        iAVCConfigInfo->nDisplayWidth = iAVCConfigInfo->nWidth - 2 * (right_offset + left_offset);
        iAVCConfigInfo->nDisplayHeight = iAVCConfigInfo->nHeight - 2 * (top_offset + bottom_offset);
    }

    /*  no need to check further */
#if USE_LATER
    //vui_parameters_present_flag
    ReadBits(psBits, 1, &temp);
    if (temp)
    {
        if (DecodeVUI(psBits))
        {
            return MP4_INVALID_VOL_PARAM;
        }
    }
#endif
    return 0; // return 0 for success
}

#if USE_LATER
/* unused for now */
int32 DecodeVUI(mp4StreamType *psBits)
{
    uint32 temp;
    uint32 temp32;
    uint32 aspect_ratio_idc, overscan_appopriate_flag, video_format, video_full_range_flag;

    ReadBits(psBits, 1, &temp); /* aspect_ratio_info_present_flag */
    if (temp)
    {
        ReadBits(psBits, 8, &aspect_ratio_idc);
        if (aspect_ratio_idc == 255)
        {
            ReadBits(psBits, 16, &temp); /* sar_width */
            ReadBits(psBits, 16, &temp); /* sar_height */
        }
    }
    ReadBits(psBits, 1, &temp); /* overscan_info_present */
    if (temp)
    {
        ReadBits(psBits, 1, &overscan_appopriate_flag);
    }
    ReadBits(psBits, 1, &temp); /* video_signal_type_present_flag */
    if (temp)
    {
        ReadBits(psBits, 3, &video_format);
        ReadBits(psBits, 1, &video_full_range_flag);
        ReadBits(psBits, 1, &temp); /* colour_description_present_flag */
        if (temp)
        {
            ReadBits(psBits, 8, &temp); /* colour_primaries */
            ReadBits(psBits, 8, &temp); /* transfer_characteristics */
            ReadBits(psBits, 8, &temp); /* matrix coefficients */
        }
    }
    ReadBits(psBits, 1, &temp);/*   chroma_loc_info_present_flag */
    if (temp)
    {
        ue_v(psBits, &temp); /*  chroma_sample_loc_type_top_field */
        ue_v(psBits, &temp); /*  chroma_sample_loc_type_bottom_field */
    }

    ReadBits(psBits, 1, &temp); /*  timing_info_present_flag*/
    if (temp)
    {
        ReadBits(psBits, 32, &temp32); /*  num_unit_in_tick*/
        ReadBits(psBits, 32, &temp32); /*   time_scale */
        ReadBits(psBits, 1, &temp); /*  fixed_frame_rate_flag */
    }

    ReadBits(psBits, 1, &temp); /*  nal_hrd_parameters_present_flag */
    if (temp)
    {
        if (DecodeHRD(psBits))
        {
            return 1;
        }
    }
    ReadBits(psBits, 1, &temp32); /*    vcl_hrd_parameters_present_flag*/
    if (temp32)
    {
        if (DecodeHRD(psBits))
        {
            return 1;
        }
    }
    if (temp || temp32)
    {
        ReadBits(psBits, 1, &temp);     /*  low_delay_hrd_flag */
    }
    ReadBits(psBits, 1, &temp); /*  pic_struct_present_flag */
    ReadBits(psBits, 1, &temp); /* bitstream_restriction_flag */

    if (temp)
    {
        ReadBits(psBits, 1, &temp); /*  motion_vectors_over_pic_boundaries_flag */
        ue_v(psBits, &temp); /* max_bytes_per_pic_denom */
        ue_v(psBits, &temp); /* max_bits_per_mb_denom */
        ue_v(psBits, &temp); /* log2_max_mv_length_horizontal */
        ue_v(psBits, &temp); /* log2_max_mv_length_vertical */
        ue_v(psBits, &temp); /* num_reorder_frames */
        ue_v(psBits, &temp); /* max_dec_frame_buffering */
    }

    return 0; // 0 for success
}

/* unused for now */
int32 DecodeHRD(mp4StreamType *psBits)
{
    uint32 temp;
    uint32 cpb_cnt_minus1;
    uint i;
    int32 status;

    ue_v(psBits, &cpb_cnt_minus1);
    ReadBits(psBits, 4, &temp); /*  bit_rate_scale */
    ReadBits(psBits, 4, &temp); /*  cpb_size_scale */
    for (i = 0; i <= cpb_cnt_minus1; i++)
    {
        ue_v(psBits, &temp); /* bit_rate_value_minus1[i] */
        ue_v(psBits, &temp); /* cpb_size_value_minus1[i] */
        ue_v(psBits, &temp); /* cbr_flag[i] */
    }
    ReadBits(psBits, 5, &temp); /*  initial_cpb_removal_delay_length_minus1 */
    ReadBits(psBits, 5, &temp); /*  cpb_removal_delay_length_minus1 */
    ReadBits(psBits, 5, &temp); /*  dpb_output_delay_length_minus1 */
    status = ReadBits(psBits, 5, &temp); /* time_offset_length  */

    if (status != 0) // buffer overrun
    {
        return 1;
    }

    return 0; // 0 for success
}
#endif

// only check for entropy coding mode
int32 DecodePPS(mp4StreamType *psBits, AVCConfigInfo* iAVCConfigInfo)
{
    uint32 iCnt, temp, total_bit_left;
    int32 ntemp;
    uint32 num_slice_groups_minus1, slice_group_map_type;
    uint32 pic_size_in_map_units_minus1, numBits;

    //pic_parameter_set == 8
    ReadBits(psBits, 8, &temp);
    if ((temp & 0x1F) != 8) return MP4_INVALID_VOL_PARAM;

    //pic_parameter_set_id
    ue_v(psBits, &temp);

    //seq_parameter_set_id
    ue_v(psBits, &temp);

    //entropy_coding_mode_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bEntropyCodingCABAC = (bool)temp;

    //pic_order_present_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bMBAFF = (bool)temp;

    //num_slice_groups_minus1
    ue_v(psBits, &num_slice_groups_minus1);
    iAVCConfigInfo->nNumSliceGroups = num_slice_groups_minus1 + 1;
    if (num_slice_groups_minus1 > 7) //Max number of slice group
    {
        return 1;
    }

    iAVCConfigInfo->nSliceGroupMapType = 1; //Default to 1
    if (num_slice_groups_minus1 > 0)
    {
        iAVCConfigInfo->bEnableFMO = 1;
        //slice_group_map_type
        ue_v(psBits, &(slice_group_map_type));
        iAVCConfigInfo->nSliceGroupMapType = slice_group_map_type;

        if (slice_group_map_type > 6)
        {
            return 1;
        }

        if (slice_group_map_type == 0)
        {
            for (iCnt = 0; iCnt <= num_slice_groups_minus1; iCnt++)
            {
                ue_v(psBits, &temp);
            }
        }
        else if (slice_group_map_type == 2)
        {
            for (iCnt = 0; iCnt < num_slice_groups_minus1; iCnt++)
            {
                ue_v(psBits, &temp);
                ue_v(psBits, &temp);
            }
        }
        else if (slice_group_map_type == 3 ||
                 slice_group_map_type == 4 ||
                 slice_group_map_type == 5)
        {
            //slice_group_change_direction_flag
            ReadBits(psBits, 1, &temp);
            //slice_group_change_rate_minus1
            ue_v(psBits, &temp);
        }
        else if (slice_group_map_type == 6)
        {
            //pic_size_in_map_units_minus1
            ue_v(psBits, &pic_size_in_map_units_minus1);

            numBits = 0;/* ceil(log2(num_slice_groups_minus1+1)) bits */
            iCnt = num_slice_groups_minus1;
            while (iCnt > 0)
            {
                numBits++;
                iCnt >>= 1;
            }

            for (iCnt = 0; iCnt < (pic_size_in_map_units_minus1 + 1); iCnt++)
            {
                // slice_group_id[]
                ReadBits(psBits, numBits, &temp);
            }
        }
    }

    //num_ref_idx_l0_active_minus1
    ue_v(psBits, &temp);
    iAVCConfigInfo->nRefIdxl0ActiveMinus1 = temp;
    if (temp > 31)
    {
        return 1;
    }

    //num_ref_idx_l1_active_minus1
    ue_v(psBits, &temp);
    iAVCConfigInfo->nRefIdxl1ActiveMinus1 = temp;
    if (temp > 31)
    {
        return 1;
    }

    //weighted_pred_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bWeightedPPrediction = (bool)temp;

    //weighted_bipred_idc
    ReadBits(psBits, 2, &temp);
    iAVCConfigInfo->nWeightedBipredictionMode = temp;
    if (temp > 2)
    {
        return 1;
    }

    //pic_init_qp_minus26
    se_v(psBits, &ntemp);
    if (ntemp < -26 || ntemp > 25)
    {
        return 1;
    }
    iAVCConfigInfo->pic_init_qp = ntemp + 26;

    //pic_init_qs_minus26
    se_v(psBits, &ntemp);
    if (ntemp < -26 || ntemp > 25)
    {
        return 1;
    }

    //chroma_qp_index_offset
    se_v(psBits, &ntemp);
    if (ntemp < -12 || ntemp > 12)
    {
        return 1;
    }

    // deblocking_filter_control_present_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bDblkFilterFlag = (bool)temp;

    // constrained_intra_pred_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bconstIpred = (bool)temp;

    // redundant_pic_cnt_present_flag
    ReadBits(psBits, 1, &temp);
    iAVCConfigInfo->bEnableRS = (bool)temp;

    //if( more_rbsp_data( ) )
    if ((psBits->numBytes << 3) > psBits->dataBitPos)
    {
        total_bit_left = (psBits->numBytes << 3) - psBits->dataBitPos;
        if (total_bit_left <= 8)
        {
            ShowBits(psBits,  total_bit_left , &temp);
            if (temp == (uint32)(1 << (total_bit_left - 1)))
            {
                // end of NAL;
                return 0;
            }
        }
        // transform_8x8_mode_flag
        ReadBits(psBits, 1, &temp);
    }

    return 0;
}

void ue_v(mp4StreamType *psBits, uint32 *codeNum)
{
    uint32 temp;
    uint tmp_cnt;
    int32 leading_zeros = 0;
    ShowBits(psBits, 16, &temp);
    tmp_cnt = temp  | 0x1;

    PV_CLZ(leading_zeros, tmp_cnt)

    if (leading_zeros < 8)
    {
        *codeNum = (temp >> (15 - (leading_zeros << 1))) - 1;
        FlushBits(psBits, (leading_zeros << 1) + 1);
    }
    else
    {
        ReadBits(psBits, (leading_zeros << 1) + 1, &temp);
        *codeNum = temp - 1;
    }

}

void  se_v(mp4StreamType *psBits, int32 *value)
{
    uint32 temp, tmp_cnt;
    int leading_zeros = 0;
    ShowBits(psBits, 16, &temp);
    tmp_cnt = temp | 0x1;

    PV_CLZ(leading_zeros, tmp_cnt)

    if (leading_zeros < 8)
    {
        temp >>= (15 - (leading_zeros << 1));
        FlushBits(psBits, (leading_zeros << 1) + 1);
    }
    else
    {
        ReadBits(psBits, (leading_zeros << 1) + 1, &temp);
    }

    *value = temp >> 1;

    if (temp & 0x01)                          // lsb is signed bit
        *value = -(*value);
}

void Parser_EBSPtoRBSP(uint8 *nal_unit, int32 *size)
{
    int32 i, j;
    int32 count = 0;


    for (i = 0; i < *size; i++)
    {
        if (count == 2 && nal_unit[i] == 0x03)
        {
            break;
        }

        if (nal_unit[i])
            count = 0;
        else
            count++;
    }

    count = 0;
    j = i++;
    for (; i < *size; i++)
    {
        if (count == 2 && nal_unit[i] == 0x03)
        {
            i++;
            count = 0;
        }
        nal_unit[j] = nal_unit[i];
        if (nal_unit[i])
            count = 0;
        else
            count++;
        j++;
    }

    *size = j;
}


bool getNALType(uint8 *bitstream, int size, int32 *nal_type)
{
    int forbidden_zero_bit;
    if (size > 0)
    {
        forbidden_zero_bit = bitstream[0] >> 7;
        if (forbidden_zero_bit != 0)
            return false;

        *nal_type = (int32)(bitstream[0] & 0x1F);
        return true;
    }

    return false;
}

