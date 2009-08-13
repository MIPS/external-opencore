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
#include "pv_2way_source_and_sinks_perf_test.h"
#include "pv_2way_sink.h"
#include "pv_2way_source.h"


#include "pv_media_output_node_factory.h"
#include "pvmf_media_input_node_factory.h"

#include "pv_2way_interface.h"
#include "pvmf_fileoutput_factory.h"
#include "pv_2way_media.h"
#include "pvmf_dummy_fileinput_node_factory.h"
#include "pvmf_dummy_fileoutput_factory.h"

#include "pv2way_file_names.h"



OSCL_EXPORT_REF PV2WaySourceAndSinksPerfTest::PV2WaySourceAndSinksPerfTest(PV2Way324InitInfo& aSdkInitInfo) :
        PV2WaySourceAndSinksBase(aSdkInitInfo)
{
    SetPerfFileSettings();
}


OSCL_EXPORT_REF PV2WaySourceAndSinksPerfTest::~PV2WaySourceAndSinksPerfTest()
{
    Cleanup();
}


OSCL_EXPORT_REF PVMFNodeInterface* PV2WaySourceAndSinksPerfTest::CreateMIONode(CodecSpecifier* aformat,
        TPVDirection adir)
{
    PVMFNodeInterface* mioNode = NULL;
    PVMFFormatType format = aformat->GetFormat();
    if (aformat->GetType() == EPVFileInput)
    {
        FileCodecSpecifier* temp = OSCL_REINTERPRET_CAST(FileCodecSpecifier*, aformat);
        PVMFFileInputSettings perfFileSettings = temp->GetSpecifierType();
        if (adir == INCOMING)
        {
            PVMFFormatType fileSettings = PV2WayMedia::GetMediaFormat(perfFileSettings.iFileName.get_cstr());
            return PVMFDummyFileOutputNodeFactory::CreateDummyFileOutput(perfFileSettings.iFileName, fileSettings);
        }
        else if (adir == OUTGOING)
        {
            return PVMFDummyFileInputNodeFactory::CreateDummyFileInputNode(&perfFileSettings);
        }
    }
    return mioNode;
}

OSCL_EXPORT_REF void PV2WaySourceAndSinksPerfTest::DeleteMIONode(CodecSpecifier* aformat,
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
    if (aformat->GetType() == EPVFileInput)
    {
        if (adir == INCOMING)
        {
            PVMFDummyFileOutputNodeFactory::DeleteDummyFileOutput(*aMioNode);
        }
        else if (adir == OUTGOING)
        {
            PVMFDummyFileInputNodeFactory::DeleteDummyFileInputNode(*aMioNode);
        }
    }
    *aMioNode = NULL;
}

OSCL_EXPORT_REF int PV2WaySourceAndSinksPerfTest::SetPerfFileSettings()
{
    PVMFFileInputSettings audioSourceFileSettings;
    audioSourceFileSettings.iMediaFormat = PVMF_MIME_AMR_IF2;
    audioSourceFileSettings.iLoopInputFile = true;
    audioSourceFileSettings.iFileName = AUDIO_SOURCE_FILENAME_FOR_PERF;
    audioSourceFileSettings.iSamplingFrequency = 8000;
    audioSourceFileSettings.iNumChannels = 1;
    audioSourceFileSettings.iFrameRateSimulation = true;
    iAudioSource->AddCodec(audioSourceFileSettings);

    // create audio sink for performance testing
    PVMFFileInputSettings audioSinkFileSettings;
    audioSinkFileSettings.iFileName = AUDIO_SINK_FILENAME;
    audioSinkFileSettings.iMediaFormat = PVMF_MIME_AMR_IF2;
    iAudioSink->AddCodec(audioSinkFileSettings);

    // create the video source for performance testing
    PVMFFileInputSettings videoSourceFileSettings;
    videoSourceFileSettings.iMediaFormat = PVMF_MIME_H2632000; // console was: PVMF_MIME_H263
    videoSourceFileSettings.iLoopInputFile = true;
    videoSourceFileSettings.iFileName = VIDEO_SOURCE_FILENAME_FOR_PERF;
    videoSourceFileSettings.iTimescale = VIDEO_TIMESCALE;
    videoSourceFileSettings.iFrameHeight = VIDEO_FRAMEHEIGHT;
    videoSourceFileSettings.iFrameWidth = VIDEO_FRAMEWIDTH;
    videoSourceFileSettings.iFrameRate = VIDEO_FRAMERATE;
    videoSourceFileSettings.iFrameRateSimulation = true;
    iVideoSource->AddCodec(videoSourceFileSettings);

    // create the video sink for performance testing
    PVMFFileInputSettings videoSinkFileSettings;
    videoSinkFileSettings.iFileName = VIDEO_SINK_FILENAME;
    videoSinkFileSettings.iMediaFormat = PVMF_MIME_H2632000;
    iVideoSink->AddCodec(videoSinkFileSettings);

    return PVMFSuccess;
}




