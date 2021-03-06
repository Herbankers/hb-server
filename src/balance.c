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

static bool local_balance(struct connection *conn, msgpack_packer *pack)
{
	MYSQL_RES *sqlres = NULL;
	MYSQL_ROW row;
	char *balance_str;

	/* retrieve this account's balance from the database */
	sqlres = query(conn, "SELECT `balance` FROM `accounts` WHERE `iban` = '%s'", conn->iban);
	if (!(row = mysql_fetch_row(sqlres))) {
		dprintf("invalid IBAN: %s\n", conn->iban);
		goto err;
	}

	/* create a new string and add the decimal point (hence +2 including null terminator) */
	if (!(balance_str = malloc(strlen(row[0]) + 4)))
		goto err;

	if (strcmp(row[0], "0") == 0) {
		strcpy(balance_str, "0.00");
	} else {
		memcpy(balance_str, row[0], strlen(row[0]) - 2);
		balance_str[strlen(row[0]) - 2] = '.';
		memcpy(balance_str + strlen(row[0]) - 1, row[0] + strlen(row[0]) - 2, 3);
	}

	/* @param balance */
	msgpack_pack_str(pack, strlen(balance_str));
	msgpack_pack_str_body(pack, balance_str, strlen(balance_str));

	free(balance_str);
	mysql_free_result(sqlres);

	return true;

err:
	mysql_free_result(sqlres);

	return false;
}

static bool noob_balance(struct connection *conn, msgpack_packer *pack)
{
	char balance_str[BUF_SIZE + 1];
	long status;

	status = noob_request(balance_str, "balance", conn->iban, conn->pin, NULL);

	/* check if the length of the balance string is somewhat sensible */
	if (strlen(balance_str) > 16)
		return false;

	/* this should always be true */
	if (status != 209)
		return false;

	/* @param balance */
	msgpack_pack_str(pack, strlen(balance_str));
	msgpack_pack_str_body(pack, balance_str, strlen(balance_str));

	return true;
}

bool balance(struct connection *conn, const char *data, uint16_t len, struct hbp_header *reply, msgpack_packer *pack)
{
	/* @param type */
	reply->type = HBP_REP_BALANCE;

	if (!conn->foreign)
		return local_balance(conn, pack);
	else
		return noob_balance(conn, pack);

	return conn->foreign ? noob_balance(conn, pack) : local_balance(conn, pack);
}
