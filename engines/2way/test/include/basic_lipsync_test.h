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
#ifndef BASIC_LIPSYNC_TEST_H_INCLUDED
#define BASIC_LIPSYNC_TEST_H_INCLUDED

#include "av_test.h"
#include "lipsync_dummy_output_mio.h"
#include "lipsync_singleton_object.h"

class basic_lipsync_test : public av_test,
        public LipSyncDummyMIOObserver
{
    public:
        basic_lipsync_test(bool aUseProxy = false,
                           uint32 aTimeConnection = TEST_DURATION,
                           uint32 aMaxTestDuration = MAX_TEST_DURATION*2)
                : av_test(aUseProxy, aTimeConnection, aMaxTestDuration)
                , iCurrentVideoTS(0)
                , iCurrentAudioTS(0)
                , iDiffVidAudTS(0)
                , iSqrVidAudTS(0)
                , icount(0)
                , iRtMnSq(0)
                , iAudioPresent(false)
                , iTimerRender(3)
                , iTimeoutInfoRender(0)
        {
            iTestName = _STRLIT_CHAR("basic lipsync");
        }

        ~basic_lipsync_test()
        {

        }

        void test();

        void Run();

    protected:
        virtual void TimeoutOccurred(int32 timerID, int32 timeoutInfo);

    private:
        void MIOFramesTSUpdate(bool aIsAudio, uint32 aTS);
        int32 iCurrentVideoTS;
        int32  iCurrentAudioTS;
        float iDiffVidAudTS;
        float iSqrVidAudTS;
        int32 icount;
        float iRtMnSq;
        bool  iAudioPresent;
        ShareParams* iShareParams;
        int32 iTimerRender;
        int32 iTimeoutInfoRender;
};


#endif


