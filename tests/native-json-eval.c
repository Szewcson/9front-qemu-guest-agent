/* SPDX-License-Identifier: MIT */
/* Read-only development comparison. libjson is not linked into qga9.
 * Absolute native header avoids shadowing by this project's src/json.h. */
#include "../src/json.h"
#include "/sys/include/json.h"

static int checks;

static void
check(int ok, char *name)
{
	checks++;
	if(!ok) sysfatal("FAIL: %s", name);
}

static void
pair(Jdoc *d, char *a, char *b)
{
	JSON *x, *y;
	QgaInt64 n, m;
	Jbuf out;
	char text[64];

	x = jsonparse(a);
	y = jsonparse(b);
	check(x != nil && y != nil && x->t == JSONNumber && y->t == JSONNumber, "native numeric parse");
	check(x->n == y->n, "distinct integers collapse in double");
	check(jparse(d, a, strlen(a)) && jgetint64(d, 0, &n) == 0, "first exact integer");
	check(jparse(d, b, strlen(b)) && jgetint64(d, 0, &m) == 0 && n != m, "distinct exact integers preserved");
	jbinit(&out, text, sizeof text);
	jbint64(&out, m);
	check(!out.failed && out.used == (int)strlen(b) && memcmp(text, b, out.used) == 0, "exact integer serialization");
	print("libjson: %s and %s compare equal as double; qga9 preserves both\n", a, b);
	jsonfree(x);
	jsonfree(y);
}

void
main(void)
{
	Jdoc *d;
	JSON *j;
	char *text;
	char nested[36];
	int i;

	d = malloc(sizeof *d);
	check(d != nil, "fixture allocation");
	pair(d, "9007199254740992", "9007199254740993");
	pair(d, "-9007199254740992", "-9007199254740993");
	pair(d, "9223372036854775806", "9223372036854775807");
	pair(d, "1791702643663620854", "1791702643663620855");
	text = "{\"x\":1,\"x\":2}";
	j = jsonparse(text);
	check(j != nil && j->first != nil && j->first->next != nil, "libjson accepts duplicate keys");
	check(!jparse(d, text, strlen(text)), "qga9 rejects duplicate keys");
	jsonfree(j);
	for(i = 0; i < 17; i++) nested[i] = '[';
	nested[17] = '0';
	for(i = 18; i < 35; i++) nested[i] = ']';
	nested[35] = 0;
	j = jsonparse(nested);
	check(j != nil && !jparse(d, nested, 35), "libjson has no qga9 depth limit");
	jsonfree(j);
	JSONfmtinstall();
	j = jsonparse("{\"a\\\"b\":1}");
	check(j != nil, "escaped object key parse");
	text = smprint("%J", j);
	check(text != nil && !jparse(d, text, strlen(text)), "native formatter does not escape object keys");
	free(text);
	jsonfree(j);
	free(d);
	print("native libjson evaluation passed: %d checks; not a drop-in replacement\n", checks);
	exits(nil);
}
