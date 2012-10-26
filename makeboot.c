/*
 * very very simple makefile parser
 *
 * Copyright (c) 2003 SuSE Linux AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#if defined _XOPEN_SOURCE && (_XOPEN_SOURCE - 0) >= 600
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#ifndef POSIX_FADV_SEQUENTIAL
#define posix_fadvise(fd, off, len, adv)	(-1)
#endif
#ifndef O_DIRECT
#define O_DIRECT 0
#endif
static int o_flags = O_RDONLY;
#endif
#ifdef USE_BLOGD
# include <libblogger.h>
#endif
#include "makeboot.h"


int tree_entries = 0;
struct makenode *tree_list = NULL;

/*
 * search for the node with the given name
 * returns the node pointer or NULL if not found.
 *
 * FIXME: we should use hash for the effective search.
 */
static struct makenode *lookup_target(const char *name)
{
	struct makenode *t;

	for (t = tree_list; t; t = t->next)
		if (! strcmp(t->name, name))
			return t;
	return NULL;
}

/*
 * look for the node with the given name.  if not exist,
 * create a new one and append to the node list.
 */
static struct makenode *add_target(const char *name)
{
	struct makenode *__restrict node;
	struct makenode *prev, *t;

	node = lookup_target(name);
	if (node)
		return node;
	if (posix_memalign((void*)&node, sizeof(void*), alignof(struct makenode)+strsize(name)) < 0) {
		fprintf(stderr, "Can't malloc: %s\n", strerror(errno));
		exit(1);
	}
	memset(node, 0, alignof(struct makenode)+strsize(name));
	node->name = ((char*)node)+alignof(struct makenode);
	strcpy(node->name, name);

	/* append to the list in alphabetical order */
	prev = NULL;
	for (t = tree_list; t; prev = t, t = t->next)
		if (strcmp(node->name, t->name) < 0)
			break;
	if (prev)
		prev->next = node;
	else
		tree_list = node;
	node->next = t;
	tree_entries++;
	return node;
}

/*
 * Set and propagate importance of a node to all depencies of this node
 */
static void add_importance(struct makenode *node, int importance)
{
	struct makelist *s = node->depend;

	node->importance += importance;
	for (s = node->depend; s; s = s->next)
		add_importance(s->node, importance);
}

/*
 * create a dependecy/selection node
 */
static struct makelist *new_list(struct makenode *node, struct makelist *next)
{
	struct makelist *x;

	x = xcalloc(1, sizeof(*x));
	x->node = node;
	x->next = next;
	return x;
}

/*
 * check whether the given target would create an infinte loop
 */
static int loop;
static int check_loop(struct makenode *dep, struct makenode *src)
{
	struct makelist *s;
	for (s = dep->depend; s; s = s->next) {
		if (s->node == src) {
			fprintf(stderr, "loop exists %s in %s!\n", dep->name, src->name);
			return 1;
		}
		if (loop++ > 99999) {
			fprintf(stderr, "too many loops! (loop=%d, dep->name=%s, src->name=%s)\n",
				loop, dep->name, src->name);
			return 1;
		}
		if (check_loop(s->node, src))
			return 1;
	}
	return 0;
}

/*
 * add to the dependecy and selection lists
 */
static void add_depend(struct makenode *node, const char *dst)
{
	struct makenode *dep;

	dep = add_target(dst);
	loop = 0;
	if (check_loop(dep, node))
		return;
	dep->select = new_list(node, dep->select);
	dep->num_sels++;
	node->depend = new_list(dep, node->depend);
	node->num_deps++;
}

/*
 * mark the selected service as an interactive task
 * that should run solely
 */
static void mark_interactive(const char *name)
{
	struct makenode *node = lookup_target(name);
	if (node)
		node->interactive = 1;
}


#define DELIMITER	" \t\r\n"

/*
 * parse (pseudo) makefile
 *
 * it may have only the following form:
 *
 * TARGETS = xxx ...
 * INTERACTIVE = yyy ...
 * aaa:
 * bbb: xxx ddd ...
 *
 * other lines are ignored.
 */
