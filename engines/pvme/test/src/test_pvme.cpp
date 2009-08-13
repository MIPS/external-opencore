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
#include "test_pvme.h"
#endif

#include "test_pvme_testset1.h"

FILE* file;

#define MAX_LEN 100

int fgetline(Oscl_File* aFp, char aLine[], int aMax)
{
    int nch = 0;
    char cc[2] = "\0";
    aMax = aMax - 1;

    while (aFp->Read((char*)cc, 1, 1) != 0)
    {
        if (nch < aMax)
        {
            aLine[nch] = cc[0];
            nch = nch + 1;
            if (cc[0] == '\n')
            {
                break;
            }
        }
    }

    if ((int)cc[0] == 0 && nch == 0)
    {
        return 0;
    }
    aLine[nch] = '\0';
    return nch;
}

bool PVLoggerConfigFile::get_next_line(const char *start_ptr, const char * end_ptr,
                                       const char *& line_start,
                                       const char *& line_end)
{
    // Finds the boundaries of the next non-empty line within start
    // and end ptrs

    // This initializes line_start to the first non-whitespace character
    line_start = skip_whitespace_and_line_term(start_ptr, end_ptr);

    line_end = skip_to_line_term(line_start, end_ptr);

    return (line_start < end_ptr);

}


bool PVLoggerConfigFile::IsLoggerConfigFilePresent()
{
    if (-1 != ReadAndParseLoggerConfigFile())
        return true;
    return false;
}

//Read and parse the config file
//retval = -1 if the config file doesnt exist
int8 PVLoggerConfigFile::ReadAndParseLoggerConfigFile()
{
    int8 retval = 1;

    if (0 != iLogFile.Open(iLogFileName, Oscl_File::MODE_READ, iFileServer))
    {
        retval = -1;
    }
    else
    {
        if (!iLogFileRead)
        {
            iLogFile.Read(ibuffer, 1, sizeof(ibuffer));
            //Parse the buffer for \n chars
            Oscl_Vector<char*, OsclMemAllocator> LogConfigStrings;

            const char *end_ptr = ibuffer + oscl_strlen(ibuffer) ; // Point just beyond the end
            const char *section_start_ptr;
            const char *line_start_ptr, *line_end_ptr;
            char* end_temp_ptr;

            section_start_ptr = skip_whitespace_and_line_term(ibuffer, end_ptr);

            while (section_start_ptr < end_ptr)
            {
                if (!get_next_line(section_start_ptr, end_ptr,
                                   line_start_ptr, line_end_ptr))
                {
                    break;
                }


                section_start_ptr = line_end_ptr + 1;

                end_temp_ptr = (char*)line_end_ptr;
                *end_temp_ptr = '\0';

                LogConfigStrings.push_back((char*)line_start_ptr);

            }

            //Populate the  LoggerConfigElements vector
            {
                if (!LogConfigStrings.empty())
                {
                    Oscl_Vector<char*, OsclMemAllocator>::iterator it;
                    it = LogConfigStrings.begin();
                    uint32 appenderType;
                    PV_atoi(*it, 'd', oscl_strlen(*it), appenderType);
                    iAppenderType = appenderType;

                    if (LogConfigStrings.size() > 1)
                    {
                        for (it = LogConfigStrings.begin() + 1; it != LogConfigStrings.end(); it++)
                        {
                            char* CommaIndex = (char*)oscl_strstr(*it, ",");
                            if (CommaIndex != NULL)
                            {
                                *CommaIndex = '\0';
                                LoggerConfigElement obj;
                                uint32 logLevel;
                                PV_atoi(*it, 'd', oscl_strlen(*it), logLevel);
                                obj.iLogLevel = logLevel;
                                obj.iLoggerString = CommaIndex + 1;
                                iLoggerConfigElements.push_back(obj);
                            }
                        }
                    }
                    else
                    {
                        //Add the config element for complete logging fo all the modules
                        LoggerConfigElement obj;
                        obj.iLoggerString = (char*)"";
                        obj.iLogLevel = 8;
                        iLoggerConfigElements.push_back(obj);
                    }
                }
            }
            iLogFile.Close();
            iLogFileRead = true;
        }
    }
    return retval;
}

