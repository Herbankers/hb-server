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
	bool	valid;
	char	iban[KBP_IBAN_MAX + 1];
	time_t	expiry_time;
};

static int accounts_get(char **buf)
{
	free(*buf);
	*buf = NULL;

	/* TODO */

	return -1;
}

static int balance_get(MYSQL *sql, char **buf)
{
	char iban[KBP_IBAN_MAX + 1];
	char *_q, *q = NULL;
	MYSQL_RES *res = NULL;
	MYSQL_ROW row;
	int64_t *val;

	memcpy(&iban, *buf, KBP_IBAN_MAX + 1);
	free(*buf);
	*buf = NULL;

	/* Prepare the query */
	_q = "SELECT `balance` FROM `accounts` WHERE `iban` = '%s'";
	if (!(q = malloc(strlen(_q) + strlen(iban))))
		goto err;
	sprintf(q, _q, iban);

	/* Run it */
	if (mysql_query(sql, q))
		goto err;
	if (!(res = mysql_store_result(sql)))
		goto err;
	if (!(row = mysql_fetch_row(res)))
		goto err;

	/* Store the result */
	if (!(val = malloc(sizeof(int64_t))))
		goto err;
	*val = (int64_t) (strtod(row[0], NULL) * 100);
	*buf = (char *) val;

	free(q);
	mysql_free_result(res);
	return sizeof(int64_t);

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

void *session(void *_conn)
{
	struct connection *conn = _conn;
	struct token tok;
	struct kbp_request req;
	struct kbp_reply rep;
	char addr[INET_ADDRSTRLEN];
	char *buf;
	int res, errcnt = 0;
	MYSQL *sql = NULL;
	SSL *ssl = NULL;

	memset(&tok, 0, sizeof(tok));
	rep.magic = KBP_MAGIC;

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

	if (!mysql_real_connect(sql, "localhost", "root", "", "kech", 0, NULL,
			0)) {
		fprintf(stderr, "database connection error\n");
		goto ret;
	}

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

		if (verbose)
			printf("%s: request with type %u\n", addr, req.type);

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

		/* Check if session hasn't timed out */
		if (time(NULL) > tok.expiry_time) {
			if (verbose)
				printf("%s: session timeout: %s\n", addr,
						tok.iban);
			tok.valid = 0;
			rep.status = KBP_S_TIMEOUT;
		} else if (tok.valid) {
			rep.status = KBP_S_INVALID;
		} else {
			rep.status = KBP_S_TIMEOUT;
		}

		res = 0;

		/* Process the request */
		switch (req.type) {
		case KBP_T_ACCOUNTS:
			if (!tok.valid)
				break;

			if ((res = accounts_get(&buf)) < 0)
				rep.status = KBP_S_FAIL;
			else
				rep.status = KBP_S_OK;
			break;
		case KBP_T_BALANCE:
			if (!tok.valid)
				break;

			if (req.length != KBP_IBAN_MAX + 1) {
				rep.status = KBP_S_INVALID;
				break;
			}

			if ((res = balance_get(sql, &buf)) < 0)
				rep.status = KBP_S_FAIL;
			else
				rep.status = KBP_S_OK;
			break;
		case KBP_T_PIN_UPDATE:
			if (!tok.valid)
				break;

			if (req.length != KBP_PIN_MAX + 1) {
				rep.status = KBP_S_INVALID;
				break;
			}

			if ((res = pin_update(&buf)) < 0)
				rep.status = KBP_S_FAIL;
			else
				rep.status = KBP_S_OK;
			break;
		case KBP_T_LOGIN:
			if (req.length != sizeof(struct kbp_request_login)) {
				rep.status = KBP_S_INVALID;
				break;
			}

			if ((res = login(&buf, &tok)) < 0) {
				rep.status = KBP_S_FAIL;
			} else {
				if (verbose)
					printf("%s: session login: %s\n", addr,
							tok.iban);
				rep.status = KBP_S_OK;
			}
			break;
		case KBP_T_LOGOUT:
			if (!tok.valid)
				break;

			if (verbose)
				printf("%s: session logout: %s\n", addr,
						tok.iban);

			res = 0;
			tok.valid = 0;
			rep.status = KBP_S_TIMEOUT;
			break;
		case KBP_T_TRANSACTIONS:
			if (!tok.valid)
				break;

			if (req.length != KBP_IBAN_MAX + 1) {
				rep.status = KBP_S_INVALID;
				break;
			}

			if ((res = transactions_get(&buf)) < 0)
				rep.status = KBP_S_FAIL;
			else
				rep.status = KBP_S_OK;
			break;
		case KBP_T_TRANSFER:
			if (!tok.valid)
				break;

			if (req.length != sizeof(struct kbp_request_transfer)) {
				rep.status = KBP_S_INVALID;
				break;
			}

			if ((res = transfer(&buf)) < 0)
				rep.status = KBP_S_FAIL;
			else
				rep.status = KBP_S_OK;
			break;
		/* Invalid or unimplemented request */
		default:
			res = 0;
			break;
		}
		rep.length = (res < 0) ? 0 : res;

		/* Send the header */
		if (SSL_write(ssl, &rep, sizeof(rep)) <= 0) {
			fprintf(stderr, "%s: header write error\n", addr);
			errcnt++;
			goto next;
		}

		/* Finally, send back the data */
		if (rep.length && SSL_write(ssl, buf, rep.length) <= 0) {
			fprintf(stderr, "%s: write error: %d\n", addr, rep.length);
			errcnt++;
			goto next;
		}

		if (verbose)
			printf("%s: request has been processed\n", addr);

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
