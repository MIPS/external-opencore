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

#include "pv_audio_config_parser.h"
#include "oscl_mem.h"
#include "getactualaacconfig.h"

#include "oscl_dll.h"

//Macros for WMA
#define GetUnalignedWord( pb, w ) \
            (w) = ((uint16) *(pb + 1) << 8) + *pb;

#define GetUnalignedDword( pb, dw ) \
            (dw) = ((uint32) *(pb + 3) << 24) + \
                   ((uint32) *(pb + 2) << 16) + \
                   ((uint16) *(pb + 1) << 8) + *pb;

#define GetUnalignedWordEx( pb, w )     GetUnalignedWord( pb, w ); (pb) += sizeof(uint16);
#define GetUnalignedDwordEx( pb, dw )   GetUnalignedDword( pb, dw ); (pb) += sizeof(uint32);

#define LoadWORD( w, p )    GetUnalignedWordEx( p, w )
#define LoadDWORD( dw, p )  GetUnalignedDwordEx( p, dw )


#define WAVE_FORMAT_MSAUDIO1  0x0160
#define WAVE_FORMAT_WMAUDIO2  0x0161
#define WAVE_FORMAT_WMAUDIO3  0x0162
#define WAVE_FORMAT_WMAUDIO_LOSSLESS  0x0163
#define WAVE_FORMAT_WMAUDIO2_ES  0x0165
#define WAVE_FORMAT_WMASPDIF 0x164
#define WAVE_FORMAT_WMAUDIO3_ES  0x0166
#define WAVE_FORMAT_WMAUDIO_LOSSLESS_ES  0x0167
#define WAVE_FORMAT_PLAYREADY 0x5052

#define WAVE_FORMAT_WMAVOICE9  10

#define AAC_DEFAULT_SAMPLES_PER_FRAME 1024

#define RA_DEFAULT_SAMPLE_RATE 22050
#define RA_DEFAULT_NUM_CHANNELS 1
#define RA_DEFAULT_SAMPLES_PER_FRAME 512

OSCL_DLL_ENTRY_POINT_DEFAULT()


