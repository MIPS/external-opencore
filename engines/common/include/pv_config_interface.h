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
#ifndef PV_CONFIG_INTERFACE_H_INCLUDED
#define PV_CONFIG_INTERFACE_H_INCLUDED

#include "oscl_base.h"
#include "oscl_vector.h"

/**
 * Base interface for all configuration classes
 */
class PVConfigInterface
{
        /**
         * This command provides a pointer to the configuration interface of the specified UUID.
         *
         * @param aUuid The UUID of the desired interface
         * @param aInterfacePtr Output pointer to the desired interface
         * @param aContextData Optional opaque data to be passed back to user with the command response
         * @returns A unique command id for asynchronous completion
         */
        virtual PVCommandId QueryInterface(const PVUUID& aUuid,
                                           PVInterfacePtr& aInterfacePtr,
                                           OsclAny* aContextData = NULL) = 0;
}

#endif // PV_CONFIG_INTERFACE_H_INCLUDED




