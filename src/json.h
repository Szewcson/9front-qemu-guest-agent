/* SPDX-License-Identifier: MIT */
#ifndef QGA9_JSON_H
#define QGA9_JSON_H
#include "qga.h"

enum { Jobject, Jarray, Jstring, Jnumber, Jtrue, Jfalse, Jnull };

typedef struct Jtoken Jtoken;

struct Jtoken {
	int type;
	int start;
	int end;
	int next;
	int count;
};

struct Jdoc {
	const char *text;             /* Borrowed until response serialization. */
	int size;
	int pos;
	int ntoken;
	Jtoken token[QgaTokenMax];
};

int jparse(Jdoc*, const char*, int);
/* Test/client-side responses; daemon request parsing remains capped at 8192. */
int jparseresponse(Jdoc*, const char*, int);
int jmember(Jdoc*, int, const char*);
int jmatch(Jdoc*, int, const char*);
int jint64(Jdoc*, int);
int jgetint64(Jdoc*, int, QgaInt64*);
/* Decode UTF-8 into caller storage, terminate it, and reject interior NUL. */
int jgetstring(Jdoc*, int, char*, int);
void jbint64(Jbuf*, QgaInt64);
void jbinit(Jbuf*, char*, int);
void jbraw(Jbuf*, const char*, int);
void jbtext(Jbuf*, const char*);
void jbchar(Jbuf*, int);
void jbquote(Jbuf*, const char*);
void jbstring(Jbuf*, const char*, int);
void jbvalue(Jbuf*, Jdoc*, int);

#endif
