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
#define PVMF_AACFFPARSER_NODE_H_INCLUDED


#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#include "pvmf_node_interface_impl.h"
#endif
#ifndef PVMF_MEMPOOL_H_INCLUDED
#include "pvmf_mempool.h"
#endif
#ifndef PVMF_MEDIA_PRESENTATION_INFO_H_INCLUDED
#include "pvmf_media_presentation_info.h"
#endif
#ifndef PVMF_AACFFPARSER_DEFS_H_INCLUDED
#include "pvmf_aacffparser_defs.h"
#endif
#ifndef PVMF_META_DATA_EXTENSION_H_INCLUDED
#include "pvmf_meta_data_extension.h"
#endif
#ifndef AACFILEPARSER_H_INCLUDED
#include "aacfileparser.h"
#endif
#ifndef PVMF_DOWNLOAD_PROGRESS_EXTENSION_H
#include "pvmf_download_progress_interface.h"
#endif
#ifndef PVMI_KVP_INCLUDED
#include "pvmi_kvp.h"
#endif
#ifndef PVMI_KVP_UTIL_H_INCLUDED
#include "pvmi_kvp_util.h"
#endif
#ifndef PVMF_MEDIA_PRESENTATION_INFO_H_INCLUDED
#include "pvmf_media_presentation_info.h"
#endif
#ifndef PVMF_SIMPLE_MEDIA_BUFFER_H_INCLUDED
#include "pvmf_simple_media_buffer.h"
#endif
#ifndef PVMF_MEDIA_FRAG_GROUP_H_INCLUDED
#include "pvmf_media_frag_group.h"
#endif
#ifndef CPM_H_INCLUDED
#include "cpm.h"
#endif
#ifndef PVMF_CPMPLUGIN_ACCESS_INTERFACE_H_INCLUDED
#include "pvmf_cpmplugin_access_interface.h"
#endif
#ifndef PVMF_AACFFPARSER_OUTPORT_H_INCLUDED
#include "pvmf_aacffparser_outport.h"
#endif
#ifndef PVMF_RESIZABLE_SIMPLE_MEDIAMSG_H_INCLUDED
#include "pvmf_resizable_simple_mediamsg.h"
#endif
#ifndef PVMF_CPMPLUGIN_LICENSE_INTERFACE_H_INCLUDED
#include "pvmf_cpmplugin_license_interface.h"
#endif
#ifndef OSCL_MIME_STRING_UTILS_H
#include "pv_mime_string_utils.h"
#endif
#ifndef __MEDIA_CLOCK_CONVERTER_H
#include "media_clock_converter.h"
#endif
#ifndef OSCL_EXCLUSIVE_PTR_H_INCLUDED
#include "oscl_exclusive_ptr.h"
#endif
#ifndef PVMF_LOCAL_DATA_SOURCE_H_INCLUDED
#include "pvmf_local_data_source.h"
#endif
#ifndef PVMF_SOURCE_CONTEXT_DATA_H_INCLUDED
#include "pvmf_source_context_data.h"
#endif
#ifndef PVMF_DATA_SOURCE_INIT_EXTENSION_H_INCLUDED
#include "pvmf_data_source_init_extension.h"
#endif
#ifndef PVMF_TRACK_SELECTION_EXTENSION_H_INCLUDED
#include "pvmf_track_selection_extension.h"
#endif
#ifndef PVMF_DATA_SOURCE_PLAYBACK_CONTROL_H_INCLUDED
#include "pvmf_data_source_playback_control.h"
#endif
#ifndef PVMI_DATASTREAMUSER_INTERFACE_H_INCLUDED
#include "pvmi_datastreamuser_interface.h"
#endif
#ifndef PVMF_FORMAT_PROGDOWNLOAD_SUPPORT_EXTENSION_H_INCLUDED
#include "pvmf_format_progdownload_support_extension.h"
#endif
#ifndef OSCL_SNPRINTF_H
#include "oscl_snprintf.h"
#endif

// Track info

// Allocator wrapper for the memory pool that saves the last block pointer allocated
// so it can be resized later
class TrackDataMemPoolProxyAlloc : public Oscl_DefAlloc
{
    public:
        TrackDataMemPoolProxyAlloc(OsclMemPoolResizableAllocator& aMemPool)
        {
            iMemPoolAllocPtr = &aMemPool;
            iLastAllocatedBlockPtr = NULL;
        }

