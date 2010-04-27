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
#ifndef PV_FRAME_METADATA_UTILITY_H_INCLUDED
#define PV_FRAME_METADATA_UTILITY_H_INCLUDED


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

#ifndef PV_PLAYER_DATASOURCE_H_INCLUDED
#include "pv_player_datasource.h"
#endif

#ifndef PVMF_LOCAL_DATA_SOURCE_H_INCLUDED
#include "pvmf_local_data_source.h"
#endif

#ifndef PV_FRAME_METADATA_INTERFACE_H_INCLUDED
#include "pv_frame_metadata_interface.h"
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

#ifndef PV_PLAYER_DATASINKPVMFNODE_H_INCLUDED
#include "pv_player_datasinkpvmfnode.h"
#endif

#ifndef PV_FRAME_METADATA_MIO_VIDEO_H_INCLUDED
#include "pv_frame_metadata_mio_video.h"
#endif

#ifndef PV_FRAME_METADATA_MIO_AUDIO_H_INCLUDED
#include "pv_frame_metadata_mio_audio.h"
#endif

#ifndef OSCL_TIMER_H_INCLUDED
#include "oscl_timer.h"
#endif


/**
 * PVFMUtilityState enum
 *
 *  Enumeration of internal pvFrameAndMetadata utility state.
 *
 **/
typedef enum
{
    PVFM_UTILITY_STATE_IDLE = 1,
    PVFM_UTILITY_STATE_INITIALIZING,
    PVFM_UTILITY_STATE_INITIALIZED,
    PVFM_UTILITY_STATE_RESETTING,
    PVFM_UTILITY_STATE_HANDLINGERROR,
    PVFM_UTILITY_STATE_ERROR
} PVFMUtilityState;



typedef union PVFMUtilityCommandParamUnion
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
} _PVFMUtilityCommandParamUnion;


/**
 * PVFMUtilityCommand Class
 *
 * PVFMUtilityCommand class is a data class to hold issued commands inside pvFrameAndMetadata utility
 **/
class PVFMUtilityCommand
{
    public:
        /**
         * The constructor for PVFMUtilityCommand which allows the data values to be set.
         *
         * @param aCmdType The command type value for this command. The value is an utility-specific 32-bit value.
         * @param aCmdId The command ID assigned by the utiliy for this command.
         * @param aContextData The pointer to the passed-in context data for this command.
         *
         * @returns None
         **/
        PVFMUtilityCommand(int32 aCmdType, PVCommandId aCmdId, OsclAny* aContextData = NULL,
                           Oscl_Vector<PVFMUtilityCommandParamUnion, OsclMemAllocator>* aParamVector = NULL, bool aAPICommand = true) :
                iCmdType(aCmdType), iCmdId(aCmdId), iContextData(aContextData), iAPICommand(aAPICommand)
        {
            iParamVector.clear();
            if (aParamVector)
            {
                iParamVector = *aParamVector;
            }
        }

