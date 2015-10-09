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
char init_cwd[PATH_MAX];
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
void
args_to_constraints(int ac, char **av)
{
	(void)ac;
	if (!strcmp(av[1], "pull")) {
		constraints.cn_arg = PULL;
	} else if (!strcmp(av[1], "author")) {
		constraints.cn_arg = AUTHOR;
	} else if (!strcmp(av[1], "aliases")) {
		constraints.cn_arg = ALIASES;
	} else if (!strcmp(av[1], "centrality")) {
		constraints.cn_arg = CENTRALITY;
	}
}
void open_fds();
void load_repositories();
void update_all_repos();
void purge_unrecognized_repos();
int
main(int ac, char **av)
{
	ILLUMETRICS_GOT_HERE(__LINE__);
	illumetrics_umem_init();
	args_to_constraints(ac, av);
	open_fds();
	load_repositories();
	if (constraints.cn_arg == PULL) {
		printf("Pulling in all repos...\n");
		update_all_repos();
		printf("Done.\n");
	}
	purge_unrecognized_repos();
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
	int fs = fstat(fd, &st);
	if (fs < 0) {
		perror("get_line:fstat");
		exit(-1);
	}
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
 * Given a repo_t, we use its URL to determine what the owner's username is and
 * what the repository's name is. We can use this data to determine where in
 * the storage area we will/did clone the repo into. This function checks to
 * see if it recognized the domain name. If the domain name is not recognized,
 * we bail. This is becuase we want to make sure that we properly extract the
 * username and repository-name from the URL.
 *
 * We assume that the username corresponds to the patron-name. For example
 * 'joyent'. This isn't a particularly good assumption to make, but it is
 * expedient. If this becomes a problem we should create a mapping between
 * usernames and patrons. A patron, btw, is essentially any commercial entity
 * that contributes code to illumos (or related projects), or funds code
 * contributions to illumos (or related projects).
 */
void
repo_derive_url(repo_t *r)
{
	char *url = r->rp_url;
	int cmp = strncmp(url, "git://github.com/", 17);
	char username[FILENAME_MAX];
	int ulen = 0;
	char reponame[FILENAME_MAX];
	int rlen = 0;
	bzero(username, FILENAME_MAX);
	bzero(reponame, FILENAME_MAX);
	if (!cmp) {
		/*
		 * This is the slash ('/') that divides the username from the
		 * repo name.
		 */
		char *slash = strchr(url + 17, '/');
		/*
		 * This is the dot ('.') that divides the repo name from the
		 * file extention (.git).
		 */
		char *dot = strchr(url + 17, '.');
		if (slash == NULL) {
			fprintf(stderr,
			    "URL %s doesn't contain a slash (/).\n",
			    url);
			exit(-1);
		}
		if (dot == NULL) {
			fprintf(stderr,
			    "URL %s doesn't contain a '.git' suffix.\n",
			    url);
			exit(-1);
		}
		char *cur = url + 17; /* cursor */
		int i = 0;
		while (cur != slash) {
			username[i] = *cur;
			i++;
			cur++;
		}
		ulen = i;
		cur++; /* jump over the slash */
		i = 0;
		while (cur != dot) {
			//crash here
			reponame[i] = *cur;
			i++;
			cur++;
		}
		rlen = i;
		char *owner = ilm_mk_zbuf(ulen + 1);
		char *name = ilm_mk_zbuf(rlen + 1);
		bcopy(username, owner, ulen);
		bcopy(reponame, name, rlen);
		r->rp_owner = owner;
		r->rp_name = name;
	}
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
		repo_derive_url(r);
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
	DIR *ddir = opendir(dir);
	if (ddir == NULL) {
		perror("copy_dir:opendir:dir");
		exit(-1);
	}

	int dfd = dirfd(ddir);
	if (dfd < 0) {
		perror("copy_dir:dirfd:ddir");
		exit(-1);
	}

	struct dirent *de;
	struct stat st;
	while (1) {
		int errno_old = errno;
		de = readdir(ddir);
		if (de == NULL) {
			if (errno != errno_old) {
				perror("copy_dir:readdir");
				exit(-1);
			}
			break;
		}
		int cp_from = openat(dfd, de->d_name, O_RDONLY);
		int fst = fstat(cp_from, &st);
		if (fst < 0) {
			perror("copy_dir:fstat");
			exit(-1);
		}
		int cp_to = openat(fd, de->d_name, O_RDWR | O_CREAT, S_IRWXU);
		void *buf = ilm_mk_buf(st.st_size);
		if (buf == NULL) {
			fprintf(stderr, "%s %s\n",
				"Out of memory when copying",
				"list files to ~/.illumetrics.");
			exit(-1);
		}
		atomic_read(cp_from, buf, st.st_size);
		atomic_write(cp_to, buf, st.st_size);
		ilm_rm_buf(buf, st.st_size);
		close(cp_from);
		close(cp_to);
	}
	close(dfd);
}

/*
 * This function essentially goes through the list-files and fills out the
 * repos slablist.
 */
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
			copy_dir(PREFIX"config/lists/", lists_fd);
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
				exit(-1);
			} else {
				int slstat = slablist_add(repos, srep, 0);
				if (slstat == SL_EDUP) {
					fprintf(stderr, "In File: %s ",
					    repo_list_paths[i]);
					fprintf(stderr,
					    "Duplicate repository URL: %s\n",
					    urls[j]);
					exit(-1);
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
 * a recognizable name like `joyent` or `omniti`.
 * 
 */



/*
 * Here we open the main FDs that we will use for the lifetime of the process.
 */
void
open_fds()
{
	char *cwdret = getcwd(init_cwd, PATH_MAX);
	if (cwdret == NULL) {
		perror("open_fds:getcwd");
		exit(-1);
	}
	uid = getuid();
	pwd = getpwuid(uid);
	home = pwd->pw_dir;
	DIR *home_dir = opendir(home);
	if (home_dir == NULL) {
		perror("open_fds:opendir:$HOME");
		exit(-1);
	}
	home_fd = dirfd(home_dir);
	int ch = chdir(home);
	if (ch < 0) {
		perror("open_fds:chdir:$HOME");
		exit(-1);
	}
	if (home_fd < 0) {
		perror("open_fds:dirfd:$HOME");
		exit(-1);
	}

	int mkd = mkdirat(home_fd, ".illumetrics", S_IRWXU);
	/* It's quite possible that users have an existing .illumetrics db */
	if (mkd < 0 && errno != EEXIST) {
		perror("open_fds:mkdir:.illumetrics");
		exit(-1);
	}

	DIR *illumetrics_dir = opendir(".illumetrics");
	if (illumetrics_dir == NULL) {
		perror("open_fds:opendir:$HOME/.illumetrics");
		exit(-1);
	}
	illumetrics_fd = dirfd(illumetrics_dir);


	mkd = mkdirat(illumetrics_fd, "stor", S_IRWXU);
	if (mkd < 0 && errno != EEXIST) {
		perror("open_fds:mkdir:stor");
		exit(-1);
	}

	mkd = mkdirat(illumetrics_fd, "lists", S_IRWXU);
	if (mkd < 0 && errno != EEXIST) {
		perror("open_fds:mkdir:lists");
		exit(-1);
	}

	ch = fchdir(illumetrics_fd);
	if (ch < 0) {
		perror("open_fds:fchdir:illumetrics_fd");
		exit(-1);
	}
	DIR *stor_dir = opendir("stor");
	if (stor_dir == NULL) {
		perror("open_fds:opendir:stor");
		exit(-1);
	}
	stor_fd = dirfd(stor_dir);
	if (stor_fd < 0) {
		perror("open_fds:dirfd:stor");
		exit(-1);
	}

	DIR *lists_dir = opendir("lists");
	if (lists_dir == NULL) {
		perror("open_fds:opendir:lists");
		exit(-1);
	}
	lists_fd = dirfd(lists_dir);
	if (lists_fd < 0) {
		perror("open_fds:open:lists");
		exit(-1);
	}
	/* we change back to our original cwd */
	ch = chdir(init_cwd);
	if (ch < 0) {
		perror("open_fds:chdir:$OLDCWD");
		exit(-1);
	}
}



/*
 * Abstract Repository Functions
 * =============================
 *
 * We define wrapper functions that wrap over the actual functions that read a
 * repository format. Branching is done via a switch statement that switches on
 * `rp_vcs` member of repo_t.
 */


/* handles libgit2 errors */
void
handle_git_error(int error)
{
	const git_error *e = giterr_last();
	printf("Error %d/%d: %s\n", error, e->klass, e->message);
	exit(error);
}

/*
 * Synchronizes on-disk repo with canonical remote repo. If there is no on-disk
 * repo, we clone one into the expected location.
 */
void
repo_pull(repo_t *r)
{
	int clone = 1; /* we try to clone by default */
	int mkd = mkdirat(stor_fd, r->rp_owner, S_IRWXU);
	if (mkd < 0 && errno != EEXIST) {
		perror("repo_pull:mkdirat:stor/owner");
		exit(-1);
	}
	fchdir(stor_fd);
	DIR *owner_dir = opendir(r->rp_owner);
	if (owner_dir == NULL) {
		perror("repo_pull:opendir:stor/owner");
		exit(-1);
	}
	int owner_fd = dirfd(owner_dir);
	if (owner_fd) {
		perror("repo_pull:dirfd:stor/owner");
		exit(-1);
	}
	mkd = mkdirat(owner_fd, r->rp_name, S_IRWXU);
	if (mkd < 0 && errno != EEXIST) {
		perror("repo_pull:mkdirat:stor/owner");
		exit(-1);
	} else if (mkd < 0 && errno == EEXIST) {
		/* but we try to fetch/pull if dir exists */
		clone = 0;
	}

	/*
	 * We now get the full pathname  of the repo, by cd'ing into it and
	 * getting the cwd. Some libraries (like libgit2) accept pathnames
	 * instead of file descriptors.
	 */
	fchdir(owner_fd);
	chdir(r->rp_name);
	char repo_path[PATH_MAX];
	bzero(repo_path, PATH_MAX);
	char *cwd_ret = getcwd(repo_path, PATH_MAX);
	if (cwd_ret == NULL) {
		perror("repo_pull:getcwd");
		exit(-1);
	}
	int error;
	/* git structure declarations */
	git_repository_t *gr = NULL;
	git_remote_t *grem = NULL;

	switch (r->rp_vcs) {

	case GIT:

		if (clone) {
			error = git_clone(&gr, r->rp_url, repo_path, NULL);
			if (error < 0) {
				handle_git_error(error);
			}
		} else {
			error = git_repository_open(&gr, repo_path);
			if (error < 0) {
				handle_git_error(error);
			}
			error = git_remote_lookup(&grem, gr, "origin");
			if (error < 0) {
				handle_git_error(error);
			}
			/*
			 * XXX the libgit2 interfaces keep changing, so this
			 * function has an extra arg in newer version of
			 * libgit2. I never thought that the github guys were
			 * such amatuers.
			 */
			error = git_remote_fetch(grem, NULL, NULL);
			if (error < 0) {
				handle_git_error(error);
			}
		}
		break;
	case HG:
		fprintf(stderr, "Pull not supported on Mercurial repositories.\n");
		fprintf(stderr, "Skipping repository %s.\n", r->rp_url);
		break;
	case SVN:
		fprintf(stderr, "Pull not supported on SVN repositories.\n");
		fprintf(stderr, "Skipping repository %s.\n", r->rp_url);
		break;
	case CVS:
		fprintf(stderr, "Pull not supported on CVS repositories.\n");
		fprintf(stderr, "Skipping repository %s.\n", r->rp_url);
		break;
	case SCCS:
		fprintf(stderr, "Pull not supported on SCCS repositories.\n");
		fprintf(stderr, "Skipping repository %s.\n", r->rp_url);
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
		return (NULL);
		break;
	case HG:
		return (NULL);
		break;
	case SVN:
		return (NULL);
		break;
	case CVS:
		return (NULL);
		break;
	case SCCS:
		return (NULL);
		break;
	}
	return (NULL);
}


/*
 * This function is currently serial -- we pull repos one-by-one. Would be cool
 * if we could parallelize this.
 */

selem_t
pull_foldr(selem_t ignore, selem_t *e, uint64_t sz)
{
	repo_t *r = NULL;
	uint64_t i = 0;
	while (i < sz) {
		r = e[i].sle_p;
		repo_pull(r);
		i++;
	}
	return (ignore);
}

/*
 * Using an abstract set of repository commands, we update all of the
 * repositories that we've loaded from the list-files.
 */
void
update_all_repos()
{
	selem_t ignore;
	(void)slablist_foldr(repos, pull_foldr, ignore);
}

/*
 * Once we've loaded the repo_t slablist, we scan the `stor` directory and
 * remove any repositories that are not in the slablist.
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
