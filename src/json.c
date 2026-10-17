/* SPDX-License-Identifier: MIT */
/* Protocol policy and bounded storage adapter, not a JSON grammar parser. */
#include "qjson.h"

#ifdef QGA_HOST_TEST
#include <stdio.h>
#define snprint snprintf
#endif

static int
newtoken(Jdoc *d, int type)
{
	int i;

	if(d->ntoken >= QgaTokenMax)
		return -1;
	i = d->ntoken++;
	d->token[i].type = type;
	return i;
}

/* Primitive spellings/decoded text fit the input size plus one: strings lose
 * quotes, and separate numbers already require at least one delimiter byte.
 * Still check every copy; no ownership escapes the temporary native tree. */
static char*
copytext(Jdoc *d, char *s, ulong n)
{
	char *out;

	if(s == nil || n >= sizeof d->data-d->used)
		return nil;
	out = d->data+d->used;
	memmove(out, s, n);
	out[n] = 0;
	d->used += n+1;
	return out;
}

static int
stringtoken(Jdoc *d, char *s, ulong n)
{
	int i;
	Jtoken *t;

	i = newtoken(d, Jstring);
	if(i < 0)
		return -1;
	t = &d->token[i];
	t->text.s = copytext(d, s, n);
	if(t->text.s == nil)
		return -1;
	t->text.n = n;
	t->value.t = JSONStringN;
	t->value.text = &t->text;
	t->next = d->ntoken;
	return i;
}

static int
flatten(Jdoc *d, JSON *j, int depth)
{
	JSONEl *e, **tail;
	JSONElN *edge;
	Jtoken *t;
	char *s;
	ulong n;
	int i, key, child, type;

	if(j == nil || depth > QgaDepthMax)
		return -1;
	if(j->t == JSONStringN){
		s = jsonstring(j, &n);
		return stringtoken(d, s, n);
	}
	switch(j->t){
	case JSONObjectN: type = Jobject; break;
	case JSONArray: type = Jarray; break;
	case JSONNumberText: type = Jnumber; break;
	case JSONBool: type = j->n ? Jtrue : Jfalse; break;
	case JSONNull: type = Jnull; break;
	default: return -1;
	}
	i = newtoken(d, type);
	if(i < 0)
		return -1;
	t = &d->token[i];
	t->value = *j;
	if(type == Jnumber){
		t->value.s = copytext(d, j->s, strlen(j->s));
		if(t->value.s == nil)
			return -1;
	}else if(type == Jobject || type == Jarray){
		t->value.first = nil;
		tail = &t->value.first;
		for(e = j->first; e != nil; e = e->next){
			key = -1;
			if(type == Jobject){
				s = jsonkey(j, e, &n);
				key = stringtoken(d, s, n);
				if(key < 0)
					return -1;
			}
			child = flatten(d, e->val, depth+1);
			if(child < 0)
				return -1;
			edge = &d->token[key >= 0 ? key : child].edge;
			if(key >= 0){
				edge->el.name = d->token[key].text.s;
				edge->n = d->token[key].text.n;
			}
			edge->el.val = &d->token[child].value;
			*tail = &edge->el;
			tail = &edge->el.next;
			t->count++;
		}
	}
	t->next = d->ntoken;
	return i;
}

static int
parse(Jdoc *d, const char *s, int n, int limit)
{
	JSONLimits limits;
	JSON *j;
	int ok;

	if(d == nil)
		return 0;
	memset(d, 0, sizeof *d);
	if(s == nil || n < 0 || n > limit)
		return 0;
	limits.maxdepth = QgaDepthMax;
	limits.maxnodes = QgaTokenMax;
	limits.maxbytes = limit == QgaMessageMax ? 65536 : 262144;
	j = jsonparsebounded((char*)s, n, &limits);
	if(j == nil)
		return 0;
	ok = flatten(d, j, 0) == 0;
	jsonfree(j);
	if(!ok){
		d->ntoken = 0;
		d->used = 0;
	}
	return ok;
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
	char *text;
	ulong n;

	if(d == nil || s == nil || t < 0 || t >= d->ntoken || d->token[t].type != Jstring)
		return 0;
	text = jsonstring(&d->token[t].value, &n);
	return text != nil && n == strlen((char*)s) && memcmp(text, (void*)s, n) == 0;
}

int
jmember(Jdoc *d, int t, const char *s)
{
	int i;

	if(d == nil || t < 0 || t >= d->ntoken || d->token[t].type != Jobject)
		return -1;
	for(i = t+1; i < d->token[t].next; i = d->token[i+1].next)
		if(jmatch(d, i, s))
			return i+1;
	return -1;
}

int
jgetint64(Jdoc *d, int t, QgaInt64 *value)
{
	vlong n;

	if(d == nil || value == nil || t < 0 || t >= d->ntoken ||
	   jsonint64(&d->token[t].value, &n) < 0)
		return -1;
	*value = n;
	return 0;
}

int
jint64(Jdoc *d, int t)
{
	QgaInt64 n;

	return jgetint64(d, t, &n) == 0;
}

int
jgetstring(Jdoc *d, int t, char *out, int size)
{
	char *s;
	ulong n;

	if(d == nil || out == nil || size < 1 || t < 0 || t >= d->ntoken)
		return -1;
	out[0] = 0;
	s = jsonstring(&d->token[t].value, &n);
	if(s == nil || n >= (ulong)size || memchr(s, 0, n) != nil)
		return -1;
	memmove(out, s, n);
	out[n] = 0;
	return n;
}

void
jbint64(Jbuf *b, QgaInt64 value)
{
	char text[21];
	int n;

	/* Native integer formatting: sign + 19 digits + NUL, no double conversion. */
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

static int
full(Fmt *f)
{
	((Jbuf*)f->farg)->failed = 1;
	return 0;
}

static void
writevalue(Jbuf *b, JSON *j)
{
	Fmt f;

	if(b->failed)
		return;
	memset(&f, 0, sizeof f);
	f.start = f.to = b->text+b->used;
	f.stop = b->text+b->size;
	f.flush = full;
	f.farg = b;
	if(jsonfmt(&f, j, JSONCompact|JSONAscii) < 0 || b->failed){
		b->failed = 1;
		return;
	}
	b->used = (char*)f.to-b->text;
}

void
jbquote(Jbuf *b, const char *s)
{
	if(s == nil){ b->failed = 1; return; }
	jbstring(b, s, strlen((char*)s));
}

void
jbstring(Jbuf *b, const char *s, int n)
{
	JSON j;
	JSONText text;

	if(s == nil || n < 0){ b->failed = 1; return; }
	text.s = (char*)s;
	text.n = n;
	j.t = JSONStringN;
	j.text = &text;
	writevalue(b, &j);
}

void
jbvalue(Jbuf *b, Jdoc *d, int t)
{
	if(d == nil || t < 0 || t >= d->ntoken){ b->failed = 1; return; }
	writevalue(b, &d->token[t].value);
}
