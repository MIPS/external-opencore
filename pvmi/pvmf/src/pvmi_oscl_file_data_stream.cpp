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
#include "pvmi_data_stream.h"
#endif
#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

// Local constants
#define PVMI_FD_DATASTREAM_GETINFO_SUCCESS 0
#define PVMI_FD_DATASTREAM_GETINFO_FSTAT_FAIL 1

// Macros associated with the state flag
#define PVMI_FD_DS_INVALID_STATE_FLAG   1
#define PVMI_FD_DS_CLOSE_FLAG       2
#define RESET_FD_DS_STATE(x) x=0
#define SET_INVALID_FD_DS_STATE(x) x |= PVMI_FD_DS_INVALID_STATE_FLAG
#define SET_CLOSE_FD_DS_FLAG(x) x |= PVMI_FD_DS_CLOSE_FLAG
#define CHECK_FD_DS_CLOSE_FLAG(x) (x & PVMI_FD_DS_CLOSE_FLAG)
#define CHECK_FD_DS_INVALID_STATE(x) (x & PVMI_FD_DS_INVALID_STATE_FLAG)

PvmiDataStream::PvmiDataStream(int input_fd)
{
}

OSCL_EXPORT_REF PvmiDataStream::PvmiDataStream(const char *filename)
{
    RESET_FD_DS_STATE(iDataStreamState);

    ipDSObserver = NULL;
    iTmpBuffer = NULL;
    iTmpSize = 0;

    // Create the Oscl_File
    ipFile = OSCL_NEW(Oscl_File, ());
    if (NULL == ipFile)
    {
        SET_INVALID_FD_DS_STATE(iDataStreamState);
        return;
    }

    ipFile->SetAsyncReadBufferSize(0);
    ipFile->SetLoggingEnable(false);

    ipFile->SetPVCacheSize(0);

    ipFile->SetSummaryStatsLoggingEnable(false);
    // Connect to File Server
    iFileServer.Connect();
    int32 result = ipFile->Open(filename, (Oscl_File::MODE_READ | Oscl_File::MODE_BINARY), iFileServer);

    //If open failed, cleanup the file object
    if (result != 0)
    {
        OSCL_DELETE(ipFile);
        ipFile = NULL;
        SET_INVALID_FD_DS_STATE(iDataStreamState);
        return;
    }

    // get information on the file
    if (PVMI_FD_DATASTREAM_GETINFO_SUCCESS != GetFileInfo())
    {
        ipFile->Close();
        OSCL_DELETE(ipFile);
        ipFile = NULL;
        SET_INVALID_FD_DS_STATE(iDataStreamState);
    }

    SET_CLOSE_FD_DS_FLAG(iDataStreamState);
}

void PvmiDataStream::SetCallbackHandler(PvmiBasicDataStreamObs* aDSObserver)
{
    ipDSObserver = aDSObserver;
}

int PvmiDataStream::GetFileInfo()
{
    ipFile->Seek(0, Oscl_File::SEEKEND);
    iFileSize = ipFile->Tell();
    ipFile->Seek(0, Oscl_File::SEEKCUR);

    return PVMI_FD_DATASTREAM_GETINFO_SUCCESS;
}

PvmiDataStream::~PvmiDataStream()
{
    if (ipFile)
    {
        ipFile->Close();
        OSCL_DELETE(ipFile);
        ipFile = NULL;
    }
    iFileServer.Close();
}

PvmiDataStreamStatus PvmiDataStream::GetFileSize(OsclOffsetT& size)
{
    if (CHECK_FD_DS_INVALID_STATE(iDataStreamState))
    {
        size = -1;
        return PVDS_INVALID_STATE;
    }
    size = iFileSize;
    return PVDS_SUCCESS;
}

PvmiDataStreamStatus PvmiDataStream::Read(PvmiDataStreamSession aSessionId,
        void* aBuffer,
        OsclSizeT& aSize)
{
    return InternalRead(aSessionId, aBuffer, aSize, 0, false);
}


