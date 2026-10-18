/* SPDX-License-Identifier: MIT */
#include "qjson.h"

struct Qga {
	QgaWrite write;
	void *context;                /* Borrowed; owner outlives the instance. */
	QgaQuery query;
	QgaLoadQuery loadquery;
	void *loadcontext;
	QgaShutdown shutdown;
	void *shutdowncontext;
	QgaFileop fileop;
	void *filecontext;
	QgaSettime settime;
	void *timecontext;
	QgaFileop execop;
	void *execcontext;
	int shuttingdown;
	char message[QgaMessageMax];
	char output[QgaOutputMax];
	Jdoc doc;
	Jbuf buf;
	int used;
	int depth;
	int quoted;
	int escape;
	int discard;
	int failed;
};

typedef struct Command Command;
enum {
	CmdSync, CmdSyncDelimited, CmdPing, CmdInfo,
	CmdQuery, CmdLoad, CmdFile, CmdSettime, CmdExec, CmdShutdown,
	CmdNoReply = 1,
	CmdBlockShutdown = 2,
};
struct Command {
	const char *name;
	int kind;
	int op;
	int flags;
};

static const Command commands[] = {
	{"guest-sync", CmdSync, 0, 0},
	{"guest-sync-delimited", CmdSyncDelimited, 0, 0},
	{"guest-ping", CmdPing, 0, 0},
	{"guest-info", CmdInfo, 0, 0},
	{"guest-get-osinfo", CmdQuery, QgaOsinfo, 0},
	{"guest-get-host-name", CmdQuery, QgaHostname, 0},
	{"guest-get-time", CmdQuery, QgaTime, 0},
	{"guest-network-get-interfaces", CmdQuery, QgaInterfaces, 0},
	{"guest-shutdown", CmdShutdown, 0, CmdNoReply},
	{"guest-get-vcpus", CmdQuery, QgaVcpus, 0},
	{"guest-get-timezone", CmdQuery, QgaTimezone, 0},
	{"guest-network-get-route", CmdQuery, QgaRoutes, 0},
	{"guest-file-open", CmdFile, QgaFileOpen, CmdBlockShutdown},
	{"guest-file-close", CmdFile, QgaFileClose, CmdBlockShutdown},
	{"guest-file-read", CmdFile, QgaFileRead, CmdBlockShutdown},
	{"guest-file-write", CmdFile, QgaFileWrite, CmdBlockShutdown},
	{"guest-file-seek", CmdFile, QgaFileSeek, CmdBlockShutdown},
	{"guest-file-flush", CmdFile, QgaFileFlush, CmdBlockShutdown},
	{"guest-set-time", CmdSettime, 0, CmdBlockShutdown},
	{"guest-exec", CmdExec, QgaExec, CmdBlockShutdown},
	{"guest-exec-status", CmdExec, QgaExecStatus, CmdBlockShutdown},
	{"guest-get-disks", CmdQuery, QgaDisks, 0},
	{"guest-get-load", CmdLoad, 0, 0},
};

static const char *const unsupported[] = {
	"guest-set-user-password",
	"guest-fsfreeze-status", "guest-fsfreeze-freeze",
	"guest-fsfreeze-freeze-list", "guest-fsfreeze-thaw", "guest-fstrim",
	"guest-ssh-get-authorized-keys",
	"guest-ssh-add-authorized-keys", "guest-ssh-remove-authorized-keys",
	"guest-suspend-ram", "guest-suspend-disk", "guest-suspend-hybrid",
	"guest-set-vcpus", "guest-get-memory-blocks", "guest-set-memory-blocks",
	"guest-get-memory-block-info", "guest-get-cpustats",
	"guest-get-fsinfo", "guest-get-diskstats",
	"guest-get-users", "guest-get-devices",
};

static void
reset(Qga *q)
{
	q->used = 0;
	q->depth = 0;
	q->quoted = 0;
	q->escape = 0;
	q->discard = 0;
}

