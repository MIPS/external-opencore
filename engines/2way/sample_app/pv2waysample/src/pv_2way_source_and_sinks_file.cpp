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
#include "pv_2way_source_and_sinks_file.h"
#include "pv_2way_sink.h"
#include "pv_2way_source.h"
#include "pv_media_output_node_factory.h"
#include "pvmf_media_input_node_factory.h"

OSCL_EXPORT_REF PV2WaySourceAndSinksFile::PV2WaySourceAndSinksFile(PV2Way324InitInfo& aSdkInitInfo) :
        PV2WaySourceAndSinksBase(aSdkInitInfo),
        iAudioRenderingDevice(false)
{
}

OSCL_EXPORT_REF PV2WaySourceAndSinksFile::~PV2WaySourceAndSinksFile()
{
    Cleanup();
}

OSCL_EXPORT_REF int PV2WaySourceAndSinksFile::AddPreferredCodec(TPVDirection aDir,
        PV2WayMediaType aMediaType,
        PvmiMIOFileInputSettings& aFileSettings)
{
    PV2WayMIO* mio = GetMIO(aDir, aMediaType);
    if (mio)

    {
        mio->AddCodec(aFileSettings);
        return 0;
    }
    OutputInfo(PVLOGMSG_ERR, "PV2WaySourceAndSinksBase::AddPreferredCodec: Error!  No MIO of given dir, type");
    return -1;
}


OSCL_EXPORT_REF PVMFNodeInterface* PV2WaySourceAndSinksFile::CreateMIONode(CodecSpecifier* aformat,
        TPVDirection adir)
{
    PVMFNodeInterface* mioNode = NULL;
    PVMFFormatType format = aformat->GetFormat();
    if (aformat->GetType() == EPVMIOFileInput)
    {
        MIOFileCodecSpecifier* temp = OSCL_REINTERPRET_CAST(MIOFileCodecSpecifier*, aformat);
        PvmiMIOFileInputSettings fileSettings = temp->GetSpecifierType();
        /*!

          Step 10: Create MIO Node
          Create appropriate MIO Node (audio/video, incoming/outgoing)
          Note: here were are creating file-based MIO nodes
        */
        if (adir == INCOMING)
        {
            if (format.isAudio())
            {
                //if we've added a audio rendering device then we should notify the factory
                mioNode = iMioAudioOutputFactory.Create(fileSettings, iAudioRenderingDevice);
            }
            else if (format.isVideo())
            {
                mioNode = iMioVideoOutputFactory.Create(fileSettings);
            }
        }
        else if (adir == OUTGOING)
        {
            if (format.isAudio())
            {
                mioNode = iMioAudioInputFactory.Create(fileSettings);
            }
            else if (format.isVideo())
            {
                mioNode = iMioVideoInputFactory.Create(fileSettings);
            }
        }
    }
    return mioNode;
}

OSCL_EXPORT_REF void PV2WaySourceAndSinksFile::DeleteMIONode(CodecSpecifier* aformat,
        TPVDirection adir,
        PVMFNodeInterface** aMioNode)
{
    if (!aformat)
    {
        if (aMioNode)
        {
            OSCL_DELETE(*aMioNode);
            *aMioNode = NULL;
        }
        return;
    }
    PVMFFormatType format = aformat->GetFormat();
    if (aformat->GetType() == EPVMIOFileInput)
    {
        if (adir == INCOMING)
        {
            if (format.isAudio())
            {
                iMioAudioOutputFactory.Delete(aMioNode);
            }
            else if (format.isVideo())
            {
                iMioVideoOutputFactory.Delete(aMioNode);
            }
        }
        else if (adir == OUTGOING)
        {
            if (format.isAudio())
            {
                iMioAudioInputFactory.Delete(aMioNode);
            }
            else if (format.isVideo())
            {
                iMioVideoInputFactory.Delete(aMioNode);
            }
        }
    }
    *aMioNode = NULL;
}

OSCL_EXPORT_REF void PV2WaySourceAndSinksFile::AddCapturingRenderingDevice(TPVDirection aDir,
        PV2WayMediaType aMediaType)
{
    //depending on the direction and mediaType we will add the device
    if ((aMediaType == PV_AUDIO) && (aDir == INCOMING))
        iAudioRenderingDevice = true;
    //TODO: eventually more devices will be added like video capture, audio capture etc etc
}


