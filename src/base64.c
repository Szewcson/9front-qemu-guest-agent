/* SPDX-License-Identifier: MIT */
#include "base64.h"

#ifdef QGA_HOST_TEST
/* Development-only platform adapter; never linked into the native daemon. */
int qgahostenc64(char*, int, uchar*, int);
int qgahostdec64(uchar*, int, char*, int);
#define enc64 qgahostenc64
#define dec64 qgahostdec64
#endif

static int
digit64(int c)
{
	if(c >= 'A' && c <= 'Z') return c-'A';
	if(c >= 'a' && c <= 'z') return c-'a'+26;
	if(c >= '0' && c <= '9') return c-'0'+52;
	if(c == '+') return 62;
	if(c == '/') return 63;
	return -1;
}

/* libc dec64 skips invalid characters and can return a truncated result.
 * Validate the entire canonical input and exact capacity before it writes. */
int
qgadecode64(const char *s, int size, uchar *data, int capacity)
{
	int i, pad, end, n;

	if(s == nil || data == nil || size < 0 || capacity < 0 || size%4 != 0)
		return -1;
	if(size == 0)
		return 0;
	pad = 0;
	if(s[size-1] == '=') pad++;
	if(s[size-2] == '=') pad++;
	end = size-pad;
	for(i = 0; i < end; i++)
		if(digit64((uchar)s[i]) < 0)
			return -1;
	if((pad == 2 && (digit64((uchar)s[end-1]) & 15) != 0) ||
	   (pad == 1 && (digit64((uchar)s[end-1]) & 3) != 0))
		return -1;
	n = (size/4)*3-pad;
	if(n > capacity)
		return -1;
	return dec64(data, n, (char*)s, size) == n ? n : -1;
}

void
qgaencode64(Jbuf *out, const uchar *data, int size)
{
	int encoded, used;

	if(out == nil || out->failed)
		return;
	if(size < 0 || (data == nil && size != 0) ||
	   size > 3*((QgaOutputMax-2)/4)){
		out->failed = 1;
		return;
	}
	encoded = ((size+2)/3)*4;
	if(encoded+2 > out->size-out->used){
		out->failed = 1;
		return;
	}
	jbchar(out, '"');
	if(size > 0){
		/* The final quote's slot temporarily holds libc's terminating NUL. */
		used = enc64(out->text+out->used, encoded+1, (uchar*)data, size);
		if(used != encoded){
			out->failed = 1;
			return;
		}
		out->used += used;
	}
	jbchar(out, '"');
}
