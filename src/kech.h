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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <openssl/ssl.h>
#include <netinet/in.h>

#include <mysql.h>

/* Session token */
struct token {
	bool		valid;
	uint32_t	user_id;
	uint32_t	card_id;
	time_t		expiry_time;
};

/* argon2 hashing parameters */
#define ARGON2_PASS	2	/* 2 passes */
#define ARGON2_MEMORY	65536	/* 64 Mb memory usage limit */
#define ARGON2_PARALLEL	1	/* number of threads */
#define ARGON2_SALT_LEN 16	/* length of the salt in bytes */
#define ARGON2_HASH_LEN 32	/* length of the output hash in bytes */
#define ARGON2_ENC_LEN	108	/* length of the output encoded string (hash + salt + params) in bytes */

extern char port[6];
#if SSLSOCK
extern SSL_CTX *ctx;
#endif
extern char *sql_host, *sql_db, *sql_user, *sql_pass;
extern uint16_t sql_port;

void lprintf(const char *msg, ...);

int iban_getcheck(const char *_iban);
bool iban_validate(const char *iban);

int accounts_get(MYSQL *sql, struct token *tok, char **buf);
int login(MYSQL *sql, struct token *tok, char **buf);
int pin_update(MYSQL *sql, struct token *tok, char **buf);
int transactions_get(char **buf);
int transfer(MYSQL *sql, struct token *tok, char **buf);

void *session(void *_conn);
