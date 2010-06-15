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
#include "pvmf_mp4ffparser_node.h"

#include "impeg4file.h"


#include "media_clock_converter.h"

#include "pv_mime_string_utils.h"

#include "oscl_snprintf.h"

#include "pvmf_duration_infomessage.h"

#include "pvmi_kvp_util.h"

#include "h263decoderspecificinfo.h"

#include "oscl_exclusive_ptr.h"

#include "getactualaacconfig.h"

#include "oscl_string_utils.h"

#define NUMMETADATAKEYS 64


/**
 * Macros for calling PVLogger
 */
#define LOGGAPLESSINFO(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iGaplessLogger,PVLOGMSG_INFO,m);


uint32 PVMFMP4FFParserNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetNumMetadataValues() called"));
    uint32 numvalentries = 0;

    if (iCPMMetaDataExtensionInterface != NULL)
    {
        numvalentries =
            iCPMMetaDataExtensionInterface->GetNumMetadataValues(aKeyList);
    }

    PVMFMetadataList* keylistptr = &aKeyList;
    if (aKeyList.size() == 1)
    {
        if (oscl_strncmp(aKeyList[0].get_cstr(),
                         PVMP4_ALL_METADATA_KEY,
                         oscl_strlen(PVMP4_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &(iClipInfoList[iClipIndexForMetadata].iAvailableMetadataKeys);
        }
    }
    if (iMetadataParserObj)
    {
        numvalentries += iMetadataParserObj->GetNumMetadataValues(*keylistptr);
    }

    return numvalentries;
}

PVMFStatus PVMFMP4FFParserNode::SetMetadataClipIndex(uint32 aClipNum)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetMetadataClipIndex() called clip index=%d", aClipNum));

    iClipIndexForMetadata = aClipNum;
    iMetadataParserObj = GetParserObjAtIndex(aClipNum);
    if (iMetadataParserObj)
        return PVMFSuccess;
    iMetadataParserObj = NULL;
    return PVMFFailure;
}


PVMFCommandId PVMFMP4FFParserNode::GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 starting_index, int32 max_entries, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetNodeMetadataValue() called"));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_GETNODEMETADATAVALUES, aKeyList, aValueList, starting_index, max_entries, aContext);
    return QueueCommandL(cmd);
}


