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
/*******************************************************************************************
 Class :: CCYUV422toYUV420.cpp
------------------------------------
The YUV422 is fully interleaved and output format is YUV420 planar

********************************************************************************************/
#include "colorconv_config.h"
#include "ccyuv422toyuv420.h"


OSCL_EXPORT_REF ColorConvertBase* CCYUV422toYUV420 :: New()
{
    CCYUV422toYUV420* self = OSCL_NEW(CCYUV422toYUV420, ());
    return OSCL_STATIC_CAST(ColorConvertBase*, self);
}


CCYUV422toYUV420 :: CCYUV422toYUV420()
{
}


OSCL_EXPORT_REF CCYUV422toYUV420 :: ~CCYUV422toYUV420()
{
}


int32 CCYUV422toYUV420:: Init(int32 SrcWidth, int32 SrcHeight, int32 SrcPitch, int32 DstWidth, int32 DstHeight, int32 DstPitch, int32 nRotation)
{

    // check for either shrinking or zooming both horz and vert. No combination..

    if (ColorConvertBase::Init(SrcWidth, SrcHeight, SrcPitch, DstWidth, DstHeight, DstPitch, nRotation) == 0)
    {
        return 0;
    }
    if (Stretchline_Chroma() == 0)
    {
        return 0;
    }

    _mInitialized = true;
    _mDst_mheight = DstHeight;
    // set default
    SetYuvFullRange(false);
    SetMode(0);

    return 1;

}

int32 CCYUV422toYUV420:: Stretchline_Chroma()
{
    if (_mDst_width > _mDst_pitch)
    {
        return 0;
    }
    else if ((_mRotation&1) == 0) // check for either shrinking or zooming both horz and vert. No combination..
    {
        if ((_mSrc_width > _mDst_width && _mSrc_height < _mDst_height) ||
                (_mSrc_width < _mDst_width && _mSrc_height > _mDst_height))
        {
            return 0;
        }
    }
    else
    {
        if ((_mSrc_width > _mDst_height && _mSrc_height < _mDst_width) ||
                (_mSrc_width < _mDst_height && _mSrc_height > _mDst_width))
        {
            return 0;
        }
    }

    if ((_mRotation&0x1) == 0)
    { /* no rotation */
        if ((_mDst_width != _mSrc_width) || (_mDst_height != _mSrc_height))
        { /* scaling */
            //calulate the Row
            int32 leavecode = 0;
            OSCL_TRY(leavecode,
                     _mRowPix_chroma = OSCL_ARRAY_NEW(uint8, _mSrc_width >> 1);
                     _mColPix_chroma = OSCL_ARRAY_NEW(uint8, _mSrc_height >> 1);
                    );
            OSCL_FIRST_CATCH_ANY(leavecode,
                                 if (_mRowPix_chroma)
        {
            OSCL_ARRAY_DELETE(_mRowPix_chroma);
            }
            if (_mColPix_chroma)
        {
            OSCL_ARRAY_DELETE(_mColPix_chroma);
            }
            _mRowPix_chroma = NULL;
            _mColPix_chroma = NULL;
            return 0;
                                );
            StretchLine(_mRowPix_chroma, _mSrc_width >> 1, _mDst_width >> 1);
            StretchLine(_mColPix_chroma, _mSrc_height >> 1, _mDst_height >> 1);
        }
    }
    else
    { /* rotation,  */
        if ((_mDst_height != _mSrc_width) || (_mDst_width != _mSrc_height))
        { /* scaling */
            //calulate the Row
            int32 leavecode = 0;
            OSCL_TRY(leavecode,
                     _mRowPix_chroma = OSCL_ARRAY_NEW(uint8, _mSrc_height >> 1);
                     _mColPix_chroma = OSCL_ARRAY_NEW(uint8, _mSrc_width >> 1);
                    );
            OSCL_FIRST_CATCH_ANY(leavecode,
                                 if (_mRowPix_chroma)
        {
            OSCL_ARRAY_DELETE(_mRowPix_chroma);
            }
            if (_mColPix_chroma)
        {
            OSCL_ARRAY_DELETE(_mColPix_chroma);
            }
            _mRowPix_chroma = NULL;
            _mColPix_chroma = NULL;
            return 0;
                                );
            StretchLine(_mColPix_chroma, _mSrc_width >> 1, _mDst_height >> 1);
            StretchLine(_mRowPix_chroma, _mSrc_height >> 1, _mDst_width >> 1);
        }

    }
    return 1;

}


int32 CCYUV422toYUV420:: GetOutputBufferSize(void)
{
    OSCL_ASSERT(_mInitialized == true);

    return(_mDst_pitch * _mDst_mheight*3 / 2);
}