void PVLoggerConfigFile::SetLoggerSettings()
{
    Oscl_Vector<LoggerConfigElement, OsclMemAllocator>::iterator it;

    PVLoggerAppender *appender = NULL;
    OsclRefCounter *refCounter = NULL;
    if (iLoggerConfigElements.empty())
    {
        return;
    }

    if (iAppenderType == 0)
    {
        appender = new StdErrAppender<TimeAndIdLayout, 1024>();
        OsclRefCounterSA<LogAppenderDestructDealloc<StdErrAppender<TimeAndIdLayout, 1024> > > *appenderRefCounter =
            new OsclRefCounterSA<LogAppenderDestructDealloc<StdErrAppender<TimeAndIdLayout, 1024> > >(appender);
        refCounter = appenderRefCounter;
    }
    else if (iAppenderType == 1)
    {
        OSCL_wHeapString<OsclMemAllocator> testlogfilename;
        //File to be sent to pvme for logging in threaded mode
        logfilename += OUTPUTNAME_PREPEND_WSTRING;
        logfilename += _STRLIT_WCHAR("pvme.log");
        //File for logging PVME test log statements in threaded mode, all logs in non-threaded mode
        testlogfilename += OUTPUTNAME_PREPEND_WSTRING;
        testlogfilename += _STRLIT_WCHAR("pvme_test.log");
        appender = (PVLoggerAppender*)TextFileAppender<TimeAndIdLayout, 1024>::CreateAppender(testlogfilename.get_str());
        OsclRefCounterSA<LogAppenderDestructDealloc<TextFileAppender<TimeAndIdLayout, 1024> > > *appenderRefCounter =
            new OsclRefCounterSA<LogAppenderDestructDealloc<TextFileAppender<TimeAndIdLayout, 1024> > >(appender);
        refCounter = appenderRefCounter;
    }
    else
    {
        OSCL_wHeapString<OsclMemAllocator> testlogfilename;
        logfilename += OUTPUTNAME_PREPEND_WSTRING;
        logfilename += _STRLIT_WCHAR("pvme.log");
        testlogfilename += OUTPUTNAME_PREPEND_WSTRING;
        testlogfilename += _STRLIT_WCHAR("pvme_test.log");
        appender = (PVLoggerAppender*)MemAppender<TimeAndIdLayout, 1024>::CreateAppender(testlogfilename.get_str());
        OsclRefCounterSA<LogAppenderDestructDealloc<MemAppender<TimeAndIdLayout, 1024> > > *appenderRefCounter =
            new OsclRefCounterSA<LogAppenderDestructDealloc<MemAppender<TimeAndIdLayout, 1024> > >(appender);
        refCounter = appenderRefCounter;
    }

    OsclSharedPtr<PVLoggerAppender> appenderPtr(appender, refCounter);

    for (it = iLoggerConfigElements.begin(); it != iLoggerConfigElements.end(); it++)
    {
        PVLogger *node = NULL;
        node = PVLogger::GetLoggerObject(it->iLoggerString);
        node->AddAppender(appenderPtr);
        node->SetLogLevel(it->iLogLevel);
    }
}

