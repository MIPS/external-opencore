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
#ifndef PV_2WAY_ENGINE_H_INCLUDED
#include "pv_2way_engine.h"
#endif


#include "pvlogger.h"
#include "pv_2way_dec_data_channel_datapath.h"
#include "pv_2way_enc_data_channel_datapath.h"

#include "pv_mime_string_utils.h"


///////////////////////////////////////////////////////////////////
// CPV324m2Way::NegotiatedFormatsMatch
//
//
///////////////////////////////////////////////////////////////////
bool CPV324m2Way::NegotiatedFormatsMatch(
    Oscl_Vector<FormatCapabilityInfo, OsclMemAllocator>& aInAudFormatCapability,
    Oscl_Vector<FormatCapabilityInfo, OsclMemAllocator>& aOutAudFormatCapability,
    Oscl_Vector<FormatCapabilityInfo, OsclMemAllocator>& aInVidFormatCapability,
    Oscl_Vector<FormatCapabilityInfo, OsclMemAllocator>& aOutVidFormatCapability)
{
    // note NO logger messages can appear in this function.
    // it will cause problems with Symbian- can't access file from two threads
    bool result1 = true;
    bool result2 = true;
    bool result3 = true;
    bool result4 = true;


    iReadDataLock.Lock();
    PVMFFormatType formatEncAudio = iAudioEncDatapath->GetSourceSinkFormat();
    PVMFFormatType formatEncVideo = iVideoEncDatapath->GetSourceSinkFormat();
    PVMFFormatType formatDecAudio = iAudioEncDatapath->GetSourceSinkFormat();
    PVMFFormatType formatDecVideo = iVideoEncDatapath->GetSourceSinkFormat();

    uint totalSize = aOutAudFormatCapability.size();
    if (totalSize == 0)
    {
        return false;
    }
    FormatCapabilityInfo formatInfo = aOutAudFormatCapability[0];
    if (pv_mime_strcmp(formatEncAudio.getMIMEStrPtr(),
                       formatInfo.format.getMIMEStrPtr()) == 0)
    {
        result1 = true;
    }

    ////////////////////////////////////////////////
    totalSize = aOutVidFormatCapability.size();
    if (totalSize == 0)
    {
        return false;
    }
    formatInfo = aOutVidFormatCapability[0];
    if (pv_mime_strcmp(formatEncVideo.getMIMEStrPtr(),
                       formatInfo.format.getMIMEStrPtr()) == 0)
    {
        result2 = true;
    }

    ////////////////////////////////////////////////
    totalSize = aInAudFormatCapability.size();
    if (totalSize == 0)
    {
        return false;
    }
    formatInfo = aInAudFormatCapability[0];
    if (pv_mime_strcmp(formatDecVideo.getMIMEStrPtr(),
                       formatInfo.format.getMIMEStrPtr()) == 0)
    {
        result3 = true;
    }
    ////////////////////////////////////////////////
    totalSize = aInVidFormatCapability.size();
    if (totalSize == 0)
    {
        return false;
    }
    formatInfo = aInVidFormatCapability[0];
    if (pv_mime_strcmp(formatEncVideo.getMIMEStrPtr(),
                       formatInfo.format.getMIMEStrPtr()) == 0)
    {
        result4 = true;
    }
    ///////////////////////////////////////////////

    iReadDataLock.Unlock();

    return result1 && result2 && result3 && result4;
}

bool CPV324m2Way::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    if (QueryInterface(uuid, iface))
        return true;
    return false;
}

void CPV324m2Way::addRef()
{
    iReferenceCount++;
}

void CPV324m2Way::removeRef()
{
    if (--iReferenceCount <= 0)
    {
        OSCL_DELETE(this);
    }
}

