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
//******************************************************************************
//                                                                    INCLUDES
//******************************************************************************

#include "ut.h"

#ifndef XML_TEST_INTERPRETER_H_INCLUDED
#include "xml_test_interpreter.h"
#endif

#ifndef TEXT_TEST_INTERPRETER_H_INCLUDED
#include "text_test_interpreter.h"
#endif

#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif



//==============================================================================
// InitializeReporting                                           PUBLIC STATIC
//==============================================================================
//
OSCL_EXPORT_REF void
UT::CM::InitializeReporting
(
    const char* a_pszTestname,
    OSCL_HeapString<OsclMemAllocator>& a_filename,
    FILE* a_pFileStreamParent,
    FILE*& a_pFileStreamChild
)
{
    a_pFileStreamChild = a_pFileStreamParent;

    if (0 == a_pszTestname || 0 >= a_filename.get_size() || 0 == a_pFileStreamParent)
        return;

    Oscl_FileServer fs;
    fs.Connect();

    Oscl_File f;
    if (0 == f.Open(a_filename.get_cstr(), Oscl_File::MODE_READWRITE | Oscl_File::MODE_TEXT, fs))
    {
        _STRING xfr = xml_test_interpreter::unexpected_termination_interpretation(a_pszTestname);
        f.Write(xfr.c_str(), sizeof(char), oscl_strlen(xfr.c_str()));
        f.Close();
    }
    else
    {
        fprintf(a_pFileStreamParent, "ERROR: Failed to open XML test summary log file (%s).\n", a_filename.get_cstr());
    }

    fs.Close();

    OSCL_HeapString<OsclMemAllocator> outFilename(a_filename);
    outFilename += ".out";
    // open a new stream to file and return it to client
    a_pFileStreamChild = fopen(outFilename.get_cstr(), "w");
}


//==============================================================================
// FinalizeReporting                                             PUBLIC STATIC
//==============================================================================
//
OSCL_EXPORT_REF void
UT::CM::FinalizeReporting
(
    const char* a_pszTestname,
    OSCL_HeapString<OsclMemAllocator> &a_filename,
    const test_result& a_tr,
    FILE* a_pFileStreamParent,
    FILE* a_pFileStreamChild
)
{
    if (0 == a_pFileStreamChild)
        return;

    //                     report the textual representation of the test results
    text_test_interpreter interp;
    _STRING rs = interp.interpretation(a_tr);
    fprintf(a_pFileStreamChild, "%s", rs.c_str());

    if (0 == a_pszTestname || 0 >= a_filename.get_size() || 0 == a_pFileStreamParent)
        return;

    _STRING strChild;

    fclose(a_pFileStreamChild);                              // close the stream

    OSCL_HeapString<OsclMemAllocator> outFilename(a_filename);
    outFilename += ".out";

    FILE* pFile = fopen(outFilename.get_cstr(), "rb");
    if (0 == pFile)
        fprintf(a_pFileStreamParent, "ERROR: Failed to open file (%s) for capturing test output!\n", outFilename.get_cstr());
    else
    {
        fseek(pFile, 0, SEEK_END);
        long lSize = ftell(pFile);
        rewind(pFile);
        char* buffer = new char[lSize];
        fread(buffer, 1, lSize, pFile);
        strChild = _STRING(buffer, lSize);
        fprintf(a_pFileStreamParent, "%s", strChild.c_str()); // send the captured output back out the parent stream
        delete [] buffer;
        fclose(pFile);
    }

    Oscl_FileServer fs;
    fs.Connect();

    Oscl_File f;
    if (0 == f.Open(a_filename.get_str(), Oscl_File::MODE_READWRITE | Oscl_File::MODE_TEXT, fs))
    {
        _STRING xfr = xml_test_interpreter::interpretation(a_tr, a_pszTestname, &strChild);
        fprintf(a_pFileStreamParent, "\nWrote xml-formatted test results to file: %s\n", a_filename.get_cstr());
        f.Write(xfr.c_str(), sizeof(char), oscl_strlen(xfr.c_str()));
        f.Close();
    }
    else
    {
        fprintf(a_pFileStreamParent, "\nERROR: Failed to open file (%s) for xml-formatted test results\n", a_filename.get_cstr());
    }

    fs.Close();
}

// END FILE
