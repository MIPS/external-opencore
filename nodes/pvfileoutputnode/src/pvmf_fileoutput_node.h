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
 *
 * @file pvmf_fileoutput_node.h
 * @brief Simple file output node. Writes incoming data to specified
 * file without any media type specific file format
 *
 */

#ifndef PVMF_FILEOUTPUT_NODE_H_INCLUDED
#define PVMF_FILEOUTPUT_NODE_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCLCONFIG_IO_H_INCLUDED
#include "osclconfig_io.h"
#endif
#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif
#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif
#ifndef OSCL_PRIQUEUE_H_INCLUDED
#include "oscl_priqueue.h"
#endif
#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif
#ifndef PVMF_MEDIA_CLOCK_H_INCLUDED
#include "pvmf_media_clock.h"
#endif
#ifndef PVMF_FORMAT_TYPE_H_INCLUDED
#include "pvmf_format_type.h"
#endif
#ifndef PVMF_SIMPLE_MEDIA_BUFFER_H_INCLUDED
#include "pvmf_simple_media_buffer.h"
#endif
#ifndef PVMF_MEDIA_DATA_H_INCLUDED
#include "pvmf_media_data.h"
#endif
#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#include "pvmf_node_interface_impl.h"
#endif
#ifndef PVMF_FILEOUTPUT_CONFIG_H_INCLUDED
#include "pvmf_fileoutput_config.h"
#endif
#ifndef PVMF_FILEOUTPUT_FACTORY_H_INCLUDED
#include "pvmf_fileoutput_factory.h"
#endif
#ifndef PVMF_COMPOSER_SIZE_AND_DURATION_H_INCLUDED
#include "pvmf_composer_size_and_duration.h"
#endif
#ifndef PVMF_NODES_SYNC_CONTROL_H_INCLUDED
#include "pvmf_nodes_sync_control.h"
#endif
#ifndef PVMF_NODE_UTILS_H_INCLUDED
#include "pvmf_node_utils.h"
#endif
#ifndef PVMF_FILEOUTPUT_INPORT_H
#include "pvmf_fileoutput_inport.h"
#endif

// Macros for AMR header
#define AMR_HEADER      "#!AMR\n"
#define AMR_HEADER_SIZE 6

// Macros for AMR-WB header
#define AMRWB_HEADER        "#!AMR-WB\n"
#define AMRWB_HEADER_SIZE   9

////////////////////////////////////////////////////////////////////////////
class PVMFFileOutputAlloc : public Oscl_DefAlloc
{
    public:
        void* allocate(const uint32 size)
        {
            void* tmp = (void*)oscl_malloc(size);
            return tmp;
        }

        void deallocate(void* p)
        {
            oscl_free(p);
        }
};

////////////////////////////////////////////////////////////////////////////
//memory allocator type for this node.
typedef OsclMemAllocator PVMFFileOutputNodeAllocator;

// Forward declaration
class PVMFFileOutputInPort;
class PVLogger;

//Mimetypes for the custom interface
#define PVMF_FILE_OUTPUT_NODE_CUSTOM1_MIMETYPE "pvxxx/FileOutputNode/Custom1"
#define PVMF_FILE_OUTPUT_NODE_MIMETYPE "pvxxx/FileOutputNode"
#define PVMF_BASEMIMETYPE "pvxxx"

