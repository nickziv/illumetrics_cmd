Building
========

Go to `build/illumos` directory and run `make illumetrics` to get the executable.

Run `make clean` to remove everything that was built.

Status
======

THIS CODE IS PRE-ALPHA QUALITY.

Currently the code BUILDs. However it is still just a collection of stubs and
ideas and doesn't do anything useful _at all_.

THIS CODE IS PRE-ALPHA QUALITY.

Hacking
========

There are 3 files you need to care about:

	src/illumetrics_impl.h
	src/illumentrics.c
	src/illumentrics_umem.c

The first one defines the structs used, just like in an Illumos-like code base.

The second one contains all of the code that does stuff.

The third one contains abstract allocation routines for our structs. Do not use
`malloc()` or anything else. Implement an abstract routine, or use
`ilm_mk_buf()` and `ilm_rm_buf()`.

To add new repositories for analysis modify one of the list files in:

	config/lists

And run `make install`, and then illumetrics to update your `~/.illumetrics`
files.

History
=======

This program used to be a literate program written in CWEB. You can still dig
out a (non-working) version from the commit history. The decision has been made
to move to the Illumos style code because CWEB is tyranical in its requirement
that _every_ chunk code must have corresponding descriptive prose! We, the free
programmers of the world, refuse to submit to any kind of authority! We will
not be told what to do by Knuth and we _certainly_ will not be subjugated by
our compilers! Live Free or Die!

Dependencies
============

As it turns out, this code requires some other code that I have written:

	libslablist
	libgraph

Last I checked libslablist builds on linux, but libgraph doesn't.

We use libgraph for handling some common graph operations. We have a few
modified variants of the common DFS and BFS algorithms that appear to not exist
anywhere else.

Also depends on:

	libgit2


Problems
=========

A pretty detailed spec of the command line options is available in the comment
above the main() function. This essentially describes what we want the code to
do. The implementation isn't particularly fleshed out yet, but we want to use
the libgraph to compute things like author aliases, centrality, and so forth.
