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
#include "pause_resume_test.h"

#define PAUSE_RESUME_TEST_DURATION 55


void pause_resume_test::CommandCompleted(const PVCmdResponse& aResponse)
{
    PVCommandId cmdId = aResponse.GetCmdId();
    if (cmdId < 0)
        return;

    if (iAudioPauseSourceId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {

            PV2WayUtil::OutputInfo("\n Audio Pause completed Successfully at source side\n");
        }
        else
        {
            PV2WayUtil::OutputInfo("\n Audio Pause not completed Successfully at source side");
        }
    }
    else if (iAudioPauseSinkId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            PV2WayUtil::OutputInfo("\n Audio Pause completed Successfully at sink side \n");
        }
        else
        {
            PV2WayUtil::OutputInfo("\n Audio Pause not completed Successfully at sink side \n");
        }
    }
    else if (iAudioResumeSourceId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            PV2WayUtil::OutputInfo("\n Audio Resume completed Successfully at source side \n");
        }
        else
        {
            PV2WayUtil::OutputInfo("\n Audio Resume not completed Successfully at source side \n");
        }

    }
    else if (iAudioResumeSinkId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            PV2WayUtil::OutputInfo("\n Audio Resume completed Successfully at sink side \n");
        }
        else
        {
            PV2WayUtil::OutputInfo("\n Audio Resume not completed Successfully at sink side \n");
        }
    }
    else if (iVideoResumeSinkId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            PV2WayUtil::OutputInfo("\n Video Resume completed Successfully at sink side \n");
        }
        else
        {
            PV2WayUtil::OutputInfo("\n Video Resume not completed Successfully at sink side \n");
        }
    }
    else if (iVideoResumeSourceId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            PV2WayUtil::OutputInfo("\n Video Resume completed Successfully at source side \n");

        }
        else
        {
            PV2WayUtil::OutputInfo("\n Video Resume not completed Successfully at source side \n");
        }
    }
    else if (iVideoPauseSourceId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            PV2WayUtil::OutputInfo("\n Video Pause completed Successfully at source side\n");

        }
        else
        {
            PV2WayUtil::OutputInfo("\n Video Pause not completed Successfully at source side\n");
        }


    }
    else if (iVideoPauseSinkId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            PV2WayUtil::OutputInfo("\n Video Pause completed Successfully at sink side\n");
        }
        else
        {
            PV2WayUtil::OutputInfo("\n Video Pause not completed Successfully at sink side\n");

        }
    }
    else
    {
        test_base::CommandCompleted(aResponse);
    }

}


void pause_resume_test::Pause(TPVDirection aDirection, PVCommandId &aCmdId, TPVChannelId* aChannelId)
{
    int32 error = 0;

    OSCL_TRY(error, aCmdId = terminal->Pause(aDirection, *aChannelId));

    if (error)
    {
        test_is_true(false);
        disconnect();
    }


}

void pause_resume_test::Resume(TPVDirection aDirection, PVCommandId &aCmdId, TPVChannelId* aChannelId)
{
    int32 error = 0;
    OSCL_TRY(error, aCmdId = terminal->Resume(aDirection, *aChannelId));
    if (error)
    {
        test_is_true(false);
        disconnect();
    }


}


void pause_resume_test::AllNodesAdded()
{
    test_is_true(true);
    LetCallPauseResume();
}

