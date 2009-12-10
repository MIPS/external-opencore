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
#include "av_test.h"
#include "pv_2way_source_and_sinks_base.h"




void av_test::DoCancel()
{
    int error = 0;
    OSCL_TRY(error, iCancelCmdId = terminal->CancelAllCommands());
    test_is_true(false);
    reset();
}



void av_test::AllNodesAdded()
{
    test_is_true(true);
    PV2WayUtil::OutputInfo("\nStarting timer \n");
    LetConnectionRun();
}


void av_test::AllNodesRemoved()
{
    PV2WayUtil::OutputInfo("\nTime to disconnect \n");
    disconnect();
}

void av_test::TimeoutOccurred(int32 timerID, int32 timeoutInfo)
{
    PV2WayUtil::OutputInfo("TimeoutOccurred\n");
    if (timerID == iTimerConnectionID)
    {
        PV2WayUtil::OutputInfo("TimeoutOccurred Connection\n");
        // if the timerConnection status has gone off
        // cancel other timer
        timer->Cancel(iTimerTestTimeoutID);
        // derived classes may have complicated
        // FinishTimerCallbacks.  This one just happens to be the same
        // as when we fail- to just cleanup.
        FinishTimerCallback();
    }
    else if (timerID == iTimerTestTimeoutID)
    {
        timer->Cancel(iTimerConnectionID);
        PV2WayUtil::OutputInfo("\n Test ran out of time.  Max time: %d \n", iMaxTestDuration);
        test_is_true(false);
        // cleanup because we are unsuccessful
        DisconnectSourceSinks();
    }
    else
    {
        // some other failure
        PV2WayUtil::OutputInfo("\n Failure.  Cleaning up \n");
        test_is_true(false);
        // cleanup because we are unsuccessful
        DisconnectSourceSinks();
    }

}

void av_test::FinishTimerCallback()
{
    DisconnectSourceSinks();
}

void av_test::DisconnectSourceSinks()
{
    PV2WayUtil::OutputInfo("\nRemoving source and sinks \n");
    /*!

      Step 12: Cleanup
      Step 12a: Remove source and sinks
    */
    int error = 0;
    OSCL_TRY(error, iVideoRemoveSourceId = iSourceAndSinks->RemoveVideoSource());
    if (error)
    {
        test_is_true(false);
        disconnect();
    }
    else
    {
        error = 1;
        OSCL_TRY(error, iVideoRemoveSinkId = iSourceAndSinks->RemoveVideoSink());
        if (error)
        {
            test_is_true(false);
            disconnect();
        }
    }

    OSCL_TRY(error, iAudioRemoveSourceId = iSourceAndSinks->RemoveAudioSource());
    if (error)
    {
        test_is_true(false);
        disconnect();
    }
    else
    {
        error = 1;
        OSCL_TRY(error, iAudioRemoveSinkId = iSourceAndSinks->RemoveAudioSink());
        if (error)
        {
            test_is_true(false);
            disconnect();
        }
    }
}