// This routine parses the wma config header and returns Sampling Rate, Number of Channels, and Bits Per Sample
// The header info is checked for validation and support
OSCL_EXPORT_REF int32 pv_audio_config_parser(pvAudioConfigParserInputs *aInputs, pvAudioConfigParserOutputs *aOutputs)
{
    bool bBitStreamValid = true;
    bool bBitStreamSupported = true;

#if defined(BUILD_OLDWMAAUDIOLIB)
//AdvancedEncodeOpt
#define ENCOPT4_PLUSVER   0xe000
#define ENCOPT4_PLUSV1    0xc000
#define ENCOPT4_PLUSV1ALT 0x8000
#define ENCOPT4_PLUSV2    0x2000
#define ENCOPT4_PLUSV3    0x4000

    if (aInputs->iMimeType == PVMF_MIME_WMA)
    {
        uint16 wdata;
        uint32 dwdata;
        uint32 AvgBytesPerSec;
        uint16 AdvancedEncodeOpt;

        /**** decoder header *******/
        uint8* tp = aInputs->inPtr;
        LoadWORD(wdata , tp);

        if (wdata == WAVE_FORMAT_PLAYREADY)
        {
            tp += aInputs->inBytes - 4; // skip ahead to check the last 2 bytes.
            LoadWORD(wdata, tp);
            tp = aInputs->inPtr + 2; //restart from this location.
        }


        switch (wdata)
        {
                // WMA Lossless
            case WAVE_FORMAT_WMAUDIO_LOSSLESS:
            {
                if (aInputs->inBytes < 36)
                {
                    bBitStreamValid = false;
                    break;
                }

                LoadWORD(wdata, tp);
                aOutputs->Channels        = wdata;
                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec   = dwdata;
                LoadDWORD(dwdata, tp);
                AvgBytesPerSec  = dwdata;

                tp = aInputs->inPtr + 14;
                LoadWORD(wdata , tp);
                // Round up to the byte to get the container size
                aOutputs->BitsPerSample = 8 * ((wdata + 7) / 8);

                // Has V3 specific info
                tp = aInputs->inPtr + 34;
                LoadWORD(wdata , tp);
                AdvancedEncodeOpt = wdata;

                bBitStreamSupported = false;

                // more limits according to the current PV WMA implementation
                // do not supoprt multi-channel
                if (aOutputs->Channels > 2)
                {
                    bBitStreamSupported = false;
                    break;
                }

                // do not support 24-bit
                if (aOutputs->BitsPerSample > 16)
                {
                    bBitStreamSupported = false;
                    break;
                }
            }
            break;

            // WMA Pro, Pro+
            case WAVE_FORMAT_WMAUDIO3:
            {
                if (aInputs->inBytes < 36)
                {
                    bBitStreamValid = false;
                    break;
                }

                LoadWORD(wdata, tp);
                aOutputs->Channels        = wdata;
                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec   = dwdata;
                LoadDWORD(dwdata, tp);
                AvgBytesPerSec  = dwdata;

                tp = aInputs->inPtr + 14;
                LoadWORD(wdata , tp);   //iValidBitsPerSample
                // Round up to the byte to get the container size
                aOutputs->BitsPerSample = 8 * ((wdata + 7) / 8);

                // Has V3 specific info
                tp = aInputs->inPtr + 34;
                LoadWORD(wdata , tp);
                AdvancedEncodeOpt = wdata;

                // more limits according to the current PV WMA implementation
                // do not supoprt multi-channel
                if (aOutputs->Channels > 2)
                {
                    bBitStreamSupported = false;
                    break;
                }

                // do not support 24-bit
                if (aOutputs->BitsPerSample > 16)
                {
                    bBitStreamSupported = false;
                    break;
                }

                // limit to M0-profile bitrate and sampling rate
                if (AvgBytesPerSec > 192000 || aOutputs->SamplesPerSec > 48000)
                {
                    bBitStreamSupported = false;
                    break;
                }

                // only decode PLUSV1 (not including PLUSV1ALT)
                if (ENCOPT4_PLUSV1 != (AdvancedEncodeOpt & ENCOPT4_PLUSVER))
                {
                    bBitStreamSupported = false;
                    break;
                }
            }
            break;

            //WMA Standard
            case WAVE_FORMAT_WMAUDIO2:
            {
                if (aInputs->inBytes < 28)
                {
                    bBitStreamValid = false;
                    break;
                }

                LoadWORD(wdata, tp);
                aOutputs->Channels        = wdata;
                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec   = dwdata;

                tp = aInputs->inPtr + 14;
                LoadWORD(wdata , tp);
                aOutputs->BitsPerSample = wdata;

                if (aOutputs->SamplesPerSec > 48000)
                {
                    // not a valid sample rate for WMA Std spec
                    bBitStreamValid = false;
                    break;
                }
                if (aOutputs->Channels > 2)
                {
                    // not a valid number of channels for the WMA Std spec
                    bBitStreamValid = false;
                    break;
                }

                if (aOutputs->BitsPerSample != 16)
                {
                    // not a valid number of bits per sample for the WMA Std spec
                    bBitStreamValid = false;
                    break;
                }
            }
            break;

            //WMA Standard (bitstream v1)
            case WAVE_FORMAT_MSAUDIO1:
            {
                if (aInputs->inBytes < 22)
                {
                    bBitStreamValid = false;
                    break;
                }

                tp = aInputs->inPtr +  4;
                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec = dwdata;
                tp = aInputs->inPtr +  2;
                LoadWORD(wdata , tp);
                aOutputs->Channels = wdata;

                tp = aInputs->inPtr + 14;
                LoadWORD(wdata , tp);
                aOutputs->BitsPerSample = wdata;

                if (aOutputs->SamplesPerSec > 48000)
                {
                    // not a valid sample rate for WMA Std spec
                    bBitStreamValid = false;
                    break;
                }
                if (aOutputs->Channels > 2)
                {
                    // not a valid number of channels for the WMA Std spec
                    bBitStreamValid = false;
                    break;
                }

                if (aOutputs->BitsPerSample != 16)
                {
                    // not a valid number of bits per sample for the WMA Std spec
                    bBitStreamValid = false;
                    break;
                }
            }
            break;

            // WMA Voice
            case WAVE_FORMAT_WMAVOICE9:
            {
                if (aInputs->inBytes < 64) // sizeof(WMAVOICEWAVEFORMAT)
                {
                    bBitStreamValid = false;
                    break;
                }

                LoadWORD(wdata, tp);
                aOutputs->Channels = wdata;

                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec = dwdata;

                LoadDWORD(dwdata, tp);
                // wmavFormat.wfx.nAvgBytesPerSec

                LoadWORD(wdata, tp);
                // wmavFormat.wfx.nBlockAlign = wdata;

                LoadWORD(wdata, tp);
                aOutputs->BitsPerSample = wdata;

                // WMAVoice is always mono, 16-bit
                if (aOutputs->Channels != 1)
                {
                    bBitStreamValid = false;
                }

                if (aOutputs->BitsPerSample != 16)
                {
                    bBitStreamValid = false;
                }

                if ((aOutputs->SamplesPerSec != 8000)
                        && (aOutputs->SamplesPerSec != 11025)
                        && (aOutputs->SamplesPerSec != 16000)
                        && (aOutputs->SamplesPerSec != 22050))
                {
                    bBitStreamValid = false;
                }
            }
            break;


            case WAVE_FORMAT_WMASPDIF:
            case WAVE_FORMAT_WMAUDIO2_ES:
            case WAVE_FORMAT_WMAUDIO3_ES:
            case WAVE_FORMAT_WMAUDIO_LOSSLESS_ES:
            {
                // these formats aren't supported
                bBitStreamSupported = false;
            }
            break;

            default:
            {
                // invalid wma format
                bBitStreamValid = false;
            }
        }
    }
#else // BUILD_OLDWMAAUDIOLIB
    if (aInputs->iMimeType == PVMF_MIME_WMA)
    {
        uint16 wdata;
        uint32 dwdata;
        uint32 AvgBytesPerSec;
        uint16 AdvancedEncodeOpt;

        /**** decoder header *******/
        uint8* tp = aInputs->inPtr;
        LoadWORD(wdata , tp);

        if (wdata == WAVE_FORMAT_PLAYREADY)
        {
            tp += aInputs->inBytes - 4; // skip ahead to check the last 2 bytes.
            LoadWORD(wdata, tp);
            tp = aInputs->inPtr + 2; //restart from this location.
        }

        switch (wdata)
        {
                // WMA Lossless
            case WAVE_FORMAT_WMAUDIO_LOSSLESS:
            {
                if (aInputs->inBytes < 36)
                {
                    bBitStreamValid = false;
                    break;
                }

                LoadWORD(wdata, tp);
                aOutputs->Channels        = wdata;
                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec   = dwdata;
                LoadDWORD(dwdata, tp);
                AvgBytesPerSec  = dwdata;

                tp = aInputs->inPtr + 14;
                LoadWORD(wdata , tp);
                // Round up to the byte to get the container size
                aOutputs->BitsPerSample = 8 * ((wdata + 7) / 8);

                // Has V3 specific info
                tp = aInputs->inPtr + 34;
                LoadWORD(wdata , tp);
                AdvancedEncodeOpt = wdata;

#if !defined(WMA_AUDIO_SUPPORTED)
                bBitStreamSupported = false;
                break;
#endif  // WMA_AUDIO_SUPPORTED

                if (aOutputs->Channels > 8)
                {
                    bBitStreamSupported = false;
                    break;
                }

#if !defined(PV_WMA_ENABLE_MULTICH)
                // device does not support multichannel, therefore automatically downmix
                if (aOutputs->Channels > 2)
                {
                    aOutputs->Channels = 2;
                }
#endif // PV_WMA_ENABLE_MULTICH

#if !defined(PV_WMA_ENABLE_24BIT)
                // do not support 24-bit, therefore automaticall requantize
                if (aOutputs->BitsPerSample > 16)
                {
                    aOutputs->BitsPerSample = 16;
                }
#endif // PV_WMA_ENABLE_24BIT

#if !defined(PV_WMA_ENABLE_HIGHFS)
                // hardware does not support sampling rates greater than 48kHz,
                //lossless does not have half-transform downsampling
                if (aOutputs->SamplesPerSec > 48000)
                {
                    bBitStreamSupported = false;
                    break;
                }
#endif // PV_WMA_ENABLE_HIGHFS
            }
            break;

            // WMA Pro, LBRv1, LBRv2, LBRv3
            case WAVE_FORMAT_WMAUDIO3:
            {
                if (aInputs->inBytes < 36)
                {
                    bBitStreamValid = false;
                    break;
                }

                LoadWORD(wdata, tp);
                aOutputs->Channels        = wdata;
                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec   = dwdata;
                LoadDWORD(dwdata, tp);
                AvgBytesPerSec  = dwdata;

                tp = aInputs->inPtr + 14;
                LoadWORD(wdata , tp);   //iValidBitsPerSample
                // Round up to the byte to get the container size
                aOutputs->BitsPerSample = 8 * ((wdata + 7) / 8);

                // Has V3 specific info
                tp = aInputs->inPtr + 34;
                LoadWORD(wdata , tp);
                AdvancedEncodeOpt = wdata;

#if !defined(WMA_AUDIO_SUPPORTED)
                bBitStreamSupported = false;
                break;
#endif
                // this is the max of the format.  the codec will downmix to stereo
                if (aOutputs->Channels > 8)
                {
                    bBitStreamSupported = false;
                    break;
                }

#if !defined(PV_WMA_ENABLE_MULTICH)
                // device does not support multichannel, therefore automatically downmix
                if (aOutputs->Channels > 2)
                {
                    aOutputs->Channels = 2;
                }
#endif // PV_WMA_ENABLE_MULTICH

#if !defined(PV_WMA_ENABLE_24BIT)
                // do not support 24-bit, therefore automatically requantize
                if (aOutputs->BitsPerSample == 24)
                {
                    aOutputs->BitsPerSample = 16;
                }
#endif // PV_WMA_ENABLE_24BIT

#if !defined(PV_WMA_ENABLE_HIGHFS)
                // hardware does not support sampling rates greater than 48kHz,
                // however, WMA Pro has half-transform downsampling
                if (aOutputs->SamplesPerSec > 48000)
                {
                    aOutputs->SamplesPerSec = aOutputs->SamplesPerSec / 2;
                }
#endif // PV_WMA_ENABLE_HIGHFS
            }
            break;

            //WMA Standard
            case WAVE_FORMAT_WMAUDIO2:
            {
                if (aInputs->inBytes < 28)
                {
                    bBitStreamValid = false;
                    break;
                }

                LoadWORD(wdata, tp);
                aOutputs->Channels        = wdata;
                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec   = dwdata;

                tp = aInputs->inPtr + 14;
                LoadWORD(wdata , tp);
                aOutputs->BitsPerSample = wdata;

                if (aOutputs->SamplesPerSec > 48000)
                {
                    // not a valid sample rate for WMA Std spec
                    bBitStreamValid = false;
                    break;
                }
                if (aOutputs->Channels > 2)
                {
                    // not a valid number of channels for the WMA Std spec
                    bBitStreamValid = false;
                    break;
                }

                if (aOutputs->BitsPerSample != 16)
                {
                    // not a valid number of bits per sample for the WMA Std spec
                    bBitStreamValid = false;
                    break;
                }

#if !defined(WMA_AUDIO_SUPPORTED)
                bBitStreamSupported = false;
#endif // WMA_AUDIO_SUPPORTED

            }
            break;

            //WMA Standard (bitstream v1)
            case WAVE_FORMAT_MSAUDIO1:
            {
                if (aInputs->inBytes < 22)
                {
                    bBitStreamValid = false;
                    break;
                }

                tp = aInputs->inPtr +  4;
                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec = dwdata;
                tp = aInputs->inPtr +  2;
                LoadWORD(wdata , tp);
                aOutputs->Channels = wdata;

                tp = aInputs->inPtr + 14;
                LoadWORD(wdata , tp);
                aOutputs->BitsPerSample = wdata;

                if (aOutputs->SamplesPerSec > 48000)
                {
                    // not a valid sample rate for WMA Std spec
                    bBitStreamValid = false;
                    break;
                }
                if (aOutputs->Channels > 2)
                {
                    // not a valid number of channels for the WMA Std spec
                    bBitStreamValid = false;
                    break;
                }

                if (aOutputs->BitsPerSample != 16)
                {
                    // not a valid number of bits per sample for the WMA Std spec
                    bBitStreamValid = false;
                    break;
                }

#if !defined(WMA_AUDIO_SUPPORTED)
                bBitStreamSupported = false;
#endif // WMA_AUDIO_SUPPORTED
            }
            break;

            // WMA Voice
            case WAVE_FORMAT_WMAVOICE9:
            {
                if (aInputs->inBytes < 64) // sizeof(WMAVOICEWAVEFORMAT)
                {
                    bBitStreamValid = false;
                    break;
                }

                LoadWORD(wdata, tp);
                aOutputs->Channels = wdata;

                LoadDWORD(dwdata, tp);
                aOutputs->SamplesPerSec = dwdata;

                LoadDWORD(dwdata, tp);
                // wmavFormat.wfx.nAvgBytesPerSec

                LoadWORD(wdata, tp);
                // wmavFormat.wfx.nBlockAlign = wdata;

                LoadWORD(wdata, tp);
                aOutputs->BitsPerSample = wdata;

                // WMAVoice is always mono, 16-bit
                if (aOutputs->Channels != 1)
                {
                    bBitStreamValid = false;
                }

                if (aOutputs->BitsPerSample != 16)
                {
                    bBitStreamValid = false;
                }

                if ((aOutputs->SamplesPerSec != 8000)
                        && (aOutputs->SamplesPerSec != 11025)
                        && (aOutputs->SamplesPerSec != 16000)
                        && (aOutputs->SamplesPerSec != 22050))
                {
                    bBitStreamValid = false;
                }

#if !defined(WMA_VOICE_SUPPORTED)
                bBitStreamSupported = false;
#endif // WMA_VOICE_SUPPORTED
            }
            break;


            case WAVE_FORMAT_WMASPDIF:
            case WAVE_FORMAT_WMAUDIO2_ES:
            case WAVE_FORMAT_WMAUDIO3_ES:
            case WAVE_FORMAT_WMAUDIO_LOSSLESS_ES:
            {
                // these formats aren't supported
                bBitStreamSupported = false;
            }
            break;

            default:
            {
                // invalid wma format
                bBitStreamValid = false;
            }
        }
    }
#endif // BUILD_OLDWMAAUDIOLIB
    else if (aInputs->iMimeType == PVMF_MIME_MPEG4_AUDIO || // AAC
             aInputs->iMimeType == PVMF_MIME_3640 ||
             aInputs->iMimeType == PVMF_MIME_LATM ||
             aInputs->iMimeType == PVMF_MIME_ADIF ||
             aInputs->iMimeType == PVMF_MIME_ASF_MPEG4_AUDIO ||
             aInputs->iMimeType == PVMF_MIME_AAC_SIZEHDR)
    {
        int32   bytes_consumed, status;
        uint8   aAudioObjectType, SamplingRateIndex;
        uint32  NumChannels;
        uint32  SamplesPerFrame;

        const uint32 SampleFreqTable[13] =
        {
            96000, 88200, 64000, 48000,
            44100, 32000, 24000, 22050,
            16000, 12000, 11025, 8000,
            7350
        };

        bytes_consumed = (int32)aInputs->inBytes;

        status = GetActualAacConfig(aInputs->inPtr,
                                    &aAudioObjectType,
                                    &bytes_consumed,
                                    &SamplingRateIndex,
                                    &NumChannels,
                                    &SamplesPerFrame);
        if (status != 0)//error
        {
            bBitStreamValid  = false;
        }

        aOutputs->Channels = (uint16)NumChannels;
        if (aOutputs->Channels > 2)
        {
            // not a valid number of channels for the AAC
            bBitStreamValid = false;
        }

        if (SamplingRateIndex < 13)
        {
            aOutputs->SamplesPerSec = SampleFreqTable[(uint32)SamplingRateIndex];
        }
        else
        {
            // not a valid sampling rate for the AAC
            bBitStreamValid = false;
        }

        aOutputs->SamplesPerFrame = SamplesPerFrame;
    }
    else if (aInputs->iMimeType == PVMF_MIME_REAL_AUDIO)    //Real Audio
    {
        // use these as default value, let decoder discover the actual size and perform port reconfig
        aOutputs->Channels = RA_DEFAULT_NUM_CHANNELS;
        aOutputs->SamplesPerSec = RA_DEFAULT_SAMPLE_RATE;
        aOutputs->SamplesPerFrame = RA_DEFAULT_SAMPLES_PER_FRAME;
    }

    if (!bBitStreamValid)
    {
        bBitStreamSupported = false;
    }

    return (bBitStreamSupported ? 1 : 0);
}
