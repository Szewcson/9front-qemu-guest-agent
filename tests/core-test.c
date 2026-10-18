/* SPDX-License-Identifier: MIT */
/* The identical core tests build under stock 9front and the Linux test shim. */
#include "../src/qjson.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#include <unistd.h>
#endif

static void
fail(const char *message)
{
#ifdef QGA_HOST_TEST
	fprintf(stderr, "FAIL: %s\n", message);
	exit(1);
#else
	sysfatal("FAIL: %s", (char*)message);
#endif
}

typedef struct Sink Sink;
struct Sink {
	char text[QgaOutputMax+1];
	int used;
	int step;
	int mode;
};

static long
capture(void *context, const uchar *s, long n)
{
	Sink *out;

	out = context;
	if(out->mode == 1)
		return 0;
	if(out->mode == 2)
		return -1;
	if(out->mode == 3)
		return n+1;
	if(n > out->step)
		n = out->step;
	if(n > QgaOutputMax - out->used)
		return -1;
	memmove(out->text + out->used, (void*)s, n);
	out->used += n;
	out->text[out->used] = 0;
	return n;
}

static void
feedcheck(Qga *q, Sink *out, const char *input, const char *expected)
{
	int i, n;

	out->used = 0;
	n = strlen((char*)input);
	for(i = 0; i < n; i++)
		if(qgafeed(q, (const uchar*)input+i, 1) < 0)
			fail("fragmented input");
	if(qgaend(q) < 0)
		fail("end of stream");
	if(out->used != (int)strlen((char*)expected) || memcmp(out->text, (void*)expected, out->used) != 0)
		fail(input);
}

static void
check(Sink *out, const char *input, const char *expected)
{
	Qga *q;

	out->mode = 0;
	out->step = 1;
	q = qganew(capture, out);
	if(q == nil)
		fail("allocation");
	feedcheck(q, out, input, expected);
	qgafree(q);
}

/* Mocks have no native I/O and can never shut down the test machine. */
static int
querymock(int command, Jbuf *b)
{
	switch(command){
	case QgaOsinfo: jbtext(b, "{\"name\":\"9front\"}"); break;
	case QgaHostname: jbtext(b, "{\"host-name\":\"test\"}"); break;
	case QgaTime: jbtext(b, "42"); break;
	case QgaInterfaces: jbtext(b, "[]"); break;
	case QgaVcpus: jbtext(b, "[{\"logical-id\":0,\"online\":true,\"can-offline\":false}]"); break;
	case QgaTimezone: jbtext(b, "{\"offset\":3600,\"zone\":\"CET\"}"); break;
	case QgaRoutes: jbtext(b, "[]"); break;
	case QgaDisks: jbtext(b, "[]"); break;
	default: return -1;
	}
	return 0;
}

static int
queryfail(int command, Jbuf *b)
{
	if(command < 0)
		return -1;
	jbtext(b, "{\"partial\":");
	return -1;
}

static int
queryoverflow(int command, Jbuf *b)
{
	int i;

	if(command < 0)
		return -1;
	for(i = 0; i < QgaOutputMax+1; i++)
		jbchar(b, ' ');
	return 0;
}

typedef struct ShutdownMock ShutdownMock;
struct ShutdownMock { int calls, mode, failure; };

static int
shutdownmock(void *context, int mode)
{
	ShutdownMock *s;

	s = context;
	s->calls++;
	s->mode = mode;
	return s->failure ? -1 : 0;
}

/* Independent wire inventory: neither table order nor production selectors
 * define expected availability, operation IDs or post-shutdown behavior. */
