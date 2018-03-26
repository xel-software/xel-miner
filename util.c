/*
* Copyright 2010 Jeff Garzik
* Copyright 2012 Luke Dashjr
* Copyright 2012-2014 pooler
* Copyright 2016 sprocket
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#define _GNU_SOURCE

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "miner.h"
#include "elist.h"

#if defined(WIN32)
#include "compat/winansi.h"
#endif

struct thread_q *tq_new(void)
{
	struct thread_q *tq;

	tq = (struct thread_q*) calloc(1, sizeof(*tq));
	if (!tq)
		return NULL;

	INIT_LIST_HEAD(&tq->q);
	pthread_mutex_init(&tq->mutex, NULL);
	pthread_cond_init(&tq->cond, NULL);

	return tq;
}

char* strdupcs(const char* s){
	char *d = malloc (strlen (s) + 1);   // Space for length plus nul
  if (d == NULL) return NULL;          // No memory
  strcpy (d,s);                        // Copy the characters
  return d;
}

void tq_free(struct thread_q *tq) {
	struct tq_ent *ent, *iter;

	if (!tq)
		return;

	list_for_each_entry_safe(ent, iter, &tq->q, q_node, struct tq_ent) {
		list_del(&ent->q_node);
		free(ent);
	}

	pthread_cond_destroy(&tq->cond);
	pthread_mutex_destroy(&tq->mutex);

	memset(tq, 0, sizeof(*tq));	/* poison */
	free(tq);
}

static void tq_freezethaw(struct thread_q *tq, bool frozen) {
	pthread_mutex_lock(&tq->mutex);

	tq->frozen = frozen;

	pthread_cond_signal(&tq->cond);
	pthread_mutex_unlock(&tq->mutex);
}

void tq_freeze(struct thread_q *tq)
{
	tq_freezethaw(tq, true);
}

void tq_thaw(struct thread_q *tq)
{
	tq_freezethaw(tq, false);
}

bool tq_push(struct thread_q *tq, void *data) {
	struct tq_ent *ent;
	bool rc = true;

	ent = (struct tq_ent*) calloc(1, sizeof(*ent));
	if (!ent)
		return false;

	ent->data = data;
	INIT_LIST_HEAD(&ent->q_node);

	pthread_mutex_lock(&tq->mutex);

	if (!tq->frozen) {
		list_add_tail(&ent->q_node, &tq->q);
	}
	else {
		free(ent);
		rc = false;
	}

	pthread_cond_signal(&tq->cond);
	pthread_mutex_unlock(&tq->mutex);

	return rc;
}

void *tq_pop(struct thread_q *tq, const struct timespec *abstime) {
	struct tq_ent *ent;
	void *rval = NULL;
	int rc;

	pthread_mutex_lock(&tq->mutex);

	if (!list_empty(&tq->q))
		goto pop;

	if (abstime)
		rc = pthread_cond_timedwait(&tq->cond, &tq->mutex, abstime);
	else
		rc = pthread_cond_wait(&tq->cond, &tq->mutex);
	if (rc)
		goto out;
	if (list_empty(&tq->q))
		goto out;

pop:
	ent = list_entry(tq->q.next, struct tq_ent, q_node);
	rval = ent->data;

	list_del(&ent->q_node);
	free(ent);

out:
	pthread_mutex_unlock(&tq->mutex);
	return rval;
}

void *tq_pop_nowait(struct thread_q *tq) {
	struct tq_ent *ent;
	void *rval = NULL;

	pthread_mutex_lock(&tq->mutex);

	if (!list_empty(&tq->q))
		goto pop;
	else
		goto out;

pop:
	ent = list_entry(tq->q.next, struct tq_ent, q_node);
	rval = ent->data;

	list_del(&ent->q_node);
	free(ent);

out:
	pthread_mutex_unlock(&tq->mutex);
	return rval;
}

