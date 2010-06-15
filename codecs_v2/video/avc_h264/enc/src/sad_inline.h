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
#ifndef _SAD_INLINE_H_
#define _SAD_INLINE_H_

#include "oscl_base_macros.h"// has integer values of PV_COMPILER

#ifdef __cplusplus
extern "C"
{
#endif

    __inline int32 SUB_SAD(int32 sad, int32 tmp, int32 tmp2)
    {
        tmp = tmp - tmp2;
        if (tmp > 0) sad += tmp;
        else sad -= tmp;

        return sad;
    }

    __inline int32 sad_4pixel(int32 src1, int32 src2, int32 mask)
    {
        int32 x7;

        x7 = src2 ^ src1;       /* check odd/even combination */
        if ((uint32)src2 >= (uint32)src1)
        {
            src1 = src2 - src1;     /* subs */
        }
        else
        {
            src1 = src1 - src2;
        }
        x7 = x7 ^ src1;     /* only odd bytes need to add carry */
        x7 = mask & ((uint32)x7 >> 1);
        x7 = (x7 << 8) - x7;
        src1 = src1 + (x7 >> 7); /* add 0xFF to the negative byte, add back carry */
        src1 = src1 ^(x7 >> 7);   /* take absolute value of negative byte */

        return src1;
    }

#define NUMBER 3
#define SHIFT 24

#include "sad_mb_offset.h"

#undef NUMBER
#define NUMBER 2
#undef SHIFT
#define SHIFT 16
#include "sad_mb_offset.h"

#undef NUMBER
#define NUMBER 1
#undef SHIFT
#define SHIFT 8
#include "sad_mb_offset.h"


    __inline int32 simd_sad_mb(uint8 *ref, uint8 *blk, int dmin, int lx)
    {
        int32 x4, x5, x6, x8, x9, x10, x11, x12, x14;

        x9 = 0x80808080; /* const. */

        x8 = (uint32)ref & 0x3;
        if (x8 == 3)
            goto SadMBOffset3;
        if (x8 == 2)
            goto SadMBOffset2;
        if (x8 == 1)
            goto SadMBOffset1;

//  x5 = (x4<<8)-x4; /* x5 = x4*255; */
        x4 = x5 = 0;

        x6 = 0xFFFF00FF;

        ref -= lx;
        blk -= 16;

        x8 = 16;

LOOP_SAD0:
        /****** process 8 pixels ******/
        x10 = *((uint32*)(ref += lx));
        x11 = *((uint32*)(ref + 4));
        x12 = *((uint32*)(blk += 16));
        x14 = *((uint32*)(blk + 4));

        /* process x11 & x14 */
        x11 = sad_4pixel(x11, x14, x9);

        /* process x12 & x10 */
        x10 = sad_4pixel(x10, x12, x9);

        x5 = x5 + x10; /* accumulate low bytes */
        x10 = x10 & (x6 << 8); /* x10 & 0xFF00FF00 */
        x4 = x4 + ((uint32)x10 >> 8);  /* accumulate high bytes */
        x5 = x5 + x11;  /* accumulate low bytes */
        x11 = x11 & (x6 << 8); /* x11 & 0xFF00FF00 */
        x4 = x4 + ((uint32)x11 >> 8);  /* accumulate high bytes */

        /****** process 8 pixels ******/
        x10 = *((uint32*)(ref + 8));
        x11 = *((uint32*)(ref + 12));
        x12 = *((uint32*)(blk + 8));
        x14 = *((uint32*)(blk + 12));

        /* process x11 & x14 */
        x11 = sad_4pixel(x11, x14, x9);

        /* process x12 & x10 */
        x10 = sad_4pixel(x10, x12, x9);

        x5 = x5 + x10;  /* accumulate low bytes */
        x10 = x10 & (x6 << 8); /* x10 & 0xFF00FF00 */
        x4 = x4 + ((uint32)x10 >> 8); /* accumulate high bytes */
        x5 = x5 + x11;  /* accumulate low bytes */
        x11 = x11 & (x6 << 8); /* x11 & 0xFF00FF00 */
        x4 = x4 + ((uint32)x11 >> 8);  /* accumulate high bytes */

        /****************/
        x10 = x5 - (x4 << 8); /* extract low bytes */
        x10 = x10 + x4;     /* add with high bytes */
        x10 = x10 + (x10 << 16); /* add with lower half word */

        if ((int)((uint32)x10 >> 16) <= dmin) /* compare with dmin */
        {
            if (--x8)
            {
                goto LOOP_SAD0;
            }

        }

        return ((uint32)x10 >> 16);

SadMBOffset3:

        return sad_mb_offset3(ref, blk, lx, dmin);

SadMBOffset2:

        return sad_mb_offset2(ref, blk, lx, dmin);

SadMBOffset1:

        return sad_mb_offset1(ref, blk, lx, dmin);

    }


#ifdef __cplusplus
}
#endif

#endif // _SAD_INLINE_H_