enum { Mquery=1, Mload=2, Mfile=4, Mtime=8, Mexec=16, Mshutdown=32, Mall=64 };
typedef struct MatrixCommand MatrixCommand;
struct MatrixCommand { const char *name, *arguments; int backend, op; };
static const MatrixCommand matrixcommands[] = {
	{"guest-info", "{}", 0, 0},
	{"guest-ping", "{}", 0, 0},
	{"guest-sync-delimited", "{\"id\":7}", 0, 0},
	{"guest-sync", "{\"id\":7}", 0, 0},
	{"guest-get-load", "{}", Mload, 0},
	{"guest-get-disks", "{}", Mquery, QgaDisks},
	{"guest-get-osinfo", "{}", Mquery, QgaOsinfo},
	{"guest-get-host-name", "{}", Mquery, QgaHostname},
	{"guest-get-time", "{}", Mquery, QgaTime},
	{"guest-network-get-interfaces", "{}", Mquery, QgaInterfaces},
	{"guest-get-vcpus", "{}", Mquery, QgaVcpus},
	{"guest-get-timezone", "{}", Mquery, QgaTimezone},
	{"guest-network-get-route", "{}", Mquery, QgaRoutes},
	{"guest-file-open", "{\"path\":\"/fixture\"}", Mfile, QgaFileOpen},
	{"guest-file-close", "{\"handle\":1}", Mfile, QgaFileClose},
	{"guest-file-read", "{\"handle\":1}", Mfile, QgaFileRead},
	{"guest-file-write", "{\"handle\":1,\"buf-b64\":\"AA==\"}", Mfile, QgaFileWrite},
	{"guest-file-seek", "{\"handle\":1,\"offset\":0,\"whence\":0}", Mfile, QgaFileSeek},
	{"guest-file-flush", "{\"handle\":1}", Mfile, QgaFileFlush},
	{"guest-set-time", "{\"time\":42}", Mtime, 0},
	{"guest-exec-status", "{\"pid\":1}", Mexec, QgaExecStatus},
	{"guest-exec", "{\"path\":\"/bin/echo\"}", Mexec, QgaExec},
	/* Last only in this test, so earlier calls exercise the active state. */
	{"guest-shutdown", "{}", Mshutdown, QgaPowerdown},
};
typedef struct MatrixState MatrixState;
struct MatrixState { int calls, backend, op; };
/* The query API has no context argument. This test has one synchronous owner. */
static MatrixState *matrixstate;
static int matrixcount;

static void
mcheck(int ok, const char *message)
{
	matrixcount++;
	if(!ok)
		fail(message);
}

static int
mreply(MatrixState *s, int backend, int op, Jbuf *b)
{
	s->calls++;
	s->backend = backend;
	s->op = op;
	jbtext(b, "{}");
	return 0;
}

static int mquery(int op, Jbuf *b) { return mreply(matrixstate, Mquery, op, b); }
static int mload(void *s, Jbuf *b) { return mreply(s, Mload, 0, b); }
static int
mfile(void *s, int op, Jdoc *d, int a, Jbuf *b, const char **error)
{
	(void)d; (void)a; (void)error;
	return mreply(s, Mfile, op, b);
}
static int
mexec(void *s, int op, Jdoc *d, int a, Jbuf *b, const char **error)
{
	(void)d; (void)a; (void)error;
	return mreply(s, Mexec, op, b);
}
static int
mtime(void *context, int supplied, QgaInt64 time, const char **error)
{
	MatrixState *s;

	(void)error;
	mcheck(supplied == 1 && time == 42, "clock callback arguments");
	s = context; s->calls++; s->backend = Mtime; s->op = 0;
	return 0;
}
static int
mshutdown(void *context, int op)
{
	MatrixState *s;

	s = context; s->calls++; s->backend = Mshutdown; s->op = op;
	return 0;
}

static void
mparse(Sink *out, Jdoc *d, int delimited)
{
	mcheck(out->used > delimited &&
		(!delimited || (uchar)out->text[0] == 255), "response framing");
	mcheck(jparseresponse(d, out->text+delimited, out->used-delimited), "response JSON");
}

static int
mmetadata(Jdoc *d, int mask)
{
	char version[64];
	int r, a, i, t, hits, expected, total;
	const MatrixCommand *c;

	r = jmember(d, 0, "return");
	if(r < 0 || d->token[r].type != Jobject || jmember(d, 0, "error") >= 0 ||
	   jgetstring(d, jmember(d, r, "version"), version, sizeof version) < 1)
		return 0;
	a = jmember(d, r, "supported_commands");
	if(a < 0 || d->token[a].type != Jarray)
		return 0;
	total = 0;
	for(i = 0; i < (int)(sizeof matrixcommands/sizeof matrixcommands[0]); i++){
		c = &matrixcommands[i];
		expected = c->backend == 0 || (c->backend & mask) != 0;
		total += expected;
		hits = 0;
		for(r = a+1; r < d->token[a].next; r = d->token[r].next){
			if(!jmatch(d, jmember(d, r, "name"), c->name))
				continue;
			hits++;
			t = jmember(d, r, "enabled");
			if(t < 0 || d->token[t].type != Jtrue)
				return 0;
			t = jmember(d, r, "success-response");
			if(t < 0 || d->token[t].type != (c->backend == Mshutdown ? Jfalse : Jtrue))
				return 0;
		}
		if(hits != expected)
			return 0;
	}
	return d->token[a].count == total;
}

