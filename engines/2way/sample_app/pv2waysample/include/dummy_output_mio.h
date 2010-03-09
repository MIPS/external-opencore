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
#ifndef DUMMY_OUTPUT_MIO_H_INCLUDED
#define DUMMY_OUTPUT_MIO_H_INCLUDED

#ifndef PVMI_MEDIA_TRANSFER_H_INCLUDED
#include "pvmi_media_transfer.h"
#endif

#ifndef PVMI_MIO_CONTROL_H_INCLUDED
#include "pvmi_mio_control.h"
#endif

#ifndef PVMI_MEDIA_IO_OBSERVER_H_INCLUDED
#include "pvmi_media_io_observer.h"
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

#ifndef OSCL_STRING_CONTAINRES_H_INCLUDED
#include "oscl_string_containers.h"
#endif

#ifndef PVMI_MEDIA_IO_CLOCK_EXTENSION_H_INCLUDED
#include "pvmi_media_io_clock_extension.h"
#endif

#ifndef DUMMY_SETTINGS_H_INCLUDED
#include "dummy_settings.h"
#endif
#ifndef DUMMY_MIO_OBSERVER_H_INCLUDED
#include "dummy_mio_observer.h"
#endif

#ifndef OSCL_MAP_H_INCLUDED
#include "oscl_map.h"
#endif

typedef enum
{
    TestMIOStateBusy = 1,
    TestMIOStateStarted = 2
} TestMIOStates;

//This class implements the test audio MIO needed for the MOutNode test harness
class DummyOutputMIO : public OsclActiveObject
        , public PvmiMIOControl
        , public PvmiMediaTransfer
        , public PvmiCapabilityAndConfig
{
    public:

        DummyOutputMIO(const DummyMIOSettings& aSettings);
        ~DummyOutputMIO();

        // APIs from PvmiMIOControl

        virtual PVMFStatus connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver);

        PVMFStatus disconnect(PvmiMIOSession aSession);

        virtual PVMFCommandId QueryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr, const OsclAny* aContext = NULL);

        PvmiMediaTransfer* createMediaTransfer(PvmiMIOSession& aSession, PvmiKvp* read_formats = NULL, int32 read_flags = 0,
                                               PvmiKvp* write_formats = NULL, int32 write_flags = 0);

        void deleteMediaTransfer(PvmiMIOSession& aSession, PvmiMediaTransfer* media_transfer);

        PVMFCommandId Init(const OsclAny* aContext = NULL);

        PVMFCommandId Reset(const OsclAny* aContext = NULL);

        PVMFCommandId Start(const OsclAny* aContext = NULL);

        PVMFCommandId Pause(const OsclAny* aContext = NULL);

        PVMFCommandId Flush(const OsclAny* aContext = NULL);

        PVMFCommandId DiscardData(const OsclAny* aContext = NULL);

        PVMFCommandId DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext = NULL);

        PVMFCommandId Stop(const OsclAny* aContext = NULL);

        PVMFCommandId CancelAllCommands(const OsclAny* aContext = NULL);

        PVMFCommandId CancelCommand(PVMFCommandId aCmdId, const OsclAny* aContext = NULL);

        void ThreadLogon();

        void ThreadLogoff();


        // APIs from PvmiMediaTransfer

        void setPeer(PvmiMediaTransfer* aPeer);

        void useMemoryAllocators(OsclMemAllocator* write_alloc = NULL);

        virtual PVMFCommandId writeAsync(uint8 format_type, int32 format_index,
                                         uint8* data, uint32 data_len,
                                         const PvmiMediaXferHeader& data_header_info,
                                         OsclAny* aContext = NULL);

        void writeComplete(PVMFStatus aStatus,
                           PVMFCommandId  write_cmd_id,
                           OsclAny* aContext);

        PVMFCommandId  readAsync(uint8* data, uint32 max_data_len,
                                 OsclAny* aContext = NULL,
                                 int32* formats = NULL, uint16 num_formats = 0);

        void readComplete(PVMFStatus aStatus, PVMFCommandId  read_cmd_id, int32 format_index,
                          const PvmiMediaXferHeader& data_header_info, OsclAny* aContext);

        void statusUpdate(uint32 status_flags);

        void cancelCommand(PVMFCommandId  command_id);

        void cancelAllCommands();

        // Pure virtuals from PvmiCapabilityAndConfig

        void setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver);

        PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier,
                                     PvmiKvp*& aParameters, int& num_parameter_elements, PvmiCapabilityContext aContext);

        virtual PVMFStatus releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);

        void createContext(PvmiMIOSession aSession, PvmiCapabilityContext& aContext);

        void setContextParameters(PvmiMIOSession aSession, PvmiCapabilityContext& aContext,
                                  PvmiKvp* aParameters, int num_parameter_elements);

        void DeleteContext(PvmiMIOSession aSession, PvmiCapabilityContext& aContext);

        void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                               int num_elements, PvmiKvp * & aRet_kvp);

        PVMFCommandId setParametersAsync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                         int num_elements, PvmiKvp*& aRet_kvp, OsclAny* context = NULL);

        uint32 getCapabilityMetric(PvmiMIOSession aSession);

        PVMFStatus verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);

        /**
        * From PVInterface
        * see base-class for more information
        */
        virtual bool queryInterface(const PVUuid& uuid, PVInterface*& iface);


        //LipSyncNotifyTSObserver
//        void MIOFramesTimeStamps(bool aIsAudio, uint32 aOrigTS, uint32 aRenderTS);
    protected:
        virtual void DealWithData(PVMFTimestamp aTimestamp, uint8* aData, uint32 aDataLen);

        //from OsclActiveObject
        void Run();

        void ResetData();
        void Cleanup();

        bool iIsMIOConfigured;

        PvmiMediaTransfer* iPeer;

        PvmiMIOObserver* iObserver;
        DummyMIOObserver* iMIOObserver;
        PVLogger* iLogger;

        uint32 iCommandCounter;

        //Control states.
        typedef enum
        {
            STATE_IDLE
            , STATE_LOGGED_ON
            , STATE_INITIALIZED
            , STATE_STARTED
            , STATE_PAUSED
        } ControlStates;
        ControlStates iState;


        //Config Parameters
//        int32 iBitsPerSample;
//        int32 iNumChannels;
//        int32 iSamplingRate;

//        bool iSamplingRateSet;
//        bool iNumChannelsSet;
//        bool iBitsPerSampleSet;
        bool iIsAudioMIO;
        bool iIsVideoMIO;
        bool iIsCompressed;
        DummyMIOSettings iSettings;

        // private:
        //Control command handling.
        class CommandResponse
        {
            public:
                CommandResponse(PVMFStatus s, PVMFCommandId id, const OsclAny* ctx)
                        : iStatus(s), iCmdId(id), iContext(ctx)
                {}

                PVMFStatus iStatus;
                PVMFCommandId iCmdId;
                const OsclAny* iContext;
        };
        void QueueCommandResponse(CommandResponse&);


        //a queue of pending commands.
        Oscl_Vector<CommandResponse, OsclMemAllocator> iCommandPendingQueue;
        //a queue of completed commands.
        Oscl_Vector<CommandResponse, OsclMemAllocator> iCommandResponseQueue;

};
#endif
