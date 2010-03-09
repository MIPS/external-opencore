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
#include "pv_frame_metadata_mio_video.h"
#include "pvlogger.h"
#include "pv_mime_string_utils.h"
#include "oscl_snprintf.h"
#include "cczoomrotationbase.h"

PVFMVideoMIO::PVFMVideoMIO() :
        PVFMMIO()
{
    InitData();
}


void PVFMVideoMIO::InitData()
{
    iVideoFormat = PVMF_MIME_FORMAT_UNKNOWN;
    iVideoSubFormat = PVMF_MIME_FORMAT_UNKNOWN;
    iVideoHeightValid = false;
    iVideoWidthValid = false;
    iVideoDisplayHeightValid = false;
    iVideoDisplayWidthValid = false;
    iVideoHeight = 0;
    iVideoWidth = 0;
    iVideoDisplayHeight = 0;
    iVideoDisplayWidth = 0;

    iThumbnailWidth = 320;
    iThumbnailHeight = 240;


    iColorConverter = NULL;
    iCCRGBFormatType = PVMF_MIME_FORMAT_UNKNOWN;

    iFrameRetrievalInfo.iRetrievalRequested = false;
    iFrameRetrievalInfo.iGetFrameObserver = NULL;
    iFrameRetrievalInfo.iUseFrameIndex = false;
    iFrameRetrievalInfo.iUseTimeOffset = false;
    iFrameRetrievalInfo.iFrameIndex = 0;
    iFrameRetrievalInfo.iTimeOffset = 0;
    iFrameRetrievalInfo.iFrameBuffer = NULL;
    iFrameRetrievalInfo.iBufferSize = NULL;

    // Init the input format capabilities vector
    iInputFormatCapability.clear();
    iInputFormatCapability.push_back(PVMF_MIME_YUV420);
    iInputFormatCapability.push_back(PVMF_MIME_YUV422);
    iInputFormatCapability.push_back(PVMF_MIME_YUV422_INTERLEAVED_UYVY);
    iInputFormatCapability.push_back(PVMF_MIME_RGB8);
    iInputFormatCapability.push_back(PVMF_MIME_RGB12);
    iInputFormatCapability.push_back(PVMF_MIME_RGB16);
    iInputFormatCapability.push_back(PVMF_MIME_RGB24);

    iYUV422toYUV420ColorConvert = NULL;

    PVFMMIO::InitData();
}


void PVFMVideoMIO::ResetData()
{
    PVFMMIO::Cleanup();

    // Reset all the received media parameters.
    iVideoFormat = PVMF_MIME_FORMAT_UNKNOWN;
    iVideoSubFormat = PVMF_MIME_FORMAT_UNKNOWN;
    iVideoHeightValid = false;
    iVideoWidthValid = false;
    iVideoDisplayHeightValid = false;
    iVideoDisplayWidthValid = false;
    iVideoHeight = 0;
    iVideoWidth = 0;
    iVideoDisplayHeight = 0;
    iVideoDisplayWidth = 0;
}

PVFMVideoMIO::~PVFMVideoMIO()
{
    PVFMMIO::Cleanup();

    if (iColorConverter)
    {
        DestroyYUVToRGBColorConverter(iColorConverter, iCCRGBFormatType);
    }

    if (iYUV422toYUV420ColorConvert)
    {
        DestroyYUV422toYUV420ColorConvert();
    }
}

void PVFMVideoMIO::setThumbnailDimensions(uint32 aWidth, uint32 aHeight)
{
    iThumbnailWidth = aWidth;
    iThumbnailHeight = aHeight;
}