PVMFStatus PVMFMP4FFParserNode::ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
        uint32 start,
        uint32 end)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::ReleaseNodeMetadataValues() called"));

    if (iMetadataParserObj == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFMP4FFParserNode::ReleaseNodeMetadataValues() \
                       MP4 file not parsed yet"));
        return PVMFFailure;
    }

    if (iCPMMetaDataExtensionInterface != NULL)
    {
        PVMFStatus status = iCPMMetaDataExtensionInterface->ReleaseNodeMetadataValues(aValueList, start, end);
        if (status != PVMFSuccess)
        {
            return status;
        }
    }

    end = OSCL_MIN(aValueList.size(), iClipInfoList[iClipIndexForMetadata].iMetadataValueCount);

    if (start > end || aValueList.size() == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFMP4FFParserNode::ReleaseNodeMetadataValues() \
                        Invalid start/end index"));
        return PVMFErrArgument;
    }

    //First few entries in value list have ID3 specific data. We do not have to release that data
    //as it will be released by ID3Parcom. Hence we need to modify the start value if it is less than
    //iTotalID3MetaDataTagInValueList
    if (start < iClipInfoList[iClipIndexForMetadata].iTotalID3MetaDataTagInValueList)
    {
        start = iClipInfoList[iClipIndexForMetadata].iTotalID3MetaDataTagInValueList;
    }

    // Go through the specified values and free it
    for (uint32 i = start; i < end; i++)
    {
        iMetadataParserObj->ReleaseMetadataValue(aValueList[i]);
    }
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::DoGetNodeMetadataValues()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode::DoGetMetadataValues() In"));

    PVMFMetadataList* keylistptr_in = NULL;
    PVMFMetadataList* keylistptr = NULL;
    PVMFMetadataList completeKeyList;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    int32 numentriesadded = 0;
    uint32 numvalentries = 0;

    // Extract parameters from command structure
    iCurrentCommand.PVMFNodeCommand::Parse(keylistptr_in,
                                           valuelistptr,
                                           starting_index,
                                           max_entries);

    if (iMetadataParserObj == NULL || keylistptr_in == NULL || valuelistptr == NULL)
    {
        // The list pointer is invalid, or we cannot access the mp4 ff library.
        return PVMFFailure;
    }

    keylistptr = keylistptr_in;
    //If numkeys is one, just check to see if the request
    //is for ALL metadata
    if (keylistptr_in->size() == 1)
    {
        if (oscl_strncmp((*keylistptr)[0].get_cstr(),
                         PVMP4_ALL_METADATA_KEY,
                         oscl_strlen(PVMP4_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &iClipInfoList[iClipIndexForMetadata].iAvailableMetadataKeys;
        }
    }

    uint32 numKeys = keylistptr->size();
    // The underlying mp4 ff library will fill in the values.
    iClipInfoList[iClipIndexForMetadata].iTotalID3MetaDataTagInValueList = 0;
    PVMFStatus status = iMetadataParserObj->GetMetadataValues(*keylistptr, *valuelistptr,
                        starting_index, max_entries,
                        numentriesadded,
                        iClipInfoList[iClipIndexForMetadata].iTotalID3MetaDataTagInValueList);

    numvalentries = numvalentries + numentriesadded;

    if (numvalentries >= (uint32)max_entries)
    {
        //If required number of entries have already been added
        iClipInfoList[iClipIndexForMetadata].iMetadataValueCount = (*valuelistptr).size();
        return PVMFSuccess;
    }

    uint32 lcv = 0;
    // Retrieve the track ID list.
    OsclExclusiveArrayPtr<uint32> trackidlistexclusiveptr;
    uint32* trackidlist = NULL;
    uint32 numTracks = (uint32)(iMetadataParserObj->getNumTracks());
    status = CreateNewArray(&trackidlist, numTracks);
    if (PVMFErrNoMemory == status)
    {
        return PVMFErrNoMemory;
    }
    oscl_memset(trackidlist, 0, sizeof(uint32)*(numTracks));
    iMetadataParserObj->getTrackIDList(trackidlist, numTracks);
    trackidlistexclusiveptr.set(trackidlist);

    for (lcv = 0; lcv < numKeys; lcv++)
    {
        int32 leavecode = 0;
        PvmiKvp KeyVal;
        KeyVal.key = NULL;
        KeyVal.value.pWChar_value = NULL;
        KeyVal.value.pChar_value = NULL;

        bool IsMetadataValAddedBefore = false;

        if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_SELECTED_KEY) != NULL)
        {

            // Track selected info

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMetadataParserObj->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                PVMFStatus retval = PVMFErrArgument;

                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                if (numvalentries > starting_index)
                {
                    char indexparam[16];
                    oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                    indexparam[15] = '\0';

                    // Check if the track has been selected by looking up
                    // the current index's track ID in the NodeTrackPort vector
                    bool trackselected = false;
                    for (uint32 j = 0; j < iNodeTrackPortList.size(); ++j)
                    {
                        if ((uint32)iNodeTrackPortList[j].iTrackId == trackidlist[i])
                        {
                            trackselected = true;
                            break;
                        }
                    }
                    retval = PVMFCreateKVPUtils::CreateKVPForBoolValue(trackkvp, PVMP4METADATA_TRACKINFO_SELECTED_KEY, trackselected, indexparam);

                    if (retval != PVMFSuccess && retval != PVMFErrArgument)
                    {
                        break;
                    }

                    if (trackkvp.key != NULL)
                    {
                        leavecode = AddToValueList(*valuelistptr, trackkvp);
                        if (leavecode != 0)
                        {
                            OSCL_ARRAY_DELETE(trackkvp.key);
                            trackkvp.key = NULL;
                        }
                        else
                        {
                            // Increment the value list entry counter
                            ++numentriesadded;
                            IsMetadataValAddedBefore = true;
                        }

                        // Check if the max number of value entries were added
                        if (max_entries > 0 && numentriesadded >= max_entries)
                        {
                            iClipInfoList[iClipIndexForMetadata].iMetadataValueCount = (*valuelistptr).size();
                            return PVMFSuccess;
                        }
                    }
                }
            }
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_RANDOM_ACCESS_DENIED_KEY) == 0)
        {
            /*
             * Random Access
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;

            /* Create a value entry if past the starting index */
            if (numvalentries > (uint32)starting_index)
            {
                uint64 duration64 = iMetadataParserObj->getMovieDuration();
                uint32 duration = Oscl_Int64_Utils::get_uint64_lower32(duration64);
                bool random_access_denied = false;
                if (duration > 0)
                {
                    random_access_denied = false;
                }
                else
                {
                    random_access_denied = true;
                }

                if (iMetadataParserObj->IsMovieFragmentsPresent())
                {
                    uint32* trackList = NULL;
                    uint32 numTracks = iNodeTrackPortList.size();
                    CreateNewArray(&trackList, numTracks);
                    if (trackList)
                    {
                        for (uint32 i = 0; i < iNodeTrackPortList.size(); i++)
                        {
                            // Save the track list while in this loop
                            trackList[i] = iNodeTrackPortList[i].iTrackId;
                        }

                        if (!iMetadataParserObj->IsTFRAPresentForAllTrack(numTracks, trackList))
                            random_access_denied = true;

                        OSCL_ARRAY_DELETE(trackList);
                    }
                }

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                            PVMP4METADATA_RANDOM_ACCESS_DENIED_KEY,
                            random_access_denied,
                            NULL);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_CLIP_TYPE_KEY) == 0)
        {
            // clip-type
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 len = 0;
                char* clipType = NULL;
                if (download_progress_interface != NULL)
                {
                    len = oscl_strlen("download");
                    clipType = OSCL_ARRAY_NEW(char, len + 1);
                    oscl_memset(clipType, 0, len + 1);
                    oscl_strncpy(clipType, ("download"), len);
                }
                else
                {
                    len = oscl_strlen("local");
                    clipType = OSCL_ARRAY_NEW(char, len + 1);
                    oscl_memset(clipType, 0, len + 1);
                    oscl_strncpy(clipType, ("local"), len);
                }

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                            PVMP4METADATA_CLIP_TYPE_KEY,
                            clipType);

                OSCL_ARRAY_DELETE(clipType);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_WIDTH_KEY) != NULL)
        {
            // Video track width

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMetadataParserObj->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMetadataParserObj->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_VISUAL)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 trackwidth = (uint32)(FindVideoDisplayWidth(trackidlist[i]));
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_VIDEO_WIDTH_KEY, trackwidth, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {
                                iClipInfoList[iClipIndexForMetadata].iMetadataValueCount = (*valuelistptr).size();
                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_HEIGHT_KEY) != NULL)
        {
            // Video track height

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMetadataParserObj->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMetadataParserObj->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_VISUAL)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 trackheight = (uint32)(FindVideoDisplayHeight(trackidlist[i]));
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_VIDEO_HEIGHT_KEY, trackheight, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {
                                iClipInfoList[iClipIndexForMetadata].iMetadataValueCount = (*valuelistptr).size();
                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_SAMPLERATE_KEY) != NULL)
        {
            // Sampling rate (only for video tracks)

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMetadataParserObj->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMetadataParserObj->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_AUDIO)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 samplerate = GetAudioSampleRate(trackidlist[i]);
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_SAMPLERATE_KEY, samplerate, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {
                                iClipInfoList[iClipIndexForMetadata].iMetadataValueCount = (*valuelistptr).size();
                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_NUMCHANNELS_KEY) != NULL)
        {
            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMetadataParserObj->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMetadataParserObj->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_AUDIO)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 numAudioChannels = (GetNumAudioChannels(trackidlist[i]));
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_AUDIO_NUMCHANNELS_KEY, numAudioChannels, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {
                                iClipInfoList[iClipIndexForMetadata].iMetadataValueCount = (*valuelistptr).size();
                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_BITS_PER_SAMPLE_KEY) != NULL)
        {
            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMetadataParserObj->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMetadataParserObj->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_AUDIO)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 numbitspersample = (GetAudioBitsPerSample(trackidlist[i]));
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_AUDIO_BITS_PER_SAMPLE_KEY, numbitspersample, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {
                                iClipInfoList[iClipIndexForMetadata].iMetadataValueCount = (*valuelistptr).size();
                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }

        // Add the KVP to the list if the key string was created
        if ((KeyVal.key != NULL) && (!IsMetadataValAddedBefore))
        {
            leavecode = AddToValueList(*valuelistptr, KeyVal);
            if (leavecode != 0)
            {
                switch (GetValTypeFromKeyString(KeyVal.key))
                {
                    case PVMI_KVPVALTYPE_CHARPTR:
                        if (KeyVal.value.pChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                            KeyVal.value.pChar_value = NULL;
                        }
                        break;

                    case PVMI_KVPVALTYPE_WCHARPTR:
                        if (KeyVal.value.pWChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                            KeyVal.value.pWChar_value = NULL;
                        }
                        break;

                    default:
                        // Add more case statements if other value types are returned
                        break;
                }

                OSCL_ARRAY_DELETE(KeyVal.key);
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
                //return PVMFSuccess;
                break;
            }
        }
    }

    iClipInfoList[iClipIndexForMetadata].iMetadataValueCount = (*valuelistptr).size();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode::DoGetMetadataValues() iClipIndexForMetadata=%d iMetadataValueCount=%d iTotalID3MetaDataTagInValueList=%d",
                     iClipIndexForMetadata, iClipInfoList[iClipIndexForMetadata].iMetadataValueCount,
                     iClipInfoList[iClipIndexForMetadata].iTotalID3MetaDataTagInValueList));

    if (iCPMMetaDataExtensionInterface != NULL)
    {
        iCPMGetMetaDataValuesCmdId =
            (iCPMMetaDataExtensionInterface)->GetNodeMetadataValues(iCPMSessionID,
                    (*keylistptr_in),
                    (*valuelistptr),
                    0);
        return PVMFPending;
    }
    return status;
}



