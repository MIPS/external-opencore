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
#ifndef PVMF_PARSERNODE_IMPL_H_INCLUDED
#define PVMF_PARSERNODE_IMPL_H_INCLUDED

#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#include "pvmf_node_interface_impl.h"
#endif
#ifndef PVMF_DATASTREAM_INIT_EXTENSION_H_INCLUDED
#include "pvmf_datastream_init_extension.h"
#endif
#ifndef PVMF_DATA_SOURCE_INIT_EXTENSION_H_INCLUDED
#include "pvmf_data_source_init_extension.h"
#endif
#ifndef PVMF_TRACK_SELECTION_EXTENSION_H_INCLUDED
#include "pvmf_track_selection_extension.h"
#endif
#ifndef PVMF_FORMAT_TYPE_H_INCLUDED
#include "pvmf_format_type.h"
#endif
#ifndef PVMF_RETURN_CODES_H_INCLUDED
#include "pvmf_return_codes.h"
#endif
#ifndef PVMF_AUDIO_PARSER_H_INCLUDED
#include "pvmf_audio_parser.h"
#endif
#ifndef PVMF_PARSERNODE_PORT_H_INCLUDED
#include "pvmf_parsernode_port.h"
#endif
#ifndef PVMF_DATA_SOURCE_PLAYBACK_CONTROL_H_INCLUDED
#include "pvmf_data_source_playback_control.h"
#endif
#ifndef PVMF_META_DATA_EXTENSION_H_INCLUDED
#include "pvmf_meta_data_extension.h"
#endif
#ifndef PVMF_PARSER_DEFS_H_INCLUDED
#include "pvmf_parser_defs.h"
#endif

class PvmiDataStreamInterface;
class PVMFFileParserInterface;
class PVLogger;

class PVMFParserNodeImpl: public PVMFDataStreamInitExtension
        , public PVMFDataSourceInitializationExtensionInterface
        , public PVMFTrackSelectionExtensionInterface
        , public PVMFNodeInterfaceImpl
        , public PvmfDataSourcePlaybackControlInterface
        , public PVMFMetadataExtensionInterface
{
    public:
        PVMFParserNodeImpl(int32 aPriority = OsclActiveObject::EPriorityNominal);
        ~PVMFParserNodeImpl();

        // From PVInterface
        void addRef();
        void removeRef();
        bool queryInterface(const PVUuid& uuid, PVInterface*& iface);

        // From PVMFDataStreamInitExtension
        PVMFStatus InitializeDataStreamData(PvmiDataStreamInterface* aDataStream,
                                            PVMFFormatType& aSourceFormat);

        // From PVMFDataSourceInitializationExtensionInterface
        PVMFStatus SetSourceInitializationData(OSCL_wString& aSourceURL,
                                               PVMFFormatType& aSourceFormat,
                                               OsclAny* aSourceData,
                                               uint32 aClipIndex,
                                               PVMFFormatTypeDRMInfo aType = PVMF_FORMAT_TYPE_CONNECT_DRM_INFO_UNKNOWN)
        {
            return PVMFSuccess;
        };

        PVMFStatus SetClientPlayBackClock(PVMFMediaClock* aClientClock)
        {
            return PVMFSuccess;
        };

        PVMFStatus SetEstimatedServerClock(PVMFMediaClock* aClientClock)
        {
            return PVMFSuccess;
        };

        void AudioSinkEvent(PVMFStatus aEvent, uint32 aClipId) {};

        // From PVMFTrackSelectionExtensionInterface
        PVMFStatus GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo);
        PVMFStatus SelectTracks(PVMFMediaPresentationInfo& aInfo);

        // From PvmfDataSourcePlaybackControlInterface
        PVMFCommandId SetDataSourcePosition(PVMFSessionId aSessionId,
                                            PVMFTimestamp aTargetNPT,
                                            PVMFTimestamp& aActualNPT,
                                            PVMFTimestamp& aActualMediaDataTS,
                                            bool aSeekToSyncPoint = true,
                                            uint32 aStreamID = 0,
                                            OsclAny* aContext = NULL);

        PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId,
                                              PVMFTimestamp aTargetNPT,
                                              PVMFTimestamp& aActualNPT,
                                              bool aSeekToSyncPoint = true,
                                              OsclAny* aContext = NULL);

        PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId,
                                              PVMFTimestamp aTargetNPT,
                                              PVMFTimestamp& aSyncBeforeTargetNPT,
                                              PVMFTimestamp& aSyncAfterTargetNPT,
                                              OsclAny* aContext = NULL,
                                              bool aSeekToSyncPoint = true);

        PVMFCommandId SetDataSourceRate(PVMFSessionId aSessionId,
                                        int32 aRate,
                                        PVMFTimebase* aTimebase = NULL,
                                        OsclAny* aContext = NULL);

        // From PVMFMetadataExtensionInterface
        PVMFStatus SetMetadataClipIndex(uint32 aClipIndex);

        uint32 GetNumMetadataValues(PVMFMetadataList& aKeyList);

        PVMFCommandId GetNodeMetadataValues(PVMFSessionId aSessionId,
                                            PVMFMetadataList& aKeyList,
                                            Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                            uint32 aStartingValueIndex,
                                            int32 aMaxValueEntries = -1,
                                            const OsclAny* aContextData = NULL);

        PVMFStatus ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                             uint32 aStartingValueIndex,
                                             uint32 aEndValueIndex);

        // From PVMFNodeInterfaceImpl
        PVMFStatus HandleExtensionAPICommands();
        PVMFStatus CancelCurrentCommand();
        PVMFStatus DoQueryInterface();
        PVMFStatus DoInit();
        PVMFStatus DoRequestPort(PVMFPortInterface*& aPort);
        PVMFStatus DoStop();
        PVMFStatus DoReleasePort();
        PVMFStatus DoReset();

        // From PVActiveBase
        void Run();

        // From PVMFPortActivityHandler
        void HandlePortActivity(const PVMFPortActivity& aActivity);

        friend class PVMFParserNodePort;

    private:
        void ProcessPortActivity(PVMFParserNodeTrackPortInfo* aTrackPortInfo);

        PVMFStatus QueueMediaSample(PVMFParserNodeTrackPortInfo* aTrackPortInfo);

        PVMFStatus DoSetDataSourcePosition();

        void ClearTrackPortInfo();

        PVMFStatus DoGetNodeMetadataValues();

        // Source of the data
        PvmiDataStreamInterface* ipDataStream;
        PVMFFileParserInterface* ipParser;

        // Storing File Info
        PVMFFormatType iClipFmtType;
        int64 iClipDuration;
        uint32 iClipTimescale;
        PVFileInfo iFileInfo;

        // For Tracks Info
        PVMFParserNodeTrackPortInfo iTrackPortInfo;
        Oscl_Vector<PVMFParserNodeTrackPortInfo, OsclMemAllocator> iTrkPortInfoVec;

        // Reading Data
        GAU* ipGau;

        // For Logging
        PVLogger* ipLogger;
        PVLogger* ipDatapathLog;
};

#endif
