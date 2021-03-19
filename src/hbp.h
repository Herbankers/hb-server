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

/** @brief Version of the HBP protocol */
#define HBP_VERSION	1
/** @brief Default port on which hb-server is hosted */
#define HBP_PORT	8420


/*
 * Limits
 */

/** @brief Maximum number of erroneous requests before closing the connection */
#define HBP_ERROR_MAX	10
/** @brief Maximum request/reply data length in bytes */
#define HBP_LENGTH_MAX	8192
/** @brief Minimum length for an IBAN (per ISO 13616-1:2007) */
#define HBP_IBAN_MIN	9
/** @brief Maximum length for an IBAN (per ISO 13616-1:2007) */
#define HBP_IBAN_MAX	34
/** @brief Minimum length for a PIN (per ISO 9564-1:2011) */
#define HBP_PIN_MIN	4
/** @brief Maximum length for a PIN (per ISO 9564-1:2011) */
#define HBP_PIN_MAX	12
/** @brief Maximum times PIN entry can be attempted before blocking the card */
#define HBP_PINTRY_MAX	3
/** @brief Session timeout in seconds */
#define HBP_TIMEOUT	(10 * 60)
/** @brief Card UI length in bytes */
#define HBP_UID_MAX	6

/** @brief Requests types */
typedef enum {
	/**
	 * @brief Request to start a new session
	 *
	 * A session will automatically time out after #HBP_TIMEOUT seconds, regardless of activity. Loss of connection
	 * or sending more than #HBP_ERROR_MAX invalid requests will also end an active session.
	 * Only one session per connection is possible.
	 *
	 */
	HBP_REQ_LOGIN,

	/**
	 * @brief Request to terminate the current session
	 *
	 * Logout of the current session. After this, a new session can be started again immediately.
	 */
	HBP_REQ_LOGOUT,

	/**
	 * @brief Not yet implemented
	 *
	 * TODO
	 */
	HBP_REQ_INFO,

	/**
	 * @brief Not yet implemented
	 *
	 * TODO
	 */
	HBP_REQ_BALANCE,

	/**
	 * @brief Not yet implemented
	 *
	 * TODO
	 */
	HBP_REQ_TRANSFER
} hbp_request_t;

/** @brief Reply types */
typedef enum {
	/**
	 * @brief Reply to a request for a new session
	 *
	 * For the request associated with this reply, see #HBP_REQ_LOGIN
	 *
	 * @param type HBP_REP_LOGIN
	 * @param status See #hbp_login_t
	 */
	HBP_REP_LOGIN,

	/**
	 * @brief Reply to a request to logout (also sent when the connection is about to be terminated unexpectedly)
	 *
	 * For the request associated with this reply, see #HBP_REQ_LOGOUT
	 *
	 * This reply may also be sent when either the server is about to be shut down or if the session has expired.
	 *
	 * @param reason See #hbp_term_t
	 */
	HBP_REP_TERMINATED,

	/**
	 * @brief Not yet implemented
	 *
	 * TODO
	 */
	HBP_REP_INFO,

	/**
	 * @brief Not yet implemented
	 *
	 * TODO
	 */
	HBP_REP_BALANCE,

	/**
	 * @brief Not yet implemented
	 *
	 * TODO
	 */
	HBP_REP_TRANSFER
};

/** @brief Indicates whether the login failed or succeeded */
typedef enum {
	/** The login was successful */
	HBP_LOGIN_GRANTED,
	/** The card UID or PIN-code is incorrect */
	HBP_LOGIN_DENIED,
	/** This card has been blocked because of a number of invalid logins */
	HBP_LOGIN_BLOCKED,
	/** The server couldn't complete the request because of an error, check the server logs */
	HBP_LOGIN_ERROR
} hbp_login_t;

/** @brief Indicates why the session has ended */
typedef enum {
	/** The client has request to logout. Logout has succeeded */
	HBP_TERM_LOGOUT,
	/** The current session has expired because #HBP_TIMEOUT has been reached */
	HBP_TERM_EXPIRED,
	/** The server couldn't complete the request because of an error, check the server logs */
	HBP_TERM_ERROR
} hbp_term_t;

#if 0
/* Account types */
typedef enum {
	/* Checkings account */
	HBP_A_CHECKING,
	/* Savings account */
	HBP_A_SAVINGS
} hbp_account_t;
#endif
