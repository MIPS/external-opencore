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
#ifndef PVMF_PARSERNODE_IMPL_H_INCLUDED
#include "pvmf_parsernode_impl.h"
#endif
#ifndef PVMF_AMR_PARSER_H_INCLUDED
#include "pvmf_amr_parser.h"
#endif
#ifndef PVMF_WAV_PARSER_H_INCLUDED
#include "pvmf_wav_parser.h"
#endif
#ifndef PVMI_DS_BASIC_INTERFACE_H_INCLUDED
#include "pvmi_ds_basic_interface.h"
#endif
#ifndef PVMF_RESIZABLE_SIMPLE_MEDIAMSG_H_INCLUDED
#include "pvmf_resizable_simple_mediamsg.h"
#endif
#ifndef __MEDIA_CLOCK_CONVERTER_H
#include "media_clock_converter.h"
#endif
#ifndef PVMF_DURATION_INFOMESSAGE_H_INCLUDED
#include "pvmf_duration_infomessage.h"
#endif
#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif
#ifndef PVMI_KVP_UTIL_H_INCLUDED
#include "pvmi_kvp_util.h"
#endif

#define NUM_SUPPORTED_KEYS 2

PVMFParserNodeImpl::PVMFParserNodeImpl(int32 aPriority)
        : PVMFNodeInterfaceImpl(aPriority, "PVMFParserNodeImpl")
{
    ipDataStream = NULL;
    ipParser = NULL;
    iClipFmtType = PVMF_MIME_FORMAT_UNKNOWN;
    iClipDuration = 0;
    iClipTimescale = 0;
    ipGau = NULL;
    ipLogger = PVLogger::GetLoggerObject("PVMFParserNodeImpl");
    ipDatapathLog = PVLogger::GetLoggerObject("datapath.sourcenode.commonparsernode");
}

PVMFParserNodeImpl::~PVMFParserNodeImpl()
{
    // Clear the track port info
    if (!iTrkPortInfoVec.empty())
        ClearTrackPortInfo();

    // Delete the GAU structure
    if (ipGau)
    {
        PVMF_BASE_NODE_DELETE(ipGau);
        ipGau = NULL;
    }

    // Delete the parser
    if (ipParser)
    {
        PVMF_BASE_NODE_DELETE(ipParser);
        ipParser = NULL;
    }
}

void PVMFParserNodeImpl::addRef()
{
    iExtensionRefCount++;
}

void PVMFParserNodeImpl::removeRef()
{
    iExtensionRefCount--;
}

bool PVMFParserNodeImpl::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::queryInterface: In"));

    if (uuid == PVMF_DATASTREAM_INIT_INTERFACE_UUID)
    {
        PVMFDataStreamInitExtension* myInf = OSCL_STATIC_CAST(PVMFDataStreamInitExtension*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInf);
    }
    else if (uuid == PVMF_DATA_SOURCE_INIT_INTERFACE_UUID)
    {
        PVMFDataSourceInitializationExtensionInterface* myInf = OSCL_STATIC_CAST(PVMFDataSourceInitializationExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInf);
    }
    else if (uuid == PVMF_TRACK_SELECTION_INTERFACE_UUID)
    {
        PVMFTrackSelectionExtensionInterface* myInf = OSCL_STATIC_CAST(PVMFTrackSelectionExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInf);
    }
    else if (uuid == PvmfDataSourcePlaybackControlUuid)
    {
        PvmfDataSourcePlaybackControlInterface* myInf = OSCL_STATIC_CAST(PvmfDataSourcePlaybackControlInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInf);
    }
    else if (uuid == KPVMFMetadataExtensionUuid)
    {
        PVMFMetadataExtensionInterface* myInf = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInf);
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::queryInterface: Not Supported UUID Out"));
        return false;
    }

    addRef();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::queryInterface: Out"));
    return true;
}

PVMFStatus PVMFParserNodeImpl::InitializeDataStreamData(PvmiDataStreamInterface* aDataStream, PVMFFormatType& aSourceFormat)
{
    ipDataStream = aDataStream;
    iClipFmtType = aSourceFormat;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFParserNodeImpl::InitializeDataStreamData: DataStream %p, Source Format %s", ipDataStream, iClipFmtType.getMIMEStrPtr()));
    return PVMFSuccess;
}

