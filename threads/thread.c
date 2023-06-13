#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "lib/kernel/list.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed_point.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#include "userprog/syscall.h"
#endif

#define IDLE_TID 2
#define MAIN_TID 1
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

/*
*  1️⃣ ⏰ ALARM 과제:✅ sleep queue 구조체 선언
* 초기의 pintOS는 busy-waiting 방식으로 lock이 걸려있어도 대기 없이 계속 자원사용가능한 지 요청했었음
* ALARM에서는 mutex-blocking 하도록 개선
List of processes that go to sleep for sleep/wakeup-based alarm clock*/
static struct list sleep_list;

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

/* 1️⃣ ⏰ ALARM 과제: ✅ 글로벌 tick 저장 변수 선언, global_tick = 로컬tick 중에서 가장 작은 값이므로 long long int 단위 최대값으로 초기화해주기*/
int64_t global_tick;

/*  3️⃣4BSD SCHEDULING: ✅ */
int load_avg;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);
/*🧑‍💻project2*/
static void close_all_file(struct thread *);
/**
 * 2️⃣3️⃣
 * list_less_function for ready_list_insert_ordered.
Inserting thread in order by larger priority values.
*/
/* tick이 적은 순대로 오름차순 정렬, thread_sleep에서 가장 tick작은 값부터 */
// bool order_by_least_wakeup_tick(const struct list_elem *a, const struct list_elem *b,
// 								void *aux UNUSED);

// bool priority_greatest_function(const struct list_elem *a, const struct list_elem *b,
// 								void *aux UNUSED);

/**
 * 3️⃣ 4BSD LIKE SCHEDULER (Multi-Level Feedback Queue Scheduler)
 * 👀 recent_cpu와 nice 사용해서 priority 계산하는 함수들
 * 👀 load_avg 계산할 함수들
 *
 * Priority
*  숫자가 클수록 우선순위가 높음
* 스레드 생성시 초기화 (default : 31)
* All thread : 1초 마다 재 계산
* Current thread : 4 clock tick 마다 재 계산
 * priority = PRI_MAX – (recent_cpu / 4) – (nice * 2)

 * ================================================== calcutations ==================================================
 */

#define Q 14
#define F (1 << Q) /*fixed point 1*/

/*Convert n to fixed point:*/
int int_to_float(int n)
{
	return n * F;
}

int float_to_int(int x)
{
	if (x >= 0)
		return (x + (F / 2)) / F;
	else
		return (x - (F / 2)) / F;
}

/*================================================== calcutations ==================================================*/

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

