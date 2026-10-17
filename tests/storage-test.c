/* SPDX-License-Identifier: MIT */
#include "../src/storage.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#endif

static int checks;
static int largequery;
static const char good[] = "inquiry QEMU HARDDISK\ngeometry 100 512\npart data 0 100\npart fs 10 90\npart empty 42 42\n";

static void
check(int ok, const char *name)
{
	checks++;
	if(ok)
		return;
#ifdef QGA_HOST_TEST
	fprintf(stderr, "FAIL: %s\n", name);
	exit(1);
#else
	sysfatal("FAIL: %s", (char*)name);
#endif
}

static int
query(int command, Jbuf *b)
{
	QgaDiskUnit *u;
	int i, count, result;

	if(command != QgaDisks)
		return -1;
	if(largequery){
		jbchar(b, '"');
		for(i = 0; i < 60000; i++)
			jbchar(b, 'a');
		jbchar(b, '"');
		return b->failed ? -1 : 0;
	}
	u = malloc(sizeof *u);
	if(u == nil)
		return -1;
	count = 0;
	jbchar(b, '[');
	result = qgadiskparse(u, good, sizeof good-1);
	if(result == 0)
		result = qgadiskemit(b, "sd00", u, &count);
	jbchar(b, ']');
	free(u);
	return result;
}

typedef struct Sink Sink;
struct Sink {
	char text[QgaOutputMax+1];
	int used;
};

static long
capture(void *context, const uchar *text, long size)
{
	Sink *s;

	s = context;
	if(size > 3)
		size = 3;
	if(size <= 0 || size > QgaOutputMax-s->used)
		return -1;
	memmove(s->text+s->used, text, size);
	s->used += size;
	s->text[s->used] = 0;
	return size;
}

static void
streamchecks(Sink *s, Jdoc *d, char *input)
{
	Qga *q;
	int i, used, r;
	QgaInt64 id;
	const char *request = "{\"execute\":\"guest-get-disks\",\"arguments\":{},\"id\":17}";

	q = qganew(capture, s);
	check(q != nil, "stream allocation");
	qgaqueries(q, query);
	s->used = 0;
	for(i = 0; i < (int)strlen(request); i++)
		check(qgafeed(q, (const uchar*)request+i, 1) == 0, "fragmented disk query");
	check(jparseresponse(d, s->text, s->used), "disk response JSON");
	r = jmember(d, 0, "return");
	check(r >= 0 && d->token[r].type == Jarray && d->token[r].count == 2 &&
		jgetint64(d, jmember(d, 0, "id"), &id) == 0 && id == 17, "disk dispatch and ID");
	request = "{\"execute\":\"guest-get-disks\",\"arguments\":{\"path\":\"/anything\"}}";
	s->used = 0;
	check(qgafeed(q, (const uchar*)request, strlen(request)) == 0 &&
		jparse(d, s->text, s->used) && jmember(d, 0, "error") >= 0, "no caller-selected device path");
	request = "{\"execute\":\"guest-info\"}";
	s->used = 0;
	check(qgafeed(q, (const uchar*)request, strlen(request)) == 0 &&
		jparse(d, s->text, s->used), "disk availability metadata");
	r = jmember(d, jmember(d, 0, "return"), "supported_commands");
	check(r >= 0 && d->token[r].count == 12 &&
		strstr(s->text, "guest-get-disks") != nil &&
		strstr(s->text, "guest-get-fsinfo") == nil, "only registered queries advertised");
	request = "{\"execute\":\"guest-get-fsinfo\",\"arguments\":{},\"id\":17}";
	s->used = 0;
	check(qgafeed(q, (const uchar*)request, strlen(request)) == 0 &&
		jparse(d, s->text, s->used) &&
		jmatch(d, jmember(d, jmember(d, 0, "error"), "class"), "Unsupported") &&
		jmember(d, 0, "return") < 0 &&
		jgetint64(d, jmember(d, 0, "id"), &id) == 0 && id == 17,
		"storage backend does not imply verified filesystem provenance");
	used = strlen("{\"execute\":\"guest-get-disks\",\"id\":\"");
	memmove(input, "{\"execute\":\"guest-get-disks\",\"id\":\"", used);
	for(i = 0; i < 3000; i++){
		input[used++] = (char)0xc2;
		input[used++] = (char)0xbf;
	}
	memmove(input+used, "\"}", 2);
	used += 2;
	largequery = 1;
	s->used = 0;
	check(qgafeed(q, (const uchar*)input, used) == 0 &&
		jparseresponse(d, s->text, s->used) && jmember(d, 0, "error") >= 0 &&
		jmember(d, 0, "id") >= 0, "large escaped ID plus query fails cleanly");
	largequery = 0;
	request = "{\"execute\":\"guest-ping\"}";
	s->used = 0;
	check(qgafeed(q, (const uchar*)request, strlen(request)) == 0 &&
		jparse(d, s->text, s->used) && jmember(d, 0, "return") >= 0, "ping after response-space failure");
	qgafree(q);
}

