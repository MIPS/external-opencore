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
#ifndef TEST_BASE_H_INCLUDED
#include "test_base.h"
#endif

#ifndef PVLOGGER_CFG_FILE_PARSER_H_INCLUDED
#include "pvlogger_cfg_file_parser.h"
#endif

#include "pvlogger_file_appender.h"
#include "pvlogger_mem_appender.h"


#include "pv_mime_string_utils.h"

#ifndef TEST_UTILITY_H_HEADER
#include "test_utility.h"
#endif
#include "pv_2way_codecspecifier.h"


uint32 test_base::iTestCounter = 0;

void test_base::test()
{
    PV2WayUtil::OutputInfo("\n-------- Start %s test --------\n", iTestName.get_cstr());
    PV2WayUtil::OutputInfo("\n** Test Number: %d. ** \n", iTestNum);
    PV2WayUtil::OutputInfo("\nSETTINGS:\nProxy %d\nConnection Duration: %d\nMax Test Time: %d",
                           iUseProxy, iTimeConnection, iMaxTestDuration);
    iSourceAndSinks->PrintFormatTypes();
    PV2WayUtil::OutputInfo("\n----------------------------------\n");
    int error = 0;
    scheduler = OsclExecScheduler::Current();

    this->AddToScheduler();

    iStarttime = OsclTickCount::TicksToMsec(OsclTickCount::TickCount());
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

void test_base::Run()
{
    /*!

      Step 12: Cleanup
      Step 12d: Delete terminal
    */

    cleanup();

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

void test_base::H324MConfigCommandCompletedL(PVMFCmdResp& aResponse)
{
    OSCL_UNUSED_ARG(aResponse);
}

void test_base::H324MConfigHandleInformationalEventL(PVMFAsyncEvent& aNotification)
{
    OSCL_UNUSED_ARG(aNotification);
}

void test_base::CreateH324Component(bool aCreateH324)
{
    OSCL_UNUSED_ARG(aCreateH324);

    /*!

      Step 4b: Create Component
      First step in creating h324m ocmponent- querying for the TSC node
      using the PVH324MConfigUuid UUID.
    */
    // get TSC node
    iQueryInterfaceCmdId = terminal->QueryInterface(PVH324MConfigUuid,
                           iTempH324MConfigIterface);

}

void test_base::QueryInterfaceSucceeded()
{
    /*!

      Step 6b: Query H324m component
      Using the appropriate uuid, and the TSC node, query for the H324m component
    */
    if (iH324MConfig)
    {
        iH324MConfig->SetObserver(this);

        PVUuid uuid = PVUuidH324ComponentInterface;

        iH324MConfig->queryInterface(uuid, (PVInterface*&)iComponentInterface);
    }
    // now we have created the component, we can do init.
    Init();
}

bool test_base::Init()
{
    int32 error = 0;
    OSCL_FastString aStr;

    /*!

      Step 7: Initialize terminal
      @param PV2Way324InitInfo
      Initialize the terminal using PV2Way324InitInfo structure to set
      the formats that the application supports
    */
    OSCL_TRY(error, iInitCmdId = terminal->Init(iSdkInitInfo));
    if (error)
    {
        PV2WayUtil::OutputInfo("\n*************** Test FAILED: could not initialize terminal *************** \n");
        test_is_true(false);

        if (iUseProxy)
        {
            CPV2WayProxyFactory::DeleteTerminal(terminal);
        }
        else
        {
            CPV2WayEngineFactory::DeleteTerminal(terminal);
        }

        terminal = NULL;
        return false;
    }
    return true;
}


void test_base::InitSucceeded()
{
    connect();
}

void test_base::CancelTimers()
{
    if (timer)
    {
        timer->Cancel(iTimerConnectionID);
        timer->Cancel(iTimerTestTimeoutID);
    }
}

void test_base::InitFailed()
{
    PV2WayUtil::OutputInfo("\n*************** Test FAILED: InitFailed *************** \n");
    CancelTimers();
    test_is_true(false);
    disconnect();
}

void test_base::InitCancelled()
{
    InitFailed();
}

void test_base::EncoderIFSucceeded()
{
}

void test_base::EncoderIFFailed()
{
    PV2WayUtil::OutputInfo("\n*************** Test FAILED: EncoderIF Failed *************** \n");
}

void test_base::ConnectSucceeded()
{
}

void test_base::ConnectFailed()
{
    PV2WayUtil::OutputInfo("\n*************** Test FAILED: connect failed *************** \n");
    test_is_true(false);
    reset();
}

void test_base::ConnectCancelled()
{
    ConnectFailed();
}

void test_base::CancelCmdCompleted()
{
    PV2WayUtil::OutputInfo("\nCancel Completed \n");
    test_is_true(true);
    RunIfNotReady();
}

void test_base::RstCmdCompleted()
{
    PV2WayUtil::OutputInfo("\nRst Completed \n");
    iSourceAndSinks->CloseSourceAndSinks();
    cleanup();
    CancelTimers();

    RunIfNotReady();
}

void test_base::DisCmdSucceeded()
{
    reset();

}

void test_base::DisCmdFailed()
{
    reset();
    PV2WayUtil::OutputInfo("test_base::DisCmdFailed() \n");
}

void test_base::AudioAddSinkSucceeded()
{
    iAudioSinkAdded = true;
    CheckForSucceeded();
}

void test_base::AudioAddSourceSucceeded()
{
    iAudioSourceAdded = true;
    CheckForSucceeded();
}

void test_base::AudioAddSinkFailed()
{
    int error = 1;
    PV2WayUtil::OutputInfo("\n****** Test FAILED: add audio sink failed \n");
    test_is_true(false);
    OSCL_TRY(error, iAudioRemoveSinkId = iSourceAndSinks->RemoveAudioSink());

}

void test_base::AudioAddSourceFailed()
{
    int error = 1;
    PV2WayUtil::OutputInfo("\n****** Test FAILED: add audio source failed \n");
    test_is_true(false);
    OSCL_TRY(error, iAudioRemoveSourceId = iSourceAndSinks->RemoveAudioSource());
}

void test_base::VideoAddSinkSucceeded()
{
    iVideoSinkAdded = true;
    CheckForSucceeded();
}

void test_base::VideoAddSinkFailed()
{
    PV2WayUtil::OutputInfo("\n***** Test FAILED: add video sink failed  \n");
    test_is_true(false);
    disconnect();
}

void test_base::VideoAddSourceSucceeded()
{
    iVideoSourceAdded = true;
    CheckForSucceeded();
}

void test_base::VideoAddSourceFailed()
{
    PV2WayUtil::OutputInfo("\n***** Test FAILED: add video source failed \n");
    iVideoSourceAdded = false;
}

void test_base::AudioRemoveSourceCompleted()
{
    iAudioSourceAdded = false;
    PV2WayUtil::OutputInfo("Audio source removed \n");
    CheckForRemoved();
}

void test_base::AudioRemoveSinkCompleted()
{
    iAudioSinkAdded = false;
    PV2WayUtil::OutputInfo("Audio sink removed \n");
    CheckForRemoved();
}

void test_base::CheckForSucceeded()
{
    if (iAudioSourceAdded && iAudioSinkAdded &&
            iVideoSourceAdded && iVideoSinkAdded)
    {
        AllNodesAdded();
        AllAudioNodesAdded();
        AllVideoNodesAdded();
    }
    else if (iAudioSourceAdded && iAudioSinkAdded)
    {
        AllAudioNodesAdded();
    }
    else if (iVideoSourceAdded && iVideoSinkAdded)
    {
        AllVideoNodesAdded();
    }
}

void test_base::CheckForRemoved()
{
    if (!iAudioSourceAdded && !iAudioSinkAdded &&
            !iVideoSourceAdded && !iVideoSinkAdded)
    {
        AllNodesRemoved();
        AllAudioNodesRemoved();
        AllVideoNodesRemoved();
    }
    else if (!iAudioSourceAdded && !iAudioSinkAdded)
    {
        AllAudioNodesRemoved();
    }
    else if (!iVideoSourceAdded && !iVideoSinkAdded)
    {
        AllVideoNodesRemoved();
    }
}


void test_base::VideoRemoveSourceCompleted()
{
    iVideoSourceAdded = false;
    PV2WayUtil::OutputInfo("Video source removed \n");
    CheckForRemoved();
}

void test_base::VideoRemoveSinkCompleted()
{
    iVideoSinkAdded = false;
    PV2WayUtil::OutputInfo("Video sink removed \n");
    CheckForRemoved();
}


void test_base::CommandCompleted(const PVCmdResponse& aResponse)
{
    PVCommandId cmdId = aResponse.GetCmdId();
    if (cmdId < 0)
        return;

    if (iQueryInterfaceCmdId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            if (iTempH324MConfigIterface)
            {
                /*!

                  Step 6: Finish Querying Component

                  Step 6a: Receive notification that TSC node interface pointer has been set.
                  At this point we are notified that the TSC node interface pointer is valid.
                  Convert the pointer to H324MConfigInterface
                */
                iH324MConfig = OSCL_STATIC_CAST(H324MConfigInterface*, iTempH324MConfigIterface);
                iH324MConfig->addRef();
                iTempH324MConfigIterface->removeRef();
                iTempH324MConfigIterface = NULL;
                QueryInterfaceSucceeded();
            }

        }
    }
    else if (iInitCmdId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            InitSucceeded();
        }
        else if (aResponse.GetCmdStatus() == PVMFErrCancelled)
        {
            InitCancelled();
        }
        else
        {
            InitFailed();
        }
    }
    else if (iEncoderIFCommandId == cmdId)
    {
        if (aResponse.GetCmdStatus() != PVMFSuccess)
        {
            EncoderIFFailed();
        }
        else
        {
            EncoderIFSucceeded();
        }
    }
    else if (iConnectCmdId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            ConnectSucceeded();
        }
        else if (aResponse.GetCmdStatus() == PVMFErrCancelled)
        {
            ConnectCancelled();
        }
        else
        {
            ConnectFailed();
        }
    }
    else if (iDisCmdId == cmdId)
    {
        if (aResponse.GetCmdStatus() != PVMFSuccess)
        {
            DisCmdFailed();
        }
        else
        {
            DisCmdSucceeded();
        }
    }
    else if (iRstCmdId == cmdId)
    {
        RstCmdCompleted();
    }
    else if (iAudioAddSinkId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {

            AudioAddSinkSucceeded();

        }
        else
        {

            AudioAddSinkFailed();

        }
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iAudioAddSourceId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {

            AudioAddSourceSucceeded();
        }
        else
        {
            AudioAddSourceFailed();
        }
        iSourceAndSinks->CommandCompleted(aResponse);
    }

    else if (iAudioRemoveSourceId == cmdId)
    {
        AudioRemoveSourceCompleted();
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iAudioRemoveSinkId == cmdId)
    {
        AudioRemoveSinkCompleted();
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iVideoAddSinkId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            VideoAddSinkSucceeded();
        }
        else
        {
            VideoAddSinkFailed();
        }
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iVideoAddSourceId == cmdId)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            VideoAddSourceSucceeded();
        }
        else
        {
            VideoAddSourceFailed();
        }
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iVideoRemoveSourceId == cmdId)
    {
        VideoRemoveSourceCompleted();
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iVideoRemoveSinkId == cmdId)
    {
        VideoRemoveSinkCompleted();
        iSourceAndSinks->CommandCompleted(aResponse);
    }
    else if (iCancelCmdId == cmdId)
    {
        CancelCmdCompleted();
    }

}

