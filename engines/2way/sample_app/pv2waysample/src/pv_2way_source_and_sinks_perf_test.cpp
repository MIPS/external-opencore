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
#include "pv_2way_source_and_sinks_perf_test.h"
#include "pv_2way_sink.h"
#include "pv_2way_source.h"


#include "pv_media_output_node_factory.h"
#include "pvmf_media_input_node_factory.h"

#include "pv_2way_interface.h"
#include "pv_2way_media.h"
#include "pvmf_dummy_fileinput_node_factory.h"
#include "pvmf_dummy_fileoutput_factory.h"

#include "pv2way_file_names.h"



OSCL_EXPORT_REF PV2WaySourceAndSinksPerfTest::PV2WaySourceAndSinksPerfTest(PV2Way324InitInfo& arSdkInitInfo) :
        PV2WaySourceAndSinksBase(arSdkInitInfo)
{
}


OSCL_EXPORT_REF PV2WaySourceAndSinksPerfTest::~PV2WaySourceAndSinksPerfTest()
{
    Cleanup();
}


OSCL_EXPORT_REF PVMFNodeInterface* PV2WaySourceAndSinksPerfTest::CreateMIONode(CodecSpecifier* aformat,
        TPVDirection adir)
{
    PVMFNodeInterface* mioNode = NULL;
    return mioNode;

}

OSCL_EXPORT_REF void PV2WaySourceAndSinksPerfTest::DeleteMIONode(CodecSpecifier* aformat,
        TPVDirection adir,
        PVMFNodeInterface** aMioNode)
{
    if (!aformat)
    {
        if (aMioNode)
        {
            OSCL_DELETE(*aMioNode);
            *aMioNode = NULL;
        }
        return;
    }

    *aMioNode = NULL;
}

OSCL_EXPORT_REF int PV2WaySourceAndSinksPerfTest::AddPreferredCodec(TPVDirection aDir,
        PV2WayMediaType aMediaType,
        PVMFFileInputSettings& arFileSettings)
{
    PV2WayMIO* mio = GetMIO(aDir, aMediaType);
    if (mio)

    {
        mio->AddCodec(arFileSettings);
        return 0;
    }
    OutputInfo(PVLOGMSG_ERR, "PV2WaySourceAndSinksPerfTest::AddPreferredCodec: Error!  No MIO of given dir, type");
    return -1;
}


