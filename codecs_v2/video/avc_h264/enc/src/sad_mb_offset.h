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
#include "oscl_base_macros.h"// has integer values of PV_COMPILER

/*C Code*/
#if (NUMBER==3)
__inline int32 sad_mb_offset3(uint8 *ref, uint8 *blk, int lx, int dmin)
#elif (NUMBER==2)
__inline int32 sad_mb_offset2(uint8 *ref, uint8 *blk, int lx, int dmin)
#elif (NUMBER==1)
__inline int32 sad_mb_offset1(uint8 *ref, uint8 *blk, int lx, int dmin)
#endif
{
    int32 x4, x5, x6, x8, x9, x10, x11, x12, x14;

    //  x5 = (x4<<8) - x4;
    x4 = x5 = 0;
    x6 = 0xFFFF00FF;
    x9 = 0x80808080; /* const. */
    ref -= NUMBER; /* bic ref, ref, #3 */
    ref -= lx;
    blk -= 16;
    x8 = 16;

#if (NUMBER==3)
LOOP_SAD3:
#elif (NUMBER==2)
LOOP_SAD2:
#elif (NUMBER==1)
LOOP_SAD1:
#endif
    /****** process 8 pixels ******/
    x10 = *((uint32*)(ref += lx)); /* D C B A */
    x11 = *((uint32*)(ref + 4));    /* H G F E */
    x12 = *((uint32*)(ref + 8));    /* L K J I */

    x10 = ((uint32)x10 >> SHIFT); /* 0 0 0 D */
    x10 = x10 | (x11 << (32 - SHIFT));        /* G F E D */
    x11 = ((uint32)x11 >> SHIFT); /* 0 0 0 H */
    x11 = x11 | (x12 << (32 - SHIFT));        /* K J I H */

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
    x10 = *((uint32*)(ref + 8)); /* D C B A */
    x11 = *((uint32*)(ref + 12));   /* H G F E */
    x12 = *((uint32*)(ref + 16));   /* L K J I */

    x10 = ((uint32)x10 >> SHIFT); /* mvn x10, x10, lsr #24  = 0xFF 0xFF 0xFF ~D */
    x10 = x10 | (x11 << (32 - SHIFT));        /* bic x10, x10, x11, lsl #8 = ~G ~F ~E ~D */
    x11 = ((uint32)x11 >> SHIFT); /* 0xFF 0xFF 0xFF ~H */
    x11 = x11 | (x12 << (32 - SHIFT));        /* ~K ~J ~I ~H */

    x12 = *((uint32*)(blk + 8));
    x14 = *((uint32*)(blk + 12));

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

    /****************/
    x10 = x5 - (x4 << 8); /* extract low bytes */
    x10 = x10 + x4;     /* add with high bytes */
    x10 = x10 + (x10 << 16); /* add with lower half word */

    if ((int)((uint32)x10 >> 16) <= dmin) /* compare with dmin */
    {
        if (--x8)
        {
#if (NUMBER==3)
            goto         LOOP_SAD3;
#elif (NUMBER==2)
            goto         LOOP_SAD2;
#elif (NUMBER==1)
            goto         LOOP_SAD1;
#endif
        }

    }

    return ((uint32)x10 >> 16);
}



