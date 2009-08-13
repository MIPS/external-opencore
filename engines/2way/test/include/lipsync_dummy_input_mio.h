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
#ifndef LIPSYNC_DUMMY_INPUT_MIO_H_INCLUDED
#define LIPSYNC_DUMMY_INPUT_MIO_H_INCLUDED

#ifndef PVMI_MEDIA_TRANSFER_H_INCLUDED
#include "pvmi_media_transfer.h"
#endif

#ifndef PVMI_MIO_CONTROL_H_INCLUDED
#include "pvmi_mio_control.h"
#endif

#ifndef LIPSYNC_SINGLETON_OBJECT_H_INCLUDED
#include "lipsync_singleton_object.h"
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

#ifndef OSCL_MEM_MEMPOOL_H_INCLUDED
#include "oscl_mem_mempool.h"
#endif

#ifndef PVMI_MEDIA_IO_CLOCK_EXTENSION_H_INCLUDED
#include "pvmi_media_io_clock_extension.h"
#endif

#ifndef LIPSYNC_DUMMY_SETTINGS_H_INCLUDED
#include "lipsync_dummy_settings.h"
#endif



typedef enum
{
    QUERY_UUID,
    QUERY_INTERFACE,
    INIT,
    START,
    PAUSE,
    FLUSH,
    STOP,
    CANCEL_ALL_COMMANDS,
    CANCEL_COMMAND,
    RESET,
    CMD_DATA_EVENT,
    INVALID
} LipSyncDummyInputMIOCmdType;


/**
 * Class containing information for a command or data event
 */
class LipSyncDummyInputMIOCmd
{
    public:
        LipSyncDummyInputMIOCmd()
        {
            iId = 0;
            iType = INVALID;
            iContext = NULL;
            iData1 = NULL;
        }

        LipSyncDummyInputMIOCmd(const LipSyncDummyInputMIOCmd& aCmd)
        {
            Copy(aCmd);
        }

        ~LipSyncDummyInputMIOCmd() {}

        LipSyncDummyInputMIOCmd& operator=(const LipSyncDummyInputMIOCmd& aCmd)
        {
            Copy(aCmd);
            return (*this);
        }

        PVMFCommandId iId; /** ID assigned to this command */
        int32 iType;  /** LipSyncDummyInputMIOCmdType value */
        OsclAny* iContext;  /** Other data associated with this command */
        OsclAny* iData1;  /** Other data associated with this command */


    private:

        void Copy(const LipSyncDummyInputMIOCmd& aCmd)
        {
            iId = aCmd.iId;
            iType = aCmd.iType;
            iContext = aCmd.iContext;
            iData1 = aCmd.iData1;
        }
};

