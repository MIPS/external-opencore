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
#include "pvmf_omx_audiodec_node.h"
#include "oscl_string_utils.h"

#define CONFIG_SIZE_AND_VERSION(param) \
        param.nSize=sizeof(param); \
        param.nVersion.s.nVersionMajor = SPECVERSIONMAJOR; \
        param.nVersion.s.nVersionMinor = SPECVERSIONMINOR; \
        param.nVersion.s.nRevision = SPECREVISION; \
        param.nVersion.s.nStep = SPECSTEP;



#define PVOMXAUDIODEC_MEDIADATA_POOLNUM 2*NUMBER_OUTPUT_BUFFER
#define PVOMXAUDIODEC_MEDIADATA_CHUNKSIZE 128


// Node default settings

#define PVOMXAUDIODECNODE_CONFIG_MIMETYPE_DEF 0

#define PVMF_OMXAUDIODEC_NUM_METADATA_VALUES 6



// Constant character strings for metadata keys
static const char PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_FORMAT_KEY[] = "codec-info/audio/format";
static const char PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_CHANNELS_KEY[] = "codec-info/audio/channels";
static const char PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_SAMPLERATE_KEY[] = "codec-info/audio/sample-rate";
static const char PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_AVGBITRATE_KEY[] = "codec-info/audio/avgbitrate";
static const char PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_AACOBJECTTYPE_KEY[] = "codec-info/audio/aac-objecttype";
static const char PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_AACSTREAMTYPE_KEY[] = "codec-info/audio/aac-streamtype";


static const char PVOMXAUDIODECMETADATA_SEMICOLON[] = ";";

/////////////////////////////////////////////////////////////////////////////
// Class Destructor
/////////////////////////////////////////////////////////////////////////////
PVMFOMXAudioDecNode::~PVMFOMXAudioDecNode()
{
    DeleteLATMParser();
    ReleaseAllPorts();
    iGaplessLogger = NULL;
}

// Class Constructor
PVMFOMXAudioDecNode::PVMFOMXAudioDecNode(int32 aPriority) :
        PVMFOMXBaseDecNode(aPriority, "PVMFOMXAudioDecNode")
{
    iInterfaceState = EPVMFNodeCreated;

    iNodeConfig.iMimeType = PVOMXAUDIODECNODE_CONFIG_MIMETYPE_DEF;


    int32 err;
    OSCL_TRY(err,

             //Create the input command queue.  Use a reserve to avoid lots of
             //dynamic memory allocation.
             iInputCommands.Construct(PVMF_OMXBASEDEC_NODE_COMMAND_ID_START, PVMF_OMXBASEDEC_NODE_COMMAND_VECTOR_RESERVE);

             //Set the node capability data.
             //This node can support an unlimited number of ports.
             iCapability.iCanSupportMultipleInputPorts = false;
             iCapability.iCanSupportMultipleOutputPorts = false;
             iCapability.iHasMaxNumberOfPorts = true;
             iCapability.iMaxNumberOfPorts = 2;
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_MPEG4_AUDIO);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_3640);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_ADIF);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_LATM);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_ASF_MPEG4_AUDIO);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_AAC_SIZEHDR);

             iCapability.iInputFormatCapability.push_back(PVMF_MIME_AMR_IF2);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_AMR_IETF);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_AMR);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_AMRWB_IETF);
             iCapability.iInputFormatCapability.push_back(PVMF_MIME_AMRWB);

             iCapability.iInputFormatCapability.push_back(PVMF_MIME_MP3);

             iCapability.iInputFormatCapability.push_back(PVMF_MIME_WMA);

             iCapability.iInputFormatCapability.push_back(PVMF_MIME_REAL_AUDIO);

             iCapability.iOutputFormatCapability.push_back(PVMF_MIME_PCM16);

             iAvailableMetadataKeys.reserve(PVMF_OMXAUDIODEC_NUM_METADATA_VALUES);
             iAvailableMetadataKeys.clear();
            );
    // LATM init
    iLATMParser = NULL;
    iLATMConfigBuffer = NULL;
    iLATMConfigBufferSize = 0;

    iCountSamplesInBuffer = false;
    iBufferContainsIntFrames = true;

    iComputeSamplesPerFrame = true;

    // This default value can be changed using cap-config
    iOutputBufferPCMDuration = PVOMXAUDIODEC_DEFAULT_OUTPUTPCM_TIME; // set PCM buffer duration to default (200ms)


    //Try Allocate FSI buffer

    // Do This first in case of Query
    OSCL_TRY(err, iFsiFragmentAlloc.size(PVOMXAUDIODEC_MEDIADATA_POOLNUM, sizeof(channelSampleInfo)));

    OSCL_TRY(err, iPrivateDataFsiFragmentAlloc.size(PVOMXAUDIODEC_MEDIADATA_POOLNUM, sizeof(OsclAny *)));

    iRunlLogger = PVLogger::GetLoggerObject("Run.PVMFOMXAudioDecNode");
    iDataPathLogger = PVLogger::GetLoggerObject("datapath");
    iClockLogger = PVLogger::GetLoggerObject("clock");
    iDiagnosticsLogger = PVLogger::GetLoggerObject("pvplayerdiagnostics.decnode.OMXAudioDecnode");
    iGaplessLogger = PVLogger::GetLoggerObject("gapless.decnode");
}


// this is a utility possibly called multiple times by ProcessIncomingMsg method
// it returns PVMFSuccess if the msg is a cmd msg and is processed
// PVMFFailure if the msg is a cmd msg, but cannot be processed correctly
// PVMFPending if the msg is a data msg
PVMFStatus PVMFOMXAudioDecNode::CheckIfIncomingMsgIsCmd(PVMFSharedMediaMsgPtr msg)
{

    PVMFStatus status = PVMFPending; // assume this is a data message

    if (msg->getFormatID() == PVMF_MEDIA_CMD_BOS_FORMAT_ID)
    {
        //store the stream id and time stamp of bos message
        iStreamID = msg->getStreamID();
        iCurrentClipId = msg->getClipID();

        iClipSampleCount = 0;

        iSendBOS = true;

        // if new BOS arrives, and
        //if we're in the middle of a partial frame assembly
        // abandon it and start fresh
        if (iObtainNewInputBuffer == false)
        {
            if (iInputBufferUnderConstruction != NULL)
            {
                if (iInBufMemoryPool != NULL)
                {
                    iInBufMemoryPool->deallocate((OsclAny *)(iInputBufferUnderConstruction->pMemPoolEntry));
                }
                iInputBufferUnderConstruction = NULL;
            }
            iObtainNewInputBuffer = true;

        }

        // needed to init the sequence numbers and timestamp for partial frame assembly
        iFirstDataMsgAfterBOS = true;
        iKeepDroppingMsgsUntilMarkerBit = false;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: Received BOS stream %d, clipId %d", iStreamID, iCurrentClipId));

        status = PVMFSuccess;
    }
    else if (msg->getFormatID() == PVMF_MEDIA_CMD_EOS_FORMAT_ID)
    {
        // Set EOS flag
        iEndOfDataReached = true;
        // Save the timestamp for the EOS cmd
        iEndOfDataTimestamp = msg->getTimestamp();
        iCurrentClipId = msg->getClipID();

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: Received EOS clipId %d", iCurrentClipId));


        status = PVMFSuccess;
    }
    else if (msg->getFormatID() == PVMF_MEDIA_CMD_BOC_FORMAT_ID)
    {
        // get pointer to the data fragment
        OsclRefCounterMemFrag DataFrag;
        msg->getFormatSpecificInfo(DataFrag);

        // get format specific info
        BOCInfo* bocInfoPtr = (BOCInfo*)DataFrag.getMemFragPtr();
        int32 bocInfoSize = (int32)DataFrag.getMemFragSize();
        OSCL_UNUSED_ARG(bocInfoSize); // bocInfoSize is used only for logging
        if (bocInfoPtr == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: Received BOC, memFragPtr %x, memFragSize %d, but memFragPtr is NULL",
                             bocInfoPtr, bocInfoSize));

            return PVMFFailure;
        }

        iBOCSamplesToSkip = bocInfoPtr->samplesToSkip;
        iBOCTimeStamp = msg->getTimestamp();
        iBOCReceived = true;

        if (iNodeConfig.iMimeType == PVMF_MIME_MP3)
        {
            iBOCSamplesToSkip += MP3_DECODER_DELAY;
        }

        CalculateBOCParameters();

        // only count samples if there is gapless metadata (i.e. BOC is sent)
        iCountSamplesInBuffer = true;
        iClipSampleCount = 0;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: Received BOC, memFragPtr %x, memFragSize %d, samplesToSkipBOC %d",
                         bocInfoPtr, bocInfoSize, bocInfoPtr->samplesToSkip));

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iGaplessLogger, PVLOGMSG_INFO,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg() Gapless BOC, bocInfoPtr->samplesToSkip %d, iBOCSamplesToSkip %d",
                         bocInfoPtr->samplesToSkip, iBOCSamplesToSkip));

        status = PVMFSuccess;
    }
    else if (msg->getFormatID() == PVMF_MEDIA_CMD_EOC_FORMAT_ID)
    {
        // get pointer to the data fragment
        OsclRefCounterMemFrag DataFrag;
        msg->getFormatSpecificInfo(DataFrag);

        // get format specific info
        EOCInfo* eocInfoPtr = (EOCInfo*)DataFrag.getMemFragPtr();
        int32 eocInfoSize = (int32)DataFrag.getMemFragSize();
        OSCL_UNUSED_ARG(eocInfoSize); // eocInfoSize is used only for logging

        if (eocInfoPtr == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: Received EOC, memFragPtr %x, memFragSize %d, but memFragPtr is NULL",
                             eocInfoPtr, eocInfoSize));

            return PVMFFailure;
        }

        iEOCSamplesToSkip = eocInfoPtr->samplesToSkip;
        iEOCFramesToFollow = eocInfoPtr->framesToFollow;
        iEOCTimeStamp = msg->getTimestamp();
        iEOCReceived = true;

        if (iNodeConfig.iMimeType == PVMF_MIME_MP3)
        {
            iEOCSamplesToSkip -= OSCL_MIN(MP3_DECODER_DELAY, iEOCSamplesToSkip);
        }

        CalculateEOCParameters();

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: Received EOC, memFragPtr %x, memFragSize %d, framesToFollowEOC % d, samplesToSkipEOC %d",
                         eocInfoPtr, eocInfoSize, eocInfoPtr->samplesToSkip, eocInfoPtr->framesToFollow));

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iGaplessLogger, PVLOGMSG_INFO,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg() Gapless EOC, framesToFollowEOC %d, eocInfoPtr->samplesToSkip %d, iEOCSamplesToSkip %d",
                         iEOCFramesToFollow, eocInfoPtr->samplesToSkip, iEOCSamplesToSkip));


        status = PVMFSuccess;
    }

    return status;

}
/////////////////////////////////////////////////////////////////////////////
// This routine will process incomming message from the port
/////////////////////////////////////////////////////////////////////////////
bool PVMFOMXAudioDecNode::ProcessIncomingMsg(PVMFPortInterface* aPort)
{
    //Called by the AO to process one buffer off the port's
    //incoming data queue.  This routine will dequeue and
    //dispatch the data.

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "0x%x PVMFOMXAudioDecNode::ProcessIncomingMsg: aPort=0x%x", this, aPort));

    PVMFStatus status = PVMFFailure;
#ifdef SIMULATE_DROP_MSGS
    if ((((PVMFOMXDecPort*)aPort)->iNumFramesConsumed % 300 == 299))  // && (((PVMFOMXDecPort*)aPort)->iNumFramesConsumed < 30) )
    {

        // just dequeue
        PVMFSharedMediaMsgPtr msg;

        status = aPort->DequeueIncomingMsg(msg);
        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
        status = aPort->DequeueIncomingMsg(msg);
        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
        status = aPort->DequeueIncomingMsg(msg);
        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;

#ifdef _DEBUG
        printf("PVMFOMXAudioDecNode::ProcessIncomingMsg() SIMULATED DROP 3 MSGS\n");
#endif


    }
#endif

#ifdef SIMULATE_BOS

    if ((((PVMFOMXDecPort*)aPort)->iNumFramesConsumed == 6))
    {

        PVMFSharedMediaCmdPtr BOSCmdPtr = PVMFMediaCmd::createMediaCmd();

        // Set the format ID to BOS
        BOSCmdPtr->setFormatID(PVMF_MEDIA_CMD_BOS_FORMAT_ID);

        // Set the timestamp
        BOSCmdPtr->setTimestamp(201);
        BOSCmdPtr->setStreamID(0);

        // Convert to media message and send it out
        PVMFSharedMediaMsgPtr mediaMsgOut;
        convertToPVMFMediaCmdMsg(mediaMsgOut, BOSCmdPtr);

        //store the stream id bos message
        iStreamID = mediaMsgOut->getStreamID();

        iSendBOS = true;

#ifdef _DEBUG
        printf("PVMFOMXAudioDecNode::ProcessIncomingMsg() SIMULATED BOS\n");
#endif
        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
        return true;

    }
#endif

#ifdef SIMULATE_PREMATURE_EOS
    if (((PVMFOMXDecPort*)aPort)->iNumFramesConsumed == 5)
    {
        PVMFSharedMediaCmdPtr EOSCmdPtr = PVMFMediaCmd::createMediaCmd();

        // Set the format ID to EOS
        EOSCmdPtr->setFormatID(PVMF_MEDIA_CMD_EOS_FORMAT_ID);

        // Set the timestamp
        EOSCmdPtr->setTimestamp(200);

        // Convert to media message and send it out
        PVMFSharedMediaMsgPtr mediaMsgOut;
        convertToPVMFMediaCmdMsg(mediaMsgOut, EOSCmdPtr);

        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: SIMULATED EOS"));
#ifdef _DEBUG
        printf("PVMFOMXAudioDecNode::ProcessIncomingMsg() SIMULATED EOS\n");
#endif
        // Set EOS flag
        iEndOfDataReached = true;
        // Save the timestamp for the EOS cmd
        iEndOfDataTimestamp = mediaMsgOut->getTimestamp();

        return true;
    }

#endif



    PVMFSharedMediaMsgPtr msg;

    status = aPort->DequeueIncomingMsg(msg);
    if (status != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "0x%x PVMFOMXAudioDecNode::ProcessIncomingMsg: Error - DequeueIncomingMsg failed", this));
        return false;
    }


    // check if incoming msg is a cmd msg
    status = CheckIfIncomingMsgIsCmd(msg);
    if (PVMFSuccess == status)
    {
        // if a cmd msg, it was processed inside a helper method above
        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
        return true;
    }
    else if (PVMFFailure == status)
    {
        // something went wrong when processing the msg
        return false;
    }

    // if we end up here, the msg is a data message

    if (iFirstDataMsgAfterBOS)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iDataPathLogger, PVLOGMSG_INFO,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: iTSOfFirstDataMsgAfterBOS = %d", msg->getTimestamp()));
        iTSOfFirstDataMsgAfterBOS = msg->getTimestamp();
        iInputTimestampClock_LH = iTSOfFirstDataMsgAfterBOS;
        iInputTimestampClock_UH = 0;
    }


    ///////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // For LATM data, need to convert to raw bitstream
    if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_LATM)
    {
        // Keep looping and parsing LATM data until frame complete or data queue runs out
        uint8 retval; //=FRAME_INCOMPLETE;
        // if LATM parser does not exist (very first frame), create it:
        if (iLATMParser == NULL)
        {
            // Create and configure the LATM parser based on the stream MUX config
            // which should be sent as the format specific config in the first media data
            if (CreateLATMParser() != PVMFSuccess)
            {

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::Process Incoming Msg - LATM parser cannot be created"));
                OSCL_ASSERT(false);
                ReportErrorEvent(PVMFErrResourceConfiguration);
                ChangeNodeState(EPVMFNodeError);
                return true;
            }

            // get FSI
            OsclRefCounterMemFrag DataFrag;
            msg->getFormatSpecificInfo(DataFrag);

            //get pointer to the data fragment
            uint8* initbuffer = (uint8 *) DataFrag.getMemFragPtr();
            uint32 initbufsize = (int32) DataFrag.getMemFragSize();

            iLATMConfigBufferSize = initbufsize;
            iLATMConfigBuffer = iLATMParser->ParseStreamMuxConfig(initbuffer, (int32 *) & iLATMConfigBufferSize);
            if (iLATMConfigBuffer == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg() LATM Stream MUX config parsing failed"));
                OSCL_ASSERT(false);
                ReportErrorEvent(PVMFErrResourceConfiguration);
                ChangeNodeState(EPVMFNodeError);
                return true;
            }

        }

        do
        {

            // if we end up here, the msg is a data message

            // Convert the next input media msg to media data
            PVMFSharedMediaDataPtr mediaData;
            convertToPVMFMediaData(mediaData, msg);


            ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iDataPathLogger, PVLOGMSG_INFO,
                            (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: TS=%d, SEQNUM= %d", msg->getTimestamp(), msg->getSeqNum()));


            // Convert the LATM data to raw bitstream
            retval = iLATMParser->compose(mediaData);

            // if frame is complete, break out of the loop
            if (retval != FRAME_INCOMPLETE && retval != FRAME_ERROR)
                break;

            // Log parser error
            if (retval == FRAME_ERROR)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFAACDecNode::GetInputMediaData() LATM parser error"));
            }

            // frame is not complete, keep looping
            if (aPort->IncomingMsgQueueSize() == 0)
            {
                // no more data in the input port queue, unbind current msg, and return
                msg.Unbind();
                // enable reading more data from port
                break;
            }
            else
            {
                msg.Unbind();
                aPort->DequeueIncomingMsg(msg); // dequeue the message directly from input port

            }

            // check if incoming msg is a cmd msg
            status = CheckIfIncomingMsgIsCmd(msg);
            if (PVMFSuccess == status)
            {
                // if a cmd msg, it was processed inside a helper method above
                ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
                return true;
            }
            else if (PVMFFailure == status)
            {
                // something went wrong when processing the msg
                return false;
            }

        }
        while ((retval == FRAME_INCOMPLETE || retval == FRAME_ERROR));

        if (retval == FRAME_COMPLETE)
        {
            // Save the media data containing the parser data as the input media data
            iDataIn = iLATMParser->GetOutputBuffer();
            // set the MARKER bit on the data msg, since this is a complete frame produced by LATM parser
            iDataIn->setMarkerInfo(PVMF_MEDIA_DATA_MARKER_INFO_M_BIT);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: - LATM frame assembled"));

        }
        else if ((retval == FRAME_INCOMPLETE) || (retval == FRAME_ERROR))
        {
            // Do nothing and wait for more data to come in
            PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: - incomplete LATM"));
            // return immediately (i.e. don't assign anything to iDataIn, which will prevent
            // processing
            return true;
        }
        else if (retval == FRAME_OUTPUTNOTAVAILABLE)
        {
            // This should not happen since this node processes one parsed media data at a time
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: LATM parser OUTPUT NOT AVAILABLE"));

            msg.Unbind();

            OSCL_ASSERT(false);
            ReportErrorEvent(PVMFErrResourceConfiguration);
            ChangeNodeState(EPVMFNodeError);

            return true;
        }
    }
