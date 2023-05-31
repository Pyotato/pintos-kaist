#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore
{
	unsigned value;		 /* Current value. */
	struct list waiters; /* List of waiting threads. */
};

void sema_init(struct semaphore *, unsigned value);
void sema_down(struct semaphore *);
bool sema_try_down(struct semaphore *);
void sema_up(struct semaphore *);
void sema_self_test(void);

/* Lock. */
struct lock
{
	struct thread *holder;		/* Thread holding lock (for debugging). */
	struct semaphore semaphore; /* Binary semaphore controlling access. */
};

void lock_init(struct lock *);
void lock_acquire(struct lock *);
bool lock_try_acquire(struct lock *);
void lock_release(struct lock *);
bool lock_held_by_current_thread(const struct lock *);

/* Condition variable. (conditional wait variables)

* 👀 condition variable ? (Conditional Wait variable)
Allows you to take one thread off the scheduling queue
untill there is a signal from another thread that it should be back on

this is useful when we don't want one thread to continue it's work
until the work of another thread is done
************************************ Example ************************************

* when using conditional variable we need 3 things

int is_done;  //variable indicating work is done
mutex_thread done_lock; //lock that we can apply to this variable
cond_t done_cond;

t2 wants to stop until t1's work is done

			t1					  								| 			t2
------------------------------------------------------------------------------------------------------------------------------
//after finishing the work										|  //stop here until work done
mutex_loc(&done_lock); //t1 acquires lock						|  mutex_loc(&done_lock); //acquire lock
is_done = 1; 		//change is_done to true					| if(!is_done) //check to see if work is done, if it is not done (NEED THIS SO THE SIGNAL COMES AFTER THE WORK IS DONE)
//cond_signal(&done_cond);										| 		cond_wait(&done_cond, &done_lock); //waits & unlocks the mutex
//send wakeup signal to anyone waiting on this condition 		|  	//need to unlock the mutex so that it can let A send a signal
//mutex_unlock(&done_lock);	//unlock the lock				 	|  mutex_unlock(&done_lock)	//once

*


*/
struct condition
{
	struct list waiters; /* List of waiting threads. */
};

void cond_init(struct condition *);
void cond_wait(struct condition *, struct lock *);
void cond_signal(struct condition *, struct lock *);
void cond_broadcast(struct condition *, struct lock *);

/* Optimization barrier.
 *
 * The compiler will not reorder operations across an
 * optimization barrier.  See "Optimization Barriers" in the
 * reference guide for more information.*/
#define barrier() asm volatile("" \
							   :  \
							   :  \
							   : "memory")

#endif /* threads/synch.h */
