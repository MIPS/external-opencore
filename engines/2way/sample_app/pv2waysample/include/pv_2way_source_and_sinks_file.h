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
#ifndef PV_2WAY_SOURCE_AND_SINKS_FILE_H_INCLUDED
#define PV_2WAY_SOURCE_AND_SINKS_FILE_H_INCLUDED

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

#ifndef PV_2WAY_H324M_TYPES_H_INCLUDED
#include "pv_2way_h324m_types.h"
#endif

#ifndef PV_2WAY_MEDIA_INPUT_MIO_NODE_FACTORY_H_INCLUDED
#include "pv_2way_media_input_mio_node_factory.h"
#endif

#ifndef PV_2WAY_MEDIA_OUTPUT_MIO_NODE_FACTORY_H_INCLUDED
#include "pv_2way_media_output_mio_node_factory.h"
#endif

class PV2WaySourceAndSinksFile : public PV2WaySourceAndSinksBase
{
    public:
        OSCL_IMPORT_REF PV2WaySourceAndSinksFile(PV2Way324InitInfo& aSdkInitInfo);
        OSCL_IMPORT_REF virtual ~PV2WaySourceAndSinksFile();

        OSCL_IMPORT_REF int AddPreferredCodec(TPVDirection aDir,
                                              PV2WayMediaType aMediaType,
                                              PvmiMIOFileInputSettings& aFileSettings);

        OSCL_IMPORT_REF PVMFNodeInterface* CreateMIONode(CodecSpecifier* aformat, TPVDirection adir);
        OSCL_IMPORT_REF void DeleteMIONode(CodecSpecifier* aformat,
                                           TPVDirection adir,
                                           PVMFNodeInterface** aMioNode);
        OSCL_IMPORT_REF void AddCapturingRenderingDevice(TPVDirection aDir,
                PV2WayMediaType aMediaType);
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

        PV2WayMediaInputMIONodeFactory iMioAudioInputFactory;
        PV2WayMediaOutputMIONodeFactory iMioAudioOutputFactory;
        PV2WayMediaInputMIONodeFactory iMioVideoInputFactory;
        PV2WayMediaOutputMIONodeFactory iMioVideoOutputFactory;

        //devices
        bool iAudioRenderingDevice;
};


#endif
