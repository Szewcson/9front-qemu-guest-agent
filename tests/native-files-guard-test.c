/* SPDX-License-Identifier: MIT */
/* Explicit native fixture: private environment, two denied device paths and
 * one exclusively created synthetic /tmp file. No existing files are changed. */
#include "../src/native-files.c"

static int checks;
static void check(int ok, char *why) { checks++; if(!ok) sysfatal("%s: %r", why); }
void main(void)
{
	char path[QgaFilePathMax], uniondir[128], child[128], env[128], data[3];
	QgaFiles *f;
	Dir *d;
	int fd, i;
	char *dirs[] = {"/env", "/srv"};
	check(rfork(RFENVG|RFNAMEG) == 0, "private fixture groups");
	f = nativefiles();
	check(f != nil, "private backend namespace");
	for(i=0; i<nelem(dirs); i++){
		snprint(path, sizeof path, "%s/qga9-denied-%d", dirs[i], getpid());
		d = dirstat(path); check(d == nil, "device fixture must not exist"); free(d);
		check(fileopen(nil, path, QgaOpenWrite|QgaOpenTruncate) < 0, "device create denied");
		d = dirstat(path); check(d == nil, "rejection leaves no device state"); free(d);
	}
	snprint(path, sizeof path, "/tmp/qga9-guard-%d", getpid());
	d = dirstat(path); check(d == nil, "file fixture must not exist"); free(d);
	fd = fileopen(nil, path, QgaOpenRead|QgaOpenWrite|QgaOpenTruncate);
	check(fd >= 0, "mounted file create");
	check(write(fd, "abc", 3) == 3 && seek(fd, 0, 0) == 0 &&
		read(fd, data, 3) == 3 && memcmp(data, "abc", 3) == 0, "file round-trip");
	check(close(fd) == 0 && remove(path) == 0, "remove owned synthetic file");
	/* A readable M first branch with a creatable env second branch was the
	 * preflight gap. The new pinned-parent helper must reject this union. */
	snprint(uniondir,sizeof uniondir,"/tmp/qga9-union-%d",getpid());
	d=dirstat(uniondir); check(d==nil,"union fixture absent"); free(d);
	fd=create(uniondir,OREAD|OEXCL,DMDIR|0700); check(fd>=0,"owned union directory"); close(fd);
	snprint(child,sizeof child,"qga9-union-child-%d",getpid());
	snprint(env,sizeof env,"#e/%s",child);
	d=dirstat(env); check(d==nil,"environment fixture absent"); free(d);
	check(bind("#e",uniondir,MAFTER|MCREATE)==0,"private mixed-device union");
	snprint(path,sizeof path,"%s/%s",uniondir,child);
	check(fileopen(nil,path,QgaOpenWrite|QgaOpenTruncate)<0,"mixed-device union create denied");
	d=dirstat(env); check(d==nil,"no environment side effect"); free(d);
	check(unmount(nil,uniondir)==0,"remove private union mount");
	d=dirstat(path); check(d==nil,"no mounted-file side effect"); free(d);
	check(remove(uniondir)==0,"remove owned empty directory");
	qgafilesfree(f);
	print("native file guard: %d checks passed\n", checks);
	exits(nil);
}
