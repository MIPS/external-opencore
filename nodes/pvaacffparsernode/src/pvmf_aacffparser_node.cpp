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

#ifndef PVMF_AACFFPARSER_NODE_H_INCLUDED
#include "pvmf_aacffparser_node.h"
#endif

#define PVAACFF_MEDIADATA_POOLNUM 8
#define PVAACFF_MEDIADATA_CHUNKSIZE 128


PVMFAACFFParserNode::PVMFAACFFParserNode(int32 aPriority)
        : PVMFNodeInterfaceImpl(aPriority, "PVMFAACFFParserNode")
{

    iDownloadComplete          = false;

    iOutPort = NULL;
    iAACParser = NULL;
    iUseCPMPluginRegistry = false;
    iFileHandle = NULL;
    iFirstFrame = true;
    iDataPathLogger            = NULL;
    iClientClock               = NULL;
    iCPM                       = NULL;
    iCPMSessionID              = 0xFFFFFFFF;
    iCPMContentType            = PVMF_CPM_CONTENT_FORMAT_UNKNOWN;
    iCPMContentAccessFactory   = NULL;
    iCPMInitCmdId              = 0;
    iCPMOpenSessionCmdId       = 0;
    iCPMRegisterContentCmdId   = 0;
    iCPMRequestUsageId         = 0;
    iCPMUsageCompleteCmdId     = 0;
    iCPMCloseSessionCmdId      = 0;
    iCPMResetCmdId             = 0;
    iRequestedUsage.key        = NULL;
    iApprovedUsage.key         = NULL;
    iAuthorizationDataKvp.key  = NULL;
    iPreviewMode               = false;
    oSourceIsCurrent           = false;

    iCPMLicenseInterfacePVI = NULL;
    iCPMLicenseInterface = NULL;
    iCPMMetaDataExtensionInterface = NULL;
    iCPMGetMetaDataValuesCmdId     = 0;
    iCPMGetLicenseInterfaceCmdId   = 0;
    iAACParserNodeMetadataValueCount = 0;

    iDownloadProgressInterface = NULL;
    iDownloadFileSize          = 0;
    iDataStreamInterface       = NULL;
    iDataStreamReadCapacityObserver = NULL;
    iDataStreamFactory         = NULL;
    iAutoPaused                = false;
    int32 err;
    OSCL_TRY(err, ConstructL());

    if (err != OsclErrNone)
    {
        //if a leave happened, cleanup and re-throw the error
        iInputCommands.clear();
        iNodeCapability.iInputFormatCapability.clear();
        iNodeCapability.iOutputFormatCapability.clear();
        OSCL_CLEANUP_BASE_CLASS(PVMFNodeInterfaceImpl);
        OSCL_LEAVE(err);
    }
}



void PVMFAACFFParserNode::ConstructL()
{
    //Set the node capability data.
    //This node can support an unlimited number of ports.
    iNodeCapability.iCanSupportMultipleInputPorts = false;
    iNodeCapability.iCanSupportMultipleOutputPorts = false;
    iNodeCapability.iHasMaxNumberOfPorts = true;
    iNodeCapability.iMaxNumberOfPorts = 2;//no maximum
    iNodeCapability.iInputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_AACFF));
    iNodeCapability.iInputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_RAWAAC));
    iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_MPEG4_AUDIO));

    iAvailableMetadataKeys.reserve(PVMF_AAC_NUM_METADATA_VALUES);
    iAvailableMetadataKeys.clear();

    iDataPathLogger = PVLogger::GetLoggerObject("datapath.sourcenode.aacparsernode");
    iFileServer.Connect();
}

PVMFAACFFParserNode::~PVMFAACFFParserNode()
{
    iDataPathLogger = NULL;

    if (iDataStreamInterface != NULL)
    {
        PVInterface* iFace = OSCL_STATIC_CAST(PVInterface*, iDataStreamInterface);
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        if (iDataStreamFactory)
        {
            iDataStreamFactory->DestroyPVMFCPMPluginAccessInterface(uuid, iFace);
        }
        iDataStreamInterface = NULL;
    }

    Cancel();
    if (iCPM != NULL)
    {
        iCPM->ThreadLogoff();
        PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
        iCPM = NULL;
    }

    if (IsAdded())
    {
        RemoveFromScheduler();
    }

    ReleaseTrack();
    CleanupFileSource();
    iFileServer.Close();

    if (iRequestedUsage.key)
    {
        OSCL_ARRAY_DELETE(iRequestedUsage.key);
        iRequestedUsage.key = NULL;
    }

    if (iApprovedUsage.key)
    {
        OSCL_ARRAY_DELETE(iApprovedUsage.key);
        iApprovedUsage.key = NULL;
    }

    if (iAuthorizationDataKvp.key)
    {
        OSCL_ARRAY_DELETE(iAuthorizationDataKvp.key);
        iAuthorizationDataKvp.key = NULL;
    }

    if (iFileHandle != NULL)
    {
        OSCL_ARRAY_DELETE(iFileHandle);
        iFileHandle = NULL;

    }

    if (iDownloadProgressInterface != NULL)
    {
        iDownloadProgressInterface->cancelResumeNotification();
        iDownloadProgressInterface->removeRef();
    }

}

// From PVMFMetadataExtensionInterface
uint32 PVMFAACFFParserNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    uint32 numkeys = aKeyList.size();
    if (iAACParser == NULL || numkeys == 0)
    {
        return 0;
    }

    uint32 numvalentries = 0;

    if ((iCPMMetaDataExtensionInterface != NULL))
    {
        numvalentries =
            iCPMMetaDataExtensionInterface->GetNumMetadataValues(aKeyList);
    }

    PVMFMetadataList* keylistptr = &aKeyList;

    // If "all" key is sent, browse all keys.
    if (keylistptr->size() == 1)
    {
        if (oscl_strncmp((*keylistptr)[0].get_cstr(),
                         PVAAC_ALL_METADATA_KEY,
                         oscl_strlen(PVAAC_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &iAvailableMetadataKeys;
        }
    }

    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {

        if (iAACParser->IsID3Frame((*keylistptr)[lcv]))
        {
            ++numvalentries;
        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_DURATION_KEY) &&
                 iAACFileInfo.iDuration > 0)
        {
            // Duration
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_PROFILE_KEY) &&
                 iAACFileInfo.iProfile > 0)
        {
            // Profile
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_CHANNELS_KEY) &&
                 iAACFileInfo.iChannels > 0)
        {
            // Channels
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(),  PVAACMETADATA_RANDOM_ACCESS_DENIED_KEY))
        {
            // random-acess-denied
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(),  PVAACMETADATA_NUMTRACKS_KEY))
        {
            // Number of tracks
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_TRACKINFO_BITRATE_KEY) == 0) &&
                 iAACFileInfo.iBitrate > 0)
        {
            // Bitrate
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_TRACKINFO_SAMPLERATE_KEY) == 0) &&
                 iAACFileInfo.iSampleFrequency > 0)
        {
            // Sampling rate
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_TRACKINFO_AUDIO_FORMAT_KEY) == 0) &&
                 iAACFileInfo.iFormat != EAACUnrecognized)
        {
            // Format
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_TRACKINFO_SELECTED_KEY) == 0)
        {
            // Format
            // Increment the counter for the number of values found so far
            ++numvalentries;
        }
    }

    return numvalentries;
}

// From PVMFMetadataExtensionInterface
PVMFCommandId PVMFAACFFParserNode::GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList,
        Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 starting_index, int32 max_entries, const OsclAny* aContext)
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::GetNodeMetadataValues called"));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_GETNODEMETADATAVALUES, aKeyList, aValueList, starting_index, max_entries, aContext);
    return QueueCommandL(cmd);
}


// From PVMFMetadataExtensionInterface
PVMFStatus PVMFAACFFParserNode::ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 start, uint32 end)
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::ReleaseNodeMetadataValues called"));

    if (start > end || aValueList.size() == 0)
    {
        PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACFFParserNode::ReleaseNodeMetadataValues() Invalid start/end index"));
        return PVMFErrArgument;
    }

    if (iCPMMetaDataExtensionInterface != NULL)
    {
        PVMFStatus status = iCPMMetaDataExtensionInterface->ReleaseNodeMetadataValues(aValueList, start, end);
        if (status != PVMFSuccess)
        {
            return status;
        }
    }
    end = OSCL_MIN(aValueList.size(), iAACParserNodeMetadataValueCount);

    for (uint32 i = start; i < end; i++)
    {
        if (aValueList[i].key != NULL)
        {
            switch (GetValTypeFromKeyString(aValueList[i].key))
            {
                case PVMI_KVPVALTYPE_WCHARPTR:
                    if (aValueList[i].value.pWChar_value != NULL)
                    {
                        OSCL_ARRAY_DELETE(aValueList[i].value.pWChar_value);
                        aValueList[i].value.pWChar_value = NULL;
                    }
                    break;

                case PVMI_KVPVALTYPE_CHARPTR:
                    if (aValueList[i].value.pChar_value != NULL)
                    {
                        OSCL_ARRAY_DELETE(aValueList[i].value.pChar_value);
                        aValueList[i].value.pChar_value = NULL;
                    }
                    break;

                case PVMI_KVPVALTYPE_UINT32:
                case PVMI_KVPVALTYPE_UINT8:
                    // No memory to free for these valtypes
                    break;

                default:
                    // Should not get a value that wasn't created from here
                    break;
            }

            OSCL_ARRAY_DELETE(aValueList[i].key);
            aValueList[i].key = NULL;
        }
    }

    return PVMFSuccess;
}

// From PvmfDataSourcePlaybackControlInterface
PVMFCommandId PVMFAACFFParserNode::SetDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aActualNPT
        , PVMFTimestamp& aActualMediaDataTS
        , bool aSeekToSyncPoint
        , uint32 aStreamID
        , OsclAny* aContext)
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::SetDataSourcePosition called"));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION, aTargetNPT, &aActualNPT,
                                   &aActualMediaDataTS, aSeekToSyncPoint, aStreamID, aContext);
    return QueueCommandL(cmd);
}


// From PvmfDataSourcePlaybackControlInterface
PVMFCommandId PVMFAACFFParserNode::QueryDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aActualNPT
        , bool aSeekToSyncPoint
        , OsclAny* aContext)
{
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, &aActualNPT, aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFAACFFParserNode::QueryDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aSeekPointBeforeTargetNPT
        , PVMFTimestamp& aSeekPointAfterTargetNPT
        , OsclAny* aContext
        , bool aSeekToSyncPoint)
{
    OSCL_UNUSED_ARG(aSeekPointAfterTargetNPT);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFAACFFParserNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFNodeCommand cmd;
    // Construct not changes. aSeekPointBeforeTargetNPT will act as aActualtNPT
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, &aSeekPointBeforeTargetNPT, aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}

// From PvmfDataSourcePlaybackControlInterface
PVMFCommandId PVMFAACFFParserNode::SetDataSourceRate(PVMFSessionId aSessionId
        , int32 aRate
        , PVMFTimebase* aTimebase
        , OsclAny* aContext)
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::SetDataSourceRate called"));


    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_RATE, aRate, aTimebase, aContext);
    return QueueCommandL(cmd);
}


void PVMFAACFFParserNode::Run()
{
    //Process async node commands.
    if (!iInputCommands.empty())
    {
        ProcessCommand();
    }

    // Send outgoing messages
    if (iInterfaceState == EPVMFNodeStarted || IsFlushPending())
    {
        PVAACFFNodeTrackPortInfo* trackPortInfoPtr = &iTrack;

        ProcessPortActivity(trackPortInfoPtr);

        if (CheckForPortRescheduling())
        {
            //Reschedule since there is atleast one port that needs processing
            Reschedule();
        }
    }

    // Detect Flush command completion.
    if (IsFlushPending()
            && iOutPort
            && iOutPort->OutgoingMsgQueueSize() == 0)
    {
        //Flush is complete.
        //resume port input so the ports can be re-started.
        iOutPort->ResumeInput();
        CommandComplete(iCurrentCommand, PVMFSuccess);
    }
}