void PVMFMP4FFParserNode::CompleteGetMetaDataValues()
{
    PVMFMetadataList* keylistptr = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    iCurrentCommand.PVMFNodeCommand::Parse(keylistptr, valuelistptr, starting_index, max_entries);

    for (uint32 i = 0; i < (*valuelistptr).size(); i++)
    {
        PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CompleteGetMetaDataValues - Index=%d, Key=%s", i, (*valuelistptr)[i].key));
    }

    CommandComplete(iCurrentCommand,
                    PVMFSuccess);
}

int32 PVMFMP4FFParserNode::AddToValueList(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, PvmiKvp& aNewValue)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aValueList.push_back(aNewValue));
    return leavecode;
}


void PVMFMP4FFParserNode::CreateDurationInfoMsg(uint32 adurationms, uint32 &aClipIndex)
{
    int32 leavecode = 0;
    PVMFDurationInfoMessage* eventmsg = NULL;
    OSCL_TRY(leavecode, eventmsg = OSCL_NEW(PVMFDurationInfoMessage, (adurationms)));
    uint8 localbuffer[4];
    oscl_memcpy(localbuffer, &aClipIndex, sizeof(uint32));
    PVMFAsyncEvent asyncevent(PVMFInfoEvent, PVMFInfoDurationAvailable, NULL, OSCL_STATIC_CAST(PVInterface*, eventmsg), NULL, localbuffer, 4);
    ReportInfoEvent(asyncevent);
    if (eventmsg)
    {
        eventmsg->removeRef();
    }
}

