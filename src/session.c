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
#  define READ(b, n)	SSL_read(conn->ssl, (b), (n))
#  define WRITE(b, n)	SSL_write(conn->ssl, (b), (n))
#else
#  include <errno.h>
#  define READ(b, n)	read(conn->socket, (b), (n))
#  define WRITE(b, n)	write(conn->socket, (b), (n))
#endif

#include "hbp.h"
#include "herbank.h"

#if 0
static int process(MYSQL *sql, char **buf, char *host, struct kbp_request *req,
		struct token *tok, struct kbp_reply *rep)
{
	struct kbp_request_transfer t;
	int res;

	/* Check if session hasn't timed out */
	if (tok->valid) {
		if (time(NULL) > tok->expiry_time) {
			lprintf("%s: session timeout: %u,%u\n", host, tok->user_id, tok->card_id);

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
#endif

/* connect to the client and verify the client certificate */
static bool verify(struct connection *conn)
{
	struct addrinfo *addr = NULL;
	struct addrinfo hints;

	/* retrieve the client IP */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* resolve the host */
	if (getaddrinfo(NULL, port, &hints, &addr) != 0) {
		lprintf("unable to resolve host %s\n", conn->host);
		return false;
	}

	/* FIXME this doesn't look like it's resolving the IP properly */
	if (addr->ai_addr->sa_family == AF_INET)
		inet_ntop(AF_INET, &((struct sockaddr_in *) &addr->ai_addr)->sin_addr.s_addr,
				conn->host, INET_ADDRSTRLEN);
	else if (addr->ai_addr->sa_family == AF_INET6)
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *) &addr->ai_addr)->sin6_addr.s6_addr,
				conn->host, INET6_ADDRSTRLEN);

	freeaddrinfo(addr);

#if SSLSOCK
	/* setup an SSL/TLS connection */
	if (!(conn->ssl = SSL_new(ctx))) {
		lprintf("unable to allocate SSL structure\n");
		return false;
	}
	SSL_set_fd(conn->ssl, conn->socket);

	if (SSL_accept(conn->ssl) <= 0) {
		lprintf("%s: SSL error\n", conn->host);
		return false;
	}

	/* get and verify the client certificate */
	if (!SSL_get_peer_certificate(conn->ssl)) {
		lprintf("client failed to present certificate\n");
		return false;
	}
	if (SSL_get_verify_result(conn->ssl) != X509_V_OK) {
		lprintf("certificate verfication failed\n");
		return false;
	}
#endif

	lprintf("%s: Client connected\n", conn->host);

	return true;
}

/* send a reply to the client */
static bool sendreply(struct connection *conn, struct hbp_header *reply, const char *data)
{
	/* send the reply header */
	if (WRITE(reply, sizeof(struct hbp_header)) <= 0)
		return false;

	/* send the reply data */
	if (reply->length && WRITE(data, reply->length) <= 0)
		return false;

	return true;
}

/* wait for the client to send a request */
static int receiverequest(struct connection *conn, struct hbp_header *request, char **buf)
{
	int res;

	/* wait for the client to send a request header */
	if ((res = READ(request, sizeof(struct hbp_header))) <= 0) {
#if SSLSOCK
		if (SSL_get_error(ssl, res) == SSL_ERROR_ZERO_RETURN)
			return -1;
#else
		if (!res)
			return -1;
#endif
		return 0;
	}

	/* check if the header is valid and if a compatible HBP version is used by the client */
	if (request->magic != HBP_MAGIC || request->length > HBP_LENGTH_MAX) {
		lprintf("%s: not a HBP packet, disconnecting...\n", conn->host);
		return -1;
	}
	if (request->version != HBP_VERSION) {
		lprintf("%s: HBP version mismatch (client has: %u, server wants %u), disconnecting...\n",
				conn->host, request->version, HBP_VERSION);
		return -1;
	}

	for (int i = 0; reqrepmap[i].index != -1; i++) {
		if (reqrepmap[i].index != request->type)
			continue;

		lprintf("%s: %s request\n", conn->host, reqrepmap[i].name);
		break;
	}

	/* read request data (if available) */
	if (request->length) {
		if (!(*buf = malloc(request->length))) {
			lprintf("out of memory\n");
			return 0;
		}

		if (READ(*buf, request->length) <= 0) {
			free(*buf);
			return 0;
		}
	} else {
		*buf = NULL;
	}

	return 1;
}

