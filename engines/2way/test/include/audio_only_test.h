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
#ifndef AUDIO_ONLY_TEST_H_INCLUDED
#define AUDIO_ONLY_TEST_H_INCLUDED

#include "test_base.h"

class audio_only_test : public test_base
{
    public:
        audio_only_test(bool aUseProxy,
                        uint32 aTimeConnection = TEST_DURATION,
                        uint32 aMaxTestDuration = MAX_TEST_DURATION)
                : test_base(aUseProxy, aTimeConnection, aMaxTestDuration)

        {
            iUsingAudio = true;
            iTestName = _STRLIT_CHAR("audio only");
            iAudioSourceAdded = false;
            iAudioSinkAdded = false;
        };

        ~audio_only_test()
        {
        }


        void DoCancel();

    protected:
        virtual void FinishTimerCallback();
        virtual void AllAudioNodesAdded();
        virtual void AllAudioNodesRemoved();


    private:
};


#endif


