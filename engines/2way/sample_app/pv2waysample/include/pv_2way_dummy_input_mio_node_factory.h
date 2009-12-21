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
#ifndef PV_2WAY_DUMMY_INPUT_MIO_NODE_FACTORY_H_INCLUDED
#define PV_2WAY_DUMMY_INPUT_MIO_NODE_FACTORY_H_INCLUDED

#ifndef PVMF_NODE_INTERFACE_H_INCLUDED
#include "pvmf_node_interface.h"
#endif

#ifndef PV_2WAY_MIO_NODE_FACTORY_H_INCLUDED
#include "pv_2way_mio_node_factory.h"
#endif

#ifndef DUMMY_SETTINGS_H_INCLUDED
#include "dummy_settings.h"
#endif

class PvmiMIOControl;

class PV2WayDummyInputMIONodeFactory: public HeapBase, public PV2WayMIONodeFactory
{
    public:
        void Release()
        {
            delete this;
        }
        PV2WayDummyInputMIONodeFactory() {};
        virtual ~PV2WayDummyInputMIONodeFactory() {};
        OSCL_IMPORT_REF virtual PVMFNodeInterface* Create(DummyMIOSettings& aSettings);
        OSCL_IMPORT_REF virtual void Delete(PVMFNodeInterface** mioNode);
    protected:
        PvmiMIOControl* iMediaControl;
        virtual int CreateMedia(DummyMIOSettings& aSettings);
        void DeleteMedia();
};



#endif
