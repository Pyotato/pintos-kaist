#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
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

	/* Project 1 */
	int endTick; // 1-1 Alarm clock

	// 1-3 Priority donation
	int basePrior, donatedPrior;
	struct lock *waiting_lock; // 1-3 lock waiting for (nested-donation)
	struct list donors;		   // 1-3 remember the donors (multiple-donation)
	struct list_elem d_elem;   //1-3 used to put thread into 'donors' list

	// 1-4 MLFQS
	int nice;
	int recent_cpu;

	/* Project 2 */
	// 2-3 Parent-child hierarchy
	struct list child_list;		 // keep children
	struct list_elem child_elem; // used to put current thread into 'children' list
	// 2-3 wait syscall
	struct semaphore wait_sema; // used by parent to wait for child
	int exit_status;			// used to deliver child exit_status to parent
	// 2-3 fork syscall
	struct intr_frame parent_if; // to preserve my current intr_frame and pass it down to child in fork ('parent_if' in child's perspective)
	struct semaphore fork_sema;	 // parent wait (process_wait) until child fork completes (__do_fork)
	struct semaphore free_sema;	 // Postpone child termination (process_exit) until parent receives its exit_status in 'wait' (process_wait)
	// 2-4 file descripter
	struct file **fdTable; // allocation in threac_create (thread.c)
	int fdIdx;			   // an index of an open spot in fdTable
	// 2-5 deny exec writes
	struct file *running; // executable ran by current process (process.c load, process_exit)
	// 2-extra - count the number of open stdin/stdout
	// dup2 may copy stdin or stdout; stdin or stdout is not really closed until these counts goes 0
	int stdin_count;
	int stdout_count;

	/* Shared between thread.c and synch.c. */
	struct list_elem elem; // used to put thread into 'ready_list' or sync blocked_list
	struct dir* cur_dir;
	int is_load;
	int is_exit;
	int is_fork;
#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif
struct list mmap_file_list;
/* saving stack top point */
	uintptr_t saved_sp;
	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

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

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

/* Project 1 */
// 1-1 Alarm clock
bool prior_cmp(const struct list_elem *a, const struct list_elem *b, void *aux);
bool endTick_prior_cmp(const struct list_elem *a, const struct list_elem *b, void *aux);
void sleep(void);	   // 1-1 Alarm clock
int64_t wake_up(void); // 1-1 Alarm clock
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// 1-3 Priority donation
void donateNested(struct thread *t, int new_prior); // start from thread newly added to the end of nested lock
void donateMultiple(struct thread *curr);			// start from core thread getting donation (search through list 'donor')

// 1-4 Advanced scheduler
void total_update_recentcpu();
void thread_update_recentcpu(struct thread *t);
void update_load_avg();
void total_update_priority();
void thread_update_priority(struct thread *t);
int load_avg;

// 2-4 syscall - fork
#define FDT_PAGES 3					  // pages to allocate for file descriptor tables (thread_create, process_exit)
#define FDCOUNT_LIMIT FDT_PAGES *(1 << 9) // Limit fdIdx

#endif /* threads/thread.h */
