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

#ifndef PVLOGGER_STDERR_APPENDER_H_INCLUDED
#include "pvlogger_stderr_appender.h"
#endif

#ifndef PVLOGGER_TIME_AND_ID_LAYOUT_H_INCLUDED
#include "pvlogger_time_and_id_layout.h"
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

#ifndef PVLOGGER_MEM_APPENDER_H_INCLUDED
#include "pvlogger_mem_appender.h"
#endif

#ifndef PVLOGGER_FILE_APPENDER_H_INCLUDED
#include "pvlogger_file_appender.h"
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

template<class DestructClass>
class LogAppenderDestructDealloc : public OsclDestructDealloc
{
    public:
        virtual void destruct_and_dealloc(OsclAny *ptr)
        {
            delete((DestructClass*)ptr);
        }
};

//This function is used to read the contents of a file, one line at a time.
int fgetline(Oscl_File* aFp, char aLine[], int aMax);

class PVLoggerConfigFile
{
        /*  To change the logging settings without the need to compile the test application
            Let us read the logging settings from the file instead of hard coding them over here
            The name of the config file is pvlogger.ini
            The format of entries in it is like
            First entry will decide if the file appender has to be used or error appender will be used.
            0 -> ErrAppender will be used
            1 -> File Appender will be used
            2 -> Mem Appender will be used
            Entries after this will decide the module whose logging has to be taken.For example, contents of one sample config file could be
            1
            1,PVMetadataEngine
            (pls note that no space is allowed between loglevel and logger tag)
            This means, we intend to have logging of level 1 for the module PVMetadataEngine
            on file.
        */
    public:

        uint32 iAppenderType; //Type of appender to be used for the logging 0-> Err Appender, 1-> File Appender
        Oscl_Vector<LoggerConfigElement, OsclMemAllocator> iLoggerConfigElements;
        OSCL_wHeapString<OsclMemAllocator> logfilename;

        PVLoggerConfigFile(): iLogFileRead(false)
        {
            iFileServer.Connect();
            // Full path of pvlogger.ini is: SOURCENAME_PREPEND_STRING + pvlogger.ini
            oscl_strncpy(iLogFileName, SOURCENAME_PREPEND_STRING,
                         oscl_strlen(SOURCENAME_PREPEND_STRING) + 1);
            oscl_strcat(iLogFileName, "pvlogger.ini");
            oscl_memset(ibuffer, 0, sizeof(ibuffer));
            iAppenderType = 0;

        }

        ~PVLoggerConfigFile()
        {
            iFileServer.Close();
        }

        bool get_next_line(const char *start_ptr, const char * end_ptr,
                           const char *& line_start,
                           const char *& line_end);

        bool IsLoggerConfigFilePresent();

        int8 ReadAndParseLoggerConfigFile();

        void SetLoggerSettings();

    private:

        bool iLogFileRead;
        Oscl_File iLogFile;
        Oscl_FileServer iFileServer;
        char iLogFileName[255];
        char ibuffer[1024];

};


class pvme_test_suite : public test_case
{
    public:
        pvme_test_suite(int32 aFirstTest, int32 aLastTest, int32 aLogLevel);
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
        pvmetadataengine_test(int32 aFirstTest, int32 aLastTest, int32 aLogLevel);
        ~pvmetadataengine_test();

        enum PVMetadataEngineTests
        {
            GetSourceMetadataNonThreadedModeTest = 0,

            GetSourceMetadataThreadedModeTest = GetSourceMetadataNonThreadedModeTest + 1,

            LastTest = GetSourceMetadataThreadedModeTest + 1,//placeholder

            BeyondLastTest = 999 //placeholder

        };

        // From test_case
        virtual void test();

        // From pvmetadataengine_test_observer
        void TestCompleted(test_case&);

        void SetupLoggerScheduler();

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

        // For logging
        int32 iLogLevel;
        PVLoggerConfigFile obj;

        // For memory statistics
        uint32 iTotalAlloc;
        uint32 iTotalBytes;
        uint32 iAllocFails;
        uint32 iNumAllocs;
};

#endif
