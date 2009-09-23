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

#include "testcaseparser.h"
#include "oscl_utf8conv.h"

#define COMMENT_INDICATOR_CHAR '#'
#define COMMENT_INDICATOR_STRING "#"
#define WHITESPACE_STRING " \t\r\n"

#define FILE_INDICATOR_STRING "FILE "

#define NAME_INDICATOR_STRING "NAME "


#define TERMINAL_INDICATOR_STRING "TERMINAL "
#define TERMINALA_INDICATOR_STRING "A"
#define TERMINALB_INDICATOR_STRING "B"

#define H223MUX_INDICATOR_STRING "H.223MUXLEVEL "
#define H223MUX_LEVEL0_INDICATOR_STRING "0"
#define H223MUX_LEVEL1_INDICATOR_STRING "1"
#define H223MUX_LEVEL1DF_INDICATOR_STRING "1DF" /* Double flag */
#define H223MUX_LEVEL2_INDICATOR_STRING "2"
#define H223MUX_LEVEL2OH_INDICATOR_STRING "2OH" /* Optional header */
#define H223MUX_LEVEL3_INDICATOR_STRING "3"

#define H223_WNSRP "WNSRP "


#define AL_INDICATOR_STRING "AL "
#define AL3_ALLOW_INDICATOR_STRING "AL3ALLOW "
#define AL2_ALLOW_INDICATOR_STRING "AL2ALLOW "
#define AL1_ALLOW_INDICATOR_STRING "AL1ALLOW "
#define AL3_USE_INDICATOR_STRING "AL3USE "
#define AL2_USE_INDICATOR_STRING "AL2USE "
#define AL1_USE_INDICATOR_STRING "AL1USE "

#define AL2_SN_INDICATOR_STRING "AL2SN "
#define AL3_CFO_INDICATOR_STRING "AL3CFO "

#define MAXSDUSIZE_INDICATOR_STRING "MAXSDUSIZE"
#define MAXSDUSIZER_INDICATOR_STRING "MAXSDUSIZER"
#define MSD_INDICATOR_STRING "MSD"
#define MASTER_INDICATOR_STRING "MASTER"
#define SLAVE_INDICATOR_STRING "SLAVE"

#define CAPS_INDICATOR_STRING "CAPS"
#define RECEIVE_CAPS_INDICATOR_STRING "RECEIVE"
#define SEND_CAPS_INDICATOR_STRING "SEND"
#define AUDIO_AMR_INDICATOR_STRING "AMR"
#define AUDIO_AMRWB_INDICATOR_STRING "AMR-WB"
#define AUDIO_G7321_INDICATOR_STRING "G7321"
#define VIDEO_H263BL_INDICATOR_STRING "H.263BL"
#define VIDEO_MP4SP_INDICATOR_STRING "MP4SP"
#define VIDEO_H263P3_INDICATOR_STRING "H.263P3"
#define VIDEO_H264_INDICATOR_STRING "H.264"

#define AUDIO_INDICATOR_STRING "AUDIO"
#define VIDEO_INDICATOR_STRING "VIDEO"

// only for MPEG4 encoding
#define VIDEO_EXTENSION_DP_STRING "DP"
#define VIDEO_EXTENSION_RVLC_STRING "RVLC"

#define ON_INDICATOR_STRING "ON"
#define OFF_INDICATOR_STRING "OFF"

#define USER_INPUT_CAPABILITY_STRING "USERINPUTCAPABILITY"
#define RECEIVE_INDICATOR_STRING "RECEIVE"
#define TRANSMIT_INDICATOR_STRING "TRANSMIT"
#define RECEIVE_TRANSMIT_INDICATOR_STRING "RECEIVEANDTRANSMIT"

//FILE CAPABILITIES
#define FILE_CAPABILITES_INDICATOR_STRING "FILECAP"
#define FILE_CAPABILITIES_MEDIATYPE_STRING "TYPE "


