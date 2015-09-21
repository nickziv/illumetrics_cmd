Building
========

Got to `build/` directory and run `make pdf` to get the pdf.

Run `make clean` to remove the junk.

Hacking
========

Edit the CWEB file in `src/illumetrics.w`.

There's a lot of stuff to be done. If something isn't adequately explained,
feel free to change it and submit a pull request. Also open to suggestions on
writing style. For example I once tried to write this program as a first-person
narrative from the perspective of the illumetrics project. Then I realized that
everyone would think I had gone crazy, and swiftly deleted it.

Also don't hold back when it comes to adding features. If you have any
questions about CWEB feel free to ask me. And, of course, google is you friend.
The cweb manpage sucks. There's a PDF written by Knuth somewhere on the
internet that describes CWEB.

There are trains of thought in the CWEB file that describe some of my thinking.
There is some text that may also be out dated or in reference to some piece of
code that doesn't exist anymore. If you run into something that makes no sense
-- TELL ME.

Problems
=========

So far this code doesn't run. Haven't tried to compile it yet. We should
implement the `exec` make target. For example the `LDFLAGS` are not implemented
yet, and the `LIBS` are probably wrong at worst and platform-specific at best.

Use the `code` make target to get the raw C source code (which probably won't
compile).
