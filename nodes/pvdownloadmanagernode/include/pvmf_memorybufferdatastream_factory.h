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

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)

#ifndef PVMF_MEMORYBUFFERDATASTREAM_FACTORY_H_INCLUDED
#define PVMF_MEMORYBUFFERDATASTREAM_FACTORY_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_QUEUE_H_INCLUDED
#include "oscl_queue.h"
#endif
#ifndef OSCL_ERROR_CODES_H_INCLUDED
#include "oscl_error_codes.h"
#endif
#ifndef OSCL_TYPES_H_INCLUDED
#include "oscl_types.h"
#endif
#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif
#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif
#ifndef PVMF_EVENT_HANDLING_H_INCLUDED
#include "pvmf_event_handling.h"
#endif
#ifndef PVMI_DATA_STREAM_INTERFACE_H_INCLUDED
#include "pvmi_data_stream_interface.h"
#endif
#ifndef PVMF_CPMPLUGIN_ACCESS_INTERFACE_FACTORY_H_INCLUDED
#include "pvmf_cpmplugin_access_interface_factory.h"
#endif
#ifndef PVMI_DATASTREAMUSER_INTERFACE_H_INCLUDED
#include "pvmi_datastreamuser_interface.h"
#endif
#ifndef PVMF_MEDIA_DATA_H_INCLUDED
#include "pvmf_media_data.h"
#endif
#ifndef OSCL_MUTEX_H_INCLUDED
#include "oscl_mutex.h"
#endif
#ifndef OSCL_SEMAPHORE_H_INCLUDED
#include "oscl_semaphore.h"
#endif

#ifndef OSCLCONFIG_IO_H_INCLUDED
#include "osclconfig_io.h"
#endif


#define PV_MBDS_MAX_NUMBER_OF_READ_CONNECTIONS  16

#define PV_MBDS_MAX_NUMBER_OF_WRITE_CONNECTIONS 1

#define PV_MBDS_MAX_NUMBER_OF_TOTAL_CONNECTIONS  PV_MB_MAX_NUMBER_OF_WRITE_CONNECTIONS + PV_MB_MAX_NUMBER_OF_READ_CONNECTIONS


#define PV_MBDS_TEMP_CACHE_TRIM_MARGIN_PS                   64000
#define PV_MBDS_TEMP_CACHE_TRIM_THRESHOLD_PS(capacity)              (capacity * 2) / 3

#define PV_MBDS_TEMP_CACHE_TRIM_MARGIN_PS_NO_REPOS                   32000
#define PV_MBDS_TEMP_CACHE_TRIM_THRESHOLD_PS_NO_REPOS(capacity)     (capacity * 1) / 8

// for shoutcast
#define PV_MBDS_TEMP_CACHE_TRIM_MARGIN_SC                   4096
#define PV_MBDS_TEMP_CACHE_TRIM_THRESHOLD_SC(capacity)                          capacity / 6

#define NO_LIMIT        0
#define PV_MBDS_PERM_CACHE_SIZE     NO_LIMIT

// how many bytes are we willing to wait for assuming they are coming
// instead of repositioning
// this depends on channel bandwidth and network condition
#define PV_MBDS_BYTES_TO_WAIT         4 * 1024

// In forward repositioning, if the data is going to come in soon,
// which is defined as requested offset minus the download offset (aka current write pointer)
// being less than this threshold, then don't disconnect to send a new GET request.
#define PV_MBDS_FWD_SEEKING_NO_GET_REQUEST_THRESHOLD 64000



typedef enum
{
    MBDS_CACHE_TRIM_NONE,       // invalid node
    MBDS_CACHE_TRIM_HEAD_ONLY,  // trim the beginning of cache only
    MBDS_CACHE_TRIM_TAIL_ONLY,  // trim from the end of cache only
    MBDS_CACHE_TRIM_HEAD_AND_TAIL,  // trim the both ends of cache
    MBDS_CACHE_TRIM_EMPTY,      // empty the cache
} MBDSCacheTrimMode;