/////////////////////////////////////////////////////////
    //////////////////////////
    else
    {
        // regular (i.e. Non-LATM case)
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iDataPathLogger, PVLOGMSG_INFO,
                        (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg: TS=%d, SEQNUM= %d", msg->getTimestamp(), msg->getSeqNum()));


        convertToPVMFMediaData(iDataIn, msg);
        ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed++;
    }

    iCurrFragNum = 0; // for new message, reset the fragment counter
    iIsNewDataFragment = true;


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXAudioDecNode::ProcessIncomingMsg() Received %d frames", ((PVMFOMXDecPort*)aPort)->iNumFramesConsumed));

    //return true if we processed an activity...
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// This routine will handle the PortReEnable state
/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXAudioDecNode::HandlePortReEnable()
{
    OMX_ERRORTYPE Err;
    // set the port index so that we get parameters for the proper port
    iParamPort.nPortIndex = iPortIndexForDynamicReconfig;

    CONFIG_SIZE_AND_VERSION(iParamPort);

    // get new parameters of the port
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Problem getting parameters at port %d", iPortIndexForDynamicReconfig));

        SetState(EPVMFNodeError);
        ReportErrorEvent(PVMFErrResource);
        return PVMFErrResource;
    }


    // get also input info (for frame duration if necessary)
    OMX_PTR CodecProfilePtr;
    OMX_INDEXTYPE CodecProfileIndx;
    OMX_AUDIO_PARAM_AACPROFILETYPE Audio_Aac_Param;
    OMX_AUDIO_PARAM_RATYPE Audio_Ra_Param;

    // determine the proper index and structure (based on codec type)
    if (iInPort)
    {
        // AAC
        if (((PVMFOMXDecPort*)iInPort)->iFormat ==  PVMF_MIME_MPEG4_AUDIO ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_3640 ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_LATM ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ADIF ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ASF_MPEG4_AUDIO ||
                ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AAC_SIZEHDR) // for testing
        {
            CodecProfilePtr = (OMX_PTR) & Audio_Aac_Param;
            CodecProfileIndx = OMX_IndexParamAudioAac;
            Audio_Aac_Param.nPortIndex = iInputPortIndex;

            CONFIG_SIZE_AND_VERSION(Audio_Aac_Param);


            // get parameters:
            Err = OMX_GetParameter(iOMXDecoder, CodecProfileIndx, CodecProfilePtr);
            if (Err != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Input port parameters problem"));

                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrResource);
                return PVMFErrResource;
            }
        }
        else if (((PVMFOMXDecPort*)iInPort)->iFormat ==  PVMF_MIME_REAL_AUDIO)
        {
            CodecProfilePtr = (OMX_PTR) & Audio_Ra_Param;
            CodecProfileIndx = OMX_IndexParamAudioRa;
            Audio_Ra_Param.nPortIndex = iInputPortIndex;

            CONFIG_SIZE_AND_VERSION(Audio_Ra_Param);


            // get parameters:
            Err = OMX_GetParameter(iOMXDecoder, CodecProfileIndx, CodecProfilePtr);
            if (Err != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Input port parameters problem"));

                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrResource);
                return PVMFErrResource;
            }
        }
        // for AMR, frame sizes are known, no need to get the parameters
        // for MP3, frame sizes cannot be obtained through OMX params
        // for WMA, frame sizes cannot be obtained through OMX params
    }



    PVMFFormatType Format = PVMF_MIME_FORMAT_UNKNOWN;
    if (iInPort != NULL)
    {
        Format = ((PVMFOMXDecPort*)iInPort)->iFormat;
    }
    if (Format ==  PVMF_MIME_MPEG4_AUDIO ||
            Format == PVMF_MIME_3640 ||
            Format == PVMF_MIME_LATM ||
            Format == PVMF_MIME_ADIF ||
            Format == PVMF_MIME_ASF_MPEG4_AUDIO ||
            Format == PVMF_MIME_AAC_SIZEHDR) // for testing
    {
        iSamplesPerFrame = Audio_Aac_Param.nFrameLength;
    }
    // AMR
    else if (Format == PVMF_MIME_AMR_IF2 ||
             Format == PVMF_MIME_AMR_IETF ||
             Format == PVMF_MIME_AMR)
    {
        // AMR NB has fs=8khz Mono and the frame is 20ms long, i.e. there is 160 samples per frame
        iSamplesPerFrame = PVOMXAUDIODEC_AMRNB_SAMPLES_PER_FRAME;
    }
    else if (Format == PVMF_MIME_AMRWB_IETF ||
             Format == PVMF_MIME_AMRWB)
    {
        // AMR WB has fs=16khz Mono and the frame is 20ms long, i.e. there is 320 samples per frame
        iSamplesPerFrame = PVOMXAUDIODEC_AMRWB_SAMPLES_PER_FRAME;
    }
    else if (Format == PVMF_MIME_MP3)
    {
        // frame size is either 576 or 1152 samples per frame. However, this information cannot be
        // obtained through OMX MP3 Params. Assume that it's 1152
        iSamplesPerFrame = PVOMXAUDIODEC_MP3_DEFAULT_SAMPLES_PER_FRAME;
    }
    else if (Format == PVMF_MIME_WMA)
    {
        // output frame size is unknown in WMA. However, the PV-WMA decoder can control the number
        // of samples it places in an output buffer, so we can create an output buffer of arbitrary size
        // and let the decoder control how it is filled
        iSamplesPerFrame = 0; // unknown
    }
    else if (Format == PVMF_MIME_REAL_AUDIO)
    {
        iSamplesPerFrame = Audio_Ra_Param.nSamplePerFrame;
    }


    // is this output port?
    if (iPortIndexForDynamicReconfig == iOutputPortIndex)
    {

        uint32 iBeforeConfigNumOutputBuffers = iNumOutputBuffers;

        // GET the output buffer params and sizes
        OMX_AUDIO_PARAM_PCMMODETYPE Audio_Pcm_Param;
        Audio_Pcm_Param.nPortIndex = iOutputPortIndex; // we're looking for output port params

        CONFIG_SIZE_AND_VERSION(Audio_Pcm_Param);



        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamAudioPcm, &Audio_Pcm_Param);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot get component output parameters"));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrResource);
            return PVMFErrResource;
        }

        iPCMSamplingRate = Audio_Pcm_Param.nSamplingRate; // can be set to 0 (if unknown)

        if (iPCMSamplingRate == 0) // use default sampling rate (i.e. 48000)
            iPCMSamplingRate = PVOMXAUDIODEC_DEFAULT_SAMPLINGRATE;

        iNumberOfAudioChannels = Audio_Pcm_Param.nChannels;     // should be 1 or 2
        if (iNumberOfAudioChannels != 1 && iNumberOfAudioChannels != 2)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Output parameters num channels = %d", iNumberOfAudioChannels));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrResource);
            return PVMFErrResource;
        }

        if ((iSamplesPerFrame != 0) && ((iSamplesPerFrame * 1000) > iPCMSamplingRate))
            // if this iSamplesPerFrame is known and is large enough to ensure that the iMilliSecPerFrame calculation
            // below won't be set to 0.
        {
            // CALCULATE NumBytes per frame, Msec per frame, etc.
            iNumBytesPerFrame = 2 * iSamplesPerFrame * iNumberOfAudioChannels;
            iMilliSecPerFrame = (iSamplesPerFrame * 1000) / iPCMSamplingRate;
            // Determine the size of each PCM output buffer. Size would be big enough to hold certain time amount of PCM data
            uint32 numframes = iOutputBufferPCMDuration / iMilliSecPerFrame;

            if (iOutputBufferPCMDuration % iMilliSecPerFrame)
            {
                // If there is a remainder, include one more frame
                ++numframes;
            }

            // set the output buffer size accordingly:
            iOMXComponentOutputBufferSize = numframes * iNumBytesPerFrame;
        }
        else
            iOMXComponentOutputBufferSize = (2 * iNumberOfAudioChannels * iOutputBufferPCMDuration * iPCMSamplingRate) / 1000; // assuming 16 bits per sample

        if (iOMXComponentOutputBufferSize < iParamPort.nBufferSize)
        {
            // the OMX spec says that nBuffersize is a read only field, but the client is allowed to allocate
            // a buffer size larger than nBufferSize.
            iOMXComponentOutputBufferSize = iParamPort.nBufferSize;
        }

        if (iBOCReceived)
            CalculateBOCParameters();

        if (iEOCReceived)
            CalculateEOCParameters();

        // read the alignment again - just in case
        iOutputBufferAlignment = iParamPort.nBufferAlignment;

        iNumOutputBuffers = iParamPort.nBufferCountActual;

        if (iNumOutputBuffers > NUMBER_OUTPUT_BUFFER)
            iNumOutputBuffers = NUMBER_OUTPUT_BUFFER; // make sure number of output buffers is not larger than port queue size

        // do we need to increase the number of buffers?
        if (iNumOutputBuffers < iParamPort.nBufferCountMin)
            iNumOutputBuffers = iParamPort.nBufferCountMin;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::HandlePortReEnable() new output buffers %d, size %d", iNumOutputBuffers, iOMXComponentOutputBufferSize));

        // assume that the following will fail (it's a special type of buffer allocator). If so - we'd need to
        // send the FSI at a later time.
        sendFsi = true;
        iCompactFSISettingSucceeded = false;

        //Try querying the buffer allocator KVP for output buffer allocation outside of the node
        PvmiKvp* pkvp = NULL;
        int NumKvp = 0;
        PvmiKeyType aKvpIdentifier = (PvmiKeyType)PVMF_SUPPORT_FOR_BUFFER_ALLOCATOR_IN_MIO_KEY;
        // Check if Fsi configuration need to be sent
        // Send it only if the MIO supports the buffer allocator
        if (((PVMFOMXDecPort*)iOutPort)->pvmiGetBufferAllocatorSpecificInfoSync(aKvpIdentifier, pkvp, NumKvp) == PVMFSuccess)
        {
            //Send the FSI information to media output node here, before setting output
            //port parameters to the omx component
            int fsiErrorCode = 0;
            OsclRefCounterMemFrag FsiMemfrag;

            OSCL_TRY(fsiErrorCode, FsiMemfrag = iFsiFragmentAlloc.get(););

            OSCL_FIRST_CATCH_ANY(fsiErrorCode, PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                 (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Failed to allocate memory for  FSI")));

            if (fsiErrorCode == 0)
            {
                channelSampleInfo* pcminfo = (channelSampleInfo*) FsiMemfrag.getMemFragPtr();

                if (pcminfo != NULL)
                {
                    OSCL_ASSERT(pcminfo != NULL);

                    pcminfo->samplingRate    = iPCMSamplingRate;
                    pcminfo->desiredChannels = iNumberOfAudioChannels;
                    pcminfo->bitsPerSample = 16;
                    pcminfo->num_buffers = iNumOutputBuffers;
                    pcminfo->buffer_size = iOMXComponentOutputBufferSize;

                    OsclMemAllocator alloc;
                    int32 KeyLength = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM) + 1;
                    PvmiKeyType KvpKey = (PvmiKeyType)alloc.ALLOCATE(KeyLength);

                    if (NULL == KvpKey)
                    {
                        return false;
                    }

                    oscl_strncpy(KvpKey, PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM, KeyLength);
                    int32 err;

                    OSCL_TRY(err, ((PVMFOMXDecPort*)iOutPort)->pvmiSetPortFormatSpecificInfoSync(FsiMemfrag, KvpKey););

                    if (err != OsclErrNone)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "PVMFOMXAudioDecNode::HandlePortReEnable - Problem to set FSI"));
                    }
                    else
                    {
                        // FSI setting succeeded. No need to repeat it later
                        sendFsi = false;
                        iCompactFSISettingSucceeded = true;
                    }

                    alloc.deallocate((OsclAny*)(KvpKey));
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "PVMFOMXAudioDecNode::HandlePortReEnable - Problem allocating Output FSI"));
                    SetState(EPVMFNodeError);
                    ReportErrorEvent(PVMFErrNoMemory);
                    return false; // this is going to make everything go out of scope
                }
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXAudioDecNode::HandlePortReEnable - Problem allocating Output FSI"));
                return false; // this is going to make everything go out of scope
            }

        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable Warning- It is OK for this KVP to fail"
                             "Do NOT attempt to fix the failure in the MIO unless you absolutely want to implement"
                             "the MIO BUFFER ALLOCATOR - see documentation"));


            ReportInfoEvent(PVMFPvmiBufferAllocatorNotAcquired);
        }

        //Buffer allocator kvp query and allocation has to be done again if we landed into handle port reconfiguration

        PvmiKvp* kvp = NULL;
        int numKvp = 0;
        PvmiKeyType aIdentifier = (PvmiKeyType)PVMF_BUFFER_ALLOCATOR_KEY;
        int32 err, err1;
        if (ipExternalOutputBufferAllocatorInterface)
        {
            ipExternalOutputBufferAllocatorInterface->removeRef();
            ipExternalOutputBufferAllocatorInterface = NULL;
        }

        OSCL_TRY(err, ((PVMFOMXDecPort*)iOutPort)->pvmiGetBufferAllocatorSpecificInfoSync(aIdentifier, kvp, numKvp););

        if ((err == OsclErrNone) && (NULL != kvp))
        {
            ipExternalOutputBufferAllocatorInterface = (PVInterface *)kvp->value.key_specific_value;

            if (ipExternalOutputBufferAllocatorInterface)
            {
                PVInterface* pTempPVInterfacePtr = NULL;
                OSCL_TRY(err, ipExternalOutputBufferAllocatorInterface->queryInterface(PVMFFixedSizeBufferAllocUUID, pTempPVInterfacePtr););

                OSCL_TRY(err1, ((PVMFOMXDecPort*)iOutPort)->releaseParametersSync(kvp, numKvp););

                if (err1 != OsclErrNone)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "PVMFOMXAudioDecNode::HandlePortReEnable - Unable to Release Parameters"));
                }

                if ((err == OsclErrNone) && (NULL != pTempPVInterfacePtr))
                {
                    ipFixedSizeBufferAlloc = OSCL_STATIC_CAST(PVMFFixedSizeBufferAlloc*, pTempPVInterfacePtr);

                    uint32 iNumBuffers, iBufferSize;

                    iNumBuffers = ipFixedSizeBufferAlloc->getNumBuffers();
                    iBufferSize = ipFixedSizeBufferAlloc->getBufferSize();

                    if ((iNumBuffers < iParamPort.nBufferCountMin) || (iBufferSize < iOMXComponentOutputBufferSize))
                    {
                        ipExternalOutputBufferAllocatorInterface->removeRef();
                        ipExternalOutputBufferAllocatorInterface = NULL;
                    }
                    else
                    {
                        iNumOutputBuffers = iNumBuffers;
                        iOMXComponentOutputBufferSize = iBufferSize;
                        ReportInfoEvent(PVMFPvmiBufferAlloctorAcquired);
                    }

                }
                else
                {
                    ipExternalOutputBufferAllocatorInterface->removeRef();
                    ipExternalOutputBufferAllocatorInterface = NULL;
                }
            }
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable Warning- It is OK for this KVP to fail"
                             "Do NOT attempt to fix the failure in the MIO unless you absolutely want to implement"
                             "the MIO BUFFER ALLOCATOR -see documentation"));

            ReportInfoEvent(PVMFPvmiBufferAllocatorNotAcquired);
        }


        if (iNumOutputBuffers > iBeforeConfigNumOutputBuffers)
        {
            //Reallocate FillBufferDone THREADSAFE CALLBACK AOs in case of port reconfiguration
            if (iThreadSafeHandlerFillBufferDone)
            {
                OSCL_DELETE(iThreadSafeHandlerFillBufferDone);
                iThreadSafeHandlerFillBufferDone = NULL;
            }
            // use the new queue depth of iNumOutputBuffers to prevent deadlock
            iThreadSafeHandlerFillBufferDone = OSCL_NEW(FillBufferDoneThreadSafeCallbackAO, (this, iNumOutputBuffers, "FillBufferDoneAO", Priority() + 1));

            if (NULL == iThreadSafeHandlerFillBufferDone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Can't reallocate FillBufferDone threadsafe callback queue!"));
                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrNoMemory);
                return false;
            }
        }

        // send command for port re-enabling (for this to happen, we must first recreate the buffers)
        Err = OMX_SendCommand(iOMXDecoder, OMX_CommandPortEnable, iPortIndexForDynamicReconfig, NULL);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> problem sending Port Enable command at port %d", iPortIndexForDynamicReconfig));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrResource);
            return PVMFErrResource;
        }

        /* Allocate output buffers */
        if (!CreateOutMemPool(iNumOutputBuffers))
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot allocate output buffers "));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return PVMFErrNoMemory;
        }




        if (!ProvideBuffersToComponent(iOutBufMemoryPool, // allocator
                                       iOutputAllocSize,     // size to allocate from pool (hdr only or hdr+ buffer)
                                       iNumOutputBuffers, // number of buffers
                                       iOMXComponentOutputBufferSize, // actual buffer size
                                       iOutputPortIndex, // port idx
                                       iOMXComponentSupportsExternalOutputBufferAlloc, // can component use OMX_UseBuffer
                                       false // this is not input
                                      ))
        {


            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot provide output buffers to component"));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return PVMFErrNoMemory;

        }

        // do not drop output any more, i.e. enable output to be sent downstream
        iDoNotSendOutputBuffersDownstreamFlag = false;


    }
    else
    {
        // this is input port
        uint32 iBeforeConfigNumInputBuffers = iNumInputBuffers;

        // read the alignment again - just in case
        iInputBufferAlignment = iParamPort.nBufferAlignment;

        iNumInputBuffers = iParamPort.nBufferCountActual;
        if (iNumInputBuffers > NUMBER_INPUT_BUFFER)
            iNumInputBuffers = NUMBER_INPUT_BUFFER; // make sure number of output buffers is not larger than port queue size

        iOMXComponentInputBufferSize = iParamPort.nBufferSize;
        // do we need to increase the number of buffers?
        if (iNumInputBuffers < iParamPort.nBufferCountMin)
            iNumInputBuffers = iParamPort.nBufferCountMin;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::HandlePortReEnable() new buffers %d, size %d", iNumInputBuffers, iOMXComponentInputBufferSize));

        if (iNumInputBuffers > iBeforeConfigNumInputBuffers)
        {
            //Reallocate EmptyBufferDone THREADSAFE CALLBACK AOs in case of port reconfiguration
            if (iThreadSafeHandlerEmptyBufferDone)
            {
                OSCL_DELETE(iThreadSafeHandlerEmptyBufferDone);
                iThreadSafeHandlerEmptyBufferDone = NULL;
            }
            // use the new queue depth of iNumInputBuffers to prevent deadlock
            iThreadSafeHandlerEmptyBufferDone = OSCL_NEW(EmptyBufferDoneThreadSafeCallbackAO, (this, iNumInputBuffers, "EmptyBufferDoneAO", Priority() + 1));

            if (NULL == iThreadSafeHandlerEmptyBufferDone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Can't reallocate EmptyBufferDone threadsafe callback queue!"));
                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrNoMemory);
                return false;
            }
        }


        // send command for port re-enabling (for this to happen, we must first recreate the buffers)
        Err = OMX_SendCommand(iOMXDecoder, OMX_CommandPortEnable, iPortIndexForDynamicReconfig, NULL);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> problem sending Port Enable command at port %d", iPortIndexForDynamicReconfig));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrResource);
            return PVMFErrResource;
        }


        /* Allocate input buffers */
        if (!CreateInputMemPool(iNumInputBuffers))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot allocate new input buffers to component"));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return PVMFErrNoMemory;
        }

        if (!ProvideBuffersToComponent(iInBufMemoryPool, // allocator
                                       iInputAllocSize,  // size to allocate from pool (hdr only or hdr+ buffer)
                                       iNumInputBuffers, // number of buffers
                                       iOMXComponentInputBufferSize, // actual buffer size
                                       iInputPortIndex, // port idx
                                       iOMXComponentSupportsExternalInputBufferAlloc, // can component use OMX_UseBuffer
                                       true // this is input
                                      ))
        {


            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::HandlePortReEnable() Port Reconfiguration -> Cannot provide new input buffers to component"));

            SetState(EPVMFNodeError);
            ReportErrorEvent(PVMFErrNoMemory);
            return PVMFErrNoMemory;

        }
    }

    // if the callback that the port was re-enabled has not arrived yet, wait for it
    // if it has arrived, it will set the state to either PortReconfig or to ReadyToDecode or InitDecoder (if more init buffers need to be sent)
    if (iProcessingState != EPVMFOMXBaseDecNodeProcessingState_PortReconfig &&
            iProcessingState != EPVMFOMXBaseDecNodeProcessingState_ReadyToDecode &&
            iProcessingState != EPVMFOMXBaseDecNodeProcessingState_InitDecoder)
        iProcessingState = EPVMFOMXBaseDecNodeProcessingState_WaitForPortEnable;

    return PVMFSuccess; // allow rescheduling of the node
}
////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXAudioDecNode::NegotiateComponentParameters(OMX_PTR aOutputParameters)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() In"));

    OMX_ERRORTYPE Err;
    // first get the number of ports and port indices
    OMX_PORT_PARAM_TYPE AudioPortParameters;
    uint32 NumPorts;
    uint32 ii;


    pvAudioConfigParserInputs aInputs;
    AudioOMXConfigParserOutputs *pOutputParameters;

    aInputs.inPtr = (uint8*)((PVMFOMXDecPort*)iInPort)->iTrackConfig;
    aInputs.inBytes = (int32)((PVMFOMXDecPort*)iInPort)->iTrackConfigSize;
    aInputs.iMimeType = ((PVMFOMXDecPort*)iInPort)->iFormat;
    pOutputParameters = (AudioOMXConfigParserOutputs *)aOutputParameters;

    // config parser was called prior to calling NegotiateComponentParameters method - the results are available here
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Audio config parser parameters - TrackConfig = %p, TrackConfigSize = %d, mimetype = %s", aInputs.inPtr, aInputs.inBytes, aInputs.iMimeType.getMIMEStrPtr()));


    //Initialize the timestamp delta to 0 for all other formats except AAC
    iTimestampDeltaForMemFragment = 0;

    if (aInputs.iMimeType == PVMF_MIME_WMA)
    {
        iNumberOfAudioChannels = pOutputParameters->Channels;
        iPCMSamplingRate = pOutputParameters->SamplesPerSec;
    }
    else if (aInputs.iMimeType == PVMF_MIME_MPEG4_AUDIO ||
             aInputs.iMimeType == PVMF_MIME_3640 ||
             aInputs.iMimeType == PVMF_MIME_LATM ||
             aInputs.iMimeType == PVMF_MIME_ADIF ||
             aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO ||
             aInputs.iMimeType == PVMF_MIME_AAC_SIZEHDR)

    {
        iNumberOfAudioChannels = pOutputParameters->Channels;
        iPCMSamplingRate = pOutputParameters->SamplesPerSec;
        iSamplesPerFrame = pOutputParameters->SamplesPerFrame;

        if (aInputs.iMimeType == PVMF_MIME_MPEG4_AUDIO ||
                aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO)
        {
            // Convert the duration of one AAC frame (based on sampling rate & num samples per frame) in microseconds
            // (this is similar to doing get_converted_ts on mediaclockconverter object)
            // calculate the timestamp update in OMX_TICKS (microseconds). This value
            // will be added directly to the OMX_TICKS timestamp
            OSCL_ASSERT(iPCMSamplingRate != 0);
            if (iPCMSamplingRate != 0)
            {
                uint64 value = ((uint64(iSamplesPerFrame)) * uint64(iTimeScale) + uint64(iPCMSamplingRate - 1)) / uint64(iPCMSamplingRate);
                iTimestampDeltaForMemFragment = ((uint32) value);
            }


        }
    }


    CONFIG_SIZE_AND_VERSION(AudioPortParameters);
    // get starting number
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamAudioInit, &AudioPortParameters);
    NumPorts = AudioPortParameters.nPorts; // must be at least 2 of them (in&out)

    if (Err != OMX_ErrorNone || NumPorts < 2)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() There is insuffucient (%d) ports", NumPorts));
        return false;
    }


    // loop through ports starting from the starting index to find index of the first input port
    for (ii = AudioPortParameters.nStartPortNumber ; ii < AudioPortParameters.nStartPortNumber + NumPorts; ii++)
    {
        // get port parameters, and determine if it is input or output
        // if there are more than 2 ports, the first one we encounter that has input direction is picked

        iParamPort.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);

        //port
        iParamPort.nPortIndex = ii;

        CONFIG_SIZE_AND_VERSION(iParamPort);

        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);

        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem negotiating with port %d ", ii));

            return false;
        }

        if (iParamPort.eDir == OMX_DirInput)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Found Input port index %d ", ii));

            iInputPortIndex = ii;
            break;
        }
    }
    if (ii == AudioPortParameters.nStartPortNumber + NumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Cannot find any input port "));
        return false;
    }


    // loop through ports starting from the starting index to find index of the first output port
    for (ii = AudioPortParameters.nStartPortNumber ; ii < AudioPortParameters.nStartPortNumber + NumPorts; ii++)
    {
        // get port parameters, and determine if it is input or output
        // if there are more than 2 ports, the first one we encounter that has output direction is picked


        //port
        iParamPort.nPortIndex = ii;

        CONFIG_SIZE_AND_VERSION(iParamPort);

        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);

        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem negotiating with port %d ", ii));

            return false;
        }

        if (iParamPort.eDir == OMX_DirOutput)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Found Output port index %d ", ii));

            iOutputPortIndex = ii;
            break;
        }
    }
    if (ii == AudioPortParameters.nStartPortNumber + NumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Cannot find any output port "));
        return false;
    }



    // now get input parameters

    CONFIG_SIZE_AND_VERSION(iParamPort);

    //Input port
    iParamPort.nPortIndex = iInputPortIndex;
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem negotiating with input port %d ", iInputPortIndex));
        return false;
    }

    // preset the number of input buffers
    //iNumInputBuffers = NUMBER_INPUT_BUFFER;
    iInputBufferAlignment = iParamPort.nBufferAlignment;
    iNumInputBuffers = iParamPort.nBufferCountActual;  // use the value provided by component

    // do we need to increase the number of buffers?
    if (iNumInputBuffers < iParamPort.nBufferCountMin)
        iNumInputBuffers = iParamPort.nBufferCountMin;

    iOMXComponentInputBufferSize = iParamPort.nBufferSize;

    iParamPort.nBufferCountActual = iNumInputBuffers;

    // set the number of actual input buffers
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Inport buffers %d,size %d", iNumInputBuffers, iOMXComponentInputBufferSize));




    CONFIG_SIZE_AND_VERSION(iParamPort);
    iParamPort.nPortIndex = iInputPortIndex;
    iParamPort.format.audio.bFlagErrorConcealment = (OMX_BOOL) iSilenceInsertionFlag;

    // finalize setting input port parameters
    Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem setting parameters in input port %d ", iInputPortIndex));
        return false;
    }

    //After setting the parameters, do GetParameter once again to confirm whether the parameters have been set properly or not
    iParamPort.nPortIndex = iInputPortIndex;
    CONFIG_SIZE_AND_VERSION(iParamPort);
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem negotiating with input port %d ", iInputPortIndex));
        return false;
    }

    //If the component has not taken the updated value, we can go back to the number of buffers that the component suggested
    if (iNumInputBuffers != iParamPort.nBufferCountActual)
    {
        iNumInputBuffers = iParamPort.nBufferCountActual;
    }


    // in case of WMA - config parser decodes config info and produces reliable numchannels and sampling rate
    // set these values now to prevent unnecessary port reconfig
    // in case of AAC formats - config parser produces a potentially correct numchannels and sampling rate
    // however in case of implicit AAC+ signaling, the sampling rate may change and cause a port reconfig
    if (aInputs.iMimeType == PVMF_MIME_WMA ||
            aInputs.iMimeType == PVMF_MIME_MPEG4_AUDIO ||
            aInputs.iMimeType == PVMF_MIME_3640 ||
            aInputs.iMimeType == PVMF_MIME_LATM ||
            aInputs.iMimeType == PVMF_MIME_ADIF ||
            aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO ||
            aInputs.iMimeType == PVMF_MIME_AAC_SIZEHDR)
    {
        // First get the structure
        OMX_AUDIO_PARAM_PCMMODETYPE Audio_Pcm_Param;
        Audio_Pcm_Param.nPortIndex = iOutputPortIndex; // we're looking for output port params
        CONFIG_SIZE_AND_VERSION(Audio_Pcm_Param);

        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamAudioPcm, &Audio_Pcm_Param);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem negotiating PCM parameters with output port %d ", iOutputPortIndex));
            return false;
        }

        // set the sampling rate obtained from config parser
        Audio_Pcm_Param.nSamplingRate = iPCMSamplingRate; // can be set to 0 (if unknown)

        // set number of channels obtained from config parser
        Audio_Pcm_Param.nChannels = iNumberOfAudioChannels;     // should be 1 or 2

        // Now, set the parameters
        Audio_Pcm_Param.nPortIndex = iOutputPortIndex; // we're looking for output port params
        CONFIG_SIZE_AND_VERSION(Audio_Pcm_Param);

        Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamAudioPcm, &Audio_Pcm_Param);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem Setting PCM parameters with output port %d ", iOutputPortIndex));
            return false;
        }

    }


    // Codec specific info set/get: SamplingRate, formats etc.
    // NOTE: iParamPort is modified in the routine below - it is loaded from the component output port values
    // Based on sampling rate - we also determine the desired output buffer size
    if (!GetSetCodecSpecificInfo())
        return false;

    //Port 1 for output port
    iParamPort.nPortIndex = iOutputPortIndex;

    CONFIG_SIZE_AND_VERSION(iParamPort);

    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem negotiating with output port %d ", iOutputPortIndex));
        return false;
    }

    // set number of output buffers and the size
    iOutputBufferAlignment = iParamPort.nBufferAlignment;

    iNumOutputBuffers = iParamPort.nBufferCountActual;

    if (iNumOutputBuffers > NUMBER_OUTPUT_BUFFER)
        iNumOutputBuffers = NUMBER_OUTPUT_BUFFER; // make sure to limit this number to what the port can hold


    if (iNumOutputBuffers < iParamPort.nBufferCountMin)
        iNumOutputBuffers = iParamPort.nBufferCountMin;



    //Try querying the buffer allocator KVP for output buffer allocation outside of the node

    // assume that this is going to fail and set the flags accordingly (so that FSI can be sent later on)
    sendFsi = true;
    iCompactFSISettingSucceeded = false;

    PvmiKvp* pkvp = NULL;
    int NumKvp = 0;
    PvmiKeyType aKvpIdentifier = (PvmiKeyType)PVMF_SUPPORT_FOR_BUFFER_ALLOCATOR_IN_MIO_KEY;
    // Check if Fsi configuration need to be sent
    // Send it only if the MIO supports the buffer allocator
    if (((PVMFOMXDecPort*)iOutPort)->pvmiGetBufferAllocatorSpecificInfoSync(aKvpIdentifier, pkvp, NumKvp) == PVMFSuccess)
    {
        //Send the FSI information to media output node here, before setting output
        //port parameters to the omx component
        int fsiErrorCode = 0;

        OsclRefCounterMemFrag FsiMemfrag;

        OSCL_TRY(fsiErrorCode, FsiMemfrag = iFsiFragmentAlloc.get(););

        OSCL_FIRST_CATCH_ANY(fsiErrorCode, PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                             (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Failed to allocate memory for  FSI")));

        if (fsiErrorCode == 0)
        {
            channelSampleInfo* pcminfo = (channelSampleInfo*) FsiMemfrag.getMemFragPtr();
            if (pcminfo != NULL)
            {
                OSCL_ASSERT(pcminfo != NULL);
                pcminfo->samplingRate    = iPCMSamplingRate;
                pcminfo->desiredChannels = iNumberOfAudioChannels;
                pcminfo->bitsPerSample = 16;
                pcminfo->num_buffers = iNumOutputBuffers;
                pcminfo->buffer_size = iOMXComponentOutputBufferSize;

                OsclMemAllocator alloc;
                int32 KeyLength = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM) + 1;
                PvmiKeyType KvpKey = (PvmiKeyType)alloc.ALLOCATE(KeyLength);

                if (NULL == KvpKey)
                {
                    return false;
                }

                oscl_strncpy(KvpKey, PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM, KeyLength);
                int32 err;

                bool success = false;

                OSCL_TRY(err, success = ((PVMFOMXDecPort*)iOutPort)->pvmiSetPortFormatSpecificInfoSync(FsiMemfrag, KvpKey););
                if (err != OsclErrNone || !success)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters - Problem to set FSI"));
                }
                else
                {
                    sendFsi = false;
                    iCompactFSISettingSucceeded = true;
                }
                alloc.deallocate((OsclAny*)(KvpKey));


            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters - Problem allocating Output FSI"));
                SetState(EPVMFNodeError);
                ReportErrorEvent(PVMFErrNoMemory);
                return false; // this is going to make everything go out of scope
            }
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters - Problem allocating Output FSI"));
            return false; // this is going to make everything go out of scope
        }


    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters Warning- It is OK for this KVP to fail"
                         "Do NOT attempt to fix the failure in the MIO unless you absolutely want to implement"
                         "the MIO BUFFER ALLOCATOR -see documentation"));

        ReportInfoEvent(PVMFPvmiBufferAllocatorNotAcquired);
    }
    //Try querying the buffer allocator KVP for output buffer allocation outside of the node
    PvmiKvp* kvp = NULL;
    int numKvp = 0;
    PvmiKeyType aIdentifier = (PvmiKeyType)PVMF_BUFFER_ALLOCATOR_KEY;
    int32 err, err1;

    if (ipExternalOutputBufferAllocatorInterface)
    {
        ipExternalOutputBufferAllocatorInterface->removeRef();
        ipExternalOutputBufferAllocatorInterface = NULL;
    }

    OSCL_TRY(err, ((PVMFOMXDecPort*)iOutPort)->pvmiGetBufferAllocatorSpecificInfoSync(aIdentifier, kvp, numKvp););

    if ((err == OsclErrNone) && (NULL != kvp))
    {
        ipExternalOutputBufferAllocatorInterface = (PVInterface*) kvp->value.key_specific_value;

        if (ipExternalOutputBufferAllocatorInterface)
        {
            PVInterface* pTempPVInterfacePtr = NULL;

            OSCL_TRY(err, ipExternalOutputBufferAllocatorInterface->queryInterface(PVMFFixedSizeBufferAllocUUID, pTempPVInterfacePtr););

            OSCL_TRY(err1, ((PVMFOMXDecPort*)iOutPort)->releaseParametersSync(kvp, numKvp););
            if (err1 != OsclErrNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters - Unable to Release Parameters"));
            }

            if ((err == OsclErrNone) && (NULL != pTempPVInterfacePtr))
            {
                ipFixedSizeBufferAlloc = OSCL_STATIC_CAST(PVMFFixedSizeBufferAlloc*, pTempPVInterfacePtr);

                uint32 iNumBuffers, iBufferSize;

                iNumBuffers = ipFixedSizeBufferAlloc->getNumBuffers();
                iBufferSize = ipFixedSizeBufferAlloc->getBufferSize();

                if ((iNumBuffers < iParamPort.nBufferCountMin) || (iBufferSize < iOMXComponentOutputBufferSize))
                {
                    ipExternalOutputBufferAllocatorInterface->removeRef();
                    ipExternalOutputBufferAllocatorInterface = NULL;
                }
                else
                {
                    iNumOutputBuffers = iNumBuffers;
                    iOMXComponentOutputBufferSize = iBufferSize;
                    ReportInfoEvent(PVMFPvmiBufferAlloctorAcquired);
                }

            }
            else
            {
                ipExternalOutputBufferAllocatorInterface->removeRef();
                ipExternalOutputBufferAllocatorInterface = NULL;

            }
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_WARNING,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters Warning- It is OK for this KVP to fail"
                         "Do NOT attempt to fix the failure in the MIO unless you absolutely want to implement"
                         "the MIO BUFFER ALLOCATOR -see documentation"));

        ReportInfoEvent(PVMFPvmiBufferAllocatorNotAcquired);
    }

    iParamPort.nBufferCountActual = iNumOutputBuffers;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Outport buffers %d,size %d", iNumOutputBuffers, iOMXComponentOutputBufferSize));

    CONFIG_SIZE_AND_VERSION(iParamPort);
    iParamPort.nPortIndex = iOutputPortIndex;
    // finalize setting output port parameters
    Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem setting parameters in output port %d ", iOutputPortIndex));
        return false;
    }


    //After setting the parameters, do GetParameter once again to confirm whether the parameters have been set properly or not
    iParamPort.nPortIndex = iOutputPortIndex;
    CONFIG_SIZE_AND_VERSION(iParamPort);
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Output Port parameters not set properly in the component"));
        return false;
    }

    //If the component has not taken the updated value,
    //we can go back to the number of buffers that the component suggested only if External Output Buffer Allocator is not supported
    if (iNumOutputBuffers != iParamPort.nBufferCountActual)
    {
        if ((NULL != ipExternalOutputBufferAllocatorInterface) || (iNumOutputBuffers > NUMBER_OUTPUT_BUFFER))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Output Port parameters not set properly in the component"));
            return false;
        }
        else
        {
            iNumOutputBuffers = iParamPort.nBufferCountActual;
        }
    }

    //Set input audio format
    //This is need it since a single component could handle differents roles

    // Init to desire format
    PVMFFormatType Format = PVMF_MIME_FORMAT_UNKNOWN;
    if (iInPort != NULL)
    {
        Format = ((PVMFOMXDecPort*)iInPort)->iFormat;
    }
    if (Format == PVMF_MIME_MPEG4_AUDIO ||
            Format == PVMF_MIME_3640 ||
            Format == PVMF_MIME_LATM ||
            Format == PVMF_MIME_ADIF ||
            Format == PVMF_MIME_ASF_MPEG4_AUDIO ||
            Format == PVMF_MIME_AAC_SIZEHDR)
    {
        iOMXAudioCompressionFormat = OMX_AUDIO_CodingAAC;
    }
    else if (Format == PVMF_MIME_AMR_IF2 ||
             Format == PVMF_MIME_AMR_IETF ||
             Format == PVMF_MIME_AMR ||
             Format == PVMF_MIME_AMRWB_IETF ||
             Format == PVMF_MIME_AMRWB)
    {
        iOMXAudioCompressionFormat = OMX_AUDIO_CodingAMR;
    }
    else if (Format == PVMF_MIME_MP3)
    {
        iOMXAudioCompressionFormat = OMX_AUDIO_CodingMP3;
    }
    else if (Format == PVMF_MIME_WMA)
    {
        iOMXAudioCompressionFormat = OMX_AUDIO_CodingWMA;
    }
    else if (Format == PVMF_MIME_REAL_AUDIO)
    {
        iOMXAudioCompressionFormat = OMX_AUDIO_CodingRA;
    }
    else
    {
        // Illegal codec specified.
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem setting audio compression format"));
        return false;
    }


    OMX_AUDIO_PARAM_PORTFORMATTYPE AudioPortFormat;
    CONFIG_SIZE_AND_VERSION(AudioPortFormat);
    AudioPortFormat.nPortIndex = iInputPortIndex;

    // Search the proper format index and set it.
    // Since we already know that the component has the role we need, search until finding the proper nIndex
    // if component does not find the format will return OMX_ErrorNoMore


    for (ii = 0; ii < PVOMXAUDIO_MAX_SUPPORTED_FORMAT; ii++)
    {
        AudioPortFormat.nIndex = ii;
        Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamAudioPortFormat, &AudioPortFormat);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem setting audio compression format"));
            return false;
        }
        if (iOMXAudioCompressionFormat == AudioPortFormat.eEncoding)
        {
            break;
        }
    }

    if (ii == PVOMXAUDIO_MAX_SUPPORTED_FORMAT)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() No audio compression format found"));
        return false;

    }
    // Now set the format to confirm parameters
    Err = OMX_SetParameter(iOMXDecoder, OMX_IndexParamAudioPortFormat, &AudioPortFormat);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem setting audio compression format"));
        return false;
    }


    return true;
}

