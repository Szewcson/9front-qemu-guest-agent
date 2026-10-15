/* SPDX-License-Identifier: MIT */
/* Native descriptors only. Exclusive leaf creation; no shell or stdio buffer. */
#include "files.h"
#include "native-create.h"

static int
filecreate(const char *path, int mode)
{
	char parent[QgaFilePathMax], *slash;
	Dir *d;
	int fd, n, allowed;

	/* Cheap preflight for direct-device parents. The native helper then pins
	 * the actual parent and rejects unions before exclusive relative create. */
	n = strlen((char*)path);
	if(n < 1 || n >= (int)sizeof parent || path[0] != '/') return -1;
	memmove(parent, (void*)path, n+1);
	slash = strrchr(parent, '/');
	if(slash[1] == 0 || strcmp(slash+1, ".") == 0 || strcmp(slash+1, "..") == 0)
		return -1;
	if(slash == parent) slash++;
	*slash = 0;
	fd = open(parent, OREAD|OCEXEC);
	if(fd < 0) return -1;
	d = dirfstat(fd);
	allowed = d != nil && d->type == 'M' && (d->mode&DMDIR) &&
		(d->qid.type&QTDIR) && !(d->qid.type&QTAUTH);
	free(d);
	close(fd);
	if(!allowed) return -1;
	return nativefilecreate(path, mode);
}

static int
fileopen(void *context, const char *path, int flags)
{
	Dir *d;
	int fd, mode, result;

	USED(context);
	mode = flags & QgaOpenWrite ? flags & QgaOpenRead ? ORDWR : OWRITE : OREAD;
	fd = open((char*)path, mode|OCEXEC);
	if(fd < 0 && (flags & QgaOpenTruncate))
		fd = filecreate(path, mode);
	if(fd < 0)
		return -1;
	d = dirfstat(fd);
	/* Only mounted filesystem files; never a UART/clock/process/control device.
	 * The descriptor, not a pre-open path check, determines the actual target. */
	if(d == nil || d->type != 'M' || (d->mode & DMDIR) ||
	   ((d->mode & DMAPPEND) && (flags & QgaOpenWrite)) ||
	   (d->qid.type & (QTDIR|QTAUTH))){
		free(d);
		close(fd);
		return -1;
	}
	free(d);
	if(flags & QgaOpenTruncate){
		Dir change;

		nulldir(&change);
		change.length = 0;
		result = dirfwstat(fd, &change);
		if(result < 0){
			close(fd);
			return -1;
		}
	}
	return fd;
}

static int
fileclose(void *context, int fd)
{
	USED(context);
	return close(fd);
}

static int
fileread(void *context, int fd, uchar *data, int count)
{
	USED(context);
	return read(fd, data, count);
}

static int
filewrite(void *context, int fd, const uchar *data, int count)
{
	USED(context);
	return write(fd, (void*)data, count);
}

static int
fileseek(void *context, int fd, QgaInt64 offset, int whence, QgaInt64 *position)
{
	Dir *d;
	QgaInt64 base;

	USED(context);
	base = 0;
	if(whence == 1)
		base = seek(fd, 0, 1);
	else if(whence == 2){
		d = dirfstat(fd);
		if(d == nil)
			return -1;
		base = d->length;
		free(d);
	}else if(whence != 0)
		return -1;
	if(base < 0 || (offset > 0 && base > 9223372036854775807LL-offset) ||
	   (offset < 0 && offset < -base))
		return -1;
	*position = seek(fd, base+offset, 0);
	return *position < 0 ? -1 : 0;
}

QgaFiles*
nativefiles(void)
{
	static const QgaFileIO io = {fileopen, fileclose, fileread, filewrite, fileseek};

	/* No peer can rebind the namespace between parent validation and create.
	 * Exec supervisors take their own namespace copies too. */
	if(rfork(RFNAMEG) < 0) return nil;
	return qgafilesnew(&io, nil);
}