int32 CCYUV422toYUV420::SetMode(int32 nMode)
{
    OSCL_ASSERT(_mInitialized == true);
    if (nMode == 0)
    {
        mPtrYUV422to420 =   &CCYUV422toYUV420::ccYUV_SimpleRotation_Conversion;
        _mDisp.src_pitch = _mSrc_pitch  ;
        _mDisp.dst_pitch = _mDst_pitch  ;
        _mDisp.src_width = _mSrc_width  ;
        _mDisp.src_height = _mSrc_height ;
        _mDisp.dst_width = _mDst_width  ;
        _mDisp.dst_height = _mDst_height ;
        _mState     =   0;
    }
    else
    {
        if (_mIsZoom)
        {
            if (_mRotation&0x1) /* zoom and rotate(90 degree,270 degree) */
            {
                mPtrYUV422to420 =   &CCYUV422toYUV420::cc420ZoomRotate;
            }
            else /* zoom(Up-Down)with 180 degree rotation,Flip.Flip+180 degree rotaion*/
            {
                mPtrYUV422to420 =   &CCYUV422toYUV420::cc420ZoomIn;
            }
        }
        else
        {
            mPtrYUV422to420 =   &CCYUV422toYUV420::ccYUV_SimpleRotation_Conversion;
        }
        _mState     =   nMode;
    }

    return 1;
}

int32 CCYUV422toYUV420::SetYuvFullRange(bool range)
{
    OSCL_UNUSED_ARG(range);
    OSCL_ASSERT(_mInitialized == true);

    return 1;  // has no meaning in this class. Always return 1
}


int32 CCYUV422toYUV420::Convert(uint8 *inyuv, uint8 *outyuv)
{

    if ((*this.*mPtrYUV422to420)(inyuv, outyuv))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


int32 CCYUV422toYUV420::ccYUV_SimpleRotation_Conversion(uint8 *inyuv, uint8 *outyuv)
{
    int32 i, j;
    uint8 *outy, *outcb, *outcr;
    uint32 *inyuv_4, temp, temp_down, rshiftY1, rshiftY2;
    int32 outYsize, offset;

    int32 lpitch = _mDst_pitch;
    int32 lheight = _mSrc_height;
    int32 src_pitch_temp = _mSrc_pitch;

    OSCL_ASSERT(inyuv);
    OSCL_ASSERT(outyuv);
    OSCL_ASSERT(_mInitialized == true);

    outYsize = (lpitch * _mDst_mheight);

    inyuv_4 = (uint32 *)inyuv;


    if (_mIsFlip)
    {
        if (_mRotation == 2)
        {
            _mRotation = 6;//180 degree rotation+Flip
        }
        else if (_mRotation == 0)
        {
            _mRotation = 4;//Flip Only
        }
        else
        {
            //FLip+90Rotation(not supported:)
            //FLip+270Rotation(not supported:)
            return 0;
        }

    }

    switch (_mRotation)
    {
        case 0://Rotation0

#if UY0VY1
            rshiftY1 = 8;
            rshiftY2 = 24;
#endif
#if Y0VY1U
            rshiftY1 = 0;
            rshiftY2 = 16;
#endif
#if Y1VY0U
            rshiftY1 = 16;
            rshiftY2 = 0;
#endif
            outy = outyuv;
            outcb = outy + outYsize;
            outcr = outcb + (outYsize >> 2);

            offset = lpitch - _mSrc_width;

            for (i = lheight >> 1; i > 0; i--)
            {
                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *inyuv_4++;

                    *outcb++ = PIXEL_EXTRACT(temp, RSHIFTCB);
                    *outy++  = PIXEL_EXTRACT(temp, rshiftY1);
                    *outcr++ = PIXEL_EXTRACT(temp, RSHIFTCR);
                    *outy++  = PIXEL_EXTRACT(temp, rshiftY2);

                }
                /* in case the dest pitch is larger than width */
                outcb += (offset >> 1);
                outcr += (offset >> 1);
                outy += offset;

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *inyuv_4++;

                    *outy++  = PIXEL_EXTRACT(temp, rshiftY1);
                    *outy++  = PIXEL_EXTRACT(temp, rshiftY2);
                }
                outy += offset;
            }
            break;

        case 1: // Rotation90
#if UY0VY1
            rshiftY1 = 8;
            rshiftY2 = 24;
#endif
#if Y0VY1U
            rshiftY1 = 0;
            rshiftY2 = 16;
#endif
#if Y1VY0U
            rshiftY1 = 16;
            rshiftY2 = 0;
#endif
            offset = lpitch - lheight;

            outy = outyuv + outYsize;
            outcb = outy + (outYsize >> 2);
            outcr = outcb + (outYsize >> 2);

            for (i = lheight >> 1; i > 0; i--)
            {

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *inyuv_4++;

                    outcb -= (lpitch >> 1);
                    *outcb = PIXEL_EXTRACT(temp, RSHIFTCB);
                    outy -= (lpitch << 1);
                    outy[lpitch] = PIXEL_EXTRACT(temp, rshiftY1);
                    outcr -= (lpitch >> 1);
                    *outcr = PIXEL_EXTRACT(temp, RSHIFTCR);
                    *outy = PIXEL_EXTRACT(temp, rshiftY2);
                }

                outy += (outYsize + 1);

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *inyuv_4++;
                    outy -= (lpitch << 1);
                    outy[lpitch] = PIXEL_EXTRACT(temp, rshiftY1);
                    *outy = PIXEL_EXTRACT(temp, rshiftY2);

                }

                outy += (outYsize + 1);
                outcb += ((outYsize >> 2) + 1);
                outcr += ((outYsize >> 2) + 1);
            }
            break;


        case 2://Rotation180
