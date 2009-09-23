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
#include "test_utility.h"

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
                    PV2WayUtil::OutputInfo("\nAutomated Call On\n");
                }
                else
                {
                    PV2WayUtil::OutputInfo("\nAutomated Call Off\n");
                }
                break;
            case 'l':
                if (main_engine->ChangeLoopbackCallSetting())
                {
                    PV2WayUtil::OutputInfo("\nLoopback Call On\n");
                    main_engine->CreateComponent();
                    //main_engine->Init();
                }
                else
                {
                    PV2WayUtil::OutputInfo("\nLoopback Call Off\n");
                }
                break;
            case 'd':
                PV2WayUtil::OutputInfo("\nDisabling MPC Rx and Tx modes ...\n");
                //iOptions.iDisableMpcRxModes=0xFF;
                //iOptions.iDisableMpcTxModes=0xFF;
                break;
#ifdef SUPPORT_ISDN
            case 'i':
                if (main_engine->SetCommServer(true))
                {
                    PV2WayUtil::OutputInfo("\nUsing ISDN\n");
                }
                else
                {
                    PV2WayUtil::OutputInfo("\nToo late to change to ISDN\n");
                }
                break;
#endif
            case 's':
            {
                PV2WayUtil::OutputInfo("\nUsing Sockets\n");
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
    PV2WayUtil::OutputInfo("\n Please Enter Terminal Type Client(c)/Server(s):");
    int t = getchar();
    int port = 0;

    if (tolower(t) == 's')
    {
        PV2WayUtil::OutputInfo("\n Please Enter IP Address of this machine: ");
        char ipAddr[IP_ADDRESS_LENGTH];
        scanf("%s", ipAddr);
        PV2WayUtil::OutputInfo("\n Please enter server port:");
        scanf("%d", &port);

        if (!main_engine->ConnectSocket(true, port, ipAddr))
        {
            PV2WayUtil::OutputInfo("\n Error connecting server socket!\n");
        }
    }
    if (tolower(t) == 'c')
    {
        PV2WayUtil::OutputInfo("\n Please Enter IP Address: ");
        char ipAddr[IP_ADDRESS_LENGTH];
        scanf("%s", ipAddr);
        PV2WayUtil::OutputInfo("\n Please enter peer port:");
        scanf("%d", &port);

        if (!main_engine->ConnectSocket(false, port, ipAddr))
        {
            PV2WayUtil::OutputInfo("\n Error connecting client socket!\n");
        }
    }
}

