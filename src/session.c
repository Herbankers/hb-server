/*
 *
 * hb-server
 *
 * Copyright (C) 2018 - 2021 Bastiaan Teeuwen <bastiaan@mkcl.nl>
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
#include <netdb.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#if SSLSOCK
#  include <openssl/err.h>
#  include <openssl/ssl.h>

#  define READ(b, n)	SSL_read(ssl, (b), (n))
#  define WRITE(b, n)	SSL_write(ssl, (b), (n))
#else
#  include <errno.h>

#  define READ(b, n)	read(sock, (b), (n))
#  define WRITE(b, n)	write(sock, (b), (n))
#endif

#include "hbp.h"
#include "herbank.h"

static int process(MYSQL *sql, char **buf, char *host, struct kbp_request *req,
		struct token *tok, struct kbp_reply *rep)
{
	struct kbp_request_transfer t;
	int res;

	/* Check if session hasn't timed out */
	if (tok->valid) {
		if (time(NULL) > tok->expiry_time) {
			lprintf("%s: session timeout: %u,%u\n", host,
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
			if ((kbp_login_res) **buf == KBP_L_GRANTED)
				lprintf("%s: session login: %u,%u\n", host,
						tok->user_id, tok->card_id);

			rep->status = KBP_S_OK;
		}
		break;
	case KBP_T_LOGOUT:
		if (tok->valid)
			lprintf("%s: session logout: %u,%u\n", host,
					tok->user_id, tok->card_id);

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
					host, t.iban_src, t.iban_dest,
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

void *session(void *args)
{
	struct addrinfo *addr = NULL, hints;
	struct kbp_request req;
	struct kbp_reply rep;
	struct token tok;
	char *buf, host[INET6_ADDRSTRLEN];
	int res, errcnt = 0, sock;
	MYSQL *sql = NULL;
#if SSLSOCK
	SSL *ssl = NULL;
#endif

	sock = *((int *) args);

	rep.magic = KBP_MAGIC;
	rep.version = KBP_VERSION;
	memset(&tok, 0, sizeof(tok));

	/* Get client IP */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Resolve the host */
	if (getaddrinfo(NULL, port, &hints, &addr) != 0) {
		lprintf("unable to resolve host %s\n", host);
		goto ret;
	}

	if (addr->ai_addr->sa_family == AF_INET)
		inet_ntop(AF_INET,
				&((struct sockaddr_in *) &addr->ai_addr)->sin_addr.s_addr,
				host, INET_ADDRSTRLEN);
	else if (addr->ai_addr->sa_family == AF_INET6)
		inet_ntop(AF_INET6,
				&((struct sockaddr_in6 *) &addr->ai_addr)->sin6_addr.s6_addr,
				host, INET6_ADDRSTRLEN);
	lprintf("%s: new connection\n", host);

#if SSLSOCK
	/* Setup an SSL/TLS connection */
	if (!(ssl = SSL_new(ctx))) {
		lprintf("unable to allocate SSL structure\n");
		goto ret;
	}
	SSL_set_fd(ssl, sock);

	if (SSL_accept(ssl) <= 0) {
		lprintf("%s: SSL error\n", host);
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
#endif

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
					"exceeded\n", host, KBP_ERROR_MAX);
			break;
		}

		/* Wait for requests from the client */
		if ((res = READ(&req, sizeof(req))) <= 0) {
#if SSLSOCK
			if (SSL_get_error(ssl, res) == SSL_ERROR_ZERO_RETURN)
				break;
#else
			/* Is this right? */
			if (!res)
				break;
#endif
			lprintf("%s: header read error\n", host);
			errcnt++;
			continue;
		}
		if (req.magic != KBP_MAGIC || req.length > KBP_LENGTH_MAX) {
			lprintf("%s: invalid request\n", host);
			errcnt++;
			continue;
		}
		if (req.version != KBP_VERSION) {
			lprintf("%s: KBP version mismatch (got %u want %u)\n",
					host, req.version, KBP_VERSION);
			break;
		}

		lprintf("%s: new request %u\n", host, req.type);

		/* Read request data if available */
		if (req.length) {
			if (!(buf = malloc(req.length))) {
				lprintf("out of memory\n");
				break;
			}

			if (READ(buf, req.length) <= 0) {
				lprintf("%s: read error\n", host);
				errcnt++;
				goto next;
			}
		} else {
			buf = NULL;
		}

		/* Process the request */
		if (process(sql, &buf, host, &req, &tok, &rep) < 0) {
			lprintf("%s: error processing request\n", host);
			errcnt++;
			goto next;
		}

		/* Send the header */
		if (WRITE(&rep, sizeof(struct kbp_reply)) <= 0) {
			lprintf("%s: header write error\n", host);
			errcnt++;
			goto next;
		}

		/* Finally, send back the data */
		if (rep.length && WRITE(buf, rep.length) <= 0) {
			lprintf("%s: write error: %d\n", host, rep.length);
			errcnt++;
			goto next;
		}

		lprintf("%s: request %u has been processed: %d\n", host,
				req.type, rep.status);

next:
		free(buf);
	}

ret:
	lprintf("%s: terminating connection\n", host);

	/* Close the database connection */
	mysql_close(sql);
	mysql_thread_end();

	/* Close the client connection */
	close(sock);
#if SSLSOCK
	SSL_free(ssl);
#endif

	pthread_exit(NULL);
}
