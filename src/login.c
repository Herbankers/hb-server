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

#include <stdlib.h>
#include <string.h>

#include <argon2.h>

#include "hbp.h"
#include "herbank.h"

bool login(struct connection *conn, const char *data, uint16_t len, struct hbp_header *reply, msgpack_packer *pack)
{
	msgpack_unpacker unpack;
	msgpack_unpacked unpacked;
	msgpack_object *array;
	char card_id[HBP_CID_MAX * 2 + 1], iban[HBP_IBAN_MAX + 1], pin[HBP_PIN_MAX + 1];
	MYSQL_ROW row;
	MYSQL_RES *sqlres = NULL;

	if (!msgpack_unpacker_init(&unpack, len))
		return false;

	/* adjust the buffer size if needed */
	if (msgpack_unpacker_buffer_capacity(&unpack) < len) {
		if (!msgpack_unpacker_reserve_buffer(&unpack, len)) {
			msgpack_unpacker_destroy(&unpack);
			return false;
		}
	}

	/* copy request data into the msgpack buffer */
	memcpy(msgpack_unpacker_buffer(&unpack), data, len);
	msgpack_unpacker_buffer_consumed(&unpack, len);

	/* unpack the request array */
	msgpack_unpacked_init(&unpacked);
	if (msgpack_unpacker_next(&unpack, &unpacked) != MSGPACK_UNPACK_SUCCESS)
		goto err;
	if (unpacked.data.type != MSGPACK_OBJECT_ARRAY || unpacked.data.via.array.size != HBP_REQ_LOGIN_LENGTH)
		goto err;
	array = unpacked.data.via.array.ptr;

	/* Card ID */
	if (array[HBP_REQ_LOGIN_CARD_ID].via.str.size > HBP_CID_MAX)
		goto err;
	memset(card_id, '0', HBP_CID_MAX * 2);
	memcpy(card_id, array[HBP_REQ_LOGIN_CARD_ID].via.str.ptr, array[HBP_REQ_LOGIN_CARD_ID].via.str.size);
	card_id[HBP_CID_MAX * 2] = '\0';

	/* IBAN */
	if (array[HBP_REQ_LOGIN_IBAN].via.str.size < HBP_IBAN_MIN || array[HBP_REQ_LOGIN_IBAN].via.str.size > HBP_IBAN_MAX)
		goto err;
	memcpy(iban, array[HBP_REQ_LOGIN_IBAN].via.str.ptr, array[1].via.str.size);
	iban[array[HBP_REQ_LOGIN_IBAN].via.str.size] = '\0';

	/* PIN */
	if (array[HBP_REQ_LOGIN_PIN].via.str.size > HBP_PIN_MAX)
		goto err;
	memcpy(pin, array[HBP_REQ_LOGIN_PIN].via.str.ptr, array[HBP_REQ_LOGIN_PIN].via.str.size);
	pin[array[HBP_REQ_LOGIN_PIN].via.str.size] = '\0';

	/*
	 * TODO Also check IBAN
	 *
	 * We don't need to do this right now, but this is for future proofing, when we'll be connecting to
	 * the other teams' servers.
	 */

	/* check if the card ID from the request is in the database */
	sqlres = query(conn, "SELECT `user_id`, `pin`, `attempts` FROM `cards` WHERE `card_id` = x'%s'", card_id);
	if (!(row = mysql_fetch_row(sqlres))) {
		dprintf("%s: invalid card ID\n", conn->card_id);
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
			query(conn, "UPDATE `cards` SET `attempts` = 0 WHERE `card_id` = x'%s'", card_id);

			/* and start a new session */
			conn->logged_in = true;
			conn->expiry_time = time(NULL) + HBP_TIMEOUT;
			strcpy(conn->iban, iban);
			conn->user_id = strtol(row[0], NULL, 10);
			conn->card_id = strtol(row[1], NULL, 10);

			/* @param status */
			msgpack_pack_int(pack, HBP_LOGIN_GRANTED);
		} else {
			/* wrong password, increment the failed login attempts counter */
			query(conn, "UPDATE `cards` SET `attempts` = `attempts` + 1 WHERE `card_id` = x'%s'", card_id);

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
