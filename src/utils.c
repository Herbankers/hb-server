/*
 *
 * kech-server
 * utils.c
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <mysql/mysql.h>

#include "kech.h"

/* TODO More checks */
bool isiban(const char *str)
{
	while (*str)
		if (!isalnum(*str++))
			return 0;

	return 1;
}

int ownsaccount(MYSQL *sql, struct token *tok, const char *iban)
{
	char *_q, *q = NULL;
	MYSQL_RES *res = NULL;
	int n;

	/* Prepare the query */
	_q = "SELECT 1 FROM `accounts` WHERE `user_id` = %u AND "
			"`iban` = '%s'";
	if (!(q = malloc(snprintf(NULL, 0, _q, tok->user_id, iban) + 1)))
		goto err;
	sprintf(q, _q, tok->user_id, iban);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;
	if (!(res = mysql_store_result(sql)))
		goto err;

	/* Check if there's any rows */
	n = mysql_num_rows(res);

	free(q);
	mysql_free_result(res);
	return n;

err:
	free(q);
	mysql_free_result(res);
	return -1;
}