PVMFStatus PVMFParserNodeImpl::GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::GetMediaPresentationInfo: In"));
    PVMFStatus status = PVMFSuccess;

    // Audio Only contents have 1 track
    uint32 numTracks = ipParser->GetNumTracks();

    OSCL_FastString mimeType = _STRLIT_CHAR(PVMF_MIME_FORMAT_UNKNOWN);
    ipParser->RetrieveFileInfo(iFileInfo);

    if (iFileInfo.AudioPresent)
    {
        // Check the Track type and set mime type and BufferLen to retrieve data
        if (PVMF_AMR_NB == iFileInfo.AudioTrkInfo.AudioFmt)
        {
            mimeType = _STRLIT_CHAR(PVMF_MIME_AMR_IETF);
            iTrackPortInfo.iBufferlen = PVMF_NUM_FRAMES_AMR * PVMF_MAX_FRAME_SIZE_AMR_NB;
        }
        else if (PVMF_AMR_WB == iFileInfo.AudioTrkInfo.AudioFmt)
        {
            mimeType = _STRLIT_CHAR(PVMF_MIME_AMRWB_IETF);
            iTrackPortInfo.iBufferlen = PVMF_NUM_FRAMES_AMR * PVMF_MAX_FRAME_SIZE_AMR_WB;
        }
        else if (PVMF_WAV_PCM == iFileInfo.AudioTrkInfo.AudioFmt)
        {
            if (8 == (iFileInfo.AudioTrkInfo.BitRate / iFileInfo.AudioTrkInfo.SamplingRate))
                mimeType = _STRLIT_CHAR(PVMF_MIME_PCM8);
            else if (iFileInfo.AudioTrkInfo.LittleEndian)
                mimeType = _STRLIT_CHAR(PVMF_MIME_PCM16);
            else
                mimeType = _STRLIT_CHAR(PVMF_MIME_PCM16_BE);
        }
        else if (PVMF_WAV_ALAW == iFileInfo.AudioTrkInfo.AudioFmt)
            mimeType = _STRLIT_CHAR(PVMF_MIME_ALAW);
        else if (PVMF_WAV_MULAW == iFileInfo.AudioTrkInfo.AudioFmt)
            mimeType = _STRLIT_CHAR(PVMF_MIME_ULAW);
        else
            status = PVMFErrNotSupported;

        if (0 == iTrackPortInfo.iBufferlen)
        {
            // Just set it for WAV parser
            uint32 bytesPerSample = ((iFileInfo.AudioTrkInfo.BitRate / iFileInfo.AudioTrkInfo.SamplingRate) + 7) / 8;
            iTrackPortInfo.iBufferlen = PVMF_NUM_FRAMES_WAV * iFileInfo.AudioTrkInfo.NoOfChannels * bytesPerSample;
        }
    }

    aInfo.setDurationValue(iClipDuration);
    aInfo.setDurationTimeScale(iClipTimescale);

    PVMFTrackInfo trackInfo;
    trackInfo.setTrackMimeType(mimeType);
    trackInfo.setTrackID(numTracks);
    trackInfo.setPortTag(PVMF_PARSER_NODE_PORT_TYPE_OUTPUT);
    trackInfo.setTrackDurationValue(iFileInfo.AudioTrkInfo.TrackDuration);
    trackInfo.setTrackDurationTimeScale(iFileInfo.AudioTrkInfo.TrackTimescale);
    trackInfo.setTrackBitRate(iFileInfo.AudioTrkInfo.BitRate);
    aInfo.addTrackInfo(trackInfo);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::GetMediaPresentationInfo: Out"));
    return status;
}

PVMFStatus PVMFParserNodeImpl::SelectTracks(PVMFMediaPresentationInfo& aInfo)
{
    OSCL_UNUSED_ARG(aInfo);
    return PVMFSuccess;
}

