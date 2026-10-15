/* SPDX-License-Identifier: MIT */
#include "json.h"

#ifdef QGA_HOST_TEST
#include <stdio.h>
#define snprint snprintf
int qgahostrunetochar(char*, unsigned int);
#endif

static int
hex(int c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int
hex4(const char *s, int n, int *p, unsigned int *r)
{
	int i, h;

	if(*p < 0 || n - *p < 4)
		return 0;
	*r = 0;
	for(i = 0; i < 4; i++){
		h = hex((uchar)s[(*p)++]);
		if(h < 0)
			return 0;
		*r = (*r << 4) | h;
	}
	return 1;
}

/* Decode one JSON string character; reject invalid UTF-8 and surrogates. */
static int
character(const char *s, int n, int *p, unsigned int *r)
{
	unsigned int c, low, min;
	int need, i;

	if(*p < 0 || *p >= n)
		return 0;
	c = (uchar)s[(*p)++];
	if(c == '\\'){
		if(*p >= n)
			return 0;
		c = (uchar)s[(*p)++];
		switch(c){
		case '"': case '\\': case '/': *r = c; return 1;
		case 'b': *r = 8; return 1;
		case 'f': *r = 12; return 1;
		case 'n': *r = 10; return 1;
		case 'r': *r = 13; return 1;
		case 't': *r = 9; return 1;
		case 'u':
			if(!hex4(s, n, p, &c))
				return 0;
			if(c >= 0xD800 && c <= 0xDBFF){
				if(n - *p < 6 || s[*p] != '\\' || s[*p+1] != 'u')
					return 0;
				*p += 2;
				if(!hex4(s, n, p, &low) || low < 0xDC00 || low > 0xDFFF)
					return 0;
				c = 0x10000 + ((c - 0xD800) << 10) + low - 0xDC00;
			}else if(c >= 0xDC00 && c <= 0xDFFF)
				return 0;
			*r = c;
			return 1;
		default:
			return 0;
		}
	}
	if(c < 0x20)
		return 0;
	if(c < 0x80){
		*r = c;
		return 1;
	}
	if(c >= 0xC2 && c <= 0xDF){
		need = 1;
		min = 0x80;
		c &= 0x1F;
	}else if(c >= 0xE0 && c <= 0xEF){
		need = 2;
		min = 0x800;
		c &= 0x0F;
	}else if(c >= 0xF0 && c <= 0xF4){
		need = 3;
		min = 0x10000;
		c &= 7;
	}else
		return 0;
	if(n - *p < need)
		return 0;
	for(i = 0; i < need; i++){
		low = (uchar)s[(*p)++];
		if((low & 0xC0) != 0x80)
			return 0;
		c = (c << 6) | (low & 0x3F);
	}
	if(c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
		return 0;
	*r = c;
	return 1;
}

static void
space(Jdoc *d)
{
	while(d->pos < d->size){
		switch(d->text[d->pos]){
		case ' ': case '\t': case '\r': case '\n': d->pos++; break;
		default: return;
		}
	}
}

static int
newtoken(Jdoc *d, int type)
{
	int i;

	if(d->ntoken >= QgaTokenMax)
		return -1;
	i = d->ntoken++;
	d->token[i].type = type;
	d->token[i].start = d->pos;
	d->token[i].count = 0;
	return i;
}

static int
string(Jdoc *d)
{
	int i;
	unsigned int r;

	i = newtoken(d, Jstring);
	if(i < 0)
		return -1;
	d->pos++;
	while(d->pos < d->size && d->text[d->pos] != '"')
		if(!character(d->text, d->size, &d->pos, &r))
			return -1;
	if(d->pos >= d->size)
		return -1;
	d->token[i].end = ++d->pos;
	d->token[i].next = d->ntoken;
	return i;
}

static int
equalkeys(Jdoc *d, int a, int b)
{
	int p, q, e, f;
	unsigned int r, s;

	p = d->token[a].start + 1;
	q = d->token[b].start + 1;
	e = d->token[a].end - 1;
	f = d->token[b].end - 1;
	while(p < e && q < f){
		if(!character(d->text, e, &p, &r) || !character(d->text, f, &q, &s))
			return 0;
		if(r != s)
			return 0;
	}
	return p == e && q == f;
}

static int value(Jdoc*, int);

static int
container(Jdoc *d, int type, int depth)
{
	int i, key, prev, close;

	i = newtoken(d, type);
	if(i < 0)
		return -1;
	close = type == Jobject ? '}' : ']';
	d->pos++;
	space(d);
	if(d->pos < d->size && d->text[d->pos] != close){
		for(;;){
			if(type == Jobject){
				if(d->pos >= d->size || d->text[d->pos] != '"')
					return -1;
				key = string(d);
				if(key < 0)
					return -1;
				/* Bounded quadratic check; compare decoded keys, not spelling. */
				for(prev = i+1; prev < key; prev = d->token[prev+1].next)
					if(equalkeys(d, prev, key))
						return -1;
				space(d);
				if(d->pos >= d->size || d->text[d->pos++] != ':')
					return -1;
			}
			if(value(d, depth+1) < 0)
				return -1;
			d->token[i].count++;
			space(d);
			if(d->pos >= d->size)
				return -1;
			if(d->text[d->pos] == close)
				break;
			if(d->text[d->pos++] != ',')
				return -1;
			space(d);
		}
	}
	if(d->pos >= d->size || d->text[d->pos++] != close)
		return -1;
	d->token[i].end = d->pos;
	d->token[i].next = d->ntoken;
	return i;
}

static int
digit(Jdoc *d)
{
	return d->pos < d->size && d->text[d->pos] >= '0' && d->text[d->pos] <= '9';
}

static int
number(Jdoc *d)
{
	int i;

	i = newtoken(d, Jnumber);
	if(i < 0)
		return -1;
	if(d->text[d->pos] == '-')
		d->pos++;
	if(!digit(d))
		return -1;
	if(d->text[d->pos++] != '0')
		while(digit(d))
			d->pos++;
	if(d->pos < d->size && d->text[d->pos] == '.'){
		d->pos++;
		if(!digit(d))
			return -1;
		while(digit(d))
			d->pos++;
	}
	if(d->pos < d->size && (d->text[d->pos] == 'e' || d->text[d->pos] == 'E')){
		d->pos++;
		if(d->pos < d->size && (d->text[d->pos] == '+' || d->text[d->pos] == '-'))
			d->pos++;
		if(!digit(d))
			return -1;
		while(digit(d))
			d->pos++;
	}
	d->token[i].end = d->pos;
	d->token[i].next = d->ntoken;
	return i;
}

static int
literal(Jdoc *d, int type, const char *s)
{
	int i, n;

	i = newtoken(d, type);
	/* Native libc lacks const-qualified read-only argument declarations. */
	n = strlen((char*)s);
	if(i < 0 || d->size - d->pos < n || memcmp((void*)(d->text + d->pos), (void*)s, n) != 0)
		return -1;
	d->pos += n;
	d->token[i].end = d->pos;
	d->token[i].next = d->ntoken;
	return i;
}

static int
value(Jdoc *d, int depth)
{
	space(d);
	if(depth > QgaDepthMax || d->pos >= d->size)
		return -1;
	switch(d->text[d->pos]){
	case '{': return container(d, Jobject, depth);
	case '[': return container(d, Jarray, depth);
	case '"': return string(d);
	case 't': return literal(d, Jtrue, "true");
	case 'f': return literal(d, Jfalse, "false");
	case 'n': return literal(d, Jnull, "null");
	default:
		if(d->text[d->pos] == '-' || digit(d))
			return number(d);
		return -1;
	}
}

static int
parse(Jdoc *d, const char *s, int n, int limit)
{
	if(d == nil || s == nil || n < 0 || n > limit)
		return 0;
	memset(d, 0, sizeof *d);
	d->text = s;
	d->size = n;
	if(value(d, 0) < 0)
		return 0;
	space(d);
	return d->pos == n;
}

int
jparse(Jdoc *d, const char *s, int n)
{
	return parse(d, s, n, QgaMessageMax);
}

int
jparseresponse(Jdoc *d, const char *s, int n)
{
	return parse(d, s, n, QgaOutputMax);
}

int
jmatch(Jdoc *d, int t, const char *s)
{
	int p, e;
	unsigned int r;

	if(t < 0 || t >= d->ntoken || d->token[t].type != Jstring)
		return 0;
	p = d->token[t].start + 1;
	e = d->token[t].end - 1;
	while(p < e){
		if(*s == 0 || !character(d->text, e, &p, &r) || r != (uchar)*s++)
			return 0;
	}
	return *s == 0;
}

int
jmember(Jdoc *d, int t, const char *s)
{
	int i;

	if(t < 0 || t >= d->ntoken || d->token[t].type != Jobject)
		return -1;
	for(i = t+1; i < d->token[t].next; i = d->token[i+1].next)
		if(jmatch(d, i, s))
			return i+1;
	return -1;
}

/* Lexical range check: no floating point or signed-overflow conversion. */
int
jint64(Jdoc *d, int t)
{
	int p, n, i;
	const char *limit;

	if(t < 0 || t >= d->ntoken || d->token[t].type != Jnumber)
		return 0;
	p = d->token[t].start;
	limit = "9223372036854775807";
	if(d->text[p] == '-'){
		p++;
		limit = "9223372036854775808";
	}
	n = d->token[t].end - p;
	if(n < 1 || n > 19)
		return 0;
	for(i = 0; i < n; i++)
		if(d->text[p+i] < '0' || d->text[p+i] > '9')
			return 0;
	return n < 19 || memcmp((void*)(d->text+p), (void*)limit, 19) <= 0;
}

int
jgetint64(Jdoc *d, int t, QgaInt64 *value)
{
	QgaUint64 n;
	int p, negative;

	if(d == nil || value == nil || !jint64(d, t))
		return -1;
	p = d->token[t].start;
	negative = d->text[p] == '-';
	if(negative)
		p++;
	n = 0;
	while(p < d->token[t].end)
		n = n*10 + d->text[p++]-'0';
	/* The lexical check bounds n. Never cast 2^63 or negate INT64_MIN. */
	*value = negative && n != 0 ? -(QgaInt64)(n-1)-1 : (QgaInt64)n;
	return 0;
}

int
jgetstring(Jdoc *d, int t, char *out, int size)
{
	int p, end, used, n, i;
	unsigned int r;
	uchar bytes[4];
#ifndef QGA_HOST_TEST
	Rune rune;
#endif

	if(d == nil || out == nil || size < 1 || t < 0 || t >= d->ntoken || d->token[t].type != Jstring)
		return -1;
	out[0] = 0;
	p = d->token[t].start+1;
	end = d->token[t].end-1;
	used = 0;
	while(p < end){
		if(!character(d->text, end, &p, &r) || r == 0)
			return -1;
		/* character() already rejected invalid scalars; native libc encodes.
		 * Keep the bounded JSON decoder and capacity check, not chartorune. */
#ifdef QGA_HOST_TEST
		n = qgahostrunetochar((char*)bytes, r);
#else
		rune = r;
		n = runetochar((char*)bytes, &rune);
#endif
		if(n < 1 || n > (int)sizeof bytes || n > size-1-used)
			return -1;
		for(i = 0; i < n; i++)
			out[used++] = bytes[i];
	}
	out[used] = 0;
	return used;
}

void
jbint64(Jbuf *b, QgaInt64 value)
{
	char text[21];
	int n;

	/* Proven capacity: sign + 19 decimal digits + NUL, including INT64_MIN.
	 * Native snprint reports actual bytes, so do not rely on truncation counts. */
	n = snprint(text, sizeof text, "%lld", value);
	if(n < 1 || n >= (int)sizeof text){ b->failed = 1; return; }
	jbraw(b, text, n);
}

void
jbinit(Jbuf *b, char *s, int n)
{
	b->text = s;
	b->size = n;
	b->used = 0;
	b->failed = s == nil || n < 0;
}

void
jbraw(Jbuf *b, const char *s, int n)
{
	if(b->failed)
		return;
	if(s == nil || n < 0 || n > b->size - b->used){
		b->failed = 1;
		return;
	}
	memmove(b->text + b->used, (void*)s, n);
	b->used += n;
}

void
jbtext(Jbuf *b, const char *s)
{
	jbraw(b, s, strlen((char*)s));
}

void
jbchar(Jbuf *b, int c)
{
	char s;

	s = c;
	jbraw(b, &s, 1);
}

static void
escaped(Jbuf *b, unsigned int r)
{
	const char *digits = "0123456789abcdef";
	int i;

	if(r == '"' || r == '\\'){
		jbchar(b, '\\');
		jbchar(b, r);
	}else if(r >= 0x20 && r < 0x7F)
		jbchar(b, r);
	else if(r <= 0xFFFF){
		jbtext(b, "\\u");
		for(i = 12; i >= 0; i -= 4)
			jbchar(b, digits[(r >> i) & 15]);
	}else{
		r -= 0x10000;
		escaped(b, 0xD800 + (r >> 10));
		escaped(b, 0xDC00 + (r & 1023));
	}
}

/* For trusted ASCII constants, not an input JSON string. */
void
jbquote(Jbuf *b, const char *s)
{
	jbchar(b, '"');
	while(*s != 0)
		escaped(b, (uchar)*s++);
	jbchar(b, '"');
}

/* Serialize literal UTF-8 bytes from native files, not JSON escape spelling. */
void
jbstring(Jbuf *b, const char *s, int n)
{
	int p;
	unsigned int r;

	if(s == nil || n < 0){
		b->failed = 1;
		return;
	}
	jbchar(b, '"');
	for(p = 0; p < n;){
		if((uchar)s[p] < 0x80)
			r = (uchar)s[p++];
		else if(!character(s, n, &p, &r)){
			b->failed = 1;
			return;
		}
		escaped(b, r);
	}
	jbchar(b, '"');
}

void
jbvalue(Jbuf *b, Jdoc *d, int t)
{
	int i, p, e, first;
	unsigned int r;
	Jtoken *v;

	if(t < 0 || t >= d->ntoken){
		b->failed = 1;
		return;
	}
	v = &d->token[t];
	switch(v->type){
	case Jobject: case Jarray:
		jbchar(b, v->type == Jobject ? '{' : '[');
		first = 1;
		for(i = t+1; i < v->next; i = d->token[i].next){
			if(!first)
				jbchar(b, ',');
			first = 0;
			jbvalue(b, d, i);
			if(v->type == Jobject){
				jbchar(b, ':');
				i++;
				jbvalue(b, d, i);
			}
		}
		jbchar(b, v->type == Jobject ? '}' : ']');
		break;
	case Jstring:
		jbchar(b, '"');
		p = v->start + 1;
		e = v->end - 1;
		while(p < e){
			if(!character(d->text, e, &p, &r)){
				b->failed = 1;
				return;
			}
			escaped(b, r);
		}
		jbchar(b, '"');
		break;
	default:
		jbraw(b, d->text + v->start, v->end - v->start);
		break;
	}
}