bool PVMFOMXAudioDecNode::GetSetCodecSpecificInfo()
{

    // for AAC, need to let the decoder know about the type of AAC format. Need to get the frame length
    // need to get the parameters
    OMX_PTR CodecProfilePtr = NULL;
    OMX_INDEXTYPE CodecProfileIndx = OMX_IndexAudioStartUnused;
    OMX_AUDIO_PARAM_AACPROFILETYPE Audio_Aac_Param;
    OMX_AUDIO_PARAM_AMRTYPE Audio_Amr_Param;
    OMX_AUDIO_PARAM_MP3TYPE Audio_Mp3_Param;
    OMX_AUDIO_PARAM_WMATYPE Audio_Wma_Param;
    OMX_AUDIO_PARAM_RATYPE Audio_Ra_Param;
    OMX_ERRORTYPE Err = OMX_ErrorNone;
    PVMFFormatType Format = PVMF_MIME_FORMAT_UNKNOWN;

    // determine the proper index and structure (based on codec type)

    if (iInPort != NULL)
    {
        Format = ((PVMFOMXDecPort*)iInPort)->iFormat;
    }
    if (Format ==  PVMF_MIME_MPEG4_AUDIO ||
            Format == PVMF_MIME_3640 ||
            Format == PVMF_MIME_LATM ||
            Format == PVMF_MIME_ADIF ||
            Format == PVMF_MIME_ASF_MPEG4_AUDIO ||
            Format == PVMF_MIME_AAC_SIZEHDR) // for testing
    {
        // AAC

        CodecProfilePtr = (OMX_PTR) & Audio_Aac_Param;
        CodecProfileIndx = OMX_IndexParamAudioAac;
        Audio_Aac_Param.nPortIndex = iInputPortIndex;

        CONFIG_SIZE_AND_VERSION(Audio_Aac_Param);
    }
    // AMR
    else if (Format ==  PVMF_MIME_AMR_IF2 ||
             Format == PVMF_MIME_AMR_IETF ||
             Format == PVMF_MIME_AMR ||
             Format == PVMF_MIME_AMRWB_IETF ||
             Format == PVMF_MIME_AMRWB)
    {
        CodecProfilePtr = (OMX_PTR) & Audio_Amr_Param;
        CodecProfileIndx = OMX_IndexParamAudioAmr;
        Audio_Amr_Param.nPortIndex = iInputPortIndex;

        CONFIG_SIZE_AND_VERSION(Audio_Amr_Param);
    }
    else if (Format == PVMF_MIME_MP3)
    {
        CodecProfilePtr = (OMX_PTR) & Audio_Mp3_Param;
        CodecProfileIndx = OMX_IndexParamAudioMp3;
        Audio_Mp3_Param.nPortIndex = iInputPortIndex;

        CONFIG_SIZE_AND_VERSION(Audio_Mp3_Param);
    }
    else if (Format == PVMF_MIME_WMA)
    {
        CodecProfilePtr = (OMX_PTR) & Audio_Wma_Param;
        CodecProfileIndx = OMX_IndexParamAudioWma;
        Audio_Wma_Param.nPortIndex = iInputPortIndex;

        CONFIG_SIZE_AND_VERSION(Audio_Wma_Param);
    }
    else if (Format == PVMF_MIME_REAL_AUDIO)
    {
        CodecProfilePtr = (OMX_PTR) & Audio_Ra_Param;
        CodecProfileIndx = OMX_IndexParamAudioRa;
        Audio_Ra_Param.nPortIndex = iInputPortIndex;

        CONFIG_SIZE_AND_VERSION(Audio_Ra_Param);
    }

    // first get parameters:
    Err = OMX_GetParameter(iOMXDecoder, CodecProfileIndx, CodecProfilePtr);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem getting codec profile parameter on input port %d ", iInputPortIndex));
        return false;
    }
    // Set the stream format


    // AAC FORMATS:
    if (Format ==  PVMF_MIME_MPEG4_AUDIO)
    {
        Audio_Aac_Param.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
    }
    else if (Format ==  PVMF_MIME_3640)
    {
        Audio_Aac_Param.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
    }
    else if (Format == PVMF_MIME_LATM)
    {
        Audio_Aac_Param.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4LATM;
    }
    else if (Format == PVMF_MIME_ADIF)
    {
        Audio_Aac_Param.eAACStreamFormat = OMX_AUDIO_AACStreamFormatADIF;
    }
    else if (Format == PVMF_MIME_ASF_MPEG4_AUDIO)
    {
        Audio_Aac_Param.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
    }
    else if (Format == PVMF_MIME_AAC_SIZEHDR) // for testing
    {
        Audio_Aac_Param.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
    }
    // AMR FORMATS
    else if (Format == PVMF_MIME_AMR_IF2)
    {
        Audio_Amr_Param.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatIF2;
        Audio_Amr_Param.eAMRBandMode = OMX_AUDIO_AMRBandModeNB0; // we don't know the bitrate yet, but for init
        // purposes, we'll set this to any NarrowBand bitrate
        // to indicate NB vs WB
    }
    // File format
    // NB
    else if (Format == PVMF_MIME_AMR_IETF)
    {
        Audio_Amr_Param.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatFSF;
        Audio_Amr_Param.eAMRBandMode = OMX_AUDIO_AMRBandModeNB0; // we don't know the bitrate yet, but for init
        // purposes, we'll set this to any NarrowBand bitrate
        // to indicate NB vs WB
    }
    // WB
    else if (Format == PVMF_MIME_AMRWB_IETF)
    {
        Audio_Amr_Param.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatFSF;
        Audio_Amr_Param.eAMRBandMode = OMX_AUDIO_AMRBandModeWB0; // we don't know the bitrate yet, but for init
        // purposes, we'll set this to any WideBand bitrate
        // to indicate NB vs WB
    }
    // streaming with Table of Contents
    else if (Format == PVMF_MIME_AMR)
    {
        Audio_Amr_Param.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatRTPPayload;
        Audio_Amr_Param.eAMRBandMode = OMX_AUDIO_AMRBandModeNB0; // we don't know the bitrate yet, but for init
        // purposes, we'll set this to any WideBand bitrate
        // to indicate NB vs WB
    }
    else if (Format == PVMF_MIME_AMRWB)
    {
        Audio_Amr_Param.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatRTPPayload;
        Audio_Amr_Param.eAMRBandMode = OMX_AUDIO_AMRBandModeWB0; // we don't know the bitrate yet, but for init
        // purposes, we'll set this to any WideBand bitrate
        // to indicate NB vs WB
    }
    else if (Format == PVMF_MIME_MP3)
    {
        // nothing to do here
    }
    else if (Format == PVMF_MIME_WMA)
    {
        Audio_Wma_Param.eFormat = OMX_AUDIO_WMAFormatUnused; // set this initially
    }
    else if (Format == PVMF_MIME_REAL_AUDIO)
    {
        Audio_Ra_Param.eFormat = OMX_AUDIO_RA8; // set this initially
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Unknown format in input port negotiation "));
        return false;
    }

    // set parameters to inform teh component of the stream type
    Err = OMX_SetParameter(iOMXDecoder, CodecProfileIndx, CodecProfilePtr);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem setting codec profile parameter on input port %d ", iInputPortIndex));
        return false;
    }


    // read the output frame size
    // AAC
    if (Format ==  PVMF_MIME_MPEG4_AUDIO ||
            Format == PVMF_MIME_3640 ||
            Format == PVMF_MIME_LATM ||
            Format == PVMF_MIME_ADIF ||
            Format == PVMF_MIME_ASF_MPEG4_AUDIO ||
            Format == PVMF_MIME_AAC_SIZEHDR) // for testing
    {
        // AAC frame size is 1024 samples or 2048 samples for AAC-HE
        iSamplesPerFrame = Audio_Aac_Param.nFrameLength;
    }
    // AMR
    else if (Format == PVMF_MIME_AMR_IF2 ||
             Format == PVMF_MIME_AMR_IETF ||
             Format == PVMF_MIME_AMR)
    {
        // AMR NB has fs=8khz Mono and the frame is 20ms long, i.e. there is 160 samples per frame
        iSamplesPerFrame = PVOMXAUDIODEC_AMRNB_SAMPLES_PER_FRAME;
    }
    else if (Format == PVMF_MIME_AMRWB_IETF ||
             Format == PVMF_MIME_AMRWB)
    {
        // AMR WB has fs=16khz Mono and the frame is 20ms long, i.e. there is 320 samples per frame
        iSamplesPerFrame = PVOMXAUDIODEC_AMRWB_SAMPLES_PER_FRAME;
    }
    else if (Format == PVMF_MIME_MP3)
    {
        // frame size is either 576 or 1152 samples per frame. However, this information cannot be
        // obtained through OMX MP3 Params. Assume that it's 1152
        iSamplesPerFrame = PVOMXAUDIODEC_MP3_DEFAULT_SAMPLES_PER_FRAME;
    }
    else if (Format == PVMF_MIME_WMA)
    {
        // output frame size is unknown in WMA. However, the PV-WMA decoder can control the number
        // of samples it places in an output buffer, so we can create an output buffer of arbitrary size
        // and let the decoder control how it is filled
        iSamplesPerFrame = 0; // unknown
    }
    else if (Format == PVMF_MIME_REAL_AUDIO)
    {
        // samples per frame is unknown in RA as of now, keep it to 0
        // and let the decoder control how it is filled
        iSamplesPerFrame = 0; // unknown
    }


    // iSamplesPerFrame depends on the codec.
    // for AAC: iSamplesPerFrame = 1024
    // for AAC+: iSamplesPerFrame = 2048
    // for AMRNB: iSamplesPerFrame = 160
    // for AMRWB: iSamplesPerFrame = 320
    // for MP3:   iSamplesPerFrame = unknown, but either 1152 or 576 (we pick 1152 as default)
    // for WMA:    unknown (iSamplesPerFrame is set to 0)
    // for RA:     unknown (iSamplesPerFrame is set to 0)

    // GET the output buffer params and sizes
    OMX_AUDIO_PARAM_PCMMODETYPE Audio_Pcm_Param;
    Audio_Pcm_Param.nPortIndex = iOutputPortIndex; // we're looking for output port params

    CONFIG_SIZE_AND_VERSION(Audio_Pcm_Param);


    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamAudioPcm, &Audio_Pcm_Param);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem negotiating PCM parameters with output port %d ", iOutputPortIndex));
        return false;
    }


    // these are some initial default values that may change
    iPCMSamplingRate = Audio_Pcm_Param.nSamplingRate; // can be set to 0 (if unknown)

    if (iPCMSamplingRate == 0) // use default sampling rate (i.e. 48000)
        iPCMSamplingRate = PVOMXAUDIODEC_DEFAULT_SAMPLINGRATE;

    iNumberOfAudioChannels = Audio_Pcm_Param.nChannels;     // should be 1 or 2
    if (iNumberOfAudioChannels != 1 && iNumberOfAudioChannels != 2)
        return false;


    if ((iSamplesPerFrame != 0) && ((iSamplesPerFrame * 1000) > iPCMSamplingRate))
        // if this iSamplesPerFrame is known and is large enough to ensure that the iMilliSecPerFrame calculation
        // below won't be set to 0.
    {
        // CALCULATE NumBytes per frame, Msec per frame, etc.

        iNumBytesPerFrame = 2 * iSamplesPerFrame * iNumberOfAudioChannels;
        iMilliSecPerFrame = (iSamplesPerFrame * 1000) / iPCMSamplingRate;
        // Determine the size of each PCM output buffer. Size would be big enough to hold certain time amount of PCM data
        uint32 numframes = iOutputBufferPCMDuration / iMilliSecPerFrame;

        if (iOutputBufferPCMDuration % iMilliSecPerFrame)
        {
            // If there is a remainder, include one more frame
            ++numframes;
        }
        // set the output buffer size accordingly:
        iOMXComponentOutputBufferSize = numframes * iNumBytesPerFrame;
    }
    else
        iOMXComponentOutputBufferSize = (2 * iNumberOfAudioChannels * iOutputBufferPCMDuration * iPCMSamplingRate) / 1000; // assuming 16 bits per sample

    if (iBOCReceived)
        CalculateBOCParameters();

    if (iEOCReceived)
        CalculateEOCParameters();

    //Port 1 for output port
    iParamPort.nPortIndex = iOutputPortIndex;
    CONFIG_SIZE_AND_VERSION(iParamPort);
    Err = OMX_GetParameter(iOMXDecoder, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters() Problem negotiating with output port %d ", iOutputPortIndex));
        return false;
    }

    if (iOMXComponentOutputBufferSize < iParamPort.nBufferSize)
    {
        // the OMX spec says that nBuffersize is a read only field, but the client is allowed to allocate
        // a buffer size larger than nBufferSize.
        iOMXComponentOutputBufferSize = iParamPort.nBufferSize;
    }

    return true;

}

