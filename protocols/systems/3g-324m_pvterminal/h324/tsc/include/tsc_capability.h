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
// ----------------------------------------------------------------------
//
// This Software is an original work of authorship of PacketVideo Corporation.
// Portions of the Software were developed in collaboration with NTT  DoCoMo,
// Inc. or were derived from the public domain or materials licensed from
// third parties.  Title and ownership, including all intellectual property
// rights in and to the Software shall remain with PacketVideo Corporation
// and NTT DoCoMo, Inc.
//
// -----------------------------------------------------------------------
/*****************************************************************************/
/*  file name            : tsc_capability.h                                  */
/*  file contents        :                                                   */
/*  draw                 : '96.10.09                                         */
/*---------------------------------------------------------------------------*/
/*  amendment                                                                */
/*              Copyright (C) 1996 NTT DoCoMo                                */
/*****************************************************************************/

#ifndef TSCCAPABILILTY_H_INCLUDED
#define TSCCAPABILILTY_H_INCLUDED


#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#ifndef TSC_H_INCLUDED
#include "tsc.h"
#endif

#ifndef H245DEF_H_INCLUDED
#include "h245def.h"
#endif

#ifndef H245PRI_H_INCLUDED
#include "h245pri.h"
#endif

#ifndef TSC_CONSTANTS_H_INCLUDED
#include "tsc_constants.h"
#endif

#ifndef OSCL_MAP_H_INCLUDED
#include "oscl_map.h"
#endif

class TSC_statemanager;
class TSC_component;

class TSC_capability
{
    public:
        TSC_capability(TSC_statemanager& aTSCstatemanager) :
                iTSCstatemanager(aTSCstatemanager),
                iLogger(NULL),
                iTSCcomponent(NULL)
        {
            iLogger = PVLogger::GetLoggerObject("3g324m.h245user");
        };

        ~TSC_capability();

        void Reset();

        void SetMembers(TSC_component* aTSCcomponent);
        void InitVarsSession();
        void InitVarsLocal();

        /**
         * Set remote capability to a list that is be used to select outgoing codecs
         *
         * If null capability is used then table number entry is removed from the list
         *
         * @param aCapabilityTableNumber Table number that is used to update capabilities
         * @param apMediaCapability a capability to be added to the list
         *
         * @return void
         **/
        void SetRemoteCapability(uint8 aCapabilityTableNumber, CPvtMediaCapability* apMediaCapability);

        /**
         * Get remote capability for wanted codec.
         *
         * @param aCodecType a codec type
         *
         * @return CPvtMediaCapability media capablities for this codec, capability is not found
         *                             then NULL is returned
         **/
        CPvtMediaCapability*  GetRemoteCapability(PVCodecType_t aCodecType);

        /**
         * Get remote bitrate for wanted codec.
         *
         * @param aCodecType a codec type
         *
         * @return uint32 bitrate in kbit/s
         **/
        uint32 GetRemoteBitrate(PVCodecType_t codec_type);

        void ResetCapability();
        void ExtractTcsParameters(PS_VideoCapability apVideo, CPvtH263Capability *aMedia_capability);
        void ExtractTcsParameters(PS_VideoCapability pVideo, CPvtMpeg4Capability *aMedia_capability);
        void ExtractTcsParameters(PS_VideoCapability apVideo, CPvtAvcCapability *apMedia_capability);

        /**
         * Parse media capability from capability entry
         *
         * @param arCapability reference for capability entry
         *
         * @return pointer to media capability. Must be deleted after use.
         **/
        CPvtMediaCapability* ParseTcsCapabilities(S_Capability& arCapability);

        bool IsSegmentable(TPVDirection direction, PV2WayMediaType media_type);

        PS_DataType GetOutgoingDataType(PVCodecType_t codecType, uint32 bitrate, uint16 csi_len, uint8* csi);
        PS_H223LogicalChannelParameters GetOutgoingLcnParams(PV2WayMediaType media_type,
                PS_AdaptationLayerType adaptation_layer);
        PVMFStatus ValidateIncomingDataType(bool forRev, PS_DataType pDataType);
        PVMFStatus ValidateIncomingH223LcnParams(PS_H223LogicalChannelParameters h223params, TPVDirection dir);
        PVMFStatus ValidateForwardReverseParams(PS_ForwardReverseParam forRevParams, TPVDirection dir);
        bool VerifyReverseParameters(PS_ForwardReverseParam forRevParams,
                                     TSCObserver* aObserver,
                                     PVMFStatus& status);
        PS_DataType GetDataType(PVCodecType_t codecType, uint32 bitrate, const uint8* dci, uint16 dci_len);
        uint16 GetSupportedCodecCapabilityInfo(TPVDirection dir,
                                               PV2WayMediaType mediaType,
                                               Oscl_Vector<CodecCapabilityInfo*, OsclMemAllocator>& cci);
        bool HasSymmetryConstraint(Oscl_Vector<CodecCapabilityInfo*, OsclMemAllocator>& codec_list);
        CodecCapabilityInfo* SelectOutgoingCodec(Oscl_Vector<CodecCapabilityInfo*, OsclMemAllocator>* remote_list,
                Oscl_Vector<CodecCapabilityInfo*, OsclMemAllocator>* local_list);
        CodecCapabilityInfo* SelectOutgoingCodec(Oscl_Vector<CodecCapabilityInfo*, OsclMemAllocator>* remote_list);


        Oscl_Vector<PVMFVideoResolutionRange, OsclMemAllocator> GetResolutions(TPVDirection dir);
        void SetVideoResolutions(TPVDirection dir,
                                 Oscl_Vector<PVMFVideoResolutionRange, OsclMemAllocator>& resolutions);


    private:

        // -------------------------------------------------
        // Parameters extracted from incoming TCS (RAN-32K)
        // -------------------------------------------------
        uint32 iTcsIn_H263_sqcifMPI;        // Units 1/30 second
        uint32 iTcsIn_H263_qcifMPI;     // Units 1/30 second
        uint32 iTcsIn_H263_cifMPI;      // Units 1/30 second
        uint32 iTcsIn_H263_4cifMPI;     // Units 1/30 second
        uint32 iTcsIn_H263_16cifMPI;        // Units 1/30 second


        /* Video resolutions */
        Oscl_Vector<PVMFVideoResolutionRange, OsclMemAllocator> iResolutionsRx;
        Oscl_Vector<PVMFVideoResolutionRange, OsclMemAllocator> iResolutionsTx;

        TSC_statemanager& iTSCstatemanager;

        /* Capability of local and remote and mutual capabiliites */
        Oscl_Map<uint8, CPvtMediaCapability*, OsclMemAllocator> iRemoteCapability;

        PVLogger* iLogger;

        TSC_component* iTSCcomponent;
};

#endif

