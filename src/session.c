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
	rep.version = KBP_VERSION;
	memset(&tok, 0, sizeof(tok));

	/* Get client IP */
	inet_ntop(AF_INET, &conn->addr.sin_addr, addr,
			INET_ADDRSTRLEN);
	printf("%s: new connection\n", addr);
	fflush(stdout);

	/* Setup an SSL/TLS connection */
	if (!(ssl = SSL_new(ctx))) {
		lprintf("unable to allocate SSL structure\n");
		goto ret;
	}
	SSL_set_fd(ssl, conn->sock);

	if (SSL_accept(ssl) <= 0) {
		lprintf("%s: SSL error\n", addr);
		goto ret;
	}

	/* Get and verify the client certificate */
	if (!SSL_get_peer_certificate(ssl)) {
		lprintf("client failed to present certificate\n");
		goto ret;
	}
	if (SSL_get_verify_result(ssl) != X509_V_OK) {
		lprintf("certificate verfication failed\n");
		goto ret;
	}

	/* Connect to the database */
	if (!(sql = mysql_init(NULL))) {
		lprintf("mysql internal error\n");
		goto ret;
	}

	if (!mysql_real_connect(sql, sql_host, sql_user, sql_pass, sql_db,
			sql_port, NULL, 0)) {
		lprintf("database connection error\n");
		goto ret;
	}

	/* FIXME Do this more dynamically */
	for (;;) {
		/* Check if maximum error count hasn't been exceeded. */
		if (errcnt > KBP_ERROR_MAX) {
			lprintf("%s: maximum error count (%d) has been "
					"exceeded\n", addr, KBP_ERROR_MAX);
			break;
		}

		/* Wait for requests from the client */
		if ((res = SSL_read(ssl, &req, sizeof(req))) <= 0) {
			if (SSL_get_error(ssl, res) == SSL_ERROR_ZERO_RETURN)
				break;
			lprintf("%s: header read error\n", addr);
			errcnt++;
			continue;
		}
		if (req.magic != KBP_MAGIC || req.length > KBP_LENGTH_MAX) {
			lprintf("%s: invalid request\n", addr);
			errcnt++;
			continue;
		}
		if (req.version != KBP_VERSION) {
			lprintf("%s: KBP version mismatch (got %u want %u)\n",
					addr, req.version, KBP_VERSION);
			break;
		}

		lprintf("%s: new request %u\n", addr, req.type);

		/* Read request data if available */
		if (req.length) {
			if (!(buf = malloc(req.length))) {
				lprintf("out of memory\n");
				break;
			}

			if (SSL_read(ssl, buf, req.length) <= 0) {
				lprintf("%s: read error\n", addr);
				errcnt++;
				goto next;
			}
		} else {
			buf = NULL;
		}

		/* Process the request */
		if (process(sql, &buf, addr, &req, &tok, &rep) < 0) {
			lprintf("%s: error processing request\n", addr);
			errcnt++;
			goto next;
		}

		/* Send the header */
		if (SSL_write(ssl, &rep, sizeof(struct kbp_reply)) <= 0) {
			lprintf("%s: header write error\n", addr);
			errcnt++;
			goto next;
		}

		/* Finally, send back the data */
		if (rep.length && SSL_write(ssl, buf, rep.length) <= 0) {
			lprintf("%s: write error: %d\n", addr, rep.length);
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
