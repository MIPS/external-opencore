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
#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif

#include "mpeg4_dec.h"
#include "oscl_mem.h"
#include "omx_mpeg4_component.h"

#if PROFILING_ON
#include "oscl_tickcount.h"
#endif

#define MAX_LAYERS 1
#define PVH263DEFAULTHEIGHT 288
#define PVH263DEFAULTWIDTH 352

// from m4v_config_parser.h
OSCL_IMPORT_REF int16 iGetM4VConfigInfo(uint8 *buffer, int32 length, int32 *width, int32 *height, int32 *, int32 *);

Mpeg4Decoder_OMX::Mpeg4Decoder_OMX(OmxComponentBase *pComp)
{

    ipOMXComponent = pComp;
    CodecMode = MPEG4_MODE; // this can change
    iReferenceYUVWasSet = OMX_FALSE;
    ipRefCtrPreviousReferenceBuffer = NULL;
    Mpeg4InitCompleteFlag = OMX_FALSE;

    iFrameSize = 0;
    iDisplay_Width = 0;
    iDisplay_Height = 0;

    VO_START_CODE1[0] = 0x00;
    VO_START_CODE1[1] = 0x00;
    VO_START_CODE1[2] = 0x01;
    VO_START_CODE1[3] = 0x00;

    VOSH_START_CODE1[0] = 0x00;
    VOSH_START_CODE1[1] = 0x00;
    VOSH_START_CODE1[2] = 0x01;
    VOSH_START_CODE1[3] = 0xB0;

    VOP_START_CODE1[0] = 0x00;
    VOP_START_CODE1[1] = 0x00;
    VOP_START_CODE1[2] = 0x01;
    VOP_START_CODE1[3] = 0xB6;

    H263_START_CODE1[0] = 0x00;
    H263_START_CODE1[1] = 0x00;
    H263_START_CODE1[2] = 0x80;

#if PROFILING_ON
    iTotalTicks = 0;
#endif

}


/* Initialization routine */
OMX_ERRORTYPE Mpeg4Decoder_OMX::Mp4DecInit()
{
    Mpeg4InitCompleteFlag = OMX_FALSE;
    return OMX_ErrorNone;
}