void test_base::InitializeLogs()
{
    if (false == iUseProxy)
    {
        OSCL_HeapString<OsclMemAllocator> cfgfilename(PVLOG_PREPEND_CFG_FILENAME);
        cfgfilename += PVLOG_CFG_FILENAME;
        OSCL_HeapString<OsclMemAllocator> logfilename(PVLOG_PREPEND_OUT_FILENAME);
        logfilename += PVLOG_OUT_FILENAME;
        if (false == PVLoggerCfgFileParser::Parse(cfgfilename.get_str(), logfilename.get_str()))
        {
            PVLoggerCfgFileParser::SetupLogAppender(PVLoggerCfgFileParser::ePVLOG_APPENDER_FILE, logfilename.get_str());
        }
    }

}

void test_base::LetConnectionRun()
{
    timer->Request(iTimerConnectionID, iTimeoutConnectionInfo, iTimeConnection);
}

bool test_base::start_async_test()
{
    timer = OSCL_NEW(OsclTimer<OsclMemAllocator>, ("EngineUnitTests"));
    if (timer == NULL)
    {
        PV2WayUtil::OutputInfo("\n*************** Test FAILED: no timer *************** \n");
        test_is_true(false);
        return false;
    }

    timer->SetObserver(this);
    //timer->AddToScheduler();

    PV2WayUtil::OutputInfo("TimerID = %d, TestInfo = %d, MaxDur = %d\n", iTimerTestTimeoutID, iTimeoutTestInfo, iMaxTestDuration);
    // request the test to run up to
    timer->Request(iTimerTestTimeoutID, iTimeoutTestInfo, iMaxTestDuration);


    /*!

      Step 2: Create terminal
      Create proxy/non-proxy terminal using CPV2WayProxyFactory or CPV2WayEngineFactory
      @param PVCommandStatusObserver, PVInformationalEventObserver, PVErrorEventObserver to receive notifications from SDK
    */
    int error = 0;
    if (iUseProxy)
    {
        OSCL_TRY(error, terminal = CPV2WayProxyFactory::CreateTerminal(PV_324M,
                                   (PVCommandStatusObserver *) this,
                                   (PVInformationalEventObserver *) this,
                                   (PVErrorEventObserver *) this));
    }
    else
    {
        OSCL_TRY(error, terminal = CPV2WayEngineFactory::CreateTerminal(iSIP ? PV_SIP : PV_324M,
                                   (PVCommandStatusObserver *) this,
                                   (PVInformationalEventObserver *) this,
                                   (PVErrorEventObserver *) this));
    }

    if (error)
    {
        PV2WayUtil::OutputInfo("\n*************** Test FAILED: error creating terminal *************** \n");
        test_is_true(false);
        return false;
    }

    if (iSourceAndSinks && terminal)
    {
        iSourceAndSinks->SetTerminal(terminal);
    }
    /*!

      Step 3: Initialize logs
      Initialize logs, send parameters to the terminal
    */
    InitializeLogs();
    iInitCmdId = -1;
    iQueryInterfaceCmdId = -1;
    CreateParts();
    return true;
}