bool PVMFAACFFParserNode::RetrieveTrackConfigInfo(PVAACFFNodeTrackPortInfo& aTrackPortInfo)
{
    // Check if the track has decoder config info
    uint32 specinfosize = iAACParser->GetTrackDecoderSpecificInfoSize();
    if (specinfosize == 0)
    {
        // There is no decoder specific info so return as error. AAC requires audio config header
        return false;
    }

    // Retrieve the decoder specific info from file parser
    uint8* specinfoptr = iAACParser->GetTrackDecoderSpecificInfoContent();

    if (specinfoptr == NULL)
    {
        // Error
        return false;
    }

    // Create mem frag for decoder specific config
    OsclMemAllocDestructDealloc<uint8> my_alloc;
    OsclRefCounter* my_refcnt;
    uint aligned_refcnt_size = oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));
    uint8* my_ptr = NULL;
    int32 errcode = 0;
    OSCL_TRY(errcode, my_ptr = (uint8*) my_alloc.ALLOCATE(aligned_refcnt_size + specinfosize));
    OSCL_FIRST_CATCH_ANY(errcode, return false);

    my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
    my_ptr += aligned_refcnt_size;

    OsclMemoryFragment memfrag;
    memfrag.len = specinfosize;
    memfrag.ptr = my_ptr;

    OsclRefCounterMemFrag configinfo_refcntmemfrag(memfrag, my_refcnt, specinfosize);

    // Copy the decoder specific info to the memory fragment
    oscl_memcpy(memfrag.ptr, specinfoptr, specinfosize);

    // Save format specific info
    aTrackPortInfo.iFormatSpecificConfig = configinfo_refcntmemfrag;

    return true;
}

PVMFStatus PVMFAACFFParserNode::RetrieveMediaSample(PVAACFFNodeTrackPortInfo* aTrackInfoPtr,
        PVMFSharedMediaDataPtr& aMediaDataOut)
{

    // Create a data buffer from pool
    int errcode = OsclErrNone;
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImplOut;
    mediaDataImplOut = aTrackInfoPtr->iResizableSimpleMediaMsgAlloc->allocate(MAXTRACKDATASIZE);

    if (mediaDataImplOut.GetRep() == NULL)
    {
        errcode = OsclErrNoResources;
        OsclMemPoolResizableAllocatorObserver* resizableAllocObs =
            OSCL_STATIC_CAST(OsclMemPoolResizableAllocatorObserver*, aTrackInfoPtr);

        // Enable flag to receive event when next deallocate() is called on pool
        aTrackInfoPtr->iTrackDataMemoryPool->notifyfreeblockavailable(*resizableAllocObs);
        return PVMFErrBusy;
    }

    // Now create a PVMF media data from pool
    aMediaDataOut = PVMFMediaData::createMediaData(mediaDataImplOut, aTrackInfoPtr->iMediaDataMemPool);

    if (aMediaDataOut.GetRep() == NULL)
    {
        errcode = OsclErrNoResources;
        OsclMemPoolFixedChunkAllocatorObserver* fixedChunkObs =
            OSCL_STATIC_CAST(OsclMemPoolFixedChunkAllocatorObserver*, aTrackInfoPtr);

        // Enable flag to receive event when next deallocate() is called on pool
        aTrackInfoPtr->iMediaDataMemPool->notifyfreechunkavailable(*fixedChunkObs);
        return PVMFErrBusy;
    }

    // Retrieve memory fragment to write to
    OsclRefCounterMemFrag refCtrMemFragOut;
    OsclMemoryFragment memFragOut;
    aMediaDataOut->getMediaFragment(0, refCtrMemFragOut);
    memFragOut.ptr = refCtrMemFragOut.getMemFrag().ptr;

    Oscl_Vector<uint32, OsclMemAllocator> payloadSizeVec;

    uint32 numsamples = NUM_AAC_RAW_STREAMS;
    // Set up the GAU structure
    GAU gau;
    gau.numMediaSamples = numsamples;
    gau.buf.num_fragments = 1;
    gau.buf.buf_states[0] = NULL;
    gau.buf.fragments[0].ptr = refCtrMemFragOut.getMemFrag().ptr;
    gau.buf.fragments[0].len = refCtrMemFragOut.getCapacity();

    bool oIncludeADTSHeaders = false;
    if ((aTrackInfoPtr->oADTS == true) &&
            (PVMF_AAC_PARSER_NODE_INCLUDE_ADTS_HEADERS == 1))
    {
        oIncludeADTSHeaders = true;
    }

    int32 retval = iAACParser->GetNextBundledAccessUnits(&numsamples, &gau, oIncludeADTSHeaders);
    uint32 actualdatasize = 0;
    for (uint32 i = 0; i < numsamples; i++)
    {
        actualdatasize += gau.info[i].len;
    }

    PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::RetrieveMediaSample() - Parser Returned Error - errCode=%d numSamples [%d]", retval, numsamples));
    if (retval == AACBitstreamObject::EVERYTHING_OK || (numsamples > 0))
    {
        memFragOut.len = actualdatasize;

        // Set Actual size
        aMediaDataOut->setMediaFragFilledLen(0, actualdatasize);

        // Resize memory fragment
        aTrackInfoPtr->iResizableSimpleMediaMsgAlloc->ResizeMemoryFragment(mediaDataImplOut);

        // Compute the continous timestamp
        aTrackInfoPtr->iContinuousTimeStamp = gau.info[0].ts + aTrackInfoPtr->iTimestampOffset;

        uint32 timestamp = 0xFFFFFFFF;

        if (gau.info[0].ts != 0xFFFFFFFF)
        {
            // Retrieve timestamp and convert to milliseconds
            aTrackInfoPtr->iClockConverter->update_clock(gau.info[0].ts + aTrackInfoPtr->iTimestampOffset);
            timestamp = aTrackInfoPtr->iClockConverter->get_converted_ts(1000);
        }

        // Set the media data's timestamp
        aTrackInfoPtr->iPrevSampleTimeStamp = gau.info[0].ts;
        aMediaDataOut->setTimestamp(timestamp);
        aMediaDataOut->setSeqNum(aTrackInfoPtr->iSeqNum);
        aMediaDataOut->setStreamID(iStreamID);
        if (aTrackInfoPtr->iSeqNum == 0)
        {
            aMediaDataOut->setFormatSpecificInfo(aTrackInfoPtr->iFormatSpecificConfig);
        }
        aTrackInfoPtr->iSeqNum += 1;

        // Set M bit to 1 always - ASF FF only outputs complete frames
        uint32 markerInfo = 0;
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;

        // Set Key Frame bit
        if (iFirstFrame)
        {
            markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT;
            iFirstFrame = false;
        }
        mediaDataImplOut->setMarkerInfo(markerInfo);
    }
    else if (retval == AACBitstreamObject::INSUFFICIENT_DATA)
    {
        payloadSizeVec.clear();
        if (iDownloadProgressInterface != NULL)
        {
            if (iDownloadComplete)
            {
                PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::RetrieveMediaSample() - Read Failure After Download completed"));
                aTrackInfoPtr->oEOSReached = true;
                return PVMFInfoEndOfData;
            }

            iDownloadProgressInterface->requestResumeNotification((uint32)aTrackInfoPtr->iPrevSampleTimeStamp,
                    iDownloadComplete);
            iAutoPaused = true;
            PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::RetrieveMediaSample() - Auto Pause Triggered - TS=%d", aTrackInfoPtr->iPrevSampleTimeStamp));
            PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::RetrieveMediaSample() - Auto Pause Triggered - TS=%d", aTrackInfoPtr->iPrevSampleTimeStamp));
            return PVMFErrBusy;
        }
        else
        {
            PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::RetrieveMediaSample() - EOS Received"));
            aTrackInfoPtr->oEOSReached = true;
            return PVMFInfoEndOfData;
        }
    }
    else if (retval == AACBitstreamObject::END_OF_FILE)
    {
        PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::RetrieveMediaSample() - END_OF_FILE Reached"));
        aTrackInfoPtr->oEOSReached = true;
        return PVMFInfoEndOfData;
    }
    else
    {
        PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::RetrieveMediaSample() - Sample Retrieval Failed Err[%d]", retval));
        return PVMFFailure;
    }

    return PVMFSuccess;
}

PVMFStatus PVMFAACFFParserNode::DoInit()
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::DoInit() Called"));

    if (EPVMFNodeInitialized == iInterfaceState)
    {
        PVMF_AACPARSERNODE_LOGINFOHI((0, "PVMFAACFFParserNode::DoInit() already in Initialized state"));
        return PVMFSuccess;
    }

    if (iCPM)
    {
        /*
         * Go thru CPM commands before parsing the file in case
         * of a new source file.
         * - Init CPM
         * - Open Session
         * - Register Content
         * - Get Content Type
         * - Approve Usage
         * In case the source file has already been parsed skip to
         * - Approve Usage
         */
        if (oSourceIsCurrent == false)
        {
            InitCPM();
        }
        else
        {
            RequestUsage();
        }
    }
    else
    {
        PVMFStatus status = CheckForAACHeaderAvailability();
        if (status != PVMFPending)
        {
            return status;
        }
    }
    return PVMFPending;
}

PVMFStatus PVMFAACFFParserNode::CheckForAACHeaderAvailability()
{
    iAACParser = PVMF_BASE_NODE_NEW(CAACFileParser, ());
    if (iAACParser == NULL)
    {
        PVMF_AACPARSERNODE_LOGINFOHI((0, "PVMFAACFFParserNode::CheckForAACHeaderAvailability() Instantion of AAC file parser failed"));
        return PVMFErrNoMemory;
    }

    uint32 headerSize32 = 0;
    if (iDataStreamInterface != NULL)
    {
        /*
         * First check if we have minimum number of bytes to recognize
         * the file and determine the header size.
         */
        TOsclFileOffset currCapacity = 0;
        iDataStreamInterface->QueryReadCapacity(iDataStreamSessionID,
                                                currCapacity);

        if (currCapacity < (TOsclFileOffset)AAC_MIN_DATA_SIZE_FOR_RECOGNITION)
        {
            iRequestReadCapacityNotificationID =
                iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                        *this,
                        AAC_MIN_DATA_SIZE_FOR_RECOGNITION);
            return PVMFPending;
        }

        CAACFileParams params;
        params.iClip = iSourceURL;
        params.iFileSession = &iFileServer;
        params.iCPMAccess = iDataStreamFactory;
        params.iHandle = iFileHandle;
        if (iSourceFormat == PVMF_MIME_RAWAAC)
        {
            params.iRawAACFile = true;
        }
        if (AAC_SUCCESS == iAACParser->getAACHeaderLen(params, false, &headerSize32))
        {
            if (currCapacity < (TOsclFileOffset)headerSize32)
            {
                iRequestReadCapacityNotificationID =
                    iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                            *this,
                            headerSize32);
                return PVMFPending;
            }
        }
    }
    PVMFStatus status = ParseAACFile();
    return status;
}


