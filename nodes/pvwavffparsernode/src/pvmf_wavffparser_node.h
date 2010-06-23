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
#ifndef PVMF_WAVFFPARSER_NODE_H_INCLUDED
#define PVMF_WAVFFPARSER_NODE_H_INCLUDED

#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#include "pvmf_node_interface_impl.h"
#endif

#ifndef PVMF_DATA_SOURCE_PLAYBACK_CONTROL_H_INCLUDED
#include "pvmf_data_source_playback_control.h"
#endif

#ifndef PVMF_MEMPOOL_H_INCLUDED
#include "pvmf_mempool.h"
#endif

#ifndef PVMF_SIMPLE_MEDIA_BUFFER_H_INCLUDED
#include "pvmf_simple_media_buffer.h"
#endif

#ifndef PVMF_META_DATA_EXTENSION_H_INCLUDED
#include "pvmf_meta_data_extension.h"
#endif

#ifndef PVWAVFILEPARSER_H_INCLUDED
#include "pvwavfileparser.h"
#endif

#ifndef PVMF_SOURCE_CONTEXT_DATA_H_INCLUDED
#include "pvmf_source_context_data.h"
#endif

#ifndef PVMF_DATA_SOURCE_INIT_EXTENSION_H_INCLUDED
#include "pvmf_data_source_init_extension.h"
#endif

#ifndef PVMF_TRACK_SELECTION_EXTENSION_H_INCLUDED
#include "pvmf_track_selection_extension.h"
#endif

class MediaClockConverter;
class PVMFWAVFFParserNode;
class PVWAVFFNodeTrackPortInfo : public OsclMemPoolFixedChunkAllocatorObserver
{
    public:
        enum TrackState
        {
            TRACKSTATE_UNINITIALIZED,
            TRACKSTATE_INITIALIZED,
            TRACKSTATE_TRANSMITTING_GETDATA,
            TRACKSTATE_TRANSMITTING_SENDDATA,
            TRACKSTATE_SEND_ENDOFTRACK,
            TRACKSTATE_MEDIADATAPOOLEMPTY,
            TRACKSTATE_DESTFULL,
            TRACKSTATE_SOURCEEMPTY,
            TRACKSTATE_ENDOFTRACK,
            TRACKSTATE_ERROR
        };

        PVWAVFFNodeTrackPortInfo()
        {
            iTrackId = -1;
            iPort = NULL;
            iClockConverter = NULL;
            iState = TRACKSTATE_UNINITIALIZED;
            iTrackDataMemoryPool = NULL;
            iMediaDataImplAlloc = NULL;
            iMediaDataMemPool = NULL;
            iNode = NULL;
            iSeqNum = 0;
            iSendBOS = false;
        }

        PVWAVFFNodeTrackPortInfo(const PVWAVFFNodeTrackPortInfo& aSrc):
                OsclMemPoolFixedChunkAllocatorObserver()
        {
            iTrackId = aSrc.iTrackId;
            iPort = aSrc.iPort;
            iTag = aSrc.iTag;
            iClockConverter = aSrc.iClockConverter;
            iMediaData = aSrc.iMediaData;
            iState = aSrc.iState;
            iTrackDataMemoryPool = aSrc.iTrackDataMemoryPool;
            iMediaDataImplAlloc = aSrc.iMediaDataImplAlloc;
            iMediaDataMemPool = aSrc.iMediaDataMemPool;
            iNode = aSrc.iNode;
            iSeqNum = aSrc.iSeqNum;
            iSendBOS = aSrc.iSendBOS;
        }

        virtual ~PVWAVFFNodeTrackPortInfo()
        {
        }

        // From OsclMemPoolFixedChunkAllocatorObserver
        // Callback handler when mempool's deallocate() is called after
        // calling notifyfreechunkavailable() on the mempool
        void freechunkavailable(OsclAny*);

        // Track ID number in WAV FF
        int32 iTrackId;
        // Output port to send the data downstream
        PVMFPortInterface* iPort;

