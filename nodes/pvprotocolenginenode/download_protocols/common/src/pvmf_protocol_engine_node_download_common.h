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

#ifndef PVMF_PROTOCOLENGINE_NODE_DOWNLOAD_COMMON_H_INCLUDED
#define PVMF_PROTOCOLENGINE_NODE_DOWNLOAD_COMMON_H_INCLUDED

#ifndef PVMF_PROTOCOLENGINE_NODE_COMMON_H_INCLUDED
#include "pvmf_protocol_engine_node_common.h"
#endif

#ifndef PVMF_FORMAT_PROGDOWNLOAD_SUPPORT_EXTENSION_H_INCLUDED
#include "pvmf_format_progdownload_support_extension.h"
#endif

#ifndef PVDL_CONFIG_FILE_H_INCLUDED
#include "pvdl_config_file.h"
#endif

#ifndef OSCLCONFIG_IO_H_INCLUDED
#include "osclconfig_io.h"
#endif



////////////////////////////////////////////////////////////////////////////////////
//////  DownloadContainer
////////////////////////////////////////////////////////////////////////////////////
class OSCL_IMPORT_REF DownloadContainer : public ProtocolContainer
{
    public:
        // constructor
        DownloadContainer(PVMFProtocolEngineNode *aNode = NULL);
        virtual ~DownloadContainer()
        {
            ;
        }

        virtual void deleteProtocolObjects();
        virtual int32 doPreStart();
        virtual bool doPause();
        virtual PVMFStatus doStop();
        virtual void doClear(const bool aNeedDelete = false);
        virtual void doCancelClear();
        virtual bool doInfoUpdate(const uint32 downloadStatus);
        virtual bool addSourceData(OsclAny* aSourceData);
        virtual bool createCfgFile(OSCL_String& aUri);
        virtual bool getProxy(OSCL_String& aProxyName, uint32 &aProxyPort);
        virtual void setHttpVersion(const uint32 aHttpVersion);
        virtual void setHttpExtensionHeaderField(OSCL_String &aFieldKey,
                OSCL_String &aFieldValue,
                const HttpMethod aMethod,
                const bool aPurgeOnRedirect);

        virtual bool handleContentRangeUnmatch();
        virtual bool downloadUpdateForHttpHeaderAvailable();
        virtual bool isStreamingPlayback();
        virtual bool handleProtocolStateComplete(PVProtocolEngineNodeInternalEvent &aEvent, PVProtocolEngineNodeInternalEventHandler *aEventHandler);
        virtual void checkSendResumeNotification();

    protected:
        virtual int32 initNodeOutput(PVMFProtocolEngineNodeOutput *aNodeOutput);
        virtual bool initProtocol_SetConfigInfo(HttpBasedProtocol *aProtocol);
        virtual void initDownloadControl();
        virtual void updateDownloadControl(const bool isDownloadComplete = false);
        virtual bool isDownloadComplete(const uint32 downloadStatus) const
        {
            return ((downloadStatus == PROCESS_SUCCESS_END_OF_MESSAGE ||
                     downloadStatus == PROCESS_SUCCESS_END_OF_MESSAGE_TRUNCATED ||
                     downloadStatus == PROCESS_SUCCESS_END_OF_MESSAGE_WITH_EXTRA_DATA ||
                     downloadStatus == PROCESS_SUCCESS_END_OF_MESSAGE_BY_SERVER_DISCONNECT) &&
                    iInterfacingObjectContainer->isWholeSessionDone());
        }
        virtual bool ignoreThisTimeout(const int32 timerID);
        virtual bool needToCheckResumeNotificationMaually()
        {
            return iNeedCheckResumeNotificationManually;
        }
        void setEventReporterSupportObjects();
        void requiredSocketReconnect(const bool aForceSocketReconnect = true);

    protected:
        bool iForceSocketReconnect;
        bool iNeedCheckResumeNotificationManually;
};

////////////////////////////////////////////////////////////////////////////////////
//////  pvHttpDownloadOutput
////////////////////////////////////////////////////////////////////////////////////

// This derived class adds data stream output
struct DownloadOutputConfig
{
    bool isResumeDownload;
    bool isRangeSupport;
    bool isNeedOpenDataStream;

