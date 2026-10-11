/* SPDX-License-Identifier: MIT */
#include "files.h"
#include "base64.h"

typedef struct Handle Handle;
struct Handle {
	QgaInt64 id;
	int fd;
	int mode;
	int eof;
};

struct QgaFiles {
	QgaFileIO io;
	void *context;                /* Borrowed until qgafilesfree. */
	QgaInt64 nextid;
	Handle handles[QgaFileMax];
	char path[QgaFilePathMax];
	char encoded[QgaFileBase64Max+1];
	uchar data[QgaFileChunk];
};

QgaFiles*
qgafilesnew(const QgaFileIO *io, void *context)
{
	QgaFiles *f;
	int i;

	if(io == nil || io->open == nil || io->close == nil || io->read == nil ||
	   io->write == nil || io->seek == nil)
		return nil;
	f = malloc(sizeof *f);
	if(f == nil)
		return nil;
	memset(f, 0, sizeof *f);
	f->io = *io;
	f->context = context;
	f->nextid = 1;
	for(i = 0; i < QgaFileMax; i++)
		f->handles[i].fd = -1;
	return f;
}

void
qgafilesfree(QgaFiles *f)
{
	int i;

	if(f == nil)
		return;
	for(i = 0; i < QgaFileMax; i++)
		if(f->handles[i].fd >= 0)
			f->io.close(f->context, f->handles[i].fd);
	memset(f, 0, sizeof *f);
	free(f);
}

static int
bad(const char **error, const char *message)
{
	*error = message;
	return -1;
}

static int
arguments(Jdoc *d, int a, const char *const *allowed, int count)
{
	int i, j;

	if(a < 0 || a >= d->ntoken || d->token[a].type != Jobject)
		return -1;
	for(i = a+1; i < d->token[a].next; i = d->token[i+1].next){
		for(j = 0; j < count; j++)
			if(jmatch(d, i, allowed[j]))
				break;
		if(j == count)
			return -1;
	}
	return 0;
}

static int
openfile(QgaFiles *f, Jdoc *d, int a, Jbuf *out, const char **error)
{
	const char *const allowed[] = {"path", "mode"};
	const char *const modes[] = {"r", "rb", "r+", "r+b", "rb+", "w", "wb", "w+", "w+b", "wb+"};
	char mode[8];
	Handle *h;
	int i, m, flags, fd;

	if(arguments(d, a, allowed, 2) < 0 ||
	   jgetstring(d, jmember(d, a, "path"), f->path, sizeof f->path) < 1 || f->path[0] != '/')
		return bad(error, "File path must be absolute, nonempty and shorter than 1024 bytes");
	m = jmember(d, a, "mode");
	if(m < 0)
		strcpy(mode, "r");
	else if(jgetstring(d, m, mode, sizeof mode) < 0)
		return bad(error, "Invalid file open mode");
	if(strcmp(mode, "a") == 0 || strcmp(mode, "ab") == 0 || strcmp(mode, "a+") == 0 ||
	   strcmp(mode, "a+b") == 0 || strcmp(mode, "ab+") == 0){
		*error = "Atomic per-open append is not supported by the native file interface";
		return -2;
	}
	for(i = 0; i < 10; i++)
		if(strcmp(mode, modes[i]) == 0)
			break;
	if(i == 10)
		return bad(error, "Invalid file open mode");
	flags = i < 5 ? QgaOpenRead : QgaOpenWrite|QgaOpenTruncate;
	if(i%5 >= 2)
		flags |= QgaOpenRead|QgaOpenWrite;
	h = nil;
	for(i = 0; i < QgaFileMax; i++)
		if(f->handles[i].fd < 0){
			h = &f->handles[i];
			break;
		}
	if(h == nil || f->nextid == 0)
		return bad(error, "File handle limit or identifier space exhausted");
	fd = f->io.open(f->context, f->path, flags);
	if(fd < 0)
		return bad(error, "Cannot open a regular native file");
	h->id = f->nextid;
	f->nextid = f->nextid == 9223372036854775807LL ? 0 : f->nextid+1;
	h->fd = fd;
	h->mode = flags;
	h->eof = 0;
	jbint64(out, h->id);
	return 0;
}

