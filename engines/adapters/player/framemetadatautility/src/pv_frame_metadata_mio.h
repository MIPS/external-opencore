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
#ifndef PV_FRAME_METADATA_MIO_H_INCLUDED
#define PV_FRAME_METADATA_MIO_H_INCLUDED

#ifndef PVMI_MIO_CONTROL_H_INCLUDED
#include "pvmi_mio_control.h"
#endif
#ifndef PVMI_MEDIA_TRANSFER_H_INCLUDED
#include "pvmi_media_transfer.h"
#endif
#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif
#ifndef PVMI_MEDIA_IO_OBSERVER_H_INCLUDED
#include "pvmi_media_io_observer.h"
#endif
#ifndef PVMI_CONFIG_AND_CAPABILITY_BASE_H_INCLUDED
#include "pvmi_config_and_capability_base.h"
#endif
#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif
#ifndef PVMI_MEDIA_IO_CLOCK_EXTENSION_H_INCLUDED
#include "pvmi_media_io_clock_extension.h"
#endif

class PVLogger;
class PVMFMediaClock;

// Provide the clock interface so the MIO can do synchronization
class PVFMMIOActiveTimingSupport: public PvmiClockExtensionInterface
{
    public:
        PVFMMIOActiveTimingSupport() :
                iClock(NULL)
        {}

        virtual ~PVFMMIOActiveTimingSupport()
        {}

        // From PvmiClockExtensionInterface
        PVMFStatus SetClock(PVMFMediaClock *aClock);

        // From PVInterface
        void addRef() ;
        void removeRef() ;
        bool queryInterface(const PVUuid& uuid, PVInterface*& iface) ;

        PVMFMediaClock* iClock;
};

// This class implements the generic media IO component
// for PVFrameAndMetadata utility.
class PVFMMIO
        : public OsclTimerObject
        , public PvmiMIOControl
        , public PvmiMediaTransfer
        , public PvmiCapabilityAndConfigBase
{
    public:
        PVFMMIO();
        virtual ~PVFMMIO();

        // From PvmiMIOControl
        virtual PVMFStatus connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver);
        virtual PVMFStatus disconnect(PvmiMIOSession aSession);
        virtual PVMFCommandId QueryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr, const OsclAny* aContext = NULL);
        virtual PvmiMediaTransfer* createMediaTransfer(PvmiMIOSession& aSession, PvmiKvp* read_formats = NULL, int32 read_flags = 0,
                PvmiKvp* write_formats = NULL, int32 write_flags = 0);
        virtual void deleteMediaTransfer(PvmiMIOSession& aSession, PvmiMediaTransfer* media_transfer);
        virtual PVMFCommandId Init(const OsclAny* aContext = NULL);
        virtual PVMFCommandId Reset(const OsclAny* aContext = NULL);
        virtual PVMFCommandId Start(const OsclAny* aContext = NULL);
        virtual PVMFCommandId Pause(const OsclAny* aContext = NULL);
        virtual PVMFCommandId Flush(const OsclAny* aContext = NULL);
        virtual PVMFCommandId DiscardData(const OsclAny* aContext = NULL);
        virtual PVMFCommandId DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext = NULL);
        virtual PVMFCommandId Stop(const OsclAny* aContext = NULL);
        virtual PVMFCommandId CancelAllCommands(const OsclAny* aContext = NULL);
        virtual PVMFCommandId CancelCommand(PVMFCommandId aCmdId, const OsclAny* aContext = NULL);
        virtual void ThreadLogon();
        virtual void ThreadLogoff();

        // From PvmiMediaTransfer
        virtual void setPeer(PvmiMediaTransfer* aPeer);
        virtual void useMemoryAllocators(OsclMemAllocator* write_alloc = NULL);
        virtual PVMFCommandId writeAsync(uint8 format_type, int32 format_index, uint8* data, uint32 data_len,
                                         const PvmiMediaXferHeader& data_header_info, OsclAny* aContext = NULL) = 0;
        virtual void writeComplete(PVMFStatus aStatus, PVMFCommandId write_cmd_id, OsclAny* aContext);
        virtual PVMFCommandId readAsync(uint8* data, uint32 max_data_len, OsclAny* aContext = NULL,
                                        int32* formats = NULL, uint16 num_formats = 0);
        virtual void readComplete(PVMFStatus aStatus, PVMFCommandId  read_cmd_id, int32 format_index,
                                  const PvmiMediaXferHeader& data_header_info, OsclAny* aContext);
        virtual void statusUpdate(uint32 status_flags);
        virtual void cancelCommand(PVMFCommandId  command_id);
        virtual void cancelAllCommands();

        // From PvmiCapabilityAndConfig
        virtual PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier, PvmiKvp*& aParameters,
                                             int& num_parameter_elements, PvmiCapabilityContext aContext) = 0;
        virtual PVMFStatus releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);
        virtual void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements, PvmiKvp*& aRet_kvp) = 0;
        virtual PVMFStatus verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);

        // From OsclTimerObject
        void Run();

        // State
        enum PVFMVMIOState
        {
            STATE_IDLE
            , STATE_LOGGED_ON
            , STATE_INITIALIZED
            , STATE_STARTED
            , STATE_PAUSED
        };
        PVFMVMIOState iState;

        // Control command handling.
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

        // Write command handling
        class WriteResponse
        {
            public:
                WriteResponse(PVMFStatus s, PVMFCommandId id, const OsclAny* ctx, const PVMFTimestamp& ts)
                        : iStatus(s), iCmdId(id), iContext(ctx), iTimestamp(ts)
                {}

                PVMFStatus iStatus;
                PVMFCommandId iCmdId;
                const OsclAny* iContext;
                PVMFTimestamp iTimestamp;
        };

        PVLogger* iLogger;
        PvmiMediaTransfer* iPeer;
        PvmiMIOObserver* iObserver;
        uint32 iCommandCounter;
        bool iIsMIOConfigured;
        bool iWriteBusy;
        Oscl_Vector<CommandResponse, OsclMemAllocator> iCommandResponseQueue;
        Oscl_Vector<WriteResponse, OsclMemAllocator> iWriteResponseQueue;
        Oscl_Vector<PVMFFormatType, OsclMemAllocator> iInputFormatCapability;
        // MIO clock extension so synchronization would be handled in MIO
        PVFMMIOActiveTimingSupport iActiveTiming;

        void QueueCommandResponse(CommandResponse&);
        void InitData();
        void ResetData();
        virtual void Cleanup();
};

#endif // PV_FRAME_METADATA_MIO_H_INCLUDED

