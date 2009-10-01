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
#ifndef PV_2WAY_SOURCE_AND_SINKS_BASE_H_INCLUDED
#define PV_2WAY_SOURCE_AND_SINKS_BASE_H_INCLUDED

////////////////////////////////////////////////////////////////////////////
// This file includes the base class for all MIOs (sources and sinks) within
// the application
//
// At this point it consists of one audio sink, one audio source, one video
// sink, one video source.  As all of these use the same base class it could
// be easily modified, however.
//
////////////////////////////////////////////////////////////////////////////

#include "pv_engine_observer.h"
#include "pv_2way_h324m_types.h"
#include "pv_2way_interface.h"
#include "pvmi_mio_control.h"
#include "pvmi_mio_fileinput.h"
#include "pvmi_media_io_fileoutput.h"

#include "pv_2way_mio.h"



#include "oscl_mem.h"
#include "oscl_mem_audit.h"

enum PV2WayTestCmdState
{
    EPVTestCmdIdle,
    EPVTestCmdInited,
    EPVTestCmdConnected,
    EPVTestCmdAddAudioDataSink,
    EPVTestCmdAddVideoDataSink,
    EPVTestCmdAddAudioDataSource,
    EPVTestCmdAddVideoDataSource,
    EPVTestCmdSessionParams,
    EPVTestCmdDisConnected,
    EPVTestCmdReset,
    EPVTestCmdRemoveAudioDataSink,
    EPVTestCmdRemoveVideoDataSink,
    EPVTestCmdRemoveAudioDataSource,
    EPVTestCmdRemoveVideoDataSource,
    EPVTestCmdQueryInterface
};

class PV2WaySourceAndSinksBase :   public PVCommandStatusObserver,
        public PV2WayMIOObserver
{
    public:
        OSCL_IMPORT_REF PV2WaySourceAndSinksBase(PV2Way324InitInfo& aSdkInitInfo);
        OSCL_IMPORT_REF virtual ~PV2WaySourceAndSinksBase();

        OSCL_IMPORT_REF void Init();

        OSCL_IMPORT_REF void Cleanup();
        OSCL_IMPORT_REF void ResetCompleted();
        OSCL_IMPORT_REF void HandleClosingTrack(const PVAsyncInformationalEvent& aEvent);
        OSCL_IMPORT_REF void HandleCloseTrack(const PVAsyncInformationalEvent& aEvent);

        OSCL_IMPORT_REF void CreateMIOs();
        OSCL_IMPORT_REF void CommandCompleted(const PVCmdResponse& aResponse);
        OSCL_IMPORT_REF void PrintFormatTypes();

        OSCL_IMPORT_REF void SetTerminal(CPV2WayInterface *aTerminal)
        {
            iTerminal = aTerminal;
            iAudioSource->SetTerminal(iTerminal);
            iAudioSink->SetTerminal(iTerminal);
            iVideoSource->SetTerminal(iTerminal);
            iVideoSink->SetTerminal(iTerminal);
        }

        OSCL_IMPORT_REF bool AllMIOsAdded()
        {
            if (iAudioSource->IsAdded() &&
                    iAudioSink->IsAdded() &&
                    iVideoSource->IsAdded() &&
                    iVideoSink->IsAdded())
                return true;
            return false;

        }
        OSCL_IMPORT_REF bool AllMIOsRemoved();

        OSCL_IMPORT_REF int ResetPreferredCodec(TPVDirection aDir, PV2WayMediaType aMediaType);


        OSCL_IMPORT_REF void ParseResponse(const PVAsyncInformationalEvent& aEvent,
                                           OSCL_HeapString<OsclMemAllocator>& aMimeString,
                                           int& aMedia_type);


        OSCL_IMPORT_REF void RemoveSourceAndSinks();

        OSCL_IMPORT_REF PVCommandId RemoveAudioSink()
        {
            return iAudioSink->Remove();
        }
        OSCL_IMPORT_REF PVCommandId RemoveAudioSource()
        {
            return iAudioSource->Remove();
        }
        OSCL_IMPORT_REF PVCommandId RemoveVideoSink()
        {
            return iVideoSink->Remove();
        }
        OSCL_IMPORT_REF PVCommandId RemoveVideoSource()
        {
            return iVideoSource->Remove();
        }
        OSCL_IMPORT_REF PVCommandId HandleIncomingVideo(const PVAsyncInformationalEvent& aEvent)
        {
            return iVideoSink->HandleEvent(aEvent);
        }
        OSCL_IMPORT_REF PVCommandId HandleIncomingAudio(const PVAsyncInformationalEvent& aEvent)
        {
            return iAudioSink->HandleEvent(aEvent);
        }
        OSCL_IMPORT_REF PVCommandId HandleOutgoingVideo(const PVAsyncInformationalEvent& aEvent)
        {
            return iVideoSource->HandleEvent(aEvent);
        }
        OSCL_IMPORT_REF PVCommandId HandleOutgoingAudio(const PVAsyncInformationalEvent& aEvent)
        {
            return iAudioSource->HandleEvent(aEvent);
        }

        virtual OSCL_IMPORT_REF PVMFNodeInterface* CreateMIONode(CodecSpecifier* aformat,
                TPVDirection adir) = 0;
        virtual OSCL_IMPORT_REF void DeleteMIONode(CodecSpecifier* aformat,
                TPVDirection adir,
                PVMFNodeInterface** aMioNode) = 0;
        void CloseSourceAndSinks()
        {
            iAudioSource->Closed();
            iVideoSource->Closed();
            iAudioSink->Closed();
            iVideoSink->Closed();
        }

        int GetEngineCmdState(PVCommandId aCmdId);

        OSCL_IMPORT_REF bool FormatMatchesSelectedCodec(TPVDirection aDir,
                PV2WayMediaType aMediaType, PVMFFormatType& aFormat);

    protected:
        OSCL_IMPORT_REF PV2WayMIO* GetMIO(TPVDirection aDir,
                                          PV2WayMediaType aMediaType);

        virtual void OutputInfo(PVLogger::log_level_type aLogLevel, const char * str, ...) = 0;



        //PV2WaySourceAndSinksObserver* iObserver;

        /*! @var iAudioStartRecId The command id for start recording incoming audio */
        PVCommandId iAudioStartRecId;
        /*! @var iAudioStopRecId The command id for stop recording of incoming audio */
        PVCommandId iAudioStopRecId;
        PVCommandId iVideoStartRecId;
        /*! @var iVideoStopRecId The command id for stop recording of incoming video */
        PVCommandId iVideoStopRecId;

        PV2Way324InitInfo& iSdkInitInfo;

        PV2WayMIO* iAudioSink;
        PV2WayMIO* iAudioSource;
        PV2WayMIO* iVideoSink;
        PV2WayMIO* iVideoSource;

        CPV2WayInterface *iTerminal;
};



#endif
