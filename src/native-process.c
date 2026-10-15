/* SPDX-License-Identifier: MIT */
/* Private process supervisors; no RFMEM, threads, shell expansion or spool files. */
#include "native.h"
#include "process.h"

enum { PipeLimit = 32768, ReadyMagic = 0x51474134, StartupTicks = 500 };
typedef struct NativeJob NativeJob;
struct NativeJob { int fd, groupfd, alivefd; };
typedef struct Ready Ready;
struct Ready { int magic, pid; };

static int
pipenote(void *unused, char *note)
{
	USED(unused);
	/* Broken child/IPC pipes must be errors, not fatal notes in the UART owner. */
	return strncmp(note, "sys: write on closed pipe", 25) == 0 &&
		(note[25] == 0 || strncmp(note+25, " pc=", 4) == 0);
}

static vlong
queued(int fd)
{
	Dir *d;
	vlong n;
	d = dirfstat(fd);
	if(d == nil) return -1;
	n = d->length;
	free(d);
	return n;
}

static int
makepipe(int *fd)
{
	Dir d;
	Dir *existing;
	int i, nullfd;
	/* Reserve closed stdio slots before allocating any redirection source. */
	for(i = 0; i < 3; i++){
		existing = dirfstat(i);
		if(existing != nil){ free(existing); continue; }
		nullfd = open("/dev/null", ORDWR);
		if(nullfd < 0) return -1;
		if(nullfd != i){
			if(dup(nullfd, i) < 0){ close(nullfd); return -1; }
			close(nullfd);
		}
	}
	fd[0] = fd[1] = -1;
	if(pipe(fd) < 0) return -1;
	nulldir(&d);
	d.length = PipeLimit;
	if(dirfwstat(fd[0], &d) < 0){
		close(fd[0]); close(fd[1]); fd[0] = fd[1] = -1;
		return -1;
	}
	return 0;
}

static int
nullstdio(void)
{
	int fd, i;
	fd = open("/dev/null", ORDWR);
	if(fd < 0) return -1;
	for(i = 0; i < 3; i++)
		if(dup(fd, i) < 0){ if(fd > 2) close(fd); return -1; }
	if(fd > 2) close(fd);
	return 0;
}

static void
childexec(QgaExecRequest *r, int input, int output, int error, int failure)
{
	char path[QgaExecPathMax+8], name[QgaExecTextMax], *equals;
	int keep[4], fd, i, n;
	uchar failed = 1;

	/* All source fds are above stdio; close-on-exec failure handshake stays private. */
	if(dup(input, 0) < 0 || dup(output, 1) < 0 || dup(error, 2) < 0) goto fail;
	keep[0] = 0; keep[1] = 1; keep[2] = 2; keep[3] = failure;
	if(nativecloseexcept(keep, 4) < 0) goto fail;
	for(i = 0; r->env[i] != nil; i++){
		equals = strchr(r->env[i], '=');
		if(equals == nil) goto fail;
		n = equals-r->env[i];
		if(snprint(name, sizeof name, "/env/%.*s", n, r->env[i]) >= sizeof name) goto fail;
		fd = create(name, OWRITE|OCEXEC, 0600);
		if(fd < 0) goto fail;
		n = strlen(equals+1);
		if(write(fd, equals+1, n) != n){ close(fd); goto fail; }
		close(fd);
	}
	if(r->path[0] == '/') exec(r->path, r->argv);
	else{
		snprint(path, sizeof path, "/bin/%s", r->path);
		exec(path, r->argv);
	}
fail:
	write(failure, &failed, 1);
	exits("qga9 exec setup failed");
}

static int
drain(int fd, uchar *data, int *used, int *truncated, vlong budget)
{
	uchar scratch[4096];
	vlong available;
	int n, keep;
	while(budget > 0){
		available = queued(fd);
		if(available < 0) return -1;
		if(available == 0) break;
		n = sizeof scratch;
		if(available < n) n = available;
		if(budget < n) n = budget;
		n = read(fd, scratch, n); /* Sole reader: queued bytes cannot disappear. */
		if(n <= 0) return -1;
		keep = QgaCaptureMax-*used;
		if(keep > n) keep = n;
		memmove(data+*used, scratch, keep);
		*used += keep;
		if(keep < n) *truncated = 1;
		budget -= n;
	}
	return 0;
}

static void
abandon(void)
{
	char path[64];
	int fd;
	snprint(path, sizeof path, "/proc/%d/notepg", getpid());
	fd = open(path, OWRITE);
	if(fd >= 0){ write(fd, "kill", 4); close(fd); }
	exits("qga9 abandoned job");
}