static void
mrequest(Qga *q, Sink *out, Jdoc *d, MatrixState *s, const MatrixCommand *c,
	int mask, int shuttingdown)
{
	char text[256];
	Jbuf b;
	int before, enabled, blocked, e, t;

	jbinit(&b, text, sizeof text);
	jbtext(&b, "{\"execute\":"); jbquote(&b, c->name);
	jbtext(&b, ",\"arguments\":"); jbtext(&b, c->arguments); jbchar(&b, '}');
	mcheck(!b.failed, "matrix input bounds");
	before = s->calls;
	out->used = 0;
	mcheck(qgafeed(q, (uchar*)text, b.used) == 0 && qgaend(q) == 0, "matrix dispatch");
	enabled = c->backend == 0 || (c->backend & mask) != 0;
	blocked = shuttingdown && (c->backend & (Mfile|Mtime|Mexec|Mshutdown)) != 0;
	if(!enabled || blocked){
		mparse(out, d, 0);
		e = jmember(d, 0, "error");
		mcheck(jmatch(d, jmember(d, e, "class"), enabled ? "GenericError" : "Unsupported"),
			"availability precedes shutdown-state refusal");
		mcheck(s->calls == before, "rejected commands never reach callbacks");
		return;
	}
	mcheck(s->calls == before+(c->backend != 0), "exactly one selected callback");
	if(c->backend != 0)
		mcheck(s->backend == c->backend && s->op == c->op, "callback family and operation");
	if(c->backend == Mshutdown){
		mcheck(out->used == 0, "shutdown sends no success reply");
		return;
	}
	mparse(out, d, strcmp(c->name, "guest-sync-delimited") == 0);
	t = jmember(d, 0, "return");
	mcheck(t >= 0 && jmember(d, 0, "error") < 0, "successful dispatch response");
	if(strcmp(c->name, "guest-info") == 0)
		mcheck(mmetadata(d, mask), "capability names, uniqueness, availability and reply policy");
	if(strcmp(c->name, "guest-sync") == 0 || strcmp(c->name, "guest-sync-delimited") == 0)
		mcheck(d->token[t].type == Jnumber && strcmp(d->token[t].value.s, "7") == 0,
			"sync result instead of capabilities or ping");
}

static int
matrixchecks(Sink *out, Jdoc *d)
{
	MatrixState s;
	Qga *q;
	int mask, i;

	matrixcount = 0;
	out->mode = 0;
	out->step = 3;
	for(mask = 0; mask < Mall; mask++){
		memset(&s, 0, sizeof s);
		matrixstate = &s;
		q = qganew(capture, out);
		mcheck(q != nil, "matrix allocation");
		if(mask & Mquery) qgaqueries(q, mquery);
		if(mask & Mload) qgaloadops(q, mload, &s);
		if(mask & Mfile) qgafileops(q, mfile, &s);
		if(mask & Mtime) qgasettime(q, mtime, &s);
		if(mask & Mexec) qgaexecops(q, mexec, &s);
		if(mask & Mshutdown) qgashutdown(q, mshutdown, &s);
		for(i = 0; i < (int)(sizeof matrixcommands/sizeof matrixcommands[0]); i++)
			mrequest(q, out, d, &s, &matrixcommands[i], mask, 0);
		if(mask & Mshutdown)
			for(i = 0; i < (int)(sizeof matrixcommands/sizeof matrixcommands[0]); i++)
				mrequest(q, out, d, &s, &matrixcommands[i], mask, 1);
		qgafree(q);
	}
	matrixstate = nil;
	return matrixcount;
}

/* Reuse the same semantic inventory for the native smoke-test response.
 * Empty/error/malformed output is a failure, not evidence of exclusion. */
static int
stdioinfo(Sink *out, Jdoc *d)
{
	long n;
	int r, a, mask;

	out->used = 0;
	for(;;){
		if(out->used == (int)sizeof out->text)
			return 0;
		n = read(0, out->text+out->used, sizeof out->text-out->used);
		if(n < 0)
			return 0;
		if(n == 0)
			break;
		out->used += n;
	}
	if(!jparseresponse(d, out->text, out->used))
		return 0;
	r = jmember(d, 0, "return");
	a = jmember(d, r, "supported_commands");
	if(a < 0 || d->token[a].type != Jarray)
		return 0;
	mask = Mquery|Mfile;
	for(r = a+1; r < d->token[a].next; r = d->token[r].next)
		if(jmatch(d, jmember(d, r, "name"), "guest-get-load"))
			mask |= Mload;
	return mmetadata(d, mask);
}

