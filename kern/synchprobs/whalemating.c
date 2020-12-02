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

struct lock *common_lock;
struct cv *male_cv;
struct cv *female_cv;
struct cv *match_cv;

volatile int num_males = 0;
volatile int num_females = 0;
volatile int num_matches = 0;
volatile int num_matchmakers = 0;



/*
 * Called by the driver during initialization.
 */

void whalemating_init() {
	
	if (common_lock == NULL) {
		common_lock = lock_create("common_lock");
		if (common_lock == NULL) {
			panic("common_lock creation failed!");
		}
	}

	if (male_cv == NULL) {
		male_cv = cv_create("male_cv");
		if (male_cv == NULL) {
			panic("male_cv creation failed!");
		}
	}

	if (female_cv == NULL) {
		female_cv = cv_create("female_cv");
		if (female_cv == NULL) {
			panic("female_cv creation failed!");
		}
	}

	if (match_cv == NULL) {
		match_cv = cv_create("match_cv");
		if (match_cv == NULL) {
			panic("match_cv creation failed!");
		}
	}

}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {

	lock_destroy(common_lock);

	cv_destroy(male_cv);
	cv_destroy(female_cv);
	cv_destroy(match_cv);

}

void
male(uint32_t index)
{
	(void)index;
	male_start(index);

	lock_acquire(common_lock);
	num_males++;

	if (num_females > 0 && num_matchmakers > 0) {
		cv_signal(female_cv, common_lock);
		num_females--;

		cv_signal(match_cv, common_lock);
		num_matchmakers--;
		num_males--;
	}
	else {
		cv_wait(male_cv, common_lock);
	}

	male_end(index);
	lock_release(common_lock);

	return;
}
void
female(uint32_t index)
{
	(void)index;

	female_start(index);

	lock_acquire(common_lock);
	num_females++;

	if (num_males > 0 && num_matchmakers > 0) {
		cv_signal(male_cv, common_lock);
		num_males--;

		cv_signal(match_cv, common_lock);
		num_matchmakers--;
		num_females--;
	}
	else {
		cv_wait(female_cv, common_lock);
	}

	female_end(index);
	lock_release(common_lock);
	return;
}

void
matchmaker(uint32_t index)
{
	(void)index;
	matchmaker_start(index);

	lock_acquire(common_lock);
	num_matchmakers++;

	if (num_males > 0 && num_males > 0) {
		cv_signal(male_cv, common_lock);
		num_males--;

		cv_signal(female_cv, common_lock);
		num_females--;
		num_matchmakers--;
	}
	else {
		cv_wait(match_cv, common_lock);
	}

	matchmaker_end(index);
	lock_release(common_lock);
	return;
}