//Find test range args:
//To run a range of tests by enum ID:
//  -test 17 29
void FindTestRange(cmd_line* command_line,  int32& iFirstTest, int32 &iLastTest, FILE* aFile)
{
    //default is to run all tests.
    iFirstTest = 0;
    iLastTest = 999;

    int iTestArgument = 0;
    char *iTestArgStr1 = NULL;
    char *iTestArgStr2 = NULL;
    bool iTestFound = false;
    bool cmdline_iswchar = command_line->is_wchar();

    int count = command_line->get_count();

    // Search for the "-test" argument
    char *iSourceFind = NULL;
    if (cmdline_iswchar)
    {
        iSourceFind = new char[256];
    }

    // Go through each argument
    for (int iTestSearch = 0; iTestSearch < count; iTestSearch++)
    {
        // Convert to UTF8 if necessary
        if (cmdline_iswchar)
        {
            OSCL_TCHAR* cmd = NULL;
            command_line->get_arg(iTestSearch, cmd);
            oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iSourceFind, 256);
        }
        else
        {
            iSourceFind = NULL;
            command_line->get_arg(iTestSearch, iSourceFind);
        }

        // Do the string compare
        if (oscl_strcmp(iSourceFind, "-help") == 0)
        {
            fprintf(aFile, "Test cases to run option. Default is ALL:\n");
            fprintf(aFile, "  -test x y\n");
            fprintf(aFile, "   Specify a range of test cases to run. To run one test case, use the\n");
            fprintf(aFile, "   same index for x and y.\n\n");
        }
        else if (oscl_strcmp(iSourceFind, "-test") == 0)
        {
            iTestFound = true;
            iTestArgument = ++iTestSearch;
            break;
        }
    }

    if (cmdline_iswchar)
    {
        delete[] iSourceFind;
        iSourceFind = NULL;
    }

    if (iTestFound)
    {
        // Convert to UTF8 if necessary
        if (cmdline_iswchar)
        {
            iTestArgStr1 = new char[256];
            OSCL_TCHAR* cmd;
            command_line->get_arg(iTestArgument, cmd);
            if (cmd)
            {
                oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iTestArgStr1, 256);
            }

            iTestArgStr2 = new char[256];
            command_line->get_arg(iTestArgument + 1, cmd);
            if (cmd)
            {
                oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iTestArgStr2, 256);
            }
        }
        else
        {
            command_line->get_arg(iTestArgument, iTestArgStr1);
            command_line->get_arg(iTestArgument + 1, iTestArgStr2);
        }

        //Pull out 2 integers...
        if (iTestArgStr1
                && '0' <= iTestArgStr1[0] && iTestArgStr1[0] <= '9'
                && iTestArgStr2
                && '0' <= iTestArgStr2[0] && iTestArgStr2[0] <= '9')
        {
            int len = oscl_strlen(iTestArgStr1);
            switch (len)
            {
                case 3:
                    iFirstTest = 0;
                    if ('0' <= iTestArgStr1[0] && iTestArgStr1[0] <= '9')
                    {
                        iFirstTest = iFirstTest + 100 * (iTestArgStr1[0] - '0');
                    }

                    if ('0' <= iTestArgStr1[1] && iTestArgStr1[1] <= '9')
                    {
                        iFirstTest = iFirstTest + 10 * (iTestArgStr1[1] - '0');
                    }

                    if ('0' <= iTestArgStr1[2] && iTestArgStr1[2] <= '9')
                    {
                        iFirstTest = iFirstTest + 1 * (iTestArgStr1[2] - '0');
                    }
                    break;

                case 2:
                    iFirstTest = 0;
                    if ('0' <= iTestArgStr1[0] && iTestArgStr1[0] <= '9')
                    {
                        iFirstTest = iFirstTest + 10 * (iTestArgStr1[0] - '0');
                    }

                    if ('0' <= iTestArgStr1[1] && iTestArgStr1[1] <= '9')
                    {
                        iFirstTest = iFirstTest + 1 * (iTestArgStr1[1] - '0');
                    }
                    break;

                case 1:
                    iFirstTest = 0;
                    if ('0' <= iTestArgStr1[0] && iTestArgStr1[0] <= '9')
                    {
                        iFirstTest = iFirstTest + 1 * (iTestArgStr1[0] - '0');
                    }
                    break;

                default:
                    break;
            }

            len = oscl_strlen(iTestArgStr2);
            switch (len)
            {
                case 3:
                    iLastTest = 0;
                    if ('0' <= iTestArgStr2[0] && iTestArgStr2[0] <= '9')
                    {
                        iLastTest = iLastTest + 100 * (iTestArgStr2[0] - '0');
                    }

                    if ('0' <= iTestArgStr2[1] && iTestArgStr2[1] <= '9')
                    {
                        iLastTest = iLastTest + 10 * (iTestArgStr2[1] - '0');
                    }

                    if ('0' <= iTestArgStr2[2] && iTestArgStr2[2] <= '9')
                    {
                        iLastTest = iLastTest + 1 * (iTestArgStr2[2] - '0');
                    }
                    break;

                case 2:
                    iLastTest = 0;
                    if ('0' <= iTestArgStr2[0] && iTestArgStr2[0] <= '9')
                    {
                        iLastTest = iLastTest + 10 * (iTestArgStr2[0] - '0');
                    }

                    if ('0' <= iTestArgStr2[1] && iTestArgStr2[1] <= '9')
                    {
                        iLastTest = iLastTest + 1 * (iTestArgStr2[1] - '0');
                    }
                    break;

                case 1:
                    iLastTest = 0;
                    if ('0' <= iTestArgStr2[0] && iTestArgStr2[0] <= '9')
                    {
                        iLastTest = iLastTest + 1 * (iTestArgStr2[0] - '0');
                    }
                    break;

                default:
                    break;
            }
        }
    }

    if (cmdline_iswchar)
    {
        if (iTestArgStr1)
        {
            delete[] iTestArgStr1;
            iTestArgStr1 = NULL;
        }

        if (iTestArgStr2)
        {
            delete[] iTestArgStr2;
            iTestArgStr2 = NULL;
        }

        if (iSourceFind)
        {
            delete[] iSourceFind;
            iSourceFind = NULL;
        }
    }
}


