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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <argon2.h>

#include "hbp.h"
#include "herbank.h"

static int attempts_update(MYSQL *sql, uint32_t user_id, uint32_t card_id, bool success)
{
	char *_q, *q = NULL;

	/* Prepare the query to update/reset the number of attempts made */
	if (success)
		_q = "UPDATE `cards` SET `attempts` = 0 WHERE `user_id` = %u AND `card_id` = %u";
	else
		_q = "UPDATE `cards` SET `attempts` = `attempts` + 1 WHERE `user_id` = %u AND `card_id` = %u";
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
	uint32_t user_id, card_id;
	uint8_t *lres = NULL;
	char *_q, *q = NULL;
	MYSQL_RES *res = NULL;
	MYSQL_ROW row;

	memcpy(&l, *buf, sizeof(l));
	free(*buf);
	*buf = NULL;

	/* extract card information from the database into memory */
	_q = "SELECT `user_id`, `card_id`, `pin`, `attempts` FROM `cards` "
			"WHERE SUBSTRING(HEX(`id`), 1, 12) = '%02x%02x%02x%02x%02x%02x'";
	if (!(q = malloc(snprintf(NULL, 0, _q, l.uid[5], l.uid[4], l.uid[3], l.uid[2], l.uid[1], l.uid[0]) + 1)))
		goto err;
	sprintf(q, _q, l.uid[5], l.uid[4], l.uid[3], l.uid[2], l.uid[1], l.uid[0]);

	if (mysql_query(sql, q))
		goto err;
	if (!(res = mysql_store_result(sql)))
		goto err;

	/* check if entry exists */
	if (!(row = mysql_fetch_row(res)))
		goto err;

	/* allocate memory for the reply structure */
	if (!(lres = malloc(1)))
		goto err;
	*buf = (char *) lres;

	/* check if the card is blocked */
	if (strtol(row[3], NULL, 10) >= KBP_PINTRY_MAX) {
		*lres = KBP_L_BLOCKED;
	} else {
		user_id = strtol(row[0], NULL, 10);
		card_id = strtol(row[1], NULL, 10);

		/* check if the entered PIN is correct */
		if (argon2i_verify(row[2], l.pin, strlen(KBP_PIN_MAX)) == ARGON2_OK) {
			/* reset the blocked flag */
			if (!attempts_update(sql, user_id, card_id, 1))
				goto err;

			tok->valid = 1;
			tok->user_id = user_id;
			tok->card_id = card_id;
			tok->expiry_time = time(NULL) + KBP_TIMEOUT * 60;

			*lres = KBP_L_GRANTED;
		} else {
			/* login failed, increment failed login attempts */
			if (!attempts_update(sql, user_id, card_id, 0))
				goto err;
			*lres = KBP_L_DENIED;
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
