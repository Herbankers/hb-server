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

/** @brief Version of the HBP protocol */
#define HBP_VERSION	1
/** @brief Magic number */
#define HBP_MAGIC	0x4B9A208E
/** @brief Default port on which hb-server is hosted */
#define HBP_PORT	8420

/** @brief Maximum number of erroneous requests before closing the connection */
#define HBP_ERROR_MAX	10
/** @brief Maximum request/reply data length in bytes */
#define HBP_LENGTH_MAX	32768
/** @brief Minimum length for an IBAN (per ISO 13616-1:2007) */
#define HBP_IBAN_MIN	9
/** @brief Maximum length for an IBAN (per ISO 13616-1:2007) */
#define HBP_IBAN_MAX	34
/** @brief Minimum length for a PIN (per ISO 9564-1:2011) */
#define HBP_PIN_MIN	4
/** @brief Maximum length for a PIN (per ISO 9564-1:2011) */
#define HBP_PIN_MAX	12
/** @brief Maximum times PIN entry can be attempted before the card will be automatically blocked */
#define HBP_PINTRY_MAX	3
/** @brief Session timeout in seconds */
#define HBP_TIMEOUT	(5 * 60)
/** @brief Card ID length in bytes (excluding null character) */
#define HBP_CID_MAX	12

/**
 * @brief Request and reply header
 *
 * This structure leads the msgpack data and specifies the version of HBP used (to check compatiblity), the size of the
 * following data and the type of request/reply.
 */
struct hbp_header {
	/** @brief Magic number (see #HBP_MAGIC) */
	uint32_t	magic;
	/** @brief Protocol version (see #HBP_VERSION) */
	uint8_t		version;
	/** @brief Request type (see #hbp_request_t) or Reply type (see #hbp_reply_t) */
	uint8_t		type;
	/** @brief Length of the following msgpack data (may not exceed #HBP_LENGTH_MAX bytes) */
	uint16_t	length;
} __attribute__((packed));

static const struct {
	int index;
	const char *name;
} reqrepmap[] = {
	/* requests */
	{ 0, "LOGIN" },
	{ 1, "LOGOUT" },
	{ 2, "INFO" },
	{ 3, "BALANCE" },
	{ 4, "TRANSFER" },

	/* replies */
	{ 128, "LOGIN" },
	{ 129, "TERMINATED" },
	{ 130, "INFO" },
	{ 131, "BALANCE" },
	{ 132, "TRANSFER" },
	{ 133, "ERROR" },

	{ -1, NULL }
};

/* the number of parameters per request */
#define HBP_REQ_LOGIN_PARAMS	2

/** @brief Types of requests */
typedef enum {
	/**
	 * @brief Request to start a new session
	 *
	 * A session will automatically time out after #HBP_TIMEOUT seconds, regardless of activity. Loss of connection
	 * or sending more than #HBP_ERROR_MAX invalid requests will also end an active session.
	 * Only one session per connection is possible.
	 *
	 * @param card_id The bank card's unique IDentifier (max. #HBP_CID_MAX + 1 bytes)
	 * @param pin The PIN code associated with the card ID (min. #HBP_PIN_MIN and max. #HBP_PIN_MAX + 1 bytes)
	 *
	 * @todo Have a cooldown period on logins to prevent brute forcing
	 */
	HBP_REQ_LOGIN = 0,

	/**
	 * @brief Request to terminate the current session
	 *
	 * Logout of the current session. After this, a new session can be started again immediately.
	 */
	HBP_REQ_LOGOUT,

	/**
	 * @brief Request for user information associated with the current session
	 *
	 * Not yet implemented
	 * TODO
	 */
	HBP_REQ_INFO,

	/**
	 * @brief Request for the balance of the account associated with the current session
	 *
	 * Not yet implemented
	 * TODO
	 */
	HBP_REQ_BALANCE,

	/**
	 * @brief Request to transfer, withdraw or deposit money from/to the account associated with the current session
	 *
	 * Not yet implemented
	 * TODO
	 */
	HBP_REQ_TRANSFER
} hbp_request_t;

/**
 * @brief Types of replies
 */
typedef enum {
	/**
	 * @brief Reply to a request for a new session
	 *
	 * For the request associated with this reply, see #HBP_REQ_LOGIN
	 *
	 * @param status See #hbp_login_status_t
	 */
	HBP_REP_LOGIN = 128,

	/**
	 * @brief Reply to a request to logout (also sent when the session is about to be logged out of unexpectedly)
	 *
	 * For the request associated with this reply, see #HBP_REQ_LOGOUT
	 *
	 * This reply will also be sent if the session is about to be expired of if too many errornous
	 * This reply may also be sent when either the server is about to be shut down or if the session has expired.
	 *
	 * @param reason See #hbp_term_reason_t
	 */
	HBP_REP_TERMINATED,

	/**
	 * @brief Reply to a request Request for user information associated with the current session
	 *
	 * TODO
	 */
	HBP_REP_INFO,

	/**
	 * @brief Reply to a request request for the balance of the account associated with the current session
	 *
	 * Not yet implemented
	 * TODO
	 */
	HBP_REP_BALANCE,

	/**
	 * @brief Reply to a request to request to transfer, withdraw or deposit money from/to the account associated
	 *        with the current session
	 *
	 * Not yet implemented
	 * TODO
	 */
	HBP_REP_TRANSFER,

	/**
	 * @brief The server couldn't complete the request because of an error, check the server logs
	 *
	 * Instances where this reply may be sent:
	 * - The server is out of memory
	 * - An invalid request has been received
	 */
	HBP_REP_ERROR
} hbp_reply_t;

/** @brief Indicates whether the login failed or succeeded */
typedef enum {
	/** The login was successful */
	HBP_LOGIN_GRANTED,
	/** The card ID or PIN-code is incorrect */
	HBP_LOGIN_DENIED,
	/** This card has been blocked because of a number of invalid logins */
	HBP_LOGIN_BLOCKED
} hbp_login_status_t;

/** @brief Indicates why the session has ended/the server will disconnect */
typedef enum {
	/** The client has request to logout. Logout has succeeded */
	HBP_TERM_LOGOUT,
	/** The current session has expired because #HBP_TIMEOUT has been reached */
	HBP_TERM_EXPIRED,
	/** The server has logged out your session because the server is about to shut down */
	HBP_TERM_CLOSED
} hbp_term_reason_t;

#if 0
/* Account types */
typedef enum {
	/* Checkings account */
	HBP_A_CHECKING,
	/* Savings account */
	HBP_A_SAVINGS
} hbp_account_t;
#endif
