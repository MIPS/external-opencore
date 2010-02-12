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
/**
This file contains bitstream related functions.
@publishedAll
*/

#ifndef _PV_CONFIG_BITSTREAM_H_
#define _PV_CONFIG_BITSTREAM_H_

#ifndef PVMF_RETURN_CODES_H_INCLUDED
#include "pvmf_return_codes.h"
#endif


#include "oscl_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
    Bitstream structure contains bitstream related parameters such as the pointer
    to the buffer, the current byte position and bit position.
    @publishedAll
    */
    typedef struct tagDecBitstream
    {
        uint8 *bitstreamBuffer; /* pointer to buffer memory   */
        int nal_size;  /* size of the current NAL unit */
        int data_end_pos;  /* bitstreamBuffer size in bytes */
        int read_pos;  /* next position to read from bitstreamBuffer  */
        uint curr_word; /* byte-swapped (MSB left) current word read from buffer */
        int bit_left;      /* number of bit left in current_word */
        uint next_word;  /* in case for old data in previous buffer hasn't been flushed. */
        int incnt; /* bit left in the prev_word */
        int incnt_next;
        int bitcnt;
        void *userData;
    } DecBitstream;

#define PvConfigBitstreamFlushBits(A,B)  {(A)->bitcnt += (B); (A)->incnt -= (B); (A)->curr_word <<= (B);}


    PVMFStatus PvConfigBitstreamFillCache(DecBitstream *stream);
    /**
    This function populates bitstream structure.
    \param "stream" "Pointer to bitstream structure."
    \param "buffer" "Pointer to the bitstream buffer."
    \param "size" "Size of the buffer."
    \param "nal_size" "Size of the NAL unit."
    \param "resetall" "Flag for reset everything."
    \return "DEC_SUCCESS for success and DEC_FAIL for fail."
    */
    PVMFStatus PvConfigBitstreamInit(DecBitstream *stream, uint8 *buffer, int size);

    /**
    This function reads next aligned word and remove the emulation prevention code
    if necessary.
    \param "stream" "Pointer to bitstream structure."
    \return "Next word."
    */
    uint BitstreamNextWord(DecBitstream *stream);

    /**
    This function reads nBits bits from the current position and advance the pointer.
    \param "stream" "Pointer to bitstream structure."
    \param "nBits" "Number of bits to be read."
    \param "code" "Point to the read value."
    \return "DEC_SUCCESS if successed, DEC_FAIL if number of bits
       is greater than the word-size, DEC_PACKET_LOSS or
       DEC_NO_DATA if callback to get data fails."
    */
    PVMFStatus PvConfigBitstreamReadBits(DecBitstream *stream, int nBits, uint *code);

    /**
    This function shows nBits bits from the current position without advancing the pointer.
    \param "stream" "Pointer to bitstream structure."
    \param "nBits" "Number of bits to be read."
    \param "code" "Point to the read value."
    \return "DEC_SUCCESS if successed, DEC_FAIL if number of bits
        is greater than the word-size, DEC_NO_DATA if it needs
        to callback to get data."
    */
    PVMFStatus PvConfigBitstreamShowBits(DecBitstream *stream, int nBits, uint *code);


    /**
    This function flushes nBits bits from the current position.
    \param "stream" "Pointer to bitstream structure."
    \param "nBits" "Number of bits to be read."
    \return "DEC_SUCCESS if successed, DEC_FAIL if number of bits
        is greater than the word-size It will not call back to get
          more data. Users should call BitstreamShowBits to determine
          how much they want to flush."
    */
    PVMFStatus PvConfigBitstreamFlushBitsCheck(DecBitstream *stream, int nBits);

    /**
    This function read 1 bit from the current position and advance the pointer.
    \param "stream" "Pointer to bitstream structure."
    \param "nBits" "Number of bits to be read."
    \param "code" "Point to the read value."
    \return "DEC_SUCCESS if successed, DEC_FAIL if number of bits
       is greater than the word-size, DEC_PACKET_LOSS or
       DEC_NO_DATA if callback to get data fails."
    */
    PVMFStatus PvConfigBitstreamRead1Bit(DecBitstream *stream, uint *code);

    /**
    This function checks whether the current bit position is byte-aligned or not.
    \param "stream" "Pointer to the bitstream structure."
    \return "TRUE if byte-aligned, FALSE otherwise."
    */
    bool byte_aligned(DecBitstream *stream);
    PVMFStatus PvConfigBitstreamByteAlign(DecBitstream  *stream);
    /**
    This function checks whether there are more RBSP data before the trailing bits.
    \param "stream" "Pointer to the bitstream structure."
    \return "TRUE if yes, FALSE otherwise."
    */
    bool PvConfigmore_rbsp_data(DecBitstream *stream);


#ifdef __cplusplus
}
#endif /* __cplusplus  */

#endif /* _PV_CONFIG_BITSTREAM_H_ */