PVMFStatus PVMFAACFFParserNode::ParseAACFile()
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACFFParserNode::ParseFile() In"));

    iAACParser->SetDownloadFileSize(iDownloadFileSize);
    bool oParseCompleteFile = true;
    if (iDownloadProgressInterface != NULL)
    {
        //a PDL session
        oParseCompleteFile = false;
    }

    PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
    if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
    {
        dsFactory = iDataStreamFactory;
    }

    CAACFileParams params;
    params.iClip = iSourceURL;
    params.iFileSession = &iFileServer;
    params.iCPMAccess = dsFactory;
    params.iHandle = iFileHandle;
    if (iSourceFormat == PVMF_MIME_RAWAAC)
    {
        params.iRawAACFile = true;
    }
    if (!(iAACParser->InitAACFile(params, oParseCompleteFile)))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFAACFFParserNode::ParseFile() Parsing of AAC file parser failed"));
        OSCL_DELETE(iAACParser);
        iAACParser = NULL;
        return PVMFErrNoResources;
    }

    // Retrieve the file info from the parser
    iAACFileInfoValid = iAACParser->RetrieveFileInfo(iAACFileInfo);

    if (iDataStreamInterface)
    {
        TOsclFileOffset currCapacity = 0;
        iDataStreamInterface->QueryReadCapacity(iDataStreamSessionID,
                                                currCapacity);

        if (currCapacity <  AAC_MIN_DATA_SIZE_FOR_RECOGNITION)
        {
            iRequestReadCapacityNotificationID =
                iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                        *this,
                        AAC_MIN_DATA_SIZE_FOR_RECOGNITION);
            return PVMFPending;
        }
    }


    // Retrieve the ID3 metadata from the file if available
    PvmiKvpSharedPtrVector iID3Data;
    iID3DataValid = iAACParser->RetrieveID3Info(iID3Data);

    // Initialize the meta-data keys
    iAvailableMetadataKeys.clear();

    if (iDataStreamInterface)
    {
        int32 metadataSize = iAACParser->GetMetadataSize();
        iDataStreamInterface->MakePersistent(0, metadataSize);
    }

    int32 leavecode = 0;

    if (iAACFileInfoValid)
    {
        // Following keys are available when the AAC file has been parsed

        leavecode = 0;
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_DURATION_KEY));

        leavecode = 0;
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_NUMTRACKS_KEY));

        leavecode = 0;
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_RANDOM_ACCESS_DENIED_KEY));

        leavecode = 0;
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_CHANNELS_KEY));

        leavecode = 0;
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_PROFILE_KEY));

        if (iAACFileInfo.iBitrate > 0)
        {
            leavecode = 0;
            OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_TRACKINFO_BITRATE_KEY));
        }

        if (iAACFileInfo.iSampleFrequency > 0)
        {
            leavecode = 0;
            OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_TRACKINFO_SAMPLERATE_KEY));
        }

        leavecode = 0;
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_TRACKINFO_AUDIO_FORMAT_KEY));

        //set clip duration on download progress interface applicable to PDL sessions
        if ((iDownloadProgressInterface != NULL) && (iAACFileInfo.iDuration != 0))
        {
            iDownloadProgressInterface->setClipDuration(OSCL_CONST_CAST(uint32, iAACFileInfo.iDuration));
        }

        leavecode = 0;
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_back(PVAACMETADATA_TRACKINFO_SELECTED_KEY));
    }

    if (iID3DataValid)
    {
        leavecode = 0;
        OSCL_TRY(leavecode,
                 for (uint s = 0; s < iID3Data.size(); s++)
    {
        OSCL_HeapString<OsclMemAllocator> keystr((const char *)((*iID3Data[s]).key), oscl_strlen((const char *)((*iID3Data[s]).key)));
            iAvailableMetadataKeys.push_back(keystr);
        }
                );
    }

    return PVMFSuccess;
}

PVMFStatus PVMFAACFFParserNode::DoStop()
{

    // Stop data source
    if (iDataStreamInterface != NULL)
    {
        PVInterface* iFace = OSCL_STATIC_CAST(PVInterface*, iDataStreamInterface);
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        iDataStreamFactory->DestroyPVMFCPMPluginAccessInterface(uuid, iFace);
        iDataStreamInterface = NULL;
    }

    // Clear queued messages in ports
    if (iOutPort)
    {
        iOutPort->ClearMsgQueues();
    }

    // stop and reset position to beginning
    ResetAllTracks();
    iTrack.iTimestampOffset = 0;

    // Reset the AAC FF to beginning
    if (iAACParser)
    {
        uint32 tmpuint32 = 0;
        iAACParser->ResetPlayback(0, tmpuint32);
    }
    iStreamID = 0;

    return PVMFSuccess;
}

PVMFStatus PVMFAACFFParserNode::DoReset()
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::DoReset() Called"));

    if (iDownloadProgressInterface != NULL)
    {
        iDownloadProgressInterface->cancelResumeNotification();
    }

    if ((iAACParser) && (iCPM) && (iCPMContentType != PVMF_CPM_CONTENT_FORMAT_UNKNOWN))
    {
        SendUsageComplete();
        return PVMFPending;
    }
    else
    {
        /*
         * Reset without init completing, so just reset the parser node,
         * no CPM stuff necessary
         */
        return CompleteReset();
    }
}

PVMFStatus PVMFAACFFParserNode::CompleteReset()
{
    // stop and cleanup
    ReleaseTrack();
    CleanupFileSource();
    return PVMFSuccess;
}

PVMFStatus PVMFAACFFParserNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAACFFParserNode::DoRequestPort() In"));

    // Check to make sure the AAC file has been parsed
    if (!iAACParser)  return PVMFFailure;

    TPVAacFileInfo aacinfo;
    oscl_memset(&aacinfo, 0, sizeof(TPVAacFileInfo));

    if (!iAACParser->RetrieveFileInfo(aacinfo))
    {
        return PVMFFailure;
    }

    int32 tag;
    OSCL_String* mimetype;
    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, mimetype);

    //validate the tag...
    if (tag != PVMF_AAC_PARSER_NODE_PORT_TYPE_SOURCE)
    {
        //bad port tag
        PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACFFParserNode::DoRequestPort: Error - Invalid port tag"));
        return PVMFFailure;
    }

    //Allocate a new port
    int32 leavecode = 0;
    OSCL_TRY(leavecode, iOutPort = OSCL_NEW(PVMFAACFFParserOutPort, (PVMF_AAC_PARSER_NODE_PORT_TYPE_SOURCE, this)););
    if (leavecode != OsclErrNone || !iOutPort)
    {
        PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACFFParserNode::DoRequestPort: Error - Out of memory"));
        return PVMFErrNoMemory;

    }

    //validate the requested format
    if (mimetype)
    {
        PVMFFormatType fmt = mimetype->get_str();
        if (!iOutPort->IsFormatSupported(fmt))
        {
            PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACFFParserNode::DoRequestPort: Error - Invalid Format!"));
            PVMF_BASE_NODE_DELETE(iOutPort);
            iOutPort = NULL;
            return PVMFFailure;
        }
    }

    MediaClockConverter* clockconv = NULL;
    OsclMemPoolResizableAllocator* trackdatamempool = NULL;
    TrackDataMemPoolProxyAlloc* trackdatamempoolproxy = NULL;
    PVMFSimpleMediaBufferCombinedAlloc* mediadataimplalloc = NULL;
    PVMFMemPoolFixedChunkAllocator* mediadatamempool = NULL;

    OSCL_TRY(leavecode,

             clockconv = PVMF_BASE_NODE_NEW(MediaClockConverter, (aacinfo.iTimescale));

             trackdatamempool = PVMF_BASE_NODE_NEW(OsclMemPoolResizableAllocator, (2 * MAXTRACKDATASIZE));

             trackdatamempoolproxy = PVMF_BASE_NODE_NEW(TrackDataMemPoolProxyAlloc, (*trackdatamempool));

             mediadataimplalloc = PVMF_BASE_NODE_NEW(PVMFSimpleMediaBufferCombinedAlloc, (trackdatamempoolproxy));

             mediadatamempool = OSCL_NEW(PVMFMemPoolFixedChunkAllocator, ("AacFFPar", PVAACFF_MEDIADATA_POOLNUM, PVAACFF_MEDIADATA_CHUNKSIZE));
            );

    if (leavecode || !clockconv || !trackdatamempool || !trackdatamempoolproxy || !mediadataimplalloc || !mediadatamempool)
    {
        PVMF_BASE_NODE_DELETE(((PVMFAACFFParserOutPort*)iOutPort));
        iOutPort = NULL;

        if (clockconv)
        {
            PVMF_BASE_NODE_DELETE(clockconv);
        }
        if (trackdatamempool)
        {
            trackdatamempool->removeRef();
        }

        if (trackdatamempoolproxy)
        {
            PVMF_BASE_NODE_DELETE(trackdatamempoolproxy)
        }
        if (mediadataimplalloc)
        {
            PVMF_BASE_NODE_DELETE(mediadataimplalloc);
        }
        if (mediadatamempool)
        {
            PVMF_BASE_NODE_DELETE(mediadatamempool);
        }

        return PVMFErrNoMemory;
    }

    trackdatamempool->enablenullpointerreturn();
    mediadatamempool->enablenullpointerreturn();

    // Construct iTrack
    iTrack.iTrackId = 0;  // Only support 1 channel so far
    iTrack.iPort = iOutPort;
    iTrack.iClockConverter = clockconv;
    iTrack.iTrackDataMemoryPool = trackdatamempool;
    iTrack.iTrackDataMemoryPoolProxy = trackdatamempoolproxy;
    iTrack.iMediaDataImplAlloc = mediadataimplalloc;
    iTrack.iMediaDataMemPool = mediadatamempool;
    iTrack.iNode = OSCL_STATIC_CAST(PVMFNodeInterfaceImpl*, this);
    aPort = iOutPort;
    if (aacinfo.iFormat == EAACADTS)
    {
        iTrack.oADTS = true;
    }

    PVMFResizableSimpleMediaMsgAlloc* resizableSimpleMediaDataImplAlloc = NULL;
    OsclExclusivePtr<PVMFResizableSimpleMediaMsgAlloc> resizableSimpleMediaDataImplAllocAutoPtr;
    resizableSimpleMediaDataImplAlloc = PVMF_BASE_NODE_NEW(PVMFResizableSimpleMediaMsgAlloc,
                                        (iTrack.iTrackDataMemoryPool));

    iTrack.iResizableSimpleMediaMsgAlloc = resizableSimpleMediaDataImplAlloc;

    RetrieveTrackConfigInfo(iTrack);

    return PVMFSuccess;
}

void PVMFAACFFParserNode::ResetAllTracks()
{
    iTrack.iMediaData.Unbind();
    iTrack.iSeqNum = 0;
    iFirstFrame = false;
    iTrack.oEOSReached = false;
    iTrack.oEOSSent = false;
    iTrack.oProcessOutgoingMessages = true;
    iTrack.oQueueOutgoingMessages = true;
}

bool PVMFAACFFParserNode::ReleaseTrack()
{
    if (iOutPort)
    {
        iOutPort->Disconnect();
        PVMF_BASE_NODE_DELETE(((PVMFAACFFParserOutPort*)iOutPort));
        iOutPort = NULL;
    }

    iTrack.iMediaData.Unbind();

    if (iTrack.iClockConverter)
    {
        PVMF_BASE_NODE_DELETE(iTrack.iClockConverter);
        iTrack.iClockConverter = NULL;
    }

    if (iTrack.iTrackDataMemoryPool)
    {
        iTrack.iTrackDataMemoryPool->removeRef();
        iTrack.iTrackDataMemoryPool = NULL;
    }
    if (iTrack.iTrackDataMemoryPoolProxy)
    {
        PVMF_BASE_NODE_DELETE(iTrack.iTrackDataMemoryPoolProxy);

        iTrack.iTrackDataMemoryPoolProxy = NULL;
    }
    if (iTrack.iMediaDataImplAlloc)
    {
        PVMF_BASE_NODE_DELETE(iTrack.iMediaDataImplAlloc);
        iTrack.iMediaDataImplAlloc = NULL;
    }

    if (iTrack.iMediaDataMemPool != NULL)
    {
        iTrack.iMediaDataMemPool->CancelFreeChunkAvailableCallback();
        iTrack.iMediaDataMemPool->removeRef();
        iTrack.iMediaDataMemPool = NULL;
    }
    if (iTrack.iResizableSimpleMediaMsgAlloc)
    {
        PVMF_BASE_NODE_DELETE(iTrack.iResizableSimpleMediaMsgAlloc);
        iTrack.iResizableSimpleMediaMsgAlloc = NULL;
    }

    return true;
}

void PVMFAACFFParserNode::CleanupFileSource()
{
    iAvailableMetadataKeys.clear();

    if (iAACParser)
    {
        PVMF_BASE_NODE_DELETE(iAACParser);
        iAACParser = NULL;
    }

    iAACParserNodeMetadataValueCount = 0;
    iUseCPMPluginRegistry = false;
    iCPMSourceData.iFileHandle = NULL;
    if (iCPMContentAccessFactory != NULL)
    {
        iCPMContentAccessFactory->removeRef();
        iCPMContentAccessFactory = NULL;
    }
    if (iDataStreamFactory != NULL)
    {
        iDataStreamFactory->removeRef();
        iDataStreamFactory = NULL;
    }

    iCPMContentType = PVMF_CPM_CONTENT_FORMAT_UNKNOWN;
    iPreviewMode = false;
    oSourceIsCurrent = false;
    if (iFileHandle)
    {
        OSCL_DELETE(iFileHandle);
        iFileHandle = NULL;
    }

}

