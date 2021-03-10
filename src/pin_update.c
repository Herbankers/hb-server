/*
 *
 * kech-server
 * pin_update.c
 *
 * Copyright (C) 2018 Bastiaan Teeuwen <bastiaan@mkcl.nl>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libscrypt.h>

#include "kbp.h"
#include "kech.h"

int pin_update(MYSQL *sql, struct token *tok, char **buf)
{
	char pin[KBP_PIN_MAX + 1];
	char mcf[SCRYPT_MCF_LEN + 1];
	char *_q, *q = NULL;
	unsigned int i;

	strncpy(pin, *buf, KBP_PIN_MAX + 1);
	free(*buf);
	*buf = NULL;

	if (strlen(pin) < KBP_PIN_MIN)
		goto err;

	/* Check if PIN only contains numeric characters */
	for (i = 0; i < strlen(pin); i++)
		if (!isdigit(pin[i]))
			goto err;

	if (!libscrypt_hash(mcf, pin, SCRYPT_N, SCRYPT_r, SCRYPT_p))
		goto err;

	/* Prepare the query */
	_q = "UPDATE `cards` SET `pin` = '%s' WHERE "
			"`user_id` = %u AND `card_id` = %u";
	if (!(q = malloc(snprintf(NULL, 0, _q, mcf,
			tok->user_id, tok->card_id) + 1)))
		goto err;
	sprintf(q, _q, mcf, tok->user_id, tok->card_id);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;

	free(q);
	return 0;

err:
	free(q);
	return -1;
}