typedef enum
{
    MBDS_REPOSITION_EXACT,
    MBDS_REPOSITION_WITH_MARGIN
} MBDSRepositionMode;

typedef enum
{
    MBDS_STREAM_FORMAT_UNKNOWN,
    MBDS_STREAM_FORMAT_PROGRESSIVE_PLAYBACK,
    MBDS_STREAM_FORMAT_SHOUTCAST,
    MBDS_STREAM_FORMAT_RTMPSTREAMING,
    MBDS_STREAM_FORMAT_DTCP,
    MBDS_STREAM_FORMAT_SMOOTH_STREAMING,
    MBDS_STREAM_FORMAT_APPLE_HTTP_STREAMING,

} MBDSStreamFormat;

typedef enum
{
    PVMF_MBDS_TEMPCACHE_INFINITE,
    PVMF_MBDS_TEMPCACHE_FINITE
} PVMFMBDSTempCacheStatus;


#define IS_OFFSET_IN_RANGE(firstRangeOffset, lastRangeOffset, thisOffset, inRange)  \
    {                                                                               \
        inRange = true;                                                             \
        if (lastRangeOffset >= firstRangeOffset)                                    \
        {                                                                           \
            /* not wrapped  */                                                      \
            if ((thisOffset < firstRangeOffset) || (thisOffset > lastRangeOffset))  \
            {                                                                       \
                /* out of range */                                                  \
                inRange = false;                                                    \
            }                                                                       \
        }                                                                           \
        else                                                                        \
        {                                                                           \
            /* wrapped */                                                           \
            if ((thisOffset < firstRangeOffset) && (thisOffset > lastRangeOffset))  \
            {                                                                       \
                 /* out of range */                                                 \
                inRange = false;                                                    \
            }                                                                       \
        }                                                                           \
    }


class PVMFMemoryBufferWriteDataStreamImpl;

class PVMFMemoryBufferReadDataStreamImpl;

class PVMFMemoryBufferDataStreamTempCache
{
    public:
        PVMFMemoryBufferDataStreamTempCache();
        virtual ~PVMFMemoryBufferDataStreamTempCache();

        virtual bool RemoveFirstEntry(OsclRefCounterMemFrag*& aFrag, uint8*& aFragPtr);
        virtual bool RemoveLastEntry(OsclRefCounterMemFrag*& aFrag, uint8*& aFragPtr);
        virtual uint32 GetRemainSize()
        {
            return 0xFFFFFFFF;
        };
        virtual void SetDecryptionInterface(PVMFCPMPluginAccessUnitDecryptionInterface*& aDecryptionInterface) {};
        virtual PvmiDataStreamStatus AddEntry(OsclRefCounterMemFrag* aFrag, uint8* aFragPtr, TOsclFileOffset aFragSize, TOsclFileOffset& dataWritten, TOsclFileOffset aFileOffset);

        TOsclFileOffset GetTotalBytes();

        void GetFileOffsets(TOsclFileOffset& aFirstByte, TOsclFileOffset& aLastByte);

        TOsclFileOffset ReadBytes(uint8* aBuffer, TOsclFileOffset aFirstByte, TOsclFileOffset aLastByte, uint32& firstEntry);

        void GetFirstEntryInfo(TOsclFileOffset& entryOffset, uint32& entrySize);

        void GetLastEntryInfo(TOsclFileOffset& entryOffset, uint32& entrySize);

        uint32 GetNumEntries();

    protected:

        struct MBDSTempCacheEntry
        {
            // mem frag was allocated by protocol engine
            OsclRefCounterMemFrag* frag;
            // mem ptr
            uint8* fragPtr;
            // size of mem frag
            TOsclFileOffset fragSize;
            // file offset corresponding to the first byte of the mem frag
            TOsclFileOffset fileOffset;
        };

