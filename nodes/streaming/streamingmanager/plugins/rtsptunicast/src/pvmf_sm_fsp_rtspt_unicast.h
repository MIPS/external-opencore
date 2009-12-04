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
#ifndef PVMF_SM_FSP_RTSPT_UNICAST_H
#define PVMF_SM_FSP_RTSPT_UNICAST_H

#ifndef PVMF_SM_FSP_BASE_IMPL_H
#include "pvmf_sm_fsp_base_impl.h"
#endif

#ifndef __SDP_INFO_H__
#include "sdp_info.h"
#endif

#ifndef PAYLOAD_PARSER_H_INCLUDED
#include "payload_parser.h"
#endif

#ifndef PVMF_MEDIA_PRESENTATION_INFO_H_INCLUDED
#include "pvmf_media_presentation_info.h"
#endif

#ifndef RM_MEDIAINFO_H
#include "rm_media_info.h"
#endif

/**
 * Macros for calling PVLogger
 */
#define PVMF_SM_RTSPT_LOGERROR(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iLogger,PVLOGMSG_ERR,m);
#define PVMF_SM_RTSPT_LOGINFOHI(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_NOTICE,m);
#define PVMF_SM_RTSPT_LOGINFOMED(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG,iLogger,PVLOGMSG_INFO,m);
#define PVMF_SM_RTSPT_LOGSTACKTRACE(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_STACK_TRACE,m);
#define PVMF_SM_RTSPT_LOGINFOLOW(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_DEBUG,m);
#define PVMF_SM_RTSPT_LOGINFO(m) PVMF_SM_RTSPT_LOGINFOMED(m)
#define PVMF_SM_RTSPT_LOG_COMMAND_SEQ(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iCommandSeqLogger,PVLOGMSG_DEBUG,m);
#define PVMF_SM_RTSPT_LOG_COMMAND_REPOS(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_DEBUG,m);

/*
 * Track related structures
 */
class PVMFRTSPTTrackInfo
{
    public:
        PVMFRTSPTTrackInfo()
        {
            trackID = 0;
            rdtStreamID = 0;
            portTag = 0;
            bitRate = 0;
            trackTimeScale = 1;
            iNetworkNodePort = NULL;
            iJitterBufferInputPort = NULL;
            iJitterBufferOutputPort = NULL;
            iJitterBufferRTCPPort = NULL;
            iNetworkNodeRTCPPort = NULL;
            iSessionControllerOutputPort = NULL;
            iSessionControllerFeedbackPort = NULL;
            iRTPSocketID = 0;
            iRTCPSocketID = 0;
            iRateAdaptation = false;
            iRateAdaptationFeedBackFrequency = 0;
            iFormatType = PVMF_MIME_FORMAT_UNKNOWN;
            iRTCPBwSpecified = false;
            iTrackDisable = false;
        };

        PVMFRTSPTTrackInfo(const PVMFRTSPTTrackInfo& a)
        {
            trackID = a.trackID;
            rdtStreamID = a.rdtStreamID;
            portTag = a.portTag;
            bitRate = a.bitRate;
            trackTimeScale = a.trackTimeScale;
            iTrackConfig = a.iTrackConfig;
            iTransportType = a.iTransportType;
            iFormatType = a.iFormatType;
            iMimeType = a.iMimeType;
            iNetworkNodePort = a.iNetworkNodePort;
            iJitterBufferInputPort = a.iJitterBufferInputPort;
            iJitterBufferOutputPort = a.iJitterBufferOutputPort;
            iJitterBufferRTCPPort = a.iJitterBufferRTCPPort;
            iNetworkNodeRTCPPort = a.iNetworkNodeRTCPPort;
            iSessionControllerOutputPort = a.iSessionControllerOutputPort;
            iSessionControllerFeedbackPort = a.iSessionControllerFeedbackPort;
            iRTPSocketID = a.iRTPSocketID;
            iRTCPSocketID = a.iRTCPSocketID;
            iRateAdaptation = a.iRateAdaptation;
            iRateAdaptationFeedBackFrequency = a.iRateAdaptationFeedBackFrequency;
            iRTCPBwSpecified = a.iRTCPBwSpecified;
            iTrackDisable = a.iTrackDisable;
            iRR = a.iRR;
            iRS = a.iRS;

        };

