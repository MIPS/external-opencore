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
#include "tsc_h324m_config.h"
#include "oscl_mem.h"
#include "pv_interface_cmd_message.h"
#include "pvlogger.h"

enum PVH234MessageType
{
    PVT_H324_COMMAND_QUERY_INTERFACE,
    PVT_H324_COMMAND_SET_H223_LEVEL,
    PVT_H324_COMMAND_SET_MAX_SDU_SIZE,
    PVT_H324_COMMAND_SET_MAX_SDU_SIZE_R,
    PVT_H324_COMMAND_SET_CODEC_PREFERENCE,
    PVT_H324_COMMAND_SET_FORMAT_SPECIFIC_INFO,
    PVT_H324_COMMAND_SEND_RME,
    PVT_H324_COMMAND_SET_AL2_SEQ_NUM,
    PVT_H324_COMMAND_SET_CONTROL_FIELD_OCTETS,
    PVT_H324_COMMAND_SET_MAX_PDU_SIZE,
    PVT_H324_COMMAND_SET_TERMINAL_TYPE,
    PVT_H324_COMMAND_SET_MAX_MUX_PDU_SIZE,
    PVT_H324_COMMAND_SET_MAX_MUX_CCSRL_SDU_SIZE,
    PVT_H324_COMMAND_SEND_RTD,
    PVT_H324_COMMAND_SET_VENDOR_ID,
    PVT_H324_COMMAND_SEND_VENDOR_ID,
    PVT_H324_COMMAND_SEND_END_SESSION,
    PVT_H324_COMMAND_SET_END_SESSION_TIMEOUT,
    PVT_H324_COMMAND_SET_AL_CONFIGURATION,
    PVT_H324_COMMAND_SET_TIMER_COUNTER,
    PVT_H324_COMMAND_SET_VIDEO_RESOLUTIONS,
    PVT_H324_COMMAND_SEND_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_COMMAND,
    PVT_H324_COMMAND_SEND_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_INDICATION,
    PVT_H324_COMMAND_SEND_SKEW_INDICATION,
    PVT_H324_COMMAND_SET_FAST_CSUP_OPTIONS,
    PVT_H324_COMMAND_SET_LOGICAL_CHANNEL_BUFFERING_MS,
    PVT_H324_COMMAND_SEND_USER_INPUT,
    PVT_H324_COMMAND_SET_WNSRP,
    PVT_H324_COMMAND_SEND_LOGICAL_CHANNEL_ACTIVE_INDICATION,
    PVT_H324_COMMAND_SEND_LOGICAL_CHANNEL_INACTIVE_INDICATION
};

class CPVH324InterfaceCmdMessage : public CPVCmnInterfaceCmdMessage
{
    public:
        CPVH324InterfaceCmdMessage(TPVCmnCommandType aType,
                                   OsclAny* aContextData,
                                   TPVCmnCommandId aId)
                : CPVCmnInterfaceCmdMessage(aType, aContextData)
        {
            SetId(aId);
        }
        PVMFCmdResp GetResponse(PVMFCommandId aId,
                                const OsclAny* aContext,
                                PVMFStatus aStatus,
                                OsclAny* aEventData = NULL)
        {
            return PVMFCmdResp(aId, aContext, aStatus, aEventData);
        }
    private:
        //PVMFCmdResp iResp;
};

class PVH324MessageQueryInterface : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageQueryInterface(const PVUuid& aUuid,
                                    PVInterface*& aInterfacePtr,
                                    OsclAny* aContextData,
                                    TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_QUERY_INTERFACE, aContextData, aId),
                iUuid(aUuid),
                iInterfacePtr(aInterfacePtr)
        {
            iInterfacePtr->addRef();
        }

        virtual ~PVH324MessageQueryInterface()
        {
            iInterfacePtr->removeRef();
        }

        PVUuid iUuid;
        PVInterface*& iInterfacePtr;
};

class PVH324MessageSetH223Level : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetH223Level(TPVH223Level aLevel,
                                  OsclAny* aContextData,
                                  TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_H223_LEVEL, aContextData, aId)
                , iH223Level(aLevel)
        {}

        TPVH223Level iH223Level;
};

class PVH324MessageSetMaxSduSize : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetMaxSduSize(TPVAdaptationLayer aAl,
                                   uint32 aSize,
                                   OsclAny* aContextData,
                                   TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_MAX_SDU_SIZE, aContextData, aId)
                , iAl(aAl)
                , iSize(aSize)
        {}

        TPVAdaptationLayer iAl;
        uint32 iSize;
};

class PVH324MessageSetMaxSduSizeR : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetMaxSduSizeR(TPVAdaptationLayer aAl,
                                    uint32 aSize,
                                    OsclAny* aContextData,
                                    TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_MAX_SDU_SIZE_R, aContextData, aId)
                , iAl(aAl)
                , iSize(aSize)
        {}

        TPVAdaptationLayer iAl;
        uint32 iSize;
};

class PVH324MessageSetCodecPreference : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetCodecPreference(Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aIncomingAudio,
                                        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aIncomingVideo,
                                        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aOutgoingAudio,
                                        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aOutgoingVideo,
                                        OsclAny* aContextData,
                                        TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_CODEC_PREFERENCE, aContextData, aId)
                , iIncomingAudio(aIncomingAudio)
                , iIncomingVideo(aIncomingVideo)
                , iOutgoingAudio(aOutgoingAudio)
                , iOutgoingVideo(aOutgoingVideo)
        {}

        Oscl_Vector<PVMFFormatType, OsclMemAllocator>iIncomingAudio;
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>iIncomingVideo;
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>iOutgoingAudio;
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>iOutgoingVideo;
};

class PVH324MessageSetFormatSpecificInfo : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetFormatSpecificInfo(PVMFFormatType aMediaFormat, OsclAny* aContextData, TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_FORMAT_SPECIFIC_INFO, aContextData, aId)
                , iMediaFormat(aMediaFormat)
                , ipFormatSpecificInfo(NULL)
                , iFormatSpecificInfoLen(0)
        {}

        ~PVH324MessageSetFormatSpecificInfo()
        {
            if (ipFormatSpecificInfo)
                OSCL_DEFAULT_FREE(ipFormatSpecificInfo);
        }

        bool SetFormatSpecificInfo(const uint8* apFormatSpecificInfo, uint32 aFormatSpecificInfoLen)
        {
            if (ipFormatSpecificInfo) OSCL_DEFAULT_FREE(ipFormatSpecificInfo);
            ipFormatSpecificInfo = NULL;
            iFormatSpecificInfoLen = 0;
            ipFormatSpecificInfo = (uint8*)OSCL_DEFAULT_MALLOC(aFormatSpecificInfoLen);
            if (ipFormatSpecificInfo)
            {
                iFormatSpecificInfoLen = aFormatSpecificInfoLen;
                oscl_memcpy(ipFormatSpecificInfo, apFormatSpecificInfo, iFormatSpecificInfoLen);
            }
            return (ipFormatSpecificInfo) ? true : false;
        }

        PVMFFormatType iMediaFormat;
        uint8* ipFormatSpecificInfo;
        uint32 iFormatSpecificInfoLen;
};

class PVH324MessageSendRme : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSendRme(OsclAny* aContextData,
                             TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_RME, aContextData, aId)
        {}
};

class PVH324MessageSetAl2SequenceNumbers : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetAl2SequenceNumbers(int32 aSeqNumWidth,
                                           OsclAny* aContextData,
                                           TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_AL2_SEQ_NUM, aContextData, aId)
                , iSeqNumWidth(aSeqNumWidth)
        {}

        int32 iSeqNumWidth;
};

class PVH324MessageSetAl3ControlFieldOctets : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetAl3ControlFieldOctets(int32 aCfo,
                                              OsclAny* aContextData,
                                              TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_CONTROL_FIELD_OCTETS, aContextData, aId)
                , iCfo(aCfo)
        {}

        int32 iCfo;
};

