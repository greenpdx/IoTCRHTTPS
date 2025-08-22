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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "cJSON.h"

#include "http_parser.h"

enum HTTP_KV { KEY, VALUE, CONTENT };

static int __parse_response(unsigned char *res_raw, http_response_t *res) {
	int offset = 0, prev_pos = 0;

	// Walk to HTTP/1 <- to get the number
	while (!isdigit(res_raw[offset++]));
	res->http_version[0] = res_raw[offset-1];
	// Check for minor
	if (res_raw[offset] == '.') {
		res->http_version[1] = '.';
		res->http_version[2] = res_raw[++offset];
	}

	// Walk to status code
	while (!isdigit(res_raw[offset++]));
	res->status_code[0] = res_raw[++offset];
	res->status_code[1] = res_raw[++offset];
	res->status_code[2] = res_raw[++offset];
	res->status_code[3] = '\0';

	// Walk to next space
	while (res_raw[offset++] != ' ');
	prev_pos = offset;
	// We've got to the end of the response line
	while (res_raw[offset++] != '\n');

	res_raw[offset - 1] = '\0';
	res->status_text = &res_raw[prev_pos];

	return offset;
}

/**
 * Parse http method, route and version
 * return offset so we don't parse the same line again
 *
 * -1 denotes a failure
 * a positive integer is success
 */
static int __parse_request(unsigned char *req_raw, http_request_t *req) {
	int offset = 0, prev_pos = 0;

	// GET <- walk to this piece of white space
	while (req_raw[offset++] != ' ');
	req_raw[offset - 1] = '\0';
	req->http_method = &req_raw[prev_pos];
	prev_pos = offset;

	// /name-of/path <- walk to this piece of white space
	while(req_raw[offset++] != ' ');
	req_raw[offset - 1] = '\0';
	req->path = &req_raw[prev_pos];
	prev_pos = offset;

	// HTTP/1.1 <- walk to first number
	while (!isdigit(req_raw[offset++]));
	req->http_version[0] = req_raw[offset - 1];
	// Check for minor
	if (req_raw[offset] == '.') {
		req->http_version[1] = '.';
		req->http_version[2] = req_raw[++offset];
	}

	// \r\n
	return offset + 1;
}

static int parseKey(unsigned char *raw, unsigned char *current, headers_kv_t *headers, int offset,
		int idx) {
	// replace : with a null terminator
	raw[offset] = '\0';
	headers[idx].key = current;
	headers[idx].key_len = strlen((char*)current);

	// move past : and the space after
	return offset + 2;
}

static int parseValue(unsigned char *raw, unsigned char **current, headers_kv_t *headers,
		int prev, int offset, int idx) {

	// replace \n with a null terminator
	raw[offset] = '\0';
	*current = &raw[prev];

	headers[idx].value = *current;
	headers[idx].value_len = strlen((char*)*current);

	*current = &raw[offset + 1];

	// move past \n, onto the next line
	return offset + 1;
}

/**
 * HTTP Key: Value 
 *
 * Assigns a pointer to each key and value. Effectively Chopping a buffer
 * into pieces by removing ':' and '\n' placing a '\0' null terminator.
 *
 * Easy to free as you just have to free the buffer that was passed in, if it was malloced.
 *
 * We track two pointers, one to the buffer's position and one to the raw request headers.
 *
 * i.e:
 * |H|o|s|t|:| |d|e|v|e|l|o|p|e|r|\n
 *  ^       ^   ^                  ^
 *  p1      \0  p2                \0
 *
 * Where p1 (a pointer) the address of the key, and p2 (a pointer) is the addess of the value.
 *
 * @buf - temporary buffer to of which pointers to various parts of the buffer will be assigned for the key
 * and for the value;
 * @req_raw raw request
 * @req pointer to the http_request_t object, which is where the key->values will be stored
 */
static int parseHeaders(int offset, unsigned char *req_raw, headers_kv_t *headers) {
	/**
	 * We don't want to parse the first line of the request again as that is:
	 * GET /path/of/thing HTTP/1
	 *
	 * So get the offset and start from there
	 */
	
	int num_headers = 0;
	enum HTTP_KV part = KEY;
	unsigned char *ptr = &req_raw[offset];
	int prev_pos = 0;
	int new_line_count = 0;
	int eol = 0;

	for (;;) {
		offset++;
		if (part == CONTENT) {
			break;
		}

		switch (req_raw[offset]) {
			case '\0':
			case EOF: {
				goto done;
			}

			case '\n':
				if ( eol ) {
					new_line_count++;
					eol = 0;
					prev_pos = parseValue(req_raw, &ptr, headers, prev_pos, offset, num_headers);
					part = KEY;
					num_headers++;
					
				} else 
					eol = 1;
				break;
			case '\r': {
				new_line_count++;

				if (new_line_count == 2) {
					prev_pos = parseValue(req_raw, &ptr, headers, prev_pos, offset, num_headers);
					part = KEY;
					num_headers++;
				} else if (new_line_count >= 3) {
					ptr+=2;
					part = CONTENT;
					headers[num_headers-1].value = ptr;
					headers[num_headers-1].key = (unsigned char*)"body";
				}

				break;
			}

			case ':': {
				if (part == CONTENT) {
					continue;
				}
				new_line_count = 0;
				if (part == KEY) {
					prev_pos = parseKey(req_raw, ptr, headers, offset, num_headers);
					part = VALUE;
				}
				break;
			}

			default:
				continue;
		}
	}

done:
	return num_headers;
}

headers_kv_t *httpFindHeader(headers_kv_t *headers, int num_headers, char *key) {
	for (int i = 0; i < num_headers; ++i) {
		if (strcmp((char*)headers[i].key, key) == 0)
			return &headers[i];	
	}
	return NULL;
}

unsigned char *httpGetResponseBody(http_response_t *res) {
	if (res->num_headers == 0)
		return NULL;

	headers_kv_t header = res->headers[res->num_headers+1];

	if (strcmp((char*)header.key, "body") == 0)
		return header.value;
	return NULL;
}

void httpParseRequest(unsigned char *req_raw, http_request_t *req) {
	int offset = __parse_request(req_raw, req);
	int num_headers = parseHeaders(offset, req_raw, req->headers);
	req->num_headers = num_headers;
}

void httpParseResponse(unsigned char *res_raw, http_response_t *res) {
	int offset = __parse_response(res_raw, res);
	int num_headers = parseHeaders(offset, res_raw, res->headers);
	res->num_headers = num_headers - 2; // body is -1
}
