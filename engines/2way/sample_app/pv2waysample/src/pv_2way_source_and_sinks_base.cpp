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
#include "pv_2way_source_and_sinks_base.h"
#include "pv_2way_sink.h"
#include "pv_2way_source.h"


OSCL_EXPORT_REF PV2WaySourceAndSinksBase::PV2WaySourceAndSinksBase(PV2Way324InitInfo& aSdkInitInfo):
        iAudioStartRecId(-1),
        iAudioStopRecId(-1),
        iVideoStartRecId(-1),
        iVideoStopRecId(-1),
        iSdkInitInfo(aSdkInitInfo),
        iAudioSink(NULL),
        iAudioSource(NULL),
        iVideoSink(NULL),
        iVideoSource(NULL),
        iTerminal(NULL)
{
    iAudioSink = OSCL_NEW(PV2WaySink,
                          (&iSdkInitInfo.iIncomingAudioFormats, this));
    iAudioSource = OSCL_NEW(PV2WaySource,
                            (&iSdkInitInfo.iOutgoingAudioFormats, this));
    iVideoSink = OSCL_NEW(PV2WaySink,
                          (&iSdkInitInfo.iIncomingVideoFormats, this));
    iVideoSource = OSCL_NEW(PV2WaySource,
                            (&iSdkInitInfo.iOutgoingVideoFormats, this));
}

OSCL_EXPORT_REF PV2WaySourceAndSinksBase::~PV2WaySourceAndSinksBase()
{
}

int PV2WaySourceAndSinksBase::GetEngineCmdState(PVCommandId aCmdId)
{
    if (iAudioSource->IsAddId(aCmdId))
        return EPVTestCmdAddAudioDataSource;
    else if (iAudioSource->IsRemoveId(aCmdId))
        return EPVTestCmdRemoveAudioDataSource;
    else if (iVideoSource->IsAddId(aCmdId))
        return EPVTestCmdAddVideoDataSource;
    else if (iVideoSource->IsRemoveId(aCmdId))
        return EPVTestCmdRemoveVideoDataSource;
    else if (iAudioSink->IsAddId(aCmdId))
        return EPVTestCmdAddAudioDataSink;
    else if (iAudioSink->IsRemoveId(aCmdId))
        return EPVTestCmdRemoveAudioDataSink;
    else if (iVideoSink->IsAddId(aCmdId))
        return EPVTestCmdAddVideoDataSink;
    else if (iVideoSink->IsRemoveId(aCmdId))
        return EPVTestCmdRemoveVideoDataSink;
    else
        return EPVTestCmdIdle;
}
OSCL_EXPORT_REF void PV2WaySourceAndSinksBase::Init()
{
    iAudioSink->Init();
    iAudioSource->Init();
    iVideoSink->Init();
    iVideoSource->Init();
}


OSCL_EXPORT_REF void PV2WaySourceAndSinksBase::Cleanup()
{
    if (iAudioSink)
    {
        OSCL_DELETE(iAudioSink);
        iAudioSink = NULL;
    }
    if (iVideoSink)
    {
        OSCL_DELETE(iVideoSink);
        iVideoSink = NULL;
    }
    if (iAudioSource)
    {
        OSCL_DELETE(iAudioSource);
        iAudioSource = NULL;
    }
    if (iVideoSource)
    {
        OSCL_DELETE(iVideoSource);
        iVideoSource = NULL;
    }
}

OSCL_EXPORT_REF void PV2WaySourceAndSinksBase::RemoveSourceAndSinks()
{
    iAudioSink->Remove();
    iAudioSource->Remove();
    iVideoSink->Remove();
    iVideoSource->Remove();
}

OSCL_EXPORT_REF void PV2WaySourceAndSinksBase::ResetCompleted()
{
    iAudioSink->ResetCompleted();
    iAudioSource->ResetCompleted();
    iVideoSink->ResetCompleted();
    iVideoSource->ResetCompleted();
}

OSCL_EXPORT_REF int PV2WaySourceAndSinksBase::ResetPreferredCodec(TPVDirection aDir,
        PV2WayMediaType aMediaType)
{
    PV2WayMIO* mio = 0;
    mio = GetMIO(aDir, aMediaType);
    if (mio)
    {
        mio->ClearCodecs();
        return 0;
    }
    OutputInfo(PVLOGMSG_ERR, "PV2WaySourceAndSinksBase::ResetPreferredCodec: Error!  No MIO of given dir, type");
    return -1;
}

PV2WayMIO* PV2WaySourceAndSinksBase::GetMIO(TPVDirection aDir,
        PV2WayMediaType aMediaType)
{
    PV2WayMIO *mio = 0;
    switch (aDir)
    {
        case OUTGOING:
            switch (aMediaType)
            {
                case PV_AUDIO:
                    mio = iAudioSource;
                    break;
                case PV_VIDEO:
                    mio = iVideoSource;
                    break;
                case PV_MEDIA_NONE:
                case PV_CONTROL:
                case PV_DATA:
                case PV_USER_INPUT:
                case PV_MULTIPLEXED:
                case PV_MEDIA_ALL:
                default:
                    break;
            }
            break;

        case INCOMING:
            switch (aMediaType)
            {
                case PV_AUDIO:
                    mio = iAudioSink;
                    break;
                case PV_VIDEO:
                    mio = iVideoSink;
                    break;
                case PV_MEDIA_NONE:
                case PV_CONTROL:
                case PV_DATA:
                case PV_USER_INPUT:
                case PV_MULTIPLEXED:
                case PV_MEDIA_ALL:
                default:
                    break;
            }
            break;
        case PV_DIRECTION_BOTH:
        case PV_DIRECTION_NONE:
        default:
            break;
    }
    return mio;

}


