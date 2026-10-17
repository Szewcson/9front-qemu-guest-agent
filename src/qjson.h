/* SPDX-License-Identifier: MIT */
#ifndef QGA9_JSON_H
#define QGA9_JSON_H
#include "qga.h"
#ifdef QGA_HOST_TEST
#include <u.h>
#include <libc.h>
#endif
#include <json.h>

enum { Jobject, Jarray, Jstring, Jnumber, Jtrue, Jfalse, Jnull };

typedef struct Jtoken Jtoken;

struct Jtoken {
	int type;
	int next;
	int count;
	JSON value;
	JSONText text;
	JSONElN edge;
};

struct Jdoc {
	/* Fixed, self-referential tree views. Do not copy a parsed Jdoc or jsonfree
	 * these nodes; parse releases the temporary library tree before returning. */
	int used;
	int ntoken;
	Jtoken token[QgaTokenMax];
	char data[QgaOutputMax+1];
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
