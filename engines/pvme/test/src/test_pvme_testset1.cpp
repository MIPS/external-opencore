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
#ifndef TEST_PV_METADATA_ENGINE_TESTSET1_H_INCLUDED
#include "test_pvme_testset1.h"
#endif


//
// pv_metadata_engine_test section
//
void pv_metadata_engine_test::StartTest()
{
    AddToScheduler();
    iState = STATE_CREATE;

    // Retrieve the logger object
    iLogger = PVLogger::GetLoggerObject("PVMetadataEngineTest");
    iPerfLogger = PVLogger::GetLoggerObject("pvmetestdiagnostics");

    RunIfNotReady();
}


void pv_metadata_engine_test::Run()
{
    int error = 0;

    switch (iState)
    {
        case STATE_CREATE:
        {
            iPVMEContainer.iCmdStatusObserver = this;
            iPVMEContainer.iInfoEventObserver = this;
            iPVMEContainer.iErrorEventObserver = this;
            iPVMEContainer.iMode = iMode;

            if (iPVMEContainer.iMode == PV_METADATA_ENGINE_THREADED_MODE)
            {
                //Configure the threadsafe queues that are used for engine
                //thread callbacks.
                iThreadSafeCommandQueue.Configure(this);
                iThreadSafeErrorQueue.Configure(this);
                iThreadSafeInfoQueue.Configure(this);

                iPVMEContainer.iSem.Create();
                iPVMEContainer.iAppenderType = iAppenderType;
                iPVMEContainer.iLogfilename += PVLOG_PREPEND_OUT_FILENAME;
                iPVMEContainer.iLogfilename += _STRLIT_CHAR("pvme.log");
                iPVMEContainer.iVectorLogNodeCfg = iVectorLogNodeCfg;
            }

            OSCL_TRY(error, PVMetadataEngineFactory::CreatePVMetadataEngine(iPVMEContainer));

            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::Create Called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

            if (error || iPVMEContainer.iPVMEInterface == NULL)
            {
                PVMEATB_TEST_IS_TRUE(false);
                iObserver->TestCompleted(*iTestCase);
            }
            else
            {
                iState = STATE_INIT;
                RunIfNotReady();
            }
        }
        break;


        case STATE_INIT:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::Init Issued ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

            iCurrentCmdId = iPVMEContainer.iPVMEInterface->Init((OsclAny*) & iContextObject);

        }
        break;

        case STATE_SETMETADATAKEYS:
        {
            ReadMetadataFile();
            ReadClipsFile();

            if (iClipFilePresent == false)
            {
                fprintf(file, "\nClip File Not Present \n");
                iState = STATE_RESET;
                RunIfNotReady();
            }

            else if (iMetadataKeyList.size() != 0)
            {
                PVMFStatus status;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                                (0, "PVMetadataEngineTest::SetMetadataKeys Issued ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
                status = iPVMEContainer.iPVMEInterface->SetMetaDataKeys(iMetadataKeyList);

                if (status == PVMFErrInvalidState)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                                    (0, "PVMetadataEngineTest::SetMetadataKeys called in wrong state"));
                }
                else
                {
                    iState = STATE_GETMETADATA;
                    RunIfNotReady();
                }
            }

            else
            {
                iState = STATE_GETMETADATA;
                RunIfNotReady();
            }
        }
        break;

        case STATE_GETMETADATA:
        {
            //If GetMetadata is being called second time in MultipleGetMetadataTest, we don't want these statements to execute again
            if (iDataSource == NULL)
            {
                // Create a player data source and add it
                iDataSource = new PVPlayerDataSourceURL;
            }

            OSCL_wHeapString<OsclMemAllocator> inputfilename;

            // Convert the source file name to UCS2 and extract the filename part
            PVMFFormatType iFileType;
            iFileName = *it;
            it++;
            GetSourceFormatType(iFileName, iFileType);

            oscl_UTF8ToUnicode(iFileName, oscl_strlen(iFileName), iTempWCharBuf, 512);
            wFileName.set(iTempWCharBuf, oscl_strlen(iTempWCharBuf));
            RetrieveFilename(wFileName.get_str(), inputfilename);

            printf("\nInput File Name is %s", iFileName);

            iDataSource->SetDataSourceURL(wFileName);
            iDataSource->SetDataSourceFormatType(iFileType);

            start_time = OsclTickCount::TicksToMsec(OsclTickCount::TickCount());

            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::GetMetadata Issued ClockInMS = (%d)", start_time));


            OSCL_TRY(error, iCurrentCmdId = iPVMEContainer.iPVMEInterface->GetMetadata(*iDataSource, iMetadataValueList, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVMEATB_TEST_IS_TRUE(false); iState = STATE_RESET; RunIfNotReady());
        }

        break;

        case STATE_RESET:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::Reset Issued ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

            iCurrentCmdId = iPVMEContainer.iPVMEInterface->Reset((OsclAny*) & iContextObject);

        }
        break;

        case STATE_DELETE:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::Delete Called ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
            PVMFStatus status = PVMetadataEngineFactory::DeletePVMetadataEngine(iPVMEContainer);
            if (status != PVMFSuccess)
            {
                PVMEATB_TEST_IS_TRUE(false);
            }
            delete iDataSource;
            iDataSource = NULL;

            iFS.Close();

            if (iPVMEContainer.iMode == PV_METADATA_ENGINE_THREADED_MODE)
            {
                iPVMEContainer.iSem.Close();
            }




            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::TotalNumClips=%d", numOfClips));

            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::AverageGetMetaDataTimeInMS=%d", (AverageGetMetaDataTimeInMS / numOfClips)));

            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::MaxGetMetaDataTime=%d", MaxGetMetaDataTime));

            PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                            (0, "PVMetadataEngineTest::MinGetMetaDataTime=%d", MinGetMetaDataTime));

            iObserver->TestCompleted(*iTestCase);
        }
        break;

        default:
            break;

    }
}

