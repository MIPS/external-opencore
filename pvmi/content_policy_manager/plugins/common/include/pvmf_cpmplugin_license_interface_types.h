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
#ifndef PVMF_CPMPLUGIN_LICENSE_INTERFACE_TYPES_H_INCLUDED
#define PVMF_CPMPLUGIN_LICENSE_INTERFACE_TYPES_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif
#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#define PVMF_CPM_DRM_ID_SIZE 16
#define PVMF_CPM_CONTENT_ID_SIZE PVMF_CPM_DRM_ID_SIZE
#define PVMF_CPM_METER_ID_SIZE PVMF_CPM_DRM_ID_SIZE

class PVMFCPMDrmId
{
    public:
        PVMFCPMDrmId()
        {
            Clear();
        }
        PVMFCPMDrmId(const PVMFCPMDrmId &aId)
        {
            Set((uint8*)aId.iData);
        }

        PVMFCPMDrmId operator=(const PVMFCPMDrmId &aId)
        {
            Set((uint8*)aId.iData);
            return *this;
        }

        void Clear()
        {
            oscl_memset(iData, 0, PVMF_CPM_DRM_ID_SIZE);
        }

        void Set(uint8 *aData)
        {
            if (aData)
            {
                oscl_memcpy(iData, aData, PVMF_CPM_DRM_ID_SIZE);
            }
        }
        uint8 iData[PVMF_CPM_DRM_ID_SIZE];
};

typedef PVMFCPMDrmId PVMFCPMContentId;
typedef PVMFCPMDrmId PVMFCPMMeterId;

//A class to hold detailed status information on communication with license servers.
// This information is primarily for debugging.
class PVMFCPMLicenseStatus
{
    public:
        PVMFCPMLicenseStatus():
                iNumLicenseChallengesSent(0)
                , iNumLicenseResponsesReceived(0)
                , iLastLicenseResponseResult(0)
                , iNumLicenseAckChallengesSent(0)
                , iNumLicenseAckResponsesReceived(0)
                , iLastLicenseAckResponseResult(0)
                , iNumJoinChallengesSent(0)
                , iNumJoinResponsesReceived(0)
                , iLastJoinResponseResult(0)
                , iNumLeaveChallengesSent(0)
                , iNumLeaveResponsesReceived(0)
                , iLastLeaveResponseResult(0)
                , iLastErrorResult(0)
        {}

        // The URL to which the license request was last sent.
        OSCL_HeapString<OsclMemAllocator> iLastLicenseURL;
        // Number of license challenges sent.
        uint32 iNumLicenseChallengesSent;
        // Number of license challenge responses received.
        uint32 iNumLicenseResponsesReceived;
        // The result of the last license response received.
        uint32 iLastLicenseResponseResult;

        // Number of license ack challenges sent.
        uint32 iNumLicenseAckChallengesSent;
        // Number of license ack challenge response received.
        uint32 iNumLicenseAckResponsesReceived;
        // The result of the last license ack response received.
        uint32 iLastLicenseAckResponseResult;


        // The URL to which the JoinDomain request was last sent.
        OSCL_HeapString<OsclMemAllocator> iLastJoinURL;
        // Number of JoinDomain challenges sent.
        uint32 iNumJoinChallengesSent;
        // Number of JoinDomain responses received.
        uint32 iNumJoinResponsesReceived;
        // The result of the last JoinDomain response received.
        uint32 iLastJoinResponseResult;

        // The URL to which the LeaveDomain request was last sent.
        OSCL_HeapString<OsclMemAllocator> iLastLeaveURL;
        // Number of LeaveDomain challenges sent.
        uint32 iNumLeaveChallengesSent;
        // Number of LeaveDomain responses received.
        uint32 iNumLeaveResponsesReceived;
        // The result of the last LeaveDomain response received.
        uint32 iLastLeaveResponseResult;

        // Last overall error result
        uint32 iLastErrorResult;

        void Clear()
        {
            iLastLicenseURL = "";
            iNumLicenseChallengesSent = iNumLicenseResponsesReceived = iLastLicenseResponseResult = 0;
            iNumLicenseAckChallengesSent = iNumLicenseAckResponsesReceived = iLastLicenseAckResponseResult = 0;

            iLastJoinURL = "";
            iNumJoinChallengesSent = iNumJoinResponsesReceived = iLastJoinResponseResult = 0;

            iLastLeaveURL = "";
            iNumLeaveChallengesSent = iNumLeaveResponsesReceived = iLastLeaveResponseResult = 0;

            iLastErrorResult = 0;
        }
        void Set(const PVMFCPMLicenseStatus& aStatus)
        {
            iLastLicenseURL = aStatus.iLastLicenseURL;
            iNumLicenseChallengesSent = aStatus.iNumLicenseChallengesSent;
            iNumLicenseResponsesReceived = aStatus.iNumLicenseResponsesReceived;
            iLastLicenseResponseResult = aStatus.iLastLicenseResponseResult;
            iNumLicenseAckChallengesSent = aStatus.iNumLicenseAckChallengesSent;
            iNumLicenseAckResponsesReceived = aStatus.iNumLicenseAckResponsesReceived;
            iLastLicenseAckResponseResult = aStatus.iLastLicenseAckResponseResult;
            iLastJoinURL = aStatus.iLastJoinURL;
            iNumJoinChallengesSent = aStatus.iNumJoinChallengesSent;
            iNumJoinResponsesReceived = aStatus.iNumJoinResponsesReceived;
            iLastJoinResponseResult = aStatus.iLastJoinResponseResult;
            iLastLeaveURL = aStatus.iLastLeaveURL;
            iNumLeaveChallengesSent = aStatus.iNumLeaveChallengesSent;
            iNumLeaveResponsesReceived = aStatus.iNumLeaveResponsesReceived;
            iLastLeaveResponseResult = aStatus.iLastLeaveResponseResult;
            iLastErrorResult = aStatus.iLastErrorResult;
        }
};

#endif //PVMF_CPMPLUGIN_LICENSE_INTERFACE_TYPES_H_INCLUDED

