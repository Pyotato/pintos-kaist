#include "userprog/syscall.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "list.h"
#include "stdbool.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"
#include "threads/interrupt.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/loader.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "filesys/filesys.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_address(const uint64_t *addr);
int add_file_to_fdt(struct file *file);

typedef int pid_t;

/* For filesystem global Lock */
struct lock filesys_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock);
}

static void
sys_halt()
{
	power_off();
}

void sys_exit(int status)
{
	struct thread *t = thread_current();
	t->is_exit = true;
	t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, t->exit_status);
	thread_exit();
}

static pid_t
sys_fork(const char *thread_name, struct intr_frame *f)
{
	check_address((uint64_t *)thread_name);
	pid_t c_pid;

	c_pid = process_fork(thread_name, f);
	return c_pid;
}

static int
sys_exec(const char *cmd_line)
{
	check_address((uint64_t *)cmd_line);

	int file_size = strlen(cmd_line) + 1;
	char *fn_copy = palloc_get_page(PAL_ZERO);
	/*🪲 syn_read 테케 통과 되었다가 안되는 경우 해결*/
	if (fn_copy == NULL)
	{
		sys_exit(-1);
	}
	strlcpy(fn_copy, cmd_line, file_size);

	if (process_exec(fn_copy) == -1)
	{
		sys_exit(-1);
	}

	NOT_REACHED();
	return 0;
}

static int
sys_wait(pid_t pid)
{
	int status;
	status = process_wait(pid);

	return status;
}

static bool
sys_create(const char *file, unsigned initial_size)
{
	check_address((uint64_t *)file);
	return filesys_create(file, initial_size);
}

static int
sys_open(const char *file)
{
	check_address((uint64_t *)file);

	struct file *open_file = filesys_open(file);

	if (open_file == NULL)
		return -1;

	lock_acquire(&filesys_lock);

	if (!strcmp(file, thread_current()->name))
		file_deny_write(open_file);

	lock_release(&filesys_lock);

	return add_file_to_fdt(open_file);
}

static bool
sys_remove(const char *file)
{
	check_address((uint64_t *)file);
	return filesys_remove(file);
}

static int
sys_filesize(int fd)
{
	if (thread_current()->next_fd <= fd)
		return -1;
	struct file *f = thread_current()->fdt[fd];
	return file_length(f);
}

static int
sys_read(int fd, void *buffer, unsigned size)
{
	check_address(buffer);

	if (fd == 1)
	{
		return -1;
	}
	lock_acquire(&filesys_lock);
	if ((0 > fd) || (thread_current()->next_fd <= fd))
	{
		sys_exit(-1);
	}
	// not stdin
	if (fd > 0)
	{
		struct file *f = thread_current()->fdt[fd];
		lock_release(&filesys_lock);
		return file_read(f, buffer, size);
	}
	// is stdin
	else
	{
		int count = 0;
		uint8_t temp;
		while ((temp = input_getc()) != '\0')
		{
			memcpy(buffer, &temp, sizeof(uint8_t));
			count++;
		}
		lock_release(&filesys_lock);
		return count;
	}
}

static int
sys_write(int fd, const void *buffer, unsigned size)
{
	check_address(buffer);
	if ((1 > fd) || (thread_current()->next_fd <= fd))
		sys_exit(-1);
	lock_acquire(&filesys_lock);

	if (fd == 1)
	{
		putbuf(buffer, strlen(buffer));
		lock_release(&filesys_lock);
		return strlen(buffer);
	}
	else
	{
		struct file *f = thread_current()->fdt[fd];
		lock_release(&filesys_lock);
		return file_write(f, buffer, size);
	}
}

static void
sys_seek(int fd, unsigned position)
{
	if ((0 > fd) || (thread_current()->next_fd <= fd))
		sys_exit(-1);
	file_seek(thread_current()->fdt[fd], position);
}

static unsigned
sys_tell(int fd)
{
	if ((0 > fd) || (thread_current()->next_fd <= fd))
		sys_exit(-1);
	return file_tell(thread_current()->fdt[fd]);
}

void sys_close(int fd)
{
	if ((0 > fd) || (thread_current()->next_fd <= fd))
	{
		sys_exit(-1);
	}
	struct thread *t = thread_current();
	if (t->fd_exist[fd])
	{
		t->fd_exist[fd] = false;
		struct file *f = t->fdt[fd];
		file_close(f);
	}
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{
	/* Save argument. !! MUST check valid pointer!! */
	uint64_t arg[6];
	arg[0] = f->R.rdi;
	arg[1] = f->R.rsi;
	arg[2] = f->R.rdx;
	arg[3] = f->R.r10;
	arg[4] = f->R.r8;
	arg[5] = f->R.r9;

	switch (f->R.rax)
	{
	case SYS_HALT:
		sys_halt();
		break;

	case SYS_EXIT:
		sys_exit((int)arg[0]);
		break;

	case SYS_FORK:
		f->R.rax = sys_fork((char *)arg[0], f);
		break;

	case SYS_WAIT:
		f->R.rax = sys_wait((pid_t)arg[0]);
		break;

	case SYS_EXEC:
		f->R.rax = sys_exec((const char *)arg[0]);
		break;

	case SYS_CREATE:
		f->R.rax = sys_create((const char *)arg[0], (unsigned)arg[1]);
		break;

	case SYS_OPEN:
		f->R.rax = sys_open((const char *)arg[0]);
		break;

	case SYS_REMOVE:
		f->R.rax = sys_remove((const char *)arg[0]);
		break;

	case SYS_FILESIZE:
		f->R.rax = sys_filesize((int)arg[0]);
		break;

	case SYS_READ:
		f->R.rax = sys_read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
		break;

	case SYS_WRITE:
		f->R.rax = sys_write((int)arg[0], (const void *)arg[1],
							 (unsigned)arg[2]);
		break;

	case SYS_SEEK:
		sys_seek((int)arg[0], (unsigned)arg[1]);
		break;

	case SYS_TELL:
		f->R.rax = sys_tell((int)arg[0]);
		break;

	case SYS_CLOSE:
		sys_close((int)arg[0]);
		break;
	default:
		sys_exit(-1);
		break;
	}
}
/*현재 thread의 포인터 주소 validity check */
void check_address(const uint64_t *addr)
{
	/*
	 * 현재 주소가 NULL인 경우
	 * 현재 주소가 user va를 가르키ㅣ 않는 경우
	 * 물리적(physical address) 주소를 가르키는 주소가 없는 경우
	 */
	struct thread *cur = thread_current();
	if (addr == NULL || !(is_user_vaddr(addr)) ||
		!pml4_get_page(cur->pml4, addr))
		sys_exit(-1);
}

int add_file_to_fdt(struct file *file)
{
	struct thread *cur = thread_current();
	struct file **fdt = cur->fdt;

	if (cur->next_fd == MAX_FDE)
	{
		file_close(file);
		return -1;
	}

	fdt[cur->next_fd] = file;
	cur->fd_exist[cur->next_fd] = true;
	return cur->next_fd++;
}