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
#ifndef TEST_PV_FRAME_METADATA_UTILITY_H_INCLUDED
#define TEST_PV_FRAME_METADATA_UTILITY_H_INCLUDED

#ifndef TEST_CASE_H_INCLUDED
#include "test_case.h"
#endif

#ifndef TEXT_TEST_INTERPRETER_H_INCLUDED
#include "text_test_interpreter.h"
#endif

#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif

#ifndef OSCL_EXCEPTION_H_INCLUDE
#include "oscl_exception.h"
#endif

#ifndef PV_FRAME_METADATA_FACTORY_H_INCLUDED
#include "pv_frame_metadata_factory.h"
#endif

#ifndef PV_FRAME_METADATA_INTERFACE_H_INCLUDED
#include "pv_frame_metadata_interface.h"
#endif

#ifndef PV_ENGINE_OBSERVER_H_INCLUDED
#include "pv_engine_observer.h"
#endif

#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#ifndef PVLOGGER_TIME_AND_ID_LAYOUT_H_INCLUDED
#include "pvlogger_time_and_id_layout.h"
#endif

#ifndef OSCL_UTF8CONV_H_INCLUDED
#include "oscl_utf8conv.h"
#endif

#ifndef TEST_PV_FRAME_METADATA_UTILITY_CONFIG_H_INCLUDED
#include "test_pv_frame_metadata_utility_config.h"
#endif

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

class pvframemetadata_utility_test_suite : public test_case
{
    public:
        pvframemetadata_utility_test_suite(char *aFilename, PVMFFormatType aFiletype, int32 aFirstTest, int32 aLastTest);
};


// Observer class for pvFrameMetadata async test to notify completion of test
class pvframemetadata_async_test_observer
{
    public:
        virtual ~pvframemetadata_async_test_observer() {};
        // Signals completion of test. Test instance can be deleted after this callback completes.
        virtual void TestCompleted(test_case &) = 0;
};


typedef struct
{
    pvframemetadata_async_test_observer* iObserver;
    test_case* iTestCase;
    FILE* iTestMsgOutputFile;
    char* iFileName;
    PVMFFormatType iFileType;
    PVMFFormatType iOutputFrameType;
} PVFrameMetadataAsyncTestParam;

#define PVFMUATB_TEST_IS_TRUE(condition) (iTestCase->test_is_true_stub( (condition), (#condition), __FILE__, __LINE__ ))

// The base class for all pvframemetadata utility asynchronous tests
class pvframemetadata_async_test_base : public OsclTimerObject,
        public PVCommandStatusObserver,
        public PVInformationalEventObserver,
        public PVErrorEventObserver
{
    public:
        pvframemetadata_async_test_base(PVFrameMetadataAsyncTestParam aTestParam) :
                OsclTimerObject(OsclActiveObject::EPriorityNominal, "PVFrameMetadataUtilityAsyncTestBase")
        {
            OSCL_ASSERT(aTestParam.iObserver != NULL);
            OSCL_ASSERT(aTestParam.iTestCase != NULL);
            iObserver = aTestParam.iObserver;
            iTestCase = aTestParam.iTestCase;
            iTestMsgOutputFile = aTestParam.iTestMsgOutputFile;
            iFileName = aTestParam.iFileName;
            iFileType = aTestParam.iFileType;
            iOutputFrameType = aTestParam.iOutputFrameType;
            iOutputFrameTypeString = iOutputFrameType.getMIMEStrPtr();
            oscl_UTF8ToUnicode(iOutputFrameTypeString.get_str(), iOutputFrameTypeString.get_size(), iTempWCharBuf, 512);
            iOutputFrameTypeWString.set(iTempWCharBuf, oscl_strlen(iTempWCharBuf));

            iTestCaseName = _STRLIT_CHAR(" ");

            // Initialize the variables to use for context data testing
            iContextObjectRefValue = 0x5C7A; // some random number
            iContextObject = iContextObjectRefValue;
        }

        virtual ~pvframemetadata_async_test_base() {}

        virtual void StartTest() = 0;

        virtual void CommandCompleted(const PVCmdResponse& /*aResponse*/) {}
        virtual void HandleErrorEvent(const PVAsyncErrorEvent& /*aEvent*/) {}
        virtual void HandleInformationalEvent(const PVAsyncInformationalEvent& /*aEvent*/) {}

        // Utility function to retrieve the filename from string and replace ',' with '_'
        void RetrieveFilename(const oscl_wchar* aSource, OSCL_wHeapString<OsclMemAllocator>& aFilename)
        {
            if (aSource == NULL)
            {
                return;
            }

            // Find the last '\' or '/' in the string
            oscl_wchar* lastslash = (oscl_wchar*)aSource;
            bool foundlastslash = false;
            while (!foundlastslash)
            {
                const oscl_wchar* tmp1 = oscl_strstr(lastslash, _STRLIT_WCHAR("\\"));
                const oscl_wchar* tmp2 = oscl_strstr(lastslash, _STRLIT_WCHAR("/"));
                if (tmp1 != NULL)
                {
                    lastslash = (oscl_wchar*)tmp1 + 1;
                }
                else if (tmp2 != NULL)
                {
                    lastslash = (oscl_wchar*)tmp2 + 1;
                }
                else
                {
                    foundlastslash = true;
                }
            }

            // Now copy the filename
            if (lastslash)
            {
                aFilename = lastslash;
            }

            // Replace each '.' in filename with '_'
            bool finishedreplace = false;
            while (!finishedreplace)
            {
                const oscl_wchar* tmp = oscl_strstr(aFilename.get_cstr(), _STRLIT_WCHAR("."));
                if (tmp != NULL)
                {
                    oscl_strncpy((oscl_wchar*)tmp, _STRLIT_WCHAR("_"), 1);
                }
                else
                {
                    finishedreplace = true;
                }
            }
        }
        virtual void SaveVideoFrame(Oscl_File& frameFile, uint8* frameBuffer, uint32 frameBufferSize);
        virtual void SaveMetadataInfo(char* outputBuf, Oscl_Vector<PvmiKvp, OsclMemAllocator>& valueList, Oscl_File& metadataFile);

        pvframemetadata_async_test_observer* iObserver;
        test_case* iTestCase;
        FILE* iTestMsgOutputFile;
        char *iFileName;
        oscl_wchar iTempWCharBuf[512];
        PVMFFormatType iFileType;
        PVMFFormatType iOutputFrameType;
        OSCL_HeapString<OsclMemAllocator> iOutputFrameTypeString;
        OSCL_wHeapString<OsclMemAllocator> iOutputFrameTypeWString;

        OSCL_HeapString<OsclMemAllocator> iTestCaseName;

        uint32 iContextObject;
        uint32 iContextObjectRefValue;
};