static void
parsechecks(QgaDiskUnit *u, char *input)
{
	const char *bad[] = {
		"", "inquiry disk", "geometry 100 512\n", "\ninquiry disk\n",
		"inquiry disk\ninquiry twice\n", "inquiry disk\ngeometry 1\n",
		"inquiry disk\ngeometry 1 0\n", "inquiry disk\ngeometry 1 -512\n",
		"inquiry disk\ngeometry 1 4294967296\n", "inquiry disk\ngeometry -1 512\n",
		"inquiry disk\ngeometry 9223372036854775808 1\n",
		"inquiry disk\ngeometry 18014398509481984 512\n",
		"inquiry disk\ngeometry 1 512\ngeometry 1 512\n",
		"inquiry disk\ngeometry 1 512 garbage\n",
		"inquiry disk\npart fs 0 1\n", "inquiry disk\ngeometry 1 512\npart data 0\n",
		"inquiry disk\ngeometry 1 512\npart data 0 1 extra\n",
		"inquiry disk\ngeometry 1 512\npart data 1 0\n",
		"inquiry disk\ngeometry 1 512\npart data 0 2\n",
		"inquiry disk\ngeometry 1 512\npart data -1 1\n",
		"inquiry disk\ngeometry 1 512\npart data 0 1e0\n",
		"inquiry disk\ngeometry 1 512\npart data 0 1\npart data 0 1\n",
		"inquiry disk\ngeometry 1 512\npart ../raw 0 1\n",
		"inquiry disk\ngeometry 1 512\npart . 0 1\n",
		"inquiry disk\ngeometry 1 512\npart .. 0 1\n",
		"inquiry disk\ngeometry 1 512\npart ctl 0 1\n",
		"inquiry disk\ngeometry 1 512\npart raw 0 1\n",
		"inquiry disk\ngeometry 1 512\npart a\"b 0 1\n",
		"inquiry disk\ngeometry 1 512\npart a\\b 0 1\n",
	};
	const char *names[] = {"", "s", "sd", "sd0", "sd00/ctl", "sd0-1", "sd0256", "sd000", "sd010000000000000000000", "sd.1"};
	int i, used, n;

	check(qgadiskparse(u, good, sizeof good-1) == 0 && u->npart == 3 &&
		u->sectors == 100 && u->secsize == 512 && u->part[1].start == 10 &&
		u->part[1].end == 90, "native geometry and extents");
	for(i = 0; i < (int)(sizeof bad/sizeof bad[0]); i++)
		check(qgadiskparse(u, bad[i], strlen(bad[i])) < 0, "reject malformed/traversal/overflow records");
	check(qgadiskparse(nil, good, sizeof good-1) < 0 && qgadiskparse(u, nil, 0) < 0 &&
		qgadiskparse(u, good, -1) < 0 && qgadiskparse(u, good, QgaDiskTextMax+1) < 0, "ctl input bounds");
	check(qgadiskparse(u, "inquiry\0disk\n", sizeof "inquiry\0disk\n"-1) < 0, "interior NUL rejected");
	check(qgadiskparse(u, "inquiry empty\n", sizeof "inquiry empty\n"-1) == 0 && u->npart == 0, "empty removable unit");
	check(qgadiskparse(u, "inquiry disk\ngeometry 0 512\n", sizeof "inquiry disk\ngeometry 0 512\n"-1) == 0,
		"zero-size media");
	check(qgadiskparse(u, "inquiry disk\nconfig irrelevant\ngeometry 100 512 20 10 1\npart data 0 100\n",
		sizeof "inquiry disk\nconfig irrelevant\ngeometry 100 512 20 10 1\npart data 0 100\n"-1) == 0,
		"native optional geometry and diagnostic fields");
	check(qgadiskunitname("sd00") && qgadiskunitname("sdC1") && qgadiskunitname("sdZ255"), "canonical native unit names");
	check(!qgadiskunitname(nil), "nil unit name");
	for(i = 0; i < (int)(sizeof names/sizeof names[0]); i++)
		check(!qgadiskunitname(names[i]), "reject unsafe/noncanonical unit name");
	used = strlen("inquiry disk\ngeometry 100 512\n");
	memmove(input, "inquiry disk\ngeometry 100 512\n", used);
	for(i = 0; i <= QgaDiskPartMax; i++){
#ifdef QGA_HOST_TEST
		n = snprintf(input+used, QgaDiskTextMax-used, "part p%d 0 1\n", i);
#else
		n = snprint(input+used, QgaDiskTextMax-used, "part p%d 0 1\n", i);
#endif
		check(n > 0 && n < QgaDiskTextMax-used, "fixture length");
		used += n;
		if(i == QgaDiskPartMax-1)
			check(qgadiskparse(u, input, used) == 0 && u->npart == QgaDiskPartMax, "maximum partition count");
	}
	check(qgadiskparse(u, input, used) < 0, "partition count overflow");
	memset(input, 'x', QgaDiskTextMax);
	memmove(input, "inquiry disk\nconfig ", sizeof "inquiry disk\nconfig "-1);
	input[QgaDiskTextMax-1] = '\n';
	check(qgadiskparse(u, input, QgaDiskTextMax) == 0, "bounded long diagnostic line");
}

