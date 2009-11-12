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
#ifndef PVMF_GAPLESS_METADATA_H_INCLUDED
#define PVMF_GAPLESS_METADATA_H_INCLUDED

// gapless info per track

class PVMFGaplessMetadata
{
    public:
        PVMFGaplessMetadata()
        {
            iEncoderDelay = 0;
            iZeroPadding = 0;
            iOriginalLength = 0;
            iSamplesPerFrame = 0;
            iTotalFrames = 0;
            iPartOfGaplessAlbum = false;
        };
        ~PVMFGaplessMetadata() {};

        void SetEncoderDelay(uint32 aDelay)
        {
            iEncoderDelay = aDelay;
        }
        uint32 GetEncoderDelay()
        {
            return iEncoderDelay;
        }

        void SetZeroPadding(uint32 aPadding)
        {
            iZeroPadding = aPadding;
        }
        uint32 GetZeroPadding()
        {
            return iZeroPadding;
        }

        void SetOriginalStreamLength(uint64 aLength)
        {
            iOriginalLength = aLength;
        }
        uint64 GetOriginalStreamLength()
        {
            return iOriginalLength;
        }

        void SetSamplesPerFrame(uint32 aSamples)
        {
            iSamplesPerFrame = aSamples;
        }
        uint32 GetSamplesPerFrame()
        {
            return iSamplesPerFrame;
        }

        void SetTotalFrames(uint64 aFrames)
        {
            iTotalFrames = aFrames;
        }
        uint64 GetTotalFrames()
        {
            return iTotalFrames;
        }

        void SetPartOfGaplessAlbum(bool aPGAP)
        {
            iPartOfGaplessAlbum = aPGAP;
        }
        bool GetPartOfGaplessAlbum()
        {
            return iPartOfGaplessAlbum;
        }

    private:
        // encoder delay at the start of the track, in number of samples
        uint32 iEncoderDelay;

        // zero padding at the end of the track, in number of samples
        uint32 iZeroPadding;

        // original stream length (excluding encoder delay and padding)
        uint64 iOriginalLength;

        // samples per frame
        uint32 iSamplesPerFrame;

        // total number of frames (including encoder delay and padding)
        uint64 iTotalFrames;

        // whether this track is part of a gapless album
        bool iPartOfGaplessAlbum;
};

#endif // PVMF_GAPLESS_METADATA_H_INCLUDED




