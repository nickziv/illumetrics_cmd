\def\bull{\smallskip\textindent{$\bullet$}}

@* Introduction. This is the C version of Illumetrics. The point of writing it
in C is to test out libgraph, by using it for the graph operations. In addition
to being a C version, it's also a Literate Programming version. I read some of
Knuth's programs (MMIXWare, Adventure) and read a book (Literate Programming),
after having come across (1)~a hacker news article that asks why Literate
Programming never became mainstream, and (2)~a book called {\it Physically
Based Graphics}. So, while there is a lot of opinion and philosophy as to why
Literate Programming is or isn't good, I figured I'd try it out. It is
certainly marginal, and a bit strange. However, I really can't draw a
conclusion about its feasibility without having used it on at least one
project.

I've decided to test Literate Programming out on Illumetrics, because it's
already a total mess, and I completely forgot how it works. If this variant of
programming succeeds, then great. If it fails, I'll have learned something new.

@* Graphs. We want to store our information in a few graphs, so that we can
access the data using graph operations. First let's enumerate the
intra-repository graphs.
 
\bull
An undirected graph of file-modification $\leftrightarrow$ author edges.

\bull
An undirected graph of file-modification $\leftrightarrow$ commit edges.

\bull
An undirected graph of commit $\leftrightarrow$ author edges.

And now the inter-repository graphs:

\bull
An undirected graph of repository $\leftrightarrow$ author edges.

\bull
A directed graph of repository $\leftrightarrow$ repository merge edges.

And now some author-related graphs:

\bull
An undirected graph of author $\leftrightarrow$ email edges. We can use this to
deduce the author's affiliation/patron. We can also use this to determine
whether two seemingly different authors are in fact the same one, since only
one author can own an email.

\bull
An undirected graph of author $\leftrightarrow$ alias edges. Sometimes authors
use variations of their names, such as omitting the middle initial. We want to
create a graph of author-names to aliases (we arbitrarily choose a canonical
author name). This is meant to catch anything the previous graph didn't catch.

To build up these graphs we access the commit logs of the various
repositories. We do this through the |libgit2| C library made by the folks at
github. Unfortunately, I couldn't get it to link against libssh2 on SmartOS, so
we use libssl for fetching the repository.

@* Top Down. Here is the top-down view of the program, if you want to explore
it that way.

@ NOTE: Huge thing here, is that we are doing everything all at once, and then
computing whatever the user wants us to compute. Point is we might not need to
construct all of the graphs, if we are going to do one little thing. I'll do it
this way for now, to see if we can actually fit all of the repos in memory. If
not, I'll make the loading conditional on what computation we're doing.

@c
@< Headers @>;
@< Structures and Enumerations @>;
int
main(int ac, char **av)
{
	@< Prime Storage Area @>;
	@< Construct Graphs @>;
}


@* Repository Structure. The repository structure used by Illumetrics is
essentially metadata. It contains a link to the git repository, and a type that
classifies the repository. We use this structure to fetch the actual
repositories from the internet.

@< Repository Structure @>=
typedef struct repo {
	char *rp_url;
	rep_type_t rp_type;
} repo_t;

@ This is how we classify various repositories. All of these categories
self-explanatory, and also kind of arbitrary. We use |USERLAND| as a kind of
kitchen sink for stuff that is (a)~in userland and (b)~not important enough to
be further categorized.
@< Type and Patron Enumerations @>=
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

@* Repositories of interest. We store lists of repository URLs in the
|.illumetrics/lists| directory. Each list is named after the |repo_type_t|,
such as `kernel' or `orchestration'. We create from each list of URLs a Slab
List of |repo_t| structs. 

@ This function loads all of the repository lists...
@< Load Repositories @>=
static int
load_repositories()
{
	int i;
	@< Load Build System Repositories @>;
	@< Load Compiler Repositories @>;
	@< Load Distributed Storage Repositories @>;
	@< Load Documentation Repositories @>;
	@< Load Kernel Repositories @>;
	@< Load Orchestration Repositories @>;
	@< Load Userland Repositories @>;
	@< Load Virtualization Repositories @>;
}

