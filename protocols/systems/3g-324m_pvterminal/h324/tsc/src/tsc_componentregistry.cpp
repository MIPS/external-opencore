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
#include "tsc_componentregistry.h"

TSCComponentRegistry::TSCComponentRegistry(TSC_statemanager& aTSCStateManager,
        TSC_capability& aTSCcapability,
        TSC_lc& aTSClc,
        TSC_blc& aTSCblc,
        TSC_clc& aTSCclc,
        TSC_mt& aTSCmt) :
        iTSCstatemanager(aTSCStateManager),
        iTSCcapability(aTSCcapability),
        iTSClc(aTSClc),
        iTSCblc(aTSCblc),
        iTSCclc(aTSCclc),
        iTSCmt(aTSCmt)
{
}

TSC_component* TSCComponentRegistry::Create(PVMFSessionId aSession,
        const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);
    TSC_component* component = NULL;
    if (aUuid == PVUuidH324ComponentInterface)
    {
        component = OSCL_NEW(TSC_component,
                             (iTSCstatemanager, iTSCcapability, iTSClc, iTSCblc, iTSCclc, iTSCmt));
        aInterfacePtr = (PVMFComponentInterface*)component;
        aInterfacePtr->addRef();
        component->removeRef();
    }
    return component;
}


