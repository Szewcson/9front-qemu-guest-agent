/* SPDX-License-Identifier: MIT */
/* Native leased srv boards pass descriptors across private namespace/fd groups. */
#ifndef QGA9_NATIVE_CREATE_TEST
#include "native.h"
#endif
#include "native-create.h"

static int
createdirectory(int fd)
{
	Dir *d;
	int allowed;
	d = dirfstat(fd);
	allowed = d != nil && d->type == 'M' && (d->mode&DMDIR) &&
		(d->qid.type&QTDIR) && !(d->qid.type&QTAUTH);
	free(d);
	return allowed;
}

static int
createchild(char *parent, char *leaf, int mode, int postfd, int replyfd)
{
	char number[32];
	int keep[2], fd, n, result;

	keep[0] = postfd; keep[1] = replyfd;
	/* RFFDG: closing inherited channel/job descriptors cannot affect parent.
	 * Close them BEFORE any filesystem operation that could block. */
	if(nativecloseexcept(keep, 2) < 0) return -1;
	if(chdir(parent) < 0 || bind(".", ".", MREPL|MCREATE) < 0) return -1;
	/* Native cmount refuses MCREATE when the source is a union. After the
	 * self-bind, relative create follows this exact dot Chan, not the pathname
	 * or an ancestor union. Only one pinned mounted parent can receive create. */
	fd = open(".", OREAD|OCEXEC);
	if(fd < 0) return -1;
	result = createdirectory(fd); close(fd);
	if(!result) return -1;
	fd = create(leaf, mode|OCEXEC|OEXCL, 0600);
	if(fd < 0) return -1;
	n = snprint(number, sizeof number, "%d", fd);
	result = n > 0 && n < (int)sizeof number && write(postfd, number, n) == n;
	close(fd);
	/* srv retains the created Chan until the parent receives it. Any failure
	 * tears down the leased board; it never guesses a filename to remove. */
	if(!result) return -1;
	return write(replyfd, "1", 1) == 1 ? 0 : -1;
}

int
nativefilecreate(const char *path, int mode)
{
	char parent[QgaFilePathMax], *leaf, *directory, board[32], entry[80], extra;
	Dir change;
	int lease, post, fd, pipefd[2], n, used, pid, result;

	if(path == nil || (mode != OREAD && mode != OWRITE && mode != ORDWR)) return -1;
	n = strlen((char*)path);
	if(n < 1 || n >= (int)sizeof parent || path[0] != '/') return -1;
	memmove(parent, (void*)path, n+1);
	leaf = strrchr(parent, '/');
	if(leaf[1] == 0 || strcmp(leaf+1, ".") == 0 || strcmp(leaf+1, "..") == 0) return -1;
	*leaf++ = 0;
	/* Keep the root parent separate from the leaf storage. */
	directory = parent[0] == 0 ? "/" : parent;
	lease = open("#s/clone", OREAD|OCEXEC);
	if(lease < 0) return -1;
	post = -1; pipefd[0] = pipefd[1] = -1; result = -1;
	used = 0;
	while(used < (int)sizeof board-1){
		n = read(lease, board+used, sizeof board-1-used);
		if(n < 0) goto out;
		if(n == 0) break;
		used += n;
	}
	if(used < 1 || used == (int)sizeof board-1 || read(lease, &extra, 1) != 0) goto out;
	board[used] = 0;
	for(n=0; n<used; n++) if(board[n] < '0' || board[n] > '9') goto out;
	n = snprint(entry, sizeof entry, "#s/%s", board);
	if(n < 1 || n >= (int)sizeof entry) goto out;
	nulldir(&change); change.mode = 0700;
	if(dirwstat(entry, &change) < 0) goto out;
	n = snprint(entry, sizeof entry, "#s/%s/qga9-file", board);
	if(n < 1 || n >= (int)sizeof entry) goto out;
	post = create(entry, OWRITE|OCEXEC|OEXCL|ORCLOSE, 0600);
	if(post < 0 || pipe(pipefd) < 0) goto out;
	pid = rfork(RFPROC|RFFDG|RFNAMEG|RFNOTEG|RFNOWAIT);
	if(pid < 0) goto out;
	if(pid == 0){
		result = createchild(directory, leaf, mode, post, pipefd[1]);
		_exits(result == 0 ? nil : "file create");
	}
	close(pipefd[1]); pipefd[1] = -1;
	if(read(pipefd[0], &extra, 1) != 1 || extra != '1' ||
	   read(pipefd[0], &extra, 1) != 0) goto out;
	fd = open(entry, mode|OCEXEC);
	if(fd < 0) goto out;
	result = fd;
out:
	if(pipefd[0] >= 0) close(pipefd[0]);
	if(pipefd[1] >= 0) close(pipefd[1]);
	/* ctl ORCLOSE and lease close are Qid/reference based, not remove(path).
	 * Child death/EOF, caller death and all local failures leave no srv leak. */
	if(post >= 0) close(post);
	close(lease);
	return result;
}
