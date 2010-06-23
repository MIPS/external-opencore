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

#include "pvmf_mp4ffparser_node_tuneables.h"

#include "impeg4file.h"

#include "media_clock_converter.h"

#include "oscl_snprintf.h"

#include "pvmf_fileformat_events.h"

#include "pvmf_mp4ffparser_events.h"

#include "pvmf_media_msg_format_ids.h"

#include "h263decoderspecificinfo.h"

#include "m4v_config_parser.h"

#include "oscl_exclusive_ptr.h"

#include "oscl_string_utils.h"


#define PVMF_MP4_MIME_FORMAT_AUDIO_UNKNOWN  "x-pvmf/audio/unknown"
#define PVMF_MP4_MIME_FORMAT_VIDEO_UNKNOWN  "x-pvmf/video/unknown"
#define PVMF_MP4_MIME_FORMAT_UNKNOWN        "x-pvmf/unknown-media/unknown"

static const char PVMF_PIFF_AUTH_DATA_PLAYREADY_OBJECT[] = "PlayReady object";
static const uint8 CPM_SUPPORTED_IV_SZ = 8;

// Read Each Track Individually
#define TRACK_NO_PER_RESET_PLAYBACK_CALL 1
#define MAX_TRACK_NO 256

#define DEFAULTPROFILE  0
#define DEFAULTLEVEL    0

#define MILLISECOND_TIMESCALE 1000
#define DEFAULT_VECTOR_RESERVE_NUM 16

static const uint32 MAX32BITUINT = 0xFFFFFFFF;

PVMFMP4FFParserNode::PVMFMP4FFParserNode(int32 aPriority) :
        PVMFNodeInterfaceImpl(aPriority, "PVMFMP4FFParserNode")
        , iPortIter(NULL)
        , iGaplessLogger(NULL)
        , iBackwardReposFlag(false) /* To avoid backwardlooping :: A flag to remember backward repositioning */
        , iForwardReposFlag(false)
        , iPlayBackDirection(PVMF_DATA_SOURCE_DIRECTION_FORWARD)
        , iParseAudioDuringFF(false)
        , iParseAudioDuringREW(false)
        , iParseVideoOnly(false)
        , iOpenFileOncePerTrack(true)
        , iIFrameOnlyFwdPlayback(false)
        , iDataRate(NORMAL_PLAYRATE)
        , minFileOffsetTrackID(0)
        , iTotalMoofFrags(0)
        , iCPMDecryptionDoneInplace(false)
{
    iNumClipsInPlayList = 0;
    iPlaybackClipIndex = -1;
    iLastPlayingClipIndex = -1;
    iClipIndexForMetadata = -1;
    iPlaylistRepositioning = false;
    iNextInitializedClipIndex = -1;
    iPlaylistExhausted = false;
    iInitNextClip = false;
    iPlaybackParserObj = NULL;
    iMetadataParserObj = NULL;
    iFirstClipNonAudioOnly = false;

    iClientPlayBackClock = NULL;
    iClockNotificationsInf = NULL;
    autopaused = false;
    iDownloadFileSize = 0;
    download_progress_interface = NULL;
    iThumbNailMode = false;
    iPreviewMode = false;
    iProtectedFile = false;
    iCPM = NULL;
    iCPMSessionID = 0xFFFFFFFF;
    iCPMContentType = PVMF_CPM_CONTENT_FORMAT_UNKNOWN;
    iCPMContentAccessFactory = NULL;
    iDecryptionInterface = NULL;
    iOMA2DecryptionBuffer = NULL;
    iCPMInitCmdId = 0;
    iCPMOpenSessionCmdId = 0;
    iCPMRegisterContentCmdId = 0;
    iCPMRequestUsageId = 0;
    iCPMUsageCompleteCmdId = 0;
    iCPMCloseSessionCmdId = 0;
    iCPMSequenceInProgress = false;
    iCPMResetCmdId = 0;
    iRequestedUsage.key = NULL;
    iApprovedUsage.key = NULL;
    iAuthorizationDataKvp.key = NULL;
    oWaitingOnLicense  = false;
    iPoorlyInterleavedContentEventSent = false;
    iIsByteSeekNotSupported = false;


    iParsingMode = PVMF_MP4FF_PARSER_NODE_ENABLE_PARSER_OPTIMIZATION;
    oIsAACFramesFragmented = PVMFMP4FF_BREAKUP_AAC_FRAMES_INTO_MULTIPLE_MEDIA_FRAGS;

    iUseCPMPluginRegistry = false;

    iMP4HeaderSize             = 0;
    iDataStreamInterface       = NULL;
    iDataStreamFactory         = NULL;
    iDataStreamReadCapacityObserver = NULL;
    iDownloadComplete          = false;
    iProgressivelyPlayable  = false;
    iFastTrackSession          = false;
    iLastNPTCalcInConvertSizeToTime = 0;
    iFileSizeLastConvertedToTime = 0;

    iExternalDownload = false;
    iUnderFlowEventReported = false;
    iUnderFlowCheckTimer = NULL;

    iCPMMetaDataExtensionInterface = NULL;
    iCPMLicenseInterface             = NULL;
    iCPMLicenseInterfacePVI          = NULL;
    iCPMGetMetaDataValuesCmdId     = 0;
    iCPMGetLicenseInterfaceCmdId     = 0;

    minTime = 0;
    avgTime = 0;
    maxTime = 0;
    sumTime = 0;
    iDiagnosticsLogged = false;
    iPortDataLog = false;
    iTimeTakenInReadMP4File = 0;
    iCurPos = 0;
    iDelayAddToNextTextSample = 0;
    iTextInvalidTSAfterReposition = false;
    iEOTForTextSentToMIO = false;
    iSetTextSampleDurationZero = false;

    iCacheSize = DEFAULT_CAHCE_SIZE;
    iAsyncReadBuffSize = DEFAULT_ASYNC_READ_BUFFER_SIZE;
    iPVLoggerEnableFlag = false;
    iPVLoggerStateEnableFlag = false;
    iNativeAccessMode = DEFAULT_NATIVE_ACCESS_MODE;

    iDataStreamRequestPending = false;

    iJitterBufferDurationInMs = PVMF_MP4FFPARSER_NODE_PSEUDO_STREAMING_BUFFER_DURATION_IN_MS;
    iBaseKey = INVALID;
    iJitterBufferDurationInMs = PVMF_MP4FFPARSER_NODE_PSEUDO_STREAMING_BUFFER_DURATION_IN_MS;
    iBaseKey = INVALID;

    iDataPathLogger = PVLogger::GetLoggerObject("datapath.sourcenode.mp4parsernode");
    iAVCDataPathLogger = PVLogger::GetLoggerObject("datapath.sourcenode.mp4parsernode.avc");
    if (iAVCDataPathLogger != NULL)
    {
        iAVCDataPathLogger->DisableAppenderInheritance();
    }
    iClockLogger = PVLogger::GetLoggerObject("clock");
    iDiagnosticsLogger = PVLogger::GetLoggerObject("pvplayerdiagnostics.mp4parsernode");
    iGaplessLogger = PVLogger::GetLoggerObject("gapless.sourcenode");

    iFileServer.Connect();

    int32 err;
    OSCL_TRY(err,

             //Set the node capability data.
             iNodeCapability.iCanSupportMultipleInputPorts = false;
             iNodeCapability.iCanSupportMultipleOutputPorts = true;
             iNodeCapability.iHasMaxNumberOfPorts = true;
             iNodeCapability.iMaxNumberOfPorts = 6;
             iNodeCapability.iInputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_MPEG4FF));
             iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_AMR_IETF));
             iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_AMRWB_IETF));
             iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_MPEG4_AUDIO));
             iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_MP3)); /* support mp3 track */
             iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_M4V));
             iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_H2631998));
             iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_H2632000));
             iNodeCapability.iOutputFormatCapability.push_back(PVMFFormatType(PVMF_MIME_H264_VIDEO_MP4));

             iUnderFlowCheckTimer = PVMF_BASE_NODE_NEW(OsclTimer<OsclMemAllocator>,
                                    ("PVMFMP4FFNodeUnderFlowTimer"));

             iUnderFlowCheckTimer->SetObserver(this);
             iUnderFlowCheckTimer->SetFrequency(PVMP4FF_UNDERFLOW_STATUS_EVENT_FREQUENCY);
             iMoofInfoUpdateVec.reserve(DEFAULT_VECTOR_RESERVE_NUM);
            );

    iPortIter = PVMF_BASE_NODE_NEW(PVMFMP4FFPortIter, (iNodeTrackPortList));
    if (iPortIter == NULL)
    {
        err = OsclErrNoMemory;
    }

    if (err != OsclErrNone)
    {
        //if a leave happened, cleanup and re-throw the error
        iInputCommands.clear();
        iNodeCapability.iInputFormatCapability.clear();
        iNodeCapability.iOutputFormatCapability.clear();
        OSCL_CLEANUP_BASE_CLASS(PVMFNodeInterfaceImpl);
        OSCL_LEAVE(err);
    }

    // initialize clip information vector
    iClipInfoList.reserve(PVMF_MP4_MAX_NUM_TRACKS_GAPLESS);
}


PVMFMP4FFParserNode::~PVMFMP4FFParserNode()
{
    //remove the clock observer
    if (iClientPlayBackClock != NULL)
    {
        if (iClockNotificationsInf != NULL)
        {
            iClockNotificationsInf->RemoveClockStateObserver(*this);
            iClientPlayBackClock->DestroyMediaClockNotificationsInterface(iClockNotificationsInf);
            iClockNotificationsInf = NULL;
        }
    }

    LogDiagnostics();
    Cancel();

    if (iUnderFlowCheckTimer != NULL)
    {
        iUnderFlowCheckTimer->Clear();
    }
    PVMF_BASE_NODE_DELETE(iUnderFlowCheckTimer);

    // release the download progress interface if any
    if (download_progress_interface != NULL)
    {
        download_progress_interface->cancelResumeNotification();
        download_progress_clock.Unbind();
        download_progress_interface->removeRef();
        download_progress_interface = NULL;
    }

    iPortActivityQueue.clear();
    ReleaseAllPorts();
    RemoveAllCommands();

    PVMF_BASE_NODE_DELETE(iPortIter);
    iPortIter = NULL;

    CleanupFileSource();

    iFileServer.Close();

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

    iDataPathLogger = NULL;
    iAVCDataPathLogger = NULL;
    iClockLogger = NULL;
    iDiagnosticsLogger = NULL;
    iGaplessLogger = NULL;

}

void PVMFMP4FFParserNode::addRef()
{
    ++iExtensionRefCount;
}


void PVMFMP4FFParserNode::removeRef()
{
    --iExtensionRefCount;
}

PVMFPortIter* PVMFMP4FFParserNode::GetPorts(const PVMFPortFilter* aFilter)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetPorts() called"));

    OSCL_UNUSED_ARG(aFilter);
    iPortIter->Reset();
    return iPortIter;
}

PVMFStatus PVMFMP4FFParserNode::QueryInterfaceSync(PVMFSessionId aSession,
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

bool PVMFMP4FFParserNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::queryInterface() In"));

    if (uuid == PVMF_TRACK_SELECTION_INTERFACE_UUID)
    {
        PVMFTrackSelectionExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFTrackSelectionExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMF_DATA_SOURCE_INIT_INTERFACE_UUID)
    {
        PVMFDataSourceInitializationExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFDataSourceInitializationExtensionInterface*, this);
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
    else if (uuid == PVMF_TRACK_LEVEL_INFO_INTERFACE_UUID)
    {
        PVMFTrackLevelInfoExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFTrackLevelInfoExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (PVMF_FF_PROGDOWNLOAD_SUPPORT_INTERFACE_UUID == uuid)
    {
        PVMFFormatProgDownloadSupportInterface* myInterface = OSCL_STATIC_CAST(PVMFFormatProgDownloadSupportInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMIDatastreamuserInterfaceUuid)
    {
        PVMIDatastreamuserInterface* myInterface = OSCL_STATIC_CAST(PVMIDatastreamuserInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface =  OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMFCPMPluginLicenseInterfaceUuid)
    {
        iface = OSCL_STATIC_CAST(PVInterface*, iCPMLicenseInterface);
    }
    else if (uuid == PvmfDataSourceDirectionControlUuid)
    {
        PvmfDataSourceDirectionControlInterface* myInterface = OSCL_STATIC_CAST(PvmfDataSourceDirectionControlInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else
    {
        return false;
    }
    return true;
}


PVMFStatus PVMFMP4FFParserNode::SetSourceInitializationData(OSCL_wString& aSourceURL,
        PVMFFormatType& aSourceFormat,
        OsclAny* aSourceData,
        uint32 aClipIndex,
        PVMFFormatTypeDRMInfo aType)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetSourceInitializationData() In ClipIndex %d", aClipIndex));
    if ((int32)aClipIndex == iPlaybackClipIndex)
    {
        return PVMFErrAlreadyExists;
    }

    PVMFMp4ClipInfo mp4clipInfo;
    // initiliaze parser object as null
    mp4clipInfo.iParserObj = NULL;
    mp4clipInfo.iAvailableMetadataKeys.clear();
    mp4clipInfo.iTotalID3MetaDataTagInValueList = 0;
    mp4clipInfo.iMetadataValueCount = 0;

    PVMFSourceClipInfo info;
    iUpdateExistingClip = false;
    // if the clip was already initialized no updates can be excepted
    // for this .
    // if there were no updates in existing data, treat this as no-op success
    // else update the existing clip info data.
    uint32 index = 0;
    for (index = 0; index < iClipInfoList.size(); index++)
    {
        if (iClipInfoList[index].iClipInfo.GetClipIndex() == aClipIndex)
        {
            if (iClipInfoList[index].iClipInfo.iIsInitialized)
            {
                if (false == ValidateSourceInitializationParams(aSourceURL, aSourceFormat, aClipIndex, iClipInfoList[index].iClipInfo))
                {
                    // clip is already initialized, updates cant be accepted now.
                    return PVMFErrAlreadyExists;
                }
                // this would mean that we there was no update for this track hence mark it as a no-op
                return PVMFSuccess;
            }
            iUpdateExistingClip = true;
            break;
        }
    }

    // intialize clip info
    info.SetSourceURL(aSourceURL);
    info.SetClipIndex(aClipIndex);
    info.SetFileHandle(NULL); //initialize file handle

    if (iPlaybackClipIndex == -1 && aClipIndex == 0)
    {
        // either information is recieved for the first time for zeroth index
        // of zeroth index is being updated again first same index is being updated    //cleanup any prior source.
        CleanupFileSource();
    }

    if (aClipIndex == 0)
    {
        // check if earlier mime type was correct, if it was, ignore this update
        if (aSourceFormat == PVMF_MIME_FORMAT_UNKNOWN &&
                iClipInfoList[index].iClipInfo.GetFormatType() == PVMF_MIME_MPEG4FF)
        {
            // same url is being updated, dont update the format type.
            info.SetFormatType(iClipInfoList[index].iClipInfo.GetFormatType());
        }
        else
        {
            // for clip index == 0 only mp4 and pvx and smooth streaming mime types are
            // supported by this node
            if (aSourceFormat != PVMF_MIME_MPEG4FF &&
                    aSourceFormat != PVMF_MIME_DATA_SOURCE_PVX_FILE &&
                    aSourceFormat != PVMF_MIME_DATA_SOURCE_SMOOTH_STREAMING_URL)
            {
                // Node doesnt support any other format
                return PVMFErrNotSupported;
            }
        }
    }
    else
    {
        // for clip index > 0 only mp4 and unknown mime types are supported by this node
        if (aSourceFormat != PVMF_MIME_MPEG4FF &&
                aSourceFormat != PVMF_MIME_FORMAT_UNKNOWN)
        {
            return PVMFErrNotSupported;
        }
    }

    PVMFFormatType inputFormatType = aSourceFormat;

    /* In case of FT we need to know about it, else init would not complete until the file is fully downloaded */
    if (aSourceFormat == PVMF_MIME_DATA_SOURCE_PVX_FILE)
    {
        iFastTrackSession = true;
        inputFormatType = PVMF_MIME_MPEG4FF;
    }
    info.SetFormatType(inputFormatType);

    if (inputFormatType == PVMF_MIME_MPEG4FF ||
            inputFormatType == PVMF_MIME_FORMAT_UNKNOWN)
    {
        if (aClipIndex > 0)
        {
            info.SetFormatType(inputFormatType);
        }

        if (aType != PVMF_FORMAT_TYPE_CONNECT_UNPROTECTED)
        {
            // Create a new CPM object if one does not exist already
            if (!iCPM)
            {
                //create a CPM object here...
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetSourceInitializationData() create CPM obj"));
                iUseCPMPluginRegistry = true;
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
        }
        else
        {
            //skip CPM if we for sure the content is unprotected
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetSourceInitializationData() non-create CPM obj"));
            iUseCPMPluginRegistry = false;
        }

        if (aSourceData)
        {
            PVInterface* pvInterface =
                OSCL_STATIC_CAST(PVInterface*, aSourceData);
            PVInterface* localDataSrc = NULL;
            PVUuid localDataSrcUuid(PVMF_LOCAL_DATASOURCE_UUID);
            if (pvInterface->queryInterface(localDataSrcUuid, localDataSrc))
            {
                PVMFLocalDataSource* opaqueData =
                    OSCL_STATIC_CAST(PVMFLocalDataSource*, localDataSrc);
                iPreviewMode = opaqueData->iPreviewMode;
                uint32 intent = opaqueData->iIntent;
                if (intent & BITMASK_PVMF_SOURCE_INTENT_THUMBNAILS)
                {
                    iThumbNailMode = true;
                }
                info.iCPMSourceData.iPreviewMode = iPreviewMode;
                info.iCPMSourceData.iIntent = opaqueData->iIntent;
                if (opaqueData->iFileHandle)
                {
                    OsclFileHandle* fileHandle = PVMF_BASE_NODE_NEW(OsclFileHandle, (*(opaqueData->iFileHandle)));
                    info.SetFileHandle(fileHandle);
                    info.iCPMSourceData.iFileHandle = fileHandle;
                }
                info.iCPMSourceData.iPreviewMode = opaqueData->iPreviewMode;
                info.iCPMSourceData.iIntent = opaqueData->iIntent;

                if (opaqueData->iContentAccessFactory != NULL)
                {
                    if (iUseCPMPluginRegistry == false)
                    {
                        iExternalDownload = true;
                        iDataStreamFactory = opaqueData->iContentAccessFactory;
                        //We need an instance of datastream around to check if
                        //we have enough data to even successfully parser ASF header
                        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
                        PVInterface* iFace =
                            iDataStreamFactory->CreatePVMFCPMPluginAccessInterface(uuid);
                        if (iFace != NULL)
                        {
                            iDataStreamInterface = OSCL_STATIC_CAST(PVMIDataStreamSyncInterface*, iFace);
                            iDataStreamInterface->OpenSession(iDataStreamSessionID, PVDS_READ_ONLY);
                        }
                        //Next check to see if datastreamfactory supports download progress interface
                        //some might to deal with auto-pause / auto-resume feature
                        download_progress_interface = NULL;
                        PVInterface* intf = OSCL_STATIC_CAST(PVInterface*, download_progress_interface);
                        if (iDataStreamFactory->queryInterface(PVMF_DOWNLOAD_PROGRESS_INTERFACE_UUID, intf))
                        {
                            download_progress_interface = (PVMFDownloadProgressInterface*)(intf);
                            PVMFFormatProgDownloadSupportInterface* formatPDLIntf =
                                OSCL_STATIC_CAST(PVMFFormatProgDownloadSupportInterface*, this);
                            download_progress_interface->setFormatDownloadSupportInterface(formatPDLIntf);
                        }
                    }
                    else
                    {
                        //Cannot have both plugin usage and a datastream factory
                        return PVMFErrArgument;
                    }
                }
            }
            else
            {
                // first check download context data
                PVUuid uuid(PVMF_SOURCE_CONTEXT_DATA_DOWNLOAD_HTTP_UUID);
                PVInterface* temp = NULL;
                if (pvInterface->queryInterface(uuid, temp))
                {
                }

                PVInterface* sourceDataContext = NULL;
                PVInterface* commonDataContext = NULL;
                PVUuid sourceContextUuid(PVMF_SOURCE_CONTEXT_DATA_UUID);
                PVUuid commonContextUuid(PVMF_SOURCE_CONTEXT_DATA_COMMON_UUID);
                if (pvInterface->queryInterface(sourceContextUuid, sourceDataContext))
                {
                    if (sourceDataContext->queryInterface(commonContextUuid, commonDataContext))
                    {
                        PVMFSourceContextDataCommon* cContext =
                            OSCL_STATIC_CAST(PVMFSourceContextDataCommon*, commonDataContext);
                        iPreviewMode = cContext->iPreviewMode;
                        uint32 intent = cContext->iIntent;
                        if (intent & BITMASK_PVMF_SOURCE_INTENT_THUMBNAILS)
                        {
                            iThumbNailMode = true;
                        }
                        OsclFileHandle* fileHandle = NULL;
                        if (cContext->iFileHandle)
                        {
                            fileHandle = PVMF_BASE_NODE_NEW(OsclFileHandle, (*(cContext->iFileHandle)));
                        }
                        info.SetFileHandle(fileHandle);

                        if (cContext->iContentAccessFactory != NULL)
                        {
                            if (iUseCPMPluginRegistry == false)
                            {
                                iExternalDownload = true;
                                iDataStreamFactory = cContext->iContentAccessFactory;
                                //We need an instance of datastream around to check if
                                //we have enough data to even successfully parser ASF header
                                PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
                                PVInterface* iFace =
                                    iDataStreamFactory->CreatePVMFCPMPluginAccessInterface(uuid);
                                if (iFace != NULL)
                                {
                                    iDataStreamInterface = OSCL_STATIC_CAST(PVMIDataStreamSyncInterface*, iFace);
                                    iDataStreamInterface->OpenSession(iDataStreamSessionID, PVDS_READ_ONLY);
                                }
                                //Next check to see if datastreamfactory supports download progress interface
                                //some might to deal with auto-pause / auto-resume feature
                                download_progress_interface = NULL;
                                PVInterface* intf = OSCL_STATIC_CAST(PVInterface*, download_progress_interface);
                                if (iDataStreamFactory->queryInterface(PVMF_DOWNLOAD_PROGRESS_INTERFACE_UUID, intf))
                                {
                                    download_progress_interface = (PVMFDownloadProgressInterface*)(intf);
                                    PVMFFormatProgDownloadSupportInterface* formatPDLIntf =
                                        OSCL_STATIC_CAST(PVMFFormatProgDownloadSupportInterface*, this);
                                    download_progress_interface->setFormatDownloadSupportInterface(formatPDLIntf);
                                }
                            }
                            else
                            {
                                //Cannot have both plugin usage and a datastream factory
                                return PVMFErrArgument;
                            }
                        }
                        PVMFSourceContextData* sContext =
                            OSCL_STATIC_CAST(PVMFSourceContextData*, sourceDataContext);
                        info.iSourceContextData = *sContext;
                        info.iSourceContextDataValid = true;
                    }
                }
            }
        }
        if (iUpdateExistingClip)
        {
            iClipInfoList[aClipIndex].iClipInfo = info;
        }
        else
        {
            mp4clipInfo.iClipInfo = info;
            iClipInfoList.push_back(mp4clipInfo);
            iNumClipsInPlayList++;
        }

        // this is a new clip addition request but the existing clip list has exhausted
        // need to wake up source node.
        if (!iUpdateExistingClip && iPlaylistExhausted)
        {
            if (iInterfaceState == EPVMFNodeStarted &&
                    PVMFSuccess == InitNextValidClipInPlaylist(NULL, aClipIndex))
            {
                PVMFStatus status = PVMFFailure;
                for (uint32 i = 0; i < iNodeTrackPortList.size(); ++i)
                {
                    IMpeg4File* parserObj = GetParserObjAtIndex(iNextInitializedClipIndex);
                    if (iNumClipsInPlayList > 1 &&
                            iNextInitializedClipIndex < (int32)iNumClipsInPlayList && // iNextInitializedClip index is within the clip list
                            NULL != parserObj) //next clip has been intialized successfully
                    {
                        iPlaybackClipIndex = iNextInitializedClipIndex; // pick up the next clip in list
                        iPlaybackParserObj = parserObj;
                        iNodeTrackPortList[i].iSendBOS = true;
                        iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                        Reschedule();
                        status = PVMFSuccess;
                        iPlaylistExhausted = false;
                    }
                    else
                    {
                        status = PVMFFailure;;
                    }
                }
                return status;
            }
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFMP4FFParserNode::SetSourceInitializationData() Out ClipIndex [%d]", aClipIndex));
        return PVMFSuccess;
    }
    return PVMFFailure;
}

PVMFStatus PVMFMP4FFParserNode::SetClientPlayBackClock(PVMFMediaClock* aClock)
{
    if (aClock == NULL)
    {
        return PVMFErrArgument;
    }

    if (iClockNotificationsInf && iClientPlayBackClock)
    {
        iClockNotificationsInf->RemoveClockStateObserver(*this);
        iClientPlayBackClock->DestroyMediaClockNotificationsInterface(iClockNotificationsInf);
        iClockNotificationsInf = NULL;
    }
    iClientPlayBackClock = aClock;
    iClientPlayBackClock->ConstructMediaClockNotificationsInterface(iClockNotificationsInf, *this);

    if (NULL == iClockNotificationsInf)
    {
        return PVMFErrNoMemory;
    }

    iClockNotificationsInf->SetClockStateObserver(*this);

    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::SetEstimatedServerClock(PVMFMediaClock* /*aClientClock*/)
{
    return PVMFErrNotSupported;
}

PVMFStatus PVMFMP4FFParserNode::GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetMediaPresentationInfo() called"));

    // Check to make sure the MP4 file has been parsed
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode::GetMediaPresentationInfo() In"));

    if (NULL == iPlaybackParserObj || iPlaybackClipIndex == -1)
    {
        // start playback from last playing back clip
        iPlaylistExhausted = true;

        PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
        if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
        {
            dsFactory = iDataStreamFactory;
        }
        PVMFStatus status = InitNextValidClipInPlaylist(NULL, iPlaybackClipIndex, dsFactory);

        iPlaybackParserObj = GetParserObjAtIndex(iPlaybackClipIndex);
        if (status != PVMFSuccess)
            return status;
    }

    int32 iNumTracks = iPlaybackParserObj->getNumTracks();
    // Protection
    if (iNumTracks > PVMFFFPARSERNODE_MAX_NUM_TRACKS)
    {
        iNumTracks = PVMFFFPARSERNODE_MAX_NUM_TRACKS;
    }

    aInfo.setDurationValue(iPlaybackParserObj->getMovieDuration());
    aInfo.setDurationTimeScale(iPlaybackParserObj->getMovieTimescale());

    uint32 iIdList[16];
    if (iNumTracks != iPlaybackParserObj->getTrackIDList(iIdList, iNumTracks))
    {
        return PVMFFailure;
    }

    for (int32 i = iNumTracks - 1; i >= 0; i--)
    {
        PVMFTrackInfo tmpTrackInfo;

        tmpTrackInfo.setTrackID(iIdList[i]);
        // Set the port tag to the track ID
        tmpTrackInfo.setPortTag(iIdList[i]);

        uint32 aBitRate = iPlaybackParserObj->getTrackAverageBitrate(iIdList[i]);
        tmpTrackInfo.setTrackBitRate(aBitRate);

        uint64 timescale = iPlaybackParserObj->getTrackMediaTimescale(iIdList[i]);
        tmpTrackInfo.setTrackDurationTimeScale(timescale);

        // in movie timescale
        uint64 trackDuration = iPlaybackParserObj->getTrackMediaDuration(iIdList[i]);
        tmpTrackInfo.setTrackDurationValue(trackDuration);

        OSCL_HeapString<OsclMemAllocator> trackMIMEType;
        iPlaybackParserObj->getTrackMIMEType(iIdList[i], trackMIMEType);

        OSCL_FastString iMime;
        iMime.set(trackMIMEType.get_str(), oscl_strlen(trackMIMEType.get_str()));

        PVMFFormatType trackformattype = trackMIMEType.get_str();

        tmpTrackInfo.setTrackMimeType(iMime);

        OsclRefCounterMemFrag config;
        if (!RetrieveTrackConfigInfo(iIdList[i], trackformattype, config))
        {
            return PVMFFailure;
        }
        tmpTrackInfo.setTrackConfigInfo(config);

        aInfo.addTrackInfo(tmpTrackInfo);
    }
    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::SelectTracks(PVMFMediaPresentationInfo& aInfo)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SelectTracks() called"));

    // Copy the selected tracks to internal list
    iSelectedTrackInfoList.clear();

    uint32 i;
    for (i = 0; i < aInfo.getNumTracks(); ++i)
    {
        iSelectedTrackInfoList.push_back(*(aInfo.getTrackInfo(i)));
    }

    return PVMFSuccess;
}


PVMFCommandId PVMFMP4FFParserNode::SetDataSourcePosition(PVMFSessionId aSessionId,
        PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aActualNPT,
        PVMFTimestamp& aActualMediaDataTS,
        bool aSeekToSyncPoint,
        uint32 aStreamID,
        OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode::SetDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION, aTargetNPT, &aActualNPT,
                                   &aActualMediaDataTS, aSeekToSyncPoint, aStreamID, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFMP4FFParserNode::SetDataSourcePosition(PVMFSessionId aSessionId,
        PVMFDataSourcePositionParams& aPVMFDataSourcePositionParams,
        OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode::SetDataSourcePosition() for playlist"));
    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId,
                                   PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION,
                                   &aPVMFDataSourcePositionParams, aContext);

    iPlaylistRepositioning = true;
    return QueueCommandL(cmd);

}

PVMFCommandId PVMFMP4FFParserNode::QueryDataSourcePosition(PVMFSessionId aSessionId, PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aActualNPT,
        bool aSeekToSyncPoint,
        OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    if (aActualNPT > aTargetNPT) /* eg of backward aActualNPT (CurrentPos) = 10, aTargetNPT (NewPos) = 2 */
    {
        iBackwardReposFlag = true; /* To avoid backwardlooping */
        aActualNPT = 0;
    }
    else
    {
        iForwardReposFlag = true;
        iCurPos = aActualNPT;
        aActualNPT = 0;
    }

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, &aActualNPT,
                                   aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFMP4FFParserNode::QueryDataSourcePosition(PVMFSessionId aSessionId, PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aSeekPointBeforeTargetNPT,
        PVMFTimestamp& aSeekPointAfterTargetNPT,
        OsclAny* aContext,
        bool aSeekToSyncPoint)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT,
                                   aSeekPointBeforeTargetNPT, aSeekPointAfterTargetNPT, aContext, aSeekToSyncPoint);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFMP4FFParserNode::SetDataSourceRate(PVMFSessionId aSessionId, int32 aRate, PVMFTimebase* aTimebase, OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetDataSourceRate() called"));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_RATE, aRate, aTimebase, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFMP4FFParserNode::SetDataSourceDirection(PVMFSessionId aSessionId, int32 aDirection, PVMFTimestamp& aActualNPT,
        PVMFTimestamp& aActualMediaDataTS, PVMFTimebase* aTimebase, OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetDataSourceDirection() called"));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_DIRECTION, aDirection, aActualNPT, aActualMediaDataTS,
                                   aTimebase, aContext);

    return QueueCommandL(cmd);
}

PVMFStatus PVMFMP4FFParserNode::GetAvailableTracks(Oscl_Vector<PVMFTrackInfo, OsclMemAllocator>& aTracks)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetAvailableTracks() called"));

    PVMFMediaPresentationInfo mediainfo;
    mediainfo.Reset();
    PVMFStatus retval = GetMediaPresentationInfo(mediainfo);
    if (retval != PVMFSuccess)
    {
        return retval;
    }

    for (uint32 i = 0; i < mediainfo.getNumTracks(); ++i)
    {
        PVMFTrackInfo* trackinfo = mediainfo.getTrackInfo(i);
        if (trackinfo)
        {
            aTracks.push_back(*trackinfo);
        }
        else
        {
            // This shouldn't happen
            OSCL_ASSERT(false);
            return PVMFFailure;
        }
    }

    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::GetTimestampForSampleNumber(PVMFTrackInfo& aTrackInfo, uint32 aSampleNum, PVMFTimestamp& aTimestamp)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetTimestampForSampleNumber() called"));

    if (iPlaybackParserObj == NULL)
    {
        return PVMFErrInvalidState;
    }
    // Retrieve the timestamp in track media timescale for the specified sample number
    uint64 ts = 0;
    if (iPlaybackParserObj->getTimestampForSampleNumber(aTrackInfo.getTrackID(), aSampleNum, ts) != EVERYTHING_FINE)
    {
        return PVMFFailure;
    }

    // Convert the timestamp from media timescale to milliseconds
    MediaClockConverter mcc(iPlaybackParserObj->getTrackMediaTimescale(aTrackInfo.getTrackID()));
    mcc.set_clock(ts, 0);
    aTimestamp = mcc.get_converted_ts(1000);

    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::GetSampleNumberForTimestamp(PVMFTrackInfo& aTrackInfo, PVMFTimestamp aTimestamp, uint32& aSampleNum)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetSampleNumberForTimestamp() called"));

    if (iPlaybackParserObj == NULL)
    {
        return PVMFErrInvalidState;
    }

    // Convert the timestamp to media timescale value
    MediaClockConverter mcc(1000);
    mcc.set_clock(aTimestamp, 0);
    uint64 mediats = mcc.get_converted_ts64(iPlaybackParserObj->getTrackMediaTimescale(aTrackInfo.getTrackID()));

    // Retrieve the sample number corresponding to the specified timestamp
    uint32 samplenum = 0;
    MP4_ERROR_CODE retval = iPlaybackParserObj->getSampleNumberClosestToTimeStamp(aTrackInfo.getTrackID(), samplenum, mediats);
    if (retval == EVERYTHING_FINE || retval == END_OF_TRACK)
    {
        // Conversion worked
        aSampleNum = samplenum;
    }
    else
    {
        // Error
        aSampleNum = 0;
        return PVMFFailure;
    }

    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::GetNumberOfSyncSamples(PVMFTrackInfo& aTrackInfo, int32& aNumSyncSamples)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetNumberOfSyncSamples() called"));

    if (iPlaybackParserObj == NULL)
    {
        return PVMFErrInvalidState;
    }

    // Use the MP4 FF API to retrieve the number of sync samples in a track
    uint32 numsamples = 0;
    int32 retval = iPlaybackParserObj->getTimestampForRandomAccessPoints(aTrackInfo.getTrackID(), &numsamples, NULL, NULL);

    if (retval == 2)
    {
        // All samples are sync samples.
        aNumSyncSamples = -1;
    }
    else if (retval == 1)
    {
        // OK
        aNumSyncSamples = (int32)numsamples;
    }
    else
    {
        // Error
        aNumSyncSamples = 0;
        return PVMFFailure;
    }

    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::GetSyncSampleInfo(PVMFTrackInfo& aTrackInfo, PVMFSampleNumTSList& aList, uint32 aStartIndex, int32 aMaxEntries)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetSyncSampleInfo() called"));

    if (aMaxEntries == 0 || aMaxEntries < -1)
    {
        // max number of entries must be more than 1 and only -1 has special meaning
        return PVMFErrArgument;
    }
    if (iPlaybackParserObj == NULL)
    {
        return PVMFErrInvalidState;
    }

    // Determine the total number of sync samples
    int32 totalnum = 0;
    PVMFStatus status = GetNumberOfSyncSamples(aTrackInfo, totalnum);
    if (status != PVMFSuccess)
    {
        // Can't determine the total number of sync samples so error out
        return status;
    }
    if (totalnum < 1)
    {
        // There are no sync samples or all frames are sync samples so error out
        return PVMFFailure;
    }
    if (aStartIndex >= (uint32)totalnum)
    {
        // The starting index is more than the total number of sync samples
        return PVMFErrArgument;
    }

    // Determine the number of sync sample info to retrieve
    uint32 numsamplestoget = (uint32)totalnum;
    if ((aMaxEntries > 0) && (aStartIndex + aMaxEntries) < (uint32)totalnum)
    {
        // Reduce the amount
        numsamplestoget = aStartIndex + aMaxEntries;
    }

    // Allocate memory for the info
    uint64* syncts = PVMF_BASE_NODE_ARRAY_NEW(uint64, numsamplestoget);
    uint32* syncfrnum = PVMF_BASE_NODE_ARRAY_NEW(uint32, numsamplestoget);
    if (syncts == NULL || syncfrnum == NULL)
    {
        if (syncts)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncts);
        }
        if (syncfrnum)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
        }
        return PVMFErrNoMemory;
    }

    // Retrieve the list of timestamp and frame numbers for sync samples.
    int32 retval = iPlaybackParserObj->getTimestampForRandomAccessPoints(aTrackInfo.getTrackID(), &numsamplestoget, syncts, syncfrnum);
    if (retval != 1)
    {
        // Error
        if (syncts)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncts);
        }
        if (syncfrnum)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
        }
        return PVMFFailure;
    }

    // Copy the info
    aList.clear();
    uint32 ii;
    PVMFSampleNumTS syncnumts;
    MediaClockConverter mcc(iPlaybackParserObj->getTrackMediaTimescale(aTrackInfo.getTrackID()));
    for (ii = aStartIndex; ii < numsamplestoget; ++ii)  // numsamplestoget does incorporate the aMaxEntries limit
    {
        syncnumts.iSampleNumber = syncfrnum[ii];
        // Use the media clock converter to convert from timestamp in media timescale to millisec
        mcc.update_clock(syncts[ii]);
        syncnumts.iTimestamp = mcc.get_converted_ts(1000);
        // Add to provided list
        aList.push_back(syncnumts);
    }

    if (syncts)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(syncts);
    }
    if (syncfrnum)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
    }
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::GetSyncSampleInfo(PVMFSampleNumTSList& aList, PVMFTrackInfo& aTrackInfo, int32 aTargetTimeInMS, uint32 aHowManySamples)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetSyncSampleInfo() called"));

    if (iPlaybackParserObj == NULL)
    {
        return PVMFErrInvalidState;
    }

    MediaClockConverter mcc1(1000);
    mcc1.update_clock((uint32)aTargetTimeInMS);
    uint64 targetTimestamp =
        mcc1.get_converted_ts64(iPlaybackParserObj->getTrackMediaTimescale(aTrackInfo.getTrackID()));

    int32 totalnum = 0;
    PVMFStatus status = GetNumberOfSyncSamples(aTrackInfo, totalnum);
    if (status != PVMFSuccess)
    {
        // Can't determine the total number of sync samples so error out
        return status;
    }
    if (totalnum < 1)
    {
        // There are no sync samples or all frames are sync samples so error out
        return PVMFFailure;
    }
    if (aTargetTimeInMS < 0)
        return PVMFErrArgument;

    uint32 numsamplestoget = (2 * aHowManySamples); //multiply by 2 for after and before
    if (numsamplestoget > (uint32)totalnum)
    {
        aHowManySamples = 1;
        numsamplestoget = 2;
    }
    // Allocate memory for the info
    uint64* syncts = PVMF_BASE_NODE_ARRAY_NEW(uint64, numsamplestoget + 1);
    uint32* syncfrnum = PVMF_BASE_NODE_ARRAY_NEW(uint32, numsamplestoget + 1);
    oscl_memset(syncts, 0, numsamplestoget + 1);
    oscl_memset(syncfrnum, 0, numsamplestoget + 1);

    if (syncts == NULL || syncfrnum == NULL)
    {
        if (syncts)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncts);
        }
        if (syncfrnum)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
        }
        return PVMFErrNoMemory;
    }

    // Retrieve the list of timestamp and frame numbers for sync samples.
    int32 retval = iPlaybackParserObj->getTimestampForRandomAccessPointsBeforeAfter(aTrackInfo.getTrackID(), targetTimestamp, syncts,
                   syncfrnum, numsamplestoget, aHowManySamples);
    if (retval != 1)
    {
        // Error
        if (syncts)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncts);
        }
        if (syncfrnum)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
        }
        return PVMFFailure;
    }

    // Copy the info
    aList.clear();
    uint32 ii;

    PVMFSampleNumTS syncnumts;
    MediaClockConverter mcc(iPlaybackParserObj->getTrackMediaTimescale(aTrackInfo.getTrackID()));
    for (ii = 0; ii < numsamplestoget; ++ii)  // numsamplestoget does incorporate the aMaxEntries limit
    {
        syncnumts.iSampleNumber = syncfrnum[ii];
        // Use the media clock converter to convert from timestamp in media timescale to millisec
        mcc.update_clock(syncts[ii]);
        syncnumts.iTimestamp = mcc.get_converted_ts(1000);
        // Add to provided list
        aList.push_back(syncnumts);
    }
    if (syncts)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(syncts);
    }
    if (syncfrnum)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
    }
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::GetTimestampForDataPosition(PVMFTrackInfo& aTrackInfo, TOsclFileOffset aDataPosition, PVMFTimestamp& aTimestamp)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetTimestampForDataPosition() called"));
    OSCL_UNUSED_ARG(aTrackInfo);
    OSCL_UNUSED_ARG(aDataPosition);
    OSCL_UNUSED_ARG(aTimestamp);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::GetTimestampForDataPosition() Conversion from data position to timestamp is not supported!"));
    return PVMFErrNotSupported;
}


PVMFStatus PVMFMP4FFParserNode::GetDataPositionForTimestamp(PVMFTrackInfo& aTrackInfo, PVMFTimestamp aTimestamp, TOsclFileOffset& aDataPosition)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetDataPositionForTimestamp() called"));

    if (iPlaybackParserObj == NULL)
    {
        return PVMFErrInvalidState;
    }

    // Convert the timestamp to media timescale value
    MediaClockConverter mcc(1000);
    mcc.set_clock(aTimestamp, 0);
    uint64 mediats = mcc.get_converted_ts64(iPlaybackParserObj->getTrackMediaTimescale(aTrackInfo.getTrackID()));

    // Retrieve the sample number corresponding to the specified timestamp
    TOsclFileOffset fileoffset = 0;
    int32 retVal = iPlaybackParserObj->getOffsetByTime(aTrackInfo.getTrackID(), mediats, &fileoffset, 0);

    if (retVal == EVERYTHING_FINE || retVal == END_OF_TRACK)
    {
        // Conversion worked
        aDataPosition = fileoffset;
    }
    else
    {
        // Error
        aDataPosition = 0;
        return PVMFFailure;
    }

    return PVMFSuccess;
}


/////////////////////
// Private Section //
/////////////////////

void PVMFMP4FFParserNode::Run()
{
    //Process commands.
    if (!iInputCommands.empty())
    {
        ProcessCommand();
    }

    // Process port activity
    while (!iPortActivityQueue.empty() && (iInterfaceState == EPVMFNodeStarted || IsFlushPending()))
    {
        ProcessPortActivity();
    }

    // Send out media data when started and not flushing
    if (iInterfaceState == EPVMFNodeStarted && !IsFlushPending())
    {
        HandleTrackState();
    }

    //Check for completion of a flush command...
    if (IsFlushPending() && iPortActivityQueue.empty())
    {
        //Flush is complete.
        CommandComplete(iCurrentCommand, PVMFSuccess);
    }
}

void PVMFMP4FFParserNode::CompleteCancelAfterInit()
{
    // cancel commands were pending, but not processed. return failed cancel
    if (IsCommandInProgress(iCancelCommand))
    {
        PVMFCmdResp resp(iCancelCommand.iId, iCancelCommand.iContext, PVMFFailure);
        PVMFSessionId session = iCancelCommand.iSession;

        //Report completion to the session observer.
        ReportCmdCompleteEvent(session, resp);

        //command completion reported, destroy the command.
        iCancelCommand.Destroy();
    }
}

void PVMFMP4FFParserNode::ReportMP4FFParserErrorEvent(PVMFEventType aEventType, OsclAny* aEventData, PVUuid* aEventUUID, int32* aEventCode)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::ReportMP4FFParserErrorEvent() In Type %d Data %d",
                    aEventType, aEventData));

    if (aEventUUID && aEventCode)
    {
        int32 leavecode = 0;
        PVMFBasicErrorInfoMessage* eventmsg = NULL;
        OSCL_TRY(leavecode, eventmsg = PVMF_BASE_NODE_NEW(PVMFBasicErrorInfoMessage, (*aEventCode, *aEventUUID, NULL)));
        PVMFAsyncEvent asyncevent(PVMFErrorEvent, aEventType, NULL, OSCL_STATIC_CAST(PVInterface*, eventmsg), aEventData, NULL, 0);
        PVMFNodeInterface::ReportErrorEvent(asyncevent);
        if (eventmsg)
        {
            eventmsg->removeRef();
        }
    }
    else
    {
        PVMFNodeInterface::ReportErrorEvent(aEventType, aEventData);
    }
}


void PVMFMP4FFParserNode::ReportMP4FFParserInfoEvent(PVMFEventType aEventType, OsclAny* aEventData, PVUuid* aEventUUID, int32* aEventCode)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::ReportMP4FFParserInfoEvent() In Type %d Data %d",
                    aEventType, aEventData));

    if (aEventUUID && aEventCode)
    {
        int32 leavecode = 0;
        PVMFBasicErrorInfoMessage* eventmsg = NULL;
        OSCL_TRY(leavecode, eventmsg = PVMF_BASE_NODE_NEW(PVMFBasicErrorInfoMessage, (*aEventCode, *aEventUUID, NULL)));
        PVMFAsyncEvent asyncevent(PVMFInfoEvent, aEventType, NULL, OSCL_STATIC_CAST(PVInterface*, eventmsg), aEventData, NULL, 0);
        PVMFNodeInterface::ReportInfoEvent(asyncevent);
        if (eventmsg)
        {
            eventmsg->removeRef();
        }
    }
    else
    {
        PVMFNodeInterface::ReportInfoEvent(aEventType, aEventData);
    }
}


PVMFStatus PVMFMP4FFParserNode::DoQueryInterface()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoQueryInterface() In"));

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
        return PVMFSuccess;
    }
    else
    {
        //not supported
        *ptr = NULL;
        return PVMFFailure;
    }
}


PVMFStatus PVMFMP4FFParserNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoRequestPort() In"));

    aPort = NULL;

    // Check to make sure the MP4 file has been parsed
    if (iPlaybackParserObj == NULL)
    {
        return PVMFFailure;
    }

    //retrieve port tag.
    int32 tag;
    PvmfMimeString* mimetype;
    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, mimetype);
    if (!mimetype)
    {
        return PVMFErrArgument;//no mimetype supplied.
    }

    // Allocate a port based on the request
    // Return the pointer to the port in the command complete message

    // Determine the format type from the MIME type string
    PVMFFormatType formattype = mimetype->get_cstr();
    if (formattype == PVMF_MIME_FORMAT_UNKNOWN)
    {
        // Unknown track type
        return PVMFErrArgument;
    }

    // Determine the maximum track data size and queue depth based on the format type
    uint32 trackmaxdatasize = 0;
    uint32 trackmaxqueuedepth = 0;
    GetTrackMaxParameters(formattype, trackmaxdatasize, trackmaxqueuedepth);
    OSCL_ASSERT(trackmaxdatasize > 0 && trackmaxqueuedepth > 0);

    // Track ID is the port tag
    // @TODO might need validation on the port tag==track ID.
    int32 trackid = tag;
    if (trackid < 0)
    {
        return PVMFErrArgument;
    }

    //timestamp for tracks need not always start with zero
    //initialize the ts value to track timestamp start offset
    TOsclFileOffset tsStartOffset = 0;
    if (iPlaybackParserObj->getTrackTSStartOffset(tsStartOffset, (uint32)trackid) != EVERYTHING_FINE)
    {
        return PVMFErrArgument;
    }

    //set the names for datapath logging
    OSCL_StackString<20> portname;
    OSCL_StackString<20> mempoolname;
    bool oTextTrack = false;
    if (formattype.isAudio())
    {
        portname = "PVMFMP4FFParOut(Audio)";
        mempoolname = "PVMFMP4FFPar(Audio)";
    }
    else if (formattype.isVideo())
    {
        portname = "PVMFMP4FFParOut(Video)";
        mempoolname = "PVMFMP4FFPar(Video)";
    }
    else if (formattype.isText())
    {
        oTextTrack = true;
        portname = "PVMFMP4FFParOut(Misc)";
        mempoolname = "PVMFMP4FFPar(Misc)";
    }

    int32 leavecode = 0;
    PVMFPortInterface* outport = NULL;
    MediaClockConverter* clockconv = NULL;
    OsclMemPoolResizableAllocator* trackdatamempool = NULL;
    PVMFResizableSimpleMediaMsgAlloc* mediadataimplalloc = NULL;
    PVMFTimedTextMediaDataAlloc* textmediadataimplalloc = NULL;
    PVMFMemPoolFixedChunkAllocator* mediadatamempool = NULL;
    OsclMemPoolFixedChunkAllocator* mediadatagroupimplmempool = NULL;
    PVMFMediaFragGroupCombinedAlloc<OsclMemAllocator>* mediadatagroupalloc = NULL;
    if (oTextTrack == false)
    {
        OSCL_TRY(leavecode,
                 outport = PVMF_BASE_NODE_NEW(PVMFMP4FFParserOutPort, (tag, this, portname.get_str()));
                 clockconv = PVMF_BASE_NODE_NEW(MediaClockConverter, (iPlaybackParserObj->getTrackMediaTimescale(trackid)));
                 trackdatamempool = PVMF_BASE_NODE_NEW(OsclMemPoolResizableAllocator, (trackmaxqueuedepth * trackmaxdatasize, PVMF_MP4FF_PARSER_NODE_MEM_POOL_GROWTH_LIMIT));
                 mediadataimplalloc = PVMF_BASE_NODE_NEW(PVMFResizableSimpleMediaMsgAlloc, (trackdatamempool));
                 mediadatamempool = PVMF_BASE_NODE_NEW(PVMFMemPoolFixedChunkAllocator, (mempoolname.get_str(), PVMP4FF_MEDIADATA_POOLNUM, PVMP4FF_MEDIADATA_CHUNKSIZE));
                 mediadatagroupimplmempool = PVMF_BASE_NODE_NEW(OsclMemPoolFixedChunkAllocator, (PVMP4FF_MEDIADATA_POOLNUM));
                 mediadatagroupalloc = PVMF_BASE_NODE_NEW(PVMFMediaFragGroupCombinedAlloc<OsclMemAllocator>, (PVMP4FF_MEDIADATA_POOLNUM, 20, mediadatagroupimplmempool));
                );
    }
    else
    {
        OSCL_TRY(leavecode,
                 outport = PVMF_BASE_NODE_NEW(PVMFMP4FFParserOutPort, (tag, this, portname.get_str()));
                 clockconv = PVMF_BASE_NODE_NEW(MediaClockConverter, (iPlaybackParserObj->getTrackMediaTimescale(trackid)));
                 trackdatamempool = PVMF_BASE_NODE_NEW(OsclMemPoolResizableAllocator, (trackmaxqueuedepth * trackmaxdatasize, PVMF_MP4FF_PARSER_NODE_MEM_POOL_GROWTH_LIMIT));
                 textmediadataimplalloc = PVMF_BASE_NODE_NEW(PVMFTimedTextMediaDataAlloc, (trackdatamempool));
                 mediadatamempool = PVMF_BASE_NODE_NEW(PVMFMemPoolFixedChunkAllocator, (mempoolname.get_str(), PVMP4FF_TEXT_TRACK_MEDIADATA_POOLNUM, PVMP4FF_MEDIADATA_CHUNKSIZE));
                 mediadatagroupimplmempool = PVMF_BASE_NODE_NEW(OsclMemPoolFixedChunkAllocator, (PVMP4FF_MEDIADATA_POOLNUM));
                 mediadatagroupalloc = PVMF_BASE_NODE_NEW(PVMFMediaFragGroupCombinedAlloc<OsclMemAllocator>, (PVMP4FF_TEXT_TRACK_MEDIADATA_POOLNUM, 20, mediadatagroupimplmempool));
                );
    }

    bool memerr = false;
    if (oTextTrack == false)
    {
        if (leavecode || !outport || !clockconv || !trackdatamempool || !mediadataimplalloc ||
                !mediadatamempool || !mediadatagroupimplmempool || !mediadatagroupalloc)
        {
            memerr = true;
        }
    }
    else
    {
        if (leavecode || !outport || !clockconv || !trackdatamempool ||
                !textmediadataimplalloc || !mediadatamempool || !mediadatagroupimplmempool || !mediadatagroupalloc)
        {
            memerr = true;
        }
    }
    if (memerr == true)
    {
        if (outport)
        {
            PVMF_BASE_NODE_DELETE(((PVMFMP4FFParserOutPort*)outport));
        }
        if (clockconv)
        {
            PVMF_BASE_NODE_DELETE(clockconv);
        }
        if (trackdatamempool)
        {
            trackdatamempool->removeRef();
            trackdatamempool = NULL;
        }
        if (mediadataimplalloc)
        {
            PVMF_BASE_NODE_DELETE(mediadataimplalloc);
        }
        if (textmediadataimplalloc)
        {
            PVMF_BASE_NODE_DELETE(textmediadataimplalloc);
        }
        if (mediadatamempool)
        {
            PVMF_BASE_NODE_DELETE(mediadatamempool);
        }
        if (mediadatagroupalloc)
        {
            mediadatagroupalloc->removeRef();
        }
        if (mediadatagroupimplmempool)
        {
            mediadatagroupimplmempool->removeRef();
        }
        return PVMFErrNoMemory;
    }

    mediadatagroupimplmempool->enablenullpointerreturn();
    trackdatamempool->enablenullpointerreturn();
    mediadatamempool->enablenullpointerreturn();

    mediadatagroupalloc->create();

    // Add the selected track/port to track list
    PVMP4FFNodeTrackPortInfo trackportinfo;
    trackportinfo.iTrackId = trackid;
    trackportinfo.iPortInterface = outport;
    trackportinfo.iFormatType = formattype;
    // assign the integer format type based on the format type recieved
    // these are formats being used during media data flow, so just assign
    // integer values to these types, others defined as unknown.
    if (formattype == PVMF_MIME_MPEG4_AUDIO)
    {
        trackportinfo.iFormatTypeInteger = PVMF_MP4_PARSER_NODE_MPEG4_AUDIO;
    }
    else if (formattype == PVMF_MIME_MP3)
    {
        trackportinfo.iFormatTypeInteger = PVMF_MP4_PARSER_NODE_MP3_AUDIO;
    }
    else if (formattype == PVMF_MIME_H264_VIDEO_MP4)
    {
        trackportinfo.iFormatTypeInteger = PVMF_MP4_PARSER_NODE_H264_MP4;
    }
    else if (formattype == PVMF_MIME_3GPP_TIMEDTEXT)
    {
        trackportinfo.iFormatTypeInteger = PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT;
    }
    else if (formattype == PVMF_MIME_AMR_IETF)
    {
        trackportinfo.iFormatTypeInteger = PVMF_MP4_PARSER_NODE_AMR_IETF;
    }
    else if (formattype == PVMF_MIME_AMRWB_IETF)
    {
        trackportinfo.iFormatTypeInteger = PVMF_MP4_PARSER_NODE_AMRWB_IETF;
    }
    else
    {
        trackportinfo.iFormatTypeInteger = PVMF_MP4_PARSER_NODE_FORMAT_UNKNOWN;
    }
    RetrieveTrackConfigInfo(trackid,
                            formattype,
                            trackportinfo.iFormatSpecificConfig);
    if (formattype == PVMF_MIME_MPEG4_AUDIO)
    {
        RetrieveTrackConfigInfoAndFirstSample(trackid,
                                              formattype,
                                              trackportinfo.iFormatSpecificConfigAndFirstSample);

    }
    trackportinfo.iMimeType = (*mimetype);
    trackportinfo.iClockConverter = clockconv;
    trackportinfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_UNINITIALIZED;
    trackportinfo.iTrackMaxDataSize = trackmaxdatasize;
    trackportinfo.iTrackMaxQueueDepth = trackmaxqueuedepth;
    trackportinfo.iTrackDataMemoryPool = trackdatamempool;
    trackportinfo.iMediaDataImplAlloc = mediadataimplalloc;
    trackportinfo.iTextMediaDataImplAlloc = textmediadataimplalloc;
    trackportinfo.iMediaDataMemPool = mediadatamempool;
    trackportinfo.iMediaDataGroupImplMemPool = mediadatagroupimplmempool;
    trackportinfo.iMediaDataGroupAlloc = mediadatagroupalloc;
    trackportinfo.iNode = (PVMFNodeInterfaceImpl*)this;
    trackportinfo.iTimestamp = tsStartOffset;
    trackportinfo.iSeqNum = 0;

    // TEMP: Number of samples to retrieve should be negotiated between
    // the nodes but for now hardcode the values
    // By default retrieve one bundle of samples from the file format parser
    if (formattype == PVMF_MIME_M4V)
    {
        trackportinfo.iNumSamples = M4V_NUMSAMPLES;
    }
    else if (formattype == PVMF_MIME_H2631998 ||
             formattype == PVMF_MIME_H2632000)
    {
        trackportinfo.iNumSamples = H263_NUMSAMPLES;
    }
    else if (formattype == PVMF_MIME_H264_VIDEO_MP4)
    {
        trackportinfo.iNumSamples = H264_MP4_NUMSAMPLES;
    }
    else if (formattype == PVMF_MIME_MPEG4_AUDIO)
    {
        trackportinfo.iNumSamples = MPEG4_AUDIO_NUMSAMPLES;
    }
    else if (formattype == PVMF_MIME_AMR_IETF)
    {
        if (trackportinfo.iNumAMRSamplesToRetrieve > 0)
        {
            trackportinfo.iNumSamples = trackportinfo.iNumAMRSamplesToRetrieve;
        }
        else
        {
            // Need to determine the number of AMR samples to get based on
            // number of frames to get and number of frames per sample
            int32 framespersample = iPlaybackParserObj->getNumAMRFramesPerSample(trackid);
            if (framespersample > 0)
            {
                trackportinfo.iNumAMRSamplesToRetrieve = AMR_IETF_NUMFRAMES / framespersample;
                if (trackportinfo.iNumAMRSamplesToRetrieve == 0 || (AMR_IETF_NUMFRAMES % framespersample > 0))
                {
                    // Increment if 0 or if there is a remainder
                    ++trackportinfo.iNumAMRSamplesToRetrieve;
                }
            }
            else
            {
                // Assume 1 AMR frame per sample
                trackportinfo.iNumAMRSamplesToRetrieve = AMR_IETF_NUMFRAMES;
            }
        }
        trackportinfo.iNumSamples = trackportinfo.iNumAMRSamplesToRetrieve;
    }
    else if (formattype == PVMF_MIME_AMRWB_IETF)
    {
        if (trackportinfo.iNumAMRSamplesToRetrieve > 0)
        {
            trackportinfo.iNumSamples = trackportinfo.iNumAMRSamplesToRetrieve;
        }
        else
        {
            // Need to determine the number of AMR samples to get based on
            // number of frames to get and number of frames per sample
            int32 framespersample = iPlaybackParserObj->getNumAMRFramesPerSample(trackid);
            if (framespersample > 0)
            {
                trackportinfo.iNumAMRSamplesToRetrieve = AMRWB_IETF_NUMFRAMES / framespersample;
                if (trackportinfo.iNumAMRSamplesToRetrieve == 0 || (AMRWB_IETF_NUMFRAMES % framespersample > 0))
                {
                    // Increment if 0 or if there is a remainder
                    ++trackportinfo.iNumAMRSamplesToRetrieve;
                }
            }
            else
            {
                // Assume 1 AMRWB frame per sample
                trackportinfo.iNumAMRSamplesToRetrieve = AMRWB_IETF_NUMFRAMES;
            }
        }
        trackportinfo.iNumSamples = trackportinfo.iNumAMRSamplesToRetrieve;
    }
    else if (formattype == PVMF_MIME_3GPP_TIMEDTEXT)
    {
        trackportinfo.iNumSamples = TIMEDTEXT_NUMSAMPLES;
    }
    else
    {
        trackportinfo.iNumSamples = UNKNOWN_NUMSAMPLES;
    }

    if (iPortDataLog)
    {
        OSCL_StackString<512> portLoggerTag(_STRLIT_CHAR("PVMFMP4ParserNode"));
        portLoggerTag += iLogFileIndex;
        iLogFileIndex += 1;
        if (formattype.isAudio())
        {
            portLoggerTag += _STRLIT_CHAR("audio");
        }
        else if (formattype.isVideo())
        {
            portLoggerTag += _STRLIT_CHAR("video");
        }
        else
        {
            portLoggerTag += _STRLIT_CHAR("misc");
        }
        trackportinfo.iPortLogger = PVLogger::GetLoggerObject(portLoggerTag.get_cstr());
        OSCL_StackString<512> portLogFile;
        portLogFile = portLogPath;
        portLogFile += portLoggerTag.get_cstr();
        trackportinfo.iLogFile = portLogFile;

        PVLoggerAppender *binAppender =
            BinaryFileAppender::CreateAppender((char*)(trackportinfo.iLogFile.get_cstr()));

        if (binAppender == NULL)
        {
            PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::DoRequestPort: Error - Binary Appender Create failed", this));
            return PVMFErrNoResources;
        }
        OsclRefCounterSA<PVMFMP4ParserNodeLoggerDestructDealloc>* binAppenderRefCounter =
            new OsclRefCounterSA<PVMFMP4ParserNodeLoggerDestructDealloc>(binAppender);

        OsclSharedPtr<PVLoggerAppender> appenderSharedPtr(binAppender, binAppenderRefCounter);
        trackportinfo.iBinAppenderPtr = appenderSharedPtr;
        trackportinfo.iPortLogger->AddAppender(trackportinfo.iBinAppenderPtr);
    }

    iNodeTrackPortList.push_back(trackportinfo);

    // only worry about gapless metadata for clips contain a single AAC track
    if ((iNodeTrackPortList.size() == 1) && (formattype == PVMF_MIME_MPEG4_AUDIO))
    {
        GetGaplessMetadata(iPlaybackClipIndex);
    }

    aPort = outport;

    return PVMFSuccess;
}

void PVMFMP4FFParserNode::GetTrackMaxParameters(PVMFFormatType aFormatType, uint32& aMaxDataSize, uint32& aMaxQueueDepth)
{
    if (aFormatType == PVMF_MIME_M4V)
    {
        aMaxDataSize = M4V_MAXTRACKDATASIZE;
        aMaxQueueDepth = M4V_MAXTRACKQUEUEDEPTH;
    }
    else if (aFormatType == PVMF_MIME_H2631998 ||
             aFormatType == PVMF_MIME_H2632000)
    {
        aMaxDataSize = H263_MAXTRACKDATASIZE;
        aMaxQueueDepth = H263_MAXTRACKQUEUEDEPTH;
    }
    else if (aFormatType == PVMF_MIME_H264_VIDEO_MP4)
    {
        aMaxDataSize = H264_MP4_MAXTRACKDATASIZE;
        aMaxQueueDepth = H264_MP4_MAXTRACKQUEUEDEPTH;
    }
    else if (aFormatType == PVMF_MIME_MPEG4_AUDIO)
    {
        aMaxDataSize = MPEG4_AUDIO_MAXTRACKDATASIZE;
        aMaxQueueDepth = MPEG4_AUDIO_MAXTRACKQUEUEDEPTH;
    }
    else if (aFormatType == PVMF_MIME_AMR_IETF)
    {
        aMaxDataSize = AMR_IETF_MAXTRACKDATASIZE;
        aMaxQueueDepth = AMR_IETF_MAXTRACKQUEUEDEPTH;
    }
    else if (aFormatType == PVMF_MIME_AMRWB_IETF)
    {
        aMaxDataSize = AMRWB_IETF_MAXTRACKDATASIZE;
        aMaxQueueDepth = AMRWB_IETF_MAXTRACKQUEUEDEPTH;
    }
    else if (aFormatType == PVMF_MIME_3GPP_TIMEDTEXT)
    {
        aMaxDataSize = TIMEDTEXT_MAXTRACKDATASIZE;
        aMaxQueueDepth = TIMEDTEXT_MAXTRACKQUEUEDEPTH;
    }
    else
    {
        aMaxDataSize = UNKNOWN_MAXTRACKDATASIZE;
        aMaxQueueDepth = UNKNOWN_MAXTRACKQUEUEDEPTH;
    }
}


PVMFStatus PVMFMP4FFParserNode::DoReleasePort()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoReleasePort() In"));

    LogDiagnostics();
    //Find the port in the port vector
    PVMFPortInterface* port;
    iCurrentCommand.PVMFNodeCommandBase::Parse(port);

    // Remove the selected track from the track list
    int32 i = 0;
    int32 maxtrack = iNodeTrackPortList.size();
    while (i < maxtrack)
    {
        if (iNodeTrackPortList[i].iPortInterface == port)
        {
            // Found the element. So erase it
            iNodeTrackPortList[i].iMediaData.Unbind();
            if (iNodeTrackPortList[i].iPortInterface)
            {
                PVMF_BASE_NODE_DELETE(((PVMFMP4FFParserOutPort*)iNodeTrackPortList[i].iPortInterface));
            }
            if (iNodeTrackPortList[i].iClockConverter)
            {
                PVMF_BASE_NODE_DELETE(iNodeTrackPortList[i].iClockConverter);
            }
            if (iNodeTrackPortList[i].iTrackDataMemoryPool)
            {
                iNodeTrackPortList[i].iTrackDataMemoryPool->CancelFreeChunkAvailableCallback();
                iNodeTrackPortList[i].iTrackDataMemoryPool->removeRef();
                iNodeTrackPortList[i].iTrackDataMemoryPool = NULL;
            }
            if (iNodeTrackPortList[i].iMediaDataImplAlloc)
            {
                PVMF_BASE_NODE_DELETE(iNodeTrackPortList[i].iMediaDataImplAlloc);
            }
            if (iNodeTrackPortList[i].iTextMediaDataImplAlloc)
            {
                PVMF_BASE_NODE_DELETE(iNodeTrackPortList[i].iTextMediaDataImplAlloc);
            }
            if (iNodeTrackPortList[i].iMediaDataMemPool)
            {
                iNodeTrackPortList[i].iMediaDataMemPool->CancelFreeChunkAvailableCallback();
                iNodeTrackPortList[i].iMediaDataMemPool->removeRef();
            }
            if (iNodeTrackPortList[i].iMediaDataGroupAlloc)
            {
                iNodeTrackPortList[i].iMediaDataGroupAlloc->removeRef();
            }
            if (iNodeTrackPortList[i].iMediaDataGroupImplMemPool)
            {
                iNodeTrackPortList[i].iMediaDataGroupImplMemPool->removeRef();
            }
            if (iPortDataLog)
            {
                if (iNodeTrackPortList[i].iBinAppenderPtr.GetRep() != NULL)
                {
                    iNodeTrackPortList[i].iPortLogger->RemoveAppender(iNodeTrackPortList[i].iBinAppenderPtr);
                    iNodeTrackPortList[i].iBinAppenderPtr.Unbind();
                }
            }
            iNodeTrackPortList.erase(iNodeTrackPortList.begin() + i);
            return PVMFSuccess;
        }
        ++i;
    }

    if (i >= maxtrack)
    {
        return PVMFErrBadHandle;
    }

    // Unknown port
    return PVMFFailure;
}

bool PVMFMP4FFParserNode::PopulatePIFFDRMInfo()
{
    if (iPlaybackParserObj == NULL)
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::PopulatePIFFDRMInfo - Invalid iPlaybackParserObj"));
        return false;
    }

    if ((iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE) && (iPlaybackParserObj->GetPIFFProtectionSystem() == PIFF_PROTECTION_SYSTEM_PLAYREADY))
    {
        //Cleanup any old key.
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

        int32 UseKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING)) + oscl_strlen(_STRLIT_CHAR(";track_id=0"));
        int32 AuthKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_AUTHORIZATION_DATA_KEY_STRING)) + oscl_strlen(_STRLIT_CHAR(";track_id=0"));
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
            return false;
        }

        OSCL_StackString<256> requestUsageKey;
        requestUsageKey += _STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING);
        requestUsageKey += _STRLIT_CHAR(";track_id=0");

        oscl_strncpy(iRequestedUsage.key,
                     requestUsageKey.get_cstr(),
                     UseKeyLen);
        iRequestedUsage.key[UseKeyLen] = 0;
        iRequestedUsage.length = 0;
        iRequestedUsage.capacity = 0;
        iRequestedUsage.value.uint32_value =
            (BITMASK_PVMF_CPM_DRM_INTENT_PLAY |
             BITMASK_PVMF_CPM_DRM_INTENT_PAUSE |
             BITMASK_PVMF_CPM_DRM_INTENT_SEEK_FORWARD |
             BITMASK_PVMF_CPM_DRM_INTENT_SEEK_BACK);

        oscl_strncpy(iApprovedUsage.key,
                     requestUsageKey.get_cstr(),
                     UseKeyLen);

        iApprovedUsage.key[UseKeyLen] = 0;
        iApprovedUsage.length = 0;
        iApprovedUsage.capacity = 0;
        iApprovedUsage.value.uint32_value = 0;

        OSCL_StackString<512> authorizationKey;
        authorizationKey += _STRLIT_CHAR(PVMF_PIFF_AUTH_DATA_PLAYREADY_OBJECT);
        oscl_strncpy(iAuthorizationDataKvp.key,
                     authorizationKey.get_cstr(),
                     AuthKeyLen);
        iAuthorizationDataKvp.key[AuthKeyLen] = 0;

        uint32 playReadyObjectSize = 0;
        const uint8* playReadyObject = NULL;
        iPlaybackParserObj->GetPIFFProtectionSystemSpecificData(playReadyObject, playReadyObjectSize);
        iAuthorizationDataKvp.length             = playReadyObjectSize;
        iAuthorizationDataKvp.capacity           = playReadyObjectSize;
        iAuthorizationDataKvp.value.pUint8_value = (uint8*)playReadyObject;
        return true;
    }
    else
    {
        iProtectedFile = false;
        return false;
    }
}

PVMFStatus PVMFMP4FFParserNode::InitOMA2DRMInfo(uint32 aParserIndex)
{
    IMpeg4File* parserObj = GetParserObjAtIndex(aParserIndex);
    if (parserObj == NULL)
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::InitOMA2DRMInfo - Invalid iPlaybackParserObj"));
        return PVMFErrNoResources;
    }

    if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
    {
        if (iOMA2DecryptionBuffer == NULL)
        {
            iOMA2DecryptionBuffer = PVMF_BASE_NODE_ARRAY_NEW(uint8, PVMP4FF_OMA2_DECRYPTION_BUFFER_SIZE);
        }

        int32 iNumTracks = parserObj->getNumTracks();
        uint32 iIdList[16];
        if (iNumTracks != parserObj->getTrackIDList(iIdList, iNumTracks))
        {
            PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::InitOMA2DRMInfo - Couldnt Get Track List"));
            return PVMFFailure;
        }

        for (int32 i = 0; i < iNumTracks; i++)
        {
            uint32 trackID = iIdList[i];
            PVMP4FFNodeTrackOMA2DRMInfo oma2TrackInfo;
            oma2TrackInfo.iTrackId = trackID;
            uint32 odkmBoxSize = parserObj->getTrackLevelOMA2DRMInfoSize(trackID);
            uint8* odkmBox = parserObj->getTrackLevelOMA2DRMInfo(trackID);
            if (odkmBoxSize > 0)
            {
                MediaMetaInfo info;
                uint32 numSamples = 1;
                int32 retval = EVERYTHING_FINE;
                retval = parserObj->peekNextBundledAccessUnits(trackID,
                         &numSamples,
                         &info);
                if ((retval == EVERYTHING_FINE || END_OF_TRACK == retval) && numSamples > 0)
                {
                    uint32 sampleSize = info.len;
                    if (sampleSize > 0)
                    {
                        uint8* sampleBuf = PVMF_BASE_NODE_ARRAY_NEW(uint8, (sizeof(sampleSize) +
                                           sampleSize +
                                           sizeof(odkmBoxSize) +
                                           odkmBoxSize));
                        uint8* destBuf = sampleBuf;
                        oscl_memcpy((OsclAny*)destBuf, (const OsclAny*)(&odkmBoxSize), sizeof(odkmBoxSize));
                        destBuf += sizeof(odkmBoxSize);
                        oscl_memcpy((OsclAny*)destBuf, (const OsclAny*)(odkmBox), odkmBoxSize);
                        destBuf += odkmBoxSize;
                        oscl_memcpy((OsclAny*)destBuf, (const OsclAny*)(&sampleSize), sizeof(sampleSize));
                        destBuf += sizeof(sampleSize);

                        oscl_memset(&iGau.buf, 0, sizeof(iGau.buf));
                        oscl_memset(&iGau.info, 0, sizeof(iGau.info));
                        iGau.free_buffer_states_when_done = 0;
                        iGau.numMediaSamples = 1;
                        iGau.buf.num_fragments = 1;
                        iGau.buf.buf_states[0] = NULL;
                        iGau.buf.fragments[0].ptr = (OsclAny*)destBuf;
                        iGau.buf.fragments[0].len = sampleSize;
                        iGau.frameNum = 0;
                        retval =
                            parserObj->getNextBundledAccessUnits(trackID,
                                                                 &numSamples,
                                                                 &iGau);
                        parserObj->resetPlayback();

                        oma2TrackInfo.iDRMInfoSize = (sizeof(sampleSize) + sampleSize +
                                                      sizeof(odkmBoxSize) + odkmBoxSize);
                        oma2TrackInfo.iDRMInfo = sampleBuf;
                    }
                }
            }
            iOMA2DRMInfoVec.push_back(oma2TrackInfo);
        }
    }
    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::DoInit()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoInit() In"));

    if (EPVMFNodeInitialized == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoInit() already in Initialized state"));
        return PVMFSuccess;
    }

    if (iCPM)
    {
        /*
         * Go thru CPM commands before parsing the file
         * - Init CPM
         * - Open Session
         * - Register Content
         * - Get Content Type
         * - Approve Usage
         */
        if (oWaitingOnLicense == false)
        {
            InitCPM();
        }
        else
        {
            if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
                    (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS) ||
                    (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE))
            {
                PVMFStatus status = RequestUsage(NULL);
                if (status != PVMFPending)
                {
                    status = CompleteInit(iCurrentCommand, status);
                }
                return status;
            }
            else if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
            {
                PVMP4FFNodeTrackOMA2DRMInfo* oma2trackInfo = NULL;
                if (CheckForOMA2AuthorizationComplete(oma2trackInfo) == PVMFPending)
                {
                    PVMFStatus status = RequestUsage(oma2trackInfo);
                    if (status != PVMFPending)
                    {
                        status = CompleteInit(iCurrentCommand, status);
                    }
                    return status;
                }
            }
        }
        return PVMFPending;
    }
    else
    {
        // for non-local files, need to check if enough data is available
        PVMFStatus status = CheckForMP4HeaderAvailability();
        if (status == PVMFSuccess)
        {
            status = CompleteInit(iCurrentCommand);
        }
        return status;
    }
}


PVMFStatus PVMFMP4FFParserNode::CompleteInit(PVMFNodeCommand& aCmd, PVMFStatus aStatus)
{
    PVMFStatus Status = PVMFSuccess;
    if (aStatus != PVMFSuccess)
    {
        CommandComplete(aCmd, aStatus);
        return aStatus;
    }
    if (iCPM)
    {
        if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
                (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
        {
            if (iApprovedUsage.value.uint32_value !=
                    iRequestedUsage.value.uint32_value)
            {
                if (GetCPMSourceDataIntentAt(0) == BITMASK_PVMF_SOURCE_INTENT_GETMETADATA)
                {
                    Status = PVMFSuccess;
                }
                else
                {
                    Status = PVMFErrAccessDenied;
                }
                CommandComplete(aCmd, Status);
                return PVMFCmdCompleted;
            }
            else
            {
                PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
                if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
                {
                    dsFactory = iDataStreamFactory;
                }
                InitNextValidClipInPlaylist(&Status, 0, dsFactory);
                CommandComplete(aCmd, Status);
                Status = PVMFCmdCompleted;
            }
        }
        else if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
        {
            if (GetCPMSourceDataIntentAt(0) == BITMASK_PVMF_SOURCE_INTENT_GETMETADATA)
            {
                Status = PVMFSuccess;
            }
            else
            {
                if (CheckForOMA2UsageApproval() == true)
                {
                    PVUuid uuid = PVMFCPMPluginDecryptionInterfaceUuid;
                    PVInterface* intf =
                        iCPMContentAccessFactory->CreatePVMFCPMPluginAccessInterface(uuid);
                    PVMFCPMPluginAccessInterface* interimPtr =
                        OSCL_STATIC_CAST(PVMFCPMPluginAccessInterface*, intf);
                    iDecryptionInterface = OSCL_STATIC_CAST(PVMFCPMPluginAccessUnitDecryptionInterface*, interimPtr);
                    if (iDecryptionInterface != NULL)
                    {
                        iDecryptionInterface->Init();
                        iCPMDecryptionDoneInplace = iDecryptionInterface->CanDecryptInPlace();
                        Status = PVMFSuccess;
                    }
                }
                Status = PVMFErrAccessDenied;
            }
            CommandComplete(aCmd, Status);
            return PVMFCmdCompleted;
        }
        else if (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE)
        {
            if (iProtectedFile == false)
            {
                CommandComplete(aCmd, Status);
                return PVMFCmdCompleted;
            }
            if (GetCPMSourceDataIntentAt(0) == BITMASK_PVMF_SOURCE_INTENT_GETMETADATA)
            {
                Status = PVMFSuccess;
            }
            else
            {
                PVUuid uuid = PVMFCPMPluginDecryptionInterfaceUuid;
                PVInterface* intf =
                    iCPMContentAccessFactory->CreatePVMFCPMPluginAccessInterface(uuid);
                PVMFCPMPluginAccessInterface* interimPtr =
                    OSCL_STATIC_CAST(PVMFCPMPluginAccessInterface*, intf);
                iDecryptionInterface = OSCL_STATIC_CAST(PVMFCPMPluginAccessUnitDecryptionInterface*, interimPtr);
                if (iDecryptionInterface != NULL)
                {
                    iDecryptionInterface->Init();
                    iCPMDecryptionDoneInplace = iDecryptionInterface->CanDecryptInPlace();
                    Status = PVMFSuccess;
                }
                else
                {
                    Status = PVMFErrAccessDenied;
                }
            }
            CommandComplete(aCmd, Status);
            return PVMFCmdCompleted;
        }
        else
        {
            /* CPM doesnt care about MP4 / 3GP files */
            PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
            if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
            {
                dsFactory = iDataStreamFactory;
            }
            InitNextValidClipInPlaylist(&Status, 0, dsFactory);
            CommandComplete(aCmd, Status);
            Status = PVMFCmdCompleted;
        }
    }
    else
    {
        PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
        if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
        {
            dsFactory = iDataStreamFactory;
        }
        InitNextValidClipInPlaylist(&Status, 0, dsFactory);
        CommandComplete(aCmd, Status);
        Status = PVMFCmdCompleted;
    }
    return Status;
}

PVMFStatus PVMFMP4FFParserNode::DoPrepare()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoPrepareNode() In"));

    if (EPVMFNodePrepared == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoPrepare() already in Prepared state"));
        return PVMFSuccess;
    }

    /* Do initial buffering in case of PDL / FT  or in case of External Download */
    if ((iExternalDownload == true) && (iDownloadComplete == false) && (iPlaybackParserObj != NULL))
    {
        TOsclFileOffset offset = 0;
        PVMFStatus status = GetFileOffsetForAutoResume(offset, false);
        if (status == PVMFSuccess)
        {
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DoPrepare - Calling RequestReadCapacityNotification With Offset=%d",  offset));
            //Request for callback
            MP4_ERROR_CODE retVal =
                iPlaybackParserObj->RequestReadCapacityNotification(*this, offset);
            if (retVal == EVERYTHING_FINE)
            {
                // parser node will not report underflow in this case but will set the
                // variables so that data ready event can be send to engine once datastream
                // downloads requested data.
                autopaused = true;
                iUnderFlowEventReported = true;
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DoPrepare - Holding off on Prepare Complete because of Insufficient Downloaded Data"));
                return PVMFPending;
            }
            else if (retVal == SUFFICIENT_DATA_IN_FILE)
            {
                // report prepare success and send data ready event to engine.
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DoPrepare - Sufficient data in file, send success and data ready event"));
                ReportMP4FFParserInfoEvent(PVMFInfoDataReady);
                return PVMFSuccess;
            }
            PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::DoPrepare - RequestReadCapacityNotification Failed - Ret=%d",  retVal));
        }
        else
        {
            PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::DoPrepare - GetFileOffsetForAutoResume Failed - Status=%d",  status));
        }
        return PVMFErrArgument;
    }
    else
    {
        if ((download_progress_interface != NULL) && (iDownloadComplete == false) && (iFastTrackSession == false))
        {
            if (0 == iLastNPTCalcInConvertSizeToTime)
            {
                uint32 ts = 0;

                if ((NULL != iDataStreamInterface) && (0 != iDataStreamInterface->QueryBufferingCapacity()))
                {
                    TOsclFileOffset bytesReady = 0;
                    TOsclFileOffset tempBytes = 0;
                    PvmiDataStreamStatus status = iDataStreamInterface->QueryReadCapacity(iDataStreamSessionID, tempBytes);
                    bytesReady = tempBytes;
                    if (status == PVDS_END_OF_STREAM)
                    {
                        return PVMFSuccess;
                    }
                    // if progressive streaming, playResumeNotifcation is guaranteed to be called
                    // with the proper download complete state, ignore the current download status
                    bool dlcomplete = false;
                    download_progress_interface->requestResumeNotification(ts, dlcomplete);
                }
                else
                {
                    download_progress_interface->requestResumeNotification(ts, iDownloadComplete);
                }
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DoPrepare - Holding off on Prepare Complete because of Insufficient Downloaded Data"));
                autopaused = true;
                iUnderFlowEventReported = true;
                return PVMFPending;
            }
        }
    }
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::DoStop()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoStopNode() In"));

    LogDiagnostics();
    iStreamID = 0;

    // stop and reset position to beginning
    ResetAllTracks();

    // reset direction rate state variables
    iPlayBackDirection = PVMF_DATA_SOURCE_DIRECTION_FORWARD;
    iParseAudioDuringFF = false;
    iParseAudioDuringREW = false;
    iParseVideoOnly = false;
    iDataRate = NORMAL_PLAYRATE;
    iInitNextClip = false;
    iFirstValidClipIndex = -1;

    // Reset the MP4 FF to beginning
    if (iPlaybackParserObj)
    {
        for (uint32 i = 0; i < iNodeTrackPortList.size(); i++)
        {
            iNodeTrackPortList[i].iTimestamp = 0;

            /* @TODO: This should not be required. However, FMU's GetFrame()
             *        stops the playerengine and reinitializes it. So, we would need
             *        this temporary 'workaround' to re-retrieve a thumbnail.
             */
            iNodeTrackPortList[i].iThumbSampleDone = 0;
        }
        iPlaybackParserObj->resetPlayback();
    }

    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::DoPause()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoPauseNode() In"));

    if (EPVMFNodePaused == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoPause() already in Paused state"));
        return PVMFSuccess;
    }

    if (!iUnderFlowEventReported && iExternalDownload)
    {
        /*
         * Since sourcenode is in paused state, so ..
         * Reset all PS related variable,
         * Cancel the underflow timer so that no underflow event could be sent in paused state,
         * Set all tracks state to GETDATA.
         */
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger,  PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::DoPause() - SN is paused, cancel UF timer, reset all PS variables"));
        for (uint32 i = 0; i < iNodeTrackPortList.size(); ++i)
        {
            iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
        }
        autopaused = false;
        iUnderFlowCheckTimer->Cancel(PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID);

        // Cancel any DS callback since Sourcenode is Paused.
        MP4_ERROR_CODE retVal = iPlaybackParserObj->CancelNotificationSync();

        if (retVal != EVERYTHING_FINE)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::DoPause() - Error in Cancelling, Logging this."));
            OSCL_ASSERT(retVal);
        }

    }
    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::DoReset()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoResetNode() In"));

    LogDiagnostics();

    //remove the clock observer
    if (iClientPlayBackClock != NULL)
    {
        if (iClockNotificationsInf != NULL)
        {
            iClockNotificationsInf->RemoveClockStateObserver(*this);
            iClientPlayBackClock->DestroyMediaClockNotificationsInterface(iClockNotificationsInf);
            iClockNotificationsInf = NULL;
        }
    }

    if (iUnderFlowCheckTimer != NULL)
    {
        iUnderFlowCheckTimer->Clear();
    }

    // reset direction rate state variables
    iPlayBackDirection = PVMF_DATA_SOURCE_DIRECTION_FORWARD;
    iParseAudioDuringFF = false;
    iParseAudioDuringREW = false;
    iParseVideoOnly = false;
    iDataRate = NORMAL_PLAYRATE;
    iFirstValidClipIndex = -1;

    PVMFStatus status = PVMFSuccess;

    if (download_progress_interface != NULL)
    {
        download_progress_interface->cancelResumeNotification();
    }

    if (iPlaybackParserObj != NULL)
    {
        /* Indicates that the init was successfull */
        if (iCPM)
        {
            if (iProtectedFile == false)
            {
                PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::DoReset - Unprotected Content - Skipping Usage Complete - Doing CPM Reset"));
                ResetCPM();
            }
            else
            {
                if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
                {
                    ResetOMA2Flags();
                }
                SendUsageComplete();
            }
            return PVMFPending;
        }
        else
        {
            ReleaseAllPorts();
            CleanupFileSource();
            iSelectedTrackInfoList.clear();
        }
    }

    status = CompleteReset();
    return status;
}

PVMFStatus PVMFMP4FFParserNode::CompleteReset()
{
    // stop and cleanup
    // release the download_progress_clock if any
    download_progress_clock.Unbind();
    // release the download progress interface if any
    if (download_progress_interface)
    {
        download_progress_interface->removeRef();
        download_progress_interface = NULL;
    }
    autopaused = false;
    iDownloadFileSize = 0;
    iTotalMoofFrags = 0;
    ReleaseAllPorts();
    CleanupFileSource();
    iSelectedTrackInfoList.clear();

    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::CancelCurrentCommand()
{
    if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_INIT)
    {
        if (iDataStreamInterface != NULL)
        {
            if (iCPMSequenceInProgress)
            {
                return PVMFPending;
            }
            if (iProgressivelyPlayable == true && iDataStreamRequestPending)
            {
                // send a cancel notification to datastream module.
                iDataStreamRequestPending = false;

                PvmiDataStreamStatus retVal = iDataStreamInterface->CancelNotificationSync(iDataStreamSessionID);

                if (retVal == PVDS_SUCCESS)
                {
                    // Complete Init as cancelled.
                    CommandComplete(iCurrentCommand, PVMFErrCancelled);
                }
                else
                {
                    // Not a valid DataStream Session, Complete Init as failure.
                    CommandComplete(iCurrentCommand, PVMFFailure);
                }
            }
            else if (download_progress_interface != NULL && iProgressivelyPlayable == false)
            {
                // call cancel resume notification and complete Init as cancelled.
                download_progress_interface->cancelResumeNotification();
                CommandComplete(iCurrentCommand, PVMFErrCancelled);
            }
            else
            {
                // wait on cpm commands completion.
                return PVMFPending;
            }
        }
        else
        {
            // wait on cpm commands completion.
            return PVMFPending;
        }
    }
    else if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_PREPARE)
    {
        if (autopaused)
        {
            autopaused = false;
            // Prepare in case of PDL would complete imediately, only case need to be handled here
            // is for Pseudo Streaming. There will no command in current queue in case of PDL so no
            // cancel.
            if ((iExternalDownload == true) && (iPlaybackParserObj != NULL))
            {
                //Cancel the callback
                MP4_ERROR_CODE retVal = iPlaybackParserObj->CancelNotificationSync();

                if (retVal == EVERYTHING_FINE)
                {
                    // Complete Prepare as cancelled.
                    CommandComplete(iCurrentCommand, PVMFErrCancelled);
                }
                else
                {
                    // Not a valid DataStream Session, Complete Prepare as failure.
                    CommandComplete(iCurrentCommand, PVMFFailure);
                }
            }
        }
    }
    else
    {
        return PVMFPending;
    }
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::DoSetDataSourcePositionPlaylist()
{
    PVMFDataSourcePositionParams* aReposParams;
    uint32 targetNPT = 0;
    uint32 *actualNPT = NULL;
    uint32 *actualMediaDataTS = NULL;
    bool seekToSyncPoint = false;
    uint32 streamID = 0;
    int32 reposIndex = -1;
    iMetadataParserObj = NULL;

    iPlaylistExhausted = false;

    iCurrentCommand.PVMFNodeCommand::Parse(aReposParams);


    PVMFStatus status = PVMFSuccess;

    // check for availability of clip index in the clip list.
    for (uint32 index = 0; index < iClipInfoList.size(); index++)
    {
        if (aReposParams->iPlayElementIndex == (int32) iClipInfoList[index].iClipInfo.GetClipIndex())
        {
            reposIndex = (int32) index;
            break;
        }
    }

    // reposition index wasnt found in the list.
    if (reposIndex < 0)
    {
        return PVMFErrArgument;
    }

    streamID = aReposParams->iStreamID;
    seekToSyncPoint = aReposParams->iSeekToSyncPoint;
    targetNPT = aReposParams->iTargetNPT; // target npt in msec
    actualMediaDataTS = &aReposParams->iActualMediaDataTS;
    actualNPT = &aReposParams->iActualNPT;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::DoSetDataSourcePositionPlaylist() ClipIndex[%d]", aReposParams->iPlayElementIndex));

    // reposition is causing track skipping
    // release currently used mp4 parser object
    if (iPlaybackClipIndex != reposIndex &&
            NULL != iPlaybackParserObj)
    {
        // release preexisting parser objects
        for (uint32 index = 0; index < iClipInfoList.size(); index++)
        {
            ReleaseMP4FileParser(index);
        }

        status = InitNextValidClipInPlaylist(NULL, reposIndex);

        if (status != PVMFSuccess)
        {
            for (uint32 i = 0; i < iNodeTrackPortList.size(); i++)
            {
                iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
            }
            iPlaybackClipIndex = iNumClipsInPlayList;
            RunIfNotReady();
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::DoSetDataSourcePositionPlaylist() Failed Status [%d]", status));
            return status;
        }
    }

    status = SetPlaybackStartupTime(targetNPT, actualNPT, actualMediaDataTS, seekToSyncPoint, streamID, reposIndex);
    if (status == PVMFSuccess)
    {
        // update playback clip index, as clip to which skip was issued might be corrupt.
        iPlaybackClipIndex = iNextInitializedClipIndex;
        iPlaybackParserObj = GetParserObjAtIndex(iNextInitializedClipIndex);
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::DoSetDataSourcePositionPlaylist() ClipIndex[%d]", iPlaybackClipIndex));
    return  status;
}

PVMFStatus PVMFMP4FFParserNode::DoSetDataSourcePosition()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoSetDataSourcePosition() In"));

    uint32 targetNPT = 0;
    uint32* actualNPT = NULL;
    uint32* actualMediaDataTS = NULL;
    bool seektosyncpoint = false;
    uint32 streamID = 0;

    iCurrentCommand.PVMFNodeCommand::Parse(targetNPT, actualNPT, actualMediaDataTS, seektosyncpoint, streamID);

    PVMFStatus status = SetPlaybackStartupTime(targetNPT, actualNPT, actualMediaDataTS, seektosyncpoint, streamID, iPlaybackClipIndex);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoSetDataSourcePosition() Out Status %d", status));
    return status;
}

PVMFStatus PVMFMP4FFParserNode::SetPlaybackStartupTime(uint32& aTargetNPT,
        uint32* aActualNPT,
        uint32* aActualMediaDataTS,
        bool aSeektosyncpoint,
        uint32 aStreamID,
        int32 reposIndex)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() ClipIndex[%d]", reposIndex));

    IMpeg4File* parserObj = GetParserObjAtIndex(reposIndex);
    if (NULL == parserObj)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTimeStamp() Failed, Parser not created!"));
        return PVMFFailure;
    }
    PVMFStatus aEventCode = PVMFSuccess;
    PVUuid aEventUuid;

    int32 err = 0;
    uint32* trackList = NULL;
    uint32 i = 0;
    OSCL_TRY(err, trackList = OSCL_ARRAY_NEW(uint32, MAX_TRACK_NO););
    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);
    for (i = 0; i < MAX_TRACK_NO; i++)
    {
        trackList[i] = 0; // MPEG4 Specification: TrackId will start from 1. Init to 0 is OK.
    }
    if (!trackList || iNodeTrackPortList.empty())
    {
        PVMF_BASE_NODE_ARRAY_DELETE(trackList);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Memory for track list could not be allocated or no tracks to position"));
        return PVMFFailure;
    }

    // reset download complete flag
    iDownloadComplete = false;

    /* Check for repositioning request support in case of PPB.
       In PPB, Seek is not permitted in case when byte-seek is disabled. */
    if ((aTargetNPT != 0) && (iDataStreamInterface != NULL))
    {
        if ((iDataStreamInterface->QueryBufferingCapacity() != 0)
                && (iIsByteSeekNotSupported == true))
        {
            if (iInterfaceState == EPVMFNodePrepared)
            {
                /*This means engine is trying to start the playback session at a non-zero NPT.
                In case of PPB, this is not possible if server does not support byte-seek. */
                PVMF_BASE_NODE_ARRAY_DELETE(trackList);
                return PVMFFailure;
            }
            else
            {
                PVMF_BASE_NODE_ARRAY_DELETE(trackList);
                return PVMFErrNotSupported;
            }
        }
    }

    // Validate the parameters
    if (aActualNPT == NULL || aActualMediaDataTS == NULL)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(trackList);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Invalid parameters"));
        return PVMFErrArgument;
    }

    for (i = 0; i < iNodeTrackPortList.size(); ++i)
    {
        iNodeTrackPortList[i].iSendBOS = true;
    }
    //save the stream id for next media segment
    iStreamID = aStreamID;

    // this will guarantee that reverse mode starts from a valid TS
    if (PVMF_DATA_SOURCE_DIRECTION_REVERSE == iPlayBackDirection)
    {
        iStartForNextTSSearch = aTargetNPT;
        for (uint32 i = 0; i < iNodeTrackPortList.size();  i++)
        {
            if (parserObj->getTrackMediaType(iNodeTrackPortList[i].iTrackId) == MEDIA_TYPE_VISUAL)
            {
                iPrevSampleTS = parserObj->getMediaTimestampForCurrentSample(iNodeTrackPortList[i].iTrackId);
                iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
            }
        }
    }
    // First check if MP4 file is being downloaded to make sure the requested position is before amount downloaded
    if (download_progress_clock.GetRep())
    {
        // Get the amount downloaded so far
        bool tmpbool = false;
        uint32 dltime = 0;
        download_progress_clock->GetCurrentTime32(dltime, tmpbool, PVMF_MEDIA_CLOCK_MSEC);
        // Check if the requested time is past that
        if (aTargetNPT >= dltime)
        {
            // For now, fail in this case. In future, we might want to reposition to somewhere valid.
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Positioning past the amount downloaded so return as argument error"));
            PVMF_BASE_NODE_ARRAY_DELETE(trackList);
            return PVMFErrArgument;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() TargetNPT %d, SeekToSyncPoint %d", aTargetNPT, aSeektosyncpoint));

    // The media data timestamp of the next sample will start from the maximum
    // of timestamp on all selected tracks.  This media data timestamp will
    // correspond to the actual NPT.
    // The media data timestamp of the next sample will start from the maximum of timestamp on all
    // selected tracks.  This media data timestamp will correspond to the actual NPT.
    MediaMetaInfo info;

    i = 0;
    *aActualMediaDataTS = 0;
    for (i = 0; i < iNodeTrackPortList.size(); i++)
    {
        // Save the track list while in this loop
        // trackList[i] = iNodeTrackPortList[i].iTrackId;

        // This is no need to peek to get the prev sample duration.
        // Previous gets acct for time stamp deltas of all retrieved samples

        // For end-of-track case where there is no more samples to peek, the timestamp should be past the
        // last valid sample so there is no need to advance to the timestamp

        // Retrieve the next timestamp for this track
        iNodeTrackPortList[i].iClockConverter->set_clock(iNodeTrackPortList[i].iTimestamp, 0);
        uint32 millisecTS = iNodeTrackPortList[i].iClockConverter->get_converted_ts(1000);

        // Actual media data TS is the max timestamp of all selected tracks
        if (millisecTS > *aActualMediaDataTS)
        {
            *aActualMediaDataTS = millisecTS;
        }

        // There could be more than 3 TRAK per MOOV, above loop takes first encountered different traks
        // into account and store the trackList[0];trackList[1];trackList[2] as video, audio and text.
        // even if some any track are not present the index are kept as it is.
        if ((parserObj->getTrackMediaType(iNodeTrackPortList[i].iTrackId) == MEDIA_TYPE_VISUAL)
                && (0 == trackList[0]))
        {
            trackList[0] = iNodeTrackPortList[i].iTrackId;
        }

        if ((parserObj->getTrackMediaType(iNodeTrackPortList[i].iTrackId) == MEDIA_TYPE_AUDIO)
                && (0 == trackList[1]))
        {
            trackList[1] = iNodeTrackPortList[i].iTrackId;
        }

        if ((parserObj->getTrackMediaType(iNodeTrackPortList[i].iTrackId) == MEDIA_TYPE_TEXT)
                && (0 == trackList[2]))
        {
            trackList[2] = iNodeTrackPortList[i].iTrackId;
        }
    }
    uint64 duration64 = parserObj->getMovieDuration();
    uint32 duration = Oscl_Int64_Utils::get_uint64_lower32(duration64);
    uint32 timescale = parserObj->getMovieTimescale();
    if (timescale > 0 && timescale != 1000)
    {
        // Convert to milliseconds
        MediaClockConverter mcc(timescale);
        mcc.set_clock(duration64, 0);
        duration = mcc.get_converted_ts(1000);
    }
    if (duration && (aTargetNPT >= duration) && (PVMF_DATA_SOURCE_DIRECTION_REVERSE != iPlayBackDirection))
    {
        //report EOT for all streams.
        for (i = 0; i < iNodeTrackPortList.size(); i++)
        {
            uint32 resetNPT = 0;
            iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
            // reset all tracks to zero.
            resetNPT = parserObj->resetPlayback(0, (uint16)TRACK_NO_PER_RESET_PLAYBACK_CALL,
                                                &trackList[i], aSeektosyncpoint);
            iNodeTrackPortList[i].iClockConverter->set_clock_other_timescale(*aActualMediaDataTS, 1000);
            iNodeTrackPortList[i].iTimestamp = iNodeTrackPortList[i].iClockConverter->get_current_timestamp64();
        }

        // Cancel callback notifications on Datastream
        if (autopaused || iExternalDownload)
        {
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Cancel notification callback"));
            autopaused = false;
            if (download_progress_interface != NULL && iDataStreamInterface != NULL)
            {
                download_progress_interface->cancelResumeNotification();
            }
            else if (iExternalDownload)
            {
                // Cancel the Underflow check Timer
                iUnderFlowCheckTimer->Cancel(PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID);

                //Cancel the callback. This should also succeed if there is nothing to cancel
                MP4_ERROR_CODE retVal = parserObj->CancelNotificationSync();
                if (retVal != EVERYTHING_FINE)
                {
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Wrong Datastream SessionID"));
                    OSCL_ASSERT(false);
                }
            }
        }

        *aActualNPT = duration;
        PVMF_BASE_NODE_ARRAY_DELETE(trackList);
        return PVMFSuccess;
    }

    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() *actualMediaDataTS %d",
                                         *aActualMediaDataTS));

    // Change of logic: Individual track will call separate resetPlayback
    // Sequence of call is video->audio->text.If a few is not available that call will be skipped.
    // Ony One track id is provided in tempTrackId. Updated targetNPT is passed into next resetPlayback
    // More than 3 TRAK per MOOV not handled, first occurance of individual Track will be considered.

    *aActualNPT = aTargetNPT; // init *actualNPT to targetNPT
    uint32 tempNPT = 0;
    uint32 tempTrackId = 0;

    // Rest the video present before calling the resetPlayback Individually
    parserObj->ResetVideoTrackPresentFlag();

    TOsclFileOffset minFileOffset = 0x7FFFFFFF;
    if (0 != trackList[0])
    {
        tempNPT = aTargetNPT;        // For logging Purpose
        tempTrackId = trackList[0];
        aTargetNPT = parserObj->resetPlayback(aTargetNPT, (uint16)TRACK_NO_PER_RESET_PLAYBACK_CALL,
                                              &tempTrackId, aSeektosyncpoint);

        MediaClockConverter mcc(1000);
        mcc.set_clock(aTargetNPT, 0);
        uint64 mediats64 = mcc.get_converted_ts64(parserObj->getTrackMediaTimescale(tempTrackId));

        TOsclFileOffset offset = 0;
        int32 ret = parserObj->getOffsetByTime(tempTrackId, mediats64, &offset, 0);
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() getOffsetByTime ret %d Track %d NPT %d Offset %u",
                                             ret, trackList[0], aTargetNPT, (uint32)offset));

        OSCL_UNUSED_ARG(ret);

        minFileOffset = offset;
        minFileOffsetTrackID = tempTrackId;

        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Video targetNPT = %d returns actualNPT=%d for trackId=%d",
                                             tempNPT, aTargetNPT, trackList[0]));
    }

    if (0 != trackList[1])
    {
        tempNPT = aTargetNPT;
        tempTrackId = trackList[1];
        aTargetNPT = parserObj->resetPlayback(aTargetNPT, (uint16)TRACK_NO_PER_RESET_PLAYBACK_CALL,
                                              &tempTrackId, aSeektosyncpoint);

        MediaClockConverter mcc(1000);
        mcc.set_clock(aTargetNPT, 0);
        uint64 mediats64 = mcc.get_converted_ts64(parserObj->getTrackMediaTimescale(tempTrackId));

        TOsclFileOffset offset = 0;
        int32 ret = parserObj->getOffsetByTime(tempTrackId, mediats64, &offset, 0);
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() getOffsetByTime ret %d Track %d NPT %d Offset %u",
                                             ret, trackList[1], aTargetNPT, (uint32)offset));
        OSCL_UNUSED_ARG(ret);

        if (minFileOffset > offset)
        {
            minFileOffset = offset;
            minFileOffsetTrackID = tempTrackId;
        }

        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DoSetDataSourcePosition() Audio targetNPT = %d returns actualNPT=%d for trackId=%d",
                                             tempNPT, aTargetNPT, trackList[1]));
    }

    // Resetting Text Track might send 0 NPT, *actualNPT will be decided from audio and video.
    *aActualNPT = aTargetNPT;

    if (0 != trackList[2])
    {
        tempNPT = aTargetNPT;
        tempTrackId = trackList[2];
        tempNPT = parserObj->resetPlayback(aTargetNPT, (uint16)TRACK_NO_PER_RESET_PLAYBACK_CALL,
                                           &tempTrackId, aSeektosyncpoint);
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Text targetNPT = %d returns actualNPT=%d for trackId=%d",
                                             aTargetNPT, tempNPT, trackList[2]));

        MediaClockConverter mcc(1000);
        mcc.set_clock(tempNPT, 0);
        uint64 mediats64 = mcc.get_converted_ts64(parserObj->getTrackMediaTimescale(tempTrackId));

        TOsclFileOffset offset = 0;
        int32 ret = parserObj->getOffsetByTime(tempTrackId, mediats64, &offset, 0);
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() getOffsetByTime ret %d Track %d NPT %d Offset %u",
                                             ret, trackList[2], tempNPT, (uint32)offset));
        OSCL_UNUSED_ARG(ret);

        if (minFileOffset > offset)
        {
            minFileOffset = offset;
            minFileOffsetTrackID = tempTrackId;
        }

        // Use case, Text Only Track, *actualNPT will be set to output of Text resetPlayback.
        if ((0 == trackList[0]) && (0 == trackList[1]))
        {
            *aActualNPT = tempNPT;
        }
    }

    // There is no guarantee that each track is lined up at the same timestamp after repositioning.
    // So we need to find the track with the minimum NPT timestamp which will be set as the starting
    // media data timestamp after repositioning. Then the other tracks will need to offset from that time

    // First determine the track with the minimum timestamp after repositioning
    uint32 numSamples = 1;
    uint32 mints = MAX32BITUINT;
    int32 retval = EVERYTHING_FINE;
    int32 *retValPerTrack = NULL;
    uint32 *retNumSamplesPerTrack = NULL;
    uint64 *trackTSAfterRepo = NULL;
    //array to keep the timestamp of those samples from where playback has to be started i.e. timestamp of current samples to be played back.
    trackTSAfterRepo = (uint64*) OSCL_MALLOC(iNodeTrackPortList.size() * sizeof(uint64));
    retValPerTrack = (int32*) OSCL_MALLOC(iNodeTrackPortList.size() * sizeof(int32));
    retNumSamplesPerTrack = (uint32*) OSCL_MALLOC(iNodeTrackPortList.size() * sizeof(uint32));

    if ((trackTSAfterRepo == NULL) || (retValPerTrack == NULL) || (retNumSamplesPerTrack == NULL))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Memory alloc for array to keep the timestamp of the samples failed"));
        PVMF_BASE_NODE_ARRAY_DELETE(trackList);
        OSCL_FREE(trackTSAfterRepo);
        trackTSAfterRepo = NULL;
        OSCL_FREE(retValPerTrack);
        retValPerTrack = NULL;
        OSCL_FREE(retNumSamplesPerTrack);
        retNumSamplesPerTrack = NULL;
        return PVMFErrNoMemory;
    }
    for (i = 0; i < iNodeTrackPortList.size(); i++)
    {
        // Peek the next sample to get the duration of the last sample
        numSamples = 1;
        retval = parserObj->peekNextBundledAccessUnits(iNodeTrackPortList[i].iTrackId, &numSamples, &info);
        trackTSAfterRepo[i] = info.ts;
        retNumSamplesPerTrack[i] = numSamples;
        retValPerTrack[i] = retval;

        if (((retval == EVERYTHING_FINE) || (retval == END_OF_TRACK))
                && (numSamples > 0))
        {
            // Check if sample info was returned. Only use valid
            // samples for this search
            // Set the new starting timestamp to the clock convert
            iNodeTrackPortList[i].iClockConverter->set_clock(info.ts, 0);

            // Check if this is the minimum
            if (iNodeTrackPortList[i].iFormatType != PVMF_MIME_3GPP_TIMEDTEXT)
            {
                uint32 trackstartts = iNodeTrackPortList[i].iClockConverter->get_converted_ts(1000);
                if (trackstartts < mints)
                {
                    mints = trackstartts;
                }
            }
        }
        else if (retval == END_OF_TRACK)
        {
            // do nothing. No need to use the track to calculate mints, as next sample is EOT.
        }
        else if (retval == INSUFFICIENT_DATA)
        {
            iNodeTrackPortList[i].iClockConverter->set_clock_other_timescale(*aActualMediaDataTS, 1000);
            if (iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_ENDOFTRACK
                    || iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK)
            {
                iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
            }

            iNodeTrackPortList[i].iTimestamp = iNodeTrackPortList[i].iClockConverter->get_current_timestamp();
            iNodeTrackPortList[i].iFirstFrameAfterRepositioning = true;
            iNodeTrackPortList[i].iCurrentTextSampleEntry.Unbind();
            // convert target NPT to media timescale
            MediaClockConverter mcc(1000);
            mcc.set_clock(aTargetNPT, 0);
            uint64 targetNPTtInMediaTimeScale =
                mcc.get_converted_ts64(parserObj->getTrackMediaTimescale(iNodeTrackPortList[i].iTrackId));

            iNodeTrackPortList[i].iTargetNPTInMediaTimeScale = /*(uint32)*/targetNPTtInMediaTimeScale;
        }
        else
        {
            // Return as error
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Peeking next sample failed while determining the min timestamp after repositioning"));
            if (!MapMP4ErrorCodeToEventCode(retval,  aEventUuid, aEventCode))
            {
                aEventUuid = PVMFFileFormatEventTypesUUID;
                aEventCode = PVMFFFErrMisc;
            }
            OSCL_ARRAY_DELETE(trackList);
            OSCL_FREE(trackTSAfterRepo);
            OSCL_FREE(retValPerTrack);
            OSCL_FREE(retNumSamplesPerTrack);
            trackTSAfterRepo = NULL;
            return PVMFErrResource;
        }
    }

    if (mints == MAX32BITUINT)
    {
        mints = *aActualNPT;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Minimum timestamp could not be determined so using the actual NPT %d", mints));
    }
    else if (mints != *aActualNPT)
    {
        *aActualNPT = mints;
    }

    // Now adjust the timestamp of each track in reference to this minimum
    for (i = 0; i < iNodeTrackPortList.size(); i++)
    {
        // now no need to call 2nd peek, we have timestamp of current sample and retValPerTrack[i] by 1st peek call.
        if (retValPerTrack[i] == EVERYTHING_FINE || retValPerTrack[i] == END_OF_TRACK)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, \
                            (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() targetNPT1 =%d, TrackId=%d, State =%d,\
                             Timestamp = %d,TargetNPTInMediaTimeScale=%d", aTargetNPT, iNodeTrackPortList[i].iTrackId, \
                             iNodeTrackPortList[i].iState, iNodeTrackPortList[i].iTimestamp, \
                             iNodeTrackPortList[i].iTargetNPTInMediaTimeScale));

            if (retNumSamplesPerTrack[i] > 0)
            {
                iNodeTrackPortList[i].iClockConverter->set_clock(trackTSAfterRepo[i], 0);
                uint32 trackts = iNodeTrackPortList[i].iClockConverter->get_converted_ts(1000);
                if (iNodeTrackPortList[i].iFormatType == PVMF_MIME_3GPP_TIMEDTEXT && trackts < mints)
                {
                    uint32 diffMintsTrackts = mints - trackts;

                    /**********************************************************************
                    * As trackts is smaller than mints, we will reduce the text sample duration
                    * by diffMintsTrackts and assign text sample ts as actualMediaDataTS.
                    * if (*actualMediaDataTS + (trackts-mints)) has a negative value, then we will
                    * have a negative value for TS which is incorrect. So setting TS to actualMediaTS instead
                    * of negative value and accordingly adjusting its duration.
                    ***********************************************************************/
                    iTextInvalidTSAfterReposition = true;
                    iDelayAddToNextTextSample = diffMintsTrackts;
                    iNodeTrackPortList[i].iClockConverter->set_clock_other_timescale(*aActualMediaDataTS, 1000);
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() iTextInvalidTSAfterReposition"));
                }
                else
                {
                    if (retValPerTrack[i] == END_OF_TRACK)
                    {
                        // if next sample is EOT then just assign actualMediaDataTS as the TS for the sample.
                        iNodeTrackPortList[i].iClockConverter->set_clock_other_timescale(*aActualMediaDataTS, 1000);
                    }
                    else
                    {
                        // Set the timestamp with offset from minimum TS to the TS for the next sample
                        iNodeTrackPortList[i].iClockConverter->set_clock_other_timescale(*aActualMediaDataTS + (trackts - mints), 1000);
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() TimeADJ2 *actualMediaDataTS %d,  trackts%d,  mints %d, TrackId %d Adj to %d", *aActualMediaDataTS, trackts,  mints, iNodeTrackPortList[i].iTrackId, (*aActualMediaDataTS + (trackts - mints))));

                    }
                }
            }
            else
            {
                // Since sample is not available, just set the track timestamp to the calculated starting media data TS
                iNodeTrackPortList[i].iClockConverter->set_clock_other_timescale(*aActualMediaDataTS, 1000);
            }

            if (autopaused || iExternalDownload)
            {
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Cancel notification callback"));
                autopaused = false;
                if (download_progress_interface != NULL && iDataStreamInterface != NULL)
                {
                    download_progress_interface->cancelResumeNotification();
                }
                else if (iExternalDownload)
                {
                    // Cancel the Underflow check Timer
                    iUnderFlowCheckTimer->Cancel(PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID);

                    //Cancel the callback. This should also succeed if there is nothing to cancel
                    MP4_ERROR_CODE retval = parserObj->CancelNotificationSync();
                    if (retval != EVERYTHING_FINE)
                    {
                        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Wrong Datastream SessionID"));
                        OSCL_ASSERT(false);
                    }
                }
            }
            iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;

            iNodeTrackPortList[i].iTimestamp = iNodeTrackPortList[i].iClockConverter->get_current_timestamp();
            iNodeTrackPortList[i].iFirstFrameAfterRepositioning = true;
            iNodeTrackPortList[i].iCurrentTextSampleEntry.Unbind();
            // convert target NPT to media timescale
            MediaClockConverter mcc(1000);
            mcc.set_clock(aTargetNPT, 0);
            uint64 targetNPTtInMediaTimeScale =
                mcc.get_converted_ts(parserObj->getTrackMediaTimescale(iNodeTrackPortList[i].iTrackId));
            iNodeTrackPortList[i].iTargetNPTInMediaTimeScale = /*(uint32)*/targetNPTtInMediaTimeScale;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, \
                            (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime(): targetNPT2 =%d, TrackId=%d, State =%d,\
                             Timestamp = %d,TargetNPTInMediaTimeScale=%d", aTargetNPT, iNodeTrackPortList[i].iTrackId, \
                             iNodeTrackPortList[i].iState, iNodeTrackPortList[i].iTimestamp, \
                             iNodeTrackPortList[i].iTargetNPTInMediaTimeScale));
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() targetNPT=%d, actualNPT=%d, actualTS=%d",
                     aTargetNPT, *aActualNPT, *aActualMediaDataTS));
    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() targetNPT=%d, actualNPT=%d, actualTS=%d",
                                         aTargetNPT, *aActualNPT, *aActualMediaDataTS));

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SetPlaybackStartupTime() Out"));

    PVMF_BASE_NODE_ARRAY_DELETE(trackList);
    OSCL_FREE(trackTSAfterRepo);
    trackTSAfterRepo = NULL;
    OSCL_FREE(retValPerTrack);
    OSCL_FREE(retNumSamplesPerTrack);
    retNumSamplesPerTrack = NULL;
    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::DoQueryDataSourcePosition()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoQueryDataSourcePosition() In"));

    uint32 targetNPT = 0;
    uint32* seekPointBeforeTargetNPT = NULL;
    uint32* seekPointAfterTargetNPT = NULL;
    bool seektosyncpoint = false;

    iCurrentCommand.PVMFNodeCommand::Parse(targetNPT, seekPointBeforeTargetNPT,
                                           seektosyncpoint, seekPointAfterTargetNPT);

    // Check the passed-in parameters
    if ((seekPointBeforeTargetNPT == NULL) || (seekPointBeforeTargetNPT == NULL))
    {
        return PVMFErrArgument;
    }

    // First check if MP4 file is being downloaded to make sure the requested position is before amount downloaded
    if (download_progress_clock.GetRep())
    {
        // Get the amount downloaded so far
        bool tmpbool = false;
        uint32 dltime = 0;
        download_progress_clock->GetCurrentTime32(dltime, tmpbool, PVMF_MEDIA_CLOCK_MSEC);
        // Check if the requested time is past that
        if (targetNPT >= dltime)
        {
            // For now, fail in this case. In future, we might want to return a position that is valid.
            return PVMFErrArgument;
        }
    }

    // Make sure track list is available
    if (iNodeTrackPortList.empty())
    {
        return PVMFFailure;
    }

    // Copy the track IDs into track list array
    int32 err = 0;
    uint32* trackList = NULL;
    OSCL_TRY(err, trackList = (uint32*)oscl_malloc(iNodeTrackPortList.size() * sizeof(uint32)););
    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);
    if (trackList == NULL)
    {
        return PVMFErrNoMemory;
    }

    for (uint32 i = 0; i < iNodeTrackPortList.size(); i++)
    {
        trackList[i] = iNodeTrackPortList[i].iTrackId;
    }

    // See if targetNPT is greater than or equal to clip duration.
    uint64 duration64 = iPlaybackParserObj->getMovieDuration();
    uint32 duration = Oscl_Int64_Utils::get_uint64_lower32(duration64);
    uint32 timescale = iPlaybackParserObj->getMovieTimescale();
    if (timescale > 0 && timescale != 1000)
    {
        // Convert to milliseconds
        MediaClockConverter mcc(timescale);
        mcc.set_clock(duration64, 0);
        duration = mcc.get_converted_ts(1000);
    }
    if (targetNPT >= duration)
    {
        *seekPointBeforeTargetNPT = targetNPT;
        *seekPointAfterTargetNPT = targetNPT;

        oscl_free(trackList);
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DoQueryDataSourcePosition: targetNPT=%d, closestNPTBefore=%d, closestNPTAfter=%d",
                                             targetNPT, *seekPointBeforeTargetNPT, *seekPointAfterTargetNPT));
        return PVMFSuccess;
    }

    // Determine the sync point - forwards and backwards
    bool oBeforeTargetNPT;

    oBeforeTargetNPT = true;
    *seekPointBeforeTargetNPT =
        iPlaybackParserObj->queryRepositionTime(targetNPT,
                                                (uint16)(iNodeTrackPortList.size()),
                                                trackList,
                                                seektosyncpoint,
                                                oBeforeTargetNPT);

    oBeforeTargetNPT = false;
    *seekPointAfterTargetNPT =
        iPlaybackParserObj->queryRepositionTime(targetNPT,
                                                (uint16)(iNodeTrackPortList.size()),
                                                trackList,
                                                seektosyncpoint,
                                                oBeforeTargetNPT);

    // Calculations of actual NPT will be done by Engine Now; not here

    oscl_free(trackList);

    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DoQueryDataSourcePosition: targetNPT=%d, closestNPTBefore=%d, closestNPTAfter=%d",
                                         targetNPT, *seekPointBeforeTargetNPT, *seekPointAfterTargetNPT));


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoQueryDataSourcePosition() Out"));
    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::DoSetDataSourceRate()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoSetDataSourceRate() In"));

    // Retrieve the new rate
    int32 rate;
    PVMFTimebase* timebase = NULL;
    iCurrentCommand.PVMFNodeCommand::Parse(rate, timebase);

    if (timebase == NULL)
    {
        if (rate < 10000 || rate > 500000)
        {
            // Limit to 0.1X to 5X for now.
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::DoSetDataSourceRate() Invalid playback rate %d", rate));
            return PVMFErrNotSupported;
        }
    }

    // if we are going ff to normal or vice versa, we need to set the node to disable non-video
    if (!iParseAudioDuringFF && (iDataRate != rate) && (PVMF_DATA_SOURCE_DIRECTION_FORWARD == iPlayBackDirection))
    {
        // coming to normal rate?
        if (rate == NORMAL_PLAYRATE)
        {
            iParseVideoOnly = false;
        }
        // switching to FF or rew?
        else
        {
            // since we know ff will not have audio, just disable audio now.
            // If it is going to REW, DoSetDataSourceDirection will modify it there
            iParseVideoOnly = true;
        }
    }


    iDataRate = rate;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoSetDataSourceRate() Out"));
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::DoSetDataSourceDirection()
{
    int32 direction = -1;
    uint32* actualNPT = NULL;
    uint32* actualMediaDataTS = NULL;
    PVMFTimebase* timebase;

    iCurrentCommand.PVMFNodeCommand::Parse(direction, actualNPT, actualMediaDataTS, timebase);

    /* Validate the parameters */
    if ((actualNPT == NULL) || (actualMediaDataTS == NULL))
    {
        CommandComplete(iCurrentCommand, PVMFErrArgument);
        return PVMFFailure;
    }

    *actualMediaDataTS = 0;

    /*
     * The media data timestamp of the next sample will start from the maximum
     * of timestamp on all selected tracks.  This media data timestamp will
     * correspond to the actual NPT.
     */
    uint32 i;
    for (i = 0; i < iNodeTrackPortList.size(); i++)
    {
        uint64 timeStamp = 0;
        /* Use an arbitary delta */
        timeStamp = iNodeTrackPortList[i].iTimestamp;

        iNodeTrackPortList[i].iClockConverter->set_clock(timeStamp, 0);
        timeStamp = iNodeTrackPortList[i].iClockConverter->get_converted_ts(1000);

        /*
         * Actual media data TS is the max timestamp of all selected tracks
         */
        if (timeStamp > *actualMediaDataTS)
        {
            *actualMediaDataTS = (uint32)timeStamp;
        }
    }

    /* ensure all current track TS starts after max of selected track */
    //In MP4, TS could be max of selected track, but here we need to
    //have it more than max of selected track because the duration of
    //sample in asf is not set.Therefore comparison in the sync util
    //(aDataTimeStamp + aDuration)> iResumeTimeStamp fails for the
    //first valid packet send out from the parser node to the sync util
    //and is discarded.

    MediaClockConverter mcc(1000);
    mcc.set_clock(*actualMediaDataTS, 0);
    for (i = 0; i < iNodeTrackPortList.size(); i++)
    {
        uint64 actualMediaDataTSInMediaTimeScale64 =
            mcc.get_converted_ts64(iPlaybackParserObj->getTrackMediaTimescale(iNodeTrackPortList[i].iTrackId));
        iNodeTrackPortList[i].iTimestamp = actualMediaDataTSInMediaTimeScale64;
    }

    *actualNPT = 0;
    /*
     * If SetDataSourceDirection call made in prepared state, with fwd direction
     * do nothing.
     */
    if ((iInterfaceState == EPVMFNodePrepared) &&
            (direction == PVMF_DATA_SOURCE_DIRECTION_FORWARD))
    {
        CommandComplete(iCurrentCommand, PVMFSuccess);
        return PVMFSuccess;
    }

    /*
     * If direction is reverse then actual NPT is the max of all track NPTs.
     * If direction is forward then actual NPT is the min of all track NPTs.
     * iPrevSampleTimeStamp is the NPT TS of the last retrieved sample
     */
    uint32 actualNPT32 = 0;
    if (direction == PVMF_DATA_SOURCE_DIRECTION_FORWARD)
    {
        // if switching back to forward at 100000, make sure everything is set to parse
        // othrewise check what ff should be for non-video tracks and set it

        iParseVideoOnly = ((iDataRate == NORMAL_PLAYRATE) || iParseAudioDuringFF) ? false : true;

        actualNPT32 = 0x7FFFFFFF;
        for (i = 0; i < iNodeTrackPortList.size(); i++)
        {
            uint64 lastTS = iPlaybackParserObj->getMediaTimestampForCurrentSample(iNodeTrackPortList[i].iTrackId);
            iNodeTrackPortList[i].iClockConverter->set_clock(lastTS, 0);
            lastTS = iNodeTrackPortList[i].iClockConverter->get_converted_ts(1000);
            if (lastTS < actualNPT32)
            {
                actualNPT32 = (uint32)lastTS;
            }

            // temporarily disable all tracks until the next setdatasourceposition comes in
            iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ENDOFTRACK;
        }
    }
    else if (PVMF_DATA_SOURCE_DIRECTION_REVERSE == direction)
    {
        iParseVideoOnly = iParseAudioDuringREW ? false : true;

        actualNPT32 = 0;
        for (i = 0; i < iNodeTrackPortList.size(); i++)
        {
            uint64 lastTS = iPlaybackParserObj->getMediaTimestampForCurrentSample(iNodeTrackPortList[i].iTrackId);
            iNodeTrackPortList[i].iClockConverter->set_clock(lastTS, 0);
            lastTS = iNodeTrackPortList[i].iClockConverter->get_converted_ts(1000);
            if (lastTS > actualNPT32)
                actualNPT32 = (uint32)lastTS;

            // stop transmitting until after repositioning
            iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ENDOFTRACK;
        }
    }
    else
    {
        OSCL_ASSERT(false);
    }

    *actualNPT = actualNPT32;

    iPlayBackDirection = direction;

    PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::DoSetDataSourceDirection: direction=%d, actualNPT=%d, actualTS=%d",
                                  direction, *actualNPT, *actualMediaDataTS));

    return PVMFSuccess;
}


void PVMFMP4FFParserNode::HandleTrackState()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::HandleTrackState() In"));

    for (uint32 i = 0; i < iNodeTrackPortList.size(); ++i)
    {
        switch (iNodeTrackPortList[i].iState)
        {
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_UNINITIALIZED:
                if (RetrieveTrackConfigInfo(iNodeTrackPortList[i].iTrackId, iNodeTrackPortList[i].iFormatType, iNodeTrackPortList[i].iFormatSpecificConfig) == false)
                {
                    // Failed
                    break;
                }
                iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                // Continue on to retrieve the first frame

            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA:
                if (iNodeTrackPortList[i].iSendBOS)
                {
                    iNodeTrackPortList[i].iClockConverter->update_clock(iNodeTrackPortList[i].iTimestamp);
                    uint32 timestamp = iNodeTrackPortList[i].iClockConverter->get_converted_ts(1000);
                    if (!SendBeginOfMediaStreamCommand(iNodeTrackPortList[i].iPortInterface, iStreamID, timestamp, iNodeTrackPortList[i].iSeqNum++, iPlaybackClipIndex))
                    {
                        if (iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDDATA)
                            iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DESTFULL;
                        break;
                    }
                    else
                    {
                        iNodeTrackPortList[i].iSendBOS = false;
                    }
                }
                if (iNodeTrackPortList[i].iFirstFrameAfterRepositioning)
                {
                    //after repo, let the track with min file offset retrieve data first
                    uint32 j = 0;
                    for (j = 0; j < iNodeTrackPortList.size(); ++j)
                    {
                        if (minFileOffsetTrackID == iNodeTrackPortList[j].iTrackId)
                        {
                            break;
                        }
                    }
                    if ((i != j) && (iNodeTrackPortList[j].iFirstFrameAfterRepositioning))
                    {
                        //LOGE("Ln %d UGLY? Yes. minFileOffsetTrackID %d Skipped iTrackId %d", __LINE__, minFileOffsetTrackID , iNodeTrackPortList[j].iTrackId);
                        break;
                    }
                }

                if (!RetrieveTrackData(iNodeTrackPortList[i]))
                {
                    if ((iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK) ||
                            (iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDBOC) ||
                            (iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDEOC))

                    {
                        Reschedule();
                    }
                    if (iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_SKIP_CORRUPT_SAMPLE)
                    {
                        iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                        Reschedule();
                    }
                    if (iNodeTrackPortList[i].iNoSampleInCurrentMoof)
                    {
                        iNodeTrackPortList[i].iNoSampleInCurrentMoof = false;
                        Reschedule();
                    }
                    break;
                }
                if (iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRACKMAXDATASIZE_RESIZE)
                {
                    iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                    Reschedule();
                    break;
                }
                else
                {
                    iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDDATA;
                }
                // Continue on to send the frame

            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDDATA:
                if (SendTrackData(iNodeTrackPortList[i]))
                {
                    iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                }
                Reschedule();
                break;

            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK:
            {
                // only support audio-only playlist for now
                if (iPlaybackClipIndex < (int32)(iNumClipsInPlayList - 1))
                {
                    if (iNodeTrackPortList.size() > 1)
                    {
                        uint32 numEOS = 0;
                        for (uint32 j = 0; j < iNodeTrackPortList.size(); ++j)
                        {
                            if ((iNodeTrackPortList[j].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK) ||
                                    (iNodeTrackPortList[j].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_ENDOFTRACK))
                            {
                                numEOS++;
                            }
                        }
                        if (numEOS == iNodeTrackPortList.size())
                        {
                            iInitNextClip = true;
                        }
                    }
                    else
                    {
                        iInitNextClip = true;
                    }
                }

                // lets initialize the next clip in queue.
                if (iInitNextClip)
                {
                    PVMFStatus status = InitNextValidClipInPlaylist();
                    if (status != PVMFSuccess)
                    {
                        iInitNextClip = false;
                    }
                }
                iNodeTrackPortList[i].iClockConverter->update_clock(iNodeTrackPortList[i].iTimestamp);
                uint32 timestamp = iNodeTrackPortList[i].iClockConverter->get_converted_ts(1000);
                if (iNodeTrackPortList[i].iSendBOS)
                {
                    if (!SendBeginOfMediaStreamCommand(iNodeTrackPortList[i].iPortInterface, iStreamID, timestamp, iNodeTrackPortList[i].iSeqNum++, iPlaybackClipIndex))
                    {

                        if (iNodeTrackPortList[i].iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDDATA)
                            iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DESTFULL;
                        break;
                    }
                    else
                    {
                        iNodeTrackPortList[i].iSendBOS = false;
                    }
                }

                // Initialize duration to a default value
                uint32 TextSampleDuration = PVMF_DEFAULT_TRACK_DURATION;
                if (iNodeTrackPortList[i].iFormatTypeInteger == PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT &&
                        iEOTForTextSentToMIO)
                {
                    iEOTForTextSentToMIO = false;
                    if (iSetTextSampleDurationZero)
                    {
                        TextSampleDuration = 0;
                    }
                }
                else
                {
                    //EOS timestamp(aTrackPortInfo.iTimestamp)is considered while deciding the iResumeTimeStamp in the mediaoutput node
                    //therefore its length should also be considered while making decision to forward or drop the packet
                    //at the mediaoutput node.
                    TextSampleDuration = PVMP4FF_DEFAULT_EOS_DURATION_IN_SEC * (iNodeTrackPortList[i].iClockConverter->get_timescale());
                }

                if (SendEndOfTrackCommand(iNodeTrackPortList[i].iPortInterface, iStreamID, timestamp, iNodeTrackPortList[i].iSeqNum++, iPlaybackClipIndex, TextSampleDuration))
                {
                    // EOS command sent successfully
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFMP4FFParserNode::HandleTrackState() EOS media command sent successfully"));
                    iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ENDOFTRACK;

                    // if there are more clips to playback?
                    if ((iNodeTrackPortList.size() == 1) &&
                            (iPlaybackClipIndex < (int32)(iNumClipsInPlayList - 1)) &&
                            !iPlaylistExhausted &&
                            (iNextInitializedClipIndex >= 0))
                    {
                        // start sending out data from next clip immediately
                        IMpeg4File* parserObj = GetParserObjAtIndex(iNextInitializedClipIndex);
                        if (iNumClipsInPlayList > 1 &&
                                iNextInitializedClipIndex < (int32)iNumClipsInPlayList && // iNextInitializedClip index is within the clip list
                                NULL != parserObj) //next clip has been intialized successfully
                        {
                            iLastPlayingClipIndex = iPlaybackClipIndex;
                            iPlaybackClipIndex = iNextInitializedClipIndex; // pick up the next clip in list
                            iPlaybackParserObj = parserObj;
                            iNodeTrackPortList[i].iSendBOS = true;
                            iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                            Reschedule();
                        }
                    }
                }
                else
                {
                    // EOS command sending failed -- wait on outgoing queue ready notice
                    // before trying again.
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFMP4FFParserNode::HandleTrackState() EOS media command sending failed"));
                }
            }
            break;

            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDBOC:
                // need to send BOC
                // then followed by data
                if (iClipInfoList[iPlaybackClipIndex].iClipInfo.iSendBOC &&
                        iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasBOCFrame)
                {
                    // send BOC downstream
                    if (!SendBeginOfClipCommand(iNodeTrackPortList[i]))
                    {
                        // try again
                        Reschedule();
                    }
                }
                // may need to send EOC as well
                if (iClipInfoList[iPlaybackClipIndex].iClipInfo.iSendEOC &&
                        iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasEOCFrame)
                {
                    iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDEOC;
                }
                // falling through
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDEOC:

                // need to send EOC
                // then followed by data
                if (iClipInfoList[iPlaybackClipIndex].iClipInfo.iSendEOC &&
                        iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasEOCFrame)
                {
                    // send EOC downstream
                    if (!SendEndOfClipCommand(iNodeTrackPortList[i]))
                    {
                        // try again
                        Reschedule();
                    }
                }

                // send data
                iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDDATA;

                if (SendTrackData(iNodeTrackPortList[i]))
                {
                    iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                    RunIfNotReady();
                }
                break;

            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRACKDATAPOOLEMPTY:
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_MEDIADATAPOOLEMPTY:
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_INITIALIZED:
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_DESTFULL:
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_INSUFFICIENTDATA:
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_ENDOFTRACK:
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_ERROR:
            default:
                // Do nothing for these states for now
                break;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::HandleTrackState() Out"));
}

bool PVMFMP4FFParserNode::RetrieveTrackConfigInfo(uint32 aTrackId, PVMFFormatType aFormatType, OsclRefCounterMemFrag &aConfig)
{
    if (aFormatType == PVMF_MIME_3GPP_TIMEDTEXT)
    {
        // For timed text
        // Create refcounted mem frag for text track's format specific info
        OsclMemAllocDestructDealloc<uint8> fsi_alloc;
        uint32 aligned_refcnt_size = oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));
        uint32 aligned_textfsi_size = oscl_mem_aligned_size(sizeof(PVMFTimedTextFormatSpecificInfo));
        uint8* fsi_ptr = NULL;
        int32 errcode = 0;
        OSCL_TRY(errcode, fsi_ptr = (uint8*) fsi_alloc.ALLOCATE(aligned_refcnt_size + aligned_textfsi_size));
        OSCL_FIRST_CATCH_ANY(errcode,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackConfigInfo() Format specific info for text could not be allocated"));
                             return false);

        OsclRefCounter* fsi_refcnt = OSCL_PLACEMENT_NEW(fsi_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(fsi_ptr));
        fsi_ptr += aligned_refcnt_size;

        OsclMemoryFragment memfrag;
        memfrag.len = aligned_textfsi_size;
        memfrag.ptr = fsi_ptr;
        oscl_memset(fsi_ptr, 0, aligned_textfsi_size);

        // Copy the data from the text sample entry
        PVMFTimedTextFormatSpecificInfo* textfsi = (PVMFTimedTextFormatSpecificInfo*)fsi_ptr;
        textfsi->iUID32 = PVMFTimedTextFormatSpecificInfo_UID;
        textfsi->iLayer = iPlaybackParserObj->getLayer(aTrackId);
        textfsi->iTranslationMatrix[0] = (int32)(iPlaybackParserObj->getTextTrackXOffset(aTrackId));
        textfsi->iTranslationMatrix[1] = (int32)(iPlaybackParserObj->getTextTrackYOffset(aTrackId));
        textfsi->iWidth = (uint32)(iPlaybackParserObj->getTextTrackWidth(aTrackId));
        textfsi->iHeight = (uint32)(iPlaybackParserObj->getTextTrackHeight(aTrackId));

        // Save the text track's format specific info
        aConfig = OsclRefCounterMemFrag(memfrag, fsi_refcnt, aligned_textfsi_size);
    }
    else
    {
        // For other formats
        // Check if the track has decoder config info
        uint32 specinfosize = iPlaybackParserObj->getTrackDecoderSpecificInfoSize(aTrackId);
        if (specinfosize == 0)
        {
            // There is no decoder specific info so return and continue on. Not an error
            return true;
        }

        // Retrieve the decoder specific info from file parser
        uint8* specinfoptr = iPlaybackParserObj->getTrackDecoderSpecificInfoContent(aTrackId);
        if (specinfoptr == NULL)
        {
            // Error
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackConfigInfo: Error - getTrackDecoderSpecificInfoContent failed"));
            return false;
        }

        // Create mem frag for decoder specific config
        OsclMemAllocDestructDealloc<uint8> my_alloc;
        OsclRefCounter* my_refcnt;
        uint aligned_refcnt_size = oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));
        uint8* my_ptr = NULL;
        int32 errcode = 0;
        OSCL_TRY(errcode, my_ptr = (uint8*) my_alloc.ALLOCATE(aligned_refcnt_size + specinfosize));
        OSCL_FIRST_CATCH_ANY(errcode,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackConfigInfo() Memory allocation failed size = %d", aligned_refcnt_size + specinfosize));
                             return false;
                            );

        my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
        my_ptr += aligned_refcnt_size;

        OsclMemoryFragment memfrag;
        memfrag.len = specinfosize;
        memfrag.ptr = my_ptr;

        // Copy the decoder specific info to the memory fragment
        oscl_memcpy(memfrag.ptr, specinfoptr, specinfosize);

        // Save format specific info
        aConfig = OsclRefCounterMemFrag(memfrag, my_refcnt, specinfosize);
    }
    return true;
}

bool PVMFMP4FFParserNode::RetrieveTrackConfigInfoAndFirstSample(uint32 aTrackId,
        PVMFFormatType aFormatType,
        OsclRefCounterMemFrag &aConfig)
{
    if (aFormatType == PVMF_MIME_MPEG4_AUDIO)
    {
        // Check if the track has decoder config info
        uint32 specinfosize =
            iPlaybackParserObj->getTrackDecoderSpecificInfoSize(aTrackId);
        if (specinfosize == 0)
        {
            // There is no decoder specific info so return and continue on. Not an error
            return true;
        }

        // Retrieve the decoder specific info from file parser
        uint8* specinfoptr =
            iPlaybackParserObj->getTrackDecoderSpecificInfoContent(aTrackId);
        if (specinfoptr == NULL)
        {
            // Error
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackConfigInfoAndFirstSample: Error - getTrackDecoderSpecificInfoContent failed"));
            return false;
        }

        //get height from the first frame
        MediaMetaInfo info;
        uint32 numSamples = 1;
        int32 retval = EVERYTHING_FINE;
        retval = iPlaybackParserObj->peekNextBundledAccessUnits(aTrackId,
                 &numSamples,
                 &info);

        if ((retval == EVERYTHING_FINE || END_OF_TRACK == retval) && numSamples > 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0,
                            "PVMFMP4FFParserNode::RetrieveTrackConfigInfoAndFirstSample: retval=%d, numSamples=%d, len=%d",
                            retval, numSamples, info.len));

            uint32 sampleSize = info.len;
            if (sampleSize > 0)
            {
                uint8* sampleBuf = PVMF_BASE_NODE_ARRAY_NEW(uint8, sampleSize);

                oscl_memset(&iGau.buf, 0, sizeof(iGau.buf));
                oscl_memset(&iGau.info, 0, sizeof(iGau.info));
                iGau.free_buffer_states_when_done = 0;
                iGau.numMediaSamples = 1;
                iGau.buf.num_fragments = 1;
                iGau.buf.buf_states[0] = NULL;
                iGau.buf.fragments[0].ptr = (OsclAny*)sampleBuf;
                iGau.buf.fragments[0].len = sampleSize;
                iGau.frameNum = 0;
                retval =
                    iPlaybackParserObj->getNextBundledAccessUnits(aTrackId,
                            &numSamples,
                            &iGau);
                iPlaybackParserObj->resetPlayback();

                // Create mem frag for decoder specific config
                OsclMemAllocDestructDealloc<uint8> my_alloc;
                OsclRefCounter* my_refcnt;
                uint aligned_refcnt_size =
                    oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));
                uint8* my_ptr = NULL;
                int32 errcode = 0;
                OSCL_TRY(errcode, my_ptr = (uint8*) my_alloc.ALLOCATE(aligned_refcnt_size + specinfosize + sampleSize));
                OSCL_FIRST_CATCH_ANY(errcode,
                                     PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackConfigInfo() Memory allocation failed size = %d", aligned_refcnt_size + specinfosize + sampleSize));
                                     return false;
                                    );

                my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
                my_ptr += aligned_refcnt_size;

                OsclMemoryFragment memfrag;
                memfrag.len = (specinfosize + sampleSize);
                memfrag.ptr = my_ptr;

                // Copy the decoder specific info to the memory fragment
                oscl_memcpy(my_ptr, specinfoptr, specinfosize);
                my_ptr += specinfosize;
                // Copy the first sample into the memory fragment
                oscl_memcpy(my_ptr, sampleBuf, sampleSize);

                // Save format specific info plus first sample
                aConfig = OsclRefCounterMemFrag(memfrag, my_refcnt, (specinfosize + sampleSize));
                PVMF_BASE_NODE_ARRAY_DELETE(sampleBuf);
                return true;
            }
        }
    }
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackConfigInfoAndFirstSample: Error - Incorrect Format Type"));
    return false;
}

bool PVMFMP4FFParserNode::checkTrackPosition(PVMP4FFNodeTrackPortInfo& aTrackPortInfo, uint32 numsamples)
{
    // This function should use the peek calls to determine if the data is available in the file.

    // Check the time on num_samples+1 to make sure there is enough data in the file
    ++numsamples;

    MediaMetaInfo *info = (MediaMetaInfo*) OSCL_MALLOC(numsamples * sizeof(MediaMetaInfo));
    if (NULL == info)
    {
        // If we get here, an error has occurred
        ReportMP4FFParserErrorEvent(PVMFErrProcessing, NULL, NULL, NULL);
        return false;
    }

    int32 retval = iPlaybackParserObj->peekNextBundledAccessUnits(aTrackPortInfo.iTrackId, &numsamples, info);
    if (numsamples == 0)
    {
        if (retval == END_OF_TRACK)
        {
            OSCL_FREE(info);
            //set track state to send end of track
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
            return false;
        }
        else
        {
            // If we get here, an error has occurred
            OSCL_FREE(info);
            ReportMP4FFParserErrorEvent(PVMFErrProcessing, NULL, NULL, NULL);
            return false;
        }
    }

    // create a temporary clock converter to convert the timestamp
    MediaClockConverter clock_conv(*aTrackPortInfo.iClockConverter);

    clock_conv.update_clock(info[numsamples-1].ts);

    uint32 msec = clock_conv.get_converted_ts(MILLISECOND_TIMESCALE);

    // get the download progress clock time
    uint32 clock_msec32;
    bool overload = 0;
    download_progress_clock->GetCurrentTime32(clock_msec32, overload, PVMF_MEDIA_CLOCK_MSEC);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode:: P/R checkTrackPosition() Track[%d] numsamples=%d, availTS=%d, reqTS=%d, playedTS=%d", \
                     aTrackPortInfo.iTrackId, numsamples, clock_msec32, msec, (*aTrackPortInfo.iClockConverter).get_converted_ts(MILLISECOND_TIMESCALE)));

    bool status = true;
    if ((clock_msec32 - msec) > WRAP_THRESHOLD)
    {
        // This condition indicates that the read position is getting ahead of the download data
        // Note that since the computation is based on 32-bit values, it has a limitation that
        // it will not work for durations exceeding 2^32 milliseconds = 49+ days which is an acceptable
        // limit for this application.

        if (!autopaused)
        {
            // Request to be notified when to resume
            bool dlcomplete = false;
            download_progress_interface->requestResumeNotification(msec, dlcomplete);
            if (!dlcomplete)
            {
                // Change the state and report underflow event
                aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE;
                status = false;
                autopaused = true;
            }
            else
            {
                // Download has completed so ignore check and return true
                status = true;
                // Get rid of the download progress clock to disable this check
                download_progress_clock.Unbind();
            }
        }
        else
        {
            // It is already auto-paused so set to autopause state and return false
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE;
            status = false;
        }
    }

    OSCL_FREE(info);
    return status;
}

bool PVMFMP4FFParserNode::RetrieveTrackData(PVMP4FFNodeTrackPortInfo& aTrackPortInfo)
{
    // parser object is null, assert here
    OSCL_ASSERT(NULL != iPlaybackParserObj);

    if (aTrackPortInfo.iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE)
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::RetrieveTrackData() - Auto Pause"));
        return false;
    }

    // Get the track ID
    uint32 trackid = aTrackPortInfo.iTrackId;

    // Create a data buffer from pool
    int errcode = OsclErrNoResources;

    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImplOut;
    if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT)
    {
        mediaDataImplOut = aTrackPortInfo.iTextMediaDataImplAlloc->allocate(aTrackPortInfo.iTrackMaxDataSize);
    }
    else
    {
        mediaDataImplOut = aTrackPortInfo.iMediaDataImplAlloc->allocate(aTrackPortInfo.iTrackMaxDataSize);
    }

    if (mediaDataImplOut.GetRep() != NULL)
    {
        errcode = OsclErrNone;
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFMP4FFParserNode::RetrieveTrackData() No Resource Found"));
        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRACKDATAPOOLEMPTY;
        aTrackPortInfo.iTrackDataMemoryPool->notifyfreeblockavailable(aTrackPortInfo, aTrackPortInfo.iTrackMaxDataSize);    // Enable flag to receive event when next deallocate() is called on pool
        return false;
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
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFMP4FFParserNode::RetrieveTrackData() Memory allocation for media data memory pool failed"));
        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_MEDIADATAPOOLEMPTY;
        aTrackPortInfo.iMediaDataMemPool->notifyfreechunkavailable(aTrackPortInfo);     // Enable flag to receive event when next deallocate() is called on pool
        return false;
    }

    // Try creating a PVMF media frag group if H.264
    OsclSharedPtr<PVMFMediaDataImpl> mediadatafraggroup;
    if ((aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_H264_MP4) ||
            ((aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_MPEG4_AUDIO) &&
             (oIsAACFramesFragmented)))
    {
        errcode = OsclErrNoResources;
        mediadatafraggroup = aTrackPortInfo.iMediaDataGroupAlloc->allocate();
        if (mediadatafraggroup.GetRep() != NULL)
        {
            errcode = OsclErrNone;
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::RetrieveTrackData() Allocating from media data group alloc failed"));
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_MEDIADATAFRAGGROUPPOOLEMPTY;
            aTrackPortInfo.iMediaDataGroupAlloc->notifyfreechunkavailable(aTrackPortInfo);
            return false;
        }

    }

    uint32 numsamples = aTrackPortInfo.iNumSamples;

    // If download progress clock is available then make sure that we're not reading beyond the
    // end of the file
    if (download_progress_clock.GetRep())
    {
        if (!checkTrackPosition(aTrackPortInfo, numsamples))
        {
            return false;
        }
    }

    // Retrieve memory fragment to write to
    OsclRefCounterMemFrag refCtrMemFragOut;
    mediadataout->getMediaFragment(0, refCtrMemFragOut);

    // Resets GAU structure.
    oscl_memset(&iGau.buf, 0, sizeof(iGau.buf));
    oscl_memset(&iGau.info, 0, sizeof(iGau.info));
    iGau.free_buffer_states_when_done = 0;

    iGau.numMediaSamples = numsamples;
    iGau.buf.num_fragments = 1;
    iGau.buf.buf_states[0] = NULL;
    iGau.frameNum = 0;

    if ((iCPMContentType == PVMF_CPM_FORMAT_OMA2) || ((iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE) && iProtectedFile && !iCPMDecryptionDoneInplace))
    {
        iGau.buf.fragments[0].ptr = iOMA2DecryptionBuffer;
        iGau.buf.fragments[0].len = PVMP4FF_OMA2_DECRYPTION_BUFFER_SIZE;
    }
    else
    {
        if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT)
        {
            PVMFTimedTextMediaData* textmediadata = (PVMFTimedTextMediaData*) refCtrMemFragOut.getMemFrag().ptr;
            iGau.buf.fragments[0].ptr = textmediadata->iTextSample;
            iGau.buf.fragments[0].len = textmediadata->iTextSampleCapacity;
        }
        else
        {
            iGau.buf.fragments[0].ptr = refCtrMemFragOut.getMemFrag().ptr;
            iGau.buf.fragments[0].len = refCtrMemFragOut.getCapacity();
        }
    }

    uint32 currticks = OsclTickCount::TickCount();
    uint32 StartTime = OsclTickCount::TicksToMsec(currticks);

    Oscl_Vector<PVPIFFProtectedSampleDecryptionInfo, OsclMemAllocator> protectedSampleDecryptionInfoVect;
    // Retrieve the data from the parser
    int32 retval = PVMFFailure;
    if (iThumbNailMode)
    {
        if (aTrackPortInfo.iThumbSampleDone == false)
        {
            numsamples = 0;
            PVMFStatus status = PVMFSuccess;
            uint32 keySampleNum = PVMFFF_DEFAULT_THUMB_NAIL_SAMPLE_NUMBER;
            if (!iPlaybackParserObj->IsMovieFragmentsPresent())
            {
                status = FindBestThumbnailKeyFrame(trackid, keySampleNum);
            }
            if (status == PVMFSuccess)
            {
                retval = iPlaybackParserObj->getKeyMediaSampleNumAt(trackid, keySampleNum, &iGau, &protectedSampleDecryptionInfoVect);
            }
            else
            {
                // No keyframe available.
                // Go for the best possible solution if no key frames are available in stss atom.
                // Just try to retrieve the first video frame, this could result in a distorted frame
                // if first video frame is not a sync sample but it might be still better than no thumbnail.
                // Before retrieving the frame just make sure that there are samples in video track,
                // if there are no samples in the video track just report failure from here. No thumbnail possible.
                if (iPlaybackParserObj->getSampleCountInTrack(trackid) > 0)
                {
                    // Just retrieve the first video sample
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC(
                        (0, "PVMFMP4FFParserNode:RetrieveTrackData - FindBestThumbnailKeyFrame failed, best possible solution fetch the first video sample"));
                    numsamples = 1;
                    protectedSampleDecryptionInfoVect.reserve(numsamples);
                    retval = iPlaybackParserObj->getNextBundledAccessUnits(trackid, &numsamples, &iGau, &protectedSampleDecryptionInfoVect);

                }
                else
                {
                    // no sample in the video track.
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC(
                        (0, "PVMFMP4FFParserNode:RetrieveTrackData - FindBestThumbnailKeyFrame failed, No sample in video track just return EndOfTrack"));

                    aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
                    return false;
                }
            }

            if (retval == EVERYTHING_FINE || retval == END_OF_TRACK)
            {
                numsamples = 1;
                aTrackPortInfo.iThumbSampleDone = true;
            }
        }
        else
        {
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
            return false;
        }
    }
    else if ((PVMF_DATA_SOURCE_DIRECTION_REVERSE == iPlayBackDirection) &&
             (iPlaybackParserObj->getTrackMediaType(aTrackPortInfo.iTrackId) == MEDIA_TYPE_VISUAL))
    {
        uint32 numSamples = 1;
        uint32 keySampleNumber;
        if (aTrackPortInfo.iFirstFrameAfterRepositioning)
        {
            retval = iPlaybackParserObj->getPrevKeyMediaSample(iPrevSampleTS, keySampleNumber, trackid,
                     &numSamples, &iGau, &protectedSampleDecryptionInfoVect);
            iStartForNextTSSearch = iPlaybackParserObj->getMediaTimestampForCurrentSample(trackid) - 2 * iGau.info[0].ts_delta;
        }
        else
        {
            if (iStartForNextTSSearch <= 0)
            {
                retval = END_OF_TRACK;
            }
            else
            {
                retval = iPlaybackParserObj->getPrevKeyMediaSample(iStartForNextTSSearch, keySampleNumber, trackid,
                         &numSamples, &iGau, &protectedSampleDecryptionInfoVect);
                iStartForNextTSSearch = iPlaybackParserObj->getMediaTimestampForCurrentSample(trackid) - 2 * iGau.info[0].ts_delta;
            }
        }

    }
    else  if ((iPlaybackParserObj->getTrackMediaType(aTrackPortInfo.iTrackId) == MEDIA_TYPE_VISUAL)
              && (iIFrameOnlyFwdPlayback))
    {
        if (PVMF_DATA_SOURCE_DIRECTION_FORWARD == iPlayBackDirection)
        {
            iPrevSampleTS = iPlaybackParserObj->getMediaTimestampForCurrentSample(trackid);
            uint32 numSamples = 1;
            uint32 keySampleNumber;
            retval = iPlaybackParserObj->getNextKeyMediaSample(keySampleNumber, trackid,
                     &numSamples, &iGau);
        }
    }
    else
    {
        protectedSampleDecryptionInfoVect.reserve(numsamples);
        retval = iPlaybackParserObj->getNextBundledAccessUnits(trackid, &numsamples, &iGau, &protectedSampleDecryptionInfoVect);

        for (uint32 index = 0; index < numsamples; index++)
        {
            if ((iGau.frameNum + index) == iClipInfoList[iPlaybackClipIndex].iClipInfo.iFrameBOC)
            {
                iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasBOCFrame = true;
            }
            if ((iGau.frameNum + index) == iClipInfoList[iPlaybackClipIndex].iClipInfo.iFirstFrameEOC)
            {
                iClipInfoList[iPlaybackClipIndex].iClipInfo.iFramesToFollowEOC = numsamples;
                iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasEOCFrame = true;
            }
        }
    }

    if ((retval == NO_SAMPLE_IN_CURRENT_MOOF) && numsamples == 0)
    {
        // no sample was retrieved, try to retrieve in next getNextBundledAccessUnits call
        // return false without modifying the state.
        aTrackPortInfo.iNoSampleInCurrentMoof = true;
        mediadataout->~PVMFMediaData();
        return false;
    }

    currticks = OsclTickCount::TickCount();
    uint32 EndTime = OsclTickCount::TicksToMsec(currticks);

    aTrackPortInfo.iNumTimesMediaSampleRead++;

    // Determine actual size of the retrieved data by summing each sample length in GAU

    uint32 DeltaTime = EndTime - StartTime;

    if (DeltaTime >= maxTime)
    {
        maxTime = DeltaTime;
    }
    if (DeltaTime <= minTime)
    {
        minTime = DeltaTime;
    }
    sumTime += DeltaTime;

    aTrackPortInfo.iMaxTime = maxTime;
    aTrackPortInfo.iMinTime = minTime;
    aTrackPortInfo.iSumTime = sumTime;

    uint32 actualdatasize = 0;
    uint64 tsDelta = 0;
    uint32 cttsOffset = 0;
    uint32 duration_text_msec = 0;
    bool oSetNoRenderBit = false;
    bool textOnlyClip = false;
    for (uint32 i = 0; i < iGau.numMediaSamples; ++i)
    {
        actualdatasize += iGau.info[i].len;
        if ((PVMF_DATA_SOURCE_DIRECTION_REVERSE == iPlayBackDirection) &&
                (iPlaybackParserObj->getTrackMediaType(aTrackPortInfo.iTrackId) == MEDIA_TYPE_VISUAL))
        {
            // for reverse we are just using I frames so need to get delta from total differnece in time
            tsDelta += oscl_abs((int)iGau.info[0].ts - iPrevSampleTS);
            if (0 == tsDelta)
                tsDelta += iGau.info[i].ts_delta;

            iPrevSampleTS = iGau.info[0].ts;

            // in the case of backwards playback, dont render if ts greater than targetnpt.
            if (iGau.info[i].ts > aTrackPortInfo.iTargetNPTInMediaTimeScale)
            {
                oSetNoRenderBit = true;
            }
            else
            {
                //we are past the no render point, so reset to zero
                aTrackPortInfo.iTargetNPTInMediaTimeScale = 0x7FFFFFFF;
            }
        }
        else if ((PVMF_DATA_SOURCE_DIRECTION_FORWARD == iPlayBackDirection) && (iIFrameOnlyFwdPlayback))
        {
            if (iPlaybackParserObj->getTrackMediaType(aTrackPortInfo.iTrackId) == MEDIA_TYPE_VISUAL)
            {
                tsDelta += iGau.info[0].ts - iPrevSampleTS;
                if (0 == tsDelta)
                    tsDelta += iGau.info[i].ts_delta;
            }
        }
        else
        {
            tsDelta += iGau.info[i].ts_delta;

            //@FIXME" cttsOffset is being saved does not work if we read more than one video frame
            //at a time
            cttsOffset += iGau.info[i].ctts_offset;

            if (iGau.info[i].ts < aTrackPortInfo.iTargetNPTInMediaTimeScale)
            {
                oSetNoRenderBit = true;
            }
            else
            {
                //we are past the no render point, so reset to zero
                aTrackPortInfo.iTargetNPTInMediaTimeScale = 0;
            }
        }
    }

    // Save the first frame flag for output media data's random access point flag
    bool israndomaccesspt = aTrackPortInfo.iFirstFrameAfterRepositioning;
    if (aTrackPortInfo.iFirstFrameAfterRepositioning)
    {
        // If it is first frame after repositioning, media data timestamp should be
        // at the spot set when SetDataSourcePosition was processed and doesn't need to
        // consider the delta of the current frame from previous one.
        aTrackPortInfo.iFirstFrameAfterRepositioning = false;
    }

    if (retval == EVERYTHING_FINE || retval == END_OF_TRACK)
    {
        // Truly is end of track when return value is END_OF_TRACK and number of samples
        // returned is 0 or length of data is 0
        if (retval == END_OF_TRACK && (numsamples < 1 || actualdatasize == 0))
        {
            if (aTrackPortInfo.iSendBOS == true)
            {
                // If BOS has not been sent, it needs to be send before EOS.
                // E.g., In case the first call to RetrieveTrack produces END_OF_TRACK.
                aTrackPortInfo.iClockConverter->update_clock(aTrackPortInfo.iTimestamp);
                uint32 timestamp = aTrackPortInfo.iClockConverter->get_converted_ts(1000);
                if (!SendBeginOfMediaStreamCommand(aTrackPortInfo.iPortInterface, iStreamID, timestamp))
                {
                    if (aTrackPortInfo.iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDDATA)
                        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DESTFULL;
                }
                else
                {
                    aTrackPortInfo.iSendBOS = false;
                }
            }
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
            // End of track reached so send EOS media command
            if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT)
            {
                iEOTForTextSentToMIO = true;
                iSetTextSampleDurationZero = false;
            }
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::RetrieveTrackData() EOS LOC 3 track ID %d", aTrackPortInfo.iTrackId));
            return false;
        }

        if ((iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE) && (iProtectedFile == true))
        {
            if (iDecryptionInterface != NULL)
            {
                if (iCPMDecryptionDoneInplace)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "MP4FF PIFF Decryption  numsamples[%u] actualdatasize[%u]", numsamples, actualdatasize));
                    uint32 sampleOffset = 0;
                    for (uint32 ii = 0; ii < numsamples; ii++)
                    {
                        PVPIFFProtectedSampleDecryptionInfo* protectedSampleDecryptionInfo = &protectedSampleDecryptionInfoVect[ii];
                        const uint32 payloadSize = iGau.getInfo(ii)->len;
                        uint8* srcPtr = OSCL_STATIC_CAST(uint8*, iGau.buf.fragments[0].ptr) + sampleOffset;
                        sampleOffset += payloadSize;
                        const uint8* initializationVector = NULL;
                        uint8 initializationVectorSz =  protectedSampleDecryptionInfo->GetInitializationVector(initializationVector);

                        if (initializationVectorSz == CPM_SUPPORTED_IV_SZ)
                        {
                            PVMFCPMPluginWMDRMDecryptContext decrpytContext(protectedSampleDecryptionInfo->GetKeyUsedForEncryption(), initializationVector, CPM_SUPPORTED_IV_SZ);

                            //Inplace decryption, maybe we can skip the createion of unnecessary out buffers created
                            //payload may have mix of clear/encrypted content. Therefore, lets make call only for the
                            //encrypted bytes.
                            uint32 sampleEntriesCnt = 0;
                            bool sampleEntriesAvl = protectedSampleDecryptionInfo->GetSampleEntriesCnt(sampleEntriesCnt);
                            bool oDecryptRet  = false;
                            if (!sampleEntriesAvl)
                            {
                                oDecryptRet =
                                    iDecryptionInterface->DecryptAccessUnit(srcPtr,
                                                                            payloadSize,
                                                                            aTrackPortInfo.iTrackId,
                                                                            0,
                                                                            &decrpytContext);
                            }
                            else
                            {
                                uint32 subSampleOffset = 0; //offset into sample buffer i.e. srcPtr
                                if (sampleEntriesCnt > 0)
                                {
                                    uint32 encryptedDataLen = 0; //offset into buffer
                                    uint8* buffer = OSCL_ARRAY_NEW(uint8, payloadSize);
                                    //initialize encryptedDataOffset and create buffer with encrypted data
                                    for (uint32 ii = 0; ii < sampleEntriesCnt; ii++)
                                    {
                                        EntryInfo* entryInfo = NULL;
                                        protectedSampleDecryptionInfo->GetEntryInfo(ii, entryInfo);
                                        uint16 bytesToSkip = entryInfo->GetClearBytesCount();
                                        uint16 encryptedSubSamplePayloadSz = entryInfo->GetEncryptedBytesCount();
                                        subSampleOffset += bytesToSkip;
                                        uint8* payloadToDecryptPtr = srcPtr + subSampleOffset;
                                        uint8* destBufferPtr = buffer + encryptedDataLen;
                                        oscl_memcpy(destBufferPtr, payloadToDecryptPtr, encryptedSubSamplePayloadSz);
                                        encryptedDataLen += encryptedSubSamplePayloadSz; //inplace decryption so output sz same as input sz
                                        subSampleOffset += encryptedSubSamplePayloadSz;
                                    }
                                    oDecryptRet = iDecryptionInterface->DecryptAccessUnit(buffer,
                                                  encryptedDataLen,
                                                  aTrackPortInfo.iTrackId,
                                                  0,
                                                  &decrpytContext);
                                    //Reassemble the decrypted payload in src buffer
                                    uint8* pSrcBuffOffset = srcPtr;
                                    uint8* pIntermediateBufferOffset = buffer;
                                    for (uint32 jj = 0; jj < sampleEntriesCnt; jj++)
                                    {
                                        EntryInfo* entryInfo = NULL;
                                        protectedSampleDecryptionInfo->GetEntryInfo(jj, entryInfo);
                                        uint16 bytesToSkip = entryInfo->GetClearBytesCount();
                                        uint16 encryptedSubSamplePayloadSz = entryInfo->GetEncryptedBytesCount();
                                        pSrcBuffOffset += bytesToSkip;
                                        oscl_memcpy(pSrcBuffOffset, pIntermediateBufferOffset, encryptedSubSamplePayloadSz);
                                        pSrcBuffOffset += encryptedSubSamplePayloadSz;
                                        pIntermediateBufferOffset += encryptedSubSamplePayloadSz;
                                    }
                                    OSCL_ARRAY_DELETE(buffer);
                                    buffer = NULL;
                                }
                            }

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG, (0, "MP4FF Before PIFF Decryption payloadSize[%u] After decryption payloadsize [%u] oDecryptRet[%d]", iGau.getInfo(ii)->len, payloadSize, oDecryptRet));
                            if (oDecryptRet != true)
                            {
                                return PVMFFailure;
                            }
                        }
                        else
                        {
                            //Todo: Return err to the up
                            OSCL_LEAVE(PVMFErrNotSupported);
                        }
                    }
                }
                else
                {
                    /*We support only Playready among the MS protected DRM*/
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "MP4FF support only Playready among the MS protected DRM"));
                    OSCL_ASSERT(false);
                }
            }
            else
            {
                aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ERROR;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackData Text sample is less than 2 bytes (size=%d). ReportMP4FFParserErrorEvent(PVMFErrCorrupt)", actualdatasize));
                ReportMP4FFParserErrorEvent(PVMFErrInvalidState);
                return false;
            }
        }
        else if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
        {
            if (iDecryptionInterface != NULL)
            {
                uint32 inSize = actualdatasize;
                uint32 outSize = actualdatasize;

                uint32 currticks = OsclTickCount::TickCount();
                uint32 StartTime;
                StartTime = OsclTickCount::TicksToMsec(currticks);

                if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT)
                {
                    PVMFTimedTextMediaData* textmediadata = (PVMFTimedTextMediaData*) refCtrMemFragOut.getMemFrag().ptr;
                    iDecryptionInterface->DecryptAccessUnit(iOMA2DecryptionBuffer,
                                                            inSize,
                                                            textmediadata->iTextSample,
                                                            outSize, trackid);
                }
                else
                {
                    uint8* memfrag = (uint8*)(refCtrMemFragOut.getMemFrag().ptr);

                    iDecryptionInterface->DecryptAccessUnit(iOMA2DecryptionBuffer,                                                            inSize,
                                                            memfrag,
                                                            outSize, trackid);
                }
                currticks = OsclTickCount::TickCount();
                uint32 EndTime;
                EndTime = OsclTickCount::TicksToMsec(currticks);

                actualdatasize = outSize;
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::RetrieveTrackData - DecryptAccessUnit - Mime=%s, TrackID=%d, InSize=%d, OutSize=%d, DecryptTime=%d", aTrackPortInfo.iMimeType.get_cstr(), aTrackPortInfo.iTrackId, inSize, outSize, EndTime - StartTime));
            }
            else
            {
                aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ERROR;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackData Text sample is less than 2 bytes (size=%d). ReportMP4FFParserErrorEvent(PVMFErrCorrupt)", actualdatasize));
                ReportMP4FFParserErrorEvent(PVMFErrInvalidState);
                return false;
            }
        }

        // Retrieve the output media data impl to set some properties
        OsclSharedPtr<PVMFMediaDataImpl> media_data_impl;
        mediadataout->getMediaDataImpl(media_data_impl);

        if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT)
        {
            PVMFTimedTextMediaData* textmediadata = (PVMFTimedTextMediaData*) refCtrMemFragOut.getMemFrag().ptr;

            // Check if the text sample duration read by parser is zero, then send EOT.
            textmediadata->iTextSampleDuration = iGau.info[0].ts_delta;
            if (textmediadata->iTextSampleDuration == 0)
            {
                iSetTextSampleDurationZero = true;
                iEOTForTextSentToMIO = true;
                aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
                return false;
            }

            // Adjust the duration of Text Sample (if clip is not textOnlyClip) as per new time-stamp.
            // Retrieve timestamp and convert to milliseconds

            textOnlyClip = true;
            for (uint32 i = 0; i < iNodeTrackPortList.size(); i++)
            {
                if ((iPlaybackParserObj->getTrackMediaType(iNodeTrackPortList[i].iTrackId) == MEDIA_TYPE_VISUAL) ||
                        (iPlaybackParserObj->getTrackMediaType(iNodeTrackPortList[i].iTrackId) == MEDIA_TYPE_AUDIO))
                {
                    textOnlyClip = false;
                }
            }

            if (!textOnlyClip)
            {
                if (iGau.info[0].ts_delta >= aTrackPortInfo.iTimestamp)
                {
                    iGau.info[0].ts_delta = iGau.info[0].ts_delta - (uint32)aTrackPortInfo.iTimestamp;
                }
            }

            aTrackPortInfo.iClockConverter->set_clock(iGau.info[0].ts_delta, 0);
            duration_text_msec = aTrackPortInfo.iClockConverter->get_converted_ts(1000);

            aTrackPortInfo.iClockConverter->set_clock(aTrackPortInfo.iTimestamp, 0);
            // Set sample duration, In media timescale.
            // The duration will always be reduced from text sample after reposition. Value
            // of adjustment would be different and is calculated in DoSetDataSourcePosition.
            if (iTextInvalidTSAfterReposition)
            {
                iTextInvalidTSAfterReposition = false;
                MediaClockConverter mcc(1000, iDelayAddToNextTextSample);
                uint64 durationDelayInMediaTimeScale =
                    mcc.get_converted_ts64(iPlaybackParserObj->getTrackMediaTimescale(trackid));
                if (tsDelta > durationDelayInMediaTimeScale)
                {
                    iGau.info[0].ts_delta -= (uint32)durationDelayInMediaTimeScale;
                    tsDelta -= (uint32)durationDelayInMediaTimeScale;
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode - RetrieveTrackData(): iTextInvalidTSAfterReposition true"));
                }
                else
                {
                    iGau.info[0].ts_delta = 0;
                    tsDelta = 0;
                }

                textmediadata->iTextSampleDuration = iGau.info[0].ts_delta;
            }

            // Set sample timestamp in NPT
            aTrackPortInfo.iClockConverter->set_clock(iGau.info[0].ts, 0);
            textmediadata->iTextSampleTimestampNPT = aTrackPortInfo.iClockConverter->get_converted_ts(1000);

            // Set the length of the text sample
            textmediadata->iTextSampleLength = actualdatasize;
            if (actualdatasize < 2)
            {
                // Sample corrupt
                // A text sample should at least be 2 bytes in size for the text string size
                aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ERROR;
                PVUuid erruuid = PVMFFileFormatEventTypesUUID;
                int32 errcode = PVMFFFErrInvalidData;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackData Text sample is less than 2 bytes (size=%d). ReportMP4FFParserErrorEvent(PVMFErrCorrupt)", actualdatasize));

                ReportMP4FFParserErrorEvent(PVMFErrCorrupt, NULL, &erruuid, &errcode);
                return false;
            }

            // Return the unused space from mempool back
            if (textmediadata->iTextSampleCapacity > actualdatasize)
            {
                // Need to go to the resizable memory pool and free some memory
                uint32 bytestofree = textmediadata->iTextSampleCapacity - actualdatasize;
                // Update the capacity
                media_data_impl->setCapacity(media_data_impl->getCapacity() - bytestofree);
                textmediadata->iTextSampleCapacity -= bytestofree;
                // Set buffer size
                mediadataout->setMediaFragFilledLen(0, refCtrMemFragOut.getCapacity() - bytestofree);
                //resize the fragment
                aTrackPortInfo.iTextMediaDataImplAlloc->ResizeMemoryFragment(mediaDataImplOut);
            }

            OsclBinIStreamBigEndian textSampleStream;
            textSampleStream.Attach((void *)(textmediadata->iTextSample), textmediadata->iTextSampleLength);

            uint16 textSampleLengthFromSample = 0;
            textSampleStream >> textSampleLengthFromSample;

            // Check that the text sample length is less than (total sample length -2)
            if (textSampleLengthFromSample > (textmediadata->iTextSampleLength - 2))
            {
                // Sample corrupt
                aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ERROR;
                PVUuid erruuid = PVMFFileFormatEventTypesUUID;
                int32 errcode = PVMFFFErrInvalidData;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackData Text sample length is bigger than (total sample size-2) (len=%d, total=%d). ReportMP4FFParserErrorEvent(PVMFErrCorrupt)", textSampleLengthFromSample, textmediadata->iTextSampleLength));

                ReportMP4FFParserErrorEvent(PVMFErrCorrupt, NULL, &erruuid, &errcode);
                return false;
            }

            //Set string format type, skip first two bytes, they represent
            //text sample length
            uint8 byte1 = 0;
            uint8 byte2 = 0;
            if (textmediadata->iTextSampleLength >= 4)
            {
                // Only check the marker if there is at least 4 bytes in the text sample
                byte1 = textmediadata->iTextSample[2];
                byte2 = textmediadata->iTextSample[3];
            }

            if ((byte1 == PVMF_TIMED_TEXT_UTF_16_MARKER_BYTE_1) &&
                    (byte2 == PVMF_TIMED_TEXT_UTF_16_MARKER_BYTE_2))
            {
                textmediadata->iFormatType = PVMF_TIMED_TEXT_STRING_FORMAT_UTF16;
                // Skip size & utf-16 marker
                // By being within this IF block, the text sample length should be greater or equal to 4.
                OSCL_ASSERT(textmediadata->iTextSampleLength >= 4);
                if (textmediadata->iTextSampleLength == 4)
                {
                    // If there are only 4 bytes and it is UTF-16, the text string is not available
                    textmediadata->iTextSample = NULL;
                }
                else
                {
                    textmediadata->iTextSample += 4;
                }
                textmediadata->iTextSampleLength -= 4;
                textmediadata->iTextSampleCapacity -= 4;
                // text sample length includes the 0xFEFF marker
                textSampleLengthFromSample -= 2;
            }
            else
            {
                textmediadata->iFormatType = PVMF_TIMED_TEXT_STRING_FORMAT_UTF8;
                // Skip size only
                // By being within this IF block, the text sample length should be greater or equal to 2.
                OSCL_ASSERT(textmediadata->iTextSampleLength >= 2);
                if (textmediadata->iTextSampleLength == 2)
                {
                    textmediadata->iTextSample = NULL;
                }
                else
                {
                    textmediadata->iTextSample += 2;
                }
                textmediadata->iTextSampleLength -= 2;
                textmediadata->iTextSampleCapacity -= 2;
            }

            textmediadata->iTextStringLengthInBytes = textSampleLengthFromSample;
        }
        else
        {
            // Set buffer size
            mediadataout->setMediaFragFilledLen(0, actualdatasize);
            media_data_impl->setCapacity(actualdatasize);
            // Return the unused space from mempool back
            if (refCtrMemFragOut.getCapacity() > actualdatasize)
            {
                // Need to go to the resizable memory pool and free some memory
                aTrackPortInfo.iMediaDataImplAlloc->ResizeMemoryFragment(mediaDataImplOut);
            }
        }

        // Set the random access point flag if this is the first frame after repositioning
        uint32 markerInfo = 0;
        if (israndomaccesspt)
        {
            markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT;
        }

        // Save the media data in the trackport info
        aTrackPortInfo.iMediaData = mediadataout;

        // Retrieve duration and convert to milliseconds
        aTrackPortInfo.iClockConverter->set_clock(tsDelta, 0);

        // Set duration bit in marker info
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_DURATION_AVAILABLE_BIT;
        // Set M bit to 1 always - MP4 FF only outputs complete frames
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;
        // Set No Render bit where applicable
        if (oSetNoRenderBit == true)
        {
            markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_NO_RENDER_BIT;
        }
        if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_H264_MP4)
        {
            // Set End-of-NAL bit to 1 always - no NAL fragmentation for now
            markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_END_OF_NAL_BIT;
        }

        media_data_impl->setMarkerInfo(markerInfo);

        // Retrieve timestamp and convert to milliseconds
        aTrackPortInfo.iClockConverter->set_clock((aTrackPortInfo.iTimestamp + cttsOffset), 0);
        uint32 timestamp = aTrackPortInfo.iClockConverter->get_converted_ts(1000);

        // Set the media data's timestamp
        aTrackPortInfo.iMediaData->setTimestamp(timestamp);

        // Set the media data sequence number
        aTrackPortInfo.iMediaData->setSeqNum(aTrackPortInfo.iSeqNum);

        //for logging
        MediaClockConverter mcc(iPlaybackParserObj->getTrackMediaTimescale(aTrackPortInfo.iTrackId));
        mcc.update_clock(iGau.info[0].ts);

        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::RetrieveTrackData - Mime=%s, TrackID=%d, Size=%d, NPT=%d, MediaTS=%d, SEQNUM=%d, DUR=%d, Marker=0x%x", aTrackPortInfo.iMimeType.get_cstr(), aTrackPortInfo.iTrackId, actualdatasize, mcc.get_converted_ts(1000), timestamp, aTrackPortInfo.iSeqNum, tsDelta, markerInfo));

        // Advance aTrackPortInfo.iTimestamp to the timestamp at the end of the
        // all samples in the group, i.e. current duration.
        aTrackPortInfo.iTimestamp += tsDelta;
        aTrackPortInfo.iClockConverter->set_clock(aTrackPortInfo.iTimestamp, 0);
        uint32 timestamp_next = aTrackPortInfo.iClockConverter->get_converted_ts(1000);
        uint32 duration_msec = timestamp_next - timestamp;
        if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT)
        {
            aTrackPortInfo.iMediaData->setDuration(duration_text_msec);
        }
        else
        {
            aTrackPortInfo.iMediaData->setDuration(duration_msec);
        }
        // increment media data sequence number
        aTrackPortInfo.iSeqNum++;
    }
    else if (retval == READ_FAILED)
    {
        // Data could not be read from file
        PVUuid erruuid = PVMFFileFormatEventTypesUUID;
        int32 errcode = PVMFFFErrFileRead;
        ReportMP4FFParserInfoEvent(PVMFInfoContentTruncated, NULL, &erruuid, &errcode);
        // Treat this as EOS
        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
        return false;
    }
    else if (retval == INSUFFICIENT_DATA)
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::RetrieveTrackData - Insufficient Data - Mime=%s, TrackID=%d",
                                             aTrackPortInfo.iMimeType.get_cstr(), aTrackPortInfo.iTrackId));
        /*
         * We have run out data during playback. This could mean a few things:
         * - A pseudo streaming session that has hit a data limit
         * - A regular playback session with a sample that has a bad offset.
         */
        if (download_progress_interface != NULL)
        {
            if ((NULL == iDataStreamInterface) || (0 == iDataStreamInterface->QueryBufferingCapacity()))
            {
                //not progressive streaming
                if (iDownloadComplete)
                {
                    // Data could not be read from file
                    PVUuid erruuid = PVMFFileFormatEventTypesUUID;
                    int32 errcode = PVMFFFErrFileRead;
                    ReportMP4FFParserInfoEvent(PVMFInfoContentTruncated, NULL, &erruuid, &errcode);
                    // Treat this as EOS
                    aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
                    return false;
                }
            }

            if (!autopaused)
            {
                uint32 requestedTimestamp = 0;
                // if parser library reported Insufficient data during playback or after seek,
                // peek the next sample and try to get the timestamp of the sample to be passed
                // to PE node for requesting a resume notification.
                uint32 numSamples = 1;
                MediaMetaInfo info;

                retval = iPlaybackParserObj->peekNextBundledAccessUnits(aTrackPortInfo.iTrackId, &numSamples, &info);
                if (((retval == EVERYTHING_FINE) || (retval == END_OF_TRACK))
                        && (numSamples > 0))
                {
                    // we peek the next sample even in Underflow during playback use-case so as to pass the exact
                    // timestamp to PE node. It can happen aTrackPortInfo.iTimestamp will not represent exact timestamp
                    // if there has been many seek requests before this Underflow.
                    aTrackPortInfo.iClockConverter->set_clock(info.ts, 0);
                }
                else
                {
                    // if the parser library could not peek the next sample, so just use the
                    // current sample timestamp to be passed to the PE node.
                    aTrackPortInfo.iClockConverter->set_clock(aTrackPortInfo.iTimestamp, 0);
                }
                requestedTimestamp = aTrackPortInfo.iClockConverter->get_converted_ts(1000);

                download_progress_interface->requestResumeNotification(requestedTimestamp, iDownloadComplete);
            }
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE;
            autopaused = true;
            TOsclFileOffset currentFileSize = 0;
            iPlaybackParserObj->GetCurrentFileSize(currentFileSize);
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::RetrieveTrackData() - Auto Pause Triggered, TS = %d, FileSize=%d",
                                                 Oscl_Int64_Utils::get_uint64_lower32(aTrackPortInfo.iTimestamp), currentFileSize));

            // check if content is poorly interleaved only for PS
            // After repositioning, parser will get INSUFFICIENT_DATA immediately and then the poorlyinterleavedcontent event logic will be excercised.
            // To make sure that the reposition behaviour and the playback without interruption behavior are consistent, disable the check for first INSUFFICIENT_DATA after repositioning.
            if ((NULL != iDataStreamInterface) && (iPoorlyInterleavedContentEventSent == false) && (!israndomaccesspt))
            {
                uint32 cacheSize = 0;
                cacheSize = iDataStreamInterface->QueryBufferingCapacity();
                if (cacheSize)
                {
                    TOsclFileOffset *trackOffset = NULL;
                    trackOffset = (TOsclFileOffset*) OSCL_MALLOC(iNodeTrackPortList.size() * sizeof(TOsclFileOffset));
                    if (trackOffset == NULL)
                    {
                        return false;
                    }
                    int32 retval = EVERYTHING_FINE;
                    int32 err = 0;
                    OSCL_TRY(err,
                             for (uint32 i = 0; i < iNodeTrackPortList.size(); i++)
                {
                    // Peek the next sample to get the duration of the last sample
                    uint32 numSamples = iNodeTrackPortList[i].iNumSamples;
                        MediaMetaInfo* info = NULL;
                        info = (MediaMetaInfo*) OSCL_MALLOC(numSamples * sizeof(MediaMetaInfo));
                        if (info == NULL)
                        {
                            OSCL_FREE(trackOffset);
                            return false;
                        }
                        retval = iPlaybackParserObj->peekNextBundledAccessUnits(iNodeTrackPortList[i].iTrackId, &numSamples, info);
                        if (((retval == EVERYTHING_FINE) || (retval == END_OF_TRACK))
                                && (numSamples > 0))
                        {
                            TOsclFileOffset offset = 0;

                            if ((iPlaybackParserObj->getOffsetByTime(iNodeTrackPortList[i].iTrackId, info[numSamples-1].ts, &offset, 0)) == EVERYTHING_FINE)
                            {
                                trackOffset[i] = offset;
                            }
                            else
                            {
                                trackOffset[i] = 0x7FFFFFFF;
                                aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
                                PVUuid erruuid = PVMFFileFormatEventTypesUUID;
                                int32 errcode = PVMFFFErrFileRead;
                                ReportMP4FFParserErrorEvent(PVMFErrResource, NULL, &erruuid, &errcode);
                                PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::RetrieveTrackData - getOffsetByTime Failed - TrackId=%d, TS=%d",  iNodeTrackPortList[i].iTrackId, Oscl_Int64_Utils::get_uint64_lower32(info[numSamples-1].ts)));
                                OSCL_FREE(info);
                                OSCL_FREE(trackOffset);
                                return false;
                            }
                        }
                        else
                        {
                            trackOffset[i] = 0x7FFFFFFF;
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackData Peeking next sample failed while determining the min timestamp after repositioning"));
                        }
                        OSCL_FREE(info);
                    }
                            );
                    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);

                    TOsclFileOffset maxOffsetDiff = 0;
                    for (uint32 ii = 0; ii < iNodeTrackPortList.size(); ii++)
                    {
                        if (trackOffset[ii] != 0x7FFFFFFF)
                        {
                            TOsclFileOffset offsetDiff = 0;
                            for (uint32 j = ii + 1; j < iNodeTrackPortList.size(); j++)
                            {
                                if (trackOffset[j] != 0x7FFFFFFF)
                                {
                                    if (trackOffset[ii] > trackOffset[j])
                                    {
                                        offsetDiff = trackOffset[ii] - trackOffset[j];
                                    }
                                    else
                                    {
                                        offsetDiff = trackOffset[j] - trackOffset[ii];
                                    }
                                }

                                if (offsetDiff > maxOffsetDiff)
                                {
                                    maxOffsetDiff = offsetDiff;
                                }
                            }
                        }
                    }

                    if (maxOffsetDiff > (TOsclFileOffset)cacheSize)
                    {
                        // The content is poorly interleaved.
                        ReportMP4FFParserInfoEvent(PVMFInfoPoorlyInterleavedContent);
                        iPoorlyInterleavedContentEventSent = true;
                        // @TODO - For now, don't treat this as EOS.
                        // aTrackPortInfo.iState=PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
                    }
                    OSCL_FREE(trackOffset);
                }
            }

            return false;
        }
        else
        {
            TOsclFileOffset offset = 0;
#if PVMP4FF_UNDERFLOW_REQUST_READSIZE_NOTIFICATION_PER_TRACK
            PVMFStatus status = GetFileOffsetForAutoResume(offset, &aTrackPortInfo);
#else
            PVMFStatus status = GetFileOffsetForAutoResume(offset);
#endif
            if (status == PVMFSuccess)
            {
                //Request for callback
                MP4_ERROR_CODE retVal =
                    iPlaybackParserObj->RequestReadCapacityNotification(*this, offset);
                if (retVal == EVERYTHING_FINE)
                {
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::RetrieveTrackData - RequestReadCapacityNotification - Mime=%s, TrackID=%d, offset=%d",
                                                         aTrackPortInfo.iMimeType.get_cstr(), aTrackPortInfo.iTrackId, offset));
                    if (iDownloadComplete == false)
                    {
                        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_INSUFFICIENTDATA;
                        if (CheckForUnderFlow(&aTrackPortInfo) == PVMFSuccess && iUnderFlowEventReported == false)
                        {
                            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE;
                            autopaused = true;
                            if (ReportUnderFlow() != PVMFSuccess)
                            {
                                return false;
                            }
                        }
                    }
                    else
                    {
                        // Data could not be read from file
                        PVUuid erruuid = PVMFFileFormatEventTypesUUID;
                        int32 errcode = PVMFFFErrFileRead;
                        ReportMP4FFParserInfoEvent(PVMFInfoContentTruncated, NULL, &erruuid, &errcode);
                        // Treat this as EOS
                        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
                        return false;
                    }
                }
                else
                {
                    PVUuid erruuid = PVMFFileFormatEventTypesUUID;
                    int32 errcode = PVMFFFErrFileRead;

                    ReportMP4FFParserInfoEvent(PVMFInfoContentTruncated, NULL, &erruuid, &errcode);
                    // Treat this as EOS
                    aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
                }
                return false;
            }
            // Data could not be read from file
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ERROR;
            PVUuid erruuid = PVMFFileFormatEventTypesUUID;
            int32 errcode = PVMFFFErrFileRead;
            ReportMP4FFParserErrorEvent(PVMFErrResource, NULL, &erruuid, &errcode);
            return false;
        }
    }
    else if (retval == INSUFFICIENT_BUFFER_SIZE)
    {
        /* Reset the actual size */
        if (actualdatasize > aTrackPortInfo.iTrackMaxDataSize)
        {
            /* We will attempt to retrieve this sample again with a new max size */
            aTrackPortInfo.iTrackMaxDataSize = actualdatasize;
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRACKMAXDATASIZE_RESIZE;
            return true;
        }
        /* Error */
        return false;
    }
    else
    {
        // Parsing error
        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_ERROR;
        PVUuid erruuid;
        int32 errcode;
        if (!MapMP4ErrorCodeToEventCode(retval, erruuid, errcode))
        {
            erruuid = PVMFFileFormatEventTypesUUID;
            errcode = PVMFFFErrInvalidData;
        }
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::RetrieveTrackData ReportMP4FFParserErrorEvent(PVMFErrCorrupt)"));

        ReportMP4FFParserErrorEvent(PVMFErrCorrupt, NULL, &erruuid, &errcode);
        return false;
    }

    if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_H264_MP4)
    {
        // Check that the media frag group has been allocated
        OSCL_ASSERT(mediadatafraggroup.GetRep() != NULL);
        if (GenerateAVCNALGroup(aTrackPortInfo, mediadatafraggroup) == false)
        {
            //This means that this AVC frame is corrupt, do not send it downstream
            //We do not need to declare EOS here, let us keep going
            //may be succeeding samples are ok
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SKIP_CORRUPT_SAMPLE;
            return false;
        }
    }
    else if ((aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_MPEG4_AUDIO)
             && (oIsAACFramesFragmented))
    {
        // Check that the media frag group has been allocated
        OSCL_ASSERT(mediadatafraggroup.GetRep() != NULL);
        bool ok = GenerateAACFrameFrags(aTrackPortInfo, mediadatafraggroup);
        if (!ok)
        {
            return false;
        }
    }
    else if (aTrackPortInfo.iFormatTypeInteger == PVMF_MP4_PARSER_NODE_3GPP_TIMED_TEXT)
    {
        // Check if the text sample entry needs to be set
        PVMFTimedTextMediaData* textmediadata = (PVMFTimedTextMediaData*) refCtrMemFragOut.getMemFrag().ptr;
        UpdateTextSampleEntry(aTrackPortInfo, iGau.info[0].sample_info, *textmediadata);
    }

    // should be returning true here
    // there is a data msg waiting to be sent
    // if gapless, check if BOC and/or EOC has to be sent
    // if the whole clip fits inside the frag, both BOC and EOC have to be sent
    if (iClipInfoList[iPlaybackClipIndex].iClipInfo.iSendBOC && iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasBOCFrame)
    {
        // first frame and gapless
        // send BOC msg downstream
        if (!SendBeginOfClipCommand(aTrackPortInfo))
        {
            // failed to send BOC
            // data is already in aTrackPortInfo.iMediaData
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDBOC;
            return false;
        }
    }

    if (iClipInfoList[iPlaybackClipIndex].iClipInfo.iSendEOC && iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasEOCFrame)
    {
        // first frame containing zero padding and gapless
        // send EOC msg downstream
        if (!SendEndOfClipCommand(aTrackPortInfo))
        {
            // failed to send EOC
            // data is already in aTrackPortInfo.iMediaData
            aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDEOC;
            return false;
        }
    }

    return true;
}


bool PVMFMP4FFParserNode::SendTrackData(PVMP4FFNodeTrackPortInfo& aTrackPortInfo)
{
    if (iPortDataLog)
    {
        LogMediaData(aTrackPortInfo.iMediaData, aTrackPortInfo.iPortInterface);
    }

    // if going reverse, dump all non-video data.
    if ((iParseVideoOnly || iIFrameOnlyFwdPlayback) && (iPlaybackParserObj->getTrackMediaType(aTrackPortInfo.iTrackId) != MEDIA_TYPE_VISUAL))
    {
        // Don't need the ref to iMediaData so unbind it
        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK;
        aTrackPortInfo.iMediaData.Unbind();
        return false;
    }

    // Set the track specific config info
    aTrackPortInfo.iMediaData->setFormatSpecificInfo(aTrackPortInfo.iFormatSpecificConfig);

    aTrackPortInfo.iMediaData->setStreamID(iStreamID);

    // Send frame to downstream node via port
    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaMsg(mediaMsgOut, aTrackPortInfo.iMediaData);
    if (aTrackPortInfo.iPortInterface->QueueOutgoingMsg(mediaMsgOut) != PVMFSuccess)
    {
        aTrackPortInfo.iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DESTFULL;
        return false;
    }

    // Don't need the ref to iMediaData so unbind it
    aTrackPortInfo.iMediaData.Unbind();
    return true;
}

bool PVMFMP4FFParserNode::GenerateAACFrameFrags(PVMP4FFNodeTrackPortInfo& aTrackPortInfo, OsclSharedPtr<PVMFMediaDataImpl>& aMediaFragGroup)
{
    OSCL_ASSERT(aTrackPortInfo.iMediaData.GetRep() != NULL);
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataIn;
    if (aTrackPortInfo.iMediaData->getMediaDataImpl(mediaDataIn) == false)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::GenerateAACFrameFrags() Retrieving media data impl failed"));
        return false;
    }

    OSCL_ASSERT(aMediaFragGroup.GetRep() != NULL);
    aMediaFragGroup->setMarkerInfo(mediaDataIn->getMarkerInfo());

    OsclRefCounterMemFrag memFragIn;
    aTrackPortInfo.iMediaData->getMediaFragment(0, memFragIn);
    OsclRefCounter* refCntIn = memFragIn.getRefCounter();

    uint8* sample = (uint8*)(memFragIn.getMemFrag().ptr);
    int32 samplesize = (int32)(memFragIn.getMemFrag().len);

    OsclBinIStreamBigEndian sampleStream;
    sampleStream.Attach(memFragIn.getMemFrag().ptr, memFragIn.getMemFrag().len);

    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::GenerateAACFrameFrags - SeqNum=%d, samplesize=%d, TS=%d, PTR=0x%x, RC=%d", aTrackPortInfo.iMediaData->getSeqNum(), samplesize, aTrackPortInfo.iMediaData->getTimestamp(), sample, memFragIn.getRefCounter()->getCount()));

    MediaMetaInfo *iInfo = NULL;
    bool appended_data = false;
    uint32 ii = 0;

    // While we still have data to append AND we haven't run out of media samples
    while ((samplesize > 0) && (ii < iGau.numMediaSamples))
    {
        iInfo = iGau.getInfo(ii);
        if (iInfo)
        {
            if (iInfo->len > 0)
            {
                // Subtract the current sample from the total size
                samplesize -= iInfo->len;

                // Create memory frag to append
                OsclMemoryFragment memFrag;
                memFrag.ptr = sample;
                memFrag.len = iInfo->len;

                // Append the frag to the group
                refCntIn->addRef();
                OsclRefCounterMemFrag refCountMemFragOut(memFrag, refCntIn, 0);
                aMediaFragGroup->appendMediaFragment(refCountMemFragOut);

                sampleStream.seekFromCurrentPosition(memFrag.len);
                sample += memFrag.len;

                // Set flag to show that that we have appended at least one frag
                appended_data = true;

            }  // End of if (iInfo->len > 0)
        } // End of if (iInfo) not NULL
        ii++;
        iInfo = NULL;
    }

    if (appended_data)    // If we have appended at least one frag to this group, then package and send
    {
        PVMFSharedMediaDataPtr aacFragGroup;
        aacFragGroup = PVMFMediaData::createMediaData(aMediaFragGroup, aTrackPortInfo.iMediaData->getMessageHeader());
        aacFragGroup->setSeqNum(aTrackPortInfo.iMediaData->getSeqNum());
        aacFragGroup->setTimestamp(aTrackPortInfo.iMediaData->getTimestamp());
        aacFragGroup->setFormatSpecificInfo(aTrackPortInfo.iFormatSpecificConfig);

        // Replace the track's output media data with the frag group one
        aTrackPortInfo.iMediaData = aacFragGroup;

        // Return success
        return true;
    }
    else
    {
        return false; // No frag's appended ...
    }
}

bool PVMFMP4FFParserNode::GenerateAVCNALGroup(PVMP4FFNodeTrackPortInfo& aTrackPortInfo, OsclSharedPtr<PVMFMediaDataImpl>& aMediaFragGroup)
{
    OSCL_ASSERT(aTrackPortInfo.iMediaData.GetRep() != NULL);
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataIn;
    if (aTrackPortInfo.iMediaData->getMediaDataImpl(mediaDataIn) == false)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::GenerateAVCNALGroup() Retrieving media data impl failed"));
        return false;
    }

    OSCL_ASSERT(aMediaFragGroup.GetRep() != NULL);

    // Set End-of-NAL bit to 1 always - no NAL fragmentation for now
    uint32  markerInfo = (mediaDataIn->getMarkerInfo()) | PVMF_MEDIA_DATA_MARKER_INFO_END_OF_NAL_BIT;
    aMediaFragGroup->setMarkerInfo(markerInfo);

    OsclRefCounterMemFrag memFragIn;
    aTrackPortInfo.iMediaData->getMediaFragment(0, memFragIn);
    OsclRefCounter* refCntIn = memFragIn.getRefCounter();

    uint8* sample = (uint8*)(memFragIn.getMemFrag().ptr);
    int32 samplesize = (int32)(memFragIn.getMemFrag().len);
    uint32 nallengthsize = iPlaybackParserObj->getAVCNALLengthSize(aTrackPortInfo.iTrackId);

    OsclBinIStreamBigEndian sampleStream;
    sampleStream.Attach(memFragIn.getMemFrag().ptr, memFragIn.getMemFrag().len);

    int32 numNAL = 0;
    while (samplesize > 0)
    {
        int32 nallen = 0;
        if (GetAVCNALLength(sampleStream, nallengthsize, nallen))
        {
            sample += nallengthsize;
            samplesize -= nallengthsize;

            if ((nallen < 0) || (nallen > samplesize))
            {
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC_AVC((0, "PVMFMP4FFParserNode::GenerateAVCNALGroup - Corrupt Sample - SeqNum=%d, SampleSize=%d, NALSize=%d, TS=%d",
                                                        aTrackPortInfo.iMediaData->getSeqNum(), samplesize, nallen,
                                                        aTrackPortInfo.iMediaData->getTimestamp()));
                PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::GenerateAVCNALGroup - SeqNum=%d, Size=%d, TS=%d, PTR=0x%x, RC=%d", aTrackPortInfo.iMediaData->getSeqNum(), samplesize, aTrackPortInfo.iMediaData->getTimestamp(), sample, memFragIn.getRefCounter()->getCount()));
                //ignore corrupt samples / nals
                return true;
            }

            if (nallen > 0)
            {
                OsclMemoryFragment memFrag;
                memFrag.ptr = sample;
                memFrag.len = nallen;

                refCntIn->addRef();
                OsclRefCounterMemFrag refCountMemFragOut(memFrag, refCntIn, 0);
                aMediaFragGroup->appendMediaFragment(refCountMemFragOut);

                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC_AVC((0, "  PVMFMP4FFParserNode::GenerateAVCNALGroup - SN=%d, TS=%d, SS-NALSize=%d, NALSize=%d, NALNum=%d",
                                                        aTrackPortInfo.iMediaData->getSeqNum(),
                                                        aTrackPortInfo.iMediaData->getTimestamp(),
                                                        samplesize - nallen,
                                                        nallen,
                                                        numNAL));

                sampleStream.seekFromCurrentPosition(nallen);
                numNAL++;
            }
            sample += nallen;
            samplesize -= nallen;
        }
        else
        {
            return false;
        }
    }

    //check to see if we added any memfrags
    if (aMediaFragGroup->getNumFragments() == 0)
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC_AVC((0, "PVMFMP4FFParserNode::GenerateAVCNALGroup - No Valid NALs - SeqNum=%d, SampleSize=%d, TS=%d",
                                                aTrackPortInfo.iMediaData->getSeqNum(), memFragIn.getMemFrag().len,
                                                aTrackPortInfo.iMediaData->getTimestamp()));
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::GenerateAVCNALGroup - No Valid NALs - SeqNum=%d, Size=%d, TS=%d",
                                       aTrackPortInfo.iMediaData->getSeqNum(),
                                       memFragIn.getMemFrag().len,
                                       aTrackPortInfo.iMediaData->getTimestamp()));
        //ignore corrupt samples / nals
        return false;
    }

    PVMFSharedMediaDataPtr avcNALGroup;
    avcNALGroup = PVMFMediaData::createMediaData(aMediaFragGroup, aTrackPortInfo.iMediaData->getMessageHeader());
    avcNALGroup->setSeqNum(aTrackPortInfo.iMediaData->getSeqNum());
    avcNALGroup->setTimestamp(aTrackPortInfo.iMediaData->getTimestamp());
    avcNALGroup->setFormatSpecificInfo(aTrackPortInfo.iFormatSpecificConfig);

    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC_AVC((0, "PVMFMP4FFParserNode::GenerateAVCNALGroup - SN=%d, SS=%d, NumNAL=%d, TS=%d, Marker=0x%x",
                                            avcNALGroup->getSeqNum(),
                                            memFragIn.getMemFrag().len,
                                            numNAL,
                                            avcNALGroup->getTimestamp(),
                                            avcNALGroup->getMarkerInfo()));

    // Replace the track's output media data with the frag group one
    aTrackPortInfo.iMediaData = avcNALGroup;

    return true;
}

bool PVMFMP4FFParserNode::GetAVCNALLength(OsclBinIStreamBigEndian& stream, uint32& lengthSize, int32& len)
{
    len = 0;
    if (lengthSize == 1)
    {
        uint8 len8 = 0;
        stream >> len8;
        len = (int32)(len8);
        return true;
    }
    else if (lengthSize == 2)
    {
        uint16 len16 = 0;
        stream >> len16;
        len = (int32)(len16);
        return true;
    }
    else if (lengthSize == 4)
    {
        stream >> len;
        return true;
    }
    return false;
}

bool PVMFMP4FFParserNode::UpdateTextSampleEntry(PVMP4FFNodeTrackPortInfo& aTrackPortInfo, uint32 aEntryIndex, PVMFTimedTextMediaData& aTextMediaData)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::UpdateTextSampleEntry() In"));

    // Check if the text sample entry hasn't be set or
    // it is different from the previous one
    if (aTrackPortInfo.iCurrentTextSampleEntry.GetRep() == NULL || aTrackPortInfo.iCurrentTextSampleEntryIndex != aEntryIndex)
    {
        // Retrieve the text sample entry and save as this track's format specific info
        TextSampleEntry* textsampleentry = NULL;
        // NOTE: MP4 API returns the base class pointer SampleEntry but doing a cast since the pointer returned
        // is a pointer to a TextSampleEntry. Dangerous but needs to be fixed first in the MP4 FF lib.
        textsampleentry = (TextSampleEntry*)iPlaybackParserObj->getTextSampleEntryAt(aTrackPortInfo.iTrackId, aEntryIndex);

        if (textsampleentry == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::UpdateTextSampleEntry() Text sample sample index %d could not be retrieved", aEntryIndex));
            return false;
        }

        // Create shared pointer for the text sample entry using OsclMemAllocator
        int32 errcode = 0;
        OsclSharedPtr<PVMFTimedTextSampleEntry> tmp_shared_tse;
        OSCL_TRY(errcode, tmp_shared_tse = PVMFTimedTextSampleEntryUtil::CreatePVMFTimedTextSampleEntry());
        OSCL_FIRST_CATCH_ANY(errcode,
                             PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::UpdateTextSampleEntry() Memory for PVMFTimedTextSampleEntry shared pointer could not be allocated"));
                             return false);

        OSCL_ASSERT(tmp_shared_tse.GetRep() != NULL);
        PVMFTimedTextSampleEntry* textse_ptr = tmp_shared_tse.GetRep();

        // Copy the data from the text sample entry
        textse_ptr->iDisplayFlags = textsampleentry->getDisplayFlags();
        textse_ptr->iHorizontalJustification = textsampleentry->getHorzJustification();
        textse_ptr->iVerticalJustification = textsampleentry->getVertJustification();
        uint8* backcolor = textsampleentry->getBackgroundColourRGBA();
        if (backcolor)
        {
            oscl_memcpy(textse_ptr->iBackgroundRGBA, backcolor, PVMFTIMEDTEXT_RGBA_ARRAYSIZE*sizeof(uint8));
        }

        // Copy box info
        textse_ptr->iBoxTop = textsampleentry->getBoxTop();
        textse_ptr->iBoxLeft = textsampleentry->getBoxLeft();
        textse_ptr->iBoxBottom = textsampleentry->getBoxBottom();
        textse_ptr->iBoxRight = textsampleentry->getBoxRight();

        // Copy style info
        textse_ptr->iStyleStartChar = textsampleentry->getStartChar();
        textse_ptr->iStyleEndChar = textsampleentry->getEndChar();
        textse_ptr->iStyleFontID = textsampleentry->getFontID();
        textse_ptr->iStyleFontStyleFlags = textsampleentry->getFontStyleFlags();
        textse_ptr->iStyleFontSize = textsampleentry->getfontSize();
        uint8* textcolor = textsampleentry->getTextColourRGBA();
        if (textcolor)
        {
            oscl_memcpy(textse_ptr->iStyleTextColorRGBA, textcolor, PVMFTIMEDTEXT_RGBA_ARRAYSIZE*sizeof(uint8));
        }

        // Copy font record info
        textse_ptr->iFontEntryCount = textsampleentry->getFontListSize();

        if (textse_ptr->iFontEntryCount > 0)
        {
            // Allocate memory for the font record list
            errcode = 0;
            OSCL_TRY(errcode, textse_ptr->iFontRecordList = PVMF_BASE_NODE_ARRAY_NEW(PVMFTimedTextFontRecord, textse_ptr->iFontEntryCount));
            OSCL_FIRST_CATCH_ANY(errcode,
                                 PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::UpdateTextSampleEntry() Memory for PVMFTimedTextFontRecord list could not be allocated"));
                                 return false);
            errcode = 0;
            OSCL_TRY(errcode,
                     for (uint16 i = 0; i < textse_ptr->iFontEntryCount; ++i)
        {
            FontRecord* fontrec = textsampleentry->getFontRecordAt(i);
                if (fontrec)
                {
                    textse_ptr->iFontRecordList[i].iFontID = fontrec->getFontID();
                    textse_ptr->iFontRecordList[i].iFontNameLength = fontrec->getFontLength();
                    if (textse_ptr->iFontRecordList[i].iFontNameLength > 0)
                    {
                        // Allocate memory for the font name
                        textse_ptr->iFontRecordList[i].iFontName = PVMF_BASE_NODE_ARRAY_NEW(uint8, textse_ptr->iFontRecordList[i].iFontNameLength + 1);

                        // Copy the font name string
                        oscl_strncpy((char*)(textse_ptr->iFontRecordList[i].iFontName), (char*)(fontrec->getFontName()), textse_ptr->iFontRecordList[i].iFontNameLength + 1);
                        textse_ptr->iFontRecordList[i].iFontName[textse_ptr->iFontRecordList[i].iFontNameLength] = '\0';
                    }
                }
                else
                {
                    textse_ptr->iFontRecordList[i].iFontID = 0;
                    textse_ptr->iFontRecordList[i].iFontNameLength = 0;
                    textse_ptr->iFontRecordList[i].iFontName = NULL;
                }
            }
                    );
            OSCL_FIRST_CATCH_ANY(errcode,
                                 PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::UpdateTextSampleEntry() Memory for font name could not be allocated"));
                                 return false);
        }
        // Save the text track's text sample entry
        aTrackPortInfo.iCurrentTextSampleEntry = tmp_shared_tse;
        // Save the state so text sample entry is only read when changed or not set yet
        aTrackPortInfo.iCurrentTextSampleEntryIndex = aEntryIndex;
    }

    // Set the text sample entry to current one for this track
    aTextMediaData.iTextSampleEntry = aTrackPortInfo.iCurrentTextSampleEntry;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::UpdateTextSampleEntry() Out"));
    return true;
}

void PVMFMP4FFParserNode::QueuePortActivity(const PVMFPortActivity &aActivity)
{
    //queue a new port activity event
    int32 err;
    OSCL_TRY(err, iPortActivityQueue.push_back(aActivity););
    if (err != OsclErrNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::QueuePortActivity() Error push_back to queue failed"));
        ReportMP4FFParserErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aActivity.iPort));
    }
    else
    {
        //wake up the AO to process the port activity event.
        Reschedule();
    }
}


void PVMFMP4FFParserNode::HandlePortActivity(const PVMFPortActivity &aActivity)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::HandlePortActivity() In"));

    //A port is reporting some activity or state change.  This code
    //figures out whether we need to queue a processing event
    //for the AO, and/or report a node event to the observer.

    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_CREATED:
            //Report port created info event to the node.
            ReportMP4FFParserInfoEvent(PVMFInfoPortCreated, (OsclAny*)aActivity.iPort);
            break;

        case PVMF_PORT_ACTIVITY_DELETED:
            //Report port deleted info event to the node.
            ReportMP4FFParserInfoEvent(PVMFInfoPortDeleted, (OsclAny*)aActivity.iPort);
            //Purge any port activity events already queued
            //for this port.
            {
                for (uint32 i = 0; i < iPortActivityQueue.size();)
                {
                    if (iPortActivityQueue[i].iPort == aActivity.iPort)
                    {
                        iPortActivityQueue.erase(&iPortActivityQueue[i]);
                    }
                    else
                    {
                        i++;
                    }
                }
            }
            break;

        case PVMF_PORT_ACTIVITY_CONNECT:
            //nothing needed.
            break;

        case PVMF_PORT_ACTIVITY_DISCONNECT:
            //nothing needed.
            break;

        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
            //An outgoing message was queued on this port.
            //We only need to queue a port activity event on the
            //first message.  Additional events will be queued during
            //the port processing as needed.
            if (aActivity.iPort->OutgoingMsgQueueSize() == 1)
            {
                QueuePortActivity(aActivity);
            }
            break;

        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
            // Incoming msg not supported
            break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_BUSY:
            //Outgoing queue is now busy.
            //No action is needed here-- the node checks for
            //outgoing queue busy as needed during data processing.
            break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_READY:
            //Outgoing queue was previously busy, but is now ready.
            //We may need to schedule new processing events depending on the port type.

            switch (aActivity.iPort->GetPortTag())
            {
                case PVMF_MP4FFPARSERNODE_PORT_TYPE_OUTPUT:
                {
                    // Find the trackportinfo for the port
                    int32 i = 0;
                    int32 maxtrack = iNodeTrackPortList.size();
                    while (i < maxtrack)
                    {
                        if (iNodeTrackPortList[i].iPortInterface == aActivity.iPort)
                        {
                            // Found the element.
                            switch (iNodeTrackPortList[i].iState)
                            {
                                case PVMP4FFNodeTrackPortInfo::TRACKSTATE_DESTFULL:
                                {
                                    // Change the track state to only send the data
                                    iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_SENDDATA;
                                    // Activate the node to process the track state change
                                    Reschedule();
                                }
                                break;

                                case PVMP4FFNodeTrackPortInfo::TRACKSTATE_SEND_ENDOFTRACK:
                                {
                                    //wakeup the AO to try and send the end-of-track again.
                                    Reschedule();
                                }
                                break;

                                default:
                                    break;
                            }
                        }
                        ++i;
                    }
                }
                break;

                default:
                    break;
            }
            break;

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_BUSY:
            // The connected port has become busy (its incoming queue is
            // busy).
            // No action is needed here-- the port processing code
            // checks for connected port busy during data processing.
            break;

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY:
            // The connected port has transitioned from Busy to Ready.
            // It's time to start processing outgoing messages again.
            if (aActivity.iPort->OutgoingMsgQueueSize() > 0)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFMP4FFParserNode::HandlePortActivity() Connected port is now ready Port=0x%x",
                                 aActivity.iPort));

                PVMFPortActivity activity(aActivity.iPort, PVMF_PORT_ACTIVITY_OUTGOING_MSG);
                QueuePortActivity(activity);
            }
            break;

        default:
            break;
    }
}


void PVMFMP4FFParserNode::ProcessPortActivity()
{
    //called by the AO to process a port activity message
    //Pop the queue...
    PVMFPortActivity activity(iPortActivityQueue.front());
    iPortActivityQueue.erase(&iPortActivityQueue.front());

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::ProcessPortActivity() In"));

    PVMFStatus status = PVMFSuccess;
    switch (activity.iType)
    {
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
            status = ProcessOutgoingMsg(activity.iPort);
            //Re-queue the port activity event as long as there's
            //more data to process and it isn't in a Busy state.
            if (status != PVMFErrBusy && activity.iPort->OutgoingMsgQueueSize() > 0)
            {
                QueuePortActivity(activity);
            }
            break;

        default:
            break;

    }

    //Report any unexpected failure in port processing...
    //(the InvalidState error happens when port input is suspended,
    //so don't report it.)
    if (status != PVMFErrBusy && status != PVMFSuccess && status != PVMFErrInvalidState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::ProcessPortActivity() Error unexpected failure Port=0x%x Type=0x%x",
                        activity.iPort, activity.iType));

        ReportMP4FFParserErrorEvent(PVMFErrPortProcessing);
    }
}


PVMFStatus PVMFMP4FFParserNode::ProcessIncomingMsg(PVMFPortInterface* /*aPort*/)
{
    // No incoming msg supported
    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::ProcessOutgoingMsg(PVMFPortInterface* aPort)
{
    //Called by the AO to process one message off the outgoing
    //message queue for the given port.  This routine will
    //try to send the data to the connected port.
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::ProcessOutgoingMsg() In aPort=0x%x", aPort));

    PVMFStatus status = aPort->Send();
    if (status == PVMFErrBusy)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "PVMFMP4FFParserNode::ProcessOutgoingMsg() Connected port is in busy state"));
    }

    return status;
}

void PVMFMP4FFParserNode::ResetAllTracks()
{
    for (uint32 i = 0; i < iNodeTrackPortList.size(); ++i)
    {
        iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_UNINITIALIZED;
        iNodeTrackPortList[i].iMediaData.Unbind();
        iNodeTrackPortList[i].iSeqNum = 0;
        iNodeTrackPortList[i].iPortInterface->ClearMsgQueues();
        iNodeTrackPortList[i].iCurrentTextSampleEntry.Unbind();
    }
    iPortActivityQueue.clear();
}


bool PVMFMP4FFParserNode::ReleaseAllPorts()
{
    while (!iNodeTrackPortList.empty())
    {
        iNodeTrackPortList[0].iPortInterface->Disconnect();
        iNodeTrackPortList[0].iMediaData.Unbind();
        if (iNodeTrackPortList[0].iPortInterface)
        {
            PVMF_BASE_NODE_DELETE(((PVMFMP4FFParserOutPort*)iNodeTrackPortList[0].iPortInterface));
        }
        if (iNodeTrackPortList[0].iClockConverter)
        {
            PVMF_BASE_NODE_DELETE(iNodeTrackPortList[0].iClockConverter);
        }
        if (iNodeTrackPortList[0].iTrackDataMemoryPool)
        {
            iNodeTrackPortList[0].iTrackDataMemoryPool->CancelFreeChunkAvailableCallback();
            iNodeTrackPortList[0].iTrackDataMemoryPool->removeRef();
            iNodeTrackPortList[0].iTrackDataMemoryPool = NULL;
        }
        if (iNodeTrackPortList[0].iMediaDataImplAlloc)
        {
            PVMF_BASE_NODE_DELETE(iNodeTrackPortList[0].iMediaDataImplAlloc);
        }
        if (iNodeTrackPortList[0].iTextMediaDataImplAlloc)
        {
            PVMF_BASE_NODE_DELETE(iNodeTrackPortList[0].iTextMediaDataImplAlloc);
        }
        if (iNodeTrackPortList[0].iMediaDataMemPool)
        {
            iNodeTrackPortList[0].iMediaDataMemPool->CancelFreeChunkAvailableCallback();
            iNodeTrackPortList[0].iMediaDataMemPool->removeRef();
        }
        if (iNodeTrackPortList[0].iMediaDataGroupAlloc)
        {
            iNodeTrackPortList[0].iMediaDataGroupAlloc->removeRef();
        }
        if (iNodeTrackPortList[0].iMediaDataGroupImplMemPool)
        {
            iNodeTrackPortList[0].iMediaDataGroupImplMemPool->removeRef();
        }
        if (iPortDataLog)
        {
            if (iNodeTrackPortList[0].iBinAppenderPtr.GetRep() != NULL)
            {
                iNodeTrackPortList[0].iPortLogger->RemoveAppender(iNodeTrackPortList[0].iBinAppenderPtr);
                iNodeTrackPortList[0].iBinAppenderPtr.Unbind();
            }
        }
        iNodeTrackPortList.erase(iNodeTrackPortList.begin());
    }

    return true;
}


void PVMFMP4FFParserNode::RemoveAllCommands()
{
    //The command queues are self-deleting, but we want to
    //notify the observer of unprocessed commands.
    if (IsCommandInProgress(iCurrentCommand))
    {
        CommandComplete(iCurrentCommand, PVMFFailure);
    }

    PVMFNodeCommand Cmd;
    while (iInputCommands.size() > 0)
    {
        iInputCommands.GetFrontAndErase(Cmd);
        CommandComplete(Cmd, PVMFFailure);
    }
}


void PVMFMP4FFParserNode::CleanupFileSource()
{
    iVideoDimensionInfoVec.clear();

    if (iPlaybackParserObj)
    {
        if (iExternalDownload == true)
        {
            iPlaybackParserObj->DestroyDataStreamForExternalDownload();
        }
    }

    while (!iClipInfoList.empty())
    {
        // delete file handle, if any
        PVMFMp4ClipInfo& clipInfo = iClipInfoList.back();
        OsclFileHandle* fileHandle = clipInfo.iClipInfo.GetFileHandle();
        if (fileHandle)
        {
            PVMF_BASE_NODE_DELETE(fileHandle);
            fileHandle = NULL;
        }
        // delete file parser object, if any
        if (clipInfo.iParserObj)
        {
            ReleaseMP4FileParser(iClipInfoList.size() - 1, true);
        }
        // clear the vector element
        iClipInfoList.pop_back();
    }

    // Reset the playlist related parameters.
    iPlaybackParserObj = NULL;
    iMetadataParserObj = NULL;
    iPlaylistExhausted = false;
    iInitNextClip = false;
    iNumClipsInPlayList = 0;
    iPlaybackClipIndex = -1;
    iLastPlayingClipIndex = -1;
    iClipIndexForMetadata = -1;
    iPlaylistRepositioning = false;
    iNextInitializedClipIndex = -1;
    iFirstClipNonAudioOnly = false;
    iFirstValidClipIndex = -1;

    if (iDataStreamInterface != NULL)

    {
        iDataStreamInterface->CloseSession(iDataStreamSessionID);
        PVInterface* iFace = OSCL_STATIC_CAST(PVInterface*, iDataStreamInterface);
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        iDataStreamFactory->DestroyPVMFCPMPluginAccessInterface(uuid, iFace);
        iDataStreamInterface = NULL;
    }

    if (iOMA2DecryptionBuffer != NULL)
    {
        PVMF_BASE_NODE_ARRAY_DELETE(iOMA2DecryptionBuffer);
        iOMA2DecryptionBuffer = NULL;
    }

    iDownloadComplete = false;
    iMP4HeaderSize = 0;
    iProgressivelyPlayable = false;
    iCPMSequenceInProgress = false;
    iFastTrackSession = false;
    iProtectedFile = false;
    iExternalDownload = false;
    iThumbNailMode = false;

    iUseCPMPluginRegistry = false;
    if (iCPMContentAccessFactory != NULL)
    {
        if (iDecryptionInterface != NULL)
        {
            iDecryptionInterface->Reset();
            /* Remove the decrpytion interface */
            PVUuid uuid = PVMFCPMPluginDecryptionInterfaceUuid;
            iCPMContentAccessFactory->DestroyPVMFCPMPluginAccessInterface(uuid, iDecryptionInterface);
            iDecryptionInterface = NULL;
        }
        iCPMContentAccessFactory->removeRef();
        iCPMContentAccessFactory = NULL;
    }
    if (iDataStreamFactory != NULL)
    {
        iDataStreamFactory->removeRef();
        iDataStreamFactory = NULL;
    }

    oWaitingOnLicense = false;
    iPoorlyInterleavedContentEventSent = false;
}

// From PVMFMP4ProgDownloadSupportInterface
int32 PVMFMP4FFParserNode::convertSizeToTime(TOsclFileOffset fileSize, uint32& timeStamp)
{
    if (iPlaybackParserObj == NULL)
    {
        return DEFAULT_ERROR;
    }

    timeStamp = MAX32BITUINT;
    if (iNodeTrackPortList.size() <= 0)
    {//if track selection is not done, use all the tracks for calculation to avoid deadlock.
        int32 iNumTracks = iPlaybackParserObj->getNumTracks();
        uint32 iIdList[16];
        if (iNumTracks != iPlaybackParserObj->getTrackIDList(iIdList, iNumTracks))
        {
            return DEFAULT_ERROR;//PVMFFailure;
        }
        uint64 track_timestamp = 0;
        for (int32 i = 0; i < iNumTracks; i++)
        {
            uint32 trackID = iIdList[i];
            int32 result = iPlaybackParserObj->getMaxTrackTimeStamp(trackID, fileSize, track_timestamp);
            if (EVERYTHING_FINE == result)
            {
                // Convert the timestamp from media timescale to milliseconds
                MediaClockConverter mcc(iPlaybackParserObj->getTrackMediaTimescale(trackID));
                mcc.set_clock(track_timestamp, 0);
                uint32 track_timeStamp = mcc.get_converted_ts(1000);
                if (track_timeStamp < timeStamp)
                {
                    timeStamp = track_timeStamp;
                }
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::convertSizeToTime() - getMaxTrackTimeStamp, NPT[%d]=%d, TrackTS=%d",
                                                     trackID,
                                                     timeStamp,
                                                     Oscl_Int64_Utils::get_uint64_lower32(track_timestamp)));
            }
            else
            {
                return DEFAULT_ERROR;//PVMFFailure;
            }
        }
        iLastNPTCalcInConvertSizeToTime = timeStamp;
        iFileSizeLastConvertedToTime = fileSize;
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::convertSizeToTime(), FileSize=%u, NPT=%d", iFileSizeLastConvertedToTime, iLastNPTCalcInConvertSizeToTime));
        return EVERYTHING_FINE;//PVMFSuccess;
    }

    //we do not count tracks that have reached EOS
    uint64 track_timestamp = 0;
    int32 result = EVERYTHING_FINE;
    bool oAllTracksHaveReachedEOS = true;

    if (iNodeTrackPortList[0].iState != PVMP4FFNodeTrackPortInfo::TRACKSTATE_ENDOFTRACK)
    {
        //there is atleast one track that has not reached EOS
        oAllTracksHaveReachedEOS = false;
        result = iPlaybackParserObj->getMaxTrackTimeStamp(iNodeTrackPortList[0].iTrackId,
                 fileSize,
                 track_timestamp);

        if (result !=  EVERYTHING_FINE)
        {
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::convertSizeToTime() - getMaxTrackTimeStamp Failed, TrackID=%d, Mime=%s",
                                                 iNodeTrackPortList[0].iTrackId,
                                                 iNodeTrackPortList[0].iMimeType.get_cstr()));
            return result;
        }
        else
        {
            // Convert the timestamp from media timescale to milliseconds
            MediaClockConverter mcc(iPlaybackParserObj->getTrackMediaTimescale(iNodeTrackPortList[0].iTrackId));
            mcc.update_clock(track_timestamp);
            timeStamp = mcc.get_converted_ts(1000);
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::convertSizeToTime() - getMaxTrackTimeStamp, NPT=%d, TrackID=%d, Mime=%s",
                                                 timeStamp,
                                                 iNodeTrackPortList[0].iTrackId,
                                                 iNodeTrackPortList[0].iMimeType.get_cstr()));
        }
    }


    for (uint32 ii = 1; ii < iNodeTrackPortList.size(); ++ii)
    {
        if (iNodeTrackPortList[ii].iState != PVMP4FFNodeTrackPortInfo::TRACKSTATE_ENDOFTRACK)
        {
            //there is atleast one track that has not reached EOS
            oAllTracksHaveReachedEOS = false;
            result = iPlaybackParserObj->getMaxTrackTimeStamp(iNodeTrackPortList[ii].iTrackId,
                     fileSize,
                     track_timestamp);

            if (result != EVERYTHING_FINE)
            {
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::convertSizeToTime() - getMaxTrackTimeStamp Failed, TrackID=%d, Mime=%s",
                                                     iNodeTrackPortList[ii].iTrackId,
                                                     iNodeTrackPortList[ii].iMimeType.get_cstr()));
                return result;
            }

            // Convert the timestamp from media timescale to milliseconds
            MediaClockConverter mcc(iPlaybackParserObj->getTrackMediaTimescale(iNodeTrackPortList[ii].iTrackId));
            mcc.update_clock(track_timestamp);
            track_timestamp = mcc.get_converted_ts(1000);

            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::convertSizeToTime() - getMaxTrackTimeStamp, NPT=%d, TrackID=%d, Mime=%s",
                                                 Oscl_Int64_Utils::get_uint64_lower32(track_timestamp),
                                                 iNodeTrackPortList[ii].iTrackId,
                                                 iNodeTrackPortList[ii].iMimeType.get_cstr()));

            timeStamp = (track_timestamp < timeStamp) ? (uint32)track_timestamp : timeStamp;
        }
    }
    if (oAllTracksHaveReachedEOS)
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::convertSizeToTime() - All Tracks have Reached EOS"));
        //if there are no playing tracks then this method should not even be called
        return DEFAULT_ERROR;
    }

    iLastNPTCalcInConvertSizeToTime = timeStamp;
    iFileSizeLastConvertedToTime = fileSize;
    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::convertSizeToTime(), FileSize=%d, NPT=%d", iFileSizeLastConvertedToTime, iLastNPTCalcInConvertSizeToTime));
    return EVERYTHING_FINE;
}

void PVMFMP4FFParserNode::setFileSize(const TOsclFileOffset aFileSize)
{
    iDownloadFileSize = aFileSize;
}

void PVMFMP4FFParserNode::setDownloadProgressInterface(PVMFDownloadProgressInterface* download_progress)
{
    if (download_progress_interface)
    {
        download_progress_interface->removeRef();
    }

    download_progress_interface = download_progress;

    // get the progress clock
    download_progress_clock = download_progress_interface->getDownloadProgressClock();
}


void PVMFMP4FFParserNode::playResumeNotification(bool aDownloadComplete)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::playResumeNotification() In"));

    // If download has completed, unbind the DL progress clock so no more checks occur
    iDownloadComplete = aDownloadComplete;
    if (aDownloadComplete)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::playResumeNotification() Download complete to unbind DL progress clock"));
        download_progress_clock.Unbind();
    }

    // resume playback
    if (autopaused)
    {
        autopaused = false;
        for (uint32 ii = 0; ii < iNodeTrackPortList.size(); ++ii)
        {
            switch (iNodeTrackPortList[ii].iState)
            {
                case PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE:
                    iNodeTrackPortList[ii].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                    break;

                default:
                    // nothing to do
                    break;
            }
        }

        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::playResumeNotification() - Auto Resume Triggered, FileSize = %u, NPT = %d", iFileSizeLastConvertedToTime, iLastNPTCalcInConvertSizeToTime));
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::playResumeNotification() Sending PVMFInfoDataReady event"));
        if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_PREPARE)
        {
            CommandComplete(iCurrentCommand, PVMFSuccess);
            iUnderFlowEventReported = false;
        }
        else
        {
            // Schedule AO to run again
            Reschedule();
        }
    }
    else if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_INIT)
    {
        /* Callbacks intiated  as part of node init */
        if (iDownloadComplete == true)
        {
            if (iCPMSequenceInProgress == false)
            {
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::playResumeNotification() - Download Complete"));
                /*
                 * Can come here only if we are doing PDL / PPB.
                 * Init is still pending and download has completed.
                 * A few possible scenarios here:
                 * - Non-PDL, Non-DRM clip - we are waiting for the complete
                 * file to be downloaded before we can parse the movie atom.
                 * - OMA1 DRM clip - CPM seq complete, and
                 * we were waiting on more data.
                 * - OMA2 PDCF DRM clip, PIFF DRM Clip - were waiting on more data to parse the movie atom,
                 * CPM seq not complete since we need contents of movie atom to authorize
                 * with CPM
                 * - PDL, Non-DRM clip - we were waiting on more data and
                 * download has completed
                 */
                if (iCPM)
                {
                    if ((iCPMContentType == PVMF_CPM_FORMAT_OMA2) || (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE))
                    {
                        PVMP4FFNodeTrackOMA2DRMInfo* oma2trackInfo = NULL;

                        PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
                        if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
                        {
                            dsFactory = iDataStreamFactory;
                        }
                        PVMFStatus status = InitNextValidClipInPlaylist(NULL, 0, dsFactory);
                        if (status == PVMFSuccess)
                        {
                            if ((iCPMContentType == PVMF_CPM_FORMAT_OMA2) && (CheckForOMA2AuthorizationComplete(oma2trackInfo) == PVMFPending))
                            {
                                PVMFStatus status = RequestUsage(oma2trackInfo);
                                if (status != PVMFPending)
                                {
                                    CompleteInit(iCurrentCommand, status);
                                }
                                return;
                            }
                            else if (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE)
                            {
                                PVMFStatus status = RequestUsage(NULL);
                                if (status != PVMFPending)
                                {
                                    CompleteInit(iCurrentCommand, status);
                                }
                                return;
                            }
                        }
                        else
                        {
                            CompleteInit(iCurrentCommand, status);
                            return;
                        }
                    }
                }
                CompleteInit(iCurrentCommand);
            }
        }
        //Ignore this callback we are waiting on notifydownload complete
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::playResumeNotification() Out"));
}

void PVMFMP4FFParserNode::notifyDownloadComplete()
{
    iDownloadComplete = true;
    if (!iProgressivelyPlayable)
    {
        //in case of non-PDL files, we have to hold back on init complete
        //till donwnload is fully complete. So if that is the case, call
        //playResumeNotification and this will complete the pending init cmd
        playResumeNotification(true);
    }
}


bool PVMFMP4FFParserNode::MapMP4ErrorCodeToEventCode(int32 aMP4ErrCode, PVUuid& aEventUUID, int32& aEventCode)
{
    // Set the UUID to the MP4 FF specific one and change to
    // file format one if necessary in the switch statement
    aEventUUID = PVMFMP4FFParserEventTypesUUID;

    switch (aMP4ErrCode)
    {
        case READ_FAILED:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFFFErrFileRead;
            break;

        case DEFAULT_ERROR:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFFFErrMisc;
            break;

        case READ_USER_DATA_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrUserDataAtomReadFailed;
            break;

        case READ_MEDIA_DATA_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrMediaDataAtomReadFailed;
            break;

        case READ_MOVIE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrMovieAtomReadFailed;
            break;

        case READ_MOVIE_HEADER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrMovieHeaderAtomReadFailed;
            break;

        case READ_TRACK_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrTrackAtomReadFailed;
            break;

        case READ_TRACK_HEADER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrTrackHeaderAtomReadFailed;
            break;

        case READ_TRACK_REFERENCE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrTrackReferenceAtomReadFailed;
            break;

        case READ_TRACK_REFERENCE_TYPE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrTrackReferenceTypeAtomReadFailed;
            break;

        case READ_OBJECT_DESCRIPTOR_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrObjectDescriptorAtomReadFailed;
            break;

        case READ_INITIAL_OBJECT_DESCRIPTOR_FAILED:
            aEventCode = PVMFMP4FFParserErrInitialObjectDescriptorReadFailed;
            break;

        case READ_OBJECT_DESCRIPTOR_FAILED:
            aEventCode = PVMFMP4FFParserErrObjectDescriptorReadFailed;
            break;

        case READ_MEDIA_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrMediaAtomReadFailed;
            break;

        case READ_MEDIA_HEADER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrMediaHeaderAtomReadFailed;
            break;

        case READ_HANDLER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrHandlerAtomReadFailed;
            break;

        case READ_MEDIA_INFORMATION_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrMediaInformationAtomReadFailed;
            break;

        case READ_MEDIA_INFORMATION_HEADER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrMediaInformationHeaderAtomReadFailed;
            break;

        case READ_VIDEO_MEDIA_HEADER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrVideoMediaHeaderAtomReadFailed;
            break;

        case READ_SOUND_MEDIA_HEADER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrSoundMediaHeaderAtomReadFailed;
            break;

        case READ_HINT_MEDIA_HEADER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrHintMediaHeaderAtomReadFailed;
            break;

        case READ_MPEG4_MEDIA_HEADER_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrMPEG4MediaHeaderAtomReadFailed;
            break;

        case READ_DATA_INFORMATION_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrDataInformationAtomReadFailed;
            break;

        case READ_DATA_REFERENCE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrDataReferenceAtomReadFailed;
            break;

        case READ_DATA_ENTRY_URL_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrDataEntryURLAtomReadFailed;
            break;

        case READ_DATA_ENTRY_URN_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrDataEntryURNAtomReadFailed;
            break;

        case READ_SAMPLE_TABLE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrSampleTableAtomReadFailed;
            break;

        case READ_TIME_TO_SAMPLE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrTimeToSampleAtomReadFailed;
            break;

        case READ_SAMPLE_DESCRIPTION_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrSampleDescriptionAtomReadFailed;
            break;

        case READ_SAMPLE_SIZE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrSampleSizeAtomReadFailed;
            break;

        case READ_SAMPLE_TO_CHUNK_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrSampleToChunkAtomReadFailed;
            break;

        case READ_CHUNK_OFFSET_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrChunkOffsetAtomReadFailed;
            break;

        case READ_SYNC_SAMPLE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrSyncSampleAtomReadFailed;
            break;

        case READ_SAMPLE_ENTRY_FAILED:
            aEventCode = PVMFMP4FFParserErrSampleEntryReadFailed;
            break;

        case READ_AUDIO_SAMPLE_ENTRY_FAILED:
            aEventCode = PVMFMP4FFParserErrAudioSampleEntryReadFailed;
            break;

        case READ_VISUAL_SAMPLE_ENTRY_FAILED:
            aEventCode = PVMFMP4FFParserErrVisualSampleEntryReadFailed;
            break;

        case READ_HINT_SAMPLE_ENTRY_FAILED:
            aEventCode = PVMFMP4FFParserErrHintSampleEntryReadFailed;
            break;

        case READ_MPEG_SAMPLE_ENTRY_FAILED:
            aEventCode = PVMFMP4FFParserErrMPEGSampleEntryReadFailed;
            break;

        case READ_AUDIO_HINT_SAMPLE_FAILED:
            aEventCode = PVMFMP4FFParserErrAudioHintSampleReadFailed;
            break;

        case READ_VIDEO_HINT_SAMPLE_FAILED:
            aEventCode = PVMFMP4FFParserErrVideoHintSampleReadFailed;
            break;

        case READ_ESD_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrESDAtomReadFailed;
            break;

        case READ_ES_DESCRIPTOR_FAILED:
            aEventCode = PVMFMP4FFParserErrESDescriptorReadFailed;
            break;

        case READ_SL_CONFIG_DESCRIPTOR_FAILED:
            aEventCode = PVMFMP4FFParserErrSLConfigDescriptorReadFailed;
            break;

        case READ_DECODER_CONFIG_DESCRIPTOR_FAILED:
            aEventCode = PVMFMP4FFParserErrDecoderConfigDescriptorReadFailed;
            break;

        case READ_DECODER_SPECIFIC_INFO_FAILED:
            aEventCode = PVMFMP4FFParserErrDecoderSpecificInfoReadFailed;
            break;

        case DUPLICATE_MOVIE_ATOMS:
            aEventCode = PVMFMP4FFParserErrDuplicateMovieAtoms;
            break;

        case NO_MOVIE_ATOM_PRESENT:
            aEventCode = PVMFMP4FFParserErrNoMovieAtomPresent;
            break;

        case DUPLICATE_OBJECT_DESCRIPTORS:
            aEventCode = PVMFMP4FFParserErrDuplicateObjectDescriptors;
            break;

        case NO_OBJECT_DESCRIPTOR_ATOM_PRESENT:
            aEventCode = PVMFMP4FFParserErrNoObjectDescriptorAtomPresent;
            break;

        case DUPLICATE_MOVIE_HEADERS:
            aEventCode = PVMFMP4FFParserErrDuplicateMovieHeaders;
            break;

        case NO_MOVIE_HEADER_ATOM_PRESENT:
            aEventCode = PVMFMP4FFParserErrNoMovieHeaderAtomPresent;
            break;

        case DUPLICATE_TRACK_REFERENCE_ATOMS:
            aEventCode = PVMFMP4FFParserErrDuplicateTrackReferenceAtoms;
            break;

        case DUPLICATE_TRACK_HEADER_ATOMS:
            aEventCode = PVMFMP4FFParserErrDuplicateTrackHeaderAtoms;
            break;

        case NO_TRACK_HEADER_ATOM_PRESENT:
            aEventCode = PVMFMP4FFParserErrNoTrackHeaderAtomPresent;
            break;

        case DUPLICATE_MEDIA_ATOMS:
            aEventCode = PVMFMP4FFParserErrDuplicateMediaAtoms;
            break;

        case NO_MEDIA_ATOM_PRESENT:
            aEventCode = PVMFMP4FFParserErrNoMediaAtomPresent;
            break;

        case READ_UNKNOWN_ATOM:
            aEventCode = PVMFMP4FFParserErrUnknownAtom;
            break;

        case NON_PV_CONTENT:
            aEventCode = PVMFMP4FFParserErrNonPVContent;
            break;

        case FILE_NOT_STREAMABLE:
            aEventCode = PVMFMP4FFParserErrFileNotStreamable;
            break;

        case INSUFFICIENT_BUFFER_SIZE:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFFFErrInsufficientBuffer;
            break;

        case INVALID_SAMPLE_SIZE:
            aEventCode = PVMFMP4FFParserErrInvalidSampleSize;
            break;

        case INVALID_CHUNK_OFFSET:
            aEventCode = PVMFMP4FFParserErrInvalidChunkOffset;
            break;

        case MEMORY_ALLOCATION_FAILED:
            aEventCode = PVMFMP4FFParserErrMemoryAllocationFailed;
            break;

        case READ_FILE_TYPE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrFileTypeAtomReadFailed;
            break;

        case ZERO_OR_NEGATIVE_ATOM_SIZE:
            aEventCode = PVMFMP4FFParserErrZeroOrNegativeAtomSize;
            break;

        case NO_MEDIA_TRACKS_IN_FILE:
            aEventCode = PVMFMP4FFParserErrNoMediaTracksInFile;
            break;

        case NO_META_DATA_FOR_MEDIA_TRACKS:
            aEventCode = PVMFMP4FFParserErrNoMetadataForMediaTracks;
            break;

        case MEDIA_DATA_NOT_SELF_CONTAINED:
            aEventCode = PVMFMP4FFParserErrMediaDataNotSelfContained;
            break;

        case READ_PVTI_SESSION_INFO_FAILED:
            aEventCode = PVMFMP4FFParserErrPVTISessionInfoReadFailed;
            break;

        case READ_PVTI_MEDIA_INFO_FAILED:
            aEventCode = PVMFMP4FFParserErrPVTIMediaInfoReadFailed;
            break;

        case READ_CONTENT_VERSION_FAILED:
            aEventCode = PVMFMP4FFParserErrContentVersionReadFailed;
            break;

        case READ_DOWNLOAD_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrDownloadAtomReadFailed;
            break;

        case READ_TRACK_INFO_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrTrackInfoAtomReadFailed;
            break;

        case READ_REQUIREMENTS_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrRequirementsAtomReadFailed;
            break;

        case READ_PV_USER_DATA_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrPVUserDataAtomReadFailed;
            break;

        case READ_VIDEO_INFORMATION_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrVideoInformationAtomReadFailed;
            break;

        case READ_RANDOM_ACCESS_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrRandomAccessAtomReadFailed;
            break;

        case READ_AMR_SAMPLE_ENTRY_FAILED:
            aEventCode = PVMFMP4FFParserErrAMRSampleEntryReadFailed;
            break;

        case READ_H263_SAMPLE_ENTRY_FAILED:
            aEventCode = PVMFMP4FFParserErrH263SampleEntryReadFailed;
            break;

        case FILE_OPEN_FAILED:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFFFErrFileOpen;
            break;

        case READ_UUID_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrUUIDAtomReadFailed;
            break;

        case FILE_VERSION_NOT_SUPPORTED:
            aEventCode = PVMFMP4FFParserErrFileVersionNotSupported;
            break;

        case TRACK_VERSION_NOT_SUPPORTED:
            aEventCode = PVMFMP4FFParserErrTrackVersioNotSupported;
            break;


        case READ_COPYRIGHT_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrCopyrightAtomReadFailed;
            break;

        case READ_FONT_TABLE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrFontTableAtomReadFailed;
            break;

        case READ_FONT_RECORD_FAILED:
            aEventCode = PVMFMP4FFParserErrFontRecordReadFailed;
            break;

        case FILE_PSEUDO_STREAMABLE:
            aEventCode = PVMFMP4FFParserErrPseudostreamableFile;
            break;

        case FILE_NOT_PSEUDO_STREAMABLE:
            aEventCode = PVMFMP4FFParserErrNotPseudostreamableFile;
            break;

        case READ_PV_ENTITY_TAG_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrPVEntityTagAtomReadFailed;
            break;

        case DUPLICATE_FILE_TYPE_ATOMS:
            aEventCode = PVMFMP4FFParserErrDuplicateFileTypeAtoms;
            break;

        case UNSUPPORTED_FILE_TYPE:
            aEventCode = PVMFMP4FFParserErrUnsupportedFileType;
            break;

        case FILE_TYPE_ATOM_NOT_FOUND:
            aEventCode = PVMFMP4FFParserErrFileTypeAtomNotFound;
            break;

        case READ_EDIT_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrEditAtomReadFailed;
            break;

        case READ_EDITLIST_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrEditlistAtomReadFailed;
            break;

        case ATOM_VERSION_NOT_SUPPORTED:
            aEventCode = PVMFMP4FFParserErrAtomVersionNotSupported;
            break;

        case READ_UDTA_TITL_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTATITLReadFailed;
            break;

        case READ_UDTA_DSCP_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTADSCPReadFailed;
            break;

        case READ_UDTA_CPRT_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTACPRTReadFailed;
            break;

        case READ_UDTA_PERF_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTAPERFReadFailed;
            break;

        case READ_UDTA_AUTH_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTAUTHReadFailed;
            break;

        case READ_UDTA_GNRE_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTAGNREReadFailed;
            break;

        case READ_UDTA_RTNG_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTARTNGReadFailed;
            break;

        case READ_UDTA_CLSF_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTACLSFReadFailed;
            break;

        case READ_UDTA_KYWD_FAILED:
            aEventCode = PVMFMP4FFParserErrUDTAKYWDReadFailed;
            break;

        case READ_PV_CONTENT_TYPE_ATOM_FAILED:
            aEventCode = PVMFMP4FFParserErrPVContentTypeAtomReadFailed;
            break;

        case INSUFFICIENT_DATA:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFFFErrInsufficientData;
            break;

        case NOT_SUPPORTED:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFFFErrNotSupported;
            break;

        case READ_AVC_SAMPLE_ENTRY_FAILED:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFMP4FFParserErrReadAVCSampleEntryFailed;
            break;

        case READ_AVC_CONFIG_BOX_FAILED:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFMP4FFParserErrReadAVCConfigBoxFailed;
            break;

        case READ_MPEG4_BITRATE_BOX_FAILED:
            aEventUUID = PVMFFileFormatEventTypesUUID;
            aEventCode = PVMFMP4FFParserErrReadMPEG4BitRateBoxFailed;
            break;

        case EVERYTHING_FINE:
            OSCL_ASSERT(false); // Should not pass this "error" code to this function
        default:
            return false;
    }

    return true;
}

/* CPM related */
void PVMFMP4FFParserNode::InitCPM()
{
    iCPMSequenceInProgress = true;
    iCPMInitCmdId = iCPM->Init();
}

void PVMFMP4FFParserNode::OpenCPMSession()
{
    iCPMSequenceInProgress = true;
    iCPMOpenSessionCmdId = iCPM->OpenSession(iCPMSessionID);
}

void PVMFMP4FFParserNode::GetCPMLicenseInterface()
{
    iCPMSequenceInProgress = true;
    iCPMLicenseInterfacePVI = NULL;
    iCPMGetLicenseInterfaceCmdId =
        iCPM->QueryInterface(iCPMSessionID,
                             PVMFCPMPluginLicenseInterfaceUuid,
                             iCPMLicenseInterfacePVI);
}

bool PVMFMP4FFParserNode::GetCPMContentAccessFactory()
{
    iCPMSequenceInProgress = true;
    PVMFStatus status = iCPM->GetContentAccessFactory(iCPMSessionID,
                        iCPMContentAccessFactory);
    if (status != PVMFSuccess)
    {
        return false;
    }
    return true;
}

void PVMFMP4FFParserNode::CPMRegisterContent()
{
    iCPMSequenceInProgress = true;
    if (IsValidContextData(0) == true)
    {
        iCPMRegisterContentCmdId = iCPM->RegisterContent(iCPMSessionID,
                                   GetClipURLAt(0),
                                   GetClipFormatTypeAt(0),
                                   (OsclAny*) & GetSourceContextDataAt(0));
    }
    else
    {
        iCPMRegisterContentCmdId = iCPM->RegisterContent(iCPMSessionID,
                                   GetClipURLAt(0),
                                   GetClipFormatTypeAt(0),
                                   (OsclAny*) & GetCPMSourceDataAt(0));
    }
}

void PVMFMP4FFParserNode::GetCPMContentType()
{
    iCPMContentType = iCPM->GetCPMContentType(iCPMSessionID);
}

bool PVMFMP4FFParserNode::GetCPMMetaDataExtensionInterface()
{
    iCPMSequenceInProgress = true;
    PVInterface* temp = NULL;
    bool retVal =
        iCPM->queryInterface(KPVMFMetadataExtensionUuid, temp);
    iCPMMetaDataExtensionInterface = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, temp);
    return retVal;
}

PVMP4FFNodeTrackOMA2DRMInfo*
PVMFMP4FFParserNode::LookUpOMA2TrackInfoForTrack(uint32 aTrackID)
{
    Oscl_Vector<PVMP4FFNodeTrackOMA2DRMInfo, OsclMemAllocator>::iterator it;
    for (it = iOMA2DRMInfoVec.begin(); it != iOMA2DRMInfoVec.end(); it++)
    {
        if (it->iTrackId == aTrackID)
        {
            return it;
        }
    }
    return NULL;
}

PVMFStatus PVMFMP4FFParserNode::CheckForOMA2AuthorizationComplete(PVMP4FFNodeTrackOMA2DRMInfo*& aInfo)
{
    aInfo = NULL;
    if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
    {
        /* Look for a track that needs authorization */
        Oscl_Vector<PVMP4FFNodeTrackOMA2DRMInfo, OsclMemAllocator>::iterator it;
        for (it = iOMA2DRMInfoVec.begin(); it != iOMA2DRMInfoVec.end(); it++)
        {
            if (it->iOMA2TrackAuthorizationComplete == false)
            {
                aInfo = it;
                return PVMFPending;
            }
        }
    }
    else
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::CheckForOMA2AuthorizationComplete - Invalid CPM ContentType"));
        return PVMFFailure;
    }
    return PVMFSuccess;
}

void PVMFMP4FFParserNode::OMA2TrackAuthorizationComplete()
{
    if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
    {
        /* Look for a track that needs authorization */
        Oscl_Vector<PVMP4FFNodeTrackOMA2DRMInfo, OsclMemAllocator>::iterator it;
        for (it = iOMA2DRMInfoVec.begin(); it != iOMA2DRMInfoVec.end(); it++)
        {
            if (it->iOMA2TrackAuthorizationInProgress == true)
            {
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::OMA2TrackAuthorizationComplete - TrackId=%d", it->iTrackId));
                it->iOMA2TrackAuthorizationComplete = true;
                it->iOMA2TrackAuthorizationInProgress = false;
                if (iApprovedUsage.value.uint32_value ==
                        iRequestedUsage.value.uint32_value)
                {
                    it->iOMA2TrackAuthorized = true;
                }
                return;
            }
        }
    }
    else
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::OMA2TrackAuthorizationComplete - Invalid CPM ContentType"));
        OSCL_ASSERT(false);
    }
    PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::OMA2TrackAuthorizationComplete - Couldn't Find Track"));
}

bool PVMFMP4FFParserNode::CheckForOMA2UsageApproval()
{
    if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
    {
        /* Look for a track that needs authorization */
        Oscl_Vector<PVMP4FFNodeTrackOMA2DRMInfo, OsclMemAllocator>::iterator it;
        for (it = iOMA2DRMInfoVec.begin(); it != iOMA2DRMInfoVec.end(); it++)
        {
            if (it->iOMA2TrackAuthorized == false)
            {
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForOMA2UsageApproval - Not Authorized - TrackId=%d", it->iTrackId));
                return false;
            }
        }
    }
    else
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::CheckForOMA2UsageApproval - Invalid CPM ContentType"));
        OSCL_ASSERT(false);
    }
    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForOMA2UsageApproval - All Tracks Authorized"));
    return true;
}

void PVMFMP4FFParserNode::ResetOMA2Flags()
{
    if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
    {
        /* Look for a track that needs authorization */
        Oscl_Vector<PVMP4FFNodeTrackOMA2DRMInfo, OsclMemAllocator>::iterator it;
        for (it = iOMA2DRMInfoVec.begin(); it != iOMA2DRMInfoVec.end(); it++)
        {
            it->iOMA2TrackAuthorized = false;
            it->iOMA2TrackAuthorizationComplete = false;
            it->iOMA2TrackAuthorizationInProgress = false;
        }
    }
    else
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::ResetOMA2Flags - Invalid CPM ContentType"));
        OSCL_ASSERT(false);
    }
    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::ResetOMA2Flags - Complete"));
}

PVMFStatus  PVMFMP4FFParserNode::RequestUsage(PVMP4FFNodeTrackOMA2DRMInfo* aInfo)
{
    if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::RequestUsage - TrackId=%d", aInfo->iTrackId));
        aInfo->iOMA2TrackAuthorizationInProgress = true;
        PopulateOMA2DRMInfo(aInfo);
    }
    else if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
             (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
    {
        PopulateOMA1DRMInfo();
    }
    else if (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE)
    {
        if (!PopulatePIFFDRMInfo())
        {
            if (iProtectedFile == false)
                return PVMFSuccess; //not a protected content
            else
                return PVMFFailure;
        }
    }
    else
    {
        /* Error */
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::RequestUsage - Invalid CPM Type"));
        OSCL_LEAVE(PVMFErrNotSupported);
        return PVMFFailure;
    }

    iCPM->GetContentAccessFactory(iCPMSessionID, iCPMContentAccessFactory);

    if (iDataStreamReadCapacityObserver != NULL)
    {
        int32 leavecode = 0;
        OSCL_TRY(leavecode,
                 iCPMContentAccessFactory->SetStreamReadCapacityObserver(iDataStreamReadCapacityObserver););
        if (leavecode != 0)
        {
            PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::GetCPMContentAccessFactory: SetStreamReadCapacityObserver failed"));
        }
    }

    iCPMSequenceInProgress = true;
    iCPMRequestUsageId = iCPM->ApproveUsage(iCPMSessionID,
                                            iRequestedUsage,
                                            iApprovedUsage,
                                            iAuthorizationDataKvp,
                                            iUsageID);
    oWaitingOnLicense = true;
    return PVMFPending;
}

void PVMFMP4FFParserNode::PopulateOMA2DRMInfo(PVMP4FFNodeTrackOMA2DRMInfo* aInfo)
{
    //Cleanup any old key.
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

    int32 UseKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING)) + oscl_strlen(_STRLIT_CHAR(";track_id=0"));
    int32 AuthKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_AUTHORIZATION_DATA_KEY_STRING)) + oscl_strlen(_STRLIT_CHAR(";track_id=0"));
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

    char trackID[16];
    oscl_snprintf(trackID, 16, ";track_id=%d", aInfo->iTrackId);
    trackID[15] = '\0';

    OSCL_StackString<256> requestUsageKey;
    requestUsageKey += _STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING);
    requestUsageKey += trackID;

    oscl_strncpy(iRequestedUsage.key,
                 requestUsageKey.get_cstr(),
                 UseKeyLen);
    iRequestedUsage.key[UseKeyLen] = 0;
    iRequestedUsage.length = 0;
    iRequestedUsage.capacity = 0;
    iRequestedUsage.value.uint32_value =
        (BITMASK_PVMF_CPM_DRM_INTENT_PLAY |
         BITMASK_PVMF_CPM_DRM_INTENT_PAUSE |
         BITMASK_PVMF_CPM_DRM_INTENT_SEEK_FORWARD |
         BITMASK_PVMF_CPM_DRM_INTENT_SEEK_BACK);

    oscl_strncpy(iApprovedUsage.key,
                 requestUsageKey.get_cstr(),
                 UseKeyLen);
    iApprovedUsage.key[UseKeyLen] = 0;
    iApprovedUsage.length = 0;
    iApprovedUsage.capacity = 0;
    iApprovedUsage.value.uint32_value = 0;

    OSCL_StackString<512> authorizationKey;
    authorizationKey += _STRLIT_CHAR(PVMF_CPM_AUTHORIZATION_DATA_KEY_STRING);
    authorizationKey += trackID;

    oscl_strncpy(iAuthorizationDataKvp.key,
                 authorizationKey.get_cstr(),
                 AuthKeyLen);
    iAuthorizationDataKvp.key[AuthKeyLen] = 0;

    iAuthorizationDataKvp.length = aInfo->iDRMInfoSize;
    iAuthorizationDataKvp.capacity = aInfo->iDRMInfoSize;
    iAuthorizationDataKvp.value.pUint8_value = aInfo->iDRMInfo;
}

void PVMFMP4FFParserNode::PopulateOMA1DRMInfo()
{
    //Cleanup any old key.
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

void PVMFMP4FFParserNode::SendUsageComplete()
{
    iCPMSequenceInProgress = true;
    iCPMUsageCompleteCmdId = iCPM->UsageComplete(iCPMSessionID, iUsageID);
}

void PVMFMP4FFParserNode::CloseCPMSession()
{
    iCPMCloseSessionCmdId = iCPM->CloseSession(iCPMSessionID);
}

void PVMFMP4FFParserNode::ResetCPM()
{
    iCPMResetCmdId = iCPM->Reset();
}

PVMFStatus
PVMFMP4FFParserNode::CheckCPMCommandCompleteStatus(PVMFCommandId aID,
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
            PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted -  CPM Reset Failed"));
            OSCL_ASSERT(false);
        }

    }
    else if (aID == iCPMGetLicenseInterfaceCmdId)
    {
        if (aStatus == PVMFErrNotSupported)
        {
            /* License Interface is Optional */
            status = PVMFSuccess;
        }
    }
    else if (aID == iCPMRegisterContentCmdId)
    {
        if (aStatus == PVMFErrNotSupported)
        {
            /* CPM doesnt care about this content */
            status = PVMFErrNotSupported;
        }
    }
    else if (aID == iCPMRequestUsageId)
    {
        if (GetCPMSourceDataIntentAt(0) == BITMASK_PVMF_SOURCE_INTENT_GETMETADATA)
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

void PVMFMP4FFParserNode::CPMCommandCompleted(const PVMFCmdResp& aResponse)
{
    iCPMSequenceInProgress = false;
    PVMFCommandId id = aResponse.GetCmdId();
    PVMFStatus status =
        CheckCPMCommandCompleteStatus(id, aResponse.GetCmdStatus());

    //if CPM comes back as PVMFErrNotSupported then by pass rest of the CPM
    //sequence. Fake success here so that node doesnt treat this as an error
    if (id == iCPMRegisterContentCmdId && status == PVMFErrNotSupported)
    {
        /* Unsupported format - Treat it like unprotected content */
        PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - Unknown CPM Format - Ignoring CPM"));
        if (CheckForMP4HeaderAvailability() != PVMFPending)
        {
            CompleteInit(iCurrentCommand);
            if (IsCommandInProgress(iCancelCommand))
            {
                CommandComplete(iCancelCommand, PVMFSuccess);
            }
        }
        return;
    }

    if (status != PVMFSuccess)
    {
        /*
        * If any other command fails, the sequence fails.
        */
        if (aResponse.GetEventData() == NULL)
        {
            // If any command fails, the sequence fails.
            CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
        }
        else
        {
            // Need to pass EventData (=License URL) up to UI.
            CommandComplete(iCurrentCommand,
                            aResponse.GetCmdStatus());
        }
        /*
         * if there was any pending cancel, it was waiting on
         * this command to complete-- so the cancel is now done.
         */
        if (IsCommandInProgress(iCancelCommand))
        {
            CommandComplete(iCancelCommand, PVMFSuccess);
        }
    }
    else
    {
        //process the response, and issue the next command in
        //the sequence.

        PVMFCommandId id = aResponse.GetCmdId();

        if (id == iCPMInitCmdId)
        {
            PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - CPM Init complete"));
            OpenCPMSession();
        }
        else if (id == iCPMOpenSessionCmdId)
        {
            PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - CPM OpenSession complete"));
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
            GetCPMContentType();
            if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
                    (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
            {
                iProtectedFile = true;
                GetCPMMetaDataExtensionInterface();
                PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - OMA1 - Register content complete"));
                PVMFStatus requestUsageStatus = RequestUsage(NULL);
                if (requestUsageStatus != PVMFPending)
                {
                    CompleteInit(iCurrentCommand, requestUsageStatus);
                }
            }
            else if ((iCPMContentType == PVMF_CPM_FORMAT_OMA2) || (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE))
            {
                iProtectedFile = true;
                GetCPMMetaDataExtensionInterface();
                if (CheckForMP4HeaderAvailability() != PVMFPending)
                {
                    PVMP4FFNodeTrackOMA2DRMInfo* oma2trackInfo = NULL;

                    PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
                    if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
                    {
                        dsFactory = iDataStreamFactory;
                    }
                    PVMFStatus status = InitNextValidClipInPlaylist(NULL, 0, dsFactory);
                    if (status == PVMFSuccess)
                    {
                        if ((iCPMContentType == PVMF_CPM_FORMAT_OMA2) && (CheckForOMA2AuthorizationComplete(oma2trackInfo) == PVMFPending))
                        {
                            PVMFStatus status = RequestUsage(oma2trackInfo);
                            if (status != PVMFPending)
                            {
                                CompleteInit(iCurrentCommand, status);
                            }
                        }
                        else if (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE)
                        {
                            //We will fetch the info about PIFF in the call to RequestUsage itself
                            PVMFStatus status = RequestUsage(NULL);
                            if (status != PVMFPending)
                            {
                                CompleteInit(iCurrentCommand, status);
                            }
                        }
                    }
                    else
                    {
                        CompleteInit(iCurrentCommand, status);
                        return;
                    }
                }
            }
            else
            {
                /* Unsupported format - Treat it like unprotected content */
                PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - Unknown CPM Format - Ignoring CPM"));
                if (CheckForMP4HeaderAvailability() != PVMFPending)
                {
                    CompleteInit(iCurrentCommand);
                }
            }
        }
        else if (id == iCPMRequestUsageId)
        {
            PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - Request Usage complete"));
            //End of Node Init sequence.
            OSCL_ASSERT(iCurrentCommand.iCmd == PVMF_GENERIC_NODE_INIT);
            oWaitingOnLicense = false;
            if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
            {
                //for OMA2 PDCF we need to authorize track by track
                OMA2TrackAuthorizationComplete();
                PVMP4FFNodeTrackOMA2DRMInfo* oma2trackInfo = NULL;
                PVMFStatus status = CheckForOMA2AuthorizationComplete(oma2trackInfo);
                if (status == PVMFPending)
                {
                    PVMFStatus requestUsageStatus = RequestUsage(oma2trackInfo);
                    if (requestUsageStatus != PVMFPending)
                    {
                        CompleteInit(iCurrentCommand, requestUsageStatus);
                    }
                }
                else if (status == PVMFSuccess)
                {
                    //All tracks authorized, complete init
                    CompleteInit(iCurrentCommand);
                }
                else
                {
                    PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - CheckForOMA2AuthorizationComplete Failed"));
                    OSCL_ASSERT(false);
                }
            }
            else if (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE)
            {
                CompleteInit(iCurrentCommand);
            }
            else
            {
                if (aResponse.GetCmdStatus() == PVMFSuccess)
                {
                    //OMA1 content - Authorization complete
                    if (CheckForMP4HeaderAvailability() != PVMFPending)
                    {
                        CompleteInit(iCurrentCommand);
                    }
                }
                else
                {
                    //we are just doing metadata (not necessarily from mp4 file header,
                    // say just drm metadata), so complete init from here
                    CompleteInit(iCurrentCommand);
                }
            }
        }
        else if (id == iCPMUsageCompleteCmdId)
        {
            if (iProtectedFile == true)
            {
                PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - CPM Usage Complete done"));
                CloseCPMSession();
            }
            else
            {
                PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::CPMCommandCompleted - Unprotected Content - Can't Send Usage Complete"));
                OSCL_ASSERT(false);
            }
        }
        else if (id == iCPMCloseSessionCmdId)
        {
            PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted - Close CPM Session complete"));
            ResetCPM();
        }
        else if (id == iCPMResetCmdId)
        {
            PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted -  CPM Reset complete"));
            //End of Node Reset sequence
            OSCL_ASSERT(iCurrentCommand.iCmd == PVMF_GENERIC_NODE_RESET);
            status = CompleteReset();
            CommandComplete(iCurrentCommand, status);
        }
        else if (id == iCPMGetMetaDataValuesCmdId)
        {
            PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CPMCommandCompleted -  CPM GetMetaDataValues complete"));
            /* End of GetNodeMetaDataValues */
            OSCL_ASSERT(iCurrentCommand.iCmd == PVMF_GENERIC_NODE_GETNODEMETADATAVALUES);
            CompleteGetMetaDataValues();
        }
        else
        {
            /* Unknown cmd - error */
            CommandComplete(iCurrentCommand,
                            PVMFFailure);
        }
    }
}

PVMFStatus PVMFMP4FFParserNode::GetFileOffsetForAutoResume(TOsclFileOffset& aOffset, bool aPortsAvailable)
{
    TOsclFileOffset offset = 0;
    if (aPortsAvailable == false)
    {
        int32 iNumTracks = iPlaybackParserObj->getNumTracks();
        uint32 iIdList[16];
        if (iNumTracks != iPlaybackParserObj->getTrackIDList(iIdList, iNumTracks))
        {
            return PVMFFailure;
        }
        for (int32 i = 0; i < iNumTracks; i++)
        {
            uint32 trackID = iIdList[i];
            /* Convert PVMF_MP4FFPARSER_NODE_PSEUDO_STREAMING_BUFFER_DURATION_IN_MS to media timescale */
            MediaClockConverter mcc(1000, iJitterBufferDurationInMs);
            uint64 mediats64 = mcc.get_converted_ts64(iPlaybackParserObj->getTrackMediaTimescale(trackID));

            TOsclFileOffset trackOffset = 0;
            int32 retVal =
                iPlaybackParserObj->getOffsetByTime(trackID, mediats64, &trackOffset, iJitterBufferDurationInMs);

            if (retVal != EVERYTHING_FINE)
            {
                PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::GetFileOffsetForAutoResume1 - getOffsetByTime Failed - TrackId=%u, TS=%u, RetVal=%d",  trackID, Oscl_Int64_Utils::get_uint64_lower32(mediats64), retVal));
                return PVMFFailure;
            }

            if (trackOffset > offset)
            {
                offset = trackOffset;
            }
        }
    }
    else
    {
        Oscl_Vector<PVMP4FFNodeTrackPortInfo, OsclMemAllocator>::iterator it;

        for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
        {
            /* Convert PVMF_MP4FFPARSER_NODE_PSEUDO_STREAMING_BUFFER_DURATION_IN_MS to media timescale */
            MediaClockConverter mcc(1000, iJitterBufferDurationInMs);
            uint64 mediats64 = mcc.get_converted_ts64(iPlaybackParserObj->getTrackMediaTimescale(it->iTrackId));

            TOsclFileOffset trackOffset = 0;
            uint64 ts = it->iTimestamp + mediats64;
            int32 retVal =
                iPlaybackParserObj->getOffsetByTime(it->iTrackId, ts, &trackOffset, iJitterBufferDurationInMs);

            if (retVal != EVERYTHING_FINE)
            {
                PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::GetFileOffsetForAutoResume2 - getOffsetByTime Failed - TrackId=%d, TS=%d, RetVal=%d",  it->iTrackId, Oscl_Int64_Utils::get_uint64_lower32(ts), retVal));
                return PVMFFailure;
            }

            if (trackOffset > offset)
            {
                offset = trackOffset;
            }
        }
    }
    aOffset = offset;
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::GetFileOffsetForAutoResume(TOsclFileOffset& aOffset, PVMP4FFNodeTrackPortInfo* aInfo)
{
    TOsclFileOffset offset = 0;
    /* Convert PVMF_MP4FFPARSER_NODE_PSEUDO_STREAMING_BUFFER_DURATION_IN_MS to media timescale */
    MediaClockConverter mcc(1000, iJitterBufferDurationInMs);
    uint64 mediats64 = mcc.get_converted_ts64(iPlaybackParserObj->getTrackMediaTimescale(aInfo->iTrackId));

    TOsclFileOffset trackOffset = 0;
    uint64 ts = aInfo->iTimestamp + mediats64;
    int32 retVal =
        iPlaybackParserObj->getOffsetByTime(aInfo->iTrackId, ts, &trackOffset, iJitterBufferDurationInMs);

    if (retVal != EVERYTHING_FINE)
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::GetFileOffsetForAutoResume by port - getOffsetByTime Failed - TrackId=%d, TS=%d, RetVal=%d",  aInfo->iTrackId, Oscl_Int64_Utils::get_uint64_lower32(ts), retVal));
        return PVMFFailure;
    }

    if (trackOffset > offset)
    {
        offset = trackOffset;
    }
    aOffset = offset;
    return PVMFSuccess;
}


void PVMFMP4FFParserNode::DataStreamCommandCompleted(const PVMFCmdResp& aResponse)
{
    for (uint32 i = 0; i < iNodeTrackPortList.size(); ++i)
    {
        switch (iNodeTrackPortList[i].iState)
        {
                /*
                ** Its possible that track reports inufficient data but does not report Underflow and
                ** justs sets the timer. If we receive DataStreamCommandComplete means we need not report
                ** underflow now, so set track state as GETDATA, cancel the timer and schedule Node to
                ** retrieve data. This case will only happen if CheckForUnderflow returns Pending for
                ** any track.
                */
            case PVMP4FFNodeTrackPortInfo::TRACKSTATE_INSUFFICIENTDATA:
                iNodeTrackPortList[i].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                iUnderFlowCheckTimer->Cancel(PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID);
                Reschedule();
                break;

            default:
                // nothing to do
                break;
        }

    }
    if (autopaused)
    {
        if (aResponse.GetCmdStatus() == PVMFSuccess)
        {
            autopaused = false;
            for (uint32 ii = 0; ii < iNodeTrackPortList.size(); ++ii)
            {
                switch (iNodeTrackPortList[ii].iState)
                {
                    case PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE:
                        iNodeTrackPortList[ii].iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_TRANSMITTING_GETDATA;
                        break;

                    default:
                        // nothing to do
                        break;
                }
                Reschedule();
            }

            // report data ready only if underflow was not suppressed earlier
            if (iUnderFlowEventReported == true)
            {

                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DataStreamReadCapacityNotificationCallBack() Sending PVMFInfoDataReady event"));
                ReportMP4FFParserInfoEvent(PVMFInfoDataReady);
                iUnderFlowEventReported = false;
            }
            if (iExternalDownload == true)
            {
                if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_PREPARE)
                {
                    //we could be waiting on this call back to complete prepare
                    CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
                }
            }
            // Schedule AO to run again
            Reschedule();
            return;
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::DataStreamReadCapacityNotificationCallBack() Reporting failure"));
            if (iExternalDownload == true)
            {
                if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_PREPARE)
                {
                    //we could be waiting on this call back to complete prepare
                    CommandComplete(iCurrentCommand, aResponse.GetCmdStatus());
                }
                else
                {
                    ReportMP4FFParserErrorEvent(PVMFErrResource);
                }
            }
            else
            {
                ReportMP4FFParserErrorEvent(PVMFErrResource);
            }
        }
    }
    else if (iCurrentCommand.iCmd == PVMF_GENERIC_NODE_INIT)
    {
        /* Callbacks intiated  as part of node init */
        if (aResponse.GetCmdId() == iRequestReadCapacityNotificationID)
        {
            iDataStreamRequestPending = false;
            /*
             * Can come here only if we are doing PDL / PPB.
             * Init is still pending and datastream callback has completed.
             * A few possible scenarios here:
             * - PDL/PPB (DRM/Non-DRM) - We are waiting on movie atom to be downloaded.
             * - OMA2 PDCF DRM clip - were waiting on more data to parse the movie atom,
             * CPM seq not complete since we need contents of movie atom to authorize
             * with CPM
             */
            if (iCPM)
            {
                if ((iCPMContentType == PVMF_CPM_FORMAT_OMA2) || (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE))
                {
                    PVMP4FFNodeTrackOMA2DRMInfo* oma2trackInfo = NULL;

                    PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
                    if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
                    {
                        dsFactory = iDataStreamFactory;
                    }
                    PVMFStatus status = InitNextValidClipInPlaylist(NULL, 0, dsFactory);
                    if (status == PVMFSuccess)
                    {
                        if ((iCPMContentType == PVMF_CPM_FORMAT_OMA2) && (CheckForOMA2AuthorizationComplete(oma2trackInfo) == PVMFPending))
                        {
                            RequestUsage(oma2trackInfo);
                            return;
                        }
                        else if (iCPMContentType == PVMF_CPM_FORMAT_ACCESS_BEFORE_AUTHORIZE)
                        {
                            if (RequestUsage(NULL) != PVMFPending)
                            {
                                CompleteInit(iCurrentCommand);
                                return;
                            }
                        }
                    }
                }
            }
            if (PVMFPending != CheckForMP4HeaderAvailability())
            {
                CompleteInit(iCurrentCommand);
            }
        }
        else
        {
            /* unrecognized callback */
            OSCL_ASSERT(false);
        }
    }
    else
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::DataStreamReadCapacityNotificationCallBack() in non-autopaused state"));
        // Schedule AO to run again
        Reschedule();
    }
    return;
}


void PVMFMP4FFParserNode::DataStreamInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    //if Datadownload is complete then send PVMFInfoBufferingComplete event from DS to parser node
    if (aEvent.GetEventType() == PVMFInfoBufferingComplete)
    {
        iDownloadComplete = true;
        ReportInfoEvent(PVMFInfoBufferingComplete);
    }
}

void PVMFMP4FFParserNode::DataStreamErrorEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_ASSERT(false);
}


bool PVMFMP4FFParserNode::GetTrackPortInfoForTrackID(PVMP4FFNodeTrackPortInfo*& aInfo,
        uint32 aTrackID)
{
    aInfo = NULL;
    Oscl_Vector<PVMP4FFNodeTrackPortInfo, OsclMemAllocator>::iterator it;
    for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
    {
        if (it->iTrackId == (int32)aTrackID)
        {
            aInfo = it;
            return true;
        }
    }
    return false;
}

bool PVMFMP4FFParserNode::GetTrackPortInfoForPort(PVMP4FFNodeTrackPortInfo*& aInfo,
        PVMFPortInterface* aPort)
{
    aInfo = NULL;
    Oscl_Vector<PVMP4FFNodeTrackPortInfo, OsclMemAllocator>::iterator it;
    for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
    {
        if (it->iPortInterface == aPort)
        {
            aInfo = it;
            return true;
        }
    }
    return false;
}

void PVMFMP4FFParserNode::PassDatastreamFactory(PVMFDataStreamFactory& aFactory,
        int32 aFactoryTag,
        const PvmfMimeString* aFactoryConfig)
{
    OSCL_UNUSED_ARG(aFactoryTag);
    OSCL_UNUSED_ARG(aFactoryConfig);
    // Fasttrack download does not use data streams
    if (iFastTrackSession)
        return;

    if (iDataStreamFactory  == NULL)
    {
        iDataStreamFactory  = &aFactory;
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        PVInterface* iFace =
            iDataStreamFactory->CreatePVMFCPMPluginAccessInterface(uuid);
        if (iFace != NULL)
        {
            iDataStreamInterface = OSCL_STATIC_CAST(PVMIDataStreamSyncInterface*, iFace);
            iDataStreamInterface->OpenSession(iDataStreamSessionID, PVDS_READ_ONLY);
        }
    }
    else
    {
        OSCL_ASSERT(false);
    }
}

void
PVMFMP4FFParserNode::PassDatastreamReadCapacityObserver(PVMFDataStreamReadCapacityObserver* aObserver)
{
    iDataStreamReadCapacityObserver = aObserver;
}


PVMFStatus PVMFMP4FFParserNode::CheckForMP4HeaderAvailability()
{
    // this method is called to check the number of downloaded bytes
    // this always return success for local playback
    if (iFastTrackSession == true
       ) return PVMFSuccess;

    if (iDataStreamInterface != NULL)
    {
        /*
         * First check if we have minimum number of bytes to recognize
         * the file and determine the header size.
         */
        TOsclFileOffset currCapacity = 0;
        TOsclFileOffset tempCurrCapacity = 0;
        PvmiDataStreamStatus lStatus = iDataStreamInterface->QueryReadCapacity(iDataStreamSessionID,
                                       tempCurrCapacity);
        currCapacity = tempCurrCapacity;

        if (lStatus != PVDS_END_OF_STREAM && currCapacity < (TOsclFileOffset)MP4_MIN_BYTES_FOR_GETTING_MOVIE_HDR_SIZE)
        {
            iRequestReadCapacityNotificationID =
                iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                        *this,
                        MP4_MIN_BYTES_FOR_GETTING_MOVIE_HDR_SIZE);
            iDataStreamRequestPending = true;
            return PVMFPending;
        }


        bool isProgressivelyPlayable = false;
        MP4_ERROR_CODE retCode =
            IMpeg4File::GetMetaDataSize(iDataStreamFactory,
                                        isProgressivelyPlayable,
                                        iMP4HeaderSize);

        if (retCode == EVERYTHING_FINE)
        {
            if (isProgressivelyPlayable == true)
            {
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForMP4HeaderAvailability() - MetaData ends at file offset %d", iMP4HeaderSize));
                iProgressivelyPlayable = true;
                // inform data stream that a range of bytes needs to be cached persistently (offset 0, size of moov atom)
                iDataStreamInterface->MakePersistent(0, (uint32)iMP4HeaderSize);

                if (currCapacity < (TOsclFileOffset)iMP4HeaderSize)
                {
                    iRequestReadCapacityNotificationID =
                        iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                                *this,
                                (uint32)iMP4HeaderSize);
                    iDataStreamRequestPending = true;
                    return PVMFPending;
                }
                else
                {
                    return PVMFSuccess;
                }
            }
            else
            {
                iProgressivelyPlayable = false;
                if (iDownloadComplete == false)
                {
                    // For PDL use-case always send the Info event and wait for download complete
                    // And
                    // For PS use-cases we need to check if the total file can be downloaded into the cache or not
                    // before sending the info event and wait for download complete OR
                    // send the failure for Init command.
                    uint32 cacheSize = 0;
                    cacheSize = iDataStreamInterface->QueryBufferingCapacity();
                    if (cacheSize != 0)
                    {
                        // If the content length is not known OR
                        // If the file cannot be completely downloaded to the cache
                        // (means File Size greater than cache size), error out.
                        if ((iDownloadFileSize == 0) || (iDownloadFileSize > (TOsclFileOffset)cacheSize))
                        {
                            // ToDo - Improvement - If DownloadFileSize is not known player can still try to Download file in cache,
                            // if file is downloaded completely in cache player can play the file.
                            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0,
                                                                 "PVMFMP4FFParserNode::CheckForMP4HeaderAvailability() - MP4 File Not Progressively Streamable - Cannot be downloaded completely to cache, error out"));
                            return PVMFErrContentInvalidForProgressivePlayback;
                        }
                        else
                        {
                            // The content length is known and the complete file can be downloaded to the cache,
                            // wait for download complete.
                            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0,
                                                                 "PVMFMP4FFParserNode::CheckForMP4HeaderAvailability() - MP4 File Not Progressively Streamable - File can be downloaded completely to cache"));
                        }
                    }
                    PVUuid uuid = PVMFFileFormatEventTypesUUID;
                    int32 infocode = PVMFMP4FFParserInfoNotPseudostreamableFile;
                    ReportMP4FFParserInfoEvent(PVMFInfoRemoteSourceNotification, NULL, &uuid, &infocode);
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0,
                                                         "PVMFMP4FFParserNode::CheckForMP4HeaderAvailability() - MP4 File Not Progressively Playable - Waiting for download complete"));
                }
                else
                {
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0,
                                                         "PVMFMP4FFParserNode::CheckForMP4HeaderAvailability() - MP4 File Not Progressively Playable - Download completed - Attempting to parse header"));
                    return PVMFSuccess;
                }
                /*
                 * Wait for download complete
                 */
                if (download_progress_interface != NULL)
                {
                    uint32 nptTsinMS = MAX32BITUINT;

                    if ((NULL != iDataStreamInterface) && (0 != iDataStreamInterface->QueryBufferingCapacity()))
                    {
                        // if progressive streaming, playResumeNotifcation is guaranteed to be called
                        // with the proper download complete state, ignore the current download status
                        bool dlcomplete = false;
                        download_progress_interface->requestResumeNotification(nptTsinMS, dlcomplete);
                    }
                    else
                    {
                        download_progress_interface->requestResumeNotification(nptTsinMS, iDownloadComplete);
                    }
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForMP4HeaderAvailability() - Auto Pause Triggered, TS = %d", nptTsinMS));
                    return PVMFPending;
                }
                else if (iExternalDownload == true)
                {
                    TOsclFileOffset maxSize = (TOsclFileOffset)MAX_TOSCLFILEOFFSET_VALUE;
                    iRequestReadCapacityNotificationID =
                        iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                                *this,
                                (uint32)maxSize);
                    iDataStreamRequestPending = true;
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForMP4HeaderAvailability() - Auto Pause Triggered - Waiting on download complete"));
                    return PVMFPending;
                }
            }
        }
        else if (retCode == INSUFFICIENT_DATA)
        {
            iRequestReadCapacityNotificationID =
                iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                        *this,
                        ((uint32)iMP4HeaderSize + MP4_MIN_BYTES_FOR_GETTING_MOVIE_HDR_SIZE));
            iDataStreamRequestPending = true;
            return PVMFPending;
        }
        else
        {
            PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::CheckForMP4HeaderAvailability() - GetMetaDataSize Failed %d", retCode));
        }

        return PVMFFailure;
    }
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::CheckForUnderFlow(PVMP4FFNodeTrackPortInfo* aInfo)
{
    uint32 timebase32 = 0;
    uint32 clientClock32 = 0;
    bool overload = 0;
    if (iClientPlayBackClock != NULL)
    {
        iClientPlayBackClock->GetCurrentTime32(clientClock32, overload, PVMF_MEDIA_CLOCK_MSEC, timebase32);
    }
    else
    {
        return PVMFFailure;
    }

    MediaClockConverter clock_conv(*(aInfo->iClockConverter));
    clock_conv.update_clock(aInfo->iTimestamp);
    uint32 msec = clock_conv.get_converted_ts(MILLISECOND_TIMESCALE);
    if (clientClock32 <= msec)
    {
        uint32 diff32 = (msec - clientClock32);
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForUnderFlow - Diff Bet Client Clock(%d) and Track Media TS(%d) = %d", clientClock32, msec, diff32));
        if (diff32 < PVMP4FF_UNDERFLOW_THRESHOLD_IN_MS || aInfo->iFormatType == PVMF_MIME_AMR_IETF || aInfo->iFormatType == PVMF_MIME_MPEG4_AUDIO)
        {
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForUnderFlow - Time To Auto Pause"));
            return PVMFSuccess;
        }
        else
        {
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForUnderFlow - Setting UnderFlow Timer"));
            iUnderFlowCheckTimer->Request(PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID,
                                          0,
                                          PVMP4FF_UNDERFLOW_STATUS_EVENT_CYCLES,
                                          this);
        }
    }
    else
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::CheckForUnderFlow - Client Clock Greater than Media TS, Clock=%d, MaxMTS=%d",  clientClock32, msec));
        OSCL_ASSERT(false);
    }

    /* Check if all tracks are autopaused. If so, it is time to autopause the node */
    bool oAllTracksAutoPaused = true;
    Oscl_Vector<PVMP4FFNodeTrackPortInfo, OsclMemAllocator>::iterator it;
    for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
    {
        if (it->iState != PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE)
        {
            oAllTracksAutoPaused = false;
        }
    }
    if (oAllTracksAutoPaused == true)
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForUnderFlow - All Tracks AutoPaused - Time To Auto Pause"));
        return PVMFSuccess;
    }
    //not yet time to autopause
    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::CheckForUnderFlow - Wait on player clock for Auto Pause"));
    return PVMFPending;
}


PVMFStatus PVMFMP4FFParserNode::ReportUnderFlow()
{
    /* Check if all tracks are autopaused. If so, it is time to autopause the node */
    bool oAllTracksAutoPaused = true;
    Oscl_Vector<PVMP4FFNodeTrackPortInfo, OsclMemAllocator>::iterator it;
    for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
    {
        if (it->iState != PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE)
        {
            oAllTracksAutoPaused = false;
        }
    }
    if (oAllTracksAutoPaused == true)
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::ReportUnderFlow - Reporting UnderFlow"));
        ReportInfoEvent(PVMFInfoUnderflow);
        iUnderFlowEventReported = true;
        return PVMFSuccess;
    }


    uint32 minTS = MAX32BITUINT;
    for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
    {
        if (it->iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE)
        {
            MediaClockConverter clock_conv(*(it->iClockConverter));
            clock_conv.update_clock(it->iTimestamp);
            uint32 msec = clock_conv.get_converted_ts(MILLISECOND_TIMESCALE);
            if (msec < minTS)
            {
                minTS = msec;
            }
        }
    }
    uint32 timebase32 = 0;
    uint32 clientClock32 = 0;
    bool overload = 0;

    if (iClientPlayBackClock != NULL)
        iClientPlayBackClock->GetCurrentTime32(clientClock32, overload, PVMF_MEDIA_CLOCK_MSEC, timebase32);


    TOsclFileOffset currentFileSize = 0;
    MP4_ERROR_CODE code = iPlaybackParserObj->GetCurrentFileSize(currentFileSize);
    if (code != EVERYTHING_FINE)
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::ReportUnderFlow - GetCurrentFileSize Failed - Ret=%d",  code));
        return PVMFFailure;
    }

    iUnderFlowEventReported = false;
    uint32 currNPT = 0;
    convertSizeToTime(currentFileSize, currNPT);

    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::ReportUnderFlow - ClientClock = %d", clientClock32));
    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::ReportUnderFlow - NPTInMS = %d, FileSize = %d", currNPT, currentFileSize));
    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::ReportUnderFlow - Min Media TS = %d", minTS));

    if (clientClock32 <= minTS)
    {
        uint32 diff32 = (minTS - clientClock32);
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::ReportUnderFlow - Diff Bet Client Clock and Min Media TS = %d", diff32));
        if (diff32 < PVMP4FF_UNDERFLOW_THRESHOLD_IN_MS)
        {
            /* If parserNode is reporting Underflow, all tracks should be in auto-paused state */
            for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
            {
                if (it->iState != PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE)
                {
                    it->iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE;
                }
            }
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::ReportUnderFlow - Reporting UnderFlow"));
            ReportInfoEvent(PVMFInfoUnderflow);
            iUnderFlowEventReported = true;
        }
        else
        {
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::ReportUnderFlow - Setting UnderFlow Timer"));
            iUnderFlowCheckTimer->Request(PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID,
                                          0,
                                          PVMP4FF_UNDERFLOW_STATUS_EVENT_CYCLES,
                                          this);
        }
    }
    else
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::ReportUnderFlow - Client Clock Greater than Min Media TS, Clock=%d, MaxMTS=%d",  clientClock32, minTS));
        OSCL_ASSERT(false);
    }
    return PVMFSuccess;
}

void PVMFMP4FFParserNode::TimeoutOccurred(int32 timerID,
        int32 timeoutInfo)
{
    OSCL_UNUSED_ARG(timeoutInfo);

    if (timerID == PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID)
    {
        if (autopaused == true && iUnderFlowEventReported == false)
        {
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - UnderFlow TimeOut - Still AutoPaused"));
            TOsclFileOffset currentFileSize = 0;
            MP4_ERROR_CODE code = iPlaybackParserObj->GetCurrentFileSize(currentFileSize);
            if (code == EVERYTHING_FINE)
            {
                uint32 currNPT = 0;
                convertSizeToTime(currentFileSize, currNPT);

                uint32 minTS = MAX32BITUINT;
                Oscl_Vector<PVMP4FFNodeTrackPortInfo, OsclMemAllocator>::iterator it;
                for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
                {
                    if (it->iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE)
                    {
                        MediaClockConverter clock_conv(*(it->iClockConverter));
                        clock_conv.update_clock(it->iTimestamp);
                        uint32 msec = clock_conv.get_converted_ts(MILLISECOND_TIMESCALE);
                        if (msec < minTS)
                        {
                            minTS = msec;
                        }
                    }
                }
                uint32 timebase32 = 0;
                uint32 clientClock32 = 0;
                bool overload = 0;
                if (iClientPlayBackClock != NULL)
                    iClientPlayBackClock->GetCurrentTime32(clientClock32, overload, PVMF_MEDIA_CLOCK_MSEC, timebase32);


                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - ClientClock = %d", clientClock32));
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - NPTInMS = %d,  FileSize = %d", currNPT, currentFileSize));
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - Min Media TS = %d", minTS));

                if (clientClock32 <= minTS)
                {
                    uint32 diff32 = (minTS - clientClock32);
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - Diff Bet Client Clock and Min Media TS = %d", diff32));
                    if (diff32 < PVMP4FF_UNDERFLOW_THRESHOLD_IN_MS)
                    {
                        /* If parserNode is reporting Underflow, all tracks should be in auto-paused state */
                        for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
                        {
                            if (it->iState != PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE)
                            {
                                it->iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE;
                            }
                        }

                        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - Reporting UnderFlow"));
                        ReportInfoEvent(PVMFInfoUnderflow);
                        iUnderFlowEventReported = true;
                    }
                    else
                    {
                        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - Requesting Additional Time Out"));
                        iUnderFlowCheckTimer->Request(PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID,
                                                      0,
                                                      PVMP4FF_UNDERFLOW_STATUS_EVENT_CYCLES,
                                                      this);
                    }
                }
                else
                {
                    PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::TimeoutOccurred - Client Clock Greater than Min Media TS, Clock=%d, MaxMTS=%d",  clientClock32, minTS));
                    OSCL_ASSERT(false);
                }

            }
            else
            {
                PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::TimeoutOccurred - GetCurrentFileSize Failed - Ret=%d",  code));
                ReportErrorEvent(PVMFErrResource);
            }
        }
        else if (iDownloadComplete == false && iUnderFlowEventReported == false)
        {
            uint32 timebase32 = 0;
            uint32 clientClock32 = 0;
            bool overload = 0;
            uint32 msec = 0;
            if (iClientPlayBackClock != NULL)
            {
                iClientPlayBackClock->GetCurrentTime32(clientClock32, overload, PVMF_MEDIA_CLOCK_MSEC, timebase32);
            }

            Oscl_Vector<PVMP4FFNodeTrackPortInfo, OsclMemAllocator>::iterator it;
            for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
            {
                if (it->iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_INSUFFICIENTDATA)
                {
                    MediaClockConverter clock_conv(*(it->iClockConverter));
                    clock_conv.update_clock(it->iTimestamp);
                    msec = clock_conv.get_converted_ts(MILLISECOND_TIMESCALE);
                }
            }

            if (clientClock32 <= msec)
            {
                uint32 diff32 = (msec - clientClock32);
                PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - Diff Bet Client Clock(%d) and Track Media TS(%d) = %d", clientClock32, msec, diff32));
                if (diff32 < PVMP4FF_UNDERFLOW_THRESHOLD_IN_MS)
                {
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - Time To Auto Pause"));
                    for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
                    {
                        if (it->iState == PVMP4FFNodeTrackPortInfo::TRACKSTATE_INSUFFICIENTDATA)
                        {
                            it->iState = PVMP4FFNodeTrackPortInfo::TRACKSTATE_DOWNLOAD_AUTOPAUSE;
                        }
                    }
                    autopaused = true;
                    ReportUnderFlow();
                }
                else
                {
                    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - Setting UnderFlow Timer"));
                    iUnderFlowCheckTimer->Request(PVMF_MP4FFPARSERNODE_UNDERFLOW_STATUS_TIMER_ID,
                                                  0,
                                                  PVMP4FF_UNDERFLOW_STATUS_EVENT_CYCLES,
                                                  this);
                }
            }
            else
            {
                PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::TimeoutOccurred - Client Clock Greater than Media TS, Clock=%d, MaxMTS=%d",  clientClock32, msec));
                //should never happen
                OSCL_ASSERT(false);
            }
            PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::TimeoutOccurred - UnderFlow TimeOut - After AutoResume"));
        }
    }
    return;
}

void
PVMFMP4FFParserNode::LogMediaData(PVMFSharedMediaDataPtr data,
                                  PVMFPortInterface* aPort)
{
    PVMP4FFNodeTrackPortInfo* trackInfoPtr = NULL;
    if (!GetTrackPortInfoForPort(trackInfoPtr, aPort))
    {
        return;
    }

    bool ologSizeTS = false;

    /* Get Format Specific Info, if any */
    if (trackInfoPtr->oFormatSpecificInfoLogged == false)
    {
        uint32 size = trackInfoPtr->iFormatSpecificConfig.getMemFragSize();
        if (size > 0)
        {
            PVMF_MP4FFPARSERNODE_LOGBIN(trackInfoPtr->iPortLogger, (0, 1, size, trackInfoPtr->iFormatSpecificConfig.getMemFragPtr()));
        }
        trackInfoPtr->oFormatSpecificInfoLogged = true;
    }

    ologSizeTS = false;

    PVMFMediaData* mediaData = data.GetRep();
    if (mediaData != NULL)
    {
        /* Log Media Fragments */
        uint32 numMediaFragments = mediaData->getNumFragments();
        for (uint32 i = 0; i < numMediaFragments; i++)
        {
            OsclRefCounterMemFrag memFrag;
            mediaData->getMediaFragment(i, memFrag);
            if (ologSizeTS)
            {
                PVMF_MP4FFPARSERNODE_LOGBIN(trackInfoPtr->iPortLogger, (0, 1, sizeof(uint32), memFrag.getMemFragSize()));
                PVMF_MP4FFPARSERNODE_LOGBIN(trackInfoPtr->iPortLogger, (0, 1, sizeof(uint32), data->getTimestamp()));
            }
            PVMF_MP4FFPARSERNODE_LOGBIN(trackInfoPtr->iPortLogger, (0, 1, memFrag.getMemFragSize(), memFrag.getMemFragPtr()));
        }
    }
}

void PVMFMP4FFParserNode::LogDiagnostics()
{
    if (iDiagnosticsLogged == false)
    {
        iDiagnosticsLogged = true;
        Oscl_Vector<PVMP4FFNodeTrackPortInfo, OsclMemAllocator>::iterator it;

        if (&iNodeTrackPortList)
        {
            it = iNodeTrackPortList.begin();
            for (it = iNodeTrackPortList.begin(); it != iNodeTrackPortList.end(); it++)
            {
                PVMF_MP4FFPARSERNODE_LOGDIAGNOSTICS((0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
                PVMF_MP4FFPARSERNODE_LOGDIAGNOSTICS((0, "PVMFMP4FFParserNode - Time Taken in Read MP4 File  =%d", iTimeTakenInReadMP4File));
                PVMF_MP4FFPARSERNODE_LOGDIAGNOSTICS((0, "PVMFMP4FFParserNode - Read Media Sample Minimum Time  =%2d", it->iMinTime));
                PVMF_MP4FFPARSERNODE_LOGDIAGNOSTICS((0, "PVMFMP4FFParserNode - Read Media Sample Maximum Time  =%2d", it->iMaxTime));
                PVMF_MP4FFPARSERNODE_LOGDIAGNOSTICS((0, "PVMFMP4FFParserNode - Read Media Sample Total Time  =%2d", it->iSumTime));

                uint64 avg_time = 0;
                if ((it->iNumTimesMediaSampleRead) > 0)
                    avg_time = Oscl_Int64_Utils::get_uint64_lower32(it->iSumTime) / (it->iNumTimesMediaSampleRead);

                PVMF_MP4FFPARSERNODE_LOGDIAGNOSTICS((0, "PVMFMP4FFParserNode - Read Media Sample Avg Time  =%2d", avg_time));
                PVMF_MP4FFPARSERNODE_LOGDIAGNOSTICS((0, "PVMFMP4FFParserNode - Number of Sample Read each time  =%d", it->iNumSamples));


            }
        }
    }
}

PVMFStatus PVMFMP4FFParserNode::GetVideoFrameWidth(uint32 aClipIndex, uint32 aId, int32& aWidth, int32& aDisplayWidth)
{
    int32 height = 0;
    int32 width = 0;
    int32 display_width = 0;
    int32 display_height = 0;

    IMpeg4File* parserObj = GetParserObjAtIndex(aClipIndex);

    OSCL_HeapString<OsclMemAllocator> trackMIMEType;

    parserObj->getTrackMIMEType(aId, trackMIMEType);

    if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0)
    {
        H263DecoderSpecificInfo *ptr =
            (H263DecoderSpecificInfo *)parserObj->getTrackDecoderSpecificInfoAtSDI(aId, 0);

        //In case of H263 display dimension and decode dimesnion cannot be different,so pass
        //aDisplayHeight as parser level info and Height from decoder utility

        if (ptr != NULL)
        {
            if (ptr->getMaxWidth() > 0)
            {
                aDisplayWidth = display_width = (int32)(ptr->getMaxWidth());
            }
        }

        if (width == 0)
        {
            //get width from the first frame
            MediaMetaInfo info;
            uint32 numSamples = 1;
            int32 retval = EVERYTHING_FINE;
            retval = parserObj->peekNextBundledAccessUnits(aId,
                     &numSamples,
                     &info);
            if ((retval == EVERYTHING_FINE || END_OF_TRACK == retval) && numSamples > 0)
            {
                uint32 sampleSize = info.len;
                if (sampleSize > 0)
                {
                    uint8* sampleBuf = PVMF_BASE_NODE_ARRAY_NEW(uint8, sampleSize);

                    oscl_memset(&iGau.buf, 0, sizeof(iGau.buf));
                    oscl_memset(&iGau.info, 0, sizeof(iGau.info));
                    iGau.free_buffer_states_when_done = 0;
                    iGau.numMediaSamples = 1;
                    iGau.buf.num_fragments = 1;
                    iGau.buf.buf_states[0] = NULL;
                    iGau.buf.fragments[0].ptr = (OsclAny*)sampleBuf;
                    iGau.buf.fragments[0].len = sampleSize;
                    iGau.frameNum = 0;
                    retval =
                        parserObj->getNextBundledAccessUnits(aId,
                                                             &numSamples,
                                                             &iGau);
                    if ((retval == EVERYTHING_FINE) || (retval == END_OF_TRACK))
                    {
                        mp4StreamType streamType;
                        streamType.data = sampleBuf;
                        streamType.numBytes = sampleSize;
                        streamType.bytePos = 0;
                        streamType.bitBuf = 0;
                        streamType.dataBitPos = 0;
                        streamType.bitPos = 32;

                        int16 status =
                            iDecodeShortHeader(&streamType,
                                               (int32*) & width,
                                               (int32*) & height,
                                               (int32*) & display_width,
                                               (int32*) & display_height);
                        if (status != 0)
                        {
                            return PVMFFailure;
                        }

                        aWidth = width;
                        if (aDisplayWidth == 0)
                        {
                            aDisplayWidth = display_width;
                        }
                    }
                    parserObj->resetPlayback();
                    PVMF_BASE_NODE_ARRAY_DELETE(sampleBuf);
                }
            }
        }

    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0)
    {
        display_width = parserObj->getVideoFrameWidth(aId);
        if (display_width > 0)
        {
            aDisplayWidth = display_width;
        }

        uint32 specinfosize =
            parserObj->getTrackDecoderSpecificInfoSize(aId);
        if (specinfosize != 0)
        {
            // Retrieve the decoder specific info from file parser
            uint8* specinfoptr =
                parserObj->getTrackDecoderSpecificInfoContent(aId);
            int32 profile, level = 0;// this info is discarded
            int16 status =
                iGetAVCConfigInfo(specinfoptr,
                                  (int32)specinfosize,
                                  (int32*) & width,
                                  (int32*) & height,
                                  (int32*) & display_width,
                                  (int32*) & display_height,
                                  (int32*) & profile,
                                  (int32*) & level);
            if (status != 0)
            {
                return PVMFFailure;
            }

            aWidth = width;

            if (aDisplayWidth == 0)
                aDisplayWidth = display_width;
        }
    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0)
    {
        display_width = parserObj->getVideoFrameWidth(aId);
        if (display_width > 0)
        {
            aDisplayWidth = display_width;
        }

        uint32 specinfosize =
            parserObj->getTrackDecoderSpecificInfoSize(aId);
        if (specinfosize != 0)
        {
            // Retrieve the decoder specific info from file parser
            uint8* specinfoptr =
                parserObj->getTrackDecoderSpecificInfoContent(aId);
            int16 status =
                iGetM4VConfigInfo(specinfoptr,
                                  (int32)specinfosize,
                                  (int32*) & width,
                                  (int32*) & height,
                                  (int32*) & display_width,
                                  (int32*) & display_height);
            if (status != 0)
            {
                return PVMFFailure;
            }

            aWidth = width;

            if (aDisplayWidth == 0)
                aDisplayWidth = display_width;
        }
    }

    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::GetVideoFrameHeight(uint32 aClipIndex, uint32 aId, int32& aHeight, int32& aDisplayHeight)
{
    int32 height = 0;
    int32 width = 0;
    int32 display_width = 0;
    int32 display_height = 0;

    IMpeg4File* parserObj = GetParserObjAtIndex(aClipIndex);

    OSCL_HeapString<OsclMemAllocator> trackMIMEType;
    parserObj->getTrackMIMEType(aId, trackMIMEType);

    if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0)
    {
        H263DecoderSpecificInfo *ptr =
            (H263DecoderSpecificInfo *)parserObj->getTrackDecoderSpecificInfoAtSDI(aId, 0);

        //In case of H263 display dimension and decode dimesnion cannot be different,so pass
        //aDisplayHeight as parser level info and Height from decoder utility
        if (ptr != NULL)
        {
            if (ptr->getMaxHeight() > 0)
            {
                aDisplayHeight = display_height = (int32)(ptr->getMaxHeight());
            }
        }
        if (height == 0)
        {
            //get height from the first frame
            MediaMetaInfo info;
            uint32 numSamples = 1;
            int32 retval = EVERYTHING_FINE;
            retval = parserObj->peekNextBundledAccessUnits(aId,
                     &numSamples,
                     &info);
            if ((retval == EVERYTHING_FINE || END_OF_TRACK == retval) && numSamples > 0)
            {
                uint32 sampleSize = info.len;
                if (sampleSize > 0)
                {
                    uint8* sampleBuf = PVMF_BASE_NODE_ARRAY_NEW(uint8, sampleSize);

                    oscl_memset(&iGau.buf, 0, sizeof(iGau.buf));
                    oscl_memset(&iGau.info, 0, sizeof(iGau.info));
                    iGau.free_buffer_states_when_done = 0;
                    iGau.numMediaSamples = 1;
                    iGau.buf.num_fragments = 1;
                    iGau.buf.buf_states[0] = NULL;
                    iGau.buf.fragments[0].ptr = (OsclAny*)sampleBuf;
                    iGau.buf.fragments[0].len = sampleSize;
                    iGau.frameNum = 0;
                    retval =
                        parserObj->getNextBundledAccessUnits(aId,
                                                             &numSamples,
                                                             &iGau);
                    if ((retval == EVERYTHING_FINE) || (retval == END_OF_TRACK))
                    {
                        mp4StreamType streamType;
                        streamType.data = sampleBuf;
                        streamType.numBytes = sampleSize;
                        streamType.bytePos = 0;
                        streamType.bitBuf = 0;
                        streamType.dataBitPos = 0;
                        streamType.bitPos = 32;

                        int16 status =
                            iDecodeShortHeader(&streamType,
                                               (int32*) & width,
                                               (int32*) & height,
                                               (int32*) & display_width,
                                               (int32*) & display_height);
                        if (status != 0)
                        {
                            return PVMFFailure;
                        }

                        if (aDisplayHeight == 0)
                        {
                            aDisplayHeight = display_height;
                        }
                        aHeight = height;
                    }
                    parserObj->resetPlayback();
                    PVMF_BASE_NODE_ARRAY_DELETE(sampleBuf);
                }
            }
        }
    }
    if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0)
    {
        display_height = parserObj->getVideoFrameHeight(aId);
        if (display_height > 0)
        {
            aDisplayHeight = display_height;
        }

        uint32 specinfosize =
            parserObj->getTrackDecoderSpecificInfoSize(aId);
        if (specinfosize != 0)
        {
            // Retrieve the decoder specific info from file parser
            uint8* specinfoptr =
                parserObj->getTrackDecoderSpecificInfoContent(aId);
            int profile, level = 0; // this info is discarded here
            int16 status =
                iGetAVCConfigInfo(specinfoptr,
                                  (int32)specinfosize,
                                  (int32*) & width,
                                  (int32*) & height,
                                  (int32*) & display_width,
                                  (int32*) & display_height,
                                  (int32*) & profile,
                                  (int32*) & level);
            if (status != 0)
            {
                return PVMFFailure;
            }

            aHeight = height;

            if (aDisplayHeight == 0)
                aDisplayHeight = display_height;
        }
    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0)
    {
        display_height = parserObj->getVideoFrameHeight(aId);
        if (display_height > 0)
        {
            aDisplayHeight = display_height;
        }

        uint32 specinfosize =
            parserObj->getTrackDecoderSpecificInfoSize(aId);
        if (specinfosize != 0)
        {
            // Retrieve the decoder specific info from file parser
            uint8* specinfoptr =
                parserObj->getTrackDecoderSpecificInfoContent(aId);
            int16 status =
                iGetM4VConfigInfo(specinfoptr,
                                  (int32)specinfosize,
                                  (int32*) & width,
                                  (int32*) & height,
                                  (int32*) & display_width,
                                  (int32*) & display_height);
            if (status != 0)
            {
                return PVMFFailure;
            }

            aHeight = height;

            if (aDisplayHeight == 0)
                aDisplayHeight = display_height;

        }
    }

    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::PopulateVideoDimensions(uint32 aClipIndex, uint32 aId)
{
    VideoTrackDimensionInfo vidDimInfo;
    vidDimInfo.iTrackId = aId;
    vidDimInfo.iDisplayWidth = 0;
    vidDimInfo.iDisplayHeight = 0;

    if (PVMFSuccess != GetVideoFrameWidth(aClipIndex, aId, vidDimInfo.iWidth, vidDimInfo.iDisplayWidth))
    {
        return PVMFFailure;
    }
    if (PVMFSuccess != GetVideoFrameHeight(aClipIndex, aId, vidDimInfo.iHeight, vidDimInfo.iDisplayHeight))
    {
        return PVMFFailure;
    }
    iVideoDimensionInfoVec.push_back(vidDimInfo);
    return PVMFSuccess;
}

int32 PVMFMP4FFParserNode::FindVideoWidth(uint32 aId)
{
    int32 width = 0;
    Oscl_Vector<VideoTrackDimensionInfo, OsclMemAllocator>::iterator it;
    for (it = iVideoDimensionInfoVec.begin(); it != iVideoDimensionInfoVec.end(); it++)
    {
        if (it->iTrackId == aId)
        {
            width = it->iWidth;
        }
    }
    return width;
}

int32 PVMFMP4FFParserNode::FindVideoHeight(uint32 aId)
{
    int32 height = 0;
    Oscl_Vector<VideoTrackDimensionInfo, OsclMemAllocator>::iterator it;
    for (it = iVideoDimensionInfoVec.begin(); it != iVideoDimensionInfoVec.end(); it++)
    {
        if (it->iTrackId == aId)
        {
            height = it->iHeight;
        }
    }
    return height;
}

int32 PVMFMP4FFParserNode::FindVideoDisplayWidth(uint32 aId)
{
    int32 display_width = 0;
    Oscl_Vector<VideoTrackDimensionInfo, OsclMemAllocator>::iterator it;
    for (it = iVideoDimensionInfoVec.begin(); it != iVideoDimensionInfoVec.end(); it++)
    {
        if (it->iTrackId == aId)
        {
            display_width = it->iDisplayWidth;
        }
    }
    return display_width;
}

int32 PVMFMP4FFParserNode::FindVideoDisplayHeight(uint32 aId)
{
    int32 display_height = 0;
    Oscl_Vector<VideoTrackDimensionInfo, OsclMemAllocator>::iterator it;
    for (it = iVideoDimensionInfoVec.begin(); it != iVideoDimensionInfoVec.end(); it++)
    {
        if (it->iTrackId == aId)
        {
            display_height = it->iDisplayHeight;
        }
    }
    return display_height;
}



PVMFStatus PVMFMP4FFParserNode::FindBestThumbnailKeyFrame(uint32 aId, uint32& aKeyFrameNum)
{
    aKeyFrameNum = PVMFFF_DEFAULT_THUMB_NAIL_SAMPLE_NUMBER;

    // Use the MP4 FF API to retrieve the number of sync samples in a track
    uint32 numsamples = 0;
    int32 retval = iPlaybackParserObj->getTimestampForRandomAccessPoints(aId,
                   &numsamples,
                   NULL,
                   NULL);

    if (retval == 1 && numsamples > 0)
    {
        /* It is possible that for some big contents the number of sync samples is a very big
        ** value. For these contents retrieval of timesatamps and frame numbers of sync sample
        ** will take long time. With NUMSAMPLES_BEST_THUMBNAIL_MODE Parser will try to find
        ** best thumbnail frame out of first 10 sync samples.
        */
        if (numsamples > NUMSAMPLES_BEST_THUMBNAIL_MODE)
        {
            numsamples = NUMSAMPLES_BEST_THUMBNAIL_MODE;
        }
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode:FindBestThumbnailKeyFrame - NumKeySamples=%d, TrackID=%d", numsamples, aId));

        // Allocate memory for the info
        uint64* syncts = PVMF_BASE_NODE_ARRAY_NEW(uint64, numsamples);
        uint32* syncfrnum = PVMF_BASE_NODE_ARRAY_NEW(uint32, numsamples);
        if (syncts == NULL || syncfrnum == NULL)
        {
            if (syncts)
            {
                PVMF_BASE_NODE_ARRAY_DELETE(syncts);
            }
            if (syncfrnum)
            {
                PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
            }
            return PVMFErrNoMemory;
        }

        // Retrieve the list of timestamp and frame numbers for sync samples.
        retval =
            iPlaybackParserObj->getTimestampForRandomAccessPoints(aId,
                    &numsamples,
                    syncts,
                    syncfrnum);
        if (retval != 1)
        {
            // Error
            if (syncts)
            {
                PVMF_BASE_NODE_ARRAY_DELETE(syncts);
            }
            if (syncfrnum)
            {
                PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
            }
            numsamples = 0;
            aKeyFrameNum = 0;
            return PVMFSuccess;
        }

        uint64 trackduration;
        uint32 samplecount;
        trackduration = iPlaybackParserObj->getTrackMediaDuration(aId);
        samplecount = iPlaybackParserObj->getSampleCountInTrack(aId);

        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode:FindBestThumbnailKeyFrame - TrackDuration=%2d", Oscl_Int64_Utils::get_uint64_lower32(trackduration)));
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode:FindBestThumbnailKeyFrame - TotalNumSamples=%d", samplecount));
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode:FindBestThumbnailKeyFrame - BitRate=%d", (uint32)iPlaybackParserObj->getTrackAverageBitrate(aId)));

        //go thru the key frame list and determine the optimal key frame
        uint32 keySampleNum = syncfrnum[0];
        uint32 keySampleSize = 0;
        iPlaybackParserObj->getSampleSizeAt(aId, (int32)keySampleNum, keySampleSize);
        aKeyFrameNum = 0;
        for (uint32 i = 1; i < numsamples; i++)
        {
            uint32 keySampleNumNext = syncfrnum[i];
            uint32 keySampleSizeNext = 0;
            iPlaybackParserObj->getSampleSizeAt(aId, (int32)keySampleNumNext, keySampleSizeNext);

            if (keySampleSizeNext > keySampleSize)
            {
                keySampleSize = keySampleSizeNext;
                aKeyFrameNum = i;
            }
        }
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode:FindBestThumbnailKeyFrame - Picked Best KeyFrame=%d", aKeyFrameNum));
        if (syncts)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncts);
        }
        if (syncfrnum)
        {
            PVMF_BASE_NODE_ARRAY_DELETE(syncfrnum);
        }
    }
    else if (retval == 2)
    {
        // All samples are sync samples
        if (numsamples > NUMSAMPLES_BEST_THUMBNAIL_MODE)
        {
            numsamples = NUMSAMPLES_BEST_THUMBNAIL_MODE;
        }
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode:FindBestThumbnailKeyFrame - NumKeySamples=%d, TrackID=%d", numsamples, aId));

        //go thru the key frame list and determine the optimal key frame
        uint32 maxKeySampleSize = 0;
        uint32 keySampleSize = 0;
        aKeyFrameNum = 0;
        for (uint32 i = 0; i < numsamples; i++)
        {
            keySampleSize = 0;
            iPlaybackParserObj->getSampleSizeAt(aId, i, keySampleSize);
            if (keySampleSize > maxKeySampleSize)
            {
                maxKeySampleSize = keySampleSize;
                aKeyFrameNum = i;
            }
        }
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode:FindBestThumbnailKeyFrame - Picked Best KeyFrame=%d", aKeyFrameNum));
    }
    else
    {
        PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode:FindBestThumbnailKeyFrame - No Samples present in SyncSample Table"));
        numsamples = 0;
        aKeyFrameNum = 0;
        return PVMFFailure;
    }
    return PVMFSuccess;
}

void PVMFMP4FFParserNode::NotificationsInterfaceDestroyed()
{
    iClockNotificationsInf = NULL;
}

void PVMFMP4FFParserNode::ClockStateUpdated()
{
    if ((iExternalDownload && iUnderFlowEventReported) ||
            (autopaused && download_progress_interface != NULL))
    {
        // Don't let anyone start the clock while the source node is in underflow
        if (iClientPlayBackClock != NULL)
        {
            if (iClientPlayBackClock->GetState() == PVMFMediaClock::RUNNING)
            {
                iClientPlayBackClock->Pause();
            }
        }
    }
}

PVMFStatus PVMFMP4FFParserNode::HandleExtensionAPICommands()
{
    PVMFStatus status = PVMFErrNotSupported;
    switch (iCurrentCommand.iCmd)
    {
        case PVMF_GENERIC_NODE_GETNODEMETADATAVALUES:
            status = DoGetNodeMetadataValues();
            break;
        case PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION:
        {
            if (iPlaylistRepositioning)
            {
                iPlaylistRepositioning = false;
                status = DoSetDataSourcePositionPlaylist();
            }
            else
            {
                status = DoSetDataSourcePosition();
            }
        }
        break;
        case PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION:
            status = DoQueryDataSourcePosition();
            break;
        case PVMF_GENERIC_NODE_SET_DATASOURCE_RATE:
            status = DoSetDataSourceRate();
            break;
        case PVMF_GENERIC_NODE_CAPCONFIG_SETPARAMS:
        {
            PvmiMIOSession session;
            PvmiKvp* aParameters;
            int num_elements;
            PvmiKvp** ppRet_kvp;
            iCurrentCommand.Parse(session, aParameters, num_elements, ppRet_kvp);
            setParametersSync(NULL, aParameters, num_elements, *ppRet_kvp);
            CommandComplete(iCurrentCommand, PVMFSuccess);
        }
        break;
        case PVMF_GENERIC_NODE_SET_DATASOURCE_DIRECTION:
            status = DoSetDataSourceDirection();
            break;

        default:
            // unknown command type, do nothing
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::HandleExtensionAPICommands() - Unknown Cmd type %d; Cmd Id %d", iCurrentCommand.iCmd, iCurrentCommand.iId));
            OSCL_ASSERT(false);
            break;
    }
    return status;
}


bool PVMFMP4FFParserNode::SendEndOfClipCommand(PVMP4FFNodeTrackPortInfo& aTrackPortInfo)
{
    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();
    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_EOC_FORMAT_ID);

    uint32 timestamp = aTrackPortInfo.iMediaData->getTimestamp();
    sharedMediaCmdPtr->setTimestamp(timestamp);
    sharedMediaCmdPtr->setSeqNum(aTrackPortInfo.iSeqNum++);
    sharedMediaCmdPtr->setStreamID(iStreamID);

    // set format specific info
    struct EOCInfo* fragPtr = (struct EOCInfo*)iClipInfoList[iPlaybackClipIndex].iClipInfo.iEOCFormatSpecificInfo.getMemFragPtr();
    fragPtr->framesToFollow = iClipInfoList[iPlaybackClipIndex].iClipInfo.iFramesToFollowEOC;
    sharedMediaCmdPtr->setFormatSpecificInfo(iClipInfoList[iPlaybackClipIndex].iClipInfo.iEOCFormatSpecificInfo);

    // Convert media command to media message
    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);

    if (aTrackPortInfo.iPortInterface->QueueOutgoingMsg(mediaMsgOut) != PVMFSuccess)
    {
        // Output queue is busy, so wait for the output queue being ready
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMFMP4FFParserNode::SendEndOfClipCommand: Outgoing queue busy. "));
        return false;
    }

    // EOC was sent successfully
    iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasEOCFrame = false;

    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SendEndOfClipCommand() EOC sent - Mime=%s, StreamId=%d, TS=%d",
                                         aTrackPortInfo.iMimeType.get_cstr(),
                                         iStreamID,
                                         timestamp));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SendEndOfClipCommand() EOC sent StreamId=%d ", iStreamID));

    return true;
}

bool PVMFMP4FFParserNode::SendBeginOfClipCommand(PVMP4FFNodeTrackPortInfo& aTrackPortInfo)
{
    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();
    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_BOC_FORMAT_ID);

    aTrackPortInfo.iClockConverter->update_clock(aTrackPortInfo.iTimestamp);
    uint32 timestamp = aTrackPortInfo.iClockConverter->get_converted_ts(1000);
    sharedMediaCmdPtr->setTimestamp(timestamp);
    sharedMediaCmdPtr->setSeqNum(aTrackPortInfo.iSeqNum++);
    sharedMediaCmdPtr->setStreamID(iStreamID);
    // set format specific info
    sharedMediaCmdPtr->setFormatSpecificInfo(iClipInfoList[iPlaybackClipIndex].iClipInfo.iBOCFormatSpecificInfo);

    // Convert media command to media message
    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);

    if (aTrackPortInfo.iPortInterface->QueueOutgoingMsg(mediaMsgOut) != PVMFSuccess)
    {
        // Output queue is busy, so wait for the output queue being ready
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMFMP4FFParserNode::SendBeginOfClipCommand: Outgoing queue busy. "));
        return false;
    }

    // BOC was sent successfully
    iClipInfoList[iPlaybackClipIndex].iClipInfo.iHasBOCFrame = false;

    PVMF_MP4FFPARSERNODE_LOGDATATRAFFIC((0, "PVMFMP4FFParserNode::SendBeginOfClipCommand() BOC sent - Mime=%s, StreamId=%d, TS=%d",
                                         aTrackPortInfo.iMimeType.get_cstr(),
                                         iStreamID,
                                         timestamp));
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::SendBeginOfClipCommand() BOC sent StreamId=%d ", iStreamID));

    return true;
}


bool PVMFMP4FFParserNode::setProtocolInfo(Oscl_Vector<PvmiKvp*, OsclMemAllocator>& aInfoKvpVec)
{
    if (aInfoKvpVec.empty())
    {
        return false;
    }

// to get around the build issue, we extract this part from the following long for loop within OSCL_TRY block

    int32 err = 0;
    // catch the vector push_back() exception
    OSCL_TRY(err,
             PVMFMP4MfraInfoUpdate mfra;
             for (uint32 i = 0; i < aInfoKvpVec.size(); i++)
{
    if ((aInfoKvpVec[i]) && (aInfoKvpVec[i]->key))
        {
            if (oscl_strstr(aInfoKvpVec[i]->key, PVSMOOTHSTREAMING_TOTAL_NUM_MOOFS_KEY))
            {
                iTotalMoofFrags = aInfoKvpVec[i]->value.uint32_value;
            }
            else if (oscl_strstr(aInfoKvpVec[i]->key, PVSMOOTHSTREAMING_MOOF_MFRA_INFO_UPDATE_KEY))
            {
                mfra = *((PVMFMP4MfraInfoUpdate*)aInfoKvpVec[i]->value.key_specific_value);
            }
            else if (oscl_strstr(aInfoKvpVec[i]->key, PROGRESSIVE_STREAMING_IS_BYTE_SEEK_NOT_SUPPORTED_STRING))
            {
                iIsByteSeekNotSupported = aInfoKvpVec[i]->value.bool_value;
            }
        }
    }

    // push to vector in case of mp4 library object is not instantiated
    if (mfra.isInfoValid()) iMoofInfoUpdateVec.push_back(mfra);

            );
    if (err) return false;

    if (iPlaybackParserObj)
    {
        while (!iMoofInfoUpdateVec.empty())
        {
            PVMFMP4MfraInfoUpdate moofInfo(iMoofInfoUpdateVec.front());
            iPlaybackParserObj->SetMoofInfo(moofInfo.iMoofTrackID,
                                            moofInfo.iMoofIndex, moofInfo.iMoofOffset, moofInfo.iMoofTime);
            iMoofInfoUpdateVec.erase(iMoofInfoUpdateVec.begin());
        }
    }

    return true;
}

void PVMFMP4FFParserNode::AudioSinkEvent(PVMFStatus aEvent, uint32 aClipIndex)
{
    if (aEvent == PVMFInfoEndOfData)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFMP4FFParserNode::AudioSinkEvent() EOS ClipIndex %d", aClipIndex));

        ReleaseMP4FileParser(aClipIndex);
    }
    else if (aEvent == PVMFInfoStartOfData)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFMP4FFParserNode::AudioSinkEvent() BOS ClipIndex %d", aClipIndex));
    }
    else
    {
        // unknown event, log it
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFMP4FFParserNode::AudioSinkEvent() unknown event %d ClipIndex %d", aEvent, aClipIndex));
    }
}

IMpeg4File* PVMFMP4FFParserNode::GetParserObjAtIndex(int32 aClipIndex)
{
    if ((aClipIndex < 0) || ((uint32)aClipIndex >= iClipInfoList.size()))
    {
        return NULL;
    }
    return iClipInfoList[aClipIndex].iParserObj;
}

PVMFStatus PVMFMP4FFParserNode::ConstructMP4FileParser(PVMFStatus* aStatus, int32 aClipIndex, PVMFCPMPluginAccessInterfaceFactory* aCPMFactory)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::ConstructMP4FileParser() In ClipIndex[%d]", aClipIndex));

    IMpeg4File* mp4ParserObj = NULL;
    {
        // only CommandComplee for the first clip in a playlist
        mp4ParserObj = IMpeg4File::readMP4File(iClipInfoList[aClipIndex].iClipInfo.GetSourceURL(),
                                               aCPMFactory,
                                               iClipInfoList[aClipIndex].iClipInfo.GetFileHandle(),
                                               iParsingMode, &iFileServer,
                                               iOpenFileOncePerTrack);
    }

    if (mp4ParserObj == NULL)
    {
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "IMpeg4File::readMP4File returns NULL"));

        if (NULL != aStatus)
        {
            *aStatus = PVMFErrNoMemory;
        }
        return PVMFFailure;
    }

    if (mp4ParserObj->MP4Success())
    {
        // only support audio-only playlists for now
        // find out how many tracks and what kind of tracks
        int32 numAudio = 0;
        int32 numVideo = 0;
        int32 numText = 0;
        uint32 idList[16];

        int32 numTracks = mp4ParserObj->getNumTracks();

        if (numTracks == mp4ParserObj->getTrackIDList(idList, numTracks))
        {
            for (int32 i = 0; i < numTracks; i++)
            {
                uint32 mediaType = mp4ParserObj->getTrackMediaType(idList[i]);
                if (mediaType == MEDIA_TYPE_VISUAL)
                {
                    numVideo++;
                }
                else if (mediaType == MEDIA_TYPE_AUDIO)
                {
                    numAudio++;
                }
                else if (mediaType == MEDIA_TYPE_TEXT)
                {
                    numText++;
                }
            }
        }

        if (aClipIndex == 0)
        {
            if ((numAudio == 0) || ((numText != 0) || (numVideo != 0)))
            {
                iFirstClipNonAudioOnly = true;
            }
        }
        else
        {
            // first clip was non-audio only or this clip is non-audio only, skip this clip
            if (iFirstClipNonAudioOnly ||
                    ((numAudio == 0) || ((numText != 0) || (numVideo != 0))))
            {
                PVMF_MP4FFPARSERNODE_LOGERROR((0, "PVMFMP4FFParserNode::ConstructMP4FileParser() Found non-audio-only clip in playlist"));

                // clean up the parser object
                IMpeg4File::DestroyMP4FileObject(mp4ParserObj);
                mp4ParserObj = NULL;

                if (NULL != aStatus)
                {
                    *aStatus = PVMFErrResource;
                }
                return PVMFFailure;
            }
        }

        // Init the moof count in case of smooth streaming
        mp4ParserObj->SetMoofAtomsCnt(iTotalMoofFrags);

        iClipInfoList[aClipIndex].iParserObj = mp4ParserObj;
    }
    else
    {
        // used for logging only
        PVUuid eventuuid;
        PVMFStatus eventcode;

        int32 mp4errcode = mp4ParserObj->GetMP4Error();
        if (!MapMP4ErrorCodeToEventCode(mp4errcode, eventuuid, eventcode))
        {
            eventuuid = PVMFFileFormatEventTypesUUID;
            eventcode = PVMFFFErrMisc;
        }
        PVMF_MP4FFPARSERNODE_LOGERROR((0, "IMpeg4File::readMP4File Failed - Err=%d eventcode=%d",
                                       mp4errcode, eventcode));
        OSCL_UNUSED_ARG(eventcode);

        // clean up the parser object
        IMpeg4File::DestroyMP4FileObject(mp4ParserObj);
        mp4ParserObj = NULL;

        if (NULL != aStatus)
        {
            *aStatus = PVMFErrResource;
        }
        return PVMFFailure;
    }

    if ((iExternalDownload == true) && (aClipIndex == 0))
    {
        // playlist playback of download is not supported hence there'd be only
        // one parser instance
        bool retval = mp4ParserObj->CreateDataStreamSessionForExternalDownload(iClipInfoList[aClipIndex].iClipInfo.GetSourceURL(),
                      aCPMFactory,
                      iClipInfoList[aClipIndex].iClipInfo.GetFileHandle(),
                      &iFileServer);

        if (!retval)
        {
            // clean up the parser object
            IMpeg4File::DestroyMP4FileObject(mp4ParserObj);
            mp4ParserObj = NULL;

            if (NULL != aStatus)
            {
                *aStatus = PVMFErrResource;
            }
            return PVMFFailure;
        }
    }

    PVMFStatus status = InitMetaData(aClipIndex);
    if (NULL != aStatus)
    {
        *aStatus = status;
    }

    // for audio-only playlist
    // match audio encoding params of subsequent clips with the first valid clip/track port setting
    // until RECONFIG for decnode is figured out
    if ((status == PVMFSuccess) && (!iFirstClipNonAudioOnly) && (iFirstValidClipIndex != -1) && (iClipInfoList.size() > 1))
    {
        bool matching = false;

        // should only have a track
        for (uint32 i = 0; i < iNodeTrackPortList.size(); i++)
        {
            if (iNodeTrackPortList[i].iFormatTypeInteger == iClipInfoList[aClipIndex].iFormatTypeInteger)
            {
                if ((iNodeTrackPortList[i].iFormatTypeInteger == PVMF_MP4_PARSER_NODE_AMR_IETF) ||
                        (iNodeTrackPortList[i].iFormatTypeInteger == PVMF_MP4_PARSER_NODE_AMRWB_IETF))
                {
                    matching = true;
                    break;
                }
                else if (iNodeTrackPortList[i].iFormatTypeInteger == PVMF_MP4_PARSER_NODE_MPEG4_AUDIO)
                {
                    if ((iNodeTrackPortList[i].iAACNumChans == iClipInfoList[aClipIndex].iAACNumChans) &&
                            (iNodeTrackPortList[i].iAACAudioObjectType == iClipInfoList[aClipIndex].iAACAudioObjectType) &&
                            (iNodeTrackPortList[i].iAACSampleRateIndex == iClipInfoList[aClipIndex].iAACSampleRateIndex) &&
                            (iNodeTrackPortList[i].iAACSamplesPerFrame == iClipInfoList[aClipIndex].iAACSamplesPerFrame))
                    {
                        matching = true;
                        break;
                    }
                }
            }
        }

        if (!matching)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::ConstructMP4FileParser Failed - Mismatched encoding params"));
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMP4FFNodeTrackPortInfo[0] iFormatTypeInteger=%d iAACNumChans=%d iAACAudioObjectType=%d iAACSampleRateIndex=%d iAACSamplesPerFrame=%d",
                            iNodeTrackPortList[0].iFormatTypeInteger, iNodeTrackPortList[0].iAACNumChans, iNodeTrackPortList[0].iAACAudioObjectType,
                            iNodeTrackPortList[0].iAACSampleRateIndex, iNodeTrackPortList[0].iAACSamplesPerFrame));
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "iClipInfoList[%d] iFormatTypeInteger=%d iAACNumChans=%d iAACAudioObjectType=%d iAACSampleRateIndex=%d iAACSamplesPerFrame=%d",
                            aClipIndex, iClipInfoList[aClipIndex].iFormatTypeInteger, iClipInfoList[aClipIndex].iAACNumChans, iClipInfoList[aClipIndex].iAACAudioObjectType,
                            iClipInfoList[aClipIndex].iAACSampleRateIndex, iClipInfoList[aClipIndex].iAACSamplesPerFrame));

            if (NULL != aStatus)
            {
                *aStatus = PVMFErrResource;
            }
            return PVMFFailure;
        }
    }

    // playlist is not supported for download/protected content
    // so the following needs to be done only for playback clip index 0
    // and when the node is initialized
    if ((iInterfaceState == EPVMFNodeInitialized || iInterfaceState == EPVMFNodeIdle)
            && aClipIndex == 0)
    {
        if (iCPMContentType == PVMF_CPM_FORMAT_OMA2)
        {
            status = InitOMA2DRMInfo(aClipIndex);
            if (NULL != aStatus)
            {
                *aStatus = status;
            }
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::ConstructMP4FileParser() Out"));
    return status;
}


PVMFStatus PVMFMP4FFParserNode::ReleaseMP4FileParser(int32 aClipIndex, bool cleanParserAtLastIndex)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::ReleaseMP4FileParser() In ClipIndex[%d]", aClipIndex));
    PVMFStatus status = PVMFFailure;
    // don't clean up parser object if there is only one clip in the list, unless it is requested
    if (aClipIndex >= 0 && (iNumClipsInPlayList > 1 || cleanParserAtLastIndex))
    {
        if (((uint32) aClipIndex == iClipInfoList[iClipInfoList.size() - 1].iClipInfo.GetClipIndex()) ||
                iPlaylistExhausted)
        {
            iPlaybackParserObj = NULL;
        }

        IMpeg4File* parserObj = GetParserObjAtIndex(aClipIndex);
        if (NULL != parserObj)
        {
            if (iMetadataParserObj == parserObj)
            {
                iMetadataParserObj = NULL;
            }
            if (iExternalDownload == true)
            {
                parserObj->DestroyDataStreamForExternalDownload();
            }
            IMpeg4File::DestroyMP4FileObject(parserObj);
            parserObj = NULL;
            iClipInfoList[aClipIndex].iParserObj = NULL;
        }

        iClipInfoList[aClipIndex].iAvailableMetadataKeys.clear();
        iClipInfoList[aClipIndex].iTotalID3MetaDataTagInValueList = 0;
        iClipInfoList[aClipIndex].iMetadataValueCount = 0;
        return status;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::ReleaseMP4FileParser() Out"));
    return status;
}

PVMFStatus PVMFMP4FFParserNode::InitNextValidClipInPlaylist(PVMFStatus* aStatus, int32 aSkipToTrack, PVMFDataStreamFactory* aDataStreamFactory)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFMP4FFParserNode::InitNextValidClipInPlaylist In"));

    PVMFStatus status = PVMFFailure;
    int32 clipIndex = -1;

    // reposition has occured, track needs to be changed to skipped location
    if (aSkipToTrack < 0)
    {
        // skip to next track
        clipIndex = (-1 == iPlaybackClipIndex) ? 0 : iPlaybackClipIndex + 1;
    }
    else
    {
        // skip to requested track
        clipIndex = aSkipToTrack;
    }

    while (PVMFSuccess != status && clipIndex <= (int32)(iNumClipsInPlayList - 1))
    {
        PVMFStatus retCode = ConstructMP4FileParser(aStatus, clipIndex, aDataStreamFactory);
        IMpeg4File* parserObj = GetParserObjAtIndex(clipIndex);
        if (retCode != PVMFSuccess ||
                parserObj == NULL)
        {
            if (iNumClipsInPlayList == 1)
            {
                // playback cant proceed if there's only one clip in the list
                // else skip shall happen to next clip.
                SetState(EPVMFNodeError);
                return PVMFErrResource;
            }
            else
            {
                uint8 localbuffer[4];
                oscl_memcpy(localbuffer, &clipIndex, sizeof(uint32));
                PVMFAsyncEvent asyncevent(PVMFInfoEvent, PVMFInfoClipCorrupted, NULL, OSCL_STATIC_CAST(PVInterface*, NULL), NULL, localbuffer, 4);
                ReportInfoEvent(asyncevent);
                clipIndex++; // index for next clip
                continue;
            }
        }

        // if local playback, retrieve gapless metadata if present
        if (NULL == aDataStreamFactory)
        {
            GetGaplessMetadata(clipIndex);
        }

        // report the info msg to observer
        uint32 metadataIndex = clipIndex;
        uint8 localbuffer[4];
        oscl_memcpy(localbuffer, &metadataIndex, sizeof(uint32));
        PVMFAsyncEvent asyncevent(PVMFInfoEvent, PVMFInfoMetadataAvailable, NULL, OSCL_STATIC_CAST(PVInterface*, NULL), NULL, localbuffer, 4);
        ReportInfoEvent(asyncevent);

        iNextInitializedClipIndex = iClipInfoList[clipIndex].iClipInfo.GetClipIndex();
        if (iPlaybackClipIndex == -1)
        {
            // set first valid clip in list
            iPlaybackClipIndex = iClipIndexForMetadata = iNextInitializedClipIndex;
            iPlaybackParserObj = iMetadataParserObj = parserObj;
        }
        iClipInfoList[clipIndex].iClipInfo.iIsInitialized = true;

        status = PVMFSuccess;
    }

    if (status != PVMFSuccess && clipIndex >= (int32)iNumClipsInPlayList)
    {
        // no more valid clips are found
        // clip list has exhausted, dont accept more playlist updates
        iPlaylistExhausted = true;
    }

    iInitNextClip = false;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFMP4FFParserNode::InitNextValidClipInPlaylist Out Initialized Index [%d]", iNextInitializedClipIndex));
    return status;
}


uint32 PVMFMP4FFParserNode::GetCurrentClipIndex()
{
    return iPlaybackClipIndex;
}

OSCL_wHeapString<OsclMemAllocator>& PVMFMP4FFParserNode::GetClipURLAt(uint32 aClipIndex)
{
    return iClipInfoList[aClipIndex].iClipInfo.GetSourceURL();
}

PVMFFormatType& PVMFMP4FFParserNode::GetClipFormatTypeAt(uint32 aClipIndex)
{
    return iClipInfoList[aClipIndex].iClipInfo.GetFormatType();
}

PVMFSourceContextData& PVMFMP4FFParserNode::GetSourceContextDataAt(uint32 aClipIndex)
{
    return iClipInfoList[aClipIndex].iClipInfo.iSourceContextData;
}

bool PVMFMP4FFParserNode::IsValidContextData(uint32 aClipIndex)
{
    return iClipInfoList[aClipIndex].iClipInfo.iSourceContextDataValid;
}

PVMFLocalDataSource& PVMFMP4FFParserNode::GetCPMSourceDataAt(uint32 aClipIndex)
{
    return iClipInfoList[aClipIndex].iClipInfo.iCPMSourceData;
}

uint32 PVMFMP4FFParserNode::GetCPMSourceDataIntentAt(uint32 aClipIndex)
{
    return iClipInfoList[aClipIndex].iClipInfo.iCPMSourceData.iIntent;
}

bool PVMFMP4FFParserNode::ValidateSourceInitializationParams(OSCL_wString& aSourceURL,
        PVMFFormatType& aSourceFormat,
        uint32 aClipIndex,
        PVMFSourceClipInfo aClipInfo)
{
    if (aClipInfo.GetFormatType() != aSourceFormat)
    {
        return true;
    }

    if (aClipInfo.GetClipIndex() != aClipIndex)
    {
        return true;
    }

    int32 res = oscl_strncmp(aSourceURL.get_cstr(), aClipInfo.GetSourceURL().get_cstr(), oscl_strlen(aSourceURL.get_cstr()));
    if (res != 0)
    {
        return true;
    }

    return false;
}