void print_commands()
{
    PV2WayUtil::OutputInfo("\n1 - Query H324M Config Interface");
    PV2WayUtil::OutputInfo("\n2 - Initialize 2way");
    PV2WayUtil::OutputInfo("\n3 - Connect 2way");
    PV2WayUtil::OutputInfo("\n4 - Remove Audio sink");
    PV2WayUtil::OutputInfo("\n5 - Remove Audio Source");
    PV2WayUtil::OutputInfo("\n6 - Remove Video Sink");
    PV2WayUtil::OutputInfo("\n7 - Remove Video Source");
    PV2WayUtil::OutputInfo("\n8 - Disconnect 2way");
    PV2WayUtil::OutputInfo("\n9 - Reset 2way");
    PV2WayUtil::OutputInfo("\na - Automate Call");
    PV2WayUtil::OutputInfo("\nl - Loopback Call");
#ifdef SUPPORT_ISDN
    PV2WayUtil::OutputInfo("\ni - Use Isdn");
#endif
    PV2WayUtil::OutputInfo("\ns - Use Sockets");
    PV2WayUtil::OutputInfo("\nu - Send user input alphanumeric");
    PV2WayUtil::OutputInfo("\ne - Send EndSession");
    PV2WayUtil::OutputInfo("\nv - disable use video over AL2");
    PV2WayUtil::OutputInfo("\nw - disable use video over AL3");
    PV2WayUtil::OutputInfo("\ny - disallow video over AL2");
    PV2WayUtil::OutputInfo("\nz - disallow video over AL3");
    PV2WayUtil::OutputInfo("\nh - Help");
    PV2WayUtil::OutputInfo("\nx - Exit");
    PV2WayUtil::OutputInfo("\nEnter Command: ");
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
            PV2WayUtil::OutputInfo("\n###################Memory Stats Start#################\n");
            PV2WayUtil::OutputInfo("  numBytes %d\n", stats->numBytes);
            PV2WayUtil::OutputInfo("  peakNumBytes %d\n", stats->peakNumBytes);
            PV2WayUtil::OutputInfo("  numAllocs %d\n", stats->numAllocs);
            PV2WayUtil::OutputInfo("  peakNumAllocs %d\n", stats->peakNumAllocs);
            PV2WayUtil::OutputInfo("  numAllocFails %d\n", stats->numAllocFails);
            PV2WayUtil::OutputInfo("  totalNumAllocs %d\n", stats->totalNumAllocs);
            PV2WayUtil::OutputInfo("  totalNumBytes %d\n", stats->totalNumBytes);
            PV2WayUtil::OutputInfo("\n###################Memory Stats End###################\n");
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
        PV2WayUtil::OutputInfo("ERROR: %d Memory leaks detected!\n", leaks);
        MM_AllocQueryInfo*info = auditCB.pAudit->MM_CreateAllocNodeInfo(leaks);
        uint32 leakinfo = auditCB.pAudit->MM_GetAllocNodeInfo(info, leaks, 0);
        if (leakinfo != leaks)
        {
            PV2WayUtil::OutputInfo("ERROR: Leak info is incomplete.\n");
        }
        for (uint32 i = 0; i < leakinfo; i++)
        {
            PV2WayUtil::OutputInfo("Leak Info:\n");
            PV2WayUtil::OutputInfo("  allocNum %d\n", info[i].allocNum);
            PV2WayUtil::OutputInfo("  fileName %s\n", info[i].fileName);
            PV2WayUtil::OutputInfo("  lineNo %d\n", info[i].lineNo);
            PV2WayUtil::OutputInfo("  size %d\n", info[i].size);
            uint32 ptrAddr = (uint32)info[i].pMemBlock;
            PV2WayUtil::OutputInfo("  pMemBlock 0x%x\n", ptrAddr);
            PV2WayUtil::OutputInfo("  tag %s\n", info[i].tag);
        }
        auditCB.pAudit->MM_ReleaseAllocNodeInfo(info);
    }
#endif
}

int local_main(FILE* filehandle, cmd_line *command_line)
{
    OSCL_UNUSED_ARG(command_line);
//   int result;
    global_cmd_line = command_line;

    PV2WayUtil::SetFileHandle(filehandle);

    CPV2WayEngineFactory::Init();
    OsclErrorTrap::Init();
    OsclScheduler::Init("PV2WayEngineFactory");

    main_engine = OSCL_NEW(engine_handler, ());

    //filename will be pointing to a valid string if -config or -gfctest are present
    char *filename = NULL;

    OSCL_HeapString<OsclMemAllocator> argument = "-config";
    filename = PV2WayUtil::FindConfigGfc(global_cmd_line, argument);
    //check if config file was found
    if ((filename == NULL) || (main_engine->ReadConfigFile(filename)))
    {
        PV2WayUtil::OutputInfo("config file: %s not found", filename);
    }
    //free memory if allocated
    if (filename != NULL)
    {
        OSCL_ARRAY_DELETE(filename);
        filename = NULL;
    }


    argument = "-gfctest";
    filename = PV2WayUtil::FindConfigGfc(global_cmd_line, argument);
    //check if gfc file was found
    if ((filename == NULL) || (main_engine->ReadGCFTestFile(filename)))
    {
        PV2WayUtil::OutputInfo("gfc file: %s not found", filename);
    }
    //free memory if allocated
    if (filename != NULL)
    {
        OSCL_ARRAY_DELETE(filename);
        filename = NULL;
    }

    // printf("Start 2Way, h for help\n");

    int32 leave = 0;

    OSCL_TRY(leave, main_engine->start());

    PV2WayUtil::OutputInfo("\nEngine handler exiting. Leave = %d\n", leave);

    OSCL_TRY(leave, OSCL_DELETE(main_engine););
    OsclScheduler::Cleanup();
    OsclErrorTrap::Cleanup();
    PVLogger::Cleanup();
#if !(OSCL_BYPASS_MEMMGT)
    PV2WayUtil::OutputInfo("\nMemory Stats after engine deletion\n");
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

