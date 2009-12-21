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
#ifndef DUMMY_SETTINGS_H_INCLUDED
#define DUMMY_SETTINGS_H_INCLUDED

#ifndef PVMF_FORMAT_TYPE_H_INCLUDED
#include "pvmf_format_type.h"
#endif

#ifndef DUMMY_MIO_OBSERVER_H_INCLUDED
#include "dummy_mio_observer.h"
#endif
/**
 * Contains information for setting values for dummy input & output MIOs
 */
class DummyMIOSettings
{
    public:
        DummyMIOSettings()
        {
            iMediaFormat = PVMF_MIME_FORMAT_UNKNOWN;
            iDummyMIOObserver = NULL;
            iAudioFrameRate = 0;
            iVideoFrameRate = 0;
            iNumofAudioFrame = 0;
        }

        DummyMIOSettings(const DummyMIOSettings& aSettings)
        {
            iMediaFormat = aSettings.iMediaFormat;
            iDummyMIOObserver = aSettings.iDummyMIOObserver;
            iAudioFrameRate = aSettings.iAudioFrameRate;
            iVideoFrameRate = aSettings.iVideoFrameRate;
            iNumofAudioFrame = aSettings.iNumofAudioFrame;
        }

        ~DummyMIOSettings()
        {
            iMediaFormat = PVMF_MIME_FORMAT_UNKNOWN;
            iDummyMIOObserver = NULL;
            iAudioFrameRate = 0;
            iVideoFrameRate = 0;
            iNumofAudioFrame = 0;
        }

        PVMFFormatType iMediaFormat;
        uint32 iAudioFrameRate;
        uint32 iVideoFrameRate;
        uint32 iNumofAudioFrame;
        DummyMIOObserver* iDummyMIOObserver;
};
#endif