void parse_makefile(const char *path)
{
	FILE *fp;
	char buf[LINE_MAX]; /* FIXME: is this enough big? */
	char *s, *strp, *p;
	struct makenode *node;

#if defined _XOPEN_SOURCE && (_XOPEN_SOURCE - 0) >= 600
	int fd;

	if (getuid() == (uid_t)0)
		o_flags |= O_NOATIME;
	if ((fd = open(path, o_flags)) < 0) {
		fprintf(stderr, "Can't open %s: %s\n", path, strerror(errno));
		exit(1);
	}
	(void)posix_fadvise(fd, 0, 0, POSIX_FADV_WILLNEED);
	(void)posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	(void)posix_fadvise(fd, 0, 0, POSIX_FADV_NOREUSE);

	if ((fp = fdopen(fd, "r")) == NULL)
#else
	if ((fp = fopen(path, "r")) == NULL)
#endif
	{
		fprintf(stderr, "Can't open %s: %s\n", path, strerror(errno));
		exit(1);
	}

	while (fgets(buf, sizeof(buf), fp)) {
		for (s = buf; *s && isspace(*s); s++)
			;
		if (! *s || *s == '#')
			continue;
		if (! strncmp(s, "TARGETS =", 9)) {
			s += 9;
			strp = s;
			while ((s = strsep(&strp, DELIMITER))) {
				if (! *s)
					continue;
				add_target(s);
			}
		} else if (! strncmp(s, "INTERACTIVE =", 13)) {
			s += 13;
			strp = s;
			while ((s = strsep(&strp, DELIMITER))) {
				if (! *s)
					continue;
				mark_interactive(s);
			}
		} else {
			p = strchr(s, ':');
			if (! p)
				continue;
			*p = 0;
			node = add_target(s);
			strp = p + 1;
			while ((s = strsep(&strp, DELIMITER))) {
				if (! *s)
					continue;
				add_depend(node, s);
			}
		}
	}

#if defined _XOPEN_SOURCE && (_XOPEN_SOURCE - 0) >= 600
	(void)posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
#endif

	fclose(fp);

	for (node = tree_list; node; node = node->next) {
		int importance = 0;

		if (! strcmp(node->name, "xdm")
		    || ! strncmp(node->name, "gdm", 3)
		    || ! strncmp(node->name, "kdm", 3)
		    || ! strcmp(node->name, "boot.udev")
		    || ! strcmp(node->name, "udev"))
			importance = 100;

		if (! strcmp(node->name, "sshd"))
			importance = 2000;

		if (! strncmp(node->name, "early", 5))
			importance = 8000;

		if (importance)
			add_importance(node, importance);
	}
}

/*
 * filter out the list targets
 */

static int filter_prefix;
static int dirfilter(const struct dirent *d)
{
	return *d->d_name == filter_prefix &&
		strlen(d->d_name) >= 4; /* to be sure */
}

