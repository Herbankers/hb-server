/** @file */
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

/**
 * @brief Session token
 *
 * A new instance of struct token is created for every client that connects and successfully logs in.
 * The information in this struct is only used internally and not available or sent to the client at any moment.
 */
struct token {
	bool		valid;
	uint32_t	user_id;
	uint32_t	card_id;
	time_t		expiry_time;
};

/** @brief argon2: Number of passes to make */
#define ARGON2_PASS	2
/** @brief argon2: Memory usage limit */
#define ARGON2_MEMORY	65536
/** @brief argon2: Number of threads */
#define ARGON2_PARALLEL	1
/** @brief argon2: Length of the salt in bytes */
#define ARGON2_SALT_LEN 16
/** @brief argon2: Length of the output hash in bytes */
#define ARGON2_HASH_LEN 32
/** @brief argon2: Length of the output encoded string (hash + salt + params) in bytes */
#define ARGON2_ENC_LEN	108

/** @brief Port on which the server will be hosted */
extern char port[6];
#if SSLSOCK
extern SSL_CTX *ctx;
#endif
extern char *sql_host, *sql_db, *sql_user, *sql_pass;
extern uint16_t sql_port;

/**
 * @brief Log to command-line (and optionally to a log file)
 *
 * This function prints to the command-line (when the -v flag is specified in argv).
 * The log is also written to a file (when -o is specified in argv).
 * Parameters are exactly the same as printf(3).
 *
 * @param fmt Specifies how subsequent arguments are converted
 * @param ... Variable number of arguments
 */
void lprintf(const char *fmt, ...);

int accounts_get(MYSQL *sql, struct token *tok, char **buf);
int login(MYSQL *sql, struct token *tok, char **buf);
int pin_update(MYSQL *sql, struct token *tok, char **buf);
int transactions_get(char **buf);
int transfer(MYSQL *sql, struct token *tok, char **buf);

int iban_getcheck(const char *_iban);
bool iban_validate(const char *iban);

/**
 * @brief Session thread
 *
 * This function is called on a new thread for every client that connects to our server.
 *
 * @param args An argument list received from the main thread, in our case containing only a copy of the client socket
 *             connection handler
 */
void *session(void *args);