        virtual ~TrackDataMemPoolProxyAlloc()
        {
        }

        OsclAny* allocate(const uint32 size)
        {
            OSCL_ASSERT(iMemPoolAllocPtr);
            iLastAllocatedBlockPtr = iMemPoolAllocPtr->allocate(size);
            return iLastAllocatedBlockPtr;
        }

        void deallocate(OsclAny* p)
        {
            OSCL_ASSERT(iMemPoolAllocPtr);
            iMemPoolAllocPtr->deallocate(p);
        }

        bool trim(uint32 aBytesToFree)
        {
            OSCL_ASSERT(iMemPoolAllocPtr);
            OSCL_ASSERT(iLastAllocatedBlockPtr);
            return iMemPoolAllocPtr->trim(iLastAllocatedBlockPtr, aBytesToFree);
        }

        OsclMemPoolResizableAllocator* iMemPoolAllocPtr;
        OsclAny* iLastAllocatedBlockPtr;
};

class MediaClockConverter;

class PVAACFFNodeTrackPortInfo : public OsclMemPoolFixedChunkAllocatorObserver,
        public OsclMemPoolResizableAllocatorObserver
{
    public:

        PVAACFFNodeTrackPortInfo()
        {
            oADTS = false;
            iTrackId = -1;
            iPort = NULL;
            iClockConverter = NULL;
            iTrackDataMemoryPool = NULL;
            iTrackDataMemoryPoolProxy = NULL;
            iMediaDataImplAlloc = NULL;
            iMediaDataMemPool = NULL;
            iResizableSimpleMediaMsgAlloc = NULL;
            iNode = NULL;
            iSeqNum = 0;
            iTimestampOffset = 0;
            iSendBOS = false;

            /////////////////////////////////////////////////////
            iFormatType                   = PVMF_MIME_FORMAT_UNKNOWN;
            iSeqNum                       = 0;
            oEOSReached                   = false;
            oEOSSent                      = false;
            oQueueOutgoingMessages        = true;
            oProcessOutgoingMessages      = true;
            iTrackBitRate                 = 0;
            iTrackDuration                = 0;
            iContinuousTimeStamp          = 0;
            iPrevSampleTimeStamp          = 0;
            iTrackMaxSampleSize           = 0;

            iLogger = PVLogger::GetLoggerObject("PVMFAACParserNode");
            iDataPathLogger = PVLogger::GetLoggerObject("datapath.sourcenode.aacparsernode");
            if (iDataPathLogger)
                iDataPathLogger->DisableAppenderInheritance();
            iClockLogger = PVLogger::GetLoggerObject("clock");
            iPortLogger = NULL;
            oFormatSpecificInfoLogged = false;

            iAudioSampleRate              = 0;
            iAudioNumChannels             = 0;
            iAudioBitsPerSample           = 0;
            iCodecName                    = NULL;
            iCodecDescription             = NULL;
            iResizableSimpleMediaMsgAlloc = NULL;
            /////////////////////////////////////////////////////////
        }

        PVAACFFNodeTrackPortInfo(const PVAACFFNodeTrackPortInfo& aSrc) :
                OsclMemPoolFixedChunkAllocatorObserver(aSrc),
                OsclMemPoolResizableAllocatorObserver(aSrc)
        {
            oADTS = aSrc.oADTS;
            iTrackId = aSrc.iTrackId;
            iPort = aSrc.iPort;
            iClockConverter = aSrc.iClockConverter;
            iFormatSpecificConfig = aSrc.iFormatSpecificConfig;
            iMediaData = aSrc.iMediaData;
            iTrackDataMemoryPool = aSrc.iTrackDataMemoryPool;
            iTrackDataMemoryPoolProxy = aSrc.iTrackDataMemoryPoolProxy;
            iMediaDataImplAlloc = aSrc.iMediaDataImplAlloc;
            iMediaDataMemPool = aSrc.iMediaDataMemPool;
            iNode = aSrc.iNode;
            iSeqNum = aSrc.iSeqNum;
            iTimestampOffset = aSrc.iTimestampOffset;
            iSendBOS = aSrc.iSendBOS;

            //////////////////////////////////////////////////
            iSeqNum                            = aSrc.iSeqNum;
            oEOSReached                        = aSrc.oEOSReached;
            oEOSSent                           = aSrc.oEOSSent;
            oQueueOutgoingMessages             = aSrc.oQueueOutgoingMessages;
            oProcessOutgoingMessages           = aSrc.oProcessOutgoingMessages;
            iTrackBitRate                      = aSrc.iTrackBitRate;
            iTrackDuration                     = aSrc.iTrackDuration;
            iContinuousTimeStamp               = aSrc.iContinuousTimeStamp;
            iPrevSampleTimeStamp               = aSrc.iPrevSampleTimeStamp;
            iTrackMaxSampleSize                = aSrc.iTrackMaxSampleSize;
            iLogger                            = aSrc.iLogger;
            iDataPathLogger                    = aSrc.iDataPathLogger;
            iClockLogger                       = aSrc.iClockLogger;
            iPortLogger                        = aSrc.iPortLogger;
            oFormatSpecificInfoLogged          = aSrc.oFormatSpecificInfoLogged;
            iAudioSampleRate                   = aSrc.iAudioSampleRate;
            iAudioNumChannels                  = aSrc.iAudioNumChannels;

            iResizableSimpleMediaMsgAlloc      = aSrc.iResizableSimpleMediaMsgAlloc;
            iMediaDataMemPool                  = aSrc.iMediaDataMemPool;

            iAudioBitsPerSample                = aSrc.iAudioBitsPerSample;
            iCodecName                         = aSrc.iCodecName;;
            iCodecDescription                  = aSrc.iCodecDescription;
        }

        virtual ~PVAACFFNodeTrackPortInfo()
        {
            iLogger                            = NULL;
            iDataPathLogger                    = NULL;
            iClockLogger                       = NULL;
            iPortLogger                        = NULL;
        }

        // From OsclMemPoolFixedChunkAllocatorObserver
        // Callback handler when mempool's deallocate() is called after
        // calling notifyfreechunkavailable() on the mempool
        void freechunkavailable(OsclAny* aContextData)
        {
            OSCL_UNUSED_ARG(aContextData);
            PVMF_AACPARSERNODE_LOGINFO((0, "MimeType = %s, freeblockavailable#############", iTrackMimeType.get_cstr()));
            oQueueOutgoingMessages = true;
            if (iNode)
            {
                // Activate the parent node if necessary
                iNode->Reschedule();
            }
        }

        /*
         * From OsclMemPoolResizableAllocatorObserver
         * Callback handler when mempool's deallocate() is called after
         * calling notifyfreeblockavailable() on the mempool
         */
        void freeblockavailable(OsclAny* aContextData)
        {
            OSCL_UNUSED_ARG(aContextData);
            PVMF_AACPARSERNODE_LOGINFO((0, "MimeType = %s, freeblockavailable", iTrackMimeType.get_cstr()));
            oQueueOutgoingMessages = true;
            if (iNode)
            {
                // Activate the parent node if necessary
                iNode->Reschedule();
            }
        }

        bool oADTS;

        // Track ID number in AAC FF
        int32 iTrackId;

        // Track Bitrate
        uint32 iTrackBitRate;

        // Track Duration
        uint64 iTrackDuration;

        // Output port to send the data downstream
        PVMFPortInterface* iPort;

        // PVMF mime type for track
        OSCL_HeapString<OsclMemAllocator> iTrackMimeType;

        // Format type for the port
        PVMFFormatType iFormatType;

        // bos flag
        bool iSendBOS;

        // Settings for the output port
        // Converter to convert from track timescale to milliseconds
        MediaClockConverter* iClockConverter;
        // Shared memory pointer holding the decoder specific config info for this track
        OsclRefCounterMemFrag iFormatSpecificConfig;

        // Shared memory pointer holding the currently retrieved track data
        PVMFSharedMediaDataPtr iMediaData;
        // Output buffer memory pool
        OsclMemPoolResizableAllocator *iTrackDataMemoryPool;
        // Allocator wrapper for the output buffer memory pool
        TrackDataMemPoolProxyAlloc* iTrackDataMemoryPoolProxy;
        // Allocator for simple media data buffer impl
        PVMFSimpleMediaBufferCombinedAlloc *iMediaDataImplAlloc;

        // Allocator for simple media buffer impl
        PVMFResizableSimpleMediaMsgAlloc* iResizableSimpleMediaMsgAlloc;

        // Memory pool for simple media data
        PVMFMemPoolFixedChunkAllocator *iMediaDataMemPool;
        // AAC FF parser node handle
        PVMFNodeInterfaceImpl* iNode;

        // Sequence number
        uint32 iSeqNum;
        // An offset for timestamps after repositioning.
        int32 iTimestampOffset;

        // port flow control
        bool oQueueOutgoingMessages;
        bool oProcessOutgoingMessages;

        // End of Track
        bool oEOSReached;
        bool oEOSSent;

        // Continuous timestamp
        uint64 iContinuousTimeStamp;
        uint64 iPrevSampleTimeStamp;

        // Mem pool size
        uint32 iTrackDataMemoryPoolSize;

        // Track Max Sample Size
        uint32 iTrackMaxSampleSize;

        // Loggers
        PVLogger* iLogger;
        PVLogger* iDataPathLogger;
        PVLogger* iClockLogger;

        // bitstream logging
        PVLogger* iPortLogger;
        bool oFormatSpecificInfoLogged;
        OsclSharedPtr<PVLoggerAppender> iBinAppenderPtr;

        // Audio - specfic info
        uint32 iAudioSampleRate;
        uint32 iAudioNumChannels;
        uint32 iAudioBitsPerSample;
        OSCL_wHeapString<OsclMemAllocator> iCodecName;
        OSCL_wHeapString<OsclMemAllocator> iCodecDescription;
};