        // total number of bytes of data in this cache
        uint32 iTotalBytes;
        // file offset of first byte in cache, use iLock
        TOsclFileOffset iFirstByteFileOffset;
        // file offset of last byte in cache, use iLock
        TOsclFileOffset iLastByteFileOffset;
        // list of temp cache entries
        Oscl_Vector<MBDSTempCacheEntry*, OsclMemAllocator> iEntries;

        PVLogger* iLogger;
};

class PVMFMemoryBufferDataStreamPermCache
{
    public:
        PVMFMemoryBufferDataStreamPermCache();
        virtual ~PVMFMemoryBufferDataStreamPermCache();

        virtual void SetDecryptionInterface(PVMFCPMPluginAccessUnitDecryptionInterface*& aDecryptionInterface) {};
        virtual TOsclFileOffset GetTotalBytes();
        virtual PvmiDataStreamStatus WriteBytes(uint8* aFragPtr, TOsclFileOffset aFragSize, TOsclFileOffset& dataWritten, TOsclFileOffset aFileOffset);

        // these are the offsets of bytes already in the cache
        void GetFileOffsets(TOsclFileOffset& aFirstByte, TOsclFileOffset& aLastByte);

        // this byte range have made persistent, but not all the bytes are there yet
        // use GetFileOffsets to check what bytes are available for reading
        void GetPermOffsets(TOsclFileOffset& aFirstByte, TOsclFileOffset& aLastByte);

        PvmiDataStreamStatus AddEntry(uint8* aBufPtr, TOsclFileOffset aBufSize, uint8* aFillPtr, TOsclFileOffset aFirstOffset, TOsclFileOffset aLastOffset, TOsclFileOffset aFillOffset, TOsclFileOffset aFillSize);

        TOsclFileOffset ReadBytes(uint8* aBuffer, TOsclFileOffset aFirstByte, TOsclFileOffset aLastByte);

        bool RemoveFirstEntry(uint8*& aFragPtr);

        uint32 GetNumEntries();

        TOsclFileOffset GetCacheSize();

    private:

        struct MBDSPermCacheEntry
        {
            // mem ptr from malloc, saved for freeing later
            uint8* bufPtr;
            // size of mem from malloc
            TOsclFileOffset bufSize;
            // mem ptr to the next byte to be written to
            uint8* fillBufPtr;
            // file offset of first byte in buffer
            TOsclFileOffset firstFileOffset;
            // file offset of last byte in buffer
            TOsclFileOffset lastFileOffset;
            // file offset of the next byte to be written to
            TOsclFileOffset fillFileOffset;
            // number of bytes already written to buffer
            TOsclFileOffset fillSize;
        };

        // total number of bytes allocated in this cache
        TOsclFileOffset iTotalBufferAlloc;

        // file offset of first byte to be made persistent
        TOsclFileOffset iFirstPermByteOffset;
        // file offset of last byte to be made persistent
        TOsclFileOffset iLastPermByteOffset;

    protected:
        // total number of bytes of data in this cache
        TOsclFileOffset iTotalBytes;
        // file offset of first readable byte in cache
        TOsclFileOffset iFirstByteFileOffset;
        // file offset of last readable byte in cache
        TOsclFileOffset iLastByteFileOffset;
        // list of perm cache entries
        Oscl_Vector<MBDSPermCacheEntry*, OsclMemAllocator> iEntries;

        PVLogger* iLogger;
};

//////////////////////////////////////////////////////////////////////
// PVMFMemoryBufferReadDataStreamFactoryImpl
//////////////////////////////////////////////////////////////////////
class PVMFMemoryBufferReadDataStreamFactoryImpl : public PVMFDataStreamFactory
{
    public:
        PVMFMemoryBufferReadDataStreamFactoryImpl() {};
        OSCL_IMPORT_REF PVMFMemoryBufferReadDataStreamFactoryImpl(PVMFMemoryBufferDataStreamTempCache* aTempCache,
                PVMFMemoryBufferDataStreamPermCache* aPermCache);

        virtual OSCL_IMPORT_REF void SetWriteDataStreamPtr(PVInterface* aWriteDataStream);

