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
/*********************************************************************************/
/*  Filename: fastquant_inline.h                                                        */
/*  Description: Implementation for in-line functions used in dct.cpp           */
/*  Modified:                                                                   */
/*********************************************************************************/
#ifndef _FASTQUANT_INLINE_H_
#define _FASTQUANT_INLINE_H_

#include "mp4def.h"
#include "oscl_base_macros.h"


#if   ((PV_CPU_ARCH_VERSION >=5) && (PV_COMPILER == EPV_ARM_GNUC))/* ARM GNU COMPILER  */

__inline int32 aan_scale(int32 q_value, int32 coeff,
                         int32 round, int32 QPdiv2)
{
    register int32 out;
    register int32 qv = q_value;
    register int32 cf = coeff;
    register int32 rr = round;
    register int32 qp = QPdiv2;

    asm volatile("smlabb %0, %2, %1, %3\n\t"
                 "movs %0, %0, asr #16\n\t"
                 "addle %0, %0, %4\n\t"
                 "subgt %0, %0, %4"
             : "=&r"(out)
                         : "r"(qv),
                         "r"(cf),
                         "r"(rr),
                         "r"(qp));
    return out;
}

__inline int32 coeff_quant(int32 coeff, int32 q_scale, int32 shift)
{
    register int32 out;
    register int32 temp1;
    register int32 cc = coeff;
    register int32 qs = q_scale;
    register int32 ss = shift;

    asm volatile("smulbb %0, %3, %2\n\t"
                 "mov %1, %0, asr %4\n\t"
                 "add %0, %1, %1, lsr #31"
             : "=&r"(out),
                 "=&r"(temp1)
                         : "r"(cc),
                         "r"(qs),
                         "r"(ss));

    return out;
}

__inline int32 coeff_clip(int32 q_value, int32 ac_clip)
{
    register int32 coeff;

    asm volatile("add   %1, %0, %2\n\t"
                 "subs  %1, %1, %2, lsl #1\n\t"
                 "eorhi %0, %2, %0, asr #31"
             : "+r"(q_value),
                 "=&r"(coeff)
                         : "r"(ac_clip));

    return q_value;
}

__inline int32 coeff_dequant(int32 q_value, int32 QPx2, int32 Addition, int32 tmp)
{
    register int32 out;
    register int32 temp1;
    register int32 qv = q_value;
    register int32 qp = QPx2;
    register int32 aa = Addition;
    register int32 tt = tmp;

    asm volatile("cmp    %2, #0\n\t"
                 "mul    %0, %2, %3\n\t"
                 "sublt  %0, %0, %4\n\t"
                 "addge  %0, %0, %4\n\t"
                 "add    %1, %0, %5\n\t"
                 "subs   %1, %1, #3840\n\t"
                 "subcss %1, %1, #254\n\t"
                 "eorhi  %0, %5, %0, asr #31"
             : "=&r"(out),
                 "=&r"(temp1)
                         : "r"(qv),
                         "r"(qp),
                         "r"(aa),
                         "r"(tt));

    return out;
}

__inline int32 smlabb(int32 q_value, int32 coeff, int32 round)
{
    register int32 out;
    register int32 aa = (int32)q_value;
    register int32 bb = (int32)coeff;
    register int32 cc = (int32)round;

    asm volatile("smlabb %0, %1, %2, %3"
             : "=&r"(out)
                         : "r"(aa),
                         "r"(bb),
                         "r"(cc));
    return out;
}

__inline int32 smulbb(int32 q_scale, int32 coeff)
{
    register int32 out;
    register int32 aa = (int32)q_scale;
    register int32 bb = (int32)coeff;

    asm volatile("smulbb %0, %1, %2"
             : "=&r"(out)
                         : "r"(aa),
                         "r"(bb));
    return out;
}

__inline int32 aan_dc_scale(int32 coeff, int32 QP)
{
    register int32 out;
    register int32 cc = coeff;
    register int32 qp = QP;

    asm volatile("cmp %1, #0\n\t"
                 "addle %0, %1, %2, asr #1\n\t"
                 "subgt %0, %1, %2, asr #1"
             : "=&r"(out)
                         : "r"(cc),
                         "r"(qp));
    return out;
}

__inline int32 clip_2047(int32 q_value, int32 tmp)
{
    register int32 coeff;
    asm volatile("add    %1, %0, %2\n\t"
                 "subs   %1, %1, #0xF00\n\t"
                 "subcss %1, %1, #0xFE\n\t"
                 "eorhi  %0, %2, %0, asr #31"
             : "+r"(q_value),
                 "=&r"(coeff)
                         : "r"(tmp));

    return q_value;
}