class PVH324MessageSetMaxPduSize : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetMaxPduSize(int32 aMaxPduSize,
                                   OsclAny* aContextData,
                                   TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_MAX_PDU_SIZE, aContextData, aId)
                , iMaxPduSize(aMaxPduSize)
        {}

        int32 iMaxPduSize;
};

class PVH324MessageSetTerminalType : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetTerminalType(uint8 aTerminalType,
                                     OsclAny* aContextData,
                                     TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_TERMINAL_TYPE, aContextData, aId)
                , iTerminalType(aTerminalType)
        {}

        uint8 iTerminalType;
};

class PVH324MessageSetMaxMuxPduSize : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetMaxMuxPduSize(int32 aMaxMuxPduSize,
                                      OsclAny* aContextData,
                                      TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_MAX_MUX_PDU_SIZE, aContextData, aId)
                , iMaxMuxPduSize(aMaxMuxPduSize)
        {}

        int32 iMaxMuxPduSize;
};

class PVH324MessageSetMaxMuxCcsrlSduSize : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetMaxMuxCcsrlSduSize(int32 aMaxMuxCcsrlSduSize,
                                           OsclAny* aContextData,
                                           TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_MAX_MUX_CCSRL_SDU_SIZE, aContextData, aId)
                , iMaxMuxCcsrlSduSize(aMaxMuxCcsrlSduSize)
        {}

        int32 iMaxMuxCcsrlSduSize;
};

class PVH324MessageSendRtd : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSendRtd(OsclAny* aContextData,
                             TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_RTD, aContextData, aId)
        {}
};

class PVH324MessageSetVendorId : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetVendorId(uint8 cc, uint8 ext, uint32 mc,
                                 OsclAny* aContextData,
                                 TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_VENDOR_ID, aContextData, aId),
                iCc(cc), iExt(ext), iMc(mc),
                iProduct(NULL), iProductLen(0), iVersion(NULL), iVersionLen(0)
        {}
        ~PVH324MessageSetVendorId()
        {
            if (iProduct)
                OSCL_DEFAULT_FREE(iProduct);
            if (iVersion)
                OSCL_DEFAULT_FREE(iVersion);
        }

        bool SetProduct(const uint8* product, uint16 product_len)
        {
            if (iProduct) OSCL_DEFAULT_FREE(iProduct);
            iProduct = NULL;
            iProductLen = 0;
            iProduct = (uint8*)OSCL_DEFAULT_MALLOC(product_len);
            if (iProduct)
            {
                iProductLen = product_len;
                oscl_memcpy(iProduct, product, iProductLen);
            }
            return (iProduct) ? true : false;
        }
        bool SetVersion(const uint8* version, uint16 version_len)
        {
            if (iVersion) OSCL_DEFAULT_FREE(iVersion);
            iVersion = NULL;
            iVersionLen = 0;
            iVersion = (uint8*)OSCL_DEFAULT_MALLOC(version_len);
            if (iVersion)
            {
                iVersionLen = version_len;
                oscl_memcpy(iVersion, version, iVersionLen);
            }
            return (iVersion) ? true : false;
        }
        uint8 iCc;
        uint8 iExt;
        uint32 iMc;
        uint8* iProduct;
        uint16 iProductLen;
        uint8* iVersion;
        uint16 iVersionLen;
};

class PVH324MessageSendVendorId : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSendVendorId(OsclAny* aContextData, TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_VENDOR_ID, aContextData, aId)
        {}
};

class PVH324MessageEndSession : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageEndSession(OsclAny* aContextData, TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_END_SESSION, aContextData, aId)
        {}
};

class PVH324MessageSetEndSessionTimeout : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetEndSessionTimeout(uint32 timeout,
                                          OsclAny* aContextData,
                                          TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_END_SESSION_TIMEOUT, aContextData, aId),
                iTimeout(timeout)
        {}
        uint32 iTimeout;
};

class PVH324MessageSetALConfiguration : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetALConfiguration(TPVMediaType_t aMediaType,
                                        TPVAdaptationLayer aLayer,
                                        bool aAllow,
                                        bool aUse,
                                        OsclAny* aContextData,
                                        TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_AL_CONFIGURATION, aContextData, aId),
                iMediaType(aMediaType),
                iLayer(aLayer),
                iAllow(aAllow),
                iUse(aUse)
        {}
        TPVMediaType_t iMediaType;
        TPVAdaptationLayer iLayer;
        bool iAllow;
        bool iUse;
};

class PVH324MessageSetTimerCounter: public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetTimerCounter(TPVH324TimerCounter aTimerCounter,
                                     uint8 aSeries,
                                     uint32 aSeriesOffset,
                                     uint32 aValue,
                                     OsclAny* aContextData,
                                     TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_TIMER_COUNTER, aContextData, aId),
                iTimerCounter(aTimerCounter),
                iSeries(aSeries),
                iSeriesOffset(aSeriesOffset),
                iValue(aValue)
        {}
        TPVH324TimerCounter iTimerCounter;
        uint8 iSeries;
        uint32 iSeriesOffset;
        uint32 iValue;
};

class PVH324MessageSetVideoResolutions : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetVideoResolutions(TPVDirection aDirection,
                                         Oscl_Vector<PVMFVideoResolutionRange, OsclMemAllocator>& aResolutions,
                                         OsclAny* aContextData,
                                         TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_VIDEO_RESOLUTIONS, aContextData, aId),
                iDirection(aDirection),
                iResolutions(aResolutions)
        {}
        TPVDirection iDirection;
        Oscl_Vector<PVMFVideoResolutionRange, OsclMemAllocator> iResolutions;
};

class PVH324MessageSendVideoSpatialTemporalTradeoffCommand : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSendVideoSpatialTemporalTradeoffCommand(TPVChannelId aLogicalChannel,
                uint8 aTradeoff,
                OsclAny* aContextData,
                TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_COMMAND, aContextData, aId),
                iLogicalChannel(aLogicalChannel),
                iTradeoff(aTradeoff)
        {}
        TPVChannelId iLogicalChannel;
        uint8 iTradeoff;
};

class PVH324MessageSendVideoSpatialTemporalTradeoffIndication : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSendVideoSpatialTemporalTradeoffIndication(TPVChannelId aLogicalChannel,
                uint8 aTradeoff,
                OsclAny* aContextData,
                TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_INDICATION, aContextData, aId),
                iLogicalChannel(aLogicalChannel),
                iTradeoff(aTradeoff)
        {}
        TPVChannelId iLogicalChannel;
        uint8 iTradeoff;
};

class PVH324MessageSendLogicalChannelActiveIndication : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSendLogicalChannelActiveIndication(TPVChannelId aLogicalChannel,
                OsclAny* aContextData,
                TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_LOGICAL_CHANNEL_ACTIVE_INDICATION, aContextData, aId),
                iLogicalChannel(aLogicalChannel)
        {}
        TPVChannelId iLogicalChannel;
};

class PVH324MessageSendLogicalChannelInactiveIndication : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSendLogicalChannelInactiveIndication(TPVChannelId aLogicalChannel,
                OsclAny* aContextData,
                TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_LOGICAL_CHANNEL_INACTIVE_INDICATION, aContextData, aId),
                iLogicalChannel(aLogicalChannel)
        {}
        TPVChannelId iLogicalChannel;
};

class PVH324MessageSendSkewIndication : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSendSkewIndication(TPVChannelId aLogicalChannel1,
                                        TPVChannelId aLogicalChannel2,
                                        uint16 aSkew,
                                        OsclAny* aContextData,
                                        TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SEND_SKEW_INDICATION, aContextData, aId),
                iLogicalChannel1(aLogicalChannel1),
                iLogicalChannel2(aLogicalChannel2),
                iSkew(aSkew)
        {}
        TPVChannelId iLogicalChannel1;
        TPVChannelId iLogicalChannel2;
        uint16 iSkew;
};

