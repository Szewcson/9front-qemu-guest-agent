/* SPDX-License-Identifier: MIT */
/* Private child table: exercise multi-chunk #d cleanup without touching parent. */
#include "../src/native.h"

void
main(void)
{
	Dir *d;
	Waitmsg *w;
	int extra[80], channel[2], keep[5], i, pid;
	char result;

	for(i = 0; i < nelem(extra); i++){
		extra[i] = open("/dev/null", OREAD);
		if(extra[i] < 0)
			sysfatal("fixture descriptor: %r");
	}
	if(pipe(channel) < 0)
		sysfatal("fixture pipe: %r");
	pid = rfork(RFPROC|RFFDG);
	if(pid < 0)
		sysfatal("fixture fork: %r");
	if(pid == 0){
		keep[0] = 0;
		keep[1] = 1;
		keep[2] = 2;
		keep[3] = extra[nelem(extra)-1];
		keep[4] = channel[1];
		if(nativecloseexcept(nil, 1) != -1 || nativecloseexcept(keep, 9) != -1 ||
		   nativecloseexcept(keep, -1) != -1 || nativecloseexcept(keep, nelem(keep)) < 0)
			exits("cleanup failed");
		for(i = 0; i < nelem(extra); i++){
			d = dirfstat(extra[i]);
			if((i == nelem(extra)-1) != (d != nil)){
				free(d);
				exits("wrong descriptor retained");
			}
			free(d);
		}
		result = 'Y';
		if(write(channel[1], &result, 1) != 1)
			exits("kept channel failed");
		exits(nil);
	}
	close(channel[1]);
	if(read(channel[0], &result, 1) != 1 || result != 'Y')
		sysfatal("child cleanup response failed");
	close(channel[0]);
	w = wait();
	if(w == nil || w->pid != pid || w->msg[0] != 0)
		sysfatal("cleanup child failed");
	free(w);
	for(i = 0; i < nelem(extra); i++){
		d = dirfstat(extra[i]);
		if(d == nil)
			sysfatal("parent descriptor unexpectedly closed");
		free(d);
		close(extra[i]);
	}
	print("native descriptor cleanup: 80 extras, exact keep set and private parent table passed\n");
	exits(nil);
}
