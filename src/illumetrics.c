#include "illumetrics_impl.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>

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
slablist_t *repos;

int
main(int ac, char **av)
{
	prime_storage_area();
	construct_graphs();
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
repo_cmp()
{

}

int
repo_bnd()
{

}

static int
load_kernel_repos()
{
	int fd = openat(illumetrics_fd, "lists/kernel", O_RDONLY);
}

void
load_build_sys_repos()
{
	int fd = openat(illumetrics_fd, "lists/build_system",
	    O_RDONLY);
}

void
load_compiler_repos()
{
	int fd = openat(illumetrics_fd, "lists/compiler", O_RDONLY);
}

void
load_dist_stor_repos()
{
	int fd = openat(illumetrics_fd, "lists/dist_stor", O_RDONLY);
}

void
load_doc_repos()
{
	int fd = openat(illumetrics_fd, "lists/doc", O_RDONLY);
}

void
load_orches_repos()
{
	int fd = openat(illumetrics_fd, "lists/orches", O_RDONLY);
}

void
load_virt_repos()
{
	int fd = openat(illumetrics_fd, "lists/virt", O_RDONLY);
}

void
load_userland_repos()
{
	int fd = openat(illumetrics_fd, "lists/userland", O_RDONLY);
}

void
load_repositories()
{
	repos = slablist_create("repo_sl", repo_cmp, repo_bnd, SL_SORTED);
	load_build_sys_repos();
	load_compiler_repos();
	load_dist_stor_repos();
	load_doc_repos();
	load_kernel_repos();
	load_orches_repos();
	load_userland_repos();
	load_virt_repos();
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
 * a domain name like `joyent.com` or `omnios.com`.
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
	illumetrics_fd = openat(home_fd, ".illumetrics", O_CREAT | O_RDWR);
	stor_fd = openat(illumetrics_fd, "stor", O_CREAT | O_RDWR);
}

/*
 * Using libgit2, we update all of the repositories that we've loaded from the
 * list-files.
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
 * Graph Construction. So we've gone over which graphs we want to construct, at
 * the beginning of this program. Now we want to actually construct this graph. We
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