#if UY0VY1
            rshiftY1 = 24;
            rshiftY2 = 8;
#endif
#if Y0VY1U
            rshiftY1 = 16;
            rshiftY2 = 0;
#endif
#if Y1VY0U
            rshiftY1 = 0;
            rshiftY2 = 16;
#endif
            outy = outyuv;
            offset = lpitch - _mSrc_width;
            outy += (outYsize - 1) - offset;
            outcb = outy + (outYsize >> 2) + (offset >> 1);
            outcr = outcb + (outYsize >> 2);

            for (i = lheight >> 1; i > 0; i--)
            {
                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *inyuv_4++;

                    *outcb-- =  PIXEL_EXTRACT(temp, RSHIFTCB);
                    *outy--  =  PIXEL_EXTRACT(temp, rshiftY2);
                    *outcr-- =  PIXEL_EXTRACT(temp, RSHIFTCR);
                    *outy--  =  PIXEL_EXTRACT(temp, rshiftY1);

                }
                /* in case the dest pitch is larger than width */
                outcb -= (offset >> 1);
                outcr -= (offset >> 1);
                outy -= offset;

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *inyuv_4++;

                    *outy--  =  PIXEL_EXTRACT(temp, rshiftY2);
                    *outy--  =  PIXEL_EXTRACT(temp, rshiftY1);

                }
                outy -= offset;
            }
            break;

        case 3: // Rotation270

#if UY0VY1
            rshiftY1 = 8;
            rshiftY2 = 24;
#endif
#if Y0VY1U
            rshiftY1 = 0;
            rshiftY2 = 16;
#endif
#if Y1VY0U
            rshiftY1 = 16;
            rshiftY2 = 0;
#endif

            offset = lpitch - lheight;

            outy = outyuv + outYsize;
            outcb = outy + (outYsize >> 2);
            outcr = outcb + (outYsize >> 2);

            inyuv_4 = (uint32 *)(inyuv + ((_mSrc_width << 1) * lheight));

            for (i = lheight >> 1; i > 0; i--)
            {

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *--inyuv_4;

                    outcb -= (lpitch >> 1);
                    *outcb = PIXEL_EXTRACT(temp, RSHIFTCB);
                    outy -= (lpitch << 1);
                    outy[lpitch] = PIXEL_EXTRACT(temp, rshiftY2);
                    outcr -= (lpitch >> 1);
                    *outcr = PIXEL_EXTRACT(temp, RSHIFTCR);
                    *outy = PIXEL_EXTRACT(temp, rshiftY1);

                }

                outy += (outYsize + 1);

                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *--inyuv_4;

                    outy -= (lpitch << 1);
                    outy[lpitch] = PIXEL_EXTRACT(temp, rshiftY2);
                    *outy = PIXEL_EXTRACT(temp, rshiftY1);
                }

                outy += (outYsize + 1);
                outcb += ((outYsize >> 2) + 1);
                outcr += ((outYsize >> 2) + 1);
            }
            break;
        case 4: // Flip only
#if UY0VY1
            rshiftY1 = 24;
            rshiftY2 = 8;
#endif
#if Y0VY1U
            rshiftY1 = 16;
            rshiftY2 = 0;
#endif
#if Y1VY0U
            rshiftY1 = 0;
            rshiftY2 = 16;
#endif

            offset = (_mSrc_pitch * 3);

            outy =  outyuv;
            outcb = outy + (outYsize);
            outcr = outcb + (outYsize >> 2);

            inyuv_4 = (uint32 *)(inyuv + (_mSrc_pitch << 1) - 4);

            for (i = lheight >> 1; i > 0; i--)
            {
                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *inyuv_4;
                    temp_down = inyuv_4[_mSrc_pitch>>1];
                    inyuv_4--;

                    *outy = PIXEL_EXTRACT(temp, rshiftY1);
                    outy[_mDst_pitch] = PIXEL_EXTRACT(temp_down, rshiftY1);
                    outy++;
                    *outy = PIXEL_EXTRACT(temp, rshiftY2);
                    outy[_mDst_pitch] = PIXEL_EXTRACT(temp_down, rshiftY2);
                    outy++;
                    *outcb++ = PIXEL_EXTRACT(temp, RSHIFTCB);
                    *outcr++ = PIXEL_EXTRACT(temp, RSHIFTCR);
                }
                outy += ((_mDst_pitch << 1) - _mDst_width);
                outcb += (_mDst_pitch >> 1) - (_mDst_width >> 1);
                outcr += (_mDst_pitch >> 1) - (_mDst_width >> 1);;

                inyuv_4 += (offset >> 1);
            }
            break;

        case 6: // Flip+180