PVMFCommandId PVMFParserNodeImpl::SetDataSourcePosition(PVMFSessionId aSessionId,
        PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aActualNPT,
        PVMFTimestamp& aActualMediaDataTS,
        bool aSeekToSyncPoint,
        uint32 aStreamID,
        OsclAny * aContext)
{
    PVMFNodeCommand cmd;
    cmd.Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION, aTargetNPT, &aActualNPT, &aActualMediaDataTS, aSeekToSyncPoint, aStreamID, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFParserNodeImpl::QueryDataSourcePosition(PVMFSessionId aSessionId,
        PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aActualNPT,
        bool aSeekToSyncPoint,
        OsclAny * aContext)
{
    PVMFNodeCommand cmd;
    cmd.Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, &aActualNPT, aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFParserNodeImpl::QueryDataSourcePosition(PVMFSessionId aSessionId,
        PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aSyncBeforeTargetNPT,
        PVMFTimestamp& aSyncAfterTargetNPT,
        OsclAny* aContext,
        bool aSeekToSyncPoint)
{
    PVMFNodeCommand cmd;
    cmd.Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, aSyncBeforeTargetNPT, aSyncAfterTargetNPT, aContext, aSeekToSyncPoint);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFParserNodeImpl::SetDataSourceRate(PVMFSessionId aSessionId,
        int32 aRate,
        PVMFTimebase* aTimebase,
        OsclAny * aContext)
{
    PVMFNodeCommand cmd;
    cmd.Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_RATE, aRate, aTimebase, aContext);
    return QueueCommandL(cmd);
}

PVMFStatus PVMFParserNodeImpl::SetMetadataClipIndex(uint32 aClipIndex)
{
    // Since Playlist is not supported in this node as of now
    OSCL_UNUSED_ARG(aClipIndex);
    return PVMFSuccess;
}

uint32 PVMFParserNodeImpl::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    return NUM_SUPPORTED_KEYS;
}

PVMFCommandId PVMFParserNodeImpl::GetNodeMetadataValues(PVMFSessionId aSessionId,
        PVMFMetadataList& aKeyList,
        Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
        uint32 aStartingValueIndex,
        int32 aMaxValueEntries,
        const OsclAny* aContextData)
{
    PVMFNodeCommand cmd;
    cmd.Construct(aSessionId, PVMF_GENERIC_NODE_GETNODEMETADATAVALUES, aKeyList, aValueList, aStartingValueIndex, aMaxValueEntries, aContextData);
    return QueueCommandL(cmd);
}

PVMFStatus PVMFParserNodeImpl::ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
        uint32 aStartingValueIndex,
        uint32 aEndValueIndex)
{
    while (!aValueList.empty())
        aValueList.erase(&aValueList[0]);
    return PVMFSuccess;
}

PVMFStatus PVMFParserNodeImpl::HandleExtensionAPICommands()
{
    PVMFStatus status = PVMFFailure;
    switch (iCurrentCommand.iCmd)
    {
        case PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION:
            status = DoSetDataSourcePosition();
            break;
        case PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION:
        case PVMF_GENERIC_NODE_SET_DATASOURCE_RATE:
            status = PVMFErrNotSupported;
            break;
        case PVMF_GENERIC_NODE_GETNODEMETADATAVALUES:
            status = DoGetNodeMetadataValues();
            break;
        default:
            break;
    }
    return status;
}

PVMFStatus PVMFParserNodeImpl::CancelCurrentCommand()
{
    return PVMFSuccess;
}

PVMFStatus PVMFParserNodeImpl::DoQueryInterface()
{
    PVUuid* uuid;
    PVInterface** inf;
    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, inf);
    if (queryInterface(*uuid, *inf))
        return PVMFSuccess;
    return PVMFErrNotSupported;
}

