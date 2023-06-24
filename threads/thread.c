#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif
#include "filesys/directory.h"

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* Project 1 */
static struct list sleep_list; // 1-1 Alarm clock

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4		  /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *)(pg_round_down(rrsp())))

// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void thread_init(void)
{
	ASSERT(intr_get_level() == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof(gdt) - 1,
		.address = (uint64_t)gdt};
	lgdt(&gdt_ds);

	/* Init the globla thread context */
	lock_init(&tid_lock);
	list_init(&sleep_list);
	list_init(&ready_list);
	list_init(&destruction_req);
	lock_init(&filesys_lock);
	

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();

	// 1-4 MLFQS init
	if (thread_mlfqs)
	{
		load_avg = 0;
		initial_thread->recent_cpu = 0;
		initial_thread->nice = 0;
		initial_thread->priority = PRI_MAX;
	}
	#ifdef EFILESYS
		initial_thread->cur_dir = NULL;
	#endif
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void thread_start(void)
{
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init(&idle_started, 0);
	thread_create("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void thread_tick(void)
{
	struct thread *t = thread_current();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return();
}

/* Prints thread statistics. */
void thread_print_stats(void)
{
	printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
		   idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority,
					thread_func *function, void *aux)
{
	struct thread *t;
	tid_t tid;

	ASSERT(function != NULL);

	/* Allocate thread. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread(t, name, priority);
	#ifdef EFILESYS

	

	#endif
	// 2-4 File descriptor
	//t->fdTable = palloc_get_page(PAL_ZERO); // multi-oom : need more pages to accomodate 10 stacks of 126 opens
	t->fdTable = palloc_get_multiple(PAL_ZERO, FDT_PAGES);
	if (t->fdTable == NULL)
		return TID_ERROR;
	t->fdIdx = 2; // 0 : stdin, 1 : stdout
	// 2-extra
	t->fdTable[0] = 1; // dummy values to distinguish fd 0 and 1 from NULL
	t->fdTable[1] = 2;
	t->stdin_count = 1;
	t->stdout_count = 1;

	// 1-4 MLFQS init
	if (thread_mlfqs)
	{
		t->recent_cpu = 0;
		t->nice = 0;
		t->priority = PRI_MAX; // priority = PRI_MAX – (recent_cpu / 4) – (nice * 2)
	}
	tid = t->tid = allocate_tid();
	if(thread_current()->cur_dir != NULL)
	{
			t->cur_dir = dir_reopen(thread_current()->cur_dir); //! ADD : 부모 디렉토리를 자식으로
	}
	t->is_load = 0;
	t->is_exit = 0;
	// 2-3 Parent child
	struct thread *cur = thread_current();
	list_push_back(&cur->child_list, &t->child_elem); // [parent] add new child to child_list

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t)kernel_thread;
	t->tf.R.rdi = (uint64_t)function;
	t->tf.R.rsi = (uint64_t)aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock(t);
	if (thread_current()->priority < t->priority)
	{
		thread_yield();
	}
	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void thread_block(void)
{
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);
	struct thread *curr = thread_current();
	curr->status = THREAD_BLOCKED;
	schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void thread_unblock(struct thread *t)
{
	enum intr_level old_level;

	ASSERT(is_thread(t));

	old_level = intr_disable();
	ASSERT(t->status == THREAD_BLOCKED);
	list_insert_ordered(&ready_list, &t->elem, prior_cmp, NULL); // 1-2
	t->status = THREAD_READY;
	intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name(void)
{
	return thread_current()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current(void)
{
	struct thread *t = running_thread();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void)
{
	return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit(void)
{
	ASSERT(!intr_context());

#ifdef USERPROG
	process_exit();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable();
	do_schedule(THREAD_DYING);
	NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void)
{
	ASSERT(!intr_context());
	struct thread *curr = thread_current();

	enum intr_level old_level = intr_disable();
	if (curr != idle_thread)
		list_insert_ordered(&ready_list, &curr->elem, prior_cmp, NULL); // 1-2
	do_schedule(THREAD_READY);
	intr_set_level(old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
// 1-2
void thread_set_priority(int new_priority)
{
	// 1-4
	ASSERT(!thread_mlfqs);

	// 1-3
	enum intr_level old_level = intr_disable();

	struct thread *curr = thread_current();
	curr->basePrior = new_priority;
	curr->priority = MAX(curr->basePrior, curr->donatedPrior);

	intr_set_level(old_level);

	if (list_size(&ready_list))
	{
		struct thread *cand = list_entry(list_front(&ready_list), struct thread, elem);
		if (new_priority < cand->priority)
		{
			thread_yield();
		}
	}
}

/* Returns the current thread's priority. */
int thread_get_priority(void)
{
	return thread_current()->priority;
}

// 1-4
// fixed-point representation multiplier
int f = 1 << 14;

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice UNUSED)
{
	thread_current()->nice = nice;
	thread_update_priority(thread_current()); // re-calculate priority with new nice
	struct thread *cand = list_entry(list_head(&ready_list), struct thread, elem);
	if (thread_get_priority() < cand->priority)
	{
		thread_yield();
	}
}

/* Returns the current thread's nice value. */
int thread_get_nice(void)
{
	return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void)
{
	return (load_avg * 100 + (f / 2)) / f;
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void)
{
	struct thread *t = thread_current();
	return t->recent_cpu >= 0 ? (t->recent_cpu * 100 + (f / 2)) / f
							  : (t->recent_cpu * 100 - (f / 2)) / f;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle(void *idle_started_ UNUSED)
{
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current();
	sema_up(idle_started);

	for (;;)
	{
		/* Let someone else run. */
		intr_disable();
		thread_block();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile("sti; hlt"
					 :
					 :
					 : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread(thread_func *function, void *aux)
{
	ASSERT(function != NULL);

	intr_enable(); /* The scheduler runs with interrupts off. */
	function(aux); /* Execute the thread function. */
	thread_exit(); /* If function() returns, kill the thread. */
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread(struct thread *t, const char *name, int priority)
{
	ASSERT(t != NULL);
	ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT(name != NULL);

	memset(t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	
	strlcpy(t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;

	// 1-3 Priority donation
	t->basePrior = priority;
	t->donatedPrior = -1;
	t->waiting_lock = NULL;
	list_init(&t->donors);

	// 2-3 Syscalls
	list_init(&t->child_list);
	sema_init(&t->wait_sema, 0);
	sema_init(&t->fork_sema, 0);
	sema_init(&t->free_sema, 0);
	list_init(&t->mmap_file_list);

	// 2-5
	t->running = NULL;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run(void)
{
	if (list_empty(&ready_list))
		return idle_thread;
	else
		return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void do_iret(struct intr_frame *tf)
{
	__asm __volatile(
		"movq %0, %%rsp\n"
		"movq 0(%%rsp),%%r15\n"
		"movq 8(%%rsp),%%r14\n"
		"movq 16(%%rsp),%%r13\n"
		"movq 24(%%rsp),%%r12\n"
		"movq 32(%%rsp),%%r11\n"
		"movq 40(%%rsp),%%r10\n"
		"movq 48(%%rsp),%%r9\n"
		"movq 56(%%rsp),%%r8\n"
		"movq 64(%%rsp),%%rsi\n"
		"movq 72(%%rsp),%%rdi\n"
		"movq 80(%%rsp),%%rbp\n"
		"movq 88(%%rsp),%%rdx\n"
		"movq 96(%%rsp),%%rcx\n"
		"movq 104(%%rsp),%%rbx\n"
		"movq 112(%%rsp),%%rax\n"
		"addq $120,%%rsp\n"
		"movw 8(%%rsp),%%ds\n"
		"movw (%%rsp),%%es\n"
		"addq $32, %%rsp\n"
		"iretq"
		:
		: "g"((uint64_t)tf)
		: "memory");
		
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch(struct thread *th)
{
	uint64_t tf_cur = (uint64_t)&running_thread()->tf;
	uint64_t tf = (uint64_t)&th->tf;
	ASSERT(intr_get_level() == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile(
		/* Store registers that will be used. */
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		/* Fetch input once */
		"movq %0, %%rax\n"
		"movq %1, %%rcx\n"
		"movq %%r15, 0(%%rax)\n"
		"movq %%r14, 8(%%rax)\n"
		"movq %%r13, 16(%%rax)\n"
		"movq %%r12, 24(%%rax)\n"
		"movq %%r11, 32(%%rax)\n"
		"movq %%r10, 40(%%rax)\n"
		"movq %%r9, 48(%%rax)\n"
		"movq %%r8, 56(%%rax)\n"
		"movq %%rsi, 64(%%rax)\n"
		"movq %%rdi, 72(%%rax)\n"
		"movq %%rbp, 80(%%rax)\n"
		"movq %%rdx, 88(%%rax)\n"
		"pop %%rbx\n" // Saved rcx
		"movq %%rbx, 96(%%rax)\n"
		"pop %%rbx\n" // Saved rbx
		"movq %%rbx, 104(%%rax)\n"
		"pop %%rbx\n" // Saved rax
		"movq %%rbx, 112(%%rax)\n"
		"addq $120, %%rax\n"
		"movw %%es, (%%rax)\n"
		"movw %%ds, 8(%%rax)\n"
		"addq $32, %%rax\n"
		"call __next\n" // read the current rip.
		"__next:\n"
		"pop %%rbx\n"
		"addq $(out_iret -  __next), %%rbx\n"
		"movq %%rbx, 0(%%rax)\n" // rip
		"movw %%cs, 8(%%rax)\n"	 // cs
		"pushfq\n"
		"popq %%rbx\n"
		"mov %%rbx, 16(%%rax)\n" // eflags
		"mov %%rsp, 24(%%rax)\n" // rsp
		"movw %%ss, 32(%%rax)\n"
		"mov %%rcx, %%rdi\n"
		"call do_iret\n"
		"out_iret:\n"
		:
		: "g"(tf_cur), "g"(tf)
		: "memory");
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(thread_current()->status == THREAD_RUNNING);

	while (!list_empty(&destruction_req))
	{
		struct thread *victim =
			list_entry(list_pop_front(&destruction_req), struct thread, elem);

		palloc_free_page(victim); // Project 2-3. Will be freed in 'process_wait'
	}
	thread_current()->status = status;
	schedule();
}

static void
schedule(void)
{
	struct thread *curr = running_thread();
	struct thread *next = next_thread_to_run();

	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(curr->status != THREAD_RUNNING);
	ASSERT(is_thread(next));

	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate(next);
#endif

	if (curr != next)
	{
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used bye the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread)
		{
			ASSERT(curr != next);
			list_push_back(&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch(next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid(void)
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire(&tid_lock);
	tid = next_tid++;
	lock_release(&tid_lock);

	return tid;
}

/* Project 1 */
// 1-1 Alarm clock
// comparator; sort by decreasing priority
bool prior_cmp(const struct list_elem *a, const struct list_elem *b, void *aux)
{
	struct thread *thA = list_entry(a, struct thread, elem);
	struct thread *thB = list_entry(b, struct thread, elem);
	return thA->priority > thB->priority;
};

// comparator; sort by increasing endTick and decreasing priority
bool endTick_prior_cmp(const struct list_elem *a, const struct list_elem *b, void *aux)
{
	struct thread *thA = list_entry(a, struct thread, elem);
	struct thread *thB = list_entry(b, struct thread, elem);
	if (thA->endTick == thB->endTick)
	{
		return thA->priority > thB->priority;
	}
	return thA->endTick < thB->endTick;
};

// 1-1 Block current thread, insert_ordered into sleep list - O(N)
void sleep()
{
	struct thread *curr = thread_current();
	enum intr_level old_level;

	//ASSERT(!intr_context());
	ASSERT(curr->status == THREAD_RUNNING);

	old_level = intr_disable();
	if (curr != idle_thread)
		list_insert_ordered(&sleep_list, &curr->elem, endTick_prior_cmp, NULL);
	thread_block();
	intr_set_level(old_level);
}

// 1-1 Wake-up front thread in sleep list (sorted) and return next minEndTick - O(1)
int64_t wake_up()
{
	//ASSERT(intr_context()); // wake_up should've been called by timer_interrupt
	ASSERT(intr_get_level() == INTR_OFF);

	// Unblock and remove target from sleep_list
	struct thread *target;
	target = list_entry(list_pop_front(&sleep_list), struct thread, elem); // remove from 'sleep_list'
	thread_unblock(target);												   // unblock and add to 'ready_list'
	target->endTick = -1;

	// 1-2 Q. How to preempt after waking thread up?
	// if (thread_current()->priority < target->priority){
	// 	thread_yield();
	// }

	// Get new minEndTick and return (-1 if no more threads sleeping)
	if (list_empty(&sleep_list))
		return -1;
	else
	{
		struct thread *th = list_entry(list_front(&sleep_list), struct thread, elem);
		return th->endTick;
	}
}

// 1-3
// Start from thread 't', donate 'new_prior' down the nested lock
void donateNested(struct thread *t, int new_prior)
{
	if (t->waiting_lock == NULL || t->waiting_lock == 0)
	{
		list_sort(&ready_list, prior_cmp, NULL);
		return;
	}

	struct thread *nxt = t->waiting_lock->holder; // next nested thread to donate
	if (nxt->priority < new_prior)
	{
		nxt->donatedPrior = new_prior;
		nxt->priority = MAX(nxt->basePrior, nxt->donatedPrior);
		donateNested(nxt, new_prior);
	}
	// if nested thread with higher donatedPrior met, return
	// Because that thread should've donated higher priority down already
}

// check any donor threads for current thread and find max donation
// if donors list is empty, then init val -1 is set
void donateMultiple(struct thread *curr)
{
	int maxDonation = -1;

	if (!list_empty(&curr->donors))
	{
		// prior_cmp sorts by 'decreasing' priority, so use 'list_min', not 'list_max'
		struct list_elem *de = list_min(&curr->donors, prior_cmp, NULL);
		struct thread *t = list_entry(de, struct thread, d_elem);
		maxDonation = t->priority;
	}

	curr->donatedPrior = maxDonation;
	curr->priority = MAX(curr->basePrior, curr->donatedPrior);
}

// 1-4 Advanced Scheduler
// recent_cpu and load_avg values are stored in 17.14 fixed-point format
// update every thread's recent_cpu
void total_update_recentcpu()
{
	enum intr_level old_level = intr_disable();

	struct thread *t = thread_current();
	thread_update_recentcpu(t);

	struct list_elem *e;
	for (e = list_begin(&ready_list); e != list_end(&ready_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		thread_update_recentcpu(t);
	}

	for (e = list_begin(&sleep_list); e != list_end(&sleep_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		thread_update_recentcpu(t);
	}

	intr_set_level(old_level);
}

// update single thread's recent_cpu
void thread_update_recentcpu(struct thread *t)
{
	int nom = 2 * load_avg;
	int denom = nom + f;
	int decay = (long long)nom * f / denom;

	t->recent_cpu = (long long)(t->recent_cpu) * decay / f + (t->nice * f);
}

// update load_avg value
void update_load_avg()
{
	struct thread *t = thread_current();
	int ready_threads = list_size(&ready_list) + (t != idle_thread ? 1 : 0);

	// 59/60 are rounded to zero when stored to int
	// Change coeff to fixed-pt rep
	int coeff1 = (59 * f) / 60;
	int coeff2 = f / 60;
	load_avg = ((long long)coeff1 * load_avg / f) + ((long long)coeff2 * (ready_threads * f) / f);
	// perform fixed-pt multiplication with coeffs
}

// update every thread's priority. This function should be called just before thread_tick so we don't need to consider preemption
void total_update_priority()
{
	enum intr_level old_level = intr_disable();

	struct thread *t = thread_current();
	thread_update_priority(t);

	struct list_elem *e;
	for (e = list_begin(&ready_list); e != list_end(&ready_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		thread_update_priority(t);
	}
	// sorting ready_list again -> is list.sort preserves round-robin order?
	list_sort(&ready_list, prior_cmp, NULL);

	for (e = list_begin(&sleep_list); e != list_end(&sleep_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		thread_update_priority(t);
	}
	// sorting sleep_list again
	list_sort(&sleep_list, endTick_prior_cmp, NULL);

	intr_set_level(old_level);
}

// update single thread's priority
void thread_update_priority(struct thread *t)
{
	// Change recent_cpu/4 to integer
	int recent = t->recent_cpu / 4;
	recent = recent >= 0 ? (recent + (f / 2)) / f
						 : (recent - (f / 2)) / f;

	t->priority = PRI_MAX - recent - (t->nice * 2);
}