void pv_metadata_engine_test::CommandCompleted(const PVCmdResponse& aResponse)
{
    //if this callback is from pvme thread, then push it across the thread
    //boundary.
    if (iPVMEContainer.iMode == PV_METADATA_ENGINE_THREADED_MODE)
    {
        if (!iThreadSafeCommandQueue.IsInThread())
        {
            PVCmdResponse* cmd = OSCL_NEW(PVCmdResponse, (aResponse));
            iThreadSafeCommandQueue.AddToQueue(cmd);
            return;
        }
    }

    if (aResponse.GetCmdId() != iCurrentCmdId)
    {
        // Wrong command ID.
        PVMEATB_TEST_IS_TRUE(false);
        iState = STATE_RESET;
        RunIfNotReady();
        return;
    }

    if (aResponse.GetContext() != NULL)
    {
        if (aResponse.GetContext() == (OsclAny*)&iContextObject)
        {
            if (iContextObject != iContextObjectRefValue)
            {
                // Context data value was corrupted
                PVMEATB_TEST_IS_TRUE(false);
                iState = STATE_RESET;
                RunIfNotReady();
                return;
            }
        }
        else
        {
            // Context data pointer was corrupted
            PVMEATB_TEST_IS_TRUE(false);
            iState = STATE_RESET;
            RunIfNotReady();
            return;
        }
    }

    switch (iState)
    {
        case STATE_INIT:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                                (0, "PVMetadataEngineTest::Init completed sucessfully ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

                iState = STATE_SETMETADATAKEYS;
                RunIfNotReady();
            }
            else
            {
                // Init failed
                PVMEATB_TEST_IS_TRUE(false);
                PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                                (0, "PVMetadataEngineTest::Init failed ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));

                iState = STATE_RESET;
                RunIfNotReady();
            }
            break;

        case STATE_GETMETADATA:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                end_time = OsclTickCount::TicksToMsec(OsclTickCount::TickCount());

                AverageGetMetaDataTimeInMS = AverageGetMetaDataTimeInMS + (end_time - start_time);

                if (MaxGetMetaDataTime < (end_time - start_time))
                {
                    MaxGetMetaDataTime = (end_time - start_time);
                }

                if (metadataForFirstClip == true)   //If this is the first clip
                {
                    MinGetMetaDataTime = (end_time - start_time);
                }

                if (MinGetMetaDataTime > (end_time - start_time))
                {
                    MinGetMetaDataTime = (end_time - start_time);
                }


                PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                                (0, "PVMetadataEngineTest::GetMetaData completed sucessfully ClockInMS=(%d), TimeTakenInMS=(%d)", end_time, (end_time - start_time)));

                PrintSupportedMetaDataKeys();

                if (it < iClips.end())
                {
                    iState = STATE_GETMETADATA;
                    metadataForFirstClip = false;

                }

                else
                {
                    iState = STATE_RESET;
                }
                RunIfNotReady();
            }
            else
            {
                // GetMetadata failed
                PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                                (0, "PVMetadataEngineTest::GetMetadata failed ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
                PVMEATB_TEST_IS_TRUE(false);
                iState = STATE_RESET;
                RunIfNotReady();
            }
            break;

        case STATE_RESET:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF, iPerfLogger, PVLOGMSG_INFO,
                                (0, "PVMetadataEngineTest::Reset completed sucessfully ClockInMS=%d", OsclTickCount::TicksToMsec(OsclTickCount::TickCount())));
                PVMEATB_TEST_IS_TRUE(true);
                iState = STATE_DELETE;
                RunIfNotReady();
            }
            break;

        default:
        {
            // Testing error if this is reached
            PVMEATB_TEST_IS_TRUE(false);
            iState = STATE_RESET;
            RunIfNotReady();
        }
        break;
    }
}