PVMFStatus PVMFMP4FFParserNode::InitMetaData(uint32 aParserIndex)
{
    IMpeg4File* parserObj = GetParserObjAtIndex(aParserIndex);
    if (parserObj)
    {
        int32 leavecode = 0;
        OSCL_TRY(leavecode, iClipInfoList[aParserIndex].iAvailableMetadataKeys.reserve(NUMMETADATAKEYS));
        parserObj->InitMetaData(&iClipInfoList[aParserIndex].iAvailableMetadataKeys);
    }
    else
    {
        return PVMFFailure;
    }

    int32 iNumTracks = parserObj->getNumTracks();
    uint32 iIdList[16];

    if (iNumTracks != parserObj->getTrackIDList(iIdList, iNumTracks))
    {
        return PVMFFailure;
    }
    for (int32 i = iNumTracks - 1; i >= 0; i--)
    {
        uint32 trackID = iIdList[i];

        OSCL_HeapString<OsclMemAllocator> trackMIMEType;

        parserObj->getTrackMIMEType(trackID, (OSCL_String&)trackMIMEType);

        if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0))
        {
            if (PVMFSuccess == PopulateVideoDimensions(aParserIndex, trackID))
            {
                //track id is a one based index
                char indexparam[18];
                oscl_snprintf(indexparam, 18, ";index=%d", i);
                indexparam[17] = '\0';

                PushToAvailableMetadataKeysList(aParserIndex, PVMP4METADATA_TRACKINFO_VIDEO_WIDTH_KEY, indexparam);
                PushToAvailableMetadataKeysList(aParserIndex, PVMP4METADATA_TRACKINFO_VIDEO_HEIGHT_KEY, indexparam);
            }
        }

        else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR_IETF, oscl_strlen(PVMF_MIME_AMR_IETF)) == 0)
        {
            iClipInfoList[aParserIndex].iFormatTypeInteger = PVMF_MP4_PARSER_NODE_AMR_IETF;
        }
        else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMRWB_IETF, oscl_strlen(PVMF_MIME_AMRWB_IETF)) == 0)
        {
            iClipInfoList[aParserIndex].iFormatTypeInteger = PVMF_MP4_PARSER_NODE_AMRWB_IETF;
        }
        else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_MPEG4_AUDIO, oscl_strlen(PVMF_MIME_MPEG4_AUDIO)) == 0)
        {
            iClipInfoList[aParserIndex].iFormatTypeInteger = PVMF_MP4_PARSER_NODE_MPEG4_AUDIO;

            int32 specInfoSize = (int32)(parserObj->getTrackDecoderSpecificInfoSize(trackID));
            if (specInfoSize != 0)
            {
                // Retrieve the decoder specific info from file parser
                uint8* specInfoPtr = parserObj->getTrackDecoderSpecificInfoContent(trackID);

                GetActualAacConfig(specInfoPtr,
                                   &iClipInfoList[aParserIndex].iAACAudioObjectType,
                                   &specInfoSize,
                                   &iClipInfoList[aParserIndex].iAACSampleRateIndex,
                                   &iClipInfoList[aParserIndex].iAACNumChans,
                                   &iClipInfoList[aParserIndex].iAACSamplesPerFrame);
            }
        }
    } // end for

    if (parserObj->getMovieDuration() > (uint64)0)
    {
        // Intimate the Duration info available to the engine through Informational Event.
        uint64 duration64 = parserObj->getMovieDuration();
        uint32 durationms = 0;
        uint32 duration = durationms = Oscl_Int64_Utils::get_uint64_lower32(duration64);
        uint32 timescale = parserObj->getMovieTimescale();
        if (timescale > 0 && timescale != 1000)
        {
            // Convert to milliseconds
            MediaClockConverter mcc(timescale);
            mcc.set_clock(duration64, 0);
            duration = durationms = mcc.get_converted_ts(1000);
        }
        CreateDurationInfoMsg(durationms, aParserIndex);
    }

    //set clip duration on download progress interface
    //applicable to PDL sessions
    MediaClockConverter mcc(parserObj->getMovieTimescale());
    mcc.set_clock(parserObj->getMovieDuration(), 0);
    uint32 moviedurationInMS = mcc.get_converted_ts(1000);
    if ((download_progress_interface != NULL) && (moviedurationInMS != 0))
    {
        download_progress_interface->setClipDuration(OSCL_CONST_CAST(uint32, moviedurationInMS));
    }

    return PVMFSuccess;
}

