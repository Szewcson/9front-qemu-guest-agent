/* SPDX-License-Identifier: MIT */
#include "../src/base64.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#endif

enum { BinaryMax = 3*((QgaOutputMax-2)/4) };
typedef struct Fixture Fixture;
struct Fixture {
	uchar input[BinaryMax+1];
	uchar decoded[BinaryMax+2];
	char text[QgaOutputMax+2];
};
static int checks;
static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
check(int ok, const char *name)
{
	checks++;
	if(ok) return;
#ifdef QGA_HOST_TEST
	fprintf(stderr, "FAIL: %s\n", name);
	exit(1);
#else
	sysfatal("FAIL: %s", (char*)name);
#endif
}

static void
reject64(Fixture *f, const char *s, int size, int capacity)
{
	int i, limit, unchanged;

	memset(f->decoded, 0xa5, sizeof f->decoded);
	check(qgadecode64(s, size, f->decoded+1, capacity) == -1, "reject invalid encoding/capacity");
	limit = capacity < 0 ? 2 : capacity+2;
	if(limit > (int)sizeof f->decoded) limit = sizeof f->decoded;
	unchanged = 1;
	for(i = 0; i < limit; i++)
		if(f->decoded[i] != 0xa5) unchanged = 0;
	check(unchanged && f->decoded[BinaryMax+1] == 0xa5, "rejection does not write");
}

static void
roundtrip(Fixture *f, int size)
{
	Jbuf b;
	int n;

	n = ((size+2)/3)*4;
	memset(f->text, 0x5a, sizeof f->text);
	jbinit(&b, f->text+1, n+2);
	qgaencode64(&b, f->input, size);
	check(!b.failed && b.used == n+2 && b.text[0] == '"' && b.text[n+1] == '"', "exact-size encoding");
	check(f->text[0] == 0x5a && f->text[n+3] == 0x5a, "encoder guards");
	memset(f->decoded, 0xa5, sizeof f->decoded);
	check(qgadecode64(b.text+1, n, f->decoded+1, size) == size &&
		memcmp(f->input, f->decoded+1, size) == 0, "exact-size binary round-trip");
	check(f->decoded[0] == 0xa5 && f->decoded[size+1] == 0xa5, "decoder guards");
	if(size > 0)
		reject64(f, b.text+1, n, size-1);
}

static void
vectors(Fixture *f)
{
	static const char *plain[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
	static const char *coded[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
	static const char *bad[] = {"A", "AAA", "A===", "====", "=AAA", "AA=A", "AB==", "AAB=",
		"AA==AAAA", "AAAA====", "!!!!", "AA\nAA", "AA-_", "AA== ", "AA==\xff"};
	Jbuf b;
	int i, n;

	for(i = 0; i < 7; i++){
		jbinit(&b, f->text, sizeof f->text);
		qgaencode64(&b, (const uchar*)plain[i], strlen((char*)plain[i]));
		n = strlen((char*)coded[i]);
		check(!b.failed && b.used == n+2 && memcmp(b.text+1, (void*)coded[i], n) == 0, "RFC 4648 encoding vector");
		check(qgadecode64(coded[i], n, f->decoded, sizeof f->decoded) == (int)strlen((char*)plain[i]) &&
			memcmp(f->decoded, (void*)plain[i], strlen((char*)plain[i])) == 0, "RFC 4648 decoding vector");
	}
	for(i = 0; i < (int)(sizeof bad/sizeof bad[0]); i++)
		reject64(f, bad[i], strlen((char*)bad[i]), sizeof f->decoded-1);
	reject64(f, "AA\0=", 4, 3);
	reject64(f, "", -1, 0);
	reject64(f, "", 0, -1);
	check(qgadecode64(nil, 0, f->decoded, 0) == -1 &&
		qgadecode64("", 0, nil, 0) == -1, "null decode arguments");
	check(qgadecode64("", 0, f->decoded, 0) == 0, "empty zero-capacity decode");
	jbinit(&b, f->text, 2);
	qgaencode64(&b, nil, 0);
	check(!b.failed && b.used == 2 && memcmp(b.text, "\"\"", 2) == 0, "empty encoding");
	jbinit(&b, f->text, 5);
	qgaencode64(&b, (const uchar*)"f", 1);
	check(b.failed && b.used == 0, "insufficient output rejected before writing");
	jbinit(&b, f->text, sizeof f->text);
	qgaencode64(&b, nil, 1);
	check(b.failed && b.used == 0, "null nonempty input");
	jbinit(&b, f->text, sizeof f->text);
	qgaencode64(&b, f->input, -1);
	check(b.failed && b.used == 0, "negative encode length");
	jbinit(&b, f->text, sizeof f->text);
	qgaencode64(&b, f->input, BinaryMax+1);
	check(b.failed && b.used == 0, "protocol output bound before arithmetic");
	jbinit(&b, f->text, sizeof f->text);
	qgaencode64(&b, f->input, 2147483647);
	check(b.failed && b.used == 0, "integer-overflow encode input");
	jbinit(&b, f->text, sizeof f->text);
	jbtext(&b, "prefix");
	b.failed = 1;
	qgaencode64(&b, f->input, 1);
	check(b.failed && b.used == 6, "sticky output failure");
}

#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	Fixture *f;
	int i, j;

	f = malloc(sizeof *f);
	check(f != nil, "fixture allocation");
	vectors(f);
	for(i = 0; i < 256; i++){
		f->input[0] = i;
		roundtrip(f, 1);
		for(j = 1; j < 16; j++){
			f->text[3] = alphabet[((i&3)<<4)|j];
			reject64(f, f->text+2, 4, 1);
		}
	}
	for(i = 0; i < 65536; i++){
		f->input[0] = i>>8;
		f->input[1] = i;
		roundtrip(f, 2);
		if(i%256 == 0)
			for(j = 1; j < 4; j++){
				f->text[4] = alphabet[((i&15)<<2)|j];
				reject64(f, f->text+2, 4, 2);
			}
	}
	for(i = 0; i <= BinaryMax; i++) f->input[i] = i;
	for(i = 0; i <= 4096; i++) roundtrip(f, i);
	roundtrip(f, BinaryMax);
	free(f);
#ifdef QGA_HOST_TEST
	printf("base64 checks passed: %d (Linux libresolv adapter)\n", checks);
	return 0;
#else
	print("base64 checks passed: %d (native libc enc64/dec64)\n", checks);
	exits(nil);
#endif
}
