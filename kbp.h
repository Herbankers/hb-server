/*
 *
 * Kech Bank Protocol
 * kbp.h
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

#ifndef _KBP_H
#define _KBP_H

#include <stdint.h>

/*
 * General
 */

/* Kech server MAGIC number ("KECH") */
#define KBP_MAGIC	0x4B454348
/* Kech server default port */
#define KBP_PORT	42069


/*
 * Limits
 */

/* Maximum number of errors before closing the connection (server) */
#define KBP_ERROR_MAX	10
/* Maximum request/reply data length in bytes (excluding header) */
#define KBP_LENGTH_MAX	4096
/* Maximum length for an IBAN (per ISO 13616-1:2007) */
#define KBP_IBAN_MAX	34
/* Maximum length for a PIN (per ISO 9564-1:2011) */
#define KBP_PIN_MAX	12
/* Maximum length for a PIN hash */
#define KBP_HASH_MAX	128
/* Session timeout in minutes */
#define KBP_TIMEOUT	15


/*
 * Account types
 */
enum {
	/* Checkings account */
	KBP_A_CHECKING,
	/* Savings account */
	KBP_A_SAVINGS
};


/*
 * Request types
 */

enum {
	/*
	 * Request an array of accounts associated with user belonging to the
	 * active session.
	 *
	 * Needs: active session
	 * Requests: -
	 * Returns: struct kbp_reply_account[n]
	 */
	KBP_T_ACCOUNTS,
	/*
	 * Request a PIN change for card that initiated the active session.
	 *
	 * Needs: active session
	 * Requests: char pin[KBP_PIN_MAX + 1];
	 * Returns: -
	 */
	KBP_T_PIN_UPDATE,
	/*
	 * Start a new session (one allowed per connection).
	 * A session shall last KBP_TIMEOUT minutes. Requests made after this
	 * time will be answered with status flag KBP_S_TIMEOUT. Closing the
	 * connection (unexpectedly) or sending more than KBP_ERROR_MAX invalid
	 * requests will also end the session.
	 *
	 * Needs: -
	 * Requests: struct kbp_request_login
	 * Returns: -
	 */
	KBP_T_LOGIN,
	/*
	 * End the active session. Returns KBP_S_TIMEOUT on success.
	 *
	 * Needs: active session
	 * Requests: -
	 * Returns: -
	 */
	KBP_T_LOGOUT,
	/*
	 * Request an array of transactions associated with iban, iban must
	 * belong to the user associated with the active session.
	 *
	 * Needs: active session
	 * Requests: char iban[KBP_IBAN_MAX + 1]
	 * Returns: struct kbp_reply_transaction[n]
	 */
	KBP_T_TRANSACTIONS,
	/*
	 * Transfer from iban_in to iban_out, the iban_in must belong to the
	 * user associated with the active session. An empty iban_in signifies a
	 * deposit. Likewise, an empty iban_out signifies a withdrawal.
	 *
	 * Needs: active session
	 * Requests: struct kbp_request_transfer
	 * Returns: -
	 */
	KBP_T_TRANSFER
};


/*
 * Reply status
 */

/* Session has timed out */
#define KBP_S_TIMEOUT	-2
/* Invalid request */
#define KBP_S_INVALID	-1
/* Request failed */
#define KBP_S_FAIL	0
/* Request succeeded */
#define KBP_S_OK	1


/*
 * Requests
 */

/* Request header */
struct kbp_request {
	/* Magic number (KBP_MAGIC) */
	uint32_t	magic;
	/* Request type */
	uint8_t		type;
	/* Data length in bytes (may not exceed KBP_LENGTH_MAX) */
	int32_t		length;
};

/* Login request */
struct kbp_request_login {
	/* User ID */
	uint32_t	user_id;
	/* Card ID */
	uint32_t	card_id;
	/* PIN */
	char		pin[KBP_PIN_MAX + 1];
};

/* Transfer request */
struct kbp_request_transfer {
	/* Source IBAN (must be accessible with the active session) */
	char		iban_in[KBP_IBAN_MAX + 1];
	/* Destination IBAN */
	char		iban_out[KBP_IBAN_MAX + 1];
	/* Amount in EUR * 100 (2 decimal places) */
	int64_t		amount;
};


/*
 * Replies
 */

/* Reply header */
struct kbp_reply {
	/* Magic number (KBP_MAGIC) */
	uint32_t	magic;
	/* Reply status */
	int8_t		status;
	/* Data length in bytes (may not exceed KBP_LENGTH_MAX) */
	int32_t		length;
};

/* Account reply */
struct kbp_reply_account {
	/* IBAN */
	char		iban[KBP_IBAN_MAX + 1];
	/* Account type */
	uint8_t		type;
	/* Balance in EUR * 100 (2 decimal places) */
	int64_t		balance;
};

/* Transaction reply */
struct kbp_reply_transaction {
	/* Source IBAN */
	char		iban_in[KBP_IBAN_MAX + 1];
	/* Destination IBAN */
	char		iban_out[KBP_IBAN_MAX + 1];
	/* Amount in EUR * 100 (2 decimal places) */
	int64_t		amount;
};

#endif
