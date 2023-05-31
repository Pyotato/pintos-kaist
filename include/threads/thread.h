<<<<<<< HEAD
#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int priority;			   /* Priority. */
	int64_t wakeup_tick;	   /* tick till wake up. 해당 쓰레드가 깨어나야 할 tick을 저장할 필드 */

	/* For advanced scheduler */
	int nice; /* Niceness of the thread*/
	/*
	  최근에 얼마나 많은 CPU time을 사용했는가를 표현
	 init 스레드의 초기 값은 ‘0’, 다른 스레드들은 부모의 recent_cpu값
	 recent_cpu는 timer interrupt마다 1씩 증가, 매 1초 마다 재 계산
	 int thread_get_recent_cpu(void) 함수 구현
	 스레드의 현재 recent_cpu의 100배 (rounded to the nearest interget) 를 반환
	*/
	int recent_cpu;

	/* Shared between thread.c and synch.c. */
	struct list_elem elem; /* List element. */
	/*
	 * Priority donation 구현
	 * donation 이후 우선순위를 초기화하기 위해 초기 우선순위 값을 저장할 필드
    * 해당 리스트를 위한 elem도 추가
    * 해당 쓰레드가 대기하고 있는 lock자료구조의 주소를 저장할 필드
    * multiple donation을 고려하기 위한 리스트 추가

	*/
	struct list donations;	   /*multiple donation을 고려하기 위한 리스트 추가 (priority들을 담을 리스트)*/
	struct list_elem d_elem;   /*해당 리스트를 위한 elem도 추가*/
	struct lock *wait_on_lock; /*해당 쓰레드가 대기하고 있는 lock자료구조의 주소를 저장할 필드*/
	int origin_priority;	   /*이전의 priority, Multiple donation:스레드가 두 개 이상의 lock 보유 시, 각 lock에 의해 도네이션이 발생가
능  이전 상태의 우선순위를 기억하고 있어야 함*/

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

extern int64_t global_tick;
/*
load_avg = (59/60) * load_avg + (1/60) * ready_threads
 최근 1분 동안 수행 가능한 프로세스의 평균 개수, exponentially weighted
moving average 를 사용
 ready_threads : ready_list에 있는 스레드들과 실행 중인 스레드의 개수 (idle
스레드 제외)
 int thread_get_load_avg(void) 함수 구현
 현재 system load average의 100배 (rounded to the nearest interget) 를 반환
 timer_ticks() % TIMER_FREQ == 0

*/
extern int load_avg;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

void thread_release_unlock(void);
int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void); /*mlfqs_recent_cpu*/
int thread_get_load_avg(void);	 /*mlfqs_load_avg*/

void do_iret(struct intr_frame *tf);

void thread_sleep(int64_t ticks);
void wakeup_thread(void);

bool order_by_least_wakeup_tick(const struct list_elem *a, const struct list_elem *b,
								void *aux UNUSED);
bool priority_greatest_function(const struct list_elem *a, const struct list_elem *b,
								void *aux UNUSED);

void calculate_load_avg(void);
void calculate_recent_cpu(void);
void recalc_priority(void);

=======
#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int priority;			   /* Priority. */
	int64_t wakeup_tick;	   /* tick till wake up. 해당 쓰레드가 깨어나야 할 tick을 저장할 필드 */

	/* For advanced scheduler */
	int nice; /* Niceness of the thread*/
	/*
	  최근에 얼마나 많은 CPU time을 사용했는가를 표현
	 init 스레드의 초기 값은 ‘0’, 다른 스레드들은 부모의 recent_cpu값
	 recent_cpu는 timer interrupt마다 1씩 증가, 매 1초 마다 재 계산
	 int thread_get_recent_cpu(void) 함수 구현
	 스레드의 현재 recent_cpu의 100배 (rounded to the nearest interget) 를 반환
	*/
	int recent_cpu;

	/* Shared between thread.c and synch.c. */
	struct list_elem elem; /* List element. */
	/*
	 * Priority donation 구현
	 * donation 이후 우선순위를 초기화하기 위해 초기 우선순위 값을 저장할 필드
    * 해당 리스트를 위한 elem도 추가
    * 해당 쓰레드가 대기하고 있는 lock자료구조의 주소를 저장할 필드
    * multiple donation을 고려하기 위한 리스트 추가

	*/
	struct list donations;	   /*multiple donation을 고려하기 위한 리스트 추가 (priority들을 담을 리스트)*/
	struct list_elem d_elem;   /*해당 리스트를 위한 elem도 추가*/
	struct lock *wait_on_lock; /*해당 쓰레드가 대기하고 있는 lock자료구조의 주소를 저장할 필드*/
	int origin_priority;	   /*이전의 priority, Multiple donation:스레드가 두 개 이상의 lock 보유 시, 각 lock에 의해 도네이션이 발생가
능  이전 상태의 우선순위를 기억하고 있어야 함*/

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

extern int64_t global_tick;
/*
load_avg = (59/60) * load_avg + (1/60) * ready_threads
 최근 1분 동안 수행 가능한 프로세스의 평균 개수, exponentially weighted
moving average 를 사용
 ready_threads : ready_list에 있는 스레드들과 실행 중인 스레드의 개수 (idle
스레드 제외)
 int thread_get_load_avg(void) 함수 구현
 현재 system load average의 100배 (rounded to the nearest interget) 를 반환
 timer_ticks() % TIMER_FREQ == 0

*/
extern int load_avg;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

void thread_release_unlock(void);
int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void); /*mlfqs_recent_cpu*/
int thread_get_load_avg(void);	 /*mlfqs_load_avg*/

void do_iret(struct intr_frame *tf);

void thread_sleep(int64_t ticks);
void wakeup_thread(void);

bool order_by_least_wakeup_tick(const struct list_elem *a, const struct list_elem *b,
								void *aux UNUSED);
bool priority_greatest_function(const struct list_elem *a, const struct list_elem *b,
								void *aux UNUSED);

void calculate_load_avg(void);
void calculate_recent_cpu(void);
void recalc_priority(void);

>>>>>>> 17b9e9244acceb84ebf9244258d7f3ea4631f226
#endif /* threads/thread.h */