/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXAudioDecNode::InitDecoder(PVMFSharedMediaDataPtr& DataIn)
{

    OsclRefCounterMemFrag DataFrag;
    OsclRefCounterMemFrag refCtrMemFragOut;
    uint8* initbuffer = NULL;
    uint32 initbufsize = 0;
    PVMFStatus status = PVMFSuccess;

    // NOTE: the component may not start decoding without providing the Output buffer to it,
    //      here, we're sending input/config buffers.
    //      Then, we'll go to ReadyToDecode state and send output as well

    if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_LATM)
    {
        // must have the LATM config buffer and size already present
        if (iLATMConfigBuffer != NULL)
        {
            initbuffer = iLATMConfigBuffer;
            initbufsize = iLATMConfigBufferSize;
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::InitDecoder() Error - LATM config buffer not present"));
            return PVMFFailure;
        }
    }
    else if (((PVMFOMXDecPort*)iInPort)->iFormat ==  PVMF_MIME_MPEG4_AUDIO ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_3640 ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ADIF ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ASF_MPEG4_AUDIO ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AAC_SIZEHDR) // for testing
    {
        // get format specific info and send it as config data:
        DataIn->getFormatSpecificInfo(DataFrag);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::InitDecoder() VOL header (Size=%d)", DataFrag.getMemFragSize()));

        //get pointer to the data fragment
        initbuffer = (uint8 *) DataFrag.getMemFragPtr();
        initbufsize = (int32) DataFrag.getMemFragSize();

        // in case of ASF_MPEG4 audio need to create aac config header
        if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ASF_MPEG4_AUDIO)
        {


            if (!CreateAACConfigDataFromASF(initbuffer, initbufsize, &iAACConfigData[0], iAACConfigDataLength))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::InitDecoder() Error in creating AAC Codec Config data"));

                return PVMFFailure;
            }

            initbuffer  = &iAACConfigData[0];
            initbufsize = iAACConfigDataLength;
        }


    }           // in some cases, initbufsize may be 0, and initbuf= NULL. Config is done after 1st frame of data
    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IF2 ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IETF ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB_IETF ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB ||
             ((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MP3)
    {
        initbuffer = NULL; // no special config header. Need to decode 1 frame
        initbufsize = 0;
    }

    else if ((((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMA) ||
             (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_AUDIO))
    {
        // in case of WMA, get config parameters from the port
        initbuffer = ((PVMFOMXDecPort*)iInPort)->getTrackConfig();
        initbufsize = (int32)((PVMFOMXDecPort*)iInPort)->getTrackConfigSize();

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::InitDecoder() for WMA Decoder. Initialization data Size %d.", initbufsize));
    }

    if (initbufsize > 0)
    {


        status = SendConfigBufferToOMXComponent(initbuffer, initbufsize);
        if (status != PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::InitDecoder() Error in processing config buffer"));
            iIsThereMoreConfigDataToBeSent = true; // this means that we may not have buffers available to send,
            return status;
        }

        // set the flag requiring config data processing by the component
        iIsConfigDataProcessingCompletionNeeded = true;
        iConfigDataBuffersOutstanding++;
        iIsThereMoreConfigDataToBeSent = false;
    }
    else
    {
        iIsThereMoreConfigDataToBeSent = false;
    }

    return PVMFSuccess;
}


/////////////////////////////////////////////////////////////////////////////
////////////////////// CALLBACK PROCESSING FOR FILL BUFFER DONE - output buffer is ready
/////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE PVMFOMXAudioDecNode::FillBufferDoneProcessing(OMX_OUT OMX_HANDLETYPE aComponent,
        OMX_OUT OMX_PTR aAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer)
{
    OSCL_UNUSED_ARG(aComponent);
    OSCL_UNUSED_ARG(aAppData);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::FillBufferDoneProcessing: In"));

    OSCL_ASSERT((void*) aComponent == (void*) iOMXDecoder); // component should match the component
    OSCL_ASSERT(aAppData == (OMX_PTR)(this));       // AppData should represent this node ptr

    // first, get the buffer "context", i.e. pointer to application private data that contains the
    // address of the mempool buffer (so that it can be released)
    OutputBufCtrlStruct *pContext = (OutputBufCtrlStruct*) aBuffer->pAppPrivate;

    // check for EOS flag
    if ((aBuffer->nFlags & OMX_BUFFERFLAG_EOS))
    {
        // EOS received - enable sending EOS msg
        iIsEOSReceivedFromComponent = true;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::FillBufferDoneProcessing: Output buffer has EOS set"));
    }

    if (iCountSamplesInBuffer)
    {
        CountSamplesInBuffer(aBuffer);
    }

    // check for BOC.
    if (iBOCReceived)
    {
        HandleBOCProcessing(aBuffer);
    }

    // check for EOC.
    if (iEOCReceived)
    {
        HandleEOCProcessing(aBuffer);
    }

    // if a buffer is empty, or if it should not be sent downstream (say, due to state change)
    // release the buffer back to the pool
    if ((aBuffer->nFilledLen == 0) || (iDoNotSendOutputBuffersDownstreamFlag == true))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::FillBufferDoneProcessing: Release output buffer %x pointing to buffer %x back to mempool - buffer empty or not to be sent downstream", pContext, pContext->pMemPoolEntry));

        iOutBufMemoryPool->deallocate(pContext->pMemPoolEntry);
    }
    else
    {

        // get pointer to actual buffer data
        uint8 *pBufdata = ((uint8*) aBuffer->pBuffer);
        // move the data pointer based on offset info
        pBufdata += aBuffer->nOffset;

        iOutTimeStamp = ConvertOMXTicksIntoTimestamp(aBuffer->nTimeStamp);

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::FillBufferDoneProcessing: Output frame %d with timestamp %d received", iFrameCounter++, iOutTimeStamp));


        ipPrivateData = (OsclAny *) aBuffer->pPlatformPrivate; // record the pointer

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::FillBufferDoneProcessing: Wrapping buffer %x of size %d", pBufdata, aBuffer->nFilledLen));
        // wrap the buffer into the MediaDataImpl wrapper, and queue it for sending downstream
        // wrapping will create a refcounter. When refcounter goes to 0 i.e. when media data
        // is released in downstream components, the custom deallocator will automatically release the buffer back to the
        //  mempool. To do that, the deallocator needs to have info about Context
        // NOTE: we had to wait until now to wrap the buffer data because we only know
        //          now where the actual data is located (based on buffer offset)
        OsclSharedPtr<PVMFMediaDataImpl> MediaDataOut = WrapOutputBuffer(pBufdata, (uint32)(aBuffer->nFilledLen), pContext->pMemPoolEntry);

        // if you can't get the MediaDataOut, release the buffer back to the pool
        if (MediaDataOut.GetRep() == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::FillBufferDoneProcessing: Problem wrapping buffer %x of size %d - releasing the buffer", pBufdata, aBuffer->nFilledLen));

            iOutBufMemoryPool->deallocate(pContext->pMemPoolEntry);
        }
        else
        {

            // if there's a problem queuing output buffer, MediaDataOut will expire at end of scope and
            // release buffer back to the pool, (this should not be the case)
            if (QueueOutputBuffer(MediaDataOut, aBuffer->nFilledLen))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFOMXAudioDecNode::FillBufferDoneProcessing: Buffer %x of size %d queued - reschedule the node to send out", pBufdata, aBuffer->nFilledLen));

                // if queing went OK,
                // re-schedule the node so that outgoing queue can be emptied (unless the outgoing port is busy)
                if ((iOutPort) && !(iOutPort->IsConnectedPortBusy()))
                    RunIfNotReady();
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFOMXAudioDecNode::FillBufferDoneProcessing: Problem queing buffer %x of size %d - releasing the buffer", pBufdata, aBuffer->nFilledLen));
            }


        }

    }
    // the OMX spec says that no error is to be returned
    return OMX_ErrorNone;

}