PVMFStatus PVMFParserNodeImpl::DoInit()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoInit: In"));
    // Create the parser based on ClipFmtType
    if (iClipFmtType == PVMF_MIME_AMRFF)
        ipParser = PVMF_BASE_NODE_NEW(PVMFAmrParser, ());
    else if (iClipFmtType == PVMF_MIME_WAVFF)
        ipParser = PVMF_BASE_NODE_NEW(PVMFWavParser, ());
    else
        ipParser = NULL;

    ipGau = PVMF_BASE_NODE_NEW(GAU, ());

    if (NULL == ipParser || NULL == ipGau)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoInit: Error No Memory"));
        return PVMFErrNoMemory;
    }

    if (!ipParser->Init(ipDataStream, iClipDuration, iClipTimescale))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoInit: Init on Parser Failed"));
        return PVMFFailure;
    }

    uint64 duration = iClipDuration;
    if (1000 != iClipTimescale)
    {
        // Use the media clock converter class to calculate duration with the given timescale
        MediaClockConverter mcc;
        mcc.update_clock((uint64)iClipDuration);
        duration = mcc.get_converted_ts64(iClipTimescale);
    }

    PVMFDurationInfoMessage* durInfoMsg = OSCL_NEW(PVMFDurationInfoMessage, (duration));
    uint8 local[4] = {0, 0, 0, 0};
    PVMFAsyncEvent async(PVMFInfoEvent, PVMFInfoDurationAvailable, NULL, ((PVInterface*)durInfoMsg), NULL, local, 4);
    ReportInfoEvent(async);
    if (durInfoMsg)
    {
        durInfoMsg->removeRef();
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoInit: Out"));
    return PVMFSuccess;
}

PVMFStatus PVMFParserNodeImpl::DoRequestPort(PVMFPortInterface*& aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoRequestPort: In"));
    int32 portTag;
    OSCL_String* mimeType;
    iCurrentCommand.PVMFNodeCommandBase::Parse(portTag, mimeType);

    if ((PVMF_PARSER_NODE_PORT_TYPE_OUTPUT != portTag) || (NULL == mimeType))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoRequestPort: Invalid PortTag or MimeType"));
        return PVMFErrArgument;
    }

    PVMFFormatType formatType = mimeType->get_cstr();
    if (formatType == PVMF_MIME_FORMAT_UNKNOWN)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoRequestPort: Unknown Format Type"));
        return PVMFErrArgument;
    }

    OSCL_StackString<40> portName;
    if (formatType.isAudio())
    {
        portName = "CommonParserPort(Audio)";
    }
    else if (formatType.isVideo())
    {
        portName = "CommonParserPort(Video)";
    }

    bool memErr = false;
    PVMFPortInterface* parserPort = NULL;
    MediaClockConverter* clockConv = NULL;
    OsclMemPoolResizableAllocator* resizeableMemPoolAllocator = NULL;
    PVMFResizableSimpleMediaMsgAlloc* resizeableSimpleMediaMsg = NULL;

    parserPort = PVMF_BASE_NODE_NEW(PVMFParserNodePort, (portTag, this, portName.get_cstr()));
    clockConv = PVMF_BASE_NODE_NEW(MediaClockConverter, (iClipTimescale));
    resizeableMemPoolAllocator = PVMF_BASE_NODE_NEW(OsclMemPoolResizableAllocator, (20 * 1024, 20));
    resizeableSimpleMediaMsg = PVMF_BASE_NODE_NEW(PVMFResizableSimpleMediaMsgAlloc, (resizeableMemPoolAllocator));

    if (!parserPort || !clockConv || !resizeableMemPoolAllocator || !resizeableSimpleMediaMsg)
        memErr = true;

    if (memErr)
    {
        if (parserPort)
            PVMF_BASE_NODE_DELETE(parserPort);

        if (clockConv)
            PVMF_BASE_NODE_DELETE(clockConv);

        if (resizeableMemPoolAllocator)
        {
            resizeableMemPoolAllocator->removeRef();
            resizeableMemPoolAllocator = NULL;
        }

        if (resizeableSimpleMediaMsg)
            PVMF_BASE_NODE_DELETE(resizeableSimpleMediaMsg);

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoRequestPort: Error No Memory"));
        return PVMFErrNoMemory;
    }

    resizeableMemPoolAllocator->enablenullpointerreturn();

    iTrackPortInfo.iTrackId = portTag;
    iTrackPortInfo.ipMimeString = (char*)mimeType->get_cstr();
    iTrackPortInfo.ipNode = this;
    iTrackPortInfo.ipPort = parserPort;
    iTrackPortInfo.ipClockConv = clockConv;
    iTrackPortInfo.ipResizeableMemPoolAllocator = resizeableMemPoolAllocator;
    iTrackPortInfo.ipResizeableSimpleMediaMsg = resizeableSimpleMediaMsg;
    iTrkPortInfoVec.push_back(iTrackPortInfo);

    aPort = parserPort;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoRequestPort: Out"));
    return PVMFSuccess;
}

