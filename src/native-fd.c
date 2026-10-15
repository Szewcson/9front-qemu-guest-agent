/* SPDX-License-Identifier: MIT */
#include "native.h"

/* Only in a process with its own descriptor table, before concurrent workers.
 * #d offsets are fd numbers, so closing entries does not skip later descriptors. */
int
nativecloseexcept(int *keep, int count)
{
	Dir *d;
	char *end;
	int fd, n, i, j;
	vlong target;

	if(count < 0 || count > 8 || (count > 0 && keep == nil))
		return -1;
	fd = open("#d", OREAD|OCEXEC);
	if(fd < 0)
		return -1;
	while((n = dirread(fd, &d)) > 0){
		for(i = 0; i < n; i++){
			target = strtoll(d[i].name, &end, 10);
			if(*end != 0 || target < 0 || target > 2147483647 || target == fd)
				continue;
			for(j = 0; j < count; j++)
				if(keep[j] == target)
					break;
			if(j == count)
				close(target);
		}
		free(d);
	}
	close(fd);
	return n < 0 ? -1 : 0;
}
