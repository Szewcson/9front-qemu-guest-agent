/* SPDX-License-Identifier: MIT */
#ifndef QGA9_PROCESS_H
#define QGA9_PROCESS_H
#include "qjson.h"
enum {
	QgaJobMax = 8, QgaArgMax = 32, QgaEnvMax = 32,
	QgaExecTextMax = 4096, QgaExecPathMax = 1024,
	QgaCaptureMax = 4096, QgaInputMax = 4096,
	QgaCaptureNone = 0, QgaCaptureStdout, QgaCaptureStderr,
	QgaCaptureSeparated, QgaCaptureMerged,
};
typedef struct QgaExecRequest QgaExecRequest;
struct QgaExecRequest {
	char path[QgaExecPathMax];
	char text[QgaExecTextMax];
	char *argv[QgaArgMax+2];
	char *env[QgaEnvMax+1];
	uchar input[QgaInputMax];
	int ninput, capture;
};
typedef struct QgaExecResult QgaExecResult;
struct QgaExecResult {
	int exited, exitcode;
	int nout, nerr, outtruncated, errtruncated;
	uchar out[QgaCaptureMax], err[QgaCaptureMax];
};
typedef struct QgaExecIO QgaExecIO;
/* Single owner; spawn copies the request before returning. A successful spawn
 * transfers one opaque handle. poll never blocks and cannot retain its buffer.
 * release consumes the handle once; cancel asks to terminate owned processes. */
struct QgaExecIO {
	int (*spawn)(void*, QgaExecRequest*, int*, void**);
	int (*poll)(void*, void*, QgaExecResult*);
	void (*release)(void*, void*, int);
};
typedef struct QgaProcesses QgaProcesses;
#ifndef QGA_HOST_TEST
#pragma incomplete QgaProcesses
#endif
QgaProcesses *qgaprocessnew(const QgaExecIO*, void*);
void qgaprocessfree(QgaProcesses*);
int qgaprocessop(void*, int, Jdoc*, int, Jbuf*, const char**);
#endif
