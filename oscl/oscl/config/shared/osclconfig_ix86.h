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
// -*- c++ -*-
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

//     O S C L C O N F I G   ( P L A T F O R M   C O N F I G   I N F O )

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =


/*! \file osclconfig_ix86.h
 *  \brief This file contains configuration information for the ix86 processor family
 *
 */

#ifndef OSCLCONFIG_IX86_H_INCLUDED
#define OSCLCONFIG_IX86_H_INCLUDED

#include <endian.h>


// Define macros for integer alignment and little endian byte order.
#define OSCL_INTEGERS_WORD_ALIGNED               1


// though this file is called '...x86', it is included in _ALL_
// android builds, regardless of target CPU.
// Target endian determination must be made here.

#if !defined(__BYTE_ORDER) || !defined (__BIG_ENDIAN)
#error __BYTE_ORDER macros not defined
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    #define OSCL_BYTE_ORDER_BIG_ENDIAN               1
    #define OSCL_BYTE_ORDER_LITTLE_ENDIAN            0
#else
    #define OSCL_BYTE_ORDER_BIG_ENDIAN               0
    #define OSCL_BYTE_ORDER_LITTLE_ENDIAN            1
#endif // __BYTE_ORDER

#endif //OSCLCONFIG_IX86_H_INCLUDED
