/* SPDX-License-Identifier: MIT */
#ifndef QGA9_H
#define QGA9_H

/* The test shim is not part of the native daemon or its dependencies. */
#ifdef QGA_HOST_TEST
#include <u.h>
typedef long long QgaInt64;
typedef unsigned long long QgaUint64;
#else
#include <u.h>
#include <libc.h>
typedef vlong QgaInt64;
typedef uvlong QgaUint64;
#endif

enum {
	QgaMessageMax = 8192,
	QgaOutputMax = 65536,
	QgaTokenMax = 512,
	QgaDepthMax = 16,
	QgaOsinfo = 0,
	QgaHostname,
	QgaTime,
	QgaInterfaces,
	QgaVcpus,
	QgaTimezone,
	QgaRoutes,
	QgaDisks,
	QgaLoadAverage,
	QgaFileOpen = 0,
	QgaFileClose,
	QgaFileRead,
	QgaFileWrite,
	QgaFileSeek,
	QgaFileFlush,
	QgaExec = 0,
	QgaExecStatus,
	QgaPowerdown = 0,
	QgaReboot,
};

typedef struct Jbuf Jbuf;
struct Jbuf {
	char *text;
	int size;
	int used;
	int failed;
};

typedef struct Qga Qga;
typedef struct Jdoc Jdoc;
#ifndef QGA_HOST_TEST
/* Keep native -T signatures stable across the opaque implementation boundary. */
#pragma incomplete Qga
#endif
/* Return bytes consumed, or <= 0 on failure. Never retain the buffer. */
typedef long (*QgaWrite)(void*, const uchar*, long);
typedef int (*QgaQuery)(int, Jbuf*);
typedef int (*QgaLoadQuery)(void*, Jbuf*);
typedef int (*QgaShutdown)(void*, int);
/* Returns 0, -1 (GenericError), or -2 (Unsupported); description is trusted. */
typedef int (*QgaFileop)(void*, int, Jdoc*, int, Jbuf*, const char**);
typedef int (*QgaSettime)(void*, int, QgaInt64, const char**);

Qga *qganew(QgaWrite, void*);
void qgafree(Qga*);
/* Configure once, before the first feed; callbacks remain caller-owned. */
void qgaqueries(Qga*, QgaQuery);
void qgaloadops(Qga*, QgaLoadQuery, void*);
void qgashutdown(Qga*, QgaShutdown, void*);
void qgafileops(Qga*, QgaFileop, void*);
void qgasettime(Qga*, QgaSettime, void*);
void qgaexecops(Qga*, QgaFileop, void*);
/* One caller owns an instance; these functions are not reentrant. */
int qgafeed(Qga*, const uchar*, int);
int qgaend(Qga*);

#endif