        virtual OSCL_IMPORT_REF PVMFStatus QueryAccessInterfaceUUIDs(Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids);

        virtual OSCL_IMPORT_REF PVInterface* CreatePVMFCPMPluginAccessInterface(PVUuid& aUuid);

        virtual OSCL_IMPORT_REF void DestroyPVMFCPMPluginAccessInterface(PVUuid& aUuid, PVInterface* aPtr);

        virtual OSCL_IMPORT_REF void NotifyDownloadComplete();

    private:
        void addRef() {};

        void removeRef() {};

        bool queryInterface(const PVUuid&, PVInterface*&)
        {
            return false;
        };

        PVMFMemoryBufferWriteDataStreamImpl *iWriteDataStream;

        PVMFMemoryBufferDataStreamTempCache* iTempCache;

        PVMFMemoryBufferDataStreamPermCache* iPermCache;

        bool iDownloadComplete;

        Oscl_Vector<PVMFMemoryBufferReadDataStreamImpl*, OsclMemAllocator> iReadStreamVec;
};


//////////////////////////////////////////////////////////////////////
// PVMFMemoryBufferWriteDataStreamFactoryImpl
//////////////////////////////////////////////////////////////////////
class PVMFMemoryBufferWriteDataStreamFactoryImpl : public PVMFDataStreamFactory
{
    public:
        PVMFMemoryBufferWriteDataStreamFactoryImpl()
        {
            iWriteDataStream = NULL;
        };
        OSCL_IMPORT_REF PVMFMemoryBufferWriteDataStreamFactoryImpl(PVMFMemoryBufferDataStreamTempCache* aTempCache,
                PVMFMemoryBufferDataStreamPermCache* aPermCache, MBDSStreamFormat aStreamFormat, uint32 aTempCacheCapacity);

        virtual OSCL_IMPORT_REF ~PVMFMemoryBufferWriteDataStreamFactoryImpl();

        virtual OSCL_IMPORT_REF PVMFStatus QueryAccessInterfaceUUIDs(Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids);

        virtual OSCL_IMPORT_REF PVInterface* CreatePVMFCPMPluginAccessInterface(PVUuid& aUuid);

        virtual OSCL_IMPORT_REF void DestroyPVMFCPMPluginAccessInterface(PVUuid& aUuid, PVInterface* aPtr);

        virtual OSCL_IMPORT_REF void NotifyDownloadComplete();

    private:
        void addRef() {};

        void removeRef() {};

        bool queryInterface(const PVUuid&, PVInterface*&)
        {
            return false;
        };

        PVMFMemoryBufferWriteDataStreamImpl *iWriteDataStream;

        PVMFMemoryBufferDataStreamTempCache* iTempCache;

        PVMFMemoryBufferDataStreamPermCache* iPermCache;

        bool iDownloadComplete;

        MBDSStreamFormat iStreamFormat;

        uint32 iTempCacheCapacity;
};


//////////////////////////////////////////////////////////////////////
// PVMFMemoryBufferReadDataStreamImpl
//////////////////////////////////////////////////////////////////////
class PVMFMemoryBufferReadDataStreamImpl : public PVMIDataStreamSyncInterface
{
    public:
        OSCL_IMPORT_REF PVMFMemoryBufferReadDataStreamImpl(PVMFMemoryBufferWriteDataStreamImpl* aWriteDataStream,
                PVMFMemoryBufferDataStreamTempCache* aTempCache,
                PVMFMemoryBufferDataStreamPermCache* aPermCache);

        OSCL_IMPORT_REF ~PVMFMemoryBufferReadDataStreamImpl();

        // From PVInterface
        void addRef() {};

        void removeRef() {};

        OSCL_IMPORT_REF bool queryInterface(const PVUuid& uuid, PVInterface*& iface);

        OSCL_IMPORT_REF PvmiDataStreamStatus OpenSession(PvmiDataStreamSession& aSessionID,
                PvmiDataStreamMode aMode,
                bool nonblocking = false);

