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
/**
 *
 * @file pvmf_fileoutput_node.cpp
 * @brief Simple file output node. Writes incoming data to specified
 * file
 *
 */

#include "pvmf_fileoutput_inport.h"
#include "pvmf_fileoutput_node.h"
#include "pvlogger.h"
#include "oscl_error_codes.h"
#include "pvmf_media_cmd.h"
#include "pvmf_media_msg_format_ids.h"
#include "pvmf_timedtext.h"

////////////////////////////////////////////////////////////////////////////
PVMFFileOutputNode::PVMFFileOutputNode(int32 aPriority)
        : PVMFNodeInterfaceImpl(aPriority, "PVMFFileOutputNode")
        , iCmdIdCounter(0)
        , iInPort(NULL)
        , iFileHandle(NULL)
        , iFileOpened(0)
        , iFirstMediaData(false)
        , iFormat(PVMF_MIME_FORMAT_UNKNOWN)
        , iMaxFileSizeEnabled(false)
        , iMaxDurationEnabled(false)
        , iMaxFileSize(0)
        , iMaxDuration(0)
        , iFileSize(0)
        , iFileSizeReportEnabled(false)
        , iDurationReportEnabled(false)
        , iFileSizeReportFreq(0)
        , iDurationReportFreq(0)
        , iNextFileSizeReport(0)
        , iNextDurationReport(0)
        , iClock(NULL)
        , iEarlyMargin(DEFAULT_EARLY_MARGIN)
        , iLateMargin(DEFAULT_LATE_MARGIN)
{
    ConstructL();
    int32 err;
    OSCL_TRY(err,

             //Create the port vector.
             iPortVector.Construct(PVMF_FILE_OUTPUT_NODE_PORT_VECTOR_RESERVE);

             //Set the node capability data.
             //This node can support an unlimited number of ports.
             iNodeCapability.iCanSupportMultipleInputPorts = false;
             iNodeCapability.iCanSupportMultipleOutputPorts = false;
             iNodeCapability.iHasMaxNumberOfPorts = true;
             iNodeCapability.iMaxNumberOfPorts = 1;
            );

    if (err != OsclErrNone)
    {
        //if a leave happened, cleanup and re-throw the error
        iPortVector.clear();
        iNodeCapability.iInputFormatCapability.clear();
        iNodeCapability.iOutputFormatCapability.clear();
        OSCL_CLEANUP_BASE_CLASS(PVMFNodeInterfaceImpl);
        OSCL_LEAVE(err);
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFFileOutputNode::~PVMFFileOutputNode()
{
    //Cleanup allocated ports
    if (iInPort)
    {
        OSCL_DELETE(((PVMFFileOutputInPort*)iInPort));
        iInPort = NULL;
    }

    //cleanup port activity events
    iPortActivityQueue.clear();

    if (iAlloc)
    {
        OSCL_DELETE(iAlloc);
    }
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::ConstructL()
{
    iAlloc = (Oscl_DefAlloc*)(new PVMFFileOutputAlloc());
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::CloseOutputFile()
{
    // Close output file
    if (iFileOpened)
    {
        iOutputFile.Close();
        iFs.Close();
        iFileOpened = 0;
    }
}

////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::GetCapability(PVMFNodeCapability& aNodeCapability)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFFileOutputNode:GetCapability"));
    iNodeCapability.iInputFormatCapability.clear();

    if (iFormat != PVMF_MIME_FORMAT_UNKNOWN)
    {
        // Format is already set, so return only that one
        iNodeCapability.iInputFormatCapability.push_back(iFormat);
    }
    else
    {
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_AMR_IETF);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_AMRWB_IETF);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_M4V);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_PCM8);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_PCM16);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_YUV420);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_ADTS);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_H2631998);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_H2632000);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_H264_VIDEO_RAW);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_H264_VIDEO_MP4);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_H264_VIDEO);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_PCM);
        iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_3GPP_TIMEDTEXT);
    }
    aNodeCapability = iNodeCapability;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFPortIter* PVMFFileOutputNode::GetPorts(const PVMFPortFilter* aFilter)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFFileOutputNode:GetPorts"));

    OSCL_UNUSED_ARG(aFilter);//port filter is not implemented.
    iPortVector.Reset();
    return &iPortVector;
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::addRef()
{
    ++iExtensionRefCount;
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::removeRef()
{
    if (iExtensionRefCount > 0)
        --iExtensionRefCount;
}

////////////////////////////////////////////////////////////////////////////
bool PVMFFileOutputNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    if (uuid == PvmfFileOutputNodeConfigUuid)
    {
        PvmfFileOutputNodeConfigInterface* myInterface = OSCL_STATIC_CAST(PvmfFileOutputNodeConfigInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
    }
    else if (uuid == PvmfComposerSizeAndDurationUuid)
    {
        PvmfComposerSizeAndDurationInterface* myInterface = OSCL_STATIC_CAST(PvmfComposerSizeAndDurationInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
    }
    else if (uuid == PvmfNodesSyncControlUuid)
    {
        PvmfNodesSyncControlInterface* myInterface = OSCL_STATIC_CAST(PvmfNodesSyncControlInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
    }
    else if (uuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
    }
    else
    {
        iface = NULL;
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SetOutputFileName(const OSCL_wString& aFileName)
{
    if (iInterfaceState != EPVMFNodeIdle
            && iInterfaceState != EPVMFNodeInitialized
            && iInterfaceState != EPVMFNodeCreated
            && iInterfaceState != EPVMFNodePrepared)
        return false;

    iOutputFileName = aFileName;
    return PVMFSuccess;
}

///////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SetOutputFileDescriptor(const OsclFileHandle* aFileHandle)
{
    if (iInterfaceState != EPVMFNodeIdle
            && iInterfaceState != EPVMFNodeInitialized
            && iInterfaceState != EPVMFNodeCreated
            && iInterfaceState != EPVMFNodePrepared)
        return false;

    iOutputFile.SetPVCacheSize(0);
    iOutputFile.SetAsyncReadBufferSize(0);
    iOutputFile.SetNativeBufferSize(0);
    iOutputFile.SetLoggingEnable(false);
    iOutputFile.SetSummaryStatsLoggingEnable(false);
    iOutputFile.SetFileHandle((OsclFileHandle*)aFileHandle);

    //call open
    int32 retval = iOutputFile.Open(_STRLIT_CHAR("dummy"),
                                    Oscl_File::MODE_READWRITE | Oscl_File::MODE_BINARY,
                                    iFs);

    if (retval == 0)
    {
        iFileOpened = 1;
        iFirstMediaData = true;
        return PVMFSuccess;
    }
    return PVMFFailure;
}
////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SetMaxFileSize(bool aEnable, uint32 aMaxFileSizeBytes)
{
    iMaxFileSizeEnabled = aEnable;
    if (iMaxFileSizeEnabled)
    {
        iMaxFileSize = aMaxFileSizeBytes;
    }
    else
    {
        iMaxFileSize = 0;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::GetMaxFileSizeConfig(bool& aEnable, uint32& aMaxFileSizeBytes)
{
    aEnable = iMaxFileSizeEnabled;
    aMaxFileSizeBytes = iMaxFileSize;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SetMaxDuration(bool aEnable, uint32 aMaxDurationMilliseconds)
{
    iMaxDurationEnabled = aEnable;
    if (iMaxDurationEnabled)
    {
        iMaxDuration = aMaxDurationMilliseconds;
    }
    else
    {
        iMaxDuration = 0;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::GetMaxDurationConfig(bool& aEnable, uint32& aMaxDurationMilliseconds)
{
    aEnable = iMaxDurationEnabled;
    aMaxDurationMilliseconds = iMaxDuration;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SetFileSizeProgressReport(bool aEnable, uint32 aReportFrequency)
{
    iFileSizeReportEnabled = aEnable;
    if (iFileSizeReportEnabled)
    {
        iFileSizeReportFreq = aReportFrequency;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::GetFileSizeProgressReportConfig(bool& aEnable, uint32& aReportFrequency)
{
    aEnable = iFileSizeReportEnabled;
    aReportFrequency = iFileSizeReportFreq;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SetDurationProgressReport(bool aEnable, uint32 aReportFrequency)
{
    iDurationReportEnabled = aEnable;
    if (iDurationReportEnabled)
    {
        iDurationReportFreq = aReportFrequency;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::GetDurationProgressReportConfig(bool& aEnable, uint32& aReportFrequency)
{
    aEnable = iDurationReportEnabled;
    aReportFrequency = iDurationReportFreq;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SetClock(PVMFMediaClock* aClock)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::SetClock: aClock=0x%x", aClock));

    iClock = aClock;
    if (iInPort)
    {
        return ((PVMFFileOutputInPort*)iInPort)->SetClock(aClock);
    }

    return PVMFSuccess;
}

///////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::ChangeClockRate(int32 aRate)
{
    OSCL_UNUSED_ARG(aRate);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::ChangeClockRate: aRate=%d", aRate));

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SetMargins(int32 aEarlyMargin, int32 aLateMargin)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::SetMargins: aEarlyMargin=%d, aLateMargin=%d", aEarlyMargin, aLateMargin));

    iEarlyMargin = aEarlyMargin;
    iLateMargin = aLateMargin;
    if (iInPort)
    {
        return ((PVMFFileOutputInPort*)iInPort)->SetMargins(aEarlyMargin, aLateMargin);
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::ClockStarted(void)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::ClockStarted"));

    if (iInPort)
    {
        ((PVMFFileOutputInPort*)iInPort)->Start();
    }
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::ClockStopped(void)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::ClockStopped"));

    if (iInPort)
    {
        ((PVMFFileOutputInPort*)iInPort)->Pause();
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId PVMFFileOutputNode::SkipMediaData(PVMFSessionId aSessionId,
        PVMFTimestamp aResumeTimestamp,
        uint32 aStreamID,
        bool aPlayBackPositionContinuous,
        OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::SkipMediaData: aResumeTimestamp=%d, aContext=0x%x",
                     aResumeTimestamp, aContext));

    if (!iInPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFFileOutputNode::SkipMediaData: Error - Input port has not been created"));
        OSCL_LEAVE(OsclErrNotReady);
        return 0;
    }

    switch (iInterfaceState)
    {
        case EPVMFNodeStarted:
        case EPVMFNodeInitialized:
        case EPVMFNodePaused:
            return ((PVMFFileOutputInPort*)iInPort)->SkipMediaData(aSessionId, aResumeTimestamp, aStreamID, aPlayBackPositionContinuous, aContext);

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFFileOutputNode::SkipMediaData: Error - Wrong state"));
            OSCL_LEAVE(OsclErrInvalidState);
            return 0;
    }
}

////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::Run()
{
    if (!iInputCommands.empty())
    {
        if (ProcessCommand())
        {
            //note: need to check the state before re-scheduling
            //since the node could have been reset in the ProcessCommand
            //call.
            if (iInterfaceState != EPVMFNodeCreated)
                RunIfNotReady();
            return;
        }
    }

    // Process port activity
    if (!iPortActivityQueue.empty()
            && (iInterfaceState == EPVMFNodeStarted || IsFlushPending()))
    {
        // If the port activity cannot be processed because a port is
        // busy, discard the activity and continue to process the next
        // activity in queue until getting to one that can be processed.
        while (!iPortActivityQueue.empty())
        {
            if (ProcessPortActivity())
                break; //processed a port
        }
        //Re-schedule
        Reschedule();
        return;
    }

    //If we get here we did not process any ports or commands.
    //Check for completion of a flush command...
    if (IsFlushPending()
            && iPortActivityQueue.empty())
    {
        SetState(EPVMFNodePrepared);
        iInPort->ResumeInput();
        CommandComplete(iCurrentCommand, PVMFSuccess);
        RunIfNotReady();
    }
}

void PVMFFileOutputNode::HandlePortActivity(const PVMFPortActivity &aActivity)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "0x%x PVMFFileOutputNode::PortActivity: port=0x%x, type=%d",
                     this, aActivity.iPort, aActivity.iType));

    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
            //An outgoing message was queued on this port.
            //We only need to queue a port activity event on the
            //first message.  Additional events will be queued during
            //the port processing as needed.
            if (aActivity.iPort->OutgoingMsgQueueSize() == 1)
                QueuePortActivity(aActivity);
            break;

        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
            if (aActivity.iPort->IncomingMsgQueueSize() == 1)
                QueuePortActivity(aActivity);
            break;

        case PVMF_PORT_ACTIVITY_DELETED:
            //Report port deleted info event to the node.
            ReportInfoEvent(PVMFInfoPortDeleted
                            , (OsclAny*)aActivity.iPort);
            //Purge any port activity events already queued
            //for this port.
            {
                for (uint32 i = 0; i < iPortActivityQueue.size();)
                {
                    if (iPortActivityQueue[i].iPort == aActivity.iPort)
                        iPortActivityQueue.erase(&iPortActivityQueue[i]);
                    else
                        i++;
                }
            }
            break;

        case PVMF_PORT_ACTIVITY_CONNECT:
            //nothing needed.
            break;

        case PVMF_PORT_ACTIVITY_DISCONNECT:
            //nothing needed.
            break;
        default:
            break;
    }
}

/////////////////////////////////////////////////////
// Port Processing routines
/////////////////////////////////////////////////////

void PVMFFileOutputNode::QueuePortActivity(const PVMFPortActivity &aActivity)
{
    //queue a new port activity event
    int32 err;
    OSCL_TRY(err, iPortActivityQueue.push_back(aActivity););
    if (err != OsclErrNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "0x%x PVMFFileOutputNode::PortActivity: Error - iPortActivityQueue.push_back() failed", this));
        ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aActivity.iPort));
    }
    else
    {
        //wake up the AO to process the port activity event.
        RunIfNotReady();
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::ProcessIncomingData(PVMFSharedMediaDataPtr aMediaData)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::ProcessIncomingData()"));

    PVMFStatus status = PVMFSuccess;

    OsclRefCounterMemFrag frag;
    uint32 numFrags = aMediaData->getNumFragments();
    OsclRefCounterMemFrag formatSpecificInfo;
    aMediaData->getFormatSpecificInfo(formatSpecificInfo);

    for (uint32 i = 0; (i < numFrags) && status == PVMFSuccess; i++)
    {
        aMediaData->getMediaFragment(i, frag);
        switch (iInterfaceState)
        {
            case EPVMFNodeStarted:
                if (iFirstMediaData)
                {
                    status = WriteFormatSpecificInfo(formatSpecificInfo.getMemFragPtr(), formatSpecificInfo.getMemFragSize());
                    if (status != PVMFSuccess)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                        (0, "PVMFFileOutputNode::ProcessIncomingData: Error - WriteFormatSpecificInfo failed"));
                        return PVMFFailure;
                    }
                }

                if (((PVMFFileOutputInPort*)iInPort)->iFormat == PVMF_MIME_3GPP_TIMEDTEXT)
                {
                    PVMFTimedTextMediaData* textmediadata = (PVMFTimedTextMediaData*)(frag.getMemFragPtr());
                    // Output the text sample entry
                    if (textmediadata->iTextSampleEntry.GetRep() != NULL)
                    {
                        PVMFTimedTextMediaData* textmediadata = (PVMFTimedTextMediaData*)(frag.getMemFragPtr());
                        // Output the text sample entry
                        if (textmediadata->iTextSampleEntry.GetRep() != NULL)
                        {
                            // @TODO Write out the text sample entry in a better format
                            status = WriteData((OsclAny*)(textmediadata->iTextSampleEntry.GetRep()), sizeof(PVMFTimedTextSampleEntry));
                            if (status == PVMFFailure)
                            {
                                PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                                (0, "PVMFFileOutputNode::ProcessIncomingData: Error - WriteData failed for text sample entry"));
                                return PVMFFailure;
                            }
                        }
                        // Write out the raw text sample
                        status = WriteData((OsclAny*)(textmediadata->iTextSample), textmediadata->iTextSampleLength);
                        if (status == PVMFFailure)
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                            (0, "PVMFFileOutputNode::ProcessIncomingData: Error - WriteData failed for text sample entry"));
                            return PVMFFailure;
                        }
                    }
                }
                else
                {
                    status = WriteData(frag, aMediaData->getTimestamp());
                    if (status == PVMFFailure)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                        (0, "PVMFFileOutputNode::ProcessIncomingData: Error - WriteData failed"));
                        return PVMFFailure;
                    }
                }

                break;

            case EPVMFNodeInitialized:
                // Already stopped. Ignore incoming data.
                break;

            default:
                // Wrong state
                PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFFileOutputNode::ProcessIncomingData: Error - Wrong state"));
                status = PVMFFailure;
                break;
        }
    }

    return status;
}
//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::WriteFormatSpecificInfo(OsclAny* aPtr, uint32 aSize)
{
    PVMFStatus status = PVMFSuccess;

    if (!iFileOpened)
    {
        if (iFs.Connect() != 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFFileOutputNode::WriteFormatSpecificInfo: iFs.Connect Error."));
            status = PVMFErrNoResources;
            return status;
        }

        if (0 != iOutputFile.Open(iOutputFileName.get_cstr(), Oscl_File::MODE_READWRITE | Oscl_File::MODE_BINARY, iFs))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFFileOutputNode::WriteFormatSpecificInfo: iOutputFile.Open Error."));
            status = PVMFErrNoResources;
            return status;
        }

        iFileOpened = 1;

        iFirstMediaData = true;
    }

    if (iFirstMediaData)
    {
        // Add the amr header if required
        if (((PVMFFileOutputInPort*)iInPort)->iFormat == PVMF_MIME_AMR_IETF)
        {
            // Check if the incoming data has "#!AMR\n" string
            if (aSize < AMR_HEADER_SIZE ||
                    oscl_strncmp((const char*)aPtr, AMR_HEADER, AMR_HEADER_SIZE) != 0)
            {
                // AMR header not found, add AMR header to file first
                status = WriteData((OsclAny*)AMR_HEADER, AMR_HEADER_SIZE);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFFileOutputNode::WriteFormatSpecificInfo: Error - WriteData failed"));
                    return status;
                }
            }
            iFirstMediaData = false;
        }
        // Add the amr-wb header if required
        else if (((PVMFFileOutputInPort*)iInPort)->iFormat == PVMF_MIME_AMRWB_IETF)
        {
            // Check if the incoming data has "#!AMR-WB\n" string
            if (aSize < AMRWB_HEADER_SIZE ||
                    oscl_strncmp((const char*)aPtr, AMRWB_HEADER, AMRWB_HEADER_SIZE) != 0)
            {
                // AMR header not found, add AMR header to file first
                status = WriteData((OsclAny*)AMRWB_HEADER, AMRWB_HEADER_SIZE);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFFileOutputNode::WriteFormatSpecificInfo: Error - WriteData failed"));
                    return status;
                }
            }
            iFirstMediaData = false;
        }
        else if (((PVMFFileOutputInPort*)iInPort)->iFormat == PVMF_MIME_M4V)
        {
            if (aSize > 0)
            {
                status = WriteData(aPtr, aSize);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFFileOutputNode::WriteFormatSpecificInfo: Error - WriteData failed"));
                    return status;
                }
            }
            iFirstMediaData = false;
        }
        else if (((PVMFFileOutputInPort*)iInPort)->iFormat == PVMF_MIME_PCM8)
        {
            if (aSize > 0)
            {
                status = WriteData(aPtr, aSize);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFFileOutputNode::WriteFormatSpecificInfo: Error - WriteData failed"));
                    return status;
                }
            }
            iFirstMediaData = false;
        }
        else if (((PVMFFileOutputInPort*)iInPort)->iFormat == PVMF_MIME_PCM16)
        {
            if (aSize > 0)
            {
                status = WriteData(aPtr, aSize);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFFileOutputNode::WriteFormatSpecificInfo: Error - WriteData failed"));
                    return status;
                }
            }
            iFirstMediaData = false;
        }
        else if (((PVMFFileOutputInPort*)iInPort)->iFormat == PVMF_MIME_3GPP_TIMEDTEXT)
        {
            if (aSize > 0)
            {
                // TODO Write out the text track level info in some formatted way
                status = WriteData(aPtr, aSize);
                if (status != PVMFSuccess)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFFileOutputNode::WriteFormatSpecificInfo: Error - WriteData failed"));
                    return status;
                }
            }
            iFirstMediaData = false;
        }
        else
        {
            iFirstMediaData = false;
        }
    }
    return status;
}


