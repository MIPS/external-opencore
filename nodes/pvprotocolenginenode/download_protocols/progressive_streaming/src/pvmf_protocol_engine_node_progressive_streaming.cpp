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
#include "pvmf_protocol_engine_node.h"
#include "pvmf_protocol_engine_node_progressive_streaming.h"
#include "pvmf_protocol_engine_progressive_download.h"
#include "pvmf_protocolengine_node_tunables.h"


#define IS_OVERFLOW_FOR_100x(x) ( (x)>>((sizeof((x))<<2)-PVPROTOCOLENGINE_DOWNLOAD_BYTE_PERCENTAGE_DLSIZE_RIGHTSHIFT_FACTOR) ) // (x)>>(32-7)=(x)>>25


////////////////////////////////////////////////////////////////////////////////////
//////  ProgressiveStreamingContainer implementation
////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF ProgressiveStreamingContainer::ProgressiveStreamingContainer(PVMFProtocolEngineNode *aNode) :
        ProgressiveDownloadContainer(aNode), iEnableInfoUpdate(true), iStartAfterPause(false)
{
    ;
}

OSCL_EXPORT_REF bool ProgressiveStreamingContainer::createProtocolObjects()
{
    if (!ProtocolContainer::createProtocolObjects()) return false;

    iProtocol        = OSCL_NEW(ProgressiveStreaming, ());
    iNodeOutput      = OSCL_NEW(pvProgressiveStreamingOutput, (iNode));
    iDownloadControl  = OSCL_NEW(progressiveStreamingControl, ());
    iDownloadProgess  = OSCL_NEW(ProgressiveStreamingProgress, ());
    iEventReport         = OSCL_NEW(progressiveStreamingEventReporter, (iNode));
    iCfgFileContainer = OSCL_NEW(PVProgressiveStreamingCfgFileContainer, (iDownloadSource));
    iUserAgentField  = OSCL_NEW(UserAgentFieldForProgDownload, ());
    iDownloadSource  = OSCL_NEW(PVMFDownloadDataSourceContainer, ());

    if (!iProtocol      || !iNodeOutput  || !iDownloadControl  ||
            !iDownloadProgess || !iEventReport || !iCfgFileContainer ||
            !iUserAgentField  || !iDownloadSource) return false;

    if (iNodeOutput)
    {
        iNodeOutput->setDataStreamSourceRequestObserver((PvmiDataStreamRequestObserver*)iNode);
    }

    DownloadContainer::setEventReporterSupportObjects();
    return true;
}

OSCL_EXPORT_REF PVMFStatus ProgressiveStreamingContainer::doStop()
{
    PVMFStatus status = DownloadContainer::doStop();
    if (status != PVMFSuccess) return status;
    // For progressive streaming, tell the data stream to flush,
    // so that the socket buffer can be returned to socket node for reset
    iNodeOutput->flushDataStream();

    // set resume download mode for stop and play
    OsclSharedPtr<PVDlCfgFile> aCfgFile = iCfgFileContainer->getCfgFile();
    aCfgFile->SetNewSession(true); // don't set resume download session for the next time
    if (aCfgFile->GetCurrentFileSize() >= aCfgFile->GetOverallFileSize()) aCfgFile->SetCurrentFileSize(0);

    return PVMFSuccess;
}

OSCL_EXPORT_REF PVMFStatus ProgressiveStreamingContainer::doSeek(PVMFNodeCommand& aCmd)
{
    TOsclFileOffset newOffset = getSeekOffset(aCmd);

    LOGINFODATAPATH((0, "PVMFProtocolEngineNode::DoReposition()->ProgressiveStreamingContainer::DoSeek : reposition offset=%u, iInterfaceState=%d",
                     (uint32)newOffset, (uint32)iObserver->GetObserverState()));

    return doSeekBody(newOffset);
}

OSCL_EXPORT_REF TOsclFileOffset ProgressiveStreamingContainer::getSeekOffset(PVMFNodeCommand& aCmd)
{
    //extract the parameters.
    OsclAny* aRequestData;
    aCmd.PVMFNodeCommand::Parse(aRequestData);

    TOsclFileOffset *newOffset_tmp = (TOsclFileOffset*)(aRequestData);
    TOsclFileOffset newOffset = *newOffset_tmp;

    return newOffset;
}