PVMFStatus PVFMVideoMIO::GetFrameByFrameNumber(uint32 aFrameIndex, uint8* aFrameBuffer, uint32& aBufferSize,
        PVMFFormatType aFormatType, PVFMVideoMIOGetFrameObserver& aObserver)
{
    if (iFrameRetrievalInfo.iRetrievalRequested)
    {
        // Get frame request is already pending so don't accept this request
        return PVMFErrBusy;
    }

    if (aFrameBuffer == NULL || aBufferSize == 0)
    {
        // Bad parameters
        return PVMFErrArgument;
    }

    // Signal for frame retrieval by frame number
    iFrameRetrievalInfo.iRetrievalRequested = true;
    iFrameRetrievalInfo.iGetFrameObserver = &aObserver;
    iFrameRetrievalInfo.iUseFrameIndex = true;
    iFrameRetrievalInfo.iUseTimeOffset = false;
    iFrameRetrievalInfo.iFrameIndex = aFrameIndex;
    iFrameRetrievalInfo.iFrameBuffer = aFrameBuffer;
    iFrameRetrievalInfo.iBufferSize = &aBufferSize;
    iFrameRetrievalInfo.iFrameFormatType = aFormatType;

    iFrameRetrievalInfo.iReceivedFrameCount = 0;
    iFrameRetrievalInfo.iStartingTSSet = false;
    iFrameRetrievalInfo.iStartingTS = 0;

    return PVMFSuccess;
}


PVMFStatus PVFMVideoMIO::GetFrameByTimeoffset(uint32 aTimeOffset, uint8* aFrameBuffer, uint32& aBufferSize,
        PVMFFormatType aFormatType, PVFMVideoMIOGetFrameObserver& aObserver)
{
    if (iFrameRetrievalInfo.iRetrievalRequested)
    {
        // Get frame request is already pending don't accept this request
        return PVMFErrBusy;
    }

    if (aFrameBuffer == NULL || aBufferSize == 0)
    {
        // Bad parameters
        return PVMFErrArgument;
    }

    // Signal for frame retrieval by time offset
    iFrameRetrievalInfo.iRetrievalRequested = true;
    iFrameRetrievalInfo.iGetFrameObserver = &aObserver;
    iFrameRetrievalInfo.iUseFrameIndex = false;
    iFrameRetrievalInfo.iUseTimeOffset = true;
    iFrameRetrievalInfo.iTimeOffset = aTimeOffset;
    iFrameRetrievalInfo.iFrameBuffer = aFrameBuffer;
    iFrameRetrievalInfo.iBufferSize = &aBufferSize;
    iFrameRetrievalInfo.iFrameFormatType = aFormatType;

    iFrameRetrievalInfo.iReceivedFrameCount = 0;
    iFrameRetrievalInfo.iStartingTSSet = false;
    iFrameRetrievalInfo.iStartingTS = 0;

    return PVMFSuccess;
}


PVMFStatus PVFMVideoMIO::CancelGetFrame(void)
{
    // Cancel any pending frame retrieval and reset variables
    iFrameRetrievalInfo.iRetrievalRequested = false;
    iFrameRetrievalInfo.iUseFrameIndex = false;
    iFrameRetrievalInfo.iUseTimeOffset = false;

    return PVMFSuccess;
}


