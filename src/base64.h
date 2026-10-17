/* SPDX-License-Identifier: MIT */
#ifndef QGA9_BASE64_H
#define QGA9_BASE64_H
#include "qjson.h"
/* Borrowed, nonoverlapping input/output. Invalid input or insufficient capacity
 * leaves the destination untouched. Native conversion uses libc enc64/dec64. */
int qgadecode64(const char*, int, uchar*, int);
void qgaencode64(Jbuf*, const uchar*, int);
#endif
