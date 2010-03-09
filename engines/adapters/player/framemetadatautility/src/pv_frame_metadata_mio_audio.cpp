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
#include "pv_frame_metadata_mio_audio.h"
#include "pvlogger.h"
#include "pv_mime_string_utils.h"
#include "oscl_snprintf.h"
#include "cczoomrotationbase.h"

PVFMAudioMIO::PVFMAudioMIO() :
        PVFMMIO()
{
    InitData();
}


void PVFMAudioMIO::InitData()
{
    iAudioFormat = PVMF_MIME_FORMAT_UNKNOWN;
    iAudioSamplingRateValid = false;
    iAudioNumChannelsValid = false;
    iIsMIOConfigured = false;
    iAudioSamplingRate = 0;
    iAudioNumChannels = 0;

    // Init the input format capabilities vector
    iInputFormatCapability.clear();
    iInputFormatCapability.push_back(PVMF_MIME_PCM);
    iInputFormatCapability.push_back(PVMF_MIME_PCM8);
    iInputFormatCapability.push_back(PVMF_MIME_PCM16);
    iInputFormatCapability.push_back(PVMF_MIME_PCM16_BE);
    iInputFormatCapability.push_back(PVMF_MIME_ULAW);
    iInputFormatCapability.push_back(PVMF_MIME_ALAW);

    PVFMMIO::InitData();
}


void PVFMAudioMIO::ResetData()
{
    Cleanup();

    // Reset all the received media parameters.
    iAudioFormat = PVMF_MIME_FORMAT_UNKNOWN;
    iAudioSamplingRateValid = false;
    iAudioNumChannelsValid = false;
    iAudioSamplingRate = 0;
    iAudioNumChannels = 0;
}

PVFMAudioMIO::~PVFMAudioMIO()
{
    Cleanup();

}

PVMFCommandId PVFMAudioMIO::writeAsync(uint8 aFormatType, int32 aFormatIndex, uint8* aData, uint32 aDataLen,
                                       const PvmiMediaXferHeader& data_header_info, OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aData);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMAudioMIO::writeAsync() seqnum %d ts %d context %d", data_header_info.seq_num, data_header_info.timestamp, aContext));

    PVMFStatus status = PVMFFailure;

    // Do a leave if MIO is not configured except when it is an EOS
    if (!iIsMIOConfigured &&
            !((PVMI_MEDIAXFER_FMT_TYPE_NOTIFICATION == aFormatType) &&
              (PVMI_MEDIAXFER_FMT_INDEX_END_OF_STREAM == aFormatIndex)))
    {
        iWriteBusy = true;
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    switch (aFormatType)
    {
        case PVMI_MEDIAXFER_FMT_TYPE_COMMAND :
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMAudioMIO::writeAsync() called with Command info."));
            // Ignore
            status = PVMFSuccess;
            break;

        case PVMI_MEDIAXFER_FMT_TYPE_NOTIFICATION :
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMAudioMIO::writeAsync() called with Notification info."));
            switch (aFormatIndex)
            {
                case PVMI_MEDIAXFER_FMT_INDEX_END_OF_STREAM:
                    break;

                default:
                    break;
            }
            // Ignore
            status = PVMFSuccess;
            break;

        case PVMI_MEDIAXFER_FMT_TYPE_DATA :
            switch (aFormatIndex)
            {
                case PVMI_MEDIAXFER_FMT_INDEX_FMT_SPECIFIC_INFO:
                    // Format-specific info contains codec headers.
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMAudioMIO::writeAsync() called with format-specific info."));

                    if (iState < STATE_INITIALIZED)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVFMAudioMIO::writeAsync: Error - Invalid state"));
                        iWriteBusy = true;
                        OSCL_LEAVE(OsclErrInvalidState);
                        return -1;
                    }
                    else
                    {
                        if (aDataLen > 0)
                        {
                            status = PVMFSuccess;
                        }
                        else
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMAudioMIO::writeAsync() called aDataLen==0."));
                            status = PVMFSuccess;
                        }
                    }
                    break;

                case PVMI_MEDIAXFER_FMT_INDEX_DATA:
                    // Data contains the media bitstream.

                    // Verify the state
                    if (iState != STATE_STARTED)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVFMAudioMIO::writeAsync: Error - Invalid state"));
                        iWriteBusy = true;
                        OSCL_LEAVE(OsclErrInvalidState);
                        return -1;
                    }
                    else
                    {

                        if (aDataLen > 0)
                        {
                            status = PVMFSuccess;
                        }
                        else
                        {
                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO, (0, "PVFMAudioMIO::writeAsync() called aDataLen==0."));
                            status = PVMFSuccess;
                        }
                    }
                    break;

                default:
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVFMAudioMIO::writeAsync: Error - unrecognized format index"));
                    status = PVMFFailure;
                    break;
            }
            break;

        default:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_REL, iLogger, PVLOGMSG_ERR, (0, "PVFMAudioMIO::writeAsync: Error - unrecognized format type"));
            status = PVMFFailure;
            break;
    }

    //Schedule asynchronous response
    PVMFCommandId cmdid = iCommandCounter++;
    WriteResponse resp(status, cmdid, aContext, data_header_info.timestamp);
    iWriteResponseQueue.push_back(resp);
    RunIfNotReady();
    return cmdid;

}

