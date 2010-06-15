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
#ifndef PVMF_DATASTREAM_INIT_EXTENSION_H_INCLUDED
#define PVMF_DATASTREAM_INIT_EXTENSION_H_INCLUDED

#ifndef PV_UUID_H_INCLUDED
#include "pv_uuid.h"
#endif
#ifndef PV_INTERFACE_H
#include "pv_interface.h"
#endif
#ifndef PVMF_FORMAT_TYPE_H_INCLUDED
#include "pvmf_format_type.h"
#endif
#ifndef PVMF_RETURN_CODES_H_INCLUDED
#include "pvmf_return_codes.h"
#endif

#define PVMF_DATASTREAM_INIT_INTERFACE_UUID PVUuid(0x305e2b3e,0x0bdb,0x4858,0xa9,0x47,0x0e,0x7b,0x57,0xc8,0xb5,0x6f)

class PvmiDataStreamInterface;

class PVMFDataStreamInitExtension : public PVInterface
{
    public:
        /*
         * Passes the datastream and format type to the Source node
         *
         * @param aDataStream Data stream pointer of the file opened by the user.
         * @param aSourceFormat Reference to a format type describing the Source.
         * @return PVMFSuccess if successfully set, PVMFFailure for all other failures.
        */
        virtual PVMFStatus InitializeDataStreamData(PvmiDataStreamInterface* aDataStream,
                PVMFFormatType& aSourceFormat) = 0;

        virtual ~PVMFDataStreamInitExtension() {};
};

#endif