class PVH324MessageSetLogicalChannelBufferingMs : public CPVCmnInterfaceCmdMessage
{
    public:
        PVH324MessageSetLogicalChannelBufferingMs(uint32 aInBufferingMs,
                uint32 aOutBufferingMs,
                OsclAny* aContextData,
                TPVCmnCommandId aId)
                : CPVCmnInterfaceCmdMessage(PVT_H324_COMMAND_SET_LOGICAL_CHANNEL_BUFFERING_MS, aContextData),
                iInLogicalChannelBufferingMs(aInBufferingMs),
                iOutLogicalChannelBufferingMs(aOutBufferingMs)
        {
            OSCL_UNUSED_ARG(aId);
        }
        uint32 iInLogicalChannelBufferingMs;
        uint32 iOutLogicalChannelBufferingMs;
};

class PVH324MessageSendUserInput: public CPVCmnInterfaceCmdMessage
{
    public:
        PVH324MessageSendUserInput(OsclAny* aContextData,
                                   TPVCmnCommandId aId)
                : CPVCmnInterfaceCmdMessage(PVT_H324_COMMAND_SEND_USER_INPUT, aContextData),
                iUserInput(NULL)
        {
            OSCL_UNUSED_ARG(aId);
        }
        ~PVH324MessageSendUserInput()
        {
            SetUserInput(NULL);
        }
        void SetUserInput(CPVUserInput* input)
        {
            if (iUserInput)
            {
                iUserInput->removeRef();
                iUserInput = NULL;
            }
            if (input)
            {
                iUserInput = input;
                iUserInput->addRef();
            }
        }
        CPVUserInput* iUserInput;
};

class PVH324MessageSetWnsrp : public CPVH324InterfaceCmdMessage
{
    public:
        PVH324MessageSetWnsrp(bool wnsrp,
                              OsclAny* aContextData,
                              TPVCmnCommandId aId)
                : CPVH324InterfaceCmdMessage(PVT_H324_COMMAND_SET_WNSRP, aContextData, aId),
                iWnsrp(wnsrp)
        {}
        bool iWnsrp;
};

