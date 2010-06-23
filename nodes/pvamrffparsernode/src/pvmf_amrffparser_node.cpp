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
#include "pvmf_amrffparser_node.h"
#include "pvmf_amrffparser_defs.h"
#include "amr_parsernode_tunables.h"
#include "media_clock_converter.h"
#include "pvmf_fileformat_events.h"
#include "pv_mime_string_utils.h"
#include "oscl_snprintf.h"
#include "pvmi_kvp_util.h"
#include "pvmf_amrffparser_events.h"
#include "oscl_exclusive_ptr.h"
#include "pvmf_source_context_data.h"

static const char PVAMR_ALL_METADATA_KEY[] = "all";
static const char PVAMRMETADATA_DURATION_KEY[] = "duration";
static const char PVAMRMETADATA_NUMTRACKS_KEY[] = "num-tracks";
static const char PVAMRMETADATA_TRACKINFO_BITRATE_KEY[] = "track-info/bit-rate";
static const char PVAMRMETADATA_TRACKINFO_SELECTED_KEY[] = "track-info/selected";
static const char PVAMRMETADATA_TRACKINFO_AUDIO_FORMAT_KEY[] = "track-info/audio/format";
static const char PVAMRMETADATA_CHANNEL_KEY[] = "channel";
static const char PVAMRMETADATA_CLIP_TYPE_KEY[] = "clip-type";
static const char PVAMRMETADATA_LOCAL_CLIP_TYPE_KEY[] = "local";
static const char PVAMRMETADATA_RANDOM_ACCESS_DENIED_KEY[] = "random-access-denied";
static const char PVAMRMETADATA_SEMICOLON[] = ";";
static const char PVAMRMETADATA_TIMESCALE[] = "timescale=";
static const char PVAMRMETADATA_INDEX0[] = "index=0";

#define AMR_SAMPLE_DURATION 20



PVMFAMRFFParserNode::PVMFAMRFFParserNode(int32 aPriority) :
        PVMFNodeInterfaceImpl(aPriority, "PVMFAMRFFParserNode"),
        iOutPort(NULL),
        iFileHandle(NULL),
        iAMRParser(NULL)
{
    iDataPathLogger            = NULL;
    iClockLogger               = NULL;
    iDownloadComplete          = false;

    iFileSizeLastConvertedToTime = 0;
    iLastNPTCalcInConvertSizeToTime = 0;

    iUseCPMPluginRegistry      = false;

    iCPM                       = NULL;
    iCPMSessionID              = 0xFFFFFFFF;
    iCPMContentType            = PVMF_CPM_CONTENT_FORMAT_UNKNOWN;
    iCPMContentAccessFactory   = NULL;
    iCPMMetaDataExtensionInterface = NULL;
    iCPMLicenseInterface = NULL;
    iCPMLicenseInterfacePVI = NULL;
    iCPMInitCmdId              = 0;
    iCPMOpenSessionCmdId       = 0;
    iCPMRegisterContentCmdId   = 0;
    iCPMGetLicenseInterfaceCmdId = 0;
    iCPMRequestUsageId         = 0;
    iCPMUsageCompleteCmdId     = 0;
    iCPMCloseSessionCmdId      = 0;
    iCPMResetCmdId             = 0;
    iRequestedUsage.key        = NULL;
    iApprovedUsage.key         = NULL;
    iAuthorizationDataKvp.key  = NULL;
    iCPMGetMetaDataValuesCmdId     = 0;
    iAMRParserNodeMetadataValueCount = 0;

    iDownloadProgressInterface = NULL;
    iDownloadFileSize          = 0;
    iAMRHeaderSize             = AMR_HEADER_SIZE;
    iDataStreamInterface       = NULL;
    iDataStreamFactory         = NULL;
    iDataStreamReadCapacityObserver = NULL;
    iAutoPaused                = false;

    oSourceIsCurrent           = false;

    iUseCPMPluginRegistry = false;

    iCountToCalculateRDATimeInterval = 1;
    int32 err;
    OSCL_TRY(err,
             //Set the node capability data.
             //This node can support an unlimited number of ports.
             iNodeCapability.iCanSupportMultipleInputPorts = false;
             iNodeCapability.iCanSupportMultipleOutputPorts = false;
             iNodeCapability.iHasMaxNumberOfPorts = true;
             iNodeCapability.iMaxNumberOfPorts = 1;
             iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_AMR_IETF);
             iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_AMR_IF2);
             iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_AMRWB_IETF);
            );

    if (err != OsclErrNone)
    {
        //if a leave happened, cleanup and re-throw the error
        iInputCommands.clear();
        iNodeCapability.iInputFormatCapability.clear();
        iNodeCapability.iOutputFormatCapability.clear();
        OSCL_CLEANUP_BASE_CLASS(PVMFNodeInterfaceImpl);
        OSCL_LEAVE(err);
    }

    Construct();
}

PVMFAMRFFParserNode::~PVMFAMRFFParserNode()
{
    if (iRequestedUsage.key)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(iRequestedUsage.key);
        iRequestedUsage.key = NULL;
    }
    if (iApprovedUsage.key)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(iApprovedUsage.key);
        iApprovedUsage.key = NULL;
    }
    if (iAuthorizationDataKvp.key)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(iAuthorizationDataKvp.key);
        iAuthorizationDataKvp.key = NULL;
    }

    if (iCPM != NULL)
    {
        iCPM->ThreadLogoff();
        PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
        iCPM = NULL;
    }
    if (iDownloadProgressInterface != NULL)
    {
        iDownloadProgressInterface->cancelResumeNotification();
    }

    Cancel();

    //Cleanup allocated ports
    ReleaseAllPorts();
    CleanupFileSource();
    iFileServer.Close();
}


void PVMFAMRFFParserNode::Construct()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::Construct()"));
    iFileServer.Connect();
    iAvailableMetadataKeys.reserve(4);
    iAvailableMetadataKeys.clear();
    iDataPathLogger = PVLogger::GetLoggerObject("datapath.sourcenode.amrparsernode");
}

void PVMFAMRFFParserNode::Run()
{
    if (!iInputCommands.empty())
    {
        if (ProcessCommand())
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::Run() - command processed"));
        }
    }
    // Send outgoing messages
    if (iInterfaceState == EPVMFNodeStarted || IsFlushPending())
    {
        if (!iTrack.iPort)
        {
            PVMF_AMRPARSERNODE_LOGERROR((0, "PVAMRParserNode::Run: Error - no port assigned for track"));
            return;
        }
        PVAMRFFNodeTrackPortInfo* trackPortInfoPtr = &iTrack;

        ProcessPortActivity(trackPortInfoPtr);

        if (CheckForPortRescheduling())
        {
            /*
             * Re-schedule since there is atleast one port that needs processing
             */
            Reschedule();
        }
    }

    if (IsFlushPending()
            && iOutPort
            && iOutPort->OutgoingMsgQueueSize() == 0)
    {
        iOutPort->ResumeInput();
        CommandComplete(iCurrentCommand, PVMFSuccess);
    }
}

PVMFStatus  PVMFAMRFFParserNode::ProcessOutgoingMsg(PVAMRFFNodeTrackPortInfo* aTrackInfoPtr)
{
    /*
     * Called by the AO to process one message off the outgoing
     * message queue for the given port.  This routine will
     * try to send the data to the connected port.
    */
    PVMF_AMRPARSERNODE_LOGSTACKTRACE((0, "PVMFAMRParserNode::ProcessOutgoingMsg() Called aPort=0x%x", aTrackInfoPtr->iPort));
    PVMFStatus status = aTrackInfoPtr->iPort->Send();
    if (status == PVMFErrBusy)
    {
        /* Connected port is busy */
        aTrackInfoPtr->oProcessOutgoingMessages = false;
        PVMF_AMRPARSERNODE_LOGDATATRAFFIC((0, "PVMFAMRParserNode::ProcessOutgoingMsg() Connected port is in busy state"));
    }
    else if (status != PVMFSuccess)
    {
        PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::ProcessOutgoingMsg() - aTrackInfoPtr->iPort->Send() Failed"));
    }
    return status;
}