    // constructor
    DownloadOutputConfig() : isResumeDownload(false),
            isRangeSupport(true),
            isNeedOpenDataStream(true)
    {
        ;
    }
};

class pvHttpDownloadOutput : public PVMFProtocolEngineNodeOutput
{
    public:
        OSCL_IMPORT_REF void setOutputObject(OsclAny* aOutputObject, const uint32 aObjectType = NodeOutputType_InputPortForData);
        OSCL_IMPORT_REF virtual PVMFStatus initialize(OsclAny* aInitInfo = NULL);
        OSCL_IMPORT_REF virtual int32 flushData(const uint32 aOutputType = NodeOutputType_InputPortForData);
        OSCL_IMPORT_REF virtual void discardData(const bool aNeedReopen = false);
        OSCL_IMPORT_REF TOsclFileOffset getAvailableOutputSize();
        OSCL_IMPORT_REF uint32 getMaxAvailableOutputSize();

        // constructor and destructor
        OSCL_IMPORT_REF pvHttpDownloadOutput(PVMFProtocolEngineNodeOutputObserver *aObserver = NULL);
        OSCL_IMPORT_REF virtual ~pvHttpDownloadOutput();

    protected:
        // write data to data stream object
        // return~0=0xffffffff for error.
        OSCL_IMPORT_REF uint32 writeToDataStream(OUTPUT_DATA_QUEUE &aOutputQueue);
        OSCL_IMPORT_REF bool writeToDataStream(uint8 *aBuffer, uint32 aBufferLen);
        OSCL_IMPORT_REF virtual int32 openDataStream(OsclAny* aInitInfo);
        // reset
        OSCL_IMPORT_REF virtual void reset();

    protected:
        PVMFDataStreamFactory *iDataStreamFactory;
        PVMIDataStreamSyncInterface *iDataStream;
        PvmiDataStreamSession iSessionID; // PvmiDataStreamSession = int32
        bool isOpenDataStream;
        uint32 iCounter; // for debugging purpose
};

////////////////////////////////////////////////////////////////////////////////////
//////  pvDownloadControl
////////////////////////////////////////////////////////////////////////////////////

// This class does auto-resume control and download progress update for event report
class DownloadProgressInterface;
class OSCL_IMPORT_REF pvDownloadControl : public DownloadControlInterface
{
    public:
        // constructor, may leave for creating download clock
        pvDownloadControl();
        virtual ~pvDownloadControl()
        {
            clearBody();
        }


        // set download control supporting objects:
        //      PVMFFormatProgDownloadSupportInterface object,
        //      PVMFDownloadProgressInterface object,
        //      engine playback clock object,
        //      protocol engine object,
        //      DownloadProgressInterface object,   (to get the clip duraton)
        //      PVMFProtocolEngineNodeOutput object
        void setSupportObject(OsclAny *aDLSupportObject, DownloadControlSupportObjectType aType);

        // From PVMFDownloadProgressInterface API pass down
        virtual void requestResumeNotification(const uint32 currentNPTReadPosition, bool& aDownloadComplete, bool& aNeedSendUnderflowEvent);
        void cancelResumeNotification();

        // check whether to make resume notification; if needed, then make resume notification
        // Return value: 1 means making resume notification normally (underflow->auto resume),
        //               2 means making resume notification for download complete
        //               0 means anything else
        virtual int32 checkResumeNotification(const bool aDownloadComplete = true);

        // From PVMFDownloadProgressInterface API
        virtual void getDownloadClock(OsclSharedPtr<PVMFMediaClock> &aClock)
        {
            OSCL_UNUSED_ARG(aClock);
        }
        // From PVMFDownloadProgressInterface API
        void setClipDuration(const uint32 aClipDurationMsec)
        {
            iClipDurationMsec = aClipDurationMsec;
        }

        void setPrevDownloadSize(TOsclFileOffset aPrevDownloadSize = 0)
        {
            iPrevDownloadSize = aPrevDownloadSize;
        }

        virtual void clear();

        // clear several fields for progressive playback repositioning
        virtual void clearPerRequest()
        {
            ;
        }

        virtual void updateProtocolInfo(OsclAny *aProtocolInfo)
        {
            OSCL_UNUSED_ARG(aProtocolInfo);
        }