void test_base::CreateParts()
{
    /*!

      Step 4: Create Communication, H324 component
      Create communication node, query for the H324 component
      In this case the communication node is a loopback node.
    */
    create_comm();
    CreateH324Component();
}

void test_base::HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent)
{
    switch (aEvent.GetEventType())
    {
            /*!

              Step 9: Receive track indications
              Receive either PVT_INDICATION_OUTGOING_TRACK or PVT_INDICATION_INCOMING_TRACK
              through the PVInformationalEventObserver interface
            */
        case PVT_INDICATION_OUTGOING_TRACK:
        {
            TPVChannelId *channel_id = (TPVChannelId *)(&aEvent.GetLocalBuffer()[4]);
            PV2WayUtil::OutputInfo("\n\nIndication with logical channel #%d \n", *channel_id);
            if (aEvent.GetLocalBuffer()[0] == PV_AUDIO)
            {
                if (iUsingAudio)
                {
                    iAudioAddSourceId = iSourceAndSinks->HandleOutgoingAudio(aEvent);
                    if (iAudioAddSourceId)
                    {
                        iAudioSrcChannelId = channel_id;
                        PV2WayUtil::OutputInfo("Audio");
                    }
                    else
                    {
                        PV2WayUtil::OutputInfo("\nError adding handling outgoing audio track\n");
                    }
                }
            }

            else if (aEvent.GetLocalBuffer()[0] == PV_VIDEO)
            {
                if (iUsingVideo)
                {
                    iVideoAddSourceId = iSourceAndSinks->HandleOutgoingVideo(aEvent);
                    if (iVideoAddSourceId)
                    {
                        iVideoSrcChannelId = channel_id;
                        PV2WayUtil::OutputInfo("Video");
                    }
                    else
                    {
                        PV2WayUtil::OutputInfo("\nError adding handling outgoing video track\n");
                    }
                }
            }
            else
            {
                PV2WayUtil::OutputInfo("unknown\n");
            }
            PV2WayUtil::OutputInfo(" outgoing Track\n");
            break;
        }

        case PVT_INDICATION_INCOMING_TRACK:
        {
            TPVChannelId *channel_id = (TPVChannelId *)(&aEvent.GetLocalBuffer()[4]);
            PV2WayUtil::OutputInfo("\n\nIndication with logical channel #%d \n", *channel_id);
            if (aEvent.GetLocalBuffer()[0] == PV_AUDIO)
            {
                if (iUsingAudio)
                {
                    iAudioAddSinkId = iSourceAndSinks->HandleIncomingAudio(aEvent);
                    if (iAudioAddSinkId)
                    {
                        iAudioSnkChannelId = channel_id;
                        printf("Audio");
                    }
                    else
                    {
                        PV2WayUtil::OutputInfo("\nError adding handling incoming audio track \n");
                    }
                }
            }
            else if (aEvent.GetLocalBuffer()[0] == PV_VIDEO)
            {
                if (iUsingVideo)
                {
                    iVideoAddSinkId = iSourceAndSinks->HandleIncomingVideo(aEvent);
                    if (iVideoAddSinkId)
                    {
                        iVideoSnkChannelId = channel_id;
                        printf("Video");
                    }
                    else
                    {
                        PV2WayUtil::OutputInfo("\nError adding handling incoming video track\n");
                    }
                }
            }
            else
            {
                PV2WayUtil::OutputInfo("unknown\n");
            }
            PV2WayUtil::OutputInfo(" incoming Track\n");
            break;
        }

        case PVT_INDICATION_DISCONNECT:
            iAudioSourceAdded = false;
            iVideoSourceAdded = false;
            iAudioSinkAdded = false;
            iVideoSinkAdded = false;
            break;

        case PVT_INDICATION_CLOSE_TRACK:
            break;

        case PVT_INDICATION_INTERNAL_ERROR:
            break;

        default:
            break;
    }
}