PVMFStatus PVMFAMRFFParserNode::DoGetNodeMetadataValues()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::DoGetNodeMetadataValues() In"));

    // File must be parsed
    if (!iAMRParser)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::DoGetNodeMetadataValues - Parser object didn't get initialize"));
        return PVMFFailure;
    }

    PVMFMetadataList* keylistptr_in = NULL;
    PVMFMetadataList* keylistptr = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    iCurrentCommand.PVMFNodeCommand::Parse(keylistptr_in, valuelistptr, starting_index, max_entries);

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
                         PVAMR_ALL_METADATA_KEY,
                         oscl_strlen(PVAMR_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &iAvailableMetadataKeys;
        }
    }

    uint32 numkeys = keylistptr->size();

    if (starting_index > (numkeys - 1) || numkeys == 0 || max_entries == 0)
    {
        // Don't do anything
        return PVMFErrArgument;
    }

    uint32 numvalentries = 0;
    int32 numentriesadded = 0;
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        int32 leavecode = 0;
        PvmiKvp KeyVal;
        KeyVal.key = NULL;

        if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_DURATION_KEY) == 0 &&
                iAMRFileInfo.iDuration > 0)
        {
            // Movie Duration
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                char timescalestr[20];
                oscl_snprintf(timescalestr, 20, ";%s%d", PVAMRMETADATA_TIMESCALE, iAMRFileInfo.iTimescale);
                timescalestr[19] = '\0';
                uint32 duration = Oscl_Int64_Utils::get_uint64_lower32(iAMRFileInfo.iDuration);
                int32 retval =
                    PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                            PVAMRMETADATA_DURATION_KEY,
                            duration,
                            timescalestr);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_NUMTRACKS_KEY) == 0)
        {
            // Number of tracks
            // Increment the counter for the number of values found so far
            ++numvalentries;
            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 numtracks = 1;
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVAMRMETADATA_NUMTRACKS_KEY, numtracks);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_TRACKINFO_BITRATE_KEY) == 0) &&
                 iAMRFileInfo.iBitrate > 0)
        {
            // Bitrate
            // Increment the counter for the number of values found so far
            ++numvalentries;
            int32 retval = 0;
            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                char indexparam[16];
                oscl_snprintf(indexparam, 16, ";%s", PVAMRMETADATA_INDEX0);
                indexparam[15] = '\0';
                uint32 bitrate = iAMRFileInfo.iBitrate;
                retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVAMRMETADATA_TRACKINFO_BITRATE_KEY, bitrate, indexparam);
            }
            if (retval != PVMFSuccess && retval != PVMFErrArgument)
            {
                break;
            }

        }

        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_CHANNEL_KEY) == 0) && iAMRFileInfo.iChannel > 0)
        {
            // Channel
            // Increment the counter for the number of values found so far
            ++numvalentries;
            int32 retval = 0;
            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                char indexparam[16];
                oscl_snprintf(indexparam, 16, ";%s" , PVAMRMETADATA_INDEX0);
                indexparam[15] = '\0';
                uint32 channel = iAMRFileInfo.iChannel;
                retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVAMRMETADATA_CHANNEL_KEY, channel);
            }
            if (retval != PVMFSuccess && retval != PVMFErrArgument)
            {
                break;
            }
        }


        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_TRACKINFO_SELECTED_KEY) == 0))
        {
            // Increment the counter for the number of values found so far
            ++numvalentries;
            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                bool trackselected = 1;
                char indexParam[16];
                oscl_snprintf(indexParam, 16, ";%s", PVAMRMETADATA_INDEX0);
                indexParam[15] = '\0';

                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                                    PVAMRMETADATA_TRACKINFO_SELECTED_KEY,
                                    trackselected,
                                    indexParam);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }

        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_RANDOM_ACCESS_DENIED_KEY) == 0)
        {
            /*
             * Random Access
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;

            /* Create a value entry if past the starting index */
            if (numvalentries > (uint32)starting_index)
            {
                bool random_access_denied = false;
                if (iAMRFileInfo.iDuration <= 0) random_access_denied = true;

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                            PVAMRMETADATA_RANDOM_ACCESS_DENIED_KEY,
                            random_access_denied,
                            NULL);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strncmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_CLIP_TYPE_KEY, oscl_strlen(PVAMRMETADATA_CLIP_TYPE_KEY)) == 0)
        {
            /*
             * Clip Type
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;

            /* Create a value entry if past the starting index */
            if (numvalentries > (uint32)starting_index)
            {
                uint32 len = 0;
                char* clipType = NULL;
                {
                    len = oscl_strlen(PVAMRMETADATA_LOCAL_CLIP_TYPE_KEY);
                    clipType = PVMF_BASE_NODE_ARRAY_NEW(char, len + 1);
                    oscl_memset(clipType, 0, len + 1);
                    oscl_strncpy(clipType, (PVAMRMETADATA_LOCAL_CLIP_TYPE_KEY), len);
                }

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                            PVAMRMETADATA_CLIP_TYPE_KEY,
                            clipType);

                PVMF_BASE_NODE_ARRAY_DELETE(clipType);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_TRACKINFO_AUDIO_FORMAT_KEY) == 0) &&
                 iAMRFileInfo.iAmrFormat != EAMRUnrecognized)
        {
            // Format
            // Increment the counter for the number of values found so far
            ++numvalentries;
            int32 retval = 0;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                char indexparam[16];
                oscl_snprintf(indexparam, 16, ";%s", PVAMRMETADATA_INDEX0);
                indexparam[15] = '\0';

                switch (iAMRFileInfo.iAmrFormat)
                {
                    case EAMRIF2:
                        retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal, PVAMRMETADATA_TRACKINFO_AUDIO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_AMR_IF2), indexparam);
                        break;

                    case EAMRETS:
                    case EAMRIETF_SingleNB:
                    case EAMRIETF_MultiNB:
                    case EAMRIETF_SingleWB:
                    case EAMRIETF_MultiWB:
                    case EAMRWMF:
                        retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal, PVAMRMETADATA_TRACKINFO_AUDIO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_AMR_IETF), indexparam);
                        break;

                    case EAMRUnrecognized:
                    default:
                        // Should not enter here
                        OSCL_ASSERT(false);
                        break;
                }
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }

        if (KeyVal.key != NULL)
        {
            // Add the entry to the list
            leavecode = PushBackKeyVal(valuelistptr, KeyVal);
            if (leavecode != 0)
            {
                switch (GetValTypeFromKeyString(KeyVal.key))
                {
                    case PVMI_KVPVALTYPE_CHARPTR:
                        if (KeyVal.value.pChar_value != NULL)
                        {
                            PVMF_BASE_NODE_ARRAY_DELETE(KeyVal.value.pChar_value);
                            KeyVal.value.pChar_value = NULL;
                        }
                        break;

                    default:
                        // Add more case statements if other value types are returned
                        break;
                }

                PVMF_BASE_NODE_ARRAY_DELETE(KeyVal.key);
                KeyVal.key = NULL;
            }
            else
            {
                // Increment the counter for number of value entries added to the list
                ++numentriesadded;
            }

            // Check if the max number of value entries were added
            if (max_entries > 0 && numentriesadded >= max_entries)
            {
                // Maximum number of values added so break out of the loop
                break;
            }
        }
    }

    iAMRParserNodeMetadataValueCount = (*valuelistptr).size();

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

PVMFStatus PVMFAMRFFParserNode::DoSetDataSourcePosition()
{
    //file must be parsed
    if ((!iAMRParser) || (!iTrack.iPort))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::DoSetDataSourcePosition - Parser object didn't get initialize or no port assigned for track"));
        return PVMFFailure;
    }

    uint32 targetNPT = 0;
    uint32* actualNPT = NULL;
    uint32* actualMediaDataTS = NULL;
    bool seektosyncpoint = false;
    uint32 streamID = 0;

    iCurrentCommand.PVMFNodeCommand::Parse(targetNPT, actualNPT, actualMediaDataTS, seektosyncpoint, streamID);

    iTrack.iSendBOS = true;

    //save the stream id for next media segment
    iStreamID = streamID;

    *actualNPT = 0;
    *actualMediaDataTS = 0;


    // Peek the next sample to get the duration of the last sample
    uint32 timestamp;
    int32 result = iAMRParser->PeekNextTimestamp(&timestamp);
    if (result != bitstreamObject::EVERYTHING_OK)
    {
        return PVMFErrResource;
    }

    // get media data TS (should be equal to iContinuousTimeStamp)
    if (iInterfaceState != EPVMFNodePrepared)
    {
        iTrack.iContinuousTimeStamp += PVMF_AMR_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS;
        iTrack.iClockConverter->update_clock(Oscl_Int64_Utils::get_uint64_lower32(iTrack.iContinuousTimeStamp));
    }
    uint32 millisecTS = iTrack.iClockConverter->get_converted_ts(1000);
    *actualMediaDataTS = millisecTS;

    // see if targetNPT is greater or equal than clip duration.
    uint32 duration = Oscl_Int64_Utils::get_uint64_lower32(iAMRFileInfo.iDuration);
    uint32 timescale = iAMRFileInfo.iTimescale;
    if (timescale > 0 && timescale != 1000)
    {
        // Convert to milliseconds
        MediaClockConverter mcc(timescale);
        mcc.update_clock(duration);
        duration = mcc.get_converted_ts(1000);
    }
    if (targetNPT >= duration)
    {
        // report EOS for the track.
        iTrack.iSeqNum = 0;
        iTrack.oEOSReached = true;
        iTrack.oQueueOutgoingMessages = true;
        iTrack.oEOSSent = false;

        result = iAMRParser->ResetPlayback(0);
        if (result != bitstreamObject::EVERYTHING_OK)
        {
            return PVMFErrResource;
        }

        *actualNPT = duration;
        return PVMFSuccess;
    }


    // Reposition
    // If new position is past the end of clip, AMR FF should set the position to the last frame
    result = iAMRParser->ResetPlayback(targetNPT);
    if (result != bitstreamObject::EVERYTHING_OK)
    {
        if (bitstreamObject::END_OF_FILE == result)
        {

            iTrack.iSeqNum = 0;
            iTrack.oEOSReached = true;
            iTrack.oQueueOutgoingMessages = true;
            iTrack.oEOSSent = false;

            result = iAMRParser->ResetPlayback(0);
            if (result != bitstreamObject::EVERYTHING_OK)
            {
                return PVMFErrResource;
            }

            *actualNPT = result;
            return PVMFSuccess;
        }
        else if (bitstreamObject::DATA_INSUFFICIENT == result)
        {
            // This condition could mean 2 things
            // 1) End Of File reached for a local content
            // 2) Insufficient data condition met for PDL use-case
            // For 1 treat it as End of File and send End of Track
            // For 2 we dont support reposition until the clip is fully downloaded,
            // if the clip is fully downloaded and then we get Insufficient data condition
            // treat it as End Of File.
            if (iDownloadProgressInterface != NULL)
            {
                // Check if the file is completely Downloaded or not
                if (!iDownloadComplete)
                {
                    // File not downloaded completely, return not Supported
                    return PVMFErrNotSupported;
                }
            }

            // Here either file is completely downlaoded if doing PDL or it is a local file,
            // treat it as End of File in both cases
            iTrack.iSeqNum = 0;
            iTrack.oEOSReached = true;
            iTrack.oQueueOutgoingMessages = true;
            iTrack.oEOSSent = false;

            result = iAMRParser->ResetPlayback(0);
            if (result != bitstreamObject::EVERYTHING_OK)
            {
                return PVMFErrResource;
            }

            *actualNPT = result;
            return PVMFSuccess;
        }
        else
        {
            return PVMFErrResource;
        }
    }

    //Peek new position to get the actual new timestamp
    uint32 newtimestamp;
    result = iAMRParser->PeekNextTimestamp(&newtimestamp);
    if (result != bitstreamObject::EVERYTHING_OK)
    {
        return PVMFErrResource;
    }
    *actualNPT = newtimestamp;


    ResetAllTracks();
    return PVMFSuccess;
}