PVMFStatus PVMFParserNodeImpl::DoStop()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoStop: In"));

    if (!iTrkPortInfoVec.empty())
        (&iTrkPortInfoVec[0])->reset();

    // Reset the tracks
    int64 seekTo = 0;
    ipParser->Seek(0, seekTo);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoStop: Out"));
    return PVMFSuccess;
}

PVMFStatus PVMFParserNodeImpl::DoReleasePort()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoReleasePort: In"));

    ClearTrackPortInfo();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoReleasePort: Out"));
    return PVMFSuccess;
}

PVMFStatus PVMFParserNodeImpl::DoReset()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoReset: In"));

    ClearTrackPortInfo();

    // Delete the GAU structure
    if (ipGau)
    {
        PVMF_BASE_NODE_DELETE(ipGau);
        ipGau = NULL;
    }

    // Delete the parser
    if (ipParser)
    {
        PVMF_BASE_NODE_DELETE(ipParser);
        ipParser = NULL;
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoReset: Out"));
    return PVMFSuccess;
}

void PVMFParserNodeImpl::Run()
{
    if (!iInputCommands.empty())
        ProcessCommand();

    if (EPVMFNodeStarted == iInterfaceState)
    {
        PVMFParserNodeTrackPortInfo* trackPortInfo = &iTrkPortInfoVec[0];
        if (!trackPortInfo->iOutgoingQueueBusy)
            ProcessPortActivity(trackPortInfo);

        if (trackPortInfo->iProcessOutgoingMsg)
            if (PVMFSuccess != (trackPortInfo->ipPort->Send()))
                trackPortInfo->iProcessOutgoingMsg = false;

        if (!trackPortInfo->iOutgoingQueueBusy || trackPortInfo->iProcessOutgoingMsg)
            RunIfNotReady();
    }
}

void PVMFParserNodeImpl::HandlePortActivity(const PVMFPortActivity& aActivity)
{
    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_CREATED:
            ReportInfoEvent(PVMFInfoPortCreated, (OsclAny*)aActivity.iPort);
            break;
        case PVMF_PORT_ACTIVITY_DELETED:
            ReportInfoEvent(PVMFInfoPortDeleted, (OsclAny*)aActivity.iPort);
            break;
        case PVMF_PORT_ACTIVITY_CONNECT:
        case PVMF_PORT_ACTIVITY_DISCONNECT:
            break;
        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
            // There cannot be any Incoming msg for source node, log the Error
            break;
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
        {
            // There is a msg in the outgoing queue send it to the connected port
            PVMFParserNodeTrackPortInfo* trackPortInfo = &iTrkPortInfoVec[0];
            if (!trackPortInfo->iProcessOutgoingMsg)
                if (PVMFErrBusy == (trackPortInfo->ipPort->Send()))
                    trackPortInfo->iProcessOutgoingMsg = false;
        }
        break;
        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_BUSY:
            (&iTrkPortInfoVec[0])->iProcessOutgoingMsg = false;
            break;
        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY:
        {
            PVMFParserNodeTrackPortInfo* trackPortInfo = &iTrkPortInfoVec[0];
            trackPortInfo->iProcessOutgoingMsg = true;
            if (PVMFSuccess != (trackPortInfo->ipPort->Send()))
                ReportInfoEvent(PVMFErrPortProcessing);
            RunIfNotReady();
        }
        break;
        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_BUSY:
            (&iTrkPortInfoVec[0])->iOutgoingQueueBusy = true;
            break;
        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_READY:
        {
            PVMFParserNodeTrackPortInfo* trackPortInfo = &iTrkPortInfoVec[0];
            trackPortInfo->iOutgoingQueueBusy = false;
            ProcessPortActivity(trackPortInfo);
            RunIfNotReady();
        }
        break;
        default:
            break;
    }
}

