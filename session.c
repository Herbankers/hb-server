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

#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mysql/mysql.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "kbp.h"
#include "kech.h"

struct token {
	bool		valid;
	unsigned int	customer_id;
	char		iban[KBP_IBAN_MAX + 1];
	time_t		expiry_time;
};

static int accounts_get(MYSQL *sql, struct token *tok, char **buf)
{
	struct kbp_reply_account *a;
	MYSQL_RES *res = NULL;
	MYSQL_ROW row;
	char *_q, *q = NULL;
	int n = 0;

	free(*buf);
	*buf = NULL;

	/* Prepare the query */
	_q = "SELECT `iban`, `type`, `balance` FROM `accounts` "
		"WHERE `customer_id` = %u";
	if (!(q = malloc(snprintf(NULL, 0, _q, tok->customer_id) + 1)))
		goto err;
	sprintf(q, _q, tok->customer_id);

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
		(*a).balance = (int64_t) (strtod(row[2], NULL) * 100);
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

static int pin_update(char **buf)
{
	char pin[KBP_PIN_MAX + 1];

	memcpy(&pin, *buf, KBP_PIN_MAX + 1);
	free(*buf);
	*buf = NULL;

	/* TODO */

	return -1;
}

static int login(char **buf, struct token *tok)
{
	struct kbp_request_login l;

	memcpy(&l, *buf, sizeof(l));
	free(*buf);
	*buf = NULL;

	/* TODO */
#if 1
	if (strcmp(l.pin, "1234") == 0) {
		tok->valid = 1;
		strncpy(tok->iban, l.iban, KBP_IBAN_MAX);
		tok->customer_id = 1;
		tok->expiry_time = time(NULL) + KBP_TIMEOUT * 60;
		return 0;
	}
#endif

	return -1;
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

static int transfer(char **buf)
{
	struct kbp_request_transfer t;

	memcpy(&t, *buf, sizeof(t));
	free(*buf);
	*buf = NULL;

	/* TODO */

	return -1;
}

static struct kbp_reply *process(struct kbp_request *req, struct token *tok,
		char **buf, char *addr, MYSQL *sql)
{
	struct kbp_reply *rep;
	int res;

	if (!(rep = malloc(sizeof(struct kbp_reply)))) {
		fprintf(stderr, "out of memory\n");
		return NULL;
	}

	rep->magic = KBP_MAGIC;

	/* Check if session hasn't timed out */
	if (tok->valid) {
		if (time(NULL) > tok->expiry_time) {
			if (verbose)
				printf("%s: session timeout: %s\n", addr,
						tok->iban);

			tok->valid = 0;
			rep->status = KBP_S_TIMEOUT;
			rep->length = 0;
			return rep;
		} else {
			rep->status = KBP_S_INVALID;
		}
	} else if (req->type != KBP_T_LOGIN) {
		rep->status = KBP_S_TIMEOUT;
		rep->length = 0;
		return rep;
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

		if ((res = pin_update(buf)) < 0)
			rep->status = KBP_S_FAIL;
		else
			rep->status = KBP_S_OK;
		break;
	case KBP_T_LOGIN:
		if (req->length != sizeof(struct kbp_request_login)) {
			rep->status = KBP_S_INVALID;
			break;
		}

		if ((res = login(buf, tok)) < 0) {
			rep->status = KBP_S_FAIL;
		} else {
			if (verbose)
				printf("%s: session login: %s\n", addr,
						tok->iban);
			rep->status = KBP_S_OK;
		}
		break;
	case KBP_T_LOGOUT:
		if (verbose)
			printf("%s: session logout: %s\n", addr, tok->iban);

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

		if ((res = transfer(buf)) < 0)
			rep->status = KBP_S_FAIL;
		else
			rep->status = KBP_S_OK;
		break;
	/* Invalid or unimplemented request */
	default:
		res = 0;
		break;
	}

	rep->length = (res < 0) ? 0 : res;

	return rep;
}

void *session(void *_conn)
{
	struct connection *conn = _conn;
	struct kbp_request req;
	struct kbp_reply *rep;
	struct token tok;
	char *buf, addr[INET_ADDRSTRLEN];
	int res, errcnt = 0;
	MYSQL *sql = NULL;
	SSL *ssl = NULL;

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

	/* TODO Verify certificate */

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
		rep = NULL;

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

		if (verbose)
			printf("%s: new request %u\n", addr, req.type);

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
		if (!(rep = process(&req, &tok, &buf, addr, sql))) {
			fprintf(stderr, "%s: error processing request\n", addr);
			errcnt++;
			goto next;
		}

		/* Send the header */
		if (SSL_write(ssl, rep, sizeof(struct kbp_reply)) <= 0) {
			fprintf(stderr, "%s: header write error\n", addr);
			errcnt++;
			goto next;
		}

		/* Finally, send back the data */
		if (rep->length && SSL_write(ssl, buf, rep->length) <= 0) {
			fprintf(stderr, "%s: write error: %d\n", addr,
					rep->length);
			errcnt++;
			goto next;
		}

		if (verbose)
			printf("%s: request %u has been processed\n", addr,
					req.type);

next:
		free(buf);
		free(rep);
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