PVMFStatus PVFMVideoMIO::GetFrameProperties(uint32& aFrameWidth, uint32& aFrameHeight, uint32& aDisplayWidth, uint32& aDisplayHeight)
{
    if (iVideoWidthValid == false || iVideoHeightValid == false ||
            iVideoDisplayWidthValid == false || iVideoDisplayHeightValid == false)
    {
        return PVMFErrNotReady;
    }

    aFrameWidth = iVideoWidth;
    aFrameHeight = iVideoHeight;
    aDisplayWidth = iVideoDisplayWidth;
    aDisplayHeight = iVideoDisplayHeight;

    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFCommandId PVFMVideoMIO::writeAsync(uint8 aFormatType, int32 aFormatIndex, uint8* aData, uint32 aDataLen,
                                       const PvmiMediaXferHeader& data_header_info, OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::writeAsync() seqnum %d ts %d context %d", data_header_info.seq_num, data_header_info.timestamp, aContext));

    PVMFStatus status = PVMFFailure;

    // Do a leave if MIO is not configured except when it is an EOS
    if (!iIsMIOConfigured &&
            !((PVMI_MEDIAXFER_FMT_TYPE_NOTIFICATION == aFormatType) &&
              (PVMI_MEDIAXFER_FMT_INDEX_END_OF_STREAM == aFormatIndex)))
    {
        iWriteBusy = true;
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    switch (aFormatType)
    {
        case PVMI_MEDIAXFER_FMT_TYPE_COMMAND :
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::writeAsync() called with Command info."));
            // Ignore
            status = PVMFSuccess;
            break;

        case PVMI_MEDIAXFER_FMT_TYPE_NOTIFICATION :
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::writeAsync() called with Notification info."));
            switch (aFormatIndex)
            {
                case PVMI_MEDIAXFER_FMT_INDEX_END_OF_STREAM:
                    // If waiting for frame then return errMaxReached
                    if (iFrameRetrievalInfo.iRetrievalRequested)
                    {
                        iFrameRetrievalInfo.iRetrievalRequested = false;
                        iFrameRetrievalInfo.iUseFrameIndex = false;
                        iFrameRetrievalInfo.iUseTimeOffset = false;
                        iFrameRetrievalInfo.iGetFrameObserver->HandleFrameReadyEvent(PVMFErrMaxReached);
                    }
                    break;

                default:
                    break;
            }
            // Ignore
            status = PVMFSuccess;
            break;

        case PVMI_MEDIAXFER_FMT_TYPE_DATA :
            switch (aFormatIndex)
            {
                case PVMI_MEDIAXFER_FMT_INDEX_FMT_SPECIFIC_INFO:
                    // Format-specific info contains codec headers.
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::writeAsync() called with format-specific info."));

                    if (iState < STATE_INITIALIZED)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::writeAsync: Error - Invalid state"));
                        iWriteBusy = true;
                        OSCL_LEAVE(OsclErrInvalidState);
                        return -1;
                    }
                    else
                    {
                        if (aDataLen > 0)
                        {
                            status = PVMFSuccess;
                        }
                        else
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMVideoMIO::writeAsync() called aDataLen==0."));
                            status = PVMFSuccess;
                        }
                    }
                    break;

                case PVMI_MEDIAXFER_FMT_INDEX_DATA:
                    // Data contains the media bitstream.

                    // Verify the state
                    if ((iState != STATE_STARTED) || (!iFrameRetrievalInfo.iRetrievalRequested))
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::writeAsync: Error - Invalid state"));
                        iWriteBusy = true;
                        OSCL_LEAVE(OsclErrInvalidState);
                        return -1;
                    }
                    else
                    {
                        if (iFrameRetrievalInfo.iUseFrameIndex)
                        {
                            ++iFrameRetrievalInfo.iReceivedFrameCount;
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::writeAsync() Received frames %d", iFrameRetrievalInfo.iReceivedFrameCount));
                        }
                        else if (iFrameRetrievalInfo.iUseTimeOffset && iFrameRetrievalInfo.iStartingTSSet == false)
                        {
                            iFrameRetrievalInfo.iStartingTSSet = true;
                            iFrameRetrievalInfo.iStartingTS = data_header_info.timestamp;
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::writeAsync() Starting timestamp set %d", iFrameRetrievalInfo.iStartingTS));
                        }

                        if (aDataLen > 0)
                        {
                            // Check if a frame retrieval was requested
                            if (iFrameRetrievalInfo.iRetrievalRequested)
                            {

                                // scale down output proportionally if smaller thumbnail requested
                                if (iVideoDisplayWidth > iThumbnailWidth || iVideoDisplayHeight > iThumbnailHeight)
                                {
                                    float fScaleWidth = (float)iThumbnailWidth / iVideoDisplayWidth;
                                    float fScaleHeight = (float)iThumbnailHeight / iVideoDisplayHeight;
                                    float fScale = (fScaleWidth > fScaleHeight) ? fScaleHeight : fScaleWidth;
                                    iVideoDisplayWidth = (uint32)(iVideoDisplayWidth * fScale);
                                    iVideoDisplayHeight = (uint32)(iVideoDisplayHeight * fScale);
                                    // It is possible that width and height becomes odd numbers after
                                    // scaling them down. These values might be used by ColorConverter
                                    // for ColorConversion which expects even parameters. Make these
                                    // parameters multiple of 2.
                                    iVideoDisplayWidth = ((iVideoDisplayWidth + 1) & (~1));
                                    iVideoDisplayHeight = ((iVideoDisplayHeight + 1) & (~1));
                                }

                                if ((iFrameRetrievalInfo.iUseFrameIndex == true &&
                                        iFrameRetrievalInfo.iReceivedFrameCount > iFrameRetrievalInfo.iFrameIndex) ||
                                        (iFrameRetrievalInfo.iUseTimeOffset == true &&
                                         iFrameRetrievalInfo.iStartingTSSet == true &&
                                         (data_header_info.timestamp - iFrameRetrievalInfo.iStartingTS) >= iFrameRetrievalInfo.iTimeOffset))
                                {
                                    PVMFStatus evstatus = PVMFFailure;
                                    // Copy the frame data
                                    evstatus = CopyVideoFrameData(aData, aDataLen, iVideoFormat,
                                                                  iFrameRetrievalInfo.iFrameBuffer, *(iFrameRetrievalInfo.iBufferSize), iFrameRetrievalInfo.iFrameFormatType,
                                                                  iVideoWidth, iVideoHeight, iVideoDisplayWidth, iVideoDisplayHeight);

                                    iFrameRetrievalInfo.iRetrievalRequested = false;
                                    iFrameRetrievalInfo.iUseFrameIndex = false;
                                    iFrameRetrievalInfo.iUseTimeOffset = false;

                                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::writeAsync() Retrieved requested frame Status %d", evstatus));
                                    iFrameRetrievalInfo.iGetFrameObserver->HandleFrameReadyEvent(evstatus);
                                }
                            }

                            status = PVMFSuccess;
                        }
                        else
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMVideoMIO::writeAsync() called aDataLen==0."));
                            status = PVMFSuccess;
                        }
                    }
                    break;

                default:
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::writeAsync: Error - unrecognized format index"));
                    status = PVMFFailure;
                    break;
            }
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::writeAsync: Error - unrecognized format type"));
            status = PVMFFailure;
            break;
    }

    //Schedule asynchronous response
    PVMFCommandId cmdid = iCommandCounter++;
    WriteResponse resp(status, cmdid, aContext, data_header_info.timestamp);
    iWriteResponseQueue.push_back(resp);
    RunIfNotReady();
    return cmdid;

}


