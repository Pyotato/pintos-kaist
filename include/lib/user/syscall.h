#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include <stdbool.h>
#include <debug.h>
#include <stddef.h>

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t)-1)

/* Map region identifier. */
typedef int off_t;
#define MAP_FAILED ((void *)NULL)

/* Maximum characters in a filename written by readdir(). */
#define READDIR_MAX_LEN 14

/* Typical return values from main() and arguments to exit(). */
#define EXIT_SUCCESS 0 /* Successful execution. */
#define EXIT_FAILURE 1 /* Unsuccessful execution. */

/* Projects 2 and later. */
/**
 *
 * @fn 			halt
 *
 * @brief		Terminates Pintos by calling power_off() (declared in src/include/threads/init.h).
 * 				This should be seldom used, because you lose some information about possible deadlock situations, etc.
 *
 */
void halt(void) NO_RETURN;
/**
 *
 * @fn 			exit
 *
 * @brief		Terminates the current user program, returning status to the kernel.
 * 				If the process's parent waits for it (see below), this is the status that will be returned.
 * 				Conventionally, a status of 0 indicates success and nonzero values indicate errors.
 *
 */
void exit(int status) NO_RETURN;
/**
 *
 * @fn 			fork
 *
 * @brief		Create new process which is the clone of current process with the name THREAD_NAME.
 * 				You don't need to clone the value of the registers except %RBX, %RSP, %RBP, and %R12 - %R15,
 * 				which are callee-saved registers.
 * 				Must return pid of the child process, otherwise shouldn't be a valid pid.
 *
 * 				In child process, the return value should be 0.
 * 				The child should have DUPLICATED resources including file descriptor and virtual memory space.
 *
 * 				Parent process should never return from the fork until it knows whether the child process successfully cloned.
 * 				That is, if the child process fail to duplicate the resource, the fork () call of parent should return the TID_ERROR.
 *
 * 				The template utilizes the pml4_for_each() in threads/mmu.c to copy entire user memory space, including corresponding pagetable structures,
 * 				but you need to fill missing parts of passed pte_for_each_func
 *
 */
pid_t fork(const char *thread_name);
/**
 *
 * @fn 			exec
 *
 * @brief		Change current process to the executable whose name is given in cmd_line,
 * 				passing any given arguments.
 * 				This never returns if successful.
 * 				Otherwise the process terminates with exit state -1, if the program cannot load or run for any reason.
 * 				This function does not change the name of the thread that called exec.
 *
 * 				Please note that file descriptors remain open across an exec call.
 *
 */
int exec(const char *file);
/**
 *
 * @fn 			wait
 *
 * @brief		Waits for a child process pid and retrieves the child's exit status.
 * 				If pid is still alive, waits until it terminates.
 * 				Then, returns the status that pid passed to exit.
 * 				If pid did not call exit(), but was terminated by the kernel (e.g. killed due to an exception), wait(pid) must return -1.
 * 				It is perfectly legal for a parent process to wait for child processes that have already terminated by the time the parent calls wait,
 * 				but the kernel must still allow the parent to retrieve its child’s exit status, or learn that the child was terminated by the kernel.

				wait must fail and return -1 immediately if any of the following conditions is true:

				pid does not refer to a direct child of the calling process.
				pid is a direct child of the calling process if and only if the calling process received pid as a return value from a successful call to fork.
				Note that children are not inherited: if A spawns child B and B spawns child process C, then A cannot wait for C, even if B is dead. A call to wait(C) by process A must fail. Similarly, orphaned processes are not assigned to a new parent if their parent process exits before they do.
The process that calls wait has already called wait on pid. That is, a process may wait for any given child at most once.
 */
int wait(pid_t);

/**
 *
 * @fn 			create
 *
 * @brief		Creates a new file called file initially initial_size bytes in size.
 * 				Creating a new file does not open it:
 * 				opening the new file is a separate operation which would require a open system call
 *
 * @return		bool
 *
 */
bool create(const char *file, unsigned initial_size);
/**
 *
 * @fn 			remove
 *
 * @brief		Deletes the file called file.
 * 				A file may be removed regardless of whether it is open or closed, and removing an open file does not close it.
 *
 * @return		bool (true if successful)
 *
 */
