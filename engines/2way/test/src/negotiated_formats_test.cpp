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
#include "negotiated_formats_test.h"



void negotiated_formats_test::FinishTimerCallback()
{
    bool pass = false;

    // compare values to what we are expecting
    if (iTestConfigInterface)
    {

        bool match = iTestConfigInterface->NegotiatedFormatsMatch(iInAudFormatCapability,
                     iOutAudFormatCapability, iInVidFormatCapability, iOutVidFormatCapability);
        if (match)
        {
            pass = true;
        }
    }

    test_is_true(pass);


    DisconnectSourceSinks();

}


//
//
//
void negotiated_formats_test::AddExpectedFormat(TPVDirection aDir,
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


