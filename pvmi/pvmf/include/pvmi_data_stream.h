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
#ifndef PVMI_DATA_STREAM_H_INCLUDED
#define PVMI_DATA_STREAM_H_INCLUDED

#ifndef PVMI_DS_BASIC_INTERFACE_H_INCLUDED
#include "pvmi_ds_basic_interface.h"
#endif
#ifndef OSCL_FILE_SERVER_H_INCLUDED
#include "oscl_file_server.h"
#endif

class Oscl_File;

class PvmiDataStream : public PvmiDataStreamInterface
{

    public:
        PvmiDataStream(int input_fd);
        OSCL_IMPORT_REF PvmiDataStream(const char* filename);
        virtual ~PvmiDataStream();

        // API to set an handler for callbacks
        void SetCallbackHandler(PvmiBasicDataStreamObs* aDSObserver);

        // read while moving the file pointer
        virtual PvmiDataStreamStatus Read(PvmiDataStreamSession aSessionId,
                                          void* aBuffer,
                                          OsclSizeT& aSize);

        // read without moving the file pointer
        virtual PvmiDataStreamStatus ReadAtOffset(PvmiDataStreamSession aSessionId,
                void* aBuffer,
                OsclSizeT& aSize,
                OsclOffsetT aOffset);

        // write to the current file pointer location
        virtual PvmiDataStreamStatus Write(PvmiDataStreamSession aSessionId,
                                           void* aBuffer,
                                           OsclSizeT aSize);

        // seek at offset
        PvmiDataStreamStatus Seek(PvmiDataStreamSession aSessionID,
                                  OsclOffsetT& aOffset,
                                  PvmiDataStreamSeekType aSeektype);

        // returns current location of file pointer
        OsclOffsetT GetCurrentPointerPosition(PvmiDataStreamSession aSessionID);

        virtual PvmiDataStreamStatus GetFileSize(OsclOffsetT& size);

        void DataAvailable();

    protected:
        int GetFileInfo();
        PvmiDataStreamStatus InternalRead(PvmiDataStreamSession aSessionId,
                                          void* aBuffer,
                                          OsclSizeT& aSize,
                                          OsclOffsetT aOffset,
                                          bool aUseOffset);

        PvmiDataStreamStatus InternalWrite(void* aBuffer, OsclSizeT aSize);

    private:
        int iFd;
        Oscl_File* ipFile;
        Oscl_FileServer iFileServer;
        int iDataStreamState;
        bool iRegFile;
        OsclOffsetT iFileSize;
        OsclSizeT iBlkSize;
        PvmiBasicDataStreamObs* ipDSObserver;
        void* iTmpBuffer;
        OsclSizeT iTmpSize;

};




#endif