    protected:

        // simple routine to focus on sending resume notification only
        virtual void sendResumeNotification(const bool aDownloadComplete);
        virtual void sendDownloadCompleteNotification();

        // auto-resume playback decision
        bool isResumePlayback(const uint32 aDownloadRate, const TOsclFileOffset aCurrDownloadSize, const TOsclFileOffset aFileSize);

        // create iDlProgressClock, will leave when memory allocation fails
        void createDownloadClock();
        virtual bool updateDownloadClock() = 0;

        // ready means, download clock has been created, and all the objects have passed down
        bool isInfoReady()
        {
            return !(iDlProgressClock.GetRep() == NULL ||
                     iProtocol == NULL               ||
                     iDownloadProgress == NULL       ||
                     iNodeOutput == NULL);
        };

        // called by isResumePlayback()
        virtual bool isDlAlgoPreConditionMet(const uint32 aDownloadRate,
                                             const uint32 aDurationMsec,
                                             const TOsclFileOffset aCurrDownloadSize,
                                             const TOsclFileOffset aFileSize);

        // update duration by new playback rate, called by checkAutoResumeAlgoWithConstraint
        virtual uint32 checkNewDuration(const uint32 aCurrDurationMsec)
        {
            return aCurrDurationMsec;
        }

        // called by checkAutoResumeAlgoWithConstraint()
        virtual bool approveAutoResumeDecisionShortCut(const TOsclFileOffset aCurrDownloadSize,
                const uint32 aDurationMsec,
                const uint32 aPlaybackTimeMsec,
                uint32 &aPlaybackRemainingTimeMsec)
        {
            OSCL_UNUSED_ARG(aCurrDownloadSize);
            OSCL_UNUSED_ARG(aDurationMsec);
            OSCL_UNUSED_ARG(aPlaybackTimeMsec);
            OSCL_UNUSED_ARG(aPlaybackRemainingTimeMsec);
            return false;
        }

        // No constraint: for file size/clip duration/clip bitrate(i.e. playback rate), one of them must be unavailable, except
        // file size and clip duration are available, but clip bitrate is unavailable. This only applies on PDL
        virtual bool checkAutoResumeAlgoNoConstraint(const TOsclFileOffset aCurrDownloadSize,
                const TOsclFileOffset aFileSize,
                uint32 &aDurationMsec)
        {
            OSCL_UNUSED_ARG(aCurrDownloadSize);
            OSCL_UNUSED_ARG(aFileSize);
            OSCL_UNUSED_ARG(aDurationMsec);
            return false;
        }

        // with contraint: file size and clip duration are both available
        bool checkAutoResumeAlgoWithConstraint(const uint32 aDownloadRate,
                                               const TOsclFileOffset aRemainingDownloadSize,
                                               const uint32 aDurationMsec,
                                               const TOsclFileOffset aFileSize);

        // use fixed-point calculation to replace the float-point calculation: aRemainingDLSize<0.0009*aDownloadRate*aRemainingPlaybackTime
        virtual bool approveAutoResumeDecision(const TOsclFileOffset aRemainingDLSize,
                                               const uint32 aDownloadRate,
                                               const uint32 aRemainingPlaybackTime);

        // old algorithm
        bool isResumePlaybackWithOldAlg(const uint32 aDownloadRate,
                                        const TOsclFileOffset aRemainingDownloadSize);

        virtual bool canRunAutoResumeAlgoWithConstraint(const uint32 aDurationMsec,
                const TOsclFileOffset aFileSize)
        {
            return (aDurationMsec > 0 && aFileSize > 0);
        }


        // adding buffer constraint for the algo, i.e. if buffer constraint meets (or buffer overflows), auto-resume should kick off.
        virtual bool isOutputBufferOverflow()
        {
            return false;
        }


        // handle overflow issue: // result = x*1000/y
        TOsclFileOffset divisionInMilliSec(const TOsclFileOffset x, const TOsclFileOffset y);

        // called by checkResumeNotification()
        bool checkSendingNotification(const bool aDownloadComplete = false);

