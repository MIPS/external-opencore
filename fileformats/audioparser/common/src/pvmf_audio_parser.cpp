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
#ifndef PVMF_AUDIO_PARSER_H_INCLUDED
#include "pvmf_audio_parser.h"
#endif
#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#define LOG_DEBUG_MSG(msg) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, ipLogger, PVLOGMSG_STACK_TRACE, msg)

CacheBuffer::CacheBuffer(PvmiDataStreamInterface* apDataStream)
{
    ipBuffer = new uint8 [CACHE_SIZE]; // what if this fails
    ipBufferPosition = ipBuffer;
    iBytesUsed = 0;
    iMaxBytes = 0;
    ipDataStream = apDataStream;
    iTestDSPos = -1;
    iRefill = false;
    ipLogger = PVLogger::GetLoggerObject("audioparser");

    // Fill buffer with data from current pos of DS
    Fill();
}

CacheBuffer::~CacheBuffer()
{
    ipLogger = NULL;

    delete[] ipBuffer;
}

void CacheBuffer::SetRefillRequired(bool aFlag)
{
    iRefill = aFlag;
}

bool CacheBuffer::IsRefillRequired()
{
    return iRefill;
}

PVMFBufferStatus CacheBuffer::Fill()
{
    OsclSizeT Size = CACHE_SIZE;
    LOG_DEBUG_MSG((0, "Size to read is %zd", Size));

    PvmiDataStreamStatus status = ipDataStream->Read(0, ipBufferPosition, Size);
    iMaxBytes = Size;

    if (PVDS_SUCCESS != status)
    {
        LOG_DEBUG_MSG((0, "Read data stream FAILED"));
        return PVMF_INSUFFICIENT_DATA;
    }
    return PVMF_READ_SUCCESS;
}

PVMFBufferStatus CacheBuffer::Refill()
{
    LOG_DEBUG_MSG((0, "In Refill()"));
    LOG_DEBUG_MSG((0, "Bytes Used=%d", iBytesUsed));

    PvmiDataStreamStatus status = PVDS_SUCCESS;
    OsclOffsetT remainingSize = 0;
    if (-1 != iTestDSPos)
    {
        // Read comment below about this condition
        status = ipDataStream->Seek(0, iTestDSPos, PVDS_SEEK_SET);
        iTestDSPos = -1;
    }
    else
    {
        if (CACHE_SIZE != iBytesUsed)
        {
            remainingSize = CACHE_SIZE - iBytesUsed;
            remainingSize = 0 - remainingSize;
            status = ipDataStream->Seek(0, remainingSize, PVDS_SEEK_CUR);
        }
    }

    ipBufferPosition = ipBuffer;
    OsclSizeT Size = CACHE_SIZE;
    LOG_DEBUG_MSG((0, "Size to read from DS is %zd", Size));

    status = ipDataStream->Read(0, ipBufferPosition, Size);
    LOG_DEBUG_MSG((0, "DS read %zd bytes", Size));;
    iMaxBytes  = Size;
    iBytesUsed = 0;

    switch (status)
    {
        case(PVDS_FAILURE):
        {
            LOG_DEBUG_MSG((0, "Read data stream FAILED"));
            return PVMF_READ_FAILURE;
        }
        case(PVDS_END_OF_STREAM):
        {
            LOG_DEBUG_MSG((0, "Read data stream END OF STREAM"));
            return PVMF_INSUFFICIENT_DATA;
        }
        case(PVDS_PENDING):
        {
            LOG_DEBUG_MSG((0, "Read return PENDING from Data stream"));

            // Since we have already Seek'd at remainingSize
            // and read at data stream would have moved file pointer,
            // so we need to store curr location to set it again in next refill call.
            // By the fact that we seeked back that data was there to succeed, but to simulate asyn
            // currently data stream uses count variable to return pending. when that is removed then
            // use of iTestDSPos should also be removed.
            iTestDSPos = ipDataStream->GetCurrentPointerPosition(0);
            LOG_DEBUG_MSG((0, "DS was seeked at %d before read call", iTestDSPos));
            return PVMF_REQ_PENDING;
        }
        default:
        {
            return PVMF_READ_SUCCESS;
        }
    }
}

PVMFBufferStatus CacheBuffer::GetBuffData(uint8*& apBuffer, OsclSizeT& aSize)
{
    LOG_DEBUG_MSG((0, "CacheBuffer::GetBuffData() IN"));
    LOG_DEBUG_MSG((0, "Current bytes used=%d; Max=%d", iBytesUsed, iMaxBytes));
    PVMFBufferStatus Status = PVMF_READ_SUCCESS;

    if (iRefill)
    {
        Status = Refill();
        if (PVMF_READ_SUCCESS != Status)
            return Status;
        iRefill = false;
    }

    apBuffer = ipBufferPosition;
    aSize = iMaxBytes - iBytesUsed;
    if (0 == aSize)
        return PVMF_INSUFFICIENT_DATA;
    LOG_DEBUG_MSG((0, "valid size id %d", aSize));
    LOG_DEBUG_MSG((0, "CacheBuffer::GetBuffData() OUT with status %d", Status));
    return Status;
}

void CacheBuffer::setBytesUsed(uint32 aUsedBytes)
{
    iBytesUsed += aUsedBytes;
    ipBufferPosition += aUsedBytes;
    if (iBytesUsed >= CACHE_SIZE)
        SetRefillRequired(true);

    LOG_DEBUG_MSG((0, "CacheBuffer::setBytesUsed() bytes used = %d", iBytesUsed));
}

bool CacheBuffer::Seek(OsclOffsetT aSeekAt)
{
    LOG_DEBUG_MSG((0, "CacheBuffer::Seek() IN"));
    LOG_DEBUG_MSG((0, "Seek at %ld", aSeekAt));

    OsclOffsetT curPos = ipDataStream->GetCurrentPointerPosition(0);
    OsclOffsetT offset = 0;
    if (curPos > aSeekAt)
        offset = curPos - aSeekAt;
    else
        // Seek Pos is greater than current datastream position which means we need to refill cache
        offset = iMaxBytes;

    LOG_DEBUG_MSG((0, "Difference with curr DS pos is %ld and Max bytes in cache is %d", offset, iMaxBytes));

    if (offset < OsclOffsetT(iMaxBytes))
    {
        ipBufferPosition = ipBuffer + iMaxBytes - offset;
        iBytesUsed = iMaxBytes - offset;
        LOG_DEBUG_MSG((0, "Bytes used set to %d", iBytesUsed));

    }
    else
    {
        LOG_DEBUG_MSG((0, "Need to refill cache and reset DS"));

        PvmiDataStreamStatus status = ipDataStream->Seek(0, aSeekAt, PVDS_SEEK_SET);
        if (PVDS_SUCCESS != status)
        {
            LOG_DEBUG_MSG((0, "Seek at offset %ld failed", aSeekAt));
            return false;
        }
        iBytesUsed = 0;
        ipBufferPosition = ipBuffer;
        Fill();
    }
    LOG_DEBUG_MSG((0, "CacheBuffer::Seek() OUT"));
    return true;
}