        OSCL_IMPORT_REF PvmiDataStreamStatus CloseSession(PvmiDataStreamSession aSessionID);

        OSCL_IMPORT_REF PvmiDataStreamRandomAccessType QueryRandomAccessCapability();

        OSCL_IMPORT_REF PvmiDataStreamStatus QueryReadCapacity(PvmiDataStreamSession aSessionID,
                TOsclFileOffset& capacity);

        OSCL_IMPORT_REF PvmiDataStreamCommandId RequestReadCapacityNotification(PvmiDataStreamSession aSessionID,
                PvmiDataStreamObserver& observer,
                TOsclFileOffset capacity,
                OsclAny* aContextData = NULL);

        OSCL_IMPORT_REF PvmiDataStreamStatus QueryWriteCapacity(PvmiDataStreamSession aSessionID,
                TOsclFileOffset& capacity);

        OSCL_IMPORT_REF PvmiDataStreamCommandId RequestWriteCapacityNotification(PvmiDataStreamSession aSessionID,
                PvmiDataStreamObserver& observer,
                uint32 capacity,
                OsclAny* aContextData = NULL);

        OSCL_IMPORT_REF PvmiDataStreamCommandId CancelNotification(PvmiDataStreamSession aSessionID,
                PvmiDataStreamObserver& observer,
                PvmiDataStreamCommandId aID,
                OsclAny* aContextData = NULL);

        OSCL_IMPORT_REF PvmiDataStreamStatus CancelNotificationSync(PvmiDataStreamSession aSessionID);

        OSCL_IMPORT_REF PvmiDataStreamStatus Read(PvmiDataStreamSession aSessionID, uint8* buffer,
                uint32 size, uint32& numelements);

        OSCL_IMPORT_REF PvmiDataStreamStatus Write(PvmiDataStreamSession aSessionID, uint8* buffer,
                uint32 size, uint32& numelements);

        OSCL_IMPORT_REF PvmiDataStreamStatus Write(PvmiDataStreamSession aSessionID, OsclRefCounterMemFrag* frag,
                uint32& aNumElements);

        OSCL_IMPORT_REF PvmiDataStreamStatus Seek(PvmiDataStreamSession aSessionID,
                TOsclFileOffset offset, PvmiDataStreamSeekType origin);

        OSCL_IMPORT_REF TOsclFileOffset GetCurrentPointerPosition(PvmiDataStreamSession aSessionID);

        OSCL_IMPORT_REF PvmiDataStreamStatus Flush(PvmiDataStreamSession aSessionID);

        OSCL_IMPORT_REF void NotifyDownloadComplete();

        void SetContentLength(TOsclFileOffset aContentLength)
        {
            OSCL_UNUSED_ARG(aContentLength);
        }

        OSCL_IMPORT_REF TOsclFileOffset GetContentLength();

        OSCL_IMPORT_REF PvmiDataStreamStatus SetSourceRequestObserver(PvmiDataStreamRequestObserver& aObserver);

        OSCL_IMPORT_REF PvmiDataStreamStatus SetBufferingCapacity(uint32 aMinCapacity, uint32 aTrimMargin);

        OSCL_IMPORT_REF uint32 QueryBufferingCapacity();

        OSCL_IMPORT_REF uint32 QueryBufferingTrimMargin();

        void SourceRequestCompleted(const PVMFCmdResp& aResponse)
        {
            OSCL_UNUSED_ARG(aResponse);
        }

        OSCL_IMPORT_REF PvmiDataStreamStatus MakePersistent(TOsclFileOffset aOffset, uint32 aSize);

        // This returns the offsets in the temp cache
        OSCL_IMPORT_REF void GetCurrentByteRange(TOsclFileOffset& aCurrentFirstByteOffset, TOsclFileOffset& aCurrentLastByteOffset);

    public:
        bool iDownloadComplete;

    private:
        PVMFMemoryBufferDataStreamTempCache* iTempCache;

        PVMFMemoryBufferDataStreamPermCache* iPermCache;