//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SendDurationProgress(uint32 aTimestamp)
{

    if (iDurationReportEnabled &&
            aTimestamp >= iNextDurationReport)
    {
        iNextDurationReport = aTimestamp - (aTimestamp % iDurationReportFreq) + iDurationReportFreq;
        ReportInfoEvent(PVMF_COMPOSER_DURATION_PROGRESS, (OsclAny*)aTimestamp);
    }
    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::SendFileSizeProgress()
{
    if (iFileSizeReportEnabled &&
            iFileSize >= iNextFileSizeReport)
    {
        iNextFileSizeReport = iFileSize - (iFileSize % iFileSizeReportFreq) + iFileSizeReportFreq;
        ReportInfoEvent(PVMF_COMPOSER_FILESIZE_PROGRESS, (OsclAny*)iFileSize);
    }
    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::CheckMaxFileSize(uint32 aFrameSize)
{
    if (iMaxFileSizeEnabled)
    {
        if ((iFileSize + aFrameSize) >= iMaxFileSize)
        {
            // Change state to initialized
            SetState(EPVMFNodeInitialized);

            // Clear all pending port activity
            ClearPendingPortActivity();

            // Report max file size event
            ReportInfoEvent(PVMF_COMPOSER_MAXFILESIZE_REACHED, NULL);
            return PVMFSuccess;
        }

        return PVMFPending;
    }
    return PVMFErrNotSupported;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::CheckMaxDuration(uint32 aTimestamp)
{
    if (iMaxDurationEnabled)
    {
        if (aTimestamp >= iMaxDuration)
        {
            // Change state to initialized
            SetState(EPVMFNodeInitialized);

            // Clear all pending port activity
            ClearPendingPortActivity();

            // Report max duration event
            ReportInfoEvent(PVMF_COMPOSER_MAXDURATION_REACHED, NULL);
            return PVMFSuccess;
        }

        return PVMFPending;
    }
    return PVMFErrNotSupported;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::WriteData(OsclAny* aData, uint32 aSize)
{
    if (!aData || aSize == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFFileOutputNode::WriteData: Error - Invalid data or data size"));
        return PVMFFailure;
    }

    switch (CheckMaxFileSize(aSize))
    {
        case PVMFFailure:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFFileOutputNode::WriteData: Error - CheckMaxFileSize failed"));
            return PVMFFailure;

        case PVMFSuccess:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_DEBUG,
                            (0, "PVMFFileOutputNode::WriteData: Maxmimum file size reached"));
            return PVMFSuccess;

        default:
            break;
    }

    int32 wlength = 0;
    if ((wlength = iOutputFile.Write(aData, sizeof(uint8), aSize)) != (int32)aSize)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFFileOutputNode::WriteData: Error - File write failed"));
        ReportInfoEvent(PVMFInfoProcessingFailure);
        return PVMFFailure;
    }
    else
    {
        iOutputFile.Flush();
    }

    iFileSize += wlength;
    return SendFileSizeProgress();
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::WriteData(OsclRefCounterMemFrag aMemFrag, uint32 aTimestamp)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::WriteData: aTimestamp=%d", aTimestamp));

    switch (CheckMaxDuration(aTimestamp))
    {
        case PVMFFailure:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFFileOutputNode::WriteData: Error - CheckMaxDuration failed"));
            return PVMFFailure;

        case PVMFSuccess:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_DEBUG,
                            (0, "PVMFFileOutputNode::WriteData: Maxmimum duration reached"));
            return PVMFSuccess;

        default:
            break;
    }

    if (WriteData(aMemFrag.getMemFragPtr(), aMemFrag.getMemFragSize()) == PVMFSuccess)
        return SendDurationProgress(aTimestamp);
    else
        return PVMFFailure;
}

//////////////////////////////////////////////////////////////////////////////////
void PVMFFileOutputNode::ClearPendingPortActivity()
{
    // index starts at 1 because the current command (i.e. iCmdQueue[0]) will be erased inside Run
    PVMFNodeCommand Cmd;
    while (iInputCommands.size() > 0)
    {
        iInputCommands.GetFrontAndErase(Cmd);
        CommandComplete(Cmd, PVMFErrCancelled);
    }
}

/////////////////////////////////////////////////////
bool PVMFFileOutputNode::ProcessPortActivity()
{//called by the AO to process a port activity message
    //Pop the queue...
    PVMFPortActivity activity(iPortActivityQueue.front());
    iPortActivityQueue.erase(&iPortActivityQueue.front());

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "0x%x PVMFFileOutputNode::ProcessPortActivity: port=0x%x, type=%d",
                     this, activity.iPort, activity.iType));

    int32 err = OsclErrNone;

    PVMFStatus status = PVMFSuccess;
    switch (activity.iType)
    {
        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
            status = ProcessIncomingMsg(activity.iPort);
            //if there is still data, queue another port activity event.
            if (activity.iPort->IncomingMsgQueueSize() > 0)
            {
                OSCL_TRY(err, iPortActivityQueue.push_back(activity););
            }
            break;
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
        default:
            return false;
    }

    return true;
}