/* Subtract the `struct timeval' values X and Y,
storing the result in RESULT.
Return 1 if the difference is negative, otherwise 0.  */
int timeval_subtract(struct timeval *result, struct timeval *x,	struct timeval *y) {
	/* Perform the carry for the later subtraction by updating Y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	* `tv_usec' is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

extern bool bin2hex(unsigned char *in, int in_sz, unsigned char *out, int out_sz) {
	int i;
	const char * hex = "0123456789ABCDEF";
	unsigned char *pin = in, *pout = out;

	if (in_sz <= 0 || (out_sz <= (in_sz * 2))) {
		applog(LOG_ERR, "ERROR: Can't convert array of %d bytes into %d byte hex string", in_sz, out_sz);
		return false;
	}

	for (i = 0; i < in_sz; i++) {
			pout[0] = hex[(*pin >> 4) & 0xF];
			pout[1] = hex[*pin & 0xF];
			pout += 2;
			pin++;
	}
	pout[0] = 0;
	return true;
}

extern bool ints2hex(uint32_t *in, int num, unsigned char *out, int out_sz) {
	int i;
	const char * hex = "0123456789ABCDEF";
//	unsigned char *pin, *pout = out;
	unsigned char *pout = out;

	if (num <= 0 || (out_sz <= (num * 8))) {
		applog(LOG_ERR, "ERROR: Can't convert array of %d ints into %d byte hex string", num, out_sz);
		return false;
	}

	for (i = 0; i < num; i++) {
		sprintf(pout, "%08X", in[i]);
		pout += 8;
	}

	// Alternate Method
	//for (i = 0; i < num; i++) {
	//	// Start At End Of Int (To Reverse Bytes)
	//	pin = ((unsigned char *)&in[i]) + 3;
	//	for (j = 0; j < 4; j++) {
	//		pout[0] = hex[(*pin >> 4) & 0xF];
	//		pout[1] = hex[*pin & 0xF];
	//		pout += 2;
	//		pin--;
	//	}
	//}
	//pout[0] = 0;

	return true;
}

extern bool hex2ints(uint32_t *p, int array_sz, const char *hex, int len) {
	int i, j, idx;
	unsigned char val;
	unsigned char *c = (unsigned char*)p;
	char hex_char[3];
	char *ep;

	if (array_sz <= 0 || len <= 0 || len > (8 * array_sz)) {
		applog(LOG_ERR, "ERROR: Can't convert %d byte hex string to array of %d ints", len, array_sz);
		return false;
	}

	hex_char[2] = '\0';
	idx = len - 1;

	for (i = array_sz - 1; i >= 0; i--) {
		for (j = 0; j < 4; j++) {
			val = 0;
			if (idx == 0) {
				hex_char[1] = hex[idx--];
				hex_char[0] = '0';
				val = (unsigned char)strtol(hex_char, &ep, 16);
			}
			else if (idx > 0) {
				hex_char[1] = hex[idx--];
				hex_char[0] = hex[idx--];
				val = (unsigned char)strtol(hex_char, &ep, 16);
			}
			c[(i*4) + j] = val;
		}
	}
	return true;
}

extern int32_t bin2int(unsigned char *str) {
	int i, len;
	unsigned char *c = (unsigned char*)str;
	uint32_t bin = 0;

	len = strlen(str);

	if (len < 1 || len > 32) {
		applog(LOG_ERR, "ERROR: Can't convert '%s' bin string to int", bin);
		return 0;
	}

	for (i = 0; i < len; i++) {
		if (c[i] == '1')
			bin |= (1 << (len - i - 1));
	}

	return (int32_t)bin;
}

extern bool ascii85dec(unsigned char *str, int strsz, const char *ascii85) {
	int i, j, num_chars;
	int bufsz = 0, value = 0, idx_asc = 0, idx_buf = 0;
	unsigned char chunk[5], *buf, *val = (unsigned char *)&value;

	num_chars = strlen(ascii85);

	// Calculate Buffer Size
	for (i = 0; i<num_chars; i++) {
		if (ascii85[i] == 'z') {
			value += 5;
		}
		else if ((ascii85[i] >= '!') && (ascii85[i] <= 'u')) {
			value += 1;
		}
		else {
			applog(LOG_ERR, "Invalid characters in ASCII85 payload: %s", ascii85);
			return false;
		}
	}

	bufsz = (int)((value * 4) / 5) + 1;

	if (strsz <= bufsz) {
		applog(LOG_ERR, "ERROR: Insufficient string buffer for ASCII85 payload.  String Bytes: %d, Payload Bytes Needed: %d", strsz, bufsz + 1);
		return false;
	}

	buf = calloc((bufsz + 4), sizeof(char));
	if (!buf) {
		applog(LOG_ERR, "ERROR: Unable to allocate ASCII85 payload buffer");
		return false;
	}

	// Decode Data Into Buffer
	while (idx_asc < num_chars) {
		if (ascii85[idx_asc] == 'z') {
			memset(chunk, '!', 5);
			idx_asc++;
		}
		else {
			memcpy(chunk, &ascii85[idx_asc], 5);
			idx_asc += 5;

			if (idx_asc > num_chars) {
				for (j = 0; j<(idx_asc - num_chars); j++)
					chunk[4 - j] = 'u'; // Pad extra chunk bytes with 'u'
			}
		}

		value = 0;
		value += (chunk[0] - '!') * BASE85_POW[4];
		value += (chunk[1] - '!') * BASE85_POW[3];
		value += (chunk[2] - '!') * BASE85_POW[2];
		value += (chunk[3] - '!') * BASE85_POW[1];
		value += (chunk[4] - '!') * BASE85_POW[0];

		buf[idx_buf++] = tolower(val[3]);
		buf[idx_buf++] = tolower(val[2]);
		buf[idx_buf++] = tolower(val[1]);
		buf[idx_buf++] = tolower(val[0]);
	}

	buf[bufsz-1] = '\0'; // Terminate The String
	sprintf(str, "%s", buf);
	free(buf);

	return true;
}

static void databuf_free(struct data_buffer *db)
{
	if (!db)
		return;

	free(db->buf);

	memset(db, 0, sizeof(*db));
}

static size_t all_data_cb(const void *ptr, size_t size, size_t nmemb, void *user_data)
{
	struct data_buffer *db = (struct data_buffer *) user_data;
	size_t len = size * nmemb;
	size_t oldlen, newlen;
	void *newmem;
	static const unsigned char zero = 0;

	oldlen = db->len;
	newlen = oldlen + len;

	newmem = realloc(db->buf, newlen + 1);
	if (!newmem)
		return 0;

	db->buf = newmem;
	db->len = newlen;
	memcpy((unsigned char*)db->buf + oldlen, ptr, len);
	memcpy((unsigned char*)db->buf + newlen, &zero, 1);	/* null terminate */

	return len;
}

