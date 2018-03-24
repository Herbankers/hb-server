/*
 *
 * kech-server
 * transfer.c
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

#include <mysql/mysql.h>

#include "kbp.h"
#include "kech.h"

static int modify(MYSQL *sql, const char *iban, int64_t d)
{
	char *_q, *q = NULL;
	MYSQL_RES *res = NULL;
	MYSQL_ROW row;
	int64_t bal;

	/* First, calculate the new balance */
	_q = "SELECT `balance` FROM `accounts` WHERE `iban` = '%s'";
	if (!(q = malloc(snprintf(NULL, 0, _q, iban) + 1)))
		goto err;
	sprintf(q, _q, iban);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;
	if (!(res = mysql_store_result(sql)))
		goto err;

	/* Check if entry exists */
	if (!(row = mysql_fetch_row(res)))
		goto err;

	/* Check if the new balance is positive, abort otherwise */
	if ((bal = strtoll(row[0], NULL, 10) + d) < 0)
		goto err;

	/* Clean up */
	free(q);
	q = NULL;
	mysql_free_result(res);
	res = NULL;

	/* Prepare the update */
	_q = "UPDATE `accounts` SET `balance` = %lld WHERE `iban` = '%s'";
	if (!(q = malloc(snprintf(NULL, 0, _q, bal, iban) + 1)))
		goto err;
	sprintf(q, _q, bal, iban);

	/* Do it */
	if (mysql_query(sql, q))
		goto err;

	free(q);
	return bal;

err:
	free(q);
	mysql_free_result(res);
	return -1;
}

/* FIXME Not written very efficiently */
int transfer(MYSQL *sql, struct token *tok, char **buf)
{
	struct kbp_request_transfer t;

	memcpy(&t, *buf, sizeof(t));
	free(*buf);
	*buf = NULL;

	/* Check if amount is positive and therefore valid */
	if (t.amount < 0)
		return -1;

	/* Check if the IBAN(s) are valid and exist in the database */
	if (*t.iban_in) {
		if (!isiban(t.iban_in))
			return -1;
		if (modify(sql, t.iban_in, 0) < 0)
			return -1;
	}

	if (*t.iban_out) {
		if (!isiban(t.iban_out))
			return -1;
		if (modify(sql, t.iban_out, 0) < 0)
			return -1;
	}

	/* Check if one of the accounts is accessible with the active session */
	if (ownsaccount(sql, tok, (*t.iban_in) ? t.iban_in : t.iban_out) <= 0)
		return -1;

	/* Perform the transaction */
	if (*t.iban_in)
		if (modify(sql, t.iban_in, -t.amount) < 0)
			return -1;
	if (*t.iban_out)
		if (modify(sql, t.iban_out, t.amount) < 0)
			if (*t.iban_in && modify(sql, t.iban_in, t.amount) < 0)
				return -1;

	return 0;
}
