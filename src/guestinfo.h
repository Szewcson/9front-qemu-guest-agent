/* SPDX-License-Identifier: MIT */
#ifndef QGA9_GUESTINFO_H
#define QGA9_GUESTINFO_H
#include "json.h"

enum {
	QgaCpuMax = 256,
	QgaRouteMax = 256,
	QgaNativeTextMax = 65536,
	QgaIpTextMax = 64,
};

/* Canonical address, returning 4/6 or -1. The callback never retains pointers. */
typedef int (*QgaFormatIp)(const char*, char*, int);

/* Bounded, single-owner formatters: caller retains all input/output storage. */
int qgacpus(Jbuf*, const char*, int);
int qgatimezone(Jbuf*, const char*, int, int);
int qgaroutes(Jbuf*, const char*, int, QgaFormatIp);

#endif
