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
#include "lipsync_dummy_input_mio.h"
#include "pv_mime_string_utils.h"
#include "oscl_file_server.h"
#include "oscl_file_io.h"
const uint32 KNumMsgBeforeFlowControl    = 25;
const uint32 Num_Audio_Bytes             = 20;
const uint32 KAudioMicroSecsPerDataEvent = 20000;
const uint32 KVideoMicroSecsPerDataEvent = 100000;
const uint32  TOTAL_BYTES_READ           = 50;
const uint32 One_Yuv_Frame_Size          = 38016;
#define InputFileName       "video_in.yuv420"

#define DUMMY_MEDIADATA_POOLNUM 11

#define VOP_START_BYTE_1 0x00
#define VOP_START_BYTE_2 0x00
#define VOP_START_BYTE_3 0x01
#define VOP_START_BYTE_4 0xB6
#define GOV_START_BYTE_4 0xB3


#define H263_START_BYTE_1 0x00
#define H263_START_BYTE_2 0x00
#define H263_START_BYTE_3 0x80

#define BYTES_FOR_MEMPOOL_STORAGE 38100
#define BYTE_1 1
#define BYTE_2 2
#define BYTE_3 3
#define BYTE_4 4
#define BYTE_8 8
#define BYTE_16 16

// Logging macros
#define LOG_STACK_TRACE(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, m)
#define LOG_DEBUG(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, m)
#define LOG_ERR(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iLogger,PVLOGMSG_ERR,m)

LipSyncDummyInputMIO::LipSyncDummyInputMIO(const LipSyncDummyMIOSettings& aSettings)
        : OsclTimerObject(OsclActiveObject::EPriorityNominal, (aSettings.iMediaFormat.isAudio() ? "DummyAudioInputMIO" : "DummyVideoInputMIO"))
        , iCmdIdCounter(0)
        , iMediaBufferMemPool(NULL)
        , iAudioMIO(aSettings.iMediaFormat.isAudio())
        , iVideoMIO(aSettings.iMediaFormat.isVideo())
        , iState(STATE_IDLE)
        , iThreadLoggedOn(false)
        , iSeqNumCounter(0)
        , iTimestamp(0)
        , iFormat(aSettings.iMediaFormat)
        , iCompressed(iFormat.isCompressed())
        , iMicroSecondsPerDataEvent(0)
        , iSettings(aSettings)
        , iVideoMicroSecondsPerDataEvent(0)
        , iAudioMicroSecondsPerDataEvent(0)
{
    iParams = NULL;
    iAudioOnlyOnce = false;
    iVideoOnlyOnce = false;

    iAudioTimeStamp = 0;
    iVideoTimeStamp = 0;
    iCount = 0;
    iDiffVidAudTS = 0;
    iSqrVidAudTS = 0;
    iRtMnSq = 0;

}