#define AUDIO_SAMPLING_FREQUENCY_STRING "SAMPLE "
#define AUDIO_NUM_CHANNELS_STRING "CHANNELS "
#define AUDIO_LOOP_INPUT_STRING "ALOOP "
#define AUDIO_FRAME_RATE_SIMULATION_STRING "ASIMULATION "

#define VIDEO_TIMESCALE_STRING "TIMESCALE "
#define VIDEO_FRAME_HEIGHT_STRING "FRAMEHEIGHT "
#define VIDEO_FRAME_WIDTH_STRING "FRAMEWIDTH "
#define VIDEO_FRAMERATE_STRING "FRAMERATE "
#define VIDEO_LOOP_INPUT_STRING "VLOOP "
#define VIDEO_FRAME_RATE_SIMULATION_STRING "VSIMULATION "

//Configuration for GCF test
#define CONFIGURATION_INDICATOR_STRING "GCFTESTCASE"
#define CONNECTION_CAPABILITIES_TYPE_STRING "COMMNODE "

#define SOCKET_INDICATOR_STRING "SOCKET"
#define LOOPBACK_INDICATOR_STRING "LOOPBACK"
#define SOCKET_CONNECTION_TYPE_STRING "CONNECTIONTYPE "
#define SOCKET_CONNECTION_IP_STRING "IP "
#define SOCKET_CONNECTION_PORT_STRING "PORT "



#define FILENAME_LEN   300
#define EXTN_LEN       5

// does not read next line
const char* TestCaseParser::NextCharInString(const char* str)
{
    return skip_whitespace(str);
}

bool TestCaseParser::IsCommentLine(const char* line)
{
    char t = line[0];

    return (t == COMMENT_INDICATOR_CHAR);
}

bool TestCaseParser::IsBlankLine(const char* line)
{
    bool isBlank = false;
    int len = oscl_strlen(line);
    const char* lastWhiteSpace = skip_to_whitespace(line, line + len);
    if (line == NULL)
    {
        isBlank = true;
    }
    else if ((NextCharInString(line) == line) &&
             (lastWhiteSpace == line))
    {
        // the last char in the string is ' '
        isBlank = true;
    }
    else if ((lastWhiteSpace == line) &&
             (line[len - 1] == '\n'))
    {
        // there are blanks up to newline
        isBlank = true;
    }
    return isBlank;
}

//fgets(iLine, MAX_LINE_SIZE, fp);

// fill iLine with the next non-commented iLine
bool TestCaseParser::GetNextLine()
{
    if (!iFp)
        return false;
    bool foundValidLine = false;
    while (!foundValidLine && iFp &&
            fgets(iLine, MAX_LINE_SIZE, iFp))
    {
        // look at it and see if it is a comment
        // or a blank line
        if (!IsCommentLine(iLine) &&
                !IsBlankLine(iLine))
        {
            foundValidLine = true;
        }
    }
    if (!foundValidLine || !iFp)
    {
        fclose(iFp);
        iFp = NULL;
        return false;
    }
    else
    {
        // parse the line into first string and next string
        SetFirstAndNext();
        return true;
    }
}

bool TestCaseParser::MoreToGo()
{
    if (iFp)
        return true;
    return false;
}

// does not read next line
bool TestCaseParser::isLineType(const char* aType, const char* aStr)
{
    const char* type = skip_whitespace(aType);
    if (oscl_CIstrncmp(type, aStr, oscl_strlen(type)) == 0)
        return true;
    return false;
}

