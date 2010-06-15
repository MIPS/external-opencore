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
#ifndef PVMI_DS_BASIC_INTERFACE_H_INCLUDED
#define PVMI_DS_BASIC_INTERFACE_H_INCLUDED

#ifndef PVMI_DATA_STREAM_INTERFACE_H_INCLUDED
#include "pvmi_data_stream_interface.h"
#endif

typedef TOsclFileOffset OsclOffsetT;

class PvmiBasicDataStreamObs
{
    public:
        virtual ~PvmiBasicDataStreamObs() {};

        /**
          * Function to provide the data to the observer in async way
          * @param - aBytesRead - Number of bytes read
          * @param - aStatus - Status of read operation
         */
        virtual void NotifyDataAvailable(OsclSizeT aBytesRead, PvmiDataStreamStatus aStatus) = 0;
};

class PvmiDataStreamInterface : public PVInterface
{
    public:

        virtual void addRef() {};
        virtual void removeRef() {};
        virtual bool queryInterface(const PVUuid& uuid, PVInterface*& iface)
        {
            return false;
        }

        /**
            * Reads from the data stream into the buffer a maximum of 'numelements'
            * of size 'size'.
            * @param aSessionID the session identifier of the stream
            * @param buffer pointer to buffer of type void*
            * @param size   number of requested bytes
            */
        virtual PvmiDataStreamStatus Read(PvmiDataStreamSession aSessionID,
                                          void* aBuffer,
                                          OsclSizeT& aSize) = 0;

        /**
        * Reads up to aSize bytes at offset aOffset from the data stream into the buffer.
        * It does not move the internal read pointer.
        * @param aSessionID [in] the session identifier of the stream
        * @param aBuffer    [in] pointer to buffer of type void*
        * @param aSize      [in/out] number of requested bytes
        * @param aOffset    [in] offset where read should start
        */
        virtual PvmiDataStreamStatus ReadAtOffset(PvmiDataStreamSession aSessionID,
                void* aBuffer,
                OsclSizeT& aSize,
                OsclOffsetT aOffset) = 0;

        virtual PvmiDataStreamStatus GetFileSize(OsclOffsetT& size) = 0;

        /**
            * Seek operation
            * Sets the position for the read/write pointer.
            *
            * @param aSessionID the session identifier of the stream
            * @param offset offset from the specified origin.
            * @param origin starting point
            *
            * @return returns the status of the operation.
            */
        virtual PvmiDataStreamStatus Seek(PvmiDataStreamSession aSessionID,
                                          OsclOffsetT& aOffset,
                                          PvmiDataStreamSeekType aSeektype) = 0;

        /**
        * Returns the current position (i.e., byte offset from the beginning
        * of the data stream for the read/write pointer.
        */
        virtual OsclOffsetT GetCurrentPointerPosition(PvmiDataStreamSession aSessionID) = 0;


        virtual PvmiDataStreamStatus Write(PvmiDataStreamSession aSessionID,
                                           void* aBuffer,
                                           OsclSizeT aSize) = 0;

        virtual void SetCallbackHandler(PvmiBasicDataStreamObs* aDSObserver) = 0;

        virtual void DataAvailable() = 0;
};



#endif //PVMI_DATA_STREAM_INTERFACE_H_INCLUDED

