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
#ifndef PVMF_CPMPLUGIN_METERING_INTERFACE_TYPES_H_INCLUDED
#define PVMF_CPMPLUGIN_METERING_INTERFACE_TYPES_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_TYPES_H_INCLUDED
#include "oscl_types.h"
#endif

#include "pvmf_cpmplugin_license_interface_types.h"


//A class to hold information about a metering certificate
class PVMFCPMMeterCertInfo
{
    public:
        PVMFCPMMeterCertInfo(): iValid(false)
        {}

        //Tells whether data in this class is valid
        bool iValid;

        //The ID of this metering certificate
        PVMFCPMMeterId iMeterId;

        //The URL of the metering service.
        OSCL_wHeapString<OsclMemAllocator> iURL;

        void Clear()
        {
            iValid = false;
            iURL = _STRLIT_WCHAR("");
            oscl_memset(iMeterId.iData, 0, sizeof(iMeterId.iData));
        }
        void Set(const PVMFCPMMeterCertInfo& aInfo)
        {
            iValid = aInfo.iValid;
            if (iValid)
            {
                iMeterId = aInfo.iMeterId;
                iURL = aInfo.iURL;
            }
        }
};

//A class to hold meter status information
class PVMFCPMMeterStatus
{
    public:
        PVMFCPMMeterStatus():
                iNumMeterCertChallengesSent(0)
                , iNumMeterCertResponsesReceived(0)
                , iLastMeterCertResponseResult(0)
                , iNumMeterChallengesSent(0)
                , iNumMeterResponsesReceived(0)
                , iLastMeterResponseResult(0)
        {}

        //Information about the last entry retrieved from
        //the meter cert store.
        PVMFCPMMeterCertInfo iMeterCertInfo;


        //Number of meter cert challenges sent
        uint32 iNumMeterCertChallengesSent;

        //Number of meter cert responses received
        uint32 iNumMeterCertResponsesReceived;

        //Result of processing the last meter cert response
        uint32 iLastMeterCertResponseResult;

        //Number of meter challenges sent
        uint32 iNumMeterChallengesSent;

        //Number of meter responses received
        uint32 iNumMeterResponsesReceived;

        //result of processing the last meter response
        uint32 iLastMeterResponseResult;

        //The URL of the  metering server for this content.
        OSCL_HeapString<OsclMemAllocator> iLastMeterURL;

        void Clear()
        {
            iMeterCertInfo.Clear();
            iNumMeterCertChallengesSent = 0;
            iNumMeterCertResponsesReceived = 0;
            iLastMeterCertResponseResult = 0;
            iNumMeterChallengesSent = 0;
            iNumMeterResponsesReceived = 0;
            iLastMeterResponseResult = 0;
            iLastMeterURL = "";
        }
        void Set(const PVMFCPMMeterStatus& aStatus)
        {
            iMeterCertInfo.Set(aStatus.iMeterCertInfo);
            iNumMeterCertChallengesSent = aStatus.iNumMeterCertChallengesSent;
            iNumMeterCertResponsesReceived = aStatus.iNumMeterCertResponsesReceived;
            iLastMeterCertResponseResult = aStatus.iLastMeterCertResponseResult;
            iNumMeterChallengesSent = aStatus.iNumMeterChallengesSent;
            iNumMeterResponsesReceived = aStatus.iNumMeterResponsesReceived;
            iLastMeterResponseResult = aStatus.iLastMeterResponseResult;
            iLastMeterURL = aStatus.iLastMeterURL;
        }
};

class PVMFCPMMeteringData
{
    public:
        PVMFCPMMeteringData()
        {
            /*
            ** Default value of iMaxDataSize is zero, which instructs client to submit entire metering
            ** data in one http message.
            */
            iMaxDataSize = 0;
        }

        //URL from where metering certificate can be obtained
        OSCL_HeapString<OsclMemAllocator> iMeteringCertServerUrl;

        //Metering ID of the service
        PVMFCPMMeterId iMID;

        //Maximum size of metering packets that the client should send to the server.
        uint32 iMaxDataSize;

        //CustomData string to be sent to the server with Metering challenge. It may be blank.
        OSCL_HeapString<OsclMemAllocator> iMeteringCustomData;

        //CustomData string to be sent to the server with Meter Cert challenge. It may be blank.
        OSCL_HeapString<OsclMemAllocator> iMeterCertCustomData;
};

#endif //PVMF_CPMPLUGIN_METERING_INTERFACE_TYPES_H_INCLUDED

