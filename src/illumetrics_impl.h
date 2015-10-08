/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Nick Zivkovic
 */

/*
 *
 * Introduction
 * ============
 * 
 * This is the C version of Illumetrics. The point of writing it in C is to
 * test out libgraph, by using it for the graph operations. 
 * 
 * 
 * Graphs
 * ======
 * 
 * We want to store our information in a few graphs, so that we can access the
 * data using graph operations. First let's enumerate the intra-repository
 * graphs.
 *  
 * -An undirected graph of file-modification <-> author edges.
 * 
 * -An undirected graph of file-modification <-> commit edges.
 * 
 * -An undirected graph of commit <-> author edges.
 * 
 * And now the inter-repository graphs:
 * 
 * -An undirected graph of repository <-> author edges.
 * 
 * -A directed graph of repository <-> repository merge edges.
 * 
 * And now some author-related graphs:
 * 
 * -An undirected graph of author <-> email edges. We can use this to deduce
 * the author's affiliation/patron. We can also use this to determine whether
 * two seemingly different authors are in fact the same one, since only one
 * author can own an email.
 * 
 * -An undirected graph of author <-> alias edges. Sometimes authors use
 * variations of their names, such as omitting the middle initial. We want to
 * create a graph of author-names to aliases (we arbitrarily choose a canonical
 * author name). This is meant to catch anything the previous graph didn't
 * catch.
 *
 * -To build up these graphs we access the commit logs of the various
 * repositories. We do this through the |libgit2| C library made by the folks
 * at github. Unfortunately, I couldn't get it to link against libssh2 on
 * SmartOS, so we use libssl for fetching the repository.
 */

#include <git2.h>
#include <graph.h>
#include <slablist.h>
#include <time.h>

/*
 * This is how we classify various repositories. All of these categories
 * self-explanatory, and also kind of arbitrary. We use USERLAND as a kind of
 * kitchen sink for stuff that is (a) in userland and (b) not important enough
 * to be further categorized.
 */
typedef enum rep_type {
	KERNEL,
	COMPILER,
	DISTRIBUTED_STORAGE,
	BUILD_SYSTEM,
	DOCUMENTATION,
	ORCHESTRATION,
	USERLAND,
	VIRTUALIZATION
} rep_type_t;

/*
 * We abstract away the repository operations such as getting the history and
 * commit-details, since we may want to do computations on non-git
 * repositories. In order to know which set of function calls to wrap around,
 * need the following enum.
 */
typedef enum vcs {
	GIT,
	HG,
	SVN,
	CVS,
	SCCS
} vcs_t;

/*
 * The repository structure used by Illumetrics is essentially metadata. It
 * contains a link to the git repository, and a type that classifies the
 * repository. We use this structure to fetch the actual repositories from the
 * internet.
 */
typedef struct repo {
	char *rp_url;
	rep_type_t rp_type;
	vcs_t rp_vcs;
	int rp_curcom; /* current commit */
} repo_t;

typedef struct tm tm_t;
/*
 * This is an abstract representation of a commit. Allows us to support
 * multiple repository formats and multiple backends (we can replace libgit2 if
 * something better comes along). Currently doesn't contain the number of
 * insertions and deletions corresponding to the files, but that metric doesn't
 * appear too important anyway.
 */
typedef struct repo_commit {
	char	*rc_author;
	char	*rc_email;
	char	**rc_files; /* files touched by commit */
	int	rc_nfiles;
	tm_t	rc_time; /* exact time of commit */
} repo_commit_t;

typedef enum arg {
	PULL,
	AUTHOR,
	ALIASES,
	CENTRALITY
} arg_t;

/*
 * These are global constraints on the program. They correspond to the command
 * line parameters described in the comment above main() in illumetrics.c.
 */
typedef struct constraints {
	char	*cn_author; /* name or email */
	arg_t	cn_arg; /* what's the first argument */
	repo_t	*cn_repo;
	tm_t	cn_start_date;
	tm_t	cn_end_date;
} constraints_t;


/*
 * Allocation function declarations.
 */
repo_t *ilm_mk_repo();
void ilm_rm_repo(repo_t *);
void *ilm_mk_zbuf(size_t);
void *ilm_mk_buf(size_t);
void ilm_rm_buf(void *, size_t);
