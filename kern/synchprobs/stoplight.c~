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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

void get_lt_points(int, int *);
void get_straight_points(int, int *);

struct semaphore *sem_stoplight;
struct lock *lk[4];

char *name;
char *message;

void
get_lt_points(int direction, int * points) {

	(void)direction;

	points[0] = direction;
	points[1] = (direction + 3) % 4;
	points[2] = (direction + 2) % 4;

}

void
get_straight_points(int direction, int * points) {

	(void)direction;

	// A car entering from X direction will pass through points (x), ((x+3) % 4)
	points[0] = direction;
	points[1] = (direction + 3) % 4;
	
}


/*
 * Called by the driver during initialization.
 */

void
stoplight_init() {

	sem_stoplight = sem_create("sem_stoplight", 3);
	if (sem_stoplight == NULL) {
		panic("sem_stoplight creation failed!");
	}

	name = kmalloc(4);
	message = kmalloc(32);

	for (int i = 0; i < 4; i++) {
		lk[i] = kmalloc(sizeof(struct lock));
		
		name = kstrdup("lk_");
		name = strcat(name, (char*)&i);
		message = strcat(name, " creation failed!");

		lk[i] = lock_create(name);
		if (lk[i] == NULL) {
			panic(message);
		}


	}

	if (lk == NULL) {
		panic("lk creation failed!");
	}

	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {

	sem_destroy(sem_stoplight);

	for (int i = 0; i < 4; i++) {
		lock_destroy(lk[i]);
	}

	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	(void)direction;
	(void)index;

	int tp = direction;
	
	P(sem_stoplight);
	
	lock_acquire(lk[tp]);

	inQuadrant(tp, index);
	leaveIntersection(index);
	
	lock_release(lk[tp]);

	V(sem_stoplight);

	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	(void)direction;
	(void)index;

	int *tp = kmalloc(sizeof(int) * 2);

	get_straight_points(direction, tp);

	P(sem_stoplight);

	// Wait until first position is available
	lock_acquire(lk[tp[0]]);
	
	// Move the car into the space
	inQuadrant(tp[0], index);

	// Wait until 2nd position is available
	lock_acquire(lk[tp[1]]);
	
	// Free 1st position and move the car into the 2nd position
	inQuadrant(tp[1], index);
	lock_release(lk[tp[0]]); 

	// Leave the intersection and free the space
	leaveIntersection(index);
	lock_release(lk[tp[1]]);
	
	V(sem_stoplight);

	kfree(tp);

	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	(void)direction;
	(void)index;

	int *tp = kmalloc(sizeof(int) * 3);

	get_lt_points(direction, tp);
	
	P(sem_stoplight);

	// Wait until first position is available
	lock_acquire(lk[tp[0]]);

	// Move the car into the space
	inQuadrant(tp[0], index);

	// Wait until 2nd position is available
	lock_acquire(lk[tp[1]]);
	
	// Free 1st position and move the car into the 2nd position
	inQuadrant(tp[1], index);
	lock_release(lk[tp[0]]); 
	
	// Wait until 3rd position is available
	lock_acquire(lk[tp[2]]);
	
	// Free 2nd position and move the car into the 3rd position
	inQuadrant(tp[2], index);
	lock_release(lk[tp[1]]); 

	// Leave the intersection and free the space
	leaveIntersection(index);
	lock_release(lk[tp[2]]);

	V(sem_stoplight);

	kfree(tp);
	
	return;
}
