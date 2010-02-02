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
#ifndef PVMF_CPMPLUGIN_LICENSE_MANAGER_INTERFACE_H_INCLUDED
#define PVMF_CPMPLUGIN_LICENSE_MANAGER_INTERFACE_H_INCLUDED

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
#include "pvmf_cpmplugin_license_manager_interface_types.h"
#include "pvmf_cpmplugin_domain_interface_types.h"

#define PVMF_CPMPLUGIN_LICENSE_MANAGER_INTERFACE_MIMETYPE "pvxxx/pvmf/cpm/plugin/license_manager_interface"
#define PVMFCPMPluginLicenseManagerInterfaceUuid PVUuid(0x05b8186a,0xc2b1,0x11db,0x83,0x14,0x08,0x00,0x20,0x0c,0x9a,0x66)

/**
 * License Manager interface for all Content Policy Manager Plugins
 */
class PVMFCPMPluginLicenseManagerInterface : public PVInterface
{
    public:

        /**
         * Method to perform license store maintenance.
         *
         * @param [out] Additional error code
         * @param [in] Optional opaque data associated with the request.
         * @param [in] Size of the optional opaque data.
         *
         * @returns PVMFSuccess or an error
         */
        virtual PVMFStatus LicenseStoreMaintenance(uint32 &aErrCode, OsclAny* aData = NULL
                , uint32 aDataSize = 0) = 0;

        /**
         * Method to get the status of an ongoing or recently completed
         * LicenseStoreMaintenance command.
         *
         * @param [out] aStatus: clean store status output
         *
         * @returns: PVMFSuccess if status is available, an error
         *   otherwise.
         */
        virtual PVMFStatus GetLicenseStoreMaintenanceStatus(
            PVMFCPMLicenseStoreMaintenanceStatus& aStatus) = 0;

        /**
         * Method to delete a license from the store
         *
         * @param [in] content ID
         * @param [out] aErrcode: additional error code
         *
         * @returns PVMFSuccess or an error
         */
        virtual PVMFStatus DeleteLicense(const OSCL_String& aContentID
                                         , uint32 &aErrcode) = 0;

        /**
         * Method to retrieve the list of licenses that are pending expiry and need to
         * be updated/renewed from the server. This method gives greater control to the
         * application, since it gives greater flexibility and control of updating licenses
         * that it deems necessary.
         *
         * @param [out] aSyncList: A vector that would contain the list of licenses based on the
         *    filtering criteria below.
         * @param [out] aErrCode: error code in case of failure.
         * @param [in] aMaxRemainingCount: For counted licenses, retrieve only those that have
         *    less than the specified number of play counts remaining.  To retrieve all counted
         *    licenses regardless of the counts remaining, use (-1).
         * @param [in] aMaxRemainingHours: For time-based licenses, retrieve only those that have
         *    less than the specified value of hours remaining.  To retrieve all time-based
         *    licenses regardless of the time remaining, use (-1).
         *
         * @returns: PVMFSuccess if licenses are available, an error otherwise.
         */

        virtual PVMFStatus GetLicenseSyncList(
            Oscl_Vector<PVMFCPMContentId, OsclMemAllocator> &aSyncList
            , uint32& aErrCode
            , int32 aMaxRemainingCount = -1
                                         , int32 aMaxRemainingHours = -1) = 0;

        /**
         * Method to delete all stored licenses for streaming content.
         *
         * @param [out] errcode: error code in case of failure.
         * @returns: PVMFSuccess if separate store for streaming licenses is available,
         * an error otherwise.
         */

        virtual PVMFStatus PurgeStreamingLicenses(
            uint32& errcode) = 0;
};


#endif //PVMF_CPMPLUGIN_LICENSE_INTERFACE_H_INCLUDED