#if UY0VY1
            rshiftY1 = 8;
            rshiftY2 = 24;
#endif
#if Y0VY1U
            rshiftY1 = 0;;
            rshiftY2 = 16;
#endif
#if Y1VY0U
            rshiftY1 = 16;
            rshiftY2 = 0;
#endif
            /* move the starting point to the bottom-left corner of the picture */
            offset = - (_mSrc_pitch * 3);

            outy =  outyuv;
            outcb = outy + (outYsize);
            outcr = outcb + (outYsize >> 2);

            inyuv_4 = (uint32 *)(inyuv + ((_mSrc_pitch << 1) * (lheight - 1)));
            src_pitch_temp = -_mSrc_pitch;

            for (i = lheight >> 1; i > 0; i--)
            {
                for (j = _mSrc_width >> 1; j > 0; j--)
                {
                    temp = *inyuv_4;
                    temp_down = inyuv_4[src_pitch_temp>>1];
                    inyuv_4++;

                    *outy = PIXEL_EXTRACT(temp, rshiftY1);
                    outy[_mDst_pitch] = PIXEL_EXTRACT(temp_down, rshiftY1);
                    outy++;
                    *outy = PIXEL_EXTRACT(temp, rshiftY2);
                    outy[_mDst_pitch] = PIXEL_EXTRACT(temp_down, rshiftY2);
                    outy++;
                    *outcb++ = PIXEL_EXTRACT(temp, RSHIFTCB);
                    *outcr++ = PIXEL_EXTRACT(temp, RSHIFTCR);
                }

                outy += ((_mDst_pitch << 1) - _mDst_width);
                outcb += (_mDst_pitch >> 1) - (_mDst_width >> 1);
                outcr += (_mDst_pitch >> 1) - (_mDst_width >> 1);

                inyuv_4 += (offset >> 1);
            }

        default:
            break;

    }//switch

    return 1;

}
// platform specific C function
int32 YUV422to420Scaledown(uint8 *src, uint8 *dst, uint32 *disp,
                           uint8 *_mRowPix, uint8 *_mColPix, uint8 *_mRowPix_chroma, uint8 *_mColPix_chroma);
int32 YUV422to420Scaleup(uint8 *src, uint8 *dst, uint32 *disp,
                         uint8 *_mRowPix, uint8 *_mColPix, uint8 *_mRowPix_chroma, uint8 *_mColPix_chroma);

int32 YUV422to420ScaleRotate(uint8 *src, uint8 *dst, uint32 *disp,
                             uint8 *_mRowPix, uint8 *_mColPix, uint8 *_mRowPix_chroma, uint8 *_mColPix_chroma, bool _mIsRotateClkwise);


int32 CCYUV422toYUV420::cc420ZoomIn(uint8 *inyuv, uint8 *outyuv)
{

    uint32 disp_prop[8];

    disp_prop[0] = _mDisp.src_pitch;
    disp_prop[1] =  _mDisp.dst_pitch;
    disp_prop[2] = _mDisp.src_width;
    disp_prop[3] = _mDisp.src_height;
    disp_prop[4] = _mDisp.dst_width;
    disp_prop[5] = _mDisp.dst_height;
    disp_prop[6] = (_mRotation > 0 ? 1 : 0);
    disp_prop[7] = _mIsFlip;

    if (_mDisp.src_width > _mDisp.dst_width)  /* scale down in width */
    {
        return YUV422to420Scaledown(inyuv, outyuv, disp_prop, _mRowPix, _mColPix, _mRowPix_chroma, _mColPix_chroma);
    }
    else
    {
        return YUV422to420Scaleup(inyuv, outyuv, disp_prop, _mRowPix, _mColPix, _mRowPix_chroma, _mColPix_chroma);
    }

    return 1;

}

int32 CCYUV422toYUV420::cc420ZoomRotate(uint8 *inyuv, uint8 *outyuv)
{

    uint32 disp_prop[8];

    disp_prop[0] = _mDisp.src_pitch;
    disp_prop[1] =  _mDisp.dst_pitch;
    disp_prop[2] = _mDisp.src_width;
    disp_prop[3] = _mDisp.src_height;
    disp_prop[4] = _mDisp.dst_width;
    disp_prop[5] = _mDisp.dst_height;
    disp_prop[6] = (_mRotation > 0 ? 1 : 0);
    disp_prop[7] = _mIsFlip;

    if (!_mIsFlip)
    {
        return YUV422to420ScaleRotate(inyuv, outyuv, disp_prop, _mRowPix, _mColPix,
                                      _mRowPix_chroma, _mColPix_chroma, (_mRotation == CCROTATE_CLKWISE));
    }
    else
    {
        //Rotation+Flip not supported
        return 0;
    }

}


