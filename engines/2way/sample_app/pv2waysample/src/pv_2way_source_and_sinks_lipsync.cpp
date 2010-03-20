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
#include "pv_2way_source_and_sinks_lipsync.h"


OSCL_EXPORT_REF PV2WayLipSyncSourceAndSinks::PV2WayLipSyncSourceAndSinks(PV2Way324InitInfo& aSdkInitInfo):
        PV2WayDummySourceAndSinks(aSdkInitInfo)
{
    if (iDummyMioAudioInputFactory)
    {
        OSCL_DELETE(iDummyMioAudioInputFactory);
    }
    if (iDummyMioAudioOutputFactory)
    {
        OSCL_DELETE(iDummyMioAudioOutputFactory);
    }
    if (iDummyMioVideoInputFactory)
    {
        OSCL_DELETE(iDummyMioVideoInputFactory);
    }
    if (iDummyMioVideoOutputFactory)
    {
        OSCL_DELETE(iDummyMioVideoOutputFactory);
    }
    iDummyMioAudioInputFactory = OSCL_NEW(PV2WayLipSyncInputMIONodeFactory, ());
    iDummyMioVideoInputFactory = OSCL_NEW(PV2WayLipSyncInputMIONodeFactory, ());

    iDummyMioAudioOutputFactory = OSCL_NEW(PV2WayLipSyncOutputMIONodeFactory, ());
    iDummyMioVideoOutputFactory = OSCL_NEW(PV2WayLipSyncOutputMIONodeFactory, ());
}

OSCL_EXPORT_REF PV2WayLipSyncSourceAndSinks::~PV2WayLipSyncSourceAndSinks()
{

}