        /**
         * The copy constructor for PVFMUtilityCommand. Used mainly for Oscl_Vector.
         *
         * @param aCmd The reference to the source PVFMUtilityCommand to copy the data values from.
         *
         * @returns None
         **/
        PVFMUtilityCommand(const PVFMUtilityCommand& aCmd)
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
        PVFMUtilityCommandParamUnion GetParam(uint32 aIndex)const
        {
            if (aIndex >= iParamVector.size())
            {
                PVFMUtilityCommandParamUnion param;
                oscl_memset(&param, 0, sizeof(PVFMUtilityCommandParamUnion));
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
        bool operator==(const PVFMUtilityCommand& aCmd) const
        {
            return iCmdId == aCmd.iCmdId;
        }

        int32 iCmdType;
        PVCommandId iCmdId;
        OsclAny* iContextData;
        bool iAPICommand;
        Oscl_Vector<PVFMUtilityCommandParamUnion, OsclMemAllocator> iParamVector;
        OSCL_HeapString<OsclMemAllocator> iMimeType;
        PVUuid iUuid;
};

/**
 * PVFMUtilityCommandType enum
 *
 *  Enumeration of types of commands that can be issued to pvFrameAndMetadata utility.
 *
 **/
typedef enum
{
    // Engine API commands
    PVFM_UTILITY_COMMAND_QUERY_INTERFACE = 1,
    PVFM_UTILITY_COMMAND_CANCEL_ALL_COMMANDS,
    PVFM_UTILITY_COMMAND_ADD_DATA_SOURCE,
    PVFM_UTILITY_COMMAND_GET_METADATA_VALUES,
    PVFM_UTILITY_COMMAND_SET_PARAMETERS,
    PVFM_UTILITY_COMMAND_GET_FRAME_USER_BUFFER,
    PVFM_UTILITY_COMMAND_GET_FRAME_UTILITY_BUFFER,
    PVFM_UTILITY_COMMAND_RETURN_BUFFER,
    PVFM_UTILITY_COMMAND_REMOVE_DATA_SOURCE,
    PVFM_UTILITY_COMMAND_HANDLE_PLAYER_ERROR
} PVFMUtilityCommandType;


/**
 * PVFMUtilityCommandCompareLess Class
 *
 * PVFMUtilityCommandCompareLess class is a utility class to allow the OSCL priority queue perform command priority comparison.
 * The class is meant to be used inside pvFrameAndMetadata utility and not exposed to the interface layer or above.
 **/
class PVFMUtilityCommandCompareLess
{
    public:
        /**
        * The algorithm used in OsclPriorityQueue needs a compare function
        * that returns true when A's priority is less than B's
        * @return true if A's priority is less than B's, else false
        */
        int compare(PVFMUtilityCommand& a, PVFMUtilityCommand& b) const
        {
            int a_pri = PVFMUtilityCommandCompareLess::GetPriority(a);
            int b_pri = PVFMUtilityCommandCompareLess::GetPriority(b);
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
        static int GetPriority(PVFMUtilityCommand& aCmd)
        {
            switch (aCmd.GetCmdType())
            {
                case PVFM_UTILITY_COMMAND_QUERY_INTERFACE:
                    return 5;
                case PVFM_UTILITY_COMMAND_CANCEL_ALL_COMMANDS:
                    return 8;
                case PVFM_UTILITY_COMMAND_ADD_DATA_SOURCE:
                    return 5;
                case PVFM_UTILITY_COMMAND_GET_METADATA_VALUES:
                    return 5;
                case PVFM_UTILITY_COMMAND_SET_PARAMETERS:
                    return 5;
                case PVFM_UTILITY_COMMAND_GET_FRAME_USER_BUFFER:
                    return 5;
                case PVFM_UTILITY_COMMAND_GET_FRAME_UTILITY_BUFFER:
                    return 5;
                case PVFM_UTILITY_COMMAND_RETURN_BUFFER:
                    return 5;
                case PVFM_UTILITY_COMMAND_REMOVE_DATA_SOURCE:
                    return 5;
                case PVFM_UTILITY_COMMAND_HANDLE_PLAYER_ERROR:
                    return 9;
                default:
                    return 0;
            }
        }
};

// Internal structure to use for opaque data context
struct PVFMUtilityContext
{
    PVCommandId iCmdId;
    OsclAny* iCmdContext;
    int32 iCmdType;
};


// Forward declaration
class PVPlayerInterface;
class PVMFNodeInterface;
class PVFMVideoMIO;
class PVFMAudioMIO;
class OsclMemPoolResizableAllocator;

class PVFrameAndMetadataUtility
        : public OsclTimerObject
        , public PVFrameAndMetadataInterface
        , public PvmiCapabilityAndConfigBase
        , public PVCommandStatusObserver
        , public PVErrorEventObserver
        , public PVInformationalEventObserver
        , public PVFMVideoMIOGetFrameObserver
        , public OsclTimerObserver
{
    public:
        static PVFrameAndMetadataUtility* New(char *aOutputFormatMIMEType,
                                              PVCommandStatusObserver *aCmdObserver,
                                              PVErrorEventObserver *aErrorObserver,
                                              PVInformationalEventObserver *aInfoObserver);
        ~PVFrameAndMetadataUtility();

        // From PVFrameAndMetadataInterface
        PVCommandId QueryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr, const OsclAny* aContextData = NULL);
        PVCommandId CancelAllCommands(const OsclAny* aContextData = NULL);
        PVMFStatus GetStateSync(PVFrameAndMetadataState& aState);
        PVCommandId AddDataSource(PVPlayerDataSource& aDataSource, const OsclAny* aContextData = NULL);
        PVCommandId GetMetadataValues(PVPMetadataList& aKeyList, int32 aStartingValueIndex, int32 aMaxValueEntries, int32& aNumAvailableValueEntries, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, const OsclAny* aContextData = NULL);
        PVCommandId GetFrame(PVFrameSelector& aFrameInfo, uint8* aProvidedFrameBuffer, uint32& aBufferSize, PVFrameBufferProperty& aBufferProp, const OsclAny* aContextData = NULL);
        PVCommandId GetFrame(PVFrameSelector& aFrameInfo, uint8** aFrameBufferPtr, uint32& aBufferSize, PVFrameBufferProperty& aBufferProp, const OsclAny* aContextData = NULL);
        PVCommandId ReturnBuffer(uint8* aFrameBufferPtr, const OsclAny* aContextData = NULL);
        PVCommandId RemoveDataSource(PVPlayerDataSource& aDataSource, const OsclAny* aContextData = NULL);
        PVMFStatus  SetMode(uint32 aMode);

        void        SetThumbnailDimensions(uint32 aWidth, uint32 aHeight);
        void        GetThumbnailDimensions(uint32 &aWidth, uint32 &aHeight);
        // From PvmiCapabilityAndConfig
        void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int aNumElements, PvmiKvp* &aRetKVP);

    private:
        void CleanupSourceAndSinks();
        PVFrameAndMetadataUtility();
        void Construct(char *aOutputFormatMIMEType,
                       PVCommandStatusObserver *aCmdObserver,
                       PVErrorEventObserver *aErrorObserver,
                       PVInformationalEventObserver *aInfoObserver);

        // From OsclTimerObject
        void Run();

        // From PVCommandStatusObserver
        void CommandCompleted(const PVCmdResponse& aResponse);

        // From PVErrorEventObserver
        void HandleErrorEvent(const PVAsyncErrorEvent& aEvent);

        // From PVInformationalEventObserver
        void HandleInformationalEvent(const PVAsyncInformationalEvent& aEvent);

        // From PVFMVideoMIOGetFrameObserver
        void HandleFrameReadyEvent(PVMFStatus aEventStatus);

        // From OsclTimerObserver
        void TimeoutOccurred(int32 timerID, int32 timeoutInfo);

        // Command and event queueing related functions
        PVCommandId AddCommandToQueue(int32 aCmdType, OsclAny* aContextData = NULL,
                                      Oscl_Vector<PVFMUtilityCommandParamUnion, OsclMemAllocator>* aParamVector = NULL,
                                      const PVUuid* aUuid = NULL, bool aAPICommand = true);

        // Command and event callback functions
        void UtilityCommandCompleted(PVCommandId aId, OsclAny* aContext, PVMFStatus aStatus, PVInterface* aExtInterface = NULL,
                                     OsclAny* aEventData = NULL, int32 aEventDataSize = 0);

        // Functions for utility state handling
        void SetUtilityState(PVFMUtilityState aState);
        PVFrameAndMetadataState GetUtilityState(void);

        // Keeps track of next available command ID
        PVCommandId iCommandId;

        // Current utility state
        PVFMUtilityState iState;

        // caller's intent
        uint32 iMode;

        // Queue for utility commands
        Oscl_Vector<PVFMUtilityCommand, OsclMemAllocator> iCurrentCmd; // Vector of size 1 to hold the command being currently processed
        Oscl_Vector<PVFMUtilityCommand, OsclMemAllocator> iCmdToCancel; // Vector of size 1 to hold the command being currently cancelled
        OsclPriorityQueue<PVFMUtilityCommand, OsclMemAllocator, Oscl_Vector<PVFMUtilityCommand, OsclMemAllocator>, PVFMUtilityCommandCompareLess> iPendingCmds; // Vector to hold the command that has been requested

        // Reference to observers
        PVCommandStatusObserver *iCmdStatusObserver;
        PVErrorEventObserver *iErrorEventObserver;
        PVInformationalEventObserver *iInfoEventObserver;

        // Command handling functions
        void DoCancelAllCommands(PVFMUtilityCommand& aCmd);
        void DoCancelCommandBeingProcessed(void);

        PVMFStatus DoQueryInterface(PVFMUtilityCommand& aCmd);
        PVMFStatus DoPlayerQueryInterface(PVCommandId aCmdId, OsclAny* aCmdContext, PVUuid& aUuid, PVInterface*& aInterfacePtr);
        bool queryInterface(const PVUuid& uuid, PVInterface*& iface, PVCommandId cmdid, OsclAny* context);

        PVMFStatus DoAddDataSource(PVFMUtilityCommand& aCmd);
        PVMFStatus DoGetMetadataValues(PVFMUtilityCommand& aCmd);
        PVMFStatus DoCapConfigSetParameters(PVFMUtilityCommand& aCmd);
        PVMFStatus DoVerifyAndSetFMUParameter(PvmiKvp& aParameter, bool aSetParam);
        PVMFStatus DoPlayerSetParametersSync(PVCommandId aCmdId, OsclAny* aCmdContext, PvmiKvp* aParameters, int aNumElements, PvmiKvp* &aRetKVP);
        bool HasVideo();
        PVMFStatus DoGetFrame(PVFMUtilityCommand& aCmd);
        PVMFStatus DoGFPlayerStart(PVCommandId aCmdId, OsclAny* aCmdContext, bool aResume);
        PVMFStatus DoGFPlayerPause(PVCommandId aCmdId, OsclAny* aCmdContext);
        PVMFStatus DoReturnBuffer(PVFMUtilityCommand& aCmd);
        PVMFStatus DoRemoveDataSource(PVFMUtilityCommand& aCmd);

        PVMFStatus DoPlayerShutdownRestart(void);

        enum
        {
            // Player commands
            PVFM_CMD_PlayerQueryInterface,
            PVFM_CMD_AddDataSource,
            PVFM_CMD_GetMetadataValues,
            PVFM_CMD_PlayerSetParametersSync,
            PVFM_CMD_GFPlayerSetPlaybackRange,
            PVFM_CMD_GFPlayerStart,
            PVFM_CMD_GFPlayerPause,
            PVFM_CMD_RemoveDataSource,
            PVFM_CMD_PlayerQueryCapConfigInterface
        };

        // Player command completion handling
        void HandleCommandComplete(PVFMUtilityContext& aUtilContext, const PVCmdResponse& aCmdResp);
        void HandleDataSourceCommand(PVFMUtilityContext& aUtilContext, const PVCmdResponse& aCmdResp);
        void HandleCommandComplete2(PVFMUtilityContext& aUtilContext, const PVCmdResponse& aCmdResp);
        void HandleGFPlayerStart(PVFMUtilityContext& aUtilContext, const PVCmdResponse& aCmdResp);
        void HandleGFPlayerPause(PVFMUtilityContext& aUtilContext, const PVCmdResponse& aCmdResp);

        // Player engine instance handle
        PVPlayerInterface* iPlayer;

        PvmiCapabilityAndConfig* iPlayerCapConfigIF;
        PVInterface* iPlayerCapConfigIFPVI;

        // Output Format Type
        PVMFFormatType iOutputFormatType;

        // User specified player data source handle
        PVPlayerDataSource* iDataSource;
        uint32 iSourceIntent;
        PVMFLocalDataSource* iLocalDataSource;

        // Video data sink
        PVPlayerDataSinkPVMFNode iVideoDataSink;
        PVMFNodeInterface* iVideoNode;
        PVFMVideoMIO* iVideoMIO;

        // Audio data sink
        PVPlayerDataSinkPVMFNode iAudioDataSink;
        PVMFNodeInterface* iAudioNode;
        PVFMAudioMIO* iAudioMIO;

        // Handle to the logger node
        PVLogger* iLogger;
        PVLogger* iPerfLogger;

        // Context objects
        PVFMUtilityContext iUtilityContext;
        int32 iCancelContext;
        bool iErrorHandlingInUtilityAO;

        // UUID for calling QueryInterface() on player
        PVUuid iPlayerQueryIFUUID;

        // Resizable memory pool for utility allocated frame buffers
        OsclMemPoolResizableAllocator* iVideoFrameBufferMemPool;
        uint8* iCurrentVideoFrameBuffer;
        uint32* iVideoFrameBufferSize;

        // Pointer to video frame selection info passed-in by user
        PVFrameSelector* iVideoFrameSelector;

        // Pointer to video frame buffer property structure passed-in by user
        PVFrameBufferProperty* iVideoFrameBufferProp;

        // Flags for parallel async commands
        bool iFrameReceived;
        bool iPlayerStartCompleted;

        // Hold command status info when error occurs
        PVMFStatus iAPICmdStatus;

        // OsclTimer for timeouts
        OsclTimer<OsclMemAllocator>* iTimeoutTimer;
        uint32 iErrorHandlingWaitTime;
        uint32 iFrameReadyWaitTime;

        uint32 iThumbnailWidth;
        uint32 iThumbnailHeight;

        int32 iNumPendingPlayerCommands;
};

#endif // PV_FRAME_METADATA_UTILITY_H_INCLUDED





