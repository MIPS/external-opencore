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
#ifndef PVMF_PARSERNODE_FACTORY_H_INCLUDED
#define PVMF_PARSERNODE_FACTORY_H_INCLUDED

#ifndef PV_UUID_H_INCLUDED
#include "pv_uuid.h"
#endif
#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif

#define KPVMFParserNodeUuid PVUuid(0xd03095b9,0x4010,0x4124,0x93,0x52,0xcb,0xaa,0x01,0x11,0x3d,0xad)

class PVMFNodeInterface;

class PVMFParserNodeFactory
{
    public:
        static PVMFNodeInterface* CreatePVMFParserNode(int32 aPriority = OsclActiveObject::EPriorityNominal);

        static bool DeletePVMFParserNode(PVMFNodeInterface* aNode);
};

#endif