OSCL_EXPORT_REF PVMFStatus ProgressiveStreamingContainer::doSeekBody(TOsclFileOffset aNewOffset)
{
    // reset streaming done and session done flag to restart streaming
    ProtocolStateCompleteInfo aInfo;
    iInterfacingObjectContainer->setProtocolStateCompleteInfo(aInfo, true);

    // HTTP GET request looks at the current file size to determine is Range header is needed
    // TBD, there may be a better way to do this
    OsclSharedPtr<PVDlCfgFile> aCfgFile = iCfgFileContainer->getCfgFile();
    aCfgFile->SetCurrentFileSize(aNewOffset);

    // Reconnect and send new GET request
    iProtocol->seek(aNewOffset);
    startDataFlowByCommand();

    return PVMFPending;
}

OSCL_EXPORT_REF bool ProgressiveStreamingContainer::completeRepositionRequest()
{
    PVMFNodeCommand *pCmd = iObserver->FindPendingCmd(PVPROTOCOLENGINE_NODE_CMD_DATASTREAM_REQUEST_REPOSITION);
    if (pCmd == NULL) return false;

    OsclAny* aRequestData;
    PvmiDataStreamCommandId aDataStreamCmdId;
    pCmd->PVMFNodeCommand::Parse(aRequestData, aDataStreamCmdId);

    // set current file offset to the byte range request offset
    TOsclFileOffset *newOffset_tmp = (TOsclFileOffset*)(aRequestData);
    TOsclFileOffset newOffset = *newOffset_tmp;

    iNodeOutput->seekDataStream(newOffset);
    iNodeOutput->setCurrentOutputSize(newOffset);
    iDownloadControl->setPrevDownloadSize(newOffset);

    // find out if download was completed for the previous GET request
    // reset initial buffering algo variables
    iDownloadControl->clearPerRequest();

    // Form a command response
    PVMFCmdResp resp(aDataStreamCmdId, pCmd->iContext, PVMFSuccess, NULL, NULL);
    // Make the Command Complete notification
    iNodeOutput->DataStreamCommandCompleted(resp);
    pCmd->Destroy();

    moveToStartedState();
    return true;
}

OSCL_EXPORT_REF bool ProgressiveStreamingContainer::completeStartCmd()
{
    PVMFNodeCommand *pCmd = iObserver->FindPendingCmd(PVMF_GENERIC_NODE_START);
    if (pCmd == NULL) return false;

    // During PPB when byte-seek enabled inside PE node, if the current pending Start command was issued after Pause, then complete the pending command.
    if ((iProtocol->getByteSeekMode() == BYTE_SEEK_SUPPORTED) && (iStartAfterPause == true))
    {
        iObserver->CompletePendingCmd(PVMFSuccess);
        iObserver->SetObserverState((uint32)EPVMFNodeStarted);
        iStartAfterPause = false;
        return true;
    }
    return false;
}

OSCL_EXPORT_REF bool ProgressiveStreamingContainer::downloadUpdateForHttpHeaderAvailable()
{
    return iProtocol->updateSeekMode();
}

void ProgressiveStreamingContainer::moveToStartedState()
{
    DownloadContainer::setEventReporterSupportObjects();
    iObserver->SetObserverState((uint32)EPVMFNodeStarted);
    iEventReport->startRealDataflow(); // since the state gets changed to started state, enable the buffer status update
}

OSCL_EXPORT_REF void ProgressiveStreamingContainer::updateDownloadControl(const bool isDownloadComplete)
{
    bool downloadComplete = isDownloadComplete;
    if (downloadComplete && iObserver->IsRepositionCmdPending())
    {
        // if there is a repositioning request pending for progressive streaming,
        // do not send resume notification due to download complete
        downloadComplete = false;
    }

    // check resume notification
    if (iDownloadControl->checkResumeNotification(downloadComplete) == 1)
    {
        LOGINFODATAPATH((0, "ProgressiveStreamingContainer::updateDownloadControl, send data ready event to parser node, downloadComplete=false"));
        // report data ready event
        iEventReport->sendDataReadyEvent();
    }

    // update download progress
    iDownloadProgess->update(isDownloadComplete);
}

OSCL_EXPORT_REF bool ProgressiveStreamingContainer::needToCheckResumeNotificationMaually()
{
    iEventReport->enableBufferingCompleteEvent();

    if (DownloadContainer::needToCheckResumeNotificationMaually()) return true;
    return (iNodeOutput->getAvailableOutputSize() == 0 && iEnableInfoUpdate);
}

OSCL_EXPORT_REF bool ProgressiveStreamingContainer::doInfoUpdate(const uint32 downloadStatus)
{
    // For pending reposition request, don't do auto-resume checking
    if (!iEnableInfoUpdate) return true;
    return DownloadContainer::doInfoUpdate(downloadStatus);
}

