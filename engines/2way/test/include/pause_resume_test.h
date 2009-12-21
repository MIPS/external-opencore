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
#ifndef PAUSE_RESUME_TEST_H_INCLUDED
#define PAUSE_RESUME_TEST_H_INCLUDED

#include "av_test.h"


class pause_resume_test : public av_test
{
    public:
        pause_resume_test(bool aAudioPauseSourceSide = false,
                          uint32 aAudioSourcePauseTime = 0, uint32 aAudioSourceResumeTime = 0,
                          bool aAudioPauseSinkSide = false,
                          uint32 aAudioSinkPauseTime = 0, uint32 aAudioSinkResumeTime = 0,
                          bool aVideoPauseSourceSide = false,
                          uint32 aVideoSourcePauseTime = 0, uint32 aVideoSourceResumeTime = 0,
                          bool aVideoPauseSinkSide = false,
                          uint32 aVideoSinkPauseTime = 0, uint32 aVideoSinkResumeTime = 0,
                          bool aUseProxy = false,
                          uint32 aTimeConnection = TEST_DURATION,
                          uint32 aMaxTestDuration = MAX_TEST_DURATION)
                : av_test(aUseProxy, aTimeConnection, aMaxTestDuration),
                iAudioSourceSide(aAudioPauseSourceSide),
                iAudioSinkSide(aAudioPauseSinkSide),
                iVideoSourceSide(aVideoPauseSourceSide),
                iVideoSinkSide(aVideoPauseSinkSide),
                iAudioSrcPauseTime(aAudioSourcePauseTime),
                iAudioSrcResumeTime(aAudioSourceResumeTime),
                iAudioSnkPauseTime(aAudioSinkPauseTime),
                iAudioSnkResumeTime(aAudioSinkResumeTime),
                iVideoSrcPauseTime(aVideoSourcePauseTime),
                iVideoSrcResumeTime(aVideoSourceResumeTime),
                iVideoSnkPauseTime(aVideoSinkPauseTime),
                iVideoSnkResumeTime(aVideoSinkResumeTime),
                iPauseResumeInfo(0),
                iPausResTestCaseId(10),
                iAudioSrcPauseId(11),
                iAudioSrcResumeId(12),
                iAudioSnkPauseId(13),
                iAudioSnkResumeId(14),
                iVideoSrcPauseId(15),
                iVideoSrcResumeId(16),
                iVideoSnkPauseId(17),
                iVideoSnkResumeId(18),
                aCmdId(0)

        {
            uint32 totalTime = aAudioSourcePauseTime + aAudioSourceResumeTime +
                               aAudioSinkPauseTime + aAudioSinkResumeTime +
                               aVideoSourcePauseTime + aVideoSourceResumeTime +
                               aVideoSinkPauseTime + aVideoSinkResumeTime;

            if (totalTime > aMaxTestDuration)
            {
                iMaxTestDuration = aMaxTestDuration + totalTime;
            }
            iUsingAudio = true;
            iTestName = _STRLIT_CHAR("Pause Resume");


        }

        ~pause_resume_test()
        {};

        void CommandCompleted(const PVCmdResponse& aResponse);

    private:

        bool iAudioSourceSide;
        bool iAudioSinkSide;
        bool iVideoSourceSide;
        bool iVideoSinkSide;
        uint32 iAudioSrcPauseTime;
        uint32 iAudioSrcResumeTime;
        uint32 iAudioSnkPauseTime;
        uint32 iAudioSnkResumeTime;
        uint32 iVideoSrcPauseTime;
        uint32 iVideoSrcResumeTime;
        uint32 iVideoSnkPauseTime;
        uint32 iVideoSnkResumeTime;
        int32  iPauseResumeInfo;
        int32  iPausResTestCaseId;
        int32  iAudioSrcPauseId;
        int32  iAudioSrcResumeId;
        int32  iAudioSnkPauseId;
        int32  iAudioSnkResumeId;
        int32  iVideoSrcPauseId;
        int32  iVideoSrcResumeId;
        int32  iVideoSnkPauseId;
        int32  iVideoSnkResumeId;
        PVCommandId aCmdId;
        void Pause(TPVDirection, PVCommandId&, TPVChannelId*);
        void Resume(TPVDirection, PVCommandId&, TPVChannelId*);
        void AllNodesAdded();
        void LetCallPauseResume();
        void TimeoutOccurred(int32 timerID, int32 timeoutInfo);
        void CancelAllTimers();
};

#endif


