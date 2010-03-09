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
#include "lipsync_dummy_output_mio.h"
#include "pv_mime_string_utils.h"


#define LIPSYNCDUMMYOUTPUTMIO_LOGERROR(m)       PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_ERR,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGWARNING(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_WARNING,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGINFO(m)        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_INFO,m);
#define LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG(m)       PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_DEBUG,m);


LipSyncDummyOutputMIO::LipSyncDummyOutputMIO(const DummyMIOSettings& aSettings)
        : DummyOutputMIO(aSettings)
{
    iLogger = PVLogger::GetLoggerObject("LipSyncDummyOutputMIO");
    iParams = NULL;

}



LipSyncDummyOutputMIO::~LipSyncDummyOutputMIO()
{
    Cleanup();
}

PVMFStatus LipSyncDummyOutputMIO::connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver)
{
    DummyOutputMIO::connect(aSession, aObserver);
    iParams = ShareParams::Instance();
    iParams->iObserver = this;
    return PVMFSuccess;
}

PVMFCommandId LipSyncDummyOutputMIO::QueryInterface(const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    LIPSYNCDUMMYOUTPUTMIO_LOGDEBUG((0, "LipSyncDummyOutputMIO::QueryInterface()"));

    PVMFCommandId cmdid = iCommandCounter++;

    PVMFStatus status = PVMFFailure;
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = PVMFSuccess;
    }
    else if ((aUuid == PvmiClockExtensionInterfaceUuid) && (iIsAudioMIO))
    {
        PvmiClockExtensionInterface* myInterface = OSCL_STATIC_CAST(PvmiClockExtensionInterface*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = true;
    }
    else
    {
        status = PVMFFailure;
    }

    CommandResponse resp(status, cmdid, aContext);
    QueueCommandResponse(resp);
    return cmdid;
}


bool LipSyncDummyOutputMIO::queryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr)
{
    aInterfacePtr = NULL;
    bool status = false;
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = true;
    }
    else if ((aUuid == PvmiClockExtensionInterfaceUuid) && (iIsAudioMIO))
    {
        PvmiClockExtensionInterface* myInterface = OSCL_STATIC_CAST(PvmiClockExtensionInterface*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        status = true;
    }
    else
    {
        status = false;
    }

    return status;
}

PVMFStatus LipSyncDummyOutputMIO::SetClock(PVMFMediaClock* aClockVal)
{
    iRenderClock = aClockVal;
    return PVMFSuccess;
}

void LipSyncDummyOutputMIO::addRef()
{
    // nothing to do...
}

void LipSyncDummyOutputMIO::removeRef()
{
    // nothing to do...
}

void LipSyncDummyOutputMIO::MIOFramesTimeStamps(bool aIsAudio, uint32 aOrigTS, uint32 aRenderTS)
{
    if (aIsAudio)
    {
        /* Currently we are doing for video case TS .Need to develop audio case also */


    }
    else
    {
        iCompareTS[aRenderTS] = aOrigTS;

    }
}

void LipSyncDummyOutputMIO::DealWithData(PVMFTimestamp aTimestamp,
        uint8* aData, uint32 aDataLen)
{
    if (iMIOObserver)
    {
        uint32 origTS = 0;
        if (iIsAudioMIO && iIsCompressed)
        {
            oscl_memcpy((char *)&origTS, aData, sizeof(origTS));
            iMIOObserver->MIOFramesUpdate(true, aDataLen, (PVMFTimestamp)origTS);
        }
        else if (iIsVideoMIO && iIsCompressed)
        {
            oscl_memcpy((char *)&origTS, aData + 4, sizeof(origTS));
            iMIOObserver->MIOFramesUpdate(false, aDataLen, (PVMFTimestamp)origTS);

        }
        else if (iIsVideoMIO && !iIsCompressed)
        {

            Oscl_Map<uint32, uint32, OsclMemAllocator>::iterator it;
            it = iCompareTS.find(aTimestamp);
            if (!(it == iCompareTS.end()))
            {
                origTS = (((*it).second));
            }
            iMIOObserver->MIOFramesUpdate(false, aDataLen, (PVMFTimestamp)origTS);
        }
    }

    if (iIsAudioMIO)
    {
        uint32 timebfAdjust = 0;
        bool flag = false;
        uint32 currentTimeBase32 = 0;
        bool overflowFlag = false;
        uint32 timeVal = Oscl_Int64_Utils::get_uint64_lower32(aTimestamp);
        iRenderClock->GetCurrentTime32(timebfAdjust, flag, PVMF_MEDIA_CLOCK_MSEC, currentTimeBase32);

        // Adjust the render-clock to accurately represent this.
        PVMFMediaClockAdjustTimeStatus retVal;
        retVal = iRenderClock->AdjustClockTime32(timebfAdjust,
                 currentTimeBase32,
                 timeVal,
                 PVMF_MEDIA_CLOCK_MSEC, overflowFlag);
    }
}