void PVMFAACFFParserNode::HandlePortActivity(const PVMFPortActivity &aActivity)
{

    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACFFParserNode::PortActivity: port=0x%x, type=%d",
                                      aActivity.iPort, aActivity.iType));

    if (aActivity.iPort != iOutPort)
    {
        return;
    }
    /*
     * A port is reporting some activity or state change.  This code
     * figures out whether we need to queue a processing event
     * for the AO, and/or report a node event to the observer.
     */
    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_CREATED:
            /*
             * Report port created info event to the node.
             */
            ReportInfoEvent(PVMFInfoPortCreated, (OsclAny*)aActivity.iPort);
            break;

        case PVMF_PORT_ACTIVITY_DELETED:
        {
            /*
             * Report port deleted info event to the node.
             */
            ReportInfoEvent(PVMFInfoPortDeleted, (OsclAny*)aActivity.iPort);
        }
        break;

        case PVMF_PORT_ACTIVITY_CONNECT:
            break;

        case PVMF_PORT_ACTIVITY_DISCONNECT:
            break;

        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
        {
            // An outgoing message was queued on this port.
            //Reschedule the node to process the same.
            Reschedule();
        }
        break;

        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
        {
            /*
             * There are no input ports to this node
             */
            OSCL_ASSERT(false);
        }
        break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_BUSY:
        {
            /*
             * This implies that this output port cannot accept any more
             * msgs on its outgoing queue. This implies that we should stop
             * retrieving samples from file to queue.
             */
            PVAACFFNodeTrackPortInfo* trackInfoPtr = &iTrack;
            /*
            if (!GetTrackInfo(aActivity.iPort, trackInfoPtr))
            {
                ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aActivity.iPort));
                PVMF_AACPARSERNODE_LOGERROR((0, "0x%x PVMFAACFFParserNode::HandlePortActivity: Error - GetTrackInfo failed", this));
                return;
            }*/
            trackInfoPtr->oQueueOutgoingMessages = false;
        }
        break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_READY:
        {
            /*
             * Outgoing queue was previously busy, but is now ready.
             * We are ready to send new samples from file.
             */
            PVAACFFNodeTrackPortInfo* trackInfoPtr = &iTrack;
            /*
            if (!GetTrackInfo(aActivity.iPort, trackInfoPtr))
            {
                ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aActivity.iPort));
                PVMF_AACPARSERNODE_LOGERROR((0, "0x%x PVMFAACFFParserNode::HandlePortActivity: Error - GetTrackInfo failed", this));
                return;
            }*/
            trackInfoPtr->oQueueOutgoingMessages = true;
            Reschedule();
        }
        break;

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_BUSY:
        {
            /*
             * The connected port has become busy (its incoming queue is
             * busy). There is nothing to do but to wait for connected port
             * ready.
             */
            PVAACFFNodeTrackPortInfo* trackInfoPtr = &iTrack;
            /*
            if (!GetTrackInfo(aActivity.iPort, trackInfoPtr))
            {
                ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aActivity.iPort));
                PVMF_AACPARSERNODE_LOGERROR((0, "0x%x PVMFAACFFParserNode::HandlePortActivity: Error - GetTrackInfo failed", this));
                return;
            }*/
            trackInfoPtr->oProcessOutgoingMessages = false;
        }
        break;

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY:
        {
            /*
             * The connected port has transitioned from Busy to Ready.
             * It's time to start sending messages again.
             */
            PVAACFFNodeTrackPortInfo* trackInfoPtr = &iTrack;
            /*
            if (!GetTrackInfo(aActivity.iPort, trackInfoPtr))
            {
                ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aActivity.iPort));
                PVMF_AACPARSERNODE_LOGERROR((0, "0x%x PVMFAACFFParserNode::HandlePortActivity: Error - GetTrackInfo failed", this));
                return;
            }*/
            trackInfoPtr->oProcessOutgoingMessages = true;
            Reschedule();
        }
        break;


        default:
            break;
    }
}

PVMFStatus PVMFAACFFParserNode::ProcessOutgoingMsg(PVAACFFNodeTrackPortInfo* aTrackInfoPtr)
{
    //Called by the AO to process one message off the outgoing
    //message queue for the given port.  This routine will
    //try to send the data to the connected port.


    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::ProcessOutgoingMsg() Called aPort=0x%x", aTrackInfoPtr->iPort));
    PVMFStatus status = aTrackInfoPtr->iPort->Send();
    if (status == PVMFErrBusy)
    {
        // Connected port is busy
        aTrackInfoPtr->oProcessOutgoingMessages = false;
        PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::ProcessOutgoingMsg() Connected port is in busy state"));
    }
    else if (status != PVMFSuccess)
    {
        PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::ProcessOutgoingMsg() - aTrackInfoPtr->iPort->Send() Failed"));
    }
    return status;
}

PVMFStatus PVMFAACFFParserNode::DoReleasePort()
{
    //This node supports release port from any state

    //Find the port in the port vector
    PVMFStatus status = PVMFErrNotSupported;
    PVMFPortInterface* ptr = NULL;
    iCurrentCommand.PVMFNodeCommandBase::Parse(ptr);

    PVMFAACFFParserOutPort* port = (PVMFAACFFParserOutPort*)ptr;

    if (iOutPort == port)
    {
        iTrack.iMediaData.Unbind();
        OSCL_DELETE(((PVMFAACFFParserOutPort*)iTrack.iPort));
        iTrack.iPort = NULL;
        iOutPort = NULL;

        ReleaseTrack();
        status = PVMFSuccess;
    }

    return status;
}

PVMFStatus PVMFAACFFParserNode::DoQueryInterface()
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::DoQueryInterface() Called"));

    PVMFStatus status = PVMFSuccess;
    PVUuid* uuid;
    PVInterface** ptr;
    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, ptr);

    if (queryInterface(*uuid, *ptr))
    {
        // PVMFCPMPluginLicenseInterface is not a part of this node
        if (*uuid != PVMFCPMPluginLicenseInterfaceUuid)
        {
            (*ptr)->addRef();
        }
    }
    else
    {
        //not supported
        *ptr = NULL;
        status = PVMFErrNotSupported;
    }
    return status;
}

void PVMFAACFFParserNode::addRef()
{
    ++iExtensionRefCount;
}

void PVMFAACFFParserNode::removeRef()
{
    --iExtensionRefCount;
}

PVMFStatus PVMFAACFFParserNode::QueryInterfaceSync(PVMFSessionId aSession,
        const PVUuid& aUuid,
        PVInterface*& aInterfacePtr)
{
    OSCL_UNUSED_ARG(aSession);
    aInterfacePtr = NULL;
    if (queryInterface(aUuid, aInterfacePtr))
    {
        // PVMFCPMPluginLicenseInterface is not a part of this node
        if (aUuid != PVMFCPMPluginLicenseInterfaceUuid)
        {
            aInterfacePtr->addRef();
        }
        return PVMFSuccess;
    }
    return PVMFErrNotSupported;
}

bool PVMFAACFFParserNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::queryInterface called"));
    if (uuid == PVMF_DATA_SOURCE_INIT_INTERFACE_UUID)
    {
        PVMFDataSourceInitializationExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFDataSourceInitializationExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMF_TRACK_SELECTION_INTERFACE_UUID)
    {
        PVMFTrackSelectionExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFTrackSelectionExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == KPVMFMetadataExtensionUuid)
    {
        PVMFMetadataExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PvmfDataSourcePlaybackControlUuid)
    {
        PvmfDataSourcePlaybackControlInterface* myInterface = OSCL_STATIC_CAST(PvmfDataSourcePlaybackControlInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMIDatastreamuserInterfaceUuid)
    {
        PVMIDatastreamuserInterface* myInterface = OSCL_STATIC_CAST(PVMIDatastreamuserInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMF_FF_PROGDOWNLOAD_SUPPORT_INTERFACE_UUID)
    {
        PVMFFormatProgDownloadSupportInterface* myInterface = OSCL_STATIC_CAST(PVMFFormatProgDownloadSupportInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMFCPMPluginLicenseInterfaceUuid)
    {
        iface = OSCL_STATIC_CAST(PVInterface*, iCPMLicenseInterface);
    }
    else
    {
        return false;
    }
    return true;
}

PVMFStatus PVMFAACFFParserNode::SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData, uint32 aClipIndex, PVMFFormatTypeDRMInfo aType)
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::SetSourceInitializationData called"));

    if (aClipIndex != 0)
        return PVMFErrArgument; //playlist not supported.

    if ((aSourceFormat != PVMF_MIME_AACFF) && (aSourceFormat != PVMF_MIME_RAWAAC))
    {
        PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::SetSourceInitializationData - Unsupported Format"));
        return PVMFFailure;
    }

    // Clean up any previous sources
    CleanupFileSource();
    iSourceFormat = aSourceFormat;
    iSourceURL = aSourceURL;

    if (aSourceData)
    {

        // Old context object? query for local datasource availability
        PVInterface* pvInterface =
            OSCL_STATIC_CAST(PVInterface*, aSourceData);

        PVInterface* localDataSrc = NULL;
        PVUuid localDataSrcUuid(PVMF_LOCAL_DATASOURCE_UUID);

        if (pvInterface->queryInterface(localDataSrcUuid, localDataSrc))
        {
            PVMFLocalDataSource* context =
                OSCL_STATIC_CAST(PVMFLocalDataSource*, localDataSrc);

            iPreviewMode = context->iPreviewMode;
            if (context->iFileHandle)
            {

                iFileHandle = PVMF_BASE_NODE_NEW(OsclFileHandle, (*(context->iFileHandle)));

                iCPMSourceData.iFileHandle = iFileHandle;
            }
            iCPMSourceData.iPreviewMode = iPreviewMode;
            iCPMSourceData.iIntent = context->iIntent;

        }
        else
        {
            // New context object
            PVInterface* sourceDataContext = NULL;
            PVInterface* commonDataContext = NULL;
            PVUuid sourceContextUuid(PVMF_SOURCE_CONTEXT_DATA_UUID);
            PVUuid commonContextUuid(PVMF_SOURCE_CONTEXT_DATA_COMMON_UUID);
            if (pvInterface->queryInterface(sourceContextUuid, sourceDataContext) &&
                    sourceDataContext->queryInterface(commonContextUuid, commonDataContext))
            {
                PVMFSourceContextDataCommon* context =
                    OSCL_STATIC_CAST(PVMFSourceContextDataCommon*, commonDataContext);

                iPreviewMode = context->iPreviewMode;
                if (context->iFileHandle)
                {

                    iFileHandle = PVMF_BASE_NODE_NEW(OsclFileHandle, (*(context->iFileHandle)));

                    iCPMSourceData.iFileHandle = iFileHandle;
                }
                iCPMSourceData.iPreviewMode = iPreviewMode;
                iCPMSourceData.iIntent = context->iIntent;
            }
        }

        if (aType != PVMF_FORMAT_TYPE_CONNECT_UNPROTECTED)
        {
            /*
            * create a CPM object here...
            */
            PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::SetSourceInitializationData create CPM obj"));
            iUseCPMPluginRegistry = true;
            //cleanup any prior instance
            if (iCPM)
            {
                iCPM->ThreadLogoff();
                PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
                iCPM = NULL;
            }
            iCPM = PVMFCPMFactory::CreateContentPolicyManager(*this);
            //thread logon may leave if there are no plugins
            int32 err;
            OSCL_TRY(err, iCPM->ThreadLogon(););
            OSCL_FIRST_CATCH_ANY(err,
                                 iCPM->ThreadLogoff();
                                 PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
                                 iCPM = NULL;
                                 iUseCPMPluginRegistry = false;
                                );
        }
        else
        {
            //skip CPM if we for sure the content is unprotected
            PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::SetSourceInitializationData non-create CPM obj"));
            iUseCPMPluginRegistry = false;
        }
        return PVMFSuccess;
    }
    return PVMFSuccess;
}

PVMFStatus PVMFAACFFParserNode::SetClientPlayBackClock(PVMFMediaClock* aClientClock)
{
    OSCL_UNUSED_ARG(aClientClock);
    return PVMFSuccess;
}

PVMFStatus PVMFAACFFParserNode::SetEstimatedServerClock(PVMFMediaClock* aClientClock)
{
    OSCL_UNUSED_ARG(aClientClock);
    return PVMFSuccess;
}

void PVMFAACFFParserNode::AudioSinkEvent(PVMFStatus aEvent, uint32 aStreamId)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_UNUSED_ARG(aStreamId);
    //ignore
}

