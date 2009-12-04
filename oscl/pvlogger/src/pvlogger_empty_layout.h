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
#ifndef PVLOGGER_EMPTY_LAYOUT_H_INCLUDED
#define PVLOGGER_EMPTY_LAYOUT_H_INCLUDED


#ifndef PVLOGGERACCESSORIES_H_INCLUDED
#include "pvlogger_accessories.h"
#endif

#ifndef OSCL_SNPRINTF_H_INCLUDED
#include "oscl_snprintf.h"
#endif

#ifndef OSCL_THREAD_H_INCLUDED
#include "oscl_thread.h"
#endif

#define OSCLCLOCKUNIT_SEC = 2
/**
 * Class: MsgFormatter
 *
 * This class currently implements formatting of text messages. A time stamp is
 * appended to each meessage, prior to formatting. For message buffers, time stamp addition
 * is performed, but no formatting is done.
 *
 */

class EmptyLayout : public PVLoggerLayout
{
    public:
        EmptyLayout() {};
        virtual ~EmptyLayout() {};

        typedef PVLoggerLayout::message_id_type message_id_type;

        int32 FormatString(char* formatBuf, int32 formatBufSize,
                           message_id_type msgID, const char * fmt, va_list va)
        {
            OSCL_UNUSED_ARG(msgID);
            return oscl_vsnprintf(formatBuf,
                                  formatBufSize,
                                  fmt, va);
        };

        int32 FormatOpaqueMessage(char* formatBuf, int32 maxformatBufSize,
                                  message_id_type msgID, int32 numPairs, va_list va)
        {
            OSCL_UNUSED_ARG(formatBuf);
            OSCL_UNUSED_ARG(maxformatBufSize);
            OSCL_UNUSED_ARG(msgID);
            OSCL_UNUSED_ARG(numPairs);
            OSCL_UNUSED_ARG(va);
            return 0;
        }
};

#endif // PVLOGGER_EMPTY_LAYOUT_H_INCLUDED


