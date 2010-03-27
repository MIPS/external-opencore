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
#include "turn_on_test_buffer_alloc.h"


void turn_on_test_buffer_alloc::FinishTimerCallback()
{
    bool pass = false;

    // compare values to what we are expecting
    if (iTestConfigInterface)
    {

        bool usingBuffers = iTestConfigInterface->UsingExternalVideoDecBuffers();
        if (usingBuffers)
        {
            pass = true;
        }
    }

    test_is_true(pass);


    DisconnectSourceSinks();

}