PVMFStatus PVMFAMRFFParserNode::DoQueryDataSourcePosition()
{
    //file must be parsed
    if ((!iAMRParser) || (!iTrack.iPort))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::DoQueryDataSourcePosition - Parser object didn't get initialize or no port assigned for track"));
        return PVMFFailure;
    }

    uint32 targetNPT = 0;
    uint32* actualNPT = NULL;
    bool seektosyncpoint = false;

    iCurrentCommand.PVMFNodeCommand::Parse(targetNPT, actualNPT, seektosyncpoint);
    if (actualNPT == NULL)
    {
        return PVMFErrArgument;
    }

    // Query
    // If new position is past the end of clip, AMR FF should set the position to the last frame
    *actualNPT = iAMRParser->SeekPointFromTimestamp(targetNPT);

    return PVMFSuccess;
}

PVMFStatus PVMFAMRFFParserNode::DoSetDataSourceRate()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::DoSetDataSourceRate() In"));
    return PVMFSuccess;
}


PVMFStatus PVMFAMRFFParserNode::HandleExtensionAPICommands()
{
    PVMFStatus status = PVMFErrNotSupported;
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
            status = DoGetNodeMetadataValues();
            break;
        default:
            // unknown command type, assert false
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::HandleExtensionAPICommands() - Unknown Cmd type %d; Cmd Id %d", iCurrentCommand.iCmd, iCurrentCommand.iId));
            OSCL_ASSERT(false);
            break;
    }
    return status;
}


void PVMFAMRFFParserNode::HandlePortActivity(const PVMFPortActivity &aActivity)
{
    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFAMRFFParserNode::PortActivity: Outgoing Msg"));
            RunIfNotReady();
            break;
        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFAMRFFParserNode::PortActivity: Connected port ready"));
            //This message is send by destination port to notify that the earlier Send
            //call that failed due to its busy status can be resumed now.
            if (iOutPort
                    && iOutPort->OutgoingMsgQueueSize() > 0)
            {
                RunIfNotReady();
            }
            break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_READY:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFAMRFFParserNode::PortActivity: Outgoing Queue ready"));
            //this message is sent by the OutgoingQueue when it recovers from
            //the queue full status
            RunIfNotReady();
            break;

        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
        case PVMF_PORT_ACTIVITY_CONNECT:
        case PVMF_PORT_ACTIVITY_DISCONNECT:
        default:
            break;
    }
}

PVMFStatus PVMFAMRFFParserNode::DoQueryInterface()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFAMRFFParserNode::DoQueryInterface"));

    PVUuid* uuid;
    PVInterface** ptr;
    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, ptr);
    PVMFStatus status = PVMFErrNotSupported;

    if (queryInterface(*uuid, *ptr))
    {
        // PVMFCPMPluginLicenseInterface is not a part of this node
        if (*uuid != PVMFCPMPluginLicenseInterfaceUuid)
        {
            (*ptr)->addRef();
        }
        status = PVMFSuccess;
    }
    return status;
}

PVMFStatus PVMFAMRFFParserNode::DoInit()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::DoInitNode() In"));

    if (EPVMFNodeInitialized == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFAMRFFParserNode::DoInit() already in Initialized state"));
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

        return PVMFPending;
    }
    else
    {
        if (CheckForAMRHeaderAvailability() == PVMFSuccess)
        {
            ParseAMRFile();
            return PVMFSuccess;
        }
    }
    return PVMFSuccess;
}

PVMFStatus PVMFAMRFFParserNode::ParseAMRFile()
{
    int32 err;
    OSCL_TRY(err, iAMRParser = PVMF_BASE_NODE_NEW(CAMRFileParser, ()));
    if ((err != OsclErrNone) || (!iAMRParser))
    {
        return PVMFErrNoMemory;
    }

    PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
    bool calcDuration = true;
    if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
    {
        dsFactory = iDataStreamFactory;
        calcDuration = false;
    }

    if (iAMRParser->InitAMRFile(iSourceURL, calcDuration, &iFileServer, dsFactory, iFileHandle, iCountToCalculateRDATimeInterval))
    {
        iAvailableMetadataKeys.clear();
        if (iAMRParser->RetrieveFileInfo(iAMRFileInfo))
        {
            PVMFStatus status = InitMetaData();
            if (status == PVMFSuccess)
            {
                return PVMFSuccess;
            }
            else
            {
                CleanupFileSource();

                PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::ParseAMRFile() - InitMetaData Failed"));

                CommandComplete(iCurrentCommand, status);
            }

        }
        else
        {
            return PVMFErrResource;
        }
    }
    else
    {
        //cleanup if failure
        PVMF_BASE_NODE_DELETE(iAMRParser);
        iAMRParser = NULL;
        return PVMFErrResource;
    }
    return PVMFSuccess;
}


PVMFStatus PVMFAMRFFParserNode::DoStop()
{
    iStreamID = 0;
    PVMFStatus status = PVMFSuccess;


    if (iDataStreamInterface != NULL)
    {
        PVInterface* iFace = OSCL_STATIC_CAST(PVInterface*, iDataStreamInterface);
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        iDataStreamFactory->DestroyPVMFCPMPluginAccessInterface(uuid, iFace);
        iDataStreamInterface = NULL;
    }
    // stop and reset position to beginning
    ResetAllTracks();

    // Reset the AMR FF to beginning
    if (iAMRParser)
    {
        iAMRParser->ResetPlayback(0);
    }

    //clear msg queue
    if (iOutPort)
    {
        iOutPort->ClearMsgQueues();
    }

    return status;
}

PVMFStatus PVMFAMRFFParserNode::DoReset()
{

    PVMF_AMRPARSERNODE_LOGSTACKTRACE((0, "PVMFAMRParserNode::DoReset() Called"));

    if (iDownloadProgressInterface != NULL)
    {
        iDownloadProgressInterface->cancelResumeNotification();
    }

    if ((iAMRParser) && (iCPM) && (iCPMContentType != PVMF_CPM_CONTENT_FORMAT_UNKNOWN))
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
        CompleteReset();
        return PVMFSuccess;
    }
}

PVMFStatus PVMFAMRFFParserNode::DoRequestPort(PVMFPortInterface*&aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::DoRequestPort() In"));
    aPort = NULL;

    if ((iInterfaceState != EPVMFNodePrepared) || (!iAMRParser))
    {
        PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRFFParserNode::DoRequestPort() - Invalid State"));
        return PVMFErrInvalidState;
    }

    int32 tag = 0;
    OSCL_String* mime_string;
    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, mime_string);

    if ((tag != PVMF_AMRFFPARSER_NODE_PORT_TYPE_SOURCE) || (iOutPort))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFAMRFFParserNode::DoRequestPort: Error - port already exists or Invalid port tag"));
        return PVMFFailure;
    }

    iOutPort = PVMF_BASE_NODE_NEW(PVMFAMRFFParserOutPort, (PVMF_AMRFFPARSER_NODE_PORT_TYPE_SOURCE, this));
    if (!iOutPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFAMRFFParserNode::DoRequestPort: Error - no memory"));
        return PVMFErrNoMemory;
    }
    if (mime_string)
    {
        PVMFFormatType fmt = mime_string->get_str();
        if (!iOutPort->IsFormatSupported(fmt))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFAMRFFParserNode::DoRequestPort: Error - format not supported"));
            PVMF_BASE_NODE_DELETE(iOutPort);
            iOutPort = NULL;
            return PVMFFailure;
        }
    }

    MediaClockConverter* clockconv = NULL;
    OsclMemPoolFixedChunkAllocator* trackdatamempool = NULL;
    PVMFSimpleMediaBufferCombinedAlloc* mediadataimplalloc = NULL;
    PVMFMemPoolFixedChunkAllocator* mediadatamempool = NULL;
    int32 leavecode = 0;
    OSCL_TRY(leavecode,
             clockconv = PVMF_BASE_NODE_NEW(MediaClockConverter, (iAMRFileInfo.iTimescale));
             trackdatamempool = PVMF_BASE_NODE_NEW(OsclMemPoolFixedChunkAllocator, (PVAMRFF_MEDIADATA_POOLNUM));
             mediadataimplalloc = PVMF_BASE_NODE_NEW(PVMFSimpleMediaBufferCombinedAlloc, (trackdatamempool));
             mediadatamempool = PVMF_BASE_NODE_NEW(PVMFMemPoolFixedChunkAllocator, ("AmrFFPar", PVAMRFF_MEDIADATA_POOLNUM, PVAMRFF_MEDIADATA_CHUNKSIZE));
            );

    if (leavecode || !clockconv || !trackdatamempool || !mediadataimplalloc || !mediadatamempool)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFAMRFFParserNode::DoRequestPort: Error - unable to create clockconv, trackdatamempool, mediadataimplalloc, and mediadatamempool"));
        if (iOutPort)
        {
            PVMF_BASE_NODE_DELETE(iOutPort);
            iOutPort = NULL;
        }
        if (clockconv)
        {
            PVMF_BASE_NODE_DELETE(clockconv);
        }
        if (trackdatamempool)
        {
            PVMF_BASE_NODE_DELETE(trackdatamempool);
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

    mediadatamempool->enablenullpointerreturn();

    iTrack.iTrackId = 0;  // Only support 1 channel so far
    iTrack.iTag = PVMF_AMRFFPARSER_NODE_PORT_TYPE_SOURCE;
    iTrack.iPort = iOutPort;

    iTrack.iClockConverter = clockconv;
    iTrack.iTrackDataMemoryPool = trackdatamempool;
    iTrack.iMediaDataImplAlloc = mediadataimplalloc;
    iTrack.iMediaDataMemPool = mediadatamempool;
    iTrack.iContinuousTimeStamp = 0;

    aPort = iOutPort;

    OsclMemPoolResizableAllocator* trackDataResizableMemPool = NULL;
    iTrack.iResizableDataMemoryPoolSize = PVMF_AMR_PARSER_NODE_MAX_AUDIO_DATA_MEM_POOL_SIZE;
    trackDataResizableMemPool = PVMF_BASE_NODE_NEW(OsclMemPoolResizableAllocator,
                                (iTrack.iResizableDataMemoryPoolSize,
                                 PVMF_AMR_PARSER_NODE_DATA_MEM_POOL_GROWTH_LIMIT));

    PVUuid eventuuid = PVMFAMRParserNodeEventTypesUUID;

    PVMFResizableSimpleMediaMsgAlloc* resizableSimpleMediaDataImplAlloc = NULL;
    OsclExclusivePtr<PVMFResizableSimpleMediaMsgAlloc> resizableSimpleMediaDataImplAllocAutoPtr;
    resizableSimpleMediaDataImplAlloc = PVMF_BASE_NODE_NEW(PVMFResizableSimpleMediaMsgAlloc,
                                        (trackDataResizableMemPool));

    if (trackDataResizableMemPool == NULL)
    {
        PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::DoRequestPort() - trackDataResizableMemPool Alloc Failed"));

        return PVMFErrNoMemory;
    }

    trackDataResizableMemPool->enablenullpointerreturn();

    iTrack.iResizableSimpleMediaMsgAlloc = resizableSimpleMediaDataImplAlloc;
    iTrack.iResizableDataMemoryPool = trackDataResizableMemPool;
    iTrack.iNode = this;
    uint8* typeSpecificInfoBuff = iAMRParser->getCodecSpecificInfo();
    uint32 typeSpecificDataLength = MAX_NUM_PACKED_INPUT_BYTES;
    if ((int32)typeSpecificDataLength > 0)
    {
        OsclMemAllocDestructDealloc<uint8> my_alloc;
        OsclRefCounter* my_refcnt;
        uint aligned_refcnt_size =
            oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));
        uint aligned_type_specific_info_size =
            oscl_mem_aligned_size(typeSpecificDataLength);
        uint8* my_ptr = NULL;
        int32 errcode = 0;
        OSCL_TRY(errcode,
                 my_ptr = (uint8*) my_alloc.ALLOCATE(aligned_refcnt_size + aligned_type_specific_info_size));
        if ((errcode != OsclErrNone) || (!my_ptr))
        {
            PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::PopulateTrackInfoVec - Unable to Allocate Memory"));
            return PVMFErrNoMemory;
        }

        my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
        my_ptr += aligned_refcnt_size;

        OsclMemoryFragment memfrag;
        memfrag.len = typeSpecificDataLength;
        memfrag.ptr = typeSpecificInfoBuff;

        OsclRefCounterMemFrag tmpRefcntMemFrag(memfrag, my_refcnt, memfrag.len);
        iTrack.iFormatSpecificConfig = tmpRefcntMemFrag;
    }

    return PVMFSuccess;
}

