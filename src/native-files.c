/* SPDX-License-Identifier: MIT */
/* Native descriptors only. No shell, stdio buffering or read-then-create race. */
#include "files.h"

static int
fileopen(void *context, const char *path, int flags)
{
	Dir *d;
	int fd, mode, result;

	USED(context);
	mode = flags & QgaOpenWrite ? flags & QgaOpenRead ? ORDWR : OWRITE : OREAD;
	fd = open((char*)path, mode|OCEXEC);
	if(fd < 0 && (flags & QgaOpenTruncate))
		fd = create((char*)path, mode|OCEXEC|OEXCL, 0600);
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

	return qgafilesnew(&io, nil);
}