/////////////////////////////////////////////////////
PVMFStatus PVMFFileOutputNode::ProcessIncomingMsg(PVMFPortInterface* aPort)
{
    //Called by the AO to process one buffer off the port's
    //incoming data queue.  This routine will dequeue and
    //dispatch the data.

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "0x%x PVMFFileOutputNode::ProcessIncomingMsg: aPort=0x%x", this, aPort));

    if (aPort->GetPortTag() != PVMF_FILE_OUTPUT_NODE_PORT_TYPE_SINK)
    {
        return PVMFFailure;
    }
    PVMFSharedMediaMsgPtr msg;
    PVMFStatus status = aPort->DequeueIncomingMsg(msg);
    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "0x%x PVMFFileOutputNode::ProcessIncomingMsg: Error - DequeueIncomingMsg failed", this));
        return status;
    }
    /*
       INFORMATION!!!
       The FileOutputNode is generally used by the engine unit tests as SinkNode
       For now, most of the unit tests have OBSOLETED the use of FileOutputNode,
       But still some of the tests are using the FileOutputNode in place of,
       MIO (RefFileOutput).

       Since the usage FileOutputNode is not defined yet, we are adding support for
       BOS Message as a NO-OP so that the node should be able to handle Any and all
       the BOS Messages gracefully.

       IMPORTANT!!!,
       For Complete support of BOS in the FileOutputNode, we need to make more changes.
       Those changes will be done only once the life scope of FileOutputNode is defined.
    */
    if (msg->getFormatID() == PVMF_MEDIA_CMD_BOS_FORMAT_ID)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVFileOutputNode::ProcessIncomingMsg BOS Received"));
        return PVMFSuccess;
    }

    // Transfer to the port's sync queue to do synchronization
    // This is temporary until data is directly saved to the sync queue
    uint32 dropped;
    uint32 skipped;
    status = ((PVMFFileOutputInPort*)aPort)->iDataQueue.QueueMediaData(msg, &dropped, &skipped);
    if (dropped > 0)
    {
        PVMFNodeInterface::ReportInfoEvent(PVMFInfoDataDiscarded);
    }

    if (status == PVMFErrNoMemory)
    {
        return PVMFFailure;
    }
    else
    {
        return PVMFSuccess;
    }
}