        // set file size to parser node for the new API, setFileSize()
        virtual void setFileSize(const TOsclFileOffset aFileSize);
        virtual void updateFileSize();
        bool getPlaybackTimeFromEngineClock(uint32 &aPlaybackTime);
        virtual void setProtocolInfo()
        {
            ;
        }
        virtual uint32 getDownloadRate()
        {
            return iProtocol->getDownloadRate();
        }

    private:
        void clearBody();


    protected:
        // download control
        PVMFTimebase_Tickcount iEstimatedServerClockTimeBase;
        OsclSharedPtr<PVMFMediaClock> iDlProgressClock;
        PVMFMediaClock* iCurrentPlaybackClock;
        PVMFFormatProgDownloadSupportInterface *iProgDownloadSI;
        HttpBasedProtocol *iProtocol;
        DownloadProgressInterface *iDownloadProgress;
        PVMFProtocolEngineNodeOutput *iNodeOutput;
        PVDlCfgFileContainer *iCfgFileContainer;

        bool iPlaybackUnderflow;
        bool iDownloadComplete;
        bool iRequestResumeNotification;
        bool iFirstResumeNotificationSent;
        TOsclFileOffset iCurrentNPTReadPosition;
        uint32 iClipDurationMsec;
        uint32 iPlaybackByteRate;
        TOsclFileOffset iPrevDownloadSize;
        TOsclFileOffset iFileSize;

        bool iDlAlgoPreConditionMet;
        bool iSetFileSize;
        bool iSendDownloadCompleteNotification;
        uint32 iClipByterate;

        PVLogger* iDataPathLogger;
};

////////////////////////////////////////////////////////////////////////////////////
//////  DownloadProgress
////////////////////////////////////////////////////////////////////////////////////
class OSCL_IMPORT_REF DownloadProgress : public DownloadProgressInterface
{
    public:

        // cosntructor and destructor
        DownloadProgress();
        virtual ~DownloadProgress()
        {
            reset();
        }

        // set download progress supporting objects:
        //      PVMFFormatProgDownloadSupportInterface object,
        //      protocol engine object,
        //      config file object,         (for progressive download only)
        //      track selction container    (for fastrack download only)
        //      PVMFProtocolEngineNodeOutput object (for fasttrack download only)
        virtual void setSupportObject(OsclAny *aDLSupportObject, DownloadControlSupportObjectType aType);

        // updata download progress
        bool update(const bool aDownloadComplete = false);

        // return true for the new download progress
        bool getNewProgressPercent(uint32 &aProgressPercent);

        void setNewProgressPercent(const uint32 aProgressPercent)
        {
            OSCL_UNUSED_ARG(aProgressPercent);
        }

        // return duration regardless of the difference between progressive download and fasttrack download
        void setClipDuration(const uint32 aClipDurationMsec)
        {
            iDurationMsec = aClipDurationMsec;
        }

        virtual void setDownloadProgressMode(DownloadProgressMode aMode = DownloadProgressMode_TimeBased)
        {
            OSCL_UNUSED_ARG(aMode);
        }


    protected:
        virtual uint32 getClipDuration();
        virtual bool updateDownloadClock(const bool aDownloadComplete) = 0;
        virtual bool calculateDownloadPercent(TOsclFileOffset &aDownloadProgressPercent);
        virtual void reset();

    protected:
        HttpBasedProtocol *iProtocol;
        PVMFFormatProgDownloadSupportInterface *iProgDownloadSI;
        PVMFProtocolEngineNodeOutput *iNodeOutput;

        //for progress reports
        uint32 iCurrProgressPercent;
        uint32 iPrevProgressPercent;
        uint32 iDownloadNPTTime;
        uint32 iDurationMsec;
};



////////////////////////////////////////////////////////////////////////////////////
//////  PVMFDownloadDataSourceContainer
////////////////////////////////////////////////////////////////////////////////////

// This container class wraps the data from all the download source data classes, i.e.,
// PVMFDownloadDataSourceHTTP, PVMFDownloadDataSourcePVX, PVMFSourceContextDataDownloadHTTP and PVMFSourceContextDataDownloadPVX