Qga*
qganew(QgaWrite writefn, void *context)
{
	Qga *q;

	if(writefn == nil)
		return nil;
	q = malloc(sizeof *q);
	if(q == nil)
		return nil;
	memset(q, 0, sizeof *q);
	q->write = writefn;
	q->context = context;
	return q;
}

void
qgafree(Qga *q)
{
	free(q);
}

void
qgaqueries(Qga *q, QgaQuery query)
{
	q->query = query;
}

void
qgaloadops(Qga *q, QgaLoadQuery query, void *context)
{
	q->loadquery=query; q->loadcontext=context;
}

void
qgashutdown(Qga *q, QgaShutdown shutdownfn, void *context)
{
	q->shutdown = shutdownfn;
	q->shutdowncontext = context;
}

void
qgafileops(Qga *q, QgaFileop fileop, void *context)
{
	q->fileop = fileop;
	q->filecontext = context;
}

void
qgasettime(Qga *q, QgaSettime settime, void *context)
{
	q->settime = settime;
	q->timecontext = context;
}

static int
available(Qga *q, const Command *c)
{
	switch(c->kind){
	case CmdSync: case CmdSyncDelimited: case CmdPing: case CmdInfo: return 1;
	case CmdQuery: return q->query != nil;
	case CmdLoad: return q->loadquery != nil;
	case CmdFile: return q->fileop != nil;
	case CmdSettime: return q->settime != nil;
	case CmdExec: return q->execop != nil;
	case CmdShutdown: return q->shutdown != nil;
	}
	return 0;
}

void
qgaexecops(Qga *q, QgaFileop execop, void *context)
{
	q->execop = execop;
	q->execcontext = context;
}

static int
send(Qga *q, int id)
{
	long n;
	int done;

	if(id >= 0){
		jbtext(&q->buf, ",\"id\":");
		jbvalue(&q->buf, &q->doc, id);
	}
	jbtext(&q->buf, "}\r\n");
	if(q->buf.failed){
		q->failed = 1;
		return -1;              /* Never send truncated or invalid JSON. */
	}
	for(done = 0; done < q->buf.used; done += n){
		n = q->write(q->context, (uchar*)q->output + done, q->buf.used - done);
		if(n <= 0 || n > q->buf.used - done){
			q->failed = 1;
			return -1;
		}
	}
	return 0;
}

static int
error(Qga *q, int id, const char *class, const char *desc)
{
	jbinit(&q->buf, q->output, sizeof q->output);
	jbtext(&q->buf, "{\"error\":{\"class\":");
	jbquote(&q->buf, class);
	jbtext(&q->buf, ",\"desc\":");
	jbquote(&q->buf, desc);
	jbchar(&q->buf, '}');
	return send(q, id);
}

