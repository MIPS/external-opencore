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
#include "main.h"
#include "console_engine_handler.h"
#include "oscl_utf8conv.h"
#include "oscl_string.h"
#include "oscl_stdstring.h"

#if defined(__linux__) || defined(linux)
#include "kbhit.h"
#endif

#define IP_ADDRESS_LENGTH   16

engine_handler *main_engine;
FILE* fileoutput;
cmd_line *global_cmd_line;

void evaluate_command()
{
    if (_kbhit())
    {
        int c = 0;
        c = getchar();
        switch (tolower(c))
        {
            case '1':
                main_engine->CreateComponent();
                break;
            case '2':
                main_engine->Init();
                break;
            case '3':
                main_engine->Connect();
                break;
            case '4':
                main_engine->RemoveAudioSink();
                break;
            case '5':
                main_engine->RemoveAudioSource();
                break;
            case '6':
                main_engine->RemoveVideoSink();
                break;
            case '7':
                main_engine->RemoveVideoSource();
                break;
            case '8':
                main_engine->Disconnect();
                break;
            case '9':
                main_engine->Reset();
                break;

            case 'a':
                if (main_engine->ChangeAutomatedCallSetting())
                {
                    printf("\nAutomated Call On\n");
                }
                else
                {
                    printf("\nAutomated Call Off\n");
                }
                break;
            case 'l':
                if (main_engine->ChangeLoopbackCallSetting())
                {
                    printf("\nLoopback Call On\n");
                    main_engine->CreateComponent();
                    //main_engine->Init();
                }
                else
                {
                    printf("\nLoopback Call Off\n");
                }
                break;
            case 'd':
                printf("\nDisabling MPC Rx and Tx modes ...\n");
                //iOptions.iDisableMpcRxModes=0xFF;
                //iOptions.iDisableMpcTxModes=0xFF;
                break;
#ifdef SUPPORT_ISDN
            case 'i':
                if (main_engine->SetCommServer(true))
                {
                    printf("\nUsing ISDN\n");
                }
                else
                {
                    printf("\nToo late to change to ISDN\n");
                }
                break;
#endif
            case 's':
            {
                printf("\nUsing Sockets\n");
                ConnectSocket();
            }
            break;

            case 'x':
                main_engine->StopScheduler();
                break;

            case 'h':
                print_commands();
                break;

            case 'u':
                main_engine->user_input();
                break;

            case 'e':
                main_engine->send_end_session();
                break;
            case 'v':
                main_engine->UseVideoOverAl2(false);
                break;
            case 'w':
                main_engine->UseVideoOverAl3(false);
                break;
            case 'y':
                main_engine->AllowVideoOverAl2(false);
                break;
            case 'z':
                main_engine->AllowVideoOverAl3(false);
                break;
            default:
                break;
        }
    }
}

void ConnectSocket()
{
    printf("\n Please Enter Terminal Type Client(c)/Server(s):");
    int t = getchar();
    int port = 0;

    if (tolower(t) == 's')
    {
        printf("\n Please Enter IP Address of this machine: ");
        char ipAddr[IP_ADDRESS_LENGTH];
        scanf("%s", ipAddr);
        printf("\n Please enter server port:");
        scanf("%d", &port);

        if (!main_engine->ConnectSocket(true, port, ipAddr))
        {
            printf("\n Error connecting server socket!\n");
        }
    }
    if (tolower(t) == 'c')
    {
        printf("\n Please Enter IP Address: ");
        char ipAddr[IP_ADDRESS_LENGTH];
        scanf("%s", ipAddr);
        printf("\n Please enter peer port:");
        scanf("%d", &port);

        if (!main_engine->ConnectSocket(false, port, ipAddr))
        {
            printf("\n Error connecting client socket!\n");
        }
    }
}

void print_commands()
{
    printf("\n1 - Query H324M Config Interface");
    printf("\n2 - Initialize 2way");
    printf("\n3 - Connect 2way");
    printf("\n4 - Remove Audio sink");
    printf("\n5 - Remove Audio Source");
    printf("\n6 - Remove Video Sink");
    printf("\n7 - Remove Video Source");
    printf("\n8 - Disconnect 2way");
    printf("\n9 - Reset 2way");
    printf("\na - Automate Call");
    printf("\nl - Loopback Call");
#ifdef SUPPORT_ISDN
    printf("\ni - Use Isdn");
#endif
    printf("\ns - Use Sockets");
    printf("\nu - Send user input alphanumeric");
    printf("\ne - Send EndSession");
    printf("\nv - disable use video over AL2");
    printf("\nw - disable use video over AL3");
    printf("\ny - disallow video over AL2");
    printf("\nz - disallow video over AL3");
    printf("\nh - Help");
    printf("\nx - Exit");
    printf("\nEnter Command: ");
}


void MemoryStats()
{
#if !(OSCL_BYPASS_MEMMGT)

    OsclAuditCB auditCB;
    OsclMemInit(auditCB);
    if (auditCB.pAudit)
    {
        MM_Stats_t* stats = auditCB.pAudit->MM_GetStats("");
        if (stats)
        {
            printf("\n###################Memory Stats Start#################\n");
            printf("  numBytes %d\n", stats->numBytes);
            printf("  peakNumBytes %d\n", stats->peakNumBytes);
            printf("  numAllocs %d\n", stats->numAllocs);
            printf("  peakNumAllocs %d\n", stats->peakNumAllocs);
            printf("  numAllocFails %d\n", stats->numAllocFails);
            printf("  totalNumAllocs %d\n", stats->totalNumAllocs);
            printf("  totalNumBytes %d\n", stats->totalNumBytes);
            printf("\n###################Memory Stats End###################\n");
        }

    }

#endif
}

