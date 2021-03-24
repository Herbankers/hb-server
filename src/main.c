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

#include <netinet/in.h>
#include <sys/socket.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if SSLSOCK
#  include <openssl/err.h>
#  include <openssl/ssl.h>
#else
#  include <errno.h>
#endif

#include "hbp.h"
#include "herbank.h"

char port[6];
#if SSLSOCK
static char *ca, *cert, *key;
#endif

static char *log_path;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static bool verbose;

#if SSLSOCK
SSL_CTX *ctx;
#endif

char *sql_host, *sql_db, *sql_user, *sql_pass;
uint16_t sql_port;

void lprintf(const char *fmt, ...)
{
	FILE *file = NULL;
	time_t t;
	struct tm *tm;
	va_list args;

	pthread_mutex_lock(&log_lock);

	/* write the log entry to a file if one has been specified */
	if (log_path && (file = fopen(log_path, "a"))) {
		t = time(NULL);
		tm = localtime(&t);

		va_start(args, fmt);
		fprintf(file, "[%04d-%02d-%02d %02d:%02d:%02d] ", 1900 + tm->tm_year, tm->tm_mon, tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		vfprintf(file, fmt, args);
		va_end(args);

		fclose(file);
	}

	if (verbose) {
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}

	pthread_mutex_unlock(&log_lock);
}

bool query(struct connection *conn, const char *fmt, ...)
{
	va_list args;
	char *query;

	/* allocate memory for our query */
	va_start(args, fmt);
	if (!(query = malloc(vsnprintf(NULL, 0, fmt, args)))) {
		lprintf("out of memory\n");
		return false;
	}

	vsprintf(query, fmt, args);

	va_end(args);

	/* process the query */
	if (mysql_query(conn->sql, query)) {
		lprintf("%serror running query: %s\n", mysql_error(conn->sql));
		return false;
	}

	free(query);

	return true;
}

#if SSLSOCK
/* load our CA, certificate and private key into memory */
static bool ssl_initialize(void)
{
	const SSL_METHOD *met;

	/* initialize OpenSSL */
	lprintf(" initializing OpenSSL...\n");
	SSL_library_init();
	SSL_load_error_strings();

	if (!(met = TLS_server_method()))
		goto err;
	if (!(ctx = SSL_CTX_new(met)))
		goto err;

	/* load CA */
	if (access(ca, F_OK) < 0) {
		lprintf("%s: %s\n", ca, strerror(errno));
		goto err;
	}
	lprintf("using '%s' CA\n", ca);
	if (!SSL_CTX_load_verify_locations(ctx, ca, NULL)) {
		lprintf("unable to load CA: %s\n", ca);
		goto err;
	}

	/* load certificate and private key files */
	if (access(cert, F_OK) < 0) {
		lprintf("%s: %s\n", cert, strerror(errno));
		goto err;
	}
	lprintf("using '%s' certificate\n", cert);
	if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) != 1)
		goto err;

	if (access(key, F_OK) < 0) {
		lprintf("%s: %s\n", key, strerror(errno));
		goto err;
	}
	lprintf("using '%s' private key\n", key);
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) != 1)
		goto err;

	if (!SSL_CTX_check_private_key(ctx))
		goto err;

	/* require client verification */
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	SSL_CTX_set_verify_depth(ctx, 1);

	lprintf("OpenSSL has been successfully initialized\n");

	return true;

err:
	ERR_print_errors_fp(stderr);
	if (ctx)
		SSL_CTX_free(ctx);

	return false;
}
#endif

/* test if our MySQL credentials are valid */
static bool mysql_test(void)
{
	MYSQL *sql;

	/* intialize MySQL */
	if (!(sql = mysql_init(NULL))) {
		lprintf("mysql internal error\n");
		return false;
	}

	/* test our MySQL username, password and if the database exists */
	if (!mysql_real_connect(sql, sql_host, sql_user, sql_pass, sql_db, sql_port, NULL, 0)) {
		lprintf("failed to connect to the database: %s\n", mysql_error(sql));
		return false;
	}

	/* close the connection again, this was just a test */
	mysql_close(sql);

	return true;
}

/*
 * TODO Handle SIGNALS for server termination, like waiting for clients to
 * terminate
 *
 * e.g. send all clients a that their session has terminated (HBP_REP_LOGOUT)
 */
static bool run(void)
{
	struct sockaddr_in6 server;
	pthread_t thread;
	int sock, csock, on = 1;

#if SSLSOCK
	if (!ssl_initialize())
		return false;
#endif

	/* test if our MySQL details are working */
	if (!mysql_test())
		return false;

	/* create the socket */
	if ((sock = socket(PF_INET6, SOCK_STREAM, 0)) < 0) {
		lprintf("unable to create socket: %s\n", strerror(errno));
		return false;
	}

	/* allow socket to be reused */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0) {
		lprintf("%s\n", strerror(errno));
		return false;
	}

	/* bind the socket */
	server.sin6_family = AF_INET6;
	server.sin6_addr = in6addr_any;
	server.sin6_port = htons(strtol(port, NULL, 10));

	lprintf(" Binding socket...\n");
	if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
		lprintf("unable to bind socket: %s\n", strerror(errno));
		return false;
	}

	lprintf(" Listening for connections...\n");
	if (listen(sock, SOMAXCONN) < 0) {
		lprintf("%s\n", strerror(errno));
		return false;
	}

	/* listen for clients */
	for (;;) {
		if ((csock = accept(sock, NULL, NULL)) < 0) {
			lprintf("%s\n", strerror(errno));
			continue;
		}

		/* create a thread for every new connection */
		if (pthread_create(&thread, NULL, session, &csock)) {
			lprintf("unable to allocate thread\n");
			close(csock);
		}
	}

	return true;
}