void FindMemMgmtRelatedCmdLineParams(cmd_line* command_line, bool& aPrintDetailedMemLeakInfo, FILE* aFile)
{
    aPrintDetailedMemLeakInfo = false;

    bool cmdline_iswchar = command_line->is_wchar();

    int count = command_line->get_count();

    // Search for the "-logerr"/"-logwarn" argument
    char *iSourceFind = NULL;
    if (cmdline_iswchar)
    {
        iSourceFind = new char[256];
    }

    // Go through each argument
    for (int iTestSearch = 0; iTestSearch < count; iTestSearch++)
    {
        // Convert to UTF8 if necessary
        if (cmdline_iswchar)
        {
            OSCL_TCHAR* cmd = NULL;
            command_line->get_arg(iTestSearch, cmd);
            oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iSourceFind, 256);
        }
        else
        {
            iSourceFind = NULL;
            command_line->get_arg(iTestSearch, iSourceFind);
        }

        // Do the string compare
        if (oscl_strcmp(iSourceFind, "-help") == 0)
        {
            fprintf(aFile, "Printing leak info option. Default is OFF:\n");
            fprintf(aFile, "  -leakinfo\n");
            fprintf(aFile, "   If there is a memory leak, prints out the memory leak information\n");
            fprintf(aFile, "   after all specified test cases have finished running.\n\n");
        }
        else if (oscl_strcmp(iSourceFind, "-leakinfo") == 0)
        {
            aPrintDetailedMemLeakInfo = true;
        }
    }

    if (cmdline_iswchar)
    {
        delete[] iSourceFind;
        iSourceFind = NULL;
    }
}


void FindLogLevel(cmd_line* command_line, int32& loglevel, FILE* aFile)
{
    //default is verbose
    loglevel = PVLOGMSG_DEBUG;

    bool cmdline_iswchar = command_line->is_wchar();

    int count = command_line->get_count();

    // Search for the "-logerr"/"-logwarn" argument
    char *iSourceFind = NULL;
    if (cmdline_iswchar)
    {
        iSourceFind = new char[256];
    }

    // Go through each argument
    for (int iTestSearch = 0; iTestSearch < count; iTestSearch++)
    {
        // Convert to UTF8 if necessary
        if (cmdline_iswchar)
        {
            OSCL_TCHAR* cmd = NULL;
            command_line->get_arg(iTestSearch, cmd);
            oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iSourceFind, 256);
        }
        else
        {
            iSourceFind = NULL;
            command_line->get_arg(iTestSearch, iSourceFind);
        }

        // Do the string compare
        if (oscl_strcmp(iSourceFind, "-help") == 0)
        {
            fprintf(aFile, "Log level options. Default is debug level:\n");
            fprintf(aFile, "  -logerr\n");
            fprintf(aFile, "   Log at error level\n");
            fprintf(aFile, "  -logwarn\n");
            fprintf(aFile, "   Log at warning level\n\n");
        }
        else if (oscl_strcmp(iSourceFind, "-logerr") == 0)
        {
            loglevel = PVLOGMSG_ERR;
        }
        else if (oscl_strcmp(iSourceFind, "-logwarn") == 0)
        {
            loglevel = PVLOGMSG_WARNING;
        }
    }

    if (cmdline_iswchar)
    {
        delete[] iSourceFind;
        iSourceFind = NULL;
    }
}