////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Put output buffer in outgoing queue //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
bool PVMFOMXAudioDecNode::QueueOutputBuffer(OsclSharedPtr<PVMFMediaDataImpl> &mediadataimplout, uint32 aDataLen)
{

    bool status = true;
    PVMFSharedMediaDataPtr mediaDataOut;
    int32 leavecode = OsclErrNone;

    // NOTE: ASSUMPTION IS THAT OUTGOING QUEUE IS BIG ENOUGH TO QUEUE ALL THE OUTPUT BUFFERS
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::QueueOutputFrame: In"));

    if (!iOutPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMFOMXAudioDecNode::QueueOutputFrame() Output Port doesn't exist!!!!!!!!!!"));
        return false;
    }
    // First check if we can put outgoing msg. into the queue
    if (iOutPort->IsOutgoingQueueBusy())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMFOMXAudioDecNode::QueueOutputFrame() OutgoingQueue is busy"));
        return false;
    }

    OSCL_TRY(leavecode,
             mediaDataOut = PVMFMediaData::createMediaData(mediadataimplout, iOutputMediaDataMemPool););
    if (OsclErrNone == leavecode)
    {
        // Update the filled length of the fragment
        mediaDataOut->setMediaFragFilledLen(0, aDataLen);

        // Set timestamp
        mediaDataOut->setTimestamp(iOutTimeStamp);

        // Set Duration
        // Duration = (OutputBufferSize * 1000)/(SamplingRate * NumChannels * BitsPerSample/8)
        uint32 duration = (iOMXComponentOutputBufferSize * 1000) / (iPCMSamplingRate * iNumberOfAudioChannels * 16 / 8);
        mediaDataOut->setDuration(duration);

        // Get the markerinfo and set the Duration Bit
        uint32 markerInfo = mediaDataOut->getMarkerInfo();
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_DURATION_AVAILABLE_BIT;
        mediaDataOut->setMarkerInfo(markerInfo);

        // Set sequence number
        mediaDataOut->setSeqNum(iSeqNum++);
        // set stream id
        mediaDataOut->setStreamID(iStreamID);

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iDataPathLogger, PVLOGMSG_INFO, (0, ":PVMFOMXAudioDecNode::QueueOutputFrame(): - SeqNum=%d, TS=%d", iSeqNum, iOutTimeStamp));
        int fsiErrorCode = 0;

        // Check if Fsi configuration need to be sent
        if (sendFsi && !iCompactFSISettingSucceeded)
        {

            OsclRefCounterMemFrag FsiMemfrag;

            OSCL_TRY(fsiErrorCode, FsiMemfrag = iFsiFragmentAlloc.get(););

            OSCL_FIRST_CATCH_ANY(fsiErrorCode, PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                 (0, "PVMFOMXAudioDecNode::RemoveOutputFrame() Failed to allocate memory for  FSI")));

            if (fsiErrorCode == 0)
            {

                channelSampleInfo* pcminfo = (channelSampleInfo*) FsiMemfrag.getMemFragPtr();
                if (pcminfo != NULL)
                {
                    OSCL_ASSERT(pcminfo != NULL);

                    pcminfo->samplingRate    = iPCMSamplingRate;
                    pcminfo->desiredChannels = iNumberOfAudioChannels;
                    pcminfo->bitsPerSample = 16;
                    pcminfo->num_buffers = 0;
                    pcminfo->buffer_size = 0;

                    mediaDataOut->setFormatSpecificInfo(FsiMemfrag);


                    OsclMemAllocator alloc;
                    int32 KeyLength = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM) + 1;

                    PvmiKeyType KvpKey = NULL;
                    AllocatePvmiKey(&KvpKey, &alloc, KeyLength);

                    if (NULL == KvpKey)
                    {
                        SetState(EPVMFNodeError);
                        ReportErrorEvent(PVMFErrNoMemory);
                        return false;
                    }

                    oscl_strncpy(KvpKey, PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM, KeyLength);
                    int32 err;

                    OSCL_TRY(err, ((PVMFOMXDecPort*)iOutPort)->pvmiSetPortFormatSpecificInfoSync(FsiMemfrag, KvpKey););
                    if (err != OsclErrNone)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParameters - Problem to set FSI_PCM key, will try to send ordinary FSI"));

                        alloc.deallocate((OsclAny*)(KvpKey));


                        // try to send the ordinary FSI
                        KeyLength = oscl_strlen(PVMF_FORMAT_SPECIFIC_INFO_KEY) + 1;
                        KvpKey = (PvmiKeyType)alloc.ALLOCATE(KeyLength);
                        if (NULL == KvpKey)
                        {
                            SetState(EPVMFNodeError);
                            ReportErrorEvent(PVMFErrNoMemory);
                            return false;
                        }

                        oscl_strncpy(KvpKey, PVMF_FORMAT_SPECIFIC_INFO_KEY, KeyLength);


                        OSCL_TRY(err, ((PVMFOMXDecPort*)iOutPort)->pvmiSetPortFormatSpecificInfoSync(FsiMemfrag, KvpKey););
                        if (err != OsclErrNone)
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "PVMFOMXAudioDecNode::QueueOutputFrame - Problem to set ordinary FSI as well"));

                            alloc.deallocate((OsclAny*)(KvpKey));

                            SetState(EPVMFNodeError);
                            ReportErrorEvent(PVMFErrNoMemory);
                            return false; // this is going to make everything go out of scope
                        }
                    }

                    alloc.deallocate((OsclAny*)(KvpKey));
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "PVMFOMXAudioDecNode::QueueOutputFrame - Problem allocating Output FSI"));
                    SetState(EPVMFNodeError);
                    ReportErrorEvent(PVMFErrNoMemory);
                    return false; // this is going to make everything go out of scope
                }
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXAudioDecNode::QueueOutputFrame - Problem allocating Output FSI"));
                return false; // this is going to make everything go out of scope
            }

            // Reset the flag
            sendFsi = false;
        }

        if (fsiErrorCode == 0)
        {
            // Send frame to downstream node
            PVMFSharedMediaMsgPtr mediaMsgOut;
            convertToPVMFMediaMsg(mediaMsgOut, mediaDataOut);

            if (iOutPort && (iOutPort->QueueOutgoingMsg(mediaMsgOut) == PVMFSuccess))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMFOMXAudioDecNode::QueueOutputFrame(): Queued frame OK "));

            }
            else
            {
                // we should not get here because we always check for whether queue is busy or not
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::QueueOutputFrame(): Send frame failed"));
                return false;
            }
        }

    }//end of if (OsclErrNone == leavecode)
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFOMXAudioDecNode::QueueOutputFrame() call PVMFMediaData::createMediaData is failed"));
        return false;
    }

    return status;
}

/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXAudioDecNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::DoRequestPort() In"));
    //This node supports port request from any state

    //retrieve port tag.
    int32 tag;
    OSCL_String* portconfig;

    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, portconfig);

    int32 leavecode = OsclErrNone;
    //validate the tag...
    switch (tag)
    {
        case PVMF_OMX_DEC_NODE_PORT_TYPE_INPUT:
            if (iInPort)
            {
                return PVMFFailure;
                break;
            }
            OSCL_TRY(leavecode, iInPort = OSCL_NEW(PVMFOMXDecPort, ((int32)tag, this, (OMX_STRING)PVMF_OMX_AUDIO_DEC_INPUT_PORT_NAME)););
            if (leavecode || iInPort == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::DoRequestPort: Error - Input port instantiation failed"));
                return PVMFErrArgument;
            }
            aPort = iInPort;
            break;

        case PVMF_OMX_DEC_NODE_PORT_TYPE_OUTPUT:
            if (iOutPort)
            {
                return PVMFFailure;
                break;
            }
            OSCL_TRY(leavecode, iOutPort = OSCL_NEW(PVMFOMXDecPort, ((int32)tag, this, (OMX_STRING)PVMF_OMX_AUDIO_DEC_OUTPUT_PORT_NAME)));
            if (leavecode || iOutPort == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::DoRequestPort: Error - Output port instantiation failed"));
                return PVMFErrArgument;
            }
            aPort = iOutPort;
            break;

        default:
            //bad port tag
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::DoRequestPort: Error - Invalid port tag"));
            return PVMFErrArgument;
    }
    return PVMFSuccess;
}


