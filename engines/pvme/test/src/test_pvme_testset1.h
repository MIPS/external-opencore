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
#ifndef TEST_PV_METADATA_ENGINE_TESTSET1_H_INCLUDED
#define TEST_PV_METADATA_ENGINE_TESTSET1_H_INCLUDED

/**
 *  @file test_pvme_testset1.h
 *  @brief This file contains the class definitions for the first set of
 *         test cases for pvMetadataEngine
 *
 */

#include "test_pvme.h"


#ifndef PV_PLAYER_DATASOURCEURL_H_INCLUDED
#include "pv_player_datasourceurl.h"
#endif

#ifndef PVMF_SOURCE_CONTEXT_DATA_H_INCLUDED
#include "pvmf_source_context_data.h"
#endif

#ifndef PV_ENGINE_TYPES_H_INCLUDED
#include "pv_engine_types.h"
#endif

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

#ifndef PVMI_CONFIG_AND_CAPABILITY_H_INCLUDED
#include "pvmi_config_and_capability.h"
#endif

#ifndef OSCL_ERROR_CODES_H_INCLUDED
#include "oscl_error_codes.h"
#endif

#ifndef OSCL_TICKCOUNT_H_INCLUDED
#include "oscl_tickcount.h"
#endif

#ifndef PVMI_KVP_H_INCLUDED
#include "pvmi_kvp.h"
#endif

#ifndef PVMI_KVP_UTIL_H_INCLUDED
#include "pvmi_kvp_util.h"
#endif

#ifndef PVMF_ERRORINFOMESSAGE_EXTENSION_H_INCLUDED
#include "pvmf_errorinfomessage_extension.h"
#endif

#ifndef PV_ENGINE_OBSERVER_H_INCLUDED
#include "pv_engine_observer.h"
#endif

#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif

#ifndef PVLOGGER_CFG_FILE_PARSER_H_INCLUDED
#include "pvlogger_cfg_file_parser.h"
#endif

class pvmetadataengine_test_observer;

extern FILE* file;
#define MAX_LEN 100



/*!
 *  A test case to test pvme sequence of opening a specified source and extracting
 *  metadata
 *  - Data Source: Passed in parameter
 *  - Output: Metadata[test_pvme_getmetadata_[SRCFILENAME]_metadata.txt]\n
 *  - Sequence:
 *             -# CreatePVMetadataEngine()
 *             -# Init()
 *             -# SetMetadataKeys()
 *             -# GetMetadata()
 *             -# Reset()
 *             -# DeletePVMetadataEngine()
 *
 */