int _local_main(FILE* filehandle, cmd_line* command_line);



// Entry point for the unit test program
int local_main(FILE* filehandle, cmd_line* command_line)
{
    //Init Oscl
    OsclBase::Init();
    OsclErrorTrap::Init();
    OsclMem::Init();

    int32 err = 0;

    const int numArgs = 10; //change as per the number of args below
    char *argv[numArgs];
    char arr[numArgs][MAX_LEN];

    Oscl_FileServer fileServer;
    err = fileServer.Connect();

    if (0 == err)
    {
        Oscl_File *InputFile = new Oscl_File;

        int argc = 0;

        fprintf(filehandle, "Test Program for PVME.\n");

        if (InputFile != NULL)
        {
            err = InputFile->Open("input.txt", Oscl_File::MODE_READ, fileServer);

            if (0 == err)
            {
                int ii = 0;
                int len = 0;
                if (0 == InputFile->Seek(0, Oscl_File::SEEKSET))
                {
                    while (!InputFile->EndOfFile())
                    {
                        arr[ii][0] = '\0';
                        fgetline(InputFile, arr[ii], MAX_LEN);
                        len = strlen(arr[ii]);

                        if (len == 0 || arr[ii][0] == '\n' || (arr[ii][0] == '\r' && arr[ii][1] == '\n'))
                        {
                            ii--;
                        }
                        else if (arr[ii][len-1] == '\n' && arr[ii][len-2] == '\r')
                        {
                            arr[ii][len-2] = '\0';
                            argv[ii] = arr[ii];
                        }
                        else if (arr[ii][len-1] == '\n' && arr[ii][len-2] != '\r')
                        {
                            arr[ii][len-1] = '\0';
                            argv[ii] = arr[ii];
                        }
                        else
                        {
                            arr[ii][len] = '\0';
                            argv[ii] = arr[ii];
                        }

                        ii++;

                    }
                    InputFile->Close();
                    argc = ii;
                    int n = 0;
                    command_line->setup(argc - n, &argv[n]);
                }
            }
            fileServer.Close();
        }

        delete(InputFile);
    }


    //if there is no input file with cmd line args
    //we attempt to use what was provided on cmd line
    //as a fallback

    bool oPrintDetailedMemLeakInfo = false;
    FindMemMgmtRelatedCmdLineParams(command_line, oPrintDetailedMemLeakInfo, filehandle);

    //Run the test under a trap
    int result = 0;
    err = 0;

    OSCL_TRY(err, result = _local_main(filehandle, command_line););

    //Show any exception.
    if (err != 0)
    {
        fprintf(file, "Error!  Leave %d\n", err);
    }


#if !(OSCL_BYPASS_MEMMGT)
    //Check for memory leaks before cleaning up OsclMem.
    OsclAuditCB auditCB;
    OsclMemInit(auditCB);
    if (auditCB.pAudit)
    {
        MM_Stats_t* stats = auditCB.pAudit->MM_GetStats("");
        if (stats)
        {
            fprintf(file, "\nMemory Stats:\n");
            fprintf(file, "  peakNumAllocs %d\n", stats->peakNumAllocs);
            fprintf(file, "  peakNumBytes %d\n", stats->peakNumBytes);
            fprintf(file, "  totalNumAllocs %d\n", stats->totalNumAllocs);
            fprintf(file, "  totalNumBytes %d\n", stats->totalNumBytes);
            fprintf(file, "  numAllocFails %d\n", stats->numAllocFails);
            if (stats->numAllocs)
            {
                fprintf(file, "  ERROR: Memory Leaks! numAllocs %d, numBytes %d\n", stats->numAllocs, stats->numBytes);
            }
        }
        uint32 leaks = auditCB.pAudit->MM_GetNumAllocNodes();
        if (leaks != 0)
        {
            if (oPrintDetailedMemLeakInfo)
            {
                fprintf(file, "ERROR: %d Memory leaks detected!\n", leaks);
                MM_AllocQueryInfo*info = auditCB.pAudit->MM_CreateAllocNodeInfo(leaks);
                uint32 leakinfo = auditCB.pAudit->MM_GetAllocNodeInfo(info, leaks, 0);
                if (leakinfo != leaks)
                {
                    fprintf(file, "ERROR: Leak info is incomplete.\n");
                }
                for (uint32 i = 0; i < leakinfo; i++)
                {
                    fprintf(file, "Leak Info:\n");
                    fprintf(file, "  allocNum %d\n", info[i].allocNum);
                    fprintf(file, "  fileName %s\n", info[i].fileName);
                    fprintf(file, "  lineNo %d\n", info[i].lineNo);
                    fprintf(file, "  size %d\n", info[i].size);
                    fprintf(file, "  pMemBlock 0x%x\n", (unsigned int)(info[i].pMemBlock));
                    fprintf(file, "  tag %s\n", info[i].tag);
                }
                auditCB.pAudit->MM_ReleaseAllocNodeInfo(info);
            }
        }
    }
#endif

    OsclMem::Cleanup();
    OsclErrorTrap::Cleanup();
    OsclBase::Cleanup();
    return result;
}

