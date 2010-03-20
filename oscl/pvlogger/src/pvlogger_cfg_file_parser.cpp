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

#ifndef PVLOGGER_CFG_FILE_PARSER_H_INCLUDED
#include "pvlogger_cfg_file_parser.h"
#endif

#ifndef PVLOGGER_FILE_APPENDER_H_INCLUDED
#include "pvlogger_file_appender.h"
#endif

#ifndef PVLOGGER_STDERR_APPENDER_H_INCLUDED
#include "pvlogger_stderr_appender.h"
#endif

#ifndef PVLOGGER_MEM_APPENDER_H_INCLUDED
#include "pvlogger_mem_appender.h"
#endif

#ifndef PVLOGGER_TIME_AND_ID_LAYOUT_H_INCLUDED
#include "pvlogger_time_and_id_layout.h"
#endif

#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif


//******************************************************************************
//                                                                      MACROS
//******************************************************************************

// Macro redefinition for typing convenience
#define _CSTR(x) _STRLIT_CHAR(x) // define an 8-bit char*


//******************************************************************************
//                                                                    TYPEDEFS
//******************************************************************************

typedef OSCL_HeapString<OsclMemAllocator> HeapString;


//==============================================================================
// Parse                                                         PUBLIC STATIC
//==============================================================================
// Open pszCfgFileName to read nodes (and levels) to log.  Returns 'true' on
// success, 'false' otherwise.
//
OSCL_EXPORT_REF bool
PVLoggerCfgFileParser::Parse
(
    const char* pszCfgFileName,              // name of logger cfg file to parse
    const char* pszLogFileName,                 // name of file to log output to
    eAppenderType_t* pAppenderType/*=0*/,               // optional return value
    Oscl_Vector<LogCfgElement, OsclMemAllocator>* pVectorLogNodeCfg/*=0*/
)
{
    if (0 == pszCfgFileName || 0 == pszLogFileName)
    {
        OSCL_ASSERT(pszCfgFileName);
        OSCL_ASSERT(pszLogFileName);
        return false;
    }

    Oscl_FileServer fs;
    Oscl_File fh;

    fs.Connect();              // read configuration file from working directory
    bool bFileExists = false;
    if (0 == fh.Open(pszCfgFileName, Oscl_File::MODE_READ, fs))
        bFileExists = true;

    if (false == bFileExists || 0 == fh.Size()) // file does not exist or empty?
    {
        if (true == bFileExists)
            fh.Close();
        fs.Close();                                             // close up shop
        return false;                                        // indicate failure
    }

    const int32 MAXLENGTH = (int32)fh.Size();          // max amount of data to process
    uint32 offsetFVL = 0;            // file offset to start of first valid line
    uint32 offsetNUL = 0;       // file offset to start of next unprocessed line
    OsclSharedPtr<PVLoggerAppender> appenderPtr;

    bool bAppenderCreated = false;
    bool bEof = false;
    while (false == bEof)
    {
        char* pszLine = (char*)OSCL_MALLOC(MAXLENGTH + 1);
        if (0 == pszLine)                             // memory allocation fail?
            return false;

        fh.Seek(offsetNUL, Oscl_File::SEEKSET);     // seek to beginning of file
        if (0 != fh.Read((void*)pszLine, 2, MAXLENGTH))// read as much as we can
        {
            // obtain a pointer to next newline character in file and
            // deliberately remove const-ness from oscl_strstr return
            char* pNewline = OSCL_CONST_CAST(char*, oscl_strstr(pszLine, _CSTR("\n")));
            if (0 != pNewline)                       // not end-of-file reached?
            {                            // terminate the line with a null value
                if ('\r' == *(pNewline - 1))               // win32 line-ending?
                    *(pNewline - 1) = '\0';  // terminate at the carriage return
                else
                    *pNewline = '\0';                // terminate at the newline

                offsetNUL += pNewline - pszLine + 1; // update where we left off
            }
            else
            {
                bEof = true;                     // indicate end of file reached
            }

            if ('#' == *pszLine)                              // commented line?
            {
                offsetFVL = offsetNUL;     // advance FVL offset to skip comment
                OSCL_FREE(pszLine);             // free memory to commented line
                pszLine = 0;
                continue;
            }

            if (false == bAppenderCreated)  // have not created appender object?
            {
                // if first uncommented line is:
                //    NO_LOGGING ==> all log output will be disabled
                //    LOGTOFILE  ==> a file appender will be created
                //    LOGTOMEM   ==> memory appender will be created
                // otherwise, all logs will be dumped to the console
                if ((0 == offsetNUL) || (offsetFVL == offsetNUL - (pNewline - pszLine + 1)))
                {
                    if (oscl_strstr(pszLine, _CSTR("NO_LOGGING")))
                    {
                        fh.Close();
                        fs.Close();
                        OSCL_FREE(pszLine);
                        pszLine = 0;
                        return true;
                    }

                    eAppenderType_t at;
                    if (oscl_strstr(pszLine, _CSTR("LOGTOFILE")))
                        at = ePVLOG_APPENDER_FILE;
                    else if (oscl_strstr(pszLine, _CSTR("LOGTOFILE_FLUSH")))
                        at = ePVLOG_APPENDER_FILE_FLUSH;
                    else if (oscl_strstr(pszLine, _CSTR("LOGTOMEM")))
                        at = ePVLOG_APPENDER_MEMORY;
                    else
                        at = ePVLOG_APPENDER_STDERR;

                    if (0 != pAppenderType)            // optional return value?
                        *pAppenderType = at;

                    OsclRefCounter* pRC = 0;
                    // error creating log appender?
                    if (OsclErrNone != CreateLogAppender(at, pszLogFileName,
                                                         pRC, appenderPtr))
                    {
                        OSCL_ASSERT(!_CSTR("unable to create log appender"));
                        appenderPtr.Unbind();
                        if (0 != pRC)
                            delete pRC;

                        OSCL_FREE(pszLine);
                        pszLine = 0;
                        return false;
                    }

                    bAppenderCreated = true;        // indicate appender created
                }
            }

            const char* pszMarker = oscl_strstr(pszLine, _CSTR(";"));
            if (0 == pszMarker)
            {
                OSCL_FREE(pszLine);
                pszLine = 0;
                continue;
            }

            char* pszNode = (char*)OSCL_MALLOC(MAXLENGTH + 1);
            if (0 == pszNode)
            {
                OSCL_FREE(pszLine);
                pszLine = 0;
                return false;
            }

            const uint32 nodeLen = pszMarker - pszLine;
            oscl_strncpy(pszNode, pszLine, nodeLen);
            *(pszNode + nodeLen) = '\0';
            // extract the log level
            HeapString* pStrLogLevel = OSCL_NEW(HeapString, (oscl_strstr(pszLine, _CSTR(";")) + 1));
            if (0 == pStrLogLevel)
            {
                OSCL_FREE(pszNode);
                OSCL_FREE(pszLine);
                pszLine = 0;
                return false;
            }

            const char* pszLogLevel = pStrLogLevel->get_cstr();
            bool bLogAllNodes = false;

            // if node = ALLNODES; enable logging for all. ALLNODES should
            // be written in the file before any other logger node
            if (oscl_strstr(pszNode, _CSTR("ALLNODES")))
                bLogAllNodes = true;

            PVLogger::log_level_type eLogLevel = PVLOGMSG_INFO;
            if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_EMERG")))
                eLogLevel = PVLOGMSG_EMERG;
            else if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_ALERT")))
                eLogLevel = PVLOGMSG_ALERT;
            else if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_CRIT")))
                eLogLevel = PVLOGMSG_CRIT;
            else if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_ERR")))
                eLogLevel = PVLOGMSG_ERR;
            else if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_WARNING")))
                eLogLevel = PVLOGMSG_WARNING;
            else if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_NOTICE")))
                eLogLevel = PVLOGMSG_NOTICE;
            else if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_INFO")))
                eLogLevel = PVLOGMSG_INFO;
            else if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_STACK_TRACE")))
                eLogLevel = PVLOGMSG_STACK_TRACE;
            else if (oscl_strstr(pszLogLevel, _CSTR("PVLOGMSG_DEBUG")))
                eLogLevel = PVLOGMSG_DEBUG;

            AttachLogAppender(appenderPtr, bLogAllNodes ? "" : pszNode, eLogLevel);

            if (0 != pVectorLogNodeCfg)               // caller supplied vector?
            {
                LogCfgElement elem(bLogAllNodes ? "" : pszNode, eLogLevel);
                pVectorLogNodeCfg->push_back(elem);     // add element to vector
            }

            if (true == bLogAllNodes)
                bEof = true;

            if (0 != pszNode)
                OSCL_FREE(pszNode);
            if (0 != pStrLogLevel)
                OSCL_DELETE(pStrLogLevel);
        }
        else                                              // file read returns 0
        {
            bEof = true;                          // reached the end of the file
        }

        OSCL_FREE(pszLine);
        pszLine = 0;
    } // end of while()

    fh.Close();
    fs.Close();
    return true;
}