//From PVMFTrackSelectionExtensionInterface
PVMFStatus PVMFAACFFParserNode::GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAACFFParserNode::GetMediaPresentationInfo() called"));

    // Check to make sure the AAC file has been parsed
    if (!iAACParser)  return PVMFFailure;

    aInfo.setDurationValue(iAACFileInfo.iDuration);
    // Current version of AAC parser is limited to 1 channel
    PVMFTrackInfo tmpTrackInfo;

    // set the port tag for this track
    tmpTrackInfo.setPortTag(PVMF_AAC_PARSER_NODE_PORT_TYPE_SOURCE);

    // track id
    tmpTrackInfo.setTrackID(0);

    TPVAacFileInfo aacinfo;
    if (!iAACParser->RetrieveFileInfo(aacinfo)) return PVMFErrNotSupported;

    // bitrate
    tmpTrackInfo.setTrackBitRate(aacinfo.iBitrate);

    // timescale
    tmpTrackInfo.setTrackDurationTimeScale((uint64)aacinfo.iTimescale);

    // config info

    // mime type
    OSCL_FastString mime_type;
    switch (aacinfo.iFormat)
    {
        case EAACADIF:
        case EAACRaw:
            mime_type = PVMF_MIME_ADIF;
            break;

        case EAACADTS:
            mime_type = PVMF_MIME_MPEG4_AUDIO;
            break;
        default:
            mime_type = "";
            break;
    }

    tmpTrackInfo.setTrackMimeType(mime_type);

    OsclRefCounterMemFrag config;
    PVAACFFNodeTrackPortInfo trackPortInfo;
    if (!RetrieveTrackConfigInfo(trackPortInfo))
    {
        return PVMFFailure;
    }
    config = trackPortInfo.iFormatSpecificConfig;

    tmpTrackInfo.setTrackConfigInfo(config);

    // add the track
    aInfo.addTrackInfo(tmpTrackInfo);

    return PVMFSuccess;
}

PVMFStatus PVMFAACFFParserNode::SelectTracks(PVMFMediaPresentationInfo& aInfo)
{
    OSCL_UNUSED_ARG(aInfo);
    return PVMFSuccess;
}

void PVMFAACFFParserNode::ReleaseMetadataValue(PvmiKvp& aValueKVP)
{
    switch (GetValTypeFromKeyString(aValueKVP.key))
    {
        case PVMI_KVPVALTYPE_CHARPTR:
            if (aValueKVP.value.pChar_value != NULL)
            {
                OSCL_ARRAY_DELETE(aValueKVP.value.pChar_value);
                aValueKVP.value.pChar_value = NULL;
            }
            break;

        case PVMI_KVPVALTYPE_WCHARPTR:
            if (aValueKVP.value.pWChar_value != NULL)
            {
                OSCL_ARRAY_DELETE(aValueKVP.value.pWChar_value);
                aValueKVP.value.pWChar_value = NULL;
            }
            break;

        default:
            // Add more case statements if other value types are returned
            break;
    }

    OSCL_ARRAY_DELETE(aValueKVP.key);
    aValueKVP.key = NULL;


}