class CPVXInfo;
class PVMFDownloadDataSourceContainer
{
    public:
        bool iHasDataSource;                                    // true means the constainer is already filled in the data source
        bool iIsNewSession;                                     // true if the downloading a new file, false if keep downloading a partial downloading file
        uint32 iByteSeekMode;                                   // 1 if byte-seek supported by server, 0 if not supported, 2 if value is not set by App or server not DMS.
        TOsclFileOffset iMaxFileSize;                                    // the max size of the file.
        uint32 iPlaybackControl;                                // correspond to PVMFDownloadDataSourceHTTP::TPVPlaybackControl, PVMFSourceContextDataDownloadHTTP::TPVPlaybackControl
        OSCL_wHeapString<OsclMemAllocator> iConfigFileName;     // download config file
        OSCL_wHeapString<OsclMemAllocator> iDownloadFileName;   // local file name of the downloaded clip
        OSCL_HeapString<OsclMemAllocator>  iProxyName;          // HTTP proxy name, either ip or dns
        uint32 iProxyPort;                                      // HTTP proxy port
        OSCL_HeapString<OsclMemAllocator> iUserID;              // UserID string used for HTTP basic/digest authentication
        OSCL_HeapString<OsclMemAllocator> iUserPasswd;          // password string used for HTTP basic/digest authentication
        int32 iMaxHttpHeaderFieldSize;

        CPVXInfo *iPvxInfo;                                     // Fasttrack only, contains all the info in the .pvx file except the URL

    public:
        // default constructor
        PVMFDownloadDataSourceContainer()
        {
            clear();
        }

        bool isEmpty()
        {
            return !iHasDataSource;
        }

        // major copy constructor to do type conversion
        PVMFDownloadDataSourceContainer(OsclAny* aSourceData);

        // add source data
        bool addSource(OsclAny* aSourceData);

        void clear()
        {
            iHasDataSource   = false;
            iIsNewSession    = true;
            iMaxFileSize     = 0;
            iPlaybackControl = 0;
            iProxyPort       = 0;
            iMaxHttpHeaderFieldSize = 0x7fffffff;
            iPvxInfo         = NULL;
        }

    private:
        // type conversion routine for each download source data class
        void copy(const PVMFDownloadDataSourceHTTP& aSourceData);
        void copy(const PVMFDownloadDataSourcePVX& aSourceData);
        void copy(const PVMFSourceContextDataDownloadHTTP& aSourceData);
        void copy(const PVMFSourceContextDataDownloadPVX& aSourceData);
        PVMFSourceContextDataDownloadHTTP::TPVPlaybackControl convert(const PVMFDownloadDataSourceHTTP::TPVPlaybackControl aPlaybackControl);
};



////////////////////////////////////////////////////////////////////////////////////
//////  PVDlCfgFileContainer and its derived class definition
////////////////////////////////////////////////////////////////////////////////////
class OSCL_IMPORT_REF PVDlCfgFileContainer
{
    public:
        virtual ~PVDlCfgFileContainer() {}

        PVDlCfgFileContainer(PVMFDownloadDataSourceContainer *aDataSource) :
                iPlaybackMode(PVMFDownloadDataSourceHTTP::EAsap),
                iDataSource(aDataSource)
        {
            iDataPathLogger = PVLogger::GetLoggerObject("datapath.sourcenode.protocolenginenode");
        }

        virtual PVMFStatus createCfgFile(OSCL_String &aUrl);
        void setDataSource(PVMFDownloadDataSourceContainer *aDataSource)
        {
            iDataSource = aDataSource;
        }

        // get API
        OsclSharedPtr<PVDlCfgFile> &getCfgFile()
        {
            return iCfgFileObj;
        }
        PVMFDownloadDataSourceHTTP::TPVPlaybackControl getPlaybackMode()
        {
            return iPlaybackMode;
        }
        bool isEmpty()
        {
            return (iCfgFileObj.GetRep() == NULL);
        }
        virtual void saveConfig()
        {
            if (!isEmpty()) iCfgFileObj->SaveConfig();
        }

    protected:
        virtual PVMFStatus configCfgFile(OSCL_String &aUrl);
        PVMFStatus loadOldConfig(); // utility function for configCfgFile()

    protected:
        OsclSharedPtr<PVDlCfgFile> iCfgFileObj;
        PVMFDownloadDataSourceHTTP::TPVPlaybackControl iPlaybackMode;
        PVMFDownloadDataSourceContainer *iDataSource;
        PVLogger* iDataPathLogger;
};

