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
#ifndef PV_PLAYER_DATASINKFILENAME_H_INCLUDED
#define PV_PLAYER_DATASINKFILENAME_H_INCLUDED

#ifndef PV_PLAYER_DATASINK_H_INCLUDED
#include "pv_player_datasink.h"
#endif

class PVPlayerDataSinkFilename : public PVPlayerDataSink
{
    public:
        PVPlayerDataSinkFilename() : iFilename(NULL) {};
        ~PVPlayerDataSinkFilename() {};

        PVPDataSinkType GetDataSinkType()
        {
            return PVP_DATASINKTYPE_FILENAME;
        }

        OSCL_wString& GetDataSinkFilename()
        {
            return iFilename;
        }

        PVMFNodeInterface* GetDataSinkNodeInterface()
        {
            return NULL;
        }

        void SetDataSinkFilename(const OSCL_wString& aFilename)
        {
            iFilename = aFilename;
        }

    private:
        OSCL_wHeapString<OsclMemAllocator> iFilename;
};

#endif // PV_PLAYER_DATASINKFILENAME_H_INCLUDED