static void
execwaiter(int failure, int notice)
{
	int keep[2], n;
	uchar result;
	keep[0] = failure; keep[1] = notice;
	if(nativecloseexcept(keep, 2) < 0) exits("exec handshake descriptors");
	n = read(failure, &result, 1);
	result = n == 0 ? 0 : 1;
	write(notice, &result, 1);
	exits(nil);
}

static void
supervise(int channel, int parentalive, QgaExecRequest *request)
{
	QgaExecResult *result;
	Ready ready;
	Waitmsg *w;
	char path[64], status[256];
	uchar ack;
	int input[2], output[2], error[2], failure[2], notice[2];
	int waitfd, execfd, pid, keep[2], outfd, errfd, helper;
	vlong pending;

	ready.magic = ReadyMagic; ready.pid = 0;
	/* Parent pins this fresh note group before allowing any spawn. */
	if(read(channel, &ack, 1) != 1) exits(nil);
	keep[0] = channel; keep[1] = parentalive;
	if(nativecloseexcept(keep, 2) < 0 || nullstdio() < 0) goto failed;
	result = malloc(sizeof *result);
	if(result == nil) goto failed;
	memset(result, 0, sizeof *result);
	if(makepipe(input) < 0 || makepipe(output) < 0 || makepipe(error) < 0 || makepipe(failure) < 0 || makepipe(notice) < 0)
		goto failed;
	/* Capacity was checked/set. One <=4096-byte prefill cannot fill a 32768 pipe. */
	if(request->ninput && write(input[1], request->input, request->ninput) != request->ninput) goto failed;
	snprint(path, sizeof path, "#d/%d", failure[1]);
	execfd = open(path, OWRITE|OCEXEC);
	if(execfd < 0) goto failed;
	close(failure[1]);
	snprint(path, sizeof path, "/proc/%d/wait", getpid());
	waitfd = open(path, OREAD|OCEXEC);
	if(waitfd < 0) goto failed;
	outfd = request->capture == QgaCaptureStdout || request->capture == QgaCaptureSeparated ||
		request->capture == QgaCaptureMerged ? output[1] : 1;
	errfd = request->capture == QgaCaptureStderr || request->capture == QgaCaptureSeparated ? error[1] : 2;
	if(request->capture == QgaCaptureMerged) errfd = output[1];
	pid = rfork(RFPROC|RFFDG|RFNAMEG|RFENVG|RFREND);
	if(pid < 0) goto failed;
	if(pid == 0) childexec(request, input[0], outfd, errfd, execfd);
	close(execfd); close(input[0]); close(input[1]); close(output[1]); close(error[1]);
	/* EOF means successful exec closed OCEXEC; a byte means setup/exec failed.
 	 * A private waiter blocks; the supervisor still detects death of its owner. */
	helper = rfork(RFPROC|RFFDG|RFNOWAIT);
	if(helper < 0) goto failed;
	if(helper == 0) execwaiter(failure[0], notice[1]);
	close(failure[0]); close(notice[1]);
	for(;;){
		if(pread(parentalive, status, sizeof status, 0) <= 0) abandon();
		pending = queued(notice[0]);
		if(pending < 0) goto failed;
		if(pending > 0) break;
		sleep(10);
	}
	if(read(notice[0], &ack, 1) != 1 || ack != 0) goto failed;
	close(notice[0]);
	ready.pid = pid;
	if(write(channel, &ready, sizeof ready) != sizeof ready) abandon();
	for(;;){
		if(pread(parentalive, status, sizeof status, 0) <= 0) abandon();
		if(drain(output[0], result->out, &result->nout, &result->outtruncated, PipeLimit) < 0 ||
		   drain(error[0], result->err, &result->nerr, &result->errtruncated, PipeLimit) < 0) goto failedafter;
		pending = queued(waitfd); /* Native procgen: length>0 means a wait record. */
		if(pending < 0) goto failedafter;
		if(pending > 0){
			w = wait();
			if(w == nil || w->pid != pid){ free(w); goto failedafter; }
			result->exited = 1;
			/* Plan 9 exits is a string: empty=success, nonempty=failure, no Linux signals. */
			result->exitcode = w->msg[0] == 0 ? 0 : 1;
			free(w);
			break;
		}
		sleep(10);
	}
	/* Capture up to one bounded queue snapshot after the direct child exits.
 	 * Background descendants cannot pin the result indefinitely. */
	pending = queued(output[0]);
	if(pending < 0 || drain(output[0], result->out, &result->nout, &result->outtruncated, pending) < 0) goto failedafter;
	pending = queued(error[0]);
	if(pending < 0 || drain(error[0], result->err, &result->nerr, &result->errtruncated, pending) < 0) goto failedafter;
	close(output[0]); close(error[0]); close(waitfd);
	if(write(channel, result, sizeof *result) != sizeof *result) abandon();
	/* Stay alive until reaped so PID/group identity cannot be recycled early. */
	if(read(channel, &ack, 1) != 1 || ack != 'r') abandon();
	exits(nil);
failedafter:
	/* Fatal supervisor error: status detects process disappearance, never false success. */
	abandon();
failed:
	write(channel, &ready, sizeof ready);
	read(channel, &ack, 1);
	abandon();
}

