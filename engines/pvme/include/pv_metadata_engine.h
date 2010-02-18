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
#ifndef PV_METADATA_ENGINE_H_INCLUDED
#define PV_METADATA_ENGINE_H_INCLUDED


#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif

#ifndef OSCL_VECTOR_H_INCLUDED
#include "oscl_vector.h"
#endif

#ifndef OSCL_PRIQUEUE_H_INCLUDED
#include "oscl_priqueue.h"
#endif

#ifndef PV_ENGINE_TYPES_H_INCLUDED
#include "pv_engine_types.h"
#endif


#ifndef PVMF_METADATA_ENGINE_INTERFACE_H_INCLUDED
#include "pv_metadata_engine_interface.h"
#endif

#ifndef PVMF_METADATA_ENGINE_TYPES_H_INCLUDED
#include "pv_metadata_engine_types.h"
#endif

#ifndef PV_ENGINE_OBSERVER_H_INCLUDED
#include "pv_engine_observer.h"
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

#ifndef OSCL_TIMER_H_INCLUDED
#include "oscl_timer.h"
#endif

#ifndef PVMF_METADATA_ENGINE_FACTORY_H_INCLUDED
#include "pv_metadata_engine_factory.h"
#endif

#ifndef PVME_NODE_REGISTRY_H_INCLUDED
#include "pvme_node_registry.h"
#endif

#ifndef OSCL_SCHEDULER_H_INCLUDED
#include "oscl_scheduler.h"
#endif

#ifndef OSCL_MIME_STRING_UTILS_H
#include "pv_mime_string_utils.h"
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

#ifndef PVMF_META_DATA_EXTENSION_H_INCLUDED
#include "pvmf_meta_data_extension.h"
#endif


#ifndef THREADSAFE_QUEUE_H_INCLUDED
#include "threadsafe_queue.h"
#endif

#ifndef PVMF_METADATAINFOMESSAGE_EXTENSION_H_INCLUDED
#include "pvmf_metadatainfomessage_extension.h"
#endif

#ifndef PVMF_ERRORINFOMESSAGE_EXTENSION_H_INCLUDED
#include "pvmf_errorinfomessage_extension.h"
#endif

#ifndef PVLOGGER_STDERR_APPENDER_H_INCLUDED
#include "pvlogger_stderr_appender.h"
#endif

#ifndef PVLOGGER_MEM_APPENDER_H_INCLUDED
#include "pvlogger_mem_appender.h"
#endif

#ifndef PVLOGGER_FILE_APPENDER_H_INCLUDED
#include "pvlogger_file_appender.h"
#endif

#ifndef PVLOGGER_TIME_AND_ID_LAYOUT_H_INCLUDED
#include "pvlogger_time_and_id_layout.h"
#endif

class PVMetadataEngineInterfaceContainer;


/**
 * PVMEState enum
 *
 *  Enumeration of internal pvMetadataEngine state.
 *
 **/
typedef enum
{
    PVME_INTERNAL_STATE_IDLE = 1,
    PVME_INTERNAL_STATE_INITIALIZING,
    PVME_INTERNAL_STATE_INITIALIZED,
    PVME_INTERNAL_STATE_RESETTING,
    PVME_INTERNAL_STATE_HANDLINGERROR,
    PVME_INTERNAL_STATE_ERROR
} PVMEState;

typedef union PVMECommandParamUnion
{
    bool  bool_value;
    float float_value;
    double double_value;
    uint8 uint8_value;
    int32 int32_value;
    uint32 uint32_value;
    oscl_wchar* pWChar_value;
    char* pChar_value;
    uint8* pUint8_value;
    int32* pInt32_value;
    uint32* pUint32_value;
    int64* pInt64_value;
    uint64*  pUint64_value;
    OsclAny* pOsclAny_value;
} _PVMECommandParamUnion;


/**
 * PVMECommand Class
 *
 * PVMECommand class is a data class to hold issued commands inside pvMetadataEngine
 **/
class PVMECommand
{
    public:
        /**
         * The constructor for PVMECommand which allows the data values to be set.
         *
         * @param aCmdType The command type value for this command. The value is an utility-specific 32-bit value.
         * @param aCmdId The command ID assigned by the utiliy for this command.
         * @param aContextData The pointer to the passed-in context data for this command.
         *
         * @returns None
         **/
        PVMECommand(int32 aCmdType, PVCommandId aCmdId, OsclAny* aContextData = NULL,
                    Oscl_Vector<PVMECommandParamUnion, OsclMemAllocator>* aParamVector = NULL, bool aAPICommand = true) :
                iCmdType(aCmdType), iCmdId(aCmdId), iContextData(aContextData), iAPICommand(aAPICommand)
        {
            iParamVector.clear();
            if (aParamVector)
            {
                iParamVector = *aParamVector;
            }
        }