/*
	1️⃣ ⏰ ALARM 과제 : sleep-queue data structure 초기화해주도록 코드 추가하기

	Initializes the threading system by transforming the code
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
	 * The kernel will rebuild the gdt with user context, in gdt_init ().
	 * GDT(Global Descriptor Table)은, 사용 중인 세그먼트(program의 특정 영역을 구성하는 단위)를 나타내는 table 이라고 보면 된다고 한다.
	 * 참고로 x86-64가 세그먼트(program의 기본 단위)로 세분화되어 있다.
	 * gdt.c : GDT는 시스템 내의 모든 프로세스에 의해 사용될 수 있는 segments를 정의한다고 나와있다.
	 * https://velog.io/@opjoobe/PintOS-Project-2-TIL-2
	 *  */
	struct desc_ptr gdt_ds = {
		.size = sizeof(gdt) - 1,
		.address = (uint64_t)gdt};
	lgdt(&gdt_ds);

	/* Init the global thread context */
	lock_init(&tid_lock);
	list_init(&ready_list);
	list_init(&destruction_req);
	list_init(&sleep_list); /*1️⃣ ⏰ ALARM 과제 : sleep-queue data structure 초기화해주도록 코드 추가하기
							 */

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();

	/*
	 * 1️⃣ ⏰ ALARM 과제 : Initialise global tick, integer타입의 long long int , 64비트
	 * https://stackoverflow.com/questions/58710120/whats-the-exact-maximum-value-of-long-long-int-in-c
	 */
	/*🪲*/
	global_tick = 0x7FFFFFFFFFFFFFFF; // 0x7FFFFFFFFFFFFFFF;
									  //   global_tick = INT64_MAX;
};
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
	/*project2*/
	struct thread *curr;
	tid_t tid;

	ASSERT(function != NULL);

	/* Allocate thread. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread(t, name, priority);
	tid = t->tid = allocate_tid();

	/*project2 : process hierarchy*/
	curr = thread_current();
	t->parent = curr;
	list_push_back(&curr->children, &t->c_elem);
	curr->child_head = *list_head(&curr->children);
	curr->child_tail = *list_tail(&curr->children);

	/*project2 : file manipulation*/
	t->fdt = (struct file **)malloc(sizeof(struct file *) * MAX_FDE);
	if (t->fdt == NULL)
		sys_exit(-1);
	/*fd 0,1 is stdin, stdout*/
	t->next_fd = 2;

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
	/*Thread의 unblock 후,
	 현재 실행중인 thread와 우선순위를 비교하여,
	 새로 생성된 thread의 우선순위가 높다면 thread_yield()를 통해 CPU를 양보.*/
	thread_unblock(t);
	/*👀👀 이전 interrupt 활성화/비활성화 상태 가져오기*/
	enum intr_level old_level;
	/*비활성화시키고 이전 활성화 상태가져와서 old_level에 담기*/
	old_level = intr_disable();
	/*
	priority 스케줄링
	현재 thread의 priority 보다 새 thread t의 priority 가 높고 ready list가 비지 않았다면

	Thread의 unblock 후, 현재 실행중인 thread와 우선순위를 비교하여, 새로 생성된
	thread의 우선순위가 높다면 thread_yield()를 통해 CPU를 양보.
	*/
	/*🪲fork() 디버깅 중 -> 해결!!.🦗🪲*/
	if (t->priority >= thread_get_priority() && !list_empty(&ready_list))
	// if (t->priority > thread_get_priority() && !list_empty(&ready_list))
	{
		/* 쓰레드의 ready_list 맨앞 요소의 status가 ready 상태라면 CPU 양도
		The list_entry macro allows conversion from a
		struct list_elem back to a structure object that contains it.

		list_entry(LIST_ELEM,STRUCT,MEMBER)
		* LIST_ELEM : 우리가 원하는 노드의 시작 포인터
		* STRUCT : LIST_ELEM을 포함하고 있는 구조체
		* MEMBER : 구조체에 포함된 LIST_ELEM의 멤버명
		즉, ready_list의 첫 element를 포함하고 있는 구조체 thread을 가르키는 포인터의 status를 가져오기
		*/
		if (list_entry(list_front(&ready_list), struct thread, elem)->status == THREAD_READY)
			thread_yield(); /*If the priority of the new thread is higher, call schedule() (the current thread yields CPU).
							 */
	}
	intr_set_level(old_level);

	/*project2*/
	if (t->exit_status == -1)
	{
		process_wait(tid);
		return TID_ERROR;
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
	thread_current()->status = THREAD_BLOCKED;
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
	/*👀 우선순위대로 ready_list 순서 유지하려면 block한 thread를 ready리스트에 넣기 전에 우선순위를 고려해서 정렬하기
	 ready_list에 priority를 고려해서 thread 넣기

	 Thread가 unblock 될때 우선순위 순으로 정렬 되어 ready_list에 삽입되도록 수정
	 */
	list_insert_ordered(&ready_list, &t->elem, priority_greatest_function, NULL);
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
	ASSERT(list_empty(&thread_current()->children));
	ASSERT(list_empty(&thread_current()->donations));
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

	struct thread *curr = thread_current();
	enum intr_level old_level;

	ASSERT(!intr_context());

	old_level = intr_disable();

	if (curr != idle_thread)
	{
		/* 현재 thread가 CPU를 양보하여
	 ready_list에 삽입 될 때 우선순위 순서로 정렬되어
삽입 되도록 수정*/
		list_insert_ordered(&ready_list, &curr->elem, priority_greatest_function, NULL);
	}
	do_schedule(THREAD_READY); /*call schedule()*/
	intr_set_level(old_level);
}
/*👀👀👀👀👀 쓰레드 sleep_list에 넣기

timer_sleep() 호출시 thread를 ready_list에서 제거, sleep queue에
추가
 wake up 수행
 timer interrupt가 발생시 tick 체크
 시간이 다 된 thread는 sleep queue에서 삭제하고, ready_list에 추가
*/
void thread_sleep(int64_t ticks)
{
	struct thread *curr = thread_current();
	enum intr_level old_level;

	ASSERT(!intr_context());

	old_level = intr_disable(); /* When you manipulate thread list, disable interrupt! */
	if (curr != idle_thread)	/*if the current thread is not an idle thread */
	{
		/*Update global tick*/
		if (ticks < global_tick)
			global_tick = ticks;
		/*Save local tick (store the local tick to wake up)*/
		curr->wakeup_tick = ticks;
		/*insert thread in sleep_list in order of tick values (small to big)*/
		list_insert_ordered(&sleep_list, &curr->elem, order_by_least_wakeup_tick, NULL);
		do_schedule(THREAD_BLOCKED); /*change the state of the caller thread to BLOCKED*/
	}
	intr_set_level(old_level);
}
/*👀👀👀👀👀 쓰레드 wakeup_thread 하기*/
void wakeup_thread(void)
{
	struct thread *sleep_thread;
	/*sleep_list가 빈 상태가 아니라면*/
	if (!list_empty(&sleep_list))
	{
		/*sleep 중인 thread 꺼내오기 (sleep_list를 오름차순으로 정렬했기 때문에 가장 tick이 작은 즉, 요청을 먼저한 순대로 꺠우기 )*/
		sleep_thread = list_entry(list_pop_front(&sleep_list), struct thread, elem);
		/*thread에서 pop해줬기 떄문에 global_tick값 업데이트해야함
		  리스트의 맨앞값으로 업데이트하기 위해서  elem을 이용하여 리스트 내의 노드들을 순회
		 * sleep_list가 빈 상태가 아니라면 */
		if (!list_empty(&sleep_list))
			global_tick = list_entry(list_begin(&sleep_list),
									 struct thread, elem)
							  ->wakeup_tick;
		else
			/*sleep_list가 빈 상태라면 글로벌 tick 초기화 ()*/
			// global_tick = INT64_MAX;
			/*🪲*/
			global_tick = 0x7FFFFFFFFFFFFFFF;
		list_insert_ordered(&ready_list, &sleep_thread->elem,
							priority_greatest_function, NULL);
		sleep_thread->status = THREAD_READY;
	}
}
/*👀 실행완료한 thread lock 해제
ready_list에 대기 중인 thread가 있다면 앞에서부터 priority

*/
void thread_release_unlock()
{
	/*read_list가 빈 상태가 아니라면 */
	// if (!list_empty(&ready_list))
	/*project2 : external interrupt 없는 조건 추가*/
	if (!list_empty(&ready_list) && !intr_context())
	{ /*ready list 맨앞의 thread의 priority가 현재 */
		if (list_entry(list_front(&ready_list), struct thread, elem)->priority > thread_get_priority())
		{
			thread_yield();
		}
	}
}

