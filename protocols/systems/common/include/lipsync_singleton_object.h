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
#ifndef LIPSYNC_SINGLETON_OBJECT_H_INCLUDED
#define LIPSYNC_SINGLETON_OBJECT_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef PVMF_MEDIA_CLOCK_H_INCLUDED
#include "pvmf_media_clock.h"
#endif

class LipSyncNotifyTSObserver
{
    public:
        virtual ~LipSyncNotifyTSObserver() {};
        /*
         * Signals an update in the status of the MIO.

         */

        virtual void MIOFramesTimeStamps(bool aIsAudio, uint32 aOrigTS, uint32 aRenderTS) = 0;
};

class ShareParams
{

    public:

        OSCL_IMPORT_REF static ShareParams* Instance();
        OSCL_IMPORT_REF static void Destroy();
        uint32 iAudioTS;
        uint32 iVideoTS;
        bool   iCompressed;

        LipSyncNotifyTSObserver* iObserver;

    private:
        ShareParams()
        {

            iAudioTS = 0;
            iVideoTS = 0;
            iCompressed = false;

        };
        ~ShareParams() {};

    private:
        static ShareParams* _instance;
};

#endif