        PVMFMemoryBufferWriteDataStreamImpl* iWriteDataStream;

        OSCL_wHeapString<OsclMemAllocator> iFileName;

        PvmiDataStreamSession iSessionID;

        PVLogger* iLogger;

        TOsclFileOffset iFilePtrPos;

        bool iReadSessionOpened;

        MBDSStreamFormat iStreamFormat;
};


//////////////////////////////////////////////////////////////////////
// PVMFMemoryBufferWriteDataStreamImpl
//////////////////////////////////////////////////////////////////////
class PVMFMemoryBufferWriteDataStreamImpl : public PVMIDataStreamSyncInterface
{
    public:
        OSCL_IMPORT_REF PVMFMemoryBufferWriteDataStreamImpl(PVMFMemoryBufferDataStreamTempCache* aTempCache,
                PVMFMemoryBufferDataStreamPermCache* aPermCache, MBDSStreamFormat aStreamFormat,
                uint32 aTempCacheCapacity);

        OSCL_IMPORT_REF ~PVMFMemoryBufferWriteDataStreamImpl();

        // From PVInterface
        void addRef() {};

        void removeRef() {};

        /* These functions is defined as virtual,just incase if any class is derived from this,
           then to make functions of the derive class to be called. */
        virtual OSCL_IMPORT_REF bool queryInterface(const PVUuid& uuid, PVInterface*& iface);

        virtual OSCL_IMPORT_REF void NotifyObserverToDeleteMemFrag(OsclRefCounterMemFrag* frag);

        virtual OSCL_IMPORT_REF void SetDecryptionInterface(PVMFCPMPluginAccessUnitDecryptionInterface*& aDecryptionInterface);

        virtual OSCL_IMPORT_REF void SendWriteCapacityNotification();

        virtual OSCL_IMPORT_REF bool IsWriteNotificationPending(PvmiDataStreamSession aSessionID);

        virtual OSCL_IMPORT_REF PvmiDataStreamStatus QueryWriteCapacity(PvmiDataStreamSession aSessionID,
                TOsclFileOffset& aCapacity);

        virtual PVMFMBDSTempCacheStatus GetTempCacheWriteCapacity(TOsclFileOffset& aCapacity);

        OSCL_IMPORT_REF PvmiDataStreamStatus OpenSession(PvmiDataStreamSession& aSessionID,
                PvmiDataStreamMode aMode,
                bool nonblocking = false);

        OSCL_IMPORT_REF PvmiDataStreamStatus CloseSession(PvmiDataStreamSession aSessionID);

        OSCL_IMPORT_REF PvmiDataStreamStatus OpenReadSession(PvmiDataStreamSession& aSessionID,
                PvmiDataStreamMode aMode,
                bool nonblocking = false,
                PVMFMemoryBufferReadDataStreamImpl* aReadDataStream = NULL);

        OSCL_IMPORT_REF PvmiDataStreamRandomAccessType QueryRandomAccessCapability();

        OSCL_IMPORT_REF PvmiDataStreamStatus QueryReadCapacity(PvmiDataStreamSession aSessionID,
                TOsclFileOffset& aCapacity);

        OSCL_IMPORT_REF PvmiDataStreamCommandId RequestReadCapacityNotification(PvmiDataStreamSession aSessionID,
                PvmiDataStreamObserver& aobserver,
                TOsclFileOffset aCapacity,
                OsclAny* aContextData = NULL);

        OSCL_IMPORT_REF PvmiDataStreamCommandId RequestWriteCapacityNotification(PvmiDataStreamSession aSessionID,
                PvmiDataStreamObserver& aObserver,
                uint32 aCapacity,
                OsclAny* aContextData = NULL);

        OSCL_IMPORT_REF PvmiDataStreamCommandId CancelNotification(PvmiDataStreamSession aSessionID,
                PvmiDataStreamObserver& aObserver,
                PvmiDataStreamCommandId aID,
                OsclAny* aContextData = NULL);
        OSCL_IMPORT_REF PvmiDataStreamStatus CancelNotificationSync(PvmiDataStreamSession aSessionID);

