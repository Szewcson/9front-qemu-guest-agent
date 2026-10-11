/* SPDX-License-Identifier: MIT */
#include "base64.h"

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

/* Canonical padded RFC 4648; bounded destination, no ignored suffixes. */
int
qgadecode64(const char *s, int size, uchar *data, int capacity)
{
	int i, n, a, b, c, d;

	if(s == nil || data == nil || size < 0 || capacity < 0 || size%4 != 0)
		return -1;
	n = 0;
	for(i = 0; i < size; i += 4){
		a = digit64((uchar)s[i]);
		b = digit64((uchar)s[i+1]);
		c = digit64((uchar)s[i+2]);
		d = digit64((uchar)s[i+3]);
		if(a < 0 || b < 0 || n == capacity) return -1;
		data[n++] = (a << 2) | (b >> 4);
		if(s[i+2] == '='){
			if(i+4 != size || s[i+3] != '=' || (b & 15) != 0) return -1;
			break;
		}
		if(c < 0 || n == capacity) return -1;
		data[n++] = (b << 4) | (c >> 2);
		if(s[i+3] == '='){
			if(i+4 != size || (c & 3) != 0) return -1;
			break;
		}
		if(d < 0 || n == capacity) return -1;
		data[n++] = (c << 6) | d;
	}
	return n;
}

void
qgaencode64(Jbuf *out, const uchar *data, int size)
{
	const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int i, a, b, c;

	jbchar(out, '"');
	for(i = 0; i < size; i += 3){
		a = data[i];
		b = i+1 < size ? data[i+1] : 0;
		c = i+2 < size ? data[i+2] : 0;
		jbchar(out, alphabet[a >> 2]);
		jbchar(out, alphabet[((a & 3) << 4) | (b >> 4)]);
		jbchar(out, i+1 < size ? alphabet[((b & 15) << 2) | (c >> 6)] : '=');
		jbchar(out, i+2 < size ? alphabet[c & 63] : '=');
	}
	jbchar(out, '"');
}