PVMFStatus PVMFAMRFFParserNode::DoReleasePort()
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFAMRFFParserNode::DoReleasePort"));

    // search for the matching port address
    // disconnect it, if needed
    // cleanup the buffers associated with it
    // delete the port
    // set the address to NULL

    // Remove the selected track from the track list

    if (iTrack.iPort == iCurrentCommand.iParam1)
    {
        // Found the element. So erase it
        ReleaseAllPorts();
        return PVMFSuccess;
    }


    //if we get here the track was not found
    return PVMFErrBadHandle;

}

void PVMFAMRFFParserNode::ResetAllTracks()
{

    iTrack.iMediaData.Unbind();
    iTrack.iSeqNum = 0;
    iTrack.iFirstFrame = true;

    iTrack.oEOSSent = false;
    iTrack.oEOSReached = false;
    iTrack.oQueueOutgoingMessages = true;

}

bool PVMFAMRFFParserNode::ReleaseAllPorts()
{

    if (iTrack.iPort == NULL)
    {
        return true;
    }
    iTrack.iPort->Disconnect();
    iTrack.iMediaData.Unbind();
    PVMF_BASE_NODE_DELETE(((PVMFAMRFFParserOutPort*)iTrack.iPort));
    if (iTrack.iClockConverter)
    {
        PVMF_BASE_NODE_DELETE(iTrack.iClockConverter);
    }
    if (iTrack.iTrackDataMemoryPool)
    {
        iTrack.iTrackDataMemoryPool->removeRef();
        iTrack.iTrackDataMemoryPool = NULL;
    }
    if (iTrack.iMediaDataImplAlloc)
    {
        PVMF_BASE_NODE_DELETE(iTrack.iMediaDataImplAlloc);
    }
    if (iTrack.iMediaDataMemPool)
    {
        iTrack.iMediaDataMemPool->CancelFreeChunkAvailableCallback();
        iTrack.iMediaDataMemPool->removeRef();
        iTrack.iMediaDataMemPool = NULL;
    }
    iOutPort = NULL;

    if (iTrack.iResizableSimpleMediaMsgAlloc != NULL)
    {
        PVMF_BASE_NODE_DELETE(iTrack.iResizableSimpleMediaMsgAlloc);
        iTrack.iResizableSimpleMediaMsgAlloc = NULL;
    }
    if (iTrack.iResizableDataMemoryPool != NULL)
    {
        iTrack.iResizableDataMemoryPool->removeRef();
        iTrack.iResizableDataMemoryPool = NULL;
    }
    iTrack.iPort = NULL;

    return true;
}

void PVMFAMRFFParserNode::CleanupFileSource()
{
    iAvailableMetadataKeys.clear();

    if (iAMRParser)
    {
        PVMF_BASE_NODE_DELETE(iAMRParser);
        iAMRParser = NULL;
    }

    iUseCPMPluginRegistry = false;
    iCPMSourceData.iFileHandle = NULL;
    iAMRParserNodeMetadataValueCount = 0;

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
        PVMF_BASE_NODE_DELETE(iFileHandle);
        iFileHandle = NULL;
    }

}

void PVMFAMRFFParserNode::addRef()
{
    ++iExtensionRefCount;
}

void PVMFAMRFFParserNode::removeRef()
{
    --iExtensionRefCount;
}

PVMFStatus PVMFAMRFFParserNode::QueryInterfaceSync(PVMFSessionId aSession,
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

bool PVMFAMRFFParserNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
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


PVMFStatus PVMFAMRFFParserNode::SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData, uint32 aClipIndex, PVMFFormatTypeDRMInfo aType)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::SetSourceInitializationData() called"));

    if (aClipIndex != 0)
        return PVMFErrArgument; //playlist not supported.

    if (aSourceFormat != PVMF_MIME_AMRFF)
    {
        PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::SetSourceInitializationData - Unsupported Format %s", aSourceFormat.getMIMEStrPtr()));
        return PVMFFailure;
    }

    /* Clean up any previous sources */
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

                iFileHandle = PVMF_BASE_NODE_NEW(OsclFileHandle,
                                                 (*(context->iFileHandle)));

                iCPMSourceData.iFileHandle = iFileHandle;
            }
            iCPMSourceData.iPreviewMode = iPreviewMode;
            iCPMSourceData.iIntent = context->iIntent;

        }
        else
        {
            // New context object ?
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
                    iFileHandle = PVMF_BASE_NODE_NEW(OsclFileHandle,
                                                     (*(context->iFileHandle)));

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
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::SetSourceInitializationData() create CPM obj"));
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
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::SetSourceInitializationData() non-create CPM obj"));
            iUseCPMPluginRegistry = false;
        }
        return PVMFSuccess;
    }
    return PVMFSuccess;

}

PVMFStatus PVMFAMRFFParserNode::SetClientPlayBackClock(PVMFMediaClock* aClientClock)
{
    OSCL_UNUSED_ARG(aClientClock);
    return PVMFSuccess;
}

PVMFStatus PVMFAMRFFParserNode::SetEstimatedServerClock(PVMFMediaClock* aClientClock)
{
    OSCL_UNUSED_ARG(aClientClock);
    return PVMFSuccess;
}

void PVMFAMRFFParserNode::AudioSinkEvent(PVMFStatus , uint32)
{
    //ignore this event
}

PVMFStatus PVMFAMRFFParserNode::GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::GetMediaPresentationInfo() called"));

    if (!iAMRParser)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::GetMediaPresentationInfo - iAMRParser is NULL"));
        return PVMFFailure;
    }

    aInfo.setDurationValue(iAMRFileInfo.iDuration);
    // Current version of AMR parser is limited to 1 channel
    PVMFTrackInfo tmpTrackInfo;
    tmpTrackInfo.setPortTag(PVMF_AMRFFPARSER_NODE_PORT_TYPE_SOURCE);
    tmpTrackInfo.setTrackID(0);
    TPVAmrFileInfo amrinfo;
    if (!iAMRParser->RetrieveFileInfo(amrinfo)) return PVMFErrNotSupported;

    switch (amrinfo.iAmrFormat)
    {
            // Supported formats
        case EAMRIF2:       // IF2
        case EAMRIETF_SingleNB: // IETF
        case EAMRIETF_SingleWB:
            break;

            // Everything else is not supported
        case EAMRETS:
        case EAMRIETF_MultiNB:
        case EAMRIETF_MultiWB:
        case EAMRWMF:
        case EAMRUnrecognized:
        default:
            return PVMFErrNotSupported;
    }
    tmpTrackInfo.setTrackBitRate(amrinfo.iBitrate);
    tmpTrackInfo.setTrackDurationTimeScale((uint64)amrinfo.iTimescale);
    tmpTrackInfo.setTrackDurationValue(amrinfo.iDuration);
    OSCL_FastString mime_type = _STRLIT_CHAR(PVMF_MIME_AMR_IETF);
    if (amrinfo.iAmrFormat == EAMRIF2)
    {
        mime_type = _STRLIT_CHAR(PVMF_MIME_AMR_IF2);
    }
    else if (EAMRIETF_SingleWB == amrinfo.iAmrFormat)
        mime_type = _STRLIT_CHAR(PVMF_MIME_AMRWB_IETF);


    tmpTrackInfo.setTrackMimeType(mime_type);
    aInfo.addTrackInfo(tmpTrackInfo);
    return PVMFSuccess;
}