static void usage(char *prog)
{
	printf("Usage: %s [OPTION...]\n\n%s", prog,
			"  -P PORT NUMBER       port number to bind socket to\n"
#if SSLSOCK
			"  -r FILE              CA file to use\n"
			"  -c FILE              certificate file to use\n"
			"  -k FILE              private key file to use\n"
#endif
			"  -i HOST:PORT         MySQL server host (default is localhost)\n"
			"  -d DB                MySQL database name\n"
			"  -u USER              MySQL server username\n"
			"  -p PASSWORD          MySQL server password\n"
			"  -o FILE              file to output log to\n"
			"  -h                   show this help message\n"
			"  -v                   show verbose status messages\n"
			);
}

static void finalize(void)
{
	mysql_library_end();

#if SSLSOCK
	SSL_CTX_free(ctx);

	free(ca);
	free(cert);
	free(key);
#endif
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

	mysql_library_init(0, 0, NULL);

	/* parse command-line arguments */
	while ((c = getopt(argc, argv, "P:"
#if SSLSOCK
			"C:c:k:"
#endif
			"i:d:u:p:o:hv")) != -1) {
		switch (c) {
		/* port number */
		case 'P':
			strncpy(port, optarg, 6);
			break;
#if SSLSOCK
		/* CA file path */
		case 'C':
			if (!(ca = malloc(strlen(optarg) + 1)))
				goto err;
			strcpy(ca, optarg);
			break;
		/* certificate file path */
		case 'c':
			if (!(cert = malloc(strlen(optarg) + 1)))
				goto err;
			strcpy(cert, optarg);
			break;
		/* key file path */
		case 'k':
			if (!(key = malloc(strlen(optarg) + 1)))
				goto err;
			strcpy(key, optarg);
			break;
#endif
		/* MySQL server host */
		case 'i':
			/* separate host from port */
			s = strtok(optarg, ":");

			if (!(sql_host = malloc(strlen(s) + 1)))
				goto err;
			strcpy(sql_host, s);

			s = strtok(NULL, ":");
			sql_port = strtol(s, NULL, 10);

			break;
		/* MySQL database name */
		case 'd':
			if (!(sql_db = malloc(strlen(optarg) + 1)))
				goto err;
			strcpy(sql_db, optarg);
			break;
		/* MySQL server username */
		case 'u':
			if (!(sql_user = malloc(strlen(optarg) + 1)))
				goto err;
			strcpy(sql_user, optarg);
			break;
		/* MySQL server password */
		case 'p':
			if (!(sql_pass = malloc(strlen(optarg) + 1)))
				goto err;
			strcpy(sql_pass, optarg);
			break;
		/* log file path */
		case 'o':
			if (!(log_path = malloc(strlen(optarg) + 1)))
				goto err;
			strcpy(log_path, optarg);
			break;
		/* show usage */
		case 'h':
			usage(argv[0]);

			finalize();
			return 0;
		/* verbose logging */
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage(argv[0]);
			goto err;
		}
	}

#if SSLSOCK
	if (!ca) {
		lprintf("hb-server: please specify a CA file\n");
		usage(argv[0]);
		goto err;
	} else if (!cert) {
		lprintf("hb-server: please specify a certificate file\n");
		usage(argv[0]);
		goto err;
	} else if (!key) {
		lprintf("hb-server: please specify a private key file\n");
		usage(argv[0]);
		goto err;
	}
#endif

	/* set defaults in case the user hasn't specified these */
	if (!port[0])
		sprintf(port, "%d", HBP_PORT);

	if (!sql_host) {
		s = "localhost";
		sql_host = malloc(strlen(s) + 1);
		strcpy(sql_host, s);
	}

	if (!sql_db) {
		s = "herbankdb";
		sql_db = malloc(strlen(s) + 1);
		strcpy(sql_db, s);
	}

	if (!sql_user) {
		s = "hb-server";
		sql_user = malloc(strlen(s) + 1);
		strcpy(sql_user, s);
	}

	if (!sql_pass) {
		s = "password";
		sql_pass = malloc(strlen(s) + 1);
		strcpy(sql_pass, s);
	}

	/* the exciting part, get everything started up */
	lprintf("Copyright (C) 2021 Herbank Server v1.0\n");
	lprintf("The server will be hosted on port %s\n", port);

	if (!run())
		goto err;

	finalize();
	return 0;

err:
	finalize();
	return 1;
}