OSCL_EXPORT_REF bool ProgressiveStreamingContainer::doPause()
{
    // Pause the PE node alongwith disconnection with the server, in case of PPB when byte-seek param is enabled inside PE node.
    if (iProtocol->getByteSeekMode() == BYTE_SEEK_SUPPORTED)
    {
        // Disconnection caused with the server
        if (iPortInForData) sendSocketDisconnectCmd(iPortInForData);

        iStartAfterPause = true;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////
//////  pvProgressiveStreamingOutput implementation
///////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF pvProgressiveStreamingOutput::pvProgressiveStreamingOutput(PVMFProtocolEngineNodeOutputObserver *aObserver) :
        pvHttpDownloadOutput(aObserver),
        iSourceRequestObserver(NULL)
{
    ;
}

OSCL_EXPORT_REF int32 pvProgressiveStreamingOutput::openDataStream(OsclAny* aInitInfo)
{
    int32 status = pvHttpDownloadOutput::openDataStream(aInitInfo);
    if (status == PVMFSuccess && isOpenDataStream)
    {
        // protocol engine node is the observer
        PvmiDataStreamStatus dsStatus = iDataStream->SetSourceRequestObserver(*iSourceRequestObserver);
        if ((dsStatus != PVDS_NOT_SUPPORTED) && (dsStatus != PVDS_SUCCESS)) return PROCESS_DATA_STREAM_OPEN_FAILURE;
    }
    return status;
}

/* This function is the call back function from Write data stream. After the successful registration for
   the callback to write data stream, it will be called by write data stream whenever it has enough space
   to store the data. */
void pvProgressiveStreamingOutput::DataStreamCommandCompleted(const PVMFCmdResp& aCmdResp)
{
    if (PVMFSuccess == aCmdResp.GetCmdStatus())
    {
        /* here we checkfor conext data with this pointer. If it is equal to this pointer then this callback
            is corresponding to the QueryWriteCapacity. Hence handle it accordingly by sending the remaining
            data. */
        if (this == aCmdResp.GetContext())
        {
            LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::DataStreamCommandCompleted got notification of empy space. Flush the remaing data"));
            flushData();
        }
        else
        {
            if (iDataStream) iDataStream->SourceRequestCompleted(aCmdResp);
        }
    }
}

OSCL_EXPORT_REF int32 pvProgressiveStreamingOutput::flushData(const uint32 aOutputType)
{
    int32 status = PVMFProtocolEngineNodeOutput::flushData(aOutputType);
    if (status != PROCESS_SUCCESS) return status;
    LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::flushData: iOutputFramesQueue size [%d]", iOutputFramesQueue.size()));
    while (!iOutputFramesQueue.empty())
    {
        uint32 res = writeToDataStream(iOutputFramesQueue[0], iPendingOutputDataQueue);
        if (0xFFFFFFFF == res) return PROCESS_OUTPUT_TO_DATA_STREAM_FAILURE; // This is the error case.
        else if (0 == res) break; //This is not the error case. Just we didn't have enough space to write. No need to error out. Just break.
        //LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::flushData: Erasing form iOutputFramesQueue"));
        iOutputFramesQueue.erase(iOutputFramesQueue.begin());
    }
    return PROCESS_SUCCESS;
}

uint32 pvProgressiveStreamingOutput::writeToDataStream(OUTPUT_DATA_QUEUE &aOutputQueue, PENDING_OUTPUT_DATA_QUEUE &aPendingOutputQueue)
{
    uint32 totalFragSize = 0;

    // Memory Buffer Data Stream takes memory fragments
    // Go through the queue, remove the frags, write them to the data stream
    // If the data stream is holding onto the frags, add the frags to a different queue, to be deleted later
    // Otherwise the frags are deleted in here
    while (!aOutputQueue.empty())
    {
        OsclRefCounterMemFrag frag = aOutputQueue.front();
        // make a copy otherwise erase will destroy it
        OsclRefCounterMemFrag* copyFrag = OSCL_NEW(OsclRefCounterMemFrag, (frag));
        uint32 fragSize = copyFrag->getMemFragSize();
        PvmiDataStreamStatus status = PassToDataStream(copyFrag, fragSize);
        if (PVDS_PENDING == status || PVDS_SUCCESS == status)
        {
            // Remove from output queue
            aOutputQueue.erase(aOutputQueue.begin());
            totalFragSize += fragSize;
            iCurrTotalOutputSize += totalFragSize;
        }
        else if (PVDS_NO_MEMORY == status)
        {
            return 0;
        }
        else
        {
            return 0xFFFFFFFF;
        }
    }
    LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::writeToDataStream() SIZE= %d , SEQNUM=%d, iCurrTotalOutputSize=%d, port=0x%x",
                     totalFragSize, iCounter++, iCurrTotalOutputSize, iPortIn));
    return totalFragSize;
}

