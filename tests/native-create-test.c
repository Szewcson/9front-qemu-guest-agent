/* SPDX-License-Identifier: MIT */
/* Actual creator with isolated fork/fd/namespace fixtures, not a native VM. */
#include "../src/files.h"
#include <stdio.h>
#define snprint snprintf
typedef struct Dir Dir;
struct Dir { int type; unsigned long mode; struct { int type; } qid; };
enum { OREAD=0, OWRITE=1, ORDWR=2, OCEXEC=32, OEXCL=4096, ORCLOSE=64,
	QTDIR=128, QTAUTH=8,
	MREPL=0, MCREATE=4, RFPROC=1, RFFDG=2, RFNAMEG=4, RFNOTEG=8, RFNOWAIT=16 };
#define DMDIR 0x80000000U
/* native.h is not used by the host shim; all native primitives are below. */
#define QGA_NATIVE_H
static int checks;
static struct {
	int child, unionparent, parenttype, parentfail, bindfail, collision, forkfail, closefail;
	int boardfail, statfail, postfail, ackfail, pipefail, mode;
	int opened, creates, binds, posts, leases, controls, readers, writers, files, directories;
	int closed[64], boardread, ackread, childresult;
	const char *parent, *leaf;
} f;
static void check(int ok) { checks++; if(!ok){ fprintf(stderr,"creator check %d failed\n",checks); exit(1); } }
static int createchild(char*, char*, int, int, int);
static int mockopen(char *path, int mode)
{
	if(strcmp(path,"#s/clone")==0){ check(!f.child && mode==(OREAD|OCEXEC)); f.leases++; return 30; }
	if(strcmp(path,".")==0){ check(f.child && f.binds==1); f.directories++; return 40; }
	check(strcmp(path,"#s/77/qga9-file")==0 && !f.child && f.posts==1 && mode==(f.mode|OCEXEC));
	f.readers++; return 42;
}
static int mockclose(int fd) { check(fd>=30 && fd<64); f.closed[fd]++; return 0; }
static int mockread(int fd, void *buf, int n)
{
	check(!f.child && n>0);
	if(fd==30){
		if(f.boardread++) return 0;
		memcpy(buf, f.boardfail ? "bad" : "77", f.boardfail ? 3 : 2);
		return f.boardfail ? 3 : 2;
	}
	check(fd==31);
	if(f.ackread++ || f.childresult<0 || f.ackfail) return 0;
	*(char*)buf='1'; return 1;
}
static int mockwrite(int fd, void *buf, int n)
{
	check(f.child && n>0);
	if(fd==33){ check(n==2 && memcmp(buf,"41",2)==0); if(f.postfail) return -1; f.posts++; return n; }
	check(fd==32 && n==1 && *(char*)buf=='1'); return f.ackfail ? -1 : 1;
}
static Dir *mockstat(int fd)
{
	Dir *d;
	check(f.child && fd==40);
	if(f.statfail) return nil;
	d=calloc(1,sizeof *d); check(d!=nil);
	d->type=f.parenttype; d->mode=DMDIR|0700; d->qid.type=QTDIR; return d;
}
static void mocknull(Dir *d) { memset(d,0xff,sizeof *d); }
static int mockwstat(char *path, Dir *d) { check(!f.child && strcmp(path,"#s/77")==0 && d->mode==0700); return 0; }
static int mockcreate(char *path, int mode, int perm)
{
	if(!f.child){ check(strcmp(path,"#s/77/qga9-file")==0 && mode==(OWRITE|OCEXEC|OEXCL|ORCLOSE) && perm==0600); f.controls++; return 33; }
	check(strcmp(path,f.leaf)==0 && mode==(f.mode|OCEXEC|OEXCL) && perm==0600);
	check(f.binds==1 && f.parenttype=='M' && !f.unionparent && f.closed[40]==1);
	f.creates++; return f.collision ? -1 : 41;
}
static int mockchdir(char *path) { check(f.child && strcmp(path,f.parent)==0); return f.parentfail ? -1 : 0; }
static int mockbind(char *from, char *to, int flags)
{
	check(f.child && strcmp(from,".")==0 && strcmp(to,".")==0 && flags==(MREPL|MCREATE));
	f.binds++; return f.unionparent || f.bindfail ? -1 : 0;
}
static int mockexcept(int *keep, int count)
{
	check(f.child && count==2 && keep[0]==33 && keep[1]==32);
	return f.closefail ? -1 : 0;
}
static int mockpipe(int *fd) { fd[0]=31; fd[1]=32; return f.pipefail ? -1 : 0; }
static int mockfork(int flags)
{
	check(flags==(RFPROC|RFFDG|RFNAMEG|RFNOTEG|RFNOWAIT));
	if(f.forkfail) return -1;
	/* Execute the real child function in the fixture's separate fd/ns actor.
	 * Native RFPROC execution itself is covered only by the explicit VM test. */
	f.child=1; f.childresult=createchild((char*)f.parent,(char*)f.leaf,f.mode,33,32); f.child=0;
	return 123;
}
static void mockexit(char *why) { (void)why; abort(); }
#define open mockopen
#define close mockclose
#define read mockread
#define write mockwrite
#define dirfstat mockstat
#define nulldir mocknull
#define dirwstat mockwstat
#define create mockcreate
#define chdir mockchdir
#define bind mockbind
#define nativecloseexcept mockexcept
#define pipe mockpipe
#define rfork mockfork
#define _exits mockexit
/* native.h has no guard: supply its dependencies without importing native APIs. */
#define QGA9_NATIVE_CREATE_TEST
#include "../src/native-create.c"

static void reset(void) { memset(&f,0,sizeof f); f.parent="/tmp"; f.leaf="file"; f.parenttype='M'; f.mode=ORDWR; }
int main(void)
{
	int i, result;
	reset(); check(nativefilecreate("/tmp/file",ORDWR)==42 && f.creates==1 && f.posts==1);
	check(f.closed[30]==1 && f.closed[33]==1 && f.closed[31]==1 && f.closed[32]==1 && f.closed[41]==1);
	reset(); f.parent="/"; check(nativefilecreate("/file",ORDWR)==42);
	for(i=0;i<10;i++){
		reset();
		switch(i){
		case 0: f.unionparent=1; break;
		case 1: f.parenttype='e'; break;
		case 2: f.parentfail=1; break;
		case 3: f.bindfail=1; break;
		case 4: f.closefail=1; break;
		case 5: f.forkfail=1; break;
		case 6: f.boardfail=1; break;
		case 7: f.statfail=1; break;
		case 8: f.collision=1; break;
		case 9: f.pipefail=1; break;
		}
		result=nativefilecreate("/tmp/file",ORDWR);
		check(result<0 && f.closed[30]==1 && (f.controls==0 || f.closed[33]==1));
		if(i!=8) check(f.creates==0);
	}
	reset(); f.postfail=1; check(nativefilecreate("/tmp/file",ORDWR)<0 && f.closed[41]==1 && f.closed[33]==1);
	reset(); f.ackfail=1; check(nativefilecreate("/tmp/file",ORDWR)<0 && f.closed[41]==1 && f.closed[33]==1);
	reset(); check(nativefilecreate("/tmp/",ORDWR)<0 && nativefilecreate("/tmp/..",ORDWR)<0 && nativefilecreate("relative",ORDWR)<0 && f.leases==0);
	printf("native anchored creator: %d checks passed\n",checks); return 0;
}