//This class implements the Dummy input MIO component
class LipSyncDummyInputMIO : public OsclTimerObject
        , public PvmiMIOControl
        , public PvmiMediaTransfer
        , public PvmiCapabilityAndConfig
{
    public:

        LipSyncDummyInputMIO(const LipSyncDummyMIOSettings& aSettings);
        ~LipSyncDummyInputMIO();

        // APIs from PvmiMIOControl

        PVMFStatus connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver);

        PVMFStatus disconnect(PvmiMIOSession aSession);

        PVMFCommandId QueryUUID(const PvmfMimeString& aMimeType, Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids,
                                bool aExactUuidsOnly = false, const OsclAny* aContext = NULL);

        PVMFCommandId QueryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr, const OsclAny* aContext = NULL);

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

        PVMFCommandId writeAsync(uint8 format_type, int32 format_index,
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

        PVMFStatus releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);

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


    private:

        //from OsclActiveObject
        void Run();

        PVMFCommandId AddCmdToQueue(LipSyncDummyInputMIOCmdType aType, const OsclAny* aContext, OsclAny* aData1 = NULL);
        void AddDataEventToQueue(uint32 aMicroSecondsToEvent);
        void DoRequestCompleted(const LipSyncDummyInputMIOCmd& aCmd, PVMFStatus aStatus, OsclAny* aEventData = NULL);
        PVMFStatus DoInit();
        PVMFStatus DoStart();
        PVMFStatus DoReset();
        PVMFStatus DoPause();
        PVMFStatus DoFlush();
        PVMFStatus DoStop();
        void AddMarkerInfo(uint8* aData);
        void CalculateRMSInfo(uint32 VideoData, uint32 AudioData);
        void GenerateAudioFrame(uint8* aData);
        void GenerateVideoFrame(uint8* aData);
        void ProcessAudioFrame(uint32 aCurrentTime, uint32 aInterval);
        void ProcessVideoFrame(uint32 aCurrentTime, uint32 aInterval);
        void ProcessVideoFrame();
        uint32 WriteAsyncCall(int32 &error, uint8* data, uint32 &BytesToRead, PvmiMediaXferHeader& data_hdr, uint32 &writeAsyncID);
        PvmiMediaTransfer* iPeer;

        PvmiMIOObserver* iObserver;

        PVLogger* iLogger;

        /**
         * Allocate a specified number of key-value pairs and set the keys
         *
         * @param aKvp Output parameter to hold the allocated key-value pairs
         * @param aKey Key for the allocated key-value pairs
         * @param aNumParams Number of key-value pairs to be allocated
         * @return Completion status
         */
        PVMFStatus AllocateKvp(PvmiKvp*& aKvp, PvmiKeyType aKey, int32 aNumParams);

        /**
         * Verify one key-value pair parameter against capability of the port and
         * if the aSetParam flag is set, set the value of the parameter corresponding to
         * the key.
         *
         * @param aKvp Key-value pair parameter to be verified
         * @param aSetParam If true, set the value of parameter corresponding to the key.
         * @return PVMFSuccess if parameter is supported, else PVMFFailure
         */
        PVMFStatus VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam = false);

        PVMFStatus DoRead();
        class PvmiDummyMediaData
        {
            public:
                PvmiDummyMediaData()
                {
                    iId = 0;
                    iData = NULL;
                    iNotification = false;
                }

                PvmiDummyMediaData(const PvmiDummyMediaData& aData)
                {
                    iId = aData.iId;
                    iData = aData.iData;
                    iNotification = aData.iNotification;
                }

                PVMFCommandId iId;
                OsclAny* iData;
                bool iNotification;
        };

        // Command queue
        uint32 iCmdIdCounter;
        Oscl_Vector<LipSyncDummyInputMIOCmd, OsclMemAllocator> iCmdQueue;

        // State machine
        enum LipSyncDummyInputMIOState
        {
            STATE_IDLE,
            STATE_INITIALIZED,
            STATE_STARTED,
            STATE_FLUSHING,
            STATE_PAUSED,
            STATE_STOPPED
        };
        // PvmiMIO sessions
        Oscl_Vector<PvmiMIOObserver*, OsclMemAllocator> iObservers;

        Oscl_Vector<PvmiDummyMediaData, OsclMemAllocator> iSentMediaData;
        OsclMemAllocator iAlloc;
        OsclMemPoolFixedChunkAllocator* iMediaBufferMemPool;
        bool iAudioMIO;
        bool iVideoMIO;
        LipSyncDummyInputMIOState iState;
        bool iThreadLoggedOn;
        uint32 iSeqNumCounter;
        uint32 iTimestamp;
        PVMFFormatType iFormat;
        bool iCompressed;
        int32 iMicroSecondsPerDataEvent;
        LipSyncDummyMIOSettings iSettings;
        uint32 iVideoMicroSecondsPerDataEvent;
        uint32 iAudioMicroSecondsPerDataEvent;
        PVMFTimestamp iPrevTS;
        PVMFTimestamp iCurrentTS;
        bool iAudioOnlyOnce;
        bool iVideoOnlyOnce;
        PVMFTimebase_Tickcount timebase;
        ShareParams* iParams;
        PVMFMediaClock iClock;
};
#endif
