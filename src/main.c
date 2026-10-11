/* SPDX-License-Identifier: MIT */
/* Native stock 9front entry point. There is intentionally no POSIX port. */
#include "native.h"

typedef struct Channel Channel;
struct Channel { int fd; };

static long
channelwrite(void *context, const uchar *s, long n)
{
	Channel *c;

	c = context;
	return write(c->fd, (void*)s, n);
}

static void
usage(void)
{
	fprint(2, "usage: qga9 -d device [-t uart|stream] [-c controlfile] [-b baud] [-S] [-D] | qga9 -s\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	Qga *q;
	QgaFiles *files;
	QgaProcesses *processes;
	Channel c;
	ShutdownContext shutdowncontext;
	char *device, *control, *baud, *transport, *ownedcontrol, settings[96];
	uchar input[256];
	int stdio, ctl, n, expected, infd, enableshutdown, detached, pid, baudgiven;
	char *status;

	device = control = ownedcontrol = nil;
	baud = "38400";
	transport = "uart";
	stdio = enableshutdown = detached = baudgiven = 0;
	ARGBEGIN {
	case 'd': device = EARGF(usage()); break;
	case 'c': control = EARGF(usage()); break;
	case 'b': baud = EARGF(usage()); baudgiven = 1; break;
	case 't': transport = EARGF(usage()); break;
	case 's': stdio = 1; break;
	case 'S': enableshutdown = 1; break;
	case 'D': detached = 1; break;
	default: usage();
	} ARGEND
	if(argc != 0 || (stdio && (device != nil || control != nil || enableshutdown || detached)) || (!stdio && device == nil))
		usage();
	if(strcmp(transport, "uart") != 0 && strcmp(transport, "stream") != 0)
		usage();
	if(strcmp(transport, "stream") == 0 && (stdio || control != nil || baudgiven))
		usage();
	if(strcmp(baud, "9600") != 0 && strcmp(baud, "19200") != 0 &&
	   strcmp(baud, "38400") != 0 && strcmp(baud, "57600") != 0 && strcmp(baud, "115200") != 0)
		usage();
	ctl = -1;
	c.fd = stdio ? 1 : open(device, ORDWR);
	if(c.fd < 0)
		sysfatal("open %s: %r", device);
	q = qganew(channelwrite, &c);
	if(q == nil){
		if(!stdio)
			close(c.fd);
		sysfatal("allocate protocol state: %r");
	}
	status = nil;
	files = nil;
	processes = nil;
	if(!stdio && strcmp(transport, "uart") == 0){
		if(control == nil){
			ownedcontrol = smprint("%sctl", device);
			if(ownedcontrol == nil){
				status = "no memory for control path";
				goto out;
			}
			control = ownedcontrol;
		}
		ctl = open(control, OWRITE);
		if(ctl < 0){
			fprint(2, "qga9: open %s: %r\n", control);
			status = "open UART control";
			goto out;
		}
		/* Fixed native UART commands; disable both kinds of flow control. */
		expected = snprint(settings, sizeof settings, "b%s l8 pn s1 m0 x0 c0 e0 d1 r1", baud);
		if(expected <= 0 || expected >= sizeof settings || write(ctl, settings, expected) != expected){
			fprint(2, "qga9: configure %s: %r\n", control);
			status = "configure UART";
			goto out;
		}
	}
	if(detached){
		/* Fork only after validation; private state/fds and no parent wait job. */
		pid = rfork(RFPROC|RFFDG|RFNAMEG|RFENVG|RFNOTEG|RFNOWAIT);
		if(pid < 0){
			fprint(2, "qga9: detach: %r\n");
			status = "detach";
			goto out;
		}
		if(pid > 0)
			goto out;
	}
	qgaqueries(q, nativequery);
	/* Created in the serving process, after detachment, with sole ownership. */
	files = nativefiles();
	if(files == nil){
		status = "allocate native file state";
		goto out;
	}
	qgafileops(q, qgafilesop, files);
	/* Clock mutation is management-only, never part of stdio diagnostics. */
	if(!stdio){
		qgasettime(q, nativesettime, nil);
		processes = nativeprocesses();
		if(processes == nil){ status = "allocate native process state"; goto out; }
		qgaexecops(q, qgaprocessop, processes);
	}
	if(enableshutdown){
		shutdowncontext.datafd = c.fd;
		shutdowncontext.ctlfd = ctl;
		qgashutdown(q, nativeshutdown, &shutdowncontext);
	}
	infd = stdio ? 0 : c.fd;
	for(;;){
		n = read(infd, input, sizeof input);
		if(n <= 0){
			if(stdio && n == 0){
				if(qgaend(q) < 0)
					status = "write response";
			}else{
				fprint(2, "qga9: channel closed or read failed: %r\n");
				status = "channel read";
			}
			break;                 /* No automatic retry/busy loop. */
		}
		if(qgafeed(q, input, n) < 0){
			fprint(2, "qga9: write response failed: %r\n");
			status = "write response";
			break;
		}
	}
out:
	if(ctl >= 0)
		close(ctl);
	if(!stdio)
		close(c.fd);
	free(ownedcontrol);
	qgafree(q);
	qgafilesfree(files);
	nativeprocessfree(processes);
	exits(status);
}
