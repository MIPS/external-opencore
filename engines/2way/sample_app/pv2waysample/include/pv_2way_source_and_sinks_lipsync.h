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
#ifndef PV_2WAY_SOURCE_AND_SINKS_LIPSYNC_H_INCLUDED
#define PV_2WAY_SOURCE_AND_SINKS_LIPSYNC_H_INCLUDED

#ifndef PV_2WAY_SOURCE_AND_SINKS_DUMMY_H_INCLUDED
#include "pv_2way_source_and_sinks_dummy.h"
#endif


#ifndef PV_2WAY_LIPSYNC_INPUT_MIO_NODE_FACTORY_H_INCLUDED
#include "pv_2way_lipsync_input_mio_node_factory.h"
#endif

#ifndef PV_2WAY_LIPSYNC_OUTPUT_MIO_NODE_FACTORY_H_INCLUDED
#include "pv_2way_lipsync_output_mio_node_factory.h"
#endif

////////////////////////////////////////////////////////////////////////////
// This file includes the base class dummy MIOs (sources and sinks) to be used
// for lipsync test cases within the application
//
// At this point it consists of one audio sink, one audio source, one video
// sink, one video source.  As all of these use the same base class it could
// be easily modified, however.
//
////////////////////////////////////////////////////////////////////////////



class PV2WayLipSyncSourceAndSinks : public PV2WayDummySourceAndSinks
{
    public:
        OSCL_IMPORT_REF PV2WayLipSyncSourceAndSinks(PV2Way324InitInfo& aSdkInitInfo);
        OSCL_IMPORT_REF virtual ~PV2WayLipSyncSourceAndSinks();

    protected:

};

#endif