/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXAudioDecNode::DoGetNodeMetadataValue()
{
    PVMFMetadataList* keylistptr = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    iCurrentCommand.PVMFNodeCommand::Parse(keylistptr, valuelistptr, starting_index, max_entries);

    // Check the parameters
    if (keylistptr == NULL || valuelistptr == NULL)
    {
        return PVMFErrArgument;
    }

    uint32 numkeys = keylistptr->size();

    if (starting_index > (numkeys - 1) || numkeys <= 0 || max_entries == 0)
    {
        // Don't do anything
        return PVMFErrArgument;
    }

    uint32 numvalentries = 0;
    int32 numentriesadded = 0;
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        int32 leavecode = OsclErrNone;
        int32 leavecode1 = OsclErrNone;
        PvmiKvp KeyVal;
        KeyVal.key = NULL;
        uint32 KeyLen = 0;

        if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_CHANNELS_KEY) == 0))
        {
            // PCM output channels
            if (iNumberOfAudioChannels > 0)
            {
                // Increment the counter for the number of values found so far
                ++numvalentries;

                // Create a value entry if past the starting index
                if (numvalentries > starting_index)
                {
                    KeyLen = oscl_strlen(PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_CHANNELS_KEY) + 1; // for "codec-info/audio/channels;"
                    KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                    KeyLen += oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR) + 1; // for "uint32" and NULL terminator

                    // Allocate memory for the string
                    leavecode = OsclErrNone;
                    KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);
                    if (OsclErrNone == leavecode)
                    {
                        // Copy the key string
                        oscl_strncpy(KeyVal.key, PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_CHANNELS_KEY, oscl_strlen(PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_CHANNELS_KEY) + 1);
                        oscl_strncat(KeyVal.key, PVOMXAUDIODECMETADATA_SEMICOLON, oscl_strlen(PVOMXAUDIODECMETADATA_SEMICOLON));
                        oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                        oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR));
                        KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                        // Copy the value
                        KeyVal.value.uint32_value = iNumberOfAudioChannels;
                        // Set the length and capacity
                        KeyVal.length = 1;
                        KeyVal.capacity = 1;
                    }
                    else
                    {
                        // Memory allocation failed
                        KeyVal.key = NULL;
                        break;
                    }
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_SAMPLERATE_KEY) == 0) &&
                 (iPCMSamplingRate > 0))
        {
            // PCM output sampling rate
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                KeyLen = oscl_strlen(PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_SAMPLERATE_KEY) + 1; // for "codec-info/audio/sample-rate;"
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                KeyLen += oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR) + 1; // for "uint32" and NULL terminator

                // Allocate memory for the string
                leavecode = OsclErrNone;
                KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);
                if (OsclErrNone == leavecode)
                {
                    // Copy the key string
                    oscl_strncpy(KeyVal.key, PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_SAMPLERATE_KEY, oscl_strlen(PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_SAMPLERATE_KEY) + 1);
                    oscl_strncat(KeyVal.key, PVOMXAUDIODECMETADATA_SEMICOLON, oscl_strlen(PVOMXAUDIODECMETADATA_SEMICOLON));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                    oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING_CONSTCHAR));
                    KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                    // Copy the value
                    KeyVal.value.uint32_value = iPCMSamplingRate;
                    // Set the length and capacity
                    KeyVal.length = 1;
                    KeyVal.capacity = 1;
                }
                else
                {
                    // Memory allocation failed
                    KeyVal.key = NULL;
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_FORMAT_KEY) == 0) &&
                 iInPort != NULL)
        {
            // Format
            if ((((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_LATM) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MPEG4_AUDIO) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_3640) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ADIF) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ASF_MPEG4_AUDIO) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AAC_SIZEHDR) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IF2) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IETF) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB_IETF) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MP3) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMA) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_AUDIO)

               )
            {
                // Increment the counter for the number of values found so far
                ++numvalentries;

                // Create a value entry if past the starting index
                if (numvalentries > starting_index)
                {
                    KeyLen = oscl_strlen(PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_FORMAT_KEY) + 1; // for "codec-info/audio/format;"
                    KeyLen += oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR); // for "valtype="
                    KeyLen += oscl_strlen(PVMI_KVPVALTYPE_CHARPTR_STRING_CONSTCHAR) + 1; // for "char*" and NULL terminator

                    uint32 valuelen = 0;
                    if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_LATM)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_LATM)) + 1; // Value string plus one for NULL terminator
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MPEG4_AUDIO)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_MPEG4_AUDIO)) + 1; // Value string plus one for NULL terminator
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_3640)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_3640)) + 1; // Value string plus one for NULL terminator
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ADIF)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_ADIF)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ASF_MPEG4_AUDIO)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_ASF_MPEG4_AUDIO)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IF2)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_AMR_IF2)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IETF)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_AMR_IETF)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_AMR)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB_IETF)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_AMRWB_IETF)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_AMRWB)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MP3)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_MP3)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMA)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_WMA)) + 1;
                    }
                    else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_AUDIO)
                    {
                        valuelen = oscl_strlen(_STRLIT_CHAR(PVMF_MIME_REAL_AUDIO)) + 1;
                    }
                    else
                    {
                        // Should not enter here
                        OSCL_ASSERT(false);
                        valuelen = 1;
                    }

                    // Allocate memory for the strings
                    leavecode = OsclErrNone;
                    KeyVal.key = (char*) AllocateKVPKeyArray(leavecode, PVMI_KVPVALTYPE_CHARPTR, KeyLen);
                    if (OsclErrNone == leavecode)
                    {
                        KeyVal.value.pChar_value = (char*) AllocateKVPKeyArray(leavecode1, PVMI_KVPVALTYPE_CHARPTR, valuelen);
                    }

                    if (OsclErrNone == leavecode && OsclErrNone == leavecode1)
                    {
                        // Copy the key string
                        oscl_strncpy(KeyVal.key, PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_FORMAT_KEY, oscl_strlen(PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_FORMAT_KEY) + 1);
                        oscl_strncat(KeyVal.key, PVOMXAUDIODECMETADATA_SEMICOLON, oscl_strlen(PVOMXAUDIODECMETADATA_SEMICOLON));
                        oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_STRING_CONSTCHAR));
                        oscl_strncat(KeyVal.key, PVMI_KVPVALTYPE_CHARPTR_STRING_CONSTCHAR, oscl_strlen(PVMI_KVPVALTYPE_CHARPTR_STRING_CONSTCHAR));
                        KeyVal.key[KeyLen-1] = NULL_TERM_CHAR;
                        // Copy the value
                        if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_LATM)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_LATM), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MPEG4_AUDIO)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_MPEG4_AUDIO), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_3640)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_3640), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ADIF)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_ADIF), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ASF_MPEG4_AUDIO)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_ASF_MPEG4_AUDIO), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IF2)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_AMR_IF2), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IETF)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_AMR_IETF), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_AMR), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB_IETF)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_AMRWB_IETF), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_AMRWB), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MP3)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_MP3), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMA)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_WMA), valuelen);
                        }
                        else if (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_AUDIO)
                        {
                            oscl_strncpy(KeyVal.value.pChar_value, _STRLIT_CHAR(PVMF_MIME_REAL_AUDIO), valuelen);
                        }
                        else
                        {
                            // Should not enter here
                            OSCL_ASSERT(false);
                        }
                        KeyVal.value.pChar_value[valuelen-1] = NULL_TERM_CHAR;
                        // Set the length and capacity
                        KeyVal.length = valuelen;
                        KeyVal.capacity = valuelen;
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
            }
        }

        if (KeyVal.key != NULL)
        {
            leavecode = OsclErrNone;
            leavecode = PushKVP(KeyVal, *valuelistptr);
            if (OsclErrNone != leavecode)
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
                break;
            }

        }

    }

    return PVMFSuccess;

}

/////////////////////////////////////////////////////////////////////////////
bool PVMFOMXAudioDecNode::ReleaseAllPorts()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXAudioDecNode::ReleaseAllPorts() In"));

    if (iInPort)
    {
        iInPort->ClearMsgQueues();
        iInPort->Disconnect();
        OSCL_DELETE(((PVMFOMXDecPort*)iInPort));
        iInPort = NULL;
    }

    if (iOutPort)
    {
        iOutPort->ClearMsgQueues();
        iOutPort->Disconnect();
        OSCL_DELETE(((PVMFOMXDecPort*)iOutPort));
        iOutPort = NULL;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMFOMXAudioDecNode::CreateLATMParser()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFOMXAudioDecNode::CreateLATMParser() In"));

    // First clean up if necessary
    DeleteLATMParser();

    // Instantiate the LATM parser
    iLATMParser = OSCL_NEW(PV_LATM_Parser, ());
    if (!iLATMParser)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::CreateLATMParser() LATM parser instantiation failed"));
        return PVMFErrNoMemory;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFOMXAudioDecNode::CreateLATMParser() Out"));
    return PVMFSuccess;
}

PVMFStatus PVMFOMXAudioDecNode::DeleteLATMParser()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFOMXAudioDecNode::DeleteLATMParser() In"));

    // Delete LATM parser if there is one
    if (iLATMParser)
    {
        OSCL_DELETE(iLATMParser);
        iLATMParser = NULL;
    }

    if (iLATMConfigBuffer)
    {
        oscl_free(iLATMConfigBuffer);
        iLATMConfigBuffer = NULL;
        iLATMConfigBufferSize = 0;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFOMXAudioDecNode::DeleteLATMParser() Out"));
    return PVMFSuccess;
}

/////////////////////////////////////////////////////////////////////////////
uint32 PVMFOMXAudioDecNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXAudioDecNode::GetNumMetadataValues() called"));

    uint32 numkeys = aKeyList.size();

    if (numkeys <= 0)
    {
        // Don't do anything
        return 0;
    }

    // Count the number of value entries for the provided key list
    uint32 numvalentries = 0;
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_CHANNELS_KEY) == 0))
        {
            // Channels
            if (iNumberOfAudioChannels > 0)
            {
                ++numvalentries;
            }
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_SAMPLERATE_KEY) == 0) && (iPCMSamplingRate > 0))
        {
            // Sample rate
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVOMXAUDIODECMETADATA_CODECINFO_AUDIO_FORMAT_KEY) == 0) &&
                 iInPort != NULL)
        {
            // Format
            if ((((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_LATM) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MPEG4_AUDIO) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_3640) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ADIF) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_ASF_MPEG4_AUDIO) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IF2) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR_IETF) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMR) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB_IETF) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_AMRWB) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_MP3) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_WMA) ||
                    (((PVMFOMXDecPort*)iInPort)->iFormat == PVMF_MIME_REAL_AUDIO)

               )

            {
                ++numvalentries;
            }
        }
    }

    return numvalentries;
}


// needed for WMA parameter verification
bool PVMFOMXAudioDecNode::VerifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(num_elements);
    if (pv_mime_strcmp(aParameters->key, PVMF_BITRATE_VALUE_KEY) == 0)
    {
        if (((PVMFOMXDecPort*)iOutPort)->verifyConnectedPortParametersSync(PVMF_BITRATE_VALUE_KEY, &(aParameters->value.uint32_value)) != PVMFSuccess)
        {
            return false;
        }
        return true;
    }
    else if (pv_mime_strcmp(aParameters->key, PVMF_FORMAT_SPECIFIC_INFO_KEY) < 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::VerifyParametersSync() - Unsupported Key"));
        OSCL_ASSERT(false);
    }

    bool cap_exchange_status = false;

    pvAudioConfigParserInputs aInputs;
    OMXConfigParserInputs aInputParameters;
    AudioOMXConfigParserOutputs aOutputParameters;

    aInputs.inPtr = (uint8*)(aParameters->value.key_specific_value);
    aInputs.inBytes = (int32)aParameters->capacity;
    aInputs.iMimeType = PVMF_MIME_WMA;
    aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.wma";
    aInputParameters.inBytes = aInputs.inBytes;
    aInputParameters.inPtr = aInputs.inPtr;


    if (aInputs.inBytes == 0 || aInputs.inPtr == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::VerifyParametersSync() Codec Config data is not present"));
        return cap_exchange_status;
    }


    OMX_BOOL status = OMX_FALSE;
    OMX_U32 num_comps = 0, ii;
    OMX_STRING *CompOfRole;
    OMX_ERRORTYPE Err = OMX_ErrorNone;
    //  uint32 ii;
    // call once to find out the number of components that can fit the role
    Err = OMX_MasterGetComponentsOfRole(aInputParameters.cComponentRole, &num_comps, NULL);

    if ((num_comps > 0) && (OMX_ErrorNone == Err))
    {
        CompOfRole = (OMX_STRING *)oscl_malloc(num_comps * sizeof(OMX_STRING));
        for (ii = 0; ii < num_comps; ii++)
            CompOfRole[ii] = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));

        // call 2nd time to get the component names
        Err = OMX_MasterGetComponentsOfRole(aInputParameters.cComponentRole, &num_comps, (OMX_U8 **)CompOfRole);

        if (OMX_ErrorNone == Err)
        {
            for (ii = 0; ii < num_comps; ii++)
            {
                aInputParameters.cComponentName = CompOfRole[ii];
                status = OMX_MasterConfigParser(&aInputParameters, &aOutputParameters);
                if (status == OMX_TRUE)
                {
                    break;
                }
                else
                {
                    status = OMX_FALSE;
                }
            }
        }

        // whether successful or not, need to free CompOfRoles
        for (ii = 0; ii < num_comps; ii++)
        {
            oscl_free(CompOfRole[ii]);
            CompOfRole[ii] = NULL;
        }

        oscl_free(CompOfRole);

    }
    else
    {
        // if no components support the role, nothing else to do
        return false;
    }

    if (OMX_FALSE != status)
    {
        cap_exchange_status = true;

        iPCMSamplingRate = aOutputParameters.SamplesPerSec;
        iNumberOfAudioChannels = aOutputParameters.Channels;

        if ((iNumberOfAudioChannels != 1 && iNumberOfAudioChannels != 2) ||
                (iPCMSamplingRate <= 0))
        {
            cap_exchange_status = false;
        }
    }

    return cap_exchange_status;

}


