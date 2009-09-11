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
#include "oscl_error.h"
#include "oscl_timer.h"
#include "oscl_mem.h"
#include "oscl_scheduler.h"
#include "oscl_utf8conv.h"
#include "test_engine_utility.h"

void FindTestRange(cmd_line* command_line,
                   int32& iFirstTest,
                   int32 &iLastTest,
                   FILE* aFile)
{
    //default is to run all tests.
    iFirstTest = 0;
    iLastTest = MAX_324_TEST;

    int iTestArgument = 0;
    char *iTestArgStr1 = NULL;
    char *iTestArgStr2 = NULL;
    bool cmdline_iswchar = command_line->is_wchar();

    int count = command_line->get_count();

    // Search for the "-test" argument
    char *iSourceFind = NULL;
    if (cmdline_iswchar)
    {
        iSourceFind = OSCL_ARRAY_NEW(char, 256);
    }

    int iTestSearch = 0;
    while (iTestSearch < count)
    {
        bool iTestFound = false;
        // Go through each argument
        for (; iTestSearch < count; iTestSearch++)
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
                fprintf(aFile, "   same index for x and y.\n");

                fprintf(aFile, "  -test G\n");
                fprintf(aFile, "   Run 324M test cases only.\n");

                exit(0);
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
            OSCL_ARRAY_DELETE(iSourceFind);
            iSourceFind = NULL;
        }

        if (iTestFound)
        {
            // Convert to UTF8 if necessary
            if (cmdline_iswchar)
            {
                iTestArgStr1 = OSCL_ARRAY_NEW(char, 256);
                OSCL_TCHAR* cmd;
                command_line->get_arg(iTestArgument, cmd);
                if (cmd)
                {
                    oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iTestArgStr1, 256);
                }

                iTestArgStr2 = OSCL_ARRAY_NEW(char, 256);
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

#ifndef NO_2WAY_324
            else if (iTestArgStr1
                     && iTestArgStr1[0] == 'G')
            {
                //download tests
                iFirstTest = 0;
                iLastTest = MAX_324_TEST;
            }
#endif
        }

        if (cmdline_iswchar)
        {
            if (iTestArgStr1)
            {
                OSCL_ARRAY_DELETE(iTestArgStr1);
                iTestArgStr1 = NULL;
            }

            if (iTestArgStr2)
            {
                OSCL_ARRAY_DELETE(iTestArgStr2);
                iTestArgStr2 = NULL;
            }

            if (iSourceFind)
            {
                OSCL_ARRAY_DELETE(iSourceFind);
                iSourceFind = NULL;
            }
        }
        iTestSearch += 2;
    }
}

// Find xml results filename from arguments
void FindXmlResultsFile(cmd_line* command_line, OSCL_HeapString<OsclMemAllocator> &XmlTestResultsFilename, FILE *aFile)
{
    int iFileArgument = 0;
    bool iFileFound = false;
    bool cmdline_iswchar = command_line->is_wchar();

    int count = command_line->get_count();

    // Search for the argument
    // Go through each argument
    for (int iFileSearch = 0; iFileSearch < count; iFileSearch++)
    {
        char argstr[128];
        // Convert to UTF8 if necessary
        if (cmdline_iswchar)
        {
            oscl_wchar* argwstr = NULL;
            command_line->get_arg(iFileSearch, argwstr);
            oscl_UnicodeToUTF8(argwstr, oscl_strlen(argwstr), argstr, 128);
            argstr[127] = '\0';
        }
        else
        {
            char* tmpstr = NULL;
            command_line->get_arg(iFileSearch, tmpstr);
            int32 tmpstrlen = oscl_strlen(tmpstr) + 1;
            if (tmpstrlen > 128)
            {
                tmpstrlen = 128;
            }
            oscl_strncpy(argstr, tmpstr, tmpstrlen);
            argstr[tmpstrlen-1] = '\0';
        }

        // Do the string compare
        if (oscl_strcmp(argstr, "-help") == 0)
        {
            fprintf(aFile, "XML test results file option.  Default is to not write a summary.\n");
            fprintf(aFile, "  -xmloutput file\n");
            fprintf(aFile, "   Specify a source filename to output a test results summary to.\n");
        }
        else if (oscl_strcmp(argstr, "-xmloutput") == 0)
        {
            iFileFound = true;
            iFileArgument = ++iFileSearch;
            break;
        }
    }

    if (iFileFound)
    {
        // Convert to UTF8 if necessary
        if (cmdline_iswchar)
        {
            oscl_wchar* cmd;
            command_line->get_arg(iFileArgument, cmd);
            char tmpstr[256];
            oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), tmpstr, 256);
            tmpstr[255] = '\0';
            XmlTestResultsFilename = tmpstr;
        }
        else
        {
            char* cmdlinefilename = NULL;
            command_line->get_arg(iFileArgument, cmdlinefilename);
            XmlTestResultsFilename = cmdlinefilename;
        }
    }

}

void WriteInitialXmlSummary(OSCL_HeapString<OsclMemAllocator> &xmlresultsfile, FILE* aFile)
{
    // Only print an xml summary if requested.
    if (xmlresultsfile.get_size() > 0)
    {
        Oscl_File xmlfile(0);
        Oscl_FileServer iFileServer;
        iFileServer.Connect();
        if (0 == xmlfile.Open(xmlresultsfile.get_str(), Oscl_File::MODE_READWRITE | Oscl_File::MODE_TEXT, iFileServer))
        {
            xml_test_interpreter xml_interp;
            _STRING xml_results = xml_interp.unexpected_termination_interpretation("PV2WayEngineUnitTest");
            xmlfile.Write(xml_results.c_str(), sizeof(char), oscl_strlen(xml_results.c_str()));
            xmlfile.Close();
            iFileServer.Close();
        }
        else
        {
            fprintf(aFile, "ERROR: Failed to open XML test summary log file: %s!\n", xmlresultsfile.get_cstr());
        }
    }
}

void WriteFinalXmlSummary(OSCL_HeapString<OsclMemAllocator> &xmlresultsfile, const test_result& result, FILE* aFile)
{
    // Print out xml summary if requested
    if (xmlresultsfile.get_size() > 0)
    {
        Oscl_File xmlfile(0);
        Oscl_FileServer iFileServer;
        iFileServer.Connect();
        if (0 == xmlfile.Open(xmlresultsfile.get_str(), Oscl_File::MODE_READWRITE | Oscl_File::MODE_TEXT, iFileServer))
        {
            xml_test_interpreter xml_interp;
            _STRING xml_results = xml_interp.interpretation(result, "PV2WayEngineUnitTest");
            fprintf(aFile, "\nWrote XML test summary to: %s.\n", xmlresultsfile.get_cstr());
            xmlfile.Write(xml_results.c_str(), sizeof(char), oscl_strlen(xml_results.c_str()));
            xmlfile.Close();
            iFileServer.Close();
        }
        else
        {
            fprintf(aFile, "ERROR: Failed to open XML test summary log file: %s!\n", xmlresultsfile.get_cstr());
        }
    }
}
