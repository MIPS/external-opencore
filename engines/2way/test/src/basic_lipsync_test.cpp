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

#define RENDER_DURATION 60000000

void basic_lipsync_test::test()
{
    fprintf(fileoutput, "\n-------- Start Basic lipsync test --------\n");
    fprintf(fileoutput, "\n** Test Number: %d. ** \n", iTestNum);
    fprintf(fileoutput, "\nSETTINGS:\nProxy %d", iUseProxy);
    iSourceAndSinks->PrintFormatTypes();
    fprintf(fileoutput, "\n----------------------------------\n");
    int error = 0;

    scheduler = OsclExecScheduler::Current();

    this->AddToScheduler();

    if (start_async_test())
    {
        OSCL_TRY(error, scheduler->StartScheduler());
        if (error != 0)
        {
            OSCL_LEAVE(error);
        }
    }
    this->RemoveFromScheduler();
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

    if (timer)
    {
        if (icount == 0)
        {
            fprintf(fileoutput, "\n Not able to calculate the RMS value as count value is zero %d\n", icount);
        }
        else
        {
            iRtMnSq = (float)sqrt(iSqrVidAudTS / icount);
            fprintf(fileoutput, "\n Root Mean Square value of lipsync delta on application side is %fsec\n", iRtMnSq);
            delete timer;
            timer = NULL;
        }
    }

    scheduler->StopScheduler();

}

void basic_lipsync_test::TimerCallback()
{

    if (!iRenderStarted)
    {
        fprintf(fileoutput, "\n-------- Basic lipsync test stats--------\n");
        fprintf(fileoutput, "\n------------------------------------------\n");
        timer->RunIfNotReady(RENDER_DURATION);
        iRenderStarted = true;
        return;
    }


    av_test::TimerCallback();
}

void basic_lipsync_test::MIOFramesTSUpdate(bool aIsAudio, uint32 aRenderTS)
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

