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
#include "pvmf_parsernode_factory.h"
#endif
#ifndef PVMF_PARSERNODE_IMPL_H_INCLUDED
#include "pvmf_parsernode_impl.h"
#endif

PVMFNodeInterface* PVMFParserNodeFactory::CreatePVMFParserNode(int32 aPriority)
{
    PVMFParserNodeImpl* node = NULL;
    node = OSCL_NEW(PVMFParserNodeImpl, (aPriority));
    if (NULL == node)
        OSCL_LEAVE(OsclErrNoMemory);

    return node;
}

bool PVMFParserNodeFactory::DeletePVMFParserNode(PVMFNodeInterface* aNode)
{
    if (NULL != aNode)
    {
        OSCL_DELETE(aNode);
        return true;
    }
    return false;
}