PVMFStatus LipSyncDummyInputMIO::connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver)
{
    if (!aObserver)
    {
        return PVMFFailure;
    }

    int32 err = 0;
    OSCL_TRY(err, iObservers.push_back(aObserver));
    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);
    aSession = (PvmiMIOSession)(iObservers.size() - 1); // Session ID is the index of observer in the vector
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::disconnect(PvmiMIOSession aSession)
{
    uint32 index = (uint32)aSession;
    if (index >= iObservers.size())
    {
        // Invalid session ID
        return PVMFFailure;
    }

    iObservers.erase(iObservers.begin() + index);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PvmiMediaTransfer* LipSyncDummyInputMIO::createMediaTransfer(PvmiMIOSession& aSession,
        PvmiKvp* read_formats,
        int32 read_flags,
        PvmiKvp* write_formats,
        int32 write_flags)
{
    OSCL_UNUSED_ARG(read_formats);
    OSCL_UNUSED_ARG(read_flags);
    OSCL_UNUSED_ARG(write_formats);
    OSCL_UNUSED_ARG(write_flags);

    uint32 index = (uint32)aSession;
    if (index >= iObservers.size())
    {
        // Invalid session ID
        OSCL_LEAVE(OsclErrArgument);
        return NULL;
    }

    return (PvmiMediaTransfer*)this;
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::deleteMediaTransfer(PvmiMIOSession& aSession,
        PvmiMediaTransfer* media_transfer)
{
    uint32 index = (uint32)aSession;
    if (!media_transfer || index >= iObservers.size())
    {
        // Invalid session ID
        OSCL_LEAVE(OsclErrArgument);
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::QueryUUID(const PvmfMimeString& aMimeType,
        Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids,
        bool aExactUuidsOnly,
        const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aMimeType);
    OSCL_UNUSED_ARG(aExactUuidsOnly);

    int32 err = 0;
    OSCL_TRY(err, aUuids.push_back(PVMI_CAPABILITY_AND_CONFIG_PVUUID););
    OSCL_FIRST_CATCH_ANY(err, OSCL_LEAVE(OsclErrNoMemory););

    return AddCmdToQueue(QUERY_UUID, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::QueryInterface(const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    if (aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else
    {
        aInterfacePtr = NULL;
    }

    return AddCmdToQueue(QUERY_INTERFACE, aContext, (OsclAny*)&aInterfacePtr);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::Init(const OsclAny* aContext)
{
    if ((iState != STATE_IDLE) && (iState != STATE_INITIALIZED))
    {
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(INIT, aContext);
}


////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::Start(const OsclAny* aContext)
{
    if (iState != STATE_INITIALIZED
            && iState != STATE_PAUSED
            && iState != STATE_STARTED)
    {
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(START, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::Pause(const OsclAny* aContext)
{
    if (iState != STATE_STARTED && iState != STATE_PAUSED)
    {
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(PAUSE, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::Flush(const OsclAny* aContext)
{
    if (iState != STATE_STARTED || iState != STATE_PAUSED)
    {
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(FLUSH, aContext);
}

PVMFCommandId LipSyncDummyInputMIO::Reset(const OsclAny* aContext)
{
    return AddCmdToQueue(RESET, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_UNUSED_ARG(aTimestamp);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

PVMFCommandId LipSyncDummyInputMIO::DiscardData(const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}


////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::Stop(const OsclAny* aContext)
{
    if (iState != STATE_STARTED
            && iState != STATE_PAUSED
            && iState != STATE_STOPPED)
    {
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(STOP, aContext);
}



////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::AddCmdToQueue(LipSyncDummyInputMIOCmdType aType,
        const OsclAny* aContext, OsclAny* aData1)
{
    if (aType == CMD_DATA_EVENT)
        OSCL_LEAVE(OsclErrArgument);

    LipSyncDummyInputMIOCmd cmd;
    cmd.iType = aType;
    cmd.iContext = OSCL_STATIC_CAST(OsclAny*, aContext);
    cmd.iData1 = aData1;
    cmd.iId = iCmdIdCounter;
    ++iCmdIdCounter;
    iCmdQueue.push_back(cmd);
    RunIfNotReady();
    return cmd.iId;
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::AddDataEventToQueue(uint32 aMicroSecondsToEvent)
{
    LipSyncDummyInputMIOCmd cmd;
    cmd.iType = CMD_DATA_EVENT;
    iCmdQueue.push_back(cmd);
    RunIfNotReady(aMicroSecondsToEvent);
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::DoRequestCompleted(const LipSyncDummyInputMIOCmd& aCmd, PVMFStatus aStatus, OsclAny* aEventData)
{
    PVMFCmdResp response(aCmd.iId, aCmd.iContext, aStatus, aEventData);

    for (uint32 i = 0; i < iObservers.size(); i++)
        iObservers[i]->RequestCompleted(response);
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::ThreadLogon()
{
    if (!iThreadLoggedOn)
    {
        AddToScheduler();
        iLogger = PVLogger::GetLoggerObject("LipSyncDummyInputMIO");
        iThreadLoggedOn = true;
    }
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::ThreadLogoff()
{
    if (iThreadLoggedOn)
    {
        RemoveFromScheduler();
        iLogger = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::CancelAllCommands(const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::CancelCommand(PVMFCommandId aCmdId, const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aCmdId);
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::setPeer(PvmiMediaTransfer* aPeer)
{
    iPeer = aPeer;
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::useMemoryAllocators(OsclMemAllocator* write_alloc)
{
    OSCL_UNUSED_ARG(write_alloc);
    OSCL_LEAVE(OsclErrNotSupported);
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::DoInit()
{
    if (STATE_INITIALIZED == iState)
    {
        return PVMFSuccess;
    }

    // Create memory pool for the media data, using the maximum frame size found earlier
    int32 err = 0;
    OSCL_TRY(err,
             if (iMediaBufferMemPool)
{
    OSCL_DELETE(iMediaBufferMemPool);
        iMediaBufferMemPool = NULL;

    }

    iMediaBufferMemPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator,
                                   (DUMMY_MEDIADATA_POOLNUM, BYTES_FOR_MEMPOOL_STORAGE));
    if (!iMediaBufferMemPool)
    OSCL_LEAVE(OsclErrNoMemory);

            );
    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);


    // The first allocate call will set the chunk size of the memory pool. Use the max
    // frame size calculated earlier to set the chunk size.  The allocated data will be
    // deallocated automatically as tmpPtr goes out of scope.
    OsclAny* tmpPtr = NULL;
    tmpPtr = iMediaBufferMemPool->allocate(BYTES_FOR_MEMPOOL_STORAGE);
    iMediaBufferMemPool->deallocate(tmpPtr);

    if (iAudioMIO)
    {
        iAudioMicroSecondsPerDataEvent = (int32)((1000 / iSettings.iAudioFrameRate)) * 1000;
    }
    else
    {
        iVideoMicroSecondsPerDataEvent = (int32)((1000 / iSettings.iVideoFrameRate)) * 1000;
    }

    iState = STATE_INITIALIZED;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::DoStart()
{
    if (STATE_STARTED == iState)
    {
        return PVMFSuccess;
    }
    iState = STATE_STARTED;
    RunIfNotReady(0);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::DoPause()
{
    iState = STATE_PAUSED;
    return PVMFSuccess;
}

PVMFStatus LipSyncDummyInputMIO::DoReset()
{
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::DoFlush()
{
    // This method should stop capturing media data but continue to send captured
    // media data that is already in buffer and then go to stopped state.
    // However, in this case of file input we do not have such a buffer for
    // captured data, so this behaves the same way as stop.
    return DoStop();
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::DoStop()
{
    iState = STATE_STOPPED;
    if (iCount == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "Can't calculate the value of RMS because the count value is zero. g_count=%d", iCount));
    }
    else
    {
        iRtMnSq = (int32)sqrt(iSqrVidAudTS / iCount);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "RMS value at input side . g_RtMnSq=%d", iRtMnSq));
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::writeAsync(uint8 aFormatType, int32 aFormatIndex,
        uint8* aData, uint32 aDataLen,
        const PvmiMediaXferHeader& data_header_info,
        OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aFormatType);
    OSCL_UNUSED_ARG(aFormatIndex);
    OSCL_UNUSED_ARG(aData);
    OSCL_UNUSED_ARG(aDataLen);
    OSCL_UNUSED_ARG(data_header_info);
    OSCL_UNUSED_ARG(aContext);
    // This is an active data source. writeAsync is not supported.
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}


LipSyncDummyInputMIO::~LipSyncDummyInputMIO()
{
    if (iMediaBufferMemPool)
    {
        OSCL_DELETE(iMediaBufferMemPool);
        iMediaBufferMemPool = NULL;
    }
    // release semaphore
}

void LipSyncDummyInputMIO::Run()
{


    if (!iCmdQueue.empty())
    {
        LipSyncDummyInputMIOCmd cmd = iCmdQueue[0];
        iCmdQueue.erase(iCmdQueue.begin());

        switch (cmd.iType)
        {

            case INIT:
                DoRequestCompleted(cmd, DoInit());
                break;

            case START:
                DoRequestCompleted(cmd, DoStart());
                break;

            case PAUSE:
                DoRequestCompleted(cmd, DoPause());
                break;

            case FLUSH:
                DoRequestCompleted(cmd, DoFlush());
                break;

            case RESET:
                DoRequestCompleted(cmd, DoReset());
                break;

            case STOP:
                DoRequestCompleted(cmd, DoStop());
                break;

            case CMD_DATA_EVENT:
                DoRead();
                break;

            case QUERY_UUID:
            case QUERY_INTERFACE:
                DoRequestCompleted(cmd, PVMFSuccess);
                break;

            case CANCEL_ALL_COMMANDS:
            case CANCEL_COMMAND:
                DoRequestCompleted(cmd, PVMFFailure);
                break;

            default:
                break;
        }
    }

    if (iPeer && iState == STATE_STARTED)
    {
        DoRead();
    }
    if (!iCmdQueue.empty())
    {
        // Run again if there are more things to process
        RunIfNotReady();
    }
}


void LipSyncDummyInputMIO::writeComplete(PVMFStatus aStatus,
        PVMFCommandId write_id,
        OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    if ((aStatus != PVMFSuccess) && (aStatus != PVMFErrCancelled))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "LipSyncDummyInputMIO::writeComplete: Error - writeAsync failed. aStatus=%d", aStatus));
        OSCL_LEAVE(OsclErrGeneral);
    }

    for (int i = iSentMediaData.size() - 1; i >= 0; i--)
    {
        if (iSentMediaData[i].iId == write_id)
        {
            iMediaBufferMemPool->deallocate(iSentMediaData[i].iData);
            iSentMediaData.erase(&iSentMediaData[i]);
            return;
        }
    }

    // Error: unmatching ID.
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                    (0, "LipSyncDummyInputMIO::writeComplete: Error - unmatched cmdId %d failed. QSize %d", write_id, iSentMediaData.size()));

}

PVMFStatus LipSyncDummyInputMIO::DoRead()
{
    // Create new media data buffer
    int32 error = 0;
    uint8* data = NULL;
    uint32 writeAsyncID = 0;
    uint32 bytesToRead = 0;

    //Find the frame...

    if (iVideoMIO)
    {
        iParams = ShareParams::Instance();
        data = (uint8*)iMediaBufferMemPool->allocate(BYTES_FOR_MEMPOOL_STORAGE);
        if ((iSettings.iMediaFormat == PVMF_MIME_M4V) || (iSettings.iMediaFormat == PVMF_MIME_ISO_AVC_SAMPLE_FORMAT))
        {
            bytesToRead = TOTAL_BYTES_READ;
            iTimestamp = (int32)(iSeqNumCounter * 1000 / iSettings.iVideoFrameRate);
            ++iSeqNumCounter;

        }
        else if (iSettings.iMediaFormat == PVMF_MIME_H2631998 ||
                 iSettings.iMediaFormat == PVMF_MIME_H2632000)
        {
            if (error)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "LipSyncDummyInputMIO::No buffer available for video MIO, wait till next data event"))
                RunIfNotReady(iVideoMicroSecondsPerDataEvent);
                return PVMFSuccess;
            }


            bytesToRead = TOTAL_BYTES_READ;
            iTimestamp += (uint32)(iSeqNumCounter * 1000 / iSettings.iVideoFrameRate);
            iVideoTimeStamp = iTimestamp;
            ++iSeqNumCounter;
            iParams->iCompressed = true;
            CalculateRMSInfo(iVideoTimeStamp, iAudioTimeStamp);

        }

        else if (iSettings.iMediaFormat == PVMF_MIME_YUV420 ||
                 iSettings.iMediaFormat == PVMF_MIME_RGB16)
        {
            if (error)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PvmiMIOFileInput::No buffer available for Video MIO, wait till next data event"))
                RunIfNotReady(iVideoMicroSecondsPerDataEvent);
                return PVMFSuccess;
            }

            Oscl_FileServer fs;
            fs.Connect();
            Oscl_File fileYUV;
            if (fileYUV.Open(InputFileName, Oscl_File::MODE_READ , fs))
            {
                fs.Close();
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PvmiMIOFileInput::Eror in reading the yuv file"))
                RunIfNotReady();
                return PVMFFailure;
            }
            fileYUV.Read(data, One_Yuv_Frame_Size, sizeof(char));
            fileYUV.Close();
            fs.Close();
            bytesToRead = One_Yuv_Frame_Size;
            iTimestamp = (int32)(iSeqNumCounter * 1000 / iSettings.iVideoFrameRate);
            iVideoTimeStamp = iTimestamp;
            ++iSeqNumCounter;
            iParams->iUncompressed = true;
            CalculateRMSInfo(iVideoTimeStamp, iAudioTimeStamp);

        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PvmiMIOFileInput::HandleEventPortActivity: Error - Unsupported media format"));
            return PVMFFailure;
        }
        if (iCompressed)
        {
            AddMarkerInfo(data);
        }
        PvmiMediaXferHeader data_hdr;
        data_hdr.seq_num = iSeqNumCounter - 1;
        data_hdr.timestamp = iTimestamp;
        data_hdr.flags = 0;
        data_hdr.duration = 0;
        data_hdr.stream_id = 0;
        data_hdr.private_data_length = 0;

        if (!iPeer)
        {
            iMediaBufferMemPool->deallocate(data);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_NOTICE,
                            (0, "LipSyncDummyInputMIO::DoRead - Peer missing"));
            return PVMFSuccess;
        }

        writeAsyncID = iPeer->writeAsync(PVMI_MEDIAXFER_FMT_TYPE_DATA, 0, data, bytesToRead, data_hdr);

        if (!error)
        {
            // Save the id and data pointer on iSentMediaData queue for writeComplete call
            PvmiDummyMediaData sentData;
            sentData.iId = writeAsyncID;
            sentData.iData = data;
            iSentMediaData.push_back(sentData);
            LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::DoRead stats in writeAsync call ts is %d seqNum is %d, media type is %s",
                             data_hdr.timestamp, data_hdr.seq_num, (iAudioMIO ? "Audio" : "Video")));
        }
        else if (error == OsclErrBusy)
        {
            --iSeqNumCounter;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "LipSyncDummyInputMIO::DoRead error occurred while writeAsync call, media_type is %s", (iAudioMIO ? "Audio" : "Video")))
            iMediaBufferMemPool->deallocate(data);

        }
        else
        {
            iMediaBufferMemPool->deallocate(data);

        }

    }

    else if (iAudioMIO)
    {

        GenerateAudioFrame(data);

        if (iSettings.iMediaFormat == PVMF_MIME_AMR_IF2 ||
                iSettings.iMediaFormat == PVMF_MIME_AMR_IETF ||
                iSettings.iMediaFormat == PVMF_MIME_AMRWB_IETF ||
                iSettings.iMediaFormat == PVMF_MIME_ADTS ||
                iSettings.iMediaFormat == PVMF_MIME_ADIF ||
                iSettings.iMediaFormat == PVMF_MIME_MP3)
        {
            if (error)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PvmiMIOFileInput::No buffer available for audio MIO, wait till next data event"))
                RunIfNotReady(iAudioMicroSecondsPerDataEvent);
                return PVMFSuccess;
            }

        }

        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PvmiMIOFileInput::HandleEventPortActivity: Error - Unsupported media format"));
            return PVMFFailure;
        }

    }

    uint32 aCurrentTime = 0;
    uint32 aInterval = 0;
    uint32 iTsmsec = 0;

    uint32 tick_ct_val = OsclTickCount::TickCount();
    iTsmsec = OsclTickCount::TicksToMsec(tick_ct_val);
    aCurrentTime = iTsmsec * 1000;

    if (iAudioMIO)
    {
        ProcessAudioFrame(aCurrentTime, aInterval);
    }
    else
    {
        ProcessVideoFrame(aCurrentTime, aInterval);
    }



    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::readAsync(uint8* data, uint32 max_data_len,
        OsclAny* aContext, int32* formats, uint16 num_formats)
{
    OSCL_UNUSED_ARG(data);
    OSCL_UNUSED_ARG(max_data_len);
    OSCL_UNUSED_ARG(aContext);
    OSCL_UNUSED_ARG(formats);
    OSCL_UNUSED_ARG(num_formats);
    // This is an active data source. readAsync is not supported.
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::readComplete(PVMFStatus aStatus, PVMFCommandId read_id,
                                        int32 format_index, const PvmiMediaXferHeader& data_header_info,
                                        OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aStatus);
    OSCL_UNUSED_ARG(read_id);
    OSCL_UNUSED_ARG(format_index);
    OSCL_UNUSED_ARG(data_header_info);
    OSCL_UNUSED_ARG(aContext);
    // This is an active data source. readComplete is not supported.
    OSCL_LEAVE(OsclErrNotSupported);
    return;
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::statusUpdate(uint32 status_flags)
{
    if (status_flags == PVMI_MEDIAXFER_STATUS_WRITE)
    {
        if (iAudioMIO)
        {
            RunIfNotReady(iAudioMicroSecondsPerDataEvent);
        }
        else
        {
            RunIfNotReady(iVideoMicroSecondsPerDataEvent);
        }
    }
    else
    {
        // Ideally this routine should update the status of media input component.
        // It should check then for the status. If media input buffer is consumed,
        // media input object should be resheduled.
        // Since the Media fileinput component is designed with single buffer, two
        // asynchronous reads are not possible. So this function will not be required
        // and hence not been implemented.
        OSCL_LEAVE(OsclErrNotSupported);
    }
}


////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::cancelCommand(PVMFCommandId aCmdId)
{
    OSCL_UNUSED_ARG(aCmdId);
    // This cancel command ( with a small "c" in cancel ) is for the media transfer interface.
    // implementation is similar to the cancel command of the media I/O interface.
    OSCL_LEAVE(OsclErrNotSupported);
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::cancelAllCommands()
{
    OSCL_LEAVE(OsclErrNotSupported);
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
{
    OSCL_UNUSED_ARG(aObserver);
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::getParametersSync(PvmiMIOSession session,
        PvmiKeyType identifier,
        PvmiKvp*& parameters,
        int& num_parameter_elements,
        PvmiCapabilityContext context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);

    parameters = NULL;
    num_parameter_elements = 0;
    PVMFStatus status = PVMFFailure;

    if (pv_mime_strcmp(identifier, OUTPUT_FORMATS_CAP_QUERY) == 0 ||
            pv_mime_strcmp(identifier, OUTPUT_FORMATS_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::getParametersSync for OUTPUT_FORMATS_CAP_QUERY"));
        status = AllocateKvp(parameters, (PvmiKeyType)OUTPUT_FORMATS_VALTYPE, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOG_ERR((0, "LipSyncDummyInputMIO::GetOutputParametersSync: Error - AllocateKvp failed. status=%d", status));
        }
        else
        {
            parameters[0].value.pChar_value = OSCL_STATIC_CAST(char*, iFormat.getMIMEStrPtr());

        }
    }
    else if (pv_mime_strcmp(identifier, VIDEO_OUTPUT_WIDTH_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::getParametersSync for VIDEO_OUTPUT_WIDTH_CUR_QUERY"));
        status = AllocateKvp(parameters, (PvmiKeyType)VIDEO_OUTPUT_WIDTH_CUR_VALUE, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOG_ERR((0, "LipSyncDummyInputMIO::GetOutputParametersSync: Error - AllocateKvp failed. status=%d", status));
            return status;
        }

        parameters[0].value.uint32_value = 176;
    }
    else if (pv_mime_strcmp(identifier, VIDEO_OUTPUT_HEIGHT_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::getParametersSync for VIDEO_OUTPUT_HEIGHT_CUR_QUERY"));
        status = AllocateKvp(parameters, (PvmiKeyType)VIDEO_OUTPUT_HEIGHT_CUR_VALUE, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOG_ERR((0, "LipSyncDummyInputMIO::GetOutputParametersSync: Error - AllocateKvp failed. status=%d", status));
            return status;
        }

        parameters[0].value.uint32_value = 144;
    }
    else if (pv_mime_strcmp(identifier, VIDEO_OUTPUT_FRAME_RATE_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::getParametersSync for VIDEO_OUTPUT_FRAME_RATE_CUR_QUERY"));
        status = AllocateKvp(parameters, (PvmiKeyType)VIDEO_OUTPUT_FRAME_RATE_CUR_VALUE, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOG_ERR((0, "LipSyncDummyInputMIO::GetOutputParametersSync: Error - AllocateKvp failed. status=%d", status));
            return status;
        }

        parameters[0].value.float_value = 15;
    }
    else if (pv_mime_strcmp(identifier, OUTPUT_TIMESCALE_CUR_QUERY) == 0)
    {
        LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::getParametersSync for OUTPUT_TIMESCALE_CUR_QUERY"));
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)OUTPUT_TIMESCALE_CUR_VALUE, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOG_ERR((0, "LipSyncDummyInputMIO::GetOutputParametersSync: Error - AllocateKvp failed. status=%d", status));
            return status;
        }
        else
        {
            if (iAudioMIO)
            {
                parameters[0].value.uint32_value = 8000;
            }
            else
            {
                parameters[0].value.uint32_value = 0;
            }
        }
    }
    else if (pv_mime_strcmp(identifier, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
    {
        status = PVMFFailure;
    }
    else if (pv_mime_strcmp(identifier, AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::getParametersSync for AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY"));
        status = AllocateKvp(parameters, (PvmiKeyType)AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOG_ERR((0, "PvmiMIOFileInput::GetOutputParametersSync: Error - AllocateKvp failed. status=%d", status));
            return status;
        }
        parameters[0].value.uint32_value = 8000;
    }

    else if (pv_mime_strcmp(identifier, AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY) == 0)
    {
        LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::getParametersSync for AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY"));
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOG_ERR((0, "PvmiMIOFileInput::GetOutputParametersSync: Error - AllocateKvp failed. status=%d", status));
            return status;
        }
        parameters[0].value.uint32_value = 2;
    }
    return status;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::AllocateKvp(PvmiKvp*& aKvp, PvmiKeyType aKey, int32 aNumParams)
{
    LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::AllocateKvp"));
    uint8* buf = NULL;
    uint32 keyLen = oscl_strlen(aKey) + 1;
    int32 err = 0;

    OSCL_TRY(err,
             buf = (uint8*)iAlloc.allocate(aNumParams * (sizeof(PvmiKvp) + keyLen));
             if (!buf)
             OSCL_LEAVE(OsclErrNoMemory);
            );
    OSCL_FIRST_CATCH_ANY(err,
                         LOG_ERR((0, "LipSyncDummyInputMIO::AllocateKvp: Error - kvp allocation failed"));
                         return PVMFErrNoMemory;
                        );

    int32 i = 0;
    PvmiKvp* curKvp = aKvp = new(buf) PvmiKvp;
    buf += sizeof(PvmiKvp);
    for (i = 1; i < aNumParams; i++)
    {
        curKvp += i;
        curKvp = new(buf) PvmiKvp;
        buf += sizeof(PvmiKvp);
    }

    for (i = 0; i < aNumParams; i++)
    {
        aKvp[i].key = (char*)buf;
        oscl_strncpy(aKvp[i].key, aKey, keyLen);
        buf += keyLen;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam)
{
    OSCL_UNUSED_ARG(aSetParam);
    LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::VerifyAndSetParameter: aKvp=0x%x, aSetParam=%d", aKvp, aSetParam));

    if (!aKvp)
    {
        LOG_ERR((0, "LipSyncDummyInputMIO::VerifyAndSetParameter: Error - Invalid key-value pair"));
        return PVMFFailure;
    }

    if (pv_mime_strcmp(aKvp->key, OUTPUT_FORMATS_VALTYPE) == 0)
    {
        return PVMFSuccess;
    }

    LOG_ERR((0, "LipSyncDummyInputMIO::VerifyAndSetParameter: Error - Unsupported parameter"));
    return PVMFFailure;
}


////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::releaseParameters(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(num_elements);

    if (parameters)
    {
        iAlloc.deallocate((OsclAny*)parameters);
        return PVMFSuccess;
    }
    else
    {
        return PVMFFailure;
    }
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::createContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::setContextParameters(PvmiMIOSession session,
        PvmiCapabilityContext& context,
        PvmiKvp* parameters, int num_parameter_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_parameter_elements);
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::DeleteContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
void LipSyncDummyInputMIO::setParametersSync(PvmiMIOSession session, PvmiKvp* parameters,
        int num_elements, PvmiKvp*& ret_kvp)
{
    OSCL_UNUSED_ARG(session);
    PVMFStatus status = PVMFSuccess;
    ret_kvp = NULL;

    for (int32 i = 0; i < num_elements; i++)
    {
        status = VerifyAndSetParameter(&(parameters[i]), true);
        if (status != PVMFSuccess)
        {
            LOG_ERR((0, "LipSyncDummyInputMIO::setParametersSync: Error - VerifiyAndSetParameter failed on parameter #%d", i));
            ret_kvp = &(parameters[i]);
            OSCL_LEAVE(OsclErrArgument);
        }
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId LipSyncDummyInputMIO::setParametersAsync(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements,
        PvmiKvp*& ret_kvp,
        OsclAny* context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_elements);
    OSCL_UNUSED_ARG(ret_kvp);
    OSCL_UNUSED_ARG(context);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
uint32 LipSyncDummyInputMIO::getCapabilityMetric(PvmiMIOSession session)
{
    OSCL_UNUSED_ARG(session);
    return 0;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus LipSyncDummyInputMIO::verifyParametersSync(PvmiMIOSession session,
        PvmiKvp* parameters, int num_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_elements);
    return PVMFErrNotSupported;
}

void LipSyncDummyInputMIO::AddMarkerInfo(uint8* aData)
{
    if (iFormat == PVMF_MIME_H2632000)
    {
        //Set the first 3 bytes to H263 start of frame pattern
        // This is done so that videoparser nodes send this data out
        // This is useful when the video MIO is compressed.

        oscl_memset(aData, H263_START_BYTE_1, 1);
        oscl_memset(aData + BYTE_1, H263_START_BYTE_2, 1);
        oscl_memset(aData + BYTE_2, H263_START_BYTE_3, 1);
        oscl_memset(aData + BYTE_3, 0, 1);
        //Set the timestamp as the data inside it.
        ((uint32*)aData)[1] = iTimestamp;
    }
    else if (iFormat == PVMF_MIME_M4V)
    {
        //Add marker info for M4V
        oscl_memset(aData, VOP_START_BYTE_1, 1);
        oscl_memset(aData + BYTE_1, VOP_START_BYTE_2, 1);
        oscl_memset(aData + BYTE_2, VOP_START_BYTE_3, 1);
        oscl_memset(aData + BYTE_3, 0, 1);
        //Set the timestamp as the data inside it.
        ((uint32*)aData)[1] = iTimestamp;
    }
}

void LipSyncDummyInputMIO::CalculateRMSInfo(uint32 aVideoData, uint32 aAudioData)
{
    iDiffVidAudTS = aVideoData - aAudioData;
    iSqrVidAudTS += (iDiffVidAudTS * iDiffVidAudTS);
    ++iCount;

}

void LipSyncDummyInputMIO::GenerateAudioFrame(uint8* aData)
{

    uint32 bytesToRead = 0;
    int32 error = 0;
    uint32 writeAsyncID = 0;

    for (uint32 i = 0; i < iSettings.iNumofAudioFrame; i++)
    {
        bytesToRead = Num_Audio_Bytes;

        iTimestamp += (uint32)(iSeqNumCounter * 1000 / iSettings.iAudioFrameRate);
        iAudioTimeStamp = iTimestamp;

        ++iSeqNumCounter;
        aData = (uint8*)iMediaBufferMemPool->allocate(BYTES_FOR_MEMPOOL_STORAGE);
        if (error)
        {
            --iSeqNumCounter;
        }
        ((uint32*)aData)[0] = iTimestamp;
        oscl_memset(aData + BYTE_4, 0, BYTE_16);
        PvmiMediaXferHeader data_hdr;
        data_hdr.seq_num = iSeqNumCounter - 1;
        data_hdr.timestamp = iTimestamp;
        data_hdr.flags = 0;
        data_hdr.duration = 0;
        data_hdr.stream_id = 0;
        data_hdr.private_data_length = 0;


        uint32 err = WriteAsyncCall(error, aData, bytesToRead, data_hdr, writeAsyncID);


        if (!err)
        {
            // Save the id and data pointer on iSentMediaData queue for writeComplete call
            PvmiDummyMediaData sentData;
            sentData.iId = writeAsyncID;
            sentData.iData = aData;
            iSentMediaData.push_back(sentData);
            LOG_STACK_TRACE((0, "LipSyncDummyInputMIO::DoRead stats in writeAsync call ts is %d seqNum is %d, media type is %s",
                             data_hdr.timestamp, data_hdr.seq_num, (iAudioMIO ? "Audio" : "Video")));
        }
        else if (err == OsclErrBusy)
        {
            --iSeqNumCounter;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "LipSyncDummyInputMIO::DoRead error occurred while writeAsync call, media_type is %s", (iAudioMIO ? "Audio" : "Video")))
            iMediaBufferMemPool->deallocate(aData);
        }
        else
        {
            iMediaBufferMemPool->deallocate(aData);
        }


    }

}
int32 LipSyncDummyInputMIO::WriteAsyncCall(int32 &aError, uint8 *aData, uint32 &aBytesToRead, PvmiMediaXferHeader& aData_hdr, uint32 &aWriteAsyncID)
{
    int32 err = OsclErrNone;
    OSCL_TRY(err, aWriteAsyncID = iPeer->writeAsync(PVMI_MEDIAXFER_FMT_TYPE_DATA, 0, aData, aBytesToRead, aData_hdr););
    return err;
}

void LipSyncDummyInputMIO::ProcessAudioFrame(uint32 aCurrentTime, uint32 aInterval)
{

    if (iAudioOnlyOnce == false)
    {
        RunIfNotReady(iAudioMicroSecondsPerDataEvent);
        iAudioOnlyOnce = true;
    }

    else
    {
        if (aCurrentTime < iTimestamp*1000 + iAudioMicroSecondsPerDataEvent)
        {
            aInterval = iTimestamp * 1000 + iAudioMicroSecondsPerDataEvent - aCurrentTime;


        }
        else
        {

            aInterval = iAudioMicroSecondsPerDataEvent;
        }

        RunIfNotReady(aInterval);
    }


}
void LipSyncDummyInputMIO::ProcessVideoFrame(uint32 aCurrentTime, uint32 aInterval)
{
    if (iVideoOnlyOnce == false)
    {
        RunIfNotReady(iVideoMicroSecondsPerDataEvent);
        iVideoOnlyOnce = true;
    }

    else
    {
        if (aCurrentTime < iTimestamp*1000 + iVideoMicroSecondsPerDataEvent)
        {
            aInterval = iTimestamp * 1000 + iVideoMicroSecondsPerDataEvent - aCurrentTime;
            RunIfNotReady(aInterval);
        }
        else
        {
            aInterval = iVideoMicroSecondsPerDataEvent;
        }
        RunIfNotReady(aInterval);
    }
    // Queue the next data event

}
