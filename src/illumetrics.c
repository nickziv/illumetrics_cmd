/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Nick Zivkovic
 */
#include "illumetrics_impl.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <strings.h>
#include <limits.h>

/*
 * Global Variables
 * ================
 *
 * These are global variable that are related to the file descriptors that we
 * will keep open for the lifetime of the process.
 */
uid_t uid;
struct passwd *pwd;
char *home;
int home_fd;
int illumetrics_fd;
int stor_fd;
int lists_fd;
#define REPO_LS_PATHS 8
char *repo_list_paths[REPO_LS_PATHS] = {"lists/build_system",
	"lists/distributed_storage", "lists/documentation", "lists/compiler",
	"lists/kernel", "lists/userland", "lists/orchestration",
	"lists/virtualization"};
/*
 * The following array essentially maps the above paths to the rep_type_t enum.
 */
rep_type_t repo_types[REPO_LS_PATHS] = {BUILD_SYSTEM, DISTRIBUTED_STORAGE,
	DOCUMENTATION, COMPILER, KERNEL, USERLAND, ORCHESTRATION,
	VIRTUALIZATION};
slablist_t *repos;
/* See constraints_t struct in illumetrics_impl.h */
constraints_t constraints;

/* forward declarations */
void prime_storage_area();
void construct_graphs();

/*
 * illumetrics <argument> <parameters>
 *
 * The arguments to the command are pretty simple.
 *
 * 	pull - pulls all repos in the lists
 *	aliases - outputs probable aliases based on emails
 *	author - given an author's name or email, we can drill down into
 *	    specifics
 *		-a <name | email>
 *			//specify a username or email address
 *		-w [commit | file | line]
 *			//specify quantum of work (commit/file/line)
 *		-r <repo> [-f <file | directory>]
 *			//restrict calculations to repo plus file or dir
 *		-D <date>[,<date>]
 *			//restrict calculations to date or daterange
 *		-h
 *			//generates histogram where buckets can be repos
 *			or subdirectories
 *
 *	centrality - output authors and centrality values, proximity is
 *	    determined by commits to common files. See: "The
 *	    Duality of Persons and Groups". The files are the 'Groups'.
 *		-a <name | email>
 *			//author to focus on
 *		-r <repo>
 *			//we can specify a repo. If none is specified, we treat
 *			all of the repos as 1 giant repo.
 *		-n <NUMBER>
 *			//top NUMBER authors by centrality value
 *		-d <distance>
 *			//neighboring authors to author specified in `-a`,
 *			limited by the distance/number-of-hops specified in
 *			`-d`.
 *		-c <degree | closeness | betweeness>
 *			//centrality value to use
 *
 */
int
main(int ac, char **av)
{
	prime_storage_area();
	construct_graphs();
	return (0);
}



/*
 * Repositories of interest
 * ========================
 *
 * We store lists of repository URLs in the `.illumetrics/lists` directory.
 * Each list is named after the `repo_type_t`, such as `kernel' or
 * `orchestration'. We create from each list of URLs a Slab List of `repo_t`
 * structs. 
 */
int
repo_cmp(selem_t e1, selem_t e2)
{
	repo_t *r1 = e1.sle_p;
	repo_t *r2 = e2.sle_p;
	int cmp = strcmp(r1->rp_url, r2->rp_url);
	return (cmp);
}

int
repo_bnd(selem_t e, selem_t min, selem_t max)
{
	int cmp = repo_cmp(e, min);
	if (cmp < 0) {
		return (cmp);
	}
	cmp = repo_cmp(e, max);
	if (cmp > 0) {
		return (cmp);
	}
	return (0);
}

/*
 * This function wraps around the standard read() syscall. It essentially reads
 * the entire contents of a file into the buffer, by retrying to read() until
 * all bytes are read.
 */
void
atomic_read(int fd, void *buf, size_t sz)
{
	size_t red = 0;
	while (red < sz) {
		red += read(fd, (buf + red), (sz - red));
	}
}

