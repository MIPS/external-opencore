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
#ifndef PVMF_METADATA_ENGINE_FACTORY_H_INCLUDED
#define PVMF_METADATA_ENGINE_FACTORY_H_INCLUDED

#ifndef OSCL_THREAD_H_INCLUDED
#include "oscl_thread.h"
#endif

#ifndef OSCL_SEMAPHORE_H_INCLUDED
#include "oscl_semaphore.h"
#endif

#ifndef PV_METADATA_ENGINE_H_INCLUDED
#include "pv_metadata_engine.h"
#endif

//Forward Declaration
class PVMetadataEngineInterface;
class PVCommandStatusObserver;
class PVInformationalEventObserver;
class PVErrorEventObserver;

typedef enum
{
    PV_METADATA_ENGINE_NON_THREADED_MODE,
    PV_METADATA_ENGINE_THREADED_MODE
} PVMetadataEngineThreadMode;

class LoggerConfigElement
{
    public:
        LoggerConfigElement()
        {
            iLoggerString = NULL;
            iLogLevel = 8;
        }
        char *iLoggerString;
        int8 iLogLevel;
};


class PVMetadataEngineInterfaceContainer
{
    public:
        PVMetadataEngineInterfaceContainer()
        {
            iCmdStatusObserver = NULL;
            iInfoEventObserver = NULL;
            iErrorEventObserver = NULL;
            iPVMEInterface = NULL;
            iMode = PV_METADATA_ENGINE_NON_THREADED_MODE;
            iPriority = ThreadPriorityNormal;
        }
        virtual ~PVMetadataEngineInterfaceContainer() {};

        //observers - to be provided by caller
        PVCommandStatusObserver* iCmdStatusObserver;
        PVInformationalEventObserver* iInfoEventObserver;
        PVErrorEventObserver* iErrorEventObserver;


        PVMetadataEngineInterface* iPVMEInterface;
        //Default is non-threaded mode.
        PVMetadataEngineThreadMode iMode;
        //Default is normal priority.
        OsclThreadPriority iPriority;
        OsclSemaphore iSem;

        uint32 iAppenderType; //Type of appender to be used for the logging 0-> Err Appender, 1-> File Appender
        Oscl_Vector<LoggerConfigElement, OsclMemAllocator> iLoggerConfigElements;
        OSCL_wHeapString<OsclMemAllocator> iLogfilename;
};

/**
 * PVMetadataEngineFactory Class
 *
 * PVMetaDataEngineFactory class is a singleton class which instantiates and provides
 * access to PV Metadata Engine.
 * The client is expected to contain and maintain a pointer to the instance created
 * while the engine is active.
 */
class PVMetadataEngineFactory
{
    public:
        /**
         * Creates an instance of a PVMetadataEngineInterface.
         * Created instance is wrapped inside class PVMetadataEngineInterfaceContainer. This is done to keep
         * track of the semaphore that is used in threaded mode and other thread params.In case creation fails
         * iPVMEInterface pointer inside PVMetadataEngineInterfaceContainer is set to NULL. Caller must
         * check for NULL to make sure CreatePVMetadataEngine call succeeded.
         *
         * @param aPVMEContainer Reference to PVMetadataEngineInterfaceContainer.
         * @return None
         **/
        OSCL_IMPORT_REF static void
        CreatePVMetadataEngine(PVMetadataEngineInterfaceContainer& aPVMEContainer);

        /**
         * Deletes an instance of PVMetadataEngineInterface
         * and reclaims all allocated resources.  An instance can be deleted only in
         * the idle state. An attempt to delete in any other state will fail or result in
         * undefined behaviour.
         *
         * @param aPVMEContainer Reference to PVMetadataEngineInterfaceContainer
         * @return PVMFSuccess/ PVMFFailure / PVMFErrInvalidState
         **/
        OSCL_IMPORT_REF static PVMFStatus
        DeletePVMetadataEngine(PVMetadataEngineInterfaceContainer& aPVMEContainer);
};

#endif // PVMF_METADATA_ENGINE_FACTORY_H_INCLUDED