void pv_metadata_engine_test::HandleErrorEvent(const PVAsyncErrorEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
}

void pv_metadata_engine_test::HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
}

void pv_metadata_engine_test::ReadMetadataFile()
{
    Oscl_File *MetadataFile = new Oscl_File;
    const int numKeys = 40;
    char arr[numKeys][MAX_LEN];
    char *keys[numKeys];
    int count = 0;
    char KeysFileName[255];
    int32 err = 0;

    Oscl_FileServer fileServer;
    err = fileServer.Connect();

    // Full path of MetadataFile is: SOURCENAME_PREPEND_STRING + pvlogger.ini
    oscl_strncpy(KeysFileName, SOURCENAME_PREPEND_STRING,
                 oscl_strlen(SOURCENAME_PREPEND_STRING) + 1);
    oscl_strcat(KeysFileName, "MetadataKeys.txt");

    printf("\nPath for Keys File is %s\n", KeysFileName);

    err = MetadataFile->Open(KeysFileName, Oscl_File::MODE_READ, fileServer);

    if (0 == err)
    {
        int ii = 0;
        int len = 0;
        if (0 == MetadataFile->Seek(0, Oscl_File::SEEKSET))
        {
            while (!MetadataFile->EndOfFile())
            {
                arr[ii][0] = '\0';
                fgetline(MetadataFile, arr[ii], MAX_LEN);
                len = oscl_strlen(arr[ii]);

                if (len == 0 || arr[ii][0] == '\n' || (arr[ii][0] == '\r' && arr[ii][1] == '\n'))
                {
                    ii--;
                }
                else if (arr[ii][len-1] == '\n' && arr[ii][len-2] == '\r')
                {
                    arr[ii][len-2] = '\0';
                    keys[ii] = arr[ii];
                }
                else if (arr[ii][len-1] == '\n' && arr[ii][len-2] != '\r')
                {
                    arr[ii][len-1] = '\0';
                    keys[ii] = arr[ii];
                }
                else
                {
                    arr[ii][len] = '\0';
                    keys[ii] = arr[ii];
                }

                ii++;
            }
        }

        MetadataFile->Close();

        count = ii - 1;

        printf("\nKeys are\n");
        for (int j = 0; j <= ii - 1; j++)
        {
            printf("\n%s", keys[j]);
        }

        for (int i = 0; i <= count; i++)
        {
            iMetadataKeyList.push_front(keys[i]);
        }

    }
    fileServer.Close();
    delete(MetadataFile);
}

