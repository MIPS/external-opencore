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
/*******************************************************************************************
 Class :: CCYUV420toYUV422.cpp
------------------------------------
The input is YUV420 planar and output format is YUV422 fully interleaved

********************************************************************************************/
#include "colorconv_config.h"
#include "ccyuv420toyuv422.h"


OSCL_EXPORT_REF ColorConvertBase* CCYUV420toYUV422 :: New()
{
    CCYUV420toYUV422* self = OSCL_NEW(CCYUV420toYUV422, ());
    return OSCL_STATIC_CAST(ColorConvertBase*, self);
}


CCYUV420toYUV422 :: CCYUV420toYUV422()
{
}


OSCL_EXPORT_REF CCYUV420toYUV422 :: ~CCYUV420toYUV422()
{
}


int32 CCYUV420toYUV422:: Init(int32 SrcWidth, int32 SrcHeight, int32 SrcPitch, int32 DstWidth, int32 DstHeight, int32 DstPitch, int32 nRotation)
{
    /* no scaled outputs */
    _mInitialized = false;
    if (!(nRotation&0x1))
    {
        if ((SrcWidth != DstWidth) || (SrcHeight != DstHeight))
        {
            return 0;
        }
    }
    else // with rotation
    {
        if ((SrcWidth != DstHeight) || (SrcHeight != DstWidth))
        {
            return 0;
        }
    }

    if (SrcPitch != SrcWidth) // not support source cropping
    {
        return 0;
    }
    else
    {
        _mSrc_width = SrcWidth;
        _mSrc_height = SrcHeight;
        _mSrc_pitch = SrcPitch;
        _mDst_width = DstWidth;
        _mDst_height = DstHeight;
        _mDst_pitch = DstPitch;
        _mDst_mheight = DstHeight;
        _mRotation = nRotation;

        _mInitialized = true;

        SetMode(0); // called after init

        return 1;
    }

}


int32 CCYUV420toYUV422:: GetOutputBufferSize(void)
{
    OSCL_ASSERT(_mInitialized == true);

    return(_mDst_pitch * _mDst_mheight * 2);
}


int32 CCYUV420toYUV422::SetMode(int32 nMode)
{
    OSCL_UNUSED_ARG(nMode);
    OSCL_ASSERT(_mInitialized == true);

    return 1;
}

int32 CCYUV420toYUV422::SetYuvFullRange(bool range)
{
    OSCL_UNUSED_ARG(range);
    OSCL_ASSERT(_mInitialized == true);

    return 1;  // has no meaning in this class. Always return 1
}