static void
lengthchecks(QgaDiskUnit *u, char *out)
{
	const char optical[]="inquiry QEMU DVD-ROM\nconfig 85C0\npart data 0 100\npart track 10 90\n";
	const char empty[]="inquiry empty optical\npart data 0 0\n";
	QgaUint64 length[2];
	QgaDiskUnit *other;
	Jbuf b;
	int i, count;
	const char counters[]="inquiry QEMU HARDDISK\ngeometry 100 512\nirq 999 998\npart data 0 100\npart fs 10 90\npart empty 42 42\n";

	other=malloc(sizeof *other); check(other!=nil,"semantic snapshot allocation");
	check(qgadiskparse(u,good,sizeof good-1)==0 &&
		qgadiskparse(other,counters,sizeof counters-1)==0 &&
		memcmp(u,other,sizeof *u)==0,"live diagnostic counters do not change partition inventory");
	other->part[1].end--;
	check(memcmp(u,other,sizeof *u)!=0,"partition extent changes remain visible");
	free(other);

	check(qgadiskparse(u,optical,sizeof optical-1)==0 && u->sectors==100 && u->secsize==0,
		"optical ctl lacks geometry, sector size unresolved");
	count=0; jbinit(&b,out,QgaOutputMax);
	check(qgadiskemit(&b,"sdD0",u,&count)<0 && count==0,"unresolved geometry cannot serialize");
	length[0]=204800; length[1]=163840;
	check(qgadisklengths(u,length)==0 && u->secsize==2048,"derive optical geometry from whole-data metadata");
	count=0; jbinit(&b,out,QgaOutputMax);
	check(qgadiskemit(&b,"sdD0",u,&count)==0 && count==2,"verified optical nodes serialize");
	for(i=0; i<8; i++){
		check(qgadiskparse(u,optical,sizeof optical-1)==0,"fresh optical failure fixture");
		length[0]=204800; length[1]=163840;
		switch(i){
		case 0: length[0]++; break;
		case 1: length[0]=0; break;
		case 2: length[1]--; break;
		case 3: length[0]=100*4294967296ULL; break;
		case 4: length[0]=~(QgaUint64)0; break;
		case 5: u->part[1].end=101; break;
		case 6: strcpy(u->part[1].name,"data"); break;
		case 7: strcpy(u->part[0].name,"narrow"); break;
		}
		check(qgadisklengths(u,length)<0 && u->secsize==0,"invalid metadata rejected without committing geometry");
	}
	check(qgadiskparse(u,empty,sizeof empty-1)==0,"empty optical partition");
	length[0]=0;
	check(qgadisklengths(u,length)==0 && u->secsize==0,"do not guess sector size for empty media");
	count=0; jbinit(&b,out,QgaOutputMax);
	check(qgadiskemit(&b,"sdD0",u,&count)==0 && count==0,"omit verified empty optical node");
	check(qgadisklengths(nil,length)<0 && qgadisklengths(u,nil)<0,"length resolver nil inputs");
	u->npart=QgaDiskPartMax+1;
	check(qgadisklengths(u,length)<0,"length resolver structure bound");
	check(qgadiskparse(u,good,sizeof good-1)==0,"known geometry length fixture");
	{
		QgaUint64 sizes[3]={51200,40960,0};
		check(qgadisklengths(u,sizes)==0,"known geometry exact byte lengths");
		sizes[1]++;
		check(qgadisklengths(u,sizes)<0,"known geometry length mismatch");
	}
}