/* handle the specified request and generate an appropriate reply */
static bool handle_request(struct connection *conn, struct hbp_header *request, const char *request_data,
		struct hbp_header *reply, char **reply_data)
{
	msgpack_sbuffer sbuf;
	msgpack_packer pack;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pack, &sbuf, msgpack_sbuffer_write);

	/* check if the session hasn't timed out */
	if (conn->logged_in && time(NULL) > conn->expiry_time) {
		lprintf("%s: session timeout: user%u, card%u\n", conn->host, conn->user_id, conn->card_id);

		/* reply header */
		reply->type = HBP_REP_TERMINATED;

		/* @param reason */
		msgpack_pack_uint8(&pack, HBP_TERM_EXPIRED);
	} else {
		switch (request->type) {
		case HBP_REQ_LOGIN:
			if (!login(conn, request_data, request->length, reply, &pack))
				return false;
			break;
		case HBP_REQ_LOGOUT:
			break;
		case HBP_REQ_INFO:
		case HBP_REQ_BALANCE:
		case HBP_REQ_TRANSFER:
		/* invalid request */
		default:
			return false;
		}
	}

	/* copy the msgpack buffer to a newly allocated array to be returned */
	if (!(*reply_data = malloc(sbuf.size))) {
		lprintf("out of memory\n");
		return false;
	}

	memcpy(*reply_data, sbuf.data, sbuf.size);
	reply->length = sbuf.size;

	return true;
}

void *session(void *args)
{
	struct connection conn;
	struct hbp_header request, reply;
	char *request_data, *reply_data;

	/* set reply header parameters */
	reply.magic = HBP_MAGIC;
	reply.version = HBP_VERSION;

	/* allocate initial buffers for reply and request data */
	/* TODO */
	request_data = malloc(128);
	reply_data = malloc(128);

	/* setup our connection structure */
	memset(&conn, 0, sizeof(struct connection));
	conn.socket = *((int *) args);

	/* verify the client certificate */
	if (!verify(&conn))
		goto ret;

	/* connect to the database */
	if (!(conn.sql = mysql_init(NULL))) {
		lprintf("out of memory\n");
		goto ret;
	}
	if (!mysql_real_connect(conn.sql, sql_host, sql_user, sql_pass, sql_db, sql_port, NULL, 0)) {
		lprintf("failed to connect to the database: %s\n", mysql_error(conn.sql));
		goto ret;
	}

	for (;;) {
		/* disconnect if the maximum number of erroneous requests has been exceeded */
		if (conn.errcnt > HBP_ERROR_MAX) {
			lprintf("%s: the maximum error count (%d) has been exceeded\n", conn.host, HBP_ERROR_MAX);

			break;
		}

		/* listen for requests from the client */
		switch (receiverequest(&conn, &request, &request_data)) {
		case 1:
			/* success */
			break;
		case 0:
			/* invalid request */
			lprintf("%s: invalid request\n", conn.host);
			conn.errcnt++;
			continue;
		case -1:
			/* disconnect */
			goto ret;
		}

		/* process the client's request */
		if (!handle_request(&conn, &request, request_data, &reply, &reply_data)) {
			lprintf("%s: error processing request\n", conn.host);
			conn.errcnt++;

			/* don't continue, but inform the client that processing their request has failed */
			reply.type = HBP_REP_ERROR;
			reply.length = 0;
		}

		/* send our reply */
		if (!sendreply(&conn, &reply, reply_data)) {
			lprintf("%s: error sending reply\n", conn.host);
			conn.errcnt++;
			continue;
		}

		for (int i = 0; reqrepmap[i].index != -1; i++) {
			if (reqrepmap[i].index != reply.type)
				continue;

			lprintf("%s: %s reply\n", conn.host, reqrepmap[i].name);
			break;
		}
	}

ret:
	lprintf("%s: Client disconnected\n", conn.host);

	/* free the reply and request data buffers */
	free(request_data);
	free(reply_data);

	/* close the database connection */
	mysql_close(conn.sql);
	mysql_thread_end();

	/* close the client connection */
	close(conn.socket);
#if SSLSOCK
	SSL_free(conn.ssl);
#endif

	pthread_exit(NULL);
}
