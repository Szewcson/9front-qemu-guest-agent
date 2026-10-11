/* SPDX-License-Identifier: MIT */
#include "clock.h"

typedef struct Clock Clock;
struct Clock { int systemfd, rtcfd; };

static void
clockend(void *context)
{
	Clock *c;

	c = context;
	if(c->systemfd >= 0) close(c->systemfd);
	if(c->rtcfd >= 0) close(c->rtcfd);
	c->systemfd = c->rtcfd = -1;
}

static int
clockbegin(void *context)
{
	Clock *c;

	c = context;
	c->systemfd = open("#c/bintime", OWRITE|OCEXEC);
	c->rtcfd = open("#r/rtc", ORDWR|OCEXEC);
	if(c->systemfd >= 0 && c->rtcfd >= 0)
		return 0;
	clockend(c);
	return -1;
}

static int
clockreadrtc(void *context, QgaInt64 *seconds)
{
	Clock *c;
	char text[32], extra;
	QgaInt64 value;
	int n, used, i, start, end, digit;

	c = context;
	used = 0;
	while(used < sizeof text-1){
		n = read(c->rtcfd, text+used, sizeof text-1-used);
		if(n < 0) return -1;
		if(n == 0) break;
		used += n;
	}
	if(used == sizeof text-1 && read(c->rtcfd, &extra, 1) != 0)
		return -1;
	start = 0;
	end = used;
	while(start < end && (text[start] == ' ' || text[start] == '\t')) start++;
	while(end > start && (text[end-1] == ' ' || text[end-1] == '\n')) end--;
	if(start == end)
		return -1;
	value = 0;
	for(i = start; i < end; i++){
		digit = text[i]-'0';
		if(digit < 0 || digit > 9 || value > (QgaRtcMaxSeconds-digit)/10)
			return -1;
		value = value*10+digit;
	}
	*seconds = value;
	return 0;
}

static int
clockset(void *context, QgaInt64 time)
{
	Clock *c;
	uchar command[9];
	QgaUint64 value;
	int i;

	c = context;
	value = time;
	command[0] = 'n';
	for(i = 8; i >= 1; i--){
		command[i] = value & 255;
		value >>= 8;
	}
	return pwrite(c->systemfd, command, sizeof command, 0) == sizeof command ? 0 : -1;
}

static int
clocknow(void *context, QgaInt64 *time)
{
	USED(context);
	*time = nsec();
	return *time < 0 ? -1 : 0;
}

static int
clockwritertc(void *context, QgaInt64 seconds)
{
	Clock *c;
	char text[32];
	int n;

	c = context;
	n = snprint(text, sizeof text, "%lld", seconds);
	if(n <= 0 || n >= sizeof text)
		return -1;
	return pwrite(c->rtcfd, text, n, 0) == n ? 0 : -1;
}

int
nativesettime(void *unused, int supplied, QgaInt64 time, const char **error)
{
	static const QgaClockIO io = {clockbegin, clockend, clockreadrtc, clockset, clocknow, clockwritertc};
	Clock c;

	USED(unused);
	c.systemfd = c.rtcfd = -1;
	return qgaclockset(&io, &c, supplied, time, error);
}
