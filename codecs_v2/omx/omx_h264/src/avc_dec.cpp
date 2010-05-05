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
#include "oscl_types.h"
#include "avc_dec.h"
#include "avcdec_int.h"
#include "omx_avc_component.h"

#if PROFILING_ON
#include "oscl_tickcount.h"
#endif

/*************************************/
/* functions needed for video engine */
/*************************************/

/* These two functions are for callback functions of AvcHandle */
int CBAVC_Malloc_OMX(void* aUserData, int32 aSize, int aAttribute)
{
    OSCL_UNUSED_ARG(aUserData);
    OSCL_UNUSED_ARG(aAttribute);
    void* pPtr;

    pPtr = oscl_malloc(aSize);
    return (int32) pPtr;
}

void CBAVC_Free_OMX(void* aUserData, int aMem)
{
    OSCL_UNUSED_ARG(aUserData);
    oscl_free((uint8*) aMem);
}


AVCDec_Status CBAVCDec_GetData_OMX(void* aUserData, uint8** aBuffer, uint* aSize)
{
    OSCL_UNUSED_ARG(aUserData);
    OSCL_UNUSED_ARG(aBuffer);
    OSCL_UNUSED_ARG(aSize);
    return AVCDEC_FAIL;  /* nothing for now */
}

void UnbindBuffer_OMX(void* aUserData, int i)
{

    AvcDecoder_OMX* pAvcDecoder_OMX = (AvcDecoder_OMX*)aUserData;

    if (NULL == pAvcDecoder_OMX)
    {
        return;
    }

    // check first if RefBuffer ptr exists
    if (pAvcDecoder_OMX->ReferenceBufferHdrPtr[i] != NULL)
    {
        BufferCtrlStruct *pBCTRL = (BufferCtrlStruct *)(pAvcDecoder_OMX->ReferenceBufferHdrPtr[i]->pOutputPortPrivate);
        // decrement the ref counter for the buffer
        pBCTRL->iRefCount--;

        if (0 == pBCTRL->iRefCount)
        {
            pAvcDecoder_OMX->ipOMXComponent->iNumAvailableOutputBuffers++;
            // it is possible that by releasing the reference buffer - the buffer becomes available
            // this is possible if the buffer was outstanding (downstream)- but it returned to the OMX component while it was
            // still being used as reference. When we release it now - it may become available.

            // if the ref count is 0 - the buffer is in the
            // omx component for sure - there are 2 possibilities:
            // a) this buffer may be in the output buffer queue in the component
            //    - in this case no need to do anything else
            // b) or it may have been dequeued earlier and never
            //     left the component (i.e. it has been in the decoder all the time)
            //     - if this is the case - we need to queue this buffer into the
            //       output buffer queue
            if (OMX_FALSE == pBCTRL->iIsBufferInComponentQueue)
            {
                QueueType* pOutputQueue = pAvcDecoder_OMX->ipOMXComponent->GetOutputQueue(); //ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->pBufferQueue;
                Queue(pOutputQueue, (void*)(pAvcDecoder_OMX->ReferenceBufferHdrPtr[i]));
                pBCTRL->iIsBufferInComponentQueue = OMX_TRUE; // just in case
            }


            // posssibly reschedule the component?

        }

        pAvcDecoder_OMX->ReferenceBufferHdrPtr[i] = NULL;
    }

    return;
}

int AvcDecoder_OMX::AllocateBuffer_OMX(void* aUserData, int i, uint8** aYuvBuffer)
{
    AvcDecoder_OMX* pAvcDecoder_OMX = (AvcDecoder_OMX*)aUserData;

    if (NULL == pAvcDecoder_OMX)
    {
        return 0;
    }

    //We want to go for dynamic port reconfiguration, return failure from here
    if (OMX_TRUE == pAvcDecoder_OMX->iNewSPSActivation)
    {
        return 0;
    }

    //*aYuvBuffer = pAvcDecoder_OMX->pDpbBuffer + i * pAvcDecoder_OMX->FrameSize;
    //Store the input timestamp at and reference to the correct index

    if (pAvcDecoder_OMX->ReferenceBufferHdrPtr[i] != NULL)
    {
        // if need be - i.e. if the buffer is still used -unbind it (this will take care of ref count as well)
        UnbindBuffer_OMX(aUserData, i);
    }

    // store the current output buffer to the i-th position
    pAvcDecoder_OMX->ReferenceBufferHdrPtr[i] = pAvcDecoder_OMX->pCurrentBufferHdr;

    BufferCtrlStruct *pBCTRL = (BufferCtrlStruct *)(pAvcDecoder_OMX->ReferenceBufferHdrPtr[i]->pOutputPortPrivate);
    // increment the ref counter for the buffer
    pBCTRL->iRefCount++;

    //store the timestamp
    pAvcDecoder_OMX->DisplayTimestampArray[i] = pAvcDecoder_OMX->CurrInputTimestamp;

    // make sure to invalidate the "empty" output buffer in the component, i.e. make it appear consumed
    // this will set ipOutputBuffer ptr to NULL, and set iNewOutBufRequired flag to TRUE
    pAvcDecoder_OMX->ipOMXComponent->InvalidateOutputBuffer();


    // get the yuv data ptr
    *aYuvBuffer = pAvcDecoder_OMX->ReferenceBufferHdrPtr[i]->pBuffer;

    return 1;
}




