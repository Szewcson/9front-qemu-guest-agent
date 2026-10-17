/* SPDX-License-Identifier: MIT */
/* Opt-in native experiment: a mount's historical srv name is not provenance. */
#include <u.h>
#include <libc.h>

static int
same(Dir *a, Dir *b)
{
	return a != nil && b != nil && a->type == b->type && a->dev == b->dev &&
		a->qid.path == b->qid.path && a->qid.vers == b->qid.vers &&
		a->qid.type == b->qid.type;
}

static int
snapshot(char *out, int size)
{
	char path[64], extra;
	int fd, n, used;

	if(out == nil || size < 2)
		return -1;
	snprint(path, sizeof path, "#p/%d/ns", getpid());
	fd = open(path, OREAD|OCEXEC);
	if(fd < 0)
		return -1;
	n = 0;
	used = 0;
	while(used < size-1){
		n = read(fd, out+used, size-1-used);
		if(n <= 0)
			break;
		used += n;
	}
	if(n < 0 || (used == size-1 && read(fd, &extra, 1) != 0)){
		close(fd);
		return -1;
	}
	close(fd);
	out[used] = 0;
	return used;
}

void
main(void)
{
	char id[32], board[64], before[16384], after[16384], marker, reason[ERRMAX];
	char *error;
	int lease, source, posted, pipefd[2], fd, pid, n, mounted;
	Dir *oldpipe, *newpipe, *oldmount, *newmount;
	Waitmsg *w;

	lease = source = posted = fd = pipefd[0] = pipefd[1] = -1;
	oldpipe = newpipe = oldmount = newmount = nil;
	mounted = 0;
	error = "filesystem provenance experiment";
	/* Only this process changes mounts. No inherited service entry is removed. */
	if(rfork(RFNAMEG|RFENVG|RFFDG|RFNOTEG) < 0)
		goto Done;
	alarm(15000);
	error = "private service board";
	lease = open("#s/clone", OREAD|OCEXEC);
	if(lease < 0)
		goto Done;
	n = read(lease, id, sizeof id-1);
	if(n < 1 || n >= sizeof id-1)
		goto Done;
	id[n] = 0;
	for(n = 0; id[n] >= '0' && id[n] <= '9'; n++)
		;
	if(n == 0 || (id[n] != 0 && strcmp(id+n, "\n") != 0))
		goto Done;
	id[n] = 0;
	snprint(board, sizeof board, "#s/%s", id);
	if(bind(board, "/srv", MREPL|MCREATE) < 0)
		goto Done;
	error = "start private ramfs";
	pid = rfork(RFPROC|RFFDG|RFNOTEG);
	if(pid < 0)
		goto Done;
	if(pid == 0){
		execl("/bin/ramfs", "ramfs", "-S", "qga9-fsinfo-probe", nil);
		exits("exec ramfs");
	}
	w = wait();
	if(w == nil)
		goto Done;
	n = w->pid == pid && w->msg[0] == 0;
	free(w);
	if(!n)
		goto Done;
	error = "open original service";
	source = open("/srv/qga9-fsinfo-probe", ORDWR|OCEXEC);
	if(source < 0)
		goto Done;
	oldpipe = dirfstat(source);
	if(oldpipe == nil || oldpipe->type != '|')
		goto Done;
	error = "mount original service";
	if(mount(source, -1, "/mnt", MREPL|MCREATE, "") < 0)
		goto Done;
	/* Native mount consumes the descriptor on success. */
	source = -1;
	mounted = 1;
	error = "create original marker";
	fd = create("/mnt/original", OWRITE|OCEXEC, 0600);
	if(fd < 0 || write(fd, "A", 1) != 1)
		goto Done;
	close(fd);
	fd = -1;
	error = "original mount snapshot";
	oldmount = dirstat("/mnt");
	if(oldmount == nil || oldmount->type != 'M' ||
	   snapshot(before, sizeof before) < 0 ||
	   strstr(before, "mount -c /srv/qga9-fsinfo-probe /mnt\n") == nil)
		goto Done;
	error = "replace private posting";
	if(remove("/srv/qga9-fsinfo-probe") < 0 || pipe(pipefd) < 0)
		goto Done;
	posted = create("/srv/qga9-fsinfo-probe", OWRITE|OCEXEC, 0600);
	if(posted < 0 || fprint(posted, "%d", pipefd[0]) < 1)
		goto Done;
	close(posted);
	posted = -1;
	error = "stat replacement posting";
	fd = open("/srv/qga9-fsinfo-probe", ORDWR|OCEXEC);
	if(fd < 0)
		goto Done;
	/* Stat only: no byte is read from or written to either posted channel. */
	newpipe = dirfstat(fd);
	close(fd);
	fd = -1;
	newmount = dirstat("/mnt");
	error = "compare mount and posting identities";
	if(newpipe == nil || newpipe->type != '|' || same(oldpipe, newpipe) ||
	   !same(oldmount, newmount) || snapshot(after, sizeof after) < 0 ||
	   strcmp(before, after) != 0)
		goto Done;
	error = "read original marker";
	fd = open("/mnt/original", OREAD|OCEXEC);
	if(fd < 0 || read(fd, &marker, 1) != 1 || marker != 'A')
		goto Done;
	error = nil;
Done:
	if(error != nil) rerrstr(reason, sizeof reason);
	alarm(0);
	if(fd >= 0) close(fd);
	if(posted >= 0) close(posted);
	if(mounted) unmount(nil, "/mnt");
	if(source >= 0) close(source);
	/* Dropping the private board also drops its replacement pipe reference. */
	if(lease >= 0) close(lease);
	if(pipefd[0] >= 0) close(pipefd[0]);
	if(pipefd[1] >= 0) close(pipefd[1]);
	free(oldpipe);
	free(newpipe);
	free(oldmount);
	free(newmount);
	if(error != nil)
		sysfatal("%s: %s", error, reason);
	print("fsinfo provenance: unchanged mount and namespace, different posted pipe; original file still readable\n");
	exits(nil);
}
