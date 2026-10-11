/* SPDX-License-Identifier: MIT */
/* Live test: write the current system time, not a deliberately shifted date. */
#include "../src/native.h"
#include "../src/clock.h"

void
main(void)
{
	QgaInt64 before, after, rtc;
	const char *error;
	char text[32];
	int fd, n;

	before = nsec();
	if(before < 0 || before > QgaRtcMaxTime)
		sysfatal("current time is outside RTC range");
	if(nativesettime(nil, 1, before, &error) < 0)
		sysfatal("native set-time: %s", error);
	after = nsec();
	if(after < before || after-before > 5000000000LL)
		sysfatal("system time readback is not near the supplied current time");
	fd = open("#r/rtc", OREAD);
	if(fd < 0)
		sysfatal("read RTC: %r");
	n = read(fd, text, sizeof text-1);
	close(fd);
	if(n <= 0)
		sysfatal("RTC readback failed");
	text[n] = 0;
	rtc = strtoll(text, nil, 10);
	if(rtc < before/1000000000LL || rtc > after/1000000000LL+1)
		sysfatal("RTC is not synchronized with the system clock");
	if(nativesettime(nil, 1, -1, &error) == 0 ||
	   nativesettime(nil, 1, QgaRtcMaxTime+1, &error) == 0)
		sysfatal("out-of-range time was accepted");
	print("native clock: current-time system/RTC update and range rejection passed\n");
	exits(nil);
}
