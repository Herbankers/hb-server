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

#define IPV4_IDENTIFIER	"::ffff:"

/* connect to the client and verify the client certificate */
static bool verify(struct connection *conn)
{
	struct sockaddr_in6 addr;
	socklen_t len = sizeof(addr);

	/* retrieve the client IP */
	getpeername(conn->socket, (struct sockaddr *) &addr, &len);
	inet_ntop(AF_INET6, &addr.sin6_addr, conn->host, INET6_ADDRSTRLEN);

	/* convert the IPv4 mapped IPv6 address to an IPv4 address */
	if (strncmp(conn->host, IPV4_IDENTIFIER, strlen(IPV4_IDENTIFIER)) == 0)
		memmove(conn->host, conn->host + strlen(IPV4_IDENTIFIER), strlen(conn->host) - strlen(IPV4_IDENTIFIER) + 1);

#if SSLSOCK
	/* setup an SSL/TLS connection */
	if (!(conn->ssl = SSL_new(ctx))) {
		iprintf("unable to allocate SSL structure\n");
		return false;
	}
	SSL_set_fd(conn->ssl, conn->socket);

	if (SSL_accept(conn->ssl) <= 0) {
		iprintf("%s: SSL error\n", conn->host);
		return false;
	}

	/* get and verify the client certificate */
	if (!SSL_get_peer_certificate(conn->ssl)) {
		iprintf("client failed to present certificate\n");
		return false;
	}
	if (SSL_get_verify_result(conn->ssl) != X509_V_OK) {
		iprintf("certificate verfication failed\n");
		return false;
	}
#endif

	dprintf("%s: Client connected\n", conn->host);

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
		if (SSL_get_error(conn->ssl, res) != SSL_ERROR_ZERO_RETURN)
			return -1;
#else
		if (!res)
			return -1;
#endif
		return 0;
	}

	/* check if the header is valid and if a compatible HBP version is used by the client */
	if (request->magic != HBP_MAGIC || request->length > HBP_LENGTH_MAX) {
		iprintf("%s: not a HBP packet, disconnecting...\n", conn->host);
		return -1;
	}
	if (request->version != HBP_VERSION) {
		iprintf("%s: HBP version mismatch (client has: %u, server wants %u), disconnecting...\n",
				conn->host, request->version, HBP_VERSION);
		return -1;
	}

	for (int i = 0; reqrepmap[i].index != -1; i++) {
		if (reqrepmap[i].index != request->type)
			continue;

		dprintf("%s: %s request\n", conn->host, reqrepmap[i].name);
		break;
	}

	/* read request data (if available) */
	if (request->length) {
		if (!(*buf = malloc(request->length))) {
			iprintf("out of memory\n");
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
		/* log out if the session has timed out */
		iprintf("%s: Session timeout: %s (User %u, Card %u)\n", conn->host, conn->iban, conn->user_id, conn->card_id);

		/* reply header */
		reply->type = HBP_REP_TERMINATED;

		/* @param reason */
		msgpack_pack_int(&pack, HBP_TERM_EXPIRED);
	} else {
		/* TODO instead maintain a struct of request handlers and call then appropriately
		 * (to avoid having to use a switch here)
		 */
		switch (request->type) {
		case HBP_REQ_LOGIN:
			if (conn->logged_in)
				goto err;

			if (!login(conn, request_data, request->length, reply, &pack))
				goto err;

			if (conn->logged_in)
				iprintf("%s: Session login: %s (User %u, Card %u)\n", conn->host, conn->iban,
						conn->user_id, conn->card_id);

			break;
		case HBP_REQ_LOGOUT:
			if (!conn->logged_in)
				goto err;

			iprintf("%s: Session logout: %s (User %u, Card %u)\n", conn->host, conn->iban, conn->user_id, conn->card_id);

			conn->logged_in = false;
			/* clear all other variables for security */
			conn->expiry_time = 0;
			conn->user_id = 0;
			conn->card_id = 0;

			/* also send an appropriate reply to the client that it's been logged out */
			reply->type = HBP_REP_TERMINATED;
			/* @param reason */
			msgpack_pack_int(&pack, HBP_TERM_LOGOUT);

			break;
		case HBP_REQ_INFO:
			if (!conn->logged_in)
				goto err;

			if (!info(conn, request_data, request->length, reply, &pack))
				goto err;

			break;
		case HBP_REQ_BALANCE:
			if (!conn->logged_in)
				goto err;

			if (!balance(conn, request_data, request->length, reply, &pack))
				goto err;

			break;
		case HBP_REQ_TRANSFER:
			if (!conn->logged_in)
				goto err;

			if (!transfer(conn, request_data, request->length, reply, &pack))
				goto err;

			break;
		/* invalid request */
		default:
			goto err;
		}
	}

	/* copy the msgpack buffer to a newly allocated array to be returned */
	if (!(*reply_data = realloc(*reply_data, sbuf.size))) {
		iprintf("out of memory\n");
		msgpack_sbuffer_destroy(&sbuf);
		return false;
	}

	memcpy(*reply_data, sbuf.data, sbuf.size);
	reply->length = sbuf.size;

	msgpack_sbuffer_destroy(&sbuf);

	return true;

err:
	msgpack_sbuffer_destroy(&sbuf);

	return false;
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
	request_data = malloc(128);
	reply_data = malloc(128);
	/* TODO check for enomem */

	/* setup our connection structure */
	memset(&conn, 0, sizeof(struct connection));
	conn.socket = *((int *) args);

	/* connect to the database */
	if (!(conn.sql = mysql_init(NULL))) {
		iprintf("out of memory\n");
		goto ret;
	}
	if (!mysql_real_connect(conn.sql, sql_host, sql_user, sql_pass, sql_db, sql_port, NULL, 0)) {
		iprintf("failed to connect to the database: %s\n", mysql_error(conn.sql));
		goto ret;
	}

	/* verify the client certificate */
	if (!verify(&conn))
		goto ret;

	for (;;) {
		/* disconnect if the maximum number of erroneous requests has been exceeded */
		if (conn.errcnt > HBP_ERROR_MAX) {
			iprintf("%s: the maximum error count (%d) has been exceeded\n", conn.host, HBP_ERROR_MAX);

			break;
		}

		/* listen for requests from the client */
		switch (receiverequest(&conn, &request, &request_data)) {
		case 1:
			/* success */
			break;
		case 0:
			/* invalid request */
			iprintf("%s: invalid request\n", conn.host);
			conn.errcnt++;
			continue;
		case -1:
			/* disconnect */
			goto ret;
		}

		/* process the client's request */
		if (!handle_request(&conn, &request, request_data, &reply, &reply_data)) {
			iprintf("%s: error processing request\n", conn.host);
			conn.errcnt++;

			/* don't continue, but inform the client that processing their request has failed */
			reply.type = HBP_REP_ERROR;
			reply.length = 0;
		}

		/* send our reply */
		if (!sendreply(&conn, &reply, reply_data)) {
			iprintf("%s: error sending reply\n", conn.host);
			conn.errcnt++;
			continue;
		}

		for (int i = 0; reqrepmap[i].index != -1; i++) {
			if (reqrepmap[i].index != reply.type)
				continue;

			dprintf("%s: %s reply\n", conn.host, reqrepmap[i].name);
			break;
		}
	}

ret:
	dprintf("%s: Client disconnected\n", conn.host);

	/* free the reply and request data buffers */
	free(request_data);
	free(reply_data);

	/* close the database connection */
	mysql_close(conn.sql);
	mysql_thread_end();

	/* close the client connection */
#if SSLSOCK
	/* SSL_shutdown(conn.ssl); */
	if (conn.ssl)
		SSL_free(conn.ssl);
#endif
	close(conn.socket);

	pthread_exit(NULL);
}
