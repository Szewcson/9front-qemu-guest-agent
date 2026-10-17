#define LIBJSON_HOST_SUPPORT
#include <u.h>
#include <libc.h>
#include <math.h>

typedef union Allocation Allocation;
union Allocation {
	max_align_t alignment;
	struct { uint64_t magic; } header;
};
static _Thread_local long live, remaining=-1;
static const uint64_t magic=UINT64_C(0x4a534f4e74657374);

void jsonhostfailafter(long n) { remaining=n; }
long jsonhostlive(void) { return live; }

void*
jsonhostmallocz(ulong n, int clear)
{
	Allocation *p;
	if(remaining == 0) return nil;
	if(remaining > 0) remaining--;
	p = malloc(sizeof *p+(size_t)n);
	if(p == nil) return nil;
	p->header.magic=magic;
	live++;
	if(clear) memset(p+1, 0, n);
	return p+1;
}

char*
jsonhoststrdup(const char *s)
{
	size_t n=strlen(s)+1;
	char *p;
	if(n > UINT32_MAX) abort();
	p=jsonhostmallocz((ulong)n, 0);
	if(p != nil) memcpy(p, s, n);
	return p;
}

void
jsonhostfree(void *p)
{
	Allocation *a;
	if(p == nil) return;
	a=(Allocation*)p-1;
	if(a->header.magic != magic || live <= 0) abort();
	a->header.magic=0;
	live--;
	free(a);
}

void werrstr(char *s, ...) { (void)s; }

int
fmtstrcpy(Fmt *f, char *s)
{
	int n=0;
	for(; *s; s++){
		if(f->to == f->stop && (f->flush == nil || !f->flush(f))) return -1;
		*(char*)f->to=*s;
		f->to=(char*)f->to+1;
		f->nfmt++;
		n++;
	}
	return n;
}

int
fmtprint(Fmt *f, char *format, ...)
{
	va_list args;
	char text[512];
	int n;
	va_start(args, format);
	n=vsnprintf(text, sizeof text, format, args);
	va_end(args);
	if(n < 0 || n >= (int)sizeof text) return -1;
	return fmtstrcpy(f, text);
}

int
fmtrune(Fmt *f, Rune r)
{
	char text[UTFmax+1];
	int n=runetochar(text, &r);
	if(n < 1 || n > UTFmax) return -1;
	text[n]=0;
	return fmtstrcpy(f, text);
}

int fmtinstall(int c, int(*fn)(Fmt*)) { (void)c; (void)fn; return 0; }
int isNaN(double n) { return isnan(n); }
int isInf(double n, int sign) { return isinf(n) && (sign == 0 || (sign < 0 ? n < 0 : n > 0)); }
