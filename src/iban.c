/*
 *
 * hb-server
 *
 * Copyright (C) 2018 - 2021 Bastiaan Teeuwen <bastiaan@mkcl.nl>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hbp.h"

static const uint8_t prefix_fmt = 0xCF /* 0b11001111 */;

/*
 * calculation used: https://www.ibancalculator.com/calculation.html
 */

/* UNUSED */

/* calculate check digits (conforming to ISO 7064:2003) */
int iban_getcheck(const char *_iban)
{
	char iban[HBP_IBAN_MAX], buf[5];
	unsigned int i;
	int n, check = 0;

	strncpy(iban, _iban, HBP_IBAN_MAX);

	/* move the first 4 characters to the end of the string */
	strncpy(buf, iban, 4);
	buf[4] = '\0';
	memmove(iban, iban + 4, strlen(iban + 4) + 1);
	strcat(iban, buf);

	/* perform MOD 97 on all characters */
	for (i = 0; i < strlen(iban); i++) {
		if (isdigit(iban[i])) {
			n = iban[i] - '0';
		} else if (isalpha(iban[i])) {
			if (isupper(iban[i]))
				n = iban[i] - 'A';
			else if (islower(iban[i]))
				n = iban[i] - 'a';

			check = (check * 10 + (n / 10 + 1)) % 97;
			n %= 10;
		} else {
			return 0;
		}

		check = (check * 10 + n) % 97;
	}

	/* return the checksum */
	return 98 - check;
}

bool iban_validate(const char *iban)
{
	unsigned int i;
	int l;

	/* check length IBAN */
	l = strlen(iban);
	if (l < HBP_IBAN_MIN || l > HBP_IBAN_MAX)
		return 0;

	/* check prefix format */
	for (i = 0; i < 8; i++) {
		if (prefix_fmt & (1 << (7 - i))) {
			if (!isalpha(iban[i]))
				return 0;
		} else {
			if (!isdigit(iban[i]))
				return 0;
		}
	}

	/* check BBAN format */
	for (i = 8; i < strlen(iban); i++)
		if (!isdigit(iban[i]))
			return 0;

	/* validate IBAN checksum */
	return iban_getcheck(iban);
}