int AvcDecoder_OMX::ActivateSPS_OMX(void* aUserData, uint aSizeInMbs, uint aNumBuffers)
{
    AvcDecoder_OMX* pAvcDecoder_OMX = (AvcDecoder_OMX*)aUserData;

    if (NULL == pAvcDecoder_OMX)
    {
        return 0;
    }

    pAvcDecoder_OMX->ReleaseReferenceBuffers();

    PVAVCDecGetSeqInfo(&(pAvcDecoder_OMX->AvcHandle), &(pAvcDecoder_OMX->SeqInfo));

    pAvcDecoder_OMX->FrameSize = (aSizeInMbs << 7) * 3;
    pAvcDecoder_OMX->MaxNumFs = aNumBuffers;

    pAvcDecoder_OMX->iReconfigWidth = pAvcDecoder_OMX->SeqInfo.frame_crop_right - pAvcDecoder_OMX->SeqInfo.frame_crop_left + 1;
    pAvcDecoder_OMX->iReconfigHeight = pAvcDecoder_OMX->SeqInfo.frame_crop_bottom - pAvcDecoder_OMX->SeqInfo.frame_crop_top + 1;

    pAvcDecoder_OMX->iReconfigStride = pAvcDecoder_OMX->SeqInfo.FrameWidth;
    pAvcDecoder_OMX->iReconfigSliceHeight = pAvcDecoder_OMX->SeqInfo.FrameHeight;

    if (pAvcDecoder_OMX->MaxWidth < pAvcDecoder_OMX->SeqInfo.FrameWidth)
    {
        pAvcDecoder_OMX->MaxWidth = pAvcDecoder_OMX->SeqInfo.FrameWidth;
    }
    if (pAvcDecoder_OMX->MaxHeight < pAvcDecoder_OMX->SeqInfo.FrameHeight)
    {
        pAvcDecoder_OMX->MaxHeight = pAvcDecoder_OMX->SeqInfo.FrameHeight;
    }

    pAvcDecoder_OMX->iNewSPSActivation = OMX_TRUE;

    return 1;
}

/* initialize video decoder */
OMX_BOOL AvcDecoder_OMX::InitializeVideoDecode_OMX()
{
    /* Initialize AvcHandle */
    AvcHandle.AVCObject = NULL;
    AvcHandle.userData = (void*)this;
    AvcHandle.CBAVC_DPBAlloc = ActivateSPS_OMX;
    AvcHandle.CBAVC_FrameBind = AllocateBuffer_OMX;
    AvcHandle.CBAVC_FrameUnbind = UnbindBuffer_OMX;
    AvcHandle.CBAVC_Malloc = CBAVC_Malloc_OMX;
    AvcHandle.CBAVC_Free = CBAVC_Free_OMX;

    MaxWidth = 0;
    MaxHeight = 0;
    MaxNumFs = 0;

    return OMX_TRUE;
}