static void filter_files(const char *dir, int prefix, int inverse)
{
	char path[64];
	int i, ndirs;
	static struct dirent **dirlist;
	struct makenode *t, *next;

	filter_prefix = prefix;
#ifdef SUSE	/* SuSE */
	snprintf(path, sizeof(path), "/etc/init.d/%s.d", dir);
#else		/* Debian */
	snprintf(path, sizeof(path), "/etc/%s.d", dir);
#endif
#if defined _XOPEN_SOURCE && (_XOPEN_SOURCE - 0) >= 600
	if ((i = open(path, o_flags|O_DIRECTORY|O_LARGEFILE)) >= 0) {
		(void)posix_fadvise(i, 0, 0, POSIX_FADV_SEQUENTIAL);
		(void)posix_fadvise(i, 0, 0, POSIX_FADV_NOREUSE);
	}
#endif
	ndirs = scandir(path, &dirlist, dirfilter, alphasort);
#if defined _XOPEN_SOURCE && (_XOPEN_SOURCE - 0) >= 600
	if (i >= 0) {
		(void)posix_fadvise(i, 0, 0, POSIX_FADV_DONTNEED);
		close(i);
	}
#endif
	/* mark all matching nodes */
	if (ndirs >= 0) {
		for (i = 0; i < ndirs; i++) {
			t = lookup_target(dirlist[i]->d_name + 3);
			if (t) {
				t->status = 1;
				t->filter_prefix = filter_prefix;
				if (asprintf(&t->arg0, "%s/%s", path, dirlist[i]->d_name) < 0)
					t->arg0 = (char*)0;
			}
			free(dirlist[i]);
		}
		free(dirlist);
	}
	/* deselect non-matching nodes */
	for (t = tree_list; t; t = next) {
		next = t->next;
		if ((! t->status && ! inverse) || (t->status && inverse)) {
			/* remove from the list */
			struct makelist *x, *nx;
			struct makenode *p;
			for (x = t->select; x; x = nx) {
				nx = x->next;
				x->node->num_deps--;
				free(x);
			}
			for (x = t->depend; x; x = nx) {
				nx = x->next;
				x->node->num_sels--;
				free(x);
			}
			if (t == tree_list)
				tree_list = next;
			else {
				for (p = tree_list; p->next != t; p = p->next)
					;
				p->next = next;
			}
			/* don't free the node instance itself - it may be selected
			 * by others
			 */
			tree_entries--;
			continue;
		}
		t->status = 0;
	}
}

/*
 * mark the unnecessary services as finished.
 *
 * action is either boot, start or stop.
 * prev and run are the previous and the next runlevel.
 */
void check_run_files(const char *action, const char *prev, const char *run)
{
	char buf[4] = "rc0";
	if (! strcmp(action, "boot")) {
#ifdef SUSE	/* SuSE */
		filter_files("boot", 'S', 0);
	} else if (! strcmp(action, "halt")) {
		filter_files("boot", 'K', 0);
	} else if (! strcmp(action, "start")) {
		buf[2] = *prev;
		filter_files(buf, 'K', 1);
		buf[2] = *run;
		filter_files(buf, 'S', 0);
	} else {
		buf[2] = *prev;
		filter_files(buf, 'K', 0);
		buf[2] = *run;
		filter_files(buf, 'S', 1);
#else		/* Debian */
		filter_files("rcS", 'S', 0);
	} else if (! strcmp(action, "start")) {
		buf[2] = *prev;
		filter_files(buf, 'S', 1);
		buf[2] = *run;
		filter_files(buf, 'S', 0);
	} else {
		buf[2] = *prev;
		filter_files(buf, 'K', 1);
		buf[2] = *run;
		filter_files(buf, 'K', 0);
#endif
	}
}


/*
 * call blogd
 */
#ifndef USE_BLOGD
# define bootlog(arg...)
# define closeblog()
#endif

/*
 * pick up the next running task
 * return NULL if not found.
 */
struct makenode *pickup_task(void)
{
	struct makenode *node, *best = (struct makenode*)0;

	for (node = tree_list; node; node = node->next) {
		if (node->status != T_READY)
			continue;
		if (node->num_deps > 0)
			continue;
		if (!best || (node->importance > best->importance))
			best = node;
	}
	if (best) {
#if defined _XOPEN_SOURCE && (_XOPEN_SOURCE - 0) >= 600
		char path[128];
		int fd;
		snprintf(path, sizeof(path), "/etc/init.d/%s", best->name);
		if ((fd = open(path, o_flags|O_DIRECT)) >= 0) {
			(void)posix_fadvise(fd, 0, 0, POSIX_FADV_WILLNEED);
			(void)posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
			(void)posix_fadvise(fd, 0, 0, POSIX_FADV_NOREUSE);
			close(fd);
		}
#endif
		bootlog(B_NOTICE, "service %s %s", best->name, (best->filter_prefix == 'K') ? "stop" : "start");
		best->status = T_RUNNING;
	}
	return best;
}