        /**
         * The copy constructor for PVMECommand. Used mainly for Oscl_Vector.
         *
         * @param aCmd The reference to the source PVMECommand to copy the data values from.
         *
         * @returns None
         **/
        PVMECommand(const PVMECommand& aCmd)
        {
            iCmdType = aCmd.iCmdType;
            iCmdId = aCmd.iCmdId;
            iContextData = aCmd.iContextData;
            iAPICommand = aCmd.iAPICommand;
            iParamVector = aCmd.iParamVector;
            iMimeType = aCmd.iMimeType;
            iUuid = aCmd.iUuid;
        }

        /**
         * This function returns the stored command type value.
         *
         * @returns The signed 32-bit command type value for this command.
         **/
        int32 GetCmdType()const
        {
            return iCmdType;
        }

        /**
         * This function returns the stored command ID value.
         *
         * @returns The PVCommandId value for this command.
         **/
        PVCommandId GetCmdId()const
        {
            return iCmdId;
        }

        /**
         * This function returns the stored context data pointer.
         *
         * @returns The pointer to the context data for this command
         **/
        OsclAny* GetContext()const
        {
            return iContextData;
        }

        /**
         * This function tells whether the command is an API command or not
         *
         * @returns true if API command, false if not.
         **/
        bool IsAPICommand()const
        {
            return iAPICommand;
        }

        /**
         * This function returns the command parameter from the specified index.
         * If the specified index is not available, empty parameter will be returned
         *
         * @param aIndex The index of the parameter to return
         *
         * @returns The stored parameter for this command
         **/
        PVMECommandParamUnion GetParam(uint32 aIndex)const
        {
            if (aIndex >= iParamVector.size())
            {
                PVMECommandParamUnion param;
                oscl_memset(&param, 0, sizeof(PVMECommandParamUnion));
                return param;
            }
            else
            {
                return iParamVector[aIndex];
            }
        }

        /**
         * This function returns MIME type parameter for this command
         *
         * @returns The MIME type parameter for this command
         */
        const PvmfMimeString& GetMimeType()const
        {
            return iMimeType;
        }

        /**
         * This function returns UUID parameter for this command
         *
         * @returns The UUID parameter for this command
         */
        PVUuid GetUuid()const
        {
            return iUuid;
        }

        /**
         * This function stores MIME type parameter of this command
         */
        void SetMimeType(const PvmfMimeString& aMimeType)
        {
            iMimeType = aMimeType;
        }

        /**
         * This function stores the UUID parameter of this command
         */
        void SetUuid(const PVUuid& aUuid)
        {
            iUuid = aUuid;
        }

        /**
         * Equality comparison for use with OsclPriorityQueue
         */
        bool operator==(const PVMECommand& aCmd) const
        {
            return iCmdId == aCmd.iCmdId;
        }

        int32 iCmdType;
        PVCommandId iCmdId;
        OsclAny* iContextData;
        bool iAPICommand;
        Oscl_Vector<PVMECommandParamUnion, OsclMemAllocator> iParamVector;
        OSCL_HeapString<OsclMemAllocator> iMimeType;
        PVUuid iUuid;
};

/**
 * PVMECommandType enum
 *
 *  Enumeration of types of commands that can be issued to pvMetadataEngine.
 *
 **/
typedef enum
{
    // API commands
    PVME_COMMAND_INIT = 1,
    PVME_COMMAND_GET_METADATA,
    PVME_COMMAND_RESET,
    PVME_COMMAND_CLEANUP,
    PVME_COMMAND_SET_METADATAKEYS_OOTSYNC,
    PVME_COMMAND_GET_PVME_STATE_OOTSYNC,
    PVME_COMMAND_ERROR_HANDLING
} PVMECommandType;

/**
 * PVMECommandCompareLess Class
 *
 * PVMECommandCompareLess class is a utility class to allow the OSCL priority queue perform command priority comparison.
 * The class is meant to be used inside pvMetadataEngine and not exposed to the interface layer or above.
 **/
class PVMECommandCompareLess
{
    public:
        /**
        * The algorithm used in OsclPriorityQueue needs a compare function
        * that returns true when A's priority is less than B's
        * @return true if A's priority is less than B's, else false
        */
        int compare(PVMECommand& a, PVMECommand& b) const
        {
            int a_pri = PVMECommandCompareLess::GetPriority(a);
            int b_pri = PVMECommandCompareLess::GetPriority(b);
            if (a_pri < b_pri)
            {
                // Higher priority
                return true;
            }
            else if (a_pri == b_pri)
            {
                // Same priority so look at the command ID to maintain FIFO
                return (a.GetCmdId() > b.GetCmdId());
            }
            else
            {
                // Lower priority
                return false;
            }
        }

