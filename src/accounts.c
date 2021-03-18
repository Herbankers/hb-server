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

#include <mysql.h>

#include "hbp.h"
#include "herbank.h"

int accounts_get(MYSQL *sql, struct token *tok, char **buf)
{
	struct kbp_reply_account *a;
	char *_q, *q = NULL;
	MYSQL_RES *res = NULL;
	MYSQL_ROW row;
	int n = 0;

	free(*buf);
	*buf = NULL;

	/* Prepare the query */
	_q = "SELECT `iban`, `type`, `balance` FROM `accounts` "
		"WHERE `user_id` = %u ORDER BY `type`";
	if (!(q = malloc(snprintf(NULL, 0, _q, tok->user_id) + 1)))
		goto err;
	sprintf(q, _q, tok->user_id);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;
	if (!(res = mysql_store_result(sql)))
		goto err;

	/* Allocate and fill the array */
	n = mysql_num_rows(res);
	if (!(a = malloc(n * sizeof(struct kbp_reply_account))))
		goto err;
	*buf = (char *) a;

	while ((row = mysql_fetch_row(res))) {
		strncpy((*a).iban, row[0], KBP_IBAN_MAX);
		(*a).type = strtol(row[1], NULL, 10);
		(*a).balance = strtoll(row[2], NULL, 10);
		a++;
	}

	free(q);
	mysql_free_result(res);
	return n * sizeof(struct kbp_reply_account);

err:
	free(q);
	mysql_free_result(res);
	return -1;
}
