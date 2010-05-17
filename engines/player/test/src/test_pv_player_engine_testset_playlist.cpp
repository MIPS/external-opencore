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
#include "test_pv_player_engine_testset_playlist.h"
#include "test_pv_player_engine_testset_playlist_clipinfo.h"
#include "oscl_error_codes.h"
#include "oscl_tickcount.h"
#include "pv_player_datasinkpvmfnode.h"
#include "pvmi_media_io_fileoutput.h"
#include "pv_media_output_node_factory.h"
#include "pvmf_errorinfomessage_extension.h"
#include "oscl_utf8conv.h"
#include "pvmi_kvp.h"
#include "pvmi_kvp_util.h"
#include "oscl_mem.h"
#include "oscl_mem_audit.h"
#include "pvmf_duration_infomessage.h"

// the order of these strings should match ClipTransitionEvent enum
const char* CLIPTRANSITIONEVENT_NAMESTRING[] =
    { "Unknown event",
      "PVPlayerInfoClipInitialized",
      "PVMFInfoClipCorrupted",
      "PVPlayerInfoClipPlaybackStarted",
      "PVPlayerInfoClipPlaybackEnded",
      "PVPlayerInfoEndTimeReached",
      "PVPlayerInfoEndOfClipReached",
    };


int32 ReadPlaylistFromFile(const char* aFileName, char**& aPlaylist, FILE* aTestMsgOutputFile)
{
    // Open file
    Oscl_FileServer fileServ;
    if (fileServ.Connect() != 0)
    {
        fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): ERROR - Oscl_FileServer.Connect() failed\n");
        aPlaylist = NULL;
        return 0;
    }

    Oscl_File osclFile;
    if (osclFile.Open(aFileName, Oscl_File::MODE_READ, fileServ) != 0)
    {
        fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): ERROR - Oscl_File.Open() failed for %s\n", aFileName);

        fileServ.Close();

        aPlaylist = NULL;
        return 0;
    }

    // text file should contain the number of clips and the clip names, all inside [],
    // all characters outside of [] are ignored
    // e.g. the following contains 5 clips
    //
    // [5] [MyTrack01.mp3] [MyTrack02.mp3]
    // [MyTrack03.mp3][MyTrack04.mp3]  [MyTrack05.mp3]

    // use a 6K read buffer (256 * 24)
    // should be enough for all playlists
    char* bufPtr = (char*)oscl_malloc(PLAYLIST_READ_BUFFER_SIZE);
    if (NULL == bufPtr)
    {
        fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): ERROR - oscl_malloc failed for read buffer size %d\n", PLAYLIST_READ_BUFFER_SIZE);

        osclFile.Close();
        fileServ.Close();

        aPlaylist = NULL;
        return 0;
    }

    OSCL_StackString<64> prependStr = SOURCENAME_PREPEND_STRING;
    uint32 prependLen = prependStr.get_size();

    int32 clipCount = -1;
    char** playlistPtr = NULL;
    int32 clipsInPlaylist = 0;
    bool foundStartBracket = false;
    bool foundEndBracket = false;
    bool foundNumCount = false;
    int32 startBracketIndex = 0;
    int32 endBracketIndex = 0;

    // get the clip count first
    uint32 bytesRead = osclFile.Read(bufPtr, 1, PLAYLIST_READ_BUFFER_SIZE);
    if (bytesRead > 0)
    {
        int32 i;
        for (i = 0; i < (int32)bytesRead; i++)
        {
            if (('[' == bufPtr[i]) && !foundStartBracket)
            {
                startBracketIndex = i;
                foundStartBracket = true;
                if ((-1 == clipCount) && !foundNumCount)
                {
                    // this must be the clip count
                    foundNumCount = true;
                }
            }
            else if ((']' == bufPtr[i]) && !foundEndBracket)
            {
                endBracketIndex = i;
                foundEndBracket = true;
                break;
            }
        }

        if ((-1 == clipCount) && foundNumCount)
        {
            // convert to integer
            int32 value = 0;
            for (int32 j = startBracketIndex + 1; j < endBracketIndex; j++)
            {
                value = (10 * value) + (bufPtr[j] - 0x30);
            }

            if (value > 0)
            {
                // clip count is valid
                clipCount = value;
            }
            else
            {
                // clip count is not acceptable
                fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): ERROR - invalid Clip count %d in input text file %s\n", clipCount, aFileName);

                oscl_free(bufPtr);

                osclFile.Close();
                fileServ.Close();

                *aPlaylist = NULL;
                return 0;
            }
        }
        else
        {
            // missing clip count
            fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): ERROR - can't find clip count in input text file %s\n", aFileName);

            oscl_free(bufPtr);

            osclFile.Close();
            fileServ.Close();

            aPlaylist = NULL;
            return 0;
        }

        // get the clip names
        playlistPtr = (char **)oscl_malloc(clipCount * sizeof(char *));
        if (NULL == playlistPtr)
        {
            fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): ERROR - oscl_malloc failed for playlistPtr size %d\n", clipCount * sizeof(char *));

            oscl_free(bufPtr);

            osclFile.Close();
            fileServ.Close();

            aPlaylist = NULL;
            return 0;
        }

        // init playlist
        for (i = 0; i < clipCount; i++)
        {
            playlistPtr[i] = NULL;
        }

        bool notDone = true;
        while (notDone)
        {
            foundStartBracket = false;
            foundEndBracket = false;
            int32 k = endBracketIndex + 1;

            for (i = k; i < (int32)bytesRead; i++)
            {
                // lcoate the start and end braceket
                if (('[' == bufPtr[i]) && !foundStartBracket)
                {
                    startBracketIndex = i;
                    foundStartBracket = true;
                }
                else if ((']' == bufPtr[i]) && !foundEndBracket && foundStartBracket)
                {
                    endBracketIndex = i;
                    foundEndBracket = true;
                    break;
                }
            }

            if (foundStartBracket && foundEndBracket)
            {
                // allocate space for filename
                int32 nameLen = endBracketIndex - startBracketIndex - 1;
                if (nameLen <= 0)
                {
                    // skip over this file then
                    fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): WARNING - skipping invalid clip name in input text file %s\n", aFileName);
                }
                else
                {
                    // add 1 for NULL char
                    char* clipNamePtr = (char *)oscl_malloc(prependLen + nameLen + 1 * sizeof(char));
                    if (NULL == clipNamePtr)
                    {
                        fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): ERROR - oscl_malloc failed for clipNamePtr size %d\n", nameLen + 1 * sizeof(char));

                        if (clipsInPlaylist > 0)
                        {
                            for (int j = 0; j < clipsInPlaylist; j++)
                            {
                                oscl_free(playlistPtr[j]);
                            }
                        }

                        oscl_free(playlistPtr);

                        oscl_free(bufPtr);

                        osclFile.Close();
                        fileServ.Close();

                        aPlaylist = NULL;
                        return 0;
                    }
                    else
                    {
                        // copy the clip name into buffer,
                        // replace slashes with OSCL_FILE_CHAR_PATH_DELIMITER,
                        // add terminating '\0' and
                        // store pointer in playlist
                        char* nameStrPtr = clipNamePtr;
                        // add the SOURCENAME_PREPEND_STRING
                        if (prependLen != 0)
                        {
                            char* prependPtr = prependStr.get_str();
                            for (i = 0; i < (int32)prependLen; i++)
                            {
                                *nameStrPtr++ = *prependPtr++;
                            }
                        }

                        int32 k = startBracketIndex + 1;
                        for (i = 0; i < nameLen; i++)
                        {
                            // look for backward slash and forward slash
                            if ((bufPtr[k + i] == 0x5C) || (bufPtr[k + i] == 0x2F))
                            {
                                *nameStrPtr++ = OSCL_FILE_CHAR_PATH_DELIMITER[0];
                            }
                            else
                            {
                                *nameStrPtr++ = bufPtr[k + i];
                            }
                        }
                        *nameStrPtr = '\0';

                        playlistPtr[clipsInPlaylist++] = clipNamePtr;

                        if (clipsInPlaylist >= clipCount)
                        {
                            // no room for anymore clips
                            notDone = false;
                        }
                    }
                }
            }
            else
            {
                notDone = false;
            }
        } // end while

    } // end if read ok

    // done

    if (clipsInPlaylist != clipCount)
    {
        fprintf(aTestMsgOutputFile, "ReadPlaylistFromFile(): WARNING - clip count %d != clips found %d in input text file %s\n", clipCount, clipsInPlaylist, aFileName);
    }

    oscl_free(bufPtr);

    osclFile.Close();
    fileServ.Close();

    aPlaylist = playlistPtr;
    return clipsInPlaylist;
}

// Takes a filename, iFileName, as input
// Takes in invalid clips info
// Generates a playlist and returns the pointer in iPlaylist
// Sets the format type, iFileType, if necessary
// Prints out any error messages to FILE*
// Returns the size of the playlist
int32 CreatePlaylist(const char* aFileName, PVMFFormatType& aFileType, char**& aPlaylist, int32 aNumInvalidClips, int32 aInvalidClipAtIndex,
                     FILE* aTestMsgOutputFile, bool& aStaticPlaylist)
{
    OSCL_StackString<256> validFileName = SOURCENAME_PREPEND_STRING;
    OSCL_StackString<256> invalidFileName = SOURCENAME_PREPEND_STRING;
    bool defaultPlaylist = false;
    char** validPlaylistPtr = NULL;
    char** invalidPlaylistPtr = NULL;

    // if using the default MP3/MP4 clips
    // iFileType should have been set correctly
    // generate the playlist with default clips
    if (oscl_strstr(aFileName, ".mp3") != NULL || oscl_strstr(aFileName, ".MP3") != NULL)
    {
        validFileName += DEFAULT_VALID_MP3_PLAYLIST;
        invalidFileName += DEFAULT_INVALID_MP3_PLAYLIST;
        defaultPlaylist = true;
    }
    else if (oscl_strstr(aFileName, ".mp4") != NULL || oscl_strstr(aFileName, ".MP4") != NULL)
    {
        validFileName += DEFAULT_VALID_MP4_PLAYLIST;
        invalidFileName += DEFAULT_INVALID_MP4_PLAYLIST;
        defaultPlaylist = true;
    }
    else if (oscl_strstr(aFileName, ".wma") != NULL || oscl_strstr(aFileName, ".WMA") != NULL ||
             oscl_strstr(aFileName, ".asf") != NULL || oscl_strstr(aFileName, ".ASF") != NULL)
    {
        validFileName += DEFAULT_VALID_WMA_PLAYLIST;
        invalidFileName += DEFAULT_INVALID_WMA_PLAYLIST;
        defaultPlaylist = true;
    }
    else if (oscl_strstr(aFileName, ".txt") != NULL || oscl_strstr(aFileName, ".TXT") != NULL)
    {
        // using input text file from command line
        validFileName += aFileName;
        aFileType = PVMF_MIME_FORMAT_UNKNOWN;
    }
    else
    {
        fprintf(aTestMsgOutputFile, "CreatePlaylist(): ERROR - invalid input file %s\n", aFileName);

        aPlaylist = NULL;
        return 0;
    }

    int32 clipsInPlaylist = ReadPlaylistFromFile(validFileName.get_str(), validPlaylistPtr, aTestMsgOutputFile);

    // see if we need to replace some valid clips with invalid clips
    if (aNumInvalidClips > 0 && aInvalidClipAtIndex >= 0)
    {
        int32 invalidClipCount = ReadPlaylistFromFile(invalidFileName.get_str(), invalidPlaylistPtr, aTestMsgOutputFile);
        if (invalidClipCount < clipsInPlaylist)
        {
            fprintf(aTestMsgOutputFile, "CreatePlaylist(): ERROR - invalid clip count %d in %s < valid clip count %d in %s\n",
                    invalidClipCount, invalidFileName.get_str(), clipsInPlaylist, validFileName.get_str());
        }

        // we don't want to keep around the entire invalid playlist,
        // just keep around the tracks we need
        // free the rest of the track names
        int32 startInvalidIndex = aInvalidClipAtIndex;
        int32 endInvalidIndex = aInvalidClipAtIndex + aNumInvalidClips - 1;
        for (int i = 0; i < invalidClipCount; i++)
        {
            if (i >= startInvalidIndex && i <= endInvalidIndex)
            {
                oscl_free(validPlaylistPtr[i]);
                validPlaylistPtr[i] = invalidPlaylistPtr[i];
            }
            else
            {
                oscl_free(invalidPlaylistPtr[i]);
            }
        }
        oscl_free(invalidPlaylistPtr);
    }

    aStaticPlaylist = defaultPlaylist;
    aPlaylist = validPlaylistPtr;
    return clipsInPlaylist;
}


void DestroyPlaylist(char** aPlaylist, int32 aNumClipsInPlaylist)
{
    // free memory
    if (NULL != aPlaylist)
    {
        for (int i = 0; i < aNumClipsInPlaylist; i++)
        {
            if (NULL != aPlaylist[i])
            {
                oscl_free(aPlaylist[i]);
                aPlaylist[i] = NULL;
            }
        }
    }
}



bool AddEventToSequence(Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator>& aSequence, ClipTransitionEvent aEvent,
                        uint32 aClip_index, EventTimestampMode aTimestampMode, double aTimestamp, FILE* aTestMsgOutputFile)
{
    // allocate a PlaylistClipTransitionEvent struct and add to end of vector
    // add a timestamp if requested, otherwise leave as 0
    PlaylistClipTransitionEvent* eventPtr = (PlaylistClipTransitionEvent*)oscl_malloc(sizeof(PlaylistClipTransitionEvent));
    if (NULL == eventPtr)
    {
        fprintf(aTestMsgOutputFile, "AddEventToSequence(): ERROR - oscl_malloc failed\n");
        return false;
    }

    eventPtr->index = aClip_index;
    eventPtr->event = aEvent;
    eventPtr->timestamp = (double)0;
    if (aTimestampMode == PL_GENERATE_TIMESTAMP)
    {
        uint32 tick = OsclTickCount::TickCount();
        double msec = OsclTickCount::TicksToMsec(tick);
        eventPtr->timestamp = msec;
    }
    else if (aTimestampMode == PL_COPY_TIMESTAMP)
    {
        eventPtr->timestamp = aTimestamp;
    }

    aSequence.push_back(eventPtr);

    return true;
}

void PrintEventSequence(Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator>& aSequence, FILE* aTestMsgOutputFile)
{
    for (int i = 0; i < (int)aSequence.size(); i++)
    {
        fprintf(aTestMsgOutputFile, "== %s received for clip index %d at %g ms\n",
                CLIPTRANSITIONEVENT_NAMESTRING[aSequence[i]->event],
                aSequence[i]->index, aSequence[i]->timestamp);
    }
}

