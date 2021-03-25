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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <argon2.h>

#include "hbp.h"
#include "herbank.h"

bool login(struct connection *conn, const char *data, uint16_t len, struct hbp_header *reply, msgpack_packer *pack)
{
	/* FIXME static buffer per thread (in struct connection) */
	msgpack_unpacker unpack;
	msgpack_unpacked unpacked;
	msgpack_object *array;
	char card_id[HBP_CID_MAX + 1], pin[HBP_PIN_MAX + 1];
	MYSQL_ROW row;
	MYSQL_RES *sqlres = NULL;

	/* load the data into the msgpack buffer */
	if (!msgpack_unpacker_init(&unpack, len))
		return false;
	memcpy(msgpack_unpacker_buffer(&unpack), data, len);
	msgpack_unpacker_buffer_consumed(&unpack, len);

	/* unpack the array */
	msgpack_unpacked_init(&unpacked);
	if (msgpack_unpacker_next(&unpack, &unpacked) != MSGPACK_UNPACK_SUCCESS)
		goto err;
	if (unpacked.data.type != MSGPACK_OBJECT_ARRAY || unpacked.data.via.array.size != HBP_REQ_LOGIN_PARAMS)
		goto err;
	array = unpacked.data.via.array.ptr;

	/* copy the Card ID and PIN-code from the array to memory */
	if (array[0].via.str.size > HBP_CID_MAX || array[1].via.str.size > HBP_PIN_MAX)
		goto err;
	memcpy(card_id, array[0].via.str.ptr, array[0].via.str.size);
	card_id[array[0].via.str.size] = '\0';
	memcpy(pin, array[1].via.str.ptr, array[1].via.str.size);
	pin[array[1].via.str.size] = '\0';

	/* check if the card ID from the request is in the database */
	sqlres = query(conn, "SELECT `user_id`, `pin`, `attempts` FROM `cards` WHERE `card_id` = '%s'", card_id);
	if (!(row = mysql_fetch_row(sqlres))) {
		lprintf("%s: invalid card ID\n", conn->host);
		goto err;
	}

	msgpack_unpacked_destroy(&unpacked);
	msgpack_unpacker_destroy(&unpack);

	/* @param type */
	reply->type = HBP_REP_LOGIN;

	/* check if this card is blocked */
	if (strtol(row[2], NULL, 10) >= HBP_PINTRY_MAX) {
		/* @param status */
		msgpack_pack_int(pack, HBP_LOGIN_BLOCKED);
	} else {
		/* check if the supplied password is correct */
		if (argon2id_verify(row[1], pin, strlen(pin)) == ARGON2_OK) {
			/* right password, reset the login attempts counter */
			query(conn, "UPDATE `cards` SET `attempts` = 0 WHERE `card_id` = '%s'", card_id);

			/* and start a new session */
			conn->logged_in = true;
			conn->expiry_time = time(NULL) + HBP_TIMEOUT * 60;
			conn->user_id = strtol(row[0], NULL, 10);
			conn->card_id = strtol(row[1], NULL, 10);

			/* @param status */
			msgpack_pack_int(pack, HBP_LOGIN_GRANTED);
		} else {
			/* wrong password, increment the failed login attempts counter */
			query(conn, "UPDATE `cards` SET `attempts` = `attempts` + 1 WHERE `card_id` = '%s'", card_id);

			/* @param status */
			msgpack_pack_int(pack, HBP_LOGIN_DENIED);
		}
	}

	mysql_free_result(sqlres);

	return true;

err:
	mysql_free_result(sqlres);
	msgpack_unpacked_destroy(&unpacked);
	msgpack_unpacker_destroy(&unpack);

	return false;
}
