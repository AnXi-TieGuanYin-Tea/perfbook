/*
 * smpalloc.c: simple memory allocator intended to demonstrate
 *	the parallel-fastpath concurrency design.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * Copyright (c) 2006 Paul E. McKenney, IBM Corporation.
 */

#include "../api.h"

#define TARGET_POOL_SIZE 3
#define GLOBAL_POOL_SIZE 40

struct memblock {
	char *bytes[CACHE_LINE_SIZE];
} memblocks[GLOBAL_POOL_SIZE];

struct globalmempool {
	spinlock_t mutex;
	int cur;
	struct memblock *pool[GLOBAL_POOL_SIZE];
} globalmem;

struct perthreadmempool {
	int cur;
	struct memblock *pool[2 * TARGET_POOL_SIZE];
};

DEFINE_PER_THREAD(struct perthreadmempool, perthreadmem);

struct memblock *memblock_alloc(void)
{
	int i;
	struct memblock *p;
	struct perthreadmempool *pcpp = &__get_thread_var(perthreadmem);

	if (pcpp->cur < 0) {
		spin_lock(&globalmem.mutex);
		for (i = 0; i < TARGET_POOL_SIZE && globalmem.cur >= 0; i++) {
			pcpp->pool[i] = globalmem.pool[globalmem.cur];
			globalmem.pool[globalmem.cur--] = NULL;
		}
		pcpp->cur = i - 1;
		spin_unlock(&globalmem.mutex);
	}
	if (pcpp->cur >= 0) {
		p = pcpp->pool[pcpp->cur];
		pcpp->pool[pcpp->cur--] = NULL;
		return p;
	}
	return NULL;
}

void memblock_free(struct memblock *p)
{
	int i;
	struct perthreadmempool *pcpp = &__get_thread_var(perthreadmem);

	if (pcpp->cur >= 2 * TARGET_POOL_SIZE - 1) {
		spin_lock(&globalmem.mutex);
		for (i = pcpp->cur; i >= TARGET_POOL_SIZE; i--) {
			globalmem.pool[++globalmem.cur] = pcpp->pool[i];
			pcpp->pool[i] = NULL;
		}
		pcpp->cur = i;
		spin_unlock(&globalmem.mutex);
	}
	pcpp->pool[++pcpp->cur] = p;
}

void initpools(void)
{
	int i;
	int j;

	for (i = 0; i < NR_THREADS; i++) {
		per_thread(perthreadmem, i).cur = -1;
		for (j = 0; j < 2 * TARGET_POOL_SIZE; j++) {
			per_thread(perthreadmem, i).pool[j] = NULL;
		}
	}
	spin_lock_init(&globalmem.mutex);
	globalmem.cur = -1;
	for (i = 0; i < GLOBAL_POOL_SIZE; i++) {
		memblock_free(&memblocks[i]);
	}
}

#ifdef TEST

int goflag;

DEFINE_PER_THREAD(long, results);
DEFINE_PER_THREAD(long, failures);

#define MAX_RUN 100

int memblocks_available(void)
{
	int i;
	int sum = globalmem.cur + 1;

	for_each_thread(i)
		sum += per_thread(perthreadmem, i).cur + 1;
	return sum;
}

void *memblock_test(void *arg)
{
	long cnt = 0;
	long cntfail = 0;
	int i;
	int runlength = (int)(long)arg;
	struct memblock *p[MAX_RUN];

	if (runlength > MAX_RUN)
		runlength = MAX_RUN;
	while (goflag) {
		for (i = 0; i < runlength; i++)
			p[i] = memblock_alloc();
		for (i = 0; i < runlength; i++) {
			if (p[i] == NULL) {
				cntfail++;
			} else {
				memblock_free(p[i]);
				cnt++;
			}
		}
	}
	__get_thread_var(results) += cnt;
	__get_thread_var(failures) += cntfail;

	return NULL;
}

void usage(char *progname)
{
	fprintf(stderr,
		"Usage: %s [ #threads [ alloc runlength ] ]\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int i;
	long long nc;
	long long nf;
	int nkids = 1;
	int runlength = 1;
	int totbefore;

	smp_init();
	initpools();

	if (argc > 1) {
		nkids = strtoul(argv[1], NULL, 0);
		if (nkids > NR_THREADS) {
			fprintf(stderr, "nkids = %d too large, max = %d\n",
				nkids, NR_THREADS);
			usage(argv[0]);
		}
	}
	if (argc > 2) {
		runlength = strtoul(argv[2], NULL, 0);
		if (runlength > MAX_RUN) {
			fprintf(stderr, "nkids = %d too large, max = %d\n",
				runlength, MAX_RUN);
			usage(argv[0]);
		}
	}
	printf("%d %d ", nkids, runlength);

	init_per_thread(results, 0L);
	init_per_thread(failures, 0L);
	totbefore = memblocks_available();

	goflag = 1;
	for (i = 0; i < nkids; i++)
		create_thread(memblock_test, (void *)(long)runlength);

	sleep(1);
	goflag = 0;

	wait_all_threads();
	nf = nc = 0;
	for (i = 0; i < NR_THREADS; i++) {
		nc += per_thread(results, i);
		nf += per_thread(failures, i);
	}
	printf("a/f: %Ld  fail: %Ld\n", nc, nf);
	if (memblocks_available() != totbefore) {
		printf("memblocks: before: %d after: %d\n",
		       totbefore, memblocks_available());
	}

	exit(0);
}

#endif /* #ifdef TEST */