PVMFStatus PVMFAMRFFParserNode::SelectTracks(PVMFMediaPresentationInfo& aInfo)
{
    OSCL_UNUSED_ARG(aInfo);
    return PVMFSuccess;
}

uint32 PVMFAMRFFParserNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::GetNumMetadataValues() called"));

    uint32 numkeys = aKeyList.size();
    if (!iAMRParser || numkeys == 0)
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
    //If numkeys is one, just check to see if the request
    //is for ALL metadata
    if (aKeyList.size() == 1)
    {
        if (oscl_strncmp((*keylistptr)[0].get_cstr(),
                         PVAMR_ALL_METADATA_KEY,
                         oscl_strlen(PVAMR_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &iAvailableMetadataKeys;
        }
    }

    // Count the number of metadata value entries based on the key list provided
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_DURATION_KEY) == 0 &&
                iAMRFileInfo.iDuration > 0)
        {
            // Movie Duration
            ++numvalentries;
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_NUMTRACKS_KEY) == 0)
        {
            // Number of tracks
            ++numvalentries;
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_TRACKINFO_BITRATE_KEY) == 0) &&
                 iAMRFileInfo.iBitrate > 0)
        {
            // Bitrate
            ++numvalentries;
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_TRACKINFO_SELECTED_KEY) == 0))
        {
            //Track Selected
            ++numvalentries;
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_TRACKINFO_AUDIO_FORMAT_KEY) == 0) &&
                 iAMRFileInfo.iAmrFormat != EAMRUnrecognized)
        {
            // Format
            ++numvalentries;
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_CHANNEL_KEY) == 0) &&
                 iAMRFileInfo.iChannel > 0)
        {
            // Number of Channels
            ++numvalentries;
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_RANDOM_ACCESS_DENIED_KEY) == 0)
        {
            /*
             * Random Access
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVAMRMETADATA_CLIP_TYPE_KEY) == 0)
        {
            /*
             * clip-type
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;
        }


    }

    return numvalentries;
}

PVMFCommandId PVMFAMRFFParserNode::GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList
        , uint32 aStartingValueIndex, int32 aMaxValueEntries, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::GetNodeMetadataValue() called"));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_GETNODEMETADATAVALUES, aKeyList, aValueList, aStartingValueIndex, aMaxValueEntries, aContext);
    return QueueCommandL(cmd);
}

PVMFStatus PVMFAMRFFParserNode::ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 start, uint32 end)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::ReleaseNodeMetadataValues() called"));

    if (start > end || aValueList.size() == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFAMRFFParserNode::ReleaseNodeMetadataValues() Invalid start/end index"));
        return PVMFErrArgument;
    }

    if ((iCPMMetaDataExtensionInterface != NULL))
    {
        PVMFStatus status = iCPMMetaDataExtensionInterface->ReleaseNodeMetadataValues(aValueList, start, end);
        if (status != PVMFSuccess)
        {
            return status;
        }
    }
    end = OSCL_MIN(aValueList.size(), iAMRParserNodeMetadataValueCount);

    for (uint32 i = start; i < end; i++)
    {
        if (aValueList[i].key != NULL)
        {
            switch (GetValTypeFromKeyString(aValueList[i].key))
            {
                case PVMI_KVPVALTYPE_CHARPTR:
                    if (aValueList[i].value.pChar_value != NULL)
                    {
                        PVMF_BASE_NODE_ARRAY_DELETE(aValueList[i].value.pChar_value);
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

            PVMF_BASE_NODE_ARRAY_DELETE(aValueList[i].key);
            aValueList[i].key = NULL;
        }
    }

    return PVMFSuccess;
}

PVMFCommandId PVMFAMRFFParserNode::SetDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aActualNPT
        , PVMFTimestamp& aActualMediaDataTS
        , bool aSeekToSyncPoint
        , uint32 aStreamID
        , OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFAMRFFParserNode::SetDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION, aTargetNPT, &aActualNPT,
                                   &aActualMediaDataTS, aSeekToSyncPoint, aStreamID, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFAMRFFParserNode::QueryDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aActualNPT
        , bool aSeekToSyncPoint
        , OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFAMRFFParserNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, &aActualNPT,
                                   aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFAMRFFParserNode::QueryDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aSeekPointBeforeTargetNPT
        , PVMFTimestamp& aSeekPointAfterTargetNPT
        , OsclAny* aContext
        , bool aSeekToSyncPoint)
{
    OSCL_UNUSED_ARG(aSeekPointAfterTargetNPT);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFAMRFFParserNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFNodeCommand cmd;
    // Construct not changed,aSeekPointBeforeTargetNPT has replace aActualtNPT
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, &aSeekPointBeforeTargetNPT,
                                   aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFAMRFFParserNode::SetDataSourceRate(PVMFSessionId aSessionId
        , int32 aRate
        , PVMFTimebase* aTimebase
        , OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::SetDataSourceRate() called"));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_RATE, aRate, aTimebase, aContext);
    return QueueCommandL(cmd);
}


PVMFStatus PVMFAMRFFParserNode::CheckForAMRHeaderAvailability()
{
    if (iDataStreamInterface != NULL)
    {
        /*
         * First check if we have minimum number of bytes to recognize
         * the file and determine the header size.
         */
        TOsclFileOffset currCapacity = 0;
        iDataStreamInterface->QueryReadCapacity(iDataStreamSessionID,
                                                currCapacity);

        if (currCapacity < (TOsclFileOffset)AMR_MIN_DATA_SIZE_FOR_RECOGNITION)
        {
            iRequestReadCapacityNotificationID =
                iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                        *this,
                        AMR_MIN_DATA_SIZE_FOR_RECOGNITION);
            return PVMFPending;
        }


        uint32 headerSize32 =
            Oscl_Int64_Utils::get_uint64_lower32(iAMRHeaderSize);

        if (currCapacity < (TOsclFileOffset)headerSize32)
        {
            iRequestReadCapacityNotificationID =
                iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                        *this,
                        headerSize32);
            return PVMFPending;
        }
    }
    return PVMFSuccess;
}


bool PVMFAMRFFParserNode::GetTrackInfo(int32 aTrackID,
                                       PVAMRFFNodeTrackPortInfo*& aTrackInfoPtr)
{
    if (iTrack.iTrackId == aTrackID)
    {
        aTrackInfoPtr = &iTrack;
        return true;
    }

    return false;
}


bool PVMFAMRFFParserNode::ProcessPortActivity(PVAMRFFNodeTrackPortInfo* aTrackInfoPtr)
{
    /*
     * called by the AO to process a port activity message
     */
    PVMF_AMRPARSERNODE_LOGSTACKTRACE((0, "PVMFAMRParserNode::ProcessPortActivity() Called"));

    PVMFStatus status;
    if (aTrackInfoPtr->oQueueOutgoingMessages)
    {
        status = QueueMediaSample(aTrackInfoPtr);

        if ((status != PVMFErrBusy) &&
                (status != PVMFSuccess) &&
                (status != PVMFErrInvalidState))
        {
            PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::ProcessPortActivity() QueueMediaSample Failed - Err=%d", status));
            return false;
        }
        if (iAutoPaused == true)
        {
            aTrackInfoPtr->oQueueOutgoingMessages = false;
            PVMF_AMRPARSERNODE_LOGDATATRAFFIC((0, "PVMFAMRParserNode::QueueMediaSample() - Auto Paused"));
            return PVMFErrBusy;
        }
        if (aTrackInfoPtr->iPort->IsOutgoingQueueBusy())
        {
            aTrackInfoPtr->oQueueOutgoingMessages = false;
            PVMF_AMRPARSERNODE_LOGDATATRAFFIC((0, "PVMFAMRParserNode::QueueMediaSample() Port Outgoing Queue Busy"));
            return PVMFErrBusy;
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
                PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRFFParserNode::ProcessPortActivity() ProcessOutgoingMsg Failed - Err=%d", status));
                ReportErrorEvent(PVMFErrPortProcessing);
            }
        }
        else
        {
            /* Nothing to send - wait for more data */
            aTrackInfoPtr->oProcessOutgoingMessages = false;
        }
    }
    return true;
}

bool PVMFAMRFFParserNode::CheckForPortRescheduling()
{
    if (!iTrack.iPort)
    {
        PVMF_AMRPARSERNODE_LOGERROR((0, "PVAMRParserNode::CheckForPortRescheduling: Error - no port assigned for track"));
        return false;
    }
    PVAMRFFNodeTrackPortInfo* trackInfoPtr = &iTrack;

    if ((trackInfoPtr->oProcessOutgoingMessages) ||
            (trackInfoPtr->oQueueOutgoingMessages))
    {
        /*
         * Found a port that has outstanding activity and
         * is not busy.
         */
        return true;
    }
    /*
     * No port processing needed - either all port activity queues are empty
     * or the ports are backed up due to flow control.
     */
    return false;
}

PVMFStatus PVMFAMRFFParserNode::InitMetaData()
{
    if (iAMRFileInfo.iFileSize > 0)
    {
        // Populate the metadata key vector based on info available
        if (iAMRFileInfo.iChannel > 0)
        {
            PushToAvailableMetadataKeysList(PVAMRMETADATA_CHANNEL_KEY);

        }
        PushToAvailableMetadataKeysList(PVAMRMETADATA_NUMTRACKS_KEY);
        if (iAMRFileInfo.iDuration > 0)
        {
            PushToAvailableMetadataKeysList(PVAMRMETADATA_DURATION_KEY);

        }
        if (iAMRFileInfo.iBitrate > 0)
        {
            PushToAvailableMetadataKeysList(PVAMRMETADATA_TRACKINFO_BITRATE_KEY);

        }
        if (iAMRFileInfo.iAmrFormat != EAMRUnrecognized)
        {
            PushToAvailableMetadataKeysList(PVAMRMETADATA_TRACKINFO_AUDIO_FORMAT_KEY);

        }
        PushToAvailableMetadataKeysList(PVAMRMETADATA_RANDOM_ACCESS_DENIED_KEY);
        PushToAvailableMetadataKeysList(PVAMRMETADATA_CLIP_TYPE_KEY);
        PushToAvailableMetadataKeysList(PVAMRMETADATA_TRACKINFO_SELECTED_KEY);

        //set clip duration on download progress interface
        //applicable to PDL sessions
        {
            if ((iDownloadProgressInterface != NULL) && (iAMRFileInfo.iDuration != 0))
            {
                iDownloadProgressInterface->setClipDuration(OSCL_CONST_CAST(uint32, iAMRFileInfo.iDuration));
            }
        }

        return PVMFSuccess;
    }
    else
        return PVMFFailure;

}

