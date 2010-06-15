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
#ifndef TEST_PV_METADATA_ENGINE_H_INCLUDED
#define TEST_PV_METADATA_ENGINE_H_INCLUDED

#ifndef TEST_CASE_H_INCLUDED
#include "test_case.h"
#endif

#ifndef TEXT_TEST_INTERPRETER_H_INCLUDED
#include "text_test_interpreter.h"
#endif

#ifndef OSCL_EXCEPTION_H_INCLUDE
#include "oscl_exception.h"
#endif

#ifndef PVMF_METADATA_ENGINE_FACTORY_H_INCLUDED
#include "pv_metadata_engine_factory.h"
#endif

#ifndef PVMF_METADATA_ENGINE_INTERFACE_H_INCLUDED
#include "pv_metadata_engine_interface.h"
#endif

#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#ifndef OSCL_UTF8CONV_H_INCLUDED
#include "oscl_utf8conv.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef __UNIT_TEST_TEST_ARGS__
#include "unit_test_args.h"
#endif

#ifndef OSCL_STRING_UTILS_H_INCLUDED
#include "oscl_string_utils.h"
#endif

#ifndef OSCL_MEM_AUDIT_H_INCLUDED
#include "oscl_mem_audit.h"
#endif

#ifndef OSCL_ERROR_H_INCLUDED
#include "oscl_error.h"
#endif

#ifndef OSCL_SCHEDULER_H_INCLUDED
#include "oscl_scheduler.h"
#endif

#ifndef TEST_PVME_CONFIG_H_INCLUDED
#include "test_pvme_config.h"
#endif

class pv_metadata_engine_test;

//This function is used to read the contents of a file, one line at a time.
int fgetline(Oscl_File* aFp, char aLine[], int aMax);


class pvme_test_suite : public test_case
{
    public:
        pvme_test_suite(int32 aFirstTest, int32 aLastTest);
};

#define PVMEATB_TEST_IS_TRUE(condition) (iTestCase->test_is_true_stub( (condition), (#condition), __FILE__, __LINE__ ))

// Observer class to notify completion of test
class pvmetadataengine_test_observer
{
    public:
        virtual ~pvmetadataengine_test_observer() {};
        // Signals completion of test. Test instance can be deleted after this callback completes.
        virtual void TestCompleted(test_case &) = 0;
};

typedef struct
{
    pvmetadataengine_test_observer* iObserver;
    test_case* iTestCase;
    FILE* iTestMsgOutputFile;
} PVMetadataEngineTestParam;

class pvmetadataengine_test : public test_case,
        public pvmetadataengine_test_observer
{
    public:
        pvmetadataengine_test(int32 aFirstTest, int32 aLastTest);
        ~pvmetadataengine_test();

        enum PVMetadataEngineTests
        {
            GetSourceMetadataNonThreadedModeTest = 0,

            GetSourceMetadataThreadedModeTest = GetSourceMetadataNonThreadedModeTest + 1,

            ProtectedMetadataNonThreadedModeTest = GetSourceMetadataThreadedModeTest + 1, //Hardcoded test content -- Janus_playcount_3.wma

            ProtectedMetadataWithPlayReadyContentNonThreadedModeTest = ProtectedMetadataNonThreadedModeTest + 1, //Hardcoded PlayReady test content -- ExpireImmediately.pya

            ProtectedMetadataThreadedModeTest = ProtectedMetadataWithPlayReadyContentNonThreadedModeTest + 1, //Hardcoded test content -- Janus_playcount_3.wma

            ProtectedMetadataWithPlayReadyContentThreadedModeTest = ProtectedMetadataThreadedModeTest + 1, //Hardcoded PlayReady test content -- ExpireImmediately.pya

            LastTest = ProtectedMetadataWithPlayReadyContentThreadedModeTest + 1,//placeholder

            BeyondLastTest = 999 //placeholder

        };

        // From test_case
        virtual void test();

        // From pvmetadataengine_test_observer
        void TestCompleted(test_case&);

    private:
        char *iFileName;
        PVMFFormatType iFileType;

        int iCurrentTestNumber;
        pv_metadata_engine_test* iCurrentTest;
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
