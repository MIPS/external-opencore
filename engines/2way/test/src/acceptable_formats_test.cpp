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
#include "acceptable_formats_test.h"


void acceptable_formats_test::AllNodesAdded()
{
    LetConnectionRun();
}

void acceptable_formats_test::FinishTimerCallback()
{
    PV2WayUtil::OutputInfo("acceptable_formats_test::FinishTimerCallback()\n");
    bool match1 = false;
    bool match2 = false;
    bool match3 = false;
    bool match4 = false;

    PV2WayUtil::OutputInfo("\n Incoming Audio: ");
    match1 = iSourceAndSinks->FormatMatchesSelectedCodec(INCOMING,
             PV_AUDIO, iInAudFormatCapability[0].format);
    PV2WayUtil::OutputInfo("\n Outgoing Audio: ");
    match2 = iSourceAndSinks->FormatMatchesSelectedCodec(OUTGOING,
             PV_AUDIO, iOutAudFormatCapability[0].format);
    PV2WayUtil::OutputInfo("\n Incoming Video: ");
    match3 = iSourceAndSinks->FormatMatchesSelectedCodec(INCOMING,
             PV_VIDEO, iInVidFormatCapability[0].format);
    PV2WayUtil::OutputInfo("\n Outgoing Video: ");
    match4 = iSourceAndSinks->FormatMatchesSelectedCodec(OUTGOING,
             PV_VIDEO, iOutVidFormatCapability[0].format);
    test_is_true(match1 && match2 && match3 && match4);
    DisconnectSourceSinks();
}


//
//
//
void acceptable_formats_test::AddExpectedFormat(TPVDirection aDir,
        PV2WayMediaType aType,
        const char* const aFormat)
{
    if (PV_AUDIO == aType)
    {
        if (OUTGOING == aDir)
        {
            FormatCapabilityInfo cap;
            cap.format = aFormat;
            cap.dir = OUTGOING;

            iOutAudFormatCapability.push_back(cap);
        }
        else if (INCOMING == aDir)
        {
            FormatCapabilityInfo cap;
            cap.format = aFormat;
            cap.dir = INCOMING;
            iInAudFormatCapability.push_back(cap);
        }
    }
    else if (PV_VIDEO == aType)
    {
        if (OUTGOING == aDir)
        {
            FormatCapabilityInfo cap;
            cap.format = aFormat;
            cap.dir = OUTGOING;
            iOutVidFormatCapability.push_back(cap);
        }
        else if (INCOMING == aDir)
        {
            FormatCapabilityInfo cap;
            cap.format = aFormat;
            cap.dir = INCOMING;
            iInVidFormatCapability.push_back(cap);
        }
    }
}


void acceptable_formats_test::CreateParts()
{
    test_base::CreateParts();
}

void acceptable_formats_test::CommandCompleted(const PVCmdResponse& aResponse)
{
    PVCommandId cmdId = aResponse.GetCmdId();
    if (cmdId < 0)
        return;

    test_base::CommandCompleted(aResponse);
}
