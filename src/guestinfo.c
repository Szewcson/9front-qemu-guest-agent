/* SPDX-License-Identifier: MIT */
/* Strict native text decoders, shared unchanged by native and host tests. */
#include "guestinfo.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#define snprint snprintf
#endif

static void
number(Jbuf *b, unsigned long value)
{
	char text[32];
	int n;
#ifdef QGA_HOST_TEST
	n = snprint(text, sizeof text, "%lu", value);
#else
	n = snprint(text, sizeof text, "%lud", value);
#endif
	if(n < 1 || n >= (int)sizeof text){ b->failed = 1; return; }
	jbraw(b, text, n);
}

static int
space(int c)
{
	return c == ' ' || c == '\t' || c == '\r';
}

/* Consume exactly one bounded line. No strtok/global state, input is immutable. */
static int
fields(const char *text, int size, int *pos, char field[][QgaIpTextMax], int max)
{
	int count, start, n;

	count = 0;
	while(*pos < size && text[*pos] != '\n'){
		if(space((uchar)text[*pos])){
			(*pos)++;
			continue;
		}
		start = *pos;
		while(*pos < size && !space((uchar)text[*pos]) && text[*pos] != '\n'){
			if(text[*pos] == 0)
				return -1;
			(*pos)++;
		}
		n = *pos-start;
		if(count == max || n >= QgaIpTextMax)
			return -1;
		memmove(field[count], text+start, n);
		field[count++][n] = 0;
	}
	if(*pos < size)
		(*pos)++;
	return count;
}

static int
decimal(const char *text, int max, int *value)
{
	int i, n, digit;

	n = 0;
	for(i = 0; text[i] != 0; i++){
		digit = text[i]-'0';
		if(digit < 0 || digit > 9 || n > (max-digit)/10 || digit > max)
			return -1;
		n = n*10 + digit;
	}
	if(i == 0)
		return -1;
	*value = n;
	return 0;
}

static int
counter(const char *text)
{
	int i;

	for(i = 0; text[i] != 0; i++)
		if(i >= 20 || text[i] < '0' || text[i] > '9')
			return -1;
	return i == 0 ? -1 : 0;
}

int
qgacpus(Jbuf *b, const char *text, int size)
{
	char field[10][QgaIpTextMax];
	int ids[QgaCpuMax], pos, count, n, id, i;

	if(text == nil || size < 0 || size > QgaNativeTextMax)
		return -1;
	pos = count = 0;
	jbchar(b, '[');
	while(pos < size){
		n = fields(text, size, &pos, field, 10);
		if(n == 0)
			continue;
		if(n != 10 || count == QgaCpuMax || decimal(field[0], 2147483647, &id) < 0)
			return -1;
		for(i = 1; i < n; i++)
			if(counter(field[i]) < 0)
				return -1;
		for(i = 0; i < count; i++)
			if(ids[i] == id)
				return -1;
		ids[count] = id;
		if(count++ != 0)
			jbchar(b, ',');
		jbtext(b, "{\"logical-id\":");
		number(b, id);
		/* #c/sysstat emits only active.machs; there is no CPU hotplug API. */
		jbtext(b, ",\"online\":true,\"can-offline\":false}");
	}
	jbchar(b, ']');
	return count == 0 || b->failed ? -1 : 0;
}

int
qgatimezone(Jbuf *b, const char *zone, int size, int offset)
{
	if(zone == nil || size < 0 || size > 64 || offset < -86400 || offset > 86400 ||
	   memchr(zone, 0, size) != nil)
		return -1;
	jbtext(b, "{\"offset\":");
	if(offset < 0){
		jbchar(b, '-');
		offset = -offset;             /* Validated before signed negation. */
	}
	number(b, offset);
	if(size != 0){
		jbtext(b, ",\"zone\":");
		jbstring(b, zone, size);
	}
	jbchar(b, '}');
	return b->failed ? -1 : 0;
}

/* Mirrors iproute.c's format; no Linux flag/metric semantics are invented. */
static int
routetype(const char *text, int *direct)
{
	const char *letters = "iubmpyt";
	const char *p;
	int seen, bit, i;

	if(text[0] != '4' && text[0] != '6')
		return -1;
	seen = 0;
	for(i = 1; text[i] != 0; i++){
		p = strchr((char*)letters, text[i]);
		if(p == nil)
			return -1;
		bit = 1 << (p-letters);
		if(seen & bit)
			return -1;
		seen |= bit;
	}
	if((!!(seen & 2) + !!(seen & 4) + !!(seen & 8)) > 1)
		return -1;
	*direct = (seen & 15) != 0;
	return text[0]-'0';
}

int
qgaroutes(Jbuf *b, const char *text, int size, QgaFormatIp formatip)
{
	char field[8][QgaIpTextMax], address[3][QgaIpTextMax];
	int pos, count, n, version, direct, id, bits, sbits, i, m;
	unsigned long octet;

	if(text == nil || size < 0 || size > QgaNativeTextMax || formatip == nil)
		return -1;
	pos = count = 0;
	jbchar(b, '[');
	while(pos < size){
		n = fields(text, size, &pos, field, 8);
		if(n == 0)
			continue;
		/* Empty native tags print as spaces, not a separate token. */
		if(n == 7){
			for(i = 7; i > 4; i--)
				memmove(field[i], field[i-1], sizeof field[i]);
			field[4][0] = 0;
			n = 8;
		}
		if(n != 8 || count == QgaRouteMax || (version = routetype(field[3], &direct)) < 0 ||
		   decimal(field[5], 2147483647, &id) < 0 ||
		   field[1][0] != '/' || decimal(field[1]+1, 128, &bits) < 0 ||
		   field[7][0] != '/' || decimal(field[7]+1, 128, &sbits) < 0)
			return -1;
		for(i = 0; i < 3; i++){
			/* Callback outputs are still checked before string serialization. */
			memset(address[i], 0, sizeof address[i]);
			if(formatip(field[i == 0 ? 0 : i == 1 ? 2 : 6], address[i], sizeof address[i]) != version ||
			   memchr(address[i], 0, sizeof address[i]) == nil || address[i][0] == 0)
				return -1;
		}
		if(version == 4){
			if(bits < 96 || sbits < 96)
				return -1;
			bits -= 96;
			sbits -= 96;
		}
		if(count++ != 0)
			jbchar(b, ',');
		jbtext(b, "{\"iface\":\"ipifc");
		number(b, id);
		jbtext(b, "\",\"destination\":");
		jbquote(b, address[0]);
		/* 9front routes have no numeric metric; zero denotes that absence. */
		jbtext(b, ",\"metric\":0,\"version\":");
		number(b, version);
		if(version == 4){
			jbtext(b, ",\"mask\":\"");
			for(i = 0; i < 4; i++){
				m = bits-i*8;
				octet = m <= 0 ? 0 : m >= 8 ? 255 : (255 << (8-m)) & 255;
				if(i != 0)
					jbchar(b, '.');
				number(b, octet);
			}
			jbchar(b, '"');
			if(!direct){
				jbtext(b, ",\"gateway\":");
				jbquote(b, address[1]);
			}
		}else{
			jbtext(b, ",\"desprefixlen\":\"");
			number(b, bits);
			jbtext(b, "\",\"source\":");
			jbquote(b, address[2]);
			jbtext(b, ",\"srcprefixlen\":\"");
			number(b, sbits);
			jbchar(b, '"');
			if(!direct){
				jbtext(b, ",\"nexthop\":");
				jbquote(b, address[1]);
			}
		}
		jbchar(b, '}');
	}
	jbchar(b, ']');
	return b->failed ? -1 : 0;
}