void test_base::TestCompleted()
{
    char name[128];
    oscl_snprintf(name, 128, "Test %.2d: %s", iTestNum, iTestName.get_cstr());
    m_last_result.set_name(name);
    m_last_result.set_elapsed_time(OsclTickCount::TicksToMsec(OsclTickCount::TickCount()) - iStarttime);

    // Print out the result for this test case
    PV2WayUtil::OutputInfo("\nResults for Test Case %d:\n", iTestNum);
    PV2WayUtil::OutputInfo("Successes %.2d, Failures %d\n"
                           , m_last_result.success_count(), m_last_result.failures().size());
}

void test_base::TimeoutOccurred(int32 timerID, int32 timeoutInfo)
{
    PV2WayUtil::OutputInfo("\nTimeoutOccurred\n");
    if (timerID == iTimerConnectionID)
    {
        PV2WayUtil::OutputInfo("\nTimerConnectionID \n");
        // if the timerConnection status has gone off
        // cancel other timer
        timer->Cancel(iTimerTestTimeoutID);
        // derived classes may have complicated
        // FinishTimerCallbacks.  This one just happens to be the same
        // as when we fail- to just cleanup.
        FinishTimerCallback();
    }
    else if (timerID == iTimerTestTimeoutID)
    {
        timer->Cancel(iTimerConnectionID);
        PV2WayUtil::OutputInfo("\n Test ran out of time.  Max time: %d \n", iMaxTestDuration);
        test_is_true(false);
        // cleanup because we are unsuccessful
        FinishTimerCallback();
    }
    else
    {
        // some other failure
        PV2WayUtil::OutputInfo("\n Failure.  Cleaning up \n");
        test_is_true(false);
        // cleanup because we are unsuccessful
        FinishTimerCallback();
    }
}