        int32 iTag;

        // Converter to convert from track timescale to milliseconds
        MediaClockConverter* iClockConverter;
        // Shared memory pointer holding the currently retrieved track data
        PVMFSharedMediaDataPtr iMediaData;
        // Current state of this track
        TrackState iState;
        // Output buffer memory pool
        OsclMemPoolFixedChunkAllocator *iTrackDataMemoryPool;
        // Allocator for simple media data buffer impl
        PVMFSimpleMediaBufferCombinedAlloc *iMediaDataImplAlloc;
        // Memory pool for simple media data
        PVMFMemPoolFixedChunkAllocator *iMediaDataMemPool;
        // WAV FF parser node handle
        PVMFWAVFFParserNode* iNode;
        // Sequence number
        uint32 iSeqNum;
        // bos flag
        bool iSendBOS;
};

class PV_Wav_Parser;
class PVMFWAVFFParserOutPort;

class PVMFWAVFFParserNode : public PVMFNodeInterfaceImpl,
        public PVMFDataSourceInitializationExtensionInterface,
        public PVMFTrackSelectionExtensionInterface,
        public PvmfDataSourcePlaybackControlInterface,
        public PVMFMetadataExtensionInterface
{
    public:
        // constructor
        PVMFWAVFFParserNode(int32 aPriority = OsclActiveObject::EPriorityNominal);
        // destructor
        ~PVMFWAVFFParserNode();

        // From PVMFNodeInterface
        PVMFStatus QueryInterfaceSync(PVMFSessionId aSession,
                                      const PVUuid& aUuid,
                                      PVInterface*& aInterfacePtr);

        //From PVMFDataSourceInitializationExtensionInterface
        void addRef();
        void removeRef();
        bool queryInterface(const PVUuid& uuid, PVInterface *& iface);
        PVMFStatus SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData, uint32 aClipIndex, PVMFFormatTypeDRMInfo aType = PVMF_FORMAT_TYPE_CONNECT_DRM_INFO_UNKNOWN);
        PVMFStatus SetClientPlayBackClock(PVMFMediaClock* aClientClock);
        PVMFStatus SetEstimatedServerClock(PVMFMediaClock* aClientClock);
        void AudioSinkEvent(PVMFStatus aEvent, uint32 aStreamId);

        //From PVMFTrackSelectionExtensionInterface
        PVMFStatus GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo);
        PVMFStatus SelectTracks(PVMFMediaPresentationInfo& aInfo);

        // From PVMFMetadataExtensionInterface
        PVMFStatus SetMetadataClipIndex(uint32 aClipNum)
        {
            return (aClipNum == 0) ? PVMFSuccess : PVMFErrArgument;
        }
        uint32 GetNumMetadataValues(PVMFMetadataList& aKeyList);
        PVMFCommandId GetNodeMetadataValues(PVMFSessionId aSessionId,
                                            PVMFMetadataList& aKeyList,
                                            Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                            uint32 aStartingValueIndex,
                                            int32 aMaxValueEntries,
                                            const OsclAny* aContextData = NULL);
        PVMFStatus ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
                                             uint32 aStartingValueIndex,
                                             uint32 aEndValueIndex) ;

        // From PvmfDataSourcePlaybackControlInterface
        PVMFCommandId SetDataSourcePosition(PVMFSessionId aSessionId, PVMFTimestamp aTargetNPT, PVMFTimestamp& aActualNPT, PVMFTimestamp& aActualMediaDataTS, bool aSeekToSyncPoint = true, uint32 aStreamID = 0, OsclAny* aContext = NULL);
        PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId, PVMFTimestamp aTargetNPT, PVMFTimestamp& aActualNPT, bool aSeekToSyncPoint = true, OsclAny* aContext = NULL);
        PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId, PVMFTimestamp aTargetNPT,
                                              PVMFTimestamp& aSeekPointBeforeTargetNPT, PVMFTimestamp& aSeekPointAfterTargetNPT,
                                              OsclAny* aContext = NULL, bool aSeekToSyncPoint = true);
        PVMFCommandId SetDataSourceRate(PVMFSessionId aSession, int32 aRate, PVMFTimebase* aTimebase = NULL, OsclAny* aContext = NULL);

    private:

        //from PVMFPortActivityHandler
        void HandlePortActivity(const PVMFPortActivity& aActivity);

        void Construct();
        void Run();

        // Port processing
        bool ProcessPortActivity();
        void QueuePortActivity(const PVMFPortActivity &aActivity);
        PVMFStatus ProcessOutgoingMsg(PVMFPortInterface* aPort);
        bool HandleOutgoingQueueReady(PVMFPortInterface* aPortInterface);

        //Command dispatchers
        PVMFStatus HandleExtensionAPICommands();

        PVMFStatus CancelCurrentCommand();

        //Command handlers for PVMFNodeInterface APIs
        PVMFStatus DoQueryInterface();
        PVMFStatus DoInit();
        PVMFStatus DoStop();
        PVMFStatus DoReset();
        PVMFStatus DoRequestPort(PVMFPortInterface*& aPort);
        PVMFStatus DoReleasePort();

        //Command handlers for Extension APIs
        PVMFStatus DoSetDataSourcePosition();
        PVMFStatus DoQueryDataSourcePosition();
        PVMFStatus DoSetDataSourceRate();
        PVMFStatus DoGetNodeMetadataValue();

        void InitializeTrackStructure();

        bool verify_supported_format();
        // used to configure downstream ports
        PVMFStatus NegotiateSettings(PvmiCapabilityAndConfig* configInterface);

        bool MapWAVErrorCodeToEventCode(int32 aWAVErrCode, PVUuid& aEventUUID, int32& aEventCode);

        // Track data processing
        bool HandleTrackState();
        bool RetrieveTrackData(PVWAVFFNodeTrackPortInfo& aTrackPortInfo);
        bool SendTrackData(PVWAVFFNodeTrackPortInfo& aTrackPortInfo);
        bool CheckAvailabilityForSendingNewTrackData(PVWAVFFNodeTrackPortInfo& aTrackPortInfo);
        void ResetAllTracks();
        bool ReleaseAllPorts();
        void CleanupFileSource();
        int32 PushBackPortActivity(PVMFPortActivity &aActivity);
        int32 CreateNewArray(char*& aPtr, int32 aLen);
        int32 PushBackKeyVal(Oscl_Vector<PvmiKvp, OsclMemAllocator>*& aValueListPtr, PvmiKvp &aKeyVal);

    private: // private member variables
        /**
         * Queue holding port activity. Only incoming and outgoing msg activity are
         * put on the queue.  For each port, there should only be at most one activity
         * of each type on the queue.
         */
        Oscl_Vector<PVMFPortActivity, OsclMemAllocator> iPortActivityQueue;
        // output port
        PVMFWAVFFParserOutPort* iOutPort;
        // file info struct
        PVWAVFileInfo wavinfo;
        // buffer size for track data
        uint32 trackdata_bufsize;
        // number of samples to be contained in buffer.
        uint32 trackdata_num_samples;
        // file parser object
        PV_Wav_Parser *iWAVParser;
        // track list
        Oscl_Vector<PVWAVFFNodeTrackPortInfo, OsclMemAllocator> iSelectedTrackList;
        // metadata keys vector
        Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> iAvailableMetadataKeys;
        // file handle
        OsclFileHandle *iFileHandle;
        // file server
        Oscl_FileServer iFileServer;
        // file name
        OSCL_wHeapString<OsclMemAllocator> iSourceURL;
        // source format type
        PVMFFormatType iSourceFormat;
        // friend classes
        friend class PVMFWAVFFParserOutPort;
};

#endif // PVMF_WAVFFPARSER_NODE_H_INCLUDED

