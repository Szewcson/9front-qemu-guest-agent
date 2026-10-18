/* SPDX-License-Identifier: MIT */
/* Native data sources and the management channel's fixed-argv shutdown path. */
#include "native.h"
#include "guestinfo.h"
#include <ip.h>

enum { StatusMax = 8192, InterfaceMax = 64, AddressMax = 64 };

/* Read to EOF; fail on truncation rather than treating a prefix as a whole file. */
static int
readtext(char *path, char *buf, int size)
{
	int fd, used, n, i;
	char extra;

	fd = open(path, OREAD|OCEXEC);
	if(fd < 0)
		return -1;
	used = 0;
	while(used < size-1){
		n = read(fd, buf+used, size-1-used);
		if(n < 0){
			close(fd);
			return -1;
		}
		if(n == 0)
			break;
		used += n;
	}
	if(used == size-1 && read(fd, &extra, 1) != 0){
		close(fd);
		return -1;
	}
	close(fd);
	while(used > 0 && (buf[used-1] == 0 || buf[used-1] == '\n' || buf[used-1] == '\r'))
		used--;
	for(i = 0; i < used; i++)
		if(buf[i] == 0)
			return -1;
	buf[used] = 0;
	return used;
}

static int
decimal(char *s)
{
	int i;

	for(i = 0; s[i] != 0; i++)
		if(i >= 6 || s[i] < '0' || s[i] > '9')
			return 0;
	return i != 0;
}

static int
prefix(uchar *mask, int v4)
{
	int i, bit, count, zero;

	if(v4)
		for(i = 0; i < IPv4off; i++)
			if(mask[i] != 255)
				return -1;
	count = zero = 0;
	for(i = v4 ? IPv4off : 0; i < IPaddrlen; i++)
		for(bit = 7; bit >= 0; bit--){
			if((mask[i] & (1 << bit)) != 0){
				if(zero)
					return -1;
				count++;
			}else
				zero = 1;
		}
	return count;
}

typedef struct Network Network;
struct Network {
	char status[StatusMax];
	char *line[AddressMax+3];
	char *field[64];
};

static void
hardware(Jbuf *b, char *device)
{
	char path[128], raw[32], mac[18];
	int i, j, n, c;

	/* A native status file must not redirect this read to another sensitive file. */
	if(strncmp(device, "/net/ether", 10) != 0 || !decimal(device+10))
		return;
	snprint(path, sizeof path, "%s/addr", device);
	n = readtext(path, raw, sizeof raw);
	if(n != 12)
		return;
	for(i = j = 0; i < 12; i++){
		c = raw[i];
		if(c >= 'A' && c <= 'F')
			c += 'a' - 'A';
		if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return;
		if(i > 0 && i%2 == 0)
			mac[j++] = ':';
		mac[j++] = c;
	}
	mac[j] = 0;
	jbtext(b, ",\"hardware-address\":");
	jbquote(b, mac);
}

/* One modern stock /net/ipifc/N/status, with explicit allocation/record limits. */
static int
interface(Jbuf *b, Network *net, char *index, int *first)
{
	char path[128], name[32], text[64], *device;
	uchar ip[IPaddrlen], mask[IPaddrlen];
	int n, lines, i, v4, bits, addressfirst;

	snprint(path, sizeof path, "/net/ipifc/%s/status", index);
	if(readtext(path, net->status, sizeof net->status) < 0)
		return -1;
	lines = getfields(net->status, net->line, nelem(net->line), 1, "\n");
	if(lines < 1 || lines > AddressMax+1)
		return -1;
	n = tokenize(net->line[0], net->field, nelem(net->field));
	if(n < 2 || strcmp(net->field[0], "device") != 0)
		return -1;
	device = net->field[1];
	/* An unbound conversation is not a network interface. */
	if(strcmp(device, "maxtu") == 0)
		return 0;
	if(!*first)
		jbchar(b, ',');
	*first = 0;
	snprint(name, sizeof name, "ipifc%s", index);
	jbtext(b, "{\"name\":");
	jbquote(b, name);
	hardware(b, device);
	jbtext(b, ",\"ip-addresses\":[");
	addressfirst = 1;
	for(i = 1; i < lines; i++){
		n = tokenize(net->line[i], net->field, nelem(net->field));
		if(n < 5 || parseip(ip, net->field[0]) < 0)
			return -1;
		v4 = isv4(ip);
		if(parseipmask(mask, net->field[1], v4) < 0 || (bits = prefix(mask, v4)) < 0)
			return -1;
		if(!addressfirst)
			jbchar(b, ',');
		addressfirst = 0;
		jbtext(b, "{\"ip-address\":");
		snprint(text, sizeof text, "%I", ip);
		jbquote(b, text);
		jbtext(b, ",\"ip-address-type\":");
		jbquote(b, v4 ? "ipv4" : "ipv6");
		snprint(text, sizeof text, ",\"prefix\":%d}", bits);
		jbtext(b, text);
	}
	jbtext(b, "]}");
	return b->failed ? -1 : 0;
}

static int
interfaces(Jbuf *b)
{
	Network *net;
	Dir *entries;
	int fd, n, i, count, first, result;

	net = malloc(sizeof *net);
	if(net == nil)
		return -1;
	fd = open("/net/ipifc", OREAD|OCEXEC);
	if(fd < 0){
		free(net);
		return -1;
	}
	count = 0;
	first = 1;
	result = 0;
	jbchar(b, '[');
	while((n = dirread(fd, &entries)) > 0){
		for(i = 0; i < n; i++){
			if(!decimal(entries[i].name))
				continue;
			if(++count > InterfaceMax || interface(b, net, entries[i].name, &first) < 0){
				result = -1;
				break;
			}
		}
		free(entries);
		if(result < 0)
			break;
	}
	if(n < 0)
		result = -1;
	close(fd);
	free(net);
	jbchar(b, ']');
	return b->failed ? -1 : result;
}

