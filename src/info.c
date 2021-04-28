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

bool info(struct connection *conn, const char *data, uint16_t len, struct hbp_header *reply, msgpack_packer *pack)
{
	MYSQL_RES *sqlres = NULL;
	MYSQL_ROW row;

	/* retrieve the user's first and last name from the database */
	sqlres = query(conn, "SELECT `first_name`, `last_name` FROM `users` WHERE `user_id` = '%u'", conn->user_id);
	if (!(row = mysql_fetch_row(sqlres))) {
		dprintf("invalid user ID: %s\n", conn->user_id);
		goto err;
	}

	/* @param type */
	reply->type = HBP_REP_INFO;

	msgpack_pack_array(pack, 2);

	/* @param first_name */
	msgpack_pack_str(pack, strlen(row[0]));
	msgpack_pack_str_body(pack, row[0], strlen(row[0]));

	/* @param last_name */
	msgpack_pack_str(pack, strlen(row[1]));
	msgpack_pack_str_body(pack, row[1], strlen(row[1]));

	mysql_free_result(sqlres);

	return true;

err:
	mysql_free_result(sqlres);

	return false;
}
