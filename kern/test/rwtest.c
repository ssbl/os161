/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

#define NUMTHREADS 8

int shared;
struct rwlock *rwlock;
struct semaphore *testsem, *donesem;

/*
 * Use these stubs to test your reader-writer locks.
 */

static
void
rw1locktestthread(void *junk, unsigned long num)
{
    (void)junk;
    (void)num;

    
    P(testsem);
    rwlock_acquire_write(rwlock);
    kprintf("thread acquired lock\n");
    shared = 50;
    rwlock_release_write(rwlock);
    V(donesem);
}

int rwtest(int nargs, char **args)
{
	(void)nargs;
	(void)args;

    int result;

    rwlock = rwlock_create("rwlock");

    rwlock_acquire_write(rwlock);
    testsem = sem_create("testsem", 0);
    donesem = sem_create("donesem", 0);
    shared = 100;
    result = thread_fork("rwtest1", NULL, rw1locktestthread, NULL, 0);
    KASSERT(result == 0);

    rwlock_release_write(rwlock);

    V(testsem);
    P(donesem);

    if (shared == 50) {
        kprintf("%d\n", shared);
        success(TEST161_SUCCESS, SECRET, "rwt1");
    } else {
        kprintf("%d\n", shared);
        success(TEST161_FAIL, SECRET, "rwt1");
    }

    rwlock_destroy(rwlock);
    sem_destroy(testsem);
    sem_destroy(donesem);

	return 0;
}

static
void
rwlocktestthread(void *junk, unsigned long num)
{
    (void)junk;

    P(testsem);
    kprintf("thread %ld created\n", num);
    rwlock_acquire_read(rwlock);
    kprintf("read acquired\n");
    V(donesem);
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

    int i, result;

    rwlock = rwlock_create("rwlock");
    KASSERT(rwlock);
    testsem = sem_create("testsem", 0);
    donesem = sem_create("donesem", 0);

    for (i = 0; i < NUMTHREADS; i++) {
        result = thread_fork("rwtest", NULL, rwlocktestthread, NULL, i);
        KASSERT(result == 0);
    }

	for (i=0; i<NUMTHREADS; i++) {
		kprintf_t(".");
		V(testsem);
		P(donesem);
	}

    if (rwlock->rwlock_numreaders == NUMTHREADS) {
        success(TEST161_SUCCESS, SECRET, "rwt2");
    } else {
        success(TEST161_FAIL, SECRET, "rwt2");
    }

    for (i = 0; i < NUMTHREADS; i++) {
        rwlock_release_read(rwlock);
    }

    rwlock_destroy(rwlock);
	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

    rwlock = rwlock_create("rwlock");
    if (rwlock == NULL) {
        panic("rwt3: error creating rwlock\n");
    }

    secprintf(SECRET, "Should panic...", "rwt3");
    rwlock_release_read(rwlock);

	/* Should not get here on success. */

	success(TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

    rwlock = rwlock_create("rwlock");
    if (rwlock == NULL) {
        panic("rwt4: error creating rwlock\n");
    }

    secprintf(SECRET, "Should panic...", "rwt4");
    rwlock_release_write(rwlock);

	/* Should not get here on success. */

	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args)
{
	(void)nargs;
	(void)args;

    rwlock = rwlock_create("rwlock");
    if (rwlock == NULL) {
        panic("rwt5: error creating rwlock\n");
    }

    secprintf(SECRET, "Should panic...", "rwt5");

    rwlock_acquire_write(rwlock);
    rwlock_destroy(rwlock);

	/* Should not get here on success. */

	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}

