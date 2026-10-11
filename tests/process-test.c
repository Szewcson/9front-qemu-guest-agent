/* SPDX-License-Identifier: MIT */
#include "../src/process.h"
#ifdef QGA_HOST_TEST
#include <stdio.h>
#endif
typedef struct Test Test;
struct Test {
	QgaProcesses *processes;
	Qga *agent;
	Jdoc doc;
	char output[QgaOutputMax+1], input[QgaMessageMax+1];
	int used, calls, releases, cancels, failspawn, failpoll, impossible, duplicate, exited;
	int expectedcapture;
	int slots[QgaJobMax];
};
static int checks;
static void
check(int ok, const char *message)
{
	checks++;
	if(ok) return;
#ifdef QGA_HOST_TEST
	fprintf(stderr, "FAIL: %s\n", message); exit(1);
#else
	sysfatal("FAIL: %s", (char*)message);
#endif
}
static long
writeout(void *context, const uchar *data, long size)
{
	Test *t = context;
	if(size > QgaOutputMax-t->used) return -1;
	memmove(t->output+t->used, data, size); t->used += size;
	t->output[t->used] = 0;
	return size;
}
static int
spawn(void *context, QgaExecRequest *r, int *pid, void **handle)
{
	Test *t = context;
	int i;
	t->calls++;
	check(r->capture == t->expectedcapture, "capture mode decoded");
	if(t->failspawn) return -1;
	for(i = 0; i < QgaJobMax; i++) if(t->slots[i] == 0) break;
	check(i < QgaJobMax, "bounded backend slots");
	t->slots[i] = 1; *handle = &t->slots[i]; *pid = t->duplicate ? t->duplicate : 100+i;
	if(strcmp(r->path, "/fixture") == 0){
		check(strcmp(r->argv[1], "$(no); 'literal'") == 0 && r->argv[2] == nil, "literal argv");
		check(strcmp(r->env[0], "TEST=literal;value") == 0 && r->env[1] == nil, "literal environment");
		check(r->ninput == 3 && r->input[0] == 0 && r->input[1] == 255 && r->input[2] == 'x', "binary input");
	}
	return 0;
}
static int
poll(void *context, void *handle, QgaExecResult *r)
{
	Test *t = context;
	check(*(int*)handle == 1, "poll owned handle");
	if(t->failpoll) return -1;
	r->exited = t->exited; r->exitcode = 0;
	r->nout = t->impossible == 1 ? QgaCaptureMax+1 : t->impossible == 2 ? QgaCaptureMax : 3;
	r->nerr = t->impossible == 2 ? QgaCaptureMax : 1;
	r->out[0] = 0; r->out[1] = 255; r->out[2] = 'x'; r->err[0] = 'e';
	r->outtruncated = 1;
	return 0;
}
static void
release(void *context, void *handle, int cancel)
{
	Test *t = context;
	check(*(int*)handle == 1, "release exactly once");
	*(int*)handle = 0; t->releases++; t->cancels += cancel;
}
static void
request(Test *t, const char *s)
{
	int i;
	t->used = 0;
	for(i = 0; s[i]; i++) check(qgafeed(t->agent, (const uchar*)s+i, 1) == 0, "fragmented exec feed");
	check(jparseresponse(&t->doc, t->output, t->used), "valid exec response JSON");
}
static void
iserror(Test *t, const char *s)
{
	int calls = t->calls;
	request(t, s);
	check(jmember(&t->doc, 0, "error") >= 0, "invalid exec rejected");
	check(t->calls == calls, "rejected request cannot spawn");
}
#ifdef QGA_HOST_TEST
int main(void)
#else
void main(void)
#endif
{
	const QgaExecIO io = {spawn, poll, release};
	const char *modes[] = {"false", "\"stdout\"", "\"stderr\"", "true", "\"merged\""};
	const char *invalid[] = {
		"{\"path\":\"\"}", "{\"path\":3}", "{\"path\":\"/bin/echo\\u0000x\"}",
		"{\"path\":\"relative/path\"}", "{\"path\":\"echo\",\"arg\":3}",
		"{\"path\":\"echo\",\"arg\":[null]}", "{\"path\":\"echo\",\"env\":[\"BAD\"]}",
		"{\"path\":\"echo\",\"env\":[\"../x=y\"]}", "{\"path\":\"echo\",\"env\":[\"=y\"]}",
		"{\"path\":\"echo\",\"input-data\":\"AB==\"}", "{\"path\":\"echo\",\"input-data\":\"AA==junk\"}",
		"{\"path\":\"echo\",\"capture-output\":1}", "{\"path\":\"echo\",\"capture-output\":\"all\"}",
		"{\"path\":\"echo\",\"unknown\":true}",
	};
	Test *t;
	int i, r, before;
	QgaInt64 pid;
	Jbuf b;
	t = malloc(sizeof *t); check(t != nil, "test allocation"); memset(t, 0, sizeof *t);
	check(qgaprocessnew(nil, nil) == nil, "reject missing backend");
	t->processes = qgaprocessnew(&io, t); check(t->processes != nil, "process allocation");
	t->agent = qganew(writeout, t); check(t->agent != nil, "core allocation");
	request(t, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"echo\"}}");
	check(jmember(&t->doc, 0, "error") >= 0 && t->calls == 0, "disabled exec cannot spawn");
	qgaexecops(t->agent, qgaprocessop, t->processes);
	for(i = 0; i < (int)(sizeof invalid/sizeof invalid[0]); i++){
#ifdef QGA_HOST_TEST
		snprintf(t->input, sizeof t->input, "{\"execute\":\"guest-exec\",\"arguments\":%s}", invalid[i]);
#else
		snprint(t->input, sizeof t->input, "{\"execute\":\"guest-exec\",\"arguments\":%s}", invalid[i]);
#endif
		iserror(t, t->input);
	}
	request(t, "{\"execute\":\"guest-info\"}");
	check(strstr(t->output, "guest-exec-status") != nil, "exec advertised only with backend");
	for(i = 0; i < 5; i++){
		t->expectedcapture = i;
#ifdef QGA_HOST_TEST
		snprintf(t->input, sizeof t->input, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"/fixture\",\"arg\":[\"$(no); 'literal'\"],\"env\":[\"TEST=literal;value\"],\"input-data\":\"AP94\",\"capture-output\":%s},\"id\":{\"x\":2}}", modes[i]);
#else
		snprint(t->input, sizeof t->input, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"/fixture\",\"arg\":[\"$(no); 'literal'\"],\"env\":[\"TEST=literal;value\"],\"input-data\":\"AP94\",\"capture-output\":%s},\"id\":{\"x\":2}}", modes[i]);
#endif
		request(t, t->input); r = jmember(&t->doc, 0, "return");
		check(jgetint64(&t->doc, jmember(&t->doc, r, "pid"), &pid) == 0 && pid == 100, "real numeric PID result");
		check(jmember(&t->doc, 0, "id") >= 0, "transaction ID retained");
		t->exited = 0;
		request(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":100}}");
		check(strstr(t->output, "\"exited\":false") != nil && strstr(t->output, "out-data") == nil, "running output withheld");
		t->exited = 1;
		request(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":100}}");
		check(strstr(t->output, "\"exited\":true") != nil, "exited result");
		check((strstr(t->output, "\"out-data\":\"AP94\"") != nil) == (i == 1 || i == 3 || i == 4), "stdout capture selection");
		check((strstr(t->output, "\"err-data\":\"ZQ==\"") != nil) == (i == 2 || i == 3), "stderr capture selection");
		check(t->releases == i+1 && t->cancels == 0, "status consumes finished metadata");
		iserror(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":100}}");
	}
	t->expectedcapture = 0;
	/* Array and decoded-storage limits are checked before any backend call. */
	for(r = 32; r <= 33; r++){
		jbinit(&b, t->input, sizeof t->input-1);
		jbtext(&b, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"echo\",\"arg\":[");
		for(i = 0; i < r; i++){ if(i) jbchar(&b, ','); jbtext(&b, "\"\""); }
		jbtext(&b, "]}}"); check(!b.failed, "array fixture bounds"); t->input[b.used] = 0;
		if(r == 33) iserror(t, t->input);
		else{
			request(t, t->input); check(jmember(&t->doc, 0, "return") >= 0, "32 empty args accepted");
			request(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":100}}");
		}
	}
	jbinit(&b, t->input, sizeof t->input-1);
	jbtext(&b, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"echo\",\"arg\":[\"");
	for(i = 0; i < QgaExecTextMax; i++) jbchar(&b, 'x');
	jbtext(&b, "\"]}}"); check(!b.failed, "text fixture bounds"); t->input[b.used] = 0;
	iserror(t, t->input);
	t->expectedcapture = QgaCaptureSeparated; t->impossible = 2;
	request(t, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"echo\",\"capture-output\":\"separated\"}}");
	request(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":100}}");
	check(t->used > QgaMessageMax && t->used < QgaOutputMax, "full dual capture exceeds request limit but stays bounded");
	check(!jparse(&t->doc, t->output, t->used), "request parser still rejects response-sized input");
	t->expectedcapture = 0; t->impossible = 0;
	for(i = 0; i < QgaJobMax; i++) request(t, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"echo\"}}");
	iserror(t, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"echo\"}}");
	iserror(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":\"100\"}}");
	iserror(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":-1}}");
	t->failpoll = 1; before = t->cancels;
	request(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":100}}");
	check(jmember(&t->doc, 0, "error") >= 0 && t->cancels == before+1, "poll failure cancels owned job");
	t->failpoll = 0; t->impossible = 1;
	request(t, "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":101}}");
	check(jmember(&t->doc, 0, "error") >= 0, "oversized result refused before serialization");
	t->impossible = 0; t->failspawn = 1;
	request(t, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"echo\"}}");
	check(jmember(&t->doc, 0, "error") >= 0, "spawn failure returned");
	t->failspawn = 0; t->duplicate = 102; before = t->cancels;
	request(t, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"echo\"}}");
	check(jmember(&t->doc, 0, "error") >= 0 && t->cancels == before+1, "PID collision cancels only new job");
	before = t->cancels;
	qgafree(t->agent); qgaprocessfree(t->processes);
	check(t->cancels == before+6, "normal cleanup cancels all six still-owned jobs");
	free(t);
#ifdef QGA_HOST_TEST
	printf("PASS: %d process checks\n", checks); return 0;
#else
	print("PASS: %d process checks\n", checks); exits(nil);
#endif
}
