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
#ifndef PVMF_DOWNLOADMANAGER_NODE_H_INCLUDED
#define PVMF_DOWNLOADMANAGER_NODE_H_INCLUDED


#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#include "pvmf_node_interface_impl.h"
#endif
#ifndef PVMF_NODE_UTILS_H_INCLUDED
#include "pvmf_node_utils.h"
#endif
#ifndef PVMF_DOWNLOADMANAGER_DEFS_H_INCLUDED
#include "pvmf_downloadmanager_defs.h"
#endif
#ifndef PVMF_DATA_SOURCE_PLAYBACK_CONTROL_H_INCLUDED
#include "pvmf_data_source_playback_control.h"
#endif
#ifndef PVMF_META_DATA_EXTENSION_H_INCLUDED
#include "pvmf_meta_data_extension.h"
#endif
#ifndef PVMF_DATA_SOURCE_INIT_EXTENSION_H_INCLUDED
#include "pvmf_data_source_init_extension.h"
#endif
#ifndef PVMF_TRACK_SELECTION_EXTENSION_H_INCLUDED
#include "pvmf_track_selection_extension.h"
#endif
#ifndef PVMF_DOWNLOAD_PROGRESS_INTERFACE_H_INCLUDED
#include "pvmf_download_progress_interface.h"
#endif
#ifndef PVMF_FF_PROGDOWNLOAD_SUPPORT_EXTENSION_H_INCLUDED
#include "pvmf_format_progdownload_support_extension.h"
#endif
#ifndef PVMFPROTOCOLENGINENODE_EXTENSION_H_INCLUDED
#include "pvmf_protocol_engine_node_extension.h"
#endif
#ifndef  PVMF_FILEBUFFERDATASTREAM_FACTORY_H_INCLUDED
#include "pvmf_filebufferdatastream_factory.h"
#endif
#ifndef PVMF_SOURCE_CONTEXT_DATA_H_INCLUDED
#include "pvmf_source_context_data.h"
#endif
#ifndef PVMF_RECOGNIZER_REGISTRY_H_INCLUDED
#include "pvmf_recognizer_registry.h"
#endif
#ifndef PV_PLAYER_NODE_REGISTRY_INTERFACE_H_INCLUDED
#include "pv_player_node_registry_interface.h"
#endif
#ifndef PVMI_CONFIG_AND_CAPABILITY_BASE_H_INCLUDED
#include "pvmi_config_and_capability_base.h"
#endif
#ifndef CPM_H_INCLUDED
#include "cpm.h"
#endif

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
#ifndef  PVMF_MEMORYBUFFERDATASTREAM_FACTORY_H_INCLUDED
#include "pvmf_memorybufferdatastream_factory.h"
#endif
#ifndef PVPLSFILEPARSER_H_INCLUDED
#include "pvplsfileparser.h"
#endif
#endif //PVMF_DOWNLOADMANAGER_SUPPORT_PPB

#ifndef PVMF_CPMPLUGIN_LICENSE_INTERFACE_H_INCLUDED
#include "pvmf_cpmplugin_license_interface.h"
#endif

/**
* Node command handling
*/


class PVMFDownloadManagerNode;
class PVLogger;


/**
* Base for Containers for the protocol engine, socket, and parser nodes and CPM object
*/
class PVMFDownloadManagerSubNodeContainerBase
{
    public:
        PVMFDownloadManagerSubNodeContainerBase();
        virtual ~PVMFDownloadManagerSubNodeContainerBase() {};

        /*
         *  EFormatParser indicates the format parser node
         *  EProtocolEngine indicates the protocol engine node
         *  ESocket indicates the socket node
         *  ECPM is the CPM container
         */
        enum NodeType {EFormatParser, EProtocolEngine, ESocket, ECPM, ERecognizer};

        enum CmdState
        {
            EIdle,   // no command
            EBusy    // command issued to the sub-node, completion pending.
        };