/*
 * Sets the current thread's priority to NEW_PRIORITY.
 *
 * 현재 thread
 * multi-level feedback queue scheduler 사용하려면 새 priority로 업데이트
 * 아니면 round-robin 사용
 *
 *
 *
 */
void thread_set_priority(int new_priority)
{
	struct thread *t = thread_current();

	if (thread_mlfqs == true)
	{ /* mlfqs 스케줄러 일때 우선순위를 임의로 변경할수 없도록. */

		t->priority = new_priority;
	}
	else
	{
		/*스레드 우선순위 변경시 donation의 발생을 확인 하고
		우선순위 변경을 위해 donation_priority()함수 추가*/
		if (t->origin_priority == t->priority) /*donate priority 현재와 이전 thread (priority conflict 없애기)*/
			t->priority = new_priority;
		t->origin_priority = new_priority;
		if (!list_empty(&ready_list)) /*ready_list에 thread가 있다면*/
		{

			if (list_entry(list_front(&ready_list), struct thread, elem)->priority > new_priority)
			{
				/*현재 쓰레드의 우선 순위와 ready_list에서 가장 높은 우선 순위를 비교하여
스케쥴링 하는 함수 호출
*/
				thread_yield();
			}
		}
	}
}

/* Returns the current thread's priority. */
int thread_get_priority(void)
{
	return thread_current()->priority;
}

