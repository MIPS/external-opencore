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
#ifndef PV_ENGINE_OBSERVER_H_INCLUDED
#include "pv_engine_observer.h"
#endif

#ifndef PVMF_METADATA_ENGINE_FACTORY_H_INCLUDED
#include "pv_metadata_engine_factory.h"
#endif

#ifndef OSCL_DLL_H_INCLUDED
#include "oscl_dll.h"
#endif


// Define entry point for this DLL
OSCL_DLL_ENTRY_POINT_DEFAULT()

static TOsclThreadFuncRet OSCL_THREAD_DECL PVMEEntryThread(TOsclThreadFuncArg arg)
{
    PVMetadataEngineInterfaceContainer* iContainer = (PVMetadataEngineInterfaceContainer*)arg;
    int32 leavecode = 0;

    OsclBase::Init();
    OsclErrorTrap::Init();


    //Run remainder of the thread under a trap.
    OSCL_TRY(leavecode,
             PVMetadataEngine::PVMEThread(iContainer););

    //Cleanup Oscl
    OsclErrorTrap::Cleanup();
    OsclBase::Cleanup();

    return 0;
}


OSCL_EXPORT_REF void PVMetadataEngineFactory::CreatePVMetadataEngine(PVMetadataEngineInterfaceContainer& aPVMEContainer)
{
    if (aPVMEContainer.iMode == PV_METADATA_ENGINE_NON_THREADED_MODE)
    {
        int error = 0;
        OSCL_TRY(error, aPVMEContainer.iPVMEInterface = PVMetadataEngine::New(aPVMEContainer));
        if (error != 0)
        {
            aPVMEContainer.iPVMEInterface = NULL; //explicitly set pointer to NULL to signal failure
        }

    }
    else if (aPVMEContainer.iMode == PV_METADATA_ENGINE_THREADED_MODE)
    {
        OsclThread thread;
        OsclProcStatus::eOsclProcError result = thread.Create((TOsclThreadFuncPtr)PVMEEntryThread, 0, &aPVMEContainer);
        if (result == OsclProcStatus::SUCCESS_ERROR)
        {
            aPVMEContainer.iSem.Wait();
        }
        else
        {
            aPVMEContainer.iPVMEInterface = NULL; //explicitly set pointer to NULL to signal failure
        }
    }
    return;

}


OSCL_EXPORT_REF PVMFStatus PVMetadataEngineFactory::DeletePVMetadataEngine(PVMetadataEngineInterfaceContainer& aPVMEContainer)
{

    PVMetadataEngineState iState;

    PVMFStatus status = aPVMEContainer.iPVMEInterface->GetPVMEStateSync(iState);

    if (status != PVME_STATE_IDLE)
    {
        return PVMFErrInvalidState;
    }
    aPVMEContainer.iPVMEInterface->CleanUp();

    if (aPVMEContainer.iMode == PV_METADATA_ENGINE_THREADED_MODE)
    {
        aPVMEContainer.iSem.Wait();
    }

    return PVMFSuccess;
}



