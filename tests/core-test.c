/* SPDX-License-Identifier: MIT */
/* The identical core tests build under stock 9front and the Linux test shim. */
#include "../src/qjson.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
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
int main(void)
#else
void main(void)
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
	checks = 17 + extendedchecks(out, doc);
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