void PVMFAMRFFParserNode::PushToAvailableMetadataKeysList(const char* aKeystr, char* aOptionalParam)
{
    if (aKeystr == NULL)
    {
        return;
    }
    int32 leavecode = 0;
    if (aOptionalParam)
    {
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_front(aKeystr);
                 iAvailableMetadataKeys[0] += aOptionalParam;);
    }
    else
    {
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_front(aKeystr));
    }
}

void PVMFAMRFFParserNode::InitCPM()
{
    iCPMInitCmdId = iCPM->Init();
}

void PVMFAMRFFParserNode::OpenCPMSession()
{
    iCPMOpenSessionCmdId = iCPM->OpenSession(iCPMSessionID);
}

void PVMFAMRFFParserNode::CPMRegisterContent()
{
    iCPMRegisterContentCmdId = iCPM->RegisterContent(iCPMSessionID,
                               iSourceURL,
                               iSourceFormat,
                               (OsclAny*) & iCPMSourceData);
}

void PVMFAMRFFParserNode::GetCPMLicenseInterface()
{
    iCPMLicenseInterfacePVI = NULL;
    iCPMGetLicenseInterfaceCmdId =
        iCPM->QueryInterface(iCPMSessionID,
                             PVMFCPMPluginLicenseInterfaceUuid,
                             iCPMLicenseInterfacePVI);
}

bool PVMFAMRFFParserNode::GetCPMContentAccessFactory()
{
    PVMFStatus status = iCPM->GetContentAccessFactory(iCPMSessionID,
                        iCPMContentAccessFactory);
    if (status != PVMFSuccess)
    {
        return false;
    }
    return true;
}

bool PVMFAMRFFParserNode::GetCPMMetaDataExtensionInterface()
{
    PVInterface* temp = NULL;
    bool retVal =
        iCPM->queryInterface(KPVMFMetadataExtensionUuid, temp);
    iCPMMetaDataExtensionInterface = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, temp);
    return retVal;
}

void PVMFAMRFFParserNode::RequestUsage()
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

    oSourceIsCurrent = true;
}

void PVMFAMRFFParserNode::PopulateDRMInfo()
{
    if (iRequestedUsage.key)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(iRequestedUsage.key);
        iRequestedUsage.key = NULL;
    }

    if (iApprovedUsage.key)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(iApprovedUsage.key);
        iApprovedUsage.key = NULL;
    }

    if (iAuthorizationDataKvp.key)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(iAuthorizationDataKvp.key);
        iAuthorizationDataKvp.key = NULL;
    }

    if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
            (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
    {
        int32 UseKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING));
        int32 AuthKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_AUTHORIZATION_DATA_KEY_STRING));
        int32 leavecode = 0;

        OSCL_TRY(leavecode,
                 iRequestedUsage.key = PVMF_BASE_NODE_ARRAY_NEW(char, UseKeyLen + 1);
                 iApprovedUsage.key = PVMF_BASE_NODE_ARRAY_NEW(char, UseKeyLen + 1);
                 iAuthorizationDataKvp.key = PVMF_BASE_NODE_ARRAY_NEW(char, AuthKeyLen + 1);
                );
        if (leavecode || !iRequestedUsage.key || !iApprovedUsage.key || !iAuthorizationDataKvp.key)
        {
            if (iRequestedUsage.key)
            {
                PVMF_BASE_NODE_ARRAY_DELETE(iRequestedUsage.key);
                iRequestedUsage.key = NULL;
            }
            if (iApprovedUsage.key)
            {
                PVMF_BASE_NODE_ARRAY_DELETE(iApprovedUsage.key);
                iApprovedUsage.key = NULL;
            }
            if (iAuthorizationDataKvp.key)
            {
                PVMF_BASE_NODE_ARRAY_DELETE(iAuthorizationDataKvp.key);
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

void PVMFAMRFFParserNode::SendUsageComplete()
{
    iCPMUsageCompleteCmdId = iCPM->UsageComplete(iCPMSessionID, iUsageID);
}

void PVMFAMRFFParserNode::CloseCPMSession()
{
    iCPMCloseSessionCmdId = iCPM->CloseSession(iCPMSessionID);
}

void PVMFAMRFFParserNode::ResetCPM()
{
    iCPMResetCmdId = iCPM->Reset();
}

PVMFStatus
PVMFAMRFFParserNode::CheckCPMCommandCompleteStatus(PVMFCommandId aID,
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
            PVMF_AMRPARSERNODE_LOGINFO((0, "PVMFAMRFFParserNode::CPMCommandCompleted -  CPM Reset Failed"));
            OSCL_ASSERT(false);
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

void PVMFAMRFFParserNode::CPMCommandCompleted(const PVMFCmdResp& aResponse)
{
    PVMFCommandId id = aResponse.GetCmdId();
    PVMFStatus status = CheckCPMCommandCompleteStatus(id, aResponse.GetCmdStatus());

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
        /* Unsupported format - Treat it like unprotected content */
        PVMF_AMRPARSERNODE_LOGINFO((0, "PVMFAMRParserNode::CPMCommandCompleted - Unknown CPM Format - Ignoring CPM"));
        if (CheckForAMRHeaderAvailability() == PVMFSuccess)
        {
            if (ParseAMRFile())
            {
                /* End of Node Init sequence. */
                CompleteInit();
            }
        }
        return;
    }

    if (status != PVMFSuccess)
    {
        /*
         * If any command fails, the sequence fails.
         */
        CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
    }
    else
    {
        /*
         * process the response, and issue the next command in
         * the sequence.
         */

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
            iCPMContentType = iCPM->GetCPMContentType(iCPMSessionID);

            if ((iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS)
                    || (iCPMContentType == PVMF_CPM_FORMAT_OMA1))
            {
                RequestUsage();
            }
            else
            {
                /* Unsupported format - Treat it like unprotected content */
                PVMF_AMRPARSERNODE_LOGINFO((0, "PVMFAMRParserNode::CPMCommandCompleted - Unknown CPM Format - Ignoring CPM"));
                if (CheckForAMRHeaderAvailability() == PVMFSuccess)
                {
                    if (ParseAMRFile())
                    {
                        CompleteInit();
                    }
                }
            }
        }
        else if (id == iCPMRequestUsageId)
        {
            oSourceIsCurrent = false;
            if ((iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS)
                    || (iCPMContentType == PVMF_CPM_FORMAT_OMA1))
            {
                GetCPMContentAccessFactory();
                if (CheckForAMRHeaderAvailability() == PVMFSuccess)
                {
                    if (ParseAMRFile())
                    {
                        CompleteInit();
                    }
                }
            }
            else
            {
                CompleteInit();
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
            /* End of Node Reset sequence */
            OSCL_ASSERT(PVMF_GENERIC_NODE_RESET == iCurrentCommand.iCmd);
            CompleteReset();
            CommandComplete(iCurrentCommand, PVMFSuccess);
        }
        else if (id == iCPMGetMetaDataValuesCmdId)
        {
            /* End of GetNodeMetaDataValues */
            OSCL_ASSERT(PVMF_GENERIC_NODE_GETNODEMETADATAVALUES == iCurrentCommand.iCmd);
            CompleteGetMetaDataValues();
        }
        else
        {
            /* Unknown cmd - error */
            CommandComplete(iCurrentCommand, PVMFFailure);
        }
    }
}

void PVMFAMRFFParserNode::PassDatastreamFactory(PVMFDataStreamFactory& aFactory,
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

void
PVMFAMRFFParserNode::PassDatastreamReadCapacityObserver(PVMFDataStreamReadCapacityObserver* aObserver)
{
    iDataStreamReadCapacityObserver = aObserver;
}

void PVMFAMRFFParserNode::CompleteInit()
{
    PVMF_AMRPARSERNODE_LOGSTACKTRACE((0, "PVMFAMRParserNode::CompleteInit() Called"));
    OSCL_ASSERT(PVMF_GENERIC_NODE_INIT == iCurrentCommand.iCmd);
    if (iCPM)
    {

        if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
                (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
        {
            if (iApprovedUsage.value.uint32_value !=
                    iRequestedUsage.value.uint32_value)
            {
                CommandComplete(iCurrentCommand, PVMFErrAccessDenied);
                return;
            }
        }
    }
    CommandComplete(iCurrentCommand, PVMFSuccess);
    return;
}

void PVMFAMRFFParserNode::CompleteGetMetaDataValues()
{
    CommandComplete(iCurrentCommand, PVMFSuccess);
}

void PVMFAMRFFParserNode::setFileSize(const TOsclFileOffset aFileSize)
{
    iDownloadFileSize = (uint32)aFileSize;
}

int32 PVMFAMRFFParserNode::convertSizeToTime(TOsclFileOffset aFileSize, uint32& aNPTInMS)
{
    OSCL_UNUSED_ARG(aFileSize);
    OSCL_UNUSED_ARG(aNPTInMS);
    return -1;
}

void PVMFAMRFFParserNode::setDownloadProgressInterface(PVMFDownloadProgressInterface* aInterface)
{
    if (aInterface == NULL)
    {
        OSCL_ASSERT(false);
    }
    iDownloadProgressInterface = aInterface;
}

void PVMFAMRFFParserNode::DataStreamInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_LEAVE(OsclErrNotSupported);
}

void PVMFAMRFFParserNode::DataStreamErrorEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_LEAVE(OsclErrNotSupported);
}

void PVMFAMRFFParserNode::DataStreamCommandCompleted(const PVMFCmdResp& aResponse)
{
    if (aResponse.GetCmdId() == iRequestReadCapacityNotificationID)
    {
        PVMFStatus cmdStatus = aResponse.GetCmdStatus();
        if (cmdStatus == PVMFSuccess)
        {
            if (CheckForAMRHeaderAvailability() == PVMFSuccess)
            {
                if (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS)
                {
                    if (ParseAMRFile())
                    {
                        {
                            /* End of Node Init sequence. */
                            CompleteInit();
                        }
                    }
                }
                else
                {
                    if (ParseAMRFile())
                    {
                        /* End of Node Init sequence. */
                        CompleteInit();
                    }
                }
            }
        }
        else
        {
            PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::DataStreamCommandCompleted() Failed %d", cmdStatus));
            CommandComplete(iCurrentCommand, PVMFErrResource);

        }
    }
    else
    {
        OSCL_ASSERT(false);
    }
}

void PVMFAMRFFParserNode::playResumeNotification(bool aDownloadComplete)
{
    iAutoPaused = false;
    iDownloadComplete = aDownloadComplete;
    if (!iTrack.iPort)
    {
        PVMF_AMRPARSERNODE_LOGERROR((0, "PVAMRParserNode::playResumeNotification: Error - no port assigned for track"));
        return;
    }
    PVAMRFFNodeTrackPortInfo* trackInfoPtr = &iTrack;

    if (trackInfoPtr->oQueueOutgoingMessages == false)
    {
        trackInfoPtr->oQueueOutgoingMessages = true;
    }

    PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::playResumeNotification() - Auto Resume Triggered - FileSize = %d, NPT = %d isDownloadComplete [%d]", iFileSizeLastConvertedToTime, iLastNPTCalcInConvertSizeToTime, iDownloadComplete));
    PVMF_AMRPARSERNODE_LOGDATATRAFFIC((0, "PVMFAMRParserNode::playResumeNotification() - Auto Resume Triggered - FileSize = %d, NPT = %d", iFileSizeLastConvertedToTime, iLastNPTCalcInConvertSizeToTime));
    RunIfNotReady();
}

void PVMFAMRFFParserNode::CompleteReset()
{
    PVMF_AMRPARSERNODE_LOGSTACKTRACE((0, "PVMFAMRParserNode::CompleteReset() Called"));
    /* stop and cleanup */
    ReleaseAllPorts();
    CleanupFileSource();
    return;
}

PVMFStatus PVMFAMRFFParserNode::QueueMediaSample(PVAMRFFNodeTrackPortInfo* aTrackInfoPtr)
{
    if (iAutoPaused == true)
    {
        aTrackInfoPtr->oQueueOutgoingMessages = false;
        PVMF_AMRPARSERNODE_LOGDATATRAFFIC((0, "PVMFAMRParserNode::QueueMediaSample() - Auto Paused"));
        return PVMFErrBusy;
    }
    if (aTrackInfoPtr->iPort->IsOutgoingQueueBusy())
    {
        aTrackInfoPtr->oQueueOutgoingMessages = false;
        PVMF_AMRPARSERNODE_LOGDATATRAFFIC((0, "PVMFAMRParserNode::QueueMediaSample() Port Outgoing Queue Busy"));
        return PVMFErrBusy;
    }
    if (aTrackInfoPtr->oQueueOutgoingMessages)
    {
        PVMFStatus status;
        if (aTrackInfoPtr->iSendBOS == true)
        {
            uint32 timestamp = Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp);
            status = SendBeginOfMediaStreamCommand(aTrackInfoPtr->iPort, iStreamID, timestamp);
            if (status == PVMFSuccess)
            {
                aTrackInfoPtr->iSendBOS = false;
                aTrackInfoPtr->oProcessOutgoingMessages = true;
            }
            return status;

        }

        if (aTrackInfoPtr->oEOSReached == false)
        {
            PVMFSharedMediaDataPtr mediaDataOut;
            status = RetrieveMediaSample(aTrackInfoPtr, mediaDataOut);
            if (status == PVMFErrBusy)
            {
                PVMF_AMRPARSERNODE_LOGINFO((0, "PVMFAMRParserNode::QueueMediaSample() RetrieveMediaSample - Mem Pools Busy"));
                aTrackInfoPtr->oQueueOutgoingMessages = false;
                if (iAutoPaused == true)
                {
                    if (!iTrack.iPort)
                    {
                        PVMF_AMRPARSERNODE_LOGERROR((0, "PVAMRParserNode::QueueMediaSample: Error - no port assigned for track"));
                        status = PVMFFailure;
                    }
                    else
                    {
                        PVAMRFFNodeTrackPortInfo* trackInfoPtr = &iTrack;
                        trackInfoPtr->oQueueOutgoingMessages = false;
                    }

                }
                return status;
            }
            else if (status == PVMFSuccess)
            {
                if (aTrackInfoPtr->oEOSReached == false)
                {
                    mediaDataOut->setStreamID(iStreamID);
                    PVMFSharedMediaMsgPtr msgOut;
                    convertToPVMFMediaMsg(msgOut, mediaDataOut);

                    /* For logging purposes */
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

                    status = aTrackInfoPtr->iPort->QueueOutgoingMsg(msgOut);

                    if (status != PVMFSuccess)
                    {
                        PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::QueueMediaSample: Error - QueueOutgoingMsg failed"));
                        ReportErrorEvent(PVMFErrPortProcessing);
                    }
                    /* This flag will get reset to false if the connected port is busy */
                    aTrackInfoPtr->oProcessOutgoingMessages = true;
                    return status;
                }
            }
            else
            {
                PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::QueueMediaSample() - Sample Retrieval Failed"));
                ReportErrorEvent(PVMFErrCorrupt);
                return PVMFFailure;
            }
        }
        else
        {
            if (aTrackInfoPtr->iPort->IsOutgoingQueueBusy() == true)
            {
                /* come back later */
                PVMF_AMRPARSERNODE_LOGDATATRAFFIC((0, "PVMFAMRParserNode::QueueMediaSample: Waiting - Output Queue Busy"));
                return PVMFErrBusy;
            }

            if ((aTrackInfoPtr->oEOSSent == false) && (aTrackInfoPtr->oEOSReached == true))
            {
                uint32 timestamp = Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp);
                if (SendEndOfTrackCommand(aTrackInfoPtr->iPort, iStreamID, timestamp, aTrackInfoPtr->iSeqNum))
                {
                    aTrackInfoPtr->oEOSSent = true;
                    aTrackInfoPtr->oQueueOutgoingMessages = false;
                    aTrackInfoPtr->oProcessOutgoingMessages = true;
                    return PVMFSuccess;
                }
            }
            aTrackInfoPtr->oQueueOutgoingMessages = false;
            return PVMFFailure;
        }
    }
    return PVMFSuccess;
}


