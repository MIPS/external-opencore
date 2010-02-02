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
#ifndef PVMF_CPMPLUGIN_METERING_INTERFACE_H_INCLUDED
#define PVMF_CPMPLUGIN_METERING_INTERFACE_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_TYPES_H_INCLUDED
#include "oscl_types.h"
#endif
#ifndef OSCL_VECTOR_H_INCLUDED
#include "oscl_vector.h"
#endif
#ifndef OSCL_STRING_H_INCLUDED
#include "oscl_string.h"
#endif
#ifndef PV_UUID_H_INCLUDED
#include "pv_uuid.h"
#endif
#ifndef PV_INTERFACE_H_INCLUDED
#include "pv_interface.h"
#endif
#include "pvmf_return_codes.h"
#include "pvmf_event_handling.h"
#include "pvmf_cpmplugin_metering_interface_types.h"

#define PVMF_CPMPLUGIN_METERING_INTERFACE_MIMETYPE "pvxxx/pvmf/cpm/plugin/metering_interface"
#define PVMFCPMPluginMeteringInterfaceUuid PVUuid(0x5efe8be0,0xb62f,0x11db,0xab,0xbd,0x08,0x00,0x20,0x0c,0x9a,0x66)


/**
 * Metering interface for all Content Policy Manager Plugins
 */
class PVMFCPMPluginMeteringInterface : public PVInterface
{
    public:

        /**
         * Method to report metering data for a specific Meter ID.
         * The meter cert store will be searched for a certificate with
         * the given meter ID.  If the meter ID is found but there is no
         * certificate, then an attempt will be made to acquire the
         * certificate from the server.  If the certificate is acquired
         * successfully then metering will be reported for that certificate.
         *
         * To get status during or after this operation, use GetMeteringStatus.
         * To interrupt and cancel metering, use the plugin CancelCommand.
         *
         * @param [in] aSessionId: The observer session Id.
         * @param [in] aMeteringData: PVMFCPMMeteringData structure containing
         *    data related to this metering request.
         * @param [in] aTimeoutMsec: Optional timeout in milliseconds
         *    for each server communication.  Use -1 to indicate infinite wait.
         * @param [in] aContextData: Optional user data that will be returned
         *    in the command completion callback.
         *
         * @returns A unique command id for asynchronous completion.
         */
        virtual PVMFCommandId ReportMeteringData(
            PVMFSessionId aSessionId,
            const PVMFCPMMeteringData& aMeteringData,
            int32 aTimeoutMsec = (-1),
            OsclAny* aContextData = NULL) = 0;

        /**
         * Method to get count of meter certificates present in the store.
         *
         * @param [out] aCount: Number of meter certs present.
         * @param [out] aErrCode: WMDRM error code in case an error occurred.
         *
         * @returns: PVMFSuccess or an error.
         */
        virtual PVMFStatus GetMeterIDCount(
            uint32& aCount
            , uint32& errcode) = 0;

        /**
         * Method to get MID of a meter cert present in the store.
         *
         * @param [in] aIndex: Zero-based index of the desired meter cert.
         *                     Meter cert count can be obtained using GetMeterCertCount().
         *
         * @param [out] aMID: MID of the meter cert at index aIndex.
         *
         * @param [out] errcode: error code in case of failure.
         *
         * @returns PVMFSuccess or an error.
         */
        virtual PVMFStatus GetMeterID(
            uint32 aIndex,
            PVMFCPMMeterId &aMID,
            uint32& aErrCode) = 0;

        /**
         * Method to get the status of an ongoing or recently completed
         * metering sequence.
         *
         * @param [out] aStatus: meter status output
         *
         * @returns: PVMFSuccess if meter status is available, an error
         *   otherwise.
         */
        virtual PVMFStatus GetMeteringStatus(
            PVMFCPMMeterStatus& aStatus) = 0;

        /**
         * Method deletes a metering certificate from the metering certificate
         * store.
         *
         * @param [in] aMeteringData: PVMFCPMMeteringData structure containing
         *    data related to this metering request.
         * @param [out] aErrCode: Error code
         *
         * @returns PVMFSuccess or error
         */
        virtual PVMFStatus DeleteMeterCertificate(
            const PVMFCPMMeterId& aMeteringData,
            uint32& aErrCode) = 0;

        /**
         * Method invalidates a metering certificate in a metering certificate context.
         * store.
         *
         * @param [in] aMeteringData: PVMFCPMMeteringData structure containing
         *    data related to this metering request.
         * @param [out] aErrCode: Error code.
         *
         * @returns PVMFSuccess or error
         */
        virtual PVMFStatus InvalidateMeterCertificate(
            const PVMFCPMMeterId& aMeteringData,
            uint32& aErrCode) = 0;
};

#endif //PVMF_CPMPLUGIN_DOMAIN_INTERFACE_H_INCLUDED

