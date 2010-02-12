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
#ifndef ERROR_CHECK_AUDIO_ONLY_TEST_H_INCLUDED
#define ERROR_CHECK_AUDIO_ONLY_TEST_H_INCLUDED


#include "audio_only_test.h"

class error_check_audio_only_test : public audio_only_test
{
    public:
        error_check_audio_only_test(bool aUseProxy,
                                    uint32 aTimeConnection = TEST_DURATION,
                                    uint32 aMaxTestDuration = MAX_TEST_DURATION)
                : audio_only_test(aUseProxy, aTimeConnection, aMaxTestDuration)
                , iAudSrc(false)
                , iAudSnk(false)

        {
            iUsingAudio = true;
            iTestName = _STRLIT_CHAR("error in adding audio mio");
            iAudioSourceAdded = false;
            iAudioSinkAdded = false;
        };

        ~error_check_audio_only_test()
        {
        }




    protected:
        virtual void AudioAddSourceSucceeded();
        virtual void AudioAddSinkSucceeded();
        virtual void CheckForRemoved();
        virtual void AudioAddSinkFailed();
        virtual void AudioAddSourceFailed();


    private:
        bool iAudSrc;
        bool iAudSnk;
};


#endif


