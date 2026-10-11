/* SPDX-License-Identifier: MIT */
/* Linux-only byte-stream harness. Never installed as qga9. */
#include "../src/qga.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

static long
output(void *context, const uchar *s, long n)
{
	int step;
	long result;

	step = *(int*)context;
	if(n > step)
		n = step;
	do {
		result = write(1, s, (size_t)n);
	} while(result < 0 && errno == EINTR);
	return result;
}

int
main(int argc, char **argv)
{
	Qga *q;
	uchar input[256];
	long n, block = 7, step = 3;
	int limit;
	char *end;

	if(argc > 3)
		return 2;
	if(argc > 1){
		block = strtol(argv[1], &end, 10);
		if(*end != 0 || block < 1 || block > 256)
			return 2;
	}
	if(argc > 2){
		step = strtol(argv[2], &end, 10);
		if(*end != 0 || step < 1 || step > QgaOutputMax)
			return 2;
	}
	limit = (int)step;
	q = qganew(output, &limit);
	if(q == nil)
		return 1;
	for(;;){
		n = read(0, input, (size_t)block);
		if(n < 0 && errno == EINTR)
			continue;
		if(n < 0 || (n > 0 && qgafeed(q, input, (int)n) < 0)){
			qgafree(q);
			return 1;
		}
		if(n == 0)
			break;
	}
	n = qgaend(q);
	qgafree(q);
	return n < 0 ? 1 : 0;
}