class pv_metadata_engine_test
        : public OsclTimerObject
        , public PVCommandStatusObserver
        , public PVInformationalEventObserver
        , public PVErrorEventObserver
        , public ThreadSafeQueueObserver
{
    public:
        pv_metadata_engine_test
        (
            PVMetadataEngineTestParam a_testParam,
            PVMetadataEngineThreadMode a_mode,
            PVLoggerCfgFileParser::eAppenderType_t a_appenderType,
            Oscl_Vector<PVLoggerCfgFileParser::LogCfgElement, OsclMemAllocator> a_vectorLogNodeCfg = 0
        )
                : OsclTimerObject(OsclActiveObject::EPriorityNominal, "PVMetadataEngineTest")
                , iDataSource(0)
                , iCurrentCmdId(0)
                , iTestCaseName(_STRLIT_CHAR("Get Metadata"))
                , AverageGetMetaDataTimeInMS(0)
                , MaxGetMetaDataTime(0)
                , MinGetMetaDataTime(0)
                , numOfClips(0)
                , metadataForFirstClip(true)
                , iClipFilePresent(false)
                , iMode(a_mode)
                , iAppenderType(a_appenderType)
                , iVectorLogNodeCfg(a_vectorLogNodeCfg)
                , iObserver(a_testParam.iObserver)
                , iTestCase(a_testParam.iTestCase)
                , iTestMsgOutputFile(a_testParam.iTestMsgOutputFile)

        {
            OSCL_ASSERT(0 != iObserver);
            OSCL_ASSERT(0 != iTestCase);

            // Initialize the variables to use for context data testing
            iContextObjectRefValue = 0x5C7A; // some random number
            iContextObject = iContextObjectRefValue;
        }

        ~pv_metadata_engine_test()
        {
            for (it = iClips.begin(); it < iClips.end(); ++it)
            {
                oscl_free(*it);
            }
        }

        void StartTest();
        void Run();

        void CommandCompleted(const PVCmdResponse& aResponse);
        void HandleErrorEvent(const PVAsyncErrorEvent& aEvent);
        void HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent);

        // Utility function to retrieve the filename from string and replace ',' with '_'
        void RetrieveFilename
        (
            const oscl_wchar* a_pwszSource,
            OSCL_wHeapString<OsclMemAllocator>& a_filename
        )
        {
            if (0 == a_pwszSource)
                return;

            // Find the last '\' or '/' in the string
            oscl_wchar* lastslash = (oscl_wchar*)a_pwszSource;
            while (true)
            {
                const oscl_wchar* p = oscl_strstr(lastslash, _STRLIT_WCHAR("\\"));
                if (0 != p)
                {
                    lastslash = (oscl_wchar*)p + 1;
                    continue;
                }

                p = oscl_strstr(lastslash, _STRLIT_WCHAR("/"));
                if (0 != p)
                {
                    lastslash = (oscl_wchar*)p + 1;
                    continue;
                }
                break;
            }

            if (lastslash)
                a_filename = lastslash; // copy the filename

            while (true) // replace each '.' in filename with '_'
            {
                const oscl_wchar* tmp = oscl_strstr(a_filename.get_cstr(), _STRLIT_WCHAR("."));
                if (0 != tmp)
                {
                    oscl_strncpy((oscl_wchar*)tmp, _STRLIT_WCHAR("_"), 1);
                    continue;
                }
                break;
            }
        }

        enum PVMETestState
        {
            STATE_CREATE,
            STATE_INIT,
            STATE_SETMETADATAKEYS,
            STATE_GETMETADATA,
            STATE_RESET,
            STATE_DELETE
        };

        int32                                   iNumValues;
        PVMETestState                           iState;
        PVMetadataEngineInterfaceContainer      iPVMEContainer;
        PVPlayerDataSourceURL*                  iDataSource;
        PVCommandId                             iCurrentCmdId;
        PVPMetadataList                         iMetadataKeyList;
        Oscl_Vector<PvmiKvp, OsclMemAllocator>  iMetadataValueList;
        OSCL_HeapString<OsclMemAllocator>       iTestCaseName;

    protected:

        void ReadMetadataFile();
        void ReadClipsFile();
        void GetSourceFormatType(char* aFileName, PVMFFormatType& aInputFileFormatType);
        void PrintSupportedMetaDataKeys();
        void ThreadSafeQueueDataAvailable(ThreadSafeQueue*);

        OSCL_wHeapString<OsclMemAllocator> wFileName;
        Oscl_FileServer iFS;
        Oscl_File iMetadataFile;
        char iTextOutputBuf[512];

        uint32 iContextObject;
        uint32 iContextObjectRefValue;
        uint32 start_time;
        uint32 end_time;
        uint32 AverageGetMetaDataTimeInMS;
        uint32 MaxGetMetaDataTime;
        uint32 MinGetMetaDataTime;
        int32  numOfClips;

        bool metadataForFirstClip;
        bool iClipFilePresent;

        PVMetadataEngineThreadMode iMode; // threaded or non-Threaded Mode
        PVLoggerCfgFileParser::eAppenderType_t iAppenderType;
        Oscl_Vector<PVLoggerCfgFileParser::LogCfgElement, OsclMemAllocator> iVectorLogNodeCfg;

        Oscl_Vector<char*, OsclMemAllocator> iClips; // to store the clips
        Oscl_Vector<char*, OsclMemAllocator>::iterator it;

        PVLogger* iLogger; // handle to the logger node
        PVLogger* iPerfLogger;

        pvmetadataengine_test_observer* iObserver;
        test_case* iTestCase;
        FILE* iTestMsgOutputFile;
        char* iFileName;
        oscl_wchar iTempWCharBuf[512];
        PVMFFormatType iFileType;
        ThreadSafeQueue iThreadSafeCommandQueue;
        ThreadSafeQueue iThreadSafeErrorQueue;
        ThreadSafeQueue iThreadSafeInfoQueue;
};

#endif      //TEST_PV_METADATA_ENGINE_TESTSET1_H_INCLUDED
