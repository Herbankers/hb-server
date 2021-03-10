/*
 *
 * kech-server
 * kech.h
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

#ifndef _KECH_H
#define _KECH_H

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

#endif
