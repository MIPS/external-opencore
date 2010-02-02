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
#ifndef PV_FRAME_METADATA_MIO_VIDEO_H_INCLUDED
#define PV_FRAME_METADATA_MIO_VIDEO_H_INCLUDED

#ifndef CCYUV422TOYUV420_H_INCLUDED
#include "ccyuv422toyuv420.h"
#endif
#ifndef PV_FRAME_METADATA_MIO_H_INCLUDED
#include "pv_frame_metadata_mio.h"
#endif

class PVLogger;
class PVMFMediaClock;
class ColorConvertBase;

class PVFMVideoMIOGetFrameObserver
{
    public:
        virtual ~PVFMVideoMIOGetFrameObserver() {};
        virtual void HandleFrameReadyEvent(PVMFStatus aEventStatus) = 0;
};

// This class implements the media IO component for extracting video frames
// for pvFrameAndMetadata utility.
// This class constitutes the Media IO component

class PVFMVideoMIO
        : public PVFMMIO
{
    public:
        PVFMVideoMIO();
        ~PVFMVideoMIO();

        // Frame retrieval APIs
        PVMFStatus GetFrameByFrameNumber(uint32 aFrameIndex, uint8* aFrameBuffer, uint32& aBufferSize, PVMFFormatType aFormaType, PVFMVideoMIOGetFrameObserver& aObserver);
        PVMFStatus GetFrameByTimeoffset(uint32 aTimeOffset, uint8* aFrameBuffer, uint32& aBufferSize, PVMFFormatType aFormatType, PVFMVideoMIOGetFrameObserver& aObserver);
        PVMFStatus CancelGetFrame(void);
        PVMFStatus GetFrameProperties(uint32& aFrameWidth, uint32& aFrameHeight, uint32& aDisplayWidth, uint32& aDisplayHeight);

        // From PvmiCapabilityAndConfig
        PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier, PvmiKvp*& aParameters,
                                     int& num_parameter_elements, PvmiCapabilityContext aContext);
        void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements, PvmiKvp*& aRet_kvp);

        // From PvmiMediaTransfer
        PVMFCommandId writeAsync(uint8 format_type, int32 format_index, uint8* data, uint32 data_len,
                                 const PvmiMediaXferHeader& data_header_info, OsclAny* aContext = NULL);
        void setThumbnailDimensions(uint32 aWidth, uint32 aHeight);
    private:
        void InitData();
        void ResetData();

        // Copy video frame data to provided including YUV to RGB conversion if necessary
        PVMFStatus CopyVideoFrameData(uint8* aSrcBuffer, uint32 aSrcSize, PVMFFormatType aSrcFormat,
                                      uint8* aDestBuffer, uint32& aDestSize, PVMFFormatType aDestFormat,
                                      uint32 iSrcWidth, uint32 iSrcHeight, uint32 iDestWidth, uint32 iDestHeight);
        PVMFStatus CreateYUVToRGBColorConverter(ColorConvertBase*& aCC, PVMFFormatType aRGBFormatType);
        PVMFStatus DestroyYUVToRGBColorConverter(ColorConvertBase*& aCC, PVMFFormatType aRGBFormatType);
        void convertYUV420SPtoYUV420(void* src, void* dst, uint32 len);

        PVMFStatus CreateYUV422toYUV420ColorConvert();
        PVMFStatus InitYUV422toYUV420ColorConvert(uint32 aSrcWidth, uint32 aSrcHeight,
                uint32 aDestWidth, uint32 aDestHeight);
        void DestroyYUV422toYUV420ColorConvert();

        // Video parameters
        PVMFFormatType iVideoFormat;
        PVMFFormatType iVideoSubFormat;
        uint32 iVideoHeight;
        bool iVideoHeightValid;
        uint32 iVideoWidth;
        bool iVideoWidthValid;
        uint32 iVideoDisplayHeight;
        bool iVideoDisplayHeightValid;
        uint32 iVideoDisplayWidth;
        bool iVideoDisplayWidthValid;

        int32 iNumberOfBuffers;
        int32 iBufferSize;
        bool iNumberOfBuffersValid;
        bool iBufferSizeValid;

        uint32 iThumbnailWidth;
        uint32 iThumbnailHeight;

        // Color converter if YUV to RGB is needed
        ColorConvertBase* iColorConverter;
        PVMFFormatType iCCRGBFormatType;

        // For frame retrieval feature
        struct PVFMVideoMIOFrameRetrieval
        {
            bool iRetrievalRequested;
            PVFMVideoMIOGetFrameObserver* iGetFrameObserver;
            bool iUseFrameIndex;
            bool iUseTimeOffset;
            uint32 iFrameIndex;
            uint32 iReceivedFrameCount;
            uint32 iTimeOffset;
            bool iStartingTSSet;
            uint32 iStartingTS;
            PVMFFormatType iFrameFormatType;
            uint8* iFrameBuffer;
            uint32* iBufferSize;
            uint32 iFrameWidth;
            uint32 iFrameHeight;
        };
        PVFMVideoMIOFrameRetrieval iFrameRetrievalInfo;
        CCYUV422toYUV420 *iYUV422toYUV420ColorConvert;
};

#endif // PV_FRAME_METADATA_MIO_VIDEO_H_INCLUDED

