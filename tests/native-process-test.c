/* SPDX-License-Identifier: MIT */
#include "../src/native.h"
#include "../src/base64.h"
typedef struct Test Test;
struct Test { Jdoc doc; Jbuf out; char text[QgaOutputMax]; char input[QgaMessageMax]; char fixture[1024]; };
static int checks;
static void
check(int ok, char *message)
{
	checks++;
	if(!ok) sysfatal("FAIL: %s", message);
}
static int
call(Test *t, QgaProcesses *p, int op, char *s)
{
	const char *error = "unknown";
	int result;
	check(jparse(&t->doc, s, strlen(s)), "parse native test arguments");
	jbinit(&t->out, t->text, sizeof t->text-1);
	result = qgaprocessop(p, op, &t->doc, 0, &t->out, &error);
	if(result < 0){ print("native test rejection: %s\n", (char*)error); return -1; }
	t->text[t->out.used] = 0;
	if(!jparseresponse(&t->doc, t->text, t->out.used))
		sysfatal("FAIL: parse native result (%d bytes): %.160s", t->out.used, t->text);
	return 0;
}
static int
start(Test *t, QgaProcesses *p, char *arguments)
{
	vlong pid;
	check(call(t, p, QgaExec, arguments) == 0, "native spawn");
	check(jgetint64(&t->doc, jmember(&t->doc, 0, "pid"), &pid) == 0 && pid > 0 && pid <= 2147483647, "native PID");
	return pid;
}
static void
finish(Test *t, QgaProcesses *p, int pid)
{
	int i, a;
	snprint(t->input, sizeof t->input, "{\"pid\":%d}", pid);
	for(i = 0; i < 3000; i++){
		check(call(t, p, QgaExecStatus, t->input) == 0, "native nonblocking status");
		a = jmember(&t->doc, 0, "exited");
		if(a >= 0 && t->doc.token[a].type == Jtrue) return;
		sleep(10);
	}
	sysfatal("process did not finish within test deadline");
}
static int
launch(Test *t, QgaProcesses *p, char *mode, char *capture, int literal)
{
	Jbuf b;
	jbinit(&b, t->input, sizeof t->input-1);
	jbtext(&b, "{\"path\":"); jbquote(&b, t->fixture);
	jbtext(&b, ",\"arg\":["); jbquote(&b, mode);
	if(literal) jbtext(&b, ",\"$(no); 'literal'\"");
	jbtext(&b, "],\"capture-output\":"); jbtext(&b, capture);
	if(literal) jbtext(&b, ",\"env\":[\"QGA9_TEST=literal;value\"],\"input-data\":\"AP94\"");
	jbchar(&b, '}'); check(!b.failed, "fixture arguments bounds"); t->input[b.used] = 0;
	return start(t, p, t->input);
}
static int
data(Test *t, char *field, uchar *bytes, int capacity)
{
	char encoded[6000];
	int n;
	n = jgetstring(&t->doc, jmember(&t->doc, 0, field), encoded, sizeof encoded);
	check(n >= 0, "capture field present");
	n = qgadecode64(encoded, n, bytes, capacity);
	check(n >= 0, "capture canonical base64");
	return n;
}
void
main(void)
{
	QgaProcesses *p;
	Test *t;
	vlong code;
	char path[64], cwd[1024];
	uchar bytes[8192], expected[] = {0,255,'x','e','r','r','\n'};
	int pid, n, i, fd, pair[2], child;
	Waitmsg *w;
	vlong begun;
	p = nativeprocesses(); check(p != nil, "native backend allocation");
	t = malloc(sizeof *t); check(t != nil, "test allocation");
	check(getwd(cwd, sizeof cwd) != nil && snprint(t->fixture, sizeof t->fixture, "%s/qga9-exec-fixture", cwd) < sizeof t->fixture, "absolute fixture path");
	pid = launch(t, p, "literal", "true", 1); finish(t, p, pid);
	n = data(t, "out-data", bytes, sizeof bytes);
	check(n == 3 && memcmp(bytes, expected, 3) == 0, "literal argv/env and binary stdin/stdout");
	n = data(t, "err-data", bytes, sizeof bytes);
	check(n == 4 && memcmp(bytes, "err\n", 4) == 0, "separate stderr");
	check(call(t, p, QgaExecStatus, t->input) < 0, "completed PID reaped once");
	pid = launch(t, p, "literal", "\"merged\"", 1); finish(t, p, pid);
	n = data(t, "out-data", bytes, sizeof bytes);
	check(n == 7 && memcmp(bytes, expected, 7) == 0 && jmember(&t->doc, 0, "err-data") < 0, "actual merged redirection");
	pid = launch(t, p, "literal", "\"stderr\"", 1); finish(t, p, pid);
	check(jmember(&t->doc, 0, "out-data") < 0 && data(t, "err-data", bytes, sizeof bytes) == 4, "stderr-only capture");
	pid = launch(t, p, "literal", "\"stdout\"", 1); finish(t, p, pid);
	check(jmember(&t->doc, 0, "err-data") < 0 && data(t, "out-data", bytes, sizeof bytes) == 3, "stdout-only capture");
	pid = launch(t, p, "literal", "false", 1); finish(t, p, pid);
	check(jmember(&t->doc, 0, "err-data") < 0 && jmember(&t->doc, 0, "out-data") < 0, "discard capture");
	pid = launch(t, p, "flood", "true", 0); finish(t, p, pid);
	check(data(t, "out-data", bytes, sizeof bytes) == QgaCaptureMax && data(t, "err-data", bytes, sizeof bytes) == QgaCaptureMax, "both large streams drained and capped");
	check(t->doc.token[jmember(&t->doc, 0, "out-truncated")].type == Jtrue && t->doc.token[jmember(&t->doc, 0, "err-truncated")].type == Jtrue, "both truncation flags");
	fd = open("/dev/null", ORDWR); check(fd >= 0, "extra inherited parent descriptor");
	pid = launch(t, p, "fds", "true", 0); finish(t, p, pid); close(fd);
	check(jgetint64(&t->doc, jmember(&t->doc, 0, "exitcode"), &code) == 0 && code == 0, "only stdio reaches executable");
	pid = launch(t, p, "fail", "true", 0); finish(t, p, pid);
	check(jgetint64(&t->doc, jmember(&t->doc, 0, "exitcode"), &code) == 0 && code == 1, "native nonempty exit maps to failure");
	check(call(t, p, QgaExec, "{\"path\":\"/does/not/exist/qga9\"}") < 0, "exec failure returned at spawn");
	pid = start(t, p, "{\"path\":\"echo\",\"arg\":[\"bare\"],\"capture-output\":true}"); finish(t, p, pid);
	check(data(t, "out-data", bytes, sizeof bytes) == 5 && memcmp(bytes, "bare\n", 5) == 0, "bare command resolves under /bin");
	begun = nsec(); pid = launch(t, p, "background", "true", 0); finish(t, p, pid);
	check(nsec()-begun < 1500000000LL, "background descendant cannot pin result");
	pid = launch(t, p, "sleep", "true", 0);
	snprint(t->input, sizeof t->input, "{\"pid\":%d}", pid);
	check(call(t, p, QgaExecStatus, t->input) == 0 && t->doc.token[jmember(&t->doc, 0, "exited")].type == Jfalse, "running process status is immediate");
	snprint(path, sizeof path, "/proc/%d/status", pid); fd = open(path, OREAD);
	check(fd >= 0, "pin running process identity");
	nativeprocessfree(p); sleep(100);
	check(pread(fd, bytes, sizeof bytes, 0) < 0, "cleanup kills owned job group"); close(fd);
	child = rfork(RFPROC|RFFDG|RFNAMEG|RFENVG|RFNOTEG|RFREND);
	check(child >= 0, "closed-stdio fixture fork");
	if(child == 0){
		close(0); close(1); close(2);
		p = nativeprocesses(); if(p == nil) exits("closed-stdio allocation");
		pid = launch(t, p, "fds", "true", 0); finish(t, p, pid);
		if(jgetint64(&t->doc, jmember(&t->doc, 0, "exitcode"), &code) < 0 || code != 0) exits("closed-stdio isolation");
		nativeprocessfree(p); exits(nil);
	}
	w = wait(); check(w != nil && w->pid == child && w->msg[0] == 0, "closed stdio safely relocated before pipes"); free(w);
	check(pipe(pair) == 0, "agent-death fixture pipe");
	child = rfork(RFPROC|RFFDG|RFNAMEG|RFENVG|RFNOTEG|RFREND);
	check(child >= 0, "agent-death fixture fork");
	if(child == 0){
		close(pair[0]); p = nativeprocesses();
		if(p == nil) exits("allocation");
		pid = launch(t, p, "sleep", "true", 0);
		if(write(pair[1], &pid, sizeof pid) != sizeof pid) exits("report pid");
		exits(nil); /* Deliberately bypass normal cleanup. */
	}
	close(pair[1]); check(readn(pair[0], &pid, sizeof pid) == sizeof pid, "receive owned child PID"); close(pair[0]);
	snprint(path, sizeof path, "/proc/%d/status", pid); fd = open(path, OREAD);
	w = wait(); check(w != nil && w->pid == child && w->msg[0] == 0, "fixture parent exited"); free(w);
	if(fd >= 0){
		for(i = 0; i < 100 && pread(fd, bytes, sizeof bytes, 0) > 0; i++) sleep(10);
		check(pread(fd, bytes, sizeof bytes, 0) < 0, "supervisor detects agent death and kills job"); close(fd);
	}
	free(t); print("PASS: %d native process checks\n", checks); exits(nil);
}
