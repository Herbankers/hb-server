/*
 *
 * kech-server
 * session.c
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

#include <arpa/inet.h>
#include <sys/socket.h>

#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libscrypt.h>

#include <mysql/mysql.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "kbp.h"
#include "kech.h"

struct token {
	bool		valid;
	uint32_t	user_id;
	uint32_t	card_id;
	time_t		expiry_time;
};

/* TODO More checks */
static bool isiban(const char *str)
{
	while (*str)
		if (!isalnum(*str++))
			return 0;

	return 1;
}

static int attempts_update(MYSQL *sql, uint32_t user_id, uint32_t card_id,
		bool success)
{
	char *_q, *q = NULL;

	/* Prepare the query to update/reset the number of attempts made */
	if (success)
		_q = "UPDATE `cards` SET `attempts` = 0 WHERE "
				"`user_id` = %u AND `card_id` = %u";
	else
		_q = "UPDATE `cards` SET `attempts` = `attempts` + 1 WHERE "
				"`user_id` = %u AND `card_id` = %u";
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

static int accounts_get(MYSQL *sql, struct token *tok, char **buf)
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

static int pin_update(MYSQL *sql, struct token *tok, char **buf)
{
	char pin[KBP_PIN_MAX + 1];
	char mcf[SCRYPT_MCF_LEN + 1];
	char *_q, *q = NULL;

	strncpy(pin, *buf, KBP_PIN_MAX + 1);
	free(*buf);
	*buf = NULL;

	if (strlen(pin) < KBP_PIN_MIN)
		goto err;

	if (!libscrypt_hash(mcf, pin, SCRYPT_N, SCRYPT_r, SCRYPT_p))
		goto err;

	/* Prepare the query */
	_q = "UPDATE `cards` SET `pin` = '%s' WHERE "
			"`user_id` = %u AND `card_id` = %u";
	if (!(q = malloc(snprintf(NULL, 0, _q, mcf,
			tok->user_id, tok->card_id) + 1)))
		goto err;
	sprintf(q, _q, mcf, tok->user_id, tok->card_id);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;

	free(q);
	return 0;

err:
	free(q);
	return -1;
}

/* This entire function is a fucking mess */
static int login(MYSQL *sql, struct token *tok, char **buf)
{
	struct kbp_request_login l;
	char *_q, *q = NULL;
	MYSQL_RES *res = NULL;
	MYSQL_ROW row;
	int _res = KBP_L_GRANTED;
	char *r;

	memcpy(&l, *buf, sizeof(l));
	free(*buf);
	*buf = NULL;

	/* Prepare the query */
	_q = "SELECT `pin`, `attempts` FROM `cards` WHERE `user_id` = %u AND "
			"`card_id` = %u";
	if (!(q = malloc(snprintf(NULL, 0, _q, l.user_id, l.card_id) + 1)))
		goto err;
	sprintf(q, _q, l.user_id, l.card_id);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;
	if (!(res = mysql_store_result(sql)))
		goto err;

	/* Check if entry exists */
	if (!(row = mysql_fetch_row(res)))
		goto err;

	/* Check if card is blocked */
	if (strtol(row[1], NULL, 10) >= KBP_PINTRY_MAX) {
		_res = KBP_L_BLOCKED;
		goto ret;
	}

	/* Check if the entered password is correct */
	if (libscrypt_check(row[0], l.pin) <= 0) {
		if (!attempts_update(sql, l.user_id, l.card_id, 0))
			goto err;
		_res = KBP_L_DENIED;
		goto ret;
	}

	/* TODO Set blocked flag */
	if (!attempts_update(sql, l.user_id, l.card_id, 1))
		goto err;

	tok->valid = 1;
	tok->user_id = l.user_id;
	tok->card_id = l.card_id;
	tok->expiry_time = time(NULL) + KBP_TIMEOUT * 60;

	goto ret;

err:
	_res = -1;

ret:
	free(q);
	mysql_free_result(res);

	if (_res >= 0) {
		if (!(r = malloc(sizeof(kbp_reply_login))))
			return -1;
		*buf = (char *) r;
		*r = _res;
	}

	return _res;
}

static int transactions_get(char **buf)
{
	char iban[KBP_IBAN_MAX + 1];

	memcpy(&iban, *buf, KBP_IBAN_MAX + 1);
	free(*buf);
	*buf = NULL;

	/* TODO */

	return -1;
}

static int ownsaccount(MYSQL *sql, struct token *tok, const char *iban)
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
static int transfer(MYSQL *sql, struct token *tok, char **buf)
{
	struct kbp_request_transfer t;

	memcpy(&t, *buf, sizeof(t));
	free(*buf);
	*buf = NULL;

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
		if (modify(sql, t.iban_in, t.amount) < 0)
			if (*t.iban_in && modify(sql, t.iban_in, t.amount) < 0)
				return -1;

	return 0;
}

static int process(MYSQL *sql, char **buf, char *addr, struct kbp_request *req,
		struct token *tok, struct kbp_reply *rep)
{
	struct kbp_request_transfer t;
	int res;

	/* Check if session hasn't timed out */
	if (tok->valid) {
		if (time(NULL) > tok->expiry_time) {
			lprintf("%s: session timeout: %u,%u\n", addr,
					tok->user_id, tok->card_id);

			tok->valid = 0;
			rep->status = KBP_S_TIMEOUT;
			rep->length = 0;
			return 0;
		} else {
			rep->status = KBP_S_INVALID;
		}
	} else if (req->type != KBP_T_LOGIN) {
		rep->status = KBP_S_TIMEOUT;
		rep->length = 0;
		return 0;
	}

	/* Process the request */
	switch (req->type) {
	case KBP_T_ACCOUNTS:
		if ((res = accounts_get(sql, tok, buf)) < 0)
			rep->status = KBP_S_FAIL;
		else
			rep->status = KBP_S_OK;
		break;
	case KBP_T_PIN_UPDATE:
		if (req->length != KBP_PIN_MAX + 1) {
			rep->status = KBP_S_INVALID;
			break;
		}

		if ((res = pin_update(sql, tok, buf)) < 0)
			rep->status = KBP_S_FAIL;
		else
			rep->status = KBP_S_OK;
		break;
	case KBP_T_LOGIN:
		if (req->length != sizeof(struct kbp_request_login)) {
			rep->status = KBP_S_INVALID;
			break;
		}

		if ((res = login(sql, tok, buf)) < 0) {
			rep->status = KBP_S_FAIL;
		} else {
			if ((kbp_reply_login) **buf == KBP_L_GRANTED)
				lprintf("%s: session login: %u,%u\n", addr,
						tok->user_id, tok->card_id);

			rep->status = KBP_S_OK;
			rep->length = sizeof(kbp_reply_login);
			return 1;
		}
		break;
	case KBP_T_LOGOUT:
		lprintf("%s: session logout: %u,%u\n", addr, tok->user_id,
				tok->card_id);

		tok->valid = 0;
		rep->status = KBP_S_TIMEOUT;
		res = 0;
		break;
	case KBP_T_TRANSACTIONS:
		if (req->length != KBP_IBAN_MAX + 1) {
			rep->status = KBP_S_INVALID;
			break;
		}

		if ((res = transactions_get(buf)) < 0)
			rep->status = KBP_S_FAIL;
		else
			rep->status = KBP_S_OK;
		break;
	case KBP_T_TRANSFER:
		if (req->length != sizeof(struct kbp_request_transfer)) {
			rep->status = KBP_S_INVALID;
			break;
		}
		memcpy(&t, *buf, req->length);

		if ((res = transfer(sql, tok, buf)) < 0) {
			rep->status = KBP_S_FAIL;
		} else {
			lprintf("%s: transfer from '%s' to '%s': EUR %.2f\n",
					addr, t.iban_in, t.iban_out,
					(double) t.amount / 100);

			rep->status = KBP_S_OK;
		}
		break;
	/* Invalid or unimplemented request */
	default:
		res = 0;
		break;
	}

	rep->length = (res < 0) ? 0 : res;

	return 1;
}

void *session(void *_conn)
{
	struct connection *conn = _conn;
	struct kbp_request req;
	struct kbp_reply rep;
	struct token tok;
	char *buf, addr[INET_ADDRSTRLEN];
	int res, errcnt = 0;
	MYSQL *sql = NULL;
	SSL *ssl = NULL;

	rep.magic = KBP_MAGIC;
	memset(&tok, 0, sizeof(tok));

	/* Get client IP */
	inet_ntop(AF_INET, &conn->addr.sin_addr, addr,
			INET_ADDRSTRLEN);
	printf("%s: new connection\n", addr);
	fflush(stdout);

	/* Setup an SSL/TLS connection */
	if (!(ssl = SSL_new(ctx))) {
		fprintf(stderr, "unable to allocate SSL structure\n");
		goto ret;
	}
	SSL_set_fd(ssl, conn->sock);

	if (SSL_accept(ssl) <= 0) {
		fprintf(stderr, "%s: SSL error\n", addr);
		goto ret;
	}

	/* Get and verify the client certificate */
	if (!SSL_get_peer_certificate(ssl)) {
		fprintf(stderr, "client failed to present certificate\n");
		goto ret;
	}
	if (SSL_get_verify_result(ssl) != X509_V_OK) {
		fprintf(stderr, "certificate verfication failed\n");
		goto ret;
	}

	/* Connect to the database */
	if (!(sql = mysql_init(NULL))) {
		fprintf(stderr, "mysql internal error\n");
		goto ret;
	}

	if (!mysql_real_connect(sql, sql_host, sql_user, sql_pass, sql_db,
			sql_port, NULL, 0)) {
		fprintf(stderr, "database connection error\n");
		goto ret;
	}

	/* FIXME Do this more dynamically */
	for (;;) {
		/* Check if maximum error count hasn't been exceeded. */
		if (errcnt > KBP_ERROR_MAX) {
			fprintf(stderr, "%s: maximum error count (%d) has been "
					"exceeded\n", addr, KBP_ERROR_MAX);
			break;
		}

		/* Wait for requests from the client */
		if ((res = SSL_read(ssl, &req, sizeof(req))) <= 0) {
			if (SSL_get_error(ssl, res) == SSL_ERROR_ZERO_RETURN)
				break;
			fprintf(stderr, "%s: header read error\n", addr);
			errcnt++;
			continue;
		}
		if (req.magic != KBP_MAGIC || req.length > KBP_LENGTH_MAX) {
			fprintf(stderr, "%#x\n", req.type);
			fprintf(stderr, "%s: invalid request\n", addr);
			errcnt++;
			continue;
		}

		lprintf("%s: new request %u\n", addr, req.type);

		/* Read request data if available */
		if (req.length) {
			if (!(buf = malloc(req.length))) {
				fprintf(stderr, "out of memory\n");
				break;
			}

			if (SSL_read(ssl, buf, req.length) <= 0) {
				fprintf(stderr, "%s: read error\n", addr);
				errcnt++;
				goto next;
			}
		} else {
			buf = NULL;
		}

		/* Process the request */
		if (process(sql, &buf, addr, &req, &tok, &rep) < 0) {
			fprintf(stderr, "%s: error processing request\n", addr);
			errcnt++;
			goto next;
		}

		/* Send the header */
		if (SSL_write(ssl, &rep, sizeof(struct kbp_reply)) <= 0) {
			fprintf(stderr, "%s: header write error\n", addr);
			errcnt++;
			goto next;
		}

		/* Finally, send back the data */
		if (rep.length && SSL_write(ssl, buf, rep.length) <= 0) {
			fprintf(stderr, "%s: write error: %d\n", addr,
					rep.length);
			errcnt++;
			goto next;
		}

		lprintf("%s: request %u has been processed: %d\n", addr,
				req.type, rep.status);

next:
		free(buf);
	}

ret:
	printf("%s: terminating connection\n", addr);
	fflush(stdout);

	/* Close the database connection */
	mysql_close(sql);
	mysql_thread_end();

	/* Close the client connection */
	close(conn->sock);
	SSL_free(ssl);
	free(_conn);

	pthread_exit(NULL);
}
