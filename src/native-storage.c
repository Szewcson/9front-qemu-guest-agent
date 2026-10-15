/* SPDX-License-Identifier: MIT */
#include "native.h"
#include "storage.h"

/* devsd.c QID type bits: kernel partitions only, not raw/ctl/extra files. */
enum { SdTypeMask = 15, SdQpart = 6, SdEntryMax = 256 };

typedef struct Snapshot Snapshot;
struct Snapshot {
	QgaDiskUnit unit;
	QgaDiskUnit afterunit;
	char before[QgaDiskTextMax+1];
	char after[QgaDiskTextMax+1];
	int seen[QgaDiskPartMax];
	QgaUint64 length[QgaDiskPartMax];
};

static int
readctl(char *path, char *text)
{
	int fd, n, used;
	char extra;

	fd = open(path, OREAD|OCEXEC);
	if(fd < 0)
		return -1;
	used = 0;
	while(used < QgaDiskTextMax){
		n = read(fd, text+used, QgaDiskTextMax-used);
		if(n < 0){
			close(fd);
			return -1;
		}
		if(n == 0)
			break;
		used += n;
	}
	if(used == QgaDiskTextMax && read(fd, &extra, 1) != 0){
		close(fd);
		return -1;
	}
	close(fd);
	text[used] = 0;
	return used;
}

/* Cross-check every ctl partition against kernel directory metadata, no data I/O. */
static int
parts(Snapshot *s, char *path)
{
	Dir *entries;
	int fd, n, i, j, count, result;

	fd = open(path, OREAD|OCEXEC);
	if(fd < 0)
		return -1;
	count = 0;
	result = 0;
	memset(s->seen, 0, sizeof s->seen);
	while((n = dirread(fd, &entries)) > 0){
		for(i = 0; i < n; i++){
			if(++count > SdEntryMax){
				result = -1;
				break;
			}
			if((entries[i].qid.path & SdTypeMask) != SdQpart)
				continue;
			for(j = 0; j < s->unit.npart; j++)
				if(strcmp(entries[i].name, s->unit.part[j].name) == 0)
					break;
			if(j == s->unit.npart || s->seen[j] || entries[i].type != 'S' ||
			   entries[i].qid.type != QTFILE){
				result = -1;
				break;
			}
			if(entries[i].length < 0){
				result = -1;
				break;
			}
			s->length[j] = entries[i].length;
			s->seen[j] = 1;
		}
		free(entries);
		if(result < 0)
			break;
	}
	if(n < 0)
		result = -1;
	close(fd);
	for(j = 0; j < s->unit.npart; j++)
		if(!s->seen[j])
			result = -1;
	return result < 0 ? -1 : qgadisklengths(&s->unit, s->length);
}

static int
unit(Snapshot *s, char *name, Jbuf *b, int *count)
{
	char path[96], ctl[100];
	int n, m;

	if(!qgadiskunitname(name))
		return -1;
	snprint(path, sizeof path, "#S/%s", name);
	snprint(ctl, sizeof ctl, "%s/ctl", path);
	n = readctl(ctl, s->before);
	if(n < 0) return -1;
	if(qgadiskparse(&s->unit, s->before, n) < 0){
		werrstr("invalid %s ctl inventory", name); return -1;
	}
	if(parts(s, path) < 0){
		werrstr("inconsistent %s partition metadata", name); return -1;
	}
	/* Native IDE ctl includes live IRQ/error counters. Compare the parsed
	 * inventory, not diagnostic text that can change during unrelated I/O.
	 * Revalidate the second table against the same observed byte lengths. */
	m = readctl(ctl, s->after);
	if(m < 0 || qgadiskparse(&s->afterunit, s->after, m) < 0 ||
	   qgadisklengths(&s->afterunit, s->length) < 0 ||
	   memcmp(&s->unit, &s->afterunit, sizeof s->unit) != 0){
		werrstr("%s partition inventory changed", name);
		return -1;
	}
	return qgadiskemit(b, name, &s->unit, count);
}

int
nativedisks(Jbuf *b)
{
	Snapshot *s;
	Dir *entries;
	char names[QgaDiskUnitMax][16];
	int fd, n, i, j, units, count, scanned, result;

	s = malloc(sizeof *s);
	if(s == nil)
		return -1;
	fd = open("#S", OREAD|OCEXEC);
	if(fd < 0){
		free(s);
		return -1;
	}
	units = count = scanned = 0;
	result = 0;
	jbchar(b, '[');
	while((n = dirread(fd, &entries)) > 0){
		for(i = 0; i < n; i++){
			if(++scanned > SdEntryMax){
				result = -1;
				break;
			}
			if(!(entries[i].mode & DMDIR))
				continue;
			if(units == QgaDiskUnitMax || !qgadiskunitname(entries[i].name)){
				result = -1;
				break;
			}
			for(j = 0; j < units; j++)
				if(strcmp(names[j], entries[i].name) == 0)
					break;
			if(j != units || unit(s, entries[i].name, b, &count) < 0){
				result = -1;
				break;
			}
			/* Validated names are <= 6 bytes; snapshot never retains Dir pointers. */
			memmove(names[units++], entries[i].name, strlen(entries[i].name)+1);
		}
		free(entries);
		if(result < 0)
			break;
	}
	if(n < 0)
		result = -1;
	close(fd);
	free(s);
	jbchar(b, ']');
	return b->failed ? -1 : result;
}
