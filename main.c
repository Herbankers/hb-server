/*
 *
 * kech-server
 * main.c
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

#include <netinet/in.h>
#include <sys/socket.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mysql/mysql.h>

/* #include <openssl/rsa.h> */
/* #include <openssl/crypto.h> */
/* #include <openssl/x509.h> */
/* #include <openssl/pem.h> */
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "kech.h"
#include "kbp.h"

static uint16_t port = KBP_PORT;
static char *cert, *key;
bool verbose;

SSL_CTX *ctx;

char *sql_host, *sql_db, *sql_user, *sql_pass;
uint16_t sql_port;

static int init(void)
{
	const SSL_METHOD *met;

	/* Initialize OpenSSL */
	if (verbose)
		printf("initializing OpenSSL...\n");
	SSL_library_init();
	SSL_load_error_strings();

	if (!(met = TLS_server_method()))
		goto err;
	if (!(ctx = SSL_CTX_new(met)))
		goto err;

	/* Load certificate and private key files */
	if (access(cert, F_OK) < 0) {
		fprintf(stderr, "%s: %s\n", cert, strerror(errno));
		goto err;
	}
	if (verbose)
		printf("using '%s' certificate\n", cert);
	if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) != 1)
		goto err;

	if (access(key, F_OK) < 0) {
		fprintf(stderr, "%s: %s\n", key, strerror(errno));
		goto err;
	}
	if (verbose)
		printf("using '%s' private key\n", key);
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) != 1)
		goto err;

	if (!SSL_CTX_check_private_key(ctx))
		goto err;

	/* TODO Require peer certificate validation */
	/* SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback); */

	if (verbose)
		printf("OpenSSL has been successfully initialized\n");

	return 0;

err:
	ERR_print_errors_fp(stderr);
	if (ctx)
		SSL_CTX_free(ctx);

	return -1;
}

/*
 * TODO Handle SIGNALS for server termination, like waiting for clients to
 * terminate
 */
static int run(void)
{
	struct connection *conn;
	struct sockaddr_in server;
	pthread_t thread;
	int sock;
	socklen_t socklen;

	/* Create the socket */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "unable to create socket: %s\n",
				strerror(errno));
		return -1;
	}

	/* Bind the socket */
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	if (verbose)
		printf("binding socket...\n");
	if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
		fprintf(stderr, "unable to bind socket: %s\n", strerror(errno));
		return -1;
	}

	printf("waiting for connections...\n");
	if (listen(sock, SOMAXCONN) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}

	/* Wait for clients */
	socklen = sizeof(struct sockaddr_in);
	for (;;) {
		if (!(conn = malloc(sizeof(struct connection)))) {
			fprintf(stderr, "unable to allocate connection "
					"structure\n");
			continue;
		}

		if ((conn->sock = accept(sock, (struct sockaddr *) &conn->addr,
						&socklen)) < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			free(conn);
		}

		/* Create a thread for new connections */
		if (pthread_create(&thread, NULL, session, conn)) {
			fprintf(stderr, "unable to allocate thread\n");
			close(conn->sock);
			free(conn);
		}
	}

	return 0;
}

static void usage(char *prog)
{
	printf("Usage: %s [OPTION...]\n\n%s", prog,
			"  -p PORT NUMBER       port number to bind socket to\n"
			"  -c FILE              certificate file to use\n"
			"  -k FILE              private key file to use\n"
			"  -i IP ADDRESS        MySQL database host\n"
			"  -P PORT NUMBER       MySQL database port\n"
			"  -d DB                MySQL database name\n"
			"  -u USER              MySQL database username\n"
			"  -a PASSWORD          MySQL database password\n"
			"  -h                   show this help message\n"
			"  -v                   show verbose status messages\n"
			);
}

static void fini(void)
{
	mysql_library_end();

	SSL_CTX_free(ctx);

	free(cert);
	free(key);
	free(sql_host);
	free(sql_db);
	free(sql_user);
	free(sql_pass);
	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	char *s;
	int c;

	/* Parse arguments */
	while ((c = getopt(argc, argv, "p:c:k:i:P:d:u:a:hv")) == 0) {
		switch (c) {
		/* Port number */
		case 'p':
			port = strtol(optarg, NULL, 10);
			break;
		/* Certificate file path */
		case 'c':
			if (!(cert = malloc(strlen(optarg))))
				goto err;
			strcpy(cert, optarg);
			break;
		/* Key file path */
		case 'k':
			if (!(key = malloc(strlen(optarg))))
				goto err;
			strcpy(key, optarg);
			break;
		/* MySQL database host */
		case 'I':
			if (!(sql_host = malloc(strlen(optarg))))
				goto err;
			strcpy(sql_host, optarg);
			break;
		/* MySQL database port */
		case 'P':
			sql_port = strtol(optarg, NULL, 10);
			break;
		/* MySQL database database name */
		case 'd':
			if (!(sql_db = malloc(strlen(optarg))))
				goto err;
			strcpy(sql_db, optarg);
			break;
		/* MySQL database username */
		case 'u':
			if (!(sql_user = malloc(strlen(optarg))))
				goto err;
			strcpy(sql_user, optarg);
			break;
		/* MySQL database password */
		case 'a':
			if (!(sql_pass = malloc(strlen(optarg))))
				goto err;
			strcpy(sql_pass, optarg);
			break;
		/* Usage */
		case 'h':
			usage(argv[0]);
			free(cert);
			free(key);

			return 0;
		/* Verbose */
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage(argv[0]);
			goto err;
		}
	}

	if (!cert) {
		fprintf(stderr, "please specify a certificate file\n");
		usage(argv[0]);
		goto err;
	} else if (!key) {
		fprintf(stderr, "please specify a private key file\n");
		usage(argv[0]);
		goto err;
	}

	if (!sql_host) {
		s = "localhost";
		sql_host = malloc(strlen(s));
		strcpy(sql_host, s);
	}

	if (!sql_db) {
		s = "kech";
		sql_db = malloc(strlen(s));
		strcpy(sql_db, s);
	}

	if (!sql_user) {
		s = "root";
		sql_user = malloc(strlen(s));
		strcpy(sql_user, s);
	}

	if (!sql_pass) {
		s = "";
		sql_pass = malloc(strlen(s));
		strcpy(sql_pass, s);
	}

	/* TODO IPv6 */
	printf("welcome to the Kech Bank server!\n");

	if (verbose)
		printf("the server will be hosted on localhost:%u\n", port);

	if (init() < 0)
		goto err;

	mysql_library_init(0, 0, NULL);

	if (run() < 0)
		goto err;

	fini();
	return 0;

err:
	fini();
	return 1;
}
