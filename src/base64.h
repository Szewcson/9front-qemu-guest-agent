/* SPDX-License-Identifier: MIT */
#ifndef QGA9_BASE64_H
#define QGA9_BASE64_H
#include "json.h"
int qgadecode64(const char*, int, uchar*, int);
void qgaencode64(Jbuf*, const uchar*, int);
#endif