/*Decode routine */
OMX_BOOL Mpeg4Decoder_OMX::Mp4DecodeVideo(OMX_BUFFERHEADERTYPE* aOutBuffer, OMX_U32* aOutputLength,
        OMX_U8** aInputBuf, OMX_U32* aInBufSize,
        OMX_PARAM_PORTDEFINITIONTYPE* aPortParam, OMX_BOOL aDeBlocking,
        OMX_S32* aFrameCount, OMX_BOOL aMarkerFlag, OMX_BOOL *aResizeFlag)
{
    OMX_BOOL Status = OMX_TRUE;
    OMX_S32 OldWidth, OldHeight;

    OldWidth = aPortParam->format.video.nFrameWidth;
    OldHeight = aPortParam->format.video.nFrameHeight;
    *aResizeFlag = OMX_FALSE;

#ifdef _DEBUG
    static OMX_U32 FrameCount = 0;
#endif
    uint UseExtTimestamp = 0;
    uint32 TimeStamp;
    OMX_S32 InputSize, InitSize;


    if ((Mpeg4InitCompleteFlag == OMX_FALSE) && (MPEG4_MODE == CodecMode))
    {
        if (!aMarkerFlag)
        {
            InitSize = GetVideoHeader(0, *aInputBuf, *aInBufSize);
        }
        else
        {
            InitSize = *aInBufSize;
        }

        if (PV_TRUE != InitializeVideoDecode(&iDisplay_Width, &iDisplay_Height,
                                             aInputBuf, (OMX_S32*)aInBufSize, MPEG4_MODE, aDeBlocking))
            return OMX_FALSE;

        Mpeg4InitCompleteFlag = OMX_TRUE;

        if (CodecMode == H263_MODE)
        {
            /* SVH mode is activated inside the decoder.
            ** Reset The init flag to get the width/height next frame.
            */
            Mpeg4InitCompleteFlag = OMX_FALSE;
            *aInBufSize -= InitSize;
            return OMX_TRUE;
        }
        aPortParam->format.video.nFrameWidth = iDisplay_Width;
        aPortParam->format.video.nFrameHeight = iDisplay_Height;

        OMX_U32 min_stride = ((aPortParam->format.video.nFrameWidth + 15) & (~15));
        OMX_U32 min_sliceheight = ((aPortParam->format.video.nFrameHeight + 15) & (~15));


        aPortParam->format.video.nStride = min_stride;
        aPortParam->format.video.nSliceHeight = min_sliceheight;


        // finally, compute the new minimum buffer size.

        // Decoder components always output YUV420 format
        aPortParam->nBufferSize = (aPortParam->format.video.nSliceHeight * aPortParam->format.video.nStride * 3) >> 1;

        iFrameSize = (aPortParam->format.video.nSliceHeight * aPortParam->format.video.nStride);

        // normally - in case of mp4 - the config parser will read the vol header ahead of time
        // and set the port values - so there'll be no port reconfig - but just in case ...

        CheckPortReConfig(aOutBuffer, OldWidth, OldHeight, aResizeFlag, aFrameCount);

        *aInBufSize -= InitSize;
        return OMX_TRUE;
    }



    if ((*(OMX_S32*)aInBufSize) <= 0)
    {
        return OMX_FALSE;
    }

    TimeStamp = 0xFFFFFFFF;
    InputSize = *aInBufSize;

    // in case of H263, read the 1st frame to find out the sizes (use the m4v_config)
    if ((OMX_FALSE == Mpeg4InitCompleteFlag) && (H263_MODE == CodecMode))
    {
        int32 aligned_width, aligned_height;
        int32 display_width, display_height;

        if (iGetM4VConfigInfo(*aInputBuf, *aInBufSize, &aligned_width, &aligned_height, &display_width, &display_height))
        {
            return OMX_FALSE;
        }

        Mpeg4InitCompleteFlag = OMX_TRUE;
        iDisplay_Width = display_width;
        iDisplay_Height = display_height;
        aPortParam->format.video.nFrameWidth = iDisplay_Width; // use non 16byte aligned values (display_width) for H263
        aPortParam->format.video.nFrameHeight = iDisplay_Height; // like in the case of M4V (PVGetVideoDimensions also returns display_width/height)

        OMX_U32 min_stride = ((aPortParam->format.video.nFrameWidth + 15) & (~15));
        OMX_U32 min_sliceheight = ((aPortParam->format.video.nFrameHeight + 15) & (~15));


        aPortParam->format.video.nStride = min_stride;
        aPortParam->format.video.nSliceHeight = min_sliceheight;

        // finally, compute the new minimum buffer size.

        // Decoder components always output YUV420 format
        aPortParam->nBufferSize = (aPortParam->format.video.nSliceHeight * aPortParam->format.video.nStride * 3) >> 1;

        iFrameSize = (aPortParam->format.video.nSliceHeight * aPortParam->format.video.nStride);

        // in case of h263 - port reconfig is pretty common - we'll alos have to do SetReferenceYUV later

        CheckPortReConfig(aOutBuffer, OldWidth, OldHeight, aResizeFlag, aFrameCount);
        return OMX_TRUE;
    }

    // if reference yuv has not been set yet - set it now and keep the current input buffer for later processing
    // this can happen either in the case of port reconfig or unavailability of output buffer during config buffer decoding
    if (iReferenceYUVWasSet == OMX_FALSE)
    {

        PVSetReferenceYUV(&VideoCtrl, (uint8*)(aOutBuffer->pBuffer));
        // take care of ref count for the buffer that we want to use for reference
        BufferCtrlStruct *pBCTRL = (BufferCtrlStruct *)(aOutBuffer->pOutputPortPrivate);
        pBCTRL->iRefCount++;
        ipRefCtrPreviousReferenceBuffer = &(pBCTRL->iRefCount);

        iReferenceYUVWasSet = OMX_TRUE;
        return OMX_TRUE;
    }
#if PROFILING_ON
    OMX_U32 StartTime = OsclTickCount::TickCount();
#endif

    Status = (OMX_BOOL) PVDecodeVideoFrame(&VideoCtrl, aInputBuf,
                                           &TimeStamp,
                                           (int32*)aInBufSize,
                                           &UseExtTimestamp,
                                           (OMX_U8*)(aOutBuffer->pBuffer));
#if PROFILING_ON
    OMX_U32 EndTime = OsclTickCount::TickCount();
    iTotalTicks += (EndTime - StartTime);
#endif

    if (Status == PV_TRUE)
    {
#ifdef _DEBUG
        //printf("Frame number %d\n", ++FrameCount);
#endif

        // advance input buffer ptr
        *aInputBuf += (InputSize - *aInBufSize);

        // release the previous buffer (it is no longer needed for reference) - so decrement the ref.counter
        (*ipRefCtrPreviousReferenceBuffer)--;
        // it is possible that by releasing the reference buffer - the buffer becomes available
        // this is possible if the buffer was outstanding (downstream)- but it returned to the OMX component while it was
        // still being used as reference. When we release it now - it may become available.
        if ((*ipRefCtrPreviousReferenceBuffer) == 0)
        {
            ipOMXComponent->iNumAvailableOutputBuffers++;
            // posssibly reschedule the component?
        }

        // use the current buffer as reference for the next decode
        BufferCtrlStruct *pBCTRL = (BufferCtrlStruct *)(aOutBuffer->pOutputPortPrivate);
        pBCTRL->iRefCount++;
        ipRefCtrPreviousReferenceBuffer = &(pBCTRL->iRefCount);

        *aOutputLength = (iFrameSize * 3) >> 1;

        (*aFrameCount)++;
    }
    else
    {
        *aInBufSize = InputSize;
        *aOutputLength = 0;
    }

    return Status;
}