PVMFStatus PVMFOMXAudioDecNode::DoCapConfigVerifyParameters(PvmiKvp* aParameters, int aNumElements)
{
    OSCL_UNUSED_ARG(aNumElements);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::DoCapConfigVerifyParameters() In"));

    pvAudioConfigParserInputs aInputs;
    OMXConfigParserInputs aInputParameters;
    AudioOMXConfigParserOutputs aOutputParameters;

    aInputs.inPtr = (uint8*)(aParameters->value.key_specific_value);
    aInputs.inBytes = (int32)aParameters->capacity;
    aInputs.iMimeType = iNodeConfig.iMimeType;
    aInputParameters.inBytes = aInputs.inBytes;
    aInputParameters.inPtr = aInputs.inPtr;

    if (aInputs.inBytes == 0 || aInputs.inPtr == NULL)
    {
        // in case of following formats - config codec data is expected to
        // be present in the query. If not, config parser cannot be called

        if (aInputs.iMimeType == PVMF_MIME_WMA ||
                aInputs.iMimeType == PVMF_MIME_MPEG4_AUDIO ||
                aInputs.iMimeType == PVMF_MIME_3640 ||
                aInputs.iMimeType == PVMF_MIME_LATM ||
                aInputs.iMimeType == PVMF_MIME_ADIF ||
                aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO ||
                aInputs.iMimeType == PVMF_MIME_AAC_SIZEHDR ||
                aInputs.iMimeType == PVMF_MIME_REAL_AUDIO)
        {
            if (aInputs.iMimeType == PVMF_MIME_LATM)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::DoCapConfigVerifyParameters() Codec Config data is not present"));
                return PVMFErrNotSupported;
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::DoCapConfigVerifyParameters() Codec Config data is not present"));

                // DV TO_DO: remove this
                OSCL_LEAVE(OsclErrNotSupported);
            }
        }
    }

    // in case of ASF_MPEG4 audio need to create aac config header
    if (aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO)
    {
        if (!CreateAACConfigDataFromASF(aInputParameters.inPtr, aInputParameters.inBytes, &iAACConfigData[0], iAACConfigDataLength))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFOMXAudioDecNode::DoCapConfigVerifyParameters() - Error in creating AAC Codec Config data"));


            return PVMFErrNotSupported;
        }

        aInputParameters.inPtr = &iAACConfigData[0];
        aInputParameters.inBytes = iAACConfigDataLength;
    }


    if (aInputs.iMimeType == PVMF_MIME_MPEG4_AUDIO ||
            aInputs.iMimeType == PVMF_MIME_3640 ||
            aInputs.iMimeType == PVMF_MIME_LATM ||
            aInputs.iMimeType == PVMF_MIME_ADIF ||
            aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO ||
            aInputs.iMimeType == PVMF_MIME_AAC_SIZEHDR)
    {
        aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.aac";
    }
    // AMR
    else if (aInputs.iMimeType == PVMF_MIME_AMR_IF2 ||
             aInputs.iMimeType == PVMF_MIME_AMR_IETF ||
             aInputs.iMimeType == PVMF_MIME_AMR)
    {
        aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.amrnb";
    }
    else if (aInputs.iMimeType == PVMF_MIME_AMRWB_IETF ||
             aInputs.iMimeType == PVMF_MIME_AMRWB)
    {
        aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.amrwb";
    }
    else if (aInputs.iMimeType == PVMF_MIME_MP3)
    {
        aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.mp3";
    }
    else if (aInputs.iMimeType ==  PVMF_MIME_WMA)
    {
        aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.wma";
    }
    else if (aInputs.iMimeType ==  PVMF_MIME_REAL_AUDIO)
    {
        aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.ra";
    }
    else
    {
        // Illegal codec specified.
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::PVMFOMXAudioDecNode::DoCapConfigVerifyParameters() Input port format other then codec type", iName.Str()));
    }

    OMX_BOOL status = OMX_FALSE;
    OMX_U32 num_comps = 0, ii;
    OMX_STRING *CompOfRole;
    OMX_ERRORTYPE Err = OMX_ErrorNone;
    //  uint32 ii;
    // call once to find out the number of components that can fit the role
    Err = OMX_MasterGetComponentsOfRole(aInputParameters.cComponentRole, &num_comps, NULL);

    if ((num_comps > 0) && (OMX_ErrorNone == Err))
    {
        CompOfRole = (OMX_STRING *)oscl_malloc(num_comps * sizeof(OMX_STRING));
        for (ii = 0; ii < num_comps; ii++)
            CompOfRole[ii] = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));

        // call 2nd time to get the component names
        Err = OMX_MasterGetComponentsOfRole(aInputParameters.cComponentRole, &num_comps, (OMX_U8 **)CompOfRole);

        if (OMX_ErrorNone == Err)
        {
            for (ii = 0; ii < num_comps; ii++)
            {
                aInputParameters.cComponentName = CompOfRole[ii];
                status = OMX_MasterConfigParser(&aInputParameters, &aOutputParameters);
                if (status == OMX_TRUE)
                {
                    break;
                }
                else
                {
                    status = OMX_FALSE;
                }
            }
        }

        // whether successful or not, need to free CompOfRoles
        for (ii = 0; ii < num_comps; ii++)
        {
            oscl_free(CompOfRole[ii]);
            CompOfRole[ii] = NULL;
        }

        oscl_free(CompOfRole);

    }
    else
    {
        // if no component supports the role, nothing else to do
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::DoCapConfigVerifyParameters() No omx component supports this role PVMFErrNotSupported"));
        return PVMFErrNotSupported;
    }

    if (status == OMX_FALSE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::DoCapConfigVerifyParameters() ->OMXConfigParser() PVMFErrNotSupported"));
        return PVMFErrNotSupported;
    }

    if (aInputs.iMimeType == PVMF_MIME_WMA)
    {
        iNumberOfAudioChannels = aOutputParameters.Channels;
        iPCMSamplingRate = aOutputParameters.SamplesPerSec;
    }
    else if (aInputs.iMimeType == PVMF_MIME_MPEG4_AUDIO ||
             aInputs.iMimeType == PVMF_MIME_3640 ||
             aInputs.iMimeType == PVMF_MIME_LATM ||
             aInputs.iMimeType == PVMF_MIME_ADIF ||
             aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO ||
             aInputs.iMimeType == PVMF_MIME_AAC_SIZEHDR)
    {
        iNumberOfAudioChannels = aOutputParameters.Channels;
        iPCMSamplingRate = aOutputParameters.SamplesPerSec;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::DoCapConfigVerifyParameters() Out"));
    return PVMFSuccess;
}

void PVMFOMXAudioDecNode::DoCapConfigSetParameters(PvmiKvp* aParameters, int aNumElements, PvmiKvp* &aRetKVP)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() In"));

    if (aParameters == NULL || aNumElements < 1)
    {
        aRetKVP = aParameters;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() Passed in parameter invalid"));
        return;
    }

    int32 ii;

    for (ii = 0; ii < aNumElements; ii ++)
    {

        // find out if the audio dec format key is used for the query
        if (pv_mime_strcmp(aParameters[ii].key, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() set audio format specific info"));

            if (iTrackUnderVerificationConfig)
            {
                oscl_free(iTrackUnderVerificationConfig);
                iTrackUnderVerificationConfig = NULL;
                iTrackUnderVerificationConfigSize = 0;
            }

            iTrackUnderVerificationConfigSize = aParameters[ii].capacity;

            if (iTrackUnderVerificationConfigSize > 0)
            {
                iTrackUnderVerificationConfig = (uint8*)(oscl_malloc(sizeof(uint8) * iTrackUnderVerificationConfigSize));
                oscl_memcpy(iTrackUnderVerificationConfig, aParameters[ii].value.key_specific_value, iTrackUnderVerificationConfigSize);
            }

        }
        else if (pv_mime_strcmp(aParameters[ii].key,  _STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) == 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() set preferred omx component list"));


            // each kvp paramater corresponds to one omx component - a new item in the vector is created

            iOMXPreferredComponentOrderVec.push_back((OMX_STRING)(aParameters[ii].value.pChar_value));


        }
        else if (pv_mime_strcmp(aParameters[ii].key, PVMF_AUDIO_DEC_FORMAT_TYPE_VALUE_KEY) == 0)
        {
            // set the mime type if audio format is being used
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() set audio dec format type to %s", aParameters->value.pChar_value));

            iNodeConfig.iMimeType = aParameters[ii].value.pChar_value;
        }
        else if (pv_mime_strcmp(aParameters[ii].key, PVMF_AUDIO_DEC_PCM_BUFFER_DURATION_KEY) == 0)
        {
            // set the mime type if audio format is being used
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() set audio dec pcm buffer size to %d milliseconds", aParameters->value.uint32_value));

            iOutputBufferPCMDuration = aParameters[ii].value.uint32_value;
            if (iOutputBufferPCMDuration == 0)
            {

                aRetKVP = aParameters; // indicate "error" by setting return KVP to original
                iOutputBufferPCMDuration = PVOMXAUDIODEC_DEFAULT_OUTPUTPCM_TIME;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() set audio dec pcm buffer size to 0 milliseconds not allowed, reveting to default of %d", iOutputBufferPCMDuration));

            }

        }
        else if (pv_mime_strcmp(aParameters[ii].key, PVMF_AUDIO_SILENCE_INSERTION_KEY) == 0)
        {
            // Enable or Disable silence insertion in audio tracks
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() Silence insertion key \n"));

            iSilenceInsertionFlag = aParameters[ii].value.bool_value;
        }
        else
        {
            // For now, ignore other queries
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() Key not used"));

            // indicate "error" by setting return KVP to the original
            aRetKVP = aParameters;
            break;
        }
    }



    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::DoCapConfigSetParameters() Out"));
}

void PVMFOMXAudioDecNode::CalculateBOCParameters()
{
    OSCL_ASSERT(iBOCReceived);

    iBOCBytesToSkip = iBOCSamplesToSkip * iNumberOfAudioChannels * 2; // assuming 16-bit data
    iBOCBeginningOfContentTS = iBOCTimeStamp + ((iBOCSamplesToSkip * iInTimeScale + iPCMSamplingRate - 1) / iPCMSamplingRate);
}

void PVMFOMXAudioDecNode::HandleBOCProcessing(OMX_BUFFERHEADERTYPE* aBuffer)
{
    if (aBuffer->nTimeStamp > (((int64)iBOCBeginningOfContentTS *(int64)iTimeScale) / (int64)iInTimeScale))
    {
        // buffer time stamp is passed the beginning of content time stamp before dropping all BOC samples
        // this is most likely due to a reposition during the BOC silence dropping
        iBOCReceived = false;

        return;
    }

    if (iBOCBytesToSkip < aBuffer->nFilledLen)
    {
        aBuffer->nOffset += iBOCBytesToSkip;
        aBuffer->nFilledLen -= iBOCBytesToSkip;

        iBOCBytesToSkip = 0;
        iBOCReceived = false;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::HandleBOCProcessing: Dropping %d samples from buffer %x due to BOC", iBOCSamplesToSkip, (OsclAny*) aBuffer->pAppPrivate));
    }
    else
    {
        iBOCBytesToSkip -= aBuffer->nFilledLen;
        aBuffer->nFilledLen = 0;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::HandleBOCProcessing: Dropping %d samples (the entire buffer) from buffer %x due to BOC.  There are %d samples more to drop.", aBuffer->nFilledLen, (OsclAny*) aBuffer->pAppPrivate, iBOCSamplesToSkip));
    }
}

void PVMFOMXAudioDecNode::CalculateEOCParameters()
{
    OSCL_ASSERT(iEOCReceived);

    // calculate the total number of frames
    // timestamp is from media msg, therefore in milliseconds
    uint32 totalFrames = (uint32)(((int64)(iEOCTimeStamp - iTSOfFirstDataMsgAfterBOS) * (int64)iPCMSamplingRate + (((int64)iInTimeScale * (int64)iSamplesPerFrame) >> 1)) / ((int64)iInTimeScale * (int64)iSamplesPerFrame) + iEOCFramesToFollow); // round to frame boundaries

    // calculate the frame at which silence removal should begin
    iEOCSilenceRemovalStartFrame = totalFrames - ((iEOCSamplesToSkip + iSamplesPerFrame - 1) / iSamplesPerFrame);

    // calculate the offset in samples within the frame at which silence removal should begin
    iEOCSilenceRemovalSampleOffset = iSamplesPerFrame - (iEOCSamplesToSkip % iSamplesPerFrame);
}

void PVMFOMXAudioDecNode::HandleEOCProcessing(OMX_BUFFERHEADERTYPE* aBuffer)
{
    uint32 bytesPerSample = 2; // assuming 16-bit data
    uint32 bufferLengthSamples = aBuffer->nFilledLen / (bytesPerSample * iNumberOfAudioChannels);

    if (iBufferContainsIntFrames)
    {
        uint32 outputBufferStartFrame;
        uint32 outputBufferEndFrame;
        uint32 framesInBuffer;

        // calculate everything in frames, since timestamps won't be sample accurate.
        // also assume that there is always an integer number of frames per buffer.
        // note: timestamp is from OMX buffer, therefore in microseconds
        // note: frames are 0-indexed here
        int64 normTS = aBuffer->nTimeStamp - (((int64)iTSOfFirstDataMsgAfterBOS * (int64)iTimeScale) / (int64)iInTimeScale);
        outputBufferStartFrame = (uint32)((normTS * (int64)iPCMSamplingRate + (((int64)iTimeScale * (int64)iSamplesPerFrame) >> 1)) / ((int64)iTimeScale * (int64)iSamplesPerFrame)); // round to frame boundaries
        framesInBuffer = bufferLengthSamples / iSamplesPerFrame; // no rounding needed since length should be an integer number of frames

        OSCL_ASSERT((bufferLengthSamples % iSamplesPerFrame) == 0); // double check

        outputBufferEndFrame = outputBufferStartFrame + framesInBuffer - 1;

        // check to see if silence removal begins within this buffer
        if (outputBufferEndFrame >= iEOCSilenceRemovalStartFrame)
        {
            uint32 validSamples;
            uint32 newFilledLen;
            uint32 skippedSamples;

            validSamples = OSCL_MAX((int32) iEOCSilenceRemovalStartFrame - (int32)outputBufferStartFrame, 0) * iSamplesPerFrame + iEOCSilenceRemovalSampleOffset;
            newFilledLen = validSamples * bytesPerSample * iNumberOfAudioChannels;
            skippedSamples = bufferLengthSamples - validSamples;

            iEOCSamplesToSkip = OSCL_MAX((int32) iEOCSamplesToSkip - (int32) skippedSamples, 0);

            if (iEOCSamplesToSkip == 0)
            {
                iEOCReceived = false;
            }
            else
            {
                // update start point information
                iEOCSilenceRemovalSampleOffset += skippedSamples;

                iEOCSilenceRemovalStartFrame += iEOCSilenceRemovalSampleOffset / iSamplesPerFrame;
                iEOCSilenceRemovalSampleOffset = iEOCSilenceRemovalSampleOffset % iSamplesPerFrame;
            }

            OSCL_ASSERT(newFilledLen <= aBuffer->nFilledLen);

            aBuffer->nFilledLen = newFilledLen;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::HandleEOCProcessing: Dropping %d samples from buffer %x due to EOC", skippedSamples, (OsclAny*) aBuffer->pAppPrivate));
        }
    }
    else
    {
        // the method for gapless silence removal without int frames per buffer isn't guaranteed to be sample accurate

        uint32 outputBufferStartSample;
        uint32 outputBufferEndSample;
        uint32 EOCSilenceRemovalStartSample = (iEOCSilenceRemovalStartFrame * iSamplesPerFrame) + iEOCSilenceRemovalSampleOffset;

        // find the difference between the time based off of timestamps and that based off of accumulated samples
        // the timestamps have a max error dependent on their resolution (timescale) equivalent to: ceil(sampling_rate / timescale)
        // if the accumulated sample count is within this max error then most likely no data has been dropped, and the accumulated sample
        // count is more accurate than the timestamps.  however, if it is outside of that error window then we should use the
        // calculation based off of the timestamps; since while the ts calculation may not be sample accurate, it will be more accurate
        // than the accumulated sample count in the case of dropped data
        int64 normTS = aBuffer->nTimeStamp - (((int64)iTSOfFirstDataMsgAfterBOS * (int64)iTimeScale) / (int64)iInTimeScale);

        int32 sampleDelta = (int32)(((normTS * (int64)iPCMSamplingRate) / (int64)iTimeScale) - (iClipSampleCount - bufferLengthSamples));
        int32 tsMaxError = (iPCMSamplingRate + (iInTimeScale - 1)) / iInTimeScale;

        if (oscl_abs(sampleDelta) < tsMaxError)
        {
            // use accumulated sample count based calculation

            outputBufferStartSample = iClipSampleCount - bufferLengthSamples;
            outputBufferEndSample = iClipSampleCount;
        }
        else
        {
            // some data must have been dropped, use the timestamp based calculation

            outputBufferStartSample = (uint32)((normTS * (int64)iPCMSamplingRate) / (int64)iTimeScale);
            outputBufferEndSample = outputBufferStartSample + bufferLengthSamples;
        }


        // check to see if silence removal begins within this buffer
        if (outputBufferEndSample >= EOCSilenceRemovalStartSample)
        {
            uint32 validSamples;
            uint32 newFilledLen;
            uint32 skippedSamples;

            validSamples = OSCL_MAX((int32)EOCSilenceRemovalStartSample - (int32) outputBufferStartSample, 0);
            newFilledLen = validSamples * bytesPerSample * iNumberOfAudioChannels;
            skippedSamples = bufferLengthSamples - validSamples;

            iEOCSamplesToSkip = OSCL_MAX((int32) iEOCSamplesToSkip - (int32) skippedSamples, 0);

            if (iEOCSamplesToSkip == 0)
            {
                iEOCReceived = false;
            }
            else
            {
                // update start point information
                iEOCSilenceRemovalSampleOffset += skippedSamples;

                iEOCSilenceRemovalStartFrame += iEOCSilenceRemovalSampleOffset / iSamplesPerFrame;
                iEOCSilenceRemovalSampleOffset = iEOCSilenceRemovalSampleOffset % iSamplesPerFrame;
            }

            OSCL_ASSERT(newFilledLen <= aBuffer->nFilledLen);

            aBuffer->nFilledLen = newFilledLen;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::HandleEOCProcessing: Dropping %d samples from buffer %x due to EOC", skippedSamples, (OsclAny*) aBuffer->pAppPrivate));
        }
    }

}

PVMFStatus PVMFOMXAudioDecNode::RetrieveMP3FrameLength(uint8 *pBuffer)
{

    PVMFStatus err = PVMFSuccess;
    uint32  temp;
    uint8  tmp1;
    uint8  tmp2;
    uint8  tmp3;
    uint8  tmp4;
    uint32  version;

    /*
     *  MPEG Audio Version ID
     */
    tmp1 = *pBuffer;
    tmp2 = *(pBuffer + 1);
    tmp3 = *(pBuffer + 2);
    tmp4 = *(pBuffer + 3);

    temp = ((((uint32)(tmp1)) << 24) |
            (((uint32)(tmp2)) << 16) |
            (((uint32)(tmp3)) <<  8) |
            (((uint32)(tmp4))));

    if (((temp >> 21) & MP3_SYNC_WORD) != MP3_SYNC_WORD)
    {
        return PVMFErrCorrupt;
    }

    temp = (temp << 11) >> 11;


    switch (temp >> 19)  /* 2 */
    {
        case 0:
            version = MPEG_2_5;
            break;
        case 2:
            version = MPEG_2;
            break;
        case 3:
            version = MPEG_1;
            break;
        default:
            version = INVALID_VERSION;
            err = PVMFErrNotSupported;
            break;
    }



    iSamplesPerFrame = (version == MPEG_1) ?
                       2 * MP3_SUBBANDS_NUMBER * MP3_FILTERBANK_BANDS :
                       MP3_SUBBANDS_NUMBER * MP3_FILTERBANK_BANDS;

    return err;
}

