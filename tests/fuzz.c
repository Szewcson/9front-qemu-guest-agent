/* SPDX-License-Identifier: MIT */
/* Optional libFuzzer entry point: tests the same native protocol core. */
#include "../src/qga.h"
#include <stddef.h>
#include <stdint.h>

static long
discard(void *unused, const uchar *s, long n)
{
	(void)unused;
	(void)s;
	return n > 1 ? n/2 : n;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	Qga *q;
	size_t at, step;

	if(size > 1024*1024)
		return 0;
	q = qganew(discard, nil);
	if(q == nil)
		return 0;
	for(at = 0; at < size; at += step){
		step = 1 + (data[at] & 31);
		if(step > size-at)
			step = size-at;
		if(qgafeed(q, data+at, (int)step) < 0)
			break;
	}
	qgaend(q);
	qgafree(q);
	return 0;
}
