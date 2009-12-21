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
#include "receive_data_test.h"



void receive_data_test::TimeoutOccurred(int32 timerID, int32 timeoutInfo)
{
    if (timerID == iTimerConnectionID)
    {
        //PV2WayUtil::OutputInfo("\n-------- Basic lipsync test stats--------\n");
        //PV2WayUtil::OutputInfo("\n------------------------------------------\n");
        //timer->Request(iTimerRender, iTimeoutInfoRender, RENDER_DURATION);
        PV2WayUtil::OutputInfo("\nReceived: Audio %d Video %d\n",
                               iReceivedAudio, iReceivedVideo);
        if (iReceivedAudio > 0 &&
                iReceivedVideo > 0)
        {
            test_is_true(true);
        }
        else
        {
            test_is_true(false);
        }
        DisconnectSourceSinks();
    }
    else if (timerID == iTimerTestTimeoutID)
    {
        timer->Cancel(iTimerConnectionID);
        test_is_true(false);
        // cleanup because we are unsuccessful
        DisconnectSourceSinks();
    }
}

void receive_data_test::MIOFramesUpdate(bool aIsAudio, uint32 aBytes, uint32 aRenderTS)
{

    if (aIsAudio)
    {
        iReceivedAudio += aBytes;
    }
    else
    {
        iReceivedVideo += aBytes;
    }
}

