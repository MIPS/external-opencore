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
#ifndef RECONNECT_TEST_H_INCLUDED
#define RECONNECT_TEST_H_INCLUDED

#include "av_test.h"


class reconnect_test : public av_test
{
    public:
        reconnect_test(bool aUseProxy = false,
                       uint32 aTimeConnection = TEST_DURATION,
                       uint32 aMaxTestDuration = MAX_TEST_DURATION)
                : av_test(aUseProxy, aTimeConnection, aMaxTestDuration)
        {
            iTestName = _STRLIT_CHAR("reconnect");
            iReconnected = false;
        }

        virtual ~reconnect_test()
        {
        }

    protected:
        virtual void RstCmdCompleted();
        void ResetInternalValues();

    private:
        bool iReconnected;

};


#endif