////////////////////////////////////////////////////////////////////////////
class PVMFFileOutputNode :  public PVMFNodeInterfaceImpl,
        public PvmfFileOutputNodeConfigInterface,
        public PvmfComposerSizeAndDurationInterface,
        public PvmfNodesSyncControlInterface,
        public PvmiCapabilityAndConfigBase
{
    public:
        PVMFFileOutputNode(int32 aPriority);
        ~PVMFFileOutputNode();

        PVMFStatus GetCapability(PVMFNodeCapability& aNodeCapability);
        PVMFPortIter* GetPorts(const PVMFPortFilter* aFilter = NULL);

        //from PVMFPortActivityHandler
        void HandlePortActivity(const PVMFPortActivity& aActivity);

        // Pure virtual from PvInterface
        void addRef();
        void removeRef();
        bool queryInterface(const PVUuid& uuid, PVInterface*& iface);

        // Pure virtual from PvmfFileOutputNodeConfigInterface
        PVMFStatus SetOutputFileName(const OSCL_wString& aFileName);
        PVMFStatus SetOutputFileDescriptor(const OsclFileHandle* aFileHandle);

        // Pure virtual from PvmfComposerSizeAndDurationInterface
        PVMFStatus SetMaxFileSize(bool aEnable, uint32 aMaxFileSizeBytes);
        void GetMaxFileSizeConfig(bool& aEnable, uint32& aMaxFileSizeBytes);
        PVMFStatus SetMaxDuration(bool aEnable, uint32 aMaxDurationMilliseconds);
        void GetMaxDurationConfig(bool& aEnable, uint32& aMaxDurationMilliseconds);
        PVMFStatus SetFileSizeProgressReport(bool aEnable, uint32 aReportFrequency);
        void GetFileSizeProgressReportConfig(bool& aEnable, uint32& aReportFrequency);
        PVMFStatus SetDurationProgressReport(bool aEnable, uint32 aReportFrequency);
        void GetDurationProgressReportConfig(bool& aEnable, uint32& aReportFrequency);

        // Pure virtuals from PvmfNodesSyncControlInterface
        PVMFStatus SetClock(PVMFMediaClock* aClock);
        PVMFStatus ChangeClockRate(int32 aRate);
        PVMFStatus SetMargins(int32 aEarlyMargin, int32 aLateMargin);
        void ClockStarted(void);
        void ClockStopped(void);
        PVMFCommandId SkipMediaData(PVMFSessionId aSessionId,
                                    PVMFTimestamp aResumeTimestamp,
                                    uint32 aStreamID,
                                    bool aPlayBackPositionContinuous = false,
                                    OsclAny* aContext = NULL);

        friend class PVMFFileOutputInPort;
        friend class PVFileOutputNodeFactory;


        // implemetation of PvmiCapabilityAndConfig class functions here

        void setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver);

        PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier,
                                     PvmiKvp*& aParameters, int& num_parameter_elements,
                                     PvmiCapabilityContext aContext);
        PVMFStatus releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);
        void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                               int num_elements, PvmiKvp * & aRet_kvp);
        PVMFStatus verifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);

        // function used in VerifyParametersSync n SetParametersSync of capability class
        PVMFStatus VerifyAndSetConfigParameter(PvmiKvp& aParameter, bool aSetParam);

        // function used in getParametersSync of capability class
        PVMFStatus GetConfigParameter(PvmiKvp*& aParameters, int& aNumParamElements, int32 aIndex, PvmiKvpAttr reqattr);


    private:
        void ConstructL();
        void Run();

        /**
         * Process a port activity. This method is called by Run to process a port activity.
         *
         */
        bool ProcessPortActivity();
        void QueuePortActivity(const PVMFPortActivity &aActivity);

        //Command handlers.
        PVMFStatus DoReset();
        PVMFStatus DoQueryInterface();
        PVMFStatus DoRequestPort(PVMFPortInterface*&);
        PVMFStatus DoReleasePort();
        PVMFStatus DoInit();
        PVMFStatus DoStart();
        PVMFStatus DoStop();
        PVMFStatus DoFlush();
        PVMFStatus DoPause();
        PVMFStatus CancelCurrentCommand();
        PVMFStatus HandleExtensionAPICommands();

        void CloseOutputFile();

        // Handle command and data events
        PVMFStatus ProcessIncomingData(PVMFSharedMediaDataPtr aMediaData);

        PVMFStatus ProcessIncomingMsg(PVMFPortInterface* aPort);

        /**
         * Send file size progress report if enabled.
         * @return PVMFFailure if informational observer is not set, else PVMFSuccess
         */
        PVMFStatus SendFileSizeProgress();

        /**
         * Send duration progress report if enabled.
         * @param aTimestamp Timestamp of current frame in milliseconds.
         * @return PVMFFailure if informational observer is not set, else PVMFSuccess
         */
        PVMFStatus SendDurationProgress(uint32 aTimestamp);

        /**
         * Check if maximum file size or duration is reached if a maximum is set.
         *
         * @param aFrameSize Size of current frame in bytes.
         * @return PVMFSuccess if feature is enabled and the maximum file size / duration is reached.
         *         PVMFPending if feature is enabled and the max file size / duration has not been reached.
         *         PVMFErrNotSupported if feature is not enabled.
         *         PVMFFailure if informational observer is not set or if max file size or duration is set
         *         but the finalizing output file failed.
         */
        PVMFStatus CheckMaxFileSize(uint32 aFrameSize);

        /**
         * Check if maximum file size or duration is reached if a maximum is set.
         *
         * @param aTimestamp Timestamp of current frame in milliseconds.
         * @return PVMFSuccess if feature is enabled and the maximum file size / duration is reached.
         *         PVMFPending if feature is enabled and the max file size / duration has not been reached.
         *         PVMFErrNotSupported if feature is not enabled.
         *         PVMFFailure if informational observer is not set or if max file size or duration is set
         *         but the finalizing output file failed.
         */
        PVMFStatus CheckMaxDuration(uint32 aTimestamp);

        /**
         * Write data to output file.
         *
         * @param aData Data to be written to file.
         * @param aSize Size of data to be written to file.
         * @return PVMFSuccess if data is written, or maximum file size is reached, else PVMFFailure.
         */
        PVMFStatus WriteData(OsclAny* aData, uint32 aSize);

        /**
         * Write memory fragment to output file.
         *
         * @param aMemFrag Memory fragment object whose data has to be written to output file.
         * @param aTimestamp Timestamp of the frame to be written in milliseconds.
         * @return PVMFSuccess if memory fragment is written, or max file size or duration is reached, else PVMFFailure.
         */
        PVMFStatus WriteData(OsclRefCounterMemFrag aMemFrag, uint32 aTimestamp);

        /**
         * Write format specific info to output file.
         *
         * @param aData Data to be written to file.
         * @param aSize Size of data to be written to file.
         * @return PVMFSuccess if data is written, or maximum file size is reached, else PVMFFailure.
         */
        PVMFStatus WriteFormatSpecificInfo(OsclAny* aPtr, uint32 aSize);

        /** Clear all pending port activity after max file size or duration is reached. */
        void ClearPendingPortActivity();

        // Queue of commands
        PVMFCommandId iCmdIdCounter;

        // Input port
        PVMFPortInterface* iInPort;

        // Output file name
        OSCL_wHeapString<OsclMemAllocator> iOutputFileName;
        OsclFileHandle* iFileHandle;

        // Allocator
        Oscl_DefAlloc* iAlloc;

        // Output file
        Oscl_FileServer iFs;
        Oscl_File iOutputFile;
        int32 iFileOpened;

        bool iFirstMediaData;
        PVMFFormatType iFormat;

        PVMFPortVector<PVMFFileOutputInPort, PVMFFileOutputNodeAllocator> iPortVector;
        Oscl_Vector<PVMFPortActivity, PVMFFileOutputNodeAllocator> iPortActivityQueue;

        // Variables for max file size and duration feature
        bool iMaxFileSizeEnabled;
        bool iMaxDurationEnabled;
        uint32 iMaxFileSize;
        uint32 iMaxDuration;
        uint32 iFileSize;

        // Variables for progress report feature
        bool iFileSizeReportEnabled;
        bool iDurationReportEnabled;
        uint32 iFileSizeReportFreq;
        uint32 iDurationReportFreq;
        uint32 iNextFileSizeReport;
        uint32 iNextDurationReport;

        // Variables for media data queue and synchronization
        PVMFMediaClock* iClock;
        int32 iEarlyMargin;
        int32 iLateMargin;
};

#endif // PVMF_FILEOUTPUT_NODE_H_INCLUDED