PVMFStatus PVMFAACFFParserNode::DoGetMetadataValues()
{
    int32 leavecode = 0;
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::DoGetMetaDataValues Called"));

    // Check to make sure the AAC file has been parsed
    if (!iAACParser)  return PVMFFailure;

    PVMFMetadataList* keylistptr_in = NULL;
    PVMFMetadataList* keylistptr = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    iCurrentCommand.PVMFNodeCommand::Parse(keylistptr_in, valuelistptr, starting_index, max_entries);

    // Check the parameters
    if (keylistptr_in == NULL || valuelistptr == NULL)
    {
        return PVMFErrArgument;
    }

    keylistptr = keylistptr_in;
    //If numkeys is one, just check to see if the request
    //is for ALL metadata
    if (keylistptr_in->size() == 1)
    {
        if (oscl_strncmp((*keylistptr)[0].get_cstr(),
                         PVAAC_ALL_METADATA_KEY,
                         oscl_strlen(PVAAC_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &iAvailableMetadataKeys;
        }
    }
    uint32 numkeys = keylistptr->size();

    if (starting_index > (numkeys - 1) || numkeys <= 0 || max_entries == 0)
    {
        // Don't do anything
        return PVMFErrArgument;
    }

    uint32 numvalentries = 0;
    int32 numentriesadded = 0;
    uint32 lcv = 0;
    PvmiKvpSharedPtrVector iFrame;

    //add ID3 Data
    for (lcv = 0; lcv < numkeys; lcv++)
    {
        iAACParser->GetID3Frame((*keylistptr)[lcv], iFrame);
        if (iFrame.size() > 0)
        {

            PvmiKvp KeyVal;
            KeyVal.key = NULL;
            KeyVal.length = 0;
            char *key = (*(iFrame.back())).key;
            int32 len = (*(iFrame.back())).length;

            ++numvalentries;

            int32 key_len = oscl_strlen(key) + 1;

            leavecode = CreateNewArray(KeyVal.key, key_len);
            oscl_strncpy(KeyVal.key , key, key_len);
            KeyVal.length = len;
            KeyVal.capacity = (*(iFrame.back())).capacity;

            PvmiKvpValueType ValueType = GetValTypeFromKeyString(key);

            switch (ValueType)
            {

                case PVMI_KVPVALTYPE_WCHARPTR:
                    leavecode = CreateNewArray(KeyVal.value.pWChar_value, len);
                    oscl_strncpy(KeyVal.value.pWChar_value , (*(iFrame.back())).value.pWChar_value, len);
                    break;

                case PVMI_KVPVALTYPE_CHARPTR:
                    leavecode = CreateNewArray(KeyVal.value.pChar_value, len);

                    oscl_strncpy(KeyVal.value.pChar_value , (*(iFrame.back())).value.pChar_value, len);
                    break;

                case PVMI_KVPVALTYPE_UINT32:
                    KeyVal.value.uint32_value = (*(iFrame.back())).value.uint32_value;
                    break;

                default:
                    break;
            }

            leavecode = PushKVPToList(valuelistptr, KeyVal);
            OSCL_FIRST_CATCH_ANY(leavecode, ReleaseMetadataValue(KeyVal);
                                 break;);

            ++numentriesadded;

            iFrame.pop_back();
        }
    }

    //add rest of the data
    for (lcv = 0; lcv < numkeys; lcv++)
    {
        int32 leavecode = 0;
        PvmiKvp KeyVal;
        KeyVal.key = NULL;

        if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_DURATION_KEY))
        {
            // Duration
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 duration = (uint32)iAACFileInfo.iDuration;
                PVMFStatus retval = PVMFSuccess;
                if (duration > 0)
                {
                    retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(
                                 KeyVal,
                                 PVAACMETADATA_DURATION_KEY,
                                 duration,
                                 (char *)PVAACMETADATA_TIMESCALE1000);
                }
                else
                {
                    retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(
                                 KeyVal,
                                 PVAACMETADATA_DURATION_KEY,
                                 PVAACMETADATA_UNKNOWN);
                }

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

            }

        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_CHANNELS_KEY))
        {
            // Channels
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 channels = (uint32)iAACFileInfo.iChannels;
                PVMFStatus retval = PVMFSuccess;
                if (channels > 0)
                {
                    retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(
                                 KeyVal,
                                 PVAACMETADATA_CHANNELS_KEY,
                                 channels,
                                 NULL);
                }
                else
                {
                    retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(
                                 KeyVal,
                                 PVAACMETADATA_CHANNELS_KEY,
                                 PVAACMETADATA_UNKNOWN);
                }

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

            }

        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_PROFILE_KEY))
        {
            // Profile
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 profile = (uint32)iAACFileInfo.iProfile;
                PVMFStatus retval = PVMFSuccess;
                if (profile > 0)
                {
                    retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(
                                 KeyVal,
                                 PVAACMETADATA_PROFILE_KEY,
                                 profile,
                                 NULL);
                }
                else
                {
                    retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(
                                 KeyVal,
                                 PVAACMETADATA_PROFILE_KEY,
                                 PVAACMETADATA_UNKNOWN);
                }

                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

            }

        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(),  PVAACMETADATA_RANDOM_ACCESS_DENIED_KEY))
        {
            /*
             * Random Access
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > (uint32)starting_index)
            {
                bool random_access_denied = false;
                if (iAACFileInfo.iFormat == EAACADIF ||
                        iAACFileInfo.iFormat == EAACRaw ||
                        iAACFileInfo.iDuration <= 0)
                {
                    random_access_denied = true;
                }
                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                            PVAACMETADATA_RANDOM_ACCESS_DENIED_KEY,
                            random_access_denied,
                            NULL);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (!oscl_strcmp((*keylistptr)[lcv].get_cstr(),  PVAACMETADATA_NUMTRACKS_KEY))
        {
            // Number of tracks
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 numtracksuint32 = 1;
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                                    PVAACMETADATA_NUMTRACKS_KEY,
                                    numtracksuint32,
                                    NULL);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_TRACKINFO_BITRATE_KEY) == 0) &&
                 iAACFileInfo.iBitrate > 0)
        {
            // Bitrate
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 bitRate = (uint32)iAACFileInfo.iBitrate;
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                                    PVAACMETADATA_TRACKINFO_BITRATE_KEY,
                                    bitRate,
                                    NULL);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_TRACKINFO_SELECTED_KEY) == 0))
        {
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                bool trackselected = 1;
                char indexParam[16];
                oscl_snprintf(indexParam, 16, ";%s", PVAACMETADATA_INDEX0);
                indexParam[15] = '\0';
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                                    PVAACMETADATA_TRACKINFO_SELECTED_KEY,
                                    trackselected,
                                    indexParam);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_TRACKINFO_SAMPLERATE_KEY) == 0) &&
                 iAACFileInfo.iSampleFrequency > 0)
        {
            // Sampling rate
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 sampleFreq = (uint32)iAACFileInfo.iSampleFrequency;
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                                    PVAACMETADATA_TRACKINFO_BITRATE_KEY,
                                    sampleFreq,
                                    NULL);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAACMETADATA_TRACKINFO_AUDIO_FORMAT_KEY) == 0) &&
                 iAACFileInfo.iFormat != EAACUnrecognized)
        {
            // Format
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                PVMFStatus retval = PVMFSuccess;
                switch (iAACFileInfo.iFormat)
                {
                    case EAACADTS:
                        retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                                 PVAACMETADATA_TRACKINFO_AUDIO_FORMAT_KEY,
                                 _STRLIT_CHAR(PVMF_MIME_ADTS));
                        break;

                    case EAACADIF:
                        retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                                 PVAACMETADATA_TRACKINFO_AUDIO_FORMAT_KEY,
                                 _STRLIT_CHAR(PVMF_MIME_ADIF));

                        break;

                    case EAACRaw:
                        retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                                 PVAACMETADATA_TRACKINFO_AUDIO_FORMAT_KEY,
                                 _STRLIT_CHAR(PVMF_MIME_MPEG4_AUDIO));

                        break;

                    default:
                        break;
                }
            }
            else
            {
                // Memory allocation failed so clean up
                if (KeyVal.key)
                {
                    OSCL_ARRAY_DELETE(KeyVal.key);
                    KeyVal.key = NULL;
                }
                if (KeyVal.value.pChar_value)
                {
                    OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                }
                break;
            }
        }

        // Add the KVP to the list if the key string was created
        if (KeyVal.key != NULL)
        {
            leavecode = PushKVPToList(valuelistptr, KeyVal);
            OSCL_FIRST_CATCH_ANY(leavecode, ReleaseMetadataValue(KeyVal);
                                 break;);

            // Increment the counter for number of value entries added to the list
            ++numentriesadded;

            // Check if the max number of value entries were added
            if (max_entries > 0 && numentriesadded >= max_entries)
            {
                // Maximum number of values added so break out of the loop
                break;
            }
        }
    }

    iAACParserNodeMetadataValueCount = (*valuelistptr).size();

    if ((iCPMMetaDataExtensionInterface != NULL))

    {
        iCPMGetMetaDataValuesCmdId =
            iCPMMetaDataExtensionInterface->GetNodeMetadataValues(iCPMSessionID,
                    (*keylistptr_in),
                    (*valuelistptr),
                    0);
        return PVMFPending;
    }

    return PVMFSuccess;
}

PVMFStatus PVMFAACFFParserNode::DoSetDataSourcePosition()
{
    // Check to make sure the AAC file has been parsed and port exists
    if ((!iAACParser) || (!iOutPort))  return PVMFFailure;

    uint32 targetNPT = 0;
    uint32* actualNPT = NULL;
    uint32* actualMediaDataTS = NULL;
    bool jumpToIFrame = false;
    uint32 streamID = 0;

    ResetAllTracks();
    iFirstFrame = true;
    iCurrentCommand.PVMFNodeCommand::Parse(targetNPT, actualNPT, actualMediaDataTS, jumpToIFrame, streamID);

    // validate the parameters
    if (actualNPT == NULL || actualMediaDataTS == NULL)
    {
        return PVMFErrArgument;
    }

    //store to send the BOS
    iTrack.iSendBOS = true;
    //save the stream id for next media segment
    iStreamID = streamID;

    *actualNPT = 0;
    *actualMediaDataTS = 0;

    int32 result;

    // check if passed targetNPT is greater than or equal to clip duration.
    if (iAACFileInfo.iDuration > 0 && (targetNPT >= (uint32)iAACFileInfo.iDuration) && (iAACFileInfo.iFormat != EAACRaw))
    {
        if (iAACFileInfo.iFormat == EAACADIF)
        {
            uint32 tmpuint32 = 0;
            result = iAACParser->ResetPlayback(0, tmpuint32);
            if (result != AACBitstreamObject::EVERYTHING_OK)
            {
                return PVMFErrResource;
            }

            // Adjust the offset so the timestamp after reposition would begin past the current position for sure
            iTrack.iTimestampOffset += iAACFileInfo.iDuration;

            // Reset the clock to time 0
            iTrack.iClockConverter->set_clock(OSCL_STATIC_CAST(uint32, 0), 0);

            // Set the return parameters
            *actualNPT = iAACFileInfo.iDuration;
            *actualMediaDataTS = iTrack.iTimestampOffset;
            iTrack.oEOSReached = true;
        }
        else
        {
            // Peek the next sample to get the duration of the last sample
            uint32 timestamp;
            result = iAACParser->PeekNextTimestamp(timestamp);
            if (result != AACBitstreamObject::EVERYTHING_OK)
            {
                return PVMFErrResource;
            }

            uint32 millisecTS = iTrack.iClockConverter->get_converted_ts(1000);
            if (iInterfaceState != EPVMFNodePrepared)
                millisecTS += PVMF_AAC_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS;
            *actualMediaDataTS = millisecTS;

            // Reset the track to begining of clip.
            uint32 tmpuint32 = 0;
            result = iAACParser->ResetPlayback(0, tmpuint32);
            if (result != AACBitstreamObject::EVERYTHING_OK)
            {
                return PVMFErrResource;
            }

            //Peek new position to get the actual new timestamp
            uint32 newtimestamp;
            result = iAACParser->PeekNextTimestamp(newtimestamp);
            if (result != AACBitstreamObject::EVERYTHING_OK)
            {
                return PVMFErrResource;
            }
            *actualNPT = iAACFileInfo.iDuration;

            //Adjust the offset to add to future timestamps.
            iTrack.iTimestampOffset += (timestamp - newtimestamp);

            MediaClockConverter mcc(1000);
            uint32 delta = PVMF_AAC_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS;
            uint32 wrap_count = 0;
            mcc.set_clock(delta, wrap_count);
            uint32 deltaintimescale = mcc.get_converted_ts(iTrack.iClockConverter->get_timescale());
            iTrack.iTimestampOffset += deltaintimescale;

            iTrack.oEOSReached = true;
        }
        return PVMFSuccess;
    }

    if (iAACFileInfo.iFormat == EAACADIF)
    {
        // ADIF has no timestamp and repositioning is limited to beginning of file
        // Reposition to time 0
        uint32 tmpuint32 = 0;
        result = iAACParser->ResetPlayback(0, tmpuint32);
        if (result != AACBitstreamObject::EVERYTHING_OK)
        {
            return PVMFErrResource;
        }

        // Adjust the offset so the timestamp after reposition would begin past the current position for sure
        iTrack.iTimestampOffset += iAACFileInfo.iDuration;

        // Reset the clock to time 0
        iTrack.iClockConverter->set_clock(OSCL_STATIC_CAST(uint32, 0), 0);

        // Set the return parameters
        *actualNPT = 0;
        *actualMediaDataTS = iTrack.iTimestampOffset;

        PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::DoSetDataSourcePosition - ADIF -targetNPT=%d, actualNPT=%d, actualMediaDataTS=%d",
                                           targetNPT, *actualNPT, *actualMediaDataTS));

    }
    else if (iAACFileInfo.iFormat == EAACRaw)
    {
        // Raw AAC has no timestamp and repositioning is limited to beginning of file
        // Reposition to time 0
        uint32 tmpuint32 = 0;
        result = iAACParser->ResetPlayback(0, tmpuint32);
        if (result != AACBitstreamObject::EVERYTHING_OK)
        {
            return PVMFErrResource;
        }

        // Adjust the offset so the timestamp after reposition would begin past the current position for sure
        // Since duration of raw AAC bitstream file is unknown use a hardcoded value of 1 hour
        iTrack.iTimestampOffset += 3600000;

        // Reset the clock to time 0
        iTrack.iClockConverter->set_clock(OSCL_STATIC_CAST(uint32, 0), 0);

        // Set the return parameters
        *actualNPT = 0;
        *actualMediaDataTS = iTrack.iTimestampOffset;

        PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::DoSetDataSourcePosition - RAWAAC - targetNPT=%d, actualNPT=%d, actualMediaDataTS=%d",
                                           targetNPT, *actualNPT, *actualMediaDataTS));
    }
    else
    {
        // The media data timestamp of the next sample will start from the maximum
        // of timestamp on all selected tracks.  This media data timestamp will
        // correspond to the actual NPT.

        // Peek the next sample to get the duration of the last sample
        uint32 timestamp;
        result = iAACParser->PeekNextTimestamp(timestamp);
        if (result != AACBitstreamObject::EVERYTHING_OK)
        {
            return PVMFErrResource;
        }

        // Adjust the timestamp to the end of the last sample, i.e. adding the delta to next sample
        //  iTrack.iClockConverter->update_clock(timestamp);

        uint32 millisecTS = iTrack.iClockConverter->get_converted_ts(1000);
        if (iInterfaceState != EPVMFNodePrepared)
            millisecTS += PVMF_AAC_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS;
        *actualMediaDataTS = millisecTS;

        // Reset the clock to this new starting point.
        //  iTrack.iClockConverter->set_clock(timestamp,0);
        // Reposition
        // If new position is past the end of clip, AAC FF should set the position to the last frame
        uint32 tmpuint32 = 0;
        result = iAACParser->ResetPlayback(targetNPT, tmpuint32);
        if (result != AACBitstreamObject::EVERYTHING_OK)
        {
            if (AACBitstreamObject::END_OF_FILE == result ||
                    AACBitstreamObject::INSUFFICIENT_DATA == result)

            {
                uint32 timestamp;
                result = iAACParser->PeekNextTimestamp(timestamp);
                if (result != AACBitstreamObject::EVERYTHING_OK)
                {
                    return PVMFErrResource;
                }

                // Adjust the timestamp to the end of the last sample, i.e. adding the delta to next sample
                // iTrack.iClockConverter->update_clock(timestamp);

                uint32 millisecTS = iTrack.iClockConverter->get_converted_ts(1000);
                *actualMediaDataTS = millisecTS;

                // Reset the track to begining of clip.
                uint32 tmpuint32 = 0;
                result = iAACParser->ResetPlayback(0, tmpuint32);
                if (result != AACBitstreamObject::EVERYTHING_OK)
                {
                    return PVMFErrResource;
                }

                //Peek new position to get the actual new timestamp
                uint32 newtimestamp;
                result = iAACParser->PeekNextTimestamp(newtimestamp);
                if (result != AACBitstreamObject::EVERYTHING_OK)
                {
                    return PVMFErrResource;
                }
                *actualNPT = newtimestamp;

                //Adjust the offset to add to future timestamps.
                iTrack.iTimestampOffset += (timestamp - newtimestamp);

                MediaClockConverter mcc(1000);
                uint32 delta = PVMF_AAC_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS;
                uint32 wrap_count = 0;
                mcc.set_clock(delta, wrap_count);
                uint32 deltaintimescale = mcc.get_converted_ts(iTrack.iClockConverter->get_timescale());
                iTrack.iTimestampOffset += deltaintimescale;

                iTrack.oEOSReached = true;
                return PVMFSuccess;
            }
            else
            {
                return PVMFErrResource;
            }
        }

        //Peek new position to get the actual new timestamp
        uint32 newtimestamp;
        result = iAACParser->PeekNextTimestamp(newtimestamp);
        if (result != AACBitstreamObject::EVERYTHING_OK)
        {
            return PVMFErrResource;
        }
        *actualNPT = newtimestamp;

        //Adjust the offset to add to future timestamps.
        iTrack.iTimestampOffset += (timestamp - newtimestamp);

        MediaClockConverter mcc(1000);
        uint32 delta = PVMF_AAC_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS;
        uint32 wrap_count = 0;
        mcc.set_clock(delta, wrap_count);
        uint32 deltaintimescale = mcc.get_converted_ts(iTrack.iClockConverter->get_timescale());
        iTrack.iTimestampOffset += deltaintimescale;

        PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::DoSetDataSourcePosition - ADTS - targetNPT=%d, actualNPT=%d, actualMediaDataTS=%d",
                                           targetNPT, *actualNPT, *actualMediaDataTS));
    }


    return PVMFSuccess;
}


PVMFStatus PVMFAACFFParserNode::DoQueryDataSourcePosition()
{
    // Check to make sure the AAC file has been parsed and port exists
    if ((!iAACParser) || (!iOutPort))  return PVMFFailure;

    uint32 targetNPT = 0;
    uint32* actualNPT = NULL;
    bool jumpToIFrame = false;

    iCurrentCommand.PVMFNodeCommand::Parse(targetNPT, actualNPT, jumpToIFrame);

    if (actualNPT == NULL)
    {
        return PVMFErrArgument;
    }

    *actualNPT = 0;

    // Query
    // If new position is past the end of clip, AAC FF should set the position to the last frame
    *actualNPT = iAACParser->SeekPointFromTimestamp(targetNPT);

    return PVMFSuccess;
}


PVMFStatus PVMFAACFFParserNode::DoSetDataSourceRate()
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::DoSetDataSourceRate() In"));
    return PVMFSuccess;
}

// CPM related
void PVMFAACFFParserNode::InitCPM()
{
    iCPMInitCmdId = iCPM->Init();
}

void PVMFAACFFParserNode::OpenCPMSession()
{
    iCPMOpenSessionCmdId = iCPM->OpenSession(iCPMSessionID);
}

void PVMFAACFFParserNode::CPMRegisterContent()
{
    iCPMRegisterContentCmdId = iCPM->RegisterContent(iCPMSessionID,
                               iSourceURL,
                               iSourceFormat,
                               (OsclAny*) & iCPMSourceData);
}

void PVMFAACFFParserNode::GetCPMLicenseInterface()
{
    iCPMLicenseInterfacePVI = NULL;
    iCPMGetLicenseInterfaceCmdId =
        iCPM->QueryInterface(iCPMSessionID,
                             PVMFCPMPluginLicenseInterfaceUuid,
                             iCPMLicenseInterfacePVI);
}

bool PVMFAACFFParserNode::GetCPMContentAccessFactory()
{
    PVMFStatus status = iCPM->GetContentAccessFactory(iCPMSessionID,
                        iCPMContentAccessFactory);
    if (status != PVMFSuccess)
    {
        return false;
    }
    return true;
}

bool PVMFAACFFParserNode::GetCPMMetaDataExtensionInterface()
{
    PVInterface* temp = NULL;
    bool retVal =
        iCPM->queryInterface(KPVMFMetadataExtensionUuid, temp);
    iCPMMetaDataExtensionInterface = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, temp);

    return retVal;
}

void PVMFAACFFParserNode::RequestUsage()
{
    PopulateDRMInfo();

    if (iDataStreamReadCapacityObserver != NULL)
    {
        iCPMContentAccessFactory->SetStreamReadCapacityObserver(iDataStreamReadCapacityObserver);
    }

    iCPMRequestUsageId = iCPM->ApproveUsage(iCPMSessionID,
                                            iRequestedUsage,
                                            iApprovedUsage,
                                            iAuthorizationDataKvp,
                                            iUsageID,
                                            iCPMContentAccessFactory);

    // Logic for playing after acquired license
    oSourceIsCurrent = true;
}

void PVMFAACFFParserNode::PopulateDRMInfo()
{
    // Cleanup any old key
    if (iRequestedUsage.key)
    {
        OSCL_ARRAY_DELETE(iRequestedUsage.key);
        iRequestedUsage.key = NULL;
    }

    if (iApprovedUsage.key)
    {
        OSCL_ARRAY_DELETE(iApprovedUsage.key);
        iApprovedUsage.key = NULL;
    }

    if (iAuthorizationDataKvp.key)
    {
        OSCL_ARRAY_DELETE(iAuthorizationDataKvp.key);
        iAuthorizationDataKvp.key = NULL;
    }

    if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
            (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
    {
        int32 UseKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING));
        int32 AuthKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_AUTHORIZATION_DATA_KEY_STRING));
        int32 leavecode = 0;

        OSCL_TRY(leavecode,
                 iRequestedUsage.key = OSCL_ARRAY_NEW(char, UseKeyLen + 1);
                 iApprovedUsage.key = OSCL_ARRAY_NEW(char, UseKeyLen + 1);
                 iAuthorizationDataKvp.key = OSCL_ARRAY_NEW(char, AuthKeyLen + 1);
                );
        if (leavecode || !iRequestedUsage.key || !iApprovedUsage.key || !iAuthorizationDataKvp.key)
        {
            if (iRequestedUsage.key)
            {
                OSCL_ARRAY_DELETE(iRequestedUsage.key);
                iRequestedUsage.key = NULL;
            }
            if (iApprovedUsage.key)
            {
                OSCL_ARRAY_DELETE(iApprovedUsage.key);
                iApprovedUsage.key = NULL;
            }
            if (iAuthorizationDataKvp.key)
            {
                OSCL_ARRAY_DELETE(iAuthorizationDataKvp.key);
                iAuthorizationDataKvp.key = NULL;
            }

            return;
        }

        oscl_strncpy(iRequestedUsage.key,
                     _STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING),
                     UseKeyLen);
        iRequestedUsage.key[UseKeyLen] = 0;
        iRequestedUsage.length = 0;
        iRequestedUsage.capacity = 0;
        if (iPreviewMode)
        {
            iRequestedUsage.value.uint32_value =
                (BITMASK_PVMF_CPM_DRM_INTENT_PREVIEW |
                 BITMASK_PVMF_CPM_DRM_INTENT_PAUSE |
                 BITMASK_PVMF_CPM_DRM_INTENT_SEEK_FORWARD |
                 BITMASK_PVMF_CPM_DRM_INTENT_SEEK_BACK);
        }
        else
        {
            iRequestedUsage.value.uint32_value =
                (BITMASK_PVMF_CPM_DRM_INTENT_PLAY |
                 BITMASK_PVMF_CPM_DRM_INTENT_PAUSE |
                 BITMASK_PVMF_CPM_DRM_INTENT_SEEK_FORWARD |
                 BITMASK_PVMF_CPM_DRM_INTENT_SEEK_BACK);
        }
        oscl_strncpy(iApprovedUsage.key,
                     _STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING),
                     UseKeyLen);
        iApprovedUsage.key[UseKeyLen] = 0;
        iApprovedUsage.length = 0;
        iApprovedUsage.capacity = 0;
        iApprovedUsage.value.uint32_value = 0;

        oscl_strncpy(iAuthorizationDataKvp.key,
                     _STRLIT_CHAR(PVMF_CPM_AUTHORIZATION_DATA_KEY_STRING),
                     AuthKeyLen);
        iAuthorizationDataKvp.key[AuthKeyLen] = 0;
        iAuthorizationDataKvp.length = 0;
        iAuthorizationDataKvp.capacity = 0;
        iAuthorizationDataKvp.value.pUint8_value = NULL;
    }
    else
    {
        //Error
        OSCL_ASSERT(false);
    }
}

void PVMFAACFFParserNode::SendUsageComplete()
{
    iCPMUsageCompleteCmdId = iCPM->UsageComplete(iCPMSessionID, iUsageID);
}

void PVMFAACFFParserNode::CloseCPMSession()
{
    iCPMCloseSessionCmdId = iCPM->CloseSession(iCPMSessionID);
}

void PVMFAACFFParserNode::ResetCPM()
{
    iCPMResetCmdId = iCPM->Reset();
}

PVMFStatus
PVMFAACFFParserNode::CheckCPMCommandCompleteStatus(PVMFCommandId aID,
        PVMFStatus aStatus)
{
    PVMFStatus status = aStatus;

    /*
    * If UsageComplete fails, ignore that error and close CPM session
    * If close CPM session fails ignore it as well
    * Issue Reset to CPM
    * If CPM Reset fails, assert in source node.
    * Complete Reset of source node once CPM reset completes.
    */
    if (aID == iCPMUsageCompleteCmdId)
    {
        if (aStatus != PVMFSuccess)
        {
            status = PVMFSuccess;
        }

    }
    else if (aID == iCPMCloseSessionCmdId)
    {
        if (aStatus != PVMFSuccess)
        {
            status = PVMFSuccess;
        }
    }
    else if (aID == iCPMResetCmdId)
    {
        if (aStatus != PVMFSuccess)
        {
            PVMF_AACPARSERNODE_LOGINFOHI((0, "PVMFAACFFParserNode::CPMCommandCompleted -  CPM Reset Failed"));
            OSCL_ASSERT(false);
        }

    }

    if (aID == iCPMGetLicenseInterfaceCmdId)
    {
        if (aStatus == PVMFErrNotSupported)
        {
            // License Interface is Optional
            status = PVMFSuccess;
        }
    }
    else if (aID == iCPMRegisterContentCmdId)
    {
        if (aStatus == PVMFErrNotSupported)
        {
            // CPM doesnt care about this content
            status = PVMFErrNotSupported;
        }
    }
    else if (aID == iCPMRequestUsageId)
    {
        if (iCPMSourceData.iIntent == BITMASK_PVMF_SOURCE_INTENT_GETMETADATA)
        {
            if ((aStatus == PVMFErrDrmLicenseNotFound) || (aStatus == PVMFErrDrmLicenseExpired))
            {
                /*
                 * If we are doing metadata only then we don't care
                 * if license is not available
                 */
                status = PVMFSuccess;
            }
        }
    }
    return status;
}