        enum CmdType
        {
            // common commands
            ECleanup = 0
            // node commands
            , EQueryDataSourceInit = 1
            , EQueryTrackSelection = 2
            , EQueryMetadata = 3
            , EQueryDataSourcePlayback = 4
            , EQueryFFProgDownload = 5
            , EQueryDownloadProgress = 6
            , EQueryProtocolEngine = 7
            , EQueryDatastreamUser = 8
            , EInit = 9
            , ERequestPort = 10
            , EReleasePort = 11
            , ERequestPort2 = 12
            , EReleasePort2 = 13
            , EPrepare = 14
            , EStop = 15
            , EStart = 16
            , EPause = 17
            , EFlush = 18
            , EReset = 19
            , EGetMetadataValue = 20
            , ESetFFProgDownloadSupport = 21
            , ESetDataSourcePosition = 22
            , EQueryDataSourcePosition = 23
            , EParserCreate = 24
            //Recognizer module commands
            , ERecognizerStart = 25
            , ERecognizerClose = 26
            //CPM commands.
            , ECPMInit = 27
            , ECPMOpenSession = 28
            , ECPMRegisterContent = 29
            , ECPMApproveUsage = 30
            , ECPMSetDecryptInterface = 31
            , ECPMUsageComplete = 32
            , ECPMReset = 33
        };

        // Each subnode has a pointer to its container, which is the DLMGR node
        PVMFDownloadManagerNode* iContainer;

        NodeType iType;
        PVMFSessionId iSessionId;

        //Node Command processing
        PVMFCommandId iCmdId;

        CmdState iCmdState;

        //for canceling commands.
        PVMFCommandId iCancelCmdId;
        CmdState iCancelCmdState;

        int32 iCmd;

        void Construct(NodeType t, PVMFDownloadManagerNode* container);

        // The pure virtual method IssueCommand is called on a subnode container to request that the passed
        // command be issued to the contained node (or object).
        virtual PVMFStatus IssueCommand(int32) = 0;

        // CommandDone is a base class method, which is called by the subnode container when a command that was issued completes.
        // In this method we handle removing the completed subnode command from the subnode command vector, and then we call Run
        // on our container (the DLMGR) so that the next subnode command will be run. If we see that there are no more subnode
        // commands in the subnode command vector, we instead call CommandComplete on the DLMGR.
        void CommandDone(PVMFStatus, PVInterface*, OsclAny*);

        //for canceling commands.
        virtual bool CancelPendingCommand() = 0;
        void CancelCommandDone(PVMFStatus, PVInterface*, OsclAny*);

        bool CmdPending()
        {
            return iCmdState != EIdle || iCancelCmdState != EIdle;
        }
};