void PVMFMP4FFParserNode::PushToAvailableMetadataKeysList(uint32 aParserIndex, const char* aKeystr, char* aOptionalParam)
{
    if (aKeystr == NULL)
    {
        return;
    }

    if (aOptionalParam)
    {
        iClipInfoList[aParserIndex].iAvailableMetadataKeys.push_front(aKeystr);
        iClipInfoList[aParserIndex].iAvailableMetadataKeys[0] += aOptionalParam;
    }

    else
    {
        iClipInfoList[aParserIndex].iAvailableMetadataKeys.push_front(aKeystr);
    }
}

PVMFStatus PVMFMP4FFParserNode::GetIndexParamValues(const char* aString, uint32& aStartIndex, uint32& aEndIndex)
{
    // This parses a string of the form "index=N1...N2" and extracts the integers N1 and N2.
    // If string is of the format "index=N1" then N2=N1

    if (aString == NULL)
    {
        return PVMFErrArgument;
    }

    // Go to end of "index="
    char* n1string = (char*)aString + 6;

    PV_atoi(n1string, 'd', oscl_strlen(n1string), aStartIndex);

    const char* n2string = oscl_strstr(aString, _STRLIT_CHAR("..."));

    if (n2string == NULL)
    {
        aEndIndex = aStartIndex;
    }
    else
    {
        // Go to end of "index=N1..."
        n2string += 3;

        PV_atoi(n2string, 'd', oscl_strlen(n2string), aEndIndex);
    }

    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::CreateNewArray(uint32** aTrackidList, uint32 aNumTracks)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, *aTrackidList = OSCL_ARRAY_NEW(uint32, aNumTracks););
    OSCL_FIRST_CATCH_ANY(leavecode, return PVMFErrNoMemory;);
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::PushValueToList(Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> &aRefMetaDataKeys, PVMFMetadataList *&aKeyListPtr, uint32 aLcv)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aKeyListPtr->push_back(aRefMetaDataKeys[aLcv]));
    OSCL_FIRST_CATCH_ANY(leavecode, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::PushValueToList() Memory allocation failure when copying metadata key")); return PVMFErrNoMemory);
    return PVMFSuccess;
}