void PVMFAACFFParserNode::CPMCommandCompleted(const PVMFCmdResp& aResponse)
{
    PVMFCommandId id = aResponse.GetCmdId();
    PVMFStatus status =
        CheckCPMCommandCompleteStatus(id, aResponse.GetCmdStatus());

    // If the node is waiting for any pending cancel, cancel it now.
    if (IsCommandInProgress(iCancelCommand))
    {
        CommandComplete(iCurrentCommand, PVMFErrCancelled);
        CommandComplete(iCancelCommand, PVMFSuccess);
        return;
    }

    //if CPM comes back as PVMFErrNotSupported then by pass rest of the CPM
    //sequence. Fake success here so that node doesnt treat this as an error
    if (id == iCPMRegisterContentCmdId && status == PVMFErrNotSupported)
    {
        // CPM does not care about this content, so treat it as unprotected
        PVMFStatus status = CheckForAACHeaderAvailability();
        if (status == PVMFSuccess)
        {
            SetState(EPVMFNodeInitialized);
        }

        // End of Node Init sequence.
        OSCL_ASSERT(PVMF_GENERIC_NODE_INIT == iCurrentCommand.iCmd);
        CommandComplete(iCurrentCommand, status);
        return;
    }

    if (status != PVMFSuccess)
    {
        // If any command fails, the sequence fails.
        CommandComplete(iCurrentCommand, aResponse.GetCmdStatus(),
                        aResponse.GetEventExtensionInterface());

    }
    else
    {
        // process the response, and issue the next command in the sequence.
        if (id == iCPMInitCmdId)
        {
            OpenCPMSession();
        }
        else if (id == iCPMOpenSessionCmdId)
        {
            CPMRegisterContent();
        }
        else if (id == iCPMRegisterContentCmdId)
        {
            GetCPMLicenseInterface();
        }
        else if (id == iCPMGetLicenseInterfaceCmdId)
        {
            iCPMLicenseInterface = OSCL_STATIC_CAST(PVMFCPMPluginLicenseInterface*, iCPMLicenseInterfacePVI);
            iCPMLicenseInterfacePVI = NULL;
            GetCPMMetaDataExtensionInterface();
            iCPMContentType = iCPM->GetCPMContentType(iCPMSessionID);
            if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
                    (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
            {
                RequestUsage();
            }
            else
            {
                // CPM does not care about this content, so treat it as unprotected
                PVMFStatus status = CheckForAACHeaderAvailability();
                if (status == PVMFSuccess)
                {
                    SetState(EPVMFNodeInitialized);
                }

                // End of Node Init sequence.
                OSCL_ASSERT(PVMF_GENERIC_NODE_INIT == iCurrentCommand.iCmd);
                CommandComplete(iCurrentCommand, status);
            }
        }
        else if (id == iCPMRequestUsageId)
        {
            // Logic for playing after acquired license
            oSourceIsCurrent = false;
            if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
                    (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
            {
                GetCPMContentAccessFactory();
                PVMFStatus status = CheckForAACHeaderAvailability();
                if (status)
                {
                    // End of Node Init sequence.
                    CompleteInit();
                }
            }
            else
            {
                // Unknown format - should never get here
                OSCL_ASSERT(false);
            }
        }
        else if (id == iCPMUsageCompleteCmdId)
        {
            CloseCPMSession();
        }
        else if (id == iCPMCloseSessionCmdId)
        {
            ResetCPM();
        }
        else if (id == iCPMResetCmdId)
        {
            // End of Node Reset sequence
            OSCL_ASSERT(PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd);
            CompleteReset();
            CommandComplete(iCurrentCommand, PVMFSuccess);
        }
        else if (id == iCPMGetMetaDataValuesCmdId)
        {
            // End of GetNodeMetaDataValues
            CompleteGetMetaDataValues();
        }
        else
        {
            // Unknown cmd - error
            CommandComplete(iCurrentCommand, PVMFFailure);
        }
    }
}

void PVMFAACFFParserNode::PassDatastreamFactory(PVMFDataStreamFactory& aFactory,
        int32 aFactoryTag,
        const PvmfMimeString* aFactoryConfig)
{
    OSCL_UNUSED_ARG(aFactoryTag);
    OSCL_UNUSED_ARG(aFactoryConfig);

    iDataStreamFactory = &aFactory;
    PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
    PVInterface* iFace =
        iDataStreamFactory->CreatePVMFCPMPluginAccessInterface(uuid);
    if (iFace != NULL)
    {
        iDataStreamInterface = OSCL_STATIC_CAST(PVMIDataStreamSyncInterface*, iFace);
        iDataStreamInterface->OpenSession(iDataStreamSessionID, PVDS_READ_ONLY);
    }
}

void PVMFAACFFParserNode::PassDatastreamReadCapacityObserver(PVMFDataStreamReadCapacityObserver* aObserver)
{
    iDataStreamReadCapacityObserver = aObserver;
}

void PVMFAACFFParserNode::CompleteInit()
{
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::CompleteInit() Called"));

    OSCL_ASSERT(PVMF_GENERIC_NODE_INIT == iCurrentCommand.iCmd);

    if (iCPM)
    {
        if (iApprovedUsage.value.uint32_value !=
                iRequestedUsage.value.uint32_value)
        {
            CommandComplete(iCurrentCommand, PVMFErrAccessDenied);
            return;
        }
    }

    CommandComplete(iCurrentCommand, PVMFSuccess);
    return;
}

void PVMFAACFFParserNode::CompleteGetMetaDataValues()
{
    OSCL_ASSERT(PVMF_GENERIC_NODE_GETNODEMETADATAVALUES == iCurrentCommand.iCmd);

    CommandComplete(iCurrentCommand, PVMFSuccess);
}

bool PVMFAACFFParserNode::GetTrackInfo(PVMFPortInterface* aPort,
                                       PVAACFFNodeTrackPortInfo*& aTrackInfoPtr)
{
    if (iTrack.iPort == aPort)
    {
        aTrackInfoPtr = &iTrack;
        return true;
    }
    return false;
}

bool PVMFAACFFParserNode::CheckForPortRescheduling()
{


    PVAACFFNodeTrackPortInfo* trackInfoPtr = &iTrack;

    if ((trackInfoPtr->oProcessOutgoingMessages) ||
            (trackInfoPtr->oQueueOutgoingMessages))
    {
        //Found a port that has outstanding activity and is not busy.
        return true;
    }

    /*
     * No port processing needed - either all port activity queues are empty
     * or the ports are backed up due to flow control.
     */
    return false;
}

bool PVMFAACFFParserNode::ProcessPortActivity(PVAACFFNodeTrackPortInfo* aTrackInfoPtr)
{
    // called by the AO to process a port activity message

    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::ProcessPortActivity() Called"));

    PVMFStatus status;
    if (aTrackInfoPtr->oQueueOutgoingMessages)
    {
        status = QueueMediaSample(aTrackInfoPtr);

        if ((status != PVMFErrBusy) &&
                (status != PVMFSuccess) &&
                (status != PVMFErrInvalidState))
        {
            PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::ProcessPortActivity() QueueMediaSample Failed - Err=%d", status));
            return false;
        }
    }
    if (aTrackInfoPtr->oProcessOutgoingMessages)
    {
        if (aTrackInfoPtr->iPort->OutgoingMsgQueueSize() > 0)
        {
            status = ProcessOutgoingMsg(aTrackInfoPtr);
            /*
             * Report any unexpected failure in port processing...
             * (the InvalidState error happens when port input is suspended,
             * so don't report it.)
             */
            if ((status != PVMFErrBusy) &&
                    (status != PVMFSuccess) &&
                    (status != PVMFErrInvalidState))
            {
                PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::ProcessPortActivity() ProcessOutgoingMsg Failed - Err=%d", status));
                ReportErrorEvent(PVMFErrPortProcessing);
            }
        }
        else
        {
            // Nothing to send - wait for more data
            aTrackInfoPtr->oProcessOutgoingMessages = false;
        }
    }
    return true;
}

PVMFStatus PVMFAACFFParserNode::QueueMediaSample(PVAACFFNodeTrackPortInfo* aTrackInfoPtr)
{
    if (iAutoPaused == true)
    {
        aTrackInfoPtr->oQueueOutgoingMessages = false;
        PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::QueueMediaSample() - Auto Paused"));
        return PVMFErrBusy;
    }
    if (aTrackInfoPtr->iPort->IsOutgoingQueueBusy())
    {
        aTrackInfoPtr->oQueueOutgoingMessages = false;
        PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::QueueMediaSample() Port Outgoing Queue Busy"));
        return PVMFErrBusy;
    }
    if (aTrackInfoPtr->oQueueOutgoingMessages)
    {
        PVMFStatus status = PVMFFailure;
        if (aTrackInfoPtr->iSendBOS == true)
        {
            if (SendBeginOfMediaStreamCommand(aTrackInfoPtr->iPort, iStreamID, aTrackInfoPtr->iTimestampOffset))
            {
                status = PVMFSuccess;
                aTrackInfoPtr->iSendBOS = false;
            }
            return status;
        }
        if ((aTrackInfoPtr->oEOSReached == false) && (aTrackInfoPtr->oEOSSent == false))
        {
            PVMFSharedMediaDataPtr mediaDataOut;
            status = RetrieveMediaSample(&iTrack, mediaDataOut);
            if (status == PVMFErrBusy)
            {
                aTrackInfoPtr->oQueueOutgoingMessages = false;
                return status;
            }
            else if (status == PVMFSuccess)
            {
                if (aTrackInfoPtr->oEOSReached == false)
                {
                    PVMFSharedMediaMsgPtr msgOut;
                    convertToPVMFMediaMsg(msgOut, mediaDataOut);

                    // For logging purposes
                    uint32 markerInfo = mediaDataOut->getMarkerInfo();
                    uint32 noRender = 0;
                    uint32 keyFrameBit = 0;
                    if (markerInfo & PVMF_MEDIA_DATA_MARKER_INFO_NO_RENDER_BIT)
                    {
                        noRender = 1;
                    }
                    if (markerInfo & PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT)
                    {
                        keyFrameBit = 1;
                    }
                    uint32 nptTS32 = 0;
                    nptTS32 = Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iPrevSampleTimeStamp);
                    PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::QueueMediaSample() TrackID=%d, SeqNum=%d, SampleLen=%d, NptTS=%d, SampleTS=%d, NR=%d, KEY=%d, MimeType=%s",
                                                       aTrackInfoPtr->iTrackId,
                                                       msgOut->getSeqNum(),
                                                       mediaDataOut->getFilledSize(),
                                                       nptTS32,
                                                       msgOut->getTimestamp(),
                                                       noRender,
                                                       keyFrameBit,
                                                       aTrackInfoPtr->iTrackMimeType.get_cstr()));

                    status = aTrackInfoPtr->iPort->QueueOutgoingMsg(msgOut);
                    if (status != PVMFSuccess)
                    {
                        PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::QueueMediaSample: Error - QueueOutgoingMsg failed"));
                        ReportErrorEvent(PVMFErrPortProcessing);
                    }
                    // This flag will get reset to false if the connected port is busy
                    aTrackInfoPtr->oProcessOutgoingMessages = true;
                    return status;
                }
            }
            else if (status == PVMFInfoEndOfData)
            {
                if (aTrackInfoPtr->oEOSSent == false && aTrackInfoPtr->oEOSReached == true)
                {
                    aTrackInfoPtr->iContinuousTimeStamp += PVMF_AAC_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS;
                    uint32 ts32 = Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp);

                    if (SendEndOfTrackCommand(aTrackInfoPtr->iPort, iStreamID, ts32, aTrackInfoPtr->iSeqNum + 1))
                    {
                        aTrackInfoPtr->iSeqNum++;
                        aTrackInfoPtr->oEOSSent = true;
                        aTrackInfoPtr->oQueueOutgoingMessages = false;
                        aTrackInfoPtr->oProcessOutgoingMessages = true;
                        return PVMFSuccess;
                    }
                    return PVMFFailure;
                }
                aTrackInfoPtr->oQueueOutgoingMessages = false;
                return PVMFFailure;
            }
            else
            {
                PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::QueueMediaSample() - Sample Retrieval Failed"));
                ReportErrorEvent(PVMFErrCorrupt);
                return PVMFFailure;
            }
        }
        else if (aTrackInfoPtr->oEOSReached == true)
        {
            if (aTrackInfoPtr->oEOSSent == false)
            {
                aTrackInfoPtr->iContinuousTimeStamp += PVMF_AAC_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS;
                uint32 ts32 = Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp);

                if (SendEndOfTrackCommand(aTrackInfoPtr->iPort, iStreamID, ts32, aTrackInfoPtr->iSeqNum + 1))
                {
                    aTrackInfoPtr->iSeqNum++;
                    aTrackInfoPtr->oEOSSent = true;
                    aTrackInfoPtr->oQueueOutgoingMessages = false;
                    aTrackInfoPtr->oProcessOutgoingMessages = true;
                    return PVMFSuccess;
                }
                return PVMFFailure;
            }
            aTrackInfoPtr->oQueueOutgoingMessages = false;
            return PVMFFailure;
        }
    }
    return PVMFSuccess;
}