bool remove(const char *file);
/**
 *
 * @fn 			open
 *
 * @brief		Opens the file called file. Returns a nonnegative integer handle called a "file descriptor" (fd), or -1 if the file could not be opened.
 * 				File descriptors numbered 0 and 1 are reserved for the console: fd 0 (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is standard output.
 * 				The open system call will never return either of these file descriptors, which are valid as system call arguments only as explicitly described below.
 * 				Each process has an independent set of file descriptors. File descriptors are not inherited by child processes.
 * 				When a single file is opened more than once, whether by a single process or different processes, each open returns a new file descriptor.
 * 				Different file descriptors for a single file are closed independently in separate calls to close and they do not share a file position.
 * @return		bool
 *
 */
int open(const char *file);
/**
 *
 * @fn 			open
 *
 * @brief		Returns the size, in bytes, of the file open as fd.
 *
 * @return		int
 *
 */
int filesize(int fd);
/**
 *
 * @fn 			open
 *
 * @brief		Reads size bytes from the file open as fd into buffer.
 *
 * @return		int
 * 				the number of bytes actually read (0 at end of file), or -1 if the file could not be read (due to a condition other than end of file).
 * 				Fd 0 reads from the keyboard using input_getc().
 *
 */
int read(int fd, void *buffer, unsigned length);
/**
 *
 * @fn 			open
 *
 * @brief		Writes size bytes from buffer to the open file fd.
 *
 * @return		int
 * 				The number of bytes actually written , which may be less than size if some bytes could not be written.
 * 				Writing past end-of-file would normally extend the file, but file growth is not implemented by the basic file system.
 * 				The expected behavior is to write as many bytes as possible up to end-of-file and return the actual number written, or 0 if no bytes could be written at all.
 *				Fd 1 writes to the console. Your code to write to the console should write all of buffer in one call to putbuf(), at least as long as size is not bigger than a few hundred bytes.
 *				(It is reasonable to break up larger buffers.)
 *				Otherwise, lines of text output by different processes may end up interleaved on the console, confusing both human readers and our grading scripts.
 */
int write(int fd, const void *buffer, unsigned length);
/**
 *
 * @fn 			seek
 *
 * @brief		Changes the next byte to be read or written in open file fd to position, expressed in bytes from the beginning of the file.
 * 				(Thus, a position of 0 is the file's start.)
 * 				A seek past the current end of a file is not an error.
 * 				A later read obtains 0 bytes, indicating end of file.
 * 				A later write extends the file, filling any unwritten gap with zeros.
 * 				(However, in Pintos files have a fixed length until project 4 is complete, so writes past end of file will return an error.)
 * 				These semantics are implemented in the file system and do not require any special effort in system call implementation.
 */
void seek(int fd, unsigned position);

/**
 *
 * @fn 			tell
 *
 * @return		The position of the next byte to be read or written in open file fd,
 * 				expressed in bytes from the beginning of the file.
 */
unsigned tell(int fd);

/**
 *
 * @fn 			close
 *
 * @brief		Closes file descriptor fd.
 * 				Exiting or terminating a process implicitly closes all its open file descriptors,
 * 				as if by calling this function for each one.
 */
void close(int fd);

int dup2(int oldfd, int newfd);

/* Project 3 and optionally project 4. */
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);

/* Project 4 only. */
bool chdir(const char *dir);
bool mkdir(const char *dir);
bool readdir(int fd, char name[READDIR_MAX_LEN + 1]);
bool isdir(int fd);
int inumber(int fd);
int symlink(const char *target, const char *linkpath);

static inline void *get_phys_addr(void *user_addr)
{
	void *pa;
	asm volatile("movq %0, %%rax" ::"r"(user_addr));
	asm volatile("int $0x42");
	asm volatile("\t movq %%rax, %0"
				 : "=r"(pa));
	return pa;
}

static inline long long
get_fs_disk_read_cnt(void)
{
	long long read_cnt;
	asm volatile("movq $0, %rdx");
	asm volatile("movq $1, %rcx");
	asm volatile("int $0x43");
	asm volatile("\t movq %%rax, %0"
				 : "=r"(read_cnt));
	return read_cnt;
}

static inline long long
get_fs_disk_write_cnt(void)
{
	long long write_cnt;
	asm volatile("movq $0, %rdx");
	asm volatile("movq $1, %rcx");
	asm volatile("int $0x44");
	asm volatile("\t movq %%rax, %0"
				 : "=r"(write_cnt));
	return write_cnt;
}

#endif /* lib/user/syscall.h */
