/* SPDX-License-Identifier: MIT */
/* Same bounds/formatting fixtures for native libc and the host codec adapter. */
#include "../src/qjson.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#endif

static int checks;
static void
check(int ok, const char *why)
{
	checks++;
	if(ok) return;
#ifdef QGA_HOST_TEST
	fprintf(stderr, "FAIL: %s\n", why); exit(1);
#else
	sysfatal("FAIL: %s", (char*)why);
#endif
}

static void
stringcheck(const char *json, const char *bytes, int n)
{
	Jdoc d;
	uchar out[8];
	int cap, result, i;

	check(jparse(&d, json, strlen((char*)json)), "parse rune fixture");
	for(cap=0; cap<=n+1; cap++){
		memset(out, 0xa5, sizeof out);
		result = jgetstring(&d, 0, (char*)out+1, cap);
		check(cap > n ? result == n : result < 0, "rune capacity");
		check(out[0] == 0xa5, "leading guard");
		for(i=cap+1; i<(int)sizeof out; i++) check(out[i] == 0xa5, "trailing guard");
		if(cap > n) check(memcmp(out+1, (void*)bytes, n) == 0 && out[n+1] == 0,
			"exact UTF-8 and terminator");
	}
}

#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	char out[32];
	Jbuf b;
	Jdoc d;
	QgaInt64 value;
	int i, cap, n;
	const char *integers[] = {"0", "-1", "9007199254740993",
		"1791702643663620854", "9223372036854775807", "-9223372036854775808"};
	const char *invalid[] = {"\"\\ud800\"", "\"\\udfff\"", "\"\\udbff\\udbff\"",
		"\"\xc0\x80\"", "\"\xed\xa0\x80\"", "\"\xf4\x90\x80\x80\""};

	stringcheck("\"\\u007f\"", "\x7f", 1);
	stringcheck("\"\\u0080\"", "\xc2\x80", 2);
	stringcheck("\"\\u07ff\"", "\xdf\xbf", 2);
	stringcheck("\"\\u0800\"", "\xe0\xa0\x80", 3);
	stringcheck("\"\\ud7ff\"", "\xed\x9f\xbf", 3);
	stringcheck("\"\\ue000\"", "\xee\x80\x80", 3);
	stringcheck("\"\\uffff\"", "\xef\xbf\xbf", 3);
	stringcheck("\"\\ud800\\udc00\"", "\xf0\x90\x80\x80", 4);
	stringcheck("\"\\udbff\\udfff\"", "\xf4\x8f\xbf\xbf", 4);
	stringcheck("\"\xf4\x8f\xbf\xbf\"", "\xf4\x8f\xbf\xbf", 4);
	for(i=0; i<(int)(sizeof invalid/sizeof invalid[0]); i++)
		check(!jparse(&d, invalid[i], strlen((char*)invalid[i])), "reject invalid scalar");
	check(jparse(&d, "\"\\u0000\"", 8) && jgetstring(&d, 0, out, sizeof out) < 0,
		"reject interior NUL for C-string consumers");
	for(i=0; i<(int)(sizeof integers/sizeof integers[0]); i++){
		n = strlen((char*)integers[i]);
		check(jparse(&d, integers[i], n) && jgetint64(&d, 0, &value) == 0, "parse exact integer");
		for(cap=0; cap<=n; cap++){
			memset(out, 0xa5, sizeof out);
			jbinit(&b, out+1, cap); jbint64(&b, value);
			check(cap == n ? !b.failed && b.used == n &&
				memcmp(out+1, (void*)integers[i], n) == 0 : b.failed && b.used == 0,
				"bounded exact integer formatting");
			check((uchar)out[0] == 0xa5 && (uchar)out[cap+1] == 0xa5, "integer guards");
		}
	}
#ifdef QGA_HOST_TEST
	printf("JSON native-helper fixtures: %d checks passed\n", checks); return 0;
#else
	print("JSON native-helper fixtures: %d checks passed\n", checks); exits(nil);
#endif
}