json_t* json_rpc_call(CURL *curl, const char *url, const char *userpass, const char *req, int *curl_err) {
	json_t *val = NULL;
	int rc;
	struct data_buffer all_data = { 0 };
	json_error_t err;
	char curl_err_str[CURL_ERROR_SIZE] = { 0 };

	*curl_err = 0;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "");
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, all_data_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &all_data);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_err_str);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt_timeout);
	if (userpass) {
		curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
	}

	if (opt_protocol)
		applog(LOG_DEBUG, "DEBUG: RPC Request - %s", req);

	rc = curl_easy_perform(curl);
	if (rc) {
		applog(LOG_ERR, "ERROR: Curl - '%s' (code=%d)", curl_err_str, rc);
		*curl_err = rc;
		goto err_out;
	}

	if (!all_data.buf) {
		applog(LOG_ERR, "ERROR: Curl did not return any data in 'json_rpc_call'");
		*curl_err = -1;
		goto err_out;
	}

	val = JSON_LOADS(all_data.buf, &err);
	if (!val) {
		applog(LOG_ERR, "ERROR: JSON decode failed (code=%d): %s", err.line, err.text);
		*curl_err = -2;
		goto err_out;
	}

	databuf_free(&all_data);
	curl_easy_reset(curl);
	return val;

err_out:
	databuf_free(&all_data);
	if (val) json_decref(val);
	curl_easy_reset(curl);
	return NULL;
}

