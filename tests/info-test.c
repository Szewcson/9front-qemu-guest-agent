/* SPDX-License-Identifier: MIT */
/* Identical bounded text/JSON formatters, on native 9front and in sanitizers. */
#include "../src/guestinfo.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#include <arpa/inet.h>
#else
#include <ip.h>
#endif

static int checks;

static void
check(int condition, const char *name)
{
	checks++;
	if(condition)
		return;
#ifdef QGA_HOST_TEST
	fprintf(stderr, "FAIL: %s\n", name);
	exit(1);
#else
	sysfatal("FAIL: %s", (char*)name);
#endif
}

static int
formatip(const char *text, char *out, int size)
{
#ifdef QGA_HOST_TEST
	uchar bytes[16];
	int family;

	family = strchr(text, ':') == nil ? AF_INET : AF_INET6;
	if(inet_pton(family, text, bytes) != 1 || inet_ntop(family, bytes, out, size) == nil)
		return -1;
	return family == AF_INET ? 4 : 6;
#else
	uchar bytes[IPaddrlen];
	int n;

	if(parseip(bytes, (char*)text) < 0)
		return -1;
	n = snprint(out, size, "%I", bytes);
	return n <= 0 || n >= size ? -1 : isv4(bytes) ? 4 : 6;
#endif
}

/* Deliberately invalid callback: the formatter must not read outside its buffer. */
static int
badip(const char *text, char *out, int size)
{
	if(text == nil)
		return -1;
	memset(out, 'x', size);
	return 4;
}

static int
valid(Jbuf *b)
{
	Jdoc *doc;
	int result;

	doc = malloc(sizeof *doc);
	if(doc == nil)
		return 0;
	result = !b->failed && jparse(doc, b->text, b->used);
	free(doc);
	return result;
}

static void
equal(Jbuf *b, const char *expected, const char *name)
{
	check(valid(b) && b->used == (int)strlen(expected) &&
		memcmp(b->text, expected, b->used) == 0, name);
}

static void
cpuchecks(char *out, char *input)
{
	Jbuf b;
	const char *good = " 0 1 2 3 4 5 6 7 8 9\n\t3 0 0 0 0 0 0 0 0 0\r\n";
	const char *bad[] = {
		"", "\n", "0 1 2 3 4 5 6 7 8\n", "0 1 2 3 4 5 6 7 8 9 10\n",
		"-1 0 0 0 0 0 0 0 0 0", "2147483648 0 0 0 0 0 0 0 0 0",
		"0 0 0 0 0 0 0 0 0 x", "0 0 0 0 0 0 0 0 0 000000000000000000000",
		"0 0 0 0 0 0 0 0 0 0\n00 0 0 0 0 0 0 0 0 0",
	};
	int i, used;

	jbinit(&b, out, QgaOutputMax);
	check(qgacpus(&b, good, strlen(good)) == 0, "CPU snapshot");
	equal(&b, "[{\"logical-id\":0,\"online\":true,\"can-offline\":false},"
		"{\"logical-id\":3,\"online\":true,\"can-offline\":false}]", "CPU JSON and sparse IDs");
	for(i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++){
		jbinit(&b, out, QgaOutputMax);
		check(qgacpus(&b, bad[i], strlen(bad[i])) < 0, "malformed CPU record");
	}
	jbinit(&b, out, QgaOutputMax);
	check(qgacpus(&b, "0\0 0 0 0 0 0 0 0 0 0", sizeof "0\0 0 0 0 0 0 0 0 0 0"-1) < 0, "CPU interior NUL");
	jbinit(&b, out, 2);
	check(qgacpus(&b, good, strlen(good)) < 0, "CPU output bound");
	used = 0;
	for(i = 0; i <= QgaCpuMax; i++){
#ifdef QGA_HOST_TEST
		used += snprintf(input+used, QgaNativeTextMax-used, "%d 0 0 0 0 0 0 0 0 0\n", i);
#else
		used += snprint(input+used, QgaNativeTextMax-used, "%d 0 0 0 0 0 0 0 0 0\n", i);
#endif
		if(i == QgaCpuMax-1){
			jbinit(&b, out, QgaOutputMax);
			check(qgacpus(&b, input, used) == 0, "maximum CPU count");
		}
	}
	jbinit(&b, out, QgaOutputMax);
	check(qgacpus(&b, input, used) < 0, "CPU count overflow");
	check(qgacpus(&b, nil, 0) < 0 && qgacpus(&b, good, -1) < 0 &&
		qgacpus(&b, good, QgaNativeTextMax+1) < 0, "CPU input bounds");
}