void PVMFMP4FFParserNode::GetGaplessMetadata(int32 aClipIndex)
{
    // only do this for clips with a single AAC audio track
    if ((iNodeTrackPortList.size() != 1) || (iNodeTrackPortList[0].iFormatType != PVMF_MIME_MPEG4_AUDIO))
    {
        return;
    }

    // retrieve gapless metadata if present
    // do it only once per clip
    IMpeg4File* mp4File = iClipInfoList[aClipIndex].iParserObj;
    if (mp4File != NULL)
    {
        iClipInfoList[aClipIndex].iClipInfo.iGaplessInfoAvailable = mp4File->getITunesGaplessMetadata(iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata);

        if (iClipInfoList[aClipIndex].iClipInfo.iGaplessInfoAvailable)
        {
            // need to add the sample per frame and total frames here
            // AAC = 1024 samples per frame
            // AAC+ and EAAC+ = 2048 samples per frame
            uint32 totalFrames = mp4File->getSampleCountInTrack(iNodeTrackPortList[0].iTrackId);
            uint64 totalSamples = iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetZeroPadding() +
                                  iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetEncoderDelay() +
                                  iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetOriginalStreamLength();
            uint32 sfp = (uint32)(totalSamples / totalFrames);

            // total frames include the encoder delay and zero padding
            // we can probably find out the samples per frame from the decoder,
            // for now, this may be adequate
            iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.SetTotalFrames(totalFrames);
            iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.SetSamplesPerFrame(sfp);

            // check encoder delay
            uint32 delay = iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetEncoderDelay();
            if (0 != delay)
            {
                // frame numbers from the parser are 0 based
                iClipInfoList[aClipIndex].iClipInfo.iFrameBOC = 0;
                iClipInfoList[aClipIndex].iClipInfo.iSendBOC = true;

                // create format specific info for BOC
                // uint32 - number of samples to skip
                // allocate memory for BOC specific info and ref counter
                OsclMemoryFragment frag;
                frag.ptr = NULL;
                frag.len = sizeof(BOCInfo);
                uint refCounterSize = oscl_mem_aligned_size(sizeof(OsclRefCounterDA));
                uint8* memBuffer = (uint8*)iClipInfoList[aClipIndex].iClipInfo.iBOCFormatSpecificInfoAlloc.ALLOCATE(refCounterSize + frag.len);
                if (!memBuffer)
                {
                    // failure while allocating memory buffer
                    iClipInfoList[aClipIndex].iClipInfo.iSendBOC = false;
                }

                oscl_memset(memBuffer, 0, refCounterSize + frag.len);
                // create ref counter
                OsclRefCounter* refCounter = new(memBuffer) OsclRefCounterDA(memBuffer,
                        (OsclDestructDealloc*)&iClipInfoList[aClipIndex].iClipInfo.iBOCFormatSpecificInfoAlloc);
                memBuffer += refCounterSize;
                // create BOC info
                frag.ptr = (OsclAny*)(new(memBuffer) BOCInfo);
                ((BOCInfo*)frag.ptr)->samplesToSkip = delay;

                // store info in a ref counter memfrag
                // how do we make sure that we are not doing this more than once?
                iClipInfoList[aClipIndex].iClipInfo.iBOCFormatSpecificInfo = OsclRefCounterMemFrag(frag, refCounter, sizeof(struct BOCInfo));
            }

            // check zero padding
            uint32 padding = iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetZeroPadding();
            if (0 != padding)
            {
                // calculate frame number of the first frame of EOC
                uint32 spf = iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetSamplesPerFrame();
                if (0 != spf)
                {
                    uint64 total = iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetTotalFrames();
                    uint32 frames = padding / spf;
                    if (padding % spf)
                    {
                        frames++;
                    }
                    // frame numbers from the parser are 0 based
                    iClipInfoList[aClipIndex].iClipInfo.iFirstFrameEOC = total - frames;
                    iClipInfoList[aClipIndex].iClipInfo.iSendEOC = true;

                    // create format specific info for EOC
                    // uint32 - number of samples to skip
                    // allocate memory for BOC specific info and ref counter
                    OsclMemoryFragment frag;
                    frag.ptr = NULL;
                    frag.len = sizeof(EOCInfo);
                    uint refCounterSize = oscl_mem_aligned_size(sizeof(OsclRefCounterDA));
                    uint8* memBuffer = (uint8*)iClipInfoList[aClipIndex].iClipInfo.iBOCFormatSpecificInfoAlloc.ALLOCATE(refCounterSize + frag.len);
                    if (!memBuffer)
                    {
                        // failure while allocating memory buffer
                        iClipInfoList[aClipIndex].iClipInfo.iSendEOC = false;
                    }

                    oscl_memset(memBuffer, 0, refCounterSize + frag.len);
                    // create ref counter
                    OsclRefCounter* refCounter = new(memBuffer) OsclRefCounterDA(memBuffer,
                            (OsclDestructDealloc*)&iClipInfoList[aClipIndex].iClipInfo.iEOCFormatSpecificInfoAlloc);
                    memBuffer += refCounterSize;
                    // create EOC info
                    frag.ptr = (OsclAny*)(new(memBuffer) EOCInfo);
                    // but we don't know how many frames will be following this msg yet!!!
                    ((EOCInfo*)frag.ptr)->framesToFollow = 0;
                    ((EOCInfo*)frag.ptr)->samplesToSkip = padding;

                    // store info in a ref counter memfrag
                    // how do we make sure that we are not doing this more than once?
                    iClipInfoList[aClipIndex].iClipInfo.iEOCFormatSpecificInfo = OsclRefCounterMemFrag(frag, refCounter, sizeof(EOCInfo));
                }
            }

            // log the gapless metadata
            LOGGAPLESSINFO((0, "PVMFMP4FFParserNode::GetGaplessMetadata() encoder delay %d samples, zero padding %d samples, original length %d%d samples, samples per frame %d, total frames %d%d, part of gapless album %d",
                            delay, padding, (uint32)(iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetOriginalStreamLength() >> 32),
                            (uint32)(iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetOriginalStreamLength() & 0xFFFFFFFF),
                            iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetSamplesPerFrame(),
                            (uint32)(iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetTotalFrames() >> 32),
                            (uint32)(iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetTotalFrames() & 0xFFFFFFFF),
                            iClipInfoList[aClipIndex].iClipInfo.iGaplessMetadata.GetPartOfGaplessAlbum()));
        }
        else
        {
            LOGGAPLESSINFO((0, "PVMFMP4FFParserNode::GetGaplessMetadata() No gapless metadata found"));
        }
    }
}


