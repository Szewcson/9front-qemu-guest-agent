/* SPDX-License-Identifier: MIT */
#include "process.h"
#include "base64.h"
typedef struct Job Job;
struct Job { int pid, capture; void *handle; };
struct QgaProcesses {
	QgaExecIO io;
	void *context;
	Job jobs[QgaJobMax];
	QgaExecRequest request;
	QgaExecResult result;
	char encoded[((QgaInputMax+2)/3)*4+1];
};

QgaProcesses*
qgaprocessnew(const QgaExecIO *io, void *context)
{
	QgaProcesses *p;
	if(io == nil || io->spawn == nil || io->poll == nil || io->release == nil) return nil;
	p = malloc(sizeof *p);
	if(p == nil) return nil;
	memset(p, 0, sizeof *p);
	p->io = *io;
	p->context = context;
	return p;
}

void
qgaprocessfree(QgaProcesses *p)
{
	int i;
	if(p == nil) return;
	for(i = 0; i < QgaJobMax; i++)
		if(p->jobs[i].handle != nil)
			p->io.release(p->context, p->jobs[i].handle, 1);
	memset(p, 0, sizeof *p);
	free(p);
}

static int
bad(const char **error, const char *message)
{
	*error = message;
	return -1;
}

static int
members(Jdoc *d, int a, const char *const *names, int n)
{
	int i, j;
	if(a < 0 || a >= d->ntoken || d->token[a].type != Jobject) return -1;
	for(i = a+1; i < d->token[a].next; i = d->token[i+1].next){
		for(j = 0; j < n; j++) if(jmatch(d, i, names[j])) break;
		if(j == n) return -1;
	}
	return 0;
}

static int
strings(QgaExecRequest *r, Jdoc *d, int a, char **v, int max, int *used, int env)
{
	char *equals;
	int i, n, count, j;
	if(a < 0) return 0;
	if(d->token[a].type != Jarray || d->token[a].count > max) return -1;
	count = 0;
	for(i = a+1; i < d->token[a].next; i = d->token[i].next){
		v[count] = r->text+*used;
		n = jgetstring(d, i, v[count], QgaExecTextMax-*used);
		if(n < 0) return -1;
		if(env){
			equals = strchr(v[count], '=');
			if(equals == nil || equals == v[count]) return -1;
			/* Native /env names, never arbitrary paths. Values remain literal. */
			for(j = 0; v[count]+j < equals; j++)
				if(!((v[count][j] >= 'a' && v[count][j] <= 'z') ||
				     (v[count][j] >= 'A' && v[count][j] <= 'Z') ||
				     (v[count][j] >= '0' && v[count][j] <= '9') || v[count][j] == '_')) return -1;
		}
		*used += n+1;
		count++;
	}
	v[count] = nil;
	return 0;
}

static int
start(QgaProcesses *p, Jdoc *d, int a, Jbuf *out, const char **error)
{
	const char *const names[] = {"path", "arg", "env", "input-data", "capture-output"};
	const char *const modes[] = {"none", "stdout", "stderr", "separated", "merged"};
	QgaExecRequest *r;
	Job *job;
	void *handle;
	int i, t, used, n, pid, result, capture;

	r = &p->request;
	memset(r, 0, sizeof *r);
	if(members(d, a, names, 5) < 0 ||
	   jgetstring(d, jmember(d, a, "path"), r->path, sizeof r->path) < 1)
		return bad(error, "Invalid exec path or arguments");
	/* Bare command names are resolved by the native backend under /bin. */
	if(r->path[0] != '/' && strchr(r->path, '/') != nil)
		return bad(error, "Exec path must be absolute or a bare command name");
	r->argv[0] = r->path;
	used = 0;
	if(strings(r, d, jmember(d, a, "arg"), r->argv+1, QgaArgMax, &used, 0) < 0 ||
	   strings(r, d, jmember(d, a, "env"), r->env, QgaEnvMax, &used, 1) < 0)
		return bad(error, "Exec requires bounded string arrays and valid NAME=value environment entries");
	t = jmember(d, a, "capture-output");
	if(t >= 0){
		if(d->token[t].type == Jtrue) r->capture = QgaCaptureSeparated;
		else if(d->token[t].type != Jfalse){
			for(i = 0; i < 5; i++) if(jmatch(d, t, modes[i])) break;
			if(i == 5) return bad(error, "Invalid output capture mode");
			r->capture = i;
		}
	}
	t = jmember(d, a, "input-data");
	if(t >= 0){
		n = jgetstring(d, t, p->encoded, sizeof p->encoded);
		if(n < 0 || (r->ninput = qgadecode64(p->encoded, n, r->input, sizeof r->input)) < 0)
			return bad(error, "Invalid or oversized canonical exec input base64");
	}
	job = nil;
	for(i = 0; i < QgaJobMax; i++) if(p->jobs[i].handle == nil){ job = &p->jobs[i]; break; }
	if(job == nil) return bad(error, "Exec job limit reached; reap completed jobs with exec-status");
	pid = 0;
	handle = nil;
	capture = r->capture;
	result = p->io.spawn(p->context, r, &pid, &handle);
	/* Native children have private copies; do not retain caller input between jobs. */
	memset(r, 0, sizeof *r);
	memset(p->encoded, 0, sizeof p->encoded);
	if(result < 0)
		return bad(error, "Cannot start native process");
	if(pid < 1 || handle == nil){
		if(handle != nil) p->io.release(p->context, handle, 1);
		return bad(error, "Invalid native process identity");
	}
	for(i = 0; i < QgaJobMax; i++)
		if(p->jobs[i].handle != nil && p->jobs[i].pid == pid){
			p->io.release(p->context, handle, 1);
			return bad(error, "Native PID is already retained; reap its previous result");
		}
	job->pid = pid;
	job->capture = capture;
	job->handle = handle;
	jbtext(out, "{\"pid\":"); jbint64(out, pid); jbchar(out, '}');
	return 0;
}