// does not read next line
TPVH223Level TestCaseParser::ParseMuxLevel(const char* aStr)
{
    TPVH223Level lMuxlvl = H223_LEVEL_UNKNOWN;
    if (isLineType(H223MUX_LEVEL0_INDICATOR_STRING, aStr))
    {
        lMuxlvl = H223_LEVEL0;
    }
    else if (isLineType(H223MUX_LEVEL1DF_INDICATOR_STRING, aStr))
    {
        lMuxlvl = H223_LEVEL1_DF;
    }
    else if (isLineType(H223MUX_LEVEL1_INDICATOR_STRING, aStr))
    {
        lMuxlvl = H223_LEVEL1;
    }
    else if (isLineType(H223MUX_LEVEL2OH_INDICATOR_STRING, aStr))
    {
        lMuxlvl = H223_LEVEL2_OH;
    }
    else if (isLineType(H223MUX_LEVEL2_INDICATOR_STRING, aStr))
    {
        lMuxlvl = H223_LEVEL2;
    }
    else if (isLineType(H223MUX_LEVEL3_INDICATOR_STRING, aStr))
    {
        lMuxlvl = H223_LEVEL3;
    }
    return lMuxlvl;
}

// does not read next line
bool TestCaseParser::GetBoolFromText(const char* iStr)
{
    if (isLineType(ON_INDICATOR_STRING, iStr))
        return true;
    return false;
}

// does not read next line
int TestCaseParser::GetIntFromText(const char* iStr)
{
    int result = 0;
    if (iStr != NULL)
    {
        sscanf(iStr, "%d", &result);
    }
    return result;
}

// does not read next line
bool TestCaseParser::ParseALSettings(AlSettings& aAlSettings)
{
    while (GetNextLine())
    {
        if (isLineType(AL3_ALLOW_INDICATOR_STRING, iFirstString))
        {
            bool result = GetBoolFromText(iNextBegin);
            aAlSettings.iALAllow[AL3] = OSCL_STATIC_CAST(int, result);
        }
        else if (isLineType(AL2_ALLOW_INDICATOR_STRING, iFirstString))
        {
            bool result = GetBoolFromText(iNextBegin);
            aAlSettings.iALAllow[AL2] = OSCL_STATIC_CAST(int, result);
        }
        else if (isLineType(AL1_ALLOW_INDICATOR_STRING, iFirstString))
        {
            bool result = GetBoolFromText(iNextBegin);
            aAlSettings.iALAllow[AL1] = OSCL_STATIC_CAST(int, result);
        }
        else if (isLineType(AL3_USE_INDICATOR_STRING, iFirstString))
        {
            bool result = GetBoolFromText(iNextBegin);
            aAlSettings.iALUse[AL3] = OSCL_STATIC_CAST(int, result);
        }
        else if (isLineType(AL2_USE_INDICATOR_STRING, iFirstString))
        {
            bool result = GetBoolFromText(iNextBegin);
            aAlSettings.iALUse[AL2] = OSCL_STATIC_CAST(int, result);
        }
        else if (isLineType(AL1_USE_INDICATOR_STRING, iFirstString))
        {
            bool result = GetBoolFromText(iNextBegin);
            aAlSettings.iALUse[AL1] = OSCL_STATIC_CAST(int, result);
        }
        else
        {
            break;
        }
    }
    return true;
}

// does not read next line
bool TestCaseParser::ParseMaxSDUSizes(SDUSizeSettings& aSizeSettings)
{
    bool parsed = true;
    GetNextLine();
    if (isLineType(AL3_ALLOW_INDICATOR_STRING, iFirstString))
    {
        int value = GetIntFromText(iNextBegin);
        aSizeSettings.iMaxSDUSize[AL3] = value;
    }
    else if (isLineType(AL2_ALLOW_INDICATOR_STRING, iFirstString))
    {
        int value = GetIntFromText(iNextBegin);
        aSizeSettings.iMaxSDUSize[AL2] = value;
    }
    else
    {
        parsed = false;
    }
    if (parsed)
    {
        GetNextLine();
    }
    return true;
}