@ We load the kernel repos first. They are the most important of all.
@< Load Kernel Repositories @>=
int kernel_list_fd = openat(illumetrics_fd, "lists/kernel", O_RDONLY);

@ Yep...
@< Load Build System Repositories @>=
int build_sys_list_fd = openat(illumetrics_fd, "lists/build_system", O_RDONLY);

@ Yep...
@< Load Compiler Repositories @>=
int compiler_list_fd = openat(illumetrics_fd, "lists/compiler", O_RDONLY);

@ Yep...
@< Load Distributed Storage... @>=
int dist_stor_list_fd = openat(illumetrics_fd, "lists/dist_stor", O_RDONLY);

@ Yep...
@< Load Documentation... @>=
int doc_list_fd = openat(illumetrics_fd, "lists/doc", O_RDONLY);

@ Yep...
@< Load Orchestration... @>=
int orches_list_fd = openat(illumetrics_fd, "lists/orches", O_RDONLY);

@ Yep...
@< Load Virtualization... @>=
int virt_list_fd = openat(illumetrics_fd, "lists/virt", O_RDONLY);

@ Yep...
@< Load Userland... @>=
int userland_list_fd = openat(illumetrics_fd, "lists/userland", O_RDONLY);




@* Persistence. We want to fetch the repositories from their URLs, and store
them on disk. We assume ownership over the |~/.illumetrics| directory and store
the data in |.illumetrics/stor|. We create 1 directory per patron, and within
each of those directories create a directory for each |rep_type_t|. It is
within that heirarchy that we store repositories. We do this in order to
mitigate conflicts between two repos with the same name. The patron is
represented by a domain name like |"joyent.com"| or |"omnios.com"|.

Before we do any calculation, we prime the sorage area which entails the
following: (a)~we try to get all of the repositories up to date, which has the
side effect of forcing us fetch any new repositories that were added to the
program; (b)~we also purge any repositories that are removed from this program,
by doing a relative complement on the two sets.

@< Prime Storage Area @>=
@< Open FDs @>;
@< Load Repositories @>;
@< Update All Repositories @>;
@< Purge Unrecognized Repositories @>;

@ Here we open the main FDs that we will use for the lifetime of the process.
@< Open FDs @>=
uid_t uid = getuid();
struct passwd *pwd = getpwuid(uid);
char *home = pwd->pw_dir;
int home_fd = open(home, O_RDWR);
int illumetrics_fd = openat(home_fd, ".illumetrics", O_CREAT | O_RDWR);
int stor_fd = openat(illumetrics_fd, "stor", O_CREAT | O_RDWR);

@ Using libgit2, we update all of the repositories that we've loaded from the
list-files.
@< Update All Repositories @>=
int foobar = 0;

@ Once we've update all of the repositories we've recognized, we purge the ones
that are on disk but not in memory.

@< Purge Unrecognized Repositories @>=
int foobar = 0;


@* Graph Construction. So we've gone over which graphs we want to construct, at
the beginning of this program. Now we want to actually construct this graph. We
basically construct the graphs... 

@< Construct Graphs @>=


@* Cross-polination. We want to calculate crosspolination between repos. This
involves mapping merges to commits in other repos. We'll need to specify what's
related to what.

We also want to calculate where certain commits originated from (Joyent,
Nexenta, Delphix).



@ Headers.
@< Headers @>=
#include <git2.h>
#include <graph.h>

@ Structures and Enumerations.

@< Structures and Enumerations @>=
@< Repository Structure @>;
@< Type and Patron... @>;

@ The formatting code.
@f uint8_t int
@f uint16_t int
@f uint32_t int
@f uint64_t int
@f int8_t int
@f int16_t int
@f int32_t int
@f int64_t int
@f slablist_t int
@f slablist_elem_t int
@f selem_t int
@f lg_graph_t int
@f lg_gelem_t int
@f lp_grmr_t int
@f lp_ast_t int
@f rep_type_t int