OMX_BOOL AvcDecoder_OMX::FlushOutput_OMX(OMX_BUFFERHEADERTYPE **aOutBufferForRendering)
{
    AVCFrameIO Output;
    AVCDec_Status Status;
    int Index, Release;

    Output.YCbCr[0] = Output.YCbCr[1] = Output.YCbCr[2] = NULL;
    Status = PVAVCDecGetOutput(&(AvcHandle), &Index, &Release, &Output);

    *aOutBufferForRendering = NULL; // init to NULL (no output)
    if (Status == AVCDEC_FAIL)
    {
        return OMX_FALSE;
    }

    // set the buffer hdr ptr based on the returned index
    *aOutBufferForRendering = ReferenceBufferHdrPtr[Index];

    if (*aOutBufferForRendering == NULL)
    {
        return OMX_FALSE;
    }

    // set the timestamp and output size
    (*aOutBufferForRendering)->nTimeStamp = DisplayTimestampArray[Index];
    (*aOutBufferForRendering)->nFilledLen = 0; // init to 0
    (*aOutBufferForRendering)->nOffset = 0; // set to 0, not needed

    if (Output.YCbCr[0])
    {
        (*aOutBufferForRendering)->nFilledLen = (Output.pitch * Output.height * 3) >> 1;

        // else, the frame length will be reported as zero
    }

    // this buffer is no longer needed as reference by the decoder and we can do unbind
    if (Release)
    {
        // the problem is that we want to unbind the buffer,
        // but this buffer is heading straight out downstream for rendering.
        // we also need to update num of available buffers (to compensate for the update that will
        // happen when we return the buffer to OMX client. But we don't want
        // to call UnbindBuffer_OMX because it may return the buffer to the component queue


        BufferCtrlStruct *pBCTRL = (BufferCtrlStruct *)(ReferenceBufferHdrPtr[Index]->pOutputPortPrivate);
        // decrement the ref counter for the buffer
        pBCTRL->iRefCount--;

        if (0 == pBCTRL->iRefCount)
        {
            // update the count - since it will be decremented when the buffer leaves the component
            ipOMXComponent->iNumAvailableOutputBuffers++;
        }

        ReferenceBufferHdrPtr[Index] = NULL;
    }
    return OMX_TRUE;
}


/* Initialization routine */
OMX_ERRORTYPE AvcDecoder_OMX::AvcDecInit_OMX()
{
    if (OMX_FALSE == InitializeVideoDecode_OMX())
    {
        return OMX_ErrorInsufficientResources;
    }

    //Set up the cleanup object in order to do clean up work automatically
    pCleanObject = OSCL_NEW(AVCCleanupObject_OMX, (&AvcHandle));

    iAvcActiveFlag = OMX_FALSE;
    iSkipToIDR = OMX_FALSE;
    iNewSPSActivation = OMX_FALSE;

    iReconfigWidth = 0;
    iReconfigHeight = 0;
    iReconfigStride = 0;
    iReconfigSliceHeight = 0;

    return OMX_ErrorNone;
}


