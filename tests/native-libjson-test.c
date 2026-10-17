/* The same fixture compiles against actual patched 9front sources and libc.
 * LIBJSON_HOST_TEST enables only allocation-failure and pthread scaffolding. */
#include <u.h>
#include <libc.h>
#include <json.h>

static int checks;
static JSONLimits limits={16, 512, 65536};

static void
check(int ok, char *why)
{
	checks++;
	if(ok) return;
#ifdef LIBJSON_HOST_TEST
	fprintf(stderr, "FAIL: %s\n", why);
	exit(1);
#else
	sysfatal("FAIL: %s", why);
#endif
}

static JSON*
parse(char *s)
{
	return jsonparsebounded(s, strlen(s), &limits);
}

static void
roundtrip(char *s, char *expected)
{
	JSON *j, *again;
	char out[1024], second[1024];
	long n, m;

	j=parse(s);
	check(j != nil, "parse roundtrip");
	n=jsonformat(out, sizeof out, j, JSONCompact|JSONAscii);
	check(n == (long)strlen(expected) && strcmp(out, expected) == 0, "exact roundtrip bytes");
	again=jsonparsebounded(out, n, &limits);
	check(again != nil, "reparse canonical result");
	m=jsonformat(second, sizeof second, again, JSONCompact|JSONAscii);
	check(m == n && memcmp(out, second, n+1) == 0, "stable canonical result");
	jsonfree(again);
	jsonfree(j);
}

static void
basic(void)
{
	struct LegacyJSON { int t; union { double n; char *s; JSONEl *first; }; };
	struct LegacyJSONEl { char *name; JSON *val; JSONEl *next; };
	JSONEl positional={"key", nil, nil};
	JSON *j, *v;
	char *s, out[128], shortinput[1]={(char)0xF0};
	ulong n;
	vlong value;
	int i;
	char *integers[]={"0", "-0", "9007199254740993", "1792131271868666048",
		"9223372036854775807", "-9223372036854775808", "-9007199254740993", "-1"};
	vlong intvalues[]={0, 0, 9007199254740993LL, 1792131271868666048LL,
		9223372036854775807LL, -9223372036854775807LL-1, -9007199254740993LL, -1};
	char *nonintegers[]={"1.00", "1e0", "9223372036854775808", "-9223372036854775809", "1e999999"};
	char *invalid[]={"", "-", "01", "-.1", "1.", "1e", "1e+", "+1", "[1,]",
		"{\"owned\"?0}", "{\"owned\":}", "{\"owned\": [}", "{\"owned\"\\}",
		"\"\\u1\"", "\"\\ud800\"", "\"\\udc00\"", "\"\\udc00\\ud800\"",
		"\"\\ud800\\u0041\"", "\"\xc0\x80\"", "\"\xed\xa0\x80\"", "\"\xf4\x90\x80\x80\"",
		"{\"x\":0,\"\\u0078\":1}", "{\"a\\u0000b\":0,\"a\\u0000b\":1}"};

	check(sizeof(JSON) == sizeof(struct LegacyJSON) && sizeof(JSONEl) == sizeof(struct LegacyJSONEl),
		"legacy public structure sizes unchanged");
	check(strcmp(positional.name, "key") == 0 && positional.val == nil && positional.next == nil,
		"legacy positional initializer unchanged");
	for(i=0; i<(int)(sizeof integers/sizeof integers[0]); i++){
		j=parse(integers[i]);
		check(j != nil && j->t == JSONNumberText && jsonint64(j, &value) == 0, "exact int64");
		check(value == intvalues[i], "exact converted integer value");
		check(strcmp(j->s, integers[i]) == 0, "integer lexeme retained");
		jsonfree(j);
		roundtrip(integers[i], integers[i]);
	}
	for(i=0; i<(int)(sizeof nonintegers/sizeof nonintegers[0]); i++){
		j=parse(nonintegers[i]);
		value=42;
		check(j != nil && jsonint64(j, &value) < 0 && value == 42, "non-int64 leaves result untouched");
		jsonfree(j);
		roundtrip(nonintegers[i], nonintegers[i]);
	}
	for(i=0; i<(int)(sizeof invalid/sizeof invalid[0]); i++)
		check(parse(invalid[i]) == nil, "reject malformed JSON");
	check(jsonparsebounded(shortinput, 1, &limits) == nil, "truncated UTF-8 has no overread");
	check(jsonparsebounded("0\0", 2, &limits) == nil, "reject included NUL byte");
	j=jsonparsebounded("[0]not input", 3, &limits);
	check(j != nil, "explicit length does not read trailing bytes");
	jsonfree(j);
	j=parse("{\"a\\u0000b\":\"c\\u0000d\",\"a\\u0000c\":0}");
	check(j != nil && j->t == JSONObjectN && jsonkey(j, j->first, &n) != nil && n == 3, "counted object keys");
	check(jsonbynameN(j, "a\0b", 3) == j->first->val, "counted key lookup");
	v=j->first->val;
	s=jsonstring(v, &n);
	check(s != nil && n == 3 && memcmp(s, "c\0d", 3) == 0, "counted string retains NUL");
	check(jsonstr(v) == nil && jsonbyname(j, "a") == nil, "no C-string truncation alias");
	jsonfree(j);
	roundtrip("{\"a\\u0000b\":\"c\\u0000d\",\"a\\u0000c\":0}",
		"{\"a\\u0000b\":\"c\\u0000d\",\"a\\u0000c\":0}");
	roundtrip("{\"a\\\"b\":\"\\ud83d\\ude00\"}", "{\"a\\\"b\":\"\\ud83d\\ude00\"}");
	roundtrip("\"\xef\xbf\xbd\"", "\"\\ufffd\"");
	j=parse("{\"x\":\"hello\"}");
	check(j != nil && strcmp(jsonstr(jsonbyname(j, "x")), "hello") == 0, "new tree C-string convenience");
	jsonfree(j);
	j=jsonparse("{\"a\\\"b\":1.5,\"a\\\"b\":2}");
	check(j != nil && j->t == JSONObject && j->first->val->t == JSONNumber, "legacy representation and duplicates unchanged");
	check(jsonformat(out, sizeof out, j, JSONCompact) > 0 && strstr(out, "\"a\\\"b\"") != nil,
		"legacy object-key escaping fixed");
#ifndef LIBJSON_HOST_TEST
	JSONfmtinstall();
	s=smprint("%J", j);
	check(s != nil && strstr(s, "\"a\\\"b\"") != nil, "native legacy formatter callback");
	free(s);
#endif
	jsonfree(j);
	j=jsonparse("1e99999");
	check(j != nil && jsonformat(out, sizeof out, j, JSONCompact) < 0, "reject non-finite legacy number output");
	jsonfree(j);
}

