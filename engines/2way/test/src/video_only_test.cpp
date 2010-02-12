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
#include "video_only_test.h"

#ifndef PV_VIDEO_ENCNODE_EXTENSION_H_INCLUDED
#include "pv_video_encnode_extension.h"
#endif

#ifndef TEST_UTILITY_H_HEADER
#include "test_utility.h"
#endif

#define TRADEOFF_VALUE 5
#define TRADEOFF_VALUE_2 10




void video_only_test::DoCancel()
{
}


void video_only_test::AllVideoNodesAdded()
{
    test_is_true(true);
    LetConnectionRun();
}

void video_only_test::AllVideoNodesRemoved()
{
    PV2WayUtil::OutputInfo("\nTime to disconnect \n");
    disconnect();
}

void video_only_test::VideoAddSourceFailed()
{
    if (iVidSrcFormatType == PVMF_MIME_M4V)
    {
        test_is_true(true);
    }
    else
    {
        printf("\n*************** Test FAILED: error adding video source *************** \n");
        test_is_true(false);
    }
    disconnect();
}

void video_only_test::DisCmdFailed()
{
    DisCmdSucceeded();
}

void video_only_test::DisCmdSucceeded()
{
    printf("Finished disconnecting \n");
    reset();
}

void video_only_test::FinishTimerCallback()
{
    int error = 1;

    OSCL_TRY(error, iVideoRemoveSourceId = iSourceAndSinks->RemoveVideoSource());
    if (error)
    {
        printf("\n*************** Test FAILED: error removing video source *************** \n");
        test_is_true(false);
        disconnect();
    }
    OSCL_TRY(error, iVideoRemoveSinkId = iSourceAndSinks->RemoveVideoSink());
    if (error)
    {
        PV2WayUtil::OutputInfo("\n*************** Test FAILED: error removing video sink *************** \n");
        test_is_true(false);
        disconnect();
    }

}





