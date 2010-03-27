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
#include "negotiated_formats_test.h"

void av_using_test_extension::AllNodesAdded()
{
    LetConnectionRun();
}



void av_using_test_extension::CreateParts()
{
    // Get test extension interface handle
    // get TSC node
    iQueryTestInterfaceCmdId = terminal->QueryInterface(PV2WayTestEncExtensionUUID,
                               iTempTestConfigInterface);
}

void av_using_test_extension::CommandCompleted(const PVCmdResponse& aResponse)
{
    PVCommandId cmdId = aResponse.GetCmdId();
    if (cmdId < 0)
        return;

    if (iQueryTestInterfaceCmdId == cmdId)
    {

        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            if (iTempTestConfigInterface)
            {
                iTestConfigInterface = OSCL_STATIC_CAST(PV2WayTestExtensionInterface*,
                                                        iTempTestConfigInterface);
                iTempTestConfigInterface = NULL;
                // set other values
                test_base::CreateParts();
            }

        }

    }
    else
    {
        test_base::CommandCompleted(aResponse);
    }
}