PVMFStatus PVMFAMRFFParserNode::RetrieveMediaSample(PVAMRFFNodeTrackPortInfo* aTrackInfoPtr,
        PVMFSharedMediaDataPtr& aMediaDataOut)
{

    // Create a data buffer from pool
    int errcode = OsclErrNoResources;
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImplOut;
    mediaDataImplOut = aTrackInfoPtr->iResizableSimpleMediaMsgAlloc->allocate(MAXTRACKDATASIZE);

    if (mediaDataImplOut.GetRep() == NULL)
    {
        OsclMemPoolResizableAllocatorObserver* resizableAllocObs =
            OSCL_STATIC_CAST(OsclMemPoolResizableAllocatorObserver*, aTrackInfoPtr);
        aTrackInfoPtr->iResizableDataMemoryPool->notifyfreeblockavailable(*resizableAllocObs);
        return PVMFErrBusy;
    }

    // Now create a PVMF media data from pool
    errcode = OsclErrNoResources;
    aMediaDataOut = PVMFMediaData::createMediaData(mediaDataImplOut, aTrackInfoPtr->iMediaDataMemPool);

    if (aMediaDataOut.GetRep() == NULL)
    {
        OsclMemPoolFixedChunkAllocatorObserver* fixedChunkObs =
            OSCL_STATIC_CAST(OsclMemPoolFixedChunkAllocatorObserver*, aTrackInfoPtr);
        aTrackInfoPtr->iMediaDataMemPool->notifyfreechunkavailable(*fixedChunkObs);
        return PVMFErrBusy;
    }

    // Retrieve memory fragment to write to
    OsclRefCounterMemFrag refCtrMemFragOut;
    OsclMemoryFragment memFragOut;
    aMediaDataOut->getMediaFragment(0, refCtrMemFragOut);
    memFragOut.ptr = refCtrMemFragOut.getMemFrag().ptr;

    Oscl_Vector<uint32, OsclMemAllocator> payloadSizeVec;

    uint32 numsamples = NUM_AMR_FRAMES;
    // Set up the GAU structure
    GAU gau;
    gau.numMediaSamples = numsamples;
    gau.buf.num_fragments = 1;
    gau.buf.buf_states[0] = NULL;
    gau.buf.fragments[0].ptr = refCtrMemFragOut.getMemFrag().ptr;
    gau.buf.fragments[0].len = refCtrMemFragOut.getCapacity();

    int32 retval = iAMRParser->GetNextBundledAccessUnits(&numsamples, &gau);
    uint32 actualdatasize = 0;
    for (uint32 i = 0; i < numsamples; ++i)
    {
        actualdatasize += gau.info[i].len;
    }

    if (retval == bitstreamObject::EVERYTHING_OK)
    {
        memFragOut.len = actualdatasize;

        // Set Actual size
        aMediaDataOut->setMediaFragFilledLen(0, actualdatasize);

        // Resize memory fragment
        aTrackInfoPtr->iResizableSimpleMediaMsgAlloc->ResizeMemoryFragment(mediaDataImplOut);


        // set current timestamp to media msg.
        aTrackInfoPtr->iClockConverter->update_clock(Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp));

        uint32 ts32 = Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp);

        aMediaDataOut->setSeqNum(aTrackInfoPtr->iSeqNum);
        aMediaDataOut->setTimestamp(ts32);

        // update ts by adding the data samples
        aTrackInfoPtr->iContinuousTimeStamp += numsamples * AMR_SAMPLE_DURATION; // i.e 20ms


        if (aTrackInfoPtr->iSeqNum == 0)
        {
            aMediaDataOut->setFormatSpecificInfo(aTrackInfoPtr->iFormatSpecificConfig);
        }
        aTrackInfoPtr->iSeqNum += 1;

        // Set M bit to 1 always
        uint32 markerInfo = 0;
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;

        // Reset random access point if first frame after repositioning
        if (aTrackInfoPtr->iFirstFrame)
        {
            markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT;
            aTrackInfoPtr->iFirstFrame = false;
        }
        mediaDataImplOut->setMarkerInfo(markerInfo);
    }
    else if (retval == bitstreamObject::DATA_INSUFFICIENT)
    {
        payloadSizeVec.clear();
        if (iDownloadProgressInterface != NULL)
        {
            if (iDownloadComplete)
            {
                aTrackInfoPtr->oEOSReached = true;
                return PVMFSuccess;
            }
            iDownloadProgressInterface->requestResumeNotification((uint32)aTrackInfoPtr->iContinuousTimeStamp,
                    iDownloadComplete);
            iAutoPaused = true;
            PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::RetrieveMediaSample() - Auto Pause Triggered - TS=%d", aTrackInfoPtr->iContinuousTimeStamp));
            PVMF_AMRPARSERNODE_LOGDATATRAFFIC((0, "PVMFAMRParserNode::RetrieveMediaSample() - Auto Pause Triggered - TS=%d", aTrackInfoPtr->iContinuousTimeStamp));
            return PVMFErrBusy;
        }
        else
        {
            // if we recieve Insufficient data for local playback from parser library that means
            // its end of track, so change track state to send end of track.
            aTrackInfoPtr->oEOSReached = true;
        }
    }
    else if (retval == bitstreamObject::END_OF_FILE)
    {
        aTrackInfoPtr->oEOSReached = true;
    }
    else
    {
        PVMF_AMRPARSERNODE_LOGERROR((0, "PVMFAMRParserNode::RetrieveMediaSample() - Sample Retrieval Failed"));
        return PVMFFailure;
    }

    return PVMFSuccess;
}