PvmiDataStreamStatus pvProgressiveStreamingOutput::PassToDataStream(OsclRefCounterMemFrag* aFrag, uint32 fragSize)
{
    PvmiDataStreamStatus status = iDataStream->Write(iSessionID, aFrag, fragSize);
    if (PVDS_PENDING == status)
    {
        // This buffer is now part of the data stream cache
        // and cannot be freed until it is returned later on
        // Move the mem frag to the pending queue
        iPendingOutputDataQueue.push_back(aFrag);
    }
    else if (PVDS_NO_MEMORY == status)
    {
        OSCL_DELETE(aFrag); // Delete the frag. Otherwise memory leak will happen.
        RegisterForWriteNotification(fragSize);
    }
    else
    {
        // Done with this frag
        // free the reference
        LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::writeToDataStream: Releaseing the copyFrag!"));
        OSCL_DELETE(aFrag);
    }
    return status;
}
void pvProgressiveStreamingOutput::RegisterForWriteNotification(uint32 afragSize)
{
    if (iDataStream)
    {
        if (!iDataStream->IsWriteNotificationPending(iSessionID)) // Check whether notification is already pending with data stream or not.
        {
            PvmiDataStreamObserver *observer = OSCL_STATIC_CAST(PvmiDataStreamObserver*, this);
            /* Here we are passing context data as this pointer inorder to differentiate the
                callback DataStreamCommandCompleted is because of this request or something else like reposition e.t.c. */
            iDataStream->RequestWriteCapacityNotification(iSessionID,
                    *observer, afragSize, (OsclAny*)this);
            LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::RegisterForWriteNotification() register for the WriteNotification as there is not enough memory in write data stream"));
        }
        else
        {
            LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::RegisterForWriteNotification() WriteNotification is already pending with Write Data Stream."));
        }
    }
}

OSCL_EXPORT_REF bool pvProgressiveStreamingOutput::releaseMemFrag(OsclRefCounterMemFrag* aFrag)
{
    bool bFound = false;
    LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::releaseMemFrag(), frag=%x", aFrag->getMemFragPtr()));
    for (uint32 i = 0; i < iPendingOutputDataQueue.size(); i++)
    {
        // Find the frag in the queue and remove it
        OsclRefCounterMemFrag* frag = iPendingOutputDataQueue[i];
        if (aFrag->getMemFragPtr() == frag->getMemFragPtr())
        {
            LOGINFODATAPATH((0, "pvProgressiveStreamingOutput::releaseMemFrag(), found frag %x in pending Q", aFrag->getMemFragPtr()));
            iPendingOutputDataQueue.erase(&iPendingOutputDataQueue[i]);
            OSCL_DELETE(frag);
            bFound = true;
            break;
        }
        // TBD, how do we free the reference
    }
    return bFound;
}

OSCL_EXPORT_REF void pvProgressiveStreamingOutput::setContentLength(TOsclFileOffset aLength)
{
    if (iDataStream) iDataStream->SetContentLength(aLength);
}

OSCL_EXPORT_REF void pvProgressiveStreamingOutput::flushDataStream()
{
    // tell the data stream to flush all buffered data
    // for MBDS, empty temp cache and release mem buffers
    if (iDataStream) iDataStream->Flush(iSessionID);
}

OSCL_EXPORT_REF bool pvProgressiveStreamingOutput::seekDataStream(const TOsclFileOffset aSeekOffset)
{
    if (!iDataStream) return false;
    return (iDataStream->Seek(iSessionID, aSeekOffset, PVDS_SEEK_SET) == PVDS_SUCCESS);
}


////////////////////////////////////////////////////////////////////////////////////
//////  progressiveStreamingControl implementation
////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF progressiveStreamingControl::progressiveStreamingControl() : progressiveDownloadControl(), iSetProtocolInfo(false)
{
    ;
}

OSCL_EXPORT_REF void progressiveStreamingControl::requestResumeNotification(const uint32 currentNPTReadPosition, bool& aDownloadComplete, bool& aNeedSendUnderflowEvent)
{
    LOGINFODATAPATH((0, "progressiveStreamingControl::requestResumeNotification() IN, iPlaybackUnderflow=%d, iRequestResumeNotification=%d, iDownloadComplete=%d, will manually set iDownloadComplete=false",
                     (uint32)iPlaybackUnderflow, (uint32)iRequestResumeNotification, (uint32)iDownloadComplete));

    iDownloadComplete = aDownloadComplete = false;
    iSendDownloadCompleteNotification = false;
    pvDownloadControl::requestResumeNotification(currentNPTReadPosition, aDownloadComplete, aNeedSendUnderflowEvent);
}

