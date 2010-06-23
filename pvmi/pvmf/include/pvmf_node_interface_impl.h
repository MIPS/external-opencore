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
#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#define PVMF_NODE_INTERFACE_IMPL_H_INCLUDED

#ifndef PVMF_NODE_UTILS_H_INCLUDED
#include "pvmf_node_utils.h"
#endif

#ifndef PVMI_CONFIG_AND_CAPABILITY_H_INCLUDED
#include "pvmi_config_and_capability.h"
#endif

#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif

#ifndef PVMF_MEDIA_CLOCK_H_INCLUDED
#include "pvmf_media_clock.h"
#endif

#ifndef PVMF_NODE_INTERFACE_H_INCLUDED
#include "pvmf_node_interface.h"
#endif

#ifndef PVMF_META_DATA_TYPES_H_INCLUDED
#include "pvmf_meta_data_types.h"
#endif

#ifndef PVMF_BASIC_ERRORINFOMESSAGE_H_INCLUDED
#include "pvmf_basic_errorinfomessage.h"
#endif

#ifndef OSCL_MEM_AUDIT_H_INCLUDED
#include "oscl_mem_audit.h"
#endif

#ifndef PVMF_DATA_SOURCE_PLAYBACK_CONTROL_H_INCLUDED
#include "pvmf_data_source_playback_control.h"
#endif

#ifndef PVMI_DATA_STREAM_INTERFACE_H_INCLUDED
#include "pvmi_data_stream_interface.h"
#endif

/* Default vector reserve size */
#define PVMF_NODE_VECTOR_RESERVE 10
/* Starting value for command IDs  */
#define PVMF_NODE_COMMAND_ID_START 0

// Allocation/Deallocation macros
#define PVMF_BASE_NODE_NEW(T,params) OSCL_AUDIT_NEW(iAuditCB, T, params)
#define PVMF_BASE_NODE_ARRAY_NEW(T,params) OSCL_AUDIT_ARRAY_NEW(iAuditCB, T,params)
#define PVMF_BASE_NODE_DELETE(ptr) OSCL_DELETE(ptr)
#define PVMF_BASE_NODE_ARRAY_DELETE(ptr) OSCL_ARRAY_DELETE(ptr)

// Default value of media sample Duration
#define PVMF_DEFAULT_TRACK_DURATION 0xFFFFFFFF

#define PVMFNodeCommandBase PVMFGenericNodeCommand<OsclMemAllocator>  // to remove typedef warning on symbian
class PVMFNodeCommand: public PVMFNodeCommandBase
{

