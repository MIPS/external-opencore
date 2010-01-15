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
#define IMPLEMENT_AVCConfigurationBox
#include "avcconfigurationbox.h"
#include "atomdefs.h"
#include "oscl_bin_stream.h"
#include "atomutils.h"

#define LENGTH_SIZE_MINUS_ONE_MASK 0x03
#define NUM_SEQUENCE_PARAM_SETS_MASK 0x01F

typedef Oscl_Vector<AVCConfigurationBox::ParameterSet*, OsclMemAllocator> parameterSetVecType;


AVCConfigurationBox::AVCConfigurationBox(MP4_FF_FILE *fp, uint32 size, uint32 type)
        : Atom(fp, size, type)
{
    _mp4ErrorCode = READ_AVC_CONFIG_BOX_FAILED;

    _sequenceParameterSetVec = NULL;
    _pictureParameterSetVec  = NULL;
    _totalSeqParameterSetLength = 0;
    _totalPicutureParameterSetLength = 0;

    if (_success)
    {
        _success = false;
        _pparent = NULL;

        PV_MP4_FF_NEW(fp->auditCB, parameterSetVecType, (), _sequenceParameterSetVec);
        PV_MP4_FF_NEW(fp->auditCB, parameterSetVecType, (), _pictureParameterSetVec);


        uint32 configSize = size - DEFAULT_ATOM_SIZE;

        uint8* configBuffer;

        configBuffer = (uint8*) oscl_malloc(configSize * sizeof(uint8));

        oscl_memset(configBuffer, 0, configSize);

        if (!AtomUtils::readByteData(fp, configSize, configBuffer))
        {
            return;
        }
        PopulateAVCConfigurationFields(configBuffer, configSize);

        if (configBuffer != NULL)
        {
            oscl_free(configBuffer);
            configBuffer = NULL;
        }
    }
}

OSCL_EXPORT_REF AVCConfigurationBox::AVCConfigurationBox(uint32 type)
        : Atom(type)
{
    _mp4ErrorCode = READ_AVC_CONFIG_BOX_FAILED;

    _sequenceParameterSetVec = NULL;
    _pictureParameterSetVec  = NULL;
    _totalSeqParameterSetLength = 0;
    _totalPicutureParameterSetLength = 0;

    _pparent = NULL;

    PV_MP4_FF_NEW(NULL, parameterSetVecType, (), _sequenceParameterSetVec);
    PV_MP4_FF_NEW(NULL, parameterSetVecType, (), _pictureParameterSetVec);

}


OSCL_EXPORT_REF void AVCConfigurationBox::PopulateAVCConfigurationFields(uint8* AVCConfigBuffer, uint32 size)
{
    OsclBinIStreamBigEndian typeSpecificStream;

    typeSpecificStream.Attach((void *)(AVCConfigBuffer), size);

    const uint8* pAVCConfigBufferInput = AVCConfigBuffer;

    if ((size > 0) && (AVCConfigBuffer != NULL))
    {
        typeSpecificStream >> _configurationVersion;
        AVCConfigBuffer = AVCConfigBuffer + 1;

        typeSpecificStream >> _avcProfileIndication;
        AVCConfigBuffer = AVCConfigBuffer + 1;

        typeSpecificStream >> _profileCompatibility;
        AVCConfigBuffer = AVCConfigBuffer + 1;

        typeSpecificStream >> _avcLevelIndication;
        AVCConfigBuffer = AVCConfigBuffer + 1;

        _constraint_set0_flag = (uint8)((_profileCompatibility & ~0x7F) >> 7);
        _constraint_set1_flag = (uint8)((_profileCompatibility & ~0xBF) >> 6);
        _constraint_set2_flag = (uint8)((_profileCompatibility & ~0xDF) >> 5);
        _reserved_zero_5bits = 0;

        typeSpecificStream >> _lengthSizeMinusOne;
        AVCConfigBuffer = AVCConfigBuffer + 1;

        _lengthSizeMinusOne &= LENGTH_SIZE_MINUS_ONE_MASK;

        typeSpecificStream >> _numSequenceParameterSets;
        AVCConfigBuffer = AVCConfigBuffer + 1;

        _numSequenceParameterSets &= NUM_SEQUENCE_PARAM_SETS_MASK;

        uint8 i;
        uint16 parameterSetLen;

        for (i = 0; i < _numSequenceParameterSets; i++)
        {
            typeSpecificStream >> parameterSetLen;
            AVCConfigBuffer = AVCConfigBuffer + 2;

            _totalSeqParameterSetLength += parameterSetLen;

            ParameterSet *paramSet = NULL;
            PV_MP4_FF_NEW(NULL, ParameterSet, (parameterSetLen, AVCConfigBuffer), paramSet);

            AVCConfigBuffer = AVCConfigBuffer + parameterSetLen;

            if (!(paramSet->getSuccess()))
            {
                PV_MP4_FF_DELETE(NULL, ParameterSet, paramSet);
                return;
            }

            (*_sequenceParameterSetVec).push_back(paramSet);

        }

        const uint32 ppsBlockOffset = (AVCConfigBuffer - pAVCConfigBufferInput);
        int32 typeSpecificStreamSz = size - ppsBlockOffset;
        OSCL_ASSERT(typeSpecificStreamSz > 0);
        typeSpecificStream.Attach((void *)(AVCConfigBuffer), (size - ppsBlockOffset));
        typeSpecificStream >> _numPictureParameterSets;
        AVCConfigBuffer = AVCConfigBuffer + 1;
        typeSpecificStreamSz -= 1;
        for (i = 0; ((i < _numPictureParameterSets) && (typeSpecificStreamSz > 0)); i++)
        {
            typeSpecificStream >> parameterSetLen;

            AVCConfigBuffer = AVCConfigBuffer + 2;
            _totalPicutureParameterSetLength += parameterSetLen;

            ParameterSet *paramSet = NULL;
            PV_MP4_FF_NEW(NULL, ParameterSet, (parameterSetLen, AVCConfigBuffer), paramSet);
            AVCConfigBuffer = AVCConfigBuffer + parameterSetLen;
            typeSpecificStreamSz -= (2 + parameterSetLen);

            if (typeSpecificStreamSz > 0)
                typeSpecificStream.Attach((void *)(AVCConfigBuffer), typeSpecificStreamSz);


            if (!(paramSet->getSuccess()))
            {
                PV_MP4_FF_DELETE(NULL, ParameterSet, paramSet);
                return;
            }

            (*_pictureParameterSetVec).push_back(paramSet);
        }
        _success = true;
        _mp4ErrorCode = EVERYTHING_FINE;
    }


}