static void
zonechecks(char *out)
{
	Jbuf b;
	int i;
	int offsets[] = {-86400, -18000, 0, 19800, 86400};

	jbinit(&b, out, QgaOutputMax);
	check(qgatimezone(&b, "MET", 3, 7200) == 0, "timezone native offset");
	equal(&b, "{\"offset\":7200,\"zone\":\"MET\"}", "timezone JSON");
	jbinit(&b, out, QgaOutputMax);
	check(qgatimezone(&b, "", 0, -18000) == 0, "timezone without optional name");
	equal(&b, "{\"offset\":-18000}", "negative timezone JSON");
	for(i = 0; i < (int)(sizeof offsets / sizeof offsets[0]); i++){
		jbinit(&b, out, QgaOutputMax);
		check(qgatimezone(&b, "A\"\n", 3, offsets[i]) == 0 && valid(&b), "timezone bounds and escaping");
	}
	jbinit(&b, out, QgaOutputMax);
	check(qgatimezone(&b, "x", 1, -86401) < 0 &&
		qgatimezone(&b, "x", 1, 86401) < 0 &&
		qgatimezone(&b, "x", 1, (-2147483647-1)) < 0, "timezone signed bounds");
	check(qgatimezone(&b, nil, 0, 0) < 0 && qgatimezone(&b, "x", -1, 0) < 0 &&
		qgatimezone(&b, "x", 65, 0) < 0, "timezone input bounds");
	check(qgatimezone(&b, "x\0y", 3, 0) < 0, "timezone interior NUL");
	jbinit(&b, out, QgaOutputMax);
	check(qgatimezone(&b, "\300\200", 2, 0) < 0, "timezone invalid UTF-8");
	jbinit(&b, out, 2);
	check(qgatimezone(&b, "MET", 3, 7200) < 0, "timezone output bound");
}

