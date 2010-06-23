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
 * @file pvmi_io_interface_node.h
 * @brief
 */

#ifndef PV_MEDIA_OUTPUT_NODE_H_INCLUDED
#define PV_MEDIA_OUTPUT_NODE_H_INCLUDED

#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#include "pvmf_node_interface_impl.h"
#endif

#ifndef PVMF_PORT_INTERFACE_H_INCLUDED
#include "pvmf_port_interface.h"
#endif

#ifndef PVMI_MIO_CONTROL_H_INCLUDED
#include "pvmi_mio_control.h"
#endif

#ifndef PVMI_CONFIG_AND_CAPABILITY_BASE_H_INCLUDED
#include "pvmi_config_and_capability_base.h"
#endif

#ifndef PV_MEDIA_OUTPUT_NODE_INPORT_H_INCLUDED
#include "pv_media_output_node_inport.h"
#endif

#ifndef PV_MEDIA_OUTPUT_NODE_EVENTS_H_INCLUDED
#include "pv_media_output_node_events.h"
#endif

#ifndef PVMI_MEDIA_IO_CLOCK_EXTENSION_H_INCLUDED
#include "pvmi_media_io_clock_extension.h"
#endif

#ifndef PV_MEDIA_OUTPUT_NODE_FACTORY_H_INCLUDED
#include "pv_media_output_node_factory.h"
#endif

#ifndef OSCL_DLL_H_INCLUDED
#include "oscl_dll.h"
#endif

// Define entry point for this DLL
OSCL_DLL_ENTRY_POINT_DEFAULT()

// Macros for calling PVLogger
#define LOGREPOS(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iReposLogger,PVLOGMSG_INFO,m);
#define LOGERROR(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iLogger,PVLOGMSG_ERR,m);
#define LOGINFOHI(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG,iLogger,PVLOGMSG_INFO,m);
#define LOGINFOMED(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG,iLogger,PVLOGMSG_INFO,m);
#define LOGINFOLOW(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG,iLogger,PVLOGMSG_INFO,m);
#define LOGINFO(m) LOGINFOMED(m)
#define LOGDIAGNOSTICS(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_PROF,iDiagnosticsLogger,PVLOGMSG_INFO,m);

//this should always be 1. set this to zero if
//you want to bypass avsync (typically used in
//case one wants to decode and render ASAP)
#define PVMF_MEDIA_OUTPUT_NODE_ENABLE_AV_SYNC 1


// Port tags.  For now engine must use these directly since
// port tag query is not yet implemented
enum PVMediaOutputNodePortTags
{
    PVMF_MEDIAIO_NODE_INPUT_PORT_TAG = 0
};

//This node has one extra async command.
enum PVMediaOutputNodeCmdType
{
    PVMF_MEDIAOUTPUTNODE_SKIPMEDIADATA = PVMF_GENERIC_NODE_COMMAND_LAST
};