static int
dispatch(Qga *q)
{
	Jdoc *d;
	const Command *c;
	int id, execute, arguments, i, syncid, mode, argmode, first, reserved, result;
	const char *description;
	QgaInt64 time;

	d = &q->doc;
	if(!jparse(d, q->message, q->used) || d->token[0].type != Jobject)
		return error(q, -1, "GenericError", "Invalid JSON object or parser limit exceeded");
	id = jmember(d, 0, "id");
	execute = jmember(d, 0, "execute");
	arguments = jmember(d, 0, "arguments");
	if(execute < 0 || d->token[execute].type != Jstring)
		return error(q, id, "GenericError", "execute must be a string");
	for(i = 1; i < d->token[0].next; i = d->token[i+1].next)
		if(!jmatch(d, i, "execute") && !jmatch(d, i, "arguments") && !jmatch(d, i, "id"))
			return error(q, id, "GenericError", "Unexpected request member");
	if(arguments >= 0 && d->token[arguments].type != Jobject)
		return error(q, id, "GenericError", "arguments must be an object");
	c = nil;
	for(i = 0; i < (int)(sizeof commands / sizeof commands[0]); i++)
		if(jmatch(d, execute, commands[i].name)){
			c = &commands[i];
			break;
		}
	if(c == nil){
		for(i = 0; i < (int)(sizeof unsupported / sizeof unsupported[0]); i++)
			if(jmatch(d, execute, unsupported[i]))
				return error(q, id, "Unsupported", "Command is not implemented or is excluded by policy");
		return error(q, id, "CommandNotFound", "Unknown command");
	}
	if(!available(q, c))
		return error(q, id, "Unsupported", "Command is not implemented or is excluded by policy");
	if(q->shuttingdown && (c->flags & CmdBlockShutdown))
		return error(q, id, "GenericError", "Shutdown is already in progress");
	if(c->kind == CmdSettime){
		argmode = jmember(d, arguments, "time");
		time = 0;
		if((arguments >= 0 && d->token[arguments].count != (argmode >= 0 ? 1 : 0)) ||
		   (argmode >= 0 && jgetint64(d, argmode, &time) < 0))
			return error(q, id, "GenericError", "set-time accepts only an optional signed 64-bit time");
		description = "Native clock operation failed";
		result = q->settime(q->timecontext, argmode >= 0, time, &description);
		if(result < 0)
			return error(q, id, result == -2 ? "Unsupported" : "GenericError", description);
		jbinit(&q->buf, q->output, sizeof q->output);
		jbtext(&q->buf, "{\"return\":{}");
		return send(q, id);
	}
	if(c->kind == CmdFile || c->kind == CmdExec){
		/* Reserve the exact transaction ID and framing before state-changing I/O. */
		jbinit(&q->buf, q->output, sizeof q->output);
		if(id >= 0)
			jbvalue(&q->buf, d, id);
		if(q->buf.failed)
			return error(q, id, "GenericError", "Transaction ID exceeds response limits");
		reserved = q->buf.used + (id >= 0 ? 6 : 0) + 3;
		jbinit(&q->buf, q->output, sizeof q->output-reserved);
		jbtext(&q->buf, "{\"return\":");
		description = "Native file operation failed";
		if(c->kind == CmdFile)
			result = q->fileop(q->filecontext, c->op, d, arguments, &q->buf, &description);
		else{
			description = "Native process operation failed";
			result = q->execop(q->execcontext, c->op, d, arguments, &q->buf, &description);
		}
		if(result < 0 || q->buf.failed)
			return error(q, id, result == -2 ? "Unsupported" : "GenericError", description);
		q->buf.size = sizeof q->output;
		return send(q, id);
	}
	if(c->kind == CmdShutdown){
		argmode = jmember(d, arguments, "mode");
		if(arguments >= 0 && d->token[arguments].count != (argmode >= 0 ? 1 : 0))
			return error(q, id, "GenericError", "Unexpected shutdown argument");
		mode = QgaPowerdown;
		if(argmode >= 0){
			if(jmatch(d, argmode, "reboot"))
				mode = QgaReboot;
			else if(jmatch(d, argmode, "halt"))
				return error(q, id, "Unsupported", "halt without poweroff is not implemented");
			else if(!jmatch(d, argmode, "powerdown"))
				return error(q, id, "GenericError", "Invalid shutdown mode");
		}
		if(q->shuttingdown)
			return error(q, id, "GenericError", "Shutdown is already in progress");
		if(q->shutdown(q->shutdowncontext, mode) < 0)
			return error(q, id, "GenericError", "Cannot initiate native shutdown");
		q->shuttingdown = 1;
		return 0;                    /* QGA shutdown success has NO response. */
	}
	syncid = -1;
	if(c->kind == CmdSync || c->kind == CmdSyncDelimited){
		syncid = jmember(d, arguments, "id");
		if(arguments < 0 || d->token[arguments].count != 1 || !jint64(d, syncid))
			return error(q, id, "GenericError", "Sync requires exactly one signed 64-bit integer id");
	}else if(arguments >= 0 && d->token[arguments].count != 0)
		return error(q, id, "GenericError", "Command takes no arguments");
	/* Queries can fill the reply: reserve the escaped ID before native I/O too. */
	reserved = 0;
	if(c->kind == CmdQuery || c->kind == CmdLoad){
		jbinit(&q->buf, q->output, sizeof q->output);
		if(id >= 0)
			jbvalue(&q->buf, d, id);
		if(q->buf.failed)
			return error(q, id, "GenericError", "Transaction ID exceeds response limits");
		reserved = q->buf.used + (id >= 0 ? 6 : 0) + 3;
	}
	jbinit(&q->buf, q->output, sizeof q->output-reserved);
	if(c->kind == CmdSyncDelimited)
		jbchar(&q->buf, 0xFF);   /* Only a successful delimited sync is prefixed. */
	jbtext(&q->buf, "{\"return\":");
	if(c->kind == CmdSync || c->kind == CmdSyncDelimited)
		jbvalue(&q->buf, d, syncid);
	else if(c->kind == CmdPing)
		jbtext(&q->buf, "{}");
	else if(c->kind == CmdQuery || c->kind == CmdLoad){
		result = c->kind == CmdLoad ?
			q->loadquery(q->loadcontext, &q->buf) : q->query(c->op, &q->buf);
		if(result < 0 || q->buf.failed)
			return error(q, id, "GenericError", "Cannot read or serialize native guest information");
	}else if(c->kind == CmdInfo){
		jbtext(&q->buf, "{\"version\":\"qga9-0.6.1-dev\",\"supported_commands\":[");
		first = 1;
		for(i = 0; i < (int)(sizeof commands / sizeof commands[0]); i++){
			if(!available(q, &commands[i]))
				continue;
			if(!first)
				jbchar(&q->buf, ',');
			first = 0;
			jbtext(&q->buf, "{\"name\":");
			jbquote(&q->buf, commands[i].name);
			jbtext(&q->buf, ",\"enabled\":true,\"success-response\":");
			jbtext(&q->buf, (commands[i].flags & CmdNoReply) ? "false}" : "true}");
		}
		jbtext(&q->buf, "]}");
	}else
		return error(q, id, "GenericError", "Invalid command descriptor");
	q->buf.size = sizeof q->output;
	return send(q, id);
}

