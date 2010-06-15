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
#ifndef UT_H_INCLUDED
#define UT_H_INCLUDED

//******************************************************************************
//                                                                    INCLUDES
//******************************************************************************

#ifndef OSCLCONFIG_H_INCLUDED
#include "osclconfig.h"
#endif


#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

//******************************************************************************
//                                                                    FORWARDS
//******************************************************************************

class test_result;


//******************************************************************************
// UT                                                                    CLASS
//******************************************************************************
//
class UT
{

    public:

        //**********************************************************************
        // CM                                                            CLASS
        //**********************************************************************
        //
        class CM
        {
            public:
                //==============================================================
                // InitializeReporting                           PUBLIC STATIC
                //==============================================================
                // - writes a default junit xml report to 'a_filename' in case
                //   the test never returns
                // - opens a new file stream and return handle to client in
                //   'a_pFileStreamChild'
                // - any errors in this routine will be written to
                //   'a_pFileStreamParent' and referenced by 'a_pszTestname'
                // - stdout is captured on platforms supporting ANSI File IO.
                //
                OSCL_IMPORT_REF static void
                InitializeReporting
                (
                    const char* a_pszTestname,
                    OSCL_HeapString<OsclMemAllocator>& a_filename,
                    FILE* a_pFileStreamParent,
                    FILE*& a_pFileStreamChild
                );

                //==============================================================
                // FinalizeReporting                             PUBLIC STATIC
                //==============================================================
                // - parses the test result 'a_tr' object and writes junit xml
                //   to 'a_filename'
                // - any errors in this routine will be written to
                //   'a_pFileStreamParent' and referenced by 'a_pszTestname'
                // - on platforms supporting ANSI File IO, stdout will be
                //   included in the junit reporting
                //
                OSCL_IMPORT_REF static void
                FinalizeReporting
                (
                    const char* a_pszTestname,
                    OSCL_HeapString<OsclMemAllocator> &a_filename,
                    const test_result& a_tr,
                    FILE* a_pFileStreamParent,
                    FILE* a_pFileStreamChild
                );
        };
};

#endif

// END FILE

