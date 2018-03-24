/*
 *
 * kech-server
 * login.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libscrypt.h>

#include <mysql/mysql.h>

#include "kbp.h"
#include "kech.h"

static int attempts_update(MYSQL *sql, uint32_t user_id, uint32_t card_id,
		bool success)
{
	char *_q, *q = NULL;

	/* Prepare the query to update/reset the number of attempts made */
	if (success)
		_q = "UPDATE `cards` SET `attempts` = 0 WHERE "
				"`user_id` = %u AND `card_id` = %u";
	else
		_q = "UPDATE `cards` SET `attempts` = `attempts` + 1 WHERE "
				"`user_id` = %u AND `card_id` = %u";
	if (!(q = malloc(snprintf(NULL, 0, _q, user_id, card_id) + 1)))
		goto err;
	sprintf(q, _q, user_id, card_id);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;

	free(q);
	return 1;

err:
	free(q);
	return 0;
}

int login(MYSQL *sql, struct token *tok, char **buf)
{
	struct kbp_request_login l;
	uint8_t *lres = NULL;
	char *_q, *q = NULL;
	MYSQL_RES *res = NULL;
	MYSQL_ROW row;
	char *r;

	memcpy(&l, *buf, sizeof(l));
	free(*buf);
	*buf = NULL;

	/* Prepare the query */
	_q = "SELECT `pin`, `attempts` FROM `cards` WHERE `user_id` = %u AND "
			"`card_id` = %u";
	if (!(q = malloc(snprintf(NULL, 0, _q, l.user_id, l.card_id) + 1)))
		goto err;
	sprintf(q, _q, l.user_id, l.card_id);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;
	if (!(res = mysql_store_result(sql)))
		goto err;

	/* Check if entry exists */
	if (!(row = mysql_fetch_row(res)))
		goto err;

	/* Allocate reply */
	if (!(lres = malloc(1)))
		goto err;
	*buf = (char *) lres;

	/* Check if card is blocked */
	if (strtol(row[1], NULL, 10) >= KBP_PINTRY_MAX) {
		*lres = KBP_L_BLOCKED;
	} else {
		/* Check if the entered password is correct */
		if (libscrypt_check(row[0], l.pin) <= 0) {
			if (!attempts_update(sql, l.user_id, l.card_id, 0))
				goto err;
			*lres = KBP_L_DENIED;
		} else {
			/* Reset the blocked flag */
			if (!attempts_update(sql, l.user_id, l.card_id, 1))
				goto err;

			tok->valid = 1;
			tok->user_id = l.user_id;
			tok->card_id = l.card_id;
			tok->expiry_time = time(NULL) + KBP_TIMEOUT * 60;

			*lres = KBP_L_GRANTED;
		}
	}

	free(q);
	mysql_free_result(res);
	return 1;

err:
	free(lres);
	free(q);
	mysql_free_result(res);
	return -1;
}