int _local_main(FILE* filehandle, cmd_line* command_line)
{
    file = filehandle;

    // Print out the extension for help if no argument
    if (command_line->get_count() == 0)
    {
        fprintf(file, "  Specify '-help' first to get help information on options\n\n");
    }

    int32 firsttest, lasttest;
    FindTestRange(command_line, firsttest, lasttest, file);

    int32 loglevel;
    FindLogLevel(command_line, loglevel, file);

    fprintf(file, "  Test case range %d to %d\n", firsttest, lasttest);
    fprintf(file, "  Log level %d\n", loglevel);

    pvme_test_suite* pvme_tests = NULL;
    pvme_tests = new pvme_test_suite(firsttest, lasttest, loglevel);
    if (pvme_tests)
    {
        // Run the test
        pvme_tests->run_test();

        // Print out the results
        text_test_interpreter interp;
        _STRING rs = interp.interpretation(pvme_tests->last_result());
        fprintf(file, rs.c_str());

        const test_result the_result = pvme_tests->last_result();
        delete pvme_tests;
        pvme_tests = NULL;

        return (the_result.success_count() != the_result.total_test_count());
    }
    else
    {
        fprintf(file, "ERROR! pvme_test_suite could not be instantiated.\n");
        return 1;
    }
}


pvme_test_suite::pvme_test_suite(int32 aFirstTest, int32 aLastTest, int32 aLogLevel)
        : test_case()
{
    adopt_test_case(new pvmetadataengine_test(aFirstTest, aLastTest, aLogLevel));
}



pvmetadataengine_test::pvmetadataengine_test(int32 aFirstTest, int32 aLastTest, int32 aLogLevel)
{
    iCurrentTestNumber = 0;
    iCurrentTest = NULL;
    iFirstTest = aFirstTest;
    iLastTest = aLastTest;
    iLogLevel = aLogLevel;
    iTotalAlloc = 0;
    iTotalBytes = 0;
    iAllocFails = 0;
    iNumAllocs = 0;
}


pvmetadataengine_test::~pvmetadataengine_test()
{
}


void pvmetadataengine_test::TestCompleted(test_case &tc)
{
    // Print out the result for this test case
    const test_result the_result = tc.last_result();
    fprintf(file, "  Successes %d, Failures %d\n"
            , the_result.success_count() - iTotalSuccess, the_result.failures().size() - iTotalFail);
    iTotalSuccess = the_result.success_count();
    iTotalFail = the_result.failures().size();
    iTotalError = the_result.errors().size();

    // Go to next test
    ++iCurrentTestNumber;

    // Stop the scheduler
    OsclExecScheduler *sched = OsclExecScheduler::Current();
    if (sched)
    {
        sched->StopScheduler();
    }
}