static void
emitchecks(QgaDiskUnit *u, char *out, Jdoc *d)
{
	Jbuf b;
	int count;
	const char *expected = "[{\"name\":\"#S/sd00/data\",\"partition\":false},"
		"{\"name\":\"#S/sd00/fs\",\"partition\":true,\"dependencies\":[\"#S/sd00/data\"]}]";

	check(qgadiskparse(u, good, sizeof good-1) == 0, "emit fixture");
	count = 0;
	jbinit(&b, out, QgaOutputMax);
	jbchar(&b, '[');
	check(qgadiskemit(&b, "sd00", u, &count) == 0 && count == 2, "skip zero-size partition");
	jbchar(&b, ']');
	check(jparse(d, out, b.used) && b.used == (int)strlen(expected) && memcmp(out, expected, b.used) == 0,
		"real whole node and partition dependency");
	count = QgaDiskRecordMax-2;
	jbinit(&b, out, QgaOutputMax);
	check(qgadiskemit(&b, "sd00", u, &count) == 0 && count == QgaDiskRecordMax, "maximum inventory count");
	check(qgadiskemit(&b, "sd00", u, &count) < 0, "total inventory overflow");
	count = 0;
	jbinit(&b, out, 4);
	check(qgadiskemit(&b, "sd00", u, &count) < 0 && b.failed, "output bounds");
	u->npart = 1;
	memmove(&u->part[0], &u->part[1], sizeof u->part[0]);
	count = 0;
	jbinit(&b, out, QgaOutputMax);
	jbchar(&b, '[');
	check(qgadiskemit(&b, "sd00", u, &count) == 0, "deleted default data node");
	jbchar(&b, ']');
	out[b.used] = 0;
	check(jparse(d, out, b.used) && strstr(out, "dependencies") == nil && strstr(out, "data") == nil,
		"no invented node or dangling dependency");
	count = 0;
	check(qgadiskemit(&b, "sd00/../sd11", u, &count) < 0, "emitter path validation");
	memset(u->part[0].name, 'x', sizeof u->part[0].name);
	check(qgadiskemit(&b, "sd00", u, &count) < 0, "emitter unterminated name");
	u->npart = QgaDiskPartMax+1;
	check(qgadiskemit(&b, "sd00", u, &count) < 0, "emitter structure bound");
	check(qgadiskemit(nil, "sd00", u, &count) < 0 && qgadiskemit(&b, "sd00", nil, &count) < 0 &&
		qgadiskemit(&b, "sd00", u, nil) < 0, "emitter nil arguments");
}

#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	QgaDiskUnit *u;
	Jdoc *d;
	Sink *s;
	char *input, *out;

	u = malloc(sizeof *u);
	d = malloc(sizeof *d);
	s = malloc(sizeof *s);
	input = malloc(QgaDiskTextMax+1);
	out = malloc(QgaOutputMax);
	check(u != nil && d != nil && s != nil && input != nil && out != nil, "test allocation");
	parsechecks(u, input);
	lengthchecks(u, out);
	emitchecks(u, out, d);
	streamchecks(s, d, input);
	free(out);
	free(input);
	free(s);
	free(d);
	free(u);
#ifdef QGA_HOST_TEST
	printf("storage: %d checks passed\n", checks);
	return 0;
#else
	print("storage: %d checks passed\n", checks);
	exits(nil);
#endif
}