static int
formatip(const char *input, char *output, int size)
{
	uchar ip[IPaddrlen];
	int n;

	if(parseip(ip, (char*)input) < 0)
		return -1;
	n = snprint(output, size, "%I", ip);
	return n <= 0 || n >= size ? -1 : isv4(ip) ? 4 : 6;
}

static int
nativefacts(int command, Jbuf *b)
{
	char *text;
	int n, result;

	/* The maximum route/status snapshot is too large for a native process stack. */
	text = malloc(QgaNativeTextMax+1);
	if(text == nil)
		return -1;
	n = readtext(command == QgaVcpus ? "#c/sysstat" : "/net/iproute",
		text, QgaNativeTextMax+1);
	result = -1;
	if(n >= 0){
		if(command == QgaVcpus)
			result = qgacpus(b, text, n);
		else if(fmtinstall('I', eipfmt) >= 0)
			result = qgaroutes(b, text, n, formatip);
	}
	free(text);
	return result;
}

static int
timezone(Jbuf *b)
{
	Tzone *zone;
	Tm tm;
	char *end;

	/* tzload caches the fixed name; Tm is caller-owned, not localtime's static. */
	zone = tzload("local");
	if(zone == nil || tmnow(&tm, zone) == nil)
		return -1;
	end = memchr(tm.zone, 0, sizeof tm.zone);
	if(end == nil)
		return -1;
	return qgatimezone(b, tm.zone, end-tm.zone, tm.tzoff);
}

int
nativequery(int command, Jbuf *b)
{
	char text[256];
	int n;
	vlong now;

	switch(command){
	case QgaOsinfo:
		n = readtext("/env/cputype", text, sizeof text);
		if(n <= 0)
			return -1;
		jbtext(b, "{\"id\":\"9front\",\"name\":\"9front\",\"pretty-name\":\"9front\",\"machine\":");
		jbstring(b, text, n);
		jbchar(b, '}');
		break;
	case QgaHostname:
		n = readtext("#c/sysname", text, sizeof text);
		if(n <= 0)
			return -1;
		jbtext(b, "{\"host-name\":");
		jbstring(b, text, n);
		jbchar(b, '}');
		break;
	case QgaTime:
		now = nsec();
		if(now < 0)
			return -1;
		snprint(text, sizeof text, "%lld", now);
		jbtext(b, text);
		break;
	case QgaInterfaces:
		if(fmtinstall('I', eipfmt) < 0)
			return -1;
		return interfaces(b);
	case QgaVcpus:
	case QgaRoutes:
		return nativefacts(command, b);
	case QgaTimezone:
		return timezone(b);
	case QgaDisks:
		return nativedisks(b);
	default:
		return -1;
	}
	return b->failed ? -1 : 0;
}

/* Keep redirection sources above stdio even if the caller closed fd 0/1/2. */
static int
highfd(int fd)
{
	int low[3], count, copy, i;

	if(fd < 0 || fd > 2)
		return fd;
	low[0] = fd;
	count = 1;
	for(;;){
		copy = dup(fd, -1);
		if(copy < 0 || copy > 2)
			break;
		/* Each dup occupies another free low fd; there are only three. */
		if(count == nelem(low)){
			close(copy);
			copy = -1;
			break;
		}
		low[count++] = copy;
	}
	for(i = 0; i < count; i++)
		close(low[i]);
	return copy;
}

int
nativeshutdown(void *context, int mode)
{
	ShutdownContext *c;
	int pid, nullfd, consfd, keep[3];
	char *argv[4];

	c = context;
	if(mode != QgaPowerdown && mode != QgaReboot)
		return -1;
	if(access("/bin/fshalt", AREAD) < 0 || access("/bin/rc", AEXEC) < 0)
		return -1;
	nullfd = highfd(open("/dev/null", ORDWR|OCEXEC));
	consfd = highfd(open("#c/cons", OWRITE|OCEXEC));
	if(nullfd < 0 || consfd < 0){
		if(nullfd >= 0)
			close(nullfd);
		if(consfd >= 0)
			close(consfd);
		return -1;
	}
	/* Private memory/fds/note group, no waiter or host-session dependency. */
	pid = rfork(RFPROC|RFFDG|RFNAMEG|RFENVG|RFNOTEG|RFNOWAIT);
	if(pid == 0){
		close(c->datafd);
		if(c->ctlfd >= 0)
			close(c->ctlfd);
		if(dup(nullfd, 0) < 0 || dup(consfd, 1) < 0 || dup(consfd, 2) < 0)
			_exits("redirect fshalt");
		close(nullfd);
		close(consfd);
		keep[0] = 0;
		keep[1] = 1;
		keep[2] = 2;
		if(nativecloseexcept(keep, 3) < 0)
			_exits("shutdown descriptor cleanup");
		argv[0] = "rc";
		argv[1] = "/bin/fshalt";
		argv[2] = mode == QgaReboot ? "-r" : nil;
		argv[3] = nil;
		exec("/bin/rc", argv);
		fprint(2, "qga9: cannot execute fshalt: %r\n");
		_exits("exec fshalt");
	}
	close(nullfd);
	close(consfd);
	return pid < 0 ? -1 : 0;
}
