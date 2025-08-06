/**
 * Version 1.0 April 2021
 *
 * Copyright (c) 2021, James Barford-Evans
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#define MAX_HEADERS 100

typedef struct headers_kv_t {
	unsigned char *key;
	unsigned int key_len;
	unsigned char *value;
	unsigned int value_len;
} headers_kv_t;

typedef struct http_request_t {
	unsigned char *http_method;
	unsigned char *path;
	// http "1", "1.1", "2", "3"
	char http_version[3];
	int num_headers;
	headers_kv_t headers[MAX_HEADERS];
} http_request_t;

typedef struct http_response_t {
	char status_code[4];
	unsigned char *status_text;
	char http_version[3];
	int num_headers;
	headers_kv_t headers[MAX_HEADERS];
} http_response_t;

void httpParseRequest(unsigned char *req_raw, http_request_t *req);
void httpParseResponse(unsigned char *res_raw, http_response_t *res);
headers_kv_t *httpFindHeader(headers_kv_t *headers, int num_headers, char *key);
unsigned char *httpGetResponseBody(http_response_t *res);

#endif
