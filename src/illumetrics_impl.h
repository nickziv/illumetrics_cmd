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
 * -A directed graph of file-modification -> author edges.
 * 
 * -A directed graph of file-modification -> commit edges.
 * 
 * 
 * And now the inter-repository graphs:
 * 
 * -A directed graph of repository -> author edges.
 * 
 * -A directed graph of repository <-> repository merge edges.
 * 
 * And now some author-related graphs:
 * 
 * -A directed graph of email -> author edges. We can use this to deduce the
 * author's affiliation/patron. We can also use this to determine whether two
 * seemingly different authors are in fact the same one, since only one author
 * can own an email.
 * 
 * -An undirected graph of author <-> alias edges. Sometimes authors use
 * variations of their names, such as omitting the middle initial. We want to
 * create a graph of author-names to aliases (we arbitrarily choose a canonical
 * author name). This is meant to catch anything the previous graph didn't
 * catch.
 *
 * -To build up these graphs we access the commit logs of the various
 * repositories. We do this through the |libgit2| C library made by the folks
 * at github.
 *
 * Note on directedness: All of the directed graphs follow a specific format:
 * we have an entity of one type (say an email) pointing to an entity of
 * another type (an author). In such a graph, all of the edges have an email as
 * the source and an author as the destination. We do this, because both of
 * these types are just strings, and this kind of constraint on the graph
 * allows us to forgo validating the strings. From these directed graph,
 * undirected graphs can be derived, so we don't lose anything. It's very
 * difficult to go the other way (undirected to directed).
 *
 * Centrality Calculations:
 * ========================
 *
 * Most methods of calculating how signinficant or central an author is to a
 * rpject center around counting the raw number of commits made. This is a good
 * start for guaging how much work got done, but that doesn't tell the whole
 * story. It just tells us that a particular author commits a lot of code.
 *
 * But what we would like to know in addition to that is how central an author
 * is to a project. That is, if an author were to get hit by a bus, how many
 * contributors would take notice.
 *
 * This is called a centrality value, and there are a few kinds of centrality.
 * For now, we focus on the three most basic kinds of centrality:
 *
 *  - Degree Centrality: How many other authors is a single author connected
 *  to.
 *
 *  - Betweeness Centrality: How many authors have to use this author as a
 *  go-between to get to another author.
 *
 *  - Closeness Centrality: How close is this author to every other author.
 *
 * There are some other intereasting centrality measures, that we may implement
 * in the future.
 *
 * This information can be illuminating, however there is a problem. We don't
 * know what the direct connections between authors are, from the commit logs
 * alone. Even though we could inlcude external data, like mailing list data
 * and twitter data, and so forth, we would like to use the logs as much as
 * possible.
 *
 * It turns out however, that somebody was thinking about this essential
 * problem, around thirty or forty years ago. The author -- who's name escapes
 * me at the moment -- wrote a paper called the "Duality of Persons and
 * Groups". Essentially, it uses the presence of group-membership information
 * to estimate the closeness between people. It uses meta-data to estimate real
 * data.
 *
 * It essentially developed a method for transforming a graph of group <->
 * person into a graph of person <-> person and a graph of group <-> group. We
 * do essentially the same thing, except that we have a directed graph of
 * file-modification -> person. Our implemenetation differs a bit since we use
 * a slablist-backed graph of edges to represent a graph via libgraph, while
 * the author used an adjacency matrix approach. Our method is faster since our
 * complexity is a function of the number of edges, whereas the matrix-multiply
 * method is a function of the number of nodes squared (or the number of
 * possible edges which is larger than the number of actual edges).
 *
 * The commit log not only contains which files an author modified, but also
 * how much and _when_. We can use intensity of modification and temporal
 * distance between modifications when building the graphs.
 *
 * The resulting graph isn't necessarily a graph of _communication_ between
 * authors. For all we know the two authors may never have even met. However it
 * is a graph of authors who may be connected by common knowledge or common
 * familiarity of certain parts of the code base. This works when localized to
 * a single repository, but can also be applied to a set of repositories. Which
 * is conceptually no different from copying a set of repositories into a
 * common directory, and coalescing all of the commit logs.
 */

#include "illumetrics_provider.h"
#include <git2.h>
#include <graph.h>
#include <slablist.h>
#include <time.h>
#include <errno.h>

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
	char *rp_owner; /* the username of the patron (i.e. joyent, omniti) */
	char *rp_name;
	rep_type_t rp_type;
	vcs_t rp_vcs;
	int rp_curcom; /* current commit */
} repo_t;

typedef struct tm tm_t;
typedef struct git_repository git_repository_t;
typedef struct git_remote git_remote_t;
/*
 * This is an abstract representation of a commit. Allows us to support
 * multiple repository formats and multiple backends (we can replace libgit2 if
 * something better comes along). Currently doesn't contain the number of
 * insertions and deletions corresponding to the files, but that metric doesn't
 * appear too important anyway. Furthermore, we don't include the commit
 * message. This could be useful in the future, but at the moment is not
 * needed. Besides, we want to use as little memory per commit as possible.
 */
typedef struct sha1 {
	uint32_t sha1_val[5];
} sha1_t;
typedef struct repo_commit {
	repo_t	*rc_repo; /* bptr to repo */
	sha1_t	*rc_sha1; /* the commit's sha1 */
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
	CENTRALITY,
	REPOSITORY
} arg_t;

/*
 * What we should treat as the quantum of work for an author, for the run of
 * this program.
 */
typedef enum qwork {
	QW_COMMIT,
	QW_FILE,
	QW_LINE,
	QW_WTF
} qwork_t;

/*
 * What kind of centrality we want. May add more in the future.
 */
typedef enum cent {
	CENT_DEGREE,
	CENT_CLOSENESS,
	CENT_BETWEENESS,
	CENT_WTF
} cent_t;

/*
 * These are global constraints on the program. They correspond to the command
 * line parameters described in the comment above main() in illumetrics.c.
 */
typedef struct constraints {
	char	*cn_author; /* name or email */
	arg_t	cn_arg; /* what's the first argument */
	repo_t	*cn_repo;
	char	*cn_subtree;
	int64_t	cn_num;
	int64_t	cn_dist; /* limiting distance */
	tm_t	cn_start_date;
	tm_t	cn_end_date;
	qwork_t	cn_qwork;
	cent_t	cn_cent;
	int	cn_list; /* bool */
	int	cn_hist; /* bool, for histogram */
} constraints_t;


/*
 * Allocation function declarations.
 */
repo_t *ilm_mk_repo();
void ilm_rm_repo(repo_t *);
void *ilm_mk_zbuf(size_t);
void *ilm_mk_buf(size_t);
void ilm_rm_buf(void *, size_t);
int illumetrics_umem_init();