void PVMFParserNodeImpl::ProcessPortActivity(PVMFParserNodeTrackPortInfo* aTrackPortInfo)
{
    if (aTrackPortInfo->iEosSent)
    {
        // EOS has already been sent, just return and reset the booleans controlling data flow on port queues
        aTrackPortInfo->iOutgoingQueueBusy = true;
        return;
    }

    if (aTrackPortInfo->iSendBos)
    {
        // Send the BOS first
        if (SendBeginOfMediaStreamCommand(aTrackPortInfo->ipPort, iStreamID, aTrackPortInfo->iTimestamp))
            aTrackPortInfo->iSendBos = false;
        else
        {
            // Queue is not ready to accept any more data, set the boolean iOutgoingQueueBusy as true
            // and wait for Queues to signal when they are ready
            aTrackPortInfo->iOutgoingQueueBusy = true;
            return;
        }
    }

    if (!aTrackPortInfo->iSendEos)
    {
        // Can Retrieve and Queue the media sample
        PVMFStatus status = QueueMediaSample(aTrackPortInfo);
        if (PVMFSuccess != status && PVMFErrNoMemory != status)
            // Report Processing Error Event
            ReportInfoEvent(PVMFErrPortProcessing);
    }

    if (aTrackPortInfo->iSendEos)
    {
        if (SendEndOfTrackCommand(aTrackPortInfo->ipPort, iStreamID, aTrackPortInfo->iTimestamp, aTrackPortInfo->iSeqNum, 0, 10))
        {
            // EOS sent successfully, reset booleans controlling data flow on port queues
            aTrackPortInfo->iEosSent = true;
            aTrackPortInfo->iOutgoingQueueBusy = true;
        }
    }
}

PVMFStatus PVMFParserNodeImpl::QueueMediaSample(PVMFParserNodeTrackPortInfo* aTrackPortInfo)
{
    // Create a Media data pool
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImpl;
    mediaDataImpl = aTrackPortInfo->ipResizeableSimpleMediaMsg->allocate(aTrackPortInfo->iBufferlen);
    if (NULL == mediaDataImpl.GetRep())
    {
        OsclMemPoolResizableAllocatorObserver* resizableMemPoolAllocObs =
            OSCL_STATIC_CAST(OsclMemPoolResizableAllocatorObserver*, aTrackPortInfo);
        aTrackPortInfo->ipResizeableMemPoolAllocator->notifyfreeblockavailable(*resizableMemPoolAllocObs, aTrackPortInfo->iBufferlen);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipDatapathLog, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::QueueMediaSample: No Memory available"));
        return PVMFErrNoMemory;
    }

    // Now create the pvmf media from pool
    PVMFSharedMediaDataPtr mediaDataPtr;
    mediaDataPtr = PVMFMediaData::createMediaData(mediaDataImpl, aTrackPortInfo->ipResizeableMemPoolAllocator);
    if (NULL == mediaDataPtr.GetRep())
    {
        OsclMemPoolResizableAllocatorObserver* resizableMemPoolAllocObs =
            OSCL_STATIC_CAST(OsclMemPoolResizableAllocatorObserver*, aTrackPortInfo);
        aTrackPortInfo->ipResizeableMemPoolAllocator->notifyfreeblockavailable(*resizableMemPoolAllocObs, aTrackPortInfo->iBufferlen);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipDatapathLog, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::QueueMediaSample: No Memory available"));
        return PVMFErrNoMemory;
    }

    // Retrieve Memory Fragment to write to
    OsclRefCounterMemFrag refCntrMemFrag;
    mediaDataPtr->getMediaFragment(0, refCntrMemFrag);

    if (iClipFmtType == PVMF_MIME_AMRFF)
    {
        ipGau->MediaBuffer = NULL;
        ipGau->NumberOfFrames = PVMF_NUM_FRAMES_AMR;
        ipGau->BufferLen = aTrackPortInfo->iBufferlen;
    }
    else if (iClipFmtType == PVMF_MIME_WAVFF)
    {
        ipGau->MediaBuffer = (uint8*)refCntrMemFrag.getMemFragPtr();
        ipGau->BufferLen = aTrackPortInfo->iBufferlen;
        ipGau->NumberOfFrames = PVMF_NUM_FRAMES_WAV;
    }

    PVMFStatus status = PVMFSuccess;
    PVMFParserReturnCode code;
    code = ipParser->GetNextAccessUnits(ipGau);

    if (code == PVMFParserEverythingFine || code == PVMFParserInSufficientData)
    {
        if (iClipFmtType == PVMF_MIME_AMRFF)
            oscl_memcpy(refCntrMemFrag.getMemFragPtr(), ipGau->MediaBuffer, ipGau->BufferLen);

        // Set the actual size
        mediaDataPtr->setMediaFragFilledLen(0, ipGau->BufferLen);

        // Resize Memory Fragment
        aTrackPortInfo->ipResizeableSimpleMediaMsg->ResizeMemoryFragment(mediaDataImpl);

        // Special handling for first frame after seek
        if (aTrackPortInfo->iFirstFrameAfterSeek)
        {
            aTrackPortInfo->iSampleNPT = ipGau->info[0].ts;
            aTrackPortInfo->iFirstFrameAfterSeek = false;
        }

        // Sample Duration of first sample after seek will be zero. The duration can only be known once node
        // has retrieved atleast 2 samples.
        aTrackPortInfo->iSampleDur = ipGau->info[0].ts - aTrackPortInfo->iSampleNPT;
        aTrackPortInfo->iTimestamp += aTrackPortInfo->iSampleDur;
        aTrackPortInfo->iSampleNPT = ipGau->info[0].ts;

        // Set the parameters
        mediaDataPtr->setStreamID(iStreamID);
        mediaDataPtr->setTimestamp(aTrackPortInfo->iTimestamp);
        mediaDataPtr->setSeqNum(aTrackPortInfo->iSeqNum);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipDatapathLog, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFParserNodeImpl::QueueMediaSample: StreamId %d, Timestamp %d, SeqNum %d, Length %d"
                         , iStreamID, aTrackPortInfo->iTimestamp, aTrackPortInfo->iSeqNum, ipGau->BufferLen));

        aTrackPortInfo->iSeqNum++;

        // Set M bit to 1 always
        uint32 markerInfo = 0;
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;
        mediaDataImpl->setMarkerInfo(markerInfo);

        // Now queue the media sample to the Outgoing queue
        PVMFSharedMediaMsgPtr mediaMsgPtr;
        convertToPVMFMediaMsg(mediaMsgPtr, mediaDataPtr);

        status = aTrackPortInfo->ipPort->QueueOutgoingMsg(mediaMsgPtr);

        if (code == PVMFParserInSufficientData)
        {
            // For local content treat it as EOS
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipDatapathLog, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFParserNodeImpl::QueueMediaSample: Parser returned Insufficient Data for local content, Send EOS StreamId %d"
                             , iStreamID));
            aTrackPortInfo->iSendEos = true;
        }
    }
    else
    {
        // Send the EOS msg now
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipDatapathLog, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFParserNodeImpl::QueueMediaSample: Send EOS StreamId %d", iStreamID));
        aTrackPortInfo->iSendEos = true;
    }

    return status;
}