/*
 * This function wraps around the standard write() syscall. It essentially
 * writes the entire contents of a buffer into a file, by retrying to write()
 * until all bytes are written.
 */
void
atomic_write(int fd, void *buf, size_t sz)
{
	size_t written = 0;
	while (written < sz) {
		written += write(fd, (buf + written), (sz - written));
	}
}

/*
 * The get_lines function returns an array of strings, where each string
 * corresponds to a single line in the file. We allocate a buffer for the whole
 * file. We copy the contents of the file into the buffer. We then replace the
 * newlines with a NULL and store pointers to the beginnings of these strings
 * into an array, and return the array.
 */
char **
get_lines(int fd, int *lns)
{
	struct stat st;
	fstat(fd, &st);
	char *buf = ilm_mk_buf(st.st_size);
	atomic_read(fd, buf, st.st_size);
	size_t nlines = 0;
	int i = 0;
	while (i < st.st_size) {
		if (buf[i] == '\n') {
			buf[i] = '\0';
			nlines++;
		}
		i++;
	}
	char **lines = ilm_mk_buf(sizeof (char *) * nlines);
	i = 0;
	int j = 0;
	int next = 1;
	while (i < st.st_size) {
		if (next) {
			lines[j] = buf+i;
			j++;
			next = 0;
		}
		if (buf[i] == '\0') {
			next = 1;
		}
		i++;
	}
	*lns = nlines;
	return (lines);
}

/*
 * We use the URL to determine the VCS type. We are given the repo_type.
 */
repo_t *
url_to_repo(char *url, rep_type_t rt)
{
	repo_t *r = ilm_mk_repo();
	r->rp_type = rt;
	r->rp_url = url;
	/*
	 * So far, we only support git. But as we add support for other
	 * repo formats, we will expand this section to detect the repo format
	 * by URL.
	 */
	int cmp = strncmp(url, "git://", 6);
	if (!cmp) {
		r->rp_vcs = GIT;
		return (r);
	}
	return (NULL);
}

/*
 * This function copies the `dir` and its contents into the directory pointed
 * to by `fd`. If `fd` is not a directory the result is undefined.
 */
void
copy_dir(char *dir, int fd)
{
	int dfd = open(dir, O_SEARCH);
	struct dirent de;
	struct stat st;
	int g;
	while (1) {
		g = getdents(dfd, &de, FILENAME_MAX + 1);
		if (!g) {
			break;
		}
		int cp_from = openat(dfd, de.d_name, O_RDONLY);
		int fst = fstat(fd, &st);
		if (fst) {
			/* TODO handle this error */
		}
		int cp_to = openat(fd, de.d_name, O_RDWR | O_CREAT);
		void *buf = ilm_mk_buf(st.st_size);
		atomic_read(cp_from, buf, st.st_size);
		atomic_write(cp_to, buf, st.st_size);
		ilm_rm_buf(buf, st.st_size);
		close(cp_from);
		close(cp_to);
	}
	close(dfd);
}

void
load_repositories()
{
	repos = slablist_create("repo_sl", repo_cmp, repo_bnd, SL_SORTED);

	int i = 0;
	int lines = 0;
	while (i < REPO_LS_PATHS) {
retry_list_fd_open:;
		int fd = openat(illumetrics_fd, repo_list_paths[i], O_RDONLY);
		/*
		 * If the list files are missing we copy them from the prefix.
		 */
		if (fd < 0) {
			copy_dir(PREFIX"config/lists", lists_fd);
			goto retry_list_fd_open;
		}
		char **urls = get_lines(fd, &lines);
		close(fd);
		int j = 0;
		while (j < lines) {
			repo_t *rep = url_to_repo(urls[j], repo_types[i]);
			selem_t srep;
			srep.sle_p = rep;
			if (rep == NULL) {
				fprintf(stderr, "In File: %s ",
				    repo_list_paths[i]);
				fprintf(stderr,
				    "Unrecognized repository URL: %s\n",
				    urls[j]);
				exit(1);
			} else {
				int slstat = slablist_add(repos, srep, 0);
				if (slstat == SL_EDUP) {
					fprintf(stderr, "In File: %s ",
					    repo_list_paths[i]);
					fprintf(stderr,
					    "Duplicate repository URL: %s\n",
					    urls[j]);
				exit(2);
				}
			}
			j++;
		}
		i++;
	}
}