OSCL_EXPORT_REF void PV2WaySourceAndSinksBase::CommandCompleted(const PVCmdResponse& aResponse)
{
    PVCommandId cmdId = aResponse.GetCmdId();

    if (iAudioSource->IsAddId(cmdId))
    {
        iAudioSource->AddCompleted(aResponse);
    }
    else if (iVideoSource->IsAddId(cmdId))
    {
        iVideoSource->AddCompleted(aResponse);
    }
    else if (iAudioSource->IsRemoveId(cmdId))
    {
        iAudioSource->RemoveCompleted(aResponse);
    }
    else if (iVideoSource->IsRemoveId(cmdId))
    {
        iVideoSource->RemoveCompleted(aResponse);
    }
    else if (iAudioSink->IsAddId(cmdId))
    {
        iAudioSink->AddCompleted(aResponse);
    }
    else if (iVideoSink->IsAddId(cmdId))
    {
        iVideoSink->AddCompleted(aResponse);
    }
    else if (iAudioSink->IsRemoveId(cmdId))
    {
        iAudioSink->RemoveCompleted(aResponse);
    }
    else if (iVideoSink->IsRemoveId(cmdId))
    {
        iVideoSink->RemoveCompleted(aResponse);
    }
}

OSCL_EXPORT_REF void PV2WaySourceAndSinksBase::HandleClosingTrack(const PVAsyncInformationalEvent& aEvent)
{
    TPVDirection dir = (TPVDirection)((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0];
    TPVChannelId id = GetIdFromLocalBuffer((PVAsyncInformationalEvent &) aEvent);
    OutputInfo(PVLOGMSG_INFO, "PVT_INDICATION_CLOSING_TRACK dir=%d, id %u\n", dir, id);
    if (dir == INCOMING)
    {
        if (iAudioSink->IsChannelId(id))
        {
            iAudioSink->HandleClosingTrack();
        }
        else if (iVideoSink->IsChannelId(id))
        {
            iVideoSink->HandleClosingTrack();
        }
    }
    else
    {
        if (iAudioSource->IsChannelId(id))
        {
            iAudioSource->HandleClosingTrack();
        }
        else if (iVideoSource->IsChannelId(id))
        {
            iVideoSource->HandleClosingTrack();
        }
    }
}


OSCL_EXPORT_REF void PV2WaySourceAndSinksBase::HandleCloseTrack(const PVAsyncInformationalEvent& aEvent)
{

    TPVDirection dir = (TPVDirection)((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0];
    TPVChannelId id = GetIdFromLocalBuffer((PVAsyncInformationalEvent &) aEvent);
    OutputInfo(PVLOGMSG_STACK_TRACE, "PV2WaySourceAndSinksBase::HandleCloseTrack() - PVT_INDICATION_CLOSE_TRACK dir=%d, id=%u", dir, id);
    if (dir == INCOMING)
    {
        if (iVideoSink->IsChannelId(id))
        {
            OutputInfo(PVLOGMSG_INFO, "\nIncoming video track closed\n");
            iVideoSink->HandleCloseTrack();
        }
        else if (iAudioSink->IsChannelId(id))
        {
            OutputInfo(PVLOGMSG_INFO, "\nIncoming audio track closed\n");
            iAudioSink->HandleCloseTrack();
        }
    }
    else if (dir == OUTGOING)
    {
        if (iVideoSource->IsChannelId(id))
        {
            OutputInfo(PVLOGMSG_INFO, "\nOutgoing video track closed\n");
            iVideoSource->HandleCloseTrack();
        }
        else if (iAudioSource->IsChannelId(id))
        {
            OutputInfo(PVLOGMSG_INFO, "\nOutgoing audio track closed\n");
            iAudioSource->HandleCloseTrack();
        }
    }
}

OSCL_EXPORT_REF void PV2WaySourceAndSinksBase::PrintFormatTypes()
{
    OutputInfo(PVLOGMSG_INFO, "\nAudio Sink Format Types: ");
    iAudioSink->PrintFormatTypes();
    OutputInfo(PVLOGMSG_INFO, "\nAudio Source Format Types: ");
    iAudioSource->PrintFormatTypes();
    OutputInfo(PVLOGMSG_INFO, "\nVideo Sink Format Types: ");
    iVideoSink->PrintFormatTypes();
    OutputInfo(PVLOGMSG_INFO, "\nVideo Source Format Types: ");
    iVideoSource->PrintFormatTypes();
}

OSCL_EXPORT_REF bool PV2WaySourceAndSinksBase::FormatMatchesSelectedCodec(TPVDirection aDir,
        PV2WayMediaType aMediaType, PVMFFormatType& aFormat)
{
    PVMFFormatType& form = GetMIO(aDir, aMediaType)->GetSelectedFormat();
    OutputInfo(PVLOGMSG_INFO, " Expected: %s and Found: %s match? %d --------\n",
               aFormat.getMIMEStrPtr(), form.getMIMEStrPtr(), (form == aFormat));
    return (form == aFormat) ? true : false;
}

OSCL_EXPORT_REF bool PV2WaySourceAndSinksBase::AllMIOsRemoved()
{
    if (iAudioSource->IsRemoved() &&
            iAudioSink->IsRemoved() &&
            iVideoSource->IsRemoved() &&
            iVideoSink->IsRemoved())
        return true;
    return false;
}