/*Decode routine */
OMX_BOOL AvcDecoder_OMX::AvcDecodeVideo_OMX(OMX_BUFFERHEADERTYPE *aOutBuffer, // empty output buffer that the component provides to the decoder
        OMX_BUFFERHEADERTYPE **aOutBufferForRendering, // output buffer for rendering. Initially NULL, but if there is output to be flushed out
        // this ptr will be updated to point to buffer that needs to be rendered
        OMX_U8** aInputBuf, OMX_U32* aInBufSize,
        OMX_PARAM_PORTDEFINITIONTYPE* aPortParam,
        OMX_S32* iFrameCount, OMX_BOOL aMarkerFlag, OMX_BOOL *aResizeFlag)
{
    AVCDec_Status Status;
    uint8* pNalBuffer;
    int NalSize, NalType, NalRefId;

    *aResizeFlag = OMX_FALSE;
    OMX_S32 OldStride;
    OMX_U32 OldSliceHeight;

    OldStride =  aPortParam->format.video.nStride; // actual buffer width
    OldSliceHeight = aPortParam->format.video.nSliceHeight; //actual buffer height

    *aOutBufferForRendering = NULL; // init this to NULL. If there is output to be flushed out - we'll update this value
    pCurrentBufferHdr = aOutBuffer; // save the ptr to the empty output buffer we received from component

    if (!aMarkerFlag)
    {
        // if no more NALs - try to flush the output
        if (AVCDEC_FAIL == GetNextFullNAL_OMX(&pNalBuffer, &NalSize, *aInputBuf, aInBufSize))
        {
            Status = (AVCDec_Status) FlushOutput_OMX(aOutBufferForRendering);

            if (AVCDEC_FAIL != Status)
            {
                return OMX_TRUE;
            }
            else
            {
                return OMX_FALSE;
            }
        }
    }
    else
    {
        pNalBuffer = *aInputBuf;
        NalSize = *aInBufSize;
        //Assuming that the buffer with marker bit contains one full NAL
        *aInBufSize = 0;
    }

    if (AVCDEC_FAIL == PVAVCDecGetNALType(pNalBuffer, NalSize, &NalType, &NalRefId))
    {
        return OMX_FALSE;
    }

    if (AVC_NALTYPE_SPS == (AVCNalUnitType)NalType)
    {
#if PROFILING_ON
        OMX_U32 StartTime = OsclTickCount::TickCount();
#endif

        Status = PVAVCDecSeqParamSet(&(AvcHandle), pNalBuffer, NalSize);

#if PROFILING_ON
        OMX_U32 EndTime = OsclTickCount::TickCount();
        iTotalTicks += (EndTime - StartTime);
#endif

        if (Status != AVCDEC_SUCCESS)
        {
            return OMX_FALSE;
        }

        (*iFrameCount)++;
    }

    else if (AVC_NALTYPE_PPS == (AVCNalUnitType) NalType)
    {
#if PROFILING_ON
        OMX_U32 StartTime = OsclTickCount::TickCount();
#endif

        Status = PVAVCDecPicParamSet(&(AvcHandle), pNalBuffer, NalSize);

#if PROFILING_ON
        OMX_U32 EndTime = OsclTickCount::TickCount();
        iTotalTicks += (EndTime - StartTime);
#endif

        if (Status != AVCDEC_SUCCESS)
        {
            return OMX_FALSE;
        }
    }

    else if (AVC_NALTYPE_SLICE == (AVCNalUnitType) NalType ||
             AVC_NALTYPE_IDR == (AVCNalUnitType) NalType)
    {
        if (!iAvcActiveFlag)
            iAvcActiveFlag = OMX_TRUE;

        if (iSkipToIDR == OMX_TRUE)
        {
            if (AVC_NALTYPE_IDR == (AVCNalUnitType) NalType)
            {
                iSkipToIDR = OMX_FALSE;
            }
            else
            {
                return OMX_FALSE;
            }
        }

#if PROFILING_ON
        OMX_U32 StartTime = OsclTickCount::TickCount();
#endif

        Status = PVAVCDecodeSlice(&(AvcHandle), pNalBuffer, NalSize);

#if PROFILING_ON
        OMX_U32 EndTime = OsclTickCount::TickCount();
        iTotalTicks += (EndTime - StartTime);
#endif

        if (Status == AVCDEC_PICTURE_OUTPUT_READY)
        {
            FlushOutput_OMX(aOutBufferForRendering);

            //Input buffer not consumed yet, do not mark it free.
            if (aMarkerFlag)
            {
                *aInBufSize = NalSize;
            }
            else
            {
                *aInBufSize += InputBytesConsumed;
                aInputBuf -= InputBytesConsumed;
            }
        }

        if (Status == AVCDEC_PICTURE_READY)
        {
            (*iFrameCount)++;
        }



        if (AVCDEC_NOT_SUPPORTED == Status)
        {
            return OMX_TRUE;
        }
        else if ((AVCDEC_NO_DATA == Status) || (AVCDEC_FAIL == Status) ||
                 (AVCDEC_NO_BUFFER == Status) || (AVCDEC_MEMORY_FAIL == Status))
        {
            if (AVCDEC_FAIL == Status)
            {
                iSkipToIDR = OMX_TRUE;
            }
            else if ((AVCDEC_NO_BUFFER == Status) && (OMX_TRUE == iNewSPSActivation))
            {
                // SPS NAL has been activated, set the new output port parameters and
                // signal the port reconfiguration if required

                aPortParam->format.video.nFrameWidth = iReconfigWidth;
                aPortParam->format.video.nFrameHeight = iReconfigHeight;
                aPortParam->format.video.nStride = iReconfigStride;
                aPortParam->format.video.nSliceHeight = iReconfigSliceHeight;

                // finally, compute the new minimum buffer size.
                // Decoder components always output YUV420 format
                aPortParam->nBufferSize = (aPortParam->format.video.nSliceHeight * aPortParam->format.video.nStride * 3) >> 1;


                if ((OldStride != aPortParam->format.video.nStride) || (OldSliceHeight !=  aPortParam->format.video.nSliceHeight))
                {
                    *aResizeFlag = OMX_TRUE;
                }


                // make sure that the number of buffers that the decoder requires is less than what we allocated
                // we should have at least 2 more than the decoder (one for omx component and one for downstream)
                if (MaxNumFs + 2 > aPortParam->nBufferCountActual)
                {
                    // even if buffers are the correct size - we must do port reconfig because of buffer count
                    *aResizeFlag = OMX_TRUE;
                    // adjust only buffer count min - nBufferCountActual still needs to be preserved until port becomes disabled
                    aPortParam->nBufferCountMin = MaxNumFs + 2;
                }

                // Do not mark the current NAL as consumed. We will decode it once again
                if (aMarkerFlag)
                {
                    *aInBufSize = NalSize;
                }
                else
                {
                    *aInBufSize += InputBytesConsumed;
                    aInputBuf -= InputBytesConsumed;
                }

                iNewSPSActivation = OMX_FALSE;
                return OMX_TRUE;
            }

            return OMX_FALSE;
        }
    }

    else if ((AVCNalUnitType)NalType == AVC_NALTYPE_SEI)
    {
        if (PVAVCDecSEI(&(AvcHandle), pNalBuffer, NalSize) != AVCDEC_SUCCESS)
        {
            return OMX_FALSE;
        }
    }

    else if ((AVCNalUnitType)NalType == AVC_NALTYPE_AUD)
    {
        //PicType = pNalBuffer[1] >> 5;
    }

    else if ((AVCNalUnitType)NalType == AVC_NALTYPE_EOSTREAM || // end of stream
             (AVCNalUnitType)NalType == AVC_NALTYPE_EOSEQ || // end of sequence
             (AVCNalUnitType)NalType == AVC_NALTYPE_FILL) // filler data
    {
        return OMX_TRUE;
    }

    //else
    //{
    //printf("\nNAL_type = %d, unsupported nal type or not sure what to do for this type\n", NalType);
    //  return OMX_FALSE;
    //}
    return OMX_TRUE;

}


