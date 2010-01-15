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
#include "pv_2way_source_and_sinks_base.h"


#ifndef PV_2WAY_TRACK_INFO_IMPL_H_INCLUDED
#include "pv_2way_track_info_impl.h"
#endif

#include "pv_2way_mio.h"
#include "pv_mime_string_utils.h"


#define FORMAT_SIZE 100

TPVChannelId GetIdFromLocalBuffer(PVAsyncInformationalEvent &aEvent)
{
    TPVChannelId *channel = (TPVChannelId *) & aEvent.GetLocalBuffer()[4];
    return *channel;
}

TPVDirection GetDir(int dir)
{
    return (TPVDirection)dir;
}

PV2WayMediaType GetMediaType(int mediaType)
{
    return (PV2WayMediaType)mediaType;
}


OSCL_EXPORT_REF PV2WayMIO::PV2WayMIO(Oscl_Vector<PVMFFormatType, OsclMemAllocator>* aFormats,
                                     PV2WayMIOObserver* aObserver) :
        iAdded(false),
        iRemoving(false),
        iClosing(false),
        iFormats(aFormats),
        iMioNode(NULL),
        iMioNodeFactory(NULL),
        iAddId(-1),
        iRemoveId(-1),
        iPauseId(-1),
        iResumeId(-1),
        iChannelId(0),
        iNextChannelId(0),
        iPreviewHandle(0),
        iTerminal(NULL),
        iLogger(NULL),
        iObserver(aObserver),
        iSelectedCodec(NULL)
{
    iMySelectedFormat = PVMF_MIME_FORMAT_UNKNOWN;
};


OSCL_EXPORT_REF PV2WayMIO::~PV2WayMIO()
{
    Delete();
    ClearCodecs();
}

OSCL_EXPORT_REF void PV2WayMIO::Delete()
{
    iAdded = false;
    if (iMioNode)
    {
        iObserver->DeleteMIONode(iSelectedCodec, iMyDir, &iMioNode);
    }
    iMioNode = NULL;
}


OSCL_EXPORT_REF void PV2WayMIO::ResetCompleted()
{
}

OSCL_EXPORT_REF void PV2WayMIO::AddCompleted(const PVCmdResponse& aResponse)
{
    if (aResponse.GetCmdStatus() == PVMFSuccess)
    {
        iAdded = true;
    }
    else
    {
        OutputInfo(PVLOGMSG_ERR, "PV2WayMIO::AddCompleted:: Failed to add MIO");
        Closed();
    }

    iAddId = -1;
}

OSCL_EXPORT_REF PVCommandId PV2WayMIO::Add()
{

    if (iMioNode == 0)
        return -1;
    int32 error = 0;
    if (!iAdded)
    {
        /*!

          Step 11: Add MIO Node to terminal
          Add appropriate MIO Node to terminal (audio/video, incoming/outgoing)
        */
        if (iMyDir == INCOMING)
        {
            OSCL_TRY(error, iAddId = iTerminal->AddDataSink(iChannelId,
                                     *iMioNode));
            if (error)
            {
                OutputInfo(PVLOGMSG_ERR, "PV2WayMIO::Add():: Error Adding Data Sink!");
            }
            return iAddId;
        }
        else
        {
            OSCL_TRY(error, iAddId = iTerminal->AddDataSource(iChannelId,
                                     *iMioNode));
            if (error)
            {
                OutputInfo(PVLOGMSG_ERR, "PV2WayMIO::Add():: Error Adding Data Source!");
            }
            return iAddId;
        }
    }
    else if (!iRemoving)
    {
        OutputInfo(PVLOGMSG_ERR, "\nError: MIO already added!\n");
    }
    else if (iRemoving)
    {
        OutputInfo(PVLOGMSG_ERR, "\nCannot add because attempting to remove MIO!\n");
    }
    return -1;
}

OSCL_EXPORT_REF PVCommandId PV2WayMIO::Remove()
{
    int32 error = 0;
    iRemoving = true;
    /*!

      Step 12: Cleanup
      Step 12a: Remove source and sinks
    */
    if (iMioNode && iRemoveId == -1)
    {
        if (iMyDir == INCOMING)
        {
            OSCL_TRY(error, iRemoveId = iTerminal->RemoveDataSink(*iMioNode));
            if (error)
            {
                OutputInfo(PVLOGMSG_ERR, "\n Error in RemoveDataSink!\n");
            }
            return iRemoveId;
        }
        else
        {
            OSCL_TRY(error, iRemoveId = iTerminal->RemoveDataSource(*iMioNode));
            if (error)
            {
                OutputInfo(PVLOGMSG_ERR, "\n Error in RemoveDataSource!\n");
            }
            return iRemoveId;
        }
    }
    else if (!iAdded)
    {
        OutputInfo(PVLOGMSG_ERR, "\nError: MIO cannot be removed because has not been added!\n");
    }
    return -1;
}

