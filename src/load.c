/* SPDX-License-Identifier: MIT */
#include "load.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#define snprint snprintf
#endif

void qgaloadinit(QgaLoad *load) { memset(load, 0, sizeof *load); }

int
qgaloadsample(QgaLoad *load, int runnable, QgaUint64 now)
{
	static const QgaUint64 decay[] = {983471454ULL, 996672216ULL, 998889506ULL};
	QgaUint64 elapsed, value;
	int steps, i;
	if(load == nil || runnable < 0 || runnable > QgaLoadProcessesMax || now > 0xffffffffULL) return -1;
	if(!load->initialized){ load->last=now; load->initialized=1; return 0; }
	elapsed=(now-load->last)&0xffffffffULL;
	/* Do not invent samples through long stalls or a clock/source reset. */
	if(elapsed > 3000){ qgaloadinit(load); return -1; }
	load->pending += elapsed; load->last=now;
	steps=load->pending/1000; load->pending%=1000;
	value=(QgaUint64)runnable*QgaLoadScale;
	while(steps-- > 0) for(i=0; i<3; i++)
		/* 4096*10^6*10^9 plus rounding fits unsigned and signed int64. */
		load->average[i]=(load->average[i]*decay[i]+value*(1000000000ULL-decay[i])+500000000ULL)/1000000000ULL;
	return 0;
}

int
qgaloademit(Jbuf *b, const QgaUint64 *value)
{
	static const char *names[] = {"load1m", "load5m", "load15m"};
	char fraction[7];
	int i, n;
	if(b == nil || value == nil) return -1;
	for(i=0; i<3; i++) if(value[i] > (QgaUint64)QgaLoadProcessesMax*QgaLoadScale) return -1;
	jbchar(b,'{');
	for(i=0; i<3; i++){
		if(i) jbchar(b,',');
		jbquote(b,names[i]); jbchar(b,':');
		jbint64(b,value[i]/QgaLoadScale); jbchar(b,'.');
		n=snprint(fraction,sizeof fraction,"%06lld",(QgaInt64)(value[i]%QgaLoadScale));
		if(n!=6) return -1;
		jbraw(b,fraction,n);
	}
	jbchar(b,'}'); return b->failed ? -1 : 0;
}

int
qgaloadstate(const char *text, int size)
{
	int end;
	/* Native devproc: %-27s program, %-27s user, then state. Names exceeding
	 * that inspected layout are refused rather than shifting state parsing. */
	if(text == nil || size < 69 || text[27]!=' ' || text[55]!=' ') return -1;
	for(end=56; end<size && text[end]!=' '; end++)
		if((uchar)text[end] < 33 || (uchar)text[end] > 126) return -1;
	if(end==56 || end==size) return -1;
	return (end==61 && memcmp((void*)(text+56),"Ready",5)==0) ||
		(end==63 && memcmp((void*)(text+56),"Running",7)==0);
}
