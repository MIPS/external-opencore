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
#ifndef PVMF_PARSERNODE_IMPL_H_INCLUDED
#include "pvmf_parsernode_impl.h"
#endif

PVMFParserNodePort::PVMFParserNodePort(int32 aPortTag
                                       , PVMFPortActivityHandler* aNode
                                       , const char* aName)
        : PvmfPortBaseImpl(aPortTag, aNode, aName)
{
    iParserNode = OSCL_STATIC_CAST(PVMFParserNodeImpl*, aNode);
    PvmiCapabilityAndConfigPortFormatImpl::Construct(PVMF_COMMONPARSER_PORT_OUTPUT_FORMATS,
            PVMF_COMMONPARSER_PORT_OUTPUT_FORMATS_VALTYPE);
}

PVMFParserNodePort::~PVMFParserNodePort()
{
}

void PVMFParserNodePort::QueryInterface(const PVUuid& aUuid, OsclAny*& aPtr)
{
    if (PVMI_CAPABILITY_AND_CONFIG_PVUUID == aUuid)
    {
        aPtr = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
    }
}

PVMFStatus PVMFParserNodePort::Connect(PVMFPortInterface* aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iParserNode->ipLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFParserNodePort::Connect: aPort=0x%x", aPort));

    if (!aPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iParserNode->ipLogger, PVLOGMSG_ERR,
                        (0, "PVMFParserNodePort::Connect: Error - Connecting to invalid port"));
        return PVMFErrArgument;
    }

    if (iConnectedPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iParserNode->ipLogger, PVLOGMSG_ERR,
                        (0, "PVMFParserNodePort::Connect: Error - Already connected"));
        return PVMFFailure;
    }

    // Send the PCM Info for WAV only
    if (iParserNode->iClipFmtType == PVMF_MIME_WAVFF)
    {
        // Query the Cap and Config Interface
        OsclAny* temp;
        aPort->QueryInterface(PVMI_CAPABILITY_AND_CONFIG_PVUUID, temp);
        PvmiCapabilityAndConfig* portInf = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, temp);
        if (portInf)
        {
            // Send the PCM info
            channelSampleInfo pcmInfo;
            pcmInfo.desiredChannels = iParserNode->iFileInfo.AudioTrkInfo.NoOfChannels;
            pcmInfo.samplingRate = iParserNode->iFileInfo.AudioTrkInfo.SamplingRate;
            pcmInfo.bitsPerSample = (iParserNode->iFileInfo.AudioTrkInfo.BitRate / iParserNode->iFileInfo.AudioTrkInfo.SamplingRate);
            pcmInfo.num_buffers = 0;
            pcmInfo.buffer_size = 0;

            // Pass this information to the port
            PvmiKvp kvp;
            kvp.key = (char*)_STRLIT_CHAR(PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM);
            kvp.value.key_specific_value = &pcmInfo;

            PvmiKvp* retKvp = NULL;
            portInf->setParametersSync(0, &kvp, 1, retKvp);
            if (NULL != retKvp)
                return PVMFFailure;
        }
    }

    //Automatically connect the peer.
    if (aPort->PeerConnect(this) != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iParserNode->ipLogger, PVLOGMSG_ERR,
                        (0, "PVMFParserNodePort::Connect: Error - Peer Connect failed"));
        return PVMFFailure;
    }

    iConnectedPort = aPort;

    PortActivity(PVMF_PORT_ACTIVITY_CONNECT);
    return PVMFSuccess;
}

bool PVMFParserNodePort::IsFormatSupported(PVMFFormatType aFormat)
{
    if (aFormat == PVMF_MIME_PCM ||
            aFormat == PVMF_MIME_PCM8 ||
            aFormat == PVMF_MIME_PCM16 ||
            aFormat == PVMF_MIME_PCM16_BE ||
            aFormat == PVMF_MIME_ULAW ||
            aFormat == PVMF_MIME_ALAW ||
            aFormat == PVMF_MIME_AMR_IETF ||
            aFormat == PVMF_MIME_AMR_IF2 ||
            aFormat == PVMF_MIME_AMRWB_IETF)
        return true;
    return false;
}

void PVMFParserNodePort::FormatUpdated()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iParserNode->ipLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFParserNodePort::FormatUpdated: %s", iFormat.getMIMEStrPtr()));
}



