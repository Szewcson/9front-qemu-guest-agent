/* SPDX-License-Identifier: MIT */
#include "../src/qjson.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#endif

typedef struct Fixture Fixture;
struct Fixture {
	Jdoc doc;
	char input[QgaOutputMax+2];
	char output[QgaOutputMax+2];
};
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
exercise(Fixture *f)
{
	const char *source = "{\"a\\u0000b\":{\"x\":[1e999999,\"a\\u0000b\",true,null]},\"z\":-9223372036854775808}";
	const char *expected = "{\"a\\u0000b\":{\"x\":[1e999999,\"a\\u0000b\",true,null]},\"z\":-9223372036854775808}";
	Jbuf b;
	QgaInt64 value;
	int n, cap, result, i;

	n = strlen((char*)source);
	memmove(f->input, (void*)source, n);
	check(jparse(&f->doc, f->input, n), "native parse through QGA adapter");
	memset(f->input, 0xa5, n);
	check(jmember(&f->doc, 0, "a") < 0, "counted key has no truncation alias");
	check(jgetint64(&f->doc, jmember(&f->doc, 0, "z"), &value) == 0 &&
		value == -9223372036854775807LL-1, "integer survives input destruction");
	for(cap=0; cap<=n; cap++){
		memset(f->output, 0xa5, sizeof f->output);
		jbinit(&b, f->output+1, cap);
		jbvalue(&b, &f->doc, 0);
		check(cap == n ? !b.failed && b.used == n &&
			memcmp(f->output+1, (void*)expected, n) == 0 : b.failed,
			"native formatter capacity without an extra terminator byte");
		check((uchar)f->output[0] == 0xa5 && (uchar)f->output[cap+1] == 0xa5,
			"adapter output guards");
	}
	for(n=QgaMessageMax; n<=QgaOutputMax; n=QgaOutputMax){
		memset(f->input, '9', n);
		result = n == QgaMessageMax ? jparse(&f->doc, f->input, n) :
			jparseresponse(&f->doc, f->input, n);
		check(result && f->doc.used == n+1, "maximum-size exact number storage");
		value = 42;
		check(jgetint64(&f->doc, 0, &value) < 0 && value == 42, "huge number does not become int64");
		jbinit(&b, f->output, n);
		jbvalue(&b, &f->doc, 0);
		check(!b.failed && b.used == n && memcmp(f->output, f->input, n) == 0,
			"maximum-size exact number output");
		check(!jparse(&f->doc, f->input, QgaMessageMax+1) && f->doc.ntoken == 0,
			"request bound remains independent of response capacity");
		if(n == QgaOutputMax) break;
	}
	f->input[0] = '[';
	for(i=0; i<QgaTokenMax; i++){
		f->input[2*i+1] = '0';
		f->input[2*i+2] = ',';
	}
	f->input[2*QgaTokenMax-2] = ']';
	check(jparse(&f->doc, f->input, 2*QgaTokenMax-1) && f->doc.ntoken == QgaTokenMax,
		"exact token budget includes array root");
	f->input[2*QgaTokenMax-2] = ',';
	f->input[2*QgaTokenMax] = ']';
	check(!jparse(&f->doc, f->input, 2*QgaTokenMax+1) && f->doc.ntoken == 0,
		"one excess token leaves no usable document");
	check(jparse(&f->doc, "\"\\u017c\"", 8) && jmatch(&f->doc, 0, "\xc5\xbc"),
		"decoded UTF-8 matching uses counted native strings");
}

#ifdef QGA_HOST_TEST
static void
allocationfailures(Fixture *f)
{
	const char *source = "{\"x\":[{\"y\":\"z\"},-9007199254740993,true,null]}";
	int after, result;

	for(after=0; after<128; after++){
		jsonhostfailafter(after);
		result = jparse(&f->doc, source, strlen(source));
		check(jsonhostlive() == 0, "no native allocation survives QGA parse");
		check(result || f->doc.ntoken == 0, "allocation failure exposes no partial tokens");
		jsonhostfailafter(-1);
		if(result) break;
		check(jparse(&f->doc, source, strlen(source)), "parse recovers after allocation failure");
	}
	check(after > 5 && after < 128, "every native allocation prefix tested");
}
#endif

#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	Fixture *f;

	f = malloc(sizeof *f);
	check(f != nil, "fixture allocation");
	exercise(f);
#ifdef QGA_HOST_TEST
	allocationfailures(f);
	printf("native JSON adapter: %d checks passed\n", checks);
	free(f);
	return 0;
#else
	print("native JSON adapter: %d checks passed\n", checks);
	free(f);
	exits(nil);
#endif
}
