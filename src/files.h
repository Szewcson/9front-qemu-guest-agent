/* SPDX-License-Identifier: MIT */
#ifndef QGA9_FILES_H
#define QGA9_FILES_H
#include "qjson.h"

enum {
	QgaFileMax = 16,
	QgaFileChunk = 4096,
	QgaFilePathMax = 1024,
	QgaFileBase64Max = ((QgaFileChunk+2)/3)*4,
	QgaOpenRead = 1,
	QgaOpenWrite = 2,
	QgaOpenTruncate = 4,
};

typedef struct QgaFiles QgaFiles;
#ifndef QGA_HOST_TEST
#pragma incomplete QgaFiles
#endif
typedef struct QgaFileIO QgaFileIO;
/* Trusted synchronous OS callbacks; no callback retains supplied pointers.
 * Negative results are errors; close consumes its fd even on an error.
 * seek checks signed arithmetic and returns a nonnegative absolute position.
 * Writes are unbuffered, so flush has no pending user-space buffer to drain. */
struct QgaFileIO {
	int (*open)(void*, const char*, int);
	int (*close)(void*, int);
	int (*read)(void*, int, uchar*, int);
	int (*write)(void*, int, const uchar*, int);
	int (*seek)(void*, int, QgaInt64, int, QgaInt64*);
};

QgaFiles *qgafilesnew(const QgaFileIO*, void*);
/* One synchronous owner. Cleanup closes every still-owned descriptor once. */
void qgafilesfree(QgaFiles*);
int qgafilesop(void*, int, Jdoc*, int, Jbuf*, const char**);

#endif
