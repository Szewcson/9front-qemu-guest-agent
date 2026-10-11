/* SPDX-License-Identifier: MIT */
/* Clock ordering/partial failures and real core dispatch, without clock writes. */
#include "../src/clock.h"
#include "../src/json.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#endif

typedef struct Mock Mock;
struct Mock {
	int begins, ends, sets, reads, writes;
	int failbegin, failread, failset, failnow, failwrite;
	QgaInt64 rtc, current, requested, written;
};
static int checks;

static void
check(int ok, const char *what)
{
	checks++;
	if(ok) return;
#ifdef QGA_HOST_TEST
	fprintf(stderr, "FAIL: %s\n", what);
	exit(1);
#else
	sysfatal("FAIL: %s", (char*)what);
#endif
}

static int mockbegin(void *v) { Mock *m = v; m->begins++; return m->failbegin ? -1 : 0; }
static void mockend(void *v) { Mock *m = v; m->ends++; }
static int mockreadrtc(void *v, QgaInt64 *n) { Mock *m = v; m->reads++; *n = m->rtc; return m->failread ? -1 : 0; }
static int mocksetclock(void *v, QgaInt64 n) { Mock *m = v; m->sets++; m->requested = n; return m->failset ? -1 : 0; }
static int mocknow(void *v, QgaInt64 *n) { Mock *m = v; *n = m->current; return m->failnow ? -1 : 0; }
static int mockwritertc(void *v, QgaInt64 n) { Mock *m = v; m->writes++; m->written = n; return m->failwrite ? -1 : 0; }
static const QgaClockIO io = {mockbegin, mockend, mockreadrtc, mocksetclock, mocknow, mockwritertc};

static int
settime(void *context, int supplied, QgaInt64 time, const char **error)
{
	return qgaclockset(&io, context, supplied, time, error);
}

typedef struct Sink Sink;
struct Sink { char output[512]; int used; };
static long
capture(void *context, const uchar *s, long n)
{
	Sink *b;

	b = context;
	if(n > 3) n = 3;
	if(n > (int)sizeof b->output-1-b->used) return -1;
	memmove(b->output+b->used, s, n);
	b->used += n;
	b->output[b->used] = 0;
	return n;
}

#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	Mock m;
	Qga *q;
	Sink sink;
	const char *error;
	const char *invalid[] = {
		"{\"time\":true}", "{\"time\":1.0}", "{\"time\":9223372036854775808}",
		"{\"time\":-1}", "{\"time\":3155760000000000000}", "{\"extra\":1}",
	};
	char request[256];
	Jbuf b;
	int i, before;

	memset(&m, 0, sizeof m);
	m.current = 123456789123LL;
	check(settime(&m, 1, 9876543210LL, &error) == 0 && m.requested == 9876543210LL &&
		m.reads == 0 && m.written == 123 && m.ends == 1, "nanosecond system update then current-time RTC update");
	m.rtc = 42;
	check(settime(&m, 0, 0, &error) == 0 && m.requested == 42000000000LL &&
		m.reads == 1 && m.ends == 2, "omitted time derives from RTC");
	before = m.begins;
	check(settime(&m, 1, -1, &error) < 0 && settime(&m, 1, QgaRtcMaxTime+1, &error) < 0 &&
		m.begins == before, "bounds rejected before resource acquisition or mutation");
	check(settime(&m, 1, 0, &error) == 0 && settime(&m, 1, QgaRtcMaxTime, &error) == 0,
		"native RTC range endpoints");
	m.failbegin = 1;
	before = m.sets;
	check(settime(&m, 1, 1, &error) < 0 && m.sets == before, "both-device preflight precedes writes");
	m.failbegin = 0;
	m.failread = 1;
	check(settime(&m, 0, 0, &error) < 0 && m.sets == before, "failed RTC read never sets system clock");
	m.failread = 0;
	m.rtc = QgaRtcMaxSeconds+1;
	check(settime(&m, 0, 0, &error) < 0 && m.sets == before, "RTC multiplication range checked");
	m.rtc = -1;
	check(settime(&m, 0, 0, &error) < 0 && m.sets == before, "negative RTC value rejected");
	m.failset = 1;
	before = m.writes;
	check(settime(&m, 1, 1, &error) < 0 && m.writes == before, "failed system write never changes RTC");
	m.failset = 0;
	m.failnow = 1;
	check(settime(&m, 1, 1, &error) < 0 && m.writes == before && strstr(error, "System clock changed") != nil,
		"readback partial failure explicitly reported");
	m.failnow = 0;
	m.current = QgaRtcMaxTime+1;
	check(settime(&m, 1, 1, &error) < 0 && m.writes == before, "post-update current-time bounds");
	m.current = 1791651000123456789LL;
	m.failwrite = 1;
	check(settime(&m, 1, 1, &error) < 0 && strstr(error, "System clock changed but RTC update failed") != nil,
		"RTC write partial failure explicitly reported");
	m.failwrite = 0;
	check(m.ends == m.begins-1, "every successful begin has one cleanup, including failures");
	check(qgaclockset(nil, &m, 1, 1, &error) < 0 &&
		qgaclockset(&io, &m, 2, 1, &error) < 0, "callback/configuration validity");
	q = qganew(capture, &sink);
	check(q != nil, "clock protocol allocation");
	qgasettime(q, settime, &m);
	for(i = 0; i < (int)(sizeof invalid/sizeof invalid[0]); i++){
		jbinit(&b, request, sizeof request);
		jbtext(&b, "{\"execute\":\"guest-set-time\",\"arguments\":");
		jbtext(&b, invalid[i]);
		jbchar(&b, '}');
		before = m.sets;
		sink.used = 0;
		check(!b.failed && qgafeed(q, (uchar*)request, b.used) == 0 && m.sets == before &&
			strstr(sink.output, "\"error\"") != nil, "invalid clock request cannot mutate time");
	}
	sink.used = 0;
	jbinit(&b, request, sizeof request);
	jbtext(&b, "{\"execute\":\"guest-set-time\",\"arguments\":{\"time\":1791651000123456789},\"id\":9}");
	check(qgafeed(q, (uchar*)request, b.used) == 0 &&
		strcmp(sink.output, "{\"return\":{},\"id\":9}\r\n") == 0, "core clock dispatch and transaction ID");
	m.rtc = 1791651000;
	sink.used = 0;
	jbinit(&b, request, sizeof request);
	jbtext(&b, "{\"execute\":\"guest-set-time\"}");
	check(qgafeed(q, (uchar*)request, b.used) == 0 &&
		strcmp(sink.output, "{\"return\":{}}\r\n") == 0, "core omitted-time dispatch");
	qgafree(q);
#ifdef QGA_HOST_TEST
	printf("clock tests: %d checks passed\n", checks);
	return 0;
#else
	print("clock tests: %d checks passed\n", checks);
	exits(nil);
#endif
}