int32 YUV422to420Scaledown(uint8 *src, uint8 *dst, uint32 *disp_prop,
                           uint8 *_mRowPix, uint8 *_mColPix, uint8 *_mRowPix_chroma, uint8 *_mColPix_chroma)
{

    uint8 *outy, *outcb, *outcr, rshiftY1, rshiftY2;
    uint8 *rowpix, *colpix;

    int32 i32row, i32col, src_inc;
    int32 src_pitch, dst_pitch, src_width, src_height, dst_width, src_pitch_temp;

    uint32 *inyuv_4, temp_up, temp_down, outYsize, deltaY, deltaCbCr;


    OSCL_ASSERT(src);
    OSCL_ASSERT(dst);

    src_pitch   =   disp_prop[0];
    dst_pitch   =   disp_prop[1];
    src_width   =   disp_prop[2];
    src_height  =   disp_prop[3];
    dst_width   =   disp_prop[4];

    outYsize = (dst_pitch *  disp_prop[5]);/* multiplication of dst pitch and dst height*/

    if ((disp_prop[6] ^ disp_prop[7]))//EXOR for _mRotation, _mIsFlip
    {
        if (disp_prop[6])/* rotation 180 only */
        {
            /* move upto the last Quadruple at the bottom-right corner of the picture */
            deltaY = ((src_pitch << 1) * (src_height)) - 4;
            /*Src pointer will move in reverse direction*/
            src_pitch = src_pitch_temp = -src_pitch;
        }
        else /* flip only */
        {
            /* move upto the last Quadruple at the top-right corner of the picture */
            deltaY = ((src_pitch << 1)) - 4;
            src_pitch_temp = src_pitch;
            /*Src pointer needs to point at the end of third row after one Row Scaling iteration*/
            src_pitch = (src_pitch * 3);
        }
#if UY0VY1
        rshiftY1 = 24;/*In case of 180rotation or Flip, this Y pixel will come second*/
        rshiftY2 = 8; /*In case of 180rotation or Flip, this Y pixel will come first*/
#endif
#if Y0VY1U
        rshiftY1 = 0;
        rshiftY2 = 16;
#endif
#if Y1VY0U
        rshiftY1 = 16;
        rshiftY2 = 0;
#endif
        src_inc = -1;
    }
    else // rotate 180 and flip ||  no rotate, no flip
    {
        if (disp_prop[6])    // rotate 180 and flip
        {
            /* move the starting point to the bottom-left corner of the picture */
            deltaY = ((src_pitch << 1) * (src_height - 1));
            src_pitch_temp = -src_pitch;
            src_pitch = -(src_pitch * 3);

        }
        else  // no rotate, no flip
        {
            src_pitch_temp = src_pitch;
            deltaY = 0;
        }
#if UY0VY1
        rshiftY1 = 8;/*Normal sequence of Y Pixels*/
        rshiftY2 = 24;
#endif
#if Y0VY1U
        rshiftY1 = 16;
        rshiftY2 = 0;
#endif
#if Y1VY0U
        rshiftY1 = 0;
        rshiftY2 = 16;
#endif
        src_inc = 1;
    }

    inyuv_4 = (uint32 *)(src + deltaY);
    outy = dst;
    outcb = outy + outYsize;
    outcr = outcb + (outYsize >> 2);
    deltaCbCr = (dst_pitch >> 1) - (dst_width >> 1);
    colpix = _mColPix + (src_height - 1);/* decrement index, _mColPix[.] is symmetric to increment index */

    /*Scaling-Conversion for Luma*/

    for (i32row = (src_height >> 1); i32row > 0; i32row--)
    {
        rowpix = _mRowPix + (src_width - 1);
        if (colpix[-1] + colpix[0] == 0)
        {
            inyuv_4 += src_pitch_temp;
            colpix -= 2;
            continue;
        }
        if (colpix[-1] + colpix[0] == 1) // one line not skipped
        {
            for (i32col = (src_width >> 1); i32col > 0; i32col--)
            {
                temp_up = *inyuv_4;
                inyuv_4 += src_inc;
                if (*rowpix) /* compute this pixel */
                {
                    *outy = PIXEL_EXTRACT(temp_up, rshiftY1);
                    outy ++;

                }
                rowpix--;
                if (*rowpix) /* compute this pixel */
                {
                    *outy = PIXEL_EXTRACT(temp_up, rshiftY2);
                    outy ++;
                }
                rowpix--;
            }
            inyuv_4 += (src_pitch >> 1);
            outy   += (dst_pitch - dst_width);
        }
        else  //both lines not skipped
        {
            for (i32col = (src_width >> 1); i32col > 0; i32col--)
            {

                temp_up = *inyuv_4;
                temp_down = inyuv_4[src_pitch_temp>>1];
                inyuv_4 += src_inc;
                if (*rowpix) /* compute this pixel */
                {

                    *outy = PIXEL_EXTRACT(temp_up, rshiftY1);
                    outy[dst_pitch] = PIXEL_EXTRACT(temp_down, rshiftY1);
                    outy++;
                }
                rowpix--;

                if (*rowpix) /* compute this pixel */
                {
                    *outy = PIXEL_EXTRACT(temp_up, rshiftY2);
                    outy[dst_pitch] = PIXEL_EXTRACT(temp_down, rshiftY2);

                    outy++;
                }
                rowpix--;
            }
            outy += ((dst_pitch << 1) - dst_width);
            inyuv_4 += (src_pitch >> 1);

        }//both lines not skipped

        colpix -= 2;
    }


    /*Scaling-Conversion for Chroma*/
    colpix = _mColPix_chroma;
    inyuv_4 = (uint32 *)(src + deltaY);

    /*This Computation is being done on alternate Chroma Rows*/
    for (i32row = (src_height) >> 1; i32row > 0; i32row--)
    {
        rowpix = _mRowPix_chroma;
        if (*colpix++)/*This line not skipped*/
        {
            for (i32col = (src_width >> 1); i32col > 0; i32col--)
            {
                temp_up = *inyuv_4;
                inyuv_4 += src_inc;

                if (*rowpix++)/*Compute this pixel*/
                {
                    *outcb++ = PIXEL_EXTRACT(temp_up, RSHIFTCB);
                    *outcr++ = PIXEL_EXTRACT(temp_up, RSHIFTCR);
                }
            }
            inyuv_4 += (src_pitch >> 1);
            outcb += deltaCbCr;
            outcr += deltaCbCr;
        }
        else/*Skipped line*/
        {
            inyuv_4 += (src_pitch_temp);
        }
    }

    return 1;

}