OSCL_EXPORT_REF void PV2WayMIO::HandleClosingTrack()
{
    iClosing = true;
}


OSCL_EXPORT_REF void PV2WayMIO::HandleCloseTrack()
{
    Closed();
}

OSCL_EXPORT_REF void PV2WayMIO::Closed()
{
    iClosing = false;
    iChannelId = 0;
    if (iNextChannelId)
    {
        iChannelId = iNextChannelId;
        iNextChannelId = 0;
        Add();
    }
}

OSCL_EXPORT_REF void PV2WayMIO::RemoveCompleted(const PVCmdResponse& aResponse)
{
    Delete();
    iRemoveId = -1;
    iRemoving = false;
}

OSCL_EXPORT_REF PVCommandId PV2WayMIO::HandleEvent(const PVAsyncInformationalEvent& aEvent)
{
    PVCommandId retvalue = -1;
    TPVChannelId id = GetIdFromLocalBuffer((PVAsyncInformationalEvent &) aEvent);
    if (PVT_INDICATION_INCOMING_TRACK == aEvent.GetEventType())
    {
        if (((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0] == PV_VIDEO)
        {
            OutputInfo(PVLOGMSG_INFO, "PVT_INDICATION_INCOMING_TRACK video, id %d\n", id);
        }
        else if (((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0] == PV_AUDIO)
        {
            OutputInfo(PVLOGMSG_INFO, "PVT_INDICATION_INCOMING_TRACK audio, id %d\n", id);
        }
    }
    else if (PVT_INDICATION_OUTGOING_TRACK == aEvent.GetEventType())
    {
        if (((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0] == PV_VIDEO)
        {
            OutputInfo(PVLOGMSG_INFO, "PVT_INDICATION_OUTGOING_TRACK video, id %d\n", id);
        }
        else if (((PVAsyncInformationalEvent&)aEvent).GetLocalBuffer()[0] == PV_AUDIO)
        {
            OutputInfo(PVLOGMSG_INFO, "PVT_INDICATION_OUTGOING_TRACK audio, id %d\n", id);
        }
    }
    if (iChannelId == id)
    {
        OutputInfo(PVLOGMSG_WARNING, "\nDuplicate callback for  id %d\n", id);
        return -1;
    }

    if (iChannelId)
    {
        OutputInfo(PVLOGMSG_WARNING, "\nBusy ... MIO id=%d being closed\n", iChannelId);
        iNextChannelId = id;
        return -1;
    }
    if (!iClosing && !iAdded)
    {
        iChannelId = id;
        // format matching capabilities and create- create can tell us which kind to create
        // including which file we should use, etc.
        iSelectedCodec = FormatMatchesCapabilities(aEvent);
        if (iSelectedCodec)
        {
            iMioNode = iObserver->CreateMIONode(iSelectedCodec, iMyDir);
            iMySelectedFormat = iSelectedCodec->GetFormat();
            retvalue = Add();
        }
        else
        {
            OutputInfo(PVLOGMSG_WARNING, "\nDid not find a codec!!! \n");
        }
    }
    return retvalue;
}

void PV2WayMIO::ParseResponse(const PVAsyncInformationalEvent& aEvent,
                              PVMFFormatType& aMimeString,
                              int& aMedia_type)
{

    PV2WayTrackInfoInterface* aTrackInfoInterface;
    aTrackInfoInterface = (PV2WayTrackInfoInterface *)aEvent.GetEventExtensionInterface();
    if (aTrackInfoInterface)
        aTrackInfoInterface->GetFormatString(aMimeString);
    aMedia_type = aEvent.GetLocalBuffer()[0];
}

void ReleaseCodecSpecifier(OsclAny *apCodecSpecifier)
{
    CodecSpecifier* pCodecSpecifier = OSCL_STATIC_CAST(CodecSpecifier*, apCodecSpecifier);
    OSCL_DELETE(pCodecSpecifier);
}

OSCL_EXPORT_REF int PV2WayMIO::AddFormat(PvmiMIOFileInputSettings& aformat)
{
    CodecSpecifier* temp = OSCL_NEW(MIOFileCodecSpecifier, (aformat));
    OSCL_TRAPSTACK_PUSH(OsclTrapItem(ReleaseCodecSpecifier, temp));
    PVMFFormatType format = temp->GetFormat();
    if (FormatInList(format))
    {
        OSCL_TRAPSTACK_POPDEALLOC(); // remove and dealloc temp variable
        return PVMFErrAlreadyExists;
    }
    iFormatsMap[format] = temp;
    iFormats->push_back(format);
    OSCL_TRAPSTACK_POP(); // remove temp variable
    return 0;
}

OSCL_EXPORT_REF int PV2WayMIO::AddFormat(PVMFFileInputSettings& aformat)
{
    CodecSpecifier* temp = OSCL_NEW(FileCodecSpecifier, (aformat));
    OSCL_TRAPSTACK_PUSH(OsclTrapItem(ReleaseCodecSpecifier, temp));
    PVMFFormatType format = temp->GetFormat();
    if (FormatInList(format))
    {
        OSCL_TRAPSTACK_POPDEALLOC(); // temp;
        return PVMFErrAlreadyExists;
    }
    iFormatsMap[format] = temp;
    iFormats->push_back(format);
    OSCL_TRAPSTACK_POP(); // temp;
    return 0;
}

OSCL_EXPORT_REF int PV2WayMIO::AddFormat(PVMFFormatType aformat)
{
    CodecSpecifier* temp = OSCL_NEW(CharCodecSpecifier, (aformat));
    OSCL_TRAPSTACK_PUSH(OsclTrapItem(ReleaseCodecSpecifier, temp));
    PVMFFormatType format = temp->GetFormat();
    if (FormatInList(format))
    {
        OSCL_TRAPSTACK_POPDEALLOC(); // temp;
        return PVMFErrAlreadyExists;
    }
    iFormatsMap[format] = temp;
    iFormats->push_back(format);
    OSCL_TRAPSTACK_POP(); // temp;
    return 0;
}

OSCL_EXPORT_REF int PV2WayMIO::AddFormat(DummyMIOSettings& aformat)
{
    CodecSpecifier* temp = OSCL_NEW(DummyMIOCodecSpecifier, (aformat));
    OSCL_TRAPSTACK_PUSH(OsclTrapItem(ReleaseCodecSpecifier, temp));
    PVMFFormatType format = temp->GetFormat();
    if (FormatInList(format))
    {
        OSCL_TRAPSTACK_POPDEALLOC(); // temp;
        return PVMFErrAlreadyExists;
    }
    iFormatsMap[format] = temp;
    iFormats->push_back(format);
    OSCL_TRAPSTACK_POP(); // temp;
    return 0;
}

OSCL_EXPORT_REF void PV2WayMIO::PrintFormatTypes()
{
    Oscl_Map < PVMFFormatType, CodecSpecifier*,
    OsclMemAllocator, pvmf_format_type_key_compare_class >::iterator it =
        iFormatsMap.begin();
    if (it == iFormatsMap.end())
    {
        OutputInfo(PVLOGMSG_NOTICE, "No formats added.");
        return;
    }
    // loop through each, output values
    while (it != iFormatsMap.end())
    {
        CodecSpecifier* codec = (*it++).second;
        PVMFFormatType format = codec->GetFormat();
        OutputInfo(PVLOGMSG_INFO, "%s", format.getMIMEStrPtr());
        OutputInfo(PVLOGMSG_INFO, " ");
    }
}

CodecSpecifier* PV2WayMIO::FormatInList(PVMFFormatType& type)
{
    Oscl_Map < PVMFFormatType, CodecSpecifier*,
    OsclMemAllocator, pvmf_format_type_key_compare_class >::iterator it =
        iFormatsMap.begin();
    it = iFormatsMap.find(type);
    if (!(it == iFormatsMap.end()))
        return (*it).second;
    return NULL;
}

// this is FormatMatchesCapabilities for Audio
CodecSpecifier* PV2WayMIO::FormatMatchesCapabilities(const PVAsyncInformationalEvent& aEvent)
{
    PVMFFormatType aMimeString;
    int aMedia_Type = 0;
    ParseResponse(aEvent, aMimeString, aMedia_Type);
    // compare to what in iFormats instead.
    CodecSpecifier* formatInList = FormatInList(aMimeString);
    if (!formatInList)
    {
        OutputInfo(PVLOGMSG_INFO, "Format %s does not match application capability\n", aMimeString.getMIMEStrPtr());
    }
    else
    {
        OutputInfo(PVLOGMSG_INFO, "Format %s matches application capabilities\n", aMimeString.getMIMEStrPtr());
    }

    return formatInList;
}

OSCL_EXPORT_REF int PV2WayMIO::AddCodec(PVMFFormatType aFormat)
{
    int error = 0;
    OSCL_TRY(error, AddFormat(aFormat));
    return error;
}

OSCL_EXPORT_REF void PV2WayMIO::ClearCodecs()
{
    Oscl_Map < PVMFFormatType, CodecSpecifier*,
    OsclMemAllocator, pvmf_format_type_key_compare_class >::iterator it =
        iFormatsMap.begin();
    // loop through each, delete
    while (it != iFormatsMap.end())
    {
        CodecSpecifier* codec = (*it++).second;
        OSCL_DELETE(codec);
    }
    iFormatsMap.clear();
    iFormats->clear();
    iSelectedCodec = NULL;
    iMySelectedFormat = PVMF_MIME_FORMAT_UNKNOWN;
}

OSCL_EXPORT_REF void PV2WayMIO::Init()
{
    iNextChannelId = 0;
    iChannelId = 0;
}