static int
extendedchecks(Sink *out, Jdoc *doc)
{
	Qga *q;
	ShutdownMock s;
	Jbuf b;
	char text[128];
	const char *expected = "\"\\u2603\\ud83d\\ude00\\\\\\\"\\n\\u0000\"";
	char raw[] = "\342\230\203\360\237\230\200\\\"\n\0";
	const char *info = "{\"execute\":\"guest-info\"}";
	int r, a;

	out->mode = 0;
	out->step = 1;
	memset(&s, 0, sizeof s);
	q = qganew(capture, out);
	if(q == nil)
		fail("allocation");
	qgaqueries(q, querymock);
	qgashutdown(q, shutdownmock, &s);
	feedcheck(q, out, "{\"execute\":\"guest-get-osinfo\"}", "{\"return\":{\"name\":\"9front\"}}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-get-host-name\",\"id\":0}",
		"{\"return\":{\"host-name\":\"test\"},\"id\":0}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-get-time\"}", "{\"return\":42}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-network-get-interfaces\"}", "{\"return\":[]}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-get-vcpus\"}",
		"{\"return\":[{\"logical-id\":0,\"online\":true,\"can-offline\":false}]}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-get-timezone\"}",
		"{\"return\":{\"offset\":3600,\"zone\":\"CET\"}}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-network-get-route\",\"arguments\":{},\"id\":11}",
		"{\"return\":[],\"id\":11}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-get-vcpus\",\"arguments\":{\"extra\":1}}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Command takes no arguments\"}}\r\n");
	out->used = 0;
	if(qgafeed(q, (const uchar*)info, strlen((char*)info)) < 0 ||
	   !jparse(doc, out->text, out->used))
		fail("enabled metadata JSON");
	r = jmember(doc, 0, "return");
	a = jmember(doc, r, "supported_commands");
	if(a < 0 || doc->token[a].count != 13 ||
	   strstr(out->text, "\"name\":\"guest-shutdown\",\"enabled\":true,\"success-response\":false") == nil)
		fail("enabled metadata");
	feedcheck(q, out, "{\"execute\":\"guest-shutdown\",\"arguments\":{\"mode\":\"halt\"}}",
		"{\"error\":{\"class\":\"Unsupported\",\"desc\":\"halt without poweroff is not implemented\"}}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-shutdown\",\"arguments\":{\"mode\":12}}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Invalid shutdown mode\"}}\r\n");
	feedcheck(q, out, "{\"execute\":\"guest-shutdown\",\"arguments\":{\"extra\":0}}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Unexpected shutdown argument\"}}\r\n");
	if(s.calls != 0)
		fail("invalid shutdown cannot call executor");
	feedcheck(q, out, "{\"execute\":\"guest-shutdown\",\"arguments\":{\"mode\":\"reboot\"},\"id\":9}", "");
	if(s.calls != 1 || s.mode != QgaReboot)
		fail("asynchronous reboot");
	feedcheck(q, out, "{\"execute\":\"guest-shutdown\"}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Shutdown is already in progress\"}}\r\n");
	if(s.calls != 1)
		fail("duplicate shutdown executor");
	feedcheck(q, out, "{\"execute\":\"guest-ping\"}", "{\"return\":{}}\r\n");
	qgafree(q);

	s.calls = 0;
	s.failure = 1;
	q = qganew(capture, out);
	if(q == nil)
		fail("allocation");
	qgashutdown(q, shutdownmock, &s);
	feedcheck(q, out, "{\"execute\":\"guest-shutdown\"}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Cannot initiate native shutdown\"}}\r\n");
	s.failure = 0;
	feedcheck(q, out, "{\"execute\":\"guest-shutdown\"}", "");
	if(s.calls != 2 || s.mode != QgaPowerdown)
		fail("powerdown default and retry after initiation failure");
	qgafree(q);

	q = qganew(capture, out);
	if(q == nil)
		fail("allocation");
	qgaqueries(q, queryfail);
	feedcheck(q, out, "{\"execute\":\"guest-get-time\",\"id\":7}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Cannot read or serialize native guest information\"},\"id\":7}\r\n");
	qgafree(q);
	q = qganew(capture, out);
	if(q == nil)
		fail("allocation");
	qgaqueries(q, queryoverflow);
	feedcheck(q, out, "{\"execute\":\"guest-get-time\"}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Cannot read or serialize native guest information\"}}\r\n");
	qgafree(q);

	jbinit(&b, text, sizeof text);
	jbstring(&b, raw, sizeof raw-1);
	if(b.failed || b.used != (int)strlen((char*)expected) || memcmp(text, (void*)expected, b.used) != 0)
		fail("native byte string escaping");
	jbinit(&b, text, sizeof text);
	jbstring(&b, "\300\200", 2);
	if(!b.failed)
		fail("native byte string invalid UTF-8");
	jbinit(&b, text, sizeof text);
	jbstring(&b, "\342\230", 2);
	if(!b.failed)
		fail("native byte string truncated UTF-8");
	return 22;
}

