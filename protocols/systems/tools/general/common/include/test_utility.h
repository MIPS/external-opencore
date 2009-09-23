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
#ifndef TEST_UTILITY_H_HEADER
#define TEST_UTILITY_H_HEADER

#include "test_case.h"
#include "xml_test_interpreter.h"
#include "oscl_file_io.h"
#define MAX_324_TEST 100000


class PV2WayUtil
{
    public:
        OSCL_IMPORT_REF static void OutputInfo(const char * str, ...);
        OSCL_IMPORT_REF static void SetFileHandle(const FILE *aFileHandle);
        OSCL_IMPORT_REF static FILE *GetFileHandle();
        static void FindTestRange(cmd_line* command_line,
                                  uint32 &aFirstTest,
                                  uint32 &aLastTest,
                                  int32 aTestLimit = 0);
        static char* FindConfigGfc(cmd_line* command_line,
                                   OSCL_HeapString<OsclMemAllocator> &aCommand);
        static bool FindCmdArg(cmd_line* command_line,
                               OSCL_HeapString<OsclMemAllocator> &aArgument,
                               int &ArgCount);
    private:
        static FILE* iFileHandle;
        static int test;
};

void FindXmlResultsFile(cmd_line* command_line,
                        OSCL_HeapString<OsclMemAllocator> &XmlTestResultsFilename,
                        FILE *aFile);

void WriteInitialXmlSummary(OSCL_HeapString<OsclMemAllocator> &xmlresultsfile,
                            FILE * aFile);

void WriteFinalXmlSummary(OSCL_HeapString<OsclMemAllocator> &xmlresultsfile,
                          const test_result& result,
                          FILE *aFile);

#endif
