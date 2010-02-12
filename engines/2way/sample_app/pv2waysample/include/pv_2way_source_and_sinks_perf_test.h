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
#ifndef PV_2WAY_SOURCE_AND_SINKS_PERF_TEST_H_INCLUDED
#define PV_2WAY_SOURCE_AND_SINKS_PERF_TEST_H_INCLUDED

////////////////////////////////////////////////////////////////////////////
// This file includes the base class for all MIOs (sources and sinks) within
// the application
//
// At this point it consists of one audio sink, one audio source, one video
// sink, one video source.  As all of these use the same base class it could
// be easily modified, however.
//
////////////////////////////////////////////////////////////////////////////

#ifndef PV_2WAY_SOURCE_AND_SINKS_BASE_H_INCLUDED
#include "pv_2way_source_and_sinks_base.h"
#endif

#include "pv_2way_h324m_types.h"

#include "pvmf_fileinput_settings.h"

#define AUDIO_SOURCE_FILENAME_FOR_PERF _STRLIT("data/audio_in.amr")
#define VIDEO_SOURCE_FILENAME_FOR_PERF _STRLIT("data/video_in.h263")


class PV2WaySourceAndSinksPerfTest : public PV2WaySourceAndSinksBase
{
    public:
        OSCL_IMPORT_REF PV2WaySourceAndSinksPerfTest(PV2Way324InitInfo& arSdkInitInfo);
        virtual OSCL_IMPORT_REF ~PV2WaySourceAndSinksPerfTest();



        OSCL_IMPORT_REF PVMFNodeInterface* CreateMIONode(CodecSpecifier* aformat, TPVDirection adir);
        OSCL_IMPORT_REF void DeleteMIONode(CodecSpecifier* aformat,
                                           TPVDirection adir,
                                           PVMFNodeInterface** aMioNode);

        OSCL_IMPORT_REF int AddPreferredCodec(TPVDirection aDir,
                                              PV2WayMediaType aMediaType,
                                              PVMFFormatType aFormat)
        {
            return 0;
        }
        OSCL_EXPORT_REF int AddPreferredCodec(TPVDirection aDir,
                                              PV2WayMediaType aMediaType,
                                              PVMFFileInputSettings& arFileSettings);

        OSCL_IMPORT_REF int AddPreferredCodec(TPVDirection aDir,
                                              PV2WayMediaType aMediaType,
                                              PvmiMIOFileInputSettings& arFileSettings)
        {
            return 0;
        }

    protected:
        void OutputInfo(PVLogger::log_level_type aLogLevel, const char * str, ...)
        {
            OSCL_UNUSED_ARG(aLogLevel);
            // output to screen everything in formatted string
            va_list ap;
            va_start(ap, str);
            vprintf(str, ap);
            va_end(ap);
        }

};



#endif