class PVH324MessageUtils
{
    public:
        static void DestroyMessage(CPVCmnInterfaceCmdMessage *aCmd)
        {
            switch (aCmd->GetType())
            {
                case PVT_H324_COMMAND_QUERY_INTERFACE:
                    OSCL_DELETE((PVH324MessageQueryInterface*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_H223_LEVEL:
                    OSCL_DELETE((PVH324MessageSetH223Level*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_MAX_SDU_SIZE:
                    OSCL_DELETE((PVH324MessageSetMaxSduSize*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_MAX_SDU_SIZE_R:
                    OSCL_DELETE((PVH324MessageSetMaxSduSizeR*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_CODEC_PREFERENCE:
                    OSCL_DELETE((PVH324MessageSetCodecPreference*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_FORMAT_SPECIFIC_INFO:
                    OSCL_DELETE((PVH324MessageSetFormatSpecificInfo*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_RME:
                    OSCL_DELETE((PVH324MessageSendRme*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_AL2_SEQ_NUM:
                    OSCL_DELETE((PVH324MessageSetAl2SequenceNumbers*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_CONTROL_FIELD_OCTETS:
                    OSCL_DELETE((PVH324MessageSetAl3ControlFieldOctets*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_MAX_PDU_SIZE:
                    OSCL_DELETE((PVH324MessageSetMaxPduSize*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_TERMINAL_TYPE:
                    OSCL_DELETE((PVH324MessageSetTerminalType*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_MAX_MUX_PDU_SIZE:
                    OSCL_DELETE((PVH324MessageSetMaxMuxPduSize*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_MAX_MUX_CCSRL_SDU_SIZE:
                    OSCL_DELETE((PVH324MessageSetMaxMuxCcsrlSduSize*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_RTD:
                    OSCL_DELETE((PVH324MessageSendRtd*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_VENDOR_ID:
                    OSCL_DELETE((PVH324MessageSetVendorId*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_VENDOR_ID:
                    OSCL_DELETE((PVH324MessageSendVendorId*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_END_SESSION:
                    OSCL_DELETE((PVH324MessageEndSession*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_END_SESSION_TIMEOUT:
                    OSCL_DELETE((PVH324MessageSetEndSessionTimeout*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_AL_CONFIGURATION:
                    OSCL_DELETE((PVH324MessageSetALConfiguration*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_TIMER_COUNTER:
                    OSCL_DELETE((PVH324MessageSetTimerCounter*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_VIDEO_RESOLUTIONS:
                    OSCL_DELETE((PVH324MessageSetVideoResolutions*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_COMMAND:
                    OSCL_DELETE((PVH324MessageSendVideoSpatialTemporalTradeoffCommand*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_INDICATION:
                    OSCL_DELETE((PVH324MessageSendVideoSpatialTemporalTradeoffIndication*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_LOGICAL_CHANNEL_ACTIVE_INDICATION:
                    OSCL_DELETE((PVH324MessageSendLogicalChannelActiveIndication*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_LOGICAL_CHANNEL_INACTIVE_INDICATION:
                    OSCL_DELETE((PVH324MessageSendLogicalChannelInactiveIndication*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_SKEW_INDICATION:
                    OSCL_DELETE((PVH324MessageSendSkewIndication*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_LOGICAL_CHANNEL_BUFFERING_MS:
                    OSCL_DELETE((PVH324MessageSetLogicalChannelBufferingMs*)aCmd);
                    break;
                case PVT_H324_COMMAND_SEND_USER_INPUT:
                    OSCL_DELETE((PVH324MessageSendUserInput*)aCmd);
                    break;
                case PVT_H324_COMMAND_SET_WNSRP:
                    OSCL_DELETE((PVH324MessageSetWnsrp*)aCmd);
                    break;
                default:
                    break;
            }
        }
};


/////////////////////////////////////////////////
// H324MConfig
/////////////////////////////////////////////////
H324MConfig::H324MConfig(TSC_324m *aH324M, bool aUseAO) :
        OsclActiveObject(OsclActiveObject::EPriorityNominal, "H324MConfig"),
        iH324M(aH324M),
        iReferenceCount(1),
        iLogger(NULL),
        iCommandId(1),
        iObserver(NULL),
        iUseAO(aUseAO)
{
    iLogger = PVLogger::GetLoggerObject("3g324m.h324mconfig");
    if (iUseAO)
    {
        AddToScheduler();
    }

    iH324M->SetTSC_324mObserver((TSC_324mObserver*)this);
}

H324MConfig::~H324MConfig()
{
    // Gkl
    iH324M->SetTSC_324mObserver(NULL);
}

void H324MConfig::SetObserver(H324MConfigObserver* aObserver)
{
    iObserver = aObserver;
}

void H324MConfig::Run()
{
    unsigned i = 0;
    for (i = 0; i < iPendingResponses.size(); i++)
    {
        iObserver->H324MConfigCommandCompletedL(iPendingResponses[i]);
    }
    iPendingResponses.clear();

    for (i = 0; i < iPendingEvents.size(); i++)
    {
        iObserver->H324MConfigHandleInformationalEventL(iPendingEvents[i]);
    }
    iPendingEvents.clear();
}

PVMFCommandId H324MConfig::SetMultiplexLevel(TPVH223Level aLevel, OsclAny* aContextData)
{
    iH324M->SetMultiplexLevel(aLevel);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetMaxSduSize(TPVAdaptationLayer aLayer, int32 aSize, OsclAny* aContextData)
{
    iH324M->SetSduSize(OUTGOING, (uint16)aSize, aLayer);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetMaxSduSizeR(TPVAdaptationLayer aLayer, int32 aSize, OsclAny* aContextData)
{
    iH324M->SetSduSize(INCOMING, (uint16)aSize, aLayer);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetCodecPreference(Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aIncomingAudio,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aIncomingVideo,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aOutgoingAudio,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aOutgoingVideo,
        OsclAny* aContextData)
{
    iH324M->SetCodecPreference(aIncomingAudio, aIncomingVideo, aOutgoingAudio, aOutgoingVideo);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetFormatSpecificInfo(PVMFFormatType aMediaFormat, const uint8* apFormatSpecificInfo,
        uint32 aFormatSpecificInfoLen, OsclAny* aContextData)
{
    iH324M->SetFormatSpecificInfo(aMediaFormat, apFormatSpecificInfo, aFormatSpecificInfoLen);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetAl2SequenceNumbers(int32 aSeqNumWidth, OsclAny* aContextData)
{
    iH324M->SetAl2Sn(aSeqNumWidth);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetAl3ControlFieldOctets(int32 aCfo, OsclAny* aContextData)
{
    iH324M->SetAl3ControlFieldOctets(aCfo);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetOutoingPduType(TPVH223MuxPduType aOutgoingPduType, OsclAny* aContextData)
{
    iH324M->SetMaxOutgoingPduSize((uint16)aOutgoingPduType);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetMaxPduSize(int32 aMaxPduSize, OsclAny* aContextData)
{
    iH324M->SetMaxMuxPduSize((uint16)aMaxPduSize);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetTerminalType(uint8 aTerminalType, OsclAny* aContextData)
{
    iH324M->SetTerminalType(aTerminalType);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetALConfiguration(TPVMediaType_t aMediaType, TPVAdaptationLayer aLayer,
        bool aAllow, bool aUse, OsclAny* aContextData)
{
    iH324M->SetAlConfig(aMediaType, aLayer, aAllow, aUse);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SendRme(OsclAny* aContextData)
{
    iH324M->RmeSendReq();
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};


PVMFCommandId H324MConfig::SetMaxMuxPduSize(int32 aRequestMaxMuxPduSize, OsclAny* aContextData)
{
    iH324M->SetMaxMuxPduSize((uint16)aRequestMaxMuxPduSize);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetMaxMuxCcsrlSduSize(int32 aMaxCcsrlSduSize, OsclAny* aContextData)
{
    iH324M->SetMaxCcsrlSduSize(aMaxCcsrlSduSize);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::FastUpdate(PVMFNodeInterface& aTrack, OsclAny* aContextData)
{
    OSCL_UNUSED_ARG(aTrack);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SendRtd(OsclAny* aContextData)
{
    iH324M->RtdTrfReq();
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetVendor(uint8 aCc, uint8 aExt, uint32 aMc,
                                     const uint8* aProduct, uint16 aProductLen,
                                     const uint8* aVersion, uint16 aVersionLen,
                                     OsclAny* aContextData)
{
    TPVH245Vendor* h245vendor = OSCL_NEW(TPVVendorH221NonStandard,
                                         (aCc, aExt, aMc));
    iH324M->SetVendorIdInfo(h245vendor,
                            aProduct, aProductLen,
                            aVersion, aVersionLen);
    OSCL_DELETE(h245vendor);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SendEndSession(OsclAny* aContextData)
{
    iH324M->EndSessionCommand();
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetEndSessionTimeout(uint32 aTimeout, OsclAny* aContextData)
{
    iH324M->SetEndSessionTimeout(aTimeout);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}

PVMFCommandId H324MConfig::SetTimerCounter(TPVH324TimerCounter aTimerCounter,
        uint8 aSeries, uint32 aSeriesOffset,
        uint32 aValue,
        OsclAny* aContextData)
{
    iH324M->SetTimerCounter(aTimerCounter, aSeries, aSeriesOffset, aValue);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SetVideoResolutions(TPVDirection aDirection,
        Oscl_Vector<PVMFVideoResolutionRange, OsclMemAllocator>& aResolutions,
        OsclAny* aContextData)
{
    iH324M->SetVideoResolutions(aDirection, aResolutions);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SendVendorId(OsclAny* aContextData)
{
    iH324M->Tsc_IdcVi();
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
};

PVMFCommandId H324MConfig::SendVideoTemporalSpatialTradeoffCommand(TPVChannelId aLogicalChannel, uint8 aTradeoff,
        OsclAny* aContextData)
{
    iH324M->SendVideoTemporalSpatialTradeoffCommand(aLogicalChannel, aTradeoff);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}

PVMFCommandId H324MConfig::SendVideoTemporalSpatialTradeoffIndication(TPVChannelId aLogicalChannel, uint8 aTradeoff,
        OsclAny* aContextData)
{
    iH324M->SendVideoTemporalSpatialTradeoffIndication(aLogicalChannel, aTradeoff);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}

PVMFCommandId H324MConfig::SendLogicalChannelActiveIndication(TPVChannelId aLogicalChannel,
        OsclAny* aContextData)
{
    iH324M->SendLogicalChannelActiveIndication(aLogicalChannel);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}


PVMFCommandId H324MConfig::SendLogicalChannelInactiveIndication(TPVChannelId aLogicalChannel,
        OsclAny* aContextData)
{
    iH324M->SendLogicalChannelInactiveIndication(aLogicalChannel);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}

PVMFCommandId H324MConfig::SendSkewIndication(TPVChannelId aLogicalChannel1, TPVChannelId aLogicalChannel2, uint16 aSkew,
        OsclAny* aContextData)
{
    iH324M->SendSkewIndication(aLogicalChannel1, aLogicalChannel2, aSkew);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}


PVMFCommandId
H324MConfig::SetLogicalChannelBufferingMs(uint32 aInBufferingMs,
        uint32 aOutBufferingMs,
        OsclAny* aContextData)
{
    iH324M->SetLogicalChannelBufferingMs(aInBufferingMs, aOutBufferingMs);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}

PVMFCommandId
H324MConfig::SendUserInput(CPVUserInput* aUserInput,
                           OsclAny* aContextData)
{
    switch (aUserInput->GetType())
    {
        case PV_ALPHANUMERIC:
            CPVUserInputAlphanumeric *alpha;

            alpha = (CPVUserInputAlphanumeric *) aUserInput;

            if ((alpha->GetInput() == NULL) || (alpha->GetLength() == 0))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "H324MConfig::SendUserInput invalid alphanumeric string, ptr %x, len %d!\n",
                                 alpha->GetInput(), alpha->GetLength()));
                OSCL_LEAVE(PVMFErrArgument);
            }
            iH324M->Tsc_UII_Alphanumeric(alpha->GetInput(), alpha->GetLength());
            break;

        case PV_DTMF:
            CPVUserInputDtmf *dtmf;
            dtmf = (CPVUserInputDtmf *) aUserInput;
            iH324M->Tsc_UII_DTMF(dtmf->GetInput(), dtmf->GetDuration());
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "H324MConfig::SendUserInput invalid user input type!\n"));
            OSCL_LEAVE(PVMFErrArgument);
            break;
    }
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}

PVMFCommandId H324MConfig::SetWnsrp(const bool aEnableWnsrp,
                                    OsclAny* aContextData)
{
    iH324M->SetWnsrp(aEnableWnsrp);
    SendCmdResponse(iCommandId, aContextData, PVMFSuccess);
    return iCommandId++;
}

////////////////////////////////////
// PVInterface virtuals
////////////////////////////////////
void H324MConfig::addRef()
{
    iReferenceCount++;
}

void H324MConfig::removeRef()
{
    if (--iReferenceCount <= 0)
    {
        OSCL_DELETE(this);
    }
}

bool H324MConfig::queryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr)
{
    aInterfacePtr = NULL;

    if (aUuid == PVUidProxiedInterface)
    {
        H324MProxiedInterface* proxied_interface_ptr = NULL;
        proxied_interface_ptr = OSCL_NEW(H324MProxiedInterface, ());
        proxied_interface_ptr->SetH324M(iH324M);
        aInterfacePtr = proxied_interface_ptr;
        return true;
    }
    else
    {
        // could have several interfaces we may be looking for here.
        //here add way to get the component interface.
        // do in steps.  First- you know what you want.
        if (iH324M->QueryInterface(0, aUuid, aInterfacePtr))
            return true;
    }

    return false;
}

void H324MConfig::SendCmdResponse(PVMFCommandId id, OsclAny* context, PVMFStatus status)
{
    PVMFCmdResp resp(id, context, status);
    if (iUseAO)
    {
        iPendingResponses.push_back(resp);
        RunIfNotReady();
    }
    else
    {
        iObserver->H324MConfigCommandCompletedL(resp);
    }
}

void H324MConfig::SendAsyncEvent(PVMFAsyncEvent& event)
{

    if (!iUseAO)
    {
        iObserver->H324MConfigHandleInformationalEventL(event);
    }
    else
    {
        // If message includes inferface addRef is called
        PVInterface* pInterface = event.GetEventExtensionInterface();
        if (pInterface)
        {
            pInterface->addRef();
        }
        iPendingEvents.push_back(event);
        RunIfNotReady();
    }
}

void H324MConfig::IncomingVendorId(TPVH245Vendor* vendor,
                                   const uint8* pn, uint16 pn_len,
                                   const uint8* vn, uint16 vn_len)
{
    OSCL_UNUSED_ARG(vendor);
    OSCL_UNUSED_ARG(pn);
    OSCL_UNUSED_ARG(pn_len);
    OSCL_UNUSED_ARG(vn);
    OSCL_UNUSED_ARG(vn_len);
}

void H324MConfig::UserInputReceived(CPVUserInput* apUserInput)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H324MConfig::UserInputReceived\n"));

    PVMFAsyncEvent event(PVMFInfoEvent, PV_INDICATION_USER_INPUT, NULL, apUserInput, NULL);
    SendAsyncEvent(event);
}

void H324MConfig::UserInputCapabilityReceived(CPVUserInputCapability* apUserInputCapability)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "H324MConfig::UserInputCapabilityReceived\n"));

    PVMFAsyncEvent event(PVMFInfoEvent, PV_INDICATION_USER_INPUT_CAPABILITY, NULL, apUserInputCapability, NULL);
    SendAsyncEvent(event);
}

void H324MConfig::VideoSpatialTemporalTradeoffCommandReceived(CPVVideoSpatialTemporalTradeoff* apVideoSpatial)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,
                    "H324MConfig::VideoSpatialTemporalTradeoffCommandReceived\n"));
    PVMFAsyncEvent event(PVMFInfoEvent, PV_INDICATION_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_COMMAND,
                         apVideoSpatial, NULL);
    SendAsyncEvent(event);
}

void H324MConfig::VideoSpatialTemporalTradeoffIndicationReceived(CPVVideoSpatialTemporalTradeoff* apVideoSpatial)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,
                    "H324MConfig::VideoSpatialTemporalTradeoffIndicationReceived\n"));
    PVMFAsyncEvent event(PVMFInfoEvent, PV_INDICATION_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_INDICATION,
                         apVideoSpatial, NULL);
    SendAsyncEvent(event);
}


void H324MConfig::LogicalChannelActiveIndicationReceived(CPVLogicalChannelIndication* apLCIndication)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,
                    "H324MConfig::LogicalChannelActiveIndicationReceived\n"));
    PVMFAsyncEvent event(PVMFInfoEvent, PV_INDICATION_LOGICAL_CHANNEL_ACTIVE,
                         apLCIndication, NULL);
    SendAsyncEvent(event);
}


void H324MConfig::LogicalChannelInactiveIndicationReceived(CPVLogicalChannelIndication* apLCIndication)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,
                    "H324MConfig::LogicalChannelInactiveIndicationReceived\n"));
    PVMFAsyncEvent event(PVMFInfoEvent, PV_INDICATION_LOGICAL_CHANNEL_INACTIVE,
                         apLCIndication, NULL);
    SendAsyncEvent(event);
}

void H324MConfig::SkewIndicationReceived(TPVChannelId lcn1, TPVChannelId lcn2, uint16 skew)
{
    PVMFAsyncEvent event(PVMFInfoEvent, PV_INDICATION_SKEW, NULL, NULL);
    oscl_memset(event.GetLocalBuffer(), 0, PVMF_ASYNC_EVENT_LOCAL_BUF_SIZE);
    event.GetLocalBuffer()[0] = (uint8)((lcn1 >> 8) & 0xFF);
    event.GetLocalBuffer()[1] = (uint8)(lcn1 & 0xFF);
    event.GetLocalBuffer()[2] = (uint8)((lcn2 >> 8) & 0xFF);
    event.GetLocalBuffer()[3] = (uint8)(lcn2 & 0xFF);
    event.GetLocalBuffer()[4] = (uint8)((skew >> 8) & 0xFF);
    event.GetLocalBuffer()[5] = (uint8)(skew & 0xFF);
    SendAsyncEvent(event);
}

///////////////////////////////////////
// H324MConfigProxied
///////////////////////////////////////
H324MConfigProxied::H324MConfigProxied(H324MConfigInterface *aH324MConfigIF, PVMainProxy *aMainProxy) :
        iH324MConfigIF(aH324MConfigIF),
        iMainProxy(aMainProxy),
        iLoggerClient(NULL),
        iLoggerServer(NULL),
        iReferenceCount(1),
        iProxyId(0),
        iCommandId(1),
        iObserver(NULL)
{
    iProxyId = iMainProxy->RegisterProxiedInterface(*this, *this);
    iH324MConfigIF->addRef();
    iH324MConfigIF->SetObserver(this);
    iLoggerServer = PVLogger::GetLoggerObject("3g324m.h324mconfig.proxied.server");
}

H324MConfigProxied::~H324MConfigProxied()
{
    iMainProxy->UnregisterProxiedInterface(iProxyId);
    Oscl_Map<PVMFCommandId, CPVCmnInterfaceCmdMessage*, OsclMemAllocator>::iterator it = iPendingCmds.begin();
    while (it != iPendingCmds.end())
    {
        CPVCmnInterfaceCmdMessage* msg = (*it++).second;
        if (msg)PVH324MessageUtils::DestroyMessage(msg);
    }
    iPendingCmds.clear();
    iH324MConfigIF->removeRef();
}

void H324MConfigProxied::addRef()
{
    iReferenceCount++;
}

void H324MConfigProxied::removeRef()
{
    if (--iReferenceCount <= 0)
    {
        OSCL_DELETE(this);
    }
}

bool H324MConfigProxied::queryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr)
{
    aInterfacePtr = NULL;

    iH324MConfigIF->queryInterface(aUuid, aInterfacePtr);

    if (aInterfacePtr)
        return true;
    return false;

}

void H324MConfigProxied::SetObserver(H324MConfigObserver* aObserver)
{
    iObserver = aObserver;
}

void H324MConfigProxied::H324MConfigCommandCompletedL(PVMFCmdResp& aResponse)
{
    CPVCmnInterfaceCmdMessage* cmdMsg = iPendingCmds[aResponse.GetCmdId()];
    if (cmdMsg == NULL)
        return;
    CPVH324InterfaceCmdMessage* h324CmdMsg = OSCL_STATIC_CAST(CPVH324InterfaceCmdMessage*, cmdMsg);

    PVMFCmdResp *response = OSCL_NEW(PVMFCmdResp, (h324CmdMsg->GetResponse(aResponse.GetCmdId(),
                                     (OsclAny*)aResponse.GetContext(),
                                     aResponse.GetCmdStatus(),
                                     aResponse.GetEventData())));
    iMainProxy->SendNotification(iProxyId, response);
}

void H324MConfigProxied::H324MConfigHandleInformationalEventL(PVMFAsyncEvent& aNotification)
{
    PVMFAsyncEvent* event = NULL;
    event = OSCL_NEW(PVMFAsyncEvent, (aNotification));
    iMainProxy->SendNotification(iProxyId, event);
}

PVMFCommandId H324MConfigProxied::SetMultiplexLevel(TPVH223Level aLevel, OsclAny* aContextData)
{
    PVH324MessageSetH223Level *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetH223Level, (aLevel, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetMaxSduSize(TPVAdaptationLayer aLayer, int32 aSize, OsclAny* aContextData)
{
    PVH324MessageSetMaxSduSize *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetMaxSduSize, (aLayer, aSize, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetMaxSduSizeR(TPVAdaptationLayer aLayer, int32 aSize, OsclAny* aContextData)
{
    PVH324MessageSetMaxSduSizeR *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetMaxSduSizeR, (aLayer, aSize, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetCodecPreference(Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aIncomingAudio,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aIncomingVideo,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aOutgoingAudio,
        Oscl_Vector<PVMFFormatType, OsclMemAllocator>& aOutgoingVideo,
        OsclAny* aContextData)
{
    PVH324MessageSetCodecPreference *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetCodecPreference, (aIncomingAudio, aIncomingVideo, aOutgoingAudio, aOutgoingVideo, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetFormatSpecificInfo(PVMFFormatType aMediaFormat, const uint8* apFormatSpecificInfo,
        uint32 aFormatSpecificInfoLen, OsclAny* aContextData)
{
    PVH324MessageSetFormatSpecificInfo *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetFormatSpecificInfo, (aMediaFormat, aContextData, iCommandId));
    cmd->SetFormatSpecificInfo(apFormatSpecificInfo,  aFormatSpecificInfoLen);
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
};

PVMFCommandId H324MConfigProxied::SetAl2SequenceNumbers(int32 aSeqNumWidth, OsclAny* aContextData)
{
    PVH324MessageSetAl2SequenceNumbers *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetAl2SequenceNumbers, (aSeqNumWidth, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetAl3ControlFieldOctets(int32 aCfo, OsclAny* aContextData)
{
    PVH324MessageSetAl3ControlFieldOctets *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetAl3ControlFieldOctets, (aCfo, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetMaxPduSize(int32 aMaxPduSize, OsclAny* aContextData)
{
    PVH324MessageSetMaxPduSize *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetMaxPduSize, (aMaxPduSize, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetTerminalType(uint8 aTerminalType, OsclAny* aContextData)
{
    PVH324MessageSetTerminalType *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetTerminalType, (aTerminalType, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetALConfiguration(TPVMediaType_t aMediaType, TPVAdaptationLayer aLayer,
        bool aAllow, bool aUse, OsclAny* aContextData)
{
    PVH324MessageSetALConfiguration *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetALConfiguration,
                   (aMediaType, aLayer, aAllow, aUse, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SendRme(OsclAny* aContextData)
{
    PVH324MessageSendRme *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendRme, (aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetMaxMuxPduSize(int32 aRequestMaxMuxPduSize, OsclAny* aContextData)
{
    PVH324MessageSetMaxMuxPduSize *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetMaxMuxPduSize, (aRequestMaxMuxPduSize, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetMaxMuxCcsrlSduSize(int32 aMaxCcsrlSduSize, OsclAny* aContextData)
{
    PVH324MessageSetMaxMuxCcsrlSduSize *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetMaxMuxCcsrlSduSize, (aMaxCcsrlSduSize, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::FastUpdate(PVMFNodeInterface& aTrack, OsclAny* aContextData)
{
    OSCL_UNUSED_ARG(aTrack);
    OSCL_UNUSED_ARG(aContextData);
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SendRtd(OsclAny* aContextData)
{
    PVH324MessageSendRtd *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendRtd, (aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SetVendor(uint8 aCc, uint8 aExt, uint32 aMc,
        const uint8* aProduct, uint16 aProductLen,
        const uint8* aVersion, uint16 aVersionLen,
        OsclAny* aContextData)
{
    PVH324MessageSetVendorId *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetVendorId, (aCc, aExt, aMc, aContextData, iCommandId));
    cmd->SetProduct(aProduct, aProductLen);
    cmd->SetVersion(aVersion, aVersionLen);
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
};

PVMFCommandId H324MConfigProxied::SendEndSession(OsclAny* aContextData)
{
    PVH324MessageEndSession *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageEndSession, (aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
};

PVMFCommandId H324MConfigProxied::SetEndSessionTimeout(uint32 aTimeout, OsclAny* aContextData)
{
    PVH324MessageSetEndSessionTimeout *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetEndSessionTimeout, (aTimeout, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
};

PVMFCommandId H324MConfigProxied::SetTimerCounter(TPVH324TimerCounter aTimerCounter,
        uint8 aSeries, uint32 aSeriesOffset,
        uint32 aValue,
        OsclAny* aContextData)
{
    PVH324MessageSetTimerCounter *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetTimerCounter, (aTimerCounter, aSeries, aSeriesOffset, aValue, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
};

PVMFCommandId H324MConfigProxied::SetVideoResolutions(TPVDirection aDirection,
        Oscl_Vector<PVMFVideoResolutionRange, OsclMemAllocator>& aResolutions,
        OsclAny* aContextData)
{
    PVH324MessageSetVideoResolutions *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetVideoResolutions, (aDirection, aResolutions, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
};

PVMFCommandId H324MConfigProxied::SendVendorId(OsclAny* aContextData)
{
    PVH324MessageSendVendorId *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendVendorId, (aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
};

PVMFCommandId H324MConfigProxied::SendVideoTemporalSpatialTradeoffCommand(TPVChannelId aLogicalChannel, uint8 aTradeoff,
        OsclAny* aContextData)
{
    PVH324MessageSendVideoSpatialTemporalTradeoffCommand *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendVideoSpatialTemporalTradeoffCommand, (aLogicalChannel, aTradeoff, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SendVideoTemporalSpatialTradeoffIndication(TPVChannelId aLogicalChannel, uint8 aTradeoff,
        OsclAny* aContextData)
{
    PVH324MessageSendVideoSpatialTemporalTradeoffIndication *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendVideoSpatialTemporalTradeoffIndication, (aLogicalChannel, aTradeoff, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId H324MConfigProxied::SendLogicalChannelActiveIndication(TPVChannelId aLogicalChannel,
        OsclAny* aContextData)
{
    PVH324MessageSendLogicalChannelActiveIndication *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendLogicalChannelActiveIndication, (aLogicalChannel, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}


PVMFCommandId H324MConfigProxied::SendLogicalChannelInactiveIndication(TPVChannelId aLogicalChannel,
        OsclAny* aContextData)
{
    PVH324MessageSendLogicalChannelInactiveIndication *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendLogicalChannelInactiveIndication, (aLogicalChannel, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}


PVMFCommandId H324MConfigProxied::SendSkewIndication(TPVChannelId aLogicalChannel1, TPVChannelId aLogicalChannel2, uint16 aSkew,
        OsclAny* aContextData)
{
    PVH324MessageSendSkewIndication *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendSkewIndication, (aLogicalChannel1, aLogicalChannel2, aSkew, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId
H324MConfigProxied::SetLogicalChannelBufferingMs(uint32 aInBufferingMs,
        uint32 aOutBufferingMs,
        OsclAny* aContextData)
{
    PVH324MessageSetLogicalChannelBufferingMs*cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetLogicalChannelBufferingMs, (aInBufferingMs, aOutBufferingMs, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId
H324MConfigProxied::SendUserInput(CPVUserInput* aUserInput,
                                  OsclAny* aContextData)
{
    PVH324MessageSendUserInput*cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSendUserInput, (aContextData, iCommandId));
    cmd->SetUserInput(aUserInput);
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

PVMFCommandId
H324MConfigProxied::SetWnsrp(const bool aEnableWnsrp,
                             OsclAny* aContextData)
{
    PVH324MessageSetWnsrp *cmd = NULL;
    cmd = OSCL_NEW(PVH324MessageSetWnsrp, (aEnableWnsrp, aContextData, iCommandId));
    int32 error = 0;
    OSCL_TRY(error, iMainProxy->SendCommand(iProxyId, cmd));
    OSCL_FIRST_CATCH_ANY(error, PVH324MessageUtils::DestroyMessage(cmd););
    return iCommandId++;
}

void H324MConfigProxied::CleanupNotification(TPVProxyMsgId aId, OsclAny *aMsg)
{
    OSCL_UNUSED_ARG(aId);
    PVMFEventBase* event = OSCL_STATIC_CAST(PVMFEventBase*, aMsg);
    if (event)
        OSCL_DELETE(event);
}

void H324MConfigProxied::HandleCommand(TPVProxyMsgId aMsgId, OsclAny *aMsg)
{
    OSCL_UNUSED_ARG(aMsgId);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                    (0, "H324MConfigProxied::HandleCommand"));
    PVMFCommandId commandId = 0;

    switch (((CPVCmnInterfaceCmdMessage*)aMsg)->GetType())
    {
        case PVT_H324_COMMAND_QUERY_INTERFACE:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Query Interface"));
            {
                PVH324MessageQueryInterface* query_msg = OSCL_STATIC_CAST(PVH324MessageQueryInterface*, aMsg);
                if (query_msg)
                {
                    commandId = iH324MConfigIF->queryInterface(query_msg->iUuid,
                                query_msg->iInterfacePtr);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
            //
        case PVT_H324_COMMAND_SET_H223_LEVEL :
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set H223 Level"));
            {
                PVH324MessageSetH223Level* set_level_msg = OSCL_STATIC_CAST(PVH324MessageSetH223Level*, aMsg);
                if (set_level_msg)
                {
                    commandId = iH324MConfigIF->SetMultiplexLevel(set_level_msg->iH223Level,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_MAX_SDU_SIZE:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Max Sdu Size"));
            {
                PVH324MessageSetMaxSduSize* msg = OSCL_STATIC_CAST(PVH324MessageSetMaxSduSize*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetMaxSduSize(msg->iAl, msg->iSize,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_MAX_SDU_SIZE_R:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Max Sdu Size Remote"));
            {
                PVH324MessageSetMaxSduSizeR* msg = OSCL_STATIC_CAST(PVH324MessageSetMaxSduSizeR*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetMaxSduSizeR(msg->iAl, msg->iSize,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_CODEC_PREFERENCE:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Codec Preference"));
            {
                PVH324MessageSetCodecPreference* msg = OSCL_STATIC_CAST(PVH324MessageSetCodecPreference*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetCodecPreference(msg->iIncomingAudio, msg->iIncomingVideo,
                                msg->iOutgoingAudio, msg->iOutgoingVideo,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_FORMAT_SPECIFIC_INFO:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Format Specific Info"));
            {
                PVH324MessageSetFormatSpecificInfo* msg = OSCL_STATIC_CAST(PVH324MessageSetFormatSpecificInfo*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetFormatSpecificInfo(msg->iMediaFormat, msg->ipFormatSpecificInfo,
                                msg->iFormatSpecificInfoLen, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;

        case PVT_H324_COMMAND_SEND_RME:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Send RME"));
            {
                PVH324MessageSendRme* msg = OSCL_STATIC_CAST(PVH324MessageSendRme*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendRme((CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_AL2_SEQ_NUM:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set AL2 Sequence Numbers"));
            {
                PVH324MessageSetAl2SequenceNumbers* msg = OSCL_STATIC_CAST(PVH324MessageSetAl2SequenceNumbers*,
                        aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetAl2SequenceNumbers(msg->iSeqNumWidth,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_CONTROL_FIELD_OCTETS:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set AL3 Control Field Octets"));
            {
                PVH324MessageSetAl3ControlFieldOctets* msg = OSCL_STATIC_CAST(PVH324MessageSetAl3ControlFieldOctets*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetAl3ControlFieldOctets(msg->iCfo,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_MAX_PDU_SIZE:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Max Pdu Size"));
            {
                PVH324MessageSetMaxPduSize* msg = OSCL_STATIC_CAST(PVH324MessageSetMaxPduSize*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetMaxMuxPduSize(msg->iMaxPduSize,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_MAX_MUX_PDU_SIZE:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Max Mux Pdu Size"));
            {
                PVH324MessageSetMaxMuxPduSize* msg = OSCL_STATIC_CAST(PVH324MessageSetMaxMuxPduSize*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetMaxMuxPduSize(msg->iMaxMuxPduSize,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_MAX_MUX_CCSRL_SDU_SIZE:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Max CCSRL Sdu Size"));
            {
                PVH324MessageSetMaxMuxCcsrlSduSize* msg = OSCL_STATIC_CAST(PVH324MessageSetMaxMuxCcsrlSduSize*,
                        aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetMaxMuxCcsrlSduSize(msg->iMaxMuxCcsrlSduSize,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SEND_RTD:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Send RTD"));
            {
                PVH324MessageSendRtd* msg = OSCL_STATIC_CAST(PVH324MessageSendRtd*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendRtd((CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_VENDOR_ID:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Vendor ID"));
            {
                PVH324MessageSetVendorId* msg = OSCL_STATIC_CAST(PVH324MessageSetVendorId*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetVendor(msg->iCc, msg->iExt, msg->iMc,
                                                          msg->iProduct, msg->iProductLen,
                                                          msg->iVersion, msg->iVersionLen,
                                                          (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SEND_END_SESSION:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Send End Session"));
            {
                PVH324MessageEndSession* msg = OSCL_STATIC_CAST(PVH324MessageEndSession*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendEndSession((CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_END_SESSION_TIMEOUT:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set End Session Timeout"));
            {
                PVH324MessageSetEndSessionTimeout* msg = OSCL_STATIC_CAST(PVH324MessageSetEndSessionTimeout*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetEndSessionTimeout(msg->iTimeout,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_AL_CONFIGURATION:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Adaptation Layer Config"));
            {
                PVH324MessageSetALConfiguration* msg = OSCL_STATIC_CAST(PVH324MessageSetALConfiguration*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetALConfiguration(msg->iMediaType, msg->iLayer,
                                msg->iAllow, msg->iUse, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_TIMER_COUNTER:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Timer Counter"));
            {
                PVH324MessageSetTimerCounter* msg = OSCL_STATIC_CAST(PVH324MessageSetTimerCounter*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetTimerCounter(msg->iTimerCounter,
                                msg->iSeries, msg->iSeriesOffset, msg->iValue, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_VIDEO_RESOLUTIONS:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Video Resolutions"));
            {
                PVH324MessageSetVideoResolutions* msg = OSCL_STATIC_CAST(PVH324MessageSetVideoResolutions*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetVideoResolutions(msg->iDirection, msg->iResolutions,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SEND_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_COMMAND:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Spatial temporal tradeoff command"));
            {
                PVH324MessageSendVideoSpatialTemporalTradeoffCommand* msg = OSCL_STATIC_CAST(PVH324MessageSendVideoSpatialTemporalTradeoffCommand*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendVideoTemporalSpatialTradeoffCommand(msg->iLogicalChannel, msg->iTradeoff, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SEND_VIDEO_SPATIAL_TEMPORAL_TRADEOFF_INDICATION:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Spatial temporal tradeoff indication"));
            {
                PVH324MessageSendVideoSpatialTemporalTradeoffIndication* msg =
                    OSCL_STATIC_CAST(PVH324MessageSendVideoSpatialTemporalTradeoffIndication*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendVideoTemporalSpatialTradeoffIndication(msg->iLogicalChannel, msg->iTradeoff, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SEND_LOGICAL_CHANNEL_ACTIVE_INDICATION:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Logical Channel Active indication"));
            {
                PVH324MessageSendLogicalChannelActiveIndication* msg =
                    OSCL_STATIC_CAST(PVH324MessageSendLogicalChannelActiveIndication*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendLogicalChannelActiveIndication(msg->iLogicalChannel, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;

        case PVT_H324_COMMAND_SEND_LOGICAL_CHANNEL_INACTIVE_INDICATION:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Logical Channel inactive indication"));
            {
                PVH324MessageSendLogicalChannelInactiveIndication* msg =
                    OSCL_STATIC_CAST(PVH324MessageSendLogicalChannelInactiveIndication*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendLogicalChannelInactiveIndication(msg->iLogicalChannel, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;



        case PVT_H324_COMMAND_SEND_VENDOR_ID:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Send vendor id"));
            {
                PVH324MessageSendVendorId* msg = OSCL_STATIC_CAST(PVH324MessageSendVendorId*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendVendorId((CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SEND_SKEW_INDICATION:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Send skew indication"));
            {
                PVH324MessageSendSkewIndication* msg = OSCL_STATIC_CAST(PVH324MessageSendSkewIndication*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendSkewIndication(msg->iLogicalChannel1,
                                msg->iLogicalChannel2, msg->iSkew, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_LOGICAL_CHANNEL_BUFFERING_MS:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Logical Channel Buffering"));
            {
                PVH324MessageSetLogicalChannelBufferingMs* msg = OSCL_STATIC_CAST(PVH324MessageSetLogicalChannelBufferingMs*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetLogicalChannelBufferingMs(msg->iInLogicalChannelBufferingMs,
                                msg->iOutLogicalChannelBufferingMs, (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SEND_USER_INPUT:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Send user input"));
            {
                PVH324MessageSendUserInput* msg = OSCL_STATIC_CAST(PVH324MessageSendUserInput*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SendUserInput(msg->iUserInput,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_WNSRP:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set WNSRP"));
            {
                PVH324MessageSetWnsrp* msg = OSCL_STATIC_CAST(PVH324MessageSetWnsrp*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetWnsrp(msg->iWnsrp,
                                                         (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        case PVT_H324_COMMAND_SET_TERMINAL_TYPE :
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                            (0, "H324MConfigProxied::HandleCommand - Set Terminal Type"));
            {
                PVH324MessageSetTerminalType* msg = OSCL_STATIC_CAST(PVH324MessageSetTerminalType*, aMsg);
                if (msg)
                {
                    commandId = iH324MConfigIF->SetTerminalType(msg->iTerminalType,
                                (CPVCmnInterfaceCmdMessage*)aMsg);
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                                    (0, "H324MConfigProxied::HandleCommand - Failed to cast"));
                }
            }
            break;
        default:
            PVH324MessageUtils::DestroyMessage((CPVCmnInterfaceCmdMessage*)aMsg);
            return;
    }
    // Store command id and msg
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
                    (0, "H324MConfigProxied::HandleCommand Adding command id=%d to pending queue", commandId));
    iPendingCmds[commandId] = (CPVCmnInterfaceCmdMessage*)aMsg;
}

void H324MConfigProxied::HandleNotification(TPVProxyMsgId aId, OsclAny *aMsg)
{
    OSCL_UNUSED_ARG(aId);
    PVMFEventBase* event = OSCL_STATIC_CAST(PVMFEventBase*, aMsg);
    // PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
    //                 (0, "H324MConfigProxied::HandleNotification Event type=%d", event->IsA()));

    if (event->IsA() == PVMFCmdRespEvent)
    {
        PVMFCmdResp* cmdResp = OSCL_STATIC_CAST(PVMFCmdResp*, event);
        // PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLoggerServer, PVLOGMSG_STACK_TRACE,
        //               (0, "H324MConfigProxied::HandleNotification Event Command completioin received for command id=%d",
        //              cmdResp->GetCmdId()));
        CPVCmnInterfaceCmdMessage* cmdMsg = iPendingCmds[cmdResp->GetCmdId()];
        if (cmdMsg)
        {
            PVMFCmdResp response(cmdMsg->GetCommandId(), cmdMsg->GetContextData(),
                                 cmdResp->GetCmdStatus());
            if (iObserver)
            {
                iObserver->H324MConfigCommandCompletedL(response);
            }
            iPendingCmds.erase(cmdResp->GetCmdId());
            PVH324MessageUtils::DestroyMessage(cmdMsg);
        }
        OSCL_DELETE((PVMFAsyncEvent*)event);
    }
    else if (event->IsA() == PVMFInfoEvent)
    {
        PVMFAsyncEvent* async_event = OSCL_STATIC_CAST(PVMFAsyncEvent*, event);
        if (iObserver)
        {
            iObserver->H324MConfigHandleInformationalEventL(*async_event);
        }
        PVInterface* pExtensionInterface = async_event->GetEventExtensionInterface();
        if (pExtensionInterface)
        {
            pExtensionInterface->removeRef();
        }
        OSCL_DELETE(async_event);
    }
    else if (event->IsA() == PVMFErrorEvent)
    {
        OSCL_DELETE((PVMFAsyncEvent*)event);
    }
}

void H324MConfigProxied::CleanupCommand(TPVProxyMsgId aId, OsclAny *aMsg)
{
    OSCL_UNUSED_ARG(aId);
    PVMFEventBase* event = OSCL_STATIC_CAST(PVMFEventBase*, aMsg);
    if (event->IsA() == PVMFCmdRespEvent)
    {
        PVMFCmdResp* cmdResp = OSCL_STATIC_CAST(PVMFCmdResp*, event);
        CPVCmnInterfaceCmdMessage* cmdMsg = iPendingCmds[cmdResp->GetCmdId()];
        if (cmdMsg)
        {
            OSCL_DELETE(cmdMsg);
            iPendingCmds.erase(cmdResp->GetCmdId());
        }
    }
    else if (event->IsA() == PVMFInfoEvent)
    {
        OSCL_DELETE((PVMFAsyncEvent*)event);
    }
    else if (event->IsA() == PVMFErrorEvent)
    {
        OSCL_DELETE((PVMFAsyncEvent*)event);
    }
}



///////////////////////////////////////
// H324MProxiedInterface
///////////////////////////////////////
H324MProxiedInterface::H324MProxiedInterface() : iH324M(NULL), iMainProxy(NULL), iReferenceCount(1) {}
H324MProxiedInterface::~H324MProxiedInterface() {}

void H324MProxiedInterface::QueryProxiedInterface(const TPVProxyUUID &aUuid,
        PVInterface *&aInterfacePtr)
{
    if (aUuid == PVH324MConfigUuid)
    {
        H324MConfig *h324mconfig = NULL;
        h324mconfig = OSCL_NEW(H324MConfig, (iH324M, true));
        aInterfacePtr = OSCL_NEW(H324MConfigProxied, (h324mconfig, iMainProxy));
        h324mconfig->removeRef();
    }
}
void H324MProxiedInterface::SetMainProxy(PVMainProxy * aMainProxy)
{
    iMainProxy = aMainProxy;
}

void H324MProxiedInterface::addRef()
{
    iReferenceCount++;
}

void H324MProxiedInterface::removeRef()
{
    if (--iReferenceCount <= 0)
        OSCL_DELETE(this);
}
bool H324MProxiedInterface::queryInterface(const PVUuid&, PVInterface*&)
{
    return false;
}
void H324MProxiedInterface::SetH324M(TSC_324m *aH324M)
{
    iH324M = aH324M;
}