    public:
        // Constructor and parser for GetNodeMetadataValue
        void Construct(PVMFSessionId s, int32 cmd, PVMFMetadataList& aKeyList, Oscl_Vector < PvmiKvp,
                       OsclMemAllocator > & aValueList, uint32 aStartIndex, int32 aMaxEntries, const OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*) & aKeyList;
            iParam2 = (OsclAny*) & aValueList;
            iParam3 = (OsclAny*)aStartIndex;
            iParam4 = (OsclAny*)aMaxEntries;

        }
        void Parse(PVMFMetadataList* &aKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator>* &aValueList,
                   uint32 &aStartingIndex, int32 &aMaxEntries)
        {
            aKeyList = (PVMFMetadataList*)iParam1;
            aValueList = (Oscl_Vector<PvmiKvp, OsclMemAllocator>*)iParam2;
            aStartingIndex = (uint32)iParam3;
            aMaxEntries = (int32)iParam4;
        }

        // Constructor and parser for SetDataSourcePosition
        void Construct(PVMFSessionId s, int32 cmd, PVMFTimestamp aTargetNPT, PVMFTimestamp* aActualNPT,
                       PVMFTimestamp* aActualMediaDataTS, bool aSeekToSyncPoint, uint32 aStreamID, const OsclAny*aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*)aTargetNPT;
            iParam2 = (OsclAny*)aActualNPT;
            iParam3 = (OsclAny*)aActualMediaDataTS;
            iParam4 = (OsclAny*)aSeekToSyncPoint;
            iParam5 = (OsclAny*)aStreamID;
        }
        void Parse(PVMFTimestamp& aTargetNPT, PVMFTimestamp* &aActualNPT, PVMFTimestamp* &aActualMediaDataTS,
                   bool& aSeekToSyncPoint, uint32& aStreamID)
        {
            aTargetNPT = (PVMFTimestamp)iParam1;
            aActualNPT = (PVMFTimestamp*)iParam2;
            aActualMediaDataTS = (PVMFTimestamp*)iParam3;
            aSeekToSyncPoint = (iParam4 ? true : false);
            aStreamID = (uint32)iParam5;
        }

        /* Constructor and parser for setParametersAsync */
        void Construct(PVMFSessionId s, int32 cmd, PvmiMIOSession aSession, PvmiKvp* aParameters,
                       int num_elements, PvmiKvp*& aRet_kvp, OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*)aSession;
            iParam2 = (OsclAny*)aParameters;
            iParam3 = (OsclAny*)num_elements;
            iParam4 = (OsclAny*) & aRet_kvp;
        }
        void Parse(PvmiMIOSession& aSession, PvmiKvp*& aParameters,
                   int &num_elements, PvmiKvp** &ppRet_kvp)
        {
            aSession = (PvmiMIOSession)iParam1;
            aParameters = (PvmiKvp*)iParam2;
            num_elements = (int)iParam3;
            ppRet_kvp = (PvmiKvp**)iParam4;
        }

        // Constructor and parser for SetDataSourcePosition
        void Construct(PVMFSessionId aSessionId, int32 cmd, PVMFDataSourcePositionParams* aPVMFDataSourcePositionParams,
                       const OsclAny*aContext)
        {
            PVMFNodeCommandBase::Construct(aSessionId, cmd, aContext);
            iParam1 = (OsclAny*)aPVMFDataSourcePositionParams;
            iParam2 = NULL;
            iParam3 = NULL;
            iParam4 = NULL;
            iParam5 = NULL;
        }
        void Parse(PVMFDataSourcePositionParams*& aPVMFDataSourcePositionParams)
        {
            aPVMFDataSourcePositionParams = (PVMFDataSourcePositionParams*)iParam1;
        }

        // Constructor and parser for QueryDataSourcePosition
        void Construct(PVMFSessionId s, int32 cmd, PVMFTimestamp aTargetNPT, PVMFTimestamp* aActualNPT,
                       bool aSeekToSyncPoint, const OsclAny*aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*)aTargetNPT;
            iParam2 = (OsclAny*)aActualNPT;
            iParam3 = (OsclAny*)aSeekToSyncPoint;
            iParam4 = NULL;
            iParam5 = NULL;
        }
        void Parse(PVMFTimestamp& aTargetNPT, PVMFTimestamp* &aActualNPT, bool& aSeekToSyncPoint)
        {
            aTargetNPT = (PVMFTimestamp)iParam1;
            aActualNPT = (PVMFTimestamp*)iParam2;
            aSeekToSyncPoint = (iParam3 ? true : false);
        }

        // Constructor and parser for QueryDataSourcePosition with aSeekPointBeforeTargetNPT and aSeekPointAfterTargetNPT
        void Construct(PVMFSessionId s, int32 cmd, PVMFTimestamp aTargetNPT,
                       PVMFTimestamp& aSeekPointBeforeTargetNPT, PVMFTimestamp& aSeekPointAfterTargetNPT,
                       const OsclAny*aContext, bool aSeekToSyncPoint)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*)aTargetNPT;
            iParam3 = (OsclAny*)aSeekToSyncPoint;
            iParam4 = (OsclAny*) & aSeekPointBeforeTargetNPT;
            iParam5 = (OsclAny*) & aSeekPointAfterTargetNPT;
        }
        void Parse(PVMFTimestamp& aTargetNPT, PVMFTimestamp*& aSeekPointBeforeTargetNPT,
                   bool& aSeekToSyncPoint, PVMFTimestamp*& aSeekPointAfterTargetNPT)
        {
            aTargetNPT = (PVMFTimestamp)iParam1;
            aSeekPointBeforeTargetNPT = (PVMFTimestamp*)iParam4;
            aSeekPointAfterTargetNPT = (PVMFTimestamp*)iParam5;
            aSeekToSyncPoint = (iParam3) ? true : false;
        }

        /* Constructor and parser for SetDataSourceDirection */
        void Construct(PVMFSessionId s, int32 cmd, int32 aDirection,
                       PVMFTimestamp& aActualNPT, PVMFTimestamp& aActualMediaDataTS,
                       PVMFTimebase* aTimebase, OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*)aDirection;
            iParam2 = (OsclAny*) & aActualNPT;
            iParam3 = (OsclAny*) & aActualMediaDataTS;
            iParam4 = (OsclAny*)aTimebase;
            iParam5 = NULL;
        }
        void Parse(int32& aDirection, PVMFTimestamp*& aActualNPT,
                   PVMFTimestamp*& aActualMediaDataTS, PVMFTimebase*& aTimebase)
        {
            aDirection = (int32)iParam1;
            aActualNPT = (PVMFTimestamp*)iParam2;
            aActualMediaDataTS = (PVMFTimestamp*)iParam3;
            aTimebase = (PVMFTimebase*)iParam4;
        }

        // Constructor and parser for SetDataSourceRate
        void Construct(PVMFSessionId s, int32 cmd, int32 aRate, PVMFTimebase* aTimebase, const OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*)aRate;
            iParam2 = (OsclAny*)aTimebase;
            iParam3 = NULL;
            iParam4 = NULL;
            iParam5 = NULL;
        }
        void Parse(int32& aRate, PVMFTimebase*& aTimebase)
        {
            aRate = (int32)iParam1;
            aTimebase = (PVMFTimebase*)iParam2;
        }

        // Constructor and parser for GetLicenseW
        void Construct(PVMFSessionId s, int32 cmd, OSCL_wString& aContentName, OsclAny* aLicenseData,
                       uint32 aDataSize, int32 aTimeoutMsec, OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*) & aContentName;
            iParam2 = (OsclAny*)aLicenseData;
            iParam3 = (OsclAny*)aDataSize;
            iParam4 = (OsclAny*)aTimeoutMsec;
            iParam5 = NULL;
        }
        void Parse(OSCL_wString*& aContentName, OsclAny*& aLicenseData, uint32& aDataSize, int32& aTimeoutMsec)
        {
            aContentName = (OSCL_wString*)iParam1;
            aLicenseData = (PVMFTimestamp*)iParam2;
            aDataSize = (uint32)iParam3;
            aTimeoutMsec = (int32)iParam4;
        }

        // Constructor and parser for GetLicense
        void Construct(PVMFSessionId s, int32 cmd, OSCL_String& aContentName, OsclAny* aLicenseData,
                       uint32 aDataSize, int32 aTimeoutMsec, OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*) & aContentName;
            iParam2 = (OsclAny*)aLicenseData;
            iParam3 = (OsclAny*)aDataSize;
            iParam4 = (OsclAny*)aTimeoutMsec;
            iParam5 = NULL;
        }
        void Parse(OSCL_String*& aContentName, OsclAny*& aLicenseData, uint32& aDataSize, int32& aTimeoutMsec)
        {
            aContentName = (OSCL_String*)iParam1;
            aLicenseData = (PVMFTimestamp*)iParam2;
            aDataSize = (uint32)iParam3;
            aTimeoutMsec = (int32)iParam4;
        }

        //constructor for Custom2 command
        void Construct(PVMFSessionId s, int32 cmd, int32 arg1, int32 arg2, int32& arg3, const OsclAny*aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*)arg1;
            iParam2 = (OsclAny*)arg2;
            iParam3 = (OsclAny*) & arg3;
        }

        void Parse(int32&arg1, int32&arg2, int32*&arg3)
        {
            arg1 = (int32)iParam1;
            arg2 = (int32)iParam2;
            arg3 = (int32*)iParam3;
        }

        void Construct(PVMFSessionId aSession, int32 cmd, uint32 arg1, uint32 arg2, uint8* arg3, const OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(aSession, cmd, aContext);
            iParam1 = (OsclAny*) arg1;
            iParam2 = (OsclAny*) arg2;
            iParam3 = (OsclAny*) arg3;
        }
        void Parse(uint32& arg1, uint32& arg2, uint8*& arg3)
        {
            arg1 = (uint32) iParam1;
            arg2 = (uint32) iParam2;
            arg3 = (uint8*) iParam3;
        }

        // Constructor and parser for seek and bitstreamSwitch
        void Construct(PVMFSessionId s, int32 cmd, uint64& aNPTInMS,
                       uint32& aFirstSeqNumAfterChange, OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*) Oscl_Int64_Utils::get_int64_upper32(aNPTInMS);
            iParam2 = (OsclAny*) Oscl_Int64_Utils::get_int64_lower32(aNPTInMS);
            iParam3 = (OsclAny*) & aFirstSeqNumAfterChange;
        }

        void Parse(uint64& aNPTInMS, uint32*& aFirstSeqNumAfterChange)
        {
            uint64 ts64 = 0;
            Oscl_Int64_Utils::set_uint64(ts64, (uint32)iParam1, (uint32)iParam2);
            aNPTInMS = ts64;
            aFirstSeqNumAfterChange = (uint32*)iParam3;
        }

        // constructor and parser for data stream request, especially reposition request
        void Construct(PVMFSessionId s, int32 cmd, PvmiDataStreamSession aSessionID,
                       PvmiDataStreamRequest aRequestID, OsclAny* aRequestData,
                       PvmiDataStreamCommandId aDataStreamCmdId, OsclAny* aContext)
        {
            PVMFNodeCommandBase::Construct(s, cmd, aContext);
            iParam1 = (OsclAny*)aSessionID;
            iParam2 = (OsclAny*)((uint32)aRequestID);
            iParam3 = aRequestData;
            iParam4 = (OsclAny*)aDataStreamCmdId;
        }

        void Parse(PvmiDataStreamSession& aSessionID, PvmiDataStreamRequest& aRequestID,
                   OsclAny*& aRequestData, PvmiDataStreamCommandId &aDataStreamCmdId)
        {
            aSessionID   = (PvmiDataStreamSession)iParam1;
            aRequestData = iParam3;
            aDataStreamCmdId = (PvmiDataStreamCommandId)iParam4;
            uint32 requestIDNum = (uint32)iParam2;
            aRequestID   = (PvmiDataStreamRequest)requestIDNum;
        }

        void Parse(OsclAny*& aRequestData)
        {
            aRequestData = iParam3;
        }

        void Parse(OsclAny*& aRequestData, PvmiDataStreamCommandId &aDataStreamCmdId)
        {
            aRequestData = iParam3;
            aDataStreamCmdId = (PvmiDataStreamCommandId)iParam4;
        }

        // Constructor and parser for SkipMediaData
        void Construct(PVMFSessionId s, int32 aCmd,
                       PVMFTimestamp aResumeTimestamp, uint32 aStreamID,
                       bool aPlayBackPositionContinuous, const OsclAny* aContext)
        {
            iSession = s;
            iCmd = aCmd;
            iContext = aContext;
            iParam1 = (OsclAny*)aResumeTimestamp;
            iParam2 = (OsclAny*)aPlayBackPositionContinuous;
            iParam3 = (OsclAny*)aStreamID;
        }

        void Parse(PVMFTimestamp& aResumeTimestamp, bool& aPlayBackPositionContinuous, uint32& aStreamID)
        {
            aResumeTimestamp = (PVMFTimestamp)iParam1;
            aPlayBackPositionContinuous = (iParam2) ? true : false;
            aStreamID = (uint32)iParam3;
        }
};

