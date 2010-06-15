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
#include "oscl_error.h"
#include "oscl_timer.h"
#include "oscl_mem.h"
#include "oscl_scheduler.h"
#include "oscl_utf8conv.h"

#ifndef OSCL_STRING_UTILS_H_INCLUDED
#include "oscl_string_utils.h"
#endif

#include "test_utility.h"

FILE * PV2WayUtil::iFileHandle = NULL;

OSCL_EXPORT_REF void PV2WayUtil::OutputInfo(const char * str, ...)
{
    va_list vargs;          //create the variable list of arguments that could be passed to this function
    va_start(vargs, str);   //make the list to point to the argument that follows str
    if (iFileHandle != NULL)
    {
        vfprintf(iFileHandle, str, vargs);
        fflush(iFileHandle);
    }
}

OSCL_EXPORT_REF void PV2WayUtil::SetFileHandle(const FILE *aFileHandle)
{
    if (aFileHandle != NULL)
        iFileHandle = OSCL_STATIC_CAST(FILE *, aFileHandle);
}

OSCL_EXPORT_REF FILE* PV2WayUtil::GetFileHandle()
{
    return iFileHandle;
}

void PV2WayUtil::FindTestRange(cmd_line* command_line,
                               uint32 &aFirstTest,
                               uint32 &aLastTest,
                               int32 aTestLimit)
{
    //default is to run all tests.
    aFirstTest = 0;
    if (aTestLimit == 0)
    {
        aLastTest = MAX_324_TEST;
    }
    else
    {
        aLastTest = aTestLimit;
    }

    int testArgument = 0;
    char *iTestArgStr1 = NULL;
    char *iTestArgStr2 = NULL;
    bool cmdline_iswchar = command_line->is_wchar();
    //-test
    //if (iTestFound)
    OSCL_HeapString<OsclMemAllocator> argument = "-test";
    if (FindCmdArg(command_line, argument, testArgument))
    {
        // Convert to UTF8 if necessary
        if (cmdline_iswchar)
        {
            iTestArgStr1 = OSCL_ARRAY_NEW(char, 256);
            OSCL_TCHAR* cmd;
            command_line->get_arg(testArgument, cmd);
            if (cmd)
            {
                oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iTestArgStr1, 256);
            }

            iTestArgStr2 = OSCL_ARRAY_NEW(char, 256);
            command_line->get_arg(testArgument + 1, cmd);
            if (cmd)
            {
                oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iTestArgStr2, 256);
            }
        }
        else
        {
            command_line->get_arg(testArgument, iTestArgStr1);
            command_line->get_arg(testArgument + 1, iTestArgStr2);
        }

        //Pull out 2 integers...
        PV_atoi(iTestArgStr1, 'd', aFirstTest);
        PV_atoi(iTestArgStr2, 'd', aLastTest);
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
    }
}

//This function will look for any particular pattern in the cmd line parameters, if it's found then
//it will return true and alse ArgCount will be equal to the position of the current argument
bool PV2WayUtil::FindCmdArg(cmd_line* command_line, OSCL_HeapString<OsclMemAllocator> &aArgument, int &ArgCount)
{
    bool cmdline_iswchar = command_line->is_wchar();
    int count = command_line->get_count();
    if (count == 0)
    {
        // there were no command line options
        PV2WayUtil::OutputInfo("Start 2Way, h for help\n");
    }

    char *sourceFind;
    if (cmdline_iswchar)
    {
        sourceFind = OSCL_ARRAY_NEW(char, 256);
    }
    else
    {
        sourceFind = NULL;
    }
    int configSearch = 0;
    while (configSearch < count)
    {
        //this flag will be true in case aCommand is found
        bool configFound = false;
        // Go through each argument
        for (; configSearch < count; configSearch++)
        {
            // Convert to UTF8 if necessary
            if (cmdline_iswchar)
            {
                OSCL_TCHAR* cmd = NULL;
                command_line->get_arg(configSearch, cmd);
                oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), sourceFind, 256);
            }
            else
            {
                sourceFind = NULL;
                command_line->get_arg(configSearch, sourceFind);
            }

            // Do the string compare
            if (oscl_strcmp(sourceFind, "-help") == 0)
            {
                PV2WayUtil::OutputInfo("Test cases to run option. Default is ALL:\n");
                PV2WayUtil::OutputInfo("  -test x y\n");
                PV2WayUtil::OutputInfo("   Specify a range of test cases to run. To run one test case, use the\n");
                PV2WayUtil::OutputInfo("   same index for x and y.\n");
                PV2WayUtil::OutputInfo("  -test G\n");
                PV2WayUtil::OutputInfo("   Run 324M test cases only.\n");
                PV2WayUtil::OutputInfo("  -gcftest <gcf testfile name>\n");
                PV2WayUtil::OutputInfo("   Specify file name of gcf test cases to run. \n");
                PV2WayUtil::OutputInfo("  -test \n");
                PV2WayUtil::OutputInfo("   Run  menu based 324M test cases only.\n");
                return false;
            }
            else if (oscl_strcmp(sourceFind, aArgument.get_cstr()) == 0)
            {
                configFound = true;
                ArgCount = ++configSearch;
                break;
            }
        }
        if (cmdline_iswchar)
        {
            OSCL_ARRAY_DELETE(sourceFind);
            sourceFind = NULL;
        }
        if (configFound)
            return true;
    }
    return false;
}


//this helper function tries to find either -config or -gfctest and it will return the filename
char* PV2WayUtil::FindConfigGfc(cmd_line* command_line, OSCL_HeapString<OsclMemAllocator> &aCommand)
{
    int NextArg = 0;
    if (FindCmdArg(command_line, aCommand, NextArg))
    {
        // Convert to UTF8 if necessary
        char * configFileName = NULL;
        bool cmdline_iswchar = command_line->is_wchar();
        if (cmdline_iswchar)
        {
            configFileName = OSCL_ARRAY_NEW(char, 256);
            OSCL_TCHAR* cmd = NULL;
            command_line->get_arg(NextArg, cmd);
            if (cmd)
            {
                oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), configFileName, 256);
            }
        }
        else
        {
            command_line->get_arg(NextArg, configFileName);
        }
        return configFileName;
    }
    else
        return NULL;
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