static void
bounds(void)
{
	JSONLimits l;
	JSON *j;
	char nested[516], guarded[80], *expected="{\"x\":[0,\"\\u0000\"]}";
	int depth, i, cap, n, first;
	long result;

	for(depth=16; depth<=17; depth++){
		for(i=0; i<depth; i++) nested[i]='[';
		nested[depth]='0';
		for(i=0; i<depth; i++) nested[depth+1+i]=']';
		j=jsonparsebounded(nested, 2*depth+1, &limits);
		check(depth == 16 ? j != nil : j == nil, "depth bound");
		jsonfree(j);
	}
	l=limits;
	l.maxdepth=JSONMaxDepth;
	for(i=0; i<JSONMaxDepth; i++) nested[i]='[';
	nested[JSONMaxDepth]='0';
	for(i=0; i<JSONMaxDepth; i++) nested[JSONMaxDepth+1+i]=']';
	j=jsonparsebounded(nested, 2*JSONMaxDepth+1, &l);
	check(j != nil, "maximum supported depth");
	jsonfree(j);
	l.maxdepth=JSONMaxDepth+1;
	check(jsonparsebounded("0", 1, &l) == nil, "reject unsupported depth setting");
	l=limits; l.maxnodes=4;
	j=jsonparsebounded("{\"x\":[0]}", 9, &l);
	check(j != nil, "node limit counts values and keys");
	jsonfree(j);
	l.maxnodes=3;
	check(jsonparsebounded("{\"x\":[0]}", 9, &l) == nil, "node limit rejects before excess value");
	l=limits; first=-1;
	for(i=0; i<512; i++){
		l.maxbytes=i;
		j=jsonparsebounded("{\"x\":[0]}", 9, &l);
		if(j != nil){ first=i; jsonfree(j); break; }
#ifdef LIBJSON_HOST_TEST
		check(jsonhostlive() == 0, "budget failure frees partial tree");
#endif
	}
	check(first > 9 && first < 512, "cumulative allocation budget includes scratch and tree");
	j=parse(expected);
	check(j != nil, "output fixture parse");
	n=strlen(expected);
	for(cap=0; cap<=n+2; cap++){
		memset(guarded, 0xA5, sizeof guarded);
		result=jsonformat(guarded+1, cap, j, JSONCompact|JSONAscii);
		check(cap > n ? result == n && memcmp(guarded+1, expected, n+1) == 0 : result < 0,
			"output capacity and terminator");
		check((uchar)guarded[0] == 0xA5, "leading output guard");
		for(i=cap+1; i<(int)sizeof guarded; i++)
			check((uchar)guarded[i] == 0xA5, "trailing output guards");
	}
	jsonfree(j);
}