OSCL_EXPORT_REF bool AVCConfigurationBox::getPictureParamSet(int32 index, uint16 &length, uint8* &paramSet)
{
    if ((uint32)index < _pictureParameterSetVec->size())
    {
        ParameterSet *pSet = (*_pictureParameterSetVec)[index];

        length = pSet->getParameterSetLength();
        paramSet = pSet->getParameterSet();

        return true;
    }
    else
    {
        length = 0;
        paramSet = NULL;
        return false;
    }
}

OSCL_EXPORT_REF bool AVCConfigurationBox::getSequenceParamSet(int32 index, uint16 &length, uint8* &paramSet)
{
    if ((uint32)index < _sequenceParameterSetVec->size())
    {
        ParameterSet *pSet = (*_sequenceParameterSetVec)[index];

        length = pSet->getParameterSetLength();
        paramSet = pSet->getParameterSet();

        return true;
    }
    else
    {
        length = 0;
        paramSet = NULL;
        return false;
    }
}

AVCConfigurationBox::ParameterSet::ParameterSet(uint16 length, uint8* AVCConfigBuffer)
{
    _parameterSetLength = 0;
    _pParameterSet = NULL;
    _success = false;

    if ((int16)(length) > 0)
    {
        _parameterSetLength = length;

        OsclBinIStreamBigEndian typeSpecificStream;

        typeSpecificStream.Attach((void *)(AVCConfigBuffer), length);

        PV_MP4_FF_ARRAY_NEW(NULL, uint8, _parameterSetLength, _pParameterSet);

        if (!AtomUtils::readByteData(*(&AVCConfigBuffer), _parameterSetLength, _pParameterSet))
        {
            return ;
        }
        _success = true;
    }
}

// Destructor
OSCL_EXPORT_REF AVCConfigurationBox::~AVCConfigurationBox()
{
    if (_sequenceParameterSetVec != NULL)
    {
        for (uint32 i = 0; i < _sequenceParameterSetVec->size(); i++)
        {
            PV_MP4_FF_DELETE(NULL, ParameterSet, (*_sequenceParameterSetVec)[i]);
        }
        PV_MP4_FF_TEMPLATED_DELETE(NULL, parameterSetVecType, Oscl_Vector, _sequenceParameterSetVec);
    }
    if (_pictureParameterSetVec != NULL)
    {
        for (uint32 i = 0; i < _pictureParameterSetVec->size(); i++)
        {
            PV_MP4_FF_DELETE(NULL, ParameterSet, (*_pictureParameterSetVec)[i]);
        }
        PV_MP4_FF_TEMPLATED_DELETE(NULL, parameterSetVecType, Oscl_Vector, _pictureParameterSetVec);
    }
}
