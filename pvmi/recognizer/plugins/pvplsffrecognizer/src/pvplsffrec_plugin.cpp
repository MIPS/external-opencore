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
#include "pvplsffrec_plugin.h"
#include "pvplsfileparser.h"


PVMFStatus PVPLSFFRecognizerPlugin::SupportedFormats(PVMFRecognizerMIMEStringList& aSupportedFormatsList)
{
    // Return PLS as supported type
    OSCL_HeapString<OsclMemAllocator> supportedformat = PVMF_MIME_PLSFF;
    aSupportedFormatsList.push_back(supportedformat);
    return PVMFSuccess;
}


PVMFStatus
PVPLSFFRecognizerPlugin::Recognize(PVMFDataStreamFactory& aSourceDataStreamFactory,
                                   PVMFRecognizerMIMEStringList* aFormatHint,
                                   PVMFRecognizerResult& aRecognizerResult)
{
    OSCL_UNUSED_ARG(aFormatHint);

    //set it up for a definite no - in case of errors we can still say format unknown
    aRecognizerResult.iRecognizedFormat = PVMF_MIME_FORMAT_UNKNOWN;
    aRecognizerResult.iRecognitionConfidence = PVMFRecognizerConfidenceCertain;

    PVPLSFFParser plsParser;
    if (plsParser.IsPLSFile(aSourceDataStreamFactory) == PVPLSFF_PARSER_OK)
    {
        // It is an PLS file so add positive result
        aRecognizerResult.iRecognizedFormat = PVMF_MIME_PLSFF;
        aRecognizerResult.iRecognitionConfidence = PVMFRecognizerConfidenceCertain;
    }

    return PVMFSuccess;
}

PVMFStatus PVPLSFFRecognizerPlugin::GetRequiredMinBytesForRecognition(uint32& aBytes)
{
    aBytes = PLSFF_MIN_DATA_SIZE_FOR_RECOGNITION;
    return PVMFSuccess;
}