void PVMFAACFFParserNode::DataStreamCommandCompleted(const PVMFCmdResp& aResponse)
{
    if (aResponse.GetCmdId() == iRequestReadCapacityNotificationID)
    {
        PVMFStatus cmdStatus = aResponse.GetCmdStatus();
        if (cmdStatus == PVMFSuccess)
        {
            PVMFStatus status = CheckForAACHeaderAvailability();
            if (status == PVMFSuccess)
            {
                // End of Node Init sequence.
                CompleteInit();
            }
        }
        else
        {
            PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::DataStreamCommandCompleted() Failed %d", cmdStatus));
            CommandComplete(iCurrentCommand, PVMFErrResource);

        }
    }
    else
    {
        OSCL_ASSERT(false);
    }
}

void PVMFAACFFParserNode::playResumeNotification(bool aDownloadComplete)
{
    iAutoPaused = false;
    PVAACFFNodeTrackPortInfo* trackInfoPtr = &iTrack;
    iDownloadComplete = aDownloadComplete;

    if (trackInfoPtr->oQueueOutgoingMessages == false)
    {
        trackInfoPtr->oQueueOutgoingMessages = true;
    }

    PVMF_AACPARSERNODE_LOGERROR((0, "PVMFAACParserNode::playResumeNotification() - Auto Resume Triggered"));
    PVMF_AACPARSERNODE_LOGDATATRAFFIC((0, "PVMFAACParserNode::playResumeNotification() - Auto Resume Triggered"));
    Reschedule();

}

void PVMFAACFFParserNode::setFileSize(const TOsclFileOffset aFileSize)
{
    iDownloadFileSize = (uint32)aFileSize;
}

void PVMFAACFFParserNode::setDownloadProgressInterface(PVMFDownloadProgressInterface* aInterface)
{
    if (aInterface == NULL)
    {
        OSCL_ASSERT(false);
    }
    iDownloadProgressInterface = aInterface;
}

int32 PVMFAACFFParserNode::convertSizeToTime(TOsclFileOffset aFileSize, uint32& aNPTInMS)
{
    OSCL_UNUSED_ARG(aFileSize);
    OSCL_UNUSED_ARG(aNPTInMS);
    return -1;
}

void PVMFAACFFParserNode::DataStreamInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_LEAVE(OsclErrNotSupported);
}

void PVMFAACFFParserNode::DataStreamErrorEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_LEAVE(OsclErrNotSupported);
}

int32 PVMFAACFFParserNode::CreateNewArray(char*& aPtr, int32 aLen)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode,
             aPtr = OSCL_ARRAY_NEW(char, aLen););
    return leavecode;
}

int32 PVMFAACFFParserNode::CreateNewArray(oscl_wchar*& aPtr, int32 aLen)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode,
             aPtr = OSCL_ARRAY_NEW(oscl_wchar, aLen););
    return leavecode;
}

int32 PVMFAACFFParserNode::PushKVPToList(Oscl_Vector<PvmiKvp, OsclMemAllocator>*& aValueListPtr, PvmiKvp &aKeyVal)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, (*aValueListPtr).push_back(aKeyVal));
    return leavecode;
}

PVMFStatus PVMFAACFFParserNode::PushValueToList(Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> &aRefMetaDataKeys, PVMFMetadataList *&aKeyListPtr, uint32 aLcv)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aKeyListPtr->push_back(aRefMetaDataKeys[aLcv]));
    OSCL_FIRST_CATCH_ANY(leavecode, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFAACFFParserNode::PushValueToList() Memory allocation failure when copying metadata key")); return PVMFErrNoMemory);
    return PVMFSuccess;
}

bool PVMFAACFFParserNode::setProtocolInfo(Oscl_Vector<PvmiKvp*, OsclMemAllocator>& aInfoKvpVec)
{
    OSCL_UNUSED_ARG(aInfoKvpVec);
    return true;
}

PVMFStatus PVMFAACFFParserNode::HandleExtensionAPICommands()
{
    PVMFStatus status = PVMFFailure;
    PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACFFParserNode::HandleExtensionAPICommands - command=%d", iCurrentCommand.iCmd));
    switch (iCurrentCommand.iCmd)
    {

        case PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION:
            status = DoSetDataSourcePosition();
            break;

        case PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION:
            status = DoQueryDataSourcePosition();
            break;

        case PVMF_GENERIC_NODE_SET_DATASOURCE_RATE:
            status = DoSetDataSourceRate();
            break;

        case PVMF_GENERIC_NODE_GETNODEMETADATAVALUES:
            status = DoGetMetadataValues();
            break;

        default:
            //Do an assert as there shouldn't be any unidentified command
            OSCL_ASSERT(false);
            break;
    }

    return status;
}

PVMFStatus PVMFAACFFParserNode::CancelCurrentCommand()
{
    // Cancel DoFlush here and return success.
    if (IsFlushPending())
    {
        CommandComplete(iCurrentCommand, PVMFErrCancelled);
        return PVMFSuccess;
    }

    /* The pending commands DoInit, DoReset, and DoGetMetadataValues
     * would be canceled in CPMCommandCompleted.
     * So return pending for now.
     */
    return PVMFPending;
}