void checkForLeaks()
{
#if !(OSCL_BYPASS_MEMMGT)
    //Check for memory leaks before cleaning up OsclMem.
    OsclAuditCB auditCB;
    OsclMemInit(auditCB);
    uint32 leaks = auditCB.pAudit->MM_GetNumAllocNodes();
    if (leaks != 0)
    {
        printf("ERROR: %d Memory leaks detected!\n", leaks);
        MM_AllocQueryInfo*info = auditCB.pAudit->MM_CreateAllocNodeInfo(leaks);
        uint32 leakinfo = auditCB.pAudit->MM_GetAllocNodeInfo(info, leaks, 0);
        if (leakinfo != leaks)
        {
            printf("ERROR: Leak info is incomplete.\n");
        }
        for (uint32 i = 0; i < leakinfo; i++)
        {
            printf("Leak Info:\n");
            printf("  allocNum %d\n", info[i].allocNum);
            printf("  fileName %s\n", info[i].fileName);
            printf("  lineNo %d\n", info[i].lineNo);
            printf("  size %d\n", info[i].size);
            uint32 ptrAddr = (uint32)info[i].pMemBlock;
            printf("  pMemBlock 0x%x\n", ptrAddr);
            printf("  tag %s\n", info[i].tag);
        }
        auditCB.pAudit->MM_ReleaseAllocNodeInfo(info);
    }
#endif
}
void FindTestRange(cmd_line* command_line,
                   FILE* aFile)
{

    int iTestArgument = 0;
    char *iTestArgStr1 = NULL;
    char *iTestArgStr2 = NULL;
    bool cmdline_iswchar = command_line->is_wchar();

    int count = command_line->get_count();
    if (count == 0)
    {
        // there were no command line options
        printf("Start 2Way, h for help\n");
    }
    // Search for the "-gcftest" argument
    char *iSourceFind = NULL;
    if (cmdline_iswchar)
    {
        iSourceFind = OSCL_ARRAY_NEW(char, 256);
    }

    int iTestSearch = 0;
    while (iTestSearch < count)
    {
        bool iTestFound = false;
        bool iTestFound1 = false;
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
                fprintf(aFile, "  -gcftest <gcf testfile name>\n");
                fprintf(aFile, "   Specify file name of gcf test cases to run. \n");

                fprintf(aFile, "  -test \n");
                fprintf(aFile, "   Run  menu based 324M test cases only.\n");



                exit(0);
            }
            else if (oscl_strcmp(iSourceFind, "-gcftest") == 0)
            {
                iTestFound1 = true;
                iTestArgument = ++iTestSearch;
                break;
            }
            else if (oscl_strcmp(iSourceFind, "-config") == 0)
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

            }
            else
            {
                command_line->get_arg(iTestArgument, iTestArgStr1);

            }

            if (!iTestArgStr1 || !main_engine->ReadConfigFile(iTestArgStr1))
            {
                printf("file: %s not found", iTestArgStr1);
            }
        }
        if (iTestFound1)
        {
            // Convert to UTF8 if necessary
            if (cmdline_iswchar)
            {
                iTestArgStr2 = OSCL_ARRAY_NEW(char, 256);
                OSCL_TCHAR* cmd;
                command_line->get_arg(iTestArgument + 1, cmd);
                if (cmd)
                {
                    oscl_UnicodeToUTF8(cmd, oscl_strlen(cmd), iTestArgStr2, 256);
                }
            }
            else
            {
                command_line->get_arg(iTestArgument, iTestArgStr2);
            }

            if (!iTestArgStr2 || main_engine->ReadGCFTestFile(iTestArgStr2))
            {
                printf("file: %s not found", iTestArgStr2);
            }

        }
    }

}

int local_main(FILE* filehandle, cmd_line *command_line)
{
    OSCL_UNUSED_ARG(command_line);
//   int result;
    global_cmd_line = command_line;

    fileoutput = filehandle;

    CPV2WayEngineFactory::Init();
    OsclErrorTrap::Init();
    OsclScheduler::Init("PV2WayEngineFactory");

    main_engine = OSCL_NEW(engine_handler, ());

    FindTestRange(global_cmd_line, fileoutput);

    // printf("Start 2Way, h for help\n");

    int32 leave = 0;

    OSCL_TRY(leave, main_engine->start());

    printf("\nEngine handler exiting. Leave = %d\n", leave);

    OSCL_TRY(leave, OSCL_DELETE(main_engine););
    OsclScheduler::Cleanup();
    OsclErrorTrap::Cleanup();
    PVLogger::Cleanup();
#if !(OSCL_BYPASS_MEMMGT)
    printf("\nMemory Stats after engine deletion\n");
    MemoryStats();
    checkForLeaks();
#endif
    return 0;

}

#if (LINUX_MAIN==1)

int main(int argc, char *argv[])
{
    local_main(stdout, NULL);
    return 0;
}

#endif