bool CheckEventSequence(Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator>& aSequence, int32 aNumClipsInPlaylist,
                        EventCounts* aPlaylistEventCounts, EventCounts* aClipEventCounts, PVSkipAndSeekTest aWhichTest,
                        FILE* aTestMsgOutputFile)
{
    fprintf(aTestMsgOutputFile, "CheckEventSequence Begins:\n");

    if (aNumClipsInPlaylist > MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING)
    {
        // don't bother with long custom playlists
        fprintf(aTestMsgOutputFile, "CheckEventSequence(): ABORTED Number of clips = %d in playlist > Max clips allowed = %d\n", aNumClipsInPlaylist, MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING);
        return false;
    }

    fprintf(aTestMsgOutputFile, "Events for playlist:\n");
    PrintEventSequence(aSequence, aTestMsgOutputFile);

    int32 numErrors = 0;
    int i, j, m = 0;
    int32 numEvents = aSequence.size();

    // sort out the events into per clip sequences
    Oscl_Vector<PlaylistClipTransitionEvent*, OsclMemAllocator> clipEvents[MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING];

    for (i = 0; i < numEvents; i++)
    {
        uint32 clipIndex = aSequence[i]->index;
        aPlaylistEventCounts->total++;

        // clip index of 0xffffffff means the event is for playlist only, like EOS and EOT
        if (clipIndex != 0xffffffff)
        {
            AddEventToSequence(clipEvents[clipIndex], aSequence[i]->event, aSequence[i]->index, PL_COPY_TIMESTAMP, aSequence[i]->timestamp, aTestMsgOutputFile);

            aClipEventCounts[clipIndex].total++;

            switch (aSequence[i]->event)
            {
                case PL_CLIP_UNKNOWN_EVENT:
                    aClipEventCounts[clipIndex].unknown++;
                    aPlaylistEventCounts->unknown++;
                    break;
                case PL_CLIP_INIT_EVENT:
                    aClipEventCounts[clipIndex].init++;
                    aPlaylistEventCounts->init++;
                    break;
                case PL_CLIP_CORRUPT_EVENT:
                    aClipEventCounts[clipIndex].corrupt++;
                    aPlaylistEventCounts->corrupt++;
                    break;
                case PL_CLIP_START_EVENT:
                    aClipEventCounts[clipIndex].start++;
                    aPlaylistEventCounts->start++;
                    break;
                case PL_CLIP_END_EVENT:
                    aClipEventCounts[clipIndex].end++;
                    aPlaylistEventCounts->end++;
                    break;
                default:
                    aClipEventCounts[clipIndex].unknown++;
                    aPlaylistEventCounts->unknown++;
                    break;
            }
        }
        else
        {
            switch (aSequence[i]->event)
            {
                case PL_EOS_EVENT:
                    aPlaylistEventCounts->eos++;
                    break;
                case PL_EOT_EVENT:
                    aPlaylistEventCounts->eot++;
                    break;
                default:
                    aPlaylistEventCounts->unknown++;
                    break;
            }
        }
    }

    for (i = 0; i < aNumClipsInPlaylist; i++)
    {
        fprintf(aTestMsgOutputFile, "Events for clip index %d:\n", i);
        PrintEventSequence(clipEvents[i], aTestMsgOutputFile);
    }

    // for each playlist
    //
    // if there is an EOT, there should only one such event
    // if there is an EOT, it must be the last event
    // if there is an EOS, there should only one such event
    // if there is an EOS, it must be the last event
    // we either an EOS or an EOT, not both
    //
    if (aPlaylistEventCounts->eot > 1)
    {
        // we have a problem
        fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong number - EOT = %d > 1\n",
                aPlaylistEventCounts->eot);

        numErrors++;
    }
    if (aPlaylistEventCounts->eos > 1)
    {
        // we have a problem
        fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong number - EOS = %d > 1\n",
                aPlaylistEventCounts->eos);

        numErrors++;
    }
    if ((aPlaylistEventCounts->eot > 1) && (aPlaylistEventCounts->eos > 1))
    {
        // we have a problem
        fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong number - EOS = %d > 1 && EOT = %d > 1\n",
                aPlaylistEventCounts->eos, aPlaylistEventCounts->eot);

        numErrors++;
    }
    if ((aPlaylistEventCounts->eot == 1) && (PL_EOT_EVENT != aSequence[numEvents - 1]->event))
    {
        // we have a problem
        fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong order - EOT not last event\n");

        numErrors++;
    }
    if ((aPlaylistEventCounts->eos == 1) && (PL_EOS_EVENT != aSequence[numEvents - 1]->event))
    {
        // we have a problem
        fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong order - EOS not last event\n");

        numErrors++;
    }

    // for each clip, check the number and order of events using the following rules:
    //
    // the first event should be either a corrupt event or an init event
    // if we have corrupt event, that should be the only event for that clip index, no other events should be received
    // if we do not have corrupt event, there should be at least 1 init event
    // if we have init events and if we have start or end events:
    // if there are N start events, there should be N+ init events
    // if there are N end events + EOT event, there should be N+ start events
    //
    // if there are 1+ start events, the start events should interleave with the init events in time
    //    i.e. from n = N->0, (start_t(n) > init_t(n)) && (start_t(n) < init_t(n - 1))
    // if there are 1+ end events, the end events should interleave with the start events in time
    //    i.e. from n = N->0, (end_t(n) > start_t(n)) && (end_t(n) < start_(n - 1))
    //
    for (i = 0; i < aNumClipsInPlaylist; i++)
    {
        if (!clipEvents[i].empty())
        {
            // check first event, should be either corrupt or init
            ClipTransitionEvent event = clipEvents[i][0]->event;
            if (event == PL_CLIP_CORRUPT_EVENT)
            {
                // there should be no other events
                if (1 != aClipEventCounts[i].total)
                {
                    // we have a problem
                    fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong number - Total %d > Corrupt = %d, clip index %d\n",
                            aClipEventCounts[i].total, aClipEventCounts[i].corrupt, i);
                    numErrors++;
                }
            }
            else if (event == PL_CLIP_INIT_EVENT)
            {
                // the first event is init
                // look at the rest of the events for this clip
                // check the number of events
                if (0 != aClipEventCounts[i].corrupt)
                {
                    // we have a problem
                    fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong number - Corrupt = %d > 1, clip index %d\n",
                            aClipEventCounts[i].corrupt, i);
                    numErrors++;
                }
                else if ((aClipEventCounts[i].start > aClipEventCounts[i].init) && (aWhichTest != SkipToCurrentTrackTest))
                {
                    // we have a problem
                    // unless this is SkipToCurrentTrackTest
                    fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong number - Start = %d > Init = %d, clip index %d\n",
                            aClipEventCounts[i].start, aClipEventCounts[i].init, i);
                    numErrors++;
                }
                else if (aClipEventCounts[i].end > aClipEventCounts[i].start)
                {
                    // we have a problem
                    fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong number - End/EOT = %d > Start = %d, clip index %d\n",
                            aClipEventCounts[i].end + aPlaylistEventCounts->eot, aClipEventCounts[i].start, i);
                    numErrors++;
                }
                else
                {
                    // check the order of events
                    int32 numStarts = aClipEventCounts[i].start;
                    if (numStarts > 0)
                    {
                        // multiple start events should interleave with multiple init events
                        for (j = 0; ((j < (int)aClipEventCounts[i].total) && (numStarts > 0)); j++)
                        {
                            // look for the start event
                            if (PL_CLIP_START_EVENT == clipEvents[i][j]->event)
                            {
                                numStarts--;
                                bool found = false;
                                // look backward for the preceding init
                                // this check may be redundant after the first init
                                for (m = j - 1; m >= 0; m--)
                                {
                                    if (PL_CLIP_INIT_EVENT == clipEvents[i][m]->event)
                                    {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found)
                                {
                                    // we have a problem
                                    // unless this is SkipToCurrentTrackTest
                                    fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong order - No Init before Start %g, clip index %d\n",
                                            clipEvents[i][j]->timestamp, i);

                                    numErrors++;
                                }

                                // if there are more start events, look forward for the next init event, should be one before the next start
                                if (numStarts > 0)
                                {
                                    found = false;
                                    for (m = j + 1; m < (int)aClipEventCounts[i].total; m++)
                                    {
                                        if (PL_CLIP_INIT_EVENT == clipEvents[i][m]->event)
                                        {
                                            found = true;
                                            break;
                                        }
                                        else if (PL_CLIP_START_EVENT == clipEvents[i][m]->event)
                                        {
                                            // no init before next start
                                            break;
                                        }
                                    }
                                    if (!found && (aWhichTest != SkipToCurrentTrackTest))
                                    {
                                        // we have a problem
                                        fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong order - No Init before Start %g, clip index %d\n",
                                                clipEvents[i][m]->timestamp, i);

                                        numErrors++;
                                    }
                                }
                            }
                        } // end for
                    } // end else if

                    // check end events
                    int32 numEnds = aClipEventCounts[i].end;
                    if (numEnds > 0)
                    {
                        // multiple end events should interleave with multiple start events
                        for (j = 0; ((j < (int)aClipEventCounts[i].total) && (numEnds > 0)); j++)
                        {
                            // look for the end
                            if (PL_CLIP_END_EVENT == clipEvents[i][j]->event)
                            {
                                numEnds--;
                                bool found = false;
                                // look backward for the preceding start
                                // this check may be redundant after the first start
                                for (m = j - 1; m >= 0; m--)
                                {
                                    if (PL_CLIP_START_EVENT == clipEvents[i][m]->event)
                                    {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found)
                                {
                                    // we have a problem
                                    fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong order - No Start before End %g, clip index %d\n",
                                            clipEvents[i][j]->timestamp, i);

                                    numErrors++;
                                }

                                // if there are more end events, look forward for the next start event, should be one before the next end
                                if (numEnds > 0)
                                {
                                    found = false;
                                    for (m = j + 1; m < (int)aClipEventCounts[i].total; m++)
                                    {
                                        if (PL_CLIP_START_EVENT == clipEvents[i][m]->event)
                                        {
                                            found = true;
                                            break;
                                        }
                                        else if (PL_CLIP_END_EVENT == clipEvents[i][m]->event)
                                        {
                                            // no start before next end
                                            break;
                                        }
                                    }
                                    if (!found)
                                    {
                                        // we have a problem
                                        fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong order - No Start before End %g, clip index %d\n",
                                                clipEvents[i][m]->timestamp, i);

                                        numErrors++;
                                    }
                                }
                            }
                        } // end for
                    } // end else if
                } // end else
            } // end else if first init
            else
            {
                // we have a problem
                fprintf(aTestMsgOutputFile, "CheckEventSequence(): Wrong order - First event %s not Init/Corrupt, clip index %d\n",
                        CLIPTRANSITIONEVENT_NAMESTRING[clipEvents[i][0]->event], i);
            }
        }
    }

    // clean up memory
    for (i = 0; i < MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING; i++)
    {
        while (!clipEvents[i].empty())
        {
            PlaylistClipTransitionEvent* entry = clipEvents[i].front();
            clipEvents[i].erase(clipEvents[i].begin());
            oscl_free(entry);
        }
    }

    if (0 == numErrors)
    {
        fprintf(aTestMsgOutputFile, "CheckEventSequence Ends: OK\n");
        return true;
    }

    fprintf(aTestMsgOutputFile, "CheckEventSequence Ends: %d ERRORS found\n", numErrors);
    return false;
}

bool CompareEventCounts(EventCounts* aResultingPlaylistEventCounts, EventCounts* aResultingClipEventCounts,
                        EventCounts* aExpectedPlaylistEventCounts, EventCounts* aExpectedClipEventCounts, FILE* aTestMsgOutputFile)
{
    fprintf(aTestMsgOutputFile, "CompareEventCounts Begins:\n");

    uint32 numErrors = 0;
    // compare the playlist event counts
    if (aResultingPlaylistEventCounts->total != aExpectedPlaylistEventCounts->total)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts: aResultingPlaylistEventCounts->total %d != aExpectedPlaylistEventCounts->total %d \n",
                aResultingPlaylistEventCounts->total, aExpectedPlaylistEventCounts->total);
        numErrors++;
    }
    if (aResultingPlaylistEventCounts->unknown != aExpectedPlaylistEventCounts->unknown)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts: aResultingPlaylistEventCounts->unknown %d != aExpectedPlaylistEventCounts->unknown %d \n",
                aResultingPlaylistEventCounts->unknown, aExpectedPlaylistEventCounts->unknown);
        numErrors++;
    }
    if (aResultingPlaylistEventCounts->init != aExpectedPlaylistEventCounts->init)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts: aResultingPlaylistEventCounts->init %d != aExpectedPlaylistEventCounts->init %d \n",
                aResultingPlaylistEventCounts->init, aExpectedPlaylistEventCounts->init);
        numErrors++;
    }
    if (aResultingPlaylistEventCounts->corrupt != aExpectedPlaylistEventCounts->corrupt)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts: aResultingPlaylistEventCounts->corrupt %d != aExpectedPlaylistEventCounts->corrupt %d \n",
                aResultingPlaylistEventCounts->corrupt, aExpectedPlaylistEventCounts->corrupt);
        numErrors++;
    }
    if (aResultingPlaylistEventCounts->start != aExpectedPlaylistEventCounts->start)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts: aResultingPlaylistEventCounts->start %d != aExpectedPlaylistEventCounts->start %d \n",
                aResultingPlaylistEventCounts->start, aExpectedPlaylistEventCounts->start);
        numErrors++;
    }
    if (aResultingPlaylistEventCounts->end != aExpectedPlaylistEventCounts->end)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts: aResultingPlaylistEventCounts->end %d != aExpectedPlaylistEventCounts->end %d \n",
                aResultingPlaylistEventCounts->end, aExpectedPlaylistEventCounts->end);
        numErrors++;
    }
    if (aResultingPlaylistEventCounts->eos != aExpectedPlaylistEventCounts->eos)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts: aResultingPlaylistEventCounts->eos %d != aExpectedPlaylistEventCounts->eos %d \n",
                aResultingPlaylistEventCounts->eos, aExpectedPlaylistEventCounts->eos);
        numErrors++;
    }
    if (aResultingPlaylistEventCounts->eot != aExpectedPlaylistEventCounts->eot)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts: aResultingPlaylistEventCounts->eot %d != aExpectedPlaylistEventCounts->eot %d \n",
                aResultingPlaylistEventCounts->eot, aExpectedPlaylistEventCounts->eot);
        numErrors++;
    }
    // compare the clip event counts
    for (int i = 0; i < PLAYLIST_MAX_NUMCLIPS; i++)
    {
        if (aResultingClipEventCounts[i].total != aExpectedClipEventCounts[i].total)
        {
            fprintf(aTestMsgOutputFile, "CompareEventCounts: clip index %d aResultingClipEventCounts[i].total %d != aExpectedClipEventCounts[i].total %d \n",
                    i, aResultingClipEventCounts[i].total, aExpectedClipEventCounts[i].total);
            numErrors++;
        }
        if (aResultingClipEventCounts[i].unknown != aExpectedClipEventCounts[i].unknown)
        {
            fprintf(aTestMsgOutputFile, "CompareEventCounts: clip index %d aResultingClipEventCounts[i].unknown %d != aExpectedClipEventCounts[i].unknown %d \n",
                    i, aResultingClipEventCounts[i].unknown, aExpectedClipEventCounts[i].unknown);
            numErrors++;
        }
        if (aResultingClipEventCounts[i].init != aExpectedClipEventCounts[i].init)
        {
            fprintf(aTestMsgOutputFile, "CompareEventCounts: clip index %d aResultingClipEventCounts[i].init %d != aExpectedClipEventCounts[i].init %d \n",
                    i, aResultingClipEventCounts[i].init, aExpectedClipEventCounts[i].init);
            numErrors++;
        }
        if (aResultingClipEventCounts[i].corrupt != aExpectedClipEventCounts[i].corrupt)
        {
            fprintf(aTestMsgOutputFile, "CompareEventCounts: clip index %d aResultingClipEventCounts[i].corrupt %d != aExpectedClipEventCounts[i].corrupt %d \n",
                    i, aResultingClipEventCounts[i].corrupt, aExpectedClipEventCounts[i].corrupt);
            numErrors++;
        }
        if (aResultingClipEventCounts[i].start != aExpectedClipEventCounts[i].start)
        {
            fprintf(aTestMsgOutputFile, "CompareEventCounts: clip index %d aResultingClipEventCounts[i].start %d != aExpectedClipEventCounts[i].start %d \n",
                    i, aResultingClipEventCounts[i].start, aExpectedClipEventCounts[i].start);
            numErrors++;
        }
        if (aResultingClipEventCounts[i].end != aExpectedClipEventCounts[i].end)
        {
            fprintf(aTestMsgOutputFile, "CompareEventCounts: clip index %d aResultingClipEventCounts[i].end %d != aExpectedClipEventCounts[i].end %d \n",
                    i, aResultingClipEventCounts[i].end, aExpectedClipEventCounts[i].end);
            numErrors++;
        }
    }
    if (0 == numErrors)
    {
        fprintf(aTestMsgOutputFile, "CompareEventCounts Ends: OK\n");
        return true;
    }

    fprintf(aTestMsgOutputFile, "CompareEventCounts Ends: %d ERRORS found\n", numErrors);
    return false;
}


