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

static bool local_transfer(struct connection *conn, msgpack_packer *pack, const char *iban, int64_t amount)
{
	MYSQL_RES *sqlres = NULL;
	MYSQL_ROW row;

	/*
	 * check if the account entry can be found by its IBAN in the database
	 *
	 * We shouldn't have to check the IBAN; this is already done when a new session is created.
	 * Extra checks never hurt though.
	 */
	sqlres = query(conn, "SELECT `balance` FROM `accounts` WHERE `iban` = '%s'", conn->iban);
	if (!(row = mysql_fetch_row(sqlres))) {
		dprintf("invalid IBAN: %s\n", conn->iban);
		goto err;
	}
	mysql_free_result(sqlres);

	/*
	 * check if the funds of the current account are sufficient
	 *
	 * TODO? allow accounts to go below 0
	 */
	sqlres = query(conn, "SELECT `balance` FROM `accounts` WHERE `iban` = '%s' AND `balance` >= '%lld'",
			conn->iban, amount);
	if (!(row = mysql_fetch_row(sqlres))) {
		/* @param status */
		msgpack_pack_int(pack, HBP_TRANSFER_INSUFFICIENT_FUNDS);
	} else {
		if (strlen(iban) == 0) {
			/* withdrawal */

			mysql_free_result(sqlres);
			sqlres = query(conn, "UPDATE `accounts` SET `balance` = `balance` - '%lld' WHERE `iban` = '%s'",
					amount, conn->iban);

			/* @param result */
			msgpack_pack_int(pack, HBP_TRANSFER_SUCCESS);
		} else if (strcmp(conn->iban, iban) == 0) {
			/* deposit: Not Yet Implemented */

			iprintf("NYI: deposit\n");
			goto err;
		} else {
			/* transfer: Not Yet Implemented */

			iprintf("NYI: transfer\n");
			goto err;
		}
	}

	mysql_free_result(sqlres);

	return true;

err:
	mysql_free_result(sqlres);

	return false;
}

static bool noob_transfer(struct connection *conn, msgpack_packer *pack, const char *iban, int64_t amount)
{
	char inbuf[BUF_SIZE + 1];
	char outbuf[BUF_SIZE + 1];
	long status;

	/* NOOB only supports withdrawals, so we need an empty iban */
	if (strlen(iban) != 0)
		return false;

	snprintf(inbuf, BUF_SIZE + 1, ", \"amount\": %lld", amount);

	/* add in a decimal point */
	memmove(inbuf + strlen(inbuf) - 1, inbuf + strlen(inbuf) - 2, 3);
	inbuf[strlen(inbuf) - 3] = '.';

	status = noob_request(outbuf, "withdraw", conn->iban, conn->pin, inbuf);

	if (status == 437 && strcmp(outbuf, "Balance too low") == 0)
		msgpack_pack_int(pack, HBP_TRANSFER_INSUFFICIENT_FUNDS);
	else if (status == 208)
		msgpack_pack_int(pack, HBP_TRANSFER_SUCCESS);
	else
		return false;

	return true;
}

bool transfer(struct connection *conn, const char *data, uint16_t len, struct hbp_header *reply, msgpack_packer *pack)
{
	char iban[HBP_IBAN_MAX + 1], *escaped;
	int64_t amount;
	msgpack_unpacker unpack;
	msgpack_unpacked unpacked;
	msgpack_object *array;
	bool res = false;

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
	if (unpacked.data.type != MSGPACK_OBJECT_ARRAY || unpacked.data.via.array.size != HBP_REQ_TRANSFER_LENGTH)
		goto err;
	array = unpacked.data.via.array.ptr;

	/* @param iban */
	if (array[HBP_REQ_TRANSFER_IBAN].via.str.size && (array[HBP_REQ_TRANSFER_IBAN].via.str.size < HBP_IBAN_MIN ||
				array[HBP_REQ_TRANSFER_IBAN].via.str.size > HBP_IBAN_MAX))
		goto err;
	memcpy(iban, array[HBP_REQ_TRANSFER_IBAN].via.str.ptr, array[HBP_REQ_TRANSFER_IBAN].via.str.size);
	iban[array[HBP_REQ_TRANSFER_IBAN].via.str.size] = '\0';

	/* escape the IBAN */
	if ((escaped = escape(conn, iban, HBP_IBAN_MAX))) {
		strcpy(iban, escaped);
		free(escaped);
	} else {
		dprintf("invalid IBAN: %s\n", iban);
		goto err;
	}

	/* @param amount */
	amount = array[HBP_REQ_TRANSFER_AMOUNT].via.i64;

	/* @param type */
	reply->type = HBP_REP_TRANSFER;

	if (!conn->foreign)
		res = local_transfer(conn, pack, iban, amount);
	else
		res = noob_transfer(conn, pack, iban, amount);

err:
	msgpack_unpacked_destroy(&unpacked);
	msgpack_unpacker_destroy(&unpack);

	return res;
}