OSCL_EXPORT_REF void progressiveStreamingControl::clearPerRequest()
{
    // for progressive playback
    // after each repositioning (aka new GET request)
    // the following variables must be reset
    // to enable auto pause and resume to function properly
    iDlAlgoPreConditionMet = false;
    iDownloadComplete      = false;
    iSendDownloadCompleteNotification = false;
}

void progressiveStreamingControl::setProtocolInfo()
{
    if (iSetProtocolInfo) return;
    if (!iProgDownloadSI || !iProtocol) return;

    iInfoKvpVec.clear();

    bool aIsByteSeekNotSupported;
    ByteSeekMode aByteSeekMode = iProtocol->getByteSeekMode();

    if (aByteSeekMode == BYTE_SEEK_UNSUPPORTED)
    {
        aIsByteSeekNotSupported = true;
    }
    else
        return;

    // byte seek mode
    PvmiKvp byteSeekModeKVP;
    OSCL_StackString<256> byteSeekModeKVPString = _STRLIT_CHAR("x-pvmf/net/is-byte-seek-not-supported;valtype=bool");
    byteSeekModeKVP.key = byteSeekModeKVPString.get_str();
    byteSeekModeKVP.value.bool_value = aIsByteSeekNotSupported;
    iInfoKvpVec.push_back(&byteSeekModeKVP);

    iProgDownloadSI->setProtocolInfo(iInfoKvpVec);
    iSetProtocolInfo = true;
}

////////////////////////////////////////////////////////////////////////////////////
//////  ProgressiveStreamingProgress implementation
////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool ProgressiveStreamingProgress::calculateDownloadPercent(TOsclFileOffset &aDownloadProgressPercent)
{
    // in progessive streaming, the getContentLength will change after new GET request
    // from known to 0 and then to known again
    TOsclFileOffset fileSize = iProtocol->getContentLength();
    if (!fileSize && iContentLength)
    {
        fileSize = iContentLength;
    }
    if (fileSize) iContentLength = fileSize;

    return ProgressiveDownloadProgress::calculateDownloadPercentBody(aDownloadProgressPercent, fileSize);
}

////////////////////////////////////////////////////////////////////////////////////
//////  progressiveStreamingEventReporter implementation
////////////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void progressiveStreamingEventReporter::reportBufferStatusEvent(const uint32 aDownloadPercent)
{
    // calculate buffer fullness
    int32 downloadPercent = (int32)aDownloadPercent;

    uint32 aBufferFullness = getBufferFullness();
    if (aBufferFullness == 0xffffffff) return;

    iObserver->ReportEvent(PVMFInfoBufferingStatus,
                           (OsclAny*)aBufferFullness,
                           PVMFPROTOCOLENGINENODEInfo_BufferingStatus,
                           &downloadPercent,
                           sizeof(aDownloadPercent));
    LOGINFODATAPATH((0, "progressiveStreamingEventReporter::reportBufferStatusEvent() DOWNLOAD PERCENTAGE: %d", aDownloadPercent));
}

uint32 progressiveStreamingEventReporter::getBufferFullness()
{

    uint32 aCacheSize = iNodeOutput->getMaxAvailableOutputSize();
    if (aCacheSize == 0) return 0xffffffff;
    TOsclFileOffset aCacheFilledSize = iNodeOutput->getAvailableOutputSize();
    if (aCacheFilledSize >= (TOsclFileOffset)aCacheSize) return 100;

    // avoid fix-point multiplication overflow
    uint32 aBufferEmptiness = 0xffffffff;
    if (IS_OVERFLOW_FOR_100x(aCacheFilledSize) > 0)
    {
        aBufferEmptiness = ((uint32)aCacheFilledSize >> PVPROTOCOLENGINE_DOWNLOAD_BYTE_PERCENTAGE_DLSIZE_RIGHTSHIFT_FACTOR) * 100 /
                           (aCacheSize >> PVPROTOCOLENGINE_DOWNLOAD_BYTE_PERCENTAGE_DLSIZE_RIGHTSHIFT_FACTOR);
    }
    else
    {
        aBufferEmptiness = (uint32)aCacheFilledSize * 100 / aCacheSize;
    }

    return 100 - aBufferEmptiness;
}

