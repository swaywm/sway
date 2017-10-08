/*
 * Adapted from https://github.com/littlstar/b64.c
 * License under the MIT License:
 * Copyright (c) 2014 Little Star Media, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <ctype.h>
#include <stdlib.h>
#include "util.h"

static const char b64_table[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};

char *b64_encode(const char *src, size_t len, size_t *flen) {
	int i = 0;
	int j = 0;
	char *enc = NULL;
	size_t size = len * 4 / 3;
	size_t idx = 0;
	unsigned char buf[4];
	char tmp[3];

	// alloc
	enc = (char *) malloc(size + 1);
	if (NULL == enc) { return NULL; }

	// parse until end of source
	while (len--) {
		// read up to 3 bytes at a time into `tmp'
		tmp[i++] = *(src++);

		// if 3 bytes read then encode into `buf'
		if (3 == i) {
			buf[0] = (tmp[0] & 0xfc) >> 2;
			buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
			buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
			buf[3] = tmp[2] & 0x3f;

			// shouldn't really happen
			if (idx + 4 > size) {
				size += 16;
				enc = (char *) realloc(enc, size + 1);
			}
			for (i = 0; i < 4; ++i) {
				enc[idx++] = b64_table[buf[i]];
			}

			// reset index
			i = 0;
		}
	}

	// remainder
	if (i > 0) {
		// fill `tmp' with `\0' at most 3 times
		for (j = i; j < 3; ++j) {
			tmp[j] = '\0';
		}

		// perform same codec as above
		buf[0] = (tmp[0] & 0xfc) >> 2;
		buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
		buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
		buf[3] = tmp[2] & 0x3f;

		// perform same write to `enc` with new allocation
		size_t delta = (i > 3 ? 0 : 3 - i) + (j > i + 1 ? 0 : i + 1 - j);
		if (idx + delta > size) {
			size += delta;
			enc = (char *) realloc(enc, size + 1);
		}
		for (j = 0; (j < i + 1); ++j) {
			enc[idx++] = b64_table[buf[j]];
		}

		// while there is still a remainder
		// append `=' to `enc'
		while ((i++ < 3)) {
			enc[idx++] = '=';
		}
	}

	enc[idx] = '\0';

	if (flen)
		*flen = size;
	return enc;
}

unsigned char *b64_decode(const char *src, size_t len, size_t *decsize) {
	int i = 0;
	int j = 0;
	int l = 0;
	// max size estimate
	size_t size = len * 3 / 4;
	size_t idx = 0;
	unsigned char *dec = NULL;
	unsigned char buf[3];
	unsigned char tmp[4];

	// alloc
	dec = (unsigned char *) malloc(size + 1);
	if (NULL == dec) { return NULL; }

	// parse until end of source
	while (len--) {
		if (isspace(src[j])) { j++; continue; }
		// break if char is `=' or not base64 char
		if ('=' == src[j]) { break; }
		if (!(isalnum(src[j]) || '+' == src[j] || '/' == src[j])) { break; }

		// read up to 4 bytes at a time into `tmp'
		tmp[i++] = src[j++];

		// if 4 bytes read then decode into `buf'
		if (4 == i) {
			// translate values in `tmp' from table
			for (i = 0; i < 4; ++i) {
				// find translation char in `b64_table'
				for (l = 0; l < 64; ++l) {
					if (tmp[i] == b64_table[l]) {
						tmp[i] = l;
						break;
					}
				}
			}

			// decode
			buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
			buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
			buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];

			// unlikely
			if (idx + 3 > size) {
				size += 16;
				dec = (unsigned char *) realloc(dec, size + 1);
			}
			if (dec != NULL){
				for (i = 0; i < 3; ++i) {
					dec[idx++] = buf[i];
				}
			} else {
				return NULL;
			}

			// reset
			i = 0;
		}
	}

	// remainder
	if (i > 0) {
		// fill `tmp' with `\0' at most 4 times
		for (j = i; j < 4; ++j) {
			tmp[j] = '\0';
		}

		// translate remainder
		for (j = 0; j < 4; ++j) {
			// find translation char in `b64_table'
			for (l = 0; l < 64; ++l) {
				if (tmp[j] == b64_table[l]) {
					tmp[j] = l;
					break;
				}
			}
		}

		// decode remainder
		buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
		buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
		buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];

		// write remainer decoded buffer to `dec'
		if (idx + (i - 1) > size) {
			size += 16;
			dec = (unsigned char *) realloc(dec, size + 1);
		}
		if (dec != NULL){
			for (j = 0; (j < i - 1); ++j) {
				dec[idx++] = buf[j];
			}
		} else {
			return NULL;
		}
	}

	dec[idx] = '\0';
	// Return back the size of decoded string if demanded.
	if (decsize != NULL) {
		*decsize = size;
	}

	return dec;
}
