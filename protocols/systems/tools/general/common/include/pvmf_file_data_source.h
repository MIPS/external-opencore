/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
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
#ifndef PVMF_FILE_DATA_SOURCE_H_INCLUDED
#define PVMF_FILE_DATA_SOURCE_H_INCLUDED

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

#ifndef PVMF_NODE_INTERFACE_H_INCLUDED
#include "pvmf_node_interface.h"
#endif

#ifndef PVMF_PORT_BASE_IMPL_H_INCLUDED
#include "pvmf_port_base_impl.h"
#endif

#ifndef OSCL_TIMER_H_INCLUDED
#include "oscl_timer.h"
#endif

#ifndef PVMF_SIMPLE_MEDIA_BUFFER_H_INCLUDED
#include "pvmf_simple_media_buffer.h"
#endif

#ifndef PVMF_MEDIA_DATA_H_INCLUDED
#include "pvmf_media_data.h"
#endif

#ifndef PVMI_CONFIG_AND_CAPABILITY_H_INCLUDED
#include "pvmi_config_and_capability_utils.h"
#endif

#ifndef PVMF_BUFFER_DATA_SOURCE_H_INCLUDED
#include "pvmf_buffer_data_source.h"
#endif

class PVMFFileDataSourceObserver
{
    public:
        virtual ~PVMFFileDataSourceObserver() {};
        virtual void FileDataFinished() = 0;
};

class PVMFFileDataSource : public PVMFBufferDataSource
{
    public:
        OSCL_IMPORT_REF PVMFFileDataSource(int32 aPortTag,
                                           unsigned bitrate,
                                           unsigned min_sample_sz,
                                           unsigned max_sample_sz);
        OSCL_IMPORT_REF virtual ~PVMFFileDataSource();

        // timer observer
        void TimeoutOccurred(int32 timerID, int32 timeoutInfo);

        inline bool OpenFile(char *aFileName)
        {
            int8 retVal = 0;
            int32 connectRetVal = 0;
            connectRetVal = iFileServ->Connect();
            if (connectRetVal != 0)
                return false;
            retVal = iReadFile->Open(aFileName, Oscl_File::MODE_READ | Oscl_File::MODE_BINARY , *iFileServ);
            if (retVal != 0)
                return false;
            else
            {
                iIsFileDone = false;
                return true;
            }
        }

        inline void SetObserver(PVMFFileDataSourceObserver *aObserver)
        {
            iObserver = aObserver;
        }

    private:
        Oscl_File *iReadFile;
        Oscl_FileServer *iFileServ;
        PVMFFileDataSourceObserver *iObserver;
        bool iIsFileDone;
};
#endif