#ifdef LIBJSON_HOST_TEST
static void
failures(void)
{
	JSON *j;
	int n, mode;
	for(mode=0; mode<2; mode++)
		for(n=0; n<64; n++){
			jsonhostfailafter(n);
			j=mode ? parse("{\"owned\":[\"x\",{},1]}") : jsonparse("{\"owned\":[\"x\",{},1]}");
			jsonfree(j);
			jsonhostfailafter(-1);
			check(jsonhostlive() == 0, "each allocation failure frees partial legacy/new tree");
		}
	for(n=0; n<1000; n++){
		j=jsonparse("{\"owned\":}");
		check(j == nil && jsonhostlive() == 0, "legacy malformed-object cleanup");
	}
}

static void*
concurrent(void *arg)
{
	JSON *j;
	JSONLimits l=limits;
	char out[128];
	int i;
	l.maxnodes=(uintptr_t)arg ? 2 : 512;
	for(i=0; i<1000; i++){
		j=jsonparsebounded("{\"x\":[9007199254740993]}", 24, &l);
		if(l.maxnodes == 2){ if(j != nil) abort(); }
		else if(j == nil || jsonformat(out, sizeof out, j, JSONCompact) != 24) abort();
		jsonfree(j);
		if(jsonhostlive() != 0) abort();
	}
	return nil;
}
#endif

static void
mutations(void)
{
	JSON *j, *again;
	JSONLimits l={16, 64, 4096};
	char text[64], out[1024], second[1024];
	ulong random=1;
	long n, m;
	int trial, i, size;
	for(trial=0; trial<10000; trial++){
		random=random*1664525+1013904223;
		size=(random>>16)%sizeof text;
		for(i=0; i<size; i++){
			random=random*1664525+1013904223;
			text[i]=random>>24;
		}
		j=jsonparsebounded(text, size, &l);
		if(j != nil){
			n=jsonformat(out, sizeof out, j, JSONCompact|JSONAscii);
			check(n >= 0, "accepted random input formats");
			again=jsonparsebounded(out, n, &l);
			check(again != nil, "accepted random input reparses");
			m=jsonformat(second, sizeof second, again, JSONCompact|JSONAscii);
			check(m == n && memcmp(out, second, n+1) == 0, "random canonical stability");
			jsonfree(again);
		}
		jsonfree(j);
#ifdef LIBJSON_HOST_TEST
		check(jsonhostlive() == 0, "random input cleanup");
#endif
	}
}

#ifdef LIBJSON_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
#ifdef LIBJSON_HOST_TEST
	pthread_t threads[4];
	int i;
#endif
	basic();
	bounds();
	mutations();
#ifdef LIBJSON_HOST_TEST
	failures();
	for(i=0; i<4; i++) check(pthread_create(&threads[i], nil, concurrent, (void*)(uintptr_t)(i%2)) == 0, "thread create");
	for(i=0; i<4; i++) check(pthread_join(threads[i], nil) == 0, "thread join");
	check(jsonhostlive() == 0, "all allocations released");
	printf("patched native libjson: %d checks passed\n", checks);
	return 0;
#else
	print("patched native libjson: %d checks passed\n", checks);
	exits(nil);
#endif
}
