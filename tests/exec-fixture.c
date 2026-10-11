/* SPDX-License-Identifier: MIT */
/* Synthetic native guest executable; never reads operator files. */
#include <u.h>
#include <libc.h>
void
main(int argc, char **argv)
{
	uchar data[4096];
	char *env;
	Dir *d;
	int i, n, fd, pid;
	if(argc < 2) exits("mode");
	if(strcmp(argv[1], "literal") == 0){
		env = getenv("QGA9_TEST");
		if(argc != 3 || strcmp(argv[2], "$(no); 'literal'") != 0 ||
		   env == nil || strcmp(env, "literal;value") != 0) exits("literal mismatch");
		free(env);
		while((n = read(0, data, sizeof data)) > 0)
			if(write(1, data, n) != n) exits("write");
		if(n < 0) exits("read");
		write(2, "err\n", 4);
	}else if(strcmp(argv[1], "flood") == 0){
		memset(data, 'x', sizeof data);
		for(i = 0; i < 128; i++){
			if(write(1, data, sizeof data) != sizeof data ||
			   write(2, data, sizeof data) != sizeof data) exits("flood write");
		}
	}else if(strcmp(argv[1], "fds") == 0){
		fd = open("#d", OREAD);
		if(fd < 0) exits("fd directory");
		while((n = dirread(fd, &d)) > 0){
			for(i = 0; i < n; i++)
				if(strchr(d[i].name, 'c') == nil && atoi(d[i].name) > 2 && atoi(d[i].name) != fd)
					exits("inherited descriptor");
			free(d);
		}
		close(fd);
		if(n < 0) exits("fd enumeration");
	}else if(strcmp(argv[1], "sleep") == 0) sleep(5000);
	else if(strcmp(argv[1], "background") == 0){
		pid = rfork(RFPROC|RFFDG|RFNOWAIT);
		if(pid < 0) exits("fork");
		if(pid == 0){ sleep(2000); exits(nil); }
	}else if(strcmp(argv[1], "fail") == 0) exits("fixture failure");
	else exits("unknown mode");
	exits(nil);
}
