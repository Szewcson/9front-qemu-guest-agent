/* SPDX-License-Identifier: MIT */
/* Linux development adapter: existing libresolv codecs, not a second codec.
 * Native mk builds use 9front libc directly and never compile this file. */
#include "../src/qga.h"
#include <resolv.h>

int
qgahostenc64(char *out, int capacity, uchar *data, int size)
{
	if(out == nil || data == nil || capacity <= 0 || size < 0)
		return -1;
	return b64_ntop(data, size, out, capacity);
}

int
qgahostdec64(uchar *out, int capacity, char *text, int size)
{
	char word[5];
	uchar data[3];
	int i, n, used;

	if(out == nil || text == nil || size < 0 || size%4 != 0 || capacity < 0)
		return -1;
	used = 0;
	for(i = 0; i < size; i += 4){
		memmove(word, text+i, 4);
		word[4] = 0;
		n = b64_pton(word, data, sizeof data);
		if(n < 0 || n > capacity-used)
			return -1;
		memmove(out+used, data, n);
		used += n;
	}
	return used;
}