        OSCL_IMPORT_REF PvmiDataStreamStatus Read(PvmiDataStreamSession aSessionID, uint8* buffer,
                uint32 size, uint32& numelements);

        OSCL_IMPORT_REF PvmiDataStreamStatus Write(PvmiDataStreamSession aSessionID, uint8* buffer,
                uint32 size, uint32& numelements);

        OSCL_IMPORT_REF PvmiDataStreamStatus Write(PvmiDataStreamSession aSessionID, OsclRefCounterMemFrag* frag,
                uint32& aNumElements);


        OSCL_IMPORT_REF PvmiDataStreamStatus Seek(PvmiDataStreamSession aSessionID, TOsclFileOffset offset,
                PvmiDataStreamSeekType origin);

        OSCL_IMPORT_REF PvmiDataStreamStatus Reposition(PvmiDataStreamSession aSessionID,
                TOsclFileOffset aOffset, MBDSRepositionMode aMode);

        OSCL_IMPORT_REF TOsclFileOffset GetCurrentPointerPosition(PvmiDataStreamSession aSessionID) ;

        OSCL_IMPORT_REF PvmiDataStreamStatus Flush(PvmiDataStreamSession aSessionID);

        OSCL_IMPORT_REF void NotifyDownloadComplete();

        OSCL_IMPORT_REF void SetContentLength(TOsclFileOffset aContentLength);

        OSCL_IMPORT_REF TOsclFileOffset GetContentLength();

        OSCL_IMPORT_REF PvmiDataStreamStatus SetSourceRequestObserver(PvmiDataStreamRequestObserver& aObserver);

        OSCL_IMPORT_REF void SourceRequestCompleted(const PVMFCmdResp& aResponse);

        OSCL_IMPORT_REF PvmiDataStreamStatus MakePersistent(TOsclFileOffset aOffset, uint32 aSize);

        OSCL_IMPORT_REF PvmiDataStreamStatus SetBufferingCapacity(uint32 aMinCapacity, uint32 aTrimMargin);

        OSCL_IMPORT_REF uint32 QueryBufferingCapacity();

        OSCL_IMPORT_REF uint32 QueryBufferingTrimMargin();

        OSCL_IMPORT_REF PvmiDataStreamStatus SetReadPointerPosition(PvmiDataStreamSession aSessionID, TOsclFileOffset aFilePosition);


        OSCL_IMPORT_REF PvmiDataStreamStatus SetReadPointerCacheLocation(PvmiDataStreamSession aSessionID, bool aInTempCache);

        OSCL_IMPORT_REF void ManageCache();

        OSCL_IMPORT_REF void TrimTempCache(MBDSCacheTrimMode aTrimMode);

        OSCL_IMPORT_REF void UpdateReadPointersAfterMakePersistent();

        OSCL_IMPORT_REF bool GetPermCachePersistence(TOsclFileOffset& aFirstOffset, TOsclFileOffset& aLastOffset);

        OSCL_IMPORT_REF void SetStreamFormat(MBDSStreamFormat aStreamFormat);

        OSCL_IMPORT_REF void SetTempCacheCapacity(uint32 aCapacity);

        OSCL_IMPORT_REF MBDSStreamFormat GetStreamFormat();

        OSCL_IMPORT_REF uint32 GetTempCacheCapacity();

    public:
        bool iDownloadComplete;
    protected:

        OSCL_IMPORT_REF void ManageReadCapacityNotifications();

    private:

        struct ReadCapacityNotificationStruct
        {
            bool iReadStructValid;

            bool iOutstanding;

            PvmiDataStreamSession iReadSessionID;

            PvmiDataStreamObserver *iReadObserver;

            TOsclFileOffset iFilePosition;

            TOsclFileOffset iReadCapacity;

            OsclAny* iContextData;

            PVMFCommandId iCommandID;

            PVMFCommandId iCurrentCommandID;
        };