// adjust for gapless playback duration
// return agjusted duration in milliseconds
// this method is not being called now, it may need some attention when is
uint32 PVMFMP4FFParserNode::GetGaplessDuration(PVMP4FFNodeTrackPortInfo& aInfo)
{
    if (iPlaybackParserObj == NULL)
    {
        return 0;
    }

    uint32 durationMs = 0;
    uint64 duration64 = iPlaybackParserObj->getTrackMediaDuration(aInfo.iTrackId);
    uint32 duration = durationMs = Oscl_Int64_Utils::get_uint64_lower32(duration64);
    uint32 timescale = iPlaybackParserObj->getTrackMediaTimescale(aInfo.iTrackId);
    if (timescale > 0 && timescale != 1000)
    {
        // Convert to milliseconds
        MediaClockConverter mcc(timescale);
        mcc.update_clock(duration);
        durationMs = mcc.get_converted_ts(1000);
    }

    if (iClipInfoList[iPlaybackClipIndex].iClipInfo.iGaplessInfoAvailable)
    {
        // subtract the encoder delay and zero padding
        uint32 droppedSamples = iClipInfoList[iPlaybackClipIndex].iClipInfo.iGaplessMetadata.GetEncoderDelay() +
                                iClipInfoList[iPlaybackClipIndex].iClipInfo.iGaplessMetadata.GetZeroPadding();
        uint32 samplingRate = GetAudioSampleRate(aInfo.iTrackId);
        uint32 droppedMs = 0;
        if (0 != samplingRate)
        {
            droppedMs = (droppedSamples * 1000) / samplingRate;
        }
        durationMs = (durationMs > droppedMs) ? (durationMs - droppedMs) : 0;
    }

    return durationMs;
}