int32 YUV422to420Scaleup(uint8 *src, uint8 *dst, uint32 *disp_prop,
                         uint8 *_mRowPix, uint8 *_mColPix, uint8 *_mRowPix_chroma, uint8 *_mColPix_chroma)
{

    uint8 *outy, *outcb, *outcr, *outy_temp, *outcb_temp, *outcr_temp, *rowpix, *colpix;
    uint8 i, rshiftY1, rshiftY2, ui8Pixel_val1, ui8Pixel_val2;

    int32 src_pitch, dst_pitch, src_width, src_height, dst_width, src_inc, src_pitch_temp;
    int32 i32row, i32col;

    uint32 *inyuv_4, temp_up;
    uint32 outYsize, deltaY, deltaCbCr;


    OSCL_ASSERT(src);
    OSCL_ASSERT(dst);

    src_pitch   =   disp_prop[0];
    dst_pitch   =   disp_prop[1];
    src_width   =   disp_prop[2];
    src_height  =   disp_prop[3];
    dst_width   =   disp_prop[4];

    outYsize = (dst_pitch *  disp_prop[5]);/* multiplication of dst pitch and dst height*/
    src_pitch_temp = 0;

    if ((disp_prop[6] ^ disp_prop[7]))//EXOR for _mRotation, _mIsFlip
    {
        if (disp_prop[6])/* rotation 180 only */
        {
            /* move upto the last quadruple to the bottom-right corner of the picture */
            deltaY = ((src_pitch << 1) * (src_height)) - 4;
            src_pitch = -src_pitch;
        }
        else /* flip only */
        {
            /* move upto the last quadruple point at the top-right corner of the picture */
            deltaY = ((src_pitch << 1)) - 4;
            src_pitch_temp = src_pitch;
        }

#if UY0VY1
        rshiftY1 = 24;/*In case of 180 rotation or Flip, this Y pixel will come second*/
        rshiftY2 = 8;/*In case of 180 rotation or Flip, this Y pixel will come first*/
#endif
#if Y0VY1U
        rshiftY1 = 16;
        rshiftY2 = 0;
#endif
#if Y1VY0U
        rshiftY1 = 0;
        rshiftY2 = 16;
#endif
        src_inc = -1;
    }
    else // rotate 180 and flip ||  no rotate, no flip
    {
        if (disp_prop[6])    // rotate 180 and flip
        {
            /* move the starting point to the bottom-left corner of the picture */
            deltaY = ((src_pitch << 1) * (src_height - 1));
            src_pitch_temp = -(src_pitch);
            src_pitch = -(src_pitch);
        }
        else  // no rotate, no flip
        {
            deltaY = 0;
        }
#if UY0VY1
        rshiftY1 = 8;/*Normal sequence of Y Pixels*/
        rshiftY2 = 24;
#endif
#if Y0VY1U
        rshiftY1 = 0;
        rshiftY2 = 16;
#endif
#if Y1VY0U
        rshiftY1 = 16;
        rshiftY2 = 0;
#endif

        src_inc = 1;
    }

    inyuv_4 = (uint32 *)(src + deltaY);
    outy = dst;
    outcb = outy + outYsize;
    outcr = outcb + (outYsize >> 2);

    /*Scaling-UpScaling for Luma Block*/
    deltaCbCr = (dst_pitch >> 1) - (dst_width >> 1);
    colpix = _mColPix + (src_height - 1);/* decrement index, _mColPix[.] is symmetric to increment index */

    for (i32row = src_height; i32row > 0; i32row--)
    {/* decrement index, _mRowPix[.] is symmetric to increment index */
        rowpix = _mRowPix + (src_width - 1);
        outy_temp = outy;

        for (i32col = src_width >> 1; i32col > 0; i32col--)
        {
            temp_up = *inyuv_4;
            inyuv_4 += src_inc;

            ui8Pixel_val1 = PIXEL_EXTRACT(temp_up, rshiftY1);
            for (i = 0; i < *rowpix; i++)//copy top left Y *rowpix times
            {
                *outy++ = ui8Pixel_val1;
            }
            rowpix--;

            ui8Pixel_val1 = PIXEL_EXTRACT(temp_up, rshiftY2);
            for (i = 0; i < *rowpix; i++)//copy top right Y *rowpix times
            {

                *outy++ = ui8Pixel_val1;
            }
            rowpix--;

        }
        outy += ((dst_pitch) - dst_width);
        for (i = 1; i < *colpix; i++)
        {
            oscl_memcpy(outy, outy_temp, dst_pitch);
            outy += (dst_pitch);
        }
        colpix--;

        inyuv_4 += src_pitch_temp;

    }

    /*Scaling-UpScaling for Chroma Block*/
    colpix = _mColPix_chroma;
    inyuv_4 = (uint32 *)(src + deltaY);


    /*This Computation is being done on alternate Chroma Rows*/
    for (i32row = (src_height) >> 1; i32row > 0; i32row--)
    {
        rowpix = _mRowPix_chroma;
        outcb_temp = outcb;
        outcr_temp = outcr;

        for (i32col = src_width >> 1; i32col > 0; i32col--)
        {
            temp_up = *inyuv_4;
            inyuv_4 += src_inc;

            ui8Pixel_val1 = PIXEL_EXTRACT(temp_up, RSHIFTCB);
            ui8Pixel_val2 = PIXEL_EXTRACT(temp_up, RSHIFTCR);

            for (i = 0; i < *rowpix; i++)
            {
                *outcb++ = ui8Pixel_val1;
                *outcr++ = ui8Pixel_val2;
            }
            rowpix++;
        }
        outcb += deltaCbCr;
        outcr += deltaCbCr;
        for (i = 1; i < *colpix; i++)
        {
            oscl_memcpy(outcb, outcb_temp, dst_pitch >> 1);
            oscl_memcpy(outcr, outcr_temp, dst_pitch >> 1);

            outcb += (dst_pitch >> 1);
            outcr += (dst_pitch >> 1);

        }
        colpix++;
        /*In case of filp it will point at the end Quadruple of alternate rows,Hence extra offset src_pitch_temp*/
        inyuv_4 += (src_pitch >> 1) + (src_pitch_temp);

    }

    return 1;

}