        struct WriteCapacityNotificationStruct
        {
            bool iOutstanding;

            PvmiDataStreamSession iWriteSessionID;

            PvmiDataStreamObserver *iWriteObserver;

            TOsclFileOffset iFilePosition;

            uint32 iWriteCapacity;

            OsclAny* iContextData;

            PVMFCommandId iCommandID;

            PVMFCommandId iCurrentCommandID;
        };


        struct RepositionRequestStruct
        {
            bool iOutstanding;

            bool iRequestCompleted;

            PvmiDataStreamStatus iSuccess;

            PvmiDataStreamSession iRepositionSessionID;

            TOsclFileOffset iNewFilePosition;

            bool iFlushCache;

            RepositionRequestStruct():
                    iOutstanding(false),
                    iRequestCompleted(false),
                    iSuccess(PVDS_FAILURE),
                    iRepositionSessionID(-1),
                    iNewFilePosition(0),
                    iFlushCache(false)
            {}
        };

        struct ReadFilePositionStruct
        {
            bool iReadPositionStructValid;

            TOsclFileOffset iReadFilePtr;

            bool iInTempCache;

            PVMFMemoryBufferReadDataStreamImpl* iReadDataStream;
        };
    protected:
        PVMFMemoryBufferDataStreamTempCache* iTempCache;

        PVMFMemoryBufferDataStreamPermCache* iPermCache;

        uint32 iNumReadSessions;

        ReadCapacityNotificationStruct iReadNotifications[PV_MBDS_MAX_NUMBER_OF_READ_CONNECTIONS];

        RepositionRequestStruct iRepositionRequest;

        ReadFilePositionStruct iReadFilePositions[PV_MBDS_MAX_NUMBER_OF_READ_CONNECTIONS];

        PvmiDataStreamSession iSessionID;

        int32 iFileNumBytes;

        PVLogger* iLogger;

        TOsclFileOffset iContentLength;

        PvmiDataStreamRequestObserver* iRequestObserver;

        bool iWriteSessionOpened;

        WriteCapacityNotificationStruct iWriteNotification;

        // Tracks current write file pointer position
        TOsclFileOffset iFilePtrPos;

        bool iThrowAwayData;

        // Tracks the audio/video/text session read pointers
        PvmiDataStreamSession iAVTSessionID[3];
        TOsclFileOffset iAVTOffsetDelta;

        bool iMadePersistent;

        MBDSStreamFormat iStreamFormat;

        uint32 iTempCacheCapacity;

        uint32 iTempCacheTrimThreshold;

        uint32 iTempCacheTrimMargin;
};


//////////////////////////////////////////////////////////////////////
// PVMFMemoryBufferDataStream
//////////////////////////////////////////////////////////////////////
class PVMFMemoryBufferDataStream
{
    public:
        // in case we would want to pass the constructor an existing cache
        OSCL_IMPORT_REF PVMFMemoryBufferDataStream(PVMFFormatType& aStreamFormat, uint32 aTempCacheCapacity);

        OSCL_IMPORT_REF ~PVMFMemoryBufferDataStream();

        OSCL_IMPORT_REF PVMFDataStreamFactory* GetReadDataStreamFactoryPtr();

        OSCL_IMPORT_REF PVMFDataStreamFactory* GetWriteDataStreamFactoryPtr();

        OSCL_IMPORT_REF void NotifyDownloadComplete();

    private:
        PVMFMemoryBufferReadDataStreamFactoryImpl* iReadDataStreamFactory;

        PVMFMemoryBufferWriteDataStreamFactoryImpl* iWriteDataStreamFactory;

        PVInterface* iWriteDataStream;

        PVMFMemoryBufferDataStreamTempCache* iTemporaryCache;

        PVMFMemoryBufferDataStreamPermCache* iPermanentCache;

        PVLogger* iLogger;

        PVMFFormatType iFormatType;

};

#endif // PVMF_MEMORYBUFFERDATASTREAM_FACTORY_H_INCLUDED


#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB

