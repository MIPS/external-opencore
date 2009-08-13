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
#ifndef OSCL_EXCEPTION_H_INCLUDED
#include "oscl_exception.h"
#endif
#ifndef PVMF_SM_RTSPT_UNICAST_NODE_FACTORY_H_INCLUDED
#include "pvmf_sm_rtspt_unicast_node_factory.h"
#endif

#ifndef PVMF_SM_FSP_RTSPT_UNICAST_H
#include "pvmf_sm_fsp_rtspt_unicast.h"
#endif

OSCL_EXPORT_REF PVMFSMFSPBaseNode*
PVMFSMRTSPTUnicastNodeFactory::CreateSMRTSPTUnicastNode(int32 aPriority)
{
    PVMFSMFSPBaseNode* smRtsptUnicastNode = NULL;
    int32 err;
    OSCL_TRY(err,
             smRtsptUnicastNode = PVMFSMRTSPTUnicastNode::New(aPriority);
            );

    if (err != OsclErrNone)
    {
        OSCL_LEAVE(err);
    }

    return (smRtsptUnicastNode);
}

OSCL_EXPORT_REF bool PVMFSMRTSPTUnicastNodeFactory::DeleteSMRTSPTUnicastNode(PVMFSMFSPBaseNode* aSMFSPNode)
{

    bool retval = false;

    if (aSMFSPNode)
    {
        OSCL_DELETE(aSMFSPNode);
        aSMFSPNode = NULL;
        retval = true;
    }

    return retval;
}