/*
* Containers for the protocol, socket, and parser nodes (but not the cpm object and not the recognizer)
*/
class PVMFDownloadManagerSubNodeContainer
        : public PVMFDownloadManagerSubNodeContainerBase
        , public PVMFNodeErrorEventObserver
        , public PVMFNodeInfoEventObserver
        , public PVMFNodeCmdStatusObserver
{
    public:
        PVMFDownloadManagerSubNodeContainer()
        {
            iDataSourceInit = NULL;
            iProtocolEngineExtensionInt = NULL;
            iDataSourcePlayback = NULL;
            iTrackSelection = NULL;
            iDatastreamUser = NULL;
            iMetadata = NULL;
            iFormatProgDownloadSupport = NULL;
            iDownloadProgress = NULL;
            iNode = NULL;
            iLicenseInterface = NULL;
        }

        ~PVMFDownloadManagerSubNodeContainer()
        {
            Cleanup();
        }

        void Cleanup();

        // pure virtuals from PVMFDownloadManagerSubNodeContainerBase
        PVMFStatus IssueCommand(int32);
        bool CancelPendingCommand();

        // node
        PVMFNodeInterface *iNode;
        void Connect();

        // Node data-- pointers to retrieved interfaces.
        PVInterface* iDataSourceInit;
        PVInterface* iProtocolEngineExtensionInt;
        PVInterface* iTrackSelection;
        PVInterface* iMetadata;
        PVInterface* iFormatProgDownloadSupport;  // The support interface provided by the format parser node
        PVInterface* iDownloadProgress;
        PVInterface* iDataSourcePlayback;
        PVInterface* iDatastreamUser;
        PVInterface* iLicenseInterface;

        PVMFDataSourceInitializationExtensionInterface *DataSourceInit()
        {
            return (PVMFDataSourceInitializationExtensionInterface*)iDataSourceInit;
        }
        PVMFProtocolEngineNodeExtensionInterface* ProtocolEngineExtension()
        {
            return (PVMFProtocolEngineNodeExtensionInterface*)iProtocolEngineExtensionInt;
        }
        PVMFTrackSelectionExtensionInterface* TrackSelection()
        {
            return (PVMFTrackSelectionExtensionInterface*)iTrackSelection;
        }
        PVMIDatastreamuserInterface* DatastreamUser()
        {
            return (PVMIDatastreamuserInterface*)iDatastreamUser;
        }
        PVMFMetadataExtensionInterface *Metadata()
        {
            return (PVMFMetadataExtensionInterface*)iMetadata;
        }
        PVMFFormatProgDownloadSupportInterface *FormatProgDownloadSupport()
        {
            return (PVMFFormatProgDownloadSupportInterface*)iFormatProgDownloadSupport;
        }
        PVMFDownloadProgressInterface *DownloadProgress()
        {
            return (PVMFDownloadProgressInterface*)iDownloadProgress;
        }
        PvmfDataSourcePlaybackControlInterface *DataSourcePlayback()
        {
            return (PvmfDataSourcePlaybackControlInterface*)iDataSourcePlayback;
        }
        PVMFCPMPluginLicenseInterface* LicenseInterface()
        {
            return (PVMFCPMPluginLicenseInterface*)iLicenseInterface;
        }

        //Node event observers
        void HandleNodeErrorEvent(const PVMFAsyncEvent& aEvent);
        void HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent);
        void NodeCommandCompleted(const PVMFCmdResp& aResponse);
};


/**
* Container for the CPM object
*/
class PVMFDownloadManagerCPMContainer
        : public PVMFDownloadManagerSubNodeContainerBase
        , public PVMFCPMStatusObserver
{
    public:
        PVMFDownloadManagerCPMContainer()
        {
            iCPM = NULL;
            iCPMContentAccessFactory = NULL;
            iDecryptionInterface = NULL;
            iRequestedUsage.key = NULL;
            iApprovedUsage.key = NULL;
            iAuthorizationDataKvp.key = NULL;
        }
        ~PVMFDownloadManagerCPMContainer()
        {
            Cleanup();
        }

        void Cleanup();

        //pure virtuals from PVMFDownloadManagerSubNodeContainerBase
        bool GetCPMContentAccessFactory();
        PVMFStatus IssueCommand(int32);
        bool CancelPendingCommand();

        //CPM object
        PVMFCPM* iCPM;

        //CPM data
        PVMFCPMPluginAccessInterfaceFactory* iCPMContentAccessFactory;
        PVMFCPMPluginAccessUnitDecryptionInterface* iDecryptionInterface;
        PvmiKvp iRequestedUsage;
        PvmiKvp iApprovedUsage;
        PvmiKvp iAuthorizationDataKvp;
        PVMFCPMUsageID iUsageID;

        //CPM event observer
        //From PVMFCPMStatusObserver
        OSCL_IMPORT_REF void CPMCommandCompleted(const PVMFCmdResp& aResponse) ;
};


/* Container for the recognizer
 */
