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
#ifndef PVMF_METADATA_ENGINE_TYPES_H_INCLUDED
#define PVMF_METADATA_ENGINE_TYPES_H_INCLUDED

/**
 * An enumeration of the major states of the PVMetadataEngine.
 **/
typedef enum
{
    /**
        The state immediately after the PVME instance has been successfully created or instantiated.
        PVME also returns to this state after successful completion of reset command. In threaded mode
        thread has been successfully launched and is waiting for further commands.
        No resources have been allocated yet.
    **/
    PVME_STATE_IDLE         = 1,

    /**
        PVME is in this state after successfully completing initialization.
        PVME is now ready for metadata queries.
    **/
    PVME_STATE_INITIALIZED  = 2,

    /**
        PVME enters this state when it encounters an error. This is a transitional state and after PVME performs
        error recovery, it will end up in PVME_STATE_IDLE state.
    **/
    PVME_STATE_ERROR            = 3
} PVMetadataEngineState;


#endif // PVMF_METADATA_ENGINE_TYPES_H_INCLUDED
