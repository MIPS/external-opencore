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

#include "pv_2way_dummy_output_mio_node_factory.h"
#include "pv_2way_interface.h"
#include "pv_2way_media.h"
#include "dummy_output_mio.h"
#include "dummy_settings.h"
#include "pv_media_output_node_factory.h"

int PV2WayDummyOutputMIONodeFactory::CreateMedia(DummyMIOSettings& aSettings)
{
    iMediaControl = OSCL_NEW(DummyOutputMIO, (aSettings));
    return PVMFSuccess;
}

void PV2WayDummyOutputMIONodeFactory::DeleteMedia()
{
    OSCL_DELETE(iMediaControl);
    iMediaControl = NULL;
}

OSCL_EXPORT_REF void PV2WayDummyOutputMIONodeFactory::Delete(PVMFNodeInterface** aMioNode)
{
    PVMediaOutputNodeFactory::DeleteMediaOutputNode(*aMioNode);
    *aMioNode = NULL;
    DeleteMedia();
}

OSCL_EXPORT_REF PVMFNodeInterface* PV2WayDummyOutputMIONodeFactory::Create(DummyMIOSettings& aSettings)
{
    PVMFNodeInterface* mioNode = NULL;
    if (PVMFSuccess == CreateMedia(aSettings))
    {
        int error = 0;
        OSCL_TRY(error, mioNode = PVMediaOutputNodeFactory::CreateMediaOutputNode(iMediaControl));
    }

    return mioNode;
}