/*3️⃣ OPTIONAL : 4BSD LIKE SCHEDULER
 *
 * niceness moderates the priority of threads
 *
 * Nice 값 -> 착하니까 양보 , 따라서 (+)착함으로 생각하면 우선순위 낮춤
 * Nice (0) : 우선순위에 영향을 주지 않음
 * Nice (양수) : 우선순위를 감소
 * Nice (음수) : 우선순위를 증가
 * 초기 값 : 0
 *
 */
/* 3️⃣ 4BSD LIKE SCHEDULER :  Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice)
{
	/* 현제 스레드의 nice값을 변경하는 함수를 구현하다.
	해당 작업중에 인터럽트는 비활성화 해야 한다. */
	/* 현제 스레드의 nice 값을 변경한다.
	nice 값 변경 후에 현재 스레드의 우선순위를 재계산 하고
	우선순위에 의해 스케줄링 한다. */

	thread_current()->nice = nice;
}

/* Returns the current thread's nice value. */
int thread_get_nice(void)
{
	return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void)
{
	/* load_avg에 100을 곱해서 반환 한다.
해당 과정중에 인터럽트는 비활성되어야 한다. */
	return float_to_int(mul_x_y(int_to_float(100), load_avg));
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void)
{
	int recent_cpu = thread_current()->recent_cpu;
	/* recent_cpu 에 100을 곱해서 반환 한다.
	해당 과정중에 인터럽트는 비활성되어야 한다. */

	return float_to_int(mul_x_y(int_to_float(100), recent_cpu));
}

void calculate_load_avg(void)
{
	int ready_threads;

	if (thread_current() == idle_thread)
		/*number of elements in ready_list.*/
		ready_threads = list_size(&ready_list);
	else
		/*
		idle하다면 ready thread load 추가해주기
		number of elements in ready_list.*/
		ready_threads = list_size(&ready_list) + 1;

	/* load_avg = (59/60) * load_avg + (1/60) * ready_threads */
	load_avg = mul_x_y((int_to_float(59) / 60), load_avg) +
			   ((int_to_float(1) / 60) * ready_threads);
}

// Calculate for one thread
void recent_cpu_cal(struct thread *t, int decay)
{
	t->recent_cpu = add_x_n(mul_x_y(t->recent_cpu, decay), t->nice);
}

// Calculate for every thread
// mlfqs_priority
void calculate_recent_cpu(void)
{
	struct list_elem *e;
	struct thread *t;

	int decay = div_x_y((load_avg * 2), add_x_n((load_avg * 2), 1));

	t = thread_current();
	recent_cpu_cal(t, decay);
	for (e = list_begin(&ready_list); e != list_end(&ready_list);
		 e = list_next(e))
	{
		t = list_entry(e, struct thread, elem);
		recent_cpu_cal(t, decay);
	}
	for (e = list_begin(&sleep_list); e != list_end(&sleep_list);
		 e = list_next(e))
	{
		t = list_entry(e, struct thread, elem);
		recent_cpu_cal(t, decay);
	}
}

