/* SPDX-License-Identifier: MIT */
#ifndef QGA9_LOAD_H
#define QGA9_LOAD_H
#include "json.h"
enum { QgaLoadProcessesMax=4096, QgaLoadScale=1000000, QgaLoadWire=25 };
typedef struct QgaLoad QgaLoad;
struct QgaLoad { QgaUint64 average[3], last; int initialized, pending; };
/* Sole sampler owner. Monotonic process-elapsed milliseconds, modulo 2^32.
 * No allocation/shared state; failed samples cannot masquerade as zero load. */
void qgaloadinit(QgaLoad*);
int qgaloadsample(QgaLoad*, int, QgaUint64);
int qgaloademit(Jbuf*, const QgaUint64*);
int qgaloadstate(const char*, int); /* stock #p status: 1 runnable, 0 other, -1 malformed */
#endif
