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
    RESET_FD_DS_STATE(iDataStreamState);
    if ((iFd = input_fd) == -1)
    {
        SET_INVALID_FD_DS_STATE(iDataStreamState);
        return;
    }

    // get information on the file
    if (PVMI_FD_DATASTREAM_GETINFO_SUCCESS != GetFileInfo())
    {
        SET_INVALID_FD_DS_STATE(iDataStreamState);
    }

    ipDSObserver = NULL;
    iTmpBuffer = NULL;
    iTmpSize = 0;
}

PvmiDataStream::PvmiDataStream(const char *filename)
{
    RESET_FD_DS_STATE(iDataStreamState);
    if ((iFd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)
    {
        SET_INVALID_FD_DS_STATE(iDataStreamState);
        return;
    }

    // get information on the file
    if (PVMI_FD_DATASTREAM_GETINFO_SUCCESS != GetFileInfo())
    {
        close(iFd);
        SET_INVALID_FD_DS_STATE(iDataStreamState);
    }

    SET_CLOSE_FD_DS_FLAG(iDataStreamState);

    ipDSObserver = NULL;
    iTmpBuffer = NULL;
    iTmpSize = 0;
}

void PvmiDataStream::SetCallbackHandler(PvmiBasicDataStreamObs* aDSObserver)
{
    ipDSObserver = aDSObserver;
}

int PvmiDataStream::GetFileInfo()
{
    struct stat fd_stat;
    if (-1 == fstat(iFd, &fd_stat))
    {
        // problem getting file information
        return PVMI_FD_DATASTREAM_GETINFO_FSTAT_FAIL;
    }
    iRegFile = S_ISREG(fd_stat.st_mode);
    iFileSize = fd_stat.st_size;
    iBlkSize = fd_stat.st_blksize;

    return PVMI_FD_DATASTREAM_GETINFO_SUCCESS;
}

PvmiDataStream::~PvmiDataStream()
{
    if (CHECK_FD_DS_CLOSE_FLAG(iFd))
    {
        close(iFd);
    }
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

        size = read(iFd, aBuffer, aSize);

        status = Seek(0, CurrentOffset, PVDS_SEEK_SET);
        if (PVDS_SUCCESS != status)
            return status;     // seeking shouldn't be a issue, yet if it does, then it should exit.
    }
    else
    {
        size = read(iFd, aBuffer, aSize);
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

    int seekType = SEEK_CUR;

    switch (aSeektype)
    {
        case PVDS_SEEK_SET:
            seekType = SEEK_SET;
            break;
        case PVDS_SEEK_CUR:
            seekType = SEEK_CUR;
            break;
        case PVDS_SEEK_END:
            seekType = SEEK_END;
            break;
        default:
            break;
    }

    OsclOffsetT offset = lseek(iFd, aOffset, seekType);

    if (-1 == offset)
    {
        return PVDS_FAILURE;
    }

    aOffset = offset;

    return PVDS_SUCCESS;
}

OsclOffsetT PvmiDataStream::GetCurrentPointerPosition(PvmiDataStreamSession aSessionID)
{
    return lseek(iFd, 0, SEEK_CUR);
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
    size = write(iFd, aBuffer, aSize);

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