void pv_metadata_engine_test::ReadClipsFile()
{

    char* iClip;
    char ClipsFileName[255];
    int32 err = 0;

    Oscl_FileServer fileServer;
    err = fileServer.Connect();

    if (0 == err)
    {
        Oscl_File *ClipsFile = new Oscl_File;
        // Full path of ClipsFile is: SOURCENAME_PREPEND_STRING + pvlogger.ini
        oscl_strncpy(ClipsFileName, SOURCENAME_PREPEND_STRING,
                     oscl_strlen(SOURCENAME_PREPEND_STRING) + 1);
        oscl_strcat(ClipsFileName, "Clips.txt");

        printf("\nPath for Clips File is %s\n", ClipsFileName);

        err = ClipsFile->Open(ClipsFileName, Oscl_File::MODE_READ, fileServer);

        if (0 == err)
        {
            int len = 0;
            if (0 == ClipsFile->Seek(0, Oscl_File::SEEKSET))
            {
                while (!ClipsFile->EndOfFile())
                {
                    iClip = (char*)oscl_malloc(200);
                    iClip[0] = '\0';
                    fgetline(ClipsFile, iClip, 127);
                    len = oscl_strlen(iClip);
                    if (len == 0 || iClip[0] == '\n' || (iClip[0] == '\r' && iClip[1] == '\n'))
                    {
                        numOfClips--;
                        oscl_free(iClip);
                    }
                    else if (iClip[len-1] == '\n' && iClip[len-2] == '\r')
                    {
                        iClip[len-2] = '\0';
                        iClips.push_back(iClip);
                    }
                    else if (iClip[len-1] == '\n' && iClip[len-2] != '\r')
                    {
                        iClip[len-1] = '\0';
                        iClips.push_back(iClip);
                    }
                    else
                    {
                        iClip[len] = '\0';
                        iClips.push_back(iClip);
                    }

                    numOfClips++;

                }

                ClipsFile->Close();

                printf("\nClips are\n");
                for (it = iClips.begin(); it < iClips.end(); it++)
                {
                    printf("\n%s\n", *it);
                }

                it = iClips.begin();
                iClipFilePresent = true;

            }
        }
        delete(ClipsFile);
    }
    fileServer.Close();

}


void pv_metadata_engine_test::GetSourceFormatType(char* aFileName, PVMFFormatType& aInputFileFormatType)
{
    // Check the file extension to determine format type
    // AAC file
    if (oscl_strstr(aFileName, ".aac") != NULL || oscl_strstr(aFileName, ".AAC") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_AACFF;
    }
    // MP3 file
    else  if (oscl_strstr(aFileName, ".mp3") != NULL || oscl_strstr(aFileName, ".MP3") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_MP3FF;
    }
    // AMR file (IETF and IF2)
    else  if (oscl_strstr(aFileName, ".amr") != NULL || oscl_strstr(aFileName, ".AMR") != NULL ||
              oscl_strstr(aFileName, ".cod") != NULL || oscl_strstr(aFileName, ".COD") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_AMRFF;
    }
    // RTSP URL
    else  if ((!oscl_strncmp("rtsp", aFileName, 4)) ||
              (!oscl_strncmp("RTSP", aFileName, 4)))
    {
        aInputFileFormatType = PVMF_MIME_DATA_SOURCE_RTSP_URL;
    }
    // HTTP URL
    else  if (oscl_strstr(aFileName, "http:") != NULL || oscl_strstr(aFileName, "HTTP:") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_DATA_SOURCE_HTTP_URL;
    }
    // MP4/3GP file
    else  if (oscl_strstr(aFileName, ".mp4") != NULL || oscl_strstr(aFileName, ".MP4") != NULL ||
              oscl_strstr(aFileName, ".3gp") != NULL || oscl_strstr(aFileName, ".3GP") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_MPEG4FF;
    }
    // ASF file
    else  if (oscl_strstr(aFileName, ".asf") != NULL || oscl_strstr(aFileName, ".ASF") != NULL ||
              oscl_strstr(aFileName, ".wma") != NULL || oscl_strstr(aFileName, ".WMA") != NULL ||
              oscl_strstr(aFileName, ".wmv") != NULL || oscl_strstr(aFileName, ".WMV") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_ASFFF;
    }
    // SDP file
    else  if (oscl_strstr(aFileName, ".sdp") != NULL || oscl_strstr(aFileName, ".SDP") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_DATA_SOURCE_SDP_FILE;
    }
    // PVX file
    else  if (oscl_strstr(aFileName, ".pvx") != NULL || oscl_strstr(aFileName, ".PVX") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_DATA_SOURCE_PVX_FILE;
    }
    // WAV file
    else  if (oscl_strstr(aFileName, ".wav") != NULL || oscl_strstr(aFileName, ".WAV") != NULL)
    {
        aInputFileFormatType = PVMF_MIME_WAVFF;
    }
    // Unknown so set to unknown and let the player engine determine the format type
    else
    {
        aInputFileFormatType = PVMF_MIME_FORMAT_UNKNOWN;
    }
}