PVMFStatus PVMFOMXAudioDecNode::CountSamplesInBuffer(OMX_BUFFERHEADERTYPE* aBuffer)
{
    if (aBuffer->nTimeStamp < (((int64)iTSOfFirstDataMsgAfterBOS *(int64)iTimeScale) / (int64)iInTimeScale))
    {
        // time stamp is before BOS (i.e. the buffer could be from the port flush)
        // therefore don't count this buffer
        return PVMFSuccess;
    }

    uint32 bytesPerSample = 2; // assuming 16-bit data

    if ((0 == iNumberOfAudioChannels) ||
            (0 == iSamplesPerFrame))
    {
        return PVMFFailure;
    }

    uint32 samplesInBuffer = aBuffer->nFilledLen / (bytesPerSample * iNumberOfAudioChannels);

    // one way toggle
    if ((0 != (samplesInBuffer % iSamplesPerFrame)) && (iBufferContainsIntFrames == true))
    {
        iBufferContainsIntFrames = false;
    }

    iClipSampleCount += samplesInBuffer;

    return PVMFSuccess;
}

// needed for AAC in ASF
typedef enum
{
    AAC_LC = 1, // should map to 2
    AAC_LTP = 3 // should map to 4
} PROFILE_TYPES;

typedef enum
{
    AAC_LC_SBR = 5 // should map to 5
    , ENHANCED_AAC_LC_SBR_PS // should map to 29
    , INVALID_OBJ_TYPE = 0xffffffff
} EXTERNAL_OBJECT_TYPES;

typedef enum
{
    AOT_NULL_OBJECT      = 0,
    AOT_AAC_LC           = 2,   // LC
    AOT_AAC_LTP          = 4,   // LTP
    AOT_SBR              = 5,   // SBR
    AOT_PS               = 29,  // PS

} eAUDIO_OBJECT_TYPE;

static const uint32 srIndexTable[16] = {96000, 88200, 64000, 48000, 44100,
                                        32000, 24000, 22050, 16000, 12000,
                                        11025,  8000,  7350,  0, 0, 0
                                       };


bool PVMFOMXAudioDecNode::CreateAACConfigDataFromASF(uint8 *inptr, uint32 inlen, uint8 *outptr, uint32 &outlen)
{

    if (NULL == inptr || inlen < 20)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::CreateAACConfigDataFromASF() Error - insufficient config data"));

        return false;
    }



    uint32 *valPtr = OSCL_STATIC_CAST(uint32 *, inptr);
    EXTERNAL_OBJECT_TYPES ExternlObjType = INVALID_OBJ_TYPE;
    eAUDIO_OBJECT_TYPE objType = AOT_NULL_OBJECT;

    uint32 OutSampRate = 0;
    uint32 SamplingRate = 0;
    uint32 NumChannels = 0;  /* checks below assume unsigned type
                              * (i.e., less than 0 not explicitly
                              * checked, so don't change to signed.
                              */
    uint32 SamplesPerFrame = 0;

    valPtr++; // this field represents frame length in bytes (we're not interested right now)
    // e.g. if there's 1024 samples per frame stereo, frame length is 4096 (2x2x1024)
    SamplesPerFrame = *valPtr++; // read number of samples per frame (1024 or 2048)
    SamplingRate = *valPtr++;
    NumChannels = *valPtr++;

    PROFILE_TYPES profile = OSCL_STATIC_CAST(PROFILE_TYPES, *valPtr++);

    if (inlen > 20)
    {
        OutSampRate = *valPtr++;
        ExternlObjType = OSCL_STATIC_CAST(EXTERNAL_OBJECT_TYPES, *valPtr++);
        // the next field is the Down sampling mode - which we dont care about here
        //iDownSampMode = *valPtr++;
    }


    // Now - need to convert the ASF External obj type into proper AAC decoder audio obj type

    if (INVALID_OBJ_TYPE == ExternlObjType)
    {
        objType = OSCL_STATIC_CAST(eAUDIO_OBJECT_TYPE, profile + 1);
        OutSampRate = SamplingRate;
    }
    else if (AAC_LC_SBR == ExternlObjType)
    {
        objType = AOT_SBR;
    }
    else if (ENHANCED_AAC_LC_SBR_PS == ExternlObjType)
    {
        objType =  AOT_PS;
    }
    else
    {
        objType = OSCL_STATIC_CAST(eAUDIO_OBJECT_TYPE, profile + 1);
        OutSampRate = SamplingRate;
    }

    uint32 framelength = 1024; // hardcode to fit plain AAC case - since we use plain aac sampling rate as well
    int32 i;


    uint8 *aConfigHeader = outptr;

    /* Write AOT */
    if (objType > AOT_PS)
    {
        return false;
    }

    aConfigHeader[0] = (uint8)(objType << 3);   /* AudioObjectType  5 bits */

    /* Write Sampling frequency */
    for (i = 0; i < 15; i++)
    {
        if (srIndexTable[i] ==  SamplingRate)
        {
            break;
        }
    }

    if (i > 13)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::CreateAACConfigDataFromASF() Error - unsupported sampling rate"));

        return false;
    }

    aConfigHeader[0] |= (uint8)(i >> 1);    /* SamplingRate  3 of 4 bits */
    aConfigHeader[1]  = (uint8)(i << 7);    /* SamplingRate  1 of 4 bits */

    /* Write Number of Channels */
    // NumChannels is unsigned so can't be less than 0
    // only need to check if the value is too large.
    if ((NumChannels > 2))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::CreateAACConfigDataFromASF() Error - unsupported number of channels"));

        return false;
    }


    aConfigHeader[1] |= (uint8)(NumChannels << 3);   /* NumChannels  4 of 4 bits */


    /* detect extension AOT */
    if ((objType == AOT_SBR) || (objType == AOT_PS))
    {
        /* Write Final Sampling frequency */

        for (i = 0; i < 15; i++)
        {
            if (srIndexTable[i] == OutSampRate)
            {
                break;
            }
        }

        if (i > 13)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::CreateAACConfigDataFromASF() Error - unsupported sampling rate"));

            return false;
        }

        aConfigHeader[1] |= (uint8)(i >> 1);   /* SamplingRate  1 of 4 bits */
        aConfigHeader[2]  = (uint8)(i << 7);   /* SamplingRate  3 of 4 bits */

        aConfigHeader[2] |= (uint8)((AOT_AAC_LC) << 2);  /* external AudioObjectType  5 bits */

        aConfigHeader[3]  = (framelength == 960) ? 4 : 0;      /* framelength  1 of 1 bits (shifted) */
        aConfigHeader[3] |=  0;        /* dependsOnCoreCoder 1 bits, 0, see ISO/IEC 14496-3 Subpart 4, 4.4.1 */
        aConfigHeader[3] |=  0;        /* Extension Flag: Shall be 0 for supported aot */

        outlen = 4;

    }
    else
    {
        aConfigHeader[1] |= (framelength == 960) ? 4 : 0;      /* framelength  1 of 1 bits (shifted) */

        aConfigHeader[1] |=  0;        /* dependsOnCoreCoder 1 bits, 0, see ISO/IEC 14496-3 Subpart 4, 4.4.1 */
        aConfigHeader[1] |=  0;        /* Extension Flag: Shall be 0 for supported aot */

        outlen = 2;
    }


    return true;
}


PVMFStatus PVMFOMXAudioDecNode::DoCapConfigReleaseParameters(PvmiKvp* aParameters, int aNumElements)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXAudioDecNode::DoCapConfigReleaseParameters() In"));

    if (aParameters == NULL || aNumElements < 1)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::DoCapConfigReleaseParameters() KVP list is NULL or number of elements is 0"));
        return PVMFErrArgument;
    }


    if (pv_mime_strcmp(aParameters[0].key, _STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) == 0)
    {
        // everything was allocated in blocks, so it's enough to release the beginning
        oscl_free(aParameters[0].key);
        oscl_free(aParameters[0].value.pChar_value);
        // Free memory for the parameter list
        oscl_free(aParameters);
        aParameters = NULL;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXAudioDecNode::DoCapConfigReleaseParameters() Out"));
        return PVMFSuccess;
    }
    else if (pv_mime_strcmp(aParameters[0].key, _STRLIT_CHAR(PVMF_AUDIO_DEC_PCM_BUFFER_DURATION_KEY)) == 0)
    {
        oscl_free(aParameters[0].key);
        oscl_free(aParameters);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXAudioDecNode::DoCapConfigReleaseParameters() Out"));
        return PVMFSuccess;
    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::DoCapConfigReleaseParameters() KVP list is NULL or number of elements is 0"));
    return PVMFErrArgument;
}

PVMFStatus PVMFOMXAudioDecNode::DoCapConfigGetParametersSync(PvmiKeyType aIdentifier, PvmiKvp*& aParameters, int& aNumParamElements, PvmiCapabilityContext aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXAudioDecNode::DoCapConfigGetParametersSync() In"));
    OSCL_UNUSED_ARG(aContext);

    // Initialize the output parameters
    aNumParamElements = 0;
    aParameters = NULL;

    if (pv_mime_strcmp(aIdentifier, _STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) == 0)
    {
        // get the list of omx components that support the track.
        // Track config info and mime type was previously set through capconfig - setparametersync

        pvAudioConfigParserInputs aInputs;
        OMXConfigParserInputs aInputParameters;
        AudioOMXConfigParserOutputs aOutputParameters;

        aInputs.inPtr = (uint8*) iTrackUnderVerificationConfig;
        aInputs.inBytes = (int32) iTrackUnderVerificationConfigSize;
        aInputs.iMimeType = iNodeConfig.iMimeType;

        aInputParameters.inBytes = aInputs.inBytes;
        aInputParameters.inPtr = aInputs.inPtr;

        if (aInputs.inBytes == 0 || aInputs.inPtr == NULL)
        {
            // in case of following formats - config codec data is expected to
            // be present in the query. If not, config parser cannot be called

            if (aInputs.iMimeType == PVMF_MIME_WMA ||
                    aInputs.iMimeType == PVMF_MIME_MPEG4_AUDIO ||
                    aInputs.iMimeType == PVMF_MIME_3640 ||
                    aInputs.iMimeType == PVMF_MIME_LATM ||
                    aInputs.iMimeType == PVMF_MIME_ADIF ||
                    aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO ||
                    aInputs.iMimeType == PVMF_MIME_AAC_SIZEHDR)
            {

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXAudioDecNode::DoCapConfigGetParameters() Codec Config data is not present"));
                return PVMFErrNotSupported;

            }
        }


        // in case of ASF_MPEG4 audio need to create aac config header
        if (aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO)
        {
            if (!CreateAACConfigDataFromASF(aInputParameters.inPtr, aInputParameters.inBytes, &iAACConfigData[0], iAACConfigDataLength))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFOMXAudioDecNode::DoCapConfigGetParameters() - Error in creating AAC Codec Config data"));


                return PVMFErrNotSupported;
            }

            aInputParameters.inPtr = &iAACConfigData[0];
            aInputParameters.inBytes = iAACConfigDataLength;
        }


        if (aInputs.iMimeType == PVMF_MIME_MPEG4_AUDIO ||
                aInputs.iMimeType == PVMF_MIME_3640 ||
                aInputs.iMimeType == PVMF_MIME_LATM ||
                aInputs.iMimeType == PVMF_MIME_ADIF ||
                aInputs.iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO ||
                aInputs.iMimeType == PVMF_MIME_AAC_SIZEHDR)
        {
            aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.aac";
        }
        // AMR
        else if (aInputs.iMimeType == PVMF_MIME_AMR_IF2 ||
                 aInputs.iMimeType == PVMF_MIME_AMR_IETF ||
                 aInputs.iMimeType == PVMF_MIME_AMR)
        {
            aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.amrnb";
        }
        else if (aInputs.iMimeType == PVMF_MIME_AMRWB_IETF ||
                 aInputs.iMimeType == PVMF_MIME_AMRWB)
        {
            aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.amrwb";
        }
        else if (aInputs.iMimeType == PVMF_MIME_MP3)
        {
            aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.mp3";
        }
        else if (aInputs.iMimeType ==  PVMF_MIME_WMA)
        {
            aInputParameters.cComponentRole = (OMX_STRING)"audio_decoder.wma";
        }
        else
        {
            // Illegal codec specified.
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::PVMFOMXAudioDecNode::DoCapConfigGetParameters() Input port format other then codec type", iName.Str()));
            return PVMFErrNotSupported;
        }




        OMX_BOOL status = OMX_FALSE;
        OMX_U32 num_comps = 0;
        OMX_STRING *CompOfRole;
        OMX_U32 ii;
        OMX_ERRORTYPE Err = OMX_ErrorNone;

        // call once to find out the number of components that can fit the role
        Err = OMX_MasterGetComponentsOfRole(aInputParameters.cComponentRole, &num_comps, NULL);




        if ((num_comps > 0) && (OMX_ErrorNone == Err))
        {
            // allocate num_comps kvps and keys all in one block

            aParameters = (PvmiKvp*)oscl_malloc(num_comps * sizeof(PvmiKvp));
            if (aParameters == NULL)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for KVP failed"));
                return PVMFErrNoMemory;
            }

            oscl_memset(aParameters, 0, num_comps*sizeof(PvmiKvp));

            // Allocate memory for the key strings in each KVP
            PvmiKeyType memblock = (PvmiKeyType)oscl_malloc(num_comps * (sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) + 1) * sizeof(char));
            if (memblock == NULL)
            {
                oscl_free(aParameters);
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for key string failed"));
                return PVMFErrNoMemory;
            }
            oscl_strset(memblock, 0, num_comps *(sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) + 1) * sizeof(char));


            // allocate a block of memory for component names
            OMX_U8 *memblockomx = (OMX_U8 *) oscl_malloc(num_comps * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
            if (memblockomx == NULL)
            {
                oscl_free(aParameters);
                oscl_free(memblock);
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for omx component strings failed"));
                return PVMFErrNoMemory;
            }

            oscl_memset(memblockomx, 0, num_comps * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));

            // allocate a placeholder
            CompOfRole = (OMX_STRING *)oscl_malloc(num_comps * sizeof(OMX_STRING));

            for (ii = 0; ii < num_comps; ii++)
            {
                CompOfRole[ii] = (OMX_STRING)(memblockomx + (ii * PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8)));
            }

            // call 2nd time to get the component names
            Err = OMX_MasterGetComponentsOfRole(aInputParameters.cComponentRole, &num_comps, (OMX_U8 **)CompOfRole);
            if (OMX_ErrorNone == Err)
            {
                for (ii = 0; ii < num_comps; ii++)
                {
                    aInputParameters.cComponentName = CompOfRole[ii];
                    status = OMX_MasterConfigParser(&aInputParameters, &aOutputParameters);
                    if (status == OMX_TRUE)
                    {
                        // component passes the test - write the component name into kvp list
                        // write the key
                        aParameters[ii].key = memblock + (aNumParamElements * (sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)) + 1) * sizeof(char)) ;
                        oscl_strncat(aParameters[ii].key, _STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY), sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY)));
                        aParameters[ii].key[sizeof(_STRLIT_CHAR(PVMF_DEC_AVAILABLE_OMX_COMPONENTS_KEY))] = 0; // null terminate

                        // write the length
                        aParameters[ii].length = PV_OMX_MAX_COMPONENT_NAME_LENGTH;

                        aParameters[ii].value.pChar_value = CompOfRole[ii];
                        aNumParamElements++;

                    }

                }
            }

            // free memory for CompOfRole placeholder.
            // The other blocks of memory will be freed during release parameter sync
            oscl_free(CompOfRole);
        }
        else
        {
            // if no component supports the role, nothing else to do
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s::PVMFOMXVideoDecNode::DoCapConfigGetParameters() No OMX components support the role", iName.Str()));
            return PVMFErrNotSupported;
        }


        return PVMFSuccess;


    }
    else if (pv_mime_strcmp(aIdentifier, PVMF_AUDIO_DEC_PCM_BUFFER_DURATION_KEY) == 0)
    {
        // set the mime type if audio format is being used
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::DoCapConfigGetParameters() audio dec pcm buffer size is %d milliseconds", iOutputBufferPCMDuration));

        // allocate kvp
        aNumParamElements = 1;
        aParameters = (PvmiKvp*)oscl_malloc(sizeof(PvmiKvp));
        if (aParameters == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for KVP failed"));
            return PVMFErrNoMemory;
        }
        oscl_memset(aParameters, 0, sizeof(PvmiKvp));

        // Allocate memory for the key strings in each KVP
        PvmiKeyType memblock = (PvmiKeyType)oscl_malloc((sizeof(_STRLIT_CHAR(PVMF_AUDIO_DEC_PCM_BUFFER_DURATION_KEY)) + 1) * sizeof(char));
        if (memblock == NULL)
        {
            oscl_free(aParameters);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Memory allocation for key string failed"));
            return PVMFErrNoMemory;
        }
        oscl_strset(memblock, 0, (sizeof(_STRLIT_CHAR(PVMF_AUDIO_DEC_PCM_BUFFER_DURATION_KEY)) + 1) * sizeof(char));

        aParameters->key = memblock;

        oscl_strncat(aParameters->key, _STRLIT_CHAR(PVMF_AUDIO_DEC_PCM_BUFFER_DURATION_KEY), sizeof(_STRLIT_CHAR(PVMF_AUDIO_DEC_PCM_BUFFER_DURATION_KEY)));
        aParameters->key[sizeof(_STRLIT_CHAR(PVMF_AUDIO_DEC_PCM_BUFFER_DURATION_KEY))] = 0; // null terminate


        aParameters->value.uint32_value = iOutputBufferPCMDuration;

    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Out"));
    if (aNumParamElements == 0)
    {
        // If no one could get the parameter, return error
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFOMXVideoDecNode::DoCapConfigGetParametersSync() Unsupported key"));
        return PVMFFailure;
    }
    else
    {
        return PVMFSuccess;
    }

}
