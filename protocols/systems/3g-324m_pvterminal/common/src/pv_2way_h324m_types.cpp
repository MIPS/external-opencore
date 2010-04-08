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
#include "pv_2way_h324m_types.h"

OSCL_EXPORT_REF CPVUserInput::CPVUserInput(): iRefCounter(1) {}

OSCL_EXPORT_REF CPVUserInputDtmf::CPVUserInputDtmf(uint8 aInput, bool aUpdate, uint16 aDuration) :
        iInput(aInput),
        iIsUpdate(aUpdate),
        iDuration(aDuration)
{}

OSCL_EXPORT_REF CPVUserInputDtmf::~CPVUserInputDtmf()
{}

OSCL_EXPORT_REF PV2WayUserInputType CPVUserInputDtmf::GetType()
{
    return PV_DTMF;
}

OSCL_EXPORT_REF uint8 CPVUserInputDtmf::GetInput()
{
    return iInput;
}

OSCL_EXPORT_REF bool CPVUserInputDtmf::IsUpdate()
{
    return iIsUpdate;
}

OSCL_EXPORT_REF uint16 CPVUserInputDtmf::GetDuration()
{
    return iDuration;
}

OSCL_EXPORT_REF CPVUserInputAlphanumeric::CPVUserInputAlphanumeric(uint8* apInput, uint16 aLen)
{
    if (aLen)
    {
        int err;
        OSCL_TRY(err, ipInput = OSCL_STATIC_CAST(uint8*, OSCL_MALLOC(aLen)));
        OSCL_FIRST_CATCH_ANY(err, return;);

        OSCL_TRY(err, oscl_memcpy(ipInput, apInput, aLen));
        OSCL_FIRST_CATCH_ANY(err, OSCL_FREE(ipInput); return;);
        iLength = aLen;
    }
}

OSCL_EXPORT_REF CPVUserInputAlphanumeric::~CPVUserInputAlphanumeric()
{
    if (ipInput)
    {
        OSCL_DELETE(ipInput);
    }
}

OSCL_EXPORT_REF PV2WayUserInputType CPVUserInputAlphanumeric::GetType()
{
    return PV_ALPHANUMERIC;
}

OSCL_EXPORT_REF uint8* CPVUserInputAlphanumeric::GetInput()
{
    return ipInput;
}

OSCL_EXPORT_REF uint16 CPVUserInputAlphanumeric::GetLength()
{
    return iLength;
}


OSCL_EXPORT_REF CPVLogicalChannelIndication::CPVLogicalChannelIndication(TPVChannelId aChannelId) :
        iRefCounter(1),
        iChannelId(aChannelId)
{}

OSCL_EXPORT_REF CPVLogicalChannelIndication::~CPVLogicalChannelIndication()
{}

OSCL_EXPORT_REF TPVChannelId CPVLogicalChannelIndication::GetChannelId()
{
    return iChannelId;
}

OSCL_EXPORT_REF void CPVLogicalChannelIndication::addRef()
{
    iRefCounter++;
}

OSCL_EXPORT_REF void CPVLogicalChannelIndication::removeRef()
{
    --iRefCounter;
    if (iRefCounter == 0)
        OSCL_DELETE(this);
}


OSCL_EXPORT_REF CPVVideoSpatialTemporalTradeoff::CPVVideoSpatialTemporalTradeoff(TPVChannelId aChannelId,
        uint8 aTradeoff) :
        iRefCounter(1),
        iChannelId(aChannelId),
        iTradeoff(aTradeoff)
{}

OSCL_EXPORT_REF CPVVideoSpatialTemporalTradeoff::~CPVVideoSpatialTemporalTradeoff()
{}

OSCL_EXPORT_REF TPVChannelId CPVVideoSpatialTemporalTradeoff::GetChannelId()
{
    return iChannelId;
}

OSCL_EXPORT_REF uint8 CPVVideoSpatialTemporalTradeoff::GetTradeoff()
{
    return iTradeoff;
}

OSCL_EXPORT_REF void CPVVideoSpatialTemporalTradeoff::addRef()
{
    iRefCounter++;
}

OSCL_EXPORT_REF void CPVVideoSpatialTemporalTradeoff::removeRef()
{
    --iRefCounter;
    if (iRefCounter == 0)
        OSCL_DELETE(this);
}