PVMFStatus PVFMVideoMIO::CopyVideoFrameData(uint8* aSrcBuffer, uint32 aSrcSize, PVMFFormatType aSrcFormat,
        uint8* aDestBuffer, uint32& aDestSize, PVMFFormatType aDestFormat,
        uint32 aSrcWidth, uint32 aSrcHeight, uint32 aDestWidth, uint32 aDestHeight)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::CopyVideoFrameData() In"));

    if (aSrcBuffer == NULL || aSrcSize == 0 || aSrcFormat == PVMF_MIME_FORMAT_UNKNOWN ||
            aDestBuffer == NULL || aDestSize == 0 || aDestFormat == PVMF_MIME_FORMAT_UNKNOWN)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() bad input arguments."));
        return PVMFErrArgument;
    }

    if ((iVideoSubFormat == PVMF_MIME_YUV422_INTERLEAVED_UYVY) &&
            (aDestFormat == PVMF_MIME_YUV420))
    {
        // Source is YUV 4:2:2 and dest is YUV 4:2:0

        PVMFStatus status;
        uint32 yuvbufsize;
        if (!iYUV422toYUV420ColorConvert)
        {
            status = CreateYUV422toYUV420ColorConvert();
            if (status != PVMFSuccess)
            {
                // Failed to create the CC!
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() Failed to create iYUV422toYUV420ColorConvert."));
                return status;
            }
        }

        // Init CC
        status = InitYUV422toYUV420ColorConvert(aSrcWidth, aSrcHeight, aDestWidth, aDestHeight);
        if (status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() iYUV422toYUV420ColorConvert Init failed"));
            return status;
        }

        yuvbufsize = (uint32)(iYUV422toYUV420ColorConvert->GetOutputBufferSize());

        // Is the CC destination buffer smaller that the expected destination buffer size?
        if (yuvbufsize > aDestSize)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() Specified output YUV buffer does not have enough space. Needed %d Available %d.", yuvbufsize, aDestSize));
            return PVMFErrResource;
        }

        // Convert
        if (iYUV422toYUV420ColorConvert->Convert(aSrcBuffer, aDestBuffer) == 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() YUV Color conversion failed"));
            return PVMFErrResource;
        }

        // Save the YUV frame size
        aDestSize = yuvbufsize;
    }
    else if (aSrcFormat == aDestFormat)
    {
        // Same format so direct copy
        if (aDestSize < aSrcSize)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() destination and source size differ"));
            return PVMFErrArgument;
        }

        if (iVideoSubFormat == PVMF_MIME_YUV420_SEMIPLANAR_YVU)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::CopyVideoFrameData() SubFormat is YUV SemiPlanar, Convert to YUV planar first"));
            convertYUV420SPtoYUV420(aSrcBuffer, aDestBuffer, aSrcSize);
        }
        else
        {
            oscl_memcpy(aDestBuffer, aSrcBuffer, aSrcSize);
        }
        aDestSize = aSrcSize;
    }
    else if (aSrcFormat == PVMF_MIME_YUV420 &&
             (aDestFormat == PVMF_MIME_RGB12 || aDestFormat == PVMF_MIME_RGB16 || aDestFormat == PVMF_MIME_RGB24))
    {
        // Source is YUV 4:2:0 and dest is RGB 12, 16, or 24 bit

        // Validate the source and dest dimensions
        if (aSrcWidth == 0 || aSrcHeight == 0 || aDestWidth == 0 || aDestHeight == 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() Invalid frame dimensions Src(WxH): %dx%d Dest(WxH): %dx%d",
                            aSrcWidth, aSrcHeight, aDestWidth, aDestHeight));
            return PVMFErrArgument;
        }

        // Check if the proper color converter is available
        if (iColorConverter && iCCRGBFormatType != aDestFormat)
        {
            DestroyYUVToRGBColorConverter(iColorConverter, iCCRGBFormatType);
            iCCRGBFormatType = PVMF_MIME_FORMAT_UNKNOWN;
        }

        // Instantiate a new color converter if needed
        if (iColorConverter == NULL)
        {
            PVMFStatus retval = CreateYUVToRGBColorConverter(iColorConverter, aDestFormat);
            if (retval != PVMFSuccess)
            {
                // Color converter could not be instantiated
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() Appropriate YUV to RGB color converter could not be instantiated"));
                return retval;
            }
            iCCRGBFormatType = aDestFormat;
        }

        if (!(iColorConverter->Init((aSrcWidth + 1)&(~1), (aSrcHeight + 1)&(~1), (aSrcWidth + 1)&(~1), aDestWidth, (aDestHeight + 1)&(~1), (aDestWidth + 1)&(~1), CCROTATE_NONE)))
        {
            // Color converter failed Init
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() iColorConverter failed init."));
            return PVMFFailure;
        }


        iColorConverter->SetMemHeight((iVideoHeight + 1)&(~1));
        iColorConverter->SetMode(1); // Do scaling if needed.

        uint32 rgbbufsize = (uint32)(iColorConverter->GetOutputBufferSize());
        if (rgbbufsize > aDestSize)
        {
            // Specified buffer does not have enough space
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() Specified output RGB buffer does not have enough space. Needed %d Available %d", rgbbufsize, aDestSize));
            return PVMFErrResource;
        }

        // Do the color conversion
        if (iColorConverter->Convert(aSrcBuffer, aDestBuffer) == 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() Color conversion failed"));
            return PVMFErrResource;
        }
        // Save the RGB frame size
        aDestSize = rgbbufsize;
    }
    else
    {
        // Other conversions not supported yet
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() Unsupported conversion mode."));
        return PVMFErrNotSupported;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::CopyVideoFrameData() Out"));
    return PVMFSuccess;
}