PvmiDataStreamStatus PvmiDataStream::ReadAtOffset(PvmiDataStreamSession aSessionId,
        void* aBuffer,
        OsclSizeT& aSize,
        OsclOffsetT aOffset)
{
    return InternalRead(aSessionId, aBuffer, aSize, aOffset, true);
}


PvmiDataStreamStatus PvmiDataStream::InternalRead(PvmiDataStreamSession aSessionId,
        void* aBuffer,
        OsclSizeT& aSize,
        OsclOffsetT aOffset,
        bool aUseOffset)


{
    if (CHECK_FD_DS_INVALID_STATE(iDataStreamState))
    {
        aSize = 0;
        return PVDS_INVALID_STATE;
    }

    OsclSizeT size;
    OsclOffsetT CurrentOffset;
    PvmiDataStreamStatus status;

    if (aUseOffset)
    {
        CurrentOffset = GetCurrentPointerPosition(0);
        status = Seek(0, aOffset, PVDS_SEEK_SET);
        if (PVDS_SUCCESS != status)
            return PVDS_FAILURE;

        size = ipFile->Read(aBuffer, 1, aSize);

        status = Seek(0, CurrentOffset, PVDS_SEEK_SET);
        if (PVDS_SUCCESS != status)
            return status;     // seeking shouldn't be a issue, yet if it does, then it should exit.
    }
    else
    {
        size = ipFile->Read(aBuffer, 1, aSize);
    }

    aSize = size;

    switch (size)
    {
        case -1:
            status = PVDS_FAILURE;
            break;

        case 0:
            status = PVDS_END_OF_STREAM;
            break;

        default:
            status = PVDS_SUCCESS;
            break;
    }

    return status;
}

PvmiDataStreamStatus PvmiDataStream::Seek(PvmiDataStreamSession aSessionID,
        OsclOffsetT& aOffset,
        PvmiDataStreamSeekType aSeektype)
{
    if (CHECK_FD_DS_INVALID_STATE(iDataStreamState))
    {
        return PVDS_INVALID_STATE;
    }

    Oscl_File::seek_type seekType = Oscl_File::SEEKCUR;

    switch (aSeektype)
    {
        case PVDS_SEEK_SET:
            seekType = Oscl_File::SEEKSET;
            break;
        case PVDS_SEEK_CUR:
            seekType = Oscl_File::SEEKCUR;
            break;
        case PVDS_SEEK_END:
            seekType = Oscl_File::SEEKEND;
            break;
        default:
            break;
    }

    if (0 != ipFile->Seek(aOffset, seekType))
        return PVDS_FAILURE;

    return PVDS_SUCCESS;
}

OsclOffsetT PvmiDataStream::GetCurrentPointerPosition(PvmiDataStreamSession aSessionID)
{
    return ipFile->Tell();
}

PvmiDataStreamStatus PvmiDataStream::Write(PvmiDataStreamSession aSessionID,
        void* aBuffer,
        OsclSizeT aSize)
{
    return InternalWrite(aBuffer, aSize);
}

PvmiDataStreamStatus PvmiDataStream::InternalWrite(void* aBuffer, OsclSizeT aSize)
{
    if (CHECK_FD_DS_INVALID_STATE(iDataStreamState))
    {
        return PVDS_INVALID_STATE;
    }

    OsclSizeT size;
    size = ipFile->Write(aBuffer, 1, aSize);

    if (size != aSize) return PVDS_FAILURE;

    return PVDS_SUCCESS;

}

void PvmiDataStream::DataAvailable()
{
    // Data is available now, do the read and return the buffer
    PvmiDataStreamStatus status = PVDS_SUCCESS;
    status = InternalRead(0, iTmpBuffer, iTmpSize, 0, false);
    ipDSObserver->NotifyDataAvailable(iTmpSize, status);
}


