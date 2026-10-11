/* SPDX-License-Identifier: MIT */
#ifndef QGA9_CLOCK_H
#define QGA9_CLOCK_H
#include "qga.h"

/* PC RTC's native two-digit year decoder supports UTC years 1970..2069. */
#define QgaRtcMaxSeconds 3155759999LL
#define QgaRtcMaxTime 3155759999999999999LL

typedef struct QgaClockIO QgaClockIO;
/* begin acquires both devices before mutation, and cleans partial acquisition
 * on failure. end releases resources once after a successful begin. All calls
 * are synchronous and single-owner; no callback retains passed pointers. */
struct QgaClockIO {
	int (*begin)(void*);
	void (*end)(void*);
	int (*readrtc)(void*, QgaInt64*);
	int (*setclock)(void*, QgaInt64);
	int (*now)(void*, QgaInt64*);
	int (*writertc)(void*, QgaInt64);
};

int qgaclockset(const QgaClockIO*, void*, int, QgaInt64, const char**);

#endif