__inline int32 coeff_dequant_mpeg(int32 q_value, int32 stepsize, int32 QP, int32 tmp)
{
    register int32 out;
    register int32 temp1;
    register int32 qv = q_value;
    register int32 ss = stepsize;
    register int32 qp = QP;
    register int32 tt = tmp;

    asm volatile("movs    %1, %2, lsl #1\n\t"
                 "mul     %0, %3, %4\n\t"
                 "addgt   %1, %1, #1\n\t"
                 "sublt   %1, %1, #1\n\t"
                 "mul     %0, %1, %0\n\t"
                 "addlt   %0, %0, #15\n\t"
                 "mov     %0, %0, asr #4\n\t"
                 "add     %1, %0, %5\n\t"
                 "subs    %1, %1, #0xF00\n\t"
                 "subcss  %1, %1, #0xFE\n\t"
                 "eorhi   %0, %5, %0, asr #31"
             : "=&r"(out),
                 "=&r"(temp1)
                         : "r"(qv),
                         "r"(ss),
                         "r"(qp),
                         "r"(tt));

    return out;

}

__inline int32 coeff_dequant_mpeg_intra(int32 q_value, int32 tmp)
{
    register int32 out;
    register int32 temp1;
    register int32 qv = q_value;
    register int32 tt = tmp;

    asm volatile("movs    %1, %2, lsl #1\n\t"
                 "addlt   %1, %1, #15\n\t"
                 "mov     %0, %1, asr #4\n\t"
                 "add     %1, %0, %3\n\t"
                 "subs    %1, %1, #0xF00\n\t"
                 "subcss  %1, %1, #0xFE\n\t"
                 "eorhi   %0, %3, %0, asr #31"
             : "=&r"(out),
                 "=&r"(temp1)
                         : "r"(qv),
                         "r"(tt));
    return out;
}

#else
/*C Equivalent Code*/
__inline int32 aan_scale(int32 q_value, int32 coeff, int32 round, int32 QPdiv2)
{
    q_value = coeff * q_value + round;
    coeff = q_value >> 16;
    if (coeff < 0)  coeff += QPdiv2;
    else            coeff -= QPdiv2;

    return coeff;
}


__inline int32 coeff_quant(int32 coeff, int32 q_scale, int32 shift)
{
    int32 q_value;

    q_value = coeff * q_scale;      //q_value = -((-(coeff + QPdiv2)*q_scale)>>LSL);
    q_value >>= shift;                  //q_value = (((coeff - QPdiv2)*q_scale)>>LSL );
    q_value += ((UInt)q_value >> 31); /* add one if negative */

    return q_value;
}

__inline int32  coeff_clip(int32 q_value, int32 ac_clip)
{
    int32 coeff = q_value + ac_clip;

    if ((UInt)coeff > (UInt)(ac_clip << 1))
        q_value = ac_clip ^(q_value >> 31);

    return q_value;
}

__inline int32 coeff_dequant(int32 q_value, int32 QPx2, int32 Addition, int32 tmp)
{
    int32 coeff;

    OSCL_UNUSED_ARG(tmp);

    if (q_value < 0)
    {
        coeff = q_value * QPx2 - Addition;
        if (coeff < -2048)
            coeff = -2048;
    }
    else
    {
        coeff = q_value * QPx2 + Addition;
        if (coeff > 2047)
            coeff = 2047;
    }
    return coeff;
}

__inline int32 smlabb(int32 q_value, int32 coeff, int32 round)
{
    q_value = coeff * q_value + round;

    return q_value;
}

__inline int32 smulbb(int32 q_scale, int32 coeff)
{
    int32 q_value;

    q_value = coeff * q_scale;

    return q_value;
}

__inline int32 aan_dc_scale(int32 coeff, int32 QP)
{

    if (coeff < 0)  coeff += (QP >> 1);
    else            coeff -= (QP >> 1);

    return coeff;
}

__inline int32 clip_2047(int32 q_value, int32 tmp)
{
    OSCL_UNUSED_ARG(tmp);

    if (q_value < -2048)
    {
        q_value = -2048;
    }
    else if (q_value > 2047)
    {
        q_value = 2047;
    }

    return q_value;
}

__inline int32 coeff_dequant_mpeg(int32 q_value, int32 stepsize, int32 QP, int32 tmp)
{
    int32 coeff;

    OSCL_UNUSED_ARG(tmp);

    coeff = q_value << 1;
    stepsize *= QP;
    if (coeff > 0)
    {
        q_value = (coeff + 1) * stepsize;
        q_value >>= 4;
        if (q_value > 2047) q_value = 2047;
    }
    else
    {
        q_value = (coeff - 1) * stepsize;
        q_value += 15;
        q_value >>= 4;
        if (q_value < -2048)    q_value = -2048;
    }

    return q_value;
}

__inline int32 coeff_dequant_mpeg_intra(int32 q_value, int32 tmp)
{
    OSCL_UNUSED_ARG(tmp);

    q_value <<= 1;
    if (q_value > 0)
    {
        q_value >>= 4;
        if (q_value > 2047) q_value = 2047;
    }
    else
    {
        q_value += 15;
        q_value >>= 4;
        if (q_value < -2048) q_value = -2048;
    }

    return q_value;
}
#endif

#endif //_FASTQUANT_INLINE_H_