PVMFStatus PVMFFileOutputNode::DoQueryInterface()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::DoQueryInterface"));

    PVUuid* uuid;
    PVInterface** ptr;
    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, ptr);

    if (queryInterface(*uuid, *ptr))
    {
        return PVMFSuccess;
    }
    else
    {
        return PVMFFailure;
    }
}

PVMFStatus PVMFFileOutputNode::DoInit()
{
    return PVMFSuccess;
}

PVMFStatus PVMFFileOutputNode::DoStart()
{

    if (EPVMFNodeStarted == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFFileOutputNode::DoStart() already in Started state"));
        return PVMFSuccess;
    }

    if (!iClock)
    {
        // If not using sync clock, start processing incoming data
        ((PVMFFileOutputInPort*)iInPort)->Start();
    }
    if (!iFileOpened)
    {
        if (iFs.Connect() != 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFFileOutputNode::DoStart: iFs.Connect Error."));
            return PVMFErrNoResources;
        }

        if (0 != iOutputFile.Open(iOutputFileName.get_cstr(), Oscl_File::MODE_READWRITE | Oscl_File::MODE_BINARY, iFs))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFFileOutputNode::DoStart: iOutputFile.Open Error."));
            return PVMFErrNoResources;
        }
        iFileOpened = 1;
        iFirstMediaData = true;
    }
    return PVMFSuccess;
}