PVMFStatus PVFMVideoMIO::getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier, PvmiKvp*& aParameters,
        int& num_parameter_elements, PvmiCapabilityContext aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::getParametersSync() called"));

    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);

    aParameters = NULL;
    num_parameter_elements = 0;

    if (pv_mime_strcmp(aIdentifier, MOUT_VIDEO_FORMAT_KEY) == 0)
    {
        //query for video format string
        aParameters = (PvmiKvp*)oscl_malloc(sizeof(PvmiKvp));
        aParameters->value.pChar_value = (char*)iVideoFormat.getMIMEStrPtr();
        //don't bother to set the key string.
        return PVMFSuccess;
    }

    else if (pv_mime_strcmp(aIdentifier, INPUT_FORMATS_CAP_QUERY) == 0)
    {
        // This is a query for the list of supported formats.
        // This component supports all uncompressed video format
        // Generate a list of all the PVMF video formats...
        int32 count = iInputFormatCapability.size();

        aParameters = (PvmiKvp*)oscl_malloc(count * sizeof(PvmiKvp));

        if (aParameters)
        {
            num_parameter_elements = 0;
            Oscl_Vector<PVMFFormatType, OsclMemAllocator>::iterator it;
            for (it = iInputFormatCapability.begin(); it != iInputFormatCapability.end(); it++)
            {
                aParameters[num_parameter_elements++].value.pChar_value = OSCL_STATIC_CAST(char*, it->getMIMEStrPtr());
            }
            return PVMFSuccess;
        }
        return PVMFErrNoMemory;
    }

    // Other queries are not currently supported so report as unrecognized key.
    return PVMFFailure;
}