class PVMFDownloadManagerRecognizerContainer
        : public PVMFDownloadManagerSubNodeContainerBase,
        public PVMFRecognizerCommmandHandler
{
    public:
        // Recognizer to use for determining mime type of downloaded data
        // PVMFRecognizer iRecognizer;
        PVMFDownloadManagerRecognizerContainer()
        {
            iRecognizerSessionId = 0;
        };

        ~PVMFDownloadManagerRecognizerContainer()
        {
            Cleanup();
        }
        void Cleanup();

        //pure virtuals from PVMFDownloadManagerSubNodeContainerBase
        PVMFStatus IssueCommand(int32);
        bool CancelPendingCommand();

        void RecognizerCommandCompleted(const PVMFCmdResp& aResponse);

        PVMFSessionId iRecognizerSessionId;
        Oscl_Vector<PVMFRecognizerResult, OsclMemAllocator> iRecognizerResultVec;
};

/**
 * Download Manager Node.
 * This node contains the protocol engine node, the socket node, and a format parser node.
 * The format parser node is instantiated after using the recognizer to determine the
 * format type of the data stream.
 *
 * The usage sequence is as follows:
 *
 *    Create
 *    Connect
 *    ThreadLogon
 *
 *    (All extension intefaces are available for query now.)
 *
 *    QueryInterface
 *    SetSourceInitializationData
 *    Init (CPM authorization is done during this command)
 *
 *    (Track selection is available now)
 *
 *    GetMediaPresentationInfo
 *    SelectTracks
 *    Prepare
 *
 *    (The Metadata is available now)
 *    (Ports are available now)
 *
 *    RequestPort
 *    Configure & Connect ports
 *
 *    (App should wait on PVMFInfoDataReady before calling Start, but
 *      it is not a strict requirement.  If Start is called too soon,
 *      playback will likely underflow immediately.)
 *
 *    Start
 *
 *    (playback begins)
 *
 * This node does not create any ports.  Port requests are passed directly
 * to the format parser node.
 *
 */
