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
#include "pvaacffrec_plugin.h"
#include "aacfileparser.h"
#include "oscl_file_io.h"

#define PVMF_AAC_REC_PLUGIN_MIN_SIZE_FOR_RECOGNITION 512

PVMFStatus PVAACFFRecognizerPlugin::SupportedFormats(PVMFRecognizerMIMEStringList& aSupportedFormatsList)
{
    // Return AAC as supported type
    OSCL_HeapString<OsclMemAllocator> supportedformat = PVMF_MIME_AACFF;
    aSupportedFormatsList.push_back(supportedformat);
    return PVMFSuccess;
}


PVMFStatus
PVAACFFRecognizerPlugin::Recognize(PVMFDataStreamFactory& aSourceDataStreamFactory,
                                   PVMFRecognizerMIMEStringList* aFormatHint,
                                   PVMFRecognizerResult& aRecognizerResult)
{
    OSCL_UNUSED_ARG(aFormatHint);

    //set it up for a definite no - in case of errors we can still say format unknown
    aRecognizerResult.iRecognizedFormat = PVMF_MIME_FORMAT_UNKNOWN;
    aRecognizerResult.iRecognitionConfidence = PVMFRecognizerConfidenceCertain;

    OSCL_wStackString<1> tmpfilename;
    int32 leavecode = 0;
    CAACFileParser* aacfile = NULL;
    OSCL_TRY(leavecode, aacfile = OSCL_NEW(CAACFileParser, ()));
    if (leavecode || aacfile == NULL)
    {
        return PVMFErrNoMemory;
    }

    CAACFileParams params;
    params.iClip = tmpfilename;
    params.iCPMAccess = &aSourceDataStreamFactory;
    ParserErrorCode retVal = aacfile->IsAACFile(params);

    OSCL_DELETE(aacfile);
    aacfile = NULL;

    if (AAC_SUCCESS == retVal)
    {
        // It is an aac file so add positive result
        aRecognizerResult.iRecognizedFormat = PVMF_MIME_AACFF;
        aRecognizerResult.iRecognitionConfidence = PVMFRecognizerConfidenceCertain;
    }
    else if (INSUFFICIENT_DATA == retVal)
    {
        // It may be an aac file, but there is not enough data to make the determination
        aRecognizerResult.iRecognizedFormat = PVMF_MIME_AACFF;
        aRecognizerResult.iRecognitionConfidence = PVMFRecognizerConfidencePossible;
        //FIXME: We need to return a more meaningful value here. FF should provide it.
        aRecognizerResult.iAdditionalBytesRequired = 9 * 1024; //picking 9K since ADTS frame size is 8192
    }
    return PVMFSuccess;
}

PVMFStatus PVAACFFRecognizerPlugin::GetRequiredMinBytesForRecognition(uint32& aBytes)
{
    aBytes = PVMF_AAC_REC_PLUGIN_MIN_SIZE_FOR_RECOGNITION;
    return PVMFSuccess;
}