// class PVMediaOutputNode is a node wrapper around the io interface
class PVMediaOutputNode
        : public PVMFNodeInterfaceImpl
        , public PvmiMIOObserver
        , public PvmfNodesSyncControlInterface
        , public PvmiCapabilityAndConfigBase
{
    public:
        static PVMFNodeInterface* Create(PvmiMIOControl* aIOInterfacePtr);
        static void Release(PVMFNodeInterface*);

        ~PVMediaOutputNode();
        // PVMFNodeInterface implementation
        OSCL_IMPORT_REF PVMFStatus ThreadLogon();
        OSCL_IMPORT_REF PVMFStatus ThreadLogoff();
        OSCL_IMPORT_REF PVMFStatus GetCapability(PVMFNodeCapability& aNodeCapability);
        OSCL_IMPORT_REF PVMFPortIter* GetPorts(const PVMFPortFilter* aFilter = NULL);

        void HandlePortActivity(const PVMFPortActivity& aActivity)
        {
            OSCL_UNUSED_ARG(aActivity);
        }

        // Pure virtual from PvInterface
        OSCL_IMPORT_REF void addRef();
        OSCL_IMPORT_REF void removeRef();
        OSCL_IMPORT_REF bool queryInterface(const PVUuid& uuid, PVInterface*& iface);

        // Pure virtuals from PvmfNodesSyncControlInterface
        OSCL_IMPORT_REF PVMFStatus SetClock(PVMFMediaClock* aClock);
        OSCL_IMPORT_REF PVMFStatus ChangeClockRate(int32 aRate);
        OSCL_IMPORT_REF PVMFStatus SetMargins(int32 aEarlyMargin, int32 aLateMargin);
        OSCL_IMPORT_REF void ClockStarted(void);
        OSCL_IMPORT_REF void ClockStopped(void);
        OSCL_IMPORT_REF PVMFCommandId SkipMediaData(PVMFSessionId aSession,
                PVMFTimestamp aResumeTimestamp,
                uint32 aStreamID = 0,
                bool aPlayBackPositionContinuous = false,
                OsclAny* aContext = NULL);

        // PvmiMIOObserver implementation
        OSCL_IMPORT_REF void RequestCompleted(const PVMFCmdResp& aResponse);
        OSCL_IMPORT_REF void ReportErrorEvent(PVMFEventType aEventType, PVInterface* aExtMsg = NULL);
        OSCL_IMPORT_REF void ReportInfoEvent(PVMFEventType aEventType, PVInterface* aExtMsg = NULL);


        bool IsMioRequestPending()
        {
            return (iMediaIORequest != ENone);
        };

        // From PvmiCapabilityAndConfig
        // Implement pure virtuals from PvmiCapabilityAndConfig interface
        OSCL_IMPORT_REF virtual PVMFStatus verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int aNumElements);
        OSCL_IMPORT_REF virtual PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier, PvmiKvp*& aParameters, int& aNumParamElements, PvmiCapabilityContext aContext);
        OSCL_IMPORT_REF virtual PVMFStatus releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int aNumElements);

        uint32 getClockRate()
        {
            return iClockRate;
        };

        void ReportBOS();

    private:
        friend class PVMediaOutputNodePort;
        friend class PVMediaOutputNodeFactory;

        PVMediaOutputNode();

        void ConstructL(PvmiMIOControl* aIOInterfacePtr);

        //from OsclActiveObject
        void Run();

        //Command processing
        void CommandComplete(PVMFNodeCommand& aCmd, PVMFStatus aStatus, OsclAny*aEventData = NULL);

        //Command dispatchers
        PVMFStatus HandleExtensionAPICommands();
        PVMFStatus CancelCurrentCommand();

        //Command handlers for PVMFNodeInterface APIs.
        PVMFStatus DoReset();
        PVMFStatus DoQueryInterface();
        PVMFStatus DoRequestPort(PVMFPortInterface*& aPort);
        PVMFStatus DoReleasePort();
        PVMFStatus DoInit();
        PVMFStatus DoPrepare();
        PVMFStatus DoStart();
        PVMFStatus DoStop();
        PVMFStatus DoFlush();
        PVMFStatus DoPause();

        //extra command handlers.
        PVMFStatus DoSkipMediaData();
        void CompleteSkipMediaData();

        // Event reporting
        PVUuid iEventUuid;
        void ReportErrorEvent(PVMFEventType aEventType, OsclAny* aEventData = NULL, PVMFStatus aEventCode = PVMFMoutNodeErr_First);
        void ReportErrorEvent(PVMFAsyncEvent& aEvent)
        {
            PVMFNodeInterface::ReportErrorEvent(aEvent);
        }
        void ReportInfoEvent(PVMFEventType aEventType, OsclAny* aEventData = NULL, PVMFStatus aEventCode = PVMFMoutNodeErr_First);
        void ReportInfoEvent(PVMFAsyncEvent& aEvent)
        {
            PVMFNodeInterface::ReportInfoEvent(aEvent);
        }

        // Media IO control
        PvmiMIOControl* iMIOControl;
        PvmiMIOSession iMIOSession;
        PvmiCapabilityAndConfig* iMIOConfig;
        PVInterface* iMIOConfigPVI;
        enum EMioRequest
        {
            ENone
            , EQueryCapability
            , EQueryClockExtension
            , EInit
            , EStart
            , EPause
            , EStop
            , EDiscard
            , EReset
        } ;
        EMioRequest iMediaIORequest;
        enum MioStates
        {
            STATE_IDLE
            , STATE_LOGGED_ON
            , STATE_INITIALIZED
            , STATE_STARTED
            , STATE_PAUSED
        };
        MioStates iMediaIOState;
        PVMFCommandId iMediaIOCmdId;
        PVMFCommandId iMediaIOCancelCmdId;
        bool iMediaIOCancelPending;
        PVMFStatus SendMioRequest(EMioRequest);
        PVMFStatus CancelMioRequest();

        // Ports
        PVMFPortVector<PVMediaOutputNodePort, OsclMemAllocator> iInPortVector;
        bool PortQueuesEmpty();

        // Variables for media data queue and synchronization
        PVMFMediaClock* iClock;
        int32 iEarlyMargin;
        int32 iLateMargin;
        int32 iClockRate;
        PvmiClockExtensionInterface* iMIOClockExtension;
        PVInterface* iMIOClockExtensionPVI;

        /* Diagnostic log related */
        PVLogger* iDiagnosticsLogger;
        bool iDiagnosticsLogged;
        void LogDiagnostics();

        // Counter for number of callbacks for skip media data completion
        bool SkipMediaDataComplete();

        //logger
        PVLogger* iReposLogger;

        OSCL_HeapString<OsclMemAllocator> iSinkFormatString;

        uint32 iRecentBOSStreamID;
        PVMFStatus CheckForBOS();

        //Holds an event code associated with the command status
        PVMFStatus iEventCode;
};


#endif // PVMI_IO_INTERFACE_NODE_H_INCLUDED