/*
 * Persistence
 * ===========
 *
 * We want to fetch the repositories from their URLs, and store them on disk.
 * We assume ownership over the `~/.illumetrics` directory and store the data
 * in `.illumetrics/stor`. We create 1 directory per patron, and within each of
 * those directories create a directory for each `rep_type_t`. It is within
 * that heirarchy that we store repositories. We do this in order to mitigate
 * conflicts between two repos with the same name. The patron is represented by
 * a domain name like `joyent.com` or `omniti.com`.
 * 
 * Before we do any calculation, we prime the sorage area which entails the
 * following: (a) we try to get all of the repositories up to date, which has the
 * side effect of forcing us fetch any new repositories that were added to the
 * program; (b) we also purge any repositories that are removed from this program,
 * by doing a relative complement on the two sets.
 */
void open_fds();
void load_repositories();
void update_all_repos();
void purge_unrecognized_repos();

void
prime_storage_area()
{
	open_fds();
	load_repositories();
	update_all_repos();
	purge_unrecognized_repos();
}


/*
 * Here we open the main FDs that we will use for the lifetime of the process.
 */
void
open_fds()
{
	uid = getuid();
	pwd = getpwuid(uid);
	home = pwd->pw_dir;
	home_fd = open(home, O_RDWR);
	mkdirat(home_fd, ".illumetrics", S_IRWXU);
	mkdirat(illumetrics_fd, "stor", S_IRWXU);
	mkdirat(illumetrics_fd, "lists", S_IRWXU);
	illumetrics_fd = openat(home_fd, ".illumetrics", O_RDWR);
	stor_fd = openat(illumetrics_fd, "stor", O_RDWR);
	lists_fd = openat(illumetrics_fd, "lists", O_RDWR);
}

/*
 * Abstract Repository Functions
 * =============================
 *
 * We define wrapper functions that wrap over the actual functions that read a
 * repository format. Branching is done via a switch statement that switches on
 * `rp_vcs` member of repo_t.
 */

/*
 * Synchronizes on-disk repo with canonical remote repo.
 */
void
repo_pull(repo_t *r)
{
	switch (r->rp_vcs) {

	case GIT:
		break;
	}
}

/*
 * Returns the next commit. We don't want to load all of the commits into
 * memory, we just stream them, and add their information to the graphs,
 * mentioned in `illumetrics_impl.h`. The first commit retrieved is the commit
 * made closest to constraints.cn_start_date. Similarly the last commit
 * retrieved is the commit made closes to constraints.cn_end_date. If there are
 * no commits in that range we return NULL. If we reach the end of the commits
 * in that range, we return NULL.
 */
repo_commit_t *
repo_get_next_commit(repo_t *r)
{
	switch (r->rp_vcs) {

	case GIT:
		break;
	}
}


/*
 * Using an abstract set of repository commands, we update all of the
 * repositories that we've loaded from the list-files.
 */
void
update_all_repos()
{

}

/*
 * Once we've update all of the repositories we've recognized, we purge the ones
 * that are on disk but not in memory.
 */
void
purge_unrecognized_repos()
{

}


/*
 * Graph Construction. So we've gone over which graphs we want to construct, in
 * `illumetrics_impl.h`. Now we want to actually construct this graph. We
 * basically construct the graphs... 
 */
void
construct_graphs()
{

}


/*
 * Cross-polination. We want to calculate crosspolination between repos. This
 * involves mapping merges to commits in other repos. We'll need to specify what's
 * related to what.
 *
 * We also want to calculate where certain commits originated from (Joyent,
 * Nexenta, Delphix).
 */
void
cross_polination()
{

}