OMX_S32 Mpeg4Decoder_OMX::InitializeVideoDecode(
    OMX_S32* aWidth, OMX_S32* aHeight, OMX_U8** aBuffer, OMX_S32* aSize, OMX_S32 mode, OMX_BOOL aDeBlocking)
{
    OMX_S32 OK = PV_TRUE;
    CodecMode = MPEG4_MODE;

    if (mode == MODE_H263)
    {
        CodecMode = H263_MODE;
    }

    OK = PVInitVideoDecoder(&VideoCtrl, aBuffer, (int32*) aSize, 1,
                            PVH263DEFAULTWIDTH, PVH263DEFAULTHEIGHT, CodecMode);

    if (OK)
    {
        PVGetVideoDimensions(&VideoCtrl, (int32*) aWidth, (int32*) aHeight);
        CodecMode = PVGetDecBitstreamMode(&VideoCtrl);

        if (CodecMode == H263_MODE && (*aWidth == 0 || *aHeight == 0))
        {
            *aWidth = PVH263DEFAULTWIDTH;
            *aHeight = PVH263DEFAULTHEIGHT;
        }

        if (OMX_TRUE == aDeBlocking)
        {
            PVSetPostProcType(&VideoCtrl, PV_DEBLOCK);
        }
        else
        {
            PVSetPostProcType(&VideoCtrl, 0);
        }

        return PV_TRUE;
    }
    else
    {
        return PV_FALSE;
    }


}

OMX_ERRORTYPE Mpeg4Decoder_OMX::Mp4DecDeinit()
{
    OMX_BOOL Status;


    // at this point - the buffers have been freed already
    ipRefCtrPreviousReferenceBuffer = NULL;
    iReferenceYUVWasSet = OMX_FALSE;


    Status = (OMX_BOOL) PVCleanUpVideoDecoder(&VideoCtrl);
    if (Status != OMX_TRUE)
    {
        return OMX_ErrorUndefined;
    }
    return OMX_ErrorNone;
}

OMX_S32 Mpeg4Decoder_OMX::GetVideoHeader(int32 aLayer, uint8* aBuf, int32 aMaxSize)
{
    OSCL_UNUSED_ARG(aLayer);

    int32 count = 0;
    char my_sc[4];

    uint8 *tmp_bs = aBuf;

    oscl_memcpy(my_sc, tmp_bs, 4);
    my_sc[3] &= 0xf0;

    if (aMaxSize >= 4)
    {
        if (oscl_memcmp(my_sc, VOSH_START_CODE1, 4) && oscl_memcmp(my_sc, VO_START_CODE1, 4))
        {
            count = 0;
            iShortVideoHeader = OMX_TRUE;
        }
        else
        {
            count = 0;
            iShortVideoHeader = FALSE;
            while (oscl_memcmp(tmp_bs + count, VOP_START_CODE1, 4))
            {
                count++;
                if (count > 1000)
                {
                    iShortVideoHeader = OMX_TRUE;
                    break;
                }
            }
            if (iShortVideoHeader == OMX_TRUE)
            {
                count = 0;
                while (oscl_memcmp(tmp_bs + count, H263_START_CODE1, 3))
                {
                    count++;
                }
            }
        }
    }
    return count;
}


// Check for resizeflag and whether Reference YUV buff needs to set.
void Mpeg4Decoder_OMX::CheckPortReConfig(OMX_BUFFERHEADERTYPE* aOutBuffer, OMX_S32 OldWidth, OMX_S32 OldHeight, OMX_BOOL *aResizeFlag, OMX_S32* aFrameCount)
{
    if ((iDisplay_Width != OldWidth) || (iDisplay_Height != OldHeight))
    {
        *aResizeFlag = OMX_TRUE;
    }
    else if (NULL != aOutBuffer)
    {
        // if there'll be no port reconfig - the current output YUV buffer is good enough
        PVSetReferenceYUV(&VideoCtrl, (uint8*)(aOutBuffer->pBuffer));
        // take care of ref count for the buffer
        BufferCtrlStruct *pBCTRL = (BufferCtrlStruct *)(aOutBuffer->pOutputPortPrivate);
        pBCTRL->iRefCount++;
        ipRefCtrPreviousReferenceBuffer = &(pBCTRL->iRefCount);

        iReferenceYUVWasSet = OMX_TRUE;
    }
    *aFrameCount = 1;
}