////////////////////////////////////////////////////////////////////////////////////
//////  downloadEventReporter
////////////////////////////////////////////////////////////////////////////////////

class downloadEventReporter : public EventReporter
{
    public:
        // constructor
        OSCL_IMPORT_REF downloadEventReporter(EventReporterObserver *aObserver);

        OSCL_IMPORT_REF virtual void setSupportObject(OsclAny *aSupportObject, EventReporterSupportObjectType aType);
        OSCL_IMPORT_REF virtual bool checkReportEvent(const uint32 downloadStatus);
        OSCL_IMPORT_REF virtual void clear();
        OSCL_IMPORT_REF bool checkContentInfoEvent(const uint32 downloadStatus);

        // enable some specific events
        OSCL_IMPORT_REF void sendDataReadyEvent();
        OSCL_IMPORT_REF void enableBufferingCompleteEvent();
        OSCL_IMPORT_REF void sendBufferStatusEvent();

        OSCL_IMPORT_REF virtual bool checkContentLengthOrTooLarge();
        OSCL_IMPORT_REF virtual bool checkContentTruncated(const uint32 downloadStatus);

    protected:
        virtual bool needToCheckContentInfoEvent()
        {
            return true;
        }
        OSCL_IMPORT_REF virtual void checkUnexpectedDataAndServerDisconnectEvent(const uint32 downloadStatus);

        // supporting function for checkReportEvent()
        OSCL_IMPORT_REF bool checkBufferInfoEvent(const uint32 downloadStatus);
        // check and send buffer complete, data ready and unexpected data events
        OSCL_IMPORT_REF void checkBufferCompleteEvent(const uint32 downloadStatus);
        OSCL_IMPORT_REF void checkUnexpectedDataEvent(const uint32 downloadStatus);
        OSCL_IMPORT_REF virtual void checkServerDisconnectEvent(const uint32 downloadStatus);
        // for checkContentInfoEvent()
        OSCL_IMPORT_REF int32 isDownloadFileTruncated(const uint32 downloadStatus);
        bool isDownloadComplete(const uint32 downloadStatus) const
        {
            return (downloadStatus == PROCESS_SUCCESS_END_OF_MESSAGE ||
                    downloadStatus == PROCESS_SUCCESS_END_OF_MESSAGE_TRUNCATED ||
                    downloadStatus == PROCESS_SUCCESS_END_OF_MESSAGE_WITH_EXTRA_DATA ||
                    downloadStatus == PROCESS_SUCCESS_END_OF_MESSAGE_BY_SERVER_DISCONNECT);
        }
        // will be overriden in case of PS
        // called by sendBufferStatusEventBody()
        OSCL_IMPORT_REF virtual void reportBufferStatusEvent(const uint32 aDownloadPercent);
        virtual bool allowSameDownloadPercentReport(const uint32 aCurrDownloadPercent, const uint32 aPrevDownloadPercent)
        {
            OSCL_UNUSED_ARG(aCurrDownloadPercent);
            OSCL_UNUSED_ARG(aPrevDownloadPercent);
            return true;
        }

    protected:
        bool iSendBufferStartInfoEvent;
        bool iSendBufferCompleteInfoEvent;
        bool iSendMovieAtomCompleteInfoEvent;
        bool iSendInitialDataReadyEvent;
        bool iSendContentLengthEvent;
        bool iSendContentTruncateEvent;
        bool iSendContentTypeEvent;
        bool iSendUnexpectedDataEvent;
        bool iSendServerDisconnectEvent;

        // supporting objects
        DownloadProgressInterface *iDownloadProgress;
        HttpBasedProtocol *iProtocol;
        PVDlCfgFileContainer *iCfgFileContainer;
        PVMFProtocolEngineNodeTimer *iNodeTimer;
        PVMFProtocolEngineNodeOutput *iNodeOutput;
        InterfacingObjectContainer *iInterfacingObjectContainer;

    private:
        void sendBufferStatusEventBody(const bool aForceToSend = false);

    private:
        uint32 iPrevDownloadProgress;
};

#endif