extern void applog(int prio, const char *fmt, ...) {
	if (!opt_debug && prio == LOG_DEBUG)
		return;

	if (opt_quiet && prio == LOG_INFO)
		return;

	va_list ap;

	va_start(ap, fmt);

	const char* color = "";
	char *f;
	int len;
	time_t now;
	struct tm tm, *tm_p;

	time(&now);

	pthread_mutex_lock(&applog_lock);
	tm_p = localtime(&now);
	memcpy(&tm, tm_p, sizeof(struct tm));
	pthread_mutex_unlock(&applog_lock);

	switch (prio) {
	case LOG_ERR:     color = CL_RED; break;
	case LOG_WARNING: color = CL_YLW; break;
	case LOG_NOTICE:  color = CL_WHT; break;
	case LOG_INFO:    color = ""; break;
	case LOG_DEBUG:   color = CL_GRY; break;

	case LOG_BLUE:
		prio = LOG_NOTICE;
		color = CL_CYN;
		break;
	}
	if (!use_colors)
		color = "";

	len = 64 + (int)strlen(fmt) + 2;
	f = (char*)malloc(len);
	sprintf(f, "[%02d:%02d:%02d]%s %s%s\n",
		tm.tm_hour,
		tm.tm_min,
		tm.tm_sec,
		color,
		fmt,
		use_colors ? CL_N : ""
	);
	pthread_mutex_lock(&applog_lock);
	vfprintf(stdout, f, ap);	/* atomic write to stdout */

	fflush(stdout);
	free(f);
	pthread_mutex_unlock(&applog_lock);

	va_end(ap);
}













/*
A C-program for MT19937, with initialization improved 2002/1/26.
Coded by Takuji Nishimura and Makoto Matsumoto.

Before using, initialize the state by using init_genrand(seed)
or init_by_array(init_key, key_length).

Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. The names of its contributors may not be used to endorse or promote
products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Any feedback is very welcome.
http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

#include <stdio.h>

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

static unsigned long mt[N]; /* the array for the state vector  */
static int mti = N + 1; /* mti==N+1 means mt[N] is not initialized */

						/* initializes mt[N] with a seed */
void init_genrand(unsigned long s)
{
	mt[0] = s & 0xffffffffUL;
	for (mti = 1; mti<N; mti++) {
		mt[mti] =
			(1812433253UL * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti);
		/* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
		/* In the previous versions, MSBs of the seed affect   */
		/* only MSBs of the array mt[].                        */
		/* 2002/01/09 modified by Makoto Matsumoto             */
		mt[mti] &= 0xffffffffUL;
		/* for >32 bit machines */
	}
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
void init_by_array(unsigned long init_key[], int key_length)
{
	int i, j, k;
	init_genrand(19650218UL);
	i = 1; j = 0;
	k = (N>key_length ? N : key_length);
	for (; k; k--) {
		mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525UL))
			+ init_key[j] + j; /* non linear */
		mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
		i++; j++;
		if (i >= N) { mt[0] = mt[N - 1]; i = 1; }
		if (j >= key_length) j = 0;
	}
	for (k = N - 1; k; k--) {
		mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941UL))
			- i; /* non linear */
		mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
		i++;
		if (i >= N) { mt[0] = mt[N - 1]; i = 1; }
	}

	mt[0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */
}

/* generates a random number on [0,0xffffffff]-interval */
extern unsigned long genrand_int32(void)
{
	unsigned long y;
	static unsigned long mag01[2] = { 0x0UL, MATRIX_A };
	/* mag01[x] = x * MATRIX_A  for x=0,1 */

	if (mti >= N) { /* generate N words at one time */
		int kk;

		if (mti == N + 1)   /* if init_genrand() has not been called, */
			init_genrand(5489UL); /* a default initial seed is used */

		for (kk = 0; kk<N - M; kk++) {
			y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
			mt[kk] = mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		for (; kk<N - 1; kk++) {
			y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
			mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
		mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

		mti = 0;
	}

	y = mt[mti++];

	/* Tempering */
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);

	return y;
}

/* generates a random number on [0,0x7fffffff]-interval */
long genrand_int31(void)
{
	return (long)(genrand_int32() >> 1);
}

/* generates a random number on [0,1]-real-interval */
double genrand_real1(void)
{
	return genrand_int32()*(1.0 / 4294967295.0);
	/* divided by 2^32-1 */
}

/* generates a random number on [0,1)-real-interval */
double genrand_real2(void)
{
	return genrand_int32()*(1.0 / 4294967296.0);
	/* divided by 2^32 */
}

/* generates a random number on (0,1)-real-interval */
double genrand_real3(void)
{
	return (((double)genrand_int32()) + 0.5)*(1.0 / 4294967296.0);
	/* divided by 2^32 */
}

/* generates a random number on [0,1) with 53-bit resolution*/
double genrand_res53(void)
{
	unsigned long a = genrand_int32() >> 5, b = genrand_int32() >> 6;
	return(a*67108864.0 + b)*(1.0 / 9007199254740992.0);
}
/* These real versions are due to Isaku Wada, 2002/01/09 added */