//==============================================================================
// CreateLogAppender                                               FILE STATIC
//==============================================================================
//
OSCL_EXPORT_REF int32
PVLoggerCfgFileParser::CreateLogAppender
(
    eAppenderType_t eAppenderType,
    const char* pszLogFileName,
    OsclRefCounter*& pRC,
    OsclSharedPtr<PVLoggerAppender>& appenderPtr
)
{
    PVLoggerAppender* pAppender = 0;

    int32 err;
    switch (eAppenderType)
    {
        case ePVLOG_APPENDER_FILE:
        {
            OSCL_ASSERT(pszLogFileName);
            typedef TextFileAppender<TimeAndIdLayout, 1024> typeAppender;
            OSCL_TRY(
                err,
                pAppender = (PVLoggerAppender*)typeAppender::CreateAppender(pszLogFileName);
                pRC = new OsclRefCounterSA<LogAppenderDestructDealloc<typeAppender> >(pAppender);
                appenderPtr.Bind(pAppender, pRC);
            );
            break;
        }
        case ePVLOG_APPENDER_FILE_FLUSH:
        {
            OSCL_ASSERT(pszLogFileName);
            typedef TextFileAppender<TimeAndIdLayout, 1024> typeAppender;
            OSCL_TRY(
                err,
                pAppender = (PVLoggerAppender*)typeAppender::CreateAppender(pszLogFileName, 0, true); // enable flushing
                pRC = new OsclRefCounterSA<LogAppenderDestructDealloc<typeAppender> >(pAppender);
                appenderPtr.Bind(pAppender, pRC);
            );
            break;
        }
        case ePVLOG_APPENDER_MEMORY:
        {
            OSCL_ASSERT(pszLogFileName);
            typedef MemAppender<TimeAndIdLayout, 1024> typeAppender;
            OSCL_TRY(
                err,
                pAppender = (PVLoggerAppender*)typeAppender::CreateAppender(pszLogFileName);
                pRC = new OsclRefCounterSA<LogAppenderDestructDealloc<typeAppender> >(pAppender);
                appenderPtr.Bind(pAppender, pRC);
            );
            break;
        }
        case ePVLOG_APPENDER_STDERR:
        default:
        {
            typedef StdErrAppender<TimeAndIdLayout, 1024> typeAppender;
            OSCL_TRY(
                err,
                pAppender = new typeAppender();
                pRC = new OsclRefCounterSA<LogAppenderDestructDealloc<typeAppender> >(pAppender);
                appenderPtr.Bind(pAppender, pRC);
            );
            break;
        }
    }

    if (OsclErrNone != err)
    {
        delete pAppender;
        delete pRC;
        pAppender = 0;
        pRC = 0;
    }

    return err;
}



