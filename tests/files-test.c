/* SPDX-License-Identifier: MIT */
/* Native-compatible protocol + ownership tests; mocks never touch real files. */
#include "../src/files.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#endif

typedef struct Mock Mock;
typedef struct Test Test;
struct Mock {
	uchar data[QgaFileChunk];
	int length;
	struct { int used, mode; QgaInt64 position; } fd[QgaFileMax];
	int opens, closes, reads, writes;
	int failopen, failclose, failread, failwrite, failseek, shortwrite, impossible;
};
struct Test {
	Mock mock;
	QgaFiles *files;
	Qga *agent;
	Jdoc doc;
	char output[QgaOutputMax];
	char input[QgaMessageMax+1];
	char arguments[QgaMessageMax+1];
	int used;
};
static int checks;

static int
shutdownmock(void *unused, int mode)
{
	if(unused != nil || mode != QgaPowerdown)
		return -1;
	return 0;
}

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
mopen(void *context, const char *path, int flags)
{
	Mock *m;
	int i;

	m = context;
	m->opens++;
	if(m->failopen || strcmp(path, "/fixture") != 0)
		return -1;
	for(i = 0; i < QgaFileMax; i++)
		if(!m->fd[i].used){
			m->fd[i].used = 1;
			m->fd[i].mode = flags;
			m->fd[i].position = 0;
			if(flags & QgaOpenTruncate)
				m->length = 0;
			return i;
		}
	return -1;
}

static int
mclose(void *context, int fd)
{
	Mock *m;

	m = context;
	check(fd >= 0 && fd < QgaFileMax && m->fd[fd].used, "close owns a live fd exactly once");
	m->fd[fd].used = 0;
	m->closes++;
	return m->failclose ? -1 : 0;
}

static int
mread(void *context, int fd, uchar *data, int count)
{
	Mock *m;
	int n;

	m = context;
	m->reads++;
	if(m->failread) return -1;
	if(m->impossible) return count+1;
	if(m->fd[fd].position >= m->length) return 0;
	n = m->length-m->fd[fd].position;
	if(n > count) n = count;
	if(n > 17) n = 17;            /* Exercise a succession of short OS reads. */
	memmove(data, m->data+(int)m->fd[fd].position, n);
	m->fd[fd].position += n;
	return n;
}

static int
mwrite(void *context, int fd, const uchar *data, int count)
{
	Mock *m;
	int n;

	m = context;
	m->writes++;
	if(m->failwrite) return -1;
	if(m->impossible) return count+1;
	if(m->fd[fd].position > QgaFileChunk || count > QgaFileChunk-m->fd[fd].position)
		return -1;
	n = m->shortwrite && count > 1 ? 1 : count;
	memmove(m->data+(int)m->fd[fd].position, data, n);
	m->fd[fd].position += n;
	if(m->fd[fd].position > m->length)
		m->length = m->fd[fd].position;
	return n;
}

static int
mseek(void *context, int fd, QgaInt64 offset, int whence, QgaInt64 *position)
{
	Mock *m;
	QgaInt64 base;

	m = context;
	base = whence == 0 ? 0 : whence == 1 ? m->fd[fd].position : m->length;
	if(m->failseek || (offset > 0 && base > 9223372036854775807LL-offset) ||
	   (offset < 0 && offset < -base))
		return -1;
	*position = m->fd[fd].position = base+offset;
	return 0;
}

static long
capture(void *context, const uchar *data, long size)
{
	Test *t;

	t = context;
	if(size > 5) size = 5;
	if(size > QgaOutputMax-t->used)
		return -1;
	memmove(t->output+t->used, data, size);
	t->used += size;
	return size;
}

static int
ask(Test *t, const char *arguments, const char *command)
{
	Jbuf b;
	int i;

	jbinit(&b, t->input, QgaMessageMax);
	jbtext(&b, "{\"execute\":");
	jbquote(&b, command);
	jbtext(&b, ",\"arguments\":");
	jbtext(&b, arguments);
	jbtext(&b, ",\"id\":\"file-test\"}");
	check(!b.failed, "test request bound");
	t->used = 0;
	for(i = 0; i < b.used; i++)
		check(qgafeed(t->agent, (uchar*)t->input+i, 1) == 0, "fragmented file request");
	check(jparse(&t->doc, t->output, t->used), "file response is complete JSON");
	check(jmatch(&t->doc, jmember(&t->doc, 0, "id"), "file-test"), "file transaction ID");
	return jmember(&t->doc, 0, "return");
}