static void
release(void *unused, void *handle, int cancel)
{
	NativeJob *job;
	USED(unused);
	job = handle;
	if(cancel && job->groupfd >= 0) write(job->groupfd, "kill", 4);
	if(!cancel && job->fd >= 0) write(job->fd, "r", 1);
	if(job->fd >= 0) close(job->fd);
	if(job->groupfd >= 0) close(job->groupfd);
	if(job->alivefd >= 0) close(job->alivefd);
	free(job);
}

static int
spawn(void *unused, QgaExecRequest *r, int *pid, void **handle)
{
	NativeJob *job;
	Ready ready;
	char path[64];
	int channel[2], supervisor, i, parentalive, ownedfd;
	uchar ack = 1;
	vlong n;
	USED(unused);
	job = malloc(sizeof *job);
	if(job == nil) return -1;
	job->fd = job->groupfd = job->alivefd = -1;
	if(makepipe(channel) < 0){ free(job); return -1; }
	snprint(path, sizeof path, "/proc/%d/status", getpid());
	parentalive = open(path, OREAD|OCEXEC);
	if(parentalive < 0){ close(channel[0]); close(channel[1]); free(job); return -1; }
	supervisor = rfork(RFPROC|RFFDG|RFNAMEG|RFENVG|RFNOTEG|RFNOWAIT|RFREND);
	if(supervisor < 0){ close(parentalive); close(channel[0]); close(channel[1]); free(job); return -1; }
	if(supervisor == 0){ close(channel[0]); supervise(channel[1], parentalive, r); exits("supervisor"); }
	close(parentalive);
	close(channel[1]);
	job->fd = channel[0];
	snprint(path, sizeof path, "/proc/%d/notepg", supervisor);
	job->groupfd = open(path, OWRITE|OCEXEC);
	snprint(path, sizeof path, "/proc/%d/status", supervisor);
	job->alivefd = open(path, OREAD|OCEXEC);
	if(job->groupfd < 0 || job->alivefd < 0 || write(job->fd, &ack, 1) != 1) goto fail;
	/* A concurrently initiated fshalt exec must not inherit this job's IPC. */
	snprint(path, sizeof path, "#d/%d", job->fd);
	ownedfd = open(path, ORDWR|OCEXEC);
	if(ownedfd < 0) goto fail;
	close(job->fd); job->fd = ownedfd;
	for(i = 0; i < StartupTicks; i++){
		n = queued(job->fd);
		if(n < 0) goto fail;
		if(n >= sizeof ready){
			if(readn(job->fd, &ready, sizeof ready) != sizeof ready ||
			   ready.magic != ReadyMagic || ready.pid < 1) goto fail;
			*pid = ready.pid;
			*handle = job;
			return 0;
		}
		sleep(10);
	}
fail:
	release(nil, job, 1);
	return -1;
}

static int
poll(void *unused, void *handle, QgaExecResult *result)
{
	NativeJob *job;
	vlong n;
	char status[256];
	USED(unused);
	job = handle;
	n = queued(job->fd);
	if(n < 0) return -1;
	if(n < sizeof *result)
		return pread(job->alivefd, status, sizeof status, 0) > 0 ? 0 : -1;
	return readn(job->fd, result, sizeof *result) == sizeof *result ? 0 : -1;
}

QgaProcesses*
nativeprocesses(void)
{
	const QgaExecIO io = {spawn, poll, release};
	QgaProcesses *p;
	if(!atnotify(pipenote, 1)) return nil;
	p = qgaprocessnew(&io, nil);
	if(p == nil) atnotify(pipenote, 0);
	return p;
}

void
nativeprocessfree(QgaProcesses *p)
{
	if(p == nil) return;
	qgaprocessfree(p);
	atnotify(pipenote, 0);
}
