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
#ifndef PVMF_NODE_INTERFACE_H_INCLUDED
#define PVMF_NODE_INTERFACE_H_INCLUDED

#ifndef PV_INTERFACE_H_INCLUDED
#include "pv_interface.h"
#endif

/**
Node states
**/
typedef enum
{
    EPVMFNodeCreated
    , EPVMFNodeIdle
    , EPVMFNodeInitialized
    , EPVMFNodePrepared
    , EPVMFNodeStarted
    , EPVMFNodePaused
    , EPVMFNodeError
    , EPVMFNodeLastState // derived nodes can add more states as needed
} TPVMFNodeInterfaceState;

#define PVMF_NODE_DEFAULT_SESSION_RESERVE 10

typedef int32 PVMFCommandId;
typedef int32 PVMFSessionId;

class PVMFNodeInterface
{
    public:

        virtual ~PVMFNodeInterface() {};

        /**
         * Retrieves state information of this node
         **/
        virtual TPVMFNodeInterfaceState GetState()
        {
            return iInterfaceState;
        }

        /**
         * Starts initialization of the node.  At the minimum, the node should be ready to advertize its
         * capabilities after initialization is complete
         **/
        virtual PVMFCommandId Init(PVMFSessionId aSession
                                   , const OsclAny* aContext = NULL) = 0;

        /**
         * Causes the node to start servicing all connected ports.
         **/
        virtual PVMFCommandId Start(PVMFSessionId aSession
                                    , const OsclAny* aContext = NULL) = 0;

        /**
         * Causes the node to stop servicing all connected ports and
         * discard any un-processed data.
         **/
        virtual PVMFCommandId Stop(PVMFSessionId aSession
                                   , const OsclAny* aContext = NULL) = 0;

        /**
         * Causes the node to pause servicing all connected ports without
         * discarding un-processed data.
         **/
        virtual PVMFCommandId Pause(PVMFSessionId aSession
                                    , const OsclAny* aContext = NULL) = 0;

        /**
         * Resets the node.  The node should relinquish all resources that is has acquired as part of the
         * initialization process and should be ready to be deleted when this completes.
         **/
        virtual PVMFCommandId Reset(PVMFSessionId aSession
                                    , const OsclAny* aContext = NULL) = 0;

    protected:
        PVMFNodeInterface(int32 aSessionReserve = PVMF_NODE_DEFAULT_SESSION_RESERVE) {};

        TPVMFNodeInterfaceState iInterfaceState;


        /** This method can be used to update the state and
        ** notify observers of the state change event.
        */
        virtual void SetState(TPVMFNodeInterfaceState) {};
};

#endif
