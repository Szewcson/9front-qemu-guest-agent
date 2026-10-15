/* SPDX-License-Identifier: MIT */
/* Actual native adapter with bounded filesystem/namespace syscall fixtures. */
#include "../src/files.h"
#include <stdio.h>

typedef struct Dir Dir;
struct Dir { int type; unsigned long mode; struct { int type; } qid; QgaInt64 length; };
enum { OREAD=0, OWRITE=1, ORDWR=2, OCEXEC=32, OEXCL=0x1000,
	DMDIR=0x80000000U, DMAPPEND=0x40000000U, QTDIR=0x80, QTAUTH=8, RFNAMEG=1 };
#define USED(x) ((void)(x))
static struct {
	int existing, type, parenttype, parentfail, statfail, collision, forkfail;
	int createcalls, truncations, closes, parents, forks, opened;
} fs;
static int checks;
static void check(int ok) { checks++; if(!ok) { fprintf(stderr, "native file check %d failed\n", checks); exit(1); } }
static int mockopen(char *path, int mode)
{
	(void)mode;
	if(strcmp(path, "/tmp/file") == 0 || strcmp(path, "/env/file") == 0 ||
	   strcmp(path, "/srv/file") == 0 || strcmp(path, "/alias/file") == 0)
		return fs.existing ? 20 : -1;
	fs.parents++;
	return fs.parentfail ? -1 : 10;
}
static int mockcreate(char *path, int mode, int perm)
{
	check(strcmp(path, "/tmp/file") == 0);
	check((mode & (OCEXEC|OEXCL)) == (OCEXEC|OEXCL) && perm == 0600);
	fs.createcalls++;
	return fs.collision ? -1 : 20;
}
static Dir *mockstat(int fd)
{
	Dir *d;
	check(fd == 10 || fd == 20);
	if(fs.statfail) return nil;
	d = calloc(1, sizeof *d);
	check(d != nil);
	d->type = fd == 10 ? fs.parenttype : fs.type;
	d->mode = fd == 10 ? DMDIR : 0600;
	d->qid.type = fd == 10 ? QTDIR : 0;
	return d;
}
static int mockclose(int fd) { check(fd == 10 || fd == 20); fs.closes++; return 0; }
static void mocknull(Dir *d) { memset(d, 0xff, sizeof *d); }
static int mockwstat(int fd, Dir *d) { check(fd == 20 && d->length == 0); fs.truncations++; return 0; }
static int mockread(int fd, void *p, int n) { (void)fd; (void)p; (void)n; return 0; }
static int mockwrite(int fd, void *p, int n) { (void)fd; (void)p; return n; }
static QgaInt64 mockseek(int fd, QgaInt64 off, int w) { (void)fd; (void)w; return off; }
static int mockfork(int flags) { check(flags == RFNAMEG); fs.forks++; return fs.forkfail ? -1 : 0; }
#define open mockopen
#define create mockcreate
#define dirfstat mockstat
#define close mockclose
#define nulldir mocknull
#define dirfwstat mockwstat
#define read mockread
#define write mockwrite
#define seek mockseek
#define rfork mockfork
/* Backend dispatch fixture: helper's namespace/descriptor behavior has its
 * own native-create-test; here retain exact final descriptor/truncation checks. */
int nativefilecreate(const char *path, int mode)
{
	return mockcreate((char*)path, mode|OCEXEC|OEXCL, 0600);
}
#include "../src/native-files.c"

static void reset(void) { memset(&fs, 0, sizeof fs); fs.type=fs.parenttype='M'; }

typedef struct Sink Sink;
struct Sink { char text[1024]; int used; };
static long
capture(void *context, const uchar *data, long count)
{
	Sink *sink;
	sink = context;
	if(count < 0 || count >= (int)sizeof sink->text-sink->used) return -1;
	memmove(sink->text+sink->used, data, count);
	sink->used += count; sink->text[sink->used] = 0;
	return count;
}

static void
wirereject(const char *path, int parenttype)
{
	Sink sink;
	Qga *q;
	QgaFiles *files;
	char request[256];
	int n;

	reset(); fs.parenttype = parenttype; memset(&sink, 0, sizeof sink);
	files = nativefiles(); check(files != nil && fs.forks == 1);
	q = qganew(capture, &sink); check(q != nil);
	qgafileops(q, qgafilesop, files);
	n = snprintf(request, sizeof request,
		"{\"execute\":\"guest-file-open\",\"arguments\":{\"path\":\"%s\",\"mode\":\"w+\"},\"id\":42}\n", path);
	check(n > 0 && n < (int)sizeof request);
	check(qgafeed(q, (const uchar*)request, n) == 0 && qgaend(q) == 0);
	check(strstr(sink.text, "\"class\":\"GenericError\"") != nil &&
		strstr(sink.text, "\"id\":42") != nil);
	check(fs.createcalls == 0 && fs.truncations == 0 && fs.parents == 1 && fs.closes == 1);
	qgafree(q); qgafilesfree(files);
}

int main(void)
{
	QgaFiles *f;
	int i, fd;
	const char *bad[] = {"/env/file", "/srv/file", "/alias/file"};
	for(i=0; i<3; i++){
		reset(); fs.parenttype = i == 1 ? 's' : 'e';
		check(fileopen(nil, bad[i], QgaOpenWrite|QgaOpenTruncate) < 0);
		check(fs.createcalls == 0 && fs.truncations == 0 && fs.closes == 1);
		wirereject(bad[i], i == 1 ? 's' : 'e');
	}
	reset(); fd=fileopen(nil, "/tmp/file", QgaOpenRead|QgaOpenWrite|QgaOpenTruncate);
	check(fd == 20 && fs.createcalls == 1 && fs.truncations == 1 && fs.closes == 1);
	mockclose(fd);
	reset(); fs.existing=1;
	check(fileopen(nil, "/tmp/file", QgaOpenWrite|QgaOpenTruncate) == 20);
	check(fs.parents == 0 && fs.createcalls == 0 && fs.truncations == 1);
	mockclose(20);
	reset(); fs.existing=1; fs.type='e';
	check(fileopen(nil, "/env/file", QgaOpenWrite|QgaOpenTruncate) < 0);
	check(fs.createcalls == 0 && fs.truncations == 0 && fs.closes == 1);
	reset(); fs.parentfail=1;
	check(fileopen(nil, "/tmp/file", QgaOpenWrite|QgaOpenTruncate) < 0 && fs.createcalls == 0);
	reset(); fs.statfail=1;
	check(fileopen(nil, "/tmp/file", QgaOpenWrite|QgaOpenTruncate) < 0 && fs.createcalls == 0 && fs.closes == 1);
	reset(); fs.collision=1;
	check(fileopen(nil, "/tmp/file", QgaOpenWrite|QgaOpenTruncate) < 0 && fs.truncations == 0);
	reset(); check(filecreate("/tmp/", ORDWR) < 0 && filecreate("/tmp/..", ORDWR) < 0 && filecreate("/tmp/.", ORDWR) < 0);
	check(fs.createcalls == 0);
	reset(); fs.forkfail=1; check(nativefiles() == nil && fs.forks == 1);
	reset(); f=nativefiles(); check(f != nil && fs.forks == 1); qgafilesfree(f);
	printf("native file adapter: %d checks passed\n", checks);
	return 0;
}