bool ValidatePCMOutput(OSCL_wHeapString<OsclMemAllocator> aAudioSinkFileName, uint32 aExpectedFileSize, FILE* aTestMsgOutputFile)
{
    fprintf(aTestMsgOutputFile, "ValidatePCMOutput Begins:\n");
    // checking the PCM output file size for now
    // Open file
    Oscl_FileServer fileServ;
    if (fileServ.Connect() != 0)
    {
        fprintf(aTestMsgOutputFile, "ValidatePCMOutput Ends: ERROR - Oscl_FileServer.Connect() failed\n");
        return false;
    }

    char tmpCharBuffer[512];

    oscl_UnicodeToUTF8(aAudioSinkFileName.get_str(), oscl_strlen(aAudioSinkFileName.get_str()), tmpCharBuffer, 512);

    Oscl_File osclFile;
    if (osclFile.Open(tmpCharBuffer, Oscl_File::MODE_READ | Oscl_File::MODE_BINARY, fileServ) != 0)
    {
        fprintf(aTestMsgOutputFile, "ValidatePCMOutput Ends: ERROR - Oscl_File.Open() failed for %s\n", tmpCharBuffer);
        fileServ.Close();
        return false;
    }

    TOsclFileOffset resultingFileSize = osclFile.Size();

    osclFile.Close();
    fileServ.Close();

    if (aExpectedFileSize != (uint32)resultingFileSize)
    {
        fprintf(aTestMsgOutputFile, "ValidatePCMOutput Ends: ERROR - resultingFileSize %x != aExpectedFileSize %x\n",
                (uint32)resultingFileSize, (uint32)aExpectedFileSize);
        return false;
    }

    fprintf(aTestMsgOutputFile, "ValidatePCMOutput Ends: OK\n");

    return true;
}

/* TEST 821 */
//
// pvplayer_async_test_playlist_playback section
//


void pvplayer_async_test_playlist_playback::StartTest()
{
    fprintf(iTestMsgOutputFile, "Number of clips in playlist: %d\n", iNumClipsInPlaylist);
    fprintf(iTestMsgOutputFile, "Playlist format type: %s\n", iFileType.getMIMEStrPtr());

    for (int32 i = 0; i < iNumClipsInPlaylist; i++)
    {
        fprintf(iTestMsgOutputFile, "%s\n", iPlaylist[i]);
    }

    AddToScheduler();
    iState = STATE_CREATE;
    RunIfNotReady();
}

