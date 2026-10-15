/* SPDX-License-Identifier: MIT */
/* Read-only live inventory checks; stat nodes, never open partition data/raw. */
#include "../src/native.h"
#include "../src/storage.h"

static int checks;

static void
check(int ok, char *message)
{
	checks++;
	if(!ok)
		sysfatal("FAIL: %s", message);
}

void
main(void)
{
	Jbuf b;
	Jdoc *d;
	Dir *stat;
	char *out;
	char names[QgaDiskRecordMax][96], parent[96];
	int i, j, entry, token, dependencies, count;

	d = malloc(sizeof *d);
	out = malloc(QgaOutputMax);
	check(d != nil && out != nil, "allocation");
	jbinit(&b, out, QgaOutputMax);
	check(nativedisks(&b) == 0 && !b.failed, "live kernel disk inventory");
	check(jparseresponse(d, out, b.used) && d->token[0].type == Jarray, "inventory JSON");
	count = d->token[0].count;
	check(count >= 0 && count <= QgaDiskRecordMax, "inventory record bound");
	for(i = 0, entry = 1; i < count; i++, entry = d->token[entry].next){
		check(d->token[entry].type == Jobject, "disk object");
		token = jmember(d, entry, "name");
		check(jgetstring(d, token, names[i], sizeof names[i]) > 0 &&
			strncmp(names[i], "#S/sd", 5) == 0, "direct native node path");
		for(j = 0; j < i; j++)
			check(strcmp(names[j], names[i]) != 0, "unique node");
		stat = dirstat(names[i]);
		check(stat != nil, "node exists");
		check(stat->type == 'S' && stat->qid.type == QTFILE &&
			(stat->qid.path & 15) == 6 && stat->length > 0, "nonempty kernel partition node, not raw/ctl");
		free(stat);
		token = jmember(d, entry, "partition");
		check(token >= 0 && (d->token[token].type == Jtrue || d->token[token].type == Jfalse), "partition boolean");
		check(jmember(d, entry, "address") < 0 && jmember(d, entry, "smart") < 0, "unverified hardware omitted");
	}
	for(i = 0, entry = 1; i < count; i++, entry = d->token[entry].next){
		dependencies = jmember(d, entry, "dependencies");
		if(dependencies < 0)
			continue;
		check(d->token[dependencies].type == Jarray && d->token[dependencies].count == 1 &&
			jgetstring(d, dependencies+1, parent, sizeof parent) > 0, "single verified parent");
		for(j = 0; j < count; j++)
			if(strcmp(names[j], parent) == 0)
				break;
		check(j < count && j != i, "dependency appears in inventory");
	}
	free(out);
	free(d);
	print("native storage: %d read-only checks passed (%d disk/partition nodes)\n", checks, count);
	exits(nil);
}