class PVMFDownloadManagerNode
        : public PVMFNodeInterfaceImpl
        , public PvmiCapabilityAndConfigBase
        //required extension interfaces for player source nodes.
        , public PVMFDataSourceInitializationExtensionInterface
        , public PVMFTrackSelectionExtensionInterface
        , public PvmfDataSourcePlaybackControlInterface
        , public PVMFMetadataExtensionInterface
        , public PVMFDataSourceNodeRegistryInitInterface
        // For observing the playback clock states
        , public PVMFMediaClockStateObserver
        , public PVMFCPMStatusObserver
{
    public:
        PVMFDownloadManagerNode(int32 aPriority = OsclActiveObject::EPriorityNominal);
        ~PVMFDownloadManagerNode();

        // From PVMFNodeInterface
        PVMFStatus ThreadLogon();
        PVMFStatus ThreadLogoff();
        PVMFPortIter* GetPorts(const PVMFPortFilter* aFilter = NULL);

        //From PVInterface
        void addRef();
        void removeRef();
        bool queryInterface(const PVUuid& uuid, PVInterface*& iface);

        //From PVMFDataSourceInitializationExtensionInterface
        PVMFStatus SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData, uint32 aClipIndex, PVMFFormatTypeDRMInfo aType = PVMF_FORMAT_TYPE_CONNECT_DRM_INFO_UNKNOWN);
        PVMFStatus SetClientPlayBackClock(PVMFMediaClock* aClientClock);
        PVMFStatus SetEstimatedServerClock(PVMFMediaClock* aClientClock);
        void AudioSinkEvent(PVMFStatus aEvent, uint32 aStreamId);

        //From PVMFTrackSelectionExtensionInterface
        PVMFStatus GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo);
        PVMFStatus SelectTracks(PVMFMediaPresentationInfo& aInfo);

        // From PVMFMetadataExtensionInterface
        PVMFStatus SetMetadataClipIndex(uint32 aClipNum)
        {
            return (aClipNum == 0) ? PVMFSuccess : PVMFErrArgument;
        }
        uint32 GetNumMetadataValues(PVMFMetadataList& aKeyList);
        PVMFCommandId GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList,
                                            Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 aStartingValueIndex, int32 aMaxValueEntries, const OsclAny* aContextData = NULL);
        PVMFStatus ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 aStartingValueIndex, uint32 aEndValueIndex);


        // From PvmfDataSourcePositioningControlInterface
        PVMFCommandId SetDataSourcePosition(PVMFSessionId aSessionId, PVMFTimestamp aTargetNPT, PVMFTimestamp& aActualNPT,
                                            PVMFTimestamp& aActualMediaDataTS, bool aSeekToSyncPoint = true, uint32 aStreamId = 0, OsclAny* aContext = NULL);
        PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId, PVMFTimestamp aTargetNPT, PVMFTimestamp& aActualNPT,
                                              bool aSeekToSyncPoint = true, OsclAny* aContext = NULL);
        PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId, PVMFTimestamp aTargetNPT,
                                              PVMFTimestamp& aSeekPointBeforeTargetNPT, PVMFTimestamp& aSeekPointAfterTargetNPT,
                                              OsclAny* aContext = NULL, bool aSeekToSyncPoint = true);

        PVMFCommandId SetDataSourceRate(PVMFSessionId aSessionId, int32 aRate, PVMFTimebase* aTimebase = NULL, OsclAny* aContext = NULL);

        // From PVMFPortActivityHandler
        void HandlePortActivity(const PVMFPortActivity& aActivity)
        {
            OSCL_UNUSED_ARG(aActivity);
        }

        //From PVMFDataSourceNodeRegistryInitInterface
        PVMFStatus SetPlayerNodeRegistry(PVPlayerNodeRegistryInterface* aRegistry);

        void SetDebugMode()
        {
            iDebugMode = true;
        }

        //From capability and config interface
        virtual void setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
        {
            ciObserver = aObserver;
        }

        virtual PVMFStatus getParametersSync(PvmiMIOSession aSession,
                                             PvmiKeyType aIdentifier,
                                             PvmiKvp*& aParameters,
                                             int& aNumParamElements,
                                             PvmiCapabilityContext aContext);
        virtual PVMFStatus releaseParameters(PvmiMIOSession aSession,
                                             PvmiKvp* aParameters,
                                             int num_elements);
        virtual void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                       int num_elements, PvmiKvp * & aRet_kvp);

        //from PVMFMediaClockStateObserver
        void ClockStateUpdated();
        void NotificationsInterfaceDestroyed();

        /* From PVMFCPMStatusObserver */
        void CPMCommandCompleted(const PVMFCmdResp& aResponse);

    private:

        void ConstructL();

        // from OsclTimerObject
        virtual void Run();

        void CommandComplete(PVMFNodeCommand& aCmd, PVMFStatus aStatus, PVInterface* aExtMsg, OsclAny* aEventData,
                             PVUuid* aEventUUID = NULL, int32* aEventCode = NULL, int32 aEventDataLen = 0);

        // Event reporting

        void ReportInfoEvent(PVMFAsyncEvent&);
        PVMFStatus HandleExtensionAPICommands();
        PVMFStatus CancelCurrentCommand();

        // Node command handlers
        PVMFStatus DoQueryInterface();
        PVMFStatus DoRequestPort(PVMFPortInterface*& aPort);
        PVMFStatus DoReleasePort();
        PVMFStatus DoInit();
        PVMFStatus DoPrepare();
        PVMFStatus DoStart();
        PVMFStatus DoStop();
        PVMFStatus DoFlush();
        PVMFStatus DoPause();
        PVMFStatus DoReset();

        bool IsByteBasedDownloadProgress(OSCL_String &aDownloadProgressInfo);
        bool GetHttpExtensionHeaderParams(PvmiKvp &aParameter,
                                          OSCL_String &extensionHeaderKey,
                                          OSCL_String &extensionHeaderValue,
                                          HttpMethod  &httpMethod,
                                          bool &aPurgeOnRedirect);
        bool IsHttpExtensionHeaderValid(PvmiKvp &aParameter);
        // remove the ending ';', ',' or ' ' and calulate value length
        uint32 getItemLen(char *ptrItemStart, char *ptrItemEnd);

        bool IsDownloadExtensionHeaderValid(PvmiKvp &);
        bool ConfigSocketNodeMemoryPoolAndMBDS();
        bool ConfigSocketNodeMemoryPoolAndMBDSPerTrack(const uint32 aTrackMaxBitRate, const uint32 aSocketNodeBufSize, const bool isAudioTrack = true, const bool aSetBufferInfo = true);

        bool iDebugMode;

        // MIME type of the downloaded data
        OSCL_HeapString<OsclMemAllocator> iMimeType;

        // playback mode
        enum TDlMgrPlaybackMode
        {
            EPlayAsap
            , EDownloadThenPlay
            , EDownloadOnly
#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
            , EPlaybackOnly
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB
        };
        TDlMgrPlaybackMode iPlaybackMode;

        // Name of local file to store downloaded data
        OSCL_wHeapString<OsclMemAllocator> iDownloadFileName;

        // Format type of media
        PVMFFormatType iFmt;

        // The local data source info gleaned from the source data set via SetSourceInit interface
        PVMFSourceContextDataCommon iLocalDataSource;

        //Source data.
        PVMFFormatType iSourceFormat;
        OSCL_wHeapString<OsclMemAllocator> iSourceURL;
        OsclAny*iSourceData;

        friend class PVMFDownloadManagerSubNodeContainerBase;
        friend class PVMFDownloadManagerSubNodeContainer;
        friend class PVMFDownloadManagerRecognizerContainer;

        //Sub-nodes.
        PVMFDownloadManagerSubNodeContainer iFormatParserNode;
        PVMFDownloadManagerSubNodeContainer iProtocolEngineNode;
        PVMFDownloadManagerSubNodeContainer iSocketNode;

        PVMFDownloadManagerSubNodeContainer& TrackSelectNode();

        // Recognizer
        PVMFDownloadManagerRecognizerContainer iRecognizerNode;

        // Filebufferdatastream object
        PVMFFileBufferDataStream* iFileBufferDatastreamFactory;