#ifdef QGA_HOST_TEST
int main(int argc, char **argv)
#else
void main(int argc, char **argv)
#endif
{
	Sink *out;
	Qga *q;
	Jdoc *doc;
	Jbuf b;
	char small[3];
	const char *ping = "{\"execute\":\"guest-ping\"}";
	const char *duplicate = "{\"a\":0,\"\\u0061\":1}";
	int i, checks;

	out = malloc(sizeof *out);
	doc = malloc(sizeof *doc);
	if(out == nil || doc == nil)
		fail("allocation");
	if(argc == 2 && strcmp(argv[1], "--stdio-info") == 0){
		i = stdioinfo(out, doc);
		free(doc);
		free(out);
		if(!i)
			fail("invalid stdio capabilities");
#ifdef QGA_HOST_TEST
		return 0;
#else
		exits(nil);
#endif
	}
	if(argc != 1)
		fail("usage: qga9-test [--stdio-info]");
	memset(out, 0, sizeof *out);
	check(out, ping, "{\"return\":{}}\r\n");
	check(out, "{\"execute\":\"guest-sync\",\"arguments\":{\"id\":9223372036854775807}}",
		"{\"return\":9223372036854775807}\r\n");
	check(out, "{\"execute\":\"guest-sync\",\"arguments\":{\"id\":-9223372036854775808}}",
		"{\"return\":-9223372036854775808}\r\n");
	check(out, "{\"execute\":\"guest-sync-delimited\",\"arguments\":{\"id\":42}}",
		"\377{\"return\":42}\r\n");
	check(out, "{\"execute\":\"guest-\377{\"execute\":\"guest-ping\"}", "{\"return\":{}}\r\n");
	check(out, "{\"execute\":\"guest-ping\"}{\"execute\":\"guest-ping\"}",
		"{\"return\":{}}\r\n{\"return\":{}}\r\n");
	check(out, "{\"id\":{\"snow\":\"\342\230\203\",\"null\":null},\"execute\":\"guest-ping\"}",
		"{\"return\":{},\"id\":{\"snow\":\"\\u2603\",\"null\":null}}\r\n");
	check(out, "{\"execute\":\"guest-ping\",\"arguments\":null}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"arguments must be an object\"}}\r\n");
	check(out, "{\"execute\":\"guest-ping\",}",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Invalid JSON object or parser limit exceeded\"}}\r\n");
	check(out, "{\"execute\":\"guest-shutdown\"}",
		"{\"error\":{\"class\":\"Unsupported\",\"desc\":\"Command is not implemented or is excluded by policy\"}}\r\n");
	check(out, "{\"execute\":\"unknown\",\"id\":8}",
		"{\"error\":{\"class\":\"CommandNotFound\",\"desc\":\"Unknown command\"},\"id\":8}\r\n");
	check(out, "{\"execute\":",
		"{\"error\":{\"class\":\"GenericError\",\"desc\":\"Truncated JSON message at end of stream\"}}\r\n");
	/* Reject zero, failed and impossible writes without retrying forever. */
	for(i = 1; i <= 3; i++){
		out->mode = i;
		q = qganew(capture, out);
		if(q == nil)
			fail("allocation");
		if(qgafeed(q, (const uchar*)ping, strlen((char*)ping)) != -1 ||
		   qgafeed(q, (const uchar*)ping, strlen((char*)ping)) != -1 || qgaend(q) != -1)
			fail("write failure must be sticky");
		qgafree(q);
	}
	if(jparse(doc, duplicate, strlen((char*)duplicate)))
		fail("duplicate decoded keys");
	jbinit(&b, small, sizeof small);
	jbtext(&b, "1234");
	if(!b.failed || b.used != 0)
		fail("bounded output");
	checks = 17 + extendedchecks(out, doc) + matrixchecks(out, doc);
	free(doc);
	free(out);
#ifdef QGA_HOST_TEST
	printf("core tests: %d checks passed\n", checks);
	return 0;
#else
	print("core tests: %d checks passed\n", checks);
	exits(nil);
#endif
}
