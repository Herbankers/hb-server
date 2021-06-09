/*
 *
 * hb-server
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

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "hbp.h"
#include "herbank.h"

/* static const char *key = "../../ssl/private/congo-server.key";
static const char *cert = "../../ssl/certs/congo-server-chain.crt";
static const char *ca = "../../ssl/certs/congo-ca-chain.crt"; */
static const char *key =  "/Users/bastiaan/Documents/hr/prj34/ssl/private/congo-server.key";
static const char *cert = "/Users/bastiaan/Documents/hr/prj34/ssl/certs/congo-server-chain.crt";
static const char *ca =   "/Users/bastiaan/Documents/hr/prj34/ssl/certs/congo-ca-chain.crt";

static size_t write_buffer(void *buf, size_t size, size_t nmemb, void *userp)
{
	char *reply = (char *) userp;
	size_t len = size * nmemb;

	if (len > BUF_SIZE)
		return len;

	memcpy(reply, buf, len);
	reply[len] = '\0';

	return len;
}

static void append_country(char *buf, const char *iban)
{
	char country[3];

	/* extract the 2-letter country code from the provided IBAN and append it to the buffer */
	strncpy(country, iban, 2);
	/* strcat(buf, country); */
	strcat(buf, "T3");
}

static void append_bank(char *buf, const char *iban)
{
	char bank[5];

	/* extract the 4-letter bank code from the provided IBAN and append it to the buffer */
	strncpy(bank, iban + 4, 4);
	strcat(buf, bank);
}

long noob_request(char *buf, const char *endpoint, const char *_iban, const char *pin, const char *extraparams)
{
	CURL *curl;
	CURLcode res;
	long http_res;
	struct curl_slist *header = NULL;
	char url[128];
	char iban[17];
	char inbuf[BUF_SIZE + 1];

	/* strip the last 2 characters of the IBAN bcs the other groups don't support normal IBANs */
	memcpy(iban, _iban, 16);
	iban[16] = '\0';

	/* yes could've used snprintf, but what evvvvvv */

	/* construct the header */
	strcpy(inbuf, "{ \"header\": { \"receiveCountry\": \"");
	append_country(inbuf, iban);
	strcat(inbuf, "\", \"receiveBankName\": \"");
	append_bank(inbuf, iban);

	/* construct the body */
	strcat(inbuf, "\" }, \"body\": { \"iban\": \"");
	strcat(inbuf, iban);
	strcat(inbuf, "\", \"pin\": \"");
	strcat(inbuf, pin);
	strcat(inbuf, "\"");
	if (extraparams)
		strcat(inbuf, extraparams);
	strcat(inbuf, " } }");

	/* initialize curl */
	curl_global_init(CURL_GLOBAL_ALL);
	if (!(curl = curl_easy_init())) {
		curl_global_cleanup();
		return -1;
	}

	/* load in the URL + the provided endpoint */
	strcpy(url, "https://145.24.222.242:5443/");
	strcat(url, endpoint);
	curl_easy_setopt(curl, CURLOPT_URL, url);

	/* load in the certificate, private key and CA */
	curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
	curl_easy_setopt(curl, CURLOPT_SSLCERT, cert);
	curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
	curl_easy_setopt(curl, CURLOPT_SSLKEY, key);
	curl_easy_setopt(curl, CURLOPT_CAINFO, ca);

	/* don't verify the authenticity of the connection, because the certificate are incorrect */
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);

	/* set the Content-Type */
	header = curl_slist_append(header, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

	/* include the JSON data */
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, inbuf);

	/* write to our output buffer */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_buffer);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %d\n", res);

		curl_easy_cleanup(curl);
		curl_global_cleanup();

		return -1;
	}

	/* save the HTTP status code */
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_res);

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	return http_res;
}