// test_base-based class which will run async tests on pvFrameMetadata utility
class pvframemetadata_utility_test : public test_case,
        public pvframemetadata_async_test_observer
{
    public:
        pvframemetadata_utility_test(char *aFileName, PVMFFormatType aFileType, int32 aFirstTest, int32 aLastTest);
        ~pvframemetadata_utility_test();

        enum PVFrameMetadataUtilityAsyncTests
        {
            NewDeleteTest = 0,
            GetSourceMetadataTest,
            GetSourceMetadataandFrameTest,
            GetSourceMetadataandBestFrameTest,
            GetAllMetadataTest, //4

            GetFirstFrameYUV420AndMetadataTest, //5
            GetFirstFrameYUV420UtilityBufferTest,
            GetFirstFrameRGB12AndMetadataTest,
            GetFirstFrameRGB12UtilityBufferTest,
            GetFirstFrameRGB16AndMetadataTest,
            GetFirstFrameRGB16UtilityBufferTest,
            GetFirstFrameRGB24AndMetadataTest,
            GetFirstFrameRGB24UtilityBufferTest, //12

            Get30thFrameTest, //13
            Get10secFrameTest,
            CancelCommandTest, //15

            MultipleGetFirstFrameAndMetadataTest, //16
            MultipleGetFramesYUVTest,
            MultipleGetFramesRGB24Test,
            InvalidSourceFileTest,
            NoGetFrameTest, //20
            NoVideoTrackTest,
            ProtectedMetadataTest, //Hardcoded test content -- Janus_playcount_3.wma
            ProtectedMetadataWithPlayReadyContentTest, //Hardcoded PlayReady test content -- ExpireImmediately.pya
            ProtectedThumbnailTest_WithThumbnailIntent, //Hardcoded test content -- stc10v.wmv
            ProtectedThumbnailTest_WithPlayIntent, //Hardcoded test content -- stc10v.wmv
            ProtectedThumbnailTest_WithMetadataIntent, //Hardcoded test content -- stc10v.wmv
            SetTimeoutAndGetFrameTest,
            SetPlayerKeyTest,

            LastTest, //placeholder
            BeyondLastTest = 999 //placeholder
        };

        // From test_case
        virtual void test();

        // From pvframemetadata_async_test_observer
        void TestCompleted(test_case&);

        void SetupLoggerScheduler();

    private:
        char *iFileName;
        PVMFFormatType iFileType;

        int iCurrentTestNumber;
        pvframemetadata_async_test_base* iCurrentTest;
        int32 iFirstTest;
        int32 iLastTest;

        // For test results
        int iTotalSuccess;
        int iTotalError;
        int iTotalFail;

        // For memory statistics
        uint32 iTotalAlloc;
        uint32 iTotalBytes;
        uint32 iAllocFails;
        uint32 iNumAllocs;
};

#endif


