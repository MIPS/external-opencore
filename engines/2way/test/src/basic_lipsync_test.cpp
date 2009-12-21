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
#include "basic_lipsync_test.h"


#define RENDER_DURATION 60  // in seconds

void basic_lipsync_test::test()
{
    test_base::test();
    iShareParams->Destroy();
}

void basic_lipsync_test::Run()
{
    if (terminal)
    {
        if (iUseProxy)

        {

            CPV2WayProxyFactory::DeleteTerminal(terminal);
        }
        else
        {
            CPV2WayEngineFactory::DeleteTerminal(terminal);
        }
        terminal = NULL;
    }

    if (icount == 0)
    {
        PV2WayUtil::OutputInfo("\n Not able to calculate the RMS value as count value is zero %d\n", icount);
    }
    else
    {
        iRtMnSq = (float)sqrt(iSqrVidAudTS / icount);
        PV2WayUtil::OutputInfo("\n Root Mean Square value of lipsync delta on application side is %fsec\n", iRtMnSq);
    }

    scheduler->StopScheduler();

}

void basic_lipsync_test::TimeoutOccurred(int32 timerID, int32 timeoutInfo)
{
    if (timerID == iTimerConnectionID)
    {
        PV2WayUtil::OutputInfo("\n-------- Basic lipsync test stats--------\n");
        PV2WayUtil::OutputInfo("\n------------------------------------------\n");
        timer->Request(iTimerRender, iTimeoutInfoRender, RENDER_DURATION);
    }
    else if (timerID == iTimerTestTimeoutID)
    {
        timer->Cancel(iTimerConnectionID);
        timer->Cancel(iTimerRender);
        PV2WayUtil::OutputInfo("\n Test ran out of time.  Max time: %d \n", iMaxTestDuration);
        test_is_true(false);
        // cleanup because we are unsuccessful
        DisconnectSourceSinks();
    }
    else if (timerID == iTimerRender)
    {
        timer->Cancel(iTimerTestTimeoutID);
        DisconnectSourceSinks();
    }
}

void basic_lipsync_test::MIOFramesUpdate(bool aIsAudio, uint32 aBytes, uint32 aRenderTS)
{

    if (aIsAudio)
    {
        iCurrentAudioTS = aRenderTS;
        iAudioPresent = true;
    }
    else
    {
        iCurrentVideoTS = aRenderTS;
        if (iAudioPresent)
        {
            ++icount;
            iDiffVidAudTS = (iCurrentVideoTS - iCurrentAudioTS);
            iDiffVidAudTS = (iDiffVidAudTS / 1000);
            iSqrVidAudTS += (iDiffVidAudTS * iDiffVidAudTS);

        }

    }
}