static void
routechecks(char *out, char *input)
{
	Jbuf b;
	const char *good =
		"0.0.0.0 /96 192.0.2.1 4 none 0 192.0.2.0 /120\n"
		"192.0.2.0 /120 192.0.2.0 4i ifc 0 192.0.2.2 /128\n"
		":: /0 fe80::1 6 test 1 :: /0\n"
		"fe80:: /64 fe80:: 6i ifc 1 fe80::2 /128\n";
	const char *bad[] = {
		"192.0.2.0 /95 192.0.2.1 4 none 0 0.0.0.0 /96",
		"192.0.2.0 /129 192.0.2.1 4 none 0 0.0.0.0 /96",
		"192.0.2.0 /120 192.0.2.1 4 none -1 0.0.0.0 /96",
		"192.0.2.0 /120 192.0.2.1 4 none 2147483648 0.0.0.0 /96",
		"192.0.2.0 /120 192.0.2.1 4ii none 0 0.0.0.0 /96",
		"192.0.2.0 /120 192.0.2.1 4ub none 0 0.0.0.0 /96",
		"192.0.2.0 /120 192.0.2.1 4z none 0 0.0.0.0 /96",
		"192.0.2.0 /120 192.0.2.1 6 none 0 0.0.0.0 /96",
		"192.0.2.0 /120 fe80::1 4 none 0 0.0.0.0 /96",
		"broken /120 192.0.2.1 4 none 0 0.0.0.0 /96",
		"192.0.2.0 120 192.0.2.1 4 none 0 0.0.0.0 /96",
		":: /0 fe80::1 6 none 0 :: /-1",
		":: /0 fe80::1 6 none 0 :: /0 extra",
		":: /0 fe80::1 6 none 0 ::",
	};
	int i, used;

	jbinit(&b, out, QgaOutputMax);
	check(qgaroutes(&b, good, strlen(good), formatip) == 0, "mixed native route snapshot");
	equal(&b,
		"[{\"iface\":\"ipifc0\",\"destination\":\"0.0.0.0\",\"metric\":0,\"version\":4,"
		"\"mask\":\"0.0.0.0\",\"gateway\":\"192.0.2.1\"},"
		"{\"iface\":\"ipifc0\",\"destination\":\"192.0.2.0\",\"metric\":0,\"version\":4,"
		"\"mask\":\"255.255.255.0\"},"
		"{\"iface\":\"ipifc1\",\"destination\":\"::\",\"metric\":0,\"version\":6,"
		"\"desprefixlen\":\"0\",\"source\":\"::\",\"srcprefixlen\":\"0\",\"nexthop\":\"fe80::1\"},"
		"{\"iface\":\"ipifc1\",\"destination\":\"fe80::\",\"metric\":0,\"version\":6,"
		"\"desprefixlen\":\"64\",\"source\":\"fe80::2\",\"srcprefixlen\":\"128\"}]",
		"route JSON, IPv4 96-bit adjustment, IPv6 source prefix, direct-route gateway omission");
	for(i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++){
		jbinit(&b, out, QgaOutputMax);
		check(qgaroutes(&b, bad[i], strlen(bad[i]), formatip) < 0, "malformed route record");
	}
	jbinit(&b, out, QgaOutputMax);
	check(qgaroutes(&b, "\n\t\n", 3, formatip) == 0, "empty route snapshot");
	equal(&b, "[]", "empty route JSON");
	jbinit(&b, out, QgaOutputMax);
	check(qgaroutes(&b, ":: /0 fe80::1 6  0 :: /0", sizeof ":: /0 fe80::1 6  0 :: /0"-1, formatip) == 0,
		"native route with empty tag");
	check(qgaroutes(&b, nil, 0, formatip) < 0 && qgaroutes(&b, good, -1, formatip) < 0 &&
		qgaroutes(&b, good, QgaNativeTextMax+1, formatip) < 0 &&
		qgaroutes(&b, good, strlen(good), nil) < 0, "route input bounds");
	jbinit(&b, out, QgaOutputMax);
	check(qgaroutes(&b, good, strlen(good), badip) < 0, "unterminated IP callback output");
	jbinit(&b, out, 2);
	check(qgaroutes(&b, good, strlen(good), formatip) < 0, "route output bound");
	used = 0;
	for(i = 0; i <= QgaRouteMax; i++){
		const char *row = "192.0.2.0 /120 192.0.2.0 4i ifc 0 192.0.2.2 /128\n";
		memmove(input+used, row, strlen(row));
		used += strlen(row);
		if(i == QgaRouteMax-1){
			jbinit(&b, out, QgaOutputMax);
			check(qgaroutes(&b, input, used, formatip) == 0, "maximum route count");
		}
	}
	jbinit(&b, out, QgaOutputMax);
	check(qgaroutes(&b, input, used, formatip) < 0, "route count overflow");
	memset(input, '1', QgaIpTextMax);
	jbinit(&b, out, QgaOutputMax);
	check(qgaroutes(&b, input, QgaIpTextMax, formatip) < 0, "route field length bound");
}

#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	char *out, *input;

	out = malloc(QgaOutputMax);
	input = malloc(QgaNativeTextMax);
	check(out != nil && input != nil, "fixture allocations");
#ifndef QGA_HOST_TEST
	check(fmtinstall('I', eipfmt) >= 0, "native IP formatter");
#endif
	cpuchecks(out, input);
	zonechecks(out);
	routechecks(out, input);
	free(input);
	free(out);
#ifdef QGA_HOST_TEST
	printf("guest information tests: %d checks passed\n", checks);
	return 0;
#else
	print("guest information tests: %d checks passed\n", checks);
	exits(nil);
#endif
}
