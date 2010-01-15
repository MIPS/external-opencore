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
#include "engine_test.h"


void engine_test::reset()
{
    int error = 0;
    /*!

      Step 12: Cleanup
      Step 12c: Reset
      Reset the terminal
    */
    OSCL_TRY(error, iRstCmdId = terminal->Reset());
    if (error)
    {
        // print out that there was an error resetting.
        PV2WayUtil::OutputInfo("\n** Error in Reset **** \n");
    }
}

void engine_test::create_comm()
{
    /*!

      Step 4a: Create Communication
      Create communication node
      In this case the communication node is a loopback node.
    */
    iCommSettings.iMediaFormat = PVMF_MIME_H223;
    iCommSettings.iTestObserver = NULL;
    iCommServerIOControl = PvmiMIOCommLoopbackFactory::Create(iCommSettings);
    bool enableBitstreamLogging = true;
    iCommServer = PVCommsIONodeFactory::Create(iCommServerIOControl, enableBitstreamLogging);
}

void engine_test::destroy_comm()
{
    if (iCommServer)
    {
        PVCommsIONodeFactory::Delete(iCommServer);
        iCommServer = NULL;
    }

    if (iCommServerIOControl)
    {
        PvmiMIOCommLoopbackFactory::Delete(iCommServerIOControl);
        iCommServerIOControl = NULL;
    }

}


