/* SPDX-License-Identifier: MIT */
#include "../src/load.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#define USED(value) ((void)(value))
#endif
static int checks;
static void check(int ok, char *why)
{
	checks++; if(ok) return;
#ifdef QGA_HOST_TEST
	fprintf(stderr,"FAIL: %s\n",why); exit(1);
#else
	sysfatal("FAIL: %s",why);
#endif
}
typedef struct Sink Sink;
struct Sink { char text[1024]; int used; };
static long capture(void *context, const uchar *text, long n)
{
	Sink *s=context;
	if(n<0 || n>=(int)sizeof s->text-s->used) return -1;
	memmove(s->text+s->used,(void*)text,n); s->used+=n; s->text[s->used]=0; return n;
}
static int query(void *context, Jbuf *b) { return qgaloademit(b,context); }
static int failed(void *context, Jbuf *b) { USED(context); jbtext(b,"broken"); return -1; }
#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	QgaLoad load;
	QgaUint64 values[3]={1000000,500000,123456};
	Qga *q;
	Sink sink;
	Jbuf b;
	char text[256], row[180];
	int i;
	qgaloadinit(&load);
	check(qgaloadsample(&load,1,0)==0,"initial sample");
	for(i=1;i<=60;i++) check(qgaloadsample(&load,1,i*1000)==0,"continuous one-minute samples");
	check(load.average[0]>632000 && load.average[0]<633000,"one-minute exponential window");
	check(load.average[1]>181000 && load.average[1]<182000,"five-minute exponential window");
	check(load.average[2]>64000 && load.average[2]<65000,"fifteen-minute exponential window");
	for(i=61;i<=960;i++) check(qgaloadsample(&load,0,i*1000)==0,"idle decay");
	/* Nearest fixed-point rounding leaves <=30 millionths at the 60s window. */
	check(load.average[0]<=31 && load.average[1]<10000,"decay within quantization error");
	check(qgaloadsample(&load,4097,961000)<0,"reject excessive count");
	check(qgaloadsample(&load,1,966000)<0 && !load.initialized,"do not invent missing samples");
	qgaloadinit(&load); check(qgaloadsample(&load,4096,0xfffffff0ULL)==0,"wrap initial");
	check(qgaloadsample(&load,4096,984)==0 && load.average[0]>0,"monotonic 32-bit wrap");
	for(i=1;i<3600;i++) check(qgaloadsample(&load,4096,(984+i*1000)&0xffffffffULL)==0,"maximum bounded arithmetic");
	jbinit(&b,text,sizeof text); check(qgaloademit(&b,values)==0,"JSON float-number serialization");
	text[b.used]=0; check(strcmp(text,"{\"load1m\":1.000000,\"load5m\":0.500000,\"load15m\":0.123456}")==0,"exact six-decimal output");
	jbinit(&b,text,4); check(qgaloademit(&b,values)<0 && b.failed,"bounded output");
	memset(row,' ',sizeof row); memmove(row,"program name",12); memmove(row+28,"glenda",6);
	memmove(row+56,"Ready",5); row[sizeof row-1]=0;
	check(qgaloadstate(row,sizeof row-1)==1,"runnable with spaces in program name");
	memmove(row+56,"Running",7); check(qgaloadstate(row,sizeof row-1)==1,"running process");
	memmove(row+56,"Stopped",7); check(qgaloadstate(row,sizeof row-1)==0,"other state");
	row[27]='x'; check(qgaloadstate(row,sizeof row-1)<0,"refuse shifted layout");
	memset(&sink,0,sizeof sink); q=qganew(capture,&sink); check(q!=nil,"protocol allocation");
	qgaloadops(q,query,values);
	strcpy(text,"{\"execute\":\"guest-get-load\",\"id\":9007199254740993}\n");
	check(qgafeed(q,(uchar*)text,strlen(text))==0,"wire load query");
	check(strstr(sink.text,"\"load1m\":1.000000")!=nil && strstr(sink.text,"\"id\":9007199254740993")!=nil,"load plus exact transaction ID");
	sink.used=0; strcpy(text,"{\"execute\":\"guest-get-load\",\"arguments\":{\"x\":1}}\n");
	check(qgafeed(q,(uchar*)text,strlen(text))==0 && strstr(sink.text,"GenericError")!=nil,"reject arguments");
	sink.used=0; qgaloadops(q,failed,nil); strcpy(text,"{\"execute\":\"guest-get-load\"}\n");
	check(qgafeed(q,(uchar*)text,strlen(text))==0 && strstr(sink.text,"GenericError")!=nil && strstr(sink.text,"broken")==nil,"discard partial failure");
	qgafree(q);
#ifdef QGA_HOST_TEST
	printf("load estimator/protocol: %d checks passed\n",checks); return 0;
#else
	print("load estimator/protocol: %d checks passed\n",checks); exits(nil);
#endif
}