void pvmetadataengine_test::test()
{
    // Specify the starting test case
    iCurrentTestNumber = iFirstTest;
    iTotalSuccess = iTotalFail = iTotalError = 0;

    while (iCurrentTestNumber <= iLastTest || iCurrentTestNumber < BeyondLastTest)
    {
        if (iCurrentTest)
        {
            delete iCurrentTest;
            iCurrentTest = NULL;

            // Shutdown PVLogger and scheduler before checking mem stats
            OsclScheduler::Cleanup();
            PVLogger::Cleanup();
#if !(OSCL_BYPASS_MEMMGT)
            // Print out the memory usage results for this test case
            OsclAuditCB auditCB;
            OsclMemInit(auditCB);
            if (auditCB.pAudit)
            {
                MM_Stats_t* stats = auditCB.pAudit->MM_GetStats("");
                if (stats)
                {
                    fprintf(file, "  Mem stats: TotalAllocs(%d), TotalBytes(%d),\n             AllocFailures(%d), AllocLeak(%d)\n",
                            stats->totalNumAllocs - iTotalAlloc, stats->totalNumBytes - iTotalBytes, stats->numAllocFails - iAllocFails, stats->numAllocs - iNumAllocs);
                }
                else
                {
                    fprintf(file, "Retrieving memory statistics after running test case failed! Memory statistics result is not available.\n");
                }
            }
            else
            {
                fprintf(file, "Memory audit not available! Memory statistics result is not available.\n");
            }
#endif
        }

#if !(OSCL_BYPASS_MEMMGT)
        // Obtain the current mem stats before running the test case
        OsclAuditCB auditCB;
        OsclMemInit(auditCB);
        if (auditCB.pAudit)
        {
            MM_Stats_t* stats = auditCB.pAudit->MM_GetStats("");
            if (stats)
            {
                iTotalAlloc = stats->totalNumAllocs;
                iTotalBytes = stats->totalNumBytes;
                iAllocFails = stats->numAllocFails;
                iNumAllocs = stats->numAllocs;
            }
            else
            {
                fprintf(file, "Retrieving memory statistics before running test case failed! Memory statistics result would be invalid.\n");
            }
        }
        else
        {
            fprintf(file, "Memory audit not available! Memory statistics result would be invalid.\n");
        }
#endif

        // Stop at last test of selected range.
        if (iCurrentTestNumber > iLastTest || iCurrentTestNumber == LastTest)
        {
            iCurrentTestNumber = BeyondLastTest;
        }
        else
        {
            fprintf(file, "\nStarting Test %d: ", iCurrentTestNumber);
            SetupLoggerScheduler();
        }

        // Setup the standard test case parameters based on current unit test settings
        PVMetadataEngineTestParam testparam;
        testparam.iObserver = this;
        testparam.iTestCase = this;
        testparam.iTestMsgOutputFile = file;

        switch (iCurrentTestNumber)
        {
            case GetSourceMetadataNonThreadedModeTest:
                iCurrentTest = new pv_metadata_engine_test(testparam, PV_METADATA_ENGINE_NON_THREADED_MODE, obj);
                break;

            case GetSourceMetadataThreadedModeTest:
                iCurrentTest = new pv_metadata_engine_test(testparam, PV_METADATA_ENGINE_THREADED_MODE, obj);
                break;

            case BeyondLastTest:
            default:
                iCurrentTestNumber = BeyondLastTest;
                break;
        }

        if (iCurrentTest)
        {
            OsclExecScheduler *sched = OsclExecScheduler::Current();
            if (sched)
            {
                // Print out the test name
                fprintf(file, "%s\n", iCurrentTest->iTestCaseName.get_cstr());
                // Start the test
                iCurrentTest->StartTest();
                // Start the scheduler so the test case would run
#if USE_NATIVE_SCHEDULER
                // Have PV scheduler use the scheduler native to the system
                sched->StartNativeScheduler();
#else
                // Have PV scheduler use its own implementation of the scheduler
                sched->StartScheduler();
#endif
            }
            else
            {
                fprintf(file, "ERROR! Scheduler is not available. Test case could not run.");
                ++iCurrentTestNumber;
            }
        }
        else
        {
            ++iCurrentTestNumber;
            if (iCurrentTestNumber < BeyondLastTest)
            {
                // Shutdown PVLogger and scheduler before continuing on
                OsclScheduler::Cleanup();
                PVLogger::Cleanup();
            }
        }
    }
}

void pvmetadataengine_test::SetupLoggerScheduler()
{
    // Enable the following code for logging (on Symbian, RDebug)
    PVLogger::Init();
    if (obj.IsLoggerConfigFilePresent())
    {
        obj.SetLoggerSettings();
    }
    // Construct and install the active scheduler
    OsclScheduler::Init("PVMetadataEngineTestScheduler");
}



