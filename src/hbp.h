/** @file */
/*
 *
 * HerBank Protocol v1
 *
 * Copyright (C) 2021 Bastiaan Teeuwen <bastiaan@mkcl.nl>
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

#include <stdint.h>

/*
 * General
 */

/* HBP MAGIC number ("HERB") */
#define HBP_MAGIC	0x48455242
/* HBP version */
#define HBP_VERSION	1
/* HBP server default port */
#define HBP_PORT	8420


/*
 * Limits
 */

/** Maximum number of erroneous requests before closing the connection */
#define HBP_ERROR_MAX	10
/** Maximum request/reply data length in bytes */
#define HBP_LENGTH_MAX	65536
/** Minimum length for an IBAN (per ISO 13616-1:2007) */
#define HBP_IBAN_MIN	9
/** Maximum length for an IBAN (per ISO 13616-1:2007) */
#define HBP_IBAN_MAX	34
/** Minimum length for a PIN (per ISO 9564-1:2011) */
#define HBP_PIN_MIN	4
/** Maximum length for a PIN (per ISO 9564-1:2011) */
#define HBP_PIN_MAX	12
/** Maximum times PIN entry can be attempted before blocking the card */
#define HBP_PINTRY_MAX	3
/** Session timeout in seconds */
#define HBP_TIMEOUT	(10 * 60)
/** Card UI length in bytes */
#define HBP_UID_MAX	6


/*
 * Types
 */

/* Account types */
typedef enum {
	/* Checkings account */
	HBP_A_CHECKING,
	/* Savings account */
	HBP_A_SAVINGS
} kbp_account_t;

/* Login results */
typedef enum {
	/* Successful login */
	KBP_L_GRANTED,
	/* Invalid PIN */
	KBP_L_DENIED,
	/* Blocked card */
	KBP_L_BLOCKED
} kbp_login_res;

/** Requests types */
typedef enum {
	/**
	 * @brief Request to start a new session
	 *
	 * A session will automatically time out after HBP_TIMEOUT seconds, regardless of activity. Loss of connection
	 * or sending more than HBP_ERROR_MAX invalid requests will also end an active session.
	 * Only one session per connection is possible.
	 *
	 */
	HBP_REQ_LOGIN,

	HBP_REQ_LOGOUT,

	HBP_REQ_INFO,

	HBP_REQ_BALANCE,

	HBP_REQ_TRANSFER
} hbp_request_t;

/** Reply types */
typedef enum {
	HBP_REP_LOGIN,
	/**
	 * @brief Reply to a request for a new session
	 *
	 * @return HBP_L_GRANTED sdf
	 * @return HBP_L_DENIED
	 * @return HBP_L_BLOCKED
	 */

	HBP_REP_LOGOUT,

	HBP_REP_INFO,

	HBP_REP_BALANCE,

	HBP_REP_TRANSFER
};

/* Requests */
typedef enum {
	/*
	 * Start a new session (one allowed per connection).
	 * A session shall last KBP_TIMEOUT minutes. Requests made after this
	 * time will be answered with status flag KBP_S_TIMEOUT. Closing the
	 * connection (unexpectedly) or sending more than KBP_ERROR_MAX invalid
	 * requests will also end the session. KBP_L_DENIED shall be returned
	 * on an invalid pin entry. KBP_PINTRY_MAX invalid entries will result
	 * in the card being blocked after which KBP_L_BLOCKED shall be
	 * returned until the card is manually unblocked again. KBP_L_GRANTED
	 * will be returned if a session has successfully been started.
	 *
	 * Needs: -
	 * Requests: struct kbp_request_login
	 * Returns: uint8_t (kbp_login_res)
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
	 * Transfer from iban_src to iban_dest, the iban_src must belong to the
	 * user associated with the active session. An empty iban_src signifies
	 * a deposit. Likewise, an empty iban_dest signifies a withdrawal.
	 *
	 * Needs: active session
	 * Requests: struct kbp_request_transfer
	 * Returns: -
	 */
	KBP_T_TRANSFER
} kbp_request_t;

/* Reply status */
typedef enum {
	/* Session has timed out */
	KBP_S_TIMEOUT = -2,
	/* Invalid request */
	KBP_S_INVALID,
	/* Request failed */
	KBP_S_FAIL,
	/* Request succeeded */
	KBP_S_OK
} kbp_reply_s;


/*
 * Requests
 */

/* Request header */
struct kbp_request {
	/* Magic number (KBP_MAGIC) */
	uint32_t	magic;
	/* KBP Version (KBP_VERSION) */
	uint8_t		version;
	/* Request type (kbp_request_t) */
	uint8_t		type;
	/* Data length in bytes (may not exceed KBP_LENGTH_MAX) */
	uint32_t	length;
} __attribute__((packed));

/* Login request */
struct kbp_request_login {
	/* Card UID */
	uint8_t		uid[KBP_UID_MAX];
	/* PIN */
	char		pin[KBP_PIN_MAX + 1];
} __attribute__((packed));

/* Transfer request */
struct kbp_request_transfer {
	/* Source IBAN (must be accessible with the active session) */
	char		iban_src[KBP_IBAN_MAX + 1];
	/* Destination IBAN */
	char		iban_dest[KBP_IBAN_MAX + 1];
	/* Amount in EUR * 100 (2 decimal places) */
	int64_t		amount;
} __attribute__((packed));


/*
 * Replies
 */

/* Reply header */
struct kbp_reply {
	/* Magic number (KBP_MAGIC) */
	uint32_t	magic;
	/* KBP Version (KBP_VERSION) */
	uint8_t		version;
	/* Reply status (kbp_reply_s) */
	int8_t		status;
	/* Data length in bytes (may not exceed KBP_LENGTH_MAX) */
	uint32_t	length;
} __attribute__((packed));

/* Account reply */
struct kbp_reply_account {
	/* IBAN */
	char		iban[KBP_IBAN_MAX + 1];
	/* Account type (kbp_account_t) */
	uint8_t		type;
	/* Balance in EUR * 100 (2 decimal places) */
	int64_t		balance;
} __attribute__((packed));

/* Transaction reply */
struct kbp_reply_transaction {
	/* Source IBAN */
	char		iban_src[KBP_IBAN_MAX + 1];
	/* Destination IBAN */
	char		iban_dest[KBP_IBAN_MAX + 1];
	/* Amount in EUR * 100 (2 decimal places) */
	int64_t		amount;
} __attribute__((packed));