PVMFStatus PVMFParserNodeImpl::DoSetDataSourcePosition()
{
    // Parse the command
    PVMFTimestamp targetNPT = 0;
    PVMFTimestamp* actualNPT, *actualMediaDataTS = NULL;
    bool seekToSyncPoint = false;
    uint32 streamId = 0;
    iCurrentCommand.Parse(targetNPT, actualNPT, actualMediaDataTS, seekToSyncPoint, streamId);
    // Verify the streamID
    if ((streamId == iStreamID) && (EPVMFNodeStarted == iInterfaceState))
        return PVMFErrArgument;

    iStreamID = streamId;

    bool sendBosEos = false;
    PVMFTimestamp minTs = 0x7FFFFFFF;
    // Check the Target Position if greater than Clip Duration
    if (targetNPT >= iClipDuration)
    {
        // Reset the Parser to the beginning
        targetNPT = 0;
        sendBosEos = true;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipDatapathLog, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFParserNodeImpl::DoSetDataSourcePosition: Target Greater than Duration, Do BOS-EOS, Reset tracks to zero"));
    }

    for (uint32 ii = 0; ii < iTrkPortInfoVec.size(); ii++)
    {
        // New stream reset booleans for BOS-EOS and start the data flow
        iTrkPortInfoVec[ii].iSendBos = true;
        iTrkPortInfoVec[ii].iSendEos = false;
        iTrkPortInfoVec[ii].iEosSent = false;
        iTrkPortInfoVec[ii].iOutgoingQueueBusy = false;
        iTrkPortInfoVec[ii].iFirstFrameAfterSeek = true;
        if (sendBosEos)
            // Since the target position was beyond clip duration source just needs
            // to send EOS after BOS, so set the boolean for EOS
            iTrkPortInfoVec[ii].iSendEos = true;

        // Get the Timestamp for actualMediaDataTs
        if (iTrkPortInfoVec[ii].iTimestamp < minTs)
            minTs = iTrkPortInfoVec[ii].iTimestamp;
    }

    *actualMediaDataTS = minTs;
    *actualNPT = targetNPT;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipDatapathLog, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFParserNodeImpl::DoSetDataSourcePosition: TargetPos %d, ActualMediaDataTS %d"
                     , targetNPT, *actualMediaDataTS));

    // Seek parser to targetNPT
    PVMFParserReturnCode retVal;
    retVal = ipParser->Seek((int64)targetNPT, (int64&) * actualNPT);
    // Set the timestamp for playback
    for (uint32 j = 0; j < iTrkPortInfoVec.size(); j++)
        iTrkPortInfoVec[j].iTimestamp = *actualMediaDataTS + (targetNPT - *actualNPT);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipDatapathLog, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFParserNodeImpl::DoSetDataSourcePosition: New StreamId %d, ActualPos %d"
                     , iStreamID, *actualNPT));

    return (PVMFParserEverythingFine == retVal ? PVMFSuccess : PVMFFailure);
}