#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
        // MemoryBufferDataStream object
        PVMFMemoryBufferDataStream* iMemoryBufferDatastreamFactory;
#endif //PVMF_DOWNLOADMANAGER_SUPPORT_PPB
        PVMFDataStreamFactory* iReadFactory;
        PVMFDataStreamFactory* iWriteFactory;

        void NotifyDownloadComplete()
        {
#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
            if (EPlaybackOnly == iPlaybackMode)
            {
                if (iMemoryBufferDatastreamFactory != NULL)
                {
                    iMemoryBufferDatastreamFactory->NotifyDownloadComplete();
                }
            }
            else
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB
            {
                if (iFileBufferDatastreamFactory != NULL)
                {
                    iFileBufferDatastreamFactory->NotifyDownloadComplete();
                }
            }
        }

        //flags
        bool iParserInit;//set when file parse sequence is initiated.
        bool iDataReady;//set when initial data ready is received or generated.
        bool iDownloadComplete;//set when DL is complete.
        bool iMovieAtomComplete;//set when movie atom DL complete for fast-track.
        bool iNoPETrackSelect;//set after deciding that PE cannot to track selection.
        bool iParserInitAfterMovieAtom;
        bool iParserPrepareAfterMovieAtom;

        bool iRecognizerError;//set when recognizer fails.
        PVMFStatus iRecognizerStartStatus;

        bool iInitFailedLicenseRequired; //set when PVMFErrDrmLicenseNotFound failed

        ByteSeekMode iContextDataByteSeek;

        void ContinueInitAfterTrackSelectDecision();
        void ContinueFromDownloadTrackSelectionPoint();
        void ContinueAfterMovieAtom();

        PVMFNodeInterface* CreateParser();

        //event handling
        void GenerateDataReadyEvent();
        bool FilterPlaybackEventsFromSubNodes(const PVMFAsyncEvent& aEvent);

        // Socket Config Info, for configuring socket node.
        OSCL_HeapString<OsclMemAllocator> iServerAddr;

        // Ports for the protocol node and the socket node
        PVMFPortInterface *iProtocolEngineNodePort, *iProtocolEngineNodePort2;
        PVMFPortInterface *iSocketNodePort, *iSocketNodePort2;

        //The sub-node command vec contains all the sub-node commands needed for a single node command.
        class CmdElem
        {
            public:
                PVMFDownloadManagerSubNodeContainerBase* iNC;
                PVMFDownloadManagerSubNodeContainerBase::CmdType iCmd;
        };
        Oscl_Vector<CmdElem, OsclMemAllocator> iSubNodeCmdVec;
        PVMFStatus ScheduleSubNodeCommands();
        void Push(PVMFDownloadManagerSubNodeContainerBase&, PVMFDownloadManagerSubNodeContainerBase::CmdType);

        //Recognizer related
        PVPlayerNodeRegistryInterface* iPlayerNodeRegistry;

        //Vector to store the Uuids for the download manager nodes created throughout the playback
        Oscl_Vector<PVUuid, OsclMemAllocator> iDNodeUuids;

        //Count for Uuids
        uint32 iDNodeUuidCount;
        // playback clock which will be received form the engine.
        PVMFMediaClock *iPlayBackClock;
        PVMFMediaClockNotificationsInterface *iClockNotificationsInf;

        // HTTP Content-Type header MIME string hint from the server
        OSCL_HeapString<OsclMemAllocator> iContentTypeMIMEString;

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
        PVMFStatus ParsePLSFile(OSCL_wString& aPLSFile);

        // Shoutcast playlist support
        PVPLSEntry iPLSEntry;
        PVPLSFileInfo iPLSFileInfo;
        PVMFSourceContextData* iPLSSessionContextData;
