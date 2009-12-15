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
#ifndef PV_FRAME_METADATA_MIO_AUDIO_H_INCLUDED
#define PV_FRAME_METADATA_MIO_AUDIO_H_INCLUDED

#ifndef PV_FRAME_METADATA_MIO_H_INCLUDED
#include "pv_frame_metadata_mio.h"
#endif

class PVLogger;
class PVMFMediaClock;
class ColorConvertBase;

// This class constitutes the Audio Media IO component
class PVFMAudioMIO
        : public PVFMMIO
{
    public:
        PVFMAudioMIO();
        ~PVFMAudioMIO();

        // From PvmiMediaTransfer
        PVMFCommandId writeAsync(uint8 format_type, int32 format_index, uint8* data, uint32 data_len,
                                 const PvmiMediaXferHeader& data_header_info, OsclAny* aContext = NULL);

        // From PvmiCapabilityAndConfig
        PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier, PvmiKvp*& aParameters,
                                     int& num_parameter_elements, PvmiCapabilityContext aContext);
        void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements, PvmiKvp*& aRet_kvp);

    private:
        void InitData();
        void ResetData();

        int32 iNumberOfBuffers;
        int32 iBufferSize;
        bool iNumberOfBuffersValid;
        bool iBufferSizeValid;

        // Audio parameters
        PVMFFormatType iAudioFormat;
        uint32 iAudioNumChannels;
        bool iAudioNumChannelsValid;
        uint32 iAudioSamplingRate;
        bool iAudioSamplingRateValid;
};

#endif // PV_FRAME_METADATA_MIO_AUDIO_H_INCLUDED

