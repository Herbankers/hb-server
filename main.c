#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* #include <openssl/rsa.h> */
/* #include <openssl/crypto.h> */
/* #include <openssl/x509.h> */
/* #include <openssl/pem.h> */
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "kbp.h"

static char *ip = "localhost";
static uint16_t port = 80;
static char *cert, *key;

static SSL_CTX *ctx;

struct connection {
	int			sock;
	struct sockaddr_in	addr;
};

const pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static bool verbose;

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
	SSL_CTX_free(ctx);

	return -1;
}

static void *session(void *_conn)
{
	struct connection *conn = _conn;
	struct kbp_request req;
	struct kbp_reply rep;
	char addr[INET_ADDRSTRLEN];
	int errcnt = 0;
	SSL *ssl;

	rep.magic = KBP_MAGIC;

	/* Get client IP */
	inet_ntop(AF_INET, &conn->addr.sin_addr, addr,
			INET_ADDRSTRLEN);
	printf("%s: new connection @ port %u\n", addr,
			ntohs(conn->addr.sin_port));
	fflush(stdout);

	/* Setup an SSL/TLS connection */
	if (!(ssl = SSL_new(ctx)))
		fprintf(stderr, "unable to allocate SSL structure\n");
	SSL_set_fd(ssl, conn->sock);

	if (SSL_accept(ssl) != 1) {
		fprintf(stderr, "%s: SSL error\n", conn->addr);
		goto ret;
	}

	/* TODO Verify certificate */

	char test[] = "Hello World";

	for (;;) {
		/* Check if maximum error count hasn't been exceeded. */
		if (errcnt > KBP_ERROR_MAX) {
			fprintf(stderr, "%s: maximum error count (%d) has been "
					"exceeded, terminating connection",
					addr, KBP_ERROR_MAX);
			break;
		}

		/* Wait for requests from the client */
		if (SSL_read(ssl, &req, sizeof(req)) <= 0) {
			fprintf(stderr, "%s: read error\n", addr);
			errcnt++;
			continue;
		}
		if (req.magic != KBP_MAGIC || !req.type) {
			fprintf(stderr, "%s: invalid request\n", addr);
			errcnt++;
			continue;
		}

		if (req.type == KBP_T_CLOSE)
			break;

		/* TODO Process request */

		/* rep.length = 0; */
		rep.length = sizeof(test);
		rep.status = KBP_S_OK;

		if (SSL_write(ssl, &rep, sizeof(rep)) <= 0) {
			fprintf(stderr, "%s: header write error\n", addr);
			errcnt++;
			continue;
		}

		/* Check if header is received correctly */
		if (SSL_read(ssl, &req, sizeof(req)) <= 0) {
			fprintf(stderr, "%s: header read error\n", addr);
			errcnt++;
			continue;
		}
		if (req.type != KBP_T_HEAD_OK) {
			fprintf(stderr, "%s: invalid reply\n", addr);
			errcnt++;
			continue;
		}

		/* Finally, send back the requested data */
		if (SSL_write(ssl, test, sizeof(test)) <= 0) {
			fprintf(stderr, "%s: write error\n", addr);
			errcnt++;
		}
	}

	printf("%s: terminating connection\n", addr);
	fflush(stdout);

ret:
	/* Close the connection */
	close(conn->sock);
	SSL_free(ssl);

	pthread_exit(NULL);
}

static int run(void)
{
	struct connection conn;
	struct sockaddr_in server;
	pthread_t thread;
	int sock, csock;
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
		if ((conn.sock = accept(sock, (struct sockaddr *) &conn.addr,
						&socklen)) < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
		}

		/* Create a thread for new connections */
		if (pthread_create(&thread, NULL, session, &conn)) {
			fprintf(stderr, "unable to allocate thread\n");
			close(csock);
		}

		pthread_join(thread, NULL);
	}

	return 0;
}

static void usage(char *prog)
{
	printf("Usage: %s [OPTION...]\n\n%s", prog,
			"  -i IP ADDRESS        ip address to bind socket to\n"
			"  -p PORT NUMBER       port number to bind socket to\n"
			"  -c FILE              certificate file to use\n"
			"  -k FILE              private key file to use\n"
			"  -h                   show this help message\n"
			"  -v                   show verbose status messages\n"
			);
}

static void fini(void)
{
	free(cert);
	free(key);
	SSL_CTX_free(ctx);
	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	char c;

	/* Parse arguments */
	while ((c = getopt(argc, argv, "i:p:c:k:hv")) != -1) {
		switch (c) {
		/* IP address */
		case 'i':
			/* TODO */
			break;
		/* Port number */
		case 'p':
			port = strtol(optarg, NULL, 10);
			break;
		/* Certificate file path */
		case 'c':
			cert = malloc(strlen(optarg));
			strcpy(cert, optarg);
			break;
		/* Key file path */
		case 'k':
			key = malloc(strlen(optarg));
			strcpy(key, optarg);
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

	/* TODO IPv6 */
	printf("welcome to the Kech Bank server!\n");

	if (verbose)
		printf("the server will be hosted on %s:%u\n", ip, port);

	if (init() < 0)
		goto err;

	if (run() < 0)
		goto err;

	fini();
	return 0;

err:
	fini();
	return 1;
}