#endif //PVMF_DOWNLOADMANAGER_SUPPORT_PPB

        // smooth streaming related
        uint32 iMaxVideoTrackBitrate;
        uint32 iMaxAudioTrackBitrate;
        uint32 iVideoStreamIndex;
        uint32 iAudioStreamIndex;
        uint32 iNumBufForSocketNodeAudioPort;
        uint32 iNumBufForSocketNodeVideoPort;
        PVMFMemoryBufferDataStream* iMemoryBufferDatastreamFactory2;
        PVMFDataStreamFactory* iReadFactory2;
        PVMFDataStreamFactory* iWriteFactory2;
        PVMFTimestamp iActualNPT;


        friend class PVMFDownloadManagerCPMContainer;
        PVMFDownloadManagerCPMContainer iCPMNode;
};

///////////////////////////////////////////////////////////////////////////////
//
// Capability and config interface related constants and definitions
//   - based on pv_player_engine.h
//
///////////////////////////////////////////////////////////////////////////////
struct DownloadManagerKeyStringData
{
    char iString[64];
    PvmiKvpType iType;
    PvmiKvpValueType iValueType;
};

// The number of characters to allocate for the key string
#define DLMCONFIG_KEYSTRING_SIZE 128


///////////////////////////////////////////////////////////////////////////////
//
// Constants for setting up socket mem pool for progressive playback and shoutcast
// These constants are not tunables.
//
///////////////////////////////////////////////////////////////////////////////
#define PVMF_DOWNLOADMANAGER_TCP_BUFFER_SIZE_FOR_SC     1500
#define PVMF_DOWNLOADMANAGER_TCP_BUFFER_SIZE_FOR_PPB    64000
#define PVMF_DOWNLOADMANAGER_TCP_BUFFER_NOT_AVAILABLE   2
#define PVMF_DOWNLOADMANAGER_TCP_BUFFER_OVERHEAD        64
#define PVMF_DOWNLOADMANAGER_TCP_AVG_SMALL_PACKET_SIZE  250
#define PVMF_DOWNLOADMANAGER_SS_BUFFERING_TIME              4 // 4sec
#define PVMF_DOWNLOADMANAGER_TCP_BUFFER_SIZE_FOR_SS    6400
#define PVMF_DOWNLOADMANAGER_MIN_TCP_BUFFERS_FOR_SS    80




#endif // PVMF_DOWNLOADMANAGER_NODE_H_INCLUDED