        /**
        * Returns the priority of each command
        * @return A 0-based priority number. A lower number indicates lower priority.
        */
        static int GetPriority(PVMECommand& aCmd)
        {
            switch (aCmd.GetCmdType())
            {
                case PVME_COMMAND_INIT:
                    return 5;
                case PVME_COMMAND_GET_METADATA:
                    return 5;
                case PVME_COMMAND_RESET:
                    return 5;
                case PVME_COMMAND_CLEANUP:
                    return 5;
                case PVME_COMMAND_ERROR_HANDLING:
                    return 1;
                default:
                    return 0;
            }
        }
};


// Internal structure to use for opaque data context
struct PVMEContext
{
    PVCommandId iCmdId;
    OsclAny* iCmdContext;
    int32 iCmdType;
};


class PVMetadataEngine : public OsclTimerObject,
        public PVMetadataEngineInterface,
        public PVMFNodeCmdStatusObserver,
        public PVMFNodeInfoEventObserver,
        public PVMFNodeErrorEventObserver,
        public PVMERecognizerRegistryObserver,
        public ThreadSafeQueueObserver

{
    public:
        static PVMetadataEngine* New(PVMetadataEngineInterfaceContainer& aPVMEContainer);

        ~PVMetadataEngine();

        PVMFStatus GetPVMEStateSync(PVMetadataEngineState& aState);
        PVCommandId Init(const OsclAny* aContextData = NULL);
        PVMFStatus SetMetaDataKeys(PVPMetadataList& aKeyList);
        PVCommandId GetMetadata(PVPlayerDataSource& aDataSource,
                                Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                const OsclAny* aContextData = NULL);
        PVCommandId Reset(const OsclAny* aContextData = NULL);
        void CleanUp();

        static void PVMEThread(PVMetadataEngineInterfaceContainer* aPVMEContainer);


    private:
        PVMetadataEngine();

        void Construct(PVMetadataEngineInterfaceContainer& aPVMEContainer);

        // From OsclTimerObject
        void Run() ;

        // From PVMFNodeCmdStatusObserver
        void NodeCommandCompleted(const PVMFCmdResp& aResponse);

        // From PVMFNodeInfoEventObserver
        void HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent);

        // From PVMFNodeErrorEventObserver
        void HandleNodeErrorEvent(const PVMFAsyncEvent& aEvent);

        // For checking of any pending error handling cmd in the queue
        bool CheckForPendingErrorHandlingCmd();

        // Node informational event handling functions
        void HandleSourceNodeInfoEvent(const PVMFAsyncEvent& aEvent);

        // From PVMERecognizerRegistryObserver
        void RecognizeCompleted(PVMFFormatType aSourceFormatType, OsclAny* aContext);


        // Command and event queueing related functions
        PVCommandId AddCommandToQueue(int32 aCmdType, OsclAny* aContextData = NULL,
                                      Oscl_Vector<PVMECommandParamUnion, OsclMemAllocator>* aParamVector = NULL,
                                      const PVUuid* aUuid = NULL, bool aAPICommand = true, PVCommandId* aId = NULL);

        // Command and event callback functions
        void PVMECommandCompleted(PVCommandId aId, OsclAny* aContext, PVMFStatus aStatus, PVInterface* aExtInterface = NULL,
                                  OsclAny* aEventData = NULL, int32 aEventDataSize = 0);
        void SendInformationalEvent(PVMFEventType aEventType, PVInterface* aExtInterface = NULL,
                                    OsclAny* aEventData = NULL, uint8* aLocalBuffer = NULL, uint32 aLocalBufferSize = 0);
        void SendErrorEvent(PVMFEventType aEventType, PVInterface* aExtInterface = NULL,
                            OsclAny* aEventData = NULL, uint8* aLocalBuffer = NULL, uint32 aLocalBufferSize = 0);

        PVMFStatus DoInit(PVMECommand& aCmd);
        PVMFStatus DoReset(PVMECommand& aCmd);
        PVMFStatus DoCleanUp();
        PVMFStatus DoSetMetadatakeys(PVMECommand& aCmd);
        PVMFStatus DoGetMetadata(PVMECommand& aCmd);
        PVMFStatus DoGetPVMEState(PVMECommand& aCmd);
        void ReleaseSourceNodeInterfaces(void);
        void ReleaseSourceNode(PVUuid&, PVMFSessionId&, PVMFNodeInterface*&);

        // Functions for state handling
        void SetPVMEState(PVMEState aState);
        PVMetadataEngineState GetPVMEState(void);

        // Keeps track of next available command ID
        PVCommandId iCommandId;

        // Current state
        PVMEState iState;

        // Node registry for PVME
        PVMENodeRegistry iPVMENodeRegistry;

        // Recognizer registry for PVME
        PVMERecognizerRegistry iPVMERecognizerRegistry;

        // Queue for PVME commands
        Oscl_Vector<PVMECommand, OsclMemAllocator> iCurrentCmd; // Vector of size 1 to hold the command being currently processed
        OsclPriorityQueue<PVMECommand, OsclMemAllocator, Oscl_Vector<PVMECommand, OsclMemAllocator>, PVMECommandCompareLess> iPendingCmds; // Vector to hold the command that has been requested

        // Variables for completing engine commands after error handling
        PVMFStatus iCommandCompleteStatusInErrorHandling;
        PVMFBasicErrorInfoMessage* iCommandCompleteErrMsgInErrorHandling;

        //Construct starts the scheduler and Cleanup stops it
        OsclExecScheduler* sched;

        // Reference to observers
        PVCommandStatusObserver *iCmdStatusObserver;
        PVErrorEventObserver *iErrorEventObserver;
        PVInformationalEventObserver *iInfoEventObserver;


        // Handle to the logger node
        PVLogger* iLogger;
        PVLogger* iPerfLogger;

        // Context objects
        PVMEContext iPVMEContext;

        // Data source
        PVPlayerDataSource* iDataSource;
        PVMFFormatType iSourceFormatType;
        PVMFNodeInterface* iSourceNode;
        PVUuid          iSourceNodeUuid;
        PVMFSessionId iSourceNodeSessionId;

        PVMFDataSourceInitializationExtensionInterface* iSourceNodeInitIF;
        PVMFMetadataExtensionInterface* iSourceNodeMetadataExtIF;
        PVInterface* iSourceNodePVInterfaceInit;
        PVInterface* iSourceNodePVInterfaceMetadataExt;

        //Old Source Node Values
        PVMFNodeInterface* iPreviousSourceNode;
        PVUuid          iPreviousSourceNodeUuid;
        PVMFSessionId iPreviousSourceNodeSessionId;

        // Thread-safety mechanisms
        ThreadSafeQueue iThreadSafeQueue;
        void ThreadSafeQueueDataAvailable(ThreadSafeQueue*);
        OsclSemaphore iOOTSyncCommandSem;
        OsclMutex iCommandIdMut;
        PVMFStatus DoOOTSyncCommand(int32 aCmdType,
                                    Oscl_Vector<PVMECommandParamUnion, OsclMemAllocator>* aParamVector,
                                    const PVUuid* aUuid = NULL);
        void OOTSyncCommandComplete(PVMECommand& aCmd, PVMFStatus aStatus);


        //To hold the keylist passed by the app
        PVPMetadataList iRequestedMetadataKeyList;
        Oscl_Vector<PvmiKvp, OsclMemAllocator>* iValueList;

        PVMetadataEngineInterfaceContainer* aContainer;


        //Node Commands and node command handling functions
        enum
        {
            // Node commands
            PVME_CMD_SourceNodeInit,
            PVME_CMD_GetNodeMetadataValue,
            PVME_CMD_SourceNodeReset,
            // Recognizer command
            PVME_CMD_QUERYSOURCEFORMATTYPE,

        };

        // Command handling functions
        PVMFStatus DoSourceNodeInit(PVCommandId aCmdId, OsclAny* aCmdContext);
        PVMFStatus DoErrorHandling(void);

        //Node Command Handling Functions
        void HandleSourceNodeInit(PVMEContext& aNodeContext, const PVMFCmdResp& aNodeResp);
        void HandleSourceNodeReset(PVMEContext& aNodeContext, const PVMFCmdResp& aNodeResp);
        // Node error event handling functions
        void HandleSourceNodeErrorEvent(const PVMFAsyncEvent& aEvent);

        //Command Handling functions
        PVMFStatus DoQuerySourceFormatType(PVCommandId aCmdId, OsclAny* aCmdContext);
        PVMFStatus DoSetupSourceNode(PVCommandId aCmdId, OsclAny* aCmdContext);
        PVMFStatus DoSetSourceInitializationData();
        PVMFStatus DoGetMetadataValues();

};



#endif
