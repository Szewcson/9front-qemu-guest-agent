/* SPDX-License-Identifier: MIT */
/* One private sampler process; no shared protocol state, FD table or memory. */
#include "native.h"
#include "load.h"

typedef struct NativeLoad NativeLoad;
struct NativeLoad { int fd; };

static int
loadtext(char *path, char *text, int size)
{
	int fd, n, used;
	char extra;
	fd=open(path,OREAD|OCEXEC); if(fd<0) return -2;
	used=0;
	while(used<size-1){
		n=read(fd,text+used,size-1-used);
		if(n<0){ close(fd); return -1; }
		if(n==0) break;
		used+=n;
	}
	n=read(fd,&extra,1); close(fd);
	if(n!=0 || memchr(text,0,used)!=nil) return -1;
	text[used]=0; return used;
}

static int
runnable(int self)
{
	Dir *entries;
	char path[64], text[512], *end;
	long id;
	int fd, n, i, count, scanned, result, state;
	fd=open("#p",OREAD|OCEXEC); if(fd<0) return -1;
	count=scanned=0; result=0;
	while((n=dirread(fd,&entries))>0){
		for(i=0;i<n;i++){
			if(++scanned>QgaLoadProcessesMax+16){ result=-1; break; }
			if(entries[i].name[0]<'0' || entries[i].name[0]>'9') continue;
			id=strtol(entries[i].name,&end,10);
			if(*end!=0 || id<1 || id>2147483647L){ result=-1; break; }
			if(id==self) continue; /* do not count the observer on every sample */
			snprint(path,sizeof path,"#p/%ld/status",id);
			state=loadtext(path,text,sizeof text);
			if(state==-2) continue; /* a process may exit during the snapshot */
			if(state<0 || (state=qgaloadstate(text,state))<0){ result=-1; break; }
			count+=state;
		}
		free(entries); if(result<0) break;
	}
	close(fd); return n<0 || result<0 ? -1 : count;
}

static int
loadmillis(QgaUint64 *out)
{
	char text[256], *field[7], *end;
	uvlong value;
	int i, n;
	n=loadtext("#c/cputime",text,sizeof text);
	if(n<1 || tokenize(text,field,nelem(field))!=6) return -1;
	for(i=0;field[2][i]!=0;i++) if(field[2][i]<'0' || field[2][i]>'9') return -1;
	if(i<1 || i>10) return -1;
	value=strtoull(field[2],&end,10);
	if(*end || value>0xffffffffULL) return -1;
	*out=value; return 0;
}

static void
loadworker(int fd, int owner)
{
	QgaLoad model;
	Dir *d;
	uchar packet[QgaLoadWire], command;
	char status[512];
	QgaUint64 now, value;
	int keep[2], valid, count, pending, i, j, self;
	keep[0]=fd; keep[1]=owner;
	if(nativecloseexcept(keep,2)<0) _exits("load descriptors");
	notify(nil); self=getpid(); qgaloadinit(&model);
	if(write(fd,"H",1)!=1) _exits("load startup");
	for(;;){
		if(pread(owner,status,sizeof status,0)<=0) _exits(nil);
		count=runnable(self);
		valid=count>=0 && loadmillis(&now)==0 && qgaloadsample(&model,count,now)==0;
		if(!valid) qgaloadinit(&model);
		d=dirfstat(fd); if(d==nil) _exits("load channel");
		if(d->length<0 || d->length>1){ free(d); _exits("load framing"); }
		pending=d->length; free(d);
		if(pending==1){
			if(read(fd,&command,1)!=1 || command=='Q') _exits(nil);
			if(command!='L') _exits("load request");
			memset(packet,0,sizeof packet); packet[0]=valid;
			for(i=0;i<3;i++){
				value=model.average[i];
				for(j=0;j<8;j++) packet[1+i*8+j]=value>>(56-j*8);
			}
			if(write(fd,packet,sizeof packet)!=sizeof packet) _exits("load reply");
		}
		if(sleep(1000)<0) _exits(nil);
	}
}

void*
nativeloadnew(void)
{
	NativeLoad *load;
	char path[64], ack;
	int owner, pipefd[2], pid;
	load=malloc(sizeof *load); if(load==nil) return nil;
	snprint(path,sizeof path,"#p/%d/status",getpid());
	owner=open(path,OREAD|OCEXEC);
	if(owner<0){ free(load); return nil; }
	if(pipe(pipefd)<0){ close(owner); free(load); return nil; }
	pid=rfork(RFPROC|RFFDG|RFNAMEG|RFNOTEG|RFNOWAIT);
	if(pid==0) loadworker(pipefd[1],owner);
	close(owner); close(pipefd[1]);
	if(pid<0 || read(pipefd[0],&ack,1)!=1 || ack!='H'){
		if(pid>0) write(pipefd[0],"Q",1);
		close(pipefd[0]); free(load); return nil;
	}
	load->fd=pipefd[0]; return load;
}

int
nativeloadquery(void *context, Jbuf *out)
{
	NativeLoad *load;
	uchar packet[QgaLoadWire];
	QgaUint64 value[3];
	int i,j;
	load=context;
	if(load==nil || write(load->fd,"L",1)!=1 ||
	   readn(load->fd,packet,sizeof packet)!=sizeof packet || packet[0]!=1) return -1;
	for(i=0;i<3;i++){
		value[i]=0;
		for(j=0;j<8;j++) value[i]=(value[i]<<8)|packet[1+i*8+j];
	}
	return qgaloademit(out,value);
}

void
nativeloadfree(void *context)
{
	NativeLoad *load;
	load=context; if(load==nil) return;
	write(load->fd,"Q",1); close(load->fd); free(load);
}
