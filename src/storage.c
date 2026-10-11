/* SPDX-License-Identifier: MIT */
/* Native sd ctl grammar: no Linux identifiers, shell, or disk-data reads. */
#include "storage.h"

static int
letter(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int
digit(int c)
{
	return c >= '0' && c <= '9';
}

/* Components are used only below #S; no separators, dot traversal or quoting. */
static int
component(const char *s)
{
	int i, c;

	for(i = 0; s[i] != 0; i++){
		c = (uchar)s[i];
		if(i >= QgaDiskNameMax-1 || (!letter(c) && !digit(c) && c != '_' && c != '-' && c != '.'))
			return 0;
	}
	return i > 0 && strcmp(s, ".") != 0 && strcmp(s, "..") != 0 &&
		strcmp(s, "ctl") != 0 && strcmp(s, "raw") != 0;
}

static int
decimal(const char *s, QgaUint64 max, QgaUint64 *value)
{
	QgaUint64 n, d;
	int i;

	n = 0;
	for(i = 0; s[i] != 0; i++){
		if(i >= 20 || !digit(s[i]))
			return -1;
		d = s[i]-'0';
		if(d > max || n > (max-d)/10)
			return -1;
		n = n*10+d;
	}
	if(i == 0)
		return -1;
	*value = n;
	return 0;
}

int
qgadiskunitname(const char *s)
{
	QgaUint64 number;

	/* devsd.c: sd + one controller letter + decimal unit (0..255). */
	if(s == nil || s[0] != 's' || s[1] != 'd' ||
	   (!letter(s[2]) && !digit(s[2])) ||
	   decimal(s+3, 255, &number) < 0)
		return 0;
	return !(s[3] == '0' && s[4] != 0);
}

static int
fields(const char *text, int end, int *pos, char field[][QgaDiskNameMax], int max)
{
	int n, start, count;

	count = 0;
	while(*pos < end){
		if(text[*pos] == ' ' || text[*pos] == '\t' || text[*pos] == '\r'){
			(*pos)++;
			continue;
		}
		start = *pos;
		while(*pos < end && text[*pos] != ' ' && text[*pos] != '\t' && text[*pos] != '\r')
			(*pos)++;
		n = *pos-start;
		if(count == max || n >= QgaDiskNameMax)
			return -1;
		memmove(field[count], text+start, n);
		field[count++][n] = 0;
	}
	return count;
}

int
qgadiskparse(QgaDiskUnit *u, const char *text, int size)
{
	char field[8][QgaDiskNameMax];
	QgaUint64 ignored, max;
	QgaDiskPart *p;
	int pos, end, start, n, geometry, inquiry, i;

	if(u == nil || text == nil || size <= 0 || size > QgaDiskTextMax ||
	   text[size-1] != '\n' || memchr(text, 0, size) != nil)
		return -1;
	memset(u, 0, sizeof *u);
	max = ((QgaUint64)1 << 63)-1;
	pos = geometry = inquiry = 0;
	while(pos < size){
		start = pos;
		for(end = pos; end < size && text[end] != '\n'; end++)
			;
		/* Ignore device-specific diagnostic lines without tokenizing long dumps. */
		while(pos < end && (text[pos] == ' ' || text[pos] == '\t'))
			pos++;
		n = pos;
		while(n < end && text[n] != ' ' && text[n] != '\t' && text[n] != '\r')
			n++;
		if(n-pos == 7 && memcmp(text+pos, "inquiry", 7) == 0){
			if(inquiry || start != 0)
				return -1;
			inquiry = 1;
		}else if((n-pos == 8 && memcmp(text+pos, "geometry", 8) == 0) ||
		         (n-pos == 4 && memcmp(text+pos, "part", 4) == 0)){
			n = fields(text, end, &pos, field, 8);
			if(n < 0)
				return -1;
			if(strcmp(field[0], "geometry") == 0){
				if(geometry || n < 3 || decimal(field[1], max, &u->sectors) < 0 ||
				   decimal(field[2], 4294967295ULL, &u->secsize) < 0 ||
				   u->secsize == 0 || u->sectors > max/u->secsize)
					return -1;
				for(i = 3; i < n; i++)
					if(decimal(field[i], max, &ignored) < 0)
						return -1;
				geometry = 1;
			}else{
				if(!geometry || n != 4 || u->npart == QgaDiskPartMax || !component(field[1]))
					return -1;
				p = &u->part[u->npart];
				if(decimal(field[2], max, &p->start) < 0 || decimal(field[3], max, &p->end) < 0 ||
				   p->start > p->end || p->end > u->sectors)
					return -1;
				for(i = 0; i < u->npart; i++)
					if(strcmp(u->part[i].name, field[1]) == 0)
						return -1;
				memmove(p->name, field[1], strlen(field[1])+1);
				u->npart++;
			}
		}
		pos = end+1;
	}
	/* Inquiry-only is a real unit with no block media (e.g. empty removable). */
	return inquiry ? 0 : -1;
}

static void
node(Jbuf *b, const char *unit, const char *part)
{
	/* Both components were validated before any serialization. */
	jbtext(b, "\"#S/");
	jbtext(b, unit);
	jbchar(b, '/');
	jbtext(b, part);
	jbchar(b, '"');
}

int
qgadiskemit(Jbuf *b, const char *name, const QgaDiskUnit *u, int *count)
{
	const QgaDiskPart *p;
	QgaUint64 max;
	int i, j, whole;

	if(b == nil || u == nil || count == nil || !qgadiskunitname(name) ||
	   u->npart < 0 || u->npart > QgaDiskPartMax || *count < 0 || *count > QgaDiskRecordMax)
		return -1;
	max = ((QgaUint64)1 << 63)-1;
	if(u->npart > 0 && (u->secsize == 0 || u->secsize > 4294967295ULL || u->sectors > max/u->secsize))
		return -1;
	whole = -1;
	for(i = 0; i < u->npart; i++){
		p = &u->part[i];
		if(memchr(p->name, 0, sizeof p->name) == nil || !component(p->name) ||
		   p->start > p->end || p->end > u->sectors)
			return -1;
		for(j = 0; j < i; j++)
			if(strcmp(u->part[j].name, p->name) == 0)
				return -1;
		if(strcmp(p->name, "data") == 0 && p->start == 0 && p->end == u->sectors && u->sectors != 0)
			whole = i;
	}
	for(i = 0; i < u->npart; i++){
		p = &u->part[i];
		if(p->start == p->end)
			continue;
		if(*count == QgaDiskRecordMax)
			return -1;
		if((*count)++ != 0)
			jbchar(b, ',');
		jbtext(b, "{\"name\":");
		node(b, name, p->name);
		jbtext(b, i == whole ? ",\"partition\":false" : ",\"partition\":true");
		if(whole >= 0 && i != whole){
			jbtext(b, ",\"dependencies\":[");
			node(b, name, u->part[whole].name);
			jbchar(b, ']');
		}
		jbchar(b, '}');
	}
	return b->failed ? -1 : 0;
}
