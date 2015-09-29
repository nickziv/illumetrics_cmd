Building
========

Got to `build/` directory and run `make illumetrics` to get the executable.

Run `make clean` to remove everything that was built.

Status
======

Currently the code BUILDs. However it is still just a collection of stubs and
ideas and doesn't do anything useful _at all_.

Hacking
========

There are 2 files you need to care about:

	src/illumetrics_impl.h
	src/illumentrics.c

The first one defines the structs used, just like in an Illumos-like code base.

The second one contains all of the code that does stuff.

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


Problems
=========

So far this code doesn't run. Haven't tried to compile it yet. We should
implement the `exec` make target. For example the `LDFLAGS` are not implemented
yet, and the `LIBS` are probably wrong at worst and platform-specific at best.

Use the `code` make target to get the raw C source code (which probably won't
compile).