/*
 * finish the running task
 */
void finish_task(struct makenode *node)
{
	struct makelist *n;

	if (! node)
		return;
	for (n = node->select; n; n = n->next)
		n->node->num_deps--;
#if defined _XOPEN_SOURCE && (_XOPEN_SOURCE - 0) >= 600
	{
		char path[128];
		int fd;
		snprintf(path, sizeof(path), "/etc/init.d/%s", node->name);
		if ((fd = open(path, o_flags|O_DIRECT)) >= 0) {
			(void)posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
			close(fd);
		}
	}
#endif
	node->status = T_FINISHED;
	bootlog(B_NOTICE, "service %s done", node->name);
}


/*
 * Print out the status that bash can run eval.
 * The following things will be printed:
 * failed services, skipped services and the current progress value.
 */
void print_run_result(int *resvec, struct makenode **nodevec, const char *action)
{
	int i, r, stop = (! strcmp(action, "stop"));

	printf("failed_service=\"");
	i = r = 0;
	for (i = 0; i < tree_entries; i++) {
#if DEBUG
		if (resvec[i] == 255) {
			fprintf(stderr, "ERROR: forgotten process??\n");
			exit(1);
		}
#endif
		if (resvec[i] >= 1 && resvec[i] <= 4) {
			if (r)
				printf(" ");
			printf("%s", nodevec[i]->name);
			r++;
		} else if (!stop && resvec[i] == 7) {
			if (r)
				printf(" ");
			printf("%s", nodevec[i]->name);
			r++;
		}
	}
	printf("\"\n");
#ifdef SUSE	/* SuSE */
	printf("skipped_service=\"");
	i = r = 0;
	for (i = 0; i < tree_entries; i++) {
		if (resvec[i] == 5 || resvec[i] == 6) {
			if (r)
				printf(" ");
			printf("%s", nodevec[i]->name);
			r++;
		}
	}
#else  /* !SuSE */
	printf("skipped_service_not_installed=\"");
	i = r = 0;
	for (i = 0; i < tree_entries; i++) {
		if (resvec[i] == 5) {
			if (r)
				printf(" ");
			printf("%s", nodevec[i]->name);
			r++;
		}
	}
	printf("\"\n");
	printf("skipped_service_not_configured=\"");
	i = r = 0;
	for (i = 0; i < tree_entries; i++) {
		if (resvec[i] == 6) {
			if (r)
				printf(" ");
			printf("%s", nodevec[i]->name);
			r++;
		}
	}
#endif /* !SuSE */
	printf("\"\n");
}

#if DEBUG
void dump_status(void)
{
	struct makenode *node;

	for (node = tree_list; node; node = node->next)
		fprintf(stderr, "XXX %s: status = %d, dep = %d, int = %d, imp = %d\n",
			node->name, node->status, node->num_deps, node->interactive, node->importance);
}
#endif

#ifdef TEST
void *xcalloc(size_t nmemb, size_t size)
{
	void *r;
	if ((r = (void *)calloc(nmemb, size)) == 0) {
		fprintf(stderr, "calloc: out of memory\n");
		exit(1);
	}
	return r;
}

int main(int argc, char **argv)
{
	struct makenode *nodevec;
	char makefile[64];

	if (argc != 4) {
		fprintf(stderr, "usage: makeboot <action> [<prev> <run>]\n");
		goto out;
	}

	snprintf(makefile, sizeof(makefile), "depend.%s", argv[1]);
	parse_makefile(makefile);

	fprintf(stderr, "check_run_files(%s, %s, %s)\n", argv[1], argv[2],
		argv[3]);
	check_run_files(argv[1], argv[2], argv[3]);
out:
	while ((nodevec = pickup_task())) {
		fprintf(stdout, "%s (%s)\n", nodevec->name, nodevec->arg0);
		finish_task(nodevec);
	}

	return 0;
}
#endif