void PVFMVideoMIO::setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements, PvmiKvp*& aRet_kvp)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::setParametersSync() called"));

    OSCL_UNUSED_ARG(aSession);

    aRet_kvp = NULL;

    for (int32 i = 0; i < num_elements; i++)
    {
        //Check against known video parameter keys...
        if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_FORMAT_KEY) == 0)
        {
            iVideoFormat = aParameters[i].value.pChar_value;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::setParametersSync() Video Format Key, Value %s", iVideoFormat.getMIMEStrPtr()));
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_WIDTH_KEY) == 0)
        {
            iVideoWidth = (int32)aParameters[i].value.uint32_value;
            iVideoWidthValid = true;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::setParametersSync() Video Width Key, Value %d", iVideoWidth));
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_HEIGHT_KEY) == 0)
        {
            iVideoHeight = (int32)aParameters[i].value.uint32_value;
            iVideoHeightValid = true;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::setParametersSync() Video Height Key, Value %d", iVideoHeight));
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_DISPLAY_HEIGHT_KEY) == 0)
        {
            iVideoDisplayHeight = (int32)aParameters[i].value.uint32_value;
            iVideoDisplayHeightValid = true;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::setParametersSync() Video Display Height Key, Value %d", iVideoDisplayHeight));
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_DISPLAY_WIDTH_KEY) == 0)
        {
            iVideoDisplayWidth = (int32)aParameters[i].value.uint32_value;
            iVideoDisplayWidthValid = true;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::setParametersSync() Video Display Width Key, Value %d", iVideoDisplayWidth));
        }
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
        {
            //  iOutputFile.Write(aParameters[i].value.pChar_value, sizeof(uint8), (int32)aParameters[i].capacity);
        }
        else if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_SUBFORMAT_KEY) == 0)
        {
            iVideoSubFormat = aParameters[i].value.pChar_value;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::setParametersSync() Video SubFormat Key, Value %s", iVideoSubFormat.getMIMEStrPtr()));
        }
        //All FSI for video will be set here in one go
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV) == 0)
        {
            PVMFYuvFormatSpecificInfo0* yuvInfo = (PVMFYuvFormatSpecificInfo0*)aParameters->value.key_specific_value;

            iVideoWidth = (int32)yuvInfo->buffer_width;
            if (iVideoWidth)
                iVideoWidthValid = true;
            else
                iVideoWidthValid = false;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::setParametersSync() Video Width, Value %d", iVideoWidth));

            iVideoHeight = (int32)yuvInfo->buffer_height;
            if (iVideoHeight)
                iVideoHeightValid = true;
            else
                iVideoHeightValid = false;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::setParametersSync() Video Height, Value %d", iVideoHeight));

            iVideoDisplayHeight = (int32)yuvInfo->viewable_height;
            if (iVideoDisplayHeight)
                iVideoDisplayHeightValid = true;
            else
                iVideoDisplayHeightValid = false;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::setParametersSync() Video Display Height, Value %d", iVideoDisplayHeight));

            iVideoDisplayWidth = (int32)yuvInfo->viewable_width;
            if (iVideoDisplayWidth)
                iVideoDisplayWidthValid = true;
            else
                iVideoDisplayWidthValid = false;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::setParametersSync() Video Display Width, Value %d", iVideoDisplayWidth));

            iVideoSubFormat = (PVMFFormatType)yuvInfo->video_format;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::setParametersSync() Video SubFormat Key, Value %s", iVideoSubFormat.getMIMEStrPtr()));

            iNumberOfBuffers = (int32)yuvInfo->num_buffers;
            iNumberOfBuffersValid = true;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::setParametersSync() Number of Buffer, Value %d", iNumberOfBuffers));

            iBufferSize = (int32)yuvInfo->buffer_size;
            iBufferSizeValid = true;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMVideoMIO::setParametersSync() Buffer Size, Value %d", iBufferSize));

        }
        else
        {
            // If we get here the key is unrecognized.
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMVideoMIO::setParametersSync() Error, unrecognized key "));

            // Set the return value to indicate the unrecognized key and return.
            aRet_kvp = &aParameters[i];
            return;
        }
    }
    if (iVideoWidthValid && iVideoHeightValid && iVideoDisplayHeightValid && iVideoDisplayHeightValid && !iIsMIOConfigured)
    {
        if (iObserver)
        {
            iIsMIOConfigured = true;
            iObserver->ReportInfoEvent(PVMFMIOConfigurationComplete);
            if (iPeer && iWriteBusy)
            {
                iWriteBusy = false;
                iPeer->statusUpdate(PVMI_MEDIAXFER_STATUS_WRITE);
            }
        }
    }
}