PVMFStatus PVMFFileOutputNode::DoStop()
{
    // Stop data source
    if (iInPort)
    {
        ((PVMFFileOutputInPort*)iInPort)->Stop();
        CloseOutputFile();
    }

    // Clear queued messages in ports
    for (uint32 i = 0; i < iPortVector.size(); i++)
        iPortVector[i]->ClearMsgQueues();

    // Clear scheduled port activities
    iPortActivityQueue.clear();
    return PVMFSuccess;
}

PVMFStatus PVMFFileOutputNode::DoFlush()
{
    //Notify all ports to suspend their input
    {
        for (uint32 i = 0; i < iPortVector.size(); i++)
            iPortVector[i]->SuspendInput();
    }
    return PVMFPending;
}

PVMFStatus PVMFFileOutputNode::DoPause()
{

    if (EPVMFNodePaused == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFFileOutputNode::DoPause() already in Paused state"));
        return PVMFSuccess;
    }

    // Pause data source
    if (!iClock)
    {
        // If not using sync clock, pause processing of incoming data
        ((PVMFFileOutputInPort*)iInPort)->Pause();
    }
    return PVMFSuccess;
}

PVMFStatus PVMFFileOutputNode::DoReset()
{

    if (IsAdded())
    {
        if (iInPort)
        {
            OSCL_DELETE(((PVMFFileOutputInPort*)iInPort));
            iInPort = NULL;
        }
        return PVMFSuccess;
    }
    OSCL_LEAVE(OsclErrInvalidState);
    return PVMFErrInvalidState;  // to avoid compile warning
}

