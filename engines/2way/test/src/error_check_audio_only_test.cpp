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

#include "error_check_audio_only_test.h"



void error_check_audio_only_test::AudioAddSourceSucceeded()
{
    iAudSrc = true;
    AudioAddSourceFailed();
}

void error_check_audio_only_test::AudioAddSinkSucceeded()
{
    iAudSnk = true;
    AudioAddSinkFailed();
}

void error_check_audio_only_test::CheckForRemoved()
{
    if (iAudSrc && iAudSnk)
    {
        test_is_true(true);
        AllAudioNodesRemoved();

    }
}

void error_check_audio_only_test::AudioAddSinkFailed()
{
    int error = 1;
    OSCL_TRY(error, iAudioRemoveSinkId = iSourceAndSinks->RemoveAudioSink());
}
void error_check_audio_only_test::AudioAddSourceFailed()
{
    int error = 1;
    OSCL_TRY(error, iAudioRemoveSourceId = iSourceAndSinks->RemoveAudioSource());
}