/************************************** PVMFNODEINTERFACEIMPL ***********************************/

//Command queue type
typedef PVMFNodeCommandQueue<PVMFNodeCommand, OsclMemAllocator> PVMFNodeCmdQ;

class PVMFNodeInterfaceImpl : public PVMFNodeInterface,
        public OsclActiveObject
{
    public:
        // from OsclActiveObject
        OSCL_IMPORT_REF virtual void DoCancel();

        // contstructor
        OSCL_IMPORT_REF PVMFNodeInterfaceImpl(int32 aPriority, const char name[]);
        // destructor
        OSCL_IMPORT_REF virtual ~PVMFNodeInterfaceImpl();

        // from PVMFNodeInterface
        OSCL_IMPORT_REF PVMFStatus ThreadLogon();
        OSCL_IMPORT_REF PVMFStatus ThreadLogoff();
        OSCL_IMPORT_REF virtual PVMFSessionId Connect(const PVMFNodeSessionInfo &aSession);
        OSCL_IMPORT_REF virtual PVMFStatus Disconnect(PVMFSessionId aSessionId);
        OSCL_IMPORT_REF virtual PVMFStatus GetCapability(PVMFNodeCapability& aNodeCapability);
        OSCL_IMPORT_REF virtual PVMFPortIter* GetPorts(const PVMFPortFilter* aFilter = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId QueryInterface(PVMFSessionId aSession,
                const PVUuid& aUuid,
                PVInterface*& aInterfacePtr,
                const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId RequestPort(PVMFSessionId aSession,
                int32 aPortTag,
                const PvmfMimeString* aPortConfig = NULL,
                const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId ReleasePort(PVMFSessionId aSession,
                PVMFPortInterface& aPort,
                const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId Init(PVMFSessionId aSession, const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId Prepare(PVMFSessionId aSession, const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId Start(PVMFSessionId aSession, const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId Stop(PVMFSessionId aSession, const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId Flush(PVMFSessionId aSession, const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId Pause(PVMFSessionId aSession, const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId Reset(PVMFSessionId aSession, const OsclAny* aContext = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId CancelAllCommands(PVMFSessionId aSession,
                const OsclAny* aContextData = NULL);
        OSCL_IMPORT_REF virtual PVMFCommandId CancelCommand(PVMFSessionId aSession, PVMFCommandId aCmdId,
                const OsclAny* aContextData = NULL);

        OSCL_IMPORT_REF virtual void HandlePortActivity(const PVMFPortActivity& aActivity);

        OSCL_IMPORT_REF virtual PVMFCommandId QueueCommandL(PVMFNodeCommand& aCmd);

        OSCL_IMPORT_REF void SetState(TPVMFNodeInterfaceState);
        OSCL_IMPORT_REF TPVMFNodeInterfaceState GetState();
        OSCL_IMPORT_REF bool IsCommandInProgress(PVMFNodeCommand&);
        OSCL_IMPORT_REF virtual void ReportCmdCompleteEvent(PVMFSessionId s, PVMFCmdResp &resp);
        OSCL_IMPORT_REF virtual void ReportErrorEvent(PVMFEventType aEventType, OsclAny* aEventData = NULL, PVInterface*aExtMsg = NULL, int32* aEventCode = 0);
        OSCL_IMPORT_REF virtual void ReportErrorEvent(PVMFAsyncEvent&aEvent);
        OSCL_IMPORT_REF virtual void ReportInfoEvent(PVMFEventType aEventType, OsclAny* aEventData = NULL, PVInterface*aExtMsg = NULL);
        OSCL_IMPORT_REF virtual void ReportInfoEvent(PVMFAsyncEvent&aEvent);
        OSCL_IMPORT_REF virtual void Reschedule();

    protected:
        // protected routines
        OSCL_IMPORT_REF bool SendEndOfTrackCommand(PVMFPortInterface* aPort, int32 aStreamID, PVMFTimestamp aTimestamp, uint32 aSeqNum, uint32 aClipIndex = 0, uint32 aDuration = PVMF_DEFAULT_TRACK_DURATION);
        OSCL_IMPORT_REF bool SendBeginOfMediaStreamCommand(PVMFPortInterface* aPort, int32 aStreamID, PVMFTimestamp aTimestamp,  uint32 aSeqNum = 0, uint32 aClipIndex = 0);
        OSCL_IMPORT_REF virtual void CommandComplete(PVMFNodeCommand& aCmd, PVMFStatus aStatus,
                PVInterface* aExtMsg = NULL, OsclAny* aEventData = NULL, PVUuid* aEventUUID = NULL, int32* aEventCode = NULL, int32 aEventDataLen = 0);

        // command dispatcher routiness
        OSCL_IMPORT_REF bool ProcessCommand();
        OSCL_IMPORT_REF virtual PVMFStatus HandleExtensionAPICommands() = 0;
        OSCL_IMPORT_REF virtual PVMFStatus CancelCurrentCommand() = 0;

        //command handlers
        OSCL_IMPORT_REF virtual PVMFStatus DoQueryInterface() = 0;
        OSCL_IMPORT_REF virtual PVMFStatus DoInit() = 0;
        OSCL_IMPORT_REF virtual PVMFStatus DoStop() = 0;
        OSCL_IMPORT_REF virtual PVMFStatus DoReset() = 0;
        OSCL_IMPORT_REF virtual PVMFStatus DoRequestPort(PVMFPortInterface*& aPort) = 0;
        OSCL_IMPORT_REF virtual PVMFStatus DoReleasePort() = 0;

        // command handlers have implementation in base node class.
        OSCL_IMPORT_REF virtual PVMFStatus DoPrepare();
        OSCL_IMPORT_REF virtual PVMFStatus DoStart();
        OSCL_IMPORT_REF virtual PVMFStatus DoPause();
        OSCL_IMPORT_REF virtual PVMFStatus DoCancelAllCommands();
        OSCL_IMPORT_REF virtual PVMFStatus DoCancelCommand();
        OSCL_IMPORT_REF virtual PVMFStatus DoFlush();

        // Method to tell if a flush operation is in progress.
        OSCL_IMPORT_REF virtual bool IsFlushPending();

    protected:
        // protected members
        // audit control block
        OsclAuditCB iAuditCB;
        // input command queue
        PVMFNodeCmdQ iInputCommands;
        // current command
        PVMFNodeCommand iCurrentCommand;
        // cancel command
        PVMFNodeCommand iCancelCommand;
        //stream id
        uint32 iStreamID;
        //node's capabilities
        PVMFNodeCapability iNodeCapability;
        // Reference counter for extension
        uint32 iExtensionRefCount;
        // node name
        OsclNameString<PVEXECNAMELEN> iNodeName;
    public :
        // logger object
        PVLogger* iLogger;
};

#endif //PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
