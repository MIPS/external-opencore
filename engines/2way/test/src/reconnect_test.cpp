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
#include "reconnect_test.h"



void reconnect_test::ResetInternalValues()
{
    iEncoderIFCommandId = -1;
    iCancelCmdId = -1;
    iQueryInterfaceCmdId = -1;
    iStackIFSet = false;
    iEncoderIFSet = false;
    iRstCmdId = 0;
    iDisCmdId = 0;
    iConnectCmdId = 0;
    iInitCmdId = 0;
    iCommsAddSourceId = 0;
    iSourceAndSinks->CloseSourceAndSinks();
}



void reconnect_test::RstCmdCompleted()
{
    PV2WayUtil::OutputInfo("\nRst Completed \n");
    if (!iReconnected)
    {
        // run again
        PV2WayUtil::OutputInfo("\nReconnecting.... \n");
        iReconnected = true;
        ResetInternalValues();
        Init();
    }
    else
    {
        test_is_true(true);
        cleanup();
        CancelTimers();
        RunIfNotReady();
    }
}