//
// Private section
//
static inline void* byteOffset(void* p, uint32 offset)
{
    return (void*)((uint8*)p + offset);
}

// convert a frame in YUV420 semiplanar format with VU ordering to YUV420 planar format
void PVFMVideoMIO::convertYUV420SPtoYUV420(void* src, void* dst, uint32 len)
{
    // copy the Y plane
    uint32 y_plane_size = iVideoWidth * iVideoHeight;
    memcpy(dst, src, y_plane_size + iVideoWidth);

    // re-arrange U's and V's
    uint32* p = (uint32*)byteOffset(src, y_plane_size);
    uint16* pu = (uint16*)byteOffset(dst, y_plane_size);
    uint16* pv = (uint16*)byteOffset(pu, y_plane_size / 4);

    int count = y_plane_size / 8;
    do
    {
        uint32 uvuv = *p++;
        *pu++ = (uint16)(((uvuv >> 8) & 0xff) | ((uvuv >> 16) & 0xff00));
        *pv++ = (uint16)((uvuv & 0xff) | ((uvuv >> 8) & 0xff00));
    }
    while (--count);
}

PVMFStatus PVFMVideoMIO::CreateYUV422toYUV420ColorConvert()
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, iYUV422toYUV420ColorConvert = (CCYUV422toYUV420*) CCYUV422toYUV420::New());

    OSCL_FIRST_CATCH_ANY(leavecode,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CreateYUV422toYUV420ColorConvert() YUV Color converter instantiation did a leave"));
                         return PVMFErrNoResources;
                        );
    return PVMFSuccess;
}

PVMFStatus PVFMVideoMIO::InitYUV422toYUV420ColorConvert(uint32 aSrcWidth, uint32 aSrcHeight, uint32 aDestWidth, uint32 aDestHeight)
{
    if (!(iYUV422toYUV420ColorConvert->Init((aSrcWidth + 1)&(~1), (aSrcHeight + 1)&(~1),
                                            (aSrcWidth + 1)&(~1), (aDestWidth + 1)&(~1), (aDestHeight + 1)&(~1),
                                            (aDestWidth + 1)&(~1), CCROTATE_NONE)))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVFMVideoMIO::CopyVideoFrameData() YUV Color converter Init failed"));
        DestroyYUV422toYUV420ColorConvert();
        return PVMFFailure;
    }
    return PVMFSuccess;
}

void PVFMVideoMIO::DestroyYUV422toYUV420ColorConvert()
{
    OSCL_DELETE(iYUV422toYUV420ColorConvert);
    iYUV422toYUV420ColorConvert = NULL;
}

