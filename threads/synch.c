/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* list_insert_ordered에서 priority 높은 sema 순으로 정렬

*/

int find_max_priority(struct list *list);
void remove_released_thread(struct lock *lock);
void nested_donation(struct lock *lock, int priority);
bool priority_greatest_sema(const struct list_elem *a, const struct list_elem *b,
							void *aux UNUSED);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void sema_init(struct semaphore *sema, unsigned value)
{
	ASSERT(sema != NULL);

	sema->value = value;
	list_init(&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void sema_down(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);
	ASSERT(!intr_context());

	old_level = intr_disable();
	while (sema->value == 0)
	{ /*  2️⃣priority-sceduling-sync
		  sema_down()에서 waiters list를 우선수위로 정렬 하도록 수정
		  (Semaphore를 얻고 waiters 리스트 삽입 시, 우선순위대로 삽입되도록 수정)
	  */
		// list_push_back(&sema->waiters, &thread_current()->elem);
		// list_insert_ordered(&sema->waiters, &thread_current()->elem, priority_greatest_function, NULL);
		list_insert_ordered(&sema->waiters, &thread_current()->elem, priority_greatest_sema, NULL);
		thread_block();
	}
	sema->value--;
	intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema)
{
	enum intr_level old_level;
	bool success;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level(old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void sema_up(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	/*priority 높은 순으로 정렬
	 1️⃣waiter list에 있는 쓰레드의 우선순위가 변경 되었을 경우를 고려하
		여 	waiter list를 정렬 (list_sort)
 	 2️⃣세마포어 해제 후 priority preemption 기능 추가

	*/
	list_sort(&sema->waiters, priority_greatest_function, NULL); /*1️⃣*/
	if (!list_empty(&sema->waiters))
		thread_unblock(list_entry(list_pop_front(&sema->waiters),
								  struct thread, elem));
	sema->value++;
	intr_set_level(old_level);
	thread_release_unlock(); /*2️⃣ */
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void)
{
	struct semaphore sema[2];
	int i;

	printf("Testing semaphores...");
	sema_init(&sema[0], 0);
	sema_init(&sema[1], 0);
	thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up(&sema[0]);
		sema_down(&sema[1]);
	}
	printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper(void *sema_)
{
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down(&sema[0]);
		sema_up(&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init(struct lock *lock)
{
	ASSERT(lock != NULL);

	lock->holder = NULL;
	sema_init(&lock->semaphore, 1);
}

/*2️⃣ nested donation
lock을 갖고 있는 쓰레드보다 현재 thread priority가 높다면 대기중인 모든 쓰레드들의 priority를 현재 꺼로 올려줘야함
재귀적으로 lock wait하고 있는 thread들의 우선 순위 높여주기
*/
void nested_donation(struct lock *lock, int priority)
{
	struct thread *holder = lock->holder;
	/*lock holder가 있다면 */
	if (holder != NULL)
	{ /*lock을 갖고 있는 thread의 priority가 lock aquire하려는 thread보다 낮다면 lock aquire하려는 thread의 priority 갖도록*/
		if (holder->priority < priority)
			holder->priority = priority;
		if (holder->wait_on_lock != NULL)
			return nested_donation(holder->wait_on_lock, priority);
	}
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void lock_acquire(struct lock *lock)
{
	/*현재 thread의 priority*/
	int curr_priority = thread_get_priority();

	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(!lock_held_by_current_thread(lock));
	/* round-robin 사용시 */
	if (thread_mlfqs == false)
	{ /* lock을 갖고 있는 thread가 있다면 */
		if (lock->holder != NULL)
		{												/*
													  lock을 점유하고 있는 스레드와 요청 하는 스레드의 우선순위를 비교하여
													  priority donation을 수행하도록 수정
													  */
			thread_current()->wait_on_lock = lock;		/*wait을 하게 될 lock 자료구조 포인터 저장:현재 쓰레드가 요청히고 있는 lock*/
			if (lock->holder->priority < curr_priority) /*현재 쓰레드의 우선순위가 lock을 보유한 thread보다 우선순위가 높다 ? priority inversion 해결하기!*/
				nested_donation(lock, curr_priority);	/*가장 높은 우선 순위를 wait 중인 thread들한테 주기*/
			/*donation*/
			list_push_back(&lock->holder->donations, &thread_current()->d_elem);
		}
	}

	sema_down(&lock->semaphore);
	lock->holder = thread_current();
	thread_current()->wait_on_lock = NULL;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock *lock)
{
	bool success;

	ASSERT(lock != NULL);
	ASSERT(!lock_held_by_current_thread(lock));

	success = sema_try_down(&lock->semaphore);
	if (success)
		lock->holder = thread_current();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void lock_release(struct lock *lock)
{
	ASSERT(lock != NULL);
	ASSERT(lock_held_by_current_thread(lock));

	if (thread_mlfqs == false)
	{
		int holder_priority;
		int donor_priority;
		holder_priority = lock->holder->origin_priority;
		/* donation list 에서 스레드를 제거하고
		우선순위를 다시 계산하도록 remove_with_lock():donation list에서 스레드 엔트리를 제거 *
		, refresh_priority():우선순위를 다시 계산 함수를 호출 */

		if (!list_empty(&lock->holder->donations))
		{
			/*donation list에서 스레드 엔트리를 제거*/
			remove_released_thread(lock);
			/* priority를 다시 계산 함수를 호출
				lock holder의 priority 무조건 최대로하기
			*/
			/*multiple donation*/
			donor_priority = find_max_priority(&lock->holder->donations);
			if (donor_priority > holder_priority) /*lock들고 있는 thread의 priority가 크다면 그값을 갖고 아니면 donate 전 값이 높다면 그 값으로 */
				lock->holder->priority = donor_priority;
			else
				lock->holder->priority = holder_priority;
		}
		/*lock 대기 중인 thread 있음*/
		if (lock->holder->wait_on_lock != NULL)
		{ /*thread priority 물려주기*/
			nested_donation(lock->holder->wait_on_lock, lock->holder->priority);
		}
	}

	lock->holder = NULL;
	sema_up(&lock->semaphore);
}
/*release 된 thread 제거*/
void remove_released_thread(struct lock *lock)
{
	struct list_elem *e;

	struct list *list = &lock->holder->donations;

	for (e = list_begin(list); e != list_end(list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, d_elem);
		if (t->wait_on_lock == lock)
		{
			list_remove(&t->d_elem);
		}
	}
}

/*2️⃣ max priority 찾기*/
int find_max_priority(struct list *list)
{
	struct list_elem *elem;
	int max = 0;
	/*리스트 순회하면서 d_element 값이 크면 겂을 max로 */
	for (elem = list_begin(list); elem != list_end(list); elem = list_next(elem))
	{
		struct thread *t = list_entry(elem, struct thread, d_elem);
		if (t->priority > max)
			max = t->priority;
	}
	return max;
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock)
{
	ASSERT(lock != NULL);

	return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem
{
	struct list_elem elem;		/* List element. */
	struct semaphore semaphore; /* This semaphore. */
	int priority
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition *cond)
{
	ASSERT(cond != NULL);

	list_init(&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.
   👀 원자적으로 (원자가 작은 단위라서 더이상 쪼개지지 않는 거처럼)
   lock이 release 되고 다른 cond signal을 대기했다가 signal (wake)이 오면
   lock은 return 전에 reacquire됨
   🔥 lock 은 반드시 release & reaqcuoire 해야함!

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.
   🔥 여기서 구현된 monitor 스타일은 signal들을 주고 받는 연산들이
   원자적이지 않음! 따라서 wait을 마칠 때마다 condition을 재확인해줘야함

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.
   🔥 condition variable은 lock과 多:1 관계다.
   lock은 여러개의 condition variable이 있을 수 있다.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void cond_wait(struct condition *cond, struct lock *lock)
{
	struct semaphore_elem waiter;

	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));

	sema_init(&waiter.semaphore, 0);
	/*waiter의 priority 가 현재 thread의 priority
	Priority Scheduling-Synchronization 구현
	condition variable의 waiters list에 우선순위 순서로 삽입되도록 수정
	*/
	waiter.priority = thread_get_priority();
	/* wait 중인 thread 리스트 priority 순서대로 정렬*/
	list_insert_ordered(&cond->waiters, &waiter.elem, priority_greatest_sema, NULL);
	/*signal을 전송받기 위해서 일단 lock release*/
	lock_release(lock);
	/*condition variable의 waiters list를 우선순위로 재정렬
 	대기 중에 우선순위가 변경되었을 가능성이 있음으로
	wait하고 있는 세마포어들 priority 순서대로 정렬하고 다운*/
	sema_down(&waiter.semaphore);
	/*다시 lock*/
	lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));
	/*cond list의 wait 중인 쓰레드가 있다면  */
	if (!list_empty(&cond->waiters))
		/*
		condition variable에서 기다리는 가장높은 우선순위의 스레드에 signal을 보냄
		cond_wait에서 정렬해줬던 우선순위 높은 세마들을 wake해주기*/
		sema_up(&list_entry(list_pop_front(&cond->waiters),
							struct semaphore_elem, elem)
					 ->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	/*
	condition variable wait list가 비지 않으면 모든 wait 중인 thread signal해주기
	condition variable에서 기다리는 모든 스레드에 signal을 보냄
	*/
	while (!list_empty(&cond->waiters))
		cond_signal(cond, lock);
}

/*
한양대ver:bool cmp_sem_priority (const struct list_elem *a, const struct list_elem *b, void *aux
UNUSED)
첫 번째 인자의 우선순위가 높으면 1을 반환, 두 번째 인자의 우선순위가 높으면 0을
반환

 */
bool priority_greatest_sema(const struct list_elem *a, const struct list_elem *b,
							void *aux UNUSED)
{ /*semaphore_elem으로부터 각 semaphore_elem의 쓰레드 디스크립터를 획득.*/
	int priority_a = list_entry(a, struct semaphore_elem, elem)->priority;
	int priority_b = list_entry(b, struct semaphore_elem, elem)->priority;

	return priority_a > priority_b;
}
