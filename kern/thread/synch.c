/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <cpu.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

    lock->lk_wchan = wchan_create(lock->lk_name);
	if (lock->lk_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

    spinlock_init(&lock->lk_spinlock);
    lock->lk_var = 0;
    lock->lk_holder = NULL;
	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);
    KASSERT(lock->lk_holder == NULL);

    spinlock_cleanup(&lock->lk_spinlock);
    wchan_destroy(lock->lk_wchan);
	kfree(lock->lk_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
    KASSERT(lock != NULL);

    struct thread *mythread;

    if (CURCPU_EXISTS()) {
        mythread = curcpu->c_curthread;
        if (lock->lk_holder == mythread) {
            panic("Deadlock on lock %p\n", lock);
        }
    }
    else {
        mythread = NULL;
    }

    spinlock_acquire(&lock->lk_spinlock);
    HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);
    while (lock->lk_var == 1) {
		wchan_sleep(lock->lk_wchan, &lock->lk_spinlock);
    }
    lock->lk_var = 1;
    lock->lk_holder = mythread;
    HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);
    spinlock_release(&lock->lk_spinlock);
}

void
lock_release(struct lock *lock)
{
    KASSERT(lock != NULL);

    if (CURCPU_EXISTS()) {
        KASSERT(lock->lk_holder == curcpu->c_curthread);
        KASSERT(lock->lk_var == 1);
    }

	spinlock_acquire(&lock->lk_spinlock);
    lock->lk_var = 0;
	KASSERT(lock->lk_var != 1);
	wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);
    lock->lk_holder = NULL;
    HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);
	spinlock_release(&lock->lk_spinlock);
}

bool
lock_do_i_hold(struct lock *lock)
{
    KASSERT(lock != NULL);

    if (!CURCPU_EXISTS()) {
        return true;
    }

	return lock->lk_holder == curcpu->c_curthread;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

    cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

    spinlock_init(&cv->cv_spinlock);
    cv->cv_lock = NULL;

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);
    KASSERT(cv->cv_lock == NULL);

    spinlock_cleanup(&cv->cv_spinlock);
    wchan_destroy(cv->cv_wchan);
	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    KASSERT(cv != NULL);
    KASSERT(lock != NULL);
    KASSERT(lock_do_i_hold(lock));

    spinlock_acquire(&cv->cv_spinlock);
    lock_release(lock);
    wchan_sleep(cv->cv_wchan, &cv->cv_spinlock);
    spinlock_release(&cv->cv_spinlock);
    lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    KASSERT(cv != NULL);
    KASSERT(lock != NULL);
    KASSERT(lock_do_i_hold(lock));

    spinlock_acquire(&cv->cv_spinlock);
    wchan_wakeone(cv->cv_wchan, &cv->cv_spinlock);
    spinlock_release(&cv->cv_spinlock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    KASSERT(cv != NULL);
    KASSERT(lock != NULL);
    KASSERT(lock_do_i_hold(lock));

    spinlock_acquire(&cv->cv_spinlock);
    wchan_wakeall(cv->cv_wchan, &cv->cv_spinlock);
    spinlock_release(&cv->cv_spinlock);
}

////////////////////////////////////////////////////////////
//
// Reader-writer lock.


struct rwlock *
rwlock_create(const char *name)
{
    struct rwlock *rwlock;

    rwlock = kmalloc(sizeof(*rwlock));
    if (rwlock == NULL) {
        return NULL;
    }

    rwlock->rwlock_name = kstrdup(name);
    if (rwlock->rwlock_name == NULL) {
        kfree(rwlock);
        return NULL;
    }

    rwlock->rwlock_lk = lock_create("rwlock_lk");
    if (rwlock->rwlock_lk == NULL) {
        kfree(rwlock->rwlock_name);
        kfree(rwlock);
        return NULL;
    }

    rwlock->rwlock_cv = cv_create("rwlock_cv");
    if (rwlock->rwlock_cv == NULL) {
        lock_destroy(rwlock->rwlock_lk);
        kfree(rwlock->rwlock_name);
        kfree(rwlock);
        return NULL;
    }

    rwlock->rwlock_numreaders = 0;
    rwlock->rwlock_wlocked = 0;

    return rwlock;
}

void
rwlock_destroy(struct rwlock *rwlock)
{
    KASSERT(rwlock != NULL);

    cv_destroy(rwlock->rwlock_cv);
    lock_destroy(rwlock->rwlock_lk);
    kfree(rwlock->rwlock_name);
    kfree(rwlock);
}

void
rwlock_acquire_read(struct rwlock *rwlock)
{
    KASSERT(rwlock != NULL);

    lock_acquire(rwlock->rwlock_lk);
    while (rwlock->rwlock_wlocked) {
        cv_wait(rwlock->rwlock_cv, rwlock->rwlock_lk);
    }
    rwlock->rwlock_numreaders += 1;
    lock_release(rwlock->rwlock_lk);
}

void
rwlock_release_read(struct rwlock *rwlock)
{
    KASSERT(rwlock != NULL);

    lock_acquire(rwlock->rwlock_lk);
    rwlock->rwlock_numreaders -= 1;
    if (rwlock->rwlock_numreaders == 0) {
        cv_signal(rwlock->rwlock_cv, rwlock->rwlock_lk);
    }
    lock_release(rwlock->rwlock_lk);
}

void
rwlock_acquire_write(struct rwlock *rwlock)
{
    KASSERT(rwlock != NULL);

    lock_acquire(rwlock->rwlock_lk);
    while (rwlock->rwlock_numreaders > 0 || rwlock->rwlock_wlocked) {
        cv_wait(rwlock->rwlock_cv, rwlock->rwlock_lk);
    }
    rwlock->rwlock_wlocked = 1;
    lock_release(rwlock->rwlock_lk);
}

void
rwlock_release_write(struct rwlock *rwlock)
{
    KASSERT(rwlock != NULL);

    lock_acquire(rwlock->rwlock_lk);
    rwlock->rwlock_wlocked = 0;
    if (rwlock->rwlock_numreaders > 0) {
        cv_broadcast(rwlock->rwlock_cv, rwlock->rwlock_lk);
    }
    lock_release(rwlock->rwlock_lk);
}