OMX_ERRORTYPE AvcDecoder_OMX::AvcDecDeinit_OMX()
{
    if (pCleanObject)
    {
        OSCL_DELETE(pCleanObject);
        pCleanObject = NULL;
    }


    return OMX_ErrorNone;
}


AVCDec_Status AvcDecoder_OMX::GetNextFullNAL_OMX(uint8** aNalBuffer, int* aNalSize, OMX_U8* aInputBuf, OMX_U32* aInBufSize)
{
    uint8* pBuff = aInputBuf;
    OMX_U32 InputSize;

    *aNalSize = *aInBufSize;
    InputSize = *aInBufSize;

    AVCDec_Status ret_val = PVAVCAnnexBGetNALUnit(pBuff, aNalBuffer, aNalSize);

    if (ret_val == AVCDEC_FAIL)
    {
        return AVCDEC_FAIL;
    }

    InputBytesConsumed = ((*aNalSize) + (int32)(*aNalBuffer - pBuff));
    aInputBuf += InputBytesConsumed;
    *aInBufSize = InputSize - InputBytesConsumed;

    return AVCDEC_SUCCESS;
}

AVCCleanupObject_OMX::~AVCCleanupObject_OMX()
{
    PVAVCCleanUpDecoder(ipavcHandle);

}

void AvcDecoder_OMX::ResetDecoder()
{
    PVAVCDecReset(&(AvcHandle));
    ReleaseReferenceBuffers();

}

void AvcDecoder_OMX::ReleaseReferenceBuffers()
{
    // we need to unbind all reference buffers as well
    uint32 i = 0;
    for (i = 0; i < MaxNumFs; i++)
    {
        UnbindBuffer_OMX((void*) this, (int32) i);
    }
}

void AvcDecoder_OMX::CalculateBufferParameters(OMX_PARAM_PORTDEFINITIONTYPE* aPortParam)
{
    // If decoder has decoded the parameters, retain the updated values from the decoder
    // and do not recalculate
    if (OMX_FALSE == iAvcActiveFlag)
    {
        OMX_VIDEO_PORTDEFINITIONTYPE *pVideoformat = &(aPortParam->format.video);

        // check if stride needs to be adjusted - stride should be at least the 16 byte aligned width
        OMX_U32 MinStride = ((pVideoformat->nFrameWidth + 15) & (~15));
        OMX_U32 MinSliceHeight = ((pVideoformat->nFrameHeight + 15) & (~15));

        pVideoformat->nStride = MinStride;
        pVideoformat->nSliceHeight = MinSliceHeight;

        // finally, compute the new minimum buffer size
        aPortParam->nBufferSize = (pVideoformat->nSliceHeight * pVideoformat->nStride * 3) >> 1;
    }

    return ;
}

