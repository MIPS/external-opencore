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
/**
 * @file pvmp4ffcn_node.cpp
 * @brief Node for PV MPEG4 file format composer
 */

#include "pvmp4ffcn_node.h"
#include "pvmp4ffcn_factory.h"
#include "pvmp4ffcn_port.h"

#if ANDROID_FILEWRITER
// #define LOG_NDEBUG 0
#define LOG_TAG "PvMp4Composer"
#include <utils/Log.h>
#include <utils/Errors.h>
#include <utils/threads.h>
// NUMBER_OUTPUT_BUFFER is number of output buffers in encoder node, pvmf_omx_enc_node.h
// Need to change encoder node also if updated here.
#define NUMBER_OUTPUT_BUFFER 9

namespace android
{

// FragmentWriter is a queue of media fragment to be written in the
// media file. The caller enqueues the next fragment and returns
// immediately. An separate thread dequeues the fragment and writes it.
//
// This class is friend with the composer node it belongs to, in order
// to be able to call the original AddMemFragToTrack which does all
// the work.
//
// The queue is implemented using a circular buffer. mBuffer is the
// start of the array and mEnd points just after the last element in
// the array.
// mFirst points to the oldest fragment.
// mLast points to the next available cell for a new fragment.
// When the array is empty or full mFirst == mLast.

class FragmentWriter: public Thread
{
    public:
        FragmentWriter(PVMp4FFComposerNode *composer) :
                Thread(kThreadCallJava), mSize(0), mEnd(mBuffer + kCapacity),
                mFirst(mBuffer), mLast(mBuffer), mComposer(composer),
                mPrevWriteStatus(PVMFSuccess), mTid(NULL), mDropped(0), mExitRequested(false) {}

        virtual ~FragmentWriter()
        {
            Mutex::Autolock l(mRequestMutex);
            LOG_ASSERT(0 == mSize, "The queue should be empty by now.");
            LOG_ASSERT(mExitRequested, "Deleting an active instance.");
            LOGD_IF(0 < mSize, "Flushing %d frags in dtor", mSize);
            while (0 < mSize)  // make sure we are flushed
            {
                decrPendingRequests();
            }
        }

        // Mark the thread as exiting and kick it so it can see the
        // exitPending state.
        virtual void requestExit()
        {
            mExitRequested = true;
            Thread::requestExit();
            mRequestMutex.lock();
            mRequestCv.signal();
            mRequestMutex.unlock();
        }

        // Wait for all the fragment to be written.
        virtual void flush()
        {
            LOG_ASSERT(androidGetThreadId() != mTid, "Reentrant call");

            bool done = false;
            size_t iter = 0;
            while (!done)
            {
                mRequestMutex.lock();
                done = mSize == 0 || iter > kMaxFlushAttempts;
                if (!done) mRequestCv.signal();
                mRequestMutex.unlock();
                if (!done) usleep(kFlushSleepMicros);
                ++iter;
            }
            LOG_ASSERT(iter <= kMaxFlushAttempts, "Failed to flush");
        }

        // Called by the ProcessIncomingMsg method from the
        // PVMp4FFComposerNode to append the fragment to the track.
        // @return The result of the *previous* fragment written. Since the call
        //         is asynch we cannot wait.
        PVMFStatus enqueueMemFragToTrack(Oscl_Vector<OsclMemoryFragment, OsclMemAllocator> aFrame,
                                         PVMFFormatType aFormat, uint32 aSeqNum, uint32& aTimestamp,
                                         int32 aTrackId, PVMp4FFComposerPort *aPort, uint32 aSampleDuration)
        {
            if (mExitRequested) return PVMFErrCancelled;
            Mutex::Autolock lock(mRequestMutex);

            // When the queue is full, we drop the request. This frees the
            // memory fragment and keeps the system running. Decoders are
            // unhappy when there is no buffer available to write the
            // output.
            // An alternative would be to discard the oldest fragment
            // enqueued to free some space. However that would modify
            // mFirst and require extra locking because the thread maybe
            // in the process of writing it.
            if (mSize == kCapacity)
            {
                ++mDropped;
                LOGW_IF((mDropped % kLogDroppedPeriod) == 0, "Frame %d dropped.", mDropped);
                // TODO: Should we return an error code here?
                return mPrevWriteStatus;
            }

            mLast->set(aFrame, aFormat, aSeqNum, aTimestamp, aTrackId, aPort, aSampleDuration);
            incrPendingRequests();

            mRequestCv.signal();
            return mPrevWriteStatus;
        }

    private:
        static const bool kThreadCallJava = false;
        static const size_t kLogDroppedPeriod = 10;  // Arbitrary.
        // Must match the number of buffers allocated in the decoder.
        static const size_t kCapacity = NUMBER_OUTPUT_BUFFER;
        static const size_t kWarningThreshold = kCapacity * 3 / 4; // Warn at 75%
        // Flush blocks for 2 seconds max.
        static const size_t kMaxFlushAttempts = 10;
        static const int kFlushSleepMicros = 200 * 1000;

        struct Request
        {
            void set(Oscl_Vector<OsclMemoryFragment, OsclMemAllocator> aFrame,
                     PVMFFormatType aFormat, uint32 aSeqNum, uint32 aTimestamp,
                     int32 aTrackId, PVMp4FFComposerPort *aPort, uint32 aSampleDuration)
            {
                mFrame = aFrame;
                mFormat = aFormat;
                mSeqNum = aSeqNum;
                mTimestamp = aTimestamp;
                mTrackId = aTrackId;
                mPort = aPort;
                mSampleDuration = aSampleDuration;
            }

            Oscl_Vector<OsclMemoryFragment, OsclMemAllocator> mFrame;
            PVMFFormatType mFormat;
            uint32 mSeqNum;
            uint32 mTimestamp;
            uint32 mTrackId;
            PVMp4FFComposerPort *mPort;
            uint32 mSampleDuration;
        };

        void incrPendingRequests()
        {
            ++mLast;
            if (mEnd == mLast) mLast = mBuffer;
            ++mSize;
        }

        void decrPendingRequests()
        {
            mFirst->mFrame.clear();
            ++mFirst;
            if (mEnd == mFirst) mFirst = mBuffer;
            --mSize;
        }

        // Called by the base class Thread.
        // @return true if there more work to do. false when done.
        // @Override Thread
        virtual bool threadLoop()
        {
            if (!mTid) mTid = androidGetThreadId();

            LOG_ASSERT(androidGetThreadId() == mTid,
                       "Thread id has changed!: %p != %p", mTid, androidGetThreadId());

            size_t numFrags = 0;
            // Check if there's work to do. Otherwise wait for new fragment.
            mRequestMutex.lock();
            numFrags = mSize;
            mRequestMutex.unlock();

            bool doneWaiting = numFrags != 0;
            while (!doneWaiting)
            {
                mRequestMutex.lock();
                mRequestCv.wait(mRequestMutex);
                doneWaiting = mSize > 0 || mExitRequested;
                numFrags = mSize;
                mRequestMutex.unlock();
            }

            if (mExitRequested) return false;

            LOGW_IF(numFrags > kWarningThreshold, "%d fragments in queue.", numFrags);
            for (size_t i = 0; i < numFrags; ++i)
            {
                // Don't lock the array while we are calling
                // AddMemFragToTrack, which may last a long time, because
                // we are the only thread accessing mFirst.
                mPrevWriteStatus = mComposer->AddSampleToTrack(
                                       mFirst->mFrame, mFirst->mFormat, mFirst->mSeqNum, mFirst->mTimestamp,
                                       mFirst->mTrackId, mFirst->mPort, mFirst->mSampleDuration);

                mRequestMutex.lock();
                decrPendingRequests();
                mRequestMutex.unlock();
            }
            return true;
        }


        Mutex mRequestMutex;  // Protects mRequestCv, mBuffer, mFirst, mLast, mDropped
        Condition mRequestCv;
        Request mBuffer[kCapacity];
        size_t mSize;
        void *const mEnd;  // Marker for the end of the array.
        Request *mFirst, *mLast;

        // mComposer with the real implementation of the AddMemFragToTrack method.
        PVMp4FFComposerNode *mComposer;
        // TODO: lock needed for mPrevWriteStatus? Are int assignement atomic on arm?
        PVMFStatus mPrevWriteStatus;

        android_thread_id_t mTid;
        size_t mDropped;
        // Unlike exitPending(), stays to true once exit has been called.
        bool mExitRequested;
};
}
#endif // ANDROID_FILEWRITER

#define LOG_STACK_TRACE(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, m);
#define LOG_DEBUG(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, m);
#define LOG_ERR(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iLogger,PVLOGMSG_ERR,m);
#define LOGDATATRAFFIC(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iDataPathLogger,PVLOGMSG_INFO,m);

#ifdef _TEST_AE_ERROR_HANDLING
const uint32 FAIL_NODE_CMD_START = 2;
const uint32 FAIL_NODE_CMD_STOP = 3;
const uint32 FAIL_NODE_CMD_FLUSH = 4;
const uint32 FAIL_NODE_CMD_PAUSE = 5;
const uint32 FAIL_NODE_CMD_RELEASE_PORT = 7;
#endif

#define SLASH '/'

#define LANG_CODE_SIZE 3