PVMFStatus PVMFFileOutputNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::DoRequestPort"));
    //This node supports port request from any state

    //retrieve port tag.
    int32 tag;
    OSCL_String* portconfig;

    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, portconfig);

    //validate the tag...
    if (tag != PVMF_FILE_OUTPUT_NODE_PORT_TYPE_SINK)
    {
        //bad port tag
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFFileOutputNode::DoRequestPort: Error - Invalid port tag"));
        return PVMFFailure;
    }

    if (iInPort)
    {
        // it's been taken for now, so reject this request
        return PVMFFailure;
    }

    // Create and configure output port
    int32 err;
    PVMFFormatType fmt = PVMF_MIME_FORMAT_UNKNOWN;
    if (portconfig)
    {
        fmt = portconfig->get_str();
    }

    if (iFormat != PVMF_MIME_FORMAT_UNKNOWN &&
            iFormat != fmt)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFFileOutputNode::DoRequestPort: Error - Format not supported (format was preset)"));
        return PVMFErrArgument;
    }

    OSCL_TRY(err, iInPort = OSCL_NEW(PVMFFileOutputInPort, (tag, this)););
    if (err != OsclErrNone || !iInPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFFileOutputNode::DoRequestPort: Error - PVMFFileOutputInPort::Create() failed"));
        return PVMFErrNoMemory;
    }

    ((PVMFFileOutputInPort*)iInPort)->SetClock(iClock);
    ((PVMFFileOutputInPort*)iInPort)->SetMargins(iEarlyMargin, iLateMargin);

    //if format was provided in mimestring, set it now.
    if (portconfig)
    {
        PVMFFormatType fmt = portconfig->get_str();
        if (fmt != PVMF_MIME_FORMAT_UNKNOWN
                && ((PVMFFileOutputInPort*)iInPort)->IsFormatSupported(fmt))
        {
            ((PVMFFileOutputInPort*)iInPort)->iFormat = fmt;
            ((PVMFFileOutputInPort*)iInPort)->FormatUpdated();
        }
    }

    //Return the port pointer to the caller.
    aPort = iInPort;
    return PVMFSuccess;
}

PVMFStatus PVMFFileOutputNode::DoReleasePort()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFFileOutputNode::DoReleasePort"));

    if (iInPort)
    {
        OSCL_DELETE(((PVMFFileOutputInPort*)iInPort));
        iInPort = NULL;
        return PVMFSuccess;
    }
    return PVMFFailure;
}

PVMFStatus PVMFFileOutputNode::CancelCurrentCommand()
{
    CommandComplete(iCurrentCommand, PVMFErrCancelled);
    return PVMFSuccess;
}

PVMFStatus PVMFFileOutputNode::HandleExtensionAPICommands()
{
    OSCL_ASSERT(false);
    return PVMFErrNotSupported;
}


