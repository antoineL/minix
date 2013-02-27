/*	Id	*/
/*
 * Copyright (c) 2014 Eric Olson <ejolson@renomath.org>
 * Some rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <ctype.h>
#include "pass1.h"
#include "manifest.h"
#include "unicode.h"

/*
 * decode 32-bit code point from UTF-8
 * move pointer
 */
unsigned int 
u82cp(char **q)
{
	unsigned char *t = (unsigned char *)*q;
	unsigned int c, r;
	int i, sz;

	if (*t == '\\')
		c = esccon((char **)&t);
	else
		c = *t++;

	/* always eat the first value */
	*q = (char *)t;

	if (c > 0x7F) {
		if ((c & 0xE0) == 0xC0) {
			sz = 2;
			r = c & 0x1F;
		} else if ((c & 0xF0) == 0xE0) {
			sz = 3;
			r = c & 0x0F;
		} else if ((c & 0xF8) == 0xF0) {
			sz = 4;
			r = c & 0x07;
		} else if ((c & 0xFC) == 0xF8) {
			sz = 5;
			r = c & 0x03;
		} else if ((c & 0xFE) == 0xFC) {
			sz = 6;
			r = c & 0x01;
		} else {
			u8error("invalid utf-8 prefix");
			return 0xFFFF;
		}

		for (i = 1; i < sz; i++) {
			if (*t == '\\')
				c = esccon((char **)&t);
			else
				c = *t++;

			if ((c & 0xC0) == 0x80) {
				r = (r << 6) + (c & 0x3F);
			} else {
				u8error("utf-8 encoding %d bytes too short", sz - i);
				return 0xFFFF;
			}
		}

		*q = (char *)t;
	} else {
		r = c;
	}

	return r;
}
