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

#include "connect_test.h"
#include "tsc_h324m_config_interface.h"
#include "pv_video_encnode_extension.h"




void connect_test::DoCancel()
{
}

void connect_test::ConnectSucceeded()
{
    CancelTimers();
    test_is_true(true);
    disconnect();
}

void connect_test::InitSucceeded()
{
    if (iH324MConfig == NULL)
    {
        test_is_true(false);
        reset();
        return;
    }
    // set some timers via the interface
    H324MConfigInterface * t324Interface = (H324MConfigInterface *)iH324MConfig;
    if (iRunTimerTest)
    {
        t324Interface->SetTimerCounter(EH324Timer, 1, 1, 10, NULL);
        t324Interface->SetTimerCounter(EH324Counter, 4, 1, 10, NULL);
    }
    //t324Interface->removeRef();
    iStackIFSet = true;
    int error;
    PVUuid mp4h263EncUuid = PVMp4H263EncExtensionUUID;
    OSCL_TRY(error, iEncoderIFCommandId = terminal->QueryInterface(mp4h263EncUuid, iMP4H263EncoderInterface, NULL));
    if (error)
    {
        test_is_true(false);
        reset();
    }
}

void connect_test::EncoderIFSucceeded()
{
    PVUuid mp4h263EncUuid = PVMp4H263EncExtensionUUID;
    PVVideoEncExtensionInterface *ptr = (PVVideoEncExtensionInterface *) iMP4H263EncoderInterface;
    if (ptr)
    {
        // Default frame rate is 5
        ptr->SetOutputFrameRate(0, 4);
        iMP4H263EncoderInterface->removeRef();
        iEncoderIFSet = true;
    }
    connect();
}

void connect_test::EncoderIFFailed()
{
//  test_is_true(false);
    reset();
}

void connect_test::ConnectFailed()
{
    test_base::ConnectFailed();
    disconnect();
}

void connect_test::ConnectCancelled()
{
    ConnectFailed();
}

void connect_test::DisCmdSucceeded()
{
    if (++iCurrentRun < iMaxRuns)
    {
        connect();
    }
    else
    {
        reset();
    }
    //destroy_sink_source();
}

void connect_test::DisCmdFailed()
{
    test_is_true(false);
    reset();
    // yep- this is how the code was.
    if (++iCurrentRun < iMaxRuns)
    {
        connect();
    }
    else
    {
        reset();
    }
    //destroy_sink_source();
}