void PVMFParserNodeImpl::ClearTrackPortInfo()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::ClearTrackPortInfo: In"));

    while (!iTrkPortInfoVec.empty())
    {
        PVMFParserNodeTrackPortInfo* trackPortInfo = &iTrkPortInfoVec[0];
        if (trackPortInfo)
        {
            trackPortInfo->reset();

            trackPortInfo->ipMimeString = NULL;

            if (trackPortInfo->ipPort)
                PVMF_BASE_NODE_DELETE(trackPortInfo->ipPort);

            if (trackPortInfo->ipClockConv)
                PVMF_BASE_NODE_DELETE(trackPortInfo->ipClockConv);

            if (trackPortInfo->ipResizeableMemPoolAllocator)
            {
                trackPortInfo->ipResizeableMemPoolAllocator->CancelFreeChunkAvailableCallback();
                trackPortInfo->ipResizeableMemPoolAllocator->removeRef();
                trackPortInfo->ipResizeableMemPoolAllocator = NULL;
            }

            if (trackPortInfo->ipResizeableSimpleMediaMsg)
                PVMF_BASE_NODE_DELETE(trackPortInfo->ipResizeableSimpleMediaMsg);

            // Erase the element from the vector
            iTrkPortInfoVec.erase(trackPortInfo);
        }
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::ClearTrackPortInfo: In"));
}

PVMFStatus PVMFParserNodeImpl::DoGetNodeMetadataValues()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoGetNodeMetadataValues: In"));

    // Parse the command
    PVMFMetadataList* keyList = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valueList = NULL;
    uint32 startingIndex = 0;
    int32 maxEntries = 0;
    iCurrentCommand.Parse(keyList, valueList, startingIndex, maxEntries);

    if (NULL == keyList || NULL == valueList)
        return PVMFFailure;

    if (iTrkPortInfoVec.size() > 0)
    {
        // @TODO Once the GetMetaDataValue API is available at the Fileformat interface level,
        // the meta data retireval will be moved from the node to the different fileformats.
        // For now just push the mime string for the app to recognise the type of content
        // and random-access-denied to allow seek functionality
        PvmiKvp kvp;
        kvp.key = NULL;
        PVMFCreateKVPUtils::CreateKVPForCharStringValue(kvp, PVMETADATA_TRACKINFO_AUDIO_FORMAT_KEY, iTrkPortInfoVec[0].ipMimeString);
        valueList->push_back(kvp);

        kvp.key = NULL;
        bool seekDenied = false;
        PVMFCreateKVPUtils::CreateKVPForBoolValue(kvp, PVMF_COMMON_PARSER_NODE_RANDOM_ACCESS_DENIED_KEY, seekDenied);
        valueList->push_back(kvp);
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFParserNodeImpl::DoGetNodeMetadataValues: Out"));
    return PVMFSuccess;
}


