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
#include "av_test.h"
#include "pv_2way_source_and_sinks_base.h"

void av_test::test()
{
    PV2WayUtil::OutputInfo("\n-------- Start %s test --------\n", iTestName.get_cstr());
    PV2WayUtil::OutputInfo("\n** Test Number: %d. ** \n", iTestNum);
    PV2WayUtil::OutputInfo("\nSETTINGS:\nProxy %d", iUseProxy);
    iSourceAndSinks->PrintFormatTypes();
    PV2WayUtil::OutputInfo("\n----------------------------------\n");
    int error = 0;

    scheduler = OsclExecScheduler::Current();

    this->AddToScheduler();

    if (start_async_test())
    {
        /*!

          Step 5: Start scheduler
          Start scheduler- will start sending messages once initialized
        */
        OSCL_TRY(error, scheduler->StartScheduler());
        if (error != 0)
        {
            OSCL_LEAVE(error);
        }
    }

    TestCompleted();
    this->RemoveFromScheduler();
}


void av_test::Run()
{
    /*!

      Step 12: Cleanup
      Step 12d: Delete terminal
    */
    if (terminal)
    {
        if (iUseProxy)
        {
            CPV2WayProxyFactory::DeleteTerminal(terminal);
        }
        else
        {
            CPV2WayEngineFactory::DeleteTerminal(terminal);
        }
        terminal = NULL;
    }

    if (timer)
    {
        OSCL_DELETE(timer);
        timer = NULL;
    }

    /*!

      Step 12: Cleanup
      Step 12e: Stop Scheduler
    */
    scheduler->StopScheduler();
}

void av_test::DoCancel()
{
    int error = 0;
    OSCL_TRY(error, iCancelCmdId = terminal->CancelAllCommands());
    test_is_true(false);
    reset();
}

void av_test::ConnectSucceeded()
{
}


void av_test::InitFailed()
{
    PV2WayUtil::OutputInfo("\n*************** Test FAILED: Init Failed *************** \n");
    test_is_true(false);
    test_base::InitFailed();
}

void av_test::ConnectFailed()
{
    PV2WayUtil::OutputInfo("\n*** Connect Failed \n");
}

void av_test::AudioAddSinkSucceeded()
{
    iAudioSinkAdded = true;
    if (CheckAllSourceAndSinkAdded())
    {
        timer->RunIfNotReady(TEST_DURATION);
    }
}

void av_test::AudioAddSourceSucceeded()
{
    iAudioSourceAdded = true;
    if (CheckAllSourceAndSinkAdded())
    {
        timer->RunIfNotReady(TEST_DURATION);
    }
}

void av_test::VideoAddSinkSucceeded()
{
    iVideoSinkAdded = true;
    if (CheckAllSourceAndSinkAdded())
    {
        timer->RunIfNotReady(TEST_DURATION);
    }
}

void av_test::VideoAddSourceSucceeded()
{
    iVideoSourceAdded = true;
    if (CheckAllSourceAndSinkAdded())
    {
        timer->RunIfNotReady(TEST_DURATION);
    }
}

void av_test::RstCmdCompleted()
{
    test_is_true(true);
    test_base::RstCmdCompleted();
}

bool av_test::CheckAllSourceAndSinkAdded()
{
    return (iAudioSourceAdded &&
            iAudioSinkAdded &&
            iVideoSourceAdded &&
            iVideoSinkAdded);

}

void av_test::CheckForTimeToDisconnect()
{
    if (!iAudioSourceAdded &&
            !iAudioSinkAdded &&
            !iVideoSourceAdded &&
            !iVideoSinkAdded)
    {
        PV2WayUtil::OutputInfo("\nTime to disconnect \n");
        disconnect();
    }
}

void av_test::AudioRemoveSourceCompleted()
{
    PV2WayUtil::OutputInfo("\nAudio source removed \n");
    iAudioSourceAdded = false;
    CheckForTimeToDisconnect();
}

void av_test::AudioRemoveSinkCompleted()
{
    PV2WayUtil::OutputInfo("\nAudio sink removed \n");
    iAudioSinkAdded = false;
    CheckForTimeToDisconnect();
}

void av_test::VideoRemoveSourceCompleted()
{
    PV2WayUtil::OutputInfo("\nVideo source removed \n");
    iVideoSourceAdded = false;
    CheckForTimeToDisconnect();
}

void av_test::VideoRemoveSinkCompleted()
{
    PV2WayUtil::OutputInfo("\nVideo sink removed \n");
    iVideoSinkAdded = false;
    CheckForTimeToDisconnect();
}

void av_test::TimerCallback()
{
    if (inumCalled > 5)
    {
        PV2WayUtil::OutputInfo("\n Giving up waiting for process to finish \n");
        iTestStatus = false;
        DoCancel();
        return;
    }
    inumCalled++;
    if (!iAudioSourceAdded ||
            !iAudioSinkAdded ||
            !iVideoSourceAdded ||
            !iVideoSinkAdded)
    {
        // wait longer
        timer->RunIfNotReady(TEST_DURATION);
        return;
    }
    FinishTimerCallback();
}

void av_test::FinishTimerCallback()
{
    int error = 0;
    PV2WayUtil::OutputInfo("\nRemoving source and sinks \n");
    /*!

      Step 12: Cleanup
      Step 12a: Remove source and sinks
    */
    OSCL_TRY(error, iVideoRemoveSourceId = iSourceAndSinks->RemoveVideoSource());
    if (error)
    {
        iTestStatus &= false;
        disconnect();
    }
    else
    {
        error = 1;
        OSCL_TRY(error, iVideoRemoveSinkId = iSourceAndSinks->RemoveVideoSink());
        if (error)
        {
            iTestStatus &= false;
            disconnect();
        }
    }

    OSCL_TRY(error, iAudioRemoveSourceId = iSourceAndSinks->RemoveAudioSource());
    if (error)
    {
        iTestStatus &= false;
        disconnect();
    }
    else
    {
        error = 1;
        OSCL_TRY(error, iAudioRemoveSinkId = iSourceAndSinks->RemoveAudioSink());
        if (error)
        {
            iTestStatus &= false;
            disconnect();
        }
    }
}

bool av_test::start_async_test()
{
    timer = OSCL_NEW(engine_timer, (this));
    if (timer == NULL)
    {
        PV2WayUtil::OutputInfo("\n*************** Test FAILED: no timer *************** \n");
        test_is_true(false);
        return false;
    }
    iAudioSourceAdded = false;
    iAudioSinkAdded = false;

    timer->AddToScheduler();


    return test_base::start_async_test();
}





