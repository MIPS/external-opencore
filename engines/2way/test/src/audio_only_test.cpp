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
#include "audio_only_test.h"



void audio_only_test::DoCancel()
{
}

void audio_only_test::AllAudioNodesAdded()
{
    test_is_true(true);
    LetConnectionRun();
}

void audio_only_test::AllAudioNodesRemoved()
{
    PV2WayUtil::OutputInfo("\nTime to disconnect \n");
    disconnect();
}


void audio_only_test::FinishTimerCallback()
{
    int error = 1;
    OSCL_TRY(error, iAudioRemoveSourceId = iSourceAndSinks->RemoveAudioSource());
    if (error)
    {
        PV2WayUtil::OutputInfo("\n*************** Test FAILED: error removing audio source *************** \n");
        disconnect();
    }
    else
    {
        error = 1;
        OSCL_TRY(error, iAudioRemoveSinkId = iSourceAndSinks->RemoveAudioSink());
        if (error)
        {
            PV2WayUtil::OutputInfo("\n*************** Test FAILED: error removing audio sink *************** \n");
            disconnect();
        }
    }

}