static void
ok(Test *t, const char *a, const char *command)
{
	check(ask(t, a, command) >= 0, command);
}

static void
bad(Test *t, const char *a, const char *command)
{
	int e;

	check(ask(t, a, command) < 0, "invalid file operation fails");
	e = jmember(&t->doc, 0, "error");
	check(jmatch(&t->doc, jmember(&t->doc, e, "class"), "GenericError"), "file error class");
}

static QgaInt64
openhandle(Test *t, const char *mode)
{
	char a[128];
	Jbuf b;
	QgaInt64 id;
	int r;

	jbinit(&b, a, sizeof a);
	jbtext(&b, "{\"path\":\"/fixture\",\"mode\":");
	jbquote(&b, mode);
	jbchar(&b, '}');
	jbchar(&b, 0);
	r = ask(t, a, "guest-file-open");
	check(r >= 0 && jgetint64(&t->doc, r, &id) == 0 && id > 0, "opaque open handle");
	return id;
}

static void
numeric(Test *t)
{
	Jbuf b;
	QgaInt64 n;
	const char *values[] = {"0", "-0", "9223372036854775807", "-9223372036854775808"};
	int i;

	for(i = 0; i < 4; i++){
		check(jparse(&t->doc, values[i], strlen(values[i])) && jgetint64(&t->doc, 0, &n) == 0,
			"signed integer decoding");
		jbinit(&b, t->output, sizeof t->output);
		jbint64(&b, n);
		check(!b.failed && jparse(&t->doc, t->output, b.used) && jint64(&t->doc, 0),
			"signed integer serialization including minimum");
	}
}