bool PVMFAMRFFParserNode::RetrieveTrackData(PVAMRFFNodeTrackPortInfo& aTrackPortInfo)
{
    // Create a data buffer from pool
    int errcode = 0;
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImplOut;
    OSCL_TRY(errcode, mediaDataImplOut = aTrackPortInfo.iMediaDataImplAlloc->allocate(MAXTRACKDATASIZE));

    if (errcode != 0)
    {
        if (errcode == OsclErrNoResources)
        {
            aTrackPortInfo.iTrackDataMemoryPool->notifyfreechunkavailable(aTrackPortInfo);  // Enable flag to receive event when next deallocate() is called on pool
            return false;
        }
        else if (errcode == OsclErrNoMemory)
        {
            // Memory allocation for the pool failed
            ReportErrorEvent(PVMFErrNoMemory, NULL);
            return false;
        }
        else if (errcode == OsclErrArgument)
        {
            // Invalid parameters passed to mempool
            ReportErrorEvent(PVMFErrArgument, NULL);
            return false;
        }
        else
        {
            // General error
            ReportErrorEvent(PVMFFailure, NULL);
            return false;
        }

    }

    // Now create a PVMF media data from pool
    errcode = OsclErrNoResources;
    PVMFSharedMediaDataPtr mediadataout;
    mediadataout = PVMFMediaData::createMediaData(mediaDataImplOut, aTrackPortInfo.iMediaDataMemPool);

    if (mediadataout.GetRep() != NULL)
    {
        errcode = OsclErrNone;
    }
    else
    {
        aTrackPortInfo.iMediaDataMemPool->notifyfreechunkavailable(aTrackPortInfo);     // Enable flag to receive event when next deallocate() is called on pool
        return false;
    }

    // Set the random access point flag if first frame
    if (aTrackPortInfo.iFirstFrame == true)
    {
        uint32 markerInfo = 0;
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT;
        mediaDataImplOut->setMarkerInfo(markerInfo);
        aTrackPortInfo.iFirstFrame = false;
    }

    // Retrieve memory fragment to write to
    OsclRefCounterMemFrag refCtrMemFragOut;
    mediadataout->getMediaFragment(0, refCtrMemFragOut);

    // Retrieve one bundle of samples from the file format parser
    // Temporary retrieve 32 frames since AMR MDF needs 32 frames
    uint32 numsamples = NUM_AMR_FRAMES;

    // Set up the GAU structure
    GAU gau;
    gau.numMediaSamples = numsamples;
    gau.buf.num_fragments = 1;
    gau.buf.buf_states[0] = NULL;
    gau.buf.fragments[0].ptr = refCtrMemFragOut.getMemFrag().ptr;
    gau.buf.fragments[0].len = refCtrMemFragOut.getCapacity();

    int32 retval = iAMRParser->GetNextBundledAccessUnits(&numsamples, &gau);

    // Determine actual size of the retrieved data by summing each sample length in GAU
    uint32 actualdatasize = 0;
    for (uint32 i = 0; i < numsamples; ++i)
    {
        actualdatasize += gau.info[i].len;
    }

    if (retval == bitstreamObject::EVERYTHING_OK)
    {
        // Set buffer size
        mediadataout->setMediaFragFilledLen(0, actualdatasize);

        // Save the media data in the trackport info
        aTrackPortInfo.iMediaData = mediadataout;

        // Retrieve timestamp and convert to milliseconds


        aTrackPortInfo.iClockConverter->update_clock(Oscl_Int64_Utils::get_uint64_lower32(aTrackPortInfo.iContinuousTimeStamp));

        uint32 timestamp = Oscl_Int64_Utils::get_uint64_lower32(aTrackPortInfo.iContinuousTimeStamp);
        // Set the media data's timestamp
        aTrackPortInfo.iMediaData->setTimestamp(timestamp);


        // compute "next" ts based on the duration of the samples that we obtained
        aTrackPortInfo.iContinuousTimeStamp += numsamples * AMR_SAMPLE_DURATION; // i.e 20ms

        // Set the sequence number
        aTrackPortInfo.iMediaData->setSeqNum(aTrackPortInfo.iSeqNum++);

        return true;
    }
    else if (retval == bitstreamObject::READ_ERROR)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::RetrieveTrackData() AMR Parser READ_ERROR"));
        PVUuid erruuid = PVMFFileFormatEventTypesUUID;
        PVMFBasicErrorInfoMessage* eventmsg;
        eventmsg = PVMF_BASE_NODE_NEW(PVMFBasicErrorInfoMessage,
                                      (PVMFFFErrFileRead, erruuid, NULL));
        PVMFAsyncEvent asyncevent(PVMFErrorEvent,
                                  PVMFErrResource,
                                  NULL,
                                  OSCL_STATIC_CAST(PVInterface*, eventmsg),
                                  NULL,
                                  NULL,
                                  0);
        ReportErrorEvent(asyncevent);
        eventmsg->removeRef();

        // Transition the node to an error state
        iInterfaceState = EPVMFNodeError;
        return false;
    }
    else if (retval == bitstreamObject::END_OF_FILE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::RetrieveTrackData() AMR Parser End Of File!"));
        uint32 timestamp = Oscl_Int64_Utils::get_uint64_lower32(aTrackPortInfo.iContinuousTimeStamp);
        if (SendEndOfTrackCommand(aTrackPortInfo.iPort, iStreamID, timestamp, aTrackPortInfo.iSeqNum))
        {
            // EOS message sent so change state
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFAMRFFParserNode::RetrieveTrackData() Sending EOS message succeeded"));
            return false;
        }
        else
        {
            // EOS message could not be queued so keep in same state and try again later
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFAMRFFParserNode::RetrieveTrackData() Sending EOS message failed"));
            return true;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFAMRFFParserNode::RetrieveTrackData() AMR Parser Unknown Error!"));
        PVUuid erruuid = PVMFFileFormatEventTypesUUID;
        PVMFBasicErrorInfoMessage* eventmsg;
        eventmsg = PVMF_BASE_NODE_NEW(PVMFBasicErrorInfoMessage,
                                      (PVMFFFErrInvalidData, erruuid, NULL));
        PVMFAsyncEvent asyncevent(PVMFErrorEvent,
                                  PVMFErrCorrupt,
                                  NULL,
                                  OSCL_STATIC_CAST(PVInterface*, eventmsg),
                                  NULL,
                                  NULL,
                                  0);
        ReportErrorEvent(asyncevent);
        eventmsg->removeRef();

        // Transition the node to an error state
        iInterfaceState = EPVMFNodeError;
        //ReportErrorEvent(PVMFErrCorrupt);
        return false;
    }
}

int32 PVMFAMRFFParserNode::PushBackKeyVal(Oscl_Vector<PvmiKvp, OsclMemAllocator>*& aValueListPtr, PvmiKvp &aKeyVal)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, (*aValueListPtr).push_back(aKeyVal));
    return leavecode;
}

PVMFStatus PVMFAMRFFParserNode::PushValueToList(Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> &aRefMetaDataKeys, PVMFMetadataList *&aKeyListPtr, uint32 aLcv)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aKeyListPtr->push_back(aRefMetaDataKeys[aLcv]));
    OSCL_FIRST_CATCH_ANY(leavecode, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFAMRFFParserNode::PushValueToList() Memory allocation failure when copying metadata key")); return PVMFErrNoMemory);
    return PVMFSuccess;
}

PVMFStatus PVMFAMRFFParserNode::CancelCurrentCommand()
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