int
qgaprocessop(void *context, int command, Jdoc *d, int a, Jbuf *out, const char **error)
{
	const char *const names[] = {"pid"};
	QgaProcesses *p;
	QgaExecResult *r;
	QgaInt64 pid;
	Job *job;
	int i, capture;
	void *handle;

	p = context;
	if(p == nil || d == nil || out == nil || error == nil) return -1;
	/* Includes maximum base64, metadata. Core reserves transaction ID separately. */
	if(out->failed || out->size-out->used < 2*((QgaCaptureMax+2)/3)*4+256)
		return bad(error, "Insufficient exec response space");
	if(command == QgaExec) return start(p, d, a, out, error);
	if(command != QgaExecStatus || members(d, a, names, 1) < 0 ||
	   jgetint64(d, jmember(d, a, "pid"), &pid) < 0 || pid < 1 || pid > 2147483647)
		return bad(error, "Exec status requires an owned positive native PID");
	job = nil;
	for(i = 0; i < QgaJobMax; i++)
		if(p->jobs[i].handle != nil && p->jobs[i].pid == pid){ job = &p->jobs[i]; break; }
	if(job == nil) return bad(error, "Unknown or reaped exec PID");
	r = &p->result;
	memset(r, 0, sizeof *r);
	if(p->io.poll(p->context, job->handle, r) < 0){
		p->io.release(p->context, job->handle, 1);
		memset(job, 0, sizeof *job);
		return bad(error, "Cannot collect native process status");
	}
	if(!r->exited){ jbtext(out, "{\"exited\":false}"); return 0; }
	if(r->exited != 1 || r->exitcode < 0 || r->nout < 0 || r->nout > QgaCaptureMax ||
	   r->nerr < 0 || r->nerr > QgaCaptureMax || (r->outtruncated != 0 && r->outtruncated != 1) ||
	   (r->errtruncated != 0 && r->errtruncated != 1)){
		p->io.release(p->context, job->handle, 1);
		memset(job, 0, sizeof *job);
		return bad(error, "Invalid native process result bounds");
	}
	capture = job->capture;
	jbtext(out, "{\"exited\":true,\"exitcode\":"); jbint64(out, r->exitcode);
	if(capture == QgaCaptureStdout || capture == QgaCaptureSeparated || capture == QgaCaptureMerged){
		jbtext(out, ",\"out-data\":"); qgaencode64(out, r->out, r->nout);
		jbtext(out, ",\"out-truncated\":"); jbtext(out, r->outtruncated ? "true" : "false");
	}
	if(capture == QgaCaptureStderr || capture == QgaCaptureSeparated){
		jbtext(out, ",\"err-data\":"); qgaencode64(out, r->err, r->nerr);
		jbtext(out, ",\"err-truncated\":"); jbtext(out, r->errtruncated ? "true" : "false");
	}
	jbchar(out, '}');
	if(out->failed) return bad(error, "Cannot serialize native process result");
	handle = job->handle;
	memset(job, 0, sizeof *job);
	p->io.release(p->context, handle, 0);
	return 0;
}