// does not read next line
bool TestCaseParser::ParseAudioCapSettings(MediaTypes& aMediaTypes)
{
    aMediaTypes.iMediaTypes.clear();
    while (GetNextLine())
    {
        if (isLineType(AUDIO_AMRWB_INDICATOR_STRING, iLine))
        {
            aMediaTypes.iMediaTypes.push_back(PVMF_MIME_AMRWB);
        }
        else if (isLineType(AUDIO_AMR_INDICATOR_STRING, iLine))
        {
            aMediaTypes.iMediaTypes.push_back(PVMF_MIME_AMR_IF2);
        }
        else if (isLineType(AUDIO_G7321_INDICATOR_STRING, iLine))
        {
            aMediaTypes.iMediaTypes.push_back(PVMF_MIME_G723);
        }
        else
        {
            break;
        }
    }
    return true;
}

// does not read next line
bool TestCaseParser::ParseVideoCapSettings(MediaTypes& aMediaTypes)
{
    aMediaTypes.iMediaTypes.clear();
    while (GetNextLine())
    {
        if (isLineType(VIDEO_H263BL_INDICATOR_STRING, iLine))
        {
            aMediaTypes.iMediaTypes.push_back(PVMF_MIME_H2632000);
        }
        else if (isLineType(VIDEO_MP4SP_INDICATOR_STRING, iLine))
        {
            aMediaTypes.iMediaTypes.push_back(PVMF_MIME_M4V);
        }
        else if (isLineType(VIDEO_H263P3_INDICATOR_STRING, iLine))
        {
            //aMediaTypes.iMediaTypes.push_back(PVMF_H263_P3);
            // Commented as no p3 type exists
            aMediaTypes.iMediaTypes.push_back(PVMF_MIME_H2632000);
        }
        else
        {
            break;
        }
    }
    return true;
}

// MIGHT HAVE PARSED THE NEXT LINE!!
// returns true if the current line still needs evaluation
bool TestCaseParser::ParseCaps(CapSettings& aCapSettings)
{
    GetNextLine();
    if (isLineType(AUDIO_INDICATOR_STRING, iLine))
    {
        ParseAudioCapSettings(aCapSettings.iMediaCaps[AUDIO]);
    }
    if (isLineType(VIDEO_INDICATOR_STRING, iLine))
    {
        // while reading lines that belong to this type
        ParseVideoCapSettings(aCapSettings.iMediaCaps[VIDEO]);
    }

    // next line has already been read but has not been parsed.
    return true;
}

