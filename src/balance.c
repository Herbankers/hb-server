/*
 *
 * hb-server
 *
 * Copyright (C) 2021 Bastiaan Teeuwen <bastiaan@mkcl.nl>
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

#include <string.h>

#include "hbp.h"
#include "herbank.h"

bool balance(struct connection *conn, const char *data, uint16_t len, struct hbp_header *reply, msgpack_packer *pack)
{
	MYSQL_ROW row;
	MYSQL_RES *sqlres = NULL;
	char *balance_str;

	/* retrieve the user's first and last name from the database */
	sqlres = query(conn, "SELECT `balance` FROM `accounts` WHERE `iban` = '%s'", conn->iban);
	if (!(row = mysql_fetch_row(sqlres))) {
		dprintf("invalid IBAN: %s\n", conn->iban);
		goto err;
	}

	/* @param type */
	reply->type = HBP_REP_BALANCE;

	/* create a new string and add the decimal point (hence +2 including null terminator) */
	if (!(balance_str = malloc(strlen(row[0]) + 2)))
		goto err;
	memcpy(balance_str, row[0], strlen(row[0]) - 2);
	balance_str[strlen(row[0]) - 2] = '.';
	memcpy(balance_str + strlen(row[0]) - 1, row[0] + strlen(row[0]) - 2, 3);

	/* Balance */
	msgpack_pack_str(pack, strlen(balance_str));
	msgpack_pack_str_body(pack, balance_str, strlen(balance_str));

	free(balance_str);
	mysql_free_result(sqlres);

	return true;

err:
	mysql_free_result(sqlres);

	return false;
}
