/* SPDX-License-Identifier: MIT */
/* Explicit native sampler lifecycle test; reads only kernel process metadata. */
#include "../src/native.h"
#include "../src/json.h"

void
main(void)
{
	void *load;
	Jbuf out;
	char text[256];
	int i;

	load=nativeloadnew();
	if(load==nil) sysfatal("native load sampler: %r");
	for(i=0;i<3;i++){
		jbinit(&out,text,sizeof text-1);
		if(nativeloadquery(load,&out)<0){
			nativeloadfree(load);
			sysfatal("native load snapshot %d: %r",i);
		}
		text[out.used]=0;
		print("native load snapshot: %s\n",text);
		if(i<2) sleep(1100);
	}
	nativeloadfree(load);
	print("native load sampler: startup and three queries passed; stop requested\n");
	exits(nil);
}
