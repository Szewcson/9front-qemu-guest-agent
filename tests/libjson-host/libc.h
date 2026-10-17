/* Test primitives only; the parser/formatter under test are native sources. */
#ifndef LIBJSON_HOST_LIBC_H
#define LIBJSON_HOST_LIBC_H
#include <u.h>
typedef struct Fmt Fmt;
struct Fmt {
	uchar runes;
	void *start, *to, *stop;
	int (*flush)(Fmt*);
	void *farg;
	int nfmt;
	va_list args;
	int r, width, prec;
	ulong flags;
};
enum { FmtSharp=8 };
void *jsonhostmallocz(ulong, int);
char *jsonhoststrdup(const char*);
void jsonhostfree(void*);
void jsonhostfailafter(long);
long jsonhostlive(void);
void werrstr(char*, ...);
int fullrune(char*, int);
int chartorune(Rune*, char*);
int runetochar(char*, Rune*);
int fmtstrcpy(Fmt*, char*);
int fmtprint(Fmt*, char*, ...);
int fmtrune(Fmt*, Rune);
int fmtinstall(int, int(*)(Fmt*));
int isNaN(double);
int isInf(double, int);
#if defined(LIBJSON_HOST_TEST) && !defined(LIBJSON_HOST_SUPPORT)
#define mallocz jsonhostmallocz
#define strdup jsonhoststrdup
#define free jsonhostfree
#endif
#endif