void pvplayer_async_test_playlist_playback::Run()
{
    int error = 0;

    switch (iState)
    {
        case STATE_CREATE:
        {
            iPlayer = NULL;
            fprintf(iTestMsgOutputFile, "Creating Player...\n");
            OSCL_TRY(error, iPlayer = PVPlayerFactory::CreatePlayer(this, this, this));
            if (error)
            {
                PVPATB_TEST_IS_TRUE(false);
                TestCompleted();
            }
            else
            {
                iState = STATE_ADDDATASOURCE;
                RunIfNotReady();
            }
            fprintf(iTestMsgOutputFile, "...Create Done\n");
        }
        break;

        case STATE_ADDDATASOURCE:
        {
            // if iTestUpdateDataSource is false, populate the full playlist upfront
            // otherwise, start with a min of 2 valid clips and then add the rest one clip at a time to exercise UpdateDataSource()
            // iPlaylist already contains all the clips (valid/invalid) in the right order
            iDataSource = new PVPlayerDataSourceURL;
            const char* filename;
            int32 clipsAdded = 0;
            int32 remainingCountOfValidClips = MIN_VALID_CLIPS_IN_PLAYLIST;

            while (clipsAdded < iNumClipsInPlaylist)
            {
                // check for invalid clips to be added
                // add all the invalid clips at once
                // invalid clips can be anywhere in the playlist
                if ((iNumInvalidClips > 0) && (iInvalidClipAtIndex == clipsAdded))
                {
                    int32 invalidClips = iNumInvalidClips;
                    while (invalidClips > 0)
                    {
                        if (clipsAdded > 0)
                        {
                            iDataSource->ExtendClipList();
                        }
                        filename = iPlaylist[clipsAdded];
                        oscl_UTF8ToUnicode(filename, oscl_strlen(filename), iTmpWCharBuffer, 512);
                        iFileNameWStr.set(iTmpWCharBuffer, oscl_strlen(iTmpWCharBuffer));
                        iDataSource->SetDataSourceURL(iFileNameWStr);
                        if (iUnknownMimeTypeTest)
                        {
                            iDataSource->SetDataSourceFormatType(iAlternateMimeType);
                        }
                        else
                        {
                            // set correct mime type for invalid contents.
                            iDataSource->SetDataSourceFormatType(iFileType);
                        }

                        fprintf(iTestMsgOutputFile, "Adding %s to playlist at clip index %d \n", filename, clipsAdded);

                        clipsAdded++;
                        invalidClips--;
                    }
                }
                else
                {
                    // add a valid clip next
                    if (clipsAdded > 0)
                    {
                        iDataSource->ExtendClipList();
                    }

                    filename = iPlaylist[clipsAdded];
                    oscl_UTF8ToUnicode(filename, oscl_strlen(filename), iTmpWCharBuffer, 512);
                    iFileNameWStr.set(iTmpWCharBuffer, oscl_strlen(iTmpWCharBuffer));
                    iDataSource->SetDataSourceURL(iFileNameWStr);

                    if (iUnknownMimeTypeTest)
                    {
                        // set in correct mime type for valid contents.
                        iDataSource->SetDataSourceFormatType(iAlternateMimeType);
                    }
                    else if (iInvalidMimeTypeTest && clipsAdded > 0)
                    {
                        // set in correct mime type for valid contents with clip index > 0
                        iDataSource->SetDataSourceFormatType(iAlternateMimeType);
                        iClipsWithInvalidMimeType++;
                    }
                    else
                    {
                        // set correct mime type if none of the above tests are true
                        iDataSource->SetDataSourceFormatType(iFileType);
                    }
                    fprintf(iTestMsgOutputFile, "Adding %s to playlist at clip index %d \n", filename, clipsAdded);

                    clipsAdded++;
                    remainingCountOfValidClips--;

                    if (iTestUpdateDataSource && (0 == remainingCountOfValidClips))
                    {
                        // got 2 valid clips in the playlist
                        // add the rest one clip at a time later
                        break;
                    }
                }
            }

            iNumClipsAdded = clipsAdded;
            fprintf(iTestMsgOutputFile, "Calling AddDataSource...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSource(*iDataSource, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }

        break;

        case STATE_INIT:
        {
            fprintf(iTestMsgOutputFile, "Calling Init...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Init((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;
        case STATE_GETMETADATAVALUELIST:
        {
            iNumValues = 0;

            if (iRetrieveCurrentPlayingMetadata)
            {
                iRetrieveCurrentPlayingMetadata = false;
                iPrintCurrentPlayingMetadata = true;

                fprintf(iTestMsgOutputFile, "Calling GetMetadataValues for playing clip index %d...\n", iCurrentPlaybackClip);

                iCurrentPlayingMetadataValueList.clear();
                iCurrentPlayingMetadataKeyList.clear();
                iCurrentPlayingMetadataKeyList.push_back(OSCL_HeapString<OsclMemAllocator>("all"));
                OSCL_TRY(error, iCurrentCmdId = iPlayer->GetMetadataValues(iCurrentPlayingMetadataKeyList, 0, -1, iNumValues, iCurrentPlayingMetadataValueList, (OsclAny*) & iContextObject, true, iCurrentPlaybackClip));
            }
            else if (iRetrieveCurrentInitializedMetadata)
            {
                iRetrieveCurrentInitializedMetadata = false;
                iPrintCurrentInitializedMetadata = true;

                fprintf(iTestMsgOutputFile, "Calling GetMetadataValues for initialized clip index %d...\n", iCurrentInitializedClip);

                iCurrentInitializedMetadataValueList.clear();
                iCurrentInitializedMetadataKeyList.clear();
                iCurrentInitializedMetadataKeyList.push_back(OSCL_HeapString<OsclMemAllocator>("all"));
                OSCL_TRY(error, iCurrentCmdId = iPlayer->GetMetadataValues(iCurrentInitializedMetadataKeyList, 0, -1, iNumValues, iCurrentInitializedMetadataValueList, (OsclAny*) & iContextObject, true, iCurrentInitializedClip));
            }
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_ADDDATASINK_AUDIO:
        {
            iAudioSinkFileName = OUTPUTNAME_PREPEND_WSTRING;
            iAudioSinkFileName += _STRLIT_WCHAR("test_playlist_");
            iAudioSinkFileName += iSinkFileNameSubString;
            if (iCompressedAudio)
            {
                iAudioSinkFileName += _STRLIT_WCHAR("compressed_");
            }
            iAudioSinkFileName += _STRLIT_WCHAR("_audio.dat");

            iMIOFileOutAudio = iMioFactory->CreateAudioOutput((OsclAny*) & iAudioSinkFileName, NULL/*observer*/, true/*active*/,
                               10/*Queue Limit*/, false, false);
            iIONodeAudio = PVMediaOutputNodeFactory::CreateMediaOutputNode(iMIOFileOutAudio);
            iDataSinkAudio = new PVPlayerDataSinkPVMFNode;
            ((PVPlayerDataSinkPVMFNode*)iDataSinkAudio)->SetDataSinkNode(iIONodeAudio);

            fprintf(iTestMsgOutputFile, "Adding Audio Sink...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSink(*iDataSinkAudio, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_ADDDATASINK_VIDEO:
        {
            iVideoSinkFileName = OUTPUTNAME_PREPEND_WSTRING;
            iVideoSinkFileName += _STRLIT_WCHAR("test_playlist_");
            iVideoSinkFileName += iSinkFileNameSubString;
            if (iCompressedVideo)
            {
                iVideoSinkFileName += _STRLIT_WCHAR("compressed_");
            }
            iVideoSinkFileName += _STRLIT_WCHAR("_video.dat");

            iMIOFileOutVideo = iMioFactory->CreateVideoOutput((OsclAny*) & iVideoSinkFileName, MEDIATYPE_VIDEO, iCompressedVideo);
            iIONodeVideo = PVMediaOutputNodeFactory::CreateMediaOutputNode(iMIOFileOutVideo);
            iDataSinkVideo = new PVPlayerDataSinkPVMFNode;
            ((PVPlayerDataSinkPVMFNode*)iDataSinkVideo)->SetDataSinkNode(iIONodeVideo);

            fprintf(iTestMsgOutputFile, "Adding Videoo Sink...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSink(*iDataSinkVideo, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_PREPARE:
        {
            fprintf(iTestMsgOutputFile, "Calling Prepare...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Prepare((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_START:
        {
            fprintf(iTestMsgOutputFile, "Calling Start...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Start((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_UPDATEDATASOURCE:
        {
            // add more clips if playlist is not full
            if ((iNumClipsAdded < (uint32)iNumClipsInPlaylist) || iReplaceTrackTest)
            {
                const char* filename;

                if (iReplaceClip)
                {
                    iReplaceClip = false;
                    iSkipGetMetadata = true;
                    uint32 currentClip = iDataSource->GetCurrentClip();

                    // replace with first clip
                    filename = iPlaylist[0];
                    oscl_UTF8ToUnicode(filename, oscl_strlen(filename), iTmpWCharBuffer, 512);
                    iFileNameWStr.set(iTmpWCharBuffer, oscl_strlen(iTmpWCharBuffer));

                    iDataSource->SetDataSourceURL(iFileNameWStr);
                    iDataSource->SetDataSourceFormatType(iFileType);

                    fprintf(iTestMsgOutputFile, "Replacing clip index %d with filename %s\n", currentClip, filename);
                }
                else if (iNumInvalidClips > 0 && iInvalidClipAtIndex == (int32)iNumClipsAdded)
                {
                    // add all the invalid clips at once
                    int32 invalidClips = iNumInvalidClips;
                    while ((invalidClips > 0) && (iNumClipsAdded < (uint32)iNumClipsInPlaylist))
                    {
                        filename = iPlaylist[iNumClipsAdded];
                        oscl_UTF8ToUnicode(filename, oscl_strlen(filename), iTmpWCharBuffer, 512);
                        iFileNameWStr.set(iTmpWCharBuffer, oscl_strlen(iTmpWCharBuffer));

                        iDataSource->ExtendClipList();
                        iDataSource->SetDataSourceURL(iFileNameWStr);
                        iDataSource->SetDataSourceFormatType(iFileType);

                        fprintf(iTestMsgOutputFile, "Adding %s to playlist at clip index %d \n", filename, iNumClipsAdded);

                        iNumClipsAdded++;
                        invalidClips--;
                    }
                }
                else
                {
                    // add the valid clips one at a time
                    filename = iPlaylist[iNumClipsAdded];
                    oscl_UTF8ToUnicode(filename, oscl_strlen(filename), iTmpWCharBuffer, 512);
                    iFileNameWStr.set(iTmpWCharBuffer, oscl_strlen(iTmpWCharBuffer));

                    iDataSource->ExtendClipList();
                    iDataSource->SetDataSourceURL(iFileNameWStr);
                    if (iUnknownMimeTypeTest)
                    {
                        // set in correct mime type for valid contents.
                        iDataSource->SetDataSourceFormatType(iAlternateMimeType);
                    }
                    else if (iInvalidMimeTypeTest)
                    {
                        // set in correct mime type for valid contents with clip index > 0
                        iDataSource->SetDataSourceFormatType(iAlternateMimeType);
                        iClipsWithInvalidMimeType++;
                    }
                    else
                    {
                        // set correct mime type if none of the above tests are true
                        iDataSource->SetDataSourceFormatType(iFileType);
                    }
                    fprintf(iTestMsgOutputFile, "Adding %s to playlist at clip index %d \n", filename, iNumClipsAdded);

                    iNumClipsAdded++;
                }

                fprintf(iTestMsgOutputFile, "Calling UpdateDataSource...\n");
                OSCL_TRY(error, iCurrentCmdId = iPlayer->UpdateDataSource(*iDataSource, (OsclAny*) & iContextObject));
                OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
            }
        }
        break;

        case STATE_STOP:
        {
            fprintf(iTestMsgOutputFile, "Calling Stop...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Stop((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASINK_AUDIO:
        {
            fprintf(iTestMsgOutputFile, "Removing Audio Sink...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSink(*iDataSinkAudio, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASINK_VIDEO:
        {
            fprintf(iTestMsgOutputFile, "Removing Video Sink...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSink(*iDataSinkVideo, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_RESET:
        {
            fprintf(iTestMsgOutputFile, "Calling Reset...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Reset((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASOURCE:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSource(*iDataSource, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_CLEANUPANDCOMPLETE:
        {
            fprintf(iTestMsgOutputFile, "Deleting Player...\n");
            PVPATB_TEST_IS_TRUE(PVPlayerFactory::DeletePlayer(iPlayer));
            iPlayer = NULL;

            delete iSourceContextData;
            iSourceContextData = NULL;

            delete iDataSource;
            iDataSource = NULL;

            delete iDataSinkAudio;
            iDataSinkAudio = NULL;

            delete iDataSinkVideo;
            iDataSinkVideo = NULL;

            PVMediaOutputNodeFactory::DeleteMediaOutputNode(iIONodeVideo);
            iIONodeVideo = NULL;

            PVMediaOutputNodeFactory::DeleteMediaOutputNode(iIONodeAudio);
            iIONodeAudio = NULL;

            iMioFactory->DestroyVideoOutput(iMIOFileOutVideo);
            iMIOFileOutVideo = NULL;

            iMioFactory->DestroyAudioOutput(iMIOFileOutAudio);
            iMIOFileOutAudio = NULL;

            ValidateEvents();

            if (0 != iReferencePCMFileSize)
            {
                bool ok = ValidatePCMOutput(iAudioSinkFileName, iReferencePCMFileSize, iTestMsgOutputFile);
                if (!ok)
                {
                    PVPATB_TEST_IS_TRUE(false);
                }
                else
                {
                    PVPATB_TEST_IS_TRUE(true);
                }
            }

            TestCompleted();
        }
        break;

        default:
            break;

    }
}


void pvplayer_async_test_playlist_playback::CommandCompleted(const PVCmdResponse& aResponse)
{
    if (aResponse.GetCmdId() != iCurrentCmdId)
    {
        // Wrong command ID.
        PVPATB_TEST_IS_TRUE(false);
        iState = STATE_CLEANUPANDCOMPLETE;
        RunIfNotReady();
        return;
    }

    if (aResponse.GetContext() != NULL)
    {
        if (aResponse.GetContext() == (OsclAny*)&iContextObject)
        {
            if (iContextObject != iContextObjectRefValue)
            {
                // Context data value was corrupted
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
                return;
            }
        }
        else
        {
            // Context data pointer was corrupted
            PVPATB_TEST_IS_TRUE(false);
            iState = STATE_CLEANUPANDCOMPLETE;
            RunIfNotReady();
            return;
        }
    }

    switch (iState)
    {
        case STATE_ADDDATASOURCE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...AddDataSource Complete\n");
                iState = STATE_INIT;
                RunIfNotReady();
            }
            else
            {
                // AddDataSource failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_INIT:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                iRetrieveCurrentInitializedMetadata = true;
                if ((iNumClipsStarted > 0) && iMetadataTest)
                {
                    iRetrieveCurrentPlayingMetadata = true;
                }

                iState = STATE_GETMETADATAVALUELIST;
                fprintf(iTestMsgOutputFile, "...Init Complete\n");
                RunIfNotReady();
                iInitComplete = true;
            }
            else
            {
                // Init failed, was it expected?
                if (iNumInvalidClips != 0 && iNumInvalidClips == iCorruptedClipsIdentified)
                {
                    fprintf(iTestMsgOutputFile, "...Init FAILED, since all clips in playlist were invalid\n");
                    PVPATB_TEST_IS_TRUE(false);
                }
                else
                {
                    fprintf(iTestMsgOutputFile, "...Init FAILED\n");
                    PVPATB_TEST_IS_TRUE(false);
                }
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_GETMETADATAVALUELIST:
            if (PVMFSuccess == aResponse.GetCmdStatus() || PVMFErrArgument == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...GetMetadataValue Complete\n");

                if (iMetadataTest)
                {
                    if (iPrintCurrentPlayingMetadata)
                    {
                        iPrintCurrentPlayingMetadata = false;
                        fprintf(iTestMsgOutputFile, "Printing metadata for playing clip index %d...\n", iCurrentPlaybackClip);
                        PrintMetadataInfo(iCurrentPlayingMetadataKeyList, iCurrentPlayingMetadataValueList, iCurrentPlaybackClip, true);
                    }
                    else if (iPrintCurrentInitializedMetadata)
                    {
                        iPrintCurrentInitializedMetadata = false;
                        fprintf(iTestMsgOutputFile, "Printing metadata for initialized clip index %d...\n", iCurrentInitializedClip);
                        PrintMetadataInfo(iCurrentInitializedMetadataKeyList, iCurrentInitializedMetadataValueList, iCurrentInitializedClip);
                    }
                }
                else if (iPrintCurrentInitializedMetadata)
                {
                    iPrintCurrentInitializedMetadata = false;
                    GetClipDuration();
                }

                if (iNumClipsStarted > 0)
                {
                    // need to get metadata value for 2 clips, the just initialized and the playing
                    if (iMetadataTest && (iRetrieveCurrentInitializedMetadata || iRetrieveCurrentPlayingMetadata))
                    {
                        iState = STATE_GETMETADATAVALUELIST;
                        RunIfNotReady();
                    }
                    else
                    {
                        // change clip filename before clip is initialized (for clip index 3)
                        // and after clip is initialized (for clip index 4 in default list or last clip in custom list)
                        if (iReplaceTrackTest && (((iNumClipsStarted == MIN_VALID_CLIPS_IN_PLAYLIST) && (iNumClipsAdded > (uint32)iNumClipsInitialized)) ||
                                                  (iNumClipsInitialized == iNumClipsInPlaylist)))
                        {
                            iReplaceClip = true;
                            iState = STATE_UPDATEDATASOURCE;
                            RunIfNotReady();
                        }
                        else
                        {
                            // wait for EOS
                            iState = STATE_STOP;
                        }
                    }
                }
                else
                {
                    iState = STATE_ADDDATASINK_AUDIO;
                    RunIfNotReady();
                }

                // else do nothing
            }
            else
            {
                // GetMetadataValue failed
                fprintf(iTestMsgOutputFile, "...GetMetadataValue FAILED\n");
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_ADDDATASINK_AUDIO:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Add Audio Sink Complete\n");
                if (iFileType != PVMF_MIME_MP3FF)
                {
                    iState = STATE_ADDDATASINK_VIDEO;
                }
                else
                {
                    iState = STATE_PREPARE;
                }
                RunIfNotReady();
            }
            else
            {
                // AddDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_ADDDATASINK_VIDEO:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Add Video Sink Complete\n");
                iState = STATE_PREPARE;
                RunIfNotReady();
            }
            else
            {
                // AddDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_PREPARE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Prepare Complete\n");
                iState = STATE_START;
                // The delay is added between Prepare and Start to test that player
                // does not start the clock and send playstatus events prior to start.
                RunIfNotReady(2000000);
            }
            else
            {
                // Prepare failed
                fprintf(iTestMsgOutputFile, "...Prepare FAILED\n");
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_START:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                // Do nothing and wait for EOS event
                if (0 == iNumEOSReceived)
                {
                    fprintf(iTestMsgOutputFile, "...Start Complete\n");
                }

                iState = STATE_STOP;

                int32 startIndex = iNumEOSReceived;
                if (iNumInvalidClips > 0 && 0 == iInvalidClipAtIndex)
                {
                    PVPATB_TEST_IS_TRUE(iNumInvalidClips == iCorruptedClipsIdentified);
                    startIndex = iInvalidClipAtIndex + iNumInvalidClips;
                }

                fprintf(iTestMsgOutputFile, "Playback should start with clip index %d...\n", startIndex);
            }
            else
            {
                // Start failed
                fprintf(iTestMsgOutputFile, "...Start FAILED\n");
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_UPDATEDATASOURCE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...UpdateDataSource Complete\n");

                if (iSkipGetMetadata)
                {
                    iSkipGetMetadata = false;
                    iState = STATE_STOP;
                }
                else
                {
                    iRetrieveCurrentInitializedMetadata = true;
                    if ((iNumClipsStarted > 0) && iMetadataTest)
                    {
                        iRetrieveCurrentPlayingMetadata = true;
                    }

                    iState = STATE_GETMETADATAVALUELIST;
                    RunIfNotReady();
                }
            }
            else
            {
                // Update data source failure is not fatal
                // handle it gracefully.
                if (iNumClipsAdded < PLAYLIST_MAX_NUMCLIPS)
                {
                    // Add Another clip
                    RunIfNotReady(100000);
                }
                else
                {
                    iState = STATE_STOP;
                }

                // Update data source failed
                fprintf(iTestMsgOutputFile, "...UpdateDataSource failed - not fatal\n");
            }
            break;


        case STATE_STOP:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Stop Complete\n");
                iState = STATE_REMOVEDATASINK_AUDIO;
                RunIfNotReady();
            }
            else
            {
                // Stop failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASINK_AUDIO:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Remove Audio Sink Complete\n");

                if (iFileType != PVMF_MIME_MP3FF)
                {
                    iState = STATE_REMOVEDATASINK_VIDEO;
                }
                else
                {
                    iState = STATE_RESET;
                }
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_RESET;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASINK_VIDEO:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Remove Video Sink Complete\n");
                iState = STATE_RESET;
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_RESET;
                RunIfNotReady();
            }
            break;

        case STATE_RESET:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Reset Complete\n");
                iState = STATE_REMOVEDATASOURCE;
                RunIfNotReady();
            }
            else
            {
                // Reset failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASOURCE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Remove DataSource Complete\n");
                PVPATB_TEST_IS_TRUE(true);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSource failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        default:
        {
            // Testing error if this is reached
            PVPATB_TEST_IS_TRUE(false);
            iState = STATE_CLEANUPANDCOMPLETE;
            RunIfNotReady();
        }
        break;
    }
}


void pvplayer_async_test_playlist_playback::HandleErrorEvent(const PVAsyncErrorEvent& aEvent)
{
    switch (aEvent.GetEventType())
    {
        case PVMFErrResourceConfiguration:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrResource:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrCorrupt:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrProcessing:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        default:
            // Unknown error and just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;
    }

    // Wait for engine to handle the error
    Cancel();
}


void pvplayer_async_test_playlist_playback::HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent)
{
    switch (aEvent.GetEventType())
    {
        case PVMFInfoDurationAvailable:
        {
            PVUuid infomsguuid = PVMFDurationInfoMessageInterfaceUUID;
            PVMFDurationInfoMessageInterface* eventMsg = NULL;
            PVInterface* infoExtInterface = aEvent.GetEventExtensionInterface();

            PVInterface* temp = NULL;

            if (infoExtInterface &&
                    infoExtInterface->queryInterface(infomsguuid, temp))
            {
                PVUuid eventuuid;
                int32 infoCode;
                eventMsg = OSCL_STATIC_CAST(PVMFDurationInfoMessageInterface*, temp);
                eventMsg->GetCodeUUID(infoCode, eventuuid);
                if (eventuuid == infomsguuid)
                {
                    uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();
                    iSessionDuration[*clipId] = eventMsg->GetDuration();
                    fprintf(iTestMsgOutputFile, "***** Updated Duration Received = %d ms ClipIndex %d\n", iSessionDuration[*clipId], *clipId);
                }
            }
        }
        break;
        case PVPlayerInfoClipPlaybackStarted:
        {
            uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();
            iCurrentPlaybackClip = *clipId;
            iNumClipsStarted++;

            fprintf(iTestMsgOutputFile, "***** PVPlayerInfoClipPlaybackStarted received for clip index %d\n", *clipId);

            bool added = AddEventToSequence(iResultingEvents, PL_CLIP_START_EVENT, *clipId, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
            if (!added)
            {
                fprintf(iTestMsgOutputFile, "%%%% PL_CLIP_START_EVENT for clip index %d was not added to iResultingEvents vector\n", *clipId);
            }
        }
        break;
        case PVPlayerInfoClipInitialized:
        {
            uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();
            iCurrentInitializedClip = *clipId;
            iNumClipsInitialized++;

            fprintf(iTestMsgOutputFile, "***** PVPlayerInfoClipInitialized received for clip index %d\n", *clipId);

            bool added = AddEventToSequence(iResultingEvents, PL_CLIP_INIT_EVENT, *clipId, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
            if (!added)
            {
                fprintf(iTestMsgOutputFile, "%%%% PL_CLIP_INIT_EVENT for clip index %d was not added to iResultingEvents vector\n", *clipId);
            }

            // if testing UpdateDataSource, add another clip is necessary, then get the metadata which is a longer process
            // otherwise, get the metadata right away
            if (iTestUpdateDataSource && (iNumClipsAdded < (uint32)iNumClipsInPlaylist))
            {
                if (iNumClipsInitialized >= MIN_VALID_CLIPS_IN_PLAYLIST)
                {
                    iState = STATE_UPDATEDATASOURCE;
                    RunIfNotReady();
                }
            }
            else
            {
                if (*clipId != 0)
                {
                    if (iInitComplete)
                    {
                        iRetrieveCurrentInitializedMetadata = true;
                        if ((iNumClipsStarted > 0) && iMetadataTest)
                        {
                            iRetrieveCurrentPlayingMetadata = true;
                        }
                        if (STATE_UPDATEDATASOURCE != iState)
                        {
                            iState = STATE_GETMETADATAVALUELIST;
                        }
                        RunIfNotReady();
                    }
                }
            }
        }
        break;
        case PVMFInfoErrorHandlingStart:
        {
            fprintf(iTestMsgOutputFile, "***** PVMFInfoErrorHandlingStart received\n");
        }
        break;
        case PVMFInfoErrorHandlingComplete:
        {
            fprintf(iTestMsgOutputFile, "***** PVMFInfoErrorHandlingComplete received\n");
            iState = STATE_CLEANUPANDCOMPLETE;
            RunIfNotReady();
        }
        break;
        case PVPlayerInfoClipPlaybackEnded:
        {
            uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();

            iNumEOSReceived++;

            fprintf(iTestMsgOutputFile, "***** PVPlayerInfoClipPlaybackEnded received for clip index %d\n", *clipId);

            bool added = AddEventToSequence(iResultingEvents, PL_CLIP_END_EVENT, *clipId, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
            if (!added)
            {
                fprintf(iTestMsgOutputFile, "%%%% PL_CLIP_END_EVENT for clip index %d was not added to iResultingEvents vector\n", *clipId);
            }
            if (*clipId != (uint32)(iNumClipsInPlaylist - 1) &&
                    *clipId + iCorruptedClipsIdentified < (uint32)iNumClipsInPlaylist)
            {
                // there are invalid clips in the list, so not necessarily next clip in the list would be played.
                int32 nextIndex = *clipId + 1;
                if ((nextIndex == iInvalidClipAtIndex) && (iNumInvalidClips > 0))
                {
                    nextIndex += iNumInvalidClips;
                }
                fprintf(iTestMsgOutputFile, "Clip index %d should be starting... \n", nextIndex);
            }
        }
        break;
        case PVMFInfoClipCorrupted:
        {
            iCorruptedClipsIdentified++;

            uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();
            fprintf(iTestMsgOutputFile, "***** PVMFInfoClipCorrupted received for clip index %d\n", *clipId);

            if (iNumInvalidClips > 0)
            {

                // index for first corrupted clip
                uint32 startIndex = iInvalidClipAtIndex;
                // index for last corrupted clip
                uint32 endIndex = iInvalidClipAtIndex + iNumInvalidClips;

                if (*clipId >= startIndex &&
                        *clipId <= endIndex)
                {
                    PVPATB_TEST_IS_TRUE(true);
                }
                else
                {
                    PVPATB_TEST_IS_TRUE(false);
                }

                bool added = AddEventToSequence(iResultingEvents, PL_CLIP_CORRUPT_EVENT, *clipId, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
                if (!added)
                {
                    fprintf(iTestMsgOutputFile, "%%%% PL_CLIP_CORRUPT_EVENT for clip index %d was not added to iResultingEvents vector\n", *clipId);
                }

                if (iCorruptedClipsIdentified == iNumInvalidClips)
                {
                    PVPATB_TEST_IS_TRUE(true);
                }
                else if (iCorruptedClipsIdentified > iNumInvalidClips)
                {
                    PVPATB_TEST_IS_TRUE(false);
                }

                // if testing UpdateDataSource, add another clip is necessary
                if (iTestUpdateDataSource && (iNumClipsAdded < (uint32)iNumClipsInPlaylist))
                {
                    if (iNumClipsInitialized >= MIN_VALID_CLIPS_IN_PLAYLIST)
                    {
                        iState = STATE_UPDATEDATASOURCE;
                        RunIfNotReady();
                    }
                }
            }
            else if (iInvalidMimeTypeTest)
            {
                PVPATB_TEST_IS_TRUE(true);
            }
        }
        break;
        case PVMFInfoEndOfData:
        {
            // verified number of corrupted clips.
            uint32 numEOSExpected = 0;
            if (iInvalidMimeTypeTest)
            {
                numEOSExpected = iNumClipsAdded - iClipsWithInvalidMimeType;
                PVPATB_TEST_IS_TRUE(numEOSExpected == iNumEOSReceived);
            }
            else
            {
                numEOSExpected = PLAYLIST_MAX_NUMCLIPS - iNumInvalidClips;
            }
            if (numEOSExpected == iNumEOSReceived)
            {
                // all the clips have been played back successfully.
                iState = STATE_STOP;
                fprintf(iTestMsgOutputFile, "Playlist playback is complete...\n");
            }
            else if (iNumEOSReceived + iCorruptedClipsIdentified == (uint32)iNumClipsInPlaylist)
            {
                iState = STATE_STOP;
                fprintf(iTestMsgOutputFile, "Playlist playback is in-complete...\n");
                PVPATB_TEST_IS_TRUE(false);
            }

            PVInterface* iface = (PVInterface*)(aEvent.GetEventExtensionInterface());
            if (NULL == iface)
            {
                return;
            }
            PVUuid infomsguuid = PVMFErrorInfoMessageInterfaceUUID;
            PVMFErrorInfoMessageInterface* infomsgiface = NULL;
            if (true == iface->queryInterface(infomsguuid, (PVInterface*&)infomsgiface))
            {
                int32 infocode;
                PVUuid infouuid;
                infomsgiface->GetCodeUUID(infocode, infouuid);
                if ((PVPlayerErrorInfoEventTypesUUID == infouuid) &&
                        (PVPlayerInfoEndOfClipReached == infocode))
                {
                    fprintf(iTestMsgOutputFile, "EOS received. Stopping playback.\n");

                    bool added = AddEventToSequence(iResultingEvents, PL_EOS_EVENT, 0xffffffff, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
                    if (!added)
                    {
                        fprintf(iTestMsgOutputFile, "%%%% PL_EOS_EVENT was not added to iResultingEvents vector\n");
                    }

                    Cancel();
                    RunIfNotReady();
                }
                else if ((PVPlayerErrorInfoEventTypesUUID == infouuid) &&
                         (PVPlayerInfoEndTimeReached  == infocode))
                {
                    fprintf(iTestMsgOutputFile, "EOT received. Stopping playback.\n");

                    bool added = AddEventToSequence(iResultingEvents, PL_EOT_EVENT, 0xffffffff, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
                    if (!added)
                    {
                        fprintf(iTestMsgOutputFile, "%%%% PL_EOT_EVENT was not added to iResultingEvents vector\n");
                    }

                    Cancel();
                    RunIfNotReady();
                }
            }
        }
        break;
        case PVMFInfoPositionStatus:
        {
            PVInterface* iface = (PVInterface*)(aEvent.GetEventExtensionInterface());
            if (NULL == iface)
            {
                return;
            }
            PVUuid infomsguuid = PVMFErrorInfoMessageInterfaceUUID;
            PVMFErrorInfoMessageInterface* infomsgiface = NULL;
            if (true == iface->queryInterface(infomsguuid, (PVInterface*&)infomsgiface))
            {
                int32 infocode;
                PVUuid infouuid;
                infomsgiface->GetCodeUUID(infocode, infouuid);
                if ((PVPlayerErrorInfoEventTypesUUID == infouuid) &&
                        (PVPlayerInfoPlaybackPositionStatus == infocode))
                {
                    uint8* localbuf = aEvent.GetLocalBuffer();
                    uint32 pbpos = 0;
                    oscl_memcpy(&pbpos, &(localbuf[4]), 4);
                    fprintf(iTestMsgOutputFile, "---Playback status(time) %d ms\n", pbpos);

                    // playback position should not be greater than clip duration
                    if ((iSessionDuration[iCurrentPlaybackClip] != 0) && (pbpos > iSessionDuration[iCurrentPlaybackClip]))
                    {
                        PVPATB_TEST_IS_TRUE(false);

                        fprintf(iTestMsgOutputFile, "----- Error playback status %d ms > clip duration %d ms!!!\n",
                                pbpos, iSessionDuration[iCurrentPlaybackClip]);
                    }
                }
            }
        }
        break;
    }
    return;
}

void pvplayer_async_test_playlist_playback::GetClipDuration()
{
    uint32 i = 0;
    for (i = 0; i < iCurrentInitializedMetadataValueList.size(); ++i)
    {
        if (0 == oscl_strcmp((char*)(iCurrentInitializedMetadataValueList[i].key),
                             _STRLIT_CHAR("duration;valtype=uint32;timescale=1000")))
        {
            iSessionDuration[iCurrentInitializedClip] = iCurrentInitializedMetadataValueList[i].value.uint32_value;
        }
    }

    if (iSessionDuration[iCurrentInitializedClip] > 0)
    {
        fprintf(iTestMsgOutputFile, "***** Clip Duration: %d for clip index %d\n", iSessionDuration[iCurrentInitializedClip], iCurrentInitializedClip);
    }
    else
    {
        fprintf(iTestMsgOutputFile, "***** Clip Duration: \"Unknown\"\n");
    }
}
void pvplayer_async_test_playlist_playback::PrintMetadataInfo(PVPMetadataList aMetadataKeyList,
        Oscl_Vector<PvmiKvp, OsclMemAllocator> aMetadataValueList, int32 aClipIndex,
        bool aUpdateSessionDuration)
{
    uint32 i = 0;
    fprintf(iTestMsgOutputFile, "Metadata key list (count=%d):\n", aMetadataKeyList.size());
    for (i = 0; i < aMetadataKeyList.size(); ++i)
    {
        fprintf(iTestMsgOutputFile, "Key %d: %s\n", (i + 1), aMetadataKeyList[i].get_cstr());
    }

    fprintf(iTestMsgOutputFile, "\nMetadata value list (count=%d):\n", aMetadataValueList.size());
    for (i = 0; i < aMetadataValueList.size(); ++i)
    {
        fprintf(iTestMsgOutputFile, "Value %d:\n", (i + 1));
        fprintf(iTestMsgOutputFile, "   Key string: %s\n", aMetadataValueList[i].key);

        switch (GetValTypeFromKeyString(aMetadataValueList[i].key))
        {
            case PVMI_KVPVALTYPE_CHARPTR:
                fprintf(iTestMsgOutputFile, "   Value:%s\n", aMetadataValueList[i].value.pChar_value);
                break;

            case PVMI_KVPVALTYPE_WCHARPTR:
            {
                // Assume string is in UCS-2 encoding so convert to UTF-8
                char tmpstr[65];
                oscl_UnicodeToUTF8(aMetadataValueList[i].value.pWChar_value,
                                   oscl_strlen(aMetadataValueList[i].value.pWChar_value), tmpstr, 65);
                tmpstr[64] = '\0';
                fprintf(iTestMsgOutputFile, "   Value(in UTF-8, first 64 chars):%s\n", tmpstr);
            }
            break;

            case PVMI_KVPVALTYPE_UINT32:
                fprintf(iTestMsgOutputFile, "   Value:%d\n", aMetadataValueList[i].value.uint32_value);
                break;

            case PVMI_KVPVALTYPE_INT32:
                fprintf(iTestMsgOutputFile, "   Value:%d\n", aMetadataValueList[i].value.int32_value);
                if (0 == oscl_strcmp((char*)(aMetadataValueList[i].key),
                                     _STRLIT_CHAR("duration;valtype=uint32;timescale=1000")))
                {
                    if (aUpdateSessionDuration)
                    {
                        iSessionDuration[aClipIndex] = aMetadataValueList[i].value.uint32_value;
                    }
                }

                break;

            case PVMI_KVPVALTYPE_UINT8:
                fprintf(iTestMsgOutputFile, "   Value:%d\n", aMetadataValueList[i].value.uint8_value);
                break;

            case PVMI_KVPVALTYPE_FLOAT:
                fprintf(iTestMsgOutputFile, "   Value:%f\n", aMetadataValueList[i].value.float_value);
                break;

            case PVMI_KVPVALTYPE_DOUBLE:
                fprintf(iTestMsgOutputFile, "   Value:%f\n", aMetadataValueList[i].value.double_value);
                break;

            case PVMI_KVPVALTYPE_BOOL:
                if (aMetadataValueList[i].value.bool_value)
                {
                    fprintf(iTestMsgOutputFile, "   Value:true(1)\n");
                }
                else
                {
                    fprintf(iTestMsgOutputFile, "   Value:false(0)\n");
                }
                break;

            default:
                fprintf(iTestMsgOutputFile, "   Value: UNKNOWN VALUE TYPE\n");
                break;
        }

        fprintf(iTestMsgOutputFile, "   Length:%d  Capacity:%d\n", aMetadataValueList[i].length, aMetadataValueList[i].capacity);

        if ((oscl_strstr(aMetadataValueList[i].key, "duration")) && 0 == iSessionDuration[aClipIndex])
        {
            if (aUpdateSessionDuration)
            {
                iSessionDuration[aClipIndex] = aMetadataValueList[i].value.uint32_value;
            }

            // Check the timescale. If not available, assume millisecond (1000)
            const char *retTSstr;
            retTSstr = oscl_strstr(aMetadataValueList[i].key, "timescale=");
            uint32 retTSstrLen = 0;
            uint32 tsstrlen = oscl_strlen(_STRLIT_CHAR("timescale="));
            if (retTSstr != NULL)
            {
                retTSstrLen = oscl_strlen(retTSstr);
                if (retTSstrLen > tsstrlen)
                {
                    uint32 timescale = 0;
                    PV_atoi((char*)(retTSstr + tsstrlen), 'd', (retTSstrLen - tsstrlen), timescale);
                    if (timescale > 0 && timescale != 1000)
                    {
                        // Convert to milliseconds
                        MediaClockConverter mcc(timescale);
                        mcc.update_clock(iSessionDuration[aClipIndex]);
                        if (aUpdateSessionDuration)
                        {
                            iSessionDuration[aClipIndex] = mcc.get_converted_ts(1000);
                        }
                    }
                }
            }
        }
        if (0 == oscl_strcmp((char*)(aMetadataValueList[i].key),
                             _STRLIT_CHAR("duration;valtype=char*")))
        {
            fprintf(iTestMsgOutputFile, "Duration unknown for clip");
        }
    }

    fprintf(iTestMsgOutputFile, "\n\n");
}

void pvplayer_async_test_playlist_playback::ValidateEvents()
{
    bool ok = false;
    // count the events per clip and per playlist
    ok = CheckEventSequence(iResultingEvents, iNumClipsInPlaylist, &iResultingPlaylistEventCounts, iResultingClipEventCounts, iWhichTest, iTestMsgOutputFile);
    if ((iNumClipsInPlaylist <= MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING) && !ok)
    {
        PVPATB_TEST_IS_TRUE(false);
    }
    // compare the expected and the resulting event counts if default playlist is used
    if (iStaticPlaylist)
    {
        ok = CompareEventCounts(&iResultingPlaylistEventCounts, iResultingClipEventCounts, &iExpectedPlaylistEventCounts, iExpectedClipEventCounts, iTestMsgOutputFile);
        if (!ok)
        {
            PVPATB_TEST_IS_TRUE(false);
        }
    }
}



/* TEST 825 */
//
// pvplayer_async_test_playlist_playback section
//
void pvplayer_async_test_playlist_seek_skip::StartTest()
{
    fprintf(iTestMsgOutputFile, "Number of clips in playlist: %d\n", iNumClipsInPlaylist);
    fprintf(iTestMsgOutputFile, "Playlist format type: %s\n", iFileType.getMIMEStrPtr());

    for (int32 i = 0; i < iNumClipsInPlaylist; i++)
    {
        fprintf(iTestMsgOutputFile, "%s\n", iPlaylist[i]);
    }

    AddToScheduler();
    iState = STATE_CREATE;
    RunIfNotReady();
}

void pvplayer_async_test_playlist_seek_skip::Run()
{
    int error = 0;

    switch (iState)
    {
        case STATE_CREATE:
        {
            iPlayer = NULL;
            fprintf(iTestMsgOutputFile, "Creating Player...\n");
            OSCL_TRY(error, iPlayer = PVPlayerFactory::CreatePlayer(this, this, this));
            if (error)
            {
                PVPATB_TEST_IS_TRUE(false);
                TestCompleted();
            }
            else
            {
                iState = STATE_ADDDATASOURCE;
                RunIfNotReady();
            }
            fprintf(iTestMsgOutputFile, "...Create Done\n");
        }
        break;

        case STATE_ADDDATASOURCE:
        {
            // if iTestUpdateDataSource is false, populate the full playlist upfront
            // otherwise, start with a min of 2 valid clips and then add the rest one clip at a time to exercise UpdateDataSource()
            iDataSource = new PVPlayerDataSourceURL;
            int32 clipsAdded = 0;
            int32 remainingCountOfValidClips = MIN_VALID_CLIPS_IN_PLAYLIST;

            while (clipsAdded < iNumClipsInPlaylist)
            {
                const char* filename = iPlaylist[clipsAdded];
                oscl_UTF8ToUnicode(filename, oscl_strlen(filename), iTmpWCharBuffer, 512);
                iFileNameWStr.set(iTmpWCharBuffer, oscl_strlen(iTmpWCharBuffer));

                if (clipsAdded > 0)
                {
                    iDataSource->ExtendClipList();
                }
                iDataSource->SetDataSourceURL(iFileNameWStr);
                iDataSource->SetDataSourceFormatType(iFileType);

                fprintf(iTestMsgOutputFile, "Adding %s to playlist at clipindex %d \n", filename, clipsAdded);

                clipsAdded++;
                remainingCountOfValidClips--;

                if (iTestUpdateDataSource && (0 == remainingCountOfValidClips))
                {
                    // got 2 valid clips in the playlist
                    // add the rest one clip at a time later
                    break;
                }
            }

            iNumClipsAdded = clipsAdded;
            fprintf(iTestMsgOutputFile, "Calling AddDataSource...\n");
            error = AddDataSource();
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }

        break;

        case STATE_INIT:
        {
            fprintf(iTestMsgOutputFile, "Calling Init...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Init((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_ADDDATASINK_AUDIO:
        {
            OSCL_wHeapString<OsclMemAllocator> sinkfilename;
            sinkfilename = OUTPUTNAME_PREPEND_WSTRING;
            sinkfilename += _STRLIT_WCHAR("test_playlist_playback");
            sinkfilename += iSinkFileNameSubString;
            if (iCompressedAudio)
            {
                sinkfilename += _STRLIT_WCHAR("compressed_");
            }
            sinkfilename += _STRLIT_WCHAR("_audio.dat");

            iMIOFileOutAudio = iMioFactory->CreateAudioOutput((OsclAny*) & sinkfilename, NULL/*observer*/, true/*active*/,
                               10/*Queue Limit*/, false, false);
            iIONodeAudio = PVMediaOutputNodeFactory::CreateMediaOutputNode(iMIOFileOutAudio);
            iDataSinkAudio = new PVPlayerDataSinkPVMFNode;
            ((PVPlayerDataSinkPVMFNode*)iDataSinkAudio)->SetDataSinkNode(iIONodeAudio);

            fprintf(iTestMsgOutputFile, "Adding Audio Sink...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSink(*iDataSinkAudio, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_ADDDATASINK_VIDEO:
        {
            OSCL_wHeapString<OsclMemAllocator> sinkfilename;
            sinkfilename = OUTPUTNAME_PREPEND_WSTRING;
            sinkfilename += _STRLIT_WCHAR("test_playlist_playback");
            sinkfilename += iSinkFileNameSubString;
            if (iCompressedVideo)
            {
                sinkfilename += _STRLIT_WCHAR("compressed_");
            }
            sinkfilename += _STRLIT_WCHAR("_video.dat");

            iMIOFileOutVideo = iMioFactory->CreateVideoOutput((OsclAny*) & sinkfilename, MEDIATYPE_VIDEO, iCompressedVideo);
            iIONodeVideo = PVMediaOutputNodeFactory::CreateMediaOutputNode(iMIOFileOutVideo);
            iDataSinkVideo = new PVPlayerDataSinkPVMFNode;
            ((PVPlayerDataSinkPVMFNode*)iDataSinkVideo)->SetDataSinkNode(iIONodeVideo);

            fprintf(iTestMsgOutputFile, "Adding Video Sink...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSink(*iDataSinkVideo, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_PREPARE:
        {
            fprintf(iTestMsgOutputFile, "Calling Prepare...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Prepare((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_START:
        {
            fprintf(iTestMsgOutputFile, "Calling Start...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Start((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_UPDATEDATASOURCE:
        {
            // put more clips in the playlist
            const char* filename = iPlaylist[iNumClipsAdded];

            oscl_UTF8ToUnicode(filename, oscl_strlen(filename), iTmpWCharBuffer, 512);
            iFileNameWStr.set(iTmpWCharBuffer, oscl_strlen(iTmpWCharBuffer));
            iDataSource->ExtendClipList();
            iDataSource->SetDataSourceURL(iFileNameWStr);
            iDataSource->SetDataSourceFormatType(iFileType);

            fprintf(iTestMsgOutputFile, "Adding %s to playlist at clipindex %d \n", filename, iNumClipsAdded);
            fprintf(iTestMsgOutputFile, "Calling UpdateDataSource...\n");

            OSCL_TRY(error, iCurrentCmdId = iPlayer->UpdateDataSource(*iDataSource, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
            iNumClipsAdded++;
        }
        break;

        case STATE_PAUSE:
        {
            fprintf(iTestMsgOutputFile, "Calling Pause...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Pause((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_SETPLAYBACKRANGE:
        {
            PVPPlaybackPosition beginPos, endPos;
            if (iSetEOT)
            {
                // setting end of time for the current clip
                // if duration is known, set to 1/2 of duration
                // else set to 8000 ms
                iSetEOT = false;

                beginPos.iIndeterminate = true;
                beginPos.iMode = PVPPBPOS_MODE_NOW;
                beginPos.iPlayElementIndex = iNumEOSReceived;
                beginPos.iPosUnit = PVPPBPOSUNIT_MILLISEC;

                endPos.iIndeterminate = false;
                endPos.iPosUnit = PVPPBPOSUNIT_MILLISEC;
                endPos.iPosValue.millisec_value = iSessionDuration[iCurrentPlaybackClip] == 0 ? 8000 : iSessionDuration[iCurrentPlaybackClip] / 2;

                fprintf(iTestMsgOutputFile, "Setting EOT to %d ms in current clip... \n", endPos.iPosValue.millisec_value);
            }
            else if (iSeekInCurrentTrack || (SeekBeyondClipDurationTest == iWhichTest))
            {
                if (iSeekInCurrentTrack)
                {
                    // setting the begin of time for the current clip
                    // use 2000 ms for now
                    iTargetNPT = 2000;
                }
                else
                {
                    // if duration is known and seek beyond it
                    // else set it some arbitrary number
                    if (0 != iSessionDuration[iCurrentPlaybackClip])
                    {
                        iTargetNPT = iSessionDuration[iCurrentPlaybackClip] + 1000;
                    }
                    else
                    {
                        iTargetNPT = 60 * 1000;
                    }
                }

                beginPos.iPosUnit = PVPPBPOSUNIT_PLAYLIST;
                beginPos.iPlayListPosUnit = PVPPBPOSUNIT_MILLISEC;
                beginPos.iPlayListUri = NULL;

                beginPos.iIndeterminate = false;
                beginPos.iMode = PVPPBPOS_MODE_NOW;
                beginPos.iPlayElementIndex = iCurrentPlaybackClip;
                beginPos.iPlayListPosValue.millisec_value = iTargetNPT;

                endPos.iIndeterminate = true;
                endPos.iPosUnit = PVPPBPOSUNIT_PLAYLIST;

                fprintf(iTestMsgOutputFile, "Setting BOT to %d ms in current clip... \n", beginPos.iPlayListPosValue.millisec_value);
            }
            else
            {
                iSkippedToClip = -1;

                beginPos.iIndeterminate = false;
                beginPos.iMode = PVPPBPOS_MODE_NOW;
                if ((SkipAndSeekFwdNextTrackTest == iWhichTest) || (SkipFwdOneTrackTest == iWhichTest) || (PauseSkipToNextTrackResumeTest == iWhichTest))
                {
                    beginPos.iPlayElementIndex = iNumEOSReceived + 1; // skip to next track in the list
                    iForwardSkip = iNumEOSReceived + 1;
                    iSkippedToClip = iForwardSkip;
                }
                else if (SkipBwdOneTrackTest == iWhichTest)
                {
                    beginPos.iPlayElementIndex = iNumEOSReceived - 1; // skip to previous track in the list
                    iBackwardSkip = iNumEOSReceived - 1;
                    iSkippedToClip = iBackwardSkip;
                }
                else if ((SkipFwdLastTrackTest == iWhichTest) || (SkipFwdAndSeekTest == iWhichTest))
                {
                    if (iTestUpdateDataSource)
                    {
                        // playlist may not be fully propagated
                        iForwardSkip = iNumClipsInitialized - 1; // skip to last initialized track
                    }
                    beginPos.iPlayElementIndex = iForwardSkip; // skip to last track in the list
                    iSkippedToClip = iForwardSkip;
                }
                else if (SkipToCurrentTrackTest == iWhichTest)
                {
                    beginPos.iPlayElementIndex = iCurrentPlaybackClip; // skip to current track in the list
                    iForwardSkip = iCurrentPlaybackClip;
                    iSkippedToClip = iForwardSkip;
                }
                else
                {
                    beginPos.iPlayElementIndex = iForwardSkip;
                    iSkippedToClip = iForwardSkip;
                }

                beginPos.iPosUnit = PVPPBPOSUNIT_PLAYLIST;
                beginPos.iPlayListPosUnit = PVPPBPOSUNIT_MILLISEC;
                beginPos.iPlayListPosValue.millisec_value = iTargetNPT;
                beginPos.iPlayListUri = NULL;

                endPos.iPosUnit = PVPPBPOSUNIT_PLAYLIST;
                endPos.iIndeterminate = true;

                fprintf(iTestMsgOutputFile, "Skipping to clip index %d...\n", iSkippedToClip);
                fprintf(iTestMsgOutputFile, "Session duration %d ms, setting BOT to %d ms...\n", iSessionDuration[iCurrentPlaybackClip], iTargetNPT);
            }

            fprintf(iTestMsgOutputFile, "Calling SetPlaybackRange...\n");

            OSCL_TRY(error, iCurrentCmdId = iPlayer->SetPlaybackRange(beginPos, endPos, false, (OsclAny*) & iContextObject));

            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_RESUME:
        {
            fprintf(iTestMsgOutputFile, "Calling Resume...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Resume((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;


        case STATE_STOP:
        {
            fprintf(iTestMsgOutputFile, "Calling Stop...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Stop((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASINK_AUDIO:
        {
            fprintf(iTestMsgOutputFile, "Removing Audio Sink...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSink(*iDataSinkAudio, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASINK_VIDEO:
        {
            fprintf(iTestMsgOutputFile, "Removing Video Sink...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSink(*iDataSinkVideo, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_RESET:
        {
            fprintf(iTestMsgOutputFile, "Calling Reset...\n");
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Reset((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASOURCE:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSource(*iDataSource, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_CLEANUPANDCOMPLETE:
        {
            fprintf(iTestMsgOutputFile, "Deleting Player...\n");
            PVPATB_TEST_IS_TRUE(PVPlayerFactory::DeletePlayer(iPlayer));
            iPlayer = NULL;

            delete iSourceContextData;
            iSourceContextData = NULL;

            delete iDataSource;
            iDataSource = NULL;

            delete iDataSinkAudio;
            iDataSinkAudio = NULL;

            delete iDataSinkVideo;
            iDataSinkVideo = NULL;

            PVMediaOutputNodeFactory::DeleteMediaOutputNode(iIONodeAudio);
            iIONodeAudio = NULL;

            PVMediaOutputNodeFactory::DeleteMediaOutputNode(iIONodeVideo);
            iIONodeVideo = NULL;

            iMioFactory->DestroyAudioOutput(iMIOFileOutAudio);
            iMIOFileOutAudio = NULL;

            iMioFactory->DestroyAudioOutput(iMIOFileOutVideo);
            iMIOFileOutVideo = NULL;

            ValidateEvents();

            TestCompleted();
        }
        break;

        default:
            break;

    }
}

int32 pvplayer_async_test_playlist_seek_skip::AddDataSource()
{
    int32 error = OsclErrNone;
    OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSource(*iDataSource, (OsclAny*) & iContextObject));
    return error;
}
void pvplayer_async_test_playlist_seek_skip::CommandCompleted(const PVCmdResponse& aResponse)
{
    if (aResponse.GetCmdId() != iCurrentCmdId)
    {
        // Wrong command ID.
        PVPATB_TEST_IS_TRUE(false);
        iState = STATE_CLEANUPANDCOMPLETE;
        RunIfNotReady();
        return;
    }

    if (aResponse.GetContext() != NULL)
    {
        if (aResponse.GetContext() == (OsclAny*)&iContextObject)
        {
            if (iContextObject != iContextObjectRefValue)
            {
                // Context data value was corrupted
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
                return;
            }
        }
        else
        {
            // Context data pointer was corrupted
            PVPATB_TEST_IS_TRUE(false);
            iState = STATE_CLEANUPANDCOMPLETE;
            RunIfNotReady();
            return;
        }
    }

    switch (iState)
    {
        case STATE_ADDDATASOURCE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...AddDataSource Complete\n");
                iState = STATE_INIT;
                RunIfNotReady();
            }
            else
            {
                // AddDataSource failed
                fprintf(iTestMsgOutputFile, "...AddDataSource FAILED\n");
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_INIT:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Init Complete\n");
                iState = STATE_ADDDATASINK_AUDIO;
                RunIfNotReady();
            }
            else
            {
                // Init failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_ADDDATASINK_AUDIO:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Add Audio Sink Complete\n");

                if (iFileType != PVMF_MIME_MP3FF)
                {
                    iState = STATE_ADDDATASINK_VIDEO;
                }
                else
                {
                    iState = STATE_PREPARE;
                }
                RunIfNotReady();
            }
            else
            {
                // AddDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_ADDDATASINK_VIDEO:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Add Video Sink Complete\n");
                iState = STATE_PREPARE;
                RunIfNotReady();
            }
            else
            {
                // AddDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_PREPARE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Prepare Complete\n");
                iState = STATE_START;
                // The delay is added between Prepare and Start to test that player
                // does not start the clock and send playstatus events prior to start.
                RunIfNotReady(200000);
            }
            else
            {
                // Prepare failed
                fprintf(iTestMsgOutputFile, "...Prepare FAILED\n");
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_START:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                iState = STATE_STOP;
                // Do nothing and wait for EOS event
                fprintf(iTestMsgOutputFile, "...Start Complete\n");

                if (PauseSkipToNextTrackResumeTest == iWhichTest)
                {
                    iState = STATE_PAUSE;
                    // Play for half the duration and then Skip
                    RunIfNotReady(iSessionDuration[iCurrentPlaybackClip]*1000 / 2);
                }
                // with partial list, wait for more clips to be initialized
                // with full list, begin skipping, unless it is skipping backward
                else if (!iTestUpdateDataSource && (SkipBwdOneTrackTest != iWhichTest))
                {
                    iState = STATE_SETPLAYBACKRANGE;
                    RunIfNotReady();
                }
                else if (iSeekInCurrentTrack || (SeekBeyondClipDurationTest == iWhichTest))
                {
                    // reposition within the track
                    iState = STATE_SETPLAYBACKRANGE;
                    RunIfNotReady();
                }

                fprintf(iTestMsgOutputFile, "Playback should start with clip index %d...\n", iNumEOSReceived);
            }
            else
            {
                // Start failed
                fprintf(iTestMsgOutputFile, "...Start FAILED\n");
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_PAUSE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Pause Complete\n");
                iState = STATE_SETPLAYBACKRANGE;
                RunIfNotReady(2000*1000); // Wait for 2 seconds
            }
            else
            {
                // Pause failed
                fprintf(iTestMsgOutputFile, "...Pause FAILED\n");
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_UPDATEDATASOURCE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...UpdateDataSource Complete\n");

                if (iTestUpdateDataSource)
                {
                    if (!iSeekInCurrentTrack &&
                            (SeekBeyondClipDurationTest != iWhichTest) &&
                            (((iNumClipsInitialized == MIN_VALID_CLIPS_IN_PLAYLIST) && (SkipBwdOneTrackTest != iWhichTest)) || ((iNumClipsInitialized == MIN_VALID_CLIPS_IN_PLAYLIST + 1) && (SkipBwdOneTrackTest == iWhichTest))))
                    {
                        // if skipping forward,
                        // wait the 2nd clip to be initialized before skipping
                        // if skipping backward,
                        // wait the 2nd clip to start playing before skipping
                        iState = STATE_SETPLAYBACKRANGE;
                        RunIfNotReady();
                    }
                    else
                    {
                        iState = STATE_STOP;
                    }
                }
                else
                {
                    iState = STATE_STOP;
                }
            }
            else
            {
                // Update data source failed, not fatal
                fprintf(iTestMsgOutputFile, "...UpdateDataSource failed - not fatal\n");
            }
            break;

        case STATE_SETPLAYBACKRANGE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...SetPlaybackRange Complete\n");

                if (iSeekInCurrentTrack || (SeekBeyondClipDurationTest == iWhichTest))
                {
                    if (iUpdateDataSource && (iNumClipsAdded < (uint32)iNumClipsInPlaylist))
                    {
                        iUpdateDataSource = false;
                        iState = STATE_UPDATEDATASOURCE;
                        RunIfNotReady();
                    }
                    else
                    {
                        iState = STATE_STOP;
                    }
                }
                else
                {
                    iState = STATE_RESUME;
                    RunIfNotReady();
                }
            }
            else
            {
                // Skip track failed
                if (SeekBeyondClipDurationTest == iWhichTest)
                {
                    // it is expected to fail
                    // keep going
                    if (PVMFErrArgument == aResponse.GetCmdStatus())
                    {
                        fprintf(iTestMsgOutputFile, "...SetPlaybackRange not possible\n");
                        PVPATB_TEST_IS_TRUE(true);
                        iState = STATE_STOP;
                    }
                }
                else
                {
                    if (SkipFwdBeyondListTest == iWhichTest)
                    {
                        if (PVMFErrArgument == aResponse.GetCmdStatus())
                        {
                            fprintf(iTestMsgOutputFile, "...SetPlaybackRange not possible\n");
                            PVPATB_TEST_IS_TRUE(true);
                            iState = STATE_RESET;
                        }
                    }
                    else
                    {
                        fprintf(iTestMsgOutputFile, "...SetPlaybackRange FAILED\n");
                        PVPATB_TEST_IS_TRUE(false);
                        iState = STATE_STOP;
                    }

                    RunIfNotReady();
                }
            }
            break;

        case STATE_RESUME:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                PVPATB_TEST_IS_TRUE(true);
                fprintf(iTestMsgOutputFile, "...Resume Complete\n");

                if (iUpdateDataSource && (iNumClipsAdded < (uint32)iNumClipsInPlaylist))
                {
                    iUpdateDataSource = false;
                    iState = STATE_UPDATEDATASOURCE;
                    RunIfNotReady();
                }
                else
                {
                    iState = STATE_STOP;
                }
            }
            else
            {
                // resume failed
                fprintf(iTestMsgOutputFile, "...Resume FAILED\n");

                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_STOP:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Stop Complete\n");
                iState = STATE_REMOVEDATASINK_AUDIO;
                RunIfNotReady();
            }
            else
            {
                // Stop failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASINK_AUDIO:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Remove Audio Sink Complete\n");
                if (iFileType != PVMF_MIME_MP3FF)
                {
                    iState = STATE_REMOVEDATASINK_VIDEO;
                }
                else
                {
                    iState = STATE_RESET;
                }
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_RESET;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASINK_VIDEO:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Remove Video Sink Complete\n");
                iState = STATE_RESET;
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_RESET;
                RunIfNotReady();
            }
            break;

        case STATE_RESET:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Reset Complete\n");
                iState = STATE_REMOVEDATASOURCE;
                RunIfNotReady();
            }
            else
            {
                // Reset failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASOURCE:
            if (PVMFSuccess == aResponse.GetCmdStatus())
            {
                fprintf(iTestMsgOutputFile, "...Remove DataSource Complete\n");
                PVPATB_TEST_IS_TRUE(true);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSource failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        default:
        {
            // Testing error if this is reached
            PVPATB_TEST_IS_TRUE(false);
            iState = STATE_CLEANUPANDCOMPLETE;
            RunIfNotReady();
        }
        break;
    }
}


void pvplayer_async_test_playlist_seek_skip::HandleErrorEvent(const PVAsyncErrorEvent& aEvent)
{
    switch (aEvent.GetEventType())
    {
        case PVMFErrResourceConfiguration:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrResource:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrCorrupt:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrProcessing:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        default:
            // Unknown error and just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;
    }

    // Wait for engine to handle the error
    Cancel();
}


void pvplayer_async_test_playlist_seek_skip::HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent)
{
    switch (aEvent.GetEventType())
    {
        case PVPlayerInfoClipInitialized:
        {
            uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();
            fprintf(iTestMsgOutputFile, "***** PVPlayerInfoClipInitialized received for clip index %d\n", *clipId);

            bool added = AddEventToSequence(iResultingEvents, PL_CLIP_INIT_EVENT, *clipId, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
            if (!added)
            {
                fprintf(iTestMsgOutputFile, "%%%% PL_CLIP_INIT_EVENT for clip index %d was not added to iResultingEvents vector\n", *clipId);
            }

            iNumClipsInitialized++;
            // if testing UpdateDataSource, add another clip is necessary, then get the metadata which is a longer process
            // otherwise, get the metadata right away
            if (iTestUpdateDataSource && (iNumClipsAdded < (uint32)iNumClipsInPlaylist))
            {
                // make sure we are not in a middle of a setPlaybackRange
                // if so, wait until after Resume to add a new clip
                if (iNumClipsInitialized >= MIN_VALID_CLIPS_IN_PLAYLIST)
                {
                    if (iState != STATE_SETPLAYBACKRANGE)
                    {
                        iState = STATE_UPDATEDATASOURCE;
                        RunIfNotReady();
                    }
                    else
                    {
                        iUpdateDataSource = true;
                    }
                }
            }
            else if (!iTestUpdateDataSource && (SkipBwdOneTrackTest == iWhichTest))
            {
                // 2nd clip is starting, can begin skipping backward one track
                if (iNumClipsInitialized == MIN_VALID_CLIPS_IN_PLAYLIST + 1)
                {
                    iState = STATE_SETPLAYBACKRANGE;
                    RunIfNotReady();
                }
            }
        }
        break;
        case PVMFInfoErrorHandlingStart:
        {
            fprintf(iTestMsgOutputFile, "***** PVMFInfoErrorHandlingStart received\n");
        }
        break;
        case PVMFInfoErrorHandlingComplete:
        {
            fprintf(iTestMsgOutputFile, "***** PVMFInfoErrorHandlingComplete received\n");
            iState = STATE_CLEANUPANDCOMPLETE;
            RunIfNotReady();
        }
        break;
        case PVPlayerInfoClipPlaybackEnded:
        {
            uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();

            iNumEOSReceived++;
            fprintf(iTestMsgOutputFile, "***** PVPlayerInfoClipPlaybackEnded received clip index %d\n", *clipId);

            bool added = AddEventToSequence(iResultingEvents, PL_CLIP_END_EVENT, *clipId, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
            if (!added)
            {
                fprintf(iTestMsgOutputFile, "%%%% PL_CLIP_END_EVENT for clip index %d was not added to iResultingEvents vector\n", *clipId);
            }
            if (*clipId < (uint32)(iNumClipsInPlaylist - 1))
            {
                fprintf(iTestMsgOutputFile, "Clip index %d should be starting...\n", *clipId + 1);
            }

            if (1 == iNumEOSReceived)
            {
                if (SkipFwdLastTrackTest == iWhichTest)
                {
                    if ((uint32)iSkippedToClip == *clipId)
                    {
                        PVPATB_TEST_IS_TRUE(true);
                    }
                    else
                    {
                        PVPATB_TEST_IS_TRUE(false);
                    }
                }
                else if ((SkipFwdOneTrackTest == iWhichTest) || (PauseSkipToNextTrackResumeTest == iWhichTest))
                {
                    if (1 == *clipId)
                    {
                        PVPATB_TEST_IS_TRUE(true);
                    }
                    else
                    {
                        PVPATB_TEST_IS_TRUE(false);
                    }
                }
            }

            if (((SkipFwdOneTrackTest == iWhichTest) || (PauseSkipToNextTrackResumeTest == iWhichTest)) && (*clipId == 1))
            {
                // set end of time
                iSetEOT = true;
                iState = STATE_SETPLAYBACKRANGE;
                RunIfNotReady();
            }
        }
        break;
        case PVPlayerInfoClipPlaybackStarted:
        {
            uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();
            iCurrentPlaybackClip = *clipId;
            iNumClipsStarted++;

            fprintf(iTestMsgOutputFile, "***** PVPlayerInfoClipPlaybackStarted received for clip index %d\n", *clipId);

            bool added = AddEventToSequence(iResultingEvents, PL_CLIP_START_EVENT, *clipId, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
            if (!added)
            {
                fprintf(iTestMsgOutputFile, "%%%% PL_CLIP_START_EVENT for clip index %d was not added to iResultingEvents vector\n", *clipId);
            }

            if ((iSeekInCurrentTrack || (SeekBeyondClipDurationTest == iWhichTest)) && (*clipId != 0))
            {
                // reposition into the track
                iState = STATE_SETPLAYBACKRANGE;
                RunIfNotReady();
            }
        }
        break;
        case PVMFInfoEndOfData:
        {
            uint32 numEOSExpected = PLAYLIST_MAX_NUMCLIPS;
            switch (iWhichTest)
            {
                case SkipFwdLastTrackTest:
                    numEOSExpected = 1;
                    break;
                case SkipFwdOneTrackTest:
                case PauseSkipToNextTrackResumeTest:
                    numEOSExpected = iNumClipsInPlaylist - 1;
                    break;
                case SkipBwdOneTrackTest:
                    numEOSExpected = iNumClipsInPlaylist + 1;
                    break;
                case SkipAndSeekFwdNextTrackTest:
                    numEOSExpected = iNumClipsInPlaylist - 1;
                    break;
                default:
                    break;
            }

            if (numEOSExpected == iNumEOSReceived)
            {
                // all the clips have been played back successfully.
                fprintf(iTestMsgOutputFile, "Playlist playback is complete...\n");
                iState = STATE_STOP;
                RunIfNotReady();
            }

            PVInterface* iface = (PVInterface*)(aEvent.GetEventExtensionInterface());
            if (NULL == iface)
            {
                return;
            }
            PVUuid infomsguuid = PVMFErrorInfoMessageInterfaceUUID;
            PVMFErrorInfoMessageInterface* infomsgiface = NULL;
            if (true == iface->queryInterface(infomsguuid, (PVInterface*&)infomsgiface))
            {
                int32 infocode;
                PVUuid infouuid;
                infomsgiface->GetCodeUUID(infocode, infouuid);
                if ((PVPlayerErrorInfoEventTypesUUID == infouuid) &&
                        (PVPlayerInfoEndOfClipReached == infocode))
                {
                    fprintf(iTestMsgOutputFile, "EOS received. Stopping playback.\n");

                    bool added = AddEventToSequence(iResultingEvents, PL_EOS_EVENT, 0xffffffff, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
                    if (!added)
                    {
                        fprintf(iTestMsgOutputFile, "%%%% PL_EOS_EVENT was not added to iResultingEvents vector\n");
                    }

                    Cancel();
                    RunIfNotReady();
                }
                else if ((PVPlayerErrorInfoEventTypesUUID == infouuid) &&
                         (PVPlayerInfoEndTimeReached  == infocode))
                {
                    fprintf(iTestMsgOutputFile, "EOT received. Stopping playback.\n");

                    bool added = AddEventToSequence(iResultingEvents, PL_EOT_EVENT, 0xffffffff, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
                    if (!added)
                    {
                        fprintf(iTestMsgOutputFile, "%%%% PL_EOT_EVENT was not added to iResultingEvents vector\n");
                    }

                    Cancel();
                    RunIfNotReady();
                }
            }
        }
        break;
        case PVMFInfoPositionStatus:
        {
            PVInterface* iface = (PVInterface*)(aEvent.GetEventExtensionInterface());
            if (NULL == iface)
            {
                return;
            }
            PVUuid infomsguuid = PVMFErrorInfoMessageInterfaceUUID;
            PVMFErrorInfoMessageInterface* infomsgiface = NULL;
            uint32 pbpos = 0;
            if (true == iface->queryInterface(infomsguuid, (PVInterface*&)infomsgiface))
            {
                int32 infocode;
                PVUuid infouuid;
                infomsgiface->GetCodeUUID(infocode, infouuid);
                if ((PVPlayerErrorInfoEventTypesUUID == infouuid) &&
                        (PVPlayerInfoPlaybackPositionStatus == infocode))
                {
                    uint8* localbuf = aEvent.GetLocalBuffer();
                    oscl_memcpy(&pbpos, &(localbuf[4]), 4);
                    fprintf(iTestMsgOutputFile, "---Playback status(time) %d ms\n", pbpos);

                    // playback position should not be greater than clip duration
                    if (((iSessionDuration[iCurrentPlaybackClip] != 0) && (pbpos > iSessionDuration[iCurrentPlaybackClip])) ||
                            ((iSessionDuration[iCurrentPlaybackClip] == 0) && (pbpos >= 60 * 1000)))
                    {
                        PVPATB_TEST_IS_TRUE(false);

                        fprintf(iTestMsgOutputFile, "----- Error playback status %d ms > clip duration %d ms!!!\n",
                                pbpos, iSessionDuration[iCurrentPlaybackClip]);
                    }
                }
            }
        }
        break;
        case PVMFInfoDurationAvailable:
        {
            PVUuid infomsguuid = PVMFDurationInfoMessageInterfaceUUID;
            PVMFDurationInfoMessageInterface* eventMsg = NULL;
            PVInterface* infoExtInterface = aEvent.GetEventExtensionInterface();
            PVInterface* temp = NULL;
            if (infoExtInterface &&
                    infoExtInterface->queryInterface(infomsguuid, temp))
            {
                PVUuid eventuuid;
                int32 infoCode;
                eventMsg = OSCL_STATIC_CAST(PVMFDurationInfoMessageInterface*, temp);
                eventMsg->GetCodeUUID(infoCode, eventuuid);
                if (eventuuid == infomsguuid)
                {
                    uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();
                    iSessionDuration[*clipId] = eventMsg->GetDuration();
                    fprintf(iTestMsgOutputFile, "***** Updated Duration Received for clip index %d = %d ms\n", *clipId, iSessionDuration[*clipId]);
                }
            }
        }
        break;
        case PVMFInfoClipCorrupted:
        {
            uint32 *clipId = (uint32*) aEvent.GetLocalBuffer();

            fprintf(iTestMsgOutputFile, "***** PVMFInfoClipCorrupted received for clip index %d\n", *clipId);

            bool added = AddEventToSequence(iResultingEvents, PL_CLIP_CORRUPT_EVENT, *clipId, PL_GENERATE_TIMESTAMP, 0, iTestMsgOutputFile);
            if (!added)
            {
                fprintf(iTestMsgOutputFile, "%%%% PL_CLIP_CORRUPT_EVENT for clip index %d was not added to iResultingEvents vector\n", *clipId);
            }
        }
        break;
        default:
            break;
    }
}

void pvplayer_async_test_playlist_seek_skip::ValidateEvents()
{
    bool ok = false;
    // count the events per clip and per playlist
    ok = CheckEventSequence(iResultingEvents, iNumClipsInPlaylist, &iResultingPlaylistEventCounts, iResultingClipEventCounts, iWhichTest, iTestMsgOutputFile);
    if ((iNumClipsInPlaylist <= MAX_CLIPS_FOR_PLAYLIST_EVENT_CHECKING) && !ok)
    {
        PVPATB_TEST_IS_TRUE(false);
    }
    // compare the expected and the resulting event counts if default playlist is used
    if (iStaticPlaylist)
    {
        ok = CompareEventCounts(&iResultingPlaylistEventCounts, iResultingClipEventCounts, &iExpectedPlaylistEventCounts, iExpectedClipEventCounts, iTestMsgOutputFile);
        if (!ok)
        {
            PVPATB_TEST_IS_TRUE(false);
        }
    }
}

// verify gapless
//
// pvplayer_async_test_validate_gapless section
//
void pvplayer_async_test_validate_gapless::StartTest()
{
    AddToScheduler();
    iState = STATE_CREATE;
    RunIfNotReady();
}


void pvplayer_async_test_validate_gapless::Run()
{
    int error = 0;

    switch (iState)
    {
        case STATE_CREATE:
        {
            iPlayer = NULL;

            OSCL_TRY(error, iPlayer = PVPlayerFactory::CreatePlayer(this, this, this));
            if (error)
            {
                PVPATB_TEST_IS_TRUE(false);
                TestCompleted();
            }
            else
            {
                iState = STATE_ADDDATASOURCE;
                RunIfNotReady();
            }
        }
        break;

        case STATE_ADDDATASOURCE:
        {
            iDataSource = new PVPlayerDataSourceURL;

            oscl_UTF8ToUnicode(iNewFileName, oscl_strlen(iNewFileName), iTmpWCharBuffer, 512);
            iGaplessWFileName.set(iTmpWCharBuffer, oscl_strlen(iTmpWCharBuffer));
            iDataSource->SetDataSourceURL(iGaplessWFileName);
            iDataSource->SetDataSourceFormatType(iFileType);
            OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSource(*iDataSource, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }

        break;

        case STATE_INIT:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Init((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_ADDDATASINK_VIDEO:
        {
            OSCL_wHeapString<OsclMemAllocator> SinkFileName;
            SinkFileName = OUTPUTNAME_PREPEND_WSTRING;
            SinkFileName += _STRLIT_WCHAR("test_player_gapless_validate_");
            SinkFileName += iSinkFileNameSubString;
            SinkFileName += _STRLIT_WCHAR("_video.dat");

            iMIOFileOutVideo = iMioFactory->CreateVideoOutput((OsclAny*) & SinkFileName, MEDIATYPE_VIDEO, iCompressedVideo);
            iIONodeVideo = PVMediaOutputNodeFactory::CreateMediaOutputNode(iMIOFileOutVideo);
            iDataSinkVideo = new PVPlayerDataSinkPVMFNode;
            ((PVPlayerDataSinkPVMFNode*)iDataSinkVideo)->SetDataSinkNode(iIONodeVideo);

            OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSink(*iDataSinkVideo, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_ADDDATASINK_AUDIO:
        {
            iAudioSinkFileName = OUTPUTNAME_PREPEND_WSTRING;
            iAudioSinkFileName += _STRLIT_WCHAR("test_player_gapless_validate_");
            iAudioSinkFileName += iSinkFileNameSubString;
            iAudioSinkFileName += _STRLIT_WCHAR("_audio.dat");

            iMIOFileOutAudio = iMioFactory->CreateAudioOutput((OsclAny*) & iAudioSinkFileName, NULL/*observer*/, true/*active*/,
                               10/*Queue Limit*/, false, false);
            iIONodeAudio = PVMediaOutputNodeFactory::CreateMediaOutputNode(iMIOFileOutAudio);
            iDataSinkAudio = new PVPlayerDataSinkPVMFNode;
            ((PVPlayerDataSinkPVMFNode*)iDataSinkAudio)->SetDataSinkNode(iIONodeAudio);

            OSCL_TRY(error, iCurrentCmdId = iPlayer->AddDataSink(*iDataSinkAudio, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_PREPARE:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Prepare((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_START:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Start((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_EOSNOTREACHED:
        {
            // EOS event not received so initiate stop
            PVPATB_TEST_IS_TRUE(false);
            iState = STATE_STOP;
            RunIfNotReady();
        }
        break;

        case STATE_STOP:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Stop((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASINK_VIDEO:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSink(*iDataSinkVideo, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASINK_AUDIO:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSink(*iDataSinkAudio, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_RESET:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->Reset((OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_REMOVEDATASOURCE:
        {
            OSCL_TRY(error, iCurrentCmdId = iPlayer->RemoveDataSource(*iDataSource, (OsclAny*) & iContextObject));
            OSCL_FIRST_CATCH_ANY(error, PVPATB_TEST_IS_TRUE(false); iState = STATE_CLEANUPANDCOMPLETE; RunIfNotReady());
        }
        break;

        case STATE_CLEANUPANDCOMPLETE:
        {
            PVPATB_TEST_IS_TRUE(PVPlayerFactory::DeletePlayer(iPlayer));
            iPlayer = NULL;

            delete iDataSource;
            iDataSource = NULL;

            delete iDataSinkVideo;
            iDataSinkVideo = NULL;

            PVMediaOutputNodeFactory::DeleteMediaOutputNode(iIONodeVideo);
            iIONodeVideo = NULL;

            iMioFactory->DestroyVideoOutput(iMIOFileOutVideo);
            iMIOFileOutVideo = NULL;

            delete iDataSinkAudio;
            iDataSinkAudio = NULL;

            PVMediaOutputNodeFactory::DeleteMediaOutputNode(iIONodeAudio);
            iIONodeAudio = NULL;

            iMioFactory->DestroyAudioOutput(iMIOFileOutAudio);
            iMIOFileOutAudio = NULL;

            delete iDataSinkText;
            iDataSinkText = NULL;

            PVMediaOutputNodeFactory::DeleteMediaOutputNode(iIONodeText);
            iIONodeText = NULL;

            iMioFactory->DestroyTextOutput(iMIOFileOutText);
            iMIOFileOutText = NULL;

            bool ok = ValidatePCMOutput(iAudioSinkFileName, iReferencePCMFileSize, iTestMsgOutputFile);
            if (!ok)
            {
                PVPATB_TEST_IS_TRUE(false);
            }
            else
            {
                PVPATB_TEST_IS_TRUE(true);
            }

            TestCompleted();
        }
        break;

        default:
            break;

    }
}


void pvplayer_async_test_validate_gapless::CommandCompleted(const PVCmdResponse& aResponse)
{
    if (aResponse.GetCmdId() != iCurrentCmdId)
    {
        // Wrong command ID.
        PVPATB_TEST_IS_TRUE(false);
        iState = STATE_CLEANUPANDCOMPLETE;
        RunIfNotReady();
        return;
    }

    if (aResponse.GetContext() != NULL)
    {
        if (aResponse.GetContext() == (OsclAny*)&iContextObject)
        {
            if (iContextObject != iContextObjectRefValue)
            {
                // Context data value was corrupted
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
                return;
            }
        }
        else
        {
            // Context data pointer was corrupted
            PVPATB_TEST_IS_TRUE(false);
            iState = STATE_CLEANUPANDCOMPLETE;
            RunIfNotReady();
            return;
        }
    }

    switch (iState)
    {
        case STATE_ADDDATASOURCE:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_INIT;
                RunIfNotReady();
            }
            else
            {
                // AddDataSource failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_INIT:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_ADDDATASINK_VIDEO;
                RunIfNotReady();
            }
            else
            {
                // Init failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_ADDDATASINK_VIDEO:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_ADDDATASINK_AUDIO;
                RunIfNotReady();
            }
            else
            {
                // AddDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_ADDDATASINK_AUDIO:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_PREPARE;
                RunIfNotReady();
            }
            else
            {
                // AddDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_PREPARE:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_START;
                RunIfNotReady();
            }
            else
            {
                // Prepare failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_START:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_EOSNOTREACHED;
                RunIfNotReady(500000000);
            }
            else
            {
                // Start failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_STOP:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_REMOVEDATASINK_VIDEO;
                RunIfNotReady();
            }
            else
            {
                // Stop failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASINK_VIDEO:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_REMOVEDATASINK_AUDIO;
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASINK_AUDIO:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_RESET;
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSink failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_RESET:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                iState = STATE_REMOVEDATASOURCE;
                RunIfNotReady();
            }
            else
            {
                // Reset failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        case STATE_REMOVEDATASOURCE:
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                PVPATB_TEST_IS_TRUE(true);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            else
            {
                // RemoveDataSource failed
                PVPATB_TEST_IS_TRUE(false);
                iState = STATE_CLEANUPANDCOMPLETE;
                RunIfNotReady();
            }
            break;

        default:
        {
            // Testing error if this is reached
            PVPATB_TEST_IS_TRUE(false);
            iState = STATE_CLEANUPANDCOMPLETE;
            RunIfNotReady();
        }
        break;
    }
}


void pvplayer_async_test_validate_gapless::HandleErrorEvent(const PVAsyncErrorEvent& aEvent)
{
    switch (aEvent.GetEventType())
    {
        case PVMFErrResourceConfiguration:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrResource:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrCorrupt:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        case PVMFErrProcessing:
            // Just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;

        default:
            // Unknown error and just log the error
            PVPATB_TEST_IS_TRUE(false);
            break;
    }
}


void pvplayer_async_test_validate_gapless::HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent)
{
    if (aEvent.GetEventType() == PVMFInfoErrorHandlingStart)
    {
        // do nothing
    }
    if (aEvent.GetEventType() == PVMFInfoErrorHandlingComplete)
    {
        iState = STATE_CLEANUPANDCOMPLETE;
        RunIfNotReady();
    }
    // Check for stop time reached event
    if (aEvent.GetEventType() == PVMFInfoEndOfData)
    {
        PVInterface* iface = (PVInterface*)(aEvent.GetEventExtensionInterface());
        if (iface == NULL)
        {
            return;
        }
        PVUuid infomsguuid = PVMFErrorInfoMessageInterfaceUUID;
        PVMFErrorInfoMessageInterface* infomsgiface = NULL;
        if (iface->queryInterface(infomsguuid, (PVInterface*&)infomsgiface) == true)
        {
            int32 infocode;
            PVUuid infouuid;
            infomsgiface->GetCodeUUID(infocode, infouuid);
            if ((infouuid == PVPlayerErrorInfoEventTypesUUID) && (infocode == PVPlayerInfoEndOfClipReached))
            {
                iState = STATE_STOP;
                Cancel();
                RunIfNotReady();
            }
        }
    }
}