/*2️⃣  priority 연산*/
void calc_priority(struct thread *t)
{
	int priority = PRI_MAX - float_to_int(t->recent_cpu / 4) - (t->nice * 2);
	if (priority > PRI_MAX)
		t->priority = PRI_MAX;
	else if (priority < PRI_MIN)
		t->priority = PRI_MIN;
	else
		t->priority = priority;
}

/*2️⃣  priority 재연산*/
void recalc_priority(void)
{
	struct list_elem *e;
	struct thread *t;

	t = thread_current();
	calc_priority(t);
	for (e = list_begin(&ready_list); e != list_end(&ready_list);
		 e = list_next(e))
	{
		t = list_entry(e, struct thread, elem);
		calc_priority(t);
	}
	for (e = list_begin(&sleep_list); e != list_end(&sleep_list);
		 e = list_next(e))
	{
		t = list_entry(e, struct thread, elem);
		calc_priority(t);
	}
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

	intr_enable(); /* The scheduler runs with interrupts on. */
	function(aux); /* Execute the thread function. */
	thread_exit(); /* If function() returns, kill the thread. */
}

/*
 * Does basic initialization of T as a blocked thread named NAME.
 * 2️⃣ priority scheduling: 리스트안의 thread들이 inherit 받은 새 priority donate받도록
 *
 * 3️⃣ advanced scheduling : nice 값(no effect on priority), recent_cpu 초기화
 *
 */
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
	/*Priority donation관련 자료구조 초기화 코드 삽입
	 */
	t->origin_priority = t->priority;
	list_init(&t->donations);
	/*스케줄러 관련상수 정의, 변수 선언 및 초기화*/
	t->nice = 0;
	t->recent_cpu = 0;

#ifdef USERPROG
	t->is_exit = false;
	t->parent = NULL;
	list_init(&t->children);
	sema_init(&t->wait_sema, 0);
	sema_init(&t->exit_sema, 0);
	t->is_wait = false;
	memset(t->fd_exist, 0, MAX_FDE);
#endif
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

/* Use iretq to launch the thread
 * thread 간 context switch 할때 사용
 * schedule 함수를 보면 readylist에서 다음 실행할 스레드를 찾아
 * 이 함수를 thread_launch 함수의 인자로 넣어 호출한다.
 * thread_launch()에서는 현재 실행중인 스레드의 모든 정보를 현재 스레드의 인터럽트 프레임 구조체 tf에 담고,
 * 인자로 들어온 next 즉, 새로 실행할 스레드의 인터럽트 프레임 구조체를 인자로 담아 do_iret을 호출
 * do_iret은 인자로 들어온 인터럽트 프레임내의 정보를 CPU로 복원
 */
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
		palloc_free_page(victim);
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

/* Find thread by tid */
struct thread *
find_thread(tid_t tid)
{

	struct list_elem *e;
	struct thread *curr = thread_current();

	if (!list_empty(&curr->children))
	{
		for (e = list_begin(&curr->children); e != list_end(&curr->children);
			 e = list_next(e))
		{
			struct thread *t = list_entry(e, struct thread, c_elem);
			if (t->tid == tid)
				return t;
		}
	}
	else
		return NULL;
}

/* list_less_function for ready_list_insert_ordered.
Inserting thread in order by small ticks values. */
bool order_by_least_wakeup_tick(const struct list_elem *a, const struct list_elem *b,
								void *aux UNUSED)
{
	struct thread *thread_a = list_entry(a, struct thread, elem);
	struct thread *thread_b = list_entry(b, struct thread, elem);

	return thread_a->wakeup_tick < thread_b->wakeup_tick;
}

/* list_less_function for ready_list_insert_ordered.
Inserting thread in order by larger priority values.

 list_insert_ordered() 함수에서 사용
 */
bool priority_greatest_function(const struct list_elem *a, const struct list_elem *b,
								void *aux UNUSED)
{
	struct thread *thread_a = list_entry(a, struct thread, elem);
	struct thread *thread_b = list_entry(b, struct thread, elem);

	return thread_a->priority > thread_b->priority;
}