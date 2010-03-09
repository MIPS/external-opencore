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
#ifndef LIPSYNC_DUMMY_OUTPUT_MIO_H_INCLUDED
#define LIPSYNC_DUMMY_OUTPUT_MIO_H_INCLUDED


#ifndef DUMMY_OUTPUT_MIO_H_INCLUDED
#include "dummy_output_mio.h"
#endif

#ifndef PVMI_MEDIA_TRANSFER_H_INCLUDED
#include "pvmi_media_transfer.h"
#endif

#ifndef PVMI_MIO_CONTROL_H_INCLUDED
#include "pvmi_mio_control.h"
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

#ifndef PVMI_MEDIA_IO_CLOCK_EXTENSION_H_INCLUDED
#include "pvmi_media_io_clock_extension.h"
#endif

#ifndef LIPSYNC_SINGLETON_OBJECT_H_INCLUDED
#include "lipsync_singleton_object.h"
#endif
#ifndef OSCL_MAP_H_INCLUDED
#include "oscl_map.h"
#endif

//This class implements the test audio MIO needed for the MOutNode test harness
class LipSyncDummyOutputMIO : public DummyOutputMIO
        , public PvmiClockExtensionInterface
        , public LipSyncNotifyTSObserver
{
    public:

        LipSyncDummyOutputMIO(const DummyMIOSettings& aSettings);
        ~LipSyncDummyOutputMIO();
        PVMFStatus connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver);
        PVMFCommandId QueryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr, const OsclAny* aContext = NULL);

        /**
        * From PVInterface
        * see base-class for more information
        */
        bool queryInterface(const PVUuid& uuid, PVInterface*& iface);

        /**
        * From PvmiClockExtensionInterface
        * see base-class for more information
        */
        PVMFStatus SetClock(PVMFMediaClock* aClockVal);
        void addRef();

        void removeRef();



    protected:
        void DealWithData(PVMFTimestamp aTimestamp, uint8* aData, uint32 aDataLen);

        //LipSyncNotifyTSObserver
        void MIOFramesTimeStamps(bool aIsAudio, uint32 aOrigTS, uint32 aRenderTS);


        Oscl_Map<uint32, uint32, OsclMemAllocator> iCompareTS;
        ShareParams* iParams;
        PVMFMediaClock* iRenderClock;

    private:

};
#endif
