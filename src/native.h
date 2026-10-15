/* SPDX-License-Identifier: MIT */
#include "json.h"
#include "files.h"
#include "process.h"

typedef struct ShutdownContext ShutdownContext;
struct ShutdownContext {
	int datafd;
	int ctlfd;
};

int nativequery(int, Jbuf*);
int nativedisks(Jbuf*);
/* Requires a private descriptor table; closes all except the bounded keep list. */
int nativecloseexcept(int*, int);
int nativeshutdown(void*, int);
QgaFiles *nativefiles(void);
int nativesettime(void*, int, QgaInt64, const char**);
QgaProcesses *nativeprocesses(void);
void nativeprocessfree(QgaProcesses*);
void *nativeloadnew(void);
int nativeloadquery(void*, Jbuf*);
void nativeloadfree(void*);
