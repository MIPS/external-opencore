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
#ifndef PVMF_METADATA_ENGINE_INTERFACE_H_INCLUDED
#define PVMF_METADATA_ENGINE_INTERFACE_H_INCLUDED

#ifndef PVMF_METADATA_ENGINE_TYPES_H_INCLUDED
#include "pv_metadata_engine_types.h"
#endif

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif

#ifndef OSCL_STRING_H_INCLUDED
#include "oscl_string.h"
#endif

#ifndef OSCL_VECTOR_H_INCLUDED
#include "oscl_vector.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#ifndef PVMF_RETURN_CODES_H_INCLUDED
#include "pvmf_return_codes.h"
#endif

#ifndef PV_ENGINE_TYPES_H_INCLUDED
#include "pv_engine_types.h"
#endif

#ifndef PV_PLAYER_TYPES_H_INCLUDED
#include "pv_player_types.h"
#endif

#ifndef PVMI_KVP_H_INCLUDED
#include "pvmi_kvp.h"
#endif

#ifndef PV_PLAYER_DATASOURCE_H_INCLUDED
#include "pv_player_datasource.h"
#endif

/**
 * PVMetadataEngineInterface is the interface to the PVMetadataEngine, which
 * allows control of the metadata engine.
 * The PVMetadataEngineFactory class is to be used to create and
 * delete instances of this object
 **/
class PVMetadataEngineInterface
{
    public:
        virtual ~PVMetadataEngineInterface() {};
        /**
         * This function returns the current state of PVMetadataEngine as a synchronous command.
         * Application may use this info to determine if the
         * PVMetadataEngine is ready for the next request.
         *
         * @param aState
         *         A reference to a PVMetadataEngineState. Upon successful completion of this command,
         *         it will contain the current state of PVMetadataEngine.
         * @returns Status indicating whether the command succeeded or not.
         **/
        virtual PVMFStatus GetPVMEStateSync(PVMetadataEngineState& aState) = 0;

        /**
         * This function switches PVMetadataEngine from PVME_STATE_IDLE state
         * to the PVME_STATE_INITIALIZED state.
         * If initialization fails, PVME will revert to PVME_STATE_IDLE state.
         * The Command should only be called in PVME_STATE_IDLE.
         * This command request is asynchronous. PVCommandStatusObserver's
         * CommandCompleted() callback handler will be called when this command
         * request completes.
         *
         * @param aContextData
         *         Optional opaque data that will be passed back to the user with the command response
         * @leave This method can leave with one of the following error codes
         *         OsclErrInvalidState if invoked in the incorrect state
         *         OsclErrNoMemory if the SDK failed to allocate memory during this operation
         * @returns A unique command id for asynchronous completion
         **/
        virtual PVCommandId Init(const OsclAny* aContextData = NULL) = 0;

        /**
         * Typically a lot of the apps that use PVME might be interested in a fixed set of
         * metadata. This function lets the apps provide that list to PVME. This function can
         * be called in PVME_STATE_IDLE or PVME_STATE_INITIALIZED state. Any subsequent call
         * to GetMetadata would only return metadata for these keys (if available).
         * For a complete list of supported keys, please refer to "pvplayer_developers_guide.doc".
         * Any call to SetMetaDataKeys will overwrite the keys provided in any previous call
         * to SetMetaDataKeys.
         * This API is synchronous.
         *
         * @param aKeyList
         *         Reference to a list of metadata keys for which metadata values are requested.
         * @returns PVMFSuccess if the list of keys was accepted,
         *          PVMFErrInvalidState/PVMFErrArgument/PVMFFailure otherwise.
         **/
        virtual PVMFStatus SetMetaDataKeys(PVPMetadataList& aKeyList) = 0;


        /**
         * This function makes a request to return the list of all available metadata values
         * for a given datasource, in case SetMetaDataKeys was not called. If SetMetaDataKeys
         * were called, then this API would only return metadata for the keys specified in
         * SetMetaDataKeys, if available.
         * The Command should only be called in PVME_STATE_INITIALIZED.
         * The values returned in aValueList will presist till
         * reset is called or another call to GetMetadata is made.
         * This command request is asynchronous. PVCommandStatusObserver's CommandCompleted()
         * callback handler will be called when this command request completes.
         *
         * @param aDataSource
         *         Reference to the player data source to be used for metadata retrieval.
         * @param aValueList
         *         Reference to a vector of KVP to place the specified metadata values
         * @param aContextData
         *         Optional opaque data that will be passed back to the user with the command response
         * @leave This method can leave with one of the following error codes
         *         OsclErrInvalidState if invoked in the incorrect state
         *         OsclErrNoMemory if the SDK failed to allocate memory during this operation
         * @returns A unique command id for asynchronous completion
         **/
        virtual PVCommandId GetMetadata(PVPlayerDataSource& aDataSource,
                                        Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                        const OsclAny* aContextData = NULL) = 0;

        /**
         * This function cleans up resources allocated during Init or later
         * and transitions PVME to PVME_STATE_IDLE state.
         * If already in PVME_STATE_IDLE state, then nothing will occur.
         * This command request is asynchronous. PVCommandStatusObserver's CommandCompleted()
         * callback handler will be called when this command request completes.
         *
         * @param aContextData
         *         Optional opaque data that will be passed back to the user with the command response
         * @param aThreadExit
         *         Optional boolean parameter that is used in case Reset is issued by a routine that
         *         wants to exit the PVME thread. This is only used in case PVME is launched in its
         *         own thread.
         * @leave This method can leave with one of the following error codes
         *         OsclErrNoMemory if the SDK failed to allocate memory during this operation
         * @returns A unique command id for asynchronous completion
         **/
        virtual PVCommandId Reset(const OsclAny* aContextData = NULL) = 0;

        /**
         * In threaded mode this function will stop scheduler. There will not be a
         * command complete for this call. Instead the thread routine will do a sem signal
         * once it is done. This API will do synchronous cleanup in non-threaded mode.
         *
         * @leave This method can leave with one of the following error codes
         *         OsclErrNoMemory if the SDK failed to allocate memory during this operation
         *         OsclErrInvalidState if the SDK is not in proper state.
         * @return None
         **/
        virtual void CleanUp() = 0;
};

#endif // PVMF_METADATA_ENGINE_INTERFACE_H_INCLUDED