#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	static const QgaFileIO io = {mopen, mclose, mread, mwrite, mseek};
	const char *invalid[] = {"A", "AAA", "A===", "=AAA", "AA=A", "AB==", "AAB=", "AA==AAAA", "!!!!", "AA\nAA", "AA-_"};
	Test *t;
	Jbuf b;
	QgaInt64 id, n;
	const char *description;
	char a[128];
	int i, r, e, before, closes;

	t = malloc(sizeof *t);
	check(t != nil, "test allocation");
	memset(t, 0, sizeof *t);
	t->files = qgafilesnew(&io, &t->mock);
	t->agent = qganew(capture, t);
	check(t->files != nil && t->agent != nil, "file/protocol allocations");
	qgafileops(t->agent, qgafilesop, t->files);
	numeric(t);
	r = ask(t, "{}", "guest-info");
	check(r >= 0 && t->doc.token[jmember(&t->doc, r, "supported_commands")].count == 10,
		"metadata advertises six registered file operations");
	id = openhandle(t, "w+b");
	check(id == 1, "handle is not raw fd zero");
	ok(t, "{\"handle\":1,\"buf-b64\":\"AP9h\"}", "guest-file-write");
	check(t->mock.length == 3 && t->mock.data[0] == 0 && t->mock.data[1] == 255, "binary write including NUL");
	ok(t, "{\"handle\":1,\"offset\":0,\"whence\":\"set\"}", "guest-file-seek");
	r = ask(t, "{\"handle\":1}", "guest-file-read");
	check(r >= 0 && jmatch(&t->doc, jmember(&t->doc, r, "buf-b64"), "AP9h") &&
		t->doc.token[jmember(&t->doc, r, "eof")].type == Jtrue, "binary read and observed EOF");
	ok(t, "{\"handle\":1}", "guest-file-flush");
	ok(t, "{\"handle\":1,\"offset\":-1,\"whence\":2}", "guest-file-seek");
	r = ask(t, "{\"handle\":1,\"count\":1}", "guest-file-read");
	check(jmatch(&t->doc, jmember(&t->doc, r, "buf-b64"), "YQ==") &&
		t->doc.token[jmember(&t->doc, r, "eof")].type == Jfalse, "seek clears sticky EOF; exact read is not EOF");
	ok(t, "{\"handle\":1,\"count\":0}", "guest-file-read");
	bad(t, "{\"handle\":1,\"count\":4097}", "guest-file-read");
	bad(t, "{\"handle\":1,\"count\":-1}", "guest-file-read");
	bad(t, "{\"handle\":1,\"count\":1.0}", "guest-file-read");
	bad(t, "{\"handle\":1,\"offset\":-9223372036854775808,\"whence\":\"cur\"}", "guest-file-seek");
	ok(t, "{\"handle\":1,\"offset\":9223372036854775807,\"whence\":0}", "guest-file-seek");
	bad(t, "{\"handle\":1,\"offset\":1,\"whence\":1}", "guest-file-seek");
	ok(t, "{\"handle\":1,\"offset\":0,\"whence\":0}", "guest-file-seek");
	bad(t, "{\"handle\":1,\"offset\":0,\"whence\":3}", "guest-file-seek");
	before = t->mock.writes;
	for(i = 0; i < (int)(sizeof invalid/sizeof invalid[0]); i++){
		jbinit(&b, a, sizeof a);
		jbtext(&b, "{\"handle\":1,\"buf-b64\":");
		jbquote(&b, invalid[i]);
		jbtext(&b, "}");
		jbchar(&b, 0);
		bad(t, a, "guest-file-write");
	}
	bad(t, "{\"handle\":1,\"buf-b64\":\"AA==\",\"count\":2}", "guest-file-write");
	bad(t, "{\"handle\":1,\"buf-b64\":\"AA\\u0000==\"}", "guest-file-write");
	check(t->mock.writes == before, "all validation precedes file writes");
	ok(t, "{\"handle\":1,\"buf-b64\":\"\"}", "guest-file-write");
	t->mock.shortwrite = 1;
	r = ask(t, "{\"handle\":1,\"buf-b64\":\"AP9h\",\"count\":2}", "guest-file-write");
	check(jgetint64(&t->doc, jmember(&t->doc, r, "count"), &n) == 0 && n == 1, "report actual short write");
	t->mock.shortwrite = 0;
	t->mock.failwrite = 1;
	bad(t, "{\"handle\":1,\"buf-b64\":\"AA==\"}", "guest-file-write");
	t->mock.failwrite = 0;
	t->mock.failread = 1;
	bad(t, "{\"handle\":1}", "guest-file-read");
	t->mock.failread = 0;
	t->mock.impossible = 1;
	bad(t, "{\"handle\":1}", "guest-file-read");
	bad(t, "{\"handle\":1,\"buf-b64\":\"AA==\"}", "guest-file-write");
	t->mock.impossible = 0;
	ok(t, "{\"handle\":1,\"offset\":0,\"whence\":0}", "guest-file-seek");
	jbinit(&b, t->arguments, sizeof t->arguments);
	jbtext(&b, "{\"handle\":1,\"buf-b64\":\"");
	for(i = 0; i < QgaFileChunk/3; i++)
		jbtext(&b, "AAAA");
	jbtext(&b, "AA==\"}");
	jbchar(&b, 0);
	check(!b.failed, "maximum-transfer fixture bounds");
	r = ask(t, t->arguments, "guest-file-write");
	check(r >= 0 && jgetint64(&t->doc, jmember(&t->doc, r, "count"), &n) == 0 &&
		n == QgaFileChunk, "maximum binary transfer");
	before = t->mock.writes;
	jbinit(&b, t->arguments, sizeof t->arguments);
	jbtext(&b, "{\"handle\":1,\"buf-b64\":\"");
	for(i = 0; i < QgaFileChunk/3; i++)
		jbtext(&b, "AAAA");
	jbtext(&b, "AAA=\"}");
	jbchar(&b, 0);
	bad(t, t->arguments, "guest-file-write");
	check(t->mock.writes == before, "oversized decoded payload rejected before write");
	ok(t, "{\"handle\":1,\"offset\":0,\"whence\":0}", "guest-file-seek");
	r = ask(t, "{\"handle\":1,\"count\":4096}", "guest-file-read");
	check(r >= 0 && jgetint64(&t->doc, jmember(&t->doc, r, "count"), &n) == 0 &&
		n == QgaFileChunk && jgetstring(&t->doc, jmember(&t->doc, r, "buf-b64"),
		t->arguments, sizeof t->arguments) == QgaFileBase64Max,
		"maximum read and canonical encoded length");
	t->mock.failseek = 1;
	bad(t, "{\"handle\":1,\"offset\":0,\"whence\":0}", "guest-file-seek");
	t->mock.failseek = 0;
	ok(t, "{\"handle\":1}", "guest-file-close");
	bad(t, "{\"handle\":1}", "guest-file-close");
	bad(t, "{\"handle\":0}", "guest-file-read");
	bad(t, "{\"handle\":9223372036854775808}", "guest-file-read");
	bad(t, "{\"handle\":1,\"extra\":true}", "guest-file-close");
	bad(t, "{}", "guest-file-read");
	bad(t, "{\"path\":\"relative\"}", "guest-file-open");
	bad(t, "{\"path\":\"/fixture\\u0000hidden\"}", "guest-file-open");
	bad(t, "{\"path\":\"/fixture\",\"mode\":\"rx\"}", "guest-file-open");
	before = t->mock.opens;
	memset(t->arguments, 'x', QgaFilePathMax);
	t->arguments[0] = '/';
	t->arguments[QgaFilePathMax] = 0;
	jbinit(&b, t->output, sizeof t->output);
	jbtext(&b, "{\"path\":");
	jbquote(&b, t->arguments);
	jbchar(&b, '}');
	jbchar(&b, 0);
	/* ask owns output, so preserve its argument bytes separately. */
	memmove(t->arguments, t->output, b.used);
	bad(t, t->arguments, "guest-file-open");
	check(t->mock.opens == before, "oversized path never reaches the backend");
	before = t->mock.opens;
	check(ask(t, "{\"path\":\"/fixture\",\"mode\":\"a+\"}", "guest-file-open") < 0, "append explicitly unsupported");
	e = jmember(&t->doc, 0, "error");
	check(jmatch(&t->doc, jmember(&t->doc, e, "class"), "Unsupported") &&
		t->mock.opens == before, "unsupported mode has no side effect");
	t->mock.failopen = 1;
	bad(t, "{\"path\":\"/fixture\"}", "guest-file-open");
	t->mock.failopen = 0;
	id = openhandle(t, "rb");
	check(id == 2, "closed IDs are never reused");
	bad(t, "{\"handle\":1}", "guest-file-flush");
	bad(t, "{\"handle\":2,\"buf-b64\":\"AA==\"}", "guest-file-write");
	t->mock.failclose = 1;
	bad(t, "{\"handle\":2}", "guest-file-close");
	t->mock.failclose = 0;
	bad(t, "{\"handle\":2}", "guest-file-close");
	for(i = 0; i < QgaFileMax; i++)
		openhandle(t, "r+");
	before = t->mock.opens;
	bad(t, "{\"path\":\"/fixture\",\"mode\":\"w\"}", "guest-file-open");
	check(t->mock.opens == before, "handle exhaustion checked before truncation/open");
	check(jparse(&t->doc, "{\"handle\":3}", 12), "direct response-bound fixture");
	jbinit(&b, t->output, 1);
	before = t->mock.reads;
	check(qgafilesop(t->files, QgaFileRead, &t->doc, 0, &b, &description) < 0 &&
		t->mock.reads == before, "output reservation precedes read");
	qgashutdown(t->agent, shutdownmock, nil);
	t->used = 0;
	check(qgafeed(t->agent, (uchar*)"{\"execute\":\"guest-shutdown\"}",
		sizeof "{\"execute\":\"guest-shutdown\"}"-1) == 0 && t->used == 0, "mock asynchronous shutdown");
	before = t->mock.writes;
	bad(t, "{\"handle\":3,\"buf-b64\":\"AA==\"}", "guest-file-write");
	check(t->mock.writes == before, "no file mutation during shutdown");
	closes = t->mock.closes;
	qgafree(t->agent);
	qgafilesfree(t->files);
	check(t->mock.closes == closes+QgaFileMax, "cleanup closes every remaining handle");
	free(t);
#ifdef QGA_HOST_TEST
	printf("file protocol tests: %d checks passed\n", checks);
	return 0;
#else
	print("file protocol tests: %d checks passed\n", checks);
	exits(nil);
#endif
}