int32 CCYUV420toYUV422::Convert(uint8 *inyuv, uint8 *outyuv)
{

    int32 i, j, lStart;
    uint8 *incb, *incr;
    uint32 *outyuv_4, *outyuv_4_nextLine;
    uint16 *inyuv_2, *inyuv_2_nextLine;
    uint32 temp, tempY, tempU, tempV, tempUV, tempY1, tempY2, tempY1Y2;

    int inYsize, offset, outYsize;

    int lpitch = _mDst_pitch;
    int lheight = _mSrc_height;

    OSCL_ASSERT(inyuv);
    OSCL_ASSERT(outyuv);
    OSCL_ASSERT(_mInitialized == true);

    inYsize = (_mSrc_width * _mSrc_height);
    outYsize = lpitch * _mDst_height;

    outyuv_4 = (uint32*) outyuv;
    inyuv_2 = (uint16*) inyuv;

    switch (_mRotation)
    {
        case 0://Rotation0

            incb = inyuv + inYsize;
            incr = incb + (inYsize >> 2);

            offset = lpitch - _mSrc_width;

            for (i = lheight >> 1; i > 0; i--)
            {
                inyuv_2_nextLine = inyuv_2 + (_mSrc_width >> 1);
                outyuv_4_nextLine = outyuv_4 + (lpitch >> 1);

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    tempY = *inyuv_2++;  //0000y1y0
                    tempU = *incb++;  //u0
                    tempV = *incr++;  //v0
#if UY0VY1 //u0 y0 v0 y1
                    temp = ((tempY & 0xFF) << 8) | ((tempY & 0xFF00) << 16); //y100y000
                    tempUV = (tempU | (tempV << 16)); //00v000u0
                    *outyuv_4++ = (temp | tempUV);  //y1v0y0u0

                    tempY = *inyuv_2_nextLine++;  //0000y1y0
                    temp = ((tempY & 0xFF) << 8) | ((tempY & 0xFF00) << 16); //y100y000
                    *outyuv_4_nextLine++ = (temp | tempUV);  //y1v0y0u0
#endif

#if Y1VY0U //y1 v0 y0 u0
                    temp = ((tempY & 0xFF) << 16) | (tempY >> 8); //00y000y1
                    tempUV = (tempU << 24) | (tempV << 8); //u000v000
                    *outyuv_4++ = (temp | tempUV);  //u0y0v0y1

                    tempY = *inyuv_2_nextLine++;  //0000y1y0
                    temp = ((tempY & 0xFF) << 16) | (tempY >> 8); //00y000y1
                    *outyuv_4_nextLine++ = (temp | tempUV);  //u0y0v0y1
#endif

#if Y0VY1U //y0 v0 y1 u0
                    temp = (tempY & 0xFF) | ((tempY & 0xFF00) << 8) ; //00y100y0
                    tempUV = ((tempU << 16) | tempV) << 8; //u000v000
                    *outyuv_4++ = (temp | tempUV);  //u0y1v0y0

                    tempY = *inyuv_2_nextLine++;  //0000y1y0
                    temp = (tempY & 0xFF) | ((tempY & 0xFF00) << 8) ; //00y100y0
                    *outyuv_4_nextLine++ = (temp | tempUV);  //y1v0y0u0
#endif
                }

                inyuv_2 = inyuv_2_nextLine;
                inyuv_2_nextLine += (_mSrc_width >> 1) ;
                outyuv_4 = outyuv_4_nextLine + (offset >> 1);
                outyuv_4_nextLine = outyuv_4 + (lpitch >> 1);
            }
            break;

        case 1://Rotation90

            incb = inyuv + inYsize;
            incr = incb + (inYsize >> 2);

            lStart = ((lpitch * (_mDst_height - 1)) << 1);

            for (i = lheight >> 1; i > 0; i--)
            {
                outyuv_4 = (uint32*)(outyuv + lStart);
                inyuv_2_nextLine = inyuv_2 + (_mSrc_width >> 1);

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    tempY1 = *inyuv_2++;  //0000y1y0
                    tempY2 = *inyuv_2_nextLine++;  //0000y3y2
                    tempU = *incb++;  //u0
                    tempV = *incr++;  //v0
#if UY0VY1 //u0 y0 v0 y1                  
                    tempUV = (tempU) | (tempV << 16); //00v000u0
                    tempY1Y2 = ((tempY1 & 0xFF) << 8) | ((tempY2 & 0xFF) << 24); //y100y000
                    outyuv_4[0] = tempUV | tempY1Y2;

                    tempY1Y2 = ((tempY1 >> 8) << 8) | ((tempY2 >> 8) << 24); //y100y000
                    outyuv_4[-(lpitch>>1)] = tempUV | tempY1Y2;

                    outyuv_4 -= lpitch;
#endif

#if Y1VY0U //y1 v0 y0 u0

                    tempUV = (tempU << 24) | (tempV << 8); //u000v000
                    tempY1Y2 = ((tempY2 & 0xFF) | ((tempY1 & 0xFF) << 16)); //00y000y1
                    outyuv_4[0] = tempUV | tempY1Y2;

                    tempY1Y2 = ((tempY2 >> 8) | ((tempY1 >> 8) << 16));  //00y000y1
                    outyuv_4[-(lpitch>>1)] = tempUV | tempY1Y2;

                    outyuv_4 -= lpitch;
#endif

#if Y0VY1U //y0 v0 y1 u0

                    tempUV = (tempU << 24) | (tempV << 8); //u000v000
                    tempY1Y2 = ((tempY1 & 0xFF) | ((tempY2 & 0xFF) << 16)); //00y100y0
                    outyuv_4[0] = tempUV | tempY1Y2;

                    tempY1Y2 = ((tempY1 >> 8) | ((tempY2 >> 8) << 16));  //00y100y0
                    outyuv_4[-(lpitch>>1)] = tempUV | tempY1Y2;

                    outyuv_4 -= lpitch;

#endif
                }

                inyuv_2 = inyuv_2_nextLine;
                inyuv_2_nextLine += (_mSrc_width >> 1) ;
                lStart += 4;
            }
            break;

        case 2://Rotation180

            incb = inyuv + inYsize;
            incr = incb + (inYsize >> 2);

            outyuv_4 += ((outYsize >> 1) - 1);
            offset = lpitch - _mSrc_width;

            for (i = lheight >> 1; i > 0; i--)
            {
                inyuv_2_nextLine = inyuv_2 + (_mSrc_width >> 1);
                outyuv_4_nextLine = outyuv_4 - (lpitch >> 1);

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    tempY = *inyuv_2++;  //0000y1y0
                    tempU = *incb++;  //u0
                    tempV = *incr++;  //v0
#if UY0VY1 //u0 y0 v0 y1 --180 degree--> u0 y1 v0 y0
                    temp = (tempY << 24) | (tempY & 0xFF00); //y000y100
                    tempUV = (tempU | (tempV << 16)); //00v000u0
                    *outyuv_4-- = (temp | tempUV);  //y0v0y1u0

                    tempY = *inyuv_2_nextLine++;  //0000y1y0
                    temp = (tempY << 24) | (tempY & 0xFF00); //y000y100
                    *outyuv_4_nextLine-- = (temp | tempUV);  //y0v0y1u0
#endif

#if Y1VY0U //y1 v0 y0 u0 --180 degree--> y0 v0 y1 u0
                    temp = ((tempY & 0xFF00) << 8) | (tempY & 0xFF); //00y100y0
                    tempUV = (tempU << 24) | (tempV << 8); //u000v000
                    *outyuv_4-- = (temp | tempUV);  //u0y1v0y0

                    tempY = *inyuv_2_nextLine++;  //0000y1y0
                    temp = ((tempY & 0xFF00) << 8) | (tempY & 0xFF); //00y100y0
                    *outyuv_4_nextLine-- = (temp | tempUV);  //u0y1v0y0
#endif

#if Y0VY1U //y0 v0 y1 u0 --180 degree--> y1 v0 y0 u0
                    temp = ((tempY & 0xFF) << 16) | (tempY >> 8) ; //00y000y1
                    tempUV = ((tempU << 16) | tempV) << 8; //u000v000
                    *outyuv_4-- = (temp | tempUV);  //u0y0v0y1

                    tempY = *inyuv_2_nextLine++;  //0000y1y0
                    temp = ((tempY & 0xFF) << 16) | (tempY >> 8) ; //00y000y1
                    *outyuv_4_nextLine-- = (temp | tempUV);  //y0v0y1u0
#endif
                }

                inyuv_2 = inyuv_2_nextLine;
                inyuv_2_nextLine += (_mSrc_width >> 1) ;
                outyuv_4 = outyuv_4_nextLine - (offset >> 1);
                outyuv_4_nextLine = outyuv_4 - (lpitch >> 1);
            }
            break;

        case 3://Rotation270

            incb = inyuv + inYsize;
            incr = incb + (inYsize >> 2);

            lStart = (_mDst_width << 1) - 4;

            for (i = lheight >> 1; i > 0; i--)
            {
                outyuv_4 = (uint32*)(outyuv + lStart);
                inyuv_2_nextLine = inyuv_2 + (_mSrc_width >> 1);

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    tempY1 = *inyuv_2++;  //0000y1y0
                    tempY2 = *inyuv_2_nextLine++;  //0000y3y2
                    tempU = *incb++;  //u0
                    tempV = *incr++;  //v0

#if UY0VY1 //u0 y0 v0 y1                  
                    tempUV = (tempU) | (tempV << 16); //00v000u0
                    tempY1Y2 = ((tempY1 & 0xFF) << 24) | ((tempY2 & 0xFF) << 8); //y000y100
                    outyuv_4[0] = tempUV | tempY1Y2;

                    tempY1Y2 = ((tempY1 >> 8) << 24) | ((tempY2 >> 8) << 8); //y000y100
                    outyuv_4[(lpitch>>1)] = tempUV | tempY1Y2;

                    outyuv_4 += lpitch;
#endif

#if Y1VY0U //y1 v0 y0 u0

                    tempUV = (tempU << 24) | (tempV << 8); //u000v000
                    tempY1Y2 = (tempY1 & 0xFF) | ((tempY2 & 0xFF) << 16); //00y000y1
                    outyuv_4[0] = tempUV | tempY1Y2;

                    tempY1Y2 = (tempY1 >> 8) | ((tempY2 >> 8) << 16);  //00y000y1
                    outyuv_4[(lpitch>>1)] = tempUV | tempY1Y2;

                    outyuv_4 += lpitch;
#endif

#if Y0VY1U //y0 v0 y1 u0   

                    tempUV = (tempU << 24) | (tempV << 8); //u000v000
                    tempY1Y2 = ((tempY2 & 0xFF) | ((tempY1 & 0xFF) << 16)); //00y100y0
                    outyuv_4[0] = tempUV | tempY1Y2;

                    tempY1Y2 = ((tempY2 >> 8) | ((tempY1 >> 8) << 16));  //00y100y0
                    outyuv_4[(lpitch>>1)] = tempUV | tempY1Y2;

                    outyuv_4 += lpitch;

#endif
                }

                inyuv_2 = inyuv_2_nextLine;
                inyuv_2_nextLine += (_mSrc_width >> 1) ;
                lStart -= 4;
            }
            break;

        default:
            break;

    }//switch


    return 1;
}


int32 CCYUV420toYUV422::Convert(uint8 **inyuvBuf, uint8 *outyuvBuf)
{
    //TBD
    OSCL_ASSERT(inyuvBuf);

    Convert(inyuvBuf[0], outyuvBuf);

    return 1;
}