int
qgafeed(Qga *q, const uchar *s, int n)
{
	int i, c;

	if(q == nil || s == nil || n < 0 || q->failed)
		return -1;
	for(i = 0; i < n; i++){
		c = s[i];
		if(c == 0xFF){
			reset(q);              /* Reset even in a string or discarded frame. */
			continue;
		}
		if(q->discard){
			if(c == '\n')
				reset(q);
			continue;
		}
		if(q->used == 0){
			if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
				continue;
			if(c != '{'){
				if(error(q, -1, "GenericError", "Expected a JSON object") < 0)
					return -1;
				q->discard = 1;
				continue;
			}
		}
		if(q->used == QgaMessageMax){
			if(error(q, -1, "GenericError", "Incoming message exceeds 8192 bytes") < 0)
				return -1;
			reset(q);
			/* This byte itself may be the delimiter. */
			q->discard = c != '\n';
			continue;
		}
		q->message[q->used++] = c;
		if(q->quoted){
			if(c == '\n' || c == '\r'){
				if(error(q, -1, "GenericError", "Unescaped newline in string") < 0)
					return -1;
				reset(q);
			}else if(q->escape)
				q->escape = 0;
			else if(c == '\\')
				q->escape = 1;
			else if(c == '"')
				q->quoted = 0;
		}else if(c == '"')
			q->quoted = 1;
		else if(c == '{' || c == '[')
			q->depth++;
		else if(c == '}' || c == ']'){
			if(--q->depth == 0){
				if(dispatch(q) < 0)
					return -1;
				reset(q);
			}
		}
	}
	return 0;
}

int
qgaend(Qga *q)
{
	int result;

	if(q == nil || q->failed)
		return -1;
	result = 0;
	if(q->used != 0)
		result = error(q, -1, "GenericError", "Truncated JSON message at end of stream");
	reset(q);
	return result;
}
