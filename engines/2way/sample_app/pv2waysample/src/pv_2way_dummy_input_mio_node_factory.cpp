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

#include "pv_2way_dummy_input_mio_node_factory.h"
#include "pv_2way_interface.h"
#include "pv_2way_media.h"
#include "pvmf_media_input_node_factory.h"
#include "dummy_input_mio.h"
#include "dummy_settings.h"

int PV2WayDummyInputMIONodeFactory::CreateMedia(DummyMIOSettings& aSettings)
{
    iMediaControl = OSCL_NEW(DummyInputMIO, (aSettings));
    return PVMFSuccess;
}

void PV2WayDummyInputMIONodeFactory::DeleteMedia()
{
    OSCL_DELETE(iMediaControl);
    iMediaControl = NULL;
}

OSCL_EXPORT_REF void PV2WayDummyInputMIONodeFactory::Delete(PVMFNodeInterface** aMioNode)
{
    PvmfMediaInputNodeFactory::Delete(*aMioNode);
    DeleteMedia();
    *aMioNode = NULL;
}

OSCL_EXPORT_REF PVMFNodeInterface* PV2WayDummyInputMIONodeFactory::Create(
    DummyMIOSettings& aSettings)
{
    CreateMedia(aSettings);
    PVMFNodeInterface* mioNode = NULL;
    int error = 0;
    OSCL_TRY(error, mioNode = PvmfMediaInputNodeFactory::Create(iMediaControl));
    return mioNode;
}