        PVMFRTSPTTrackInfo& operator=(const PVMFRTSPTTrackInfo& a)
        {
            if (&a != this)
            {
                trackID = a.trackID;
                rdtStreamID = a.rdtStreamID;
                portTag = a.portTag;
                bitRate = a.bitRate;
                trackTimeScale = a.trackTimeScale;
                iTrackConfig = a.iTrackConfig;
                iTransportType = a.iTransportType;
                iFormatType = a.iFormatType;
                iMimeType = a.iMimeType;
                iNetworkNodePort = a.iNetworkNodePort;
                iJitterBufferInputPort = a.iJitterBufferInputPort;
                iJitterBufferOutputPort = a.iJitterBufferOutputPort;
                iJitterBufferRTCPPort = a.iJitterBufferRTCPPort;
                iNetworkNodeRTCPPort = a.iNetworkNodeRTCPPort;
                iSessionControllerOutputPort = a.iSessionControllerOutputPort;
                iSessionControllerFeedbackPort = a.iSessionControllerFeedbackPort;
                iRTPSocketID = a.iRTPSocketID;
                iRTCPSocketID = a.iRTCPSocketID;
                iRateAdaptation = a.iRateAdaptation;
                iRateAdaptationFeedBackFrequency = a.iRateAdaptationFeedBackFrequency;
                iRTCPBwSpecified = a.iRTCPBwSpecified;
                iTrackDisable = a.iTrackDisable;
                iRR = a.iRR;
                iRS = a.iRS;

            }
            return *this;
        };

        virtual ~PVMFRTSPTTrackInfo() {};

        uint32 trackID;
        uint32 rdtStreamID;
        uint32 portTag;
        uint32 bitRate;
        uint32 trackTimeScale;
        OsclRefCounterMemFrag iTrackConfig;
        OSCL_HeapString<OsclMemAllocator> iTransportType;
        PVMFFormatType iFormatType;
        OSCL_HeapString<OsclMemAllocator> iMimeType;
        PVMFPortInterface* iNetworkNodePort;
        PVMFPortInterface* iJitterBufferInputPort;
        PVMFPortInterface* iJitterBufferOutputPort;
        PVMFPortInterface* iJitterBufferRTCPPort;
        PVMFPortInterface* iNetworkNodeRTCPPort;
        PVMFPortInterface* iSessionControllerOutputPort;
        PVMFPortInterface* iSessionControllerFeedbackPort;

        uint32 iRTPSocketID;
        uint32 iRTCPSocketID;
        bool   iRateAdaptation;
        uint32 iRateAdaptationFeedBackFrequency;

        // RTCP bandwidth related
        bool   iRTCPBwSpecified;
        uint32 iRR;
        uint32 iRS;

        //Check track disable or not
        bool iTrackDisable;


};
typedef Oscl_Vector<PVMFRTSPTTrackInfo, OsclMemAllocator> PVMFRTSPTTrackInfoVector;

class   IRealChallengeGen;
class    IPayloadParser;

class PVMFSMRTSPTUnicastNode: public PVMFSMFSPBaseNode
{
    public:
        static PVMFSMRTSPTUnicastNode* New(int32 aPriority);
        OSCL_IMPORT_REF virtual ~PVMFSMRTSPTUnicastNode();


        //Function to handle command completion from child nodes
        /* From PVMFNodeCmdStatusObserver */
        OSCL_IMPORT_REF virtual void NodeCommandCompleted(const PVMFCmdResp& aResponse);

        /* From PVMFDataSourceInitializationExtensionInterface */
        OSCL_IMPORT_REF virtual PVMFStatus SetSourceInitializationData(OSCL_wString& aSourceURL,
                PVMFFormatType& aSourceFormat,
                OsclAny* aSourceData,
                uint32 aClipIndex,
                PVMFFormatTypeDRMInfo aType = PVMF_FORMAT_TYPE_CONNECT_DRM_INFO_UNKNOWN);
        OSCL_IMPORT_REF virtual PVMFStatus SetClientPlayBackClock(PVMFMediaClock* aClientClock);
        OSCL_IMPORT_REF virtual PVMFStatus SetEstimatedServerClock(PVMFMediaClock* aClientClock);
        OSCL_IMPORT_REF virtual void AudioSinkEvent(PVMFStatus aEvent, uint32 aStreamId);