void pause_resume_test::LetCallPauseResume()
{
    /* After source and sink connected, the time provided by app to pause audio/video, source/sink is called Pause Duration.
    Once Pause is done, the time provided by app to call Resume for audio/video, source/sink is called Resume Duratiion.
    In our case, we are first issuing pause duration and in resume case we are adding both the pause and resume duration.
    So now the resume duration will be (pause duration + resume duration) after source and sink succeeded. */

    timer->Request(iPausResTestCaseId, iPauseResumeInfo, PAUSE_RESUME_TEST_DURATION);
    if (iAudioSourceSide)
    {
        timer->Request(iAudioSrcPauseId, iPauseResumeInfo, iAudioSrcPauseTime);
        timer->Request(iAudioSrcResumeId, iPauseResumeInfo, iAudioSrcPauseTime + iAudioSrcResumeTime);
    }
    if (iAudioSinkSide)
    {
        timer->Request(iAudioSnkPauseId, iPauseResumeInfo, iAudioSnkPauseTime);
        timer->Request(iAudioSnkResumeId, iPauseResumeInfo, iAudioSnkPauseTime + iAudioSnkResumeTime);
    }
    if (iVideoSourceSide)
    {
        timer->Request(iVideoSrcPauseId, iPauseResumeInfo, iVideoSrcPauseTime);
        timer->Request(iVideoSrcResumeId, iPauseResumeInfo, iVideoSrcPauseTime + iVideoSrcResumeTime);
    }
    if (iVideoSinkSide)
    {
        timer->Request(iVideoSnkPauseId, iPauseResumeInfo, iVideoSnkPauseTime);
        timer->Request(iVideoSnkResumeId, iPauseResumeInfo, iVideoSnkPauseTime + iVideoSnkResumeTime);
    }


}
void pause_resume_test::TimeoutOccurred(int32 timerID, int32 timeoutInfo)
{
    if (timerID == iAudioSrcPauseId)
    {
        Pause(OUTGOING, iAudioPauseSourceId, iAudioSrcChannelId);
        PV2WayUtil::OutputInfo("\n Audio Source Pause Duration is : %d \n", iAudioSrcPauseTime);
    }
    else if (timerID == iAudioSrcResumeId)
    {
        Resume(OUTGOING, iAudioResumeSourceId, iAudioSrcChannelId);
        PV2WayUtil::OutputInfo("\n Audio Source Resume Duration is : %d \n", iAudioSrcResumeTime);

    }
    else if (timerID == iAudioSnkPauseId)
    {
        Pause(INCOMING, iAudioPauseSinkId, iAudioSnkChannelId);
        PV2WayUtil::OutputInfo("\n Audio Sink Pause Duration is : %d \n", iAudioSnkPauseTime);
    }
    else if (timerID == iAudioSnkResumeId)
    {
        Resume(INCOMING, iAudioResumeSinkId, iAudioSnkChannelId);
        PV2WayUtil::OutputInfo("\n Audio Sink Resume Duration is : %d \n", iAudioSnkResumeTime);

    }
    else if (timerID == iVideoSrcPauseId)
    {
        Pause(OUTGOING, iVideoPauseSourceId, iVideoSrcChannelId);
        PV2WayUtil::OutputInfo("\n Video Source Pause Duration is : %d \n", iVideoSrcPauseTime);
    }
    else if (timerID == iVideoSrcResumeId)
    {
        Resume(OUTGOING, iVideoResumeSourceId, iVideoSrcChannelId);
        PV2WayUtil::OutputInfo("\n Video Source Resume Duration is : %d \n", iVideoSrcResumeTime);

    }
    else if (timerID == iVideoSnkPauseId)
    {
        Pause(INCOMING, iVideoPauseSinkId, iVideoSnkChannelId);
        PV2WayUtil::OutputInfo("\n Video Sink Pause Duration is : %d \n", iVideoSnkPauseTime);

    }
    else if (timerID == iVideoSnkResumeId)
    {
        Resume(INCOMING, iVideoResumeSinkId, iVideoSnkChannelId);
        PV2WayUtil::OutputInfo("\n Video Sink Resume Duration is : %d \n", iVideoSnkResumeTime);

    }

    else if (timerID == iTimerTestTimeoutID)
    {
        timer->Cancel(iTimerConnectionID);
        PV2WayUtil::OutputInfo("\n Test ran out of time.  Max time: %d \n", iMaxTestDuration);
        test_is_true(false);
        // cleanup because we are unsuccessful
        timer->Cancel(iPausResTestCaseId);
        CancelAllTimers();
        DisconnectSourceSinks();
    }
    else if (timerID == iPausResTestCaseId)
    {
        timer->Cancel(iPausResTestCaseId);
        CancelAllTimers();
        av_test::FinishTimerCallback();
    }

}

void pause_resume_test::CancelAllTimers()
{
    if (iAudioSourceSide)
    {
        timer->Cancel(iAudioSrcPauseId);
        timer->Cancel(iAudioSrcResumeId);
    }
    if (iAudioSinkSide)
    {
        timer->Cancel(iAudioSnkPauseId);
        timer->Cancel(iAudioSnkResumeId);
    }
    if (iVideoSourceSide)
    {
        timer->Cancel(iVideoSrcPauseId);
        timer->Cancel(iVideoSrcResumeId);
    }
    if (iVideoSinkSide)
    {
        timer->Cancel(iVideoSnkPauseId);
        timer->Cancel(iVideoSnkResumeId);
    }

}