uint32 PVMFMP4FFParserNode::GetNumAudioChannels(uint32 aId)
{
    uint32 num_channels = 0;
    uint8 audioObjectType;
    uint8 sampleRateIndex;
    uint32 samplesPerFrame;

    OSCL_HeapString<OsclMemAllocator> trackMIMEType;
    iMetadataParserObj->getTrackMIMEType(aId, trackMIMEType);

    if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR, oscl_strlen(PVMF_MIME_AMR)) == 0) ||
            (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR_IETF, oscl_strlen(PVMF_MIME_AMR_IETF)) == 0) ||
            (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMRWB_IETF, oscl_strlen(PVMF_MIME_AMRWB_IETF)) == 0))
    {
        //always mono
        num_channels = 1;
    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_MPEG4_AUDIO, oscl_strlen(PVMF_MIME_MPEG4_AUDIO)) == 0)
    {
        int32 specinfosize =
            (int32)(iMetadataParserObj->getTrackDecoderSpecificInfoSize(aId));
        if (specinfosize != 0)
        {
            // Retrieve the decoder specific info from file parser
            uint8* specinfoptr =
                iMetadataParserObj->getTrackDecoderSpecificInfoContent(aId);

            GetActualAacConfig(specinfoptr,
                               &audioObjectType,
                               &specinfosize,
                               &sampleRateIndex,
                               &num_channels,
                               &samplesPerFrame);
        }
    }

    return num_channels;
}

uint32 PVMFMP4FFParserNode::GetAudioSampleRate(uint32 aId)
{
    uint32 sample_rate = 0;
    uint32 num_channels;
    uint8 audioObjectType;
    uint8 sampleRateIndex;
    uint32 samplesPerFrame;

    const uint32 sample_freq_table[13] =
        {96000, 88200, 64000, 48000,
         44100, 32000, 24000, 22050,
         16000, 12000, 11025, 8000,
         7350
        };

    OSCL_HeapString<OsclMemAllocator> trackMIMEType;
    iMetadataParserObj->getTrackMIMEType(aId, trackMIMEType);

    if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR, oscl_strlen(PVMF_MIME_AMR)) == 0) ||
            (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR_IETF, oscl_strlen(PVMF_MIME_AMR_IETF)) == 0))
    {
        //always 8KHz
        sample_rate = 8000;
    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMRWB_IETF, oscl_strlen(PVMF_MIME_AMRWB_IETF)) == 0)
    {
        //always 16KHz
        sample_rate = 16000;
    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_MPEG4_AUDIO, oscl_strlen(PVMF_MIME_MPEG4_AUDIO)) == 0)
    {
        int32 specinfosize =
            (int32)(iMetadataParserObj->getTrackDecoderSpecificInfoSize(aId));
        if (specinfosize != 0)
        {
            // Retrieve the decoder specific info from file parser
            uint8* specinfoptr =
                iMetadataParserObj->getTrackDecoderSpecificInfoContent(aId);

            GetActualAacConfig(specinfoptr,
                               &audioObjectType,
                               &specinfosize,
                               &sampleRateIndex,
                               &num_channels,
                               &samplesPerFrame);
            if (sampleRateIndex < 13)
            {
                sample_rate = sample_freq_table[(uint32)sampleRateIndex];
            }
        }
    }
    return sample_rate;
}

uint32 PVMFMP4FFParserNode::GetAudioBitsPerSample(uint32 aId)
{
    OSCL_UNUSED_ARG(aId);
    //always 16 bits per samples
    return 16;
}




