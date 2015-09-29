/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Nick Zivkovic
 */


#ifdef UMEM
#include <umem.h>
#endif
#include <stdlib.h>
#include <strings.h>
#include "illumetrics_impl.h"

#define UNUSED(x) (void)(x)
#define CTOR_HEAD       UNUSED(ignored); UNUSED(flags)

umem_cache_t *cache_repo;

#ifdef UMEM
//constructors...

int
repo_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	repo_t *r = buf;
	bzero(r, sizeof (repo_t));
	return (0);
}
#endif

int
illumetrics_umem_init()
{
#ifdef UMEM
	cache_repo = umem_cache_create("repo",
		sizeof (repo_t),
		0,
		repo_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

#endif
	return (0);

}

repo_t *
mk_repo()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_repo, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (repo_t)));
#endif
}

void
rm_repo(repo_t *r)
{
#ifdef UMEM
	bzero(r, sizeof (repo_t));
	umem_cache_free(cache_repo, r);
#else
	bzero(r, sizeof (repo_t));
	free(r);
#endif
}
