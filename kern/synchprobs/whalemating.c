/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

int whalecount;
struct lock *lk_male;
struct lock *lk_female;
struct lock *lk_mm;
struct lock *lk_ready;
struct cv *cv_ready;

/*
 * Called by the driver during initialization.
 */

void whalemating_init() {
    lk_male = lock_create("male");
    lk_female = lock_create("female");
    lk_mm = lock_create("matchmaker");
    lk_ready = lock_create("ready");
    cv_ready = cv_create("cv");
    whalecount = 0;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
    lock_destroy(lk_male);
    lock_destroy(lk_female);
    lock_destroy(lk_mm);
    lock_destroy(lk_ready);
    cv_destroy(cv_ready);
}

void
male(uint32_t index)
{
    male_start(index);
    lock_acquire(lk_male);

    lock_acquire(lk_ready);
    whalecount += 1;
    if (whalecount != 3) {
        cv_wait(cv_ready, lk_ready);
    } else {
        cv_broadcast(cv_ready, lk_ready);
    }

    whalecount -= 1;
    lock_release(lk_ready);

    male_end(index);
    lock_release(lk_male);
}

void
female(uint32_t index)
{
    female_start(index);
    lock_acquire(lk_female);

    lock_acquire(lk_ready);
    whalecount += 1;

    if (whalecount != 3) {
        cv_wait(cv_ready, lk_ready);
    } else {
        cv_broadcast(cv_ready, lk_ready);
    }

    whalecount -= 1;
    lock_release(lk_ready);

    female_end(index);
    lock_release(lk_female);
}

void
matchmaker(uint32_t index)
{
    matchmaker_start(index);
    lock_acquire(lk_mm);

    lock_acquire(lk_ready);
    whalecount += 1;

    if (whalecount != 3) {
        cv_wait(cv_ready, lk_ready);
    } else {
        cv_broadcast(cv_ready, lk_ready);
    }

    whalecount -= 1;
    lock_release(lk_ready);

    matchmaker_end(index);
    lock_release(lk_mm);
}
