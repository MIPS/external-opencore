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
#ifndef AV_USING_TEST_EXTENSION_H_INCLUDED
#define AV_USING_TEST_EXTENSION_H_INCLUDED
#include "av_test.h"


#include "pv_2way_test_extension_interface.h"

class av_using_test_extension : public av_test
{
    public:
        av_using_test_extension(bool aUseProxy = false,
                                uint32 aTimeConnection = TEST_DURATION,
                                uint32 aMaxTestDuration = MAX_TEST_DURATION)
                : av_test(aUseProxy, aTimeConnection, aMaxTestDuration),
                iTestConfigInterface(NULL),
                iQueryTestInterfaceCmdId(0),
                iTempTestConfigInterface(NULL)
        {
            iTestName = _STRLIT_CHAR("negotiated formats");
        }

        ~av_using_test_extension()
        {
        }


    protected:
        void AllNodesAdded();
        PV2WayTestExtensionInterface* iTestConfigInterface;

    private:
        void CommandCompleted(const PVCmdResponse& aResponse);
        void CreateParts();
        PVCommandId iQueryTestInterfaceCmdId;
        PVInterface* iTempTestConfigInterface;

};


#endif