class PVMFAACFFParserNode
        : public PVMFNodeInterfaceImpl,
        public PVMFDataSourceInitializationExtensionInterface,
        public PVMFTrackSelectionExtensionInterface,
        public PvmfDataSourcePlaybackControlInterface,
        public PVMFMetadataExtensionInterface,
        public PVMFCPMStatusObserver,
        public PVMIDatastreamuserInterface,
        public PvmiDataStreamObserver,
        public PVMFFormatProgDownloadSupportInterface
{
    public:

        PVMFAACFFParserNode(int32 aPriority);
        ~PVMFAACFFParserNode();

        PVMFStatus QueryInterfaceSync(PVMFSessionId aSession,
                                      const PVUuid& aUuid,
                                      PVInterface*& aInterfacePtr);

        //From PVMFDataSourceInitializationExtensionInterface
        void addRef();
        void removeRef();
        bool queryInterface(const PVUuid& uuid, PVInterface *& iface);

        PVMFStatus SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData, uint32 aClipIndex, PVMFFormatTypeDRMInfo aType = PVMF_FORMAT_TYPE_CONNECT_DRM_INFO_UNKNOWN);
        PVMFStatus SetClientPlayBackClock(PVMFMediaClock* aClientClock);
        PVMFStatus SetEstimatedServerClock(PVMFMediaClock* aClientClock);
        void AudioSinkEvent(PVMFStatus aEvent, uint32 aStreamId);

        //From PVMFTrackSelectionExtensionInterface
        PVMFStatus GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo);
        PVMFStatus SelectTracks(PVMFMediaPresentationInfo& aInfo);

        // From PVMFCPMStatusObserver
        void CPMCommandCompleted(const PVMFCmdResp& aResponse);

        // From PVMIDatastreamuserInterface
        void PassDatastreamFactory(PVMFDataStreamFactory& aFactory,
                                   int32 aFactoryTag,
                                   const PvmfMimeString* aFactoryConfig = NULL);

        void PassDatastreamReadCapacityObserver(PVMFDataStreamReadCapacityObserver* aObserver);
        bool setProtocolInfo(Oscl_Vector<PvmiKvp*, OsclMemAllocator>& aInfoKvpVec);

        // From PVMFFormatProgDownloadSupportInterface
        int32 convertSizeToTime(TOsclFileOffset fileSize, uint32& aNPTInMS);
        void setFileSize(const TOsclFileOffset aFileSize);
        void setDownloadProgressInterface(PVMFDownloadProgressInterface*);
        void playResumeNotification(bool aDownloadComplete);
        void notifyDownloadComplete()
        {
            PVMF_AACPARSERNODE_LOGSTACKTRACE((0, "PVMFAACParserNode::notifyDownloadComplete()"));
            playResumeNotification(true);
        };



        // From PVMFMetadataExtensionInterface
        PVMFStatus SetMetadataClipIndex(uint32 aClipIndex)
        {
            return (aClipIndex == 0) ? PVMFSuccess : PVMFErrArgument;
        }
        uint32 GetNumMetadataValues(PVMFMetadataList& aKeyList);
        PVMFCommandId GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                            uint32 aStartingKeyIndex, int32 aMaxKeyEntries, const OsclAny* aContextData = NULL);
        PVMFStatus ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 aStartingValueIndex, uint32 aEndValueIndex);

        // From PvmiDataStreamObserver
        void DataStreamCommandCompleted(const PVMFCmdResp& aResponse);
        void DataStreamInformationalEvent(const PVMFAsyncEvent& aEvent);
        void DataStreamErrorEvent(const PVMFAsyncEvent& aEvent);

        // From PvmfDataSourcePlaybackControlInterface
        PVMFCommandId SetDataSourcePosition(PVMFSessionId aSessionId
                                            , PVMFTimestamp aTargetNPT
                                            , PVMFTimestamp& aActualNPT
                                            , PVMFTimestamp& aActualMediaDataTS
                                            , bool aSeekToSyncPoint = true
                                                                      , uint32 aStreamID = -1
                                                                                           , OsclAny* aContext = NULL);
        PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId
                                              , PVMFTimestamp aTargetNPT
                                              , PVMFTimestamp& aActualNPT
                                              , bool aSeekToSyncPoint = true
                                                                        , OsclAny* aContext = NULL);
        PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId
                                              , PVMFTimestamp aTargetNPT
                                              , PVMFTimestamp& aSeekPointBeforeTargetNPT
                                              , PVMFTimestamp& aSeekPointAfterTargetNPT
                                              , OsclAny* aContext = NULL
                                                                    , bool aSeekToSyncPoint = true);
        PVMFCommandId SetDataSourceRate(PVMFSessionId aSession
                                        , int32 aRate
                                        , PVMFTimebase* aTimebase = NULL
                                                                    , OsclAny* aContext = NULL);

    private:

        //from PVMFPortActivityHandler
        void HandlePortActivity(const PVMFPortActivity& aActivity);

        virtual void Run();

        // Port processing
        bool ProcessPortActivity(PVAACFFNodeTrackPortInfo*);
        PVMFStatus RetrieveMediaSample(PVAACFFNodeTrackPortInfo* aTrackInfoPtr,
                                       PVMFSharedMediaDataPtr& aSharedPtr);
        PVMFStatus QueueMediaSample(PVAACFFNodeTrackPortInfo* aTrackInfoPtr);
        PVMFStatus ProcessOutgoingMsg(PVAACFFNodeTrackPortInfo* aTrackInfoPtr);
        bool GetTrackInfo(PVMFPortInterface* aPort,
                          PVAACFFNodeTrackPortInfo*& aTrackInfoPtr);
        bool CheckForPortRescheduling();
        bool CheckForPortActivityQueues();


        PVMFStatus HandleExtensionAPICommands();
        PVMFStatus CancelCurrentCommand();

        //Command handlers.
        PVMFStatus CompleteReset();
        void CompleteInit();
        void CompleteGetMetaDataValues();
        void CompleteGetLicense();

        //Command handlers for PVMFNodeInterfaceImpl APIs
        PVMFStatus DoQueryInterface();
        PVMFStatus DoInit();
        PVMFStatus DoStop();
        PVMFStatus DoReset();
        PVMFStatus DoRequestPort(PVMFPortInterface*& aPort);
        PVMFStatus DoReleasePort();

        //Command handlers for Extension APIs
        PVMFStatus DoGetMetadataValues();
        PVMFStatus DoSetDataSourcePosition();
        PVMFStatus DoQueryDataSourcePosition();
        PVMFStatus DoSetDataSourceRate();

        int32 AddToValueList(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, PvmiKvp& aNewValue);

        // Track data processing
        bool RetrieveTrackConfigInfo(PVAACFFNodeTrackPortInfo& aTrackPortInfo);
        bool RetrieveTrackData(PVAACFFNodeTrackPortInfo& aTrackPortInfo);

        void ResetAllTracks();
        bool ReleaseTrack();
        void CleanupFileSource();
        void ReleaseMetadataValue(PvmiKvp& aValueKVP);

        void ConstructL();
        void ReleaseAllPorts();
        void RemoveAllCommands();
        void ResetSourceFile();

        // Content Policy Manager related
        void InitCPM();
        void OpenCPMSession();
        void CPMRegisterContent();
        bool GetCPMContentAccessFactory();
        bool GetCPMMetaDataExtensionInterface();
        void GetCPMLicenseInterface();
        void RequestUsage();
        void SendUsageComplete();
        void CloseCPMSession();
        void ResetCPM();
        void PopulateDRMInfo();
        void GetCPMMetaDataValues();
        PVMFStatus CheckCPMCommandCompleteStatus(PVMFCommandId, PVMFStatus);
        int32 CreateNewArray(oscl_wchar*& aPtr, int32 aLen);
        int32 CreateNewArray(char*& aPtr, int32 aLen);

        int32 PushKVPToList(Oscl_Vector<PvmiKvp, OsclMemAllocator>*& aValueListPtr, PvmiKvp &aKeyVal);
        PVMFStatus PushValueToList(Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> &aRefMetadataKeys,
                                   PVMFMetadataList *&aKeyListPtr,
                                   uint32 aLcv);

        // Meta data related
        PVMFStatus InitMetaData();
        PVMFStatus GetIndexParamValues(char* aString,
                                       uint32& aStartIndex,
                                       uint32& aEndIndex);
        PVMFStatus GetMaxSizeValue(char* aString, uint32& aMaxSize);
        PVMFStatus GetTruncateFlagValue(char* aString, uint32& aTruncateFlag);

        // Progressive download related
        PVMFStatus CheckForAACHeaderAvailability();

        // bitstream logging
        void LogMediaData(PVMFSharedMediaDataPtr data, PVMFPortInterface* aPort);
        PVMFStatus ParseAACFile();

        OsclFileHandle *iFileHandle;
        Oscl_FileServer iFileServer;
        OSCL_wHeapString<OsclMemAllocator> iSourceURL;
        PVMFFormatType iSourceFormat;

        PVLogger* iDataPathLogger;

        // ports contained in this node
        PVMFAACFFParserOutPort* iOutPort;

        bool iUseCPMPluginRegistry;

        PVMFLocalDataSource iCPMSourceData;

        CAACFileParser* iAACParser;

        PVAACFFNodeTrackPortInfo iTrack;
        bool iFirstFrame;

        PVMFMediaClock* iClientClock;
        bool oSourceIsCurrent;

        //for meta-data
        PVMFMetadataList iAvailableMetadataKeys;
        bool iID3DataValid;
        TPVAacFileInfo iAACFileInfo;
        bool iAACFileInfoValid;

        Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> iCPMMetadataKeys;
        uint32 iAACParserNodeMetadataValueCount;

        // Content Policy Manager related
        bool iPreviewMode;
        PVMFCPM* iCPM;
        PVMFSessionId iCPMSessionID;
        PVMFCPMContentType iCPMContentType;
        PVMFCPMPluginAccessInterfaceFactory* iCPMContentAccessFactory;
        PVMFMetadataExtensionInterface* iCPMMetaDataExtensionInterface;
        PVMFCPMPluginLicenseInterface* iCPMLicenseInterface;
        PVInterface* iCPMLicenseInterfacePVI;
        PvmiKvp iRequestedUsage;
        PvmiKvp iApprovedUsage;
        PvmiKvp iAuthorizationDataKvp;
        PVMFCPMUsageID iUsageID;
        PVMFCommandId iCPMInitCmdId;
        PVMFCommandId iCPMOpenSessionCmdId;
        PVMFCommandId iCPMRegisterContentCmdId;
        PVMFCommandId iCPMRequestUsageId;
        PVMFCommandId iCPMUsageCompleteCmdId;
        PVMFCommandId iCPMCloseSessionCmdId;
        PVMFCommandId iCPMResetCmdId;
        PVMFCommandId iCPMGetMetaDataKeysCmdId;
        PVMFCommandId iCPMGetMetaDataValuesCmdId;
        PVMFCommandId iCPMGetLicenseInterfaceCmdId;

        PVMFStatus iCPMRequestUsageCommandStatus;

        // Progressive download related
        bool iAutoPaused;
        bool iDownloadComplete;
        PVMFDownloadProgressInterface* iDownloadProgressInterface;
        uint32 iDownloadFileSize;
        PVMIDataStreamSyncInterface* iDataStreamInterface;
        PVMFDataStreamFactory* iDataStreamFactory;
        PVMFDataStreamReadCapacityObserver* iDataStreamReadCapacityObserver;
        PvmiDataStreamSession iDataStreamSessionID;
        PvmiDataStreamCommandId iRequestReadCapacityNotificationID;

        // friend classes
        friend class PVMFAACFFParserOutPort;
};

#endif // PVMF_AACFFPARSER_NODE_H_INCLUDED