int32 YUV422to420ScaleRotate(uint8 *src, uint8 *dst, uint32 *disp_prop,
                             uint8 *_mRowPix, uint8 *_mColPix, uint8 *_mRowPix_chroma, uint8 *_mColPix_chroma, bool _mIsRotateClkwise)
{

    int8 src_inc, dst_inc;
    uint8 *outy, *outcb, *outcr, *outy_temp1, *outy_temp2, i, *outcb_temp, *outcr_temp;
    uint8 *rowpix, *colpix, ui8Pixel_val1, ui8Pixel_val2, rshiftY1, rshiftY2;

    uint32 *inyuv_4, temp_up, outYsize, i32row, i32col, j;

    int32  deltaY1, deltaY2, deltaCbCr;
    int32 src_pitch, dst_pitch, src_width, src_height, dst_width, dst_pitch_temp;


    OSCL_ASSERT(src);
    OSCL_ASSERT(dst);

    src_pitch   =   disp_prop[0];
    dst_pitch   =   disp_prop[1];
    src_width   =   disp_prop[2];
    src_height  =   disp_prop[3];
    dst_width   =   disp_prop[4];

    outYsize = (dst_pitch *  disp_prop[5]);/* multiplication of dst pitch and dst height*/
    src_inc =  1;

#if UY0VY1
    rshiftY1 = 8;
    rshiftY2 = 24;
#endif
#if Y0VY1U
    rshiftY1 = 0;
    rshiftY2 = 16;
#endif
#if Y1VY0U
    rshiftY1 = 16;
    rshiftY2 = 0;
#endif

    if (_mIsRotateClkwise)/*270 degree*/
    {
        dst_inc = -1;
        dst_pitch_temp = dst_pitch;
        outy = dst + (dst_width - 1);

        deltaY1 = -(outYsize + 1);
        deltaY2 = -(outYsize + 2);
        deltaCbCr = (dst_width >> 1) - 1;

    }
    else // rotate counterclockwise/*90 degree*/
    {
        dst_inc = 1;
        dst_pitch_temp = -dst_pitch;
        outy = dst + (outYsize - dst_pitch);

        deltaY1 = outYsize + 1;
        deltaY2 = (deltaY1 - dst_pitch);
        deltaCbCr = (outYsize >> 2) - (dst_pitch >> 1);
    }
    inyuv_4 = (uint32 *)(src);

    /* decrement index, _mRowPix[.] is symmetric to increment index */
    rowpix = _mRowPix + src_height - 1;

    /*Scaling-Conversion for Luma*/
    for (i32row = disp_prop[3]; i32row > 0; i32row--)
    {/* decrement index, _mRowPix[.] is symmetric to increment index */
        colpix = _mColPix + src_width - 1;
        if (*rowpix)
        {
            for (i32col = src_width >> 1; i32col > 0; i32col--)
            {
                temp_up = *inyuv_4;
                inyuv_4 += src_inc;

                ui8Pixel_val1 = PIXEL_EXTRACT(temp_up, rshiftY1);
                for (i = 0; i < *colpix; i++)//copy top left Y *rowpix times
                {
                    *outy = ui8Pixel_val1;
                    outy += dst_pitch_temp;
                }
                colpix--;

                ui8Pixel_val1 = PIXEL_EXTRACT(temp_up, rshiftY2);
                for (i = 0; i < *colpix; i++)//copy top right Y *rowpix times
                {
                    *outy = ui8Pixel_val1;
                    outy += dst_pitch_temp;
                }
                colpix--;

            }
            outy += deltaY1;
        }
        else
        {
            inyuv_4 += (src_pitch >> 1);
        }
        outy_temp1 = outy - (dst_inc);

        for (i = 1; i < *rowpix; i++)
        {
            outy_temp2 = outy_temp1;
            for (j = 0; j < disp_prop[5]; j++)//loop fot dst_height
            {
                *outy = *outy_temp2;
                outy += dst_pitch_temp;
                outy_temp2 += dst_pitch_temp;
            }
            outy += deltaY1;
        }
        rowpix--;

    }

    /*Scaling-UpScaling for Chroma Block*/
    outcb = (dst + outYsize) + (deltaCbCr);
    outcr = outcb + (outYsize >> 2);
    dst_pitch_temp = dst_pitch_temp >> 1;

    if (_mIsRotateClkwise)/*270 degree*/
    {
        deltaCbCr = -((outYsize >> 2) + 1);
    }
    else/*90 degree*/
    {
        deltaCbCr = (outYsize >> 2) + 1;
    }

    /*Scaling-Conversion for Chroma*/
    rowpix = _mRowPix_chroma;

    inyuv_4 = (uint32 *)(src);
    /*This Computation is being done on alternate Chroma Rows*/
    for (i32row = (src_height) >> 1; i32row > 0; i32row--)
    {
        colpix = _mColPix_chroma;
        if (*rowpix)
        {
            for (i32col = src_width >> 1; i32col > 0; i32col--)
            {
                temp_up = *inyuv_4;
                inyuv_4 += src_inc;

                ui8Pixel_val1 = PIXEL_EXTRACT(temp_up, RSHIFTCB);
                ui8Pixel_val2 = PIXEL_EXTRACT(temp_up, RSHIFTCR);

                for (i = 0; i < *colpix; i++)
                {
                    *outcb = ui8Pixel_val1;
                    *outcr = ui8Pixel_val2;
                    outcb += dst_pitch_temp;
                    outcr += dst_pitch_temp;
                }
                colpix++;
            }
            outcb += deltaCbCr;
            outcr += deltaCbCr;
        }
        else
        {
            inyuv_4 += (src_pitch >> 1);
        }

        outcb_temp = outcb - (dst_inc);
        outcr_temp = outcr - (dst_inc);

        for (i = 1; i < *rowpix; i++)
        {
            outy_temp1 = outcb_temp;
            outy_temp2 = outcr_temp;

            for (j = 0; j<disp_prop[5] >> 1; j++)//loop for dst_height
            {
                *outcb = *outy_temp1;
                outcb += dst_pitch_temp;
                outy_temp1 += dst_pitch_temp;

                *outcr = *outy_temp2;
                outcr += dst_pitch_temp;
                outy_temp2 += dst_pitch_temp;

            }
            outcb += deltaCbCr;
            outcr += deltaCbCr;
        }

        rowpix++;
        inyuv_4 += (src_pitch >> 1);
    }

    return 1;
}