// Define entry point for this DLL
OSCL_DLL_ENTRY_POINT_DEFAULT()

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFNodeInterface* PVMp4FFComposerNodeFactory::CreateMp4FFComposer(int32 aPriority)
{
    int32 err = 0;
    PVMFNodeInterface* node = NULL;

    OSCL_TRY(err,
             node = (PVMFNodeInterface*)OSCL_NEW(PVMp4FFComposerNode, (aPriority));
             if (!node)
             OSCL_LEAVE(OsclErrNoMemory);
            );

    OSCL_FIRST_CATCH_ANY(err, return NULL;);
    return node;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMp4FFComposerNodeFactory::DeleteMp4FFComposer(PVMFNodeInterface* aComposer)
{
    if (aComposer)
    {
        PVMp4FFComposerNode* node = (PVMp4FFComposerNode*)aComposer;
        OSCL_DELETE(node);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
PVMp4FFComposerNode::PVMp4FFComposerNode(int32 aPriority)
        : PVMFNodeInterfaceImpl(aPriority, "PVMp4FFComposerNode")
        , iMpeg4File(NULL)
        , iFileType(0)
        , iAuthoringMode(PVMP4FF_3GPP_DOWNLOAD_MODE)
        , iPresentationTimescale(1000)
        , iMovieFragmentDuration(2000)
        , iRecordingYear(0)
        , iClockConverter(8000)
        , iExtensionRefCount(0)
        , iRealTimeTS(false)
        , iInitTSOffset(false)
        , iTSOffset(0)
        , iMaxFileSizeEnabled(false)
        , iMaxDurationEnabled(false)
        , iMaxFileSize(0)
        , iMaxTimeDuration(0)
        , iFileSizeReportEnabled(false)
        , iDurationReportEnabled(false)
        , iFileSizeReportFreq(0)
        , iDurationReportFreq(0)
        , iNextDurationReport(0)
        , iNextFileSizeReport(0)
        , iCacheSize(0)
        , iConfigSize(0)
        , pConfig(NULL)
        , iSyncSample(0)
        , iNodeEndOfDataReached(false)
        , iSampleInTrack(false)
        , iFileRendered(false)
{
    iInterfaceState = EPVMFNodeCreated;
    iNum_PPS_Set = 0;
    iNum_SPS_Set = 0;
    iFileObject = NULL;
    oDiagnosticsLogged = false;
    iDiagnosticsLogger = PVLogger::GetLoggerObject("pvauthordiagnostics.composer.mp4");
    iDataPathLogger = PVLogger::GetLoggerObject("datapath.sinknode.mp4composer");
    int32 err;
    OSCL_TRY(err,
             //Create the port vector.
             iInPorts.Construct(PVMF_MP4FFCN_PORT_VECTOR_RESERVE);
            );

    OSCL_FIRST_CATCH_ANY(err,
                         //if a leave happened, cleanup and re-throw the error
                         iInPorts.clear();
                         memvector_sps.clear();
                         memvector_pps.clear();
                         OSCL_CLEANUP_BASE_CLASS(PVMFNodeInterface);
                         OSCL_CLEANUP_BASE_CLASS(OsclActiveObject);
                         OSCL_LEAVE(err);
                        );
    OSCL_TRY(err,
             iSampleParam = OSCL_NEW(PVMP4FFComposerSampleParam, ());
             if (!iSampleParam)
             OSCL_LEAVE(OsclErrNoMemory);
            );

#if ANDROID_FILEWRITER
    iMaxReachedEvent = 0;
    iMaxReachedReported = false;
    iFragmentWriter = new android::FragmentWriter(this);
    iFragmentWriter->run(LOG_TAG);
#endif

#ifdef _TEST_AE_ERROR_HANDLING
    iErrorHandlingAddMemFrag = false;
    iErrorHandlingAddTrack = false;
    iErrorCreateComposer = false;
    iErrorRenderToFile = false;
    iErrorAddTrack = PVMF_MIME_FORMAT_UNKNOWN;
    iErrorNodeCmd = 0;
    iTestFileSize = 0;
    iTestTimeStamp = 0;
    iErrorAddSample = 0;
    iFileSize = 0;
    iFileDuration = 0;
    iErrorDataPathStall = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////
PVMp4FFComposerNode::~PVMp4FFComposerNode()
{
    iDataPathLogger = NULL;

    if (!oDiagnosticsLogged)
    {
        LogDiagnostics();
    }

#if ANDROID_FILEWRITER
    if (iFragmentWriter != NULL)
    {
        iFragmentWriter->requestExit(); // kick the thread
        iFragmentWriter->requestExitAndWait();
    }
#endif

    if (iMpeg4File)
    {
        PVA_FF_IMpeg4File::DestroyMP4FileObject(iMpeg4File);

        if (!iFileRendered)
        {
            iFs.Connect();
            iFs.Oscl_DeleteFile(iFileName.get_cstr());
            iFs.Close();
        }
    }
    if (iFileObject)
    {
        iFileObject->Close();
        OSCL_DELETE(iFileObject);
        iFileObject = NULL;
    }
    for (uint32 i = 0; i < iKeyWordVector.size() ; i++)
    {
        if (iKeyWordVector[i] != NULL)
        {
            OSCL_DELETE(iKeyWordVector[i]);
            iKeyWordVector[i] = NULL;
        }

    }

    if (pConfig != NULL)
    {
        OSCL_FREE(pConfig);
        iConfigSize = 0;
    }

    if (iLocationInfo._location_name != NULL)
    {
        OSCL_FREE(iLocationInfo._location_name);
    }

    if (iLocationInfo._astronomical_body != NULL)
    {
        OSCL_FREE(iLocationInfo._astronomical_body);
    }

    if (iLocationInfo._additional_notes != NULL)
    {
        OSCL_FREE(iLocationInfo._additional_notes);
    }
    // Cleanup allocated ports
    while (!iInPorts.empty())
    {
        iInPorts.Erase(&iInPorts.front());

    }

    iNodeEndOfDataReached = false;

    Cancel();
    if (iInterfaceState != EPVMFNodeCreated)
        iInterfaceState = EPVMFNodeIdle;

    if (iSampleParam)
    {
        OSCL_DELETE(iSampleParam);
        iSampleParam = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::GetCapability(PVMFNodeCapability& aNodeCapability)
{
    aNodeCapability.iCanSupportMultipleInputPorts = true;
    aNodeCapability.iCanSupportMultipleOutputPorts = false;
    aNodeCapability.iHasMaxNumberOfPorts = true;
    aNodeCapability.iMaxNumberOfPorts = PVMF_MP4FFCN_MAX_INPUT_PORT + PVMF_MP4FFCN_MAX_OUTPUT_PORT;
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_M4V);
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_H264_VIDEO_MP4);
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_ISO_AVC_SAMPLE_FORMAT);
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_H2631998);
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_H2632000);
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_AMR_IETF);
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_AMRWB_IETF);
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_3GPP_TIMEDTEXT);
    aNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_MPEG4_AUDIO);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFPortIter* PVMp4FFComposerNode::GetPorts(const PVMFPortFilter* aFilter)
{
    OSCL_UNUSED_ARG(aFilter);
    iInPorts.Reset();
    return &iInPorts;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMp4FFComposerNode::addRef()
{
    ++iExtensionRefCount;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMp4FFComposerNode::removeRef()
{
    if (iExtensionRefCount > 0)
        --iExtensionRefCount;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF bool PVMp4FFComposerNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    if (uuid == KPVMp4FFCNClipConfigUuid)
    {
        PVMp4FFCNClipConfigInterface* myInterface = OSCL_STATIC_CAST(PVMp4FFCNClipConfigInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
    }
    else if (uuid == KPVMp4FFCNTrackConfigUuid)
    {
        PVMp4FFCNTrackConfigInterface* myInterface = OSCL_STATIC_CAST(PVMp4FFCNTrackConfigInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
    }
    else if (uuid == PvmfComposerSizeAndDurationUuid)
    {
        PvmfComposerSizeAndDurationInterface* myInterface = OSCL_STATIC_CAST(PvmfComposerSizeAndDurationInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
    }
    else if (uuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        ++iExtensionRefCount;
    }
    else
    {
        iface = NULL;
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////
//            PVMp4FFCNClipConfigInterface routines
////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF uint16 PVMp4FFComposerNode::ConvertLangCode(const OSCL_String & aLang)
{
    int i = 0;
    char lang[LANG_CODE_SIZE] = {0};
    oscl_strncpy(lang, aLang.get_cstr(), LANG_CODE_SIZE);

    uint16 lang_code = ((((uint16)lang[i] - 0x60) << 10) | (((uint16)lang[i+1] - 0x60) << 5) | ((uint16)lang[i+2] - 0x60));

    return lang_code;
}

/////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetOutputFileName(const OSCL_wString& aFileName)
{
    if (iInterfaceState != EPVMFNodeIdle && iInterfaceState != EPVMFNodeInitialized)
        return false;

    iFileName = aFileName;
    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetOutputFileDescriptor(const OsclFileHandle* aFileHandle)
{
    if (iInterfaceState != EPVMFNodeIdle && iInterfaceState != EPVMFNodeInitialized)
        return false;

    iFileObject = OSCL_NEW(Oscl_File, (0, (OsclFileHandle *)aFileHandle));
    iFileObject->SetPVCacheSize(0);
    iFileObject->SetAsyncReadBufferSize(0);
    iFileObject->SetNativeBufferSize(0);
    iFileObject->SetLoggingEnable(false);
    iFileObject->SetSummaryStatsLoggingEnable(false);
    iFileObject->SetFileHandle((OsclFileHandle*)aFileHandle);

    //call open
    int32 retval = iFileObject->Open(_STRLIT_CHAR("dummy"),
                                     Oscl_File::MODE_READWRITE | Oscl_File::MODE_BINARY,
                                     iFs);

    if (retval == 0)
    {
        return PVMFSuccess;
    }
    return PVMFFailure;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetAuthoringMode(PVMp4FFCN_AuthoringMode aAuthoringMode)
{
    if (iInterfaceState != EPVMFNodeIdle && iInterfaceState != EPVMFNodeInitialized)
        return PVMFErrInvalidState;

    iAuthoringMode = aAuthoringMode;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetPresentationTimescale(uint32 aTimescale)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iPresentationTimescale = aTimescale;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetVersion(const OSCL_wString& aVersion, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iVersion.iDataString = aVersion;
    iVersion.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetTitle(const OSCL_wString& aTitle, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iTitle.iDataString = aTitle;
    iTitle.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetAuthor(const OSCL_wString& aAuthor, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iAuthor.iDataString = aAuthor;
    iAuthor.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetCopyright(const OSCL_wString& aCopyright, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iCopyright.iDataString = aCopyright;
    iCopyright.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetDescription(const OSCL_wString& aDescription, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iDescription.iDataString = aDescription;
    iDescription.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetRating(const OSCL_wString& aRating, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iRating.iDataString = aRating;
    iRating.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetCreationDate(const OSCL_wString& aCreationDate)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iCreationDate = aCreationDate;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::SetRealTimeAuthoring(const bool aRealTime)
{
    if (iInterfaceState != EPVMFNodeIdle && iInterfaceState != EPVMFNodeInitialized)
        return PVMFErrInvalidState;

    iRealTimeTS = aRealTime;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetAlbumInfo(const OSCL_wString& aAlbumTitle, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iAlbumTitle.iDataString = aAlbumTitle;
    iAlbumTitle.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}


////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetRecordingYear(uint16 aRecordingYear)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iRecordingYear = aRecordingYear;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetPerformer(const OSCL_wString& aPerformer, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iPerformer.iDataString = aPerformer;
    iPerformer.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetGenre(const OSCL_wString& aGenre, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iGenre.iDataString = aGenre;
    iGenre.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}
////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetClassification(const OSCL_wString& aClassificationInfo, uint32 aClassificationEntity, uint16 aClassificationTable, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iClassification.iDataString = aClassificationInfo;
    iClassification.iClassificationEntity = aClassificationEntity;
    iClassification.iClassificationTable = aClassificationTable;
    iClassification.iLangCode = ConvertLangCode(aLangCode);
    return PVMFSuccess;
}
////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetKeyWord(const OSCL_wString& aKeyWordInfo, const OSCL_String& aLangCode)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    PVMP4FFCN_KeyWord *KeyWord = NULL;

    uint16 langCode = ConvertLangCode(aLangCode);
    KeyWord = OSCL_NEW(PVMP4FFCN_KeyWord, (aKeyWordInfo, aKeyWordInfo.get_size(), langCode));

    iKeyWordVector.push_back(KeyWord);


    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetLocationInfo(PvmfAssetInfo3GPPLocationStruct& aLocation_info)
{
    if (iInterfaceState != EPVMFNodeIdle &&
            iInterfaceState != EPVMFNodeInitialized &&
            iInterfaceState != EPVMFNodePrepared)
        return PVMFErrInvalidState;

    iLocationInfo._location_name = NULL;
    uint32 size = oscl_strlen(aLocation_info._location_name);
    iLocationInfo._location_name = (oscl_wchar*)oscl_malloc(sizeof(oscl_wchar) * size + 10);
    oscl_strncpy(iLocationInfo._location_name, aLocation_info._location_name, size);
    iLocationInfo._location_name[size+1] = 0;

    iLocationInfo._astronomical_body = NULL;
    size = oscl_strlen(aLocation_info._astronomical_body);
    iLocationInfo._astronomical_body = (oscl_wchar*)oscl_malloc(sizeof(oscl_wchar) * size + 10);
    oscl_strncpy(iLocationInfo._astronomical_body, aLocation_info._astronomical_body, size);
    iLocationInfo._astronomical_body[size+1] = 0;

    iLocationInfo._additional_notes = NULL;
    size = oscl_strlen(aLocation_info._additional_notes);
    iLocationInfo._additional_notes = (oscl_wchar*)oscl_malloc(sizeof(oscl_wchar) * size + 10);
    oscl_strncpy(iLocationInfo._additional_notes, aLocation_info._additional_notes, size);
    iLocationInfo._additional_notes[size+1] = 0;

    iLocationInfo._role = aLocation_info._role;
    iLocationInfo._longitude = aLocation_info._longitude;
    iLocationInfo._latitude = aLocation_info._latitude;
    iLocationInfo._altitude = aLocation_info._altitude;
    iLocationInfo._langCode = ConvertLangCode(aLocation_info.Lang_code);

    return PVMFSuccess;
}


////////////////////////////////////////////////////////////////////////////
//                PVMp4FFCNTrackConfigInterface routines
////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetTrackReference(const PVMFPortInterface& aPort,
        const PVMFPortInterface& aReferencePort)
{
    if (iInterfaceState != EPVMFNodeInitialized)
        return PVMFErrInvalidState;

    int32 portIndex = -1;
    int32 refPortIndex = -1;
    PVMp4FFComposerPort* port = OSCL_REINTERPRET_CAST(PVMp4FFComposerPort*, &aPort);
    PVMp4FFComposerPort* refPort = OSCL_REINTERPRET_CAST(PVMp4FFComposerPort*, &aReferencePort);

    for (uint32 i = 0; i < iInPorts.size(); i++)
    {
        if (iInPorts[i] == port)
            portIndex = i;
        if (iInPorts[i] == refPort)
            refPortIndex = i;
    }

    if (portIndex > 0 && refPortIndex > 0)
    {
        iInPorts[portIndex]->SetReferencePort(iInPorts[refPortIndex]);
        return PVMFSuccess;
    }
    else
        return PVMFFailure;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetCodecSpecificInfo(const PVMFPortInterface& aPort,
        uint8* aInfo, int32 aSize)
{
    PVMFStatus status = PVMFFailure;

    if ((status == PVMFSuccess) &&
            (iInterfaceState == EPVMFNodeStarted))
    {
        PVMp4FFComposerPort* port = OSCL_STATIC_CAST(PVMp4FFComposerPort*, &aPort);
        iMpeg4File->setDecoderSpecificInfo(aInfo, aSize, port->GetTrackId());
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////
//          PvmfComposerSizeAndDurationInterface routines
////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetMaxFileSize(bool aEnable, uint32 aMaxFileSizeBytes)
{
    iMaxFileSizeEnabled = aEnable;
    if (iMaxFileSizeEnabled)
    {
        iMaxFileSize = aMaxFileSizeBytes;
    }
    else
    {
        iMaxFileSize = 0;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMp4FFComposerNode::GetMaxFileSizeConfig(bool& aEnable, uint32& aMaxFileSizeBytes)
{
    aEnable = iMaxFileSizeEnabled;
    aMaxFileSizeBytes = iMaxFileSize;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetMaxDuration(bool aEnable, uint32 aMaxDurationMilliseconds)
{
    iMaxDurationEnabled = aEnable;
    if (iMaxDurationEnabled)
    {
        iMaxTimeDuration = aMaxDurationMilliseconds;
    }
    else
    {
        iMaxTimeDuration = 0;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMp4FFComposerNode::GetMaxDurationConfig(bool& aEnable, uint32& aMaxDurationMilliseconds)
{
    aEnable = iMaxDurationEnabled;
    aMaxDurationMilliseconds = iMaxTimeDuration;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetFileSizeProgressReport(bool aEnable, uint32 aReportFrequency)
{
    iFileSizeReportEnabled = aEnable;
    if (iFileSizeReportEnabled)
    {
        iFileSizeReportFreq = aReportFrequency;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMp4FFComposerNode::GetFileSizeProgressReportConfig(bool& aEnable, uint32& aReportFrequency)
{
    aEnable = iFileSizeReportEnabled;
    aReportFrequency = iFileSizeReportFreq;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus PVMp4FFComposerNode::SetDurationProgressReport(bool aEnable, uint32 aReportFrequency)
{
    iDurationReportEnabled = aEnable;
    if (iDurationReportEnabled)
    {
        iDurationReportFreq = aReportFrequency;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void PVMp4FFComposerNode::GetDurationProgressReportConfig(bool& aEnable, uint32& aReportFrequency)
{
    aEnable = iDurationReportEnabled;
    aReportFrequency = iDurationReportFreq;
}

////////////////////////////////////////////////////////////////////////////
//            PVMFPortActivityHandler routines
////////////////////////////////////////////////////////////////////////////
void PVMp4FFComposerNode::HandlePortActivity(const PVMFPortActivity& aActivity)
{
    OSCL_UNUSED_ARG(aActivity);
    // Scheduling to process port activities are handled in the port itself
}

////////////////////////////////////////////////////////////////////////////
//                    OsclActiveObject routines
////////////////////////////////////////////////////////////////////////////
void PVMp4FFComposerNode::Run()
{
    LOG_STACK_TRACE((0, "PVMp4FFComposerNode::Run: iInterfaceState=%d", iInterfaceState));

    // Check InputCommandQueue, If command present process commands

    if (!iInputCommands.empty())
    {
        ProcessCommand();
    }

    LOG_STACK_TRACE((0, "PVMp4FFComposerNode::Run: Out. iInterfaceState=%d", iInterfaceState));
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoQueryInterface()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMp4FFComposerNode::DoQueryInterface"));

    PVUuid* uuid;
    PVInterface** ptr;
    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, ptr);

    if (queryInterface(*uuid, *ptr))
    {
        return PVMFSuccess;
    }
    else
    {
        return PVMFFailure;
    }
}


//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMp4FFComposerNode::DoRequestPort() In"));

    aPort = NULL;
    int32 tag;
    OSCL_String* portconfig;
    iCurrentCommand.PVMFNodeCommandBase::Parse(tag, portconfig);

    //validate the tag...
    switch (tag)
    {
        case PVMF_MP4FFCN_PORT_TYPE_SINK:
            if (iInPorts.size() >= PVMF_MP4FFCN_MAX_INPUT_PORT)
            {
                LOG_ERR((0, "PVMp4FFComposerNode::DoRequestPort: Error - Max number of input port already allocated"));
                return PVMFFailure;
            }
            break;

        default:
            //bad port tag
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMp4FFComposerNode::DoRequestPort: Error - Invalid port tag"));
            return PVMFFailure;
    }

    //Allocate a new port
    OsclAny *ptr = NULL;
    int32 err;
    OSCL_TRY(err,
             ptr = iInPorts.Allocate();
             if (!ptr)
             OSCL_LEAVE(OsclErrNoMemory);
            );

    OSCL_FIRST_CATCH_ANY(err,
                         PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                                         (0, "PVMp4FFComposerNode::DoRequestPort: Error - iInPorts Out of memory"));
                         return PVMFErrNoMemory;
                        );

    OSCL_StackString<20> portname;
    portname = "PVMP4ComposerIn";

    PVMp4FFComposerPort* port = OSCL_PLACEMENT_NEW(ptr, PVMp4FFComposerPort(tag, this, Priority(), portname.get_cstr()));

    // if format was provided in mimestring, set it now.
    if (portconfig)
    {
        PVMFFormatType format = portconfig->get_str();
        if (format == PVMF_MIME_3GPP_TIMEDTEXT ||
                format == PVMF_MIME_H264_VIDEO_MP4 ||
                format == PVMF_MIME_ISO_AVC_SAMPLE_FORMAT ||
                format == PVMF_MIME_M4V ||
                format == PVMF_MIME_H2631998 ||
                format == PVMF_MIME_H2632000 ||
                format == PVMF_MIME_AMR_IETF ||
                format == PVMF_MIME_AMRWB_IETF ||
                format == PVMF_MIME_ADIF ||
                format == PVMF_MIME_ADTS ||
                format == PVMF_MIME_MPEG4_AUDIO)
        {
            port->SetFormat(format);
        }
        else
        {
            return PVMFErrNotSupported;
        }
    }

    //Add the port to the port vector.
    OSCL_TRY(err, iInPorts.AddL(port););
    OSCL_FIRST_CATCH_ANY(err,
                         iInPorts.DestructAndDealloc(port);
                         return PVMFErrNoMemory;
                        );

    // Return the port pointer to the caller.
    aPort = port;
    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoReleasePort()
{
    //Find the port in the port vector
    PVMFPortInterface* p = NULL;

    for (uint32 i = 0; i < iInPorts.size(); i++)
    {
        iCurrentCommand.PVMFNodeCommandBase::Parse(p);

        PVMp4FFComposerPort* port = (PVMp4FFComposerPort*)p;

        PVMp4FFComposerPort** portPtr = iInPorts.FindByValue(port);
        if (portPtr)
        {
            //delete the port.
            iInPorts.Erase(portPtr);

#ifdef _TEST_AE_ERROR_HANDLING
            if (FAIL_NODE_CMD_RELEASE_PORT == iErrorNodeCmd)
            {
                return PVMFFailure;

            }
            else
            {
                return PVMFSuccess;
            }
#else
            return PVMFSuccess;
#endif

        }
        else
        {
            //port not found.
            return PVMFFailure;
        }

    }
    return PVMFFailure;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoInit()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMp4FFComposerNode::DoInitNode() In"));
    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoStart()
{

    if (EPVMFNodeStarted == iInterfaceState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMp4FFComposerNode::DoStart() already in Started state"));
        return PVMFSuccess;
    }

    PVMFStatus status = PVMFSuccess;
    uint32 i = 0;
#ifdef _TEST_AE_ERROR_HANDLING
    if (FAIL_NODE_CMD_START == iErrorNodeCmd)
    {
        iInterfaceState = EPVMFNodeError;
        return PVMFErrInvalidState;
    }
#endif
    if (EPVMFNodePrepared == iInterfaceState)
    {
        iPostfix = _STRLIT("00");
        iOutputPath = _STRLIT("");
        int32 pos = 0;
        for (pos = iFileName.get_size() - 1; pos >= 0; pos--)
        {
            if (iFileName[pos] == SLASH)
                break;
        }

        if (pos == -1)
        {
            iOutputPath = _STRLIT(".");
        }
        else
        {
            for (i = 0; i <= (uint32) pos; i++)
                iOutputPath += iFileName[i];
        }

        iFileType = 0;
        for (i = 0; i < iInPorts.size(); i++)
        {
            if (iInPorts[i]->GetFormat() == PVMF_MIME_H264_VIDEO_MP4 ||
                    iInPorts[i]->GetFormat() == PVMF_MIME_ISO_AVC_SAMPLE_FORMAT ||
                    iInPorts[i]->GetFormat() == PVMF_MIME_M4V ||
                    iInPorts[i]->GetFormat() == PVMF_MIME_H2631998 ||
                    iInPorts[i]->GetFormat() == PVMF_MIME_H2632000)
            {
                iFileType |= FILE_TYPE_VIDEO;
            }
            else if (iInPorts[i]->GetFormat() == PVMF_MIME_AMR_IETF ||
                     iInPorts[i]->GetFormat() == PVMF_MIME_AMRWB_IETF ||
                     iInPorts[i]->GetFormat() == PVMF_MIME_MPEG4_AUDIO)
            {
                iFileType |= FILE_TYPE_AUDIO;
            }
            else if (iInPorts[i]->GetFormat() == PVMF_MIME_3GPP_TIMEDTEXT)
            {
                iFileType |= FILE_TYPE_TIMED_TEXT;
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                (0, "PVMp4FFComposerNode::DoStart: Error - Unsupported format"));
                return PVMFErrNotSupported;
            }
        }

        if (iMpeg4File)
        {
            LOG_ERR((0, "PVMp4FFComposerNode::DoStart: Error - File format library already exists"));
            return PVMFFailure;
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMp4FFComposerNode::DoStart: Calling PVA_FF_IMpeg4File::createMP4File(%d,0x%x,%d)",
                         iFileType, &iFs, iAuthoringMode));
#ifdef _TEST_AE_ERROR_HANDLING //test case to fail mp4 file parser
        if (iErrorCreateComposer)
        {
            //to fail createMP4File()
            OSCL_wHeapString<OsclMemAllocator> ErrFileName;

            iMpeg4File = PVA_FF_IMpeg4File::createMP4File(iFileType, iOutputPath, iPostfix,
                         (void*) & iFs, iAuthoringMode, ErrFileName, iCacheSize);

        }
        else
        {
            if (iFileObject != NULL)
            {
                iMpeg4File = PVA_FF_IMpeg4File::createMP4File(iFileType, iAuthoringMode, iFileObject, iCacheSize);

            }
            else
            {

                iMpeg4File = PVA_FF_IMpeg4File::createMP4File(iFileType, iOutputPath, iPostfix,
                             (void*) & iFs, iAuthoringMode, iFileName, iCacheSize);

            }
        }
#else
        if (iFileObject != NULL)
        {
            iMpeg4File = PVA_FF_IMpeg4File::createMP4File(iFileType, iAuthoringMode, iFileObject, iCacheSize);

        }
        else
        {
            iMpeg4File = PVA_FF_IMpeg4File::createMP4File(iFileType, iOutputPath, iPostfix,
                         (void*) & iFs, iAuthoringMode, iFileName, iCacheSize);
        }
#endif
        if (!iMpeg4File)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                            (0, "PVMp4FFComposerNode::DoStart: Error - PVA_FF_IMpeg4File::createMP4File failed"));
            return PVMFFailure;
        }

        iMpeg4File->setPresentationTimescale(iPresentationTimescale);
        iMpeg4File->setVersion(iVersion.iDataString, iVersion.iLangCode);
        iMpeg4File->setTitle(iTitle.iDataString, iTitle.iLangCode);
        iMpeg4File->setAuthor(iAuthor.iDataString, iAuthor.iLangCode);
        iMpeg4File->setCopyright(iCopyright.iDataString, iCopyright.iLangCode);
        iMpeg4File->setDescription(iDescription.iDataString, iDescription.iLangCode);
        iMpeg4File->setRating(iRating.iDataString, iRating.iLangCode);
        if (iCreationDate.get_size() > 0)
        {
            iMpeg4File->setCreationDate(iCreationDate);
        }
        iMpeg4File->setMovieFragmentDuration(iMovieFragmentDuration);
        iMpeg4File->setAlbumInfo(iAlbumTitle.iDataString, iAlbumTitle.iLangCode);
        iMpeg4File->setRecordingYear(iRecordingYear);

        iMpeg4File->setPerformer(iPerformer.iDataString, iPerformer.iLangCode);
        iMpeg4File->setGenre(iGenre.iDataString, iGenre.iLangCode);
        iMpeg4File->setClassification(iClassification.iDataString, iClassification.iClassificationEntity, iClassification.iClassificationTable, iClassification.iLangCode);

        for (i = 0; i < iKeyWordVector.size() ; i++)
        {
            iMpeg4File->setKeyWord(iKeyWordVector[i]->iKeyWordSize, iKeyWordVector[i]->iData_String, iKeyWordVector[i]->iLang_Code);
        }

        iMpeg4File->setLocationInfo(&iLocationInfo);
        for (i = 0; i < iInPorts.size(); i++)
        {
            status = AddTrack(iInPorts[i]);
            if (status != PVMFSuccess)
            {
                return status;
            }
        }

        // Check for and set reference tracks after track IDs are assigned
        PVMp4FFComposerPort* refPort = NULL;
        for (i = 0; i < iInPorts.size(); i++)
        {
            refPort = OSCL_STATIC_CAST(PVMp4FFComposerPort*, iInPorts[i]->GetReferencePort());
            if (refPort)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "PVMp4FFComposerNode::DoStart: Calling addTrackReference(%d, %d)",
                                 iInPorts[i]->GetTrackId(), refPort->GetTrackId()));
                iMpeg4File->addTrackReference(iInPorts[i]->GetTrackId(), refPort->GetTrackId());
            }
        }

        iMpeg4File->prepareToEncode();

        iInitTSOffset = true;
        iTSOffset = 0;
    }
    else
    {
        //Node is in Paused state
        for (i = 0; i < iInPorts.size(); i++)
            ((PVMp4FFComposerPort*)iInPorts[i])->ProcessIncomingMsgReady();
    }
    return status;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::AddTrack(PVMp4FFComposerPort *aPort)
{
    PVA_FF_MP4_CODEC_TYPE codecType = PVA_FF_MP4_CODEC_TYPE_UNDEFINED;
    int32 mediaType = 0;
    int32 trackId = 0;
    PVMP4FFCNFormatSpecificConfig* config = aPort->GetFormatSpecificConfig();
    if (!config)
    {
        LOG_ERR((0, "PVMp4FFComposerNode::AddTrack: Error - GetFormatSpecificConfig failed"));
        return PVMFFailure;
    }

    if (aPort->GetFormat() == PVMF_MIME_3GPP_TIMEDTEXT)
    {
        codecType = PVA_FF_MP4_CODEC_TYPE_TIMED_TEXT;
        mediaType = MEDIA_TYPE_TEXT;
    }
    else if ((aPort->GetFormat() == PVMF_MIME_H264_VIDEO_MP4) ||
             (aPort->GetFormat() == PVMF_MIME_ISO_AVC_SAMPLE_FORMAT))
    {
        codecType = PVA_FF_MP4_CODEC_TYPE_AVC_VIDEO;
        mediaType = MEDIA_TYPE_VISUAL;
    }
    else if (aPort->GetFormat() == PVMF_MIME_M4V)
    {
        codecType = PVA_FF_MP4_CODEC_TYPE_MPEG4_VIDEO;
        mediaType = MEDIA_TYPE_VISUAL;
    }
    else if (aPort->GetFormat() == PVMF_MIME_H2631998 ||
             aPort->GetFormat() == PVMF_MIME_H2632000)
    {
        codecType = PVA_FF_MP4_CODEC_TYPE_BASELINE_H263_VIDEO;
        mediaType = MEDIA_TYPE_VISUAL;
    }
    else if (aPort->GetFormat() == PVMF_MIME_AMR_IETF)
    {
        codecType = PVA_FF_MP4_CODEC_TYPE_AMR_AUDIO;
        mediaType = MEDIA_TYPE_AUDIO;
    }
    else if (aPort->GetFormat() == PVMF_MIME_AMRWB_IETF)
    {
        codecType = PVA_FF_MP4_CODEC_TYPE_AMR_WB_AUDIO;
        mediaType = MEDIA_TYPE_AUDIO;
    }
    else if (aPort->GetFormat() ==  PVMF_MIME_MPEG4_AUDIO)
    {
        codecType = PVA_FF_MP4_CODEC_TYPE_AAC_AUDIO;
        mediaType = MEDIA_TYPE_AUDIO;
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "PVMp4FFComposerNode::AddTrack: Error - Unsupported format"));
        return PVMFFailure;
    }

    aPort->SetCodecType(codecType);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "PVMp4FFComposerNode::AddTrack: Calling PVA_FF_IMpeg4File::addTrack(0x%x,0x%x)",
                     mediaType, codecType));
#ifdef _TEST_AE_ERROR_HANDLING
    if (aPort->GetFormat() == iErrorAddTrack)
    {
        return PVMFFailure;
    }
#endif
    trackId = iMpeg4File->addTrack(mediaType, codecType);
#ifdef _TEST_AE_ERROR_HANDLING
    if (iErrorHandlingAddTrack)
    {
        trackId = 0;
    }
#endif
    if (trackId == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "PVMp4FFComposerNode::AddTrack: Error - PVA_FF_IMpeg4File::addTrack failed"));
        return PVMFFailure;
    }
    aPort->SetTrackId(trackId);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "PVMp4FFComposerNode::AddTrack: PVA_FF_IMpeg4File::addTrack success. trackID=%d", trackId));

    switch (mediaType)
    {
        case MEDIA_TYPE_AUDIO:
        {
            iMpeg4File->setTargetBitrate(trackId, config->iBitrate);
            iMpeg4File->setTimeScale(trackId, config->iTimescale);
            PVMP4FFComposerAudioEncodeParams audioParams;
            audioParams.numberOfChannels = config->iNumberOfChannels;
            audioParams.samplingRate = config->iSamplingRate;
            audioParams.bitsPerSample = config->iBitsPerSample;
            iMpeg4File->setAudioEncodeParams(trackId, audioParams);
            break;
        }

        case MEDIA_TYPE_VISUAL:
            switch (codecType)
            {
                case PVA_FF_MP4_CODEC_TYPE_BASELINE_H263_VIDEO:
                    iMpeg4File->setH263ProfileLevel(trackId, config->iH263Profile, config->iH263Level);
                    // Don't break here. Continue to set other video properties
                case PVA_FF_MP4_CODEC_TYPE_AVC_VIDEO:
                case PVA_FF_MP4_CODEC_TYPE_MPEG4_VIDEO:
                    iMpeg4File->setTargetBitrate(trackId, config->iBitrate, config->iMaxBitrate, config->iBufferSizeDB);
                    iMpeg4File->setTimeScale(trackId, config->iTimescale);
                    iMpeg4File->setVideoParams(trackId, (float)config->iFrameRate,
                                               (uint16)config->iIFrameInterval, config->iWidth, config->iHeight);
                    break;
                default:
                    break;
            }
            break;
        case MEDIA_TYPE_TEXT:
            iMpeg4File->setTargetBitrate(trackId, config->iBitrate);
            iMpeg4File->setTimeScale(trackId, config->iTimescale);
            break;

    }

    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoStop()
{
    if (!oDiagnosticsLogged)
    {
        LogDiagnostics();
    }
#ifdef _TEST_AE_ERROR_HANDLING
    if (FAIL_NODE_CMD_STOP == iErrorNodeCmd)
    {
        iInterfaceState = EPVMFNodeError;
        return PVMFErrInvalidState;
    }
#endif

    if (EPVMFNodePrepared == iInterfaceState)
        return PVMFSuccess;

    PVMFStatus status = PVMFSuccess;

#if ANDROID_FILEWRITER
    iFragmentWriter->flush();
#endif
    if (!iNodeEndOfDataReached)
    {
        WriteDecoderSpecificInfo();
        if (iSampleInTrack)
        {
            status = RenderToFile();
        }

        iSampleInTrack = false;
    }

    iNodeEndOfDataReached = false;
    for (uint32 ii = 0; ii < iInPorts.size(); ii++)
    {
        iInPorts[ii]->iEndOfDataReached = false;
    }

    return status;
}

//////////////////////////////////////////////////////////////////////////////////
void PVMp4FFComposerNode::WriteDecoderSpecificInfo()
{
    uint32 i, j;
    uint32 offset = 0;
    iConfigSize = 0;
    int32 trackId;
    for (j = 0; j < iInPorts.size(); j++)
    {
        PVMp4FFComposerPort* port = iInPorts[j];
        trackId = port->GetTrackId();
        PVMFFormatType portFmt = port->GetFormat();
        if ((portFmt == PVMF_MIME_H264_VIDEO_MP4) || (portFmt == PVMF_MIME_ISO_AVC_SAMPLE_FORMAT))
        {
            for (i = 0; i < memvector_sps.size(); i++)
            {
                iConfigSize += 2;//2 bytes for SPS_len
                iConfigSize += memvector_sps[i]->len;
            }

            for (i = 0; i < memvector_pps.size(); i++)
            {
                iConfigSize += 2;//2 bytes for PPS_len
                iConfigSize += memvector_pps[i]->len;
            }
            iConfigSize = iConfigSize + 2;//extra two bytes for nunSPS and NumPPS
            pConfig = (uint8*)(OSCL_MALLOC(sizeof(uint8) * iConfigSize));


            //currently we are ignoring NAL Length information
            oscl_memcpy((void*)(pConfig + offset), (const void*)&iNum_SPS_Set, 1);//Writing Number of SPS sets
            offset += 1;

            for (i = 0; i < memvector_sps.size(); i++)
            {
                oscl_memcpy((void*)(pConfig + offset), (const void*)&memvector_sps[i]->len, 2);//Writing length of SPS
                offset += 2;
                oscl_memcpy((void*)(pConfig + offset), memvector_sps[i]->ptr, memvector_sps[i]->len);
                offset = offset + memvector_sps[i]->len;
            }

            oscl_memcpy((void*)(pConfig + offset), (const void*)&iNum_PPS_Set, 1);//Writing Number of PPS sets
            offset += 1;

            for (i = 0; i < memvector_pps.size(); i++)
            {
                oscl_memcpy((void*)(pConfig + offset), (const void*)&memvector_pps[i]->len, 2);//Writing length of PPS
                offset += 2;//2 bytes for PPS Length
                oscl_memcpy((void*)(pConfig + offset), memvector_pps[i]->ptr, memvector_pps[i]->len);
                offset = offset + memvector_pps[i]->len;
            }
            iMpeg4File->setDecoderSpecificInfo(pConfig, iConfigSize, trackId);
        }
        else if (portFmt == PVMF_MIME_3GPP_TIMEDTEXT)
        {
            for (uint32 ii = 0; ii < textdecodervector.size(); ii++)
            {
                iMpeg4File->setTextDecoderSpecificInfo(textdecodervector[ii], trackId);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::RenderToFile()
{
    PVMFStatus status = PVMFSuccess;

    // Clear queued messages in ports
    uint32 i;
    for (i = 0; i < iInPorts.size(); i++)
        iInPorts[i]->ClearMsgQueues();
#ifdef _TEST_AE_ERROR_HANDLING //to fail renderToFile
    if (iErrorRenderToFile)
    {
        if (iMpeg4File)
        {
            PVA_FF_IMpeg4File::DestroyMP4FileObject(iMpeg4File);
            iMpeg4File = NULL;
        }
    }
#endif

#if ANDROID_FILEWRITER
    iFragmentWriter->flush();
#endif

    if (!iMpeg4File || !iMpeg4File->renderToFile(iFileName))
    {
        LOG_ERR((0, "PVMp4FFComposerNode::RenderToFile: Error - renderToFile failed"));
        ReportErrorEvent(PVMF_MP4FFCN_ERROR_FINALIZE_OUTPUT_FILE_FAILED);
        status = PVMFFailure;
    }
    else
    {
        LOGDATATRAFFIC((0, "PVMp4FFComposerNode::RenderToFile() Done"));
        // Delete file format library
        if (iMpeg4File)
        {
            PVA_FF_IMpeg4File::DestroyMP4FileObject(iMpeg4File);
            iMpeg4File = NULL;
        }

        // Change state
        SetState(EPVMFNodePrepared);
        status = PVMFSuccess;
    }

    if (PVMFSuccess == status)
    {
        iFileRendered = true;
    }
    return status;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoFlush()
{
    LOG_STACK_TRACE((0, "PVMp4FFComposerNode::DoFlush() iInterfaceState:%d", iInterfaceState));
#ifdef _TEST_AE_ERROR_HANDLING
    if (FAIL_NODE_CMD_FLUSH == iErrorNodeCmd)
    {
        iInterfaceState = EPVMFNodeError;
        return PVMFFailure;
    }
#endif

    uint32 i;
    bool msgPending;
    msgPending = false;

    for (i = 0; i < iInPorts.size(); i++)
    {
        if (iInPorts[i]->IncomingMsgQueueSize() > 0)
            msgPending = true;
        iInPorts[i]->SuspendInput();
        if (iInterfaceState != EPVMFNodeStarted)
        {
            // Port is in idle if node state is not started. Call ProcessIncomingMsgReady
            // to wake up port AO
            ((PVMp4FFComposerPort*)iInPorts[i])->ProcessIncomingMsgReady();
        }
    }

    if (!msgPending)
    {
        if (FlushComplete())
            return PVMFCmdCompleted;
    }

    // Flush remains pending until it gets complete in FlushComplete
    return PVMFPending;
}

////////////////////////////////////////////////////////////////////////////
bool PVMp4FFComposerNode::FlushComplete()
{
    LOG_STACK_TRACE((0, "PVMp4FFComposerNode::FlushComplete"));

    uint32 i = 0;
    PVMFStatus status = PVMFSuccess;

    // Flush is complete only when all queues of all ports are clear.
    // Other wise, just return from this method and wait for FlushComplete
    // from the remaining ports.
    for (i = 0; i < iInPorts.size(); i++)
    {
        if (iInPorts[i]->IncomingMsgQueueSize() > 0 ||
                iInPorts[i]->OutgoingMsgQueueSize() > 0)
        {
            return false;
        }
    }
#if ANDROID_FILEWRITER
    iFragmentWriter->flush();
#endif
    if (!iNodeEndOfDataReached)
    {
        WriteDecoderSpecificInfo();
        // Finalize output file
        if (iSampleInTrack)
        {
            status = RenderToFile();
        }

        iSampleInTrack = false;

        if (status != PVMFSuccess)
            LOG_ERR((0, "PVMp4FFComposerNode::FlushComplete: Error - RenderToFile failed"));
    }

    // Resume port input so the ports can be re-started.
    for (i = 0; i < iInPorts.size(); i++)
        iInPorts[i]->ResumeInput();

    SetState(EPVMFNodePrepared);

    if (IsFlushPending())
        CommandComplete(iCurrentCommand, status);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoPause()
{
#ifdef _TEST_AE_ERROR_HANDLING
    if (FAIL_NODE_CMD_PAUSE == iErrorNodeCmd)
    {
        iInterfaceState = EPVMFNodeError;
        return PVMFErrInvalidState;
    }
#endif

    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::DoReset()
{
    PVMFStatus status = PVMFFailure;
    if (!oDiagnosticsLogged)
    {
        LogDiagnostics();
    }
    if (IsAdded())
    {
        if (iSampleInTrack)
        {
            WriteDecoderSpecificInfo();
            status = RenderToFile();
            iSampleInTrack = false;
        }

        //delete all ports and notify observer.
        while (!iInPorts.empty())
            iInPorts.Erase(&iInPorts.front());

        //restore original port vector reserve.
        iInPorts.Reconstruct();
        iNodeEndOfDataReached = false;

        status = PVMFSuccess;
    }
    else
    {
        OSCL_LEAVE(OsclErrInvalidState);
    }

    return status;
}

//////////////////////////////////////////////////////////////////////////////////
//                  Port activity processing routines
//////////////////////////////////////////////////////////////////////////////////
bool PVMp4FFComposerNode::IsProcessIncomingMsgReady()
{
    if (iInterfaceState == EPVMFNodeStarted || IsFlushPending())
        return true;
    else
        return false;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::ProcessIncomingMsg(PVMFPortInterface* aPort)
{
    LOG_STACK_TRACE((0, "PVMp4FFComposerNode::ProcessIncomingMsg: aPort=0x%x", aPort));
    PVMFStatus status = PVMFSuccess;

    switch (aPort->GetPortTag())
    {
        case PVMF_MP4FFCN_PORT_TYPE_SINK:
        {
            PVMp4FFComposerPort* port = OSCL_REINTERPRET_CAST(PVMp4FFComposerPort*, aPort);
            if (!IsProcessIncomingMsgReady())
            {
                LOG_ERR((0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - Not ready."));
                return PVMFErrBusy;
            }

            PVMFSharedMediaMsgPtr msg;
            status = port->DequeueIncomingMsg(msg);
            if (status != PVMFSuccess)
            {
                LOG_ERR((0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - DequeueIncomingMsg failed"));
                return status;
            }
            if (msg->getFormatID() == PVMF_MEDIA_CMD_EOS_FORMAT_ID)
            {
                LOGDATATRAFFIC((0, "PVMp4FFComposerNode::ProcessIncomingMsg: EOS Recvd - TrackID=%d, StreamID=%d, TS=%d, Mime=%s",
                                port->GetTrackId(), msg->getStreamID(), msg->getTimestamp(), port->GetMimeType().get_cstr()));

                port->iEndOfDataReached = true;
                //check if EOS has been received on all connected ports.
                uint32 ii = 0;
                iNodeEndOfDataReached = true;
                for (ii = 0; ii < iInPorts.size(); ii++)
                {
                    if (!iInPorts[ii]->iEndOfDataReached)
                    {
                        iNodeEndOfDataReached = false;
                    }
                }

                if (iNodeEndOfDataReached)
                {
                    //Close the file since EOS is received on every connected port
                    WriteDecoderSpecificInfo();
                    if (iSampleInTrack)
                    {
                        status = RenderToFile();
                        iSampleInTrack = false;
                    }

                    //report EOS info to engine
                    ReportInfoEvent(PVMF_COMPOSER_EOS_REACHED);
                }

                //since we do not have data to process, we can safely break here.
                break;
            }
            PVMFSharedMediaDataPtr mediaDataPtr;
            convertToPVMFMediaData(mediaDataPtr, msg);

            int32 trackId = port->GetTrackId();
            if ((mediaDataPtr->getSeqNum() == 0) && (port->GetFormat() == PVMF_MIME_M4V))
            {
                // Set VOL Header
                OsclRefCounterMemFrag volHeader;
                if (mediaDataPtr->getFormatSpecificInfo(volHeader) == false ||
                        volHeader.getMemFragSize() == 0)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - VOL Header not available"));
                    return PVMFFailure;
                }

                iMpeg4File->setDecoderSpecificInfo((uint8*)volHeader.getMemFragPtr(),
                                                   (int32)volHeader.getMemFragSize(), trackId);
            }
            if ((mediaDataPtr->getSeqNum() == 0) && (port->GetFormat() == PVMF_MIME_MPEG4_AUDIO))
            {
                // Set AAC Config
                OsclRefCounterMemFrag decSpecInfo;
                if (mediaDataPtr->getFormatSpecificInfo(decSpecInfo) == false ||
                        decSpecInfo.getMemFragSize() == 0)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - Decoder Specific not available"));
                    return PVMFFailure;
                }

                iMpeg4File->setDecoderSpecificInfo((uint8*)decSpecInfo.getMemFragPtr(),
                                                   (int32)decSpecInfo.getMemFragSize(), trackId);
            }

            if (((port->GetFormat() == PVMF_MIME_AMR_IETF) ||
                    (port->GetFormat() == PVMF_MIME_AMRWB_IETF)) && mediaDataPtr->getErrorsFlag())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_NOTICE,
                                (0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error flag set for AMR!"));
                return PVMFSuccess;
            }

            // Retrieve data from incoming queue
            uint32 i;
            uint32 sample_size = 0;
            OsclRefCounterMemFrag memFrag;
            uint32 numFrags = mediaDataPtr->getNumFragments();
            uint32 timestamp = mediaDataPtr->getTimestamp();

            Oscl_Vector<OsclMemoryFragment, OsclMemAllocator> pFrame; //vector to store the nals in the particular case of AVC
            if (port->GetFormat() == PVMF_MIME_ISO_AVC_SAMPLE_FORMAT)
            {
                //we need to break up the sample into NALs before passing to MP4 composer library
                //we would like to keep the interface to MP4 CN same be it PVMF_MIME_ISO_AVC_SAMPLE_FORMAT
                //or PVMF_MIME_H264_VIDEO_MP4.
                //in case of PVMF_MIME_ISO_AVC_SAMPLE_FORMAT, there cannot be more than one mediafrag
                //we only allow one complete frame per media msg
                status = BreakUpAVCSampleIntoNALs(mediaDataPtr, port, pFrame);
                if (status == PVMFErrCorrupt)
                {
                    //ignore corrupt samples
                    ReportInfoEvent(PVMF_MP4FFCN_ERROR_CORRUPT_AVC_SAMPLE, (OsclAny*)aPort);
                    return PVMFSuccess;
                }
                else if (status != PVMFSuccess)
                {
                    LOG_ERR((0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - BreakUpAVCSampleIntoNALs Failed"));
                    return status;
                }
            }
            else
            {
                for (i = 0; (i < numFrags) && status == PVMFSuccess; i++)
                {
                    if (!mediaDataPtr->getMediaFragment(i, memFrag))
                    {
                        LOG_ERR((0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - getMediaFragment Failed"));
                        return PVMFFailure;
                    }
                    else
                    {
                        OsclMemoryFragment memfragment;
                        memfragment.len = memFrag.getMemFragSize();
                        memfragment.ptr = memFrag.getMemFragPtr();
                        pFrame.push_back(memfragment);
                    }
                }
            }
            iSyncSample = 0;
            if (mediaDataPtr->getMarkerInfo()&PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT)
            {
                iSyncSample = 1;
            }
            //sometimes encoders / compressed media input components do not set this bit
            //so check anyway
            else if (IsRandomAccessPoint(port, pFrame))
            {
                iSyncSample = 1;
            }

            //get total sample size for diagnostics
            for (i = 0; i < pFrame.size(); i++)
            {
                sample_size += pFrame[i].len;
            }


            uint32 currticks = OsclTickCount::TickCount();
            uint32 StartTime = OsclTickCount::TicksToMsec(currticks);
#if ANDROID_FILEWRITER
            if (!iMaxReachedEvent)
            {
                // TODO: We are passing port and port->GetFormat(), should pass port only.
                status = iFragmentWriter->enqueueMemFragToTrack(
                             pFrame, port->GetFormat(), mediaDataPtr->getSeqNum(),
                             timestamp, trackId, (PVMp4FFComposerPort*)aPort, mediaDataPtr->getDuration());
            }
            else if (!iMaxReachedReported)
            {
                iMaxReachedReported = true;
                ReportInfoEvent(static_cast<PVMFComposerSizeAndDurationEvent>(iMaxReachedEvent), NULL);
                status = PVMFSuccess;
            }
#else
            status = AddSampleToTrack(pFrame,
                                      port->GetFormat(),
                                      mediaDataPtr->getSeqNum(),
                                      timestamp,
                                      trackId,
                                      (PVMp4FFComposerPort*)aPort,
                                      mediaDataPtr->getDuration());
#endif
            currticks = OsclTickCount::TickCount();
            uint32 EndTime = OsclTickCount::TicksToMsec(currticks);
            uint32 delta = EndTime - StartTime;

            ((PVMp4FFComposerPort*)aPort)->UpdateDiagnostics(delta, sample_size);

            if (status == PVMFFailure)
            {
                LOGDATATRAFFIC((0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - AddSampleToTrack Failed"));
                LOG_ERR((0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - AddSampleToTrack Failed"));
                ReportErrorEvent(PVMF_MP4FFCN_ERROR_ADD_SAMPLE_TO_TRACK_FAILED, (OsclAny*)aPort);
            }
        }
        break;

        default:
            LOG_ERR((0, "PVMp4FFComposerNode::ProcessIncomingMsg: Error - Invalid port tag"));
            ReportErrorEvent(PVMF_MP4FFCN_ERROR_ADD_SAMPLE_TO_TRACK_FAILED, (OsclAny*)aPort);
            status = PVMFFailure;
            break;
    }

    return status;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::AddSampleToTrack(Oscl_Vector<OsclMemoryFragment, OsclMemAllocator> aFrame,
        PVMFFormatType aFormat,
        uint32 aSeqNum,
        uint32& aTimestamp,
        int32 aTrackId,
        PVMp4FFComposerPort *aPort,
        uint32 aSampleDuration)
{
    //validate data
    PVMFStatus status = PVMFSuccess;
    uint32 inputSize = 0;
    uint32 ii = 0;
    for (ii = 0; ii < aFrame.size(); ii++)
    {
        if (aFrame[ii].ptr == NULL || aFrame[ii].len == 0)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                            (0, "PVMp4FFComposerNode::AddSampleToTrack: Error - Invalid data or data size"));
            return PVMFFailure;
        }
        inputSize += aFrame[ii].len;
    }

    LOGDATATRAFFIC((0, "PVMp4FFComposerNode::AddSampleToTrack: Fmt=%s, TS=%d, Size=%d, Seq=%d, TrackID=%d",
                    aFormat.getMIMEStrPtr(), aTimestamp, inputSize, aSeqNum, aTrackId));

    //Check for max duration first
    uint32 timeScale = 0;
    PVMP4FFCNFormatSpecificConfig* config = aPort->GetFormatSpecificConfig();
    if (config)
    {
        timeScale = config->iTimescale;
    }

    status = CheckMaxDuration(aTimestamp);
    if (status == PVMFFailure)
    {
        LOGDATATRAFFIC((0, "PVMp4FFComposerNode::AddSampleToTrack: Error - CheckMaxDuration failed"));
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "PVMp4FFComposerNode::AddSampleToTrack: Error - CheckMaxDuration failed"));
        return status;
    }
    else if (status == PVMFSuccess)
    {
        LOGDATATRAFFIC((0, "PVMp4FFComposerNode::AddSampleToTrack: Maxmimum duration reached"));
        return status;
    }

    //Check for max size next - use the size of the complete media msg (including all memfrags)
    status = CheckMaxFileSize(inputSize);
    if (status == PVMFFailure)
    {
        LOGDATATRAFFIC((0, "PVMp4FFComposerNode::AddSampleToTrack: Error - CheckMaxFileSize failed"));
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                        (0, "PVMp4FFComposerNode::AddSampleToTrack: Error - CheckMaxFileSize failed"));
        return status;
    }
    else if (status == PVMFSuccess)
    {
        LOGDATATRAFFIC((0, "PVMp4FFComposerNode::AddSampleToTrack: Maxmimum file size reached"));
        return status;
    }

    //used by 2-way record to file feature
    if (iRealTimeTS)
    {
        if (iInitTSOffset && (inputSize > 0))
        {
            iTSOffset = aTimestamp;
            iInitTSOffset = false;
        }
        aTimestamp = aTimestamp - iTSOffset;
    }

    uint32 i = 0;
    uint8 flags = 0;
    if (aFormat == PVMF_MIME_3GPP_TIMEDTEXT)
    {
        int32 index = 0;
        GetTextSDIndex(aSeqNum, index);
        if (index >= 0)
        {
            iSampleParam->_sampleDuration = aSampleDuration;
            iSampleParam->_flags = flags;
            iSampleParam->_timeStamp = aTimestamp;
            iSampleParam->_fragmentList = aFrame;
            iSampleParam->_index = index;

            if (!iMpeg4File->addTextSampleToTrack(aTrackId, iSampleParam))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                (0, "PVMp4FFComposerNode::AddSampleToTrack: Error - addTextSampleToTrack for Timed Text failed"));
                return PVMFFailure;
            }
        }
    }
    else if (aFormat == PVMF_MIME_H264_VIDEO_MP4 ||
             aFormat == PVMF_MIME_ISO_AVC_SAMPLE_FORMAT ||
             aFormat == PVMF_MIME_M4V ||
             aFormat == PVMF_MIME_H2631998 ||
             aFormat == PVMF_MIME_H2632000)
    {
        //used by 2-way record to file feature
        if (iRealTimeTS)
        {
            if (aTimestamp <= aPort->GetLastTS())
            {
                aTimestamp = aPort->GetLastTS() + 1;
            }
            aPort->SetLastTS(aTimestamp);
        }
        uint8 codingType = CODING_TYPE_P;
        //iSyncSample is obtained from the marker info
        //to identify the I Frame
        if (iSyncSample)
        {
            codingType = CODING_TYPE_I;
        }
        // Format: mtb (1) | layer_id (3) | coding_type (2) | ref_select_code (2)
        // flags |= ((stream->iHintTrack.MTB & 0x01) << 7); not used any more
        // flags |= ((stream->iHintTrack.LayerID & 0x07) << 4); not used any more
        // flags |= (stream->iHintTrack.RefSelCode & 0x03); not used any more
        flags |= ((codingType & 0x03) << 2);

        iSampleParam->_fragmentList = aFrame;
        iSampleParam->_timeStamp = aTimestamp;
        iSampleParam->_flags = flags;

        //add the sample
        if (!iMpeg4File->addSampleToTrack(aTrackId, iSampleParam))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                            (0, "PVMp4FFComposerNode::AddSampleToTrack: Error - addSampleToTrack failed"));
            return PVMFFailure;
        }
    }
    else if ((aFormat == PVMF_MIME_AMR_IETF) ||
             (aFormat == PVMF_MIME_AMRWB_IETF))
    {
        //used by 2-way record to file feature
        if (iRealTimeTS)
        {
            if (((int32) aTimestamp - (int32) aPort->GetLastTS()) < 20)
            {
                aTimestamp = aPort->GetLastTS() + 20;
            }
            aPort->SetLastTS(aTimestamp);
        }
        uint32 bytesProcessed = 0;
        uint32 frameSize = 0;
        Oscl_Vector<OsclMemoryFragment, OsclMemAllocator> amrfrags;
        uint32 size = 0;
        uint8* data = NULL;
        for (i = 0; i < aFrame.size(); i++)
        {
            bytesProcessed = 0;
            size = aFrame[i].len;
            data = OSCL_REINTERPRET_CAST(uint8*, aFrame[i].ptr);
            // Parse audio data and add one 20ms frame to track at a time
            while (bytesProcessed < size)
            {
                // Update clock converter
                iClockConverter.set_timescale(timeScale);
                iClockConverter.set_clock_other_timescale(aTimestamp, 1000);

                // Check max file size
                int32 frSize = GetIETFFrameSize(data[0], aPort->GetCodecType());
                if (frSize == -1)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMp4FFComposerNode::AddSampleToTrack: Error - Frame Type Not Supported - Skipping"));
                    LOGDATATRAFFIC((0, "PVMp4FFComposerNode::AddSampleToTrack - Invalid Frame: TrackID=%d, Byte=0x%x, Mime=%s",
                                    aTrackId, data[0], aPort->GetMimeType().get_cstr()));
                    return PVMFFailure;
                }
                frameSize = (uint32)frSize;

                OsclMemoryFragment amr_memfrag;
                amr_memfrag.len = frameSize;
                amr_memfrag.ptr = data;
                amrfrags.push_back(amr_memfrag);
                uint32 amrts = iClockConverter.get_current_timestamp();

                LOGDATATRAFFIC((0, "PVMp4FFComposerNode::AddSampleToTrack: TrackID=%d, Size=%d, TS=%d, Flags=%d, Mime=%s",
                                aTrackId, frameSize, amrts, flags, aPort->GetMimeType().get_cstr()));

                iSampleParam->_fragmentList = amrfrags;
                iSampleParam->_timeStamp = amrts;
                iSampleParam->_flags = flags;

                if (!iMpeg4File->addSampleToTrack(aTrackId, iSampleParam))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMp4FFComposerNode::AddSampleToTrack: Error - addSampleToTrack failed"));
                    return PVMFFailure;
                }
                iSampleInTrack = true;
                data += frameSize;
                bytesProcessed += frameSize;
                aTimestamp += 20;
                amrfrags.clear();
            }
        }
        if (iRealTimeTS)
        {
            aPort->SetLastTS(aTimestamp - 20);
        }
    }
    else if (aFormat == PVMF_MIME_MPEG4_AUDIO)
    {
        //used by 2-way record to file feature
        if (iRealTimeTS)
        {
            if (aTimestamp <= aPort->GetLastTS())
            {
                aTimestamp = aPort->GetLastTS() + 1;
            }
            aPort->SetLastTS(aTimestamp);
        }

        iClockConverter.set_timescale(timeScale);
        iClockConverter.set_clock_other_timescale(aTimestamp, 1000);
        uint32 amrts = iClockConverter.get_current_timestamp();

        iSampleParam->_fragmentList = aFrame;
        iSampleParam->_timeStamp = amrts;
        iSampleParam->_flags = flags;

        if (!iMpeg4File->addSampleToTrack(aTrackId, iSampleParam))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR,
                            (0, "PVMp4FFComposerNode::AddSampleToTrack: Error - addSampleToTrack failed"));
            return PVMFFailure;
        }
    }
#ifdef _TEST_AE_ERROR_HANDLING
    if (1 == iErrorAddSample)
    {
        if (iTestFileSize > iFileSize) //iTestFileSize set in sendProgressReport()
        {
            return PVMFFailure; //Just to trigger error handling
        }
    }
    else if (2 == iErrorAddSample)
    {
        if (aTimestamp > iFileDuration)
        {
            return PVMFFailure; //Just to trigger error handling
        }
    }
    else if (iErrorHandlingAddMemFrag == true)
    {
        return PVMFFailure; //Just to trigger error handling
    }
#endif
    iSampleInTrack = true;
    // Send progress report after sample is successfully added
    SendProgressReport(aTimestamp);
    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
int32 PVMp4FFComposerNode::GetIETFFrameSize(uint8 aFrameType,
        int32 aCodecType)
{
    uint8 frameType = (uint8)(aFrameType >> 3) & 0x0f;
    if ((uint32)aCodecType == PVA_FF_MP4_CODEC_TYPE_AMR_AUDIO)
    {
        // Find frame size for each frame type
        switch (frameType)
        {
            case 0: // AMR 4.75 Kbps
                return 13;
            case 1: // AMR 5.15 Kbps
                return 14;
            case 2: // AMR 5.90 Kbps
                return 16;
            case 3: // AMR 6.70 Kbps
                return 18;
            case 4: // AMR 7.40 Kbps
                return 20;
            case 5: // AMR 7.95 Kbps
                return 21;
            case 6: // AMR 10.2 Kbps
                return 27;
            case 7: // AMR 12.2 Kbps
                return 32;
            case 8: // AMR Frame SID
                return 6;
            case 9: // AMR Frame GSM EFR SID
                return 7;
            case 10:// AMR Frame TDMA EFR SID
            case 11:// AMR Frame PDC EFR SID
                return 6;
            case 15: // AMR Frame No Data
                return 1;
            default: // Error - For Future Use
                return -1;
        }
    }
    else if ((uint32)aCodecType == PVA_FF_MP4_CODEC_TYPE_AMR_WB_AUDIO)
    {
        // Find frame size for each frame type
        switch (frameType)
        {
            case 0: // AMR-WB 6.60 Kbps
                return 18;
            case 1: // AMR-WB 8.85 Kbps
                return 24;
            case 2: // AMR-WB 12.65 Kbps
                return 33;
            case 3: // AMR-WB 14.25 Kbps
                return 37;
            case 4: // AMR-WB 15.85 Kbps
                return 41;
            case 5: // AMR-WB 18.25 Kbps
                return 47;
            case 6: // AMR-WB 19.85 Kbps
                return 51;
            case 7: // AMR-WB 23.05 Kbps
                return 59;
            case 8: // AMR-WB 23.85 Kbps
                return 61;
            case 9: // AMR-WB SID Frame
                return 6;
            case 10: //Reserved
            case 11: //Reserved
            case 12: //Reserved
            case 13: //Reserved
                return -1;
            case 14: // AMR-WB Frame Lost
            case 15: // AMR-WB Frame No Data
                return 1;
            default: // Error - For Future Use
                return -1;
        }
    }
    return -1;
}

//////////////////////////////////////////////////////////////////////////////////
//                 Progress and max size / duration routines
//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::SendProgressReport(uint32 aTimestamp)
{
    if (iDurationReportEnabled &&
            aTimestamp >= iNextDurationReport)
    {
        iNextDurationReport = aTimestamp - (aTimestamp % iDurationReportFreq) + iDurationReportFreq;
        ReportInfoEvent(PVMF_COMPOSER_DURATION_PROGRESS, (OsclAny*)aTimestamp);
    }
    else if (iFileSizeReportEnabled)
    {
        uint32 metaDataSize = 0;
        uint32 mediaDataSize = 0;
        uint32 fileSize = 0;

        iMpeg4File->getTargetFileSize(metaDataSize, mediaDataSize);
        fileSize = metaDataSize + mediaDataSize;

        if (fileSize >= iNextFileSizeReport)
        {
            iNextFileSizeReport = fileSize - (fileSize % iFileSizeReportFreq) + iFileSizeReportFreq;
            ReportInfoEvent(PVMF_COMPOSER_FILESIZE_PROGRESS, (OsclAny*)fileSize);
        }
#ifdef _TEST_AE_ERROR_HANDLING
        iTestFileSize = fileSize; //iTestTimeStamp to fail the addSampleTrack() once a particulare time duration is reached as specified in testapp.
#endif
    }

    return PVMFSuccess;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::CheckMaxFileSize(uint32 aFrameSize)
{
    if (iMaxFileSizeEnabled)
    {
        uint32 metaDataSize = 0;
        uint32 mediaDataSize = 0;
        iMpeg4File->getTargetFileSize(metaDataSize, mediaDataSize);

        if ((metaDataSize + mediaDataSize + aFrameSize) >= iMaxFileSize)
        {
#if ANDROID_FILEWRITER
            // This code is executed on the fragment writer thread, we
            // don't want to call RenderToFile since it will call
            // flush() on the writer from this very same
            // thread. Instead, we use a marker to report an event to
            // the author node next time a new fragment is processed.
            iMaxReachedEvent = PVMF_COMPOSER_MAXFILESIZE_REACHED;
#else
            // Finalized output file
            if (iSampleInTrack)
            {
                WriteDecoderSpecificInfo();
                iSampleInTrack = false;
                if (RenderToFile() != PVMFSuccess)
                    return PVMFFailure;
            }

            ReportInfoEvent(PVMF_COMPOSER_MAXFILESIZE_REACHED, NULL);
            return PVMFSuccess;
#endif
        }

        return PVMFPending;
    }

    return PVMFErrNotSupported;
}

//////////////////////////////////////////////////////////////////////////////////
PVMFStatus PVMp4FFComposerNode::CheckMaxDuration(uint32 aTimestamp)
{
    //if(!iInfoObserver)
    //  return PVMFFailure;
    if (iMaxDurationEnabled)
    {
        if (aTimestamp >= iMaxTimeDuration)
        {
#if ANDROID_FILEWRITER
            // This code is executed on the fragment writer thread, we
            // don't want to call RenderToFile since it will call
            // flush() on the writer from this very same
            // thread. Instead, we use a marker to report an event to
            // the author node next time a new fragment is processed.
            iMaxReachedEvent = PVMF_COMPOSER_MAXDURATION_REACHED;
#else
            // Finalize output file
            if (iSampleInTrack)
            {
                iSampleInTrack = false;
                WriteDecoderSpecificInfo();
                if (RenderToFile() != PVMFSuccess)
                    return PVMFFailure;
            }


            ReportInfoEvent(PVMF_COMPOSER_MAXDURATION_REACHED, NULL);
            return PVMFSuccess;
#endif
        }

        return PVMFPending;
    }

    return PVMFErrNotSupported;
}

////////////////////////////////////////////////////////////////////////////
//                   Event reporting routines.
////////////////////////////////////////////////////////////////////////////
void PVMp4FFComposerNode::ReportErrorEvent(PvmfMp4FFCNError aErrorEvent, OsclAny* aEventData)
{
    LOG_ERR((0, "PVMp4FFComposerNode:ReportErrorEvent: aEventType=%d, aEventData=0x%x", aErrorEvent, aEventData));
    switch (aErrorEvent)
    {
        case PVMF_MP4FFCN_ERROR_FINALIZE_OUTPUT_FILE_FAILED:
        case PVMF_MP4FFCN_ERROR_ADD_SAMPLE_TO_TRACK_FAILED:
            PVMFNodeInterface::ReportErrorEvent(PVMFErrResourceConfiguration, aEventData);
            break;
        default:
            PVMFNodeInterface::ReportErrorEvent(PVMFFailure, aEventData);
            break;
    }
}

void PVMp4FFComposerNode::LogDiagnostics()
{
    oDiagnosticsLogged = true;
    for (uint32 i = 0; i < iInPorts.size(); i++)
    {
        iInPorts[i]->LogDiagnostics(iDiagnosticsLogger);
    }
}

bool PVMp4FFComposerNode::GetAVCNALLength(OsclBinIStreamBigEndian& stream, uint32& lengthSize, int32& len)
{
    len = 0;
    if (lengthSize == 1)
    {
        uint8 len8 = 0;
        stream >> len8;
        len = (int32)(len8);
        return true;
    }
    else if (lengthSize == 2)
    {
        uint16 len16 = 0;
        stream >> len16;
        len = (int32)(len16);
        return true;
    }
    else if (lengthSize == 4)
    {
        stream >> len;
        return true;
    }
    return false;
}

PVMFStatus PVMp4FFComposerNode::BreakUpAVCSampleIntoNALs(PVMFSharedMediaDataPtr& aMediaDataPtr,
        PVMp4FFComposerPort* aPort,
        Oscl_Vector<OsclMemoryFragment, OsclMemAllocator>& aMemFragVec)
{
    OsclRefCounterMemFrag memFragIn;
    if (!aMediaDataPtr->getMediaFragment(0, memFragIn))
    {
        LOG_ERR((0, "PVMp4FFComposerNode::BreakUpAVCSampleIntoNALs: Error - getMediaFragment Failed"));
        return PVMFFailure;
    }
    else
    {
        if (aPort->iFormatSpecificConfig.iNALLenSize == 0)
        {
            LOG_ERR((0, "PVMp4FFComposerNode::BreakUpAVCSampleIntoNALs: Error - Invalid iNALLenSize"));
            return PVMFFailure;
        }
    }
    uint8* sample = (uint8*)(memFragIn.getMemFrag().ptr);
    int32 samplesize = (int32)(memFragIn.getMemFrag().len);
    uint32 nallengthsize = aPort->iFormatSpecificConfig.iNALLenSize;
    OsclBinIStreamBigEndian sampleStream;
    sampleStream.Attach(memFragIn.getMemFrag().ptr, memFragIn.getMemFrag().len);
    int32 numNAL = 0;
    while (samplesize > 0)
    {
        int32 nallen = 0;
        if (GetAVCNALLength(sampleStream, nallengthsize, nallen))
        {
            sample += nallengthsize;
            samplesize -= nallengthsize;

            if ((nallen < 0) || (nallen > samplesize))
            {
                LOG_ERR((0, "PVMp4FFComposerNode::BreakUpAVCSampleIntoNALs - Invalid NAL Size"));
                //ignore corrupt samples / nals
                return PVMFErrCorrupt;
            }

            if (nallen > 0)
            {
                OsclMemoryFragment memFrag;
                memFrag.ptr = sample;
                memFrag.len = nallen;
                aMemFragVec.push_back(memFrag);
                LOGDATATRAFFIC((0, "PVMp4FFComposerNode::BreakUpAVCSampleIntoNALs: TrackID=%d, Mime=%s, Seq=%d, TS=%d, SS-NALSize=%d, NALSize=%d, NALNum=%d",
                                aPort->GetTrackId(), aPort->GetMimeType().get_cstr(),
                                aMediaDataPtr->getSeqNum(), aMediaDataPtr->getTimestamp(),
                                samplesize - nallen, nallen, numNAL));
                sampleStream.seekFromCurrentPosition(nallen);
                numNAL++;
            }
            sample += nallen;
            samplesize -= nallen;
        }
        else
        {
            LOG_ERR((0, "PVMp4FFComposerNode::BreakUpAVCSampleIntoNALs - GetAVCNALLength Failed"));
            return PVMFErrCorrupt;
        }
    }
    if (aMemFragVec.size() == 0)
    {
        LOG_ERR((0, "PVMp4FFComposerNode::BreakUpAVCSampleIntoNALs - No NALs In Frame"));
        return PVMFErrCorrupt;
    }
    return PVMFSuccess;
}

bool PVMp4FFComposerNode::IsRandomAccessPoint(PVMp4FFComposerPort* aPort,
        Oscl_Vector <OsclMemoryFragment, OsclMemAllocator>& aList)
{
    if ((aPort->GetFormat() == PVMF_MIME_H264_VIDEO_MP4) ||
            (aPort->GetFormat() == PVMF_MIME_ISO_AVC_SAMPLE_FORMAT))
    {
        return (IsAVC_IDR_NAL(aList));
    }
    else if (aPort->GetFormat() == PVMF_MIME_M4V)
    {
        return (IsMPEG4KeyFrame(aList));
    }
    else if ((aPort->GetFormat() == PVMF_MIME_H2631998) ||
             (aPort->GetFormat() == PVMF_MIME_H2632000))
    {
        return (IsH263KeyFrame(aList));
    }
    return false;
}

bool PVMp4FFComposerNode::IsAVC_IDR_NAL(Oscl_Vector <OsclMemoryFragment, OsclMemAllocator>& aList)
{
    bool oRet = false;
    //do not delete these commented lines
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_SLICE = 1;     /* non-IDR non-data partition */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_DPA = 2;       /* data partition A */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_DPB = 3;       /* data partition B */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_DPC = 4;       /* data partition C */
    uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_IDR = 5;       /* IDR NAL */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_SEI = 6;       /* supplemental enhancement info */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_SPS = 7;       /* sequence parameter set */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_PPS = 8;       /* picture parameter set */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_AUD = 9;       /* access unit delimiter */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_EOSEQ = 10;    /* end of sequence */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_EOSTREAM = 11; /* end of stream */
    //uint8 MP4_FF_COMPOSER_UT_AVC_NALTYPE_FILL = 12;     /* filler data */
    //some clips tend to have SEI NALs at the start of a frame. So best to
    //check all NALs in a frame.
    for (uint32 i = 0; i < aList.size(); i++)
    {
        if (aList[i].len > 0)
        {
            uint8* buf = (uint8*)(aList[i].ptr);
            uint8 nal_type = (buf[0] & 0x1F);
            if (nal_type == MP4_FF_COMPOSER_UT_AVC_NALTYPE_IDR)
            {
                oRet = true;
            }
        }
    }
    return (oRet);
}

bool PVMp4FFComposerNode::IsMPEG4KeyFrame(Oscl_Vector <OsclMemoryFragment, OsclMemAllocator>& aList)
{
    OsclMemoryFragment aFrag = aList[0];
    bool oRet = false;
    //we need atleast 5 bytes, 4 bytes for VOP start code
    //first 2 bits of the fifth byte signal the VOP type
    uint8 I_VOP = 0;
    //do not delete these commented lines
    //uint8 P_VOP = 1;
    //uint8 B_VOP = 2;
    if (aFrag.len >= 5)
    {
        uint8* buf = (uint8*)(aFrag.ptr);
        uint8 byte = buf[4];
        if (((byte & 0xC0) >> 6) == I_VOP)
        {
            oRet = true;
        }
    }
    return oRet;
}

bool PVMp4FFComposerNode::IsH263KeyFrame(Oscl_Vector <OsclMemoryFragment, OsclMemAllocator>& aList)
{
    OsclMemoryFragment aFrag = aList[0];
    bool oRet = false;
    // For H263 short header we need atleast 5 bytes:
    //   22 bits for PSC
    //   8 bits for TR
    //   13 bits for PTYPE
    //   - Bit 9 of PTYPE is Coding Type. Value "0" for Intra and "1" for Inter.
    //   This is the 39th bit from start.

    // For H263 extended PTYPE header, we need between 6 bytes and 8 bytes:
    //   22 bits for PSC
    //   8 bits for TR
    //   8 bits to determine if it is an extended PTYPE.
    //   3 bits for Update Full Extended PTYPE (UFEP).
    //   If the UFEP is 001 need to add 18 bits for the Optional Part of PLUSPTYPE (OPPTYPE)
    //   9 bits for the Mandatory part of PLUSPTYPE (MPPTYPE)
    //   - Bits 1-3 indicate the Picture Type Code. Value "000"(binary) means INTRA.
    uint8 INTRA = 0;
    //do not delete these commented lines
    //uint8 INTER = 1;
    if (aFrag.len >= 5)
    {
        uint8* buf = (uint8*)(aFrag.ptr);
        uint8 byte = buf[4];
        // Check for an extended ptype (PLUSPTYPE)
        if (byte & 0x1C)
        {
            // Extended ptype format found.
            // Check the UFEP for the OPPTYPE
            if (aFrag.len >= 6)
            {
                byte = buf[5];
                if (!(byte & 0x80) && ((byte & 0x70) == INTRA))
                {
                    // Optional OPPTYPE format NOT found
                    // First three bits of the MPPTYPE are zeros.
                    // This is an INTRA frame.
                    oRet = true;
                }

                if ((aFrag.len >= 8) && (byte & 0x80))
                {
                    // Optional OPPTYPE format was found.  Skip past it.
                    byte = buf[7];
                    if ((byte & 0x1C) == INTRA)
                    {

                        // Optional OPPTYPE format was found.  Skipped past it.
                        // First three bits of the MPPTYPE are zeros.
                        // This is an INTRA frame.
                        oRet = true;
                    }
                }
            }
        }
        // Extended PTYPE not found.  Check bit 9 of the PTYPE for the "Picture Coding Type"
        else if ((byte & 0x02) == INTRA)
        {
            oRet = true;
        }
    }
    return oRet;
}

void PVMp4FFComposerNode::GetTextSDIndex(uint32 aSampleNum, int32& aIndex)
{
    //default index is zero
    aIndex = 0;
    Oscl_Vector<PVA_FF_TextSampleDescInfo*, OsclMemAllocator>::iterator it;
    for (it = textdecodervector.begin(); it != textdecodervector.end(); it++)
    {
        if ((aSampleNum >= (*it)->start_sample_num) &&
                (aSampleNum <= (*it)->end_sample_num))
        {
            aIndex = (*it)->sdindex;
            break;
        }
    }
}

PVMFStatus PVMp4FFComposerNode::HandleExtensionAPICommands()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMp4FFComposerNode::HandleExtensionAPICommands - command=%d", iCurrentCommand.iCmd));
    return PVMFErrNotSupported;
}

PVMFStatus PVMp4FFComposerNode::CancelCurrentCommand()
{
    // Cancel DoFlush here and return success.
    if (IsFlushPending())
    {
        CommandComplete(iCurrentCommand, PVMFErrCancelled);
        return PVMFSuccess;
    }
    return PVMFPending;//wait on sub-node cancel to complete.
}