//==============================================================================
// AttachLogAppender                                               FILE STATIC
//==============================================================================
//
OSCL_EXPORT_REF bool
PVLoggerCfgFileParser::AttachLogAppender
(
    OsclSharedPtr<PVLoggerAppender>& appenderPtr,
    const char* pszLogNode/*=""*/,
    const PVLogger::log_level_type eLogLevel/*=PVLOGMSG_DEBUG*/
)
{
    OSCL_ASSERT(pszLogNode);
    PVLogger*const pLogger = PVLogger::GetLoggerObject(_CSTR(pszLogNode));
    pLogger->AddAppender(appenderPtr);
    pLogger->SetLogLevel(eLogLevel);
    return true;
}



//==============================================================================
// SetupLogAppender                                                FILE STATIC
//==============================================================================
//
OSCL_EXPORT_REF bool
PVLoggerCfgFileParser::SetupLogAppender
(
    eAppenderType_t eAppenderType,
    const char* pszLogFileName/*=0*/,
    const char* pszLogNode/*=""*/,
    const PVLogger::log_level_type eLogLevel/*=PVLOGMSG_DEBUG*/,
    OsclSharedPtr<PVLoggerAppender>* pSharedAppenderPtr/*=0*/
)
{
    OSCL_ASSERT(pszLogNode);

    OsclRefCounter* pRC = 0;
    OsclSharedPtr<PVLoggerAppender> appenderPtr;

    if (OsclErrNone != CreateLogAppender(eAppenderType, pszLogFileName, pRC, appenderPtr))
        return false;

    if (0 != pSharedAppenderPtr)            // provide the pointer to the caller
        *pSharedAppenderPtr = appenderPtr;

    return AttachLogAppender(appenderPtr, _CSTR(pszLogNode), eLogLevel);
}


// END FILE