        /* From PVMFTrackSelectionExtensionInterface */
        OSCL_IMPORT_REF virtual PVMFStatus GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo);
        OSCL_IMPORT_REF virtual PVMFStatus SelectTracks(PVMFMediaPresentationInfo& aInfo);

        /* From PVMFMetadataExtensionInterface */
        OSCL_IMPORT_REF PVMFStatus SetMetadataClipIndex(uint32 aClipNum);
        OSCL_IMPORT_REF virtual uint32 GetNumMetadataKeys(char* aQueryKeyString = NULL);
        OSCL_IMPORT_REF virtual uint32 GetNumMetadataValues(PVMFMetadataList& aKeyList);
        OSCL_IMPORT_REF PVMFCommandId DoGetMetadataKeys(PVMFSMFSPBaseNodeCommand& aCmd);
        OSCL_IMPORT_REF PVMFCommandId DoGetMetadataValues(PVMFSMFSPBaseNodeCommand& aCmd);
        OSCL_IMPORT_REF virtual PVMFStatus ReleaseNodeMetadataKeys(PVMFMetadataList& aKeyList,
                uint32 aStartingKeyIndex,
                uint32 aEndKeyIndex);
        OSCL_IMPORT_REF virtual PVMFStatus ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                uint32 aStartingValueIndex,
                uint32 aEndValueIndex);


        OSCL_IMPORT_REF virtual void setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver);
        OSCL_IMPORT_REF virtual PVMFStatus getParametersSync(PvmiMIOSession aSession,
                PvmiKeyType aIdentifier,
                PvmiKvp*& aParameters,
                int& aNumParamElements,
                PvmiCapabilityContext aContext);
        OSCL_IMPORT_REF virtual PVMFStatus releaseParameters(PvmiMIOSession aSession,
                PvmiKvp* aParameters,
                int num_elements);

        OSCL_IMPORT_REF virtual void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                int num_elements, PvmiKvp * & aRet_kvp);
        OSCL_IMPORT_REF virtual PVMFStatus verifyParametersSync(PvmiMIOSession aSession,
                PvmiKvp* aParameters,
                int num_elements);
        /**/
        OSCL_IMPORT_REF virtual void HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent);
        OSCL_IMPORT_REF PVMFStatus ComputeSkipTimeStamp(PVMFTimestamp aTargetNPT,
                PVMFTimestamp aActualNPT,
                PVMFTimestamp aActualMediaDataTS,
                PVMFTimestamp& aSkipTimeStamp,
                PVMFTimestamp& aStartNPT);
    protected:
        OSCL_IMPORT_REF PVMFSMRTSPTUnicastNode(int32 aPriority);
        OSCL_IMPORT_REF void Construct();
        OSCL_IMPORT_REF void CreateCommandQueues();
        OSCL_IMPORT_REF void CreateChildNodes();
        OSCL_IMPORT_REF virtual void DestroyChildNodes();
        OSCL_IMPORT_REF virtual void CreateSessionControllerNode();
        OSCL_IMPORT_REF void CreateJitterBufferNode();
        OSCL_IMPORT_REF void CleanUp();

        OSCL_IMPORT_REF virtual bool ProcessCommand(PVMFSMFSPBaseNodeCommand&);

        //Functions to service commands queued up in input command Q by base class
        //node commands
        OSCL_IMPORT_REF void DoQueryInterface(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoRequestPort(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoReleasePort(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoInit(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoPrepare(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoStart(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoStop(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoPause(PVMFSMFSPBaseNodeCommand&);


        OSCL_IMPORT_REF void DoSetDataSourcePosition(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoSetDataSourcePositionPlayList(PVMFSMFSPBaseNodeCommand&);
        OSCL_IMPORT_REF void DoQueryDataSourcePosition(PVMFSMFSPBaseNodeCommand&);


        OSCL_IMPORT_REF bool CompleteFeedBackPortsSetup();
        OSCL_IMPORT_REF virtual bool SendSessionControlPrepareCompleteParams();
        OSCL_IMPORT_REF bool SendSessionControlStartCompleteParams();

        OSCL_IMPORT_REF void GetActualMediaTSAfterSeek();

        OSCL_IMPORT_REF void ResetStopCompleteParams();

        OSCL_IMPORT_REF void QueryChildNodesExtentionInterface();

        //RequestPort
        OSCL_IMPORT_REF PVMFRTSPTTrackInfo* FindTrackInfo(uint32 tag);

        //Init
        OSCL_IMPORT_REF virtual PVMFStatus DoPreInit(PVMFSMFSPBaseNodeCommand& aCmd);
        OSCL_IMPORT_REF PVMFStatus ProcessSDP();
        OSCL_IMPORT_REF PVMFStatus PopulateAvailableMetadataKeys();
        virtual bool RequestUsageComplete()
        {
            return true;
        }

        //Graph Construction
        OSCL_IMPORT_REF PVMFStatus DoGraphConstruct();
        OSCL_IMPORT_REF bool GraphConnect();
        OSCL_IMPORT_REF bool RequestRTSPNodePorts(int32, uint32&);
        OSCL_IMPORT_REF bool RequestJitterBufferPorts(int32 portType, uint32 &numPortsRequested);
        OSCL_IMPORT_REF bool SendSessionSourceInfoToSessionController();
        OSCL_IMPORT_REF bool PopulateTrackInfoVec();
        OSCL_IMPORT_REF PVMFStatus ConnectPortPairs(PVMFPortInterface* aPort1, PVMFPortInterface* aPort2);
        OSCL_IMPORT_REF bool ConstructGraphFor3GPPTCPStreaming();
        OSCL_IMPORT_REF void CompleteGraphConstruct();
        OSCL_IMPORT_REF PVMFStatus  InitMetaData();
        OSCL_IMPORT_REF virtual void PopulateDRMInfo();

        OSCL_IMPORT_REF void DoSetDataSourcePosition();
        OSCL_IMPORT_REF bool DoRepositioningPause3GPPStreaming();
        OSCL_IMPORT_REF PVMFStatus DoRepositioningStart3GPPStreaming();
        OSCL_IMPORT_REF bool DoRepositioningStart3GPPPlayListStreamingDuringPlay();
        OSCL_IMPORT_REF bool DoRepositioningStart3GPPPlayListStreaming();


        OSCL_IMPORT_REF PVMFStatus SetRTSPPlaybackRange();
        OSCL_IMPORT_REF bool CanPerformRepositioning(bool aRandAccessDenied);

        OSCL_IMPORT_REF void HandleChildNodeCommandCompletion(const PVMFCmdResp& , bool&);

        OSCL_IMPORT_REF void CompleteInit();
        OSCL_IMPORT_REF bool CheckChildrenNodesInit();

        OSCL_IMPORT_REF void CompletePrepare();
        OSCL_IMPORT_REF bool CheckChildrenNodesPrepare();

        OSCL_IMPORT_REF void CompleteStart();
        OSCL_IMPORT_REF bool CheckChildrenNodesStart();

        OSCL_IMPORT_REF void CompleteStop();
        OSCL_IMPORT_REF bool CheckChildrenNodesStop();

        OSCL_IMPORT_REF void CompletePause();
        OSCL_IMPORT_REF bool CheckChildrenNodesPause();

        //For parsing the possible payloads received by streaming protocols
        OSCL_IMPORT_REF void PopulatePayloadParserRegistry();
        OSCL_IMPORT_REF void DestroyPayloadParserRegistry();

        OSCL_IMPORT_REF void HandleSocketNodeCommandCompleted(const PVMFCmdResp&, bool& aPerformErrHandling);
        OSCL_IMPORT_REF void HandleRTSPSessionControllerCommandCompleted(const PVMFCmdResp&, bool& aPerformErrHandling);
        OSCL_IMPORT_REF void HandleJitterBufferCommandCompleted(const PVMFCmdResp&, bool& aPerformErrHandling);
        OSCL_IMPORT_REF PVMFStatus GetConfigParameter(PvmiKvp*& aParameters, int& aNumParamElements,
                int32 aIndex, PvmiKvpAttr reqattr);
        OSCL_IMPORT_REF PVMFStatus VerifyAndSetConfigParameter(int index, PvmiKvp& aParameter, bool set);
        OSCL_IMPORT_REF void setJitterBufferDurationInMilliSeconds(uint32 duration);
        OSCL_IMPORT_REF bool IsFSPInternalCmd(PVMFCommandId aId);
        OSCL_IMPORT_REF void addRef();
        OSCL_IMPORT_REF void removeRef();
        OSCL_IMPORT_REF void ResetNodeParams(bool aReleaseMemmory = true);
        OSCL_IMPORT_REF virtual void DeleteSessionControllerNode(uint32 aIndex);

        uint32 iJitterBufferDurationInMilliSeconds;
        bool ibRdtTransport;
        PVMFMediaPresentationInfo iCompleteMediaPresetationInfo;
        PVMFMediaPresentationInfo iSelectedMediaPresetationInfo;
        IRealChallengeGen* ipRealChallengeGen;
        IPayloadParser*    ipRdtParser;

        PVMFRTSPTTrackInfoVector    iTrackInfoVec;
        OsclSharedPtr<SDPInfo> iSdpInfo;
};
#endif