int
qgafilesop(void *context, int command, Jdoc *d, int a, Jbuf *out, const char **error)
{
	const char *const common[] = {"handle", "count", "buf-b64", "offset", "whence"};
	const char *allowed[3];
	QgaFiles *f;
	Handle *h;
	QgaInt64 id, count, offset, position;
	int i, n, used, which, t, result;

	f = context;
	if(f == nil || error == nil || d == nil || out == nil)
		return -1;
	/* Reserve the largest reply before any I/O. Core separately reserves ID/framing. */
	if(out->failed || out->size-out->used < QgaFileBase64Max+128)
		return bad(error, "Insufficient file response space");
	if(command == QgaFileOpen)
		return openfile(f, d, a, out, error);
	if(command < QgaFileClose || command > QgaFileFlush)
		return bad(error, "Invalid file operation");
	allowed[0] = common[0];
	n = 1;
	if(command == QgaFileRead || command == QgaFileWrite)
		allowed[n++] = common[1];
	if(command == QgaFileWrite)
		allowed[n++] = common[2];
	if(command == QgaFileSeek){
		allowed[n++] = common[3];
		allowed[n++] = common[4];
	}
	if(arguments(d, a, allowed, n) < 0 || jgetint64(d, jmember(d, a, "handle"), &id) < 0 || id < 1)
		return bad(error, "Invalid file arguments or handle");
	h = nil;
	for(i = 0; i < QgaFileMax; i++)
		if(f->handles[i].fd >= 0 && f->handles[i].id == id){
			h = &f->handles[i];
			break;
		}
	if(h == nil)
		return bad(error, "Unknown or closed file handle");
	if(command == QgaFileClose){
		i = h->fd;
		h->fd = -1;             /* Consume once, even if the OS reports close failure. */
		result = f->io.close(f->context, i);
		if(result < 0)
			return bad(error, "Cannot close native file");
		jbtext(out, "{}");
		return 0;
	}
	if(command == QgaFileFlush){
		jbtext(out, "{}");      /* All writes go straight to the native kernel/server. */
		return 0;
	}
	if(command == QgaFileSeek){
		if(jgetint64(d, jmember(d, a, "offset"), &offset) < 0)
			return bad(error, "File seek requires a signed 64-bit offset");
		t = jmember(d, a, "whence");
		which = -1;
		if(jgetint64(d, t, &count) == 0 && count >= 0 && count <= 2)
			which = count;
		else if(jmatch(d, t, "set")) which = 0;
		else if(jmatch(d, t, "cur")) which = 1;
		else if(jmatch(d, t, "end")) which = 2;
		if(which < 0 || f->io.seek(f->context, h->fd, offset, which, &position) < 0 || position < 0)
			return bad(error, "Cannot seek native file or invalid whence");
		h->eof = 0;
		jbtext(out, "{\"position\":");
		jbint64(out, position);
		jbtext(out, ",\"eof\":false}");
		return 0;
	}
	t = jmember(d, a, "count");
	count = QgaFileChunk;
	if(t >= 0 && (jgetint64(d, t, &count) < 0 || count < 0 || count > QgaFileChunk))
		return bad(error, "File transfer count must be between 0 and 4096 bytes");
	if(command == QgaFileRead){
		if(!(h->mode & QgaOpenRead))
			return bad(error, "File handle is not readable");
		used = 0;
		while(used < count && !h->eof){
			n = f->io.read(f->context, h->fd, f->data+used, count-used);
			if(n < 0 || n > count-used)
				return bad(error, "Cannot read native file");
			if(n == 0)
				h->eof = 1;
			used += n;
		}
		jbtext(out, "{\"count\":");
		jbint64(out, used);
		jbtext(out, ",\"buf-b64\":");
		qgaencode64(out, f->data, used);
		jbtext(out, h->eof ? ",\"eof\":true}" : ",\"eof\":false}");
		return 0;
	}
	if(!(h->mode & QgaOpenWrite))
		return bad(error, "File handle is not writable");
	n = jgetstring(d, jmember(d, a, "buf-b64"), f->encoded, sizeof f->encoded);
	if(n < 0 || (used = qgadecode64(f->encoded, n, f->data, sizeof f->data)) < 0)
		return bad(error, "Invalid or oversized canonical base64 buffer");
	if(t < 0)
		count = used;
	if(count > used)
		return bad(error, "File write count exceeds decoded buffer size");
	n = count == 0 ? 0 : f->io.write(f->context, h->fd, f->data, count);
	if(n < 0 || n > count)
		return bad(error, "Cannot write native file");
	h->eof = 0;
	jbtext(out, "{\"count\":");
	jbint64(out, n);
	jbtext(out, ",\"eof\":false}");
	return 0;
}
