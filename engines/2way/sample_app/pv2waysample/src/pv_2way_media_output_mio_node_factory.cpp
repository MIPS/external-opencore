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

#include "pv_2way_media_output_mio_node_factory.h"
#include "pv_2way_interface.h"
#include "pv_2way_media.h"
#include "pvmi_media_io_fileoutput.h"
#include "pv_media_output_node_factory.h"


int PV2WayMediaOutputMIONodeFactory::CreateMedia(PvmiMIOFileInputSettings& aFileSettings)
{
    PVMFFormatType pvmf_mediatype = aFileSettings.iMediaFormat;
    MediaType mio_media_type = MEDIATYPE_UNKNOWN;
    bool compressed = false;
    if (pvmf_mediatype.isAudio())
    {
        mio_media_type = MEDIATYPE_AUDIO;
        if (iAudioRenderingDevice)
        {
        }
    }
    else if (pvmf_mediatype.isVideo())
    {
        mio_media_type = MEDIATYPE_VIDEO;
    }
    else
    {
        return PVMFErrNotSupported;
    }
    if (pvmf_mediatype.isCompressed())
    {
        compressed = true;
    }

    PVRefFileOutput* apRefFileOutput = OSCL_NEW(PVRefFileOutput,
                                       (aFileSettings.iFileName.get_cstr(), mio_media_type, compressed, aFileSettings.iTestBufferAlloc));
    apRefFileOutput->setUserClockExtnInterface(compressed);
    iMediaControl =  OSCL_REINTERPRET_CAST(PvmiMIOControl*, apRefFileOutput);
    return PVMFSuccess;
}

void PV2WayMediaOutputMIONodeFactory::DeleteMedia()
{

    if (iMediaControl)
    {
        if (iAudioRenderingDevice)
        {
        }
        else
            OSCL_DELETE(iMediaControl);

        iMediaControl = NULL;
    }
}

OSCL_EXPORT_REF void PV2WayMediaOutputMIONodeFactory::Delete(PVMFNodeInterface** aMioNode)
{
    PVMediaOutputNodeFactory::DeleteMediaOutputNode(*aMioNode);
    *aMioNode = NULL;
    DeleteMedia();
}

OSCL_EXPORT_REF PVMFNodeInterface* PV2WayMediaOutputMIONodeFactory::Create(PvmiMIOFileInputSettings& aFileSettings, bool aRenderingDevice)
{
    iAudioRenderingDevice = aRenderingDevice;
    PVMFNodeInterface* mioNode = NULL;
    if (PVMFSuccess == CreateMedia(aFileSettings))
    {
        int error = 0;
        OSCL_TRY(error, mioNode = PVMediaOutputNodeFactory::CreateMediaOutputNode(iMediaControl));
    }

    return mioNode;
}

void PV2WayMediaOutputMIONodeFactory::Release()
{
    delete this;
}