void pv_metadata_engine_test::PrintSupportedMetaDataKeys()
{

    uint32 i = 0;

    fprintf(file, "\n\nOutput for file: %s\n", iFileName);
    fprintf(file, "\nMetadata key list (count=%d):\n", iMetadataKeyList.size());

    for (i = 0; i < iMetadataKeyList.size(); ++i)
    {
        fprintf(file, "Key %d: %s\n", (i + 1), iMetadataKeyList[i].get_cstr());
    }

    fprintf(file, "\nMetadata value list (count=%d):\n", iMetadataValueList.size());

    for (i = 0; i < iMetadataValueList.size(); ++i)
    {
        fprintf(file, "Value %d:\n   Key string: %s\n", (i + 1), iMetadataValueList[i].key);

        switch (GetValTypeFromKeyString(iMetadataValueList[i].key))
        {
            case PVMI_KVPVALTYPE_CHARPTR:
                fprintf(file, "   Value:%s\n", iMetadataValueList[i].value.pChar_value);
                break;

            case PVMI_KVPVALTYPE_WCHARPTR:
            {
                // Assume string is in UCS-2 encoding so convert to UTF-8
                char tmpstr[65];
                oscl_UnicodeToUTF8(iMetadataValueList[i].value.pWChar_value,
                                   oscl_strlen(iMetadataValueList[i].value.pWChar_value), tmpstr, 65);
                tmpstr[64] = '\0';
                fprintf(file, "   Value(in UTF-8, first 64 chars):%s\n", tmpstr);
            }
            break;

            case PVMI_KVPVALTYPE_UINT32:
                fprintf(file, "   Value:%d\n", iMetadataValueList[i].value.uint32_value);
                break;

            case PVMI_KVPVALTYPE_INT32:
                fprintf(file, "   Value:%d\n", iMetadataValueList[i].value.int32_value);
                break;

            case PVMI_KVPVALTYPE_UINT8:
                fprintf(file, "   Value:%d\n", iMetadataValueList[i].value.uint8_value);
                break;

            case PVMI_KVPVALTYPE_FLOAT:
                fprintf(file, "   Value:%f\n", iMetadataValueList[i].value.float_value);
                break;

            case PVMI_KVPVALTYPE_DOUBLE:
                fprintf(file, "   Value:%f\n", iMetadataValueList[i].value.double_value);
                break;

            case PVMI_KVPVALTYPE_BOOL:
                if (iMetadataValueList[i].value.bool_value)
                {
                    fprintf(file, "   Value:true(1)\n");
                }
                else
                {
                    fprintf(file, "   Value:false(0)\n");
                }
                break;

            default:
                fprintf(file, "   Value: UNKNOWN VALUE TYPE\n");
                break;
        }

        fprintf(file, "   Length:%d  Capacity:%d\n", iMetadataValueList[i].length, iMetadataValueList[i].capacity);
    }

}

//data is available on one of the thread-safe queues.
void pv_metadata_engine_test::ThreadSafeQueueDataAvailable(ThreadSafeQueue* aQueue)
{
    if (aQueue == &iThreadSafeCommandQueue)
    {
        for (uint32 ndata = 1; ndata;)
        {
            ThreadSafeQueueId id;
            OsclAny* data;
            ndata = iThreadSafeCommandQueue.DeQueue(id, data);
            if (ndata)
            {
                PVCmdResponse* cmd = (PVCmdResponse*)data;
                CommandCompleted(*cmd);
                OSCL_DELETE(cmd);
            }
        }
    }
    if (aQueue == &iThreadSafeErrorQueue)
    {
        for (uint32 ndata = 1; ndata;)
        {
            ThreadSafeQueueId id;
            OsclAny* data;
            ndata = iThreadSafeErrorQueue.DeQueue(id, data);
            if (ndata)
            {
                int32 eventType = (PVMFEventType)data;
                PVAsyncErrorEvent event(eventType);
                HandleErrorEvent(event);
            }
        }
    }
    if (aQueue == &iThreadSafeInfoQueue)
    {
        for (uint32 ndata = 1; ndata;)
        {
            ThreadSafeQueueId id;
            OsclAny* data;
            ndata = iThreadSafeInfoQueue.DeQueue(id, data);
            if (ndata)
            {
                int32 eventType = (PVMFEventType)data;
                PVAsyncInformationalEvent event(eventType);
                HandleInformationalEvent(event);
            }
        }
    }
}
