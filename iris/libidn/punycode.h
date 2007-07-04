/* punycode.h	Declarations for punycode functions.
 * Copyright (C) 2002, 2003  Simon Josefsson
 *
 * This file is part of GNU Libidn.
 *
 * GNU Libidn is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GNU Libidn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GNU Libidn; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * This file is derived from RFC 3492 written by Adam M. Costello.
 *
 * Disclaimer and license: Regarding this entire document or any
 * portion of it (including the pseudocode and C code), the author
 * makes no guarantees and is not responsible for any damage resulting
 * from its use.  The author grants irrevocable permission to anyone
 * to use, modify, and distribute it in any way that does not diminish
 * the rights of anyone else to use, modify, and distribute it,
 * provided that redistributed derivative works do not contain
 * misleading author or version information.  Derivative works need
 * not be licensed under similar terms.
 *
 * Copyright (C) The Internet Society (2003).  All Rights Reserved.
 *
 * This document and translations of it may be copied and furnished to
 * others, and derivative works that comment on or otherwise explain it
 * or assist in its implementation may be prepared, copied, published
 * and distributed, in whole or in part, without restriction of any
 * kind, provided that the above copyright notice and this paragraph are
 * included on all such copies and derivative works.  However, this
 * document itself may not be modified in any way, such as by removing
 * the copyright notice or references to the Internet Society or other
 * Internet organizations, except as needed for the purpose of
 * developing Internet standards in which case the procedures for
 * copyrights defined in the Internet Standards process must be
 * followed, or as required to translate it into languages other than
 * English.
 *
 * The limited permissions granted above are perpetual and will not be
 * revoked by the Internet Society or its successors or assigns.
 *
 * This document and the information contained herein is provided on an
 * "AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
 * TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
 * HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _PUNYCODE_H
#define _PUNYCODE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>		/* size_t */
#include <libidn/idn-int.h>		/* my_uint32_t */

  typedef enum
  {
    PUNYCODE_SUCCESS = 0,
    PUNYCODE_BAD_INPUT,		/* Input is invalid.                       */
    PUNYCODE_BIG_OUTPUT,	/* Output would exceed the space provided. */
    PUNYCODE_OVERFLOW		/* Input needs wider integers to process.  */
  } Punycode_status;

  /* For RFC compatibility. */
  enum punycode_status
  {
    punycode_success = PUNYCODE_SUCCESS,
    punycode_bad_input = PUNYCODE_BAD_INPUT,
    punycode_big_output = PUNYCODE_BIG_OUTPUT,
    punycode_overflow = PUNYCODE_OVERFLOW
  };

  typedef my_uint32_t punycode_uint;

  int punycode_encode (size_t input_length,
		       const punycode_uint input[],
		       const unsigned char case_flags[],
		       size_t * output_length, char output[]);

  /* punycode_encode() converts Unicode to Punycode.  The input     */
  /* is represented as an array of Unicode code points (not code    */
  /* units; surrogate pairs are not allowed), and the output        */
  /* will be represented as an array of ASCII code points.  The     */
  /* output string is *not* null-terminated; it will contain        */
  /* zeros if and only if the input contains zeros.  (Of course     */
  /* the caller can leave room for a terminator and add one if      */
  /* needed.)  The input_length is the number of code points in     */
  /* the input.  The output_length is an in/out argument: the       */
  /* caller passes in the maximum number of code points that it     */
  /* can receive, and on successful return it will contain the      */
  /* number of code points actually output.  The case_flags array   */
  /* holds input_length boolean values, where nonzero suggests that */
  /* the corresponding Unicode character be forced to uppercase     */
  /* after being decoded (if possible), and zero suggests that      */
  /* it be forced to lowercase (if possible).  ASCII code points    */
  /* are encoded literally, except that ASCII letters are forced    */
  /* to uppercase or lowercase according to the corresponding       */
  /* uppercase flags.  If case_flags is a null pointer then ASCII   */
  /* letters are left as they are, and other code points are        */
  /* treated as if their uppercase flags were zero.  The return     */
  /* value can be any of the punycode_status values defined above   */
  /* except punycode_bad_input; if not punycode_success, then       */
  /* output_size and output might contain garbage.                  */

  int punycode_decode (size_t input_length,
		       const char input[],
		       size_t * output_length,
		       punycode_uint output[], unsigned char case_flags[]);

  /* punycode_decode() converts Punycode to Unicode.  The input is  */
  /* represented as an array of ASCII code points, and the output   */
  /* will be represented as an array of Unicode code points.  The   */
  /* input_length is the number of code points in the input.  The   */
  /* output_length is an in/out argument: the caller passes in      */
  /* the maximum number of code points that it can receive, and     */
  /* on successful return it will contain the actual number of      */
  /* code points output.  The case_flags array needs room for at    */
  /* least output_length values, or it can be a null pointer if the */
  /* case information is not needed.  A nonzero flag suggests that  */
  /* the corresponding Unicode character be forced to uppercase     */
  /* by the caller (if possible), while zero suggests that it be    */
  /* forced to lowercase (if possible).  ASCII code points are      */
  /* output already in the proper case, but their flags will be set */
  /* appropriately so that applying the flags would be harmless.    */
  /* The return value can be any of the punycode_status values      */
  /* defined above; if not punycode_success, then output_length,    */
  /* output, and case_flags might contain garbage.  On success, the */
  /* decoder will never need to write an output_length greater than */
  /* input_length, because of how the encoding is defined.          */

#ifdef __cplusplus
}
#endif
#endif				/* _PUNYCODE_H */
