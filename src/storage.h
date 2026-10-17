/* SPDX-License-Identifier: MIT */
#ifndef QGA9_STORAGE_H
#define QGA9_STORAGE_H
#include "qjson.h"

enum {
	QgaDiskTextMax = 16384,
	QgaDiskPartMax = 64,
	QgaDiskUnitMax = 64,
	QgaDiskRecordMax = 128,
	QgaDiskNameMax = 64,
};

typedef struct QgaDiskPart QgaDiskPart;
typedef struct QgaDiskUnit QgaDiskUnit;
struct QgaDiskPart {
	char name[QgaDiskNameMax];
	QgaUint64 start;
	QgaUint64 end;
};
struct QgaDiskUnit {
	QgaUint64 sectors;
	QgaUint64 secsize;
	int npart;
	QgaDiskPart part[QgaDiskPartMax];
};

/* Immutable ctl input; caller owns the unit and discards it on parse failure. */
int qgadiskparse(QgaDiskUnit*, const char*, int);
int qgadiskunitname(const char*);
/* Cross-check npart immutable byte lengths. Missing optical geometry is derived
 * only from an actual whole-data extent and its observed length. On failure the
 * unit is unchanged; lengths must not overlap the caller-owned unit. */
int qgadisklengths(QgaDiskUnit*, const QgaUint64*);
/* Append entries inside a caller-owned JSON array; count also bounds all units. */
int qgadiskemit(Jbuf*, const char*, const QgaDiskUnit*, int*);

#endif
