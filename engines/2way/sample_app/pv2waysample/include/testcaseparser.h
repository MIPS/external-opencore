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
#ifndef TEST_CASE_PARSER_H_INCLUDED
#define TEST_CASE_PARSER_H_INCLUDED

#ifndef PVT_COMMON_H_INCLUDED
#include "pvt_common.h"
#endif

#ifndef PVMI_MIO_FILEINPUT_H_INCLUDED
#include "pvmi_mio_fileinput.h"
#endif



#define AUDIO 0
#define VIDEO 1

#define AL1 0
#define AL2 1
#define AL3 2

#define SEND 0
#define RECEIVE 1

#define AL_SETTINGS_NOT_SET -1
#define IP_ADDRESS_LENGTH 16

#define MAX_LINE_SIZE  255



struct AlSettings
{
    /* Is AL1,AL2,AL3 enabled or disabled */
    int iALAllow[3];
    int iALUse[3];
    AlSettings()
    {
        iALAllow[0] = iALAllow[1] = iALAllow[2] = AL_SETTINGS_NOT_SET;
        iALUse[0] = iALUse[1] = iALUse[2] = AL_SETTINGS_NOT_SET;
    }
};

struct SDUSizeSettings
{
    /* max SDU size for the various ALS*/
    unsigned int iMaxSDUSize[3];
    SDUSizeSettings()
    {
        memset(this, 0, sizeof(SDUSizeSettings));
    }
};

struct MediaTypes
{
    /* Supported media types in decreasing order of priority*/
    Oscl_Vector<PVMFFormatType, BasicAlloc> iMediaTypes;
    MediaTypes()
    {
        iMediaTypes.reserve(3);
    }
};

struct CapSettings
{
    /* This is cap for one direction
    one for audio and one for video */
    MediaTypes iMediaCaps[2];
};

struct TerminalSettings
{
    typedef enum
    {
        InputCapUndef,
        Receive,
        Transmit,
        ReceiveAndTransmit
    } UserInputCapability;

    /* The Mux level of the terminal*/
    TPVH223Level iH223MuxLevel;
    /* WNSRP on/off */
    int iWNSRP;
    /* Al Settings for Audio and Video */
    AlSettings iAlSettings[2];
    /* The SDu Size Settings*/
    SDUSizeSettings iSDUSizeSettings[2];
    /* Master Slave determination*/
    TPVMasterSlave iMSD;
    /* Cap settings for fwd and reverse dirs*/
    CapSettings iCapSettings[2];

    int iAl2SequenceNumbers ;
    int iAl3ControlFieldOctets;

    // for MPEG4 encoder Data Partitioning
    int iDP;
    int iRVLC;

    UserInputCapability iInputCapability;
    TerminalSettings()
    {
        iH223MuxLevel = H223_LEVEL_UNKNOWN;
        iWNSRP = PVT_NOT_SET;
        iMSD = PVT_MSD_INDETERMINATE;
        iAl2SequenceNumbers = PVT_NOT_SET;
        iAl3ControlFieldOctets = PVT_NOT_SET;
        iDP = PVT_NOT_SET;
        iRVLC = PVT_NOT_SET;
        iInputCapability = InputCapUndef;
    }

};

struct Olc
{

};
struct TestSettings
{

    typedef enum
    {
        TestTerminalUndef,
        TestTerminalA,
        TestTerminalB
    } TestTerminalType;

    typedef enum
    {
        TestMandatory,
        TestOptional
    } TestPriorityType;

    char *iName;
    TestPriorityType iPrio;
    TestTerminalType iTerminalType;
    TerminalSettings iTerminalSettings;

    TestSettings()
    {
        iName = 0;
        iPrio = TestMandatory;
        iTerminalType = TestTerminalUndef;
    }
    ~TestSettings()
    {
        if (iName)
        {
            OSCL_ARRAY_DELETE(iName);
        }
        iName = NULL;
    }
};


enum CommServerType
{
    NO_SERVER_TYPE_SET,
    PVMF_CAPI_ISDN_COMM_SERVER,
    PVMF_LOOPBACK_COMM_SERVER,
    SOCKET_COMM_SERVER
};

enum ConnectionType
{
    SERVER = 1,
    CLIENT
};

class Pv2wayCommConfig
{
    public:
        Pv2wayCommConfig()
                : iCommNode(NULL),
                iConnectionType(NULL),
                iAddr(NULL) {}
        char* iCommNode;
        char* iConnectionType;
        char* iAddr;
        uint32 iPort;
        TestSettings::TestTerminalType iTestTerminal;
};

class TestCaseParser
{
    public:
        TestCaseParser()
        {
            iFp = NULL;
            iLine[0] = '\0';
        }
        ~TestCaseParser()
        {
            if (iFp)
            {
                fclose(iFp);
                iFp = NULL;
            }
            iLine[0] = '\0';
        }
        bool ParseFile(const char *aFileName,
                       TestSettings::TestTerminalType terminalType,
                       TestSettings& aTestSettings);
        bool ParseConFile(const oscl_wchar *aFileName,
                          PvmiMIOFileInputSettings &aCapSettings);
        bool ParseConfigFile(const char* aFileName,
                             Pv2wayCommConfig& aConSettings);
        bool ParseConFile(PvmiMIOFileInputSettings& aCapSettings);
    protected:
        bool IsCommentLine(const char* line);
        bool IsBlankLine(const char* line);
        bool ParseTestFile(TestSettings::TestTerminalType terminalType,
                           TestSettings& aSettings);
        bool SetFirstAndNext();
        bool MoreToGo();
        bool OpenTestCaseFile(const char* aFileName);
        bool OpenConTestCaseFile(const oscl_wchar* aFileName);
        const char* NextCharInString(const char* str);
        bool GetNextLine();
        bool isLineType(const char* aSome, const char* aStr);
        TPVH223Level ParseMuxLevel(const char* aStr);
        bool GetBoolFromText(const char* iStr);
        int GetIntFromText(const char* iStr);
        bool ParseALSettings(AlSettings& aAlSettings);
        bool ParseMaxSDUSizes(SDUSizeSettings& aSizeSettings);
        bool ParseAudioCapSettings(MediaTypes& aMediaTypes);
        bool ParseVideoCapSettings(MediaTypes& aMediaTypes);
        bool ParseCaps(CapSettings& aCapSettings);
        int ParseTerminalSettings(TerminalSettings& aTermSettings);




        FILE* iFp;
        char iLine[MAX_LINE_SIZE];
        char* iFirstString;
        char* iNextBegin;

};

#endif
