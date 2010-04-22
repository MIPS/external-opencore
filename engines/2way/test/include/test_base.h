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
#define TEST_BASE_H_INCLUDED

#ifndef TEST_ENGINE_H_INCLUDED
#include "test_engine.h"
#endif


#ifndef PV_2WAY_PROXY_FACTORY_H_INCLUDED
#include "pv_2way_proxy_factory.h"
#endif

#ifndef TEST_UTILITY_H_INCLUDED
#include "test_utility.h"
#endif

#define MAX_TEST_DURATION 60    // in seconds

#define TEST_DURATION 3         // in seconds

class test_base : public engine_test,
        public H324MConfigObserver
{
    public:

        test_base(bool aUseProxy = false,
                  uint32 aTimeConnection = TEST_DURATION,
                  uint32 aMaxTestDuration = MAX_TEST_DURATION,
                  int aMaxRuns = 1,
                  bool isSIP = false) :
                engine_test(aUseProxy, aMaxRuns),
                iH324MConfig(NULL),
                iComponentInterface(NULL),
                iEncoderIFCommandId(-1),
                iCancelCmdId(-1),
                iQueryInterfaceCmdId(-1),
                iStackIFSet(false),
                iEncoderIFSet(false),
                iSIP(isSIP),
                iTempH324MConfigIterface(NULL),
                iSourceAndSinks(NULL),
                iUsingAudio(false),
                iUsingVideo(false),
                iMaxTestDuration(aMaxTestDuration),
                iTimeConnection(aTimeConnection),
                iTimerConnectionID(1),
                iTimerTestTimeoutID(2),
                iTimeoutConnectionInfo(0),
                iTimeoutTestInfo(0),
                iStarttime(0)
        {
            iMaxTestDuration += aTimeConnection;
            iTestNum = iTestCounter;
            test_base::iTestCounter++;
            iTestName = _STRLIT_CHAR("unnamed test");
        }

        virtual ~test_base()
        {
            CancelTimers();
            cleanup();
            if (iSourceAndSinks)
            {
                OSCL_DELETE(iSourceAndSinks);
            }
        }
        void AddSourceAndSinks(PV2WaySourceAndSinksBase* aSourceAndSinks)
        {
            iSourceAndSinks = aSourceAndSinks;
            if (iSourceAndSinks && terminal)
            {
                iSourceAndSinks->SetTerminal(terminal);
            }
        }
        void StartTimer()
        {
        }
        static uint32 GetTestCounter()
        {
            return test_base::iTestCounter;
        }
        void cleanup()
        {
            if (iH324MConfig)
            {
                iH324MConfig->removeRef();
                iH324MConfig = NULL;
            }
            if (iComponentInterface)
            {
                iComponentInterface->removeRef();
                iComponentInterface = NULL;
            }
            if (iTempH324MConfigIterface)
            {
                iTempH324MConfigIterface->removeRef();
                iTempH324MConfigIterface = NULL;
            }
            engine_test::cleanup();
        }
        void LetConnectionRun();

        void test();

        virtual void Run();

        virtual void AllAudioNodesAdded() {};
        virtual void AllNodesAdded() {};
        virtual void AllVideoNodesAdded() {};
        virtual void AllAudioNodesRemoved() {};
        virtual void AllNodesRemoved() {};
        virtual void AllVideoNodesRemoved() {};
    protected:
        void CancelTimers();

        void InitializeLogs();
        void HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent);
        void TestCompleted();

        virtual bool Init();
        virtual void CommandCompleted(const PVCmdResponse& aResponse);

        void CreateH324Component(bool aCreateH324 = true);

        void H324MConfigCommandCompletedL(PVMFCmdResp& aResponse);
        void H324MConfigHandleInformationalEventL(PVMFAsyncEvent& aNotification);

        virtual void QueryInterfaceSucceeded();

        //------------------- Functions overridden in test classes for specific behavior------
        virtual bool start_async_test();

        virtual void InitSucceeded();
        virtual void InitFailed();
        virtual void InitCancelled();

        virtual void ConnectSucceeded();
        virtual void ConnectFailed();
        virtual void ConnectCancelled();

        virtual void CancelCmdCompleted();


        virtual void RstCmdCompleted();
        virtual void DisCmdSucceeded();
        virtual void DisCmdFailed();

        virtual void EncoderIFSucceeded();
        virtual void EncoderIFFailed();

        virtual void TimeoutOccurred(int32 timerID, int32 timeoutInfo);
        virtual void FinishTimerCallback() {};


        void CheckForSucceeded();
        virtual void CheckForRemoved();
        // audio
        virtual void AudioAddSinkSucceeded();
        virtual void AudioAddSinkFailed();
        virtual void AudioAddSourceSucceeded();
        virtual void AudioAddSourceFailed();
        virtual void AudioRemoveSourceCompleted();
        virtual void AudioRemoveSinkCompleted();

        // video
        virtual void VideoAddSinkSucceeded();
        virtual void VideoAddSinkFailed();
        virtual void VideoAddSourceSucceeded();
        virtual void VideoAddSourceFailed();
        virtual void VideoRemoveSourceCompleted();
        virtual void VideoRemoveSinkCompleted();

        virtual void CreateParts();

        //------------------- END Functions overridden in test classes for specific behavior------

        H324MConfigInterface* iH324MConfig;
        PVInterface *iComponentInterface;
        PVCommandId iEncoderIFCommandId;
        PVCommandId iCancelCmdId;
        PVCommandId iQueryInterfaceCmdId;

        bool iStackIFSet;
        bool iEncoderIFSet;
        bool iSIP;

        PVMFFormatType iAudSrcFormatType, iAudSinkFormatType;
        PVMFFormatType iVidSrcFormatType, iVidSinkFormatType;
        PVInterface* iTempH324MConfigIterface;

        PV2WaySourceAndSinksBase* iSourceAndSinks;

        bool iUsingAudio;
        bool iUsingVideo;
        static uint32 iTestCounter;
        int iTestNum;
        OSCL_HeapString<OsclMemAllocator> iTestName;
        uint32 iMaxTestDuration;
        uint32 iTimeConnection;
        int32 iTimerConnectionID;
        int32 iTimerTestTimeoutID;
        int32 iTimeoutConnectionInfo;
        int32 iTimeoutTestInfo;
        uint32 iStarttime;
};


#endif


