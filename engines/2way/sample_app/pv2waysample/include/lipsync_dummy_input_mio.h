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
#ifndef LIPSYNC_DUMMY_INPUT_MIO_H_INCLUDED
#define LIPSYNC_DUMMY_INPUT_MIO_H_INCLUDED


#ifndef DUMMY_INPUT_MIO_H_INCLUDED
#include "dummy_input_mio.h"
#endif

#ifndef PVMI_MEDIA_TRANSFER_H_INCLUDED
#include "pvmi_media_transfer.h"
#endif

#ifndef PVMI_MIO_CONTROL_H_INCLUDED
#include "pvmi_mio_control.h"
#endif

#ifndef LIPSYNC_SINGLETON_OBJECT_H_INCLUDED
#include "lipsync_singleton_object.h"
#endif

#ifndef PVMI_MEDIA_IO_OBSERVER_H_INCLUDED
#include "pvmi_media_io_observer.h"
#endif

#ifndef PVMI_CONFIG_AND_CAPABILITY_H_INCLUDED
#include "pvmi_config_and_capability.h"
#endif

#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif

#ifndef OSCL_STRING_CONTAINRES_H_INCLUDED
#include "oscl_string_containers.h"
#endif

#ifndef OSCL_MEM_MEMPOOL_H_INCLUDED
#include "oscl_mem_mempool.h"
#endif

#ifndef PVMI_MEDIA_IO_CLOCK_EXTENSION_H_INCLUDED
#include "pvmi_media_io_clock_extension.h"
#endif






//This class implements the Dummy input MIO component
class LipSyncDummyInputMIO : public DummyInputMIO
{
    public:

        LipSyncDummyInputMIO(const DummyMIOSettings& aSettings);
        ~LipSyncDummyInputMIO();

        void ThreadLogon();
    protected:
        void AdditionalGenerateAudioFrameStep(PVMFFormatType aFormat = PVMF_MIME_FORMAT_UNKNOWN);
        void AdditionalGenerateVideoFrameStep(PVMFFormatType aFormat);

    private:
        PVMFStatus DoRead();
        PVMFStatus DoStop();
        void CalculateRMSInfo(uint32 VideoData, uint32 AudioData);

        ShareParams* iParams;

        uint32 iCount;
        int32  iDiffVidAudTS;
        int32  iSqrVidAudTS;
        int32  iRtMnSq;
        uint32 iAudioTimeStamp;
        uint32 iVideoTimeStamp;
};
#endif
