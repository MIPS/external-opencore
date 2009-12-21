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
#ifndef RECEIVE_DATA_TEST_H_INCLUDED
#define RECEIVE_DATA_TEST_H_INCLUDED

#include "av_test.h"
#include "lipsync_dummy_output_mio.h"
#include "lipsync_singleton_object.h"

#define RECEIVE_TEST_TIME 20
#define RECEIVE_MAX_TEST_TIME 240
class receive_data_test : public av_test,
        public DummyMIOObserver
{
    public:
        receive_data_test(bool aUseProxy = false,
                          uint32 aTimeConnection = RECEIVE_TEST_TIME,
                          uint32 aMaxTestDuration = RECEIVE_MAX_TEST_TIME)
                : av_test(aUseProxy, aTimeConnection, aMaxTestDuration)
                , iReceivedAudio(0)
                , iReceivedVideo(0)
        {
            iTestName = _STRLIT_CHAR("Receive Data");
        }

        ~receive_data_test()
        {

        }


    protected:
        virtual void TimeoutOccurred(int32 timerID, int32 timeoutInfo);

    private:
        void MIOFramesUpdate(bool aIsAudio, uint32 aBytes, uint32 aTS);
        int32 iReceivedAudio;
        int32 iReceivedVideo;
};


#endif