PVMFStatus PVFMAudioMIO::getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier, PvmiKvp*& aParameters,
        int& num_parameter_elements, PvmiCapabilityContext aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMAudioMIO::getParametersSync() called"));

    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);

    aParameters = NULL;
    num_parameter_elements = 0;

    if (pv_mime_strcmp(aIdentifier, INPUT_FORMATS_CAP_QUERY) == 0)
    {
        // This is a query for the list of supported formats.
        // This component supports any audio format
        // Generate a list of all the PVMF audio formats...

        uint32 count = iInputFormatCapability.size();
        aParameters = (PvmiKvp*)oscl_malloc(count * sizeof(PvmiKvp));

        if (aParameters)
        {
            num_parameter_elements = 0;
            Oscl_Vector<PVMFFormatType, OsclMemAllocator>::iterator it;
            for (it = iInputFormatCapability.begin(); it != iInputFormatCapability.end(); it++)
            {
                aParameters[num_parameter_elements++].value.pChar_value = OSCL_STATIC_CAST(char*, it->getMIMEStrPtr());
            }
            return PVMFSuccess;
        }
        return PVMFErrNoMemory;
    }

    // Other queries are not currently supported so report as unrecognized key.
    return PVMFFailure;
}

void PVFMAudioMIO::setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements, PvmiKvp*& aRet_kvp)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMAudioMIO::setParametersSync() called"));

    OSCL_UNUSED_ARG(aSession);

    aRet_kvp = NULL;

    for (int32 i = 0; i < num_elements; i++)
    {
        //Check against known audio parameter keys...
        if (pv_mime_strcmp(aParameters[i].key, MOUT_AUDIO_FORMAT_KEY) == 0)
        {
            iAudioFormat = aParameters[i].value.pChar_value;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMAudioMIO::setParametersSync() Audio Format Key, Value %s", iAudioFormat.getMIMEStrPtr()));
        }
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
        {
            //  iOutputFile.Write(aParameters[i].value.pChar_value, sizeof(uint8), (int32)aParameters[i].capacity);
        }
        //All FSI for audio will be set here in one go
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY_PCM) == 0)
        {
            channelSampleInfo* pcm16Info = (channelSampleInfo*)aParameters->value.key_specific_value;

            iAudioSamplingRate = pcm16Info->samplingRate;
            if (iAudioSamplingRate)
                iAudioSamplingRateValid = true;
            else
                iAudioSamplingRateValid = false;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMAudioMIO::setParametersSync() Audio Sampling Rate Key, Value %d", iAudioSamplingRate));

            iAudioNumChannels = pcm16Info->desiredChannels;
            if (iAudioNumChannels)
                iAudioNumChannelsValid = true;
            else
                iAudioNumChannelsValid = false;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMAudioMIO::setParametersSync() Audio Num Channels Key, Value %d", iAudioNumChannels));

            iNumberOfBuffers = (int32)pcm16Info->num_buffers;
            iNumberOfBuffersValid = true;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMAudioMIO::setParametersSync() Number of Buffer, Value %d", iNumberOfBuffers));

            iBufferSize = (int32)pcm16Info->buffer_size;
            iBufferSizeValid = true;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVFMAudioMIO::setParametersSync() Buffer Size, Value %d", iBufferSize));

        }
        else
        {
            if (iAudioSamplingRateValid && iAudioNumChannelsValid && !iIsMIOConfigured)
            {
                if (iObserver)
                {
                    iIsMIOConfigured = true;
                    iObserver->ReportInfoEvent(PVMFMIOConfigurationComplete);
                    if (iPeer && iWriteBusy)
                    {
                        iWriteBusy = false;
                        iPeer->statusUpdate(PVMI_MEDIAXFER_STATUS_WRITE);
                    }
                }
            }

            // If we get here the key is unrecognized.
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVFMAudioMIO::setParametersSync() Error, unrecognized key "));

            // Set the return value to indicate the unrecognized key and return.
            aRet_kvp = &aParameters[i];
            return;
        }
    }
    if (iAudioSamplingRateValid && iAudioNumChannelsValid && !iIsMIOConfigured)
    {
        if (iObserver)
        {
            iIsMIOConfigured = true;
            iObserver->ReportInfoEvent(PVMFMIOConfigurationComplete);
            if (iPeer && iWriteBusy)
            {
                iWriteBusy = false;
                iPeer->statusUpdate(PVMI_MEDIAXFER_STATUS_WRITE);
            }
        }
    }
}