int TestCaseParser::ParseTerminalSettings(TerminalSettings& aTermSettings)
{
    GetNextLine();
    while (MoreToGo())
    {
        bool NeedToGetNextLine = true;
        if (isLineType(H223MUX_INDICATOR_STRING, iFirstString))
        {
            aTermSettings.iH223MuxLevel = ParseMuxLevel(iNextBegin);
        }
        else if (isLineType(AL_INDICATOR_STRING, iFirstString))
        {
            if (iNextBegin && isLineType(VIDEO_INDICATOR_STRING, iNextBegin))
            {
                if (ParseALSettings(aTermSettings.iAlSettings[VIDEO]))
                {
                    NeedToGetNextLine = false;
                }
            }
            if (iNextBegin && isLineType(AUDIO_INDICATOR_STRING, iNextBegin))
            {
                if (ParseALSettings(aTermSettings.iAlSettings[AUDIO]))
                {
                    // already got next line
                    NeedToGetNextLine = false;
                }
            }
        }
        else if (iNextBegin && isLineType(AL2_SN_INDICATOR_STRING, iFirstString))
        {
            aTermSettings.iAl2SequenceNumbers = GetIntFromText(iNextBegin);
        }
        else if (iNextBegin && isLineType(AL3_CFO_INDICATOR_STRING, iFirstString))
        {
            aTermSettings.iAl3ControlFieldOctets = GetIntFromText(iNextBegin);
        }
        else if (isLineType(TERMINAL_INDICATOR_STRING, iFirstString))
        {
            // need to jump out of this function and start over.
            return 1;
        }
        else if (isLineType(MAXSDUSIZE_INDICATOR_STRING, iFirstString))
        {
            if (ParseMaxSDUSizes(aTermSettings.iSDUSizeSettings[0]))
            {
                NeedToGetNextLine = false;
            }
        }
        else if (isLineType(MAXSDUSIZER_INDICATOR_STRING, iFirstString))
        {
            if (ParseMaxSDUSizes(aTermSettings.iSDUSizeSettings[1]))
            {
                NeedToGetNextLine = false;
            }
        }
        else if (isLineType(MSD_INDICATOR_STRING, iFirstString))
        {
            if (iNextBegin && isLineType(MASTER_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iMSD = PVT_MASTER;
            }
            else if (iNextBegin && isLineType(SLAVE_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iMSD = PVT_SLAVE;
            }
        }
        else if (isLineType(CAPS_INDICATOR_STRING, iFirstString))
        {
            GetNextLine();
            if (iFirstString && isLineType(RECEIVE_CAPS_INDICATOR_STRING, iFirstString))
            {
                if (ParseCaps(aTermSettings.iCapSettings[RECEIVE]))
                {
                    // if ParseCaps returns true => DO NOT need to getnext line (already have it)
                    NeedToGetNextLine = false;
                }
            }
            if (iFirstString && isLineType(SEND_CAPS_INDICATOR_STRING, iFirstString))
            {
                if (ParseCaps(aTermSettings.iCapSettings[SEND]))
                {
                    NeedToGetNextLine = false;
                }
            }
        }
        else if (isLineType(USER_INPUT_CAPABILITY_STRING, iFirstString))
        {
            if (iNextBegin && isLineType(TRANSMIT_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iInputCapability = TerminalSettings::Transmit;
            }
            else if (iNextBegin && isLineType(RECEIVE_TRANSMIT_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iInputCapability = TerminalSettings::ReceiveAndTransmit;
            }
            else if (iNextBegin && isLineType(RECEIVE_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iInputCapability = TerminalSettings::Receive;
            }
        }
        else if (isLineType(VIDEO_EXTENSION_DP_STRING, iFirstString))
        {
            if (isLineType(ON_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iDP = 1;
            }
            if (isLineType(OFF_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iDP = 0;
            }
        }
        else if (isLineType(VIDEO_EXTENSION_RVLC_STRING, iFirstString))
        {
            if (isLineType(ON_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iRVLC = 1;
            }
            if (isLineType(OFF_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iRVLC = 0;
            }
        }
        else if (isLineType(H223_WNSRP, iFirstString))
        {
            if (isLineType(ON_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iWNSRP = 1;
            }
            if (isLineType(OFF_INDICATOR_STRING, iNextBegin))
            {
                aTermSettings.iWNSRP = 0;
            }
        }
        else
        {
            printf("%s\n", iLine);
        }
        if (NeedToGetNextLine)
        {
            GetNextLine();
        }
    }
    return 0;
}

bool TestCaseParser::ParseTestFile(TestSettings::TestTerminalType terminalType,
                                   TestSettings& aSettings)
{
    bool ok = false;
    while (MoreToGo())
    {
        bool NeedToGetNextLine = true;
        if (isLineType(NAME_INDICATOR_STRING, iFirstString))
        {
            if (aSettings.iName)
            {
                OSCL_ARRAY_DELETE(aSettings.iName);
            }

            uint32 len = oscl_strlen(iNextBegin) + 1;
            aSettings.iName = OSCL_ARRAY_NEW(char, len);
            oscl_strncpy(aSettings.iName, iNextBegin, len - 1);
            // end with a null
            aSettings.iName[len - 1] = '\0';
        }
        else if (isLineType(TERMINAL_INDICATOR_STRING, iFirstString))
        {
            TestSettings::TestTerminalType lTermType;
            if (isLineType(TERMINALA_INDICATOR_STRING, iNextBegin))
                lTermType = TestSettings::TestTerminalA;
            else
                lTermType = TestSettings::TestTerminalB;
            if (lTermType == terminalType)
            {
                aSettings.iTerminalType = terminalType;
                if (ParseTerminalSettings(aSettings.iTerminalSettings))
                {
                    NeedToGetNextLine = false;
                }
                ok = true;
            }
        }
        if (NeedToGetNextLine)
        {
            GetNextLine();
        }
    }
    return ok;
}

bool TestCaseParser::OpenTestCaseFile(const char* aFileName)
{
    printf("\nProcessing File: %s\n", aFileName);
    iFp = fopen(aFileName, "r");
    if (iFp)
        return true;
    printf("\nERROR!  Could not open %s\n", aFileName);
    return false;
}

bool TestCaseParser::OpenConTestCaseFile(const oscl_wchar* aFileName)
{
    char configfile[FILENAME_LEN];

    oscl_UnicodeToUTF8(aFileName, oscl_strlen(aFileName), configfile, (FILENAME_LEN - EXTN_LEN));

    oscl_strcat(configfile, ".con");
    return OpenTestCaseFile(configfile);
}

bool TestCaseParser::SetFirstAndNext()
{
    iFirstString = OSCL_CONST_CAST(char*,
                                   skip_whitespace(iLine));
    iNextBegin = OSCL_CONST_CAST(char*,
                                 skip_to_whitespace(iFirstString, iFirstString + oscl_strlen(iFirstString)));
    if (iNextBegin)
    {
        iNextBegin = skip_whitespace(iNextBegin);
        return true;
    }
    return false;
}

bool TestCaseParser::ParseFile(const char* aFileName,
                               TestSettings::TestTerminalType terminalType,
                               TestSettings& aTestSettings)
{
    if (!OpenTestCaseFile(aFileName))
        return false;
    bool ok = false;

    FILE* fpSave = NULL;
    while (GetNextLine())
    {
        if (isLineType(FILE_INDICATOR_STRING, iFirstString))
        {
            fpSave = iFp;
            iFp = NULL;
            ok = ParseFile(iNextBegin, terminalType, aTestSettings);
            fclose(iFp);
            iFp = fpSave;
        }
        else
        {   // fill the various values

            ok = ParseTestFile(terminalType, aTestSettings);
        }
    }
    return ok;
}

bool TestCaseParser::ParseConFile(PvmiMIOFileInputSettings& aCapSettings)
{
    return ParseConFile(aCapSettings.iFileName.get_cstr(), aCapSettings);
}

bool TestCaseParser::ParseConFile(const oscl_wchar* aFileName, PvmiMIOFileInputSettings& aCapSettings)
{
    OpenConTestCaseFile(aFileName);

    GetNextLine();
    if (isLineType(FILE_CAPABILITES_INDICATOR_STRING, iLine))
    {
        while (GetNextLine())
        {
            if (isLineType(FILE_CAPABILITIES_MEDIATYPE_STRING, iFirstString))
            {
                if (isLineType(AUDIO_INDICATOR_STRING, iNextBegin))
                {
                    while (GetNextLine() && MoreToGo())
                    {
                        if (isLineType(AUDIO_SAMPLING_FREQUENCY_STRING, iFirstString))
                        {
                            aCapSettings.iSamplingFrequency = GetIntFromText(iNextBegin);
                            continue;
                        }
                        else if (isLineType(AUDIO_NUM_CHANNELS_STRING, iFirstString))
                        {
                            aCapSettings.iNumChannels = GetIntFromText(iNextBegin);
                            continue;

                        }
                        else if (isLineType(AUDIO_LOOP_INPUT_STRING, iFirstString))
                        {
                            if (isLineType(ON_INDICATOR_STRING, iNextBegin))
                            {
                                aCapSettings.iLoopInputFile = 1;
                            }
                            else
                            {
                                aCapSettings.iLoopInputFile = 0;
                            }
                            continue;
                        }

                    }
                }
                else if (isLineType(VIDEO_INDICATOR_STRING, iNextBegin))
                {
                    while (GetNextLine())
                    {
                        if (isLineType(VIDEO_TIMESCALE_STRING, iFirstString))
                        {
                            aCapSettings.iTimescale = GetIntFromText(iNextBegin);
                            continue;
                        }
                        else if (isLineType(VIDEO_FRAME_HEIGHT_STRING, iFirstString))
                        {
                            aCapSettings.iFrameHeight = GetIntFromText(iNextBegin);
                            continue;

                        }
                        else if (isLineType(VIDEO_FRAME_WIDTH_STRING, iFirstString))
                        {
                            aCapSettings.iFrameWidth = GetIntFromText(iNextBegin);
                            continue;
                        }
                        else if (isLineType(VIDEO_FRAMERATE_STRING, iFirstString))
                        {
                            aCapSettings.iFrameRate = atof(iNextBegin);
                            continue;
                        }
                        else if (isLineType(VIDEO_LOOP_INPUT_STRING, iFirstString))
                        {
                            if (isLineType(ON_INDICATOR_STRING, iNextBegin))
                            {
                                aCapSettings.iLoopInputFile = 1;
                            }
                            else
                            {
                                aCapSettings.iLoopInputFile = 0;
                            }
                            continue;
                        }

                    }
                }
            }
        }
        return true;
    }
    return false;
}

bool TestCaseParser::ParseConfigFile(const char* aFileName,
                                     Pv2wayCommConfig& aConSettings)
{
    OpenTestCaseFile(aFileName);

    GetNextLine();
    if (isLineType(CONFIGURATION_INDICATOR_STRING, iLine))
    {
        while (GetNextLine())
        {
            if (isLineType(TERMINAL_INDICATOR_STRING, iFirstString))
            {
                printf("%s\n", iLine);
                if (isLineType(TERMINALA_INDICATOR_STRING, iNextBegin))
                    aConSettings.iTestTerminal = TestSettings::TestTerminalA;
                else
                    aConSettings.iTestTerminal = TestSettings::TestTerminalB;
            }
            else if (isLineType(CONNECTION_CAPABILITIES_TYPE_STRING, iFirstString))
            {
                if (isLineType(SOCKET_INDICATOR_STRING, iNextBegin))
                {
                    printf("%s\n", iLine);
                    aConSettings.iCommNode = OSCL_ARRAY_NEW(char, 7);
                    oscl_strncpy(aConSettings.iCommNode, "SOCKET", 7);
                    while (GetNextLine())
                    {
                        if (isLineType(SOCKET_CONNECTION_TYPE_STRING, iFirstString))
                        {
                            printf("%s\n", iLine);
                            aConSettings.iConnectionType = OSCL_ARRAY_NEW(char, oscl_strlen(iNextBegin) + 1);
                            oscl_strncpy(aConSettings.iConnectionType, iNextBegin, oscl_strlen(iNextBegin));
                            continue;
                        }
                        else if (isLineType(SOCKET_CONNECTION_IP_STRING, iFirstString))
                        {
                            printf("%s\n", iLine);
                            aConSettings.iAddr = OSCL_ARRAY_NEW(char, oscl_strlen(iNextBegin) + 1);
                            oscl_strncpy(aConSettings.iAddr, iNextBegin, oscl_strlen(iNextBegin));
                            continue;

                        }
                        else if (isLineType(SOCKET_CONNECTION_PORT_STRING, iFirstString))
                        {
                            printf("%s\n", iLine);
                            aConSettings.iPort = GetIntFromText(iNextBegin);
                        }

                    }
                }
                else if (isLineType(LOOPBACK_INDICATOR_STRING, iNextBegin))
                {
                    printf("%s\n", iLine);
                    aConSettings.iCommNode = OSCL_ARRAY_NEW(char, 9);
                    oscl_strncmp(aConSettings.iCommNode, "LOOPBACK", 9);
                }
            }
        }
        return true;
    }
    return false;
}

