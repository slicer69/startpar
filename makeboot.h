/*
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

enum {
	T_READY, T_RUNNING, T_FINISHED
};

/* target nodes */
struct makenode {
	char *name;
	char *arg0;
	int num_deps;
	struct makelist *depend;
	int num_sels;
	struct makelist *select;
	int status;
	struct makenode *next;
	int interactive;
	int importance;
	int filter_prefix;
};

/* dependency and selection list nodes */
struct makelist {
	struct makenode *node;
	struct makelist *next;
};

extern int tree_entries;
extern struct makenode *tree_list;

extern void parse_makefile(const char *path);
extern void check_run_files(const char *action, const char *prev, const char *run);
extern struct makenode *pickup_task(void);
extern void finish_task(struct makenode *n);
extern void *xcalloc(size_t nmemb, size_t size);
extern void print_run_result(int *resvec, struct makenode **nodevec, const char *action);

#define alignof(type)		((sizeof(type)+(sizeof(void*)-1)) & ~(sizeof(void*)-1))
#define strsize(string)		((strlen(string)+1)*sizeof(char))
