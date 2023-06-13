#include <syscall.h>
#include <stdint.h>
#include "../syscall-nr.h"

__attribute__((always_inline)) static __inline int64_t syscall(uint64_t num_, uint64_t a1_, uint64_t a2_,
															   uint64_t a3_, uint64_t a4_, uint64_t a5_, uint64_t a6_)
{
	int64_t ret;
	register uint64_t *num asm("rax") = (uint64_t *)num_;
	register uint64_t *a1 asm("rdi") = (uint64_t *)a1_;
	register uint64_t *a2 asm("rsi") = (uint64_t *)a2_;
	register uint64_t *a3 asm("rdx") = (uint64_t *)a3_;
	register uint64_t *a4 asm("r10") = (uint64_t *)a4_;
	register uint64_t *a5 asm("r8") = (uint64_t *)a5_;
	register uint64_t *a6 asm("r9") = (uint64_t *)a6_;

	__asm __volatile(
		"mov %1, %%rax\n"
		"mov %2, %%rdi\n"
		"mov %3, %%rsi\n"
		"mov %4, %%rdx\n"
		"mov %5, %%r10\n"
		"mov %6, %%r8\n"
		"mov %7, %%r9\n"
		"syscall\n"
		: "=a"(ret)
		: "g"(num), "g"(a1), "g"(a2), "g"(a3), "g"(a4), "g"(a5), "g"(a6)
		: "cc", "memory");
	return ret;
}

/* Invokes syscall NUMBER, passing no arguments, and returns the
   return value as an `int'. */
#define syscall0(NUMBER) ( \
	syscall(((uint64_t)NUMBER), 0, 0, 0, 0, 0, 0))

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0) ( \
	syscall(((uint64_t)NUMBER),  \
			((uint64_t)ARG0), 0, 0, 0, 0, 0))
/* Invokes syscall NUMBER, passing arguments ARG0 and ARG1, and
   returns the return value as an `int'. */
#define syscall2(NUMBER, ARG0, ARG1) ( \
	syscall(((uint64_t)NUMBER),        \
			((uint64_t)ARG0),          \
			((uint64_t)ARG1),          \
			0, 0, 0, 0))

#define syscall3(NUMBER, ARG0, ARG1, ARG2) ( \
	syscall(((uint64_t)NUMBER),              \
			((uint64_t)ARG0),                \
			((uint64_t)ARG1),                \
			((uint64_t)ARG2), 0, 0, 0))

#define syscall4(NUMBER, ARG0, ARG1, ARG2, ARG3) ( \
	syscall(((uint64_t *)NUMBER),                  \
			((uint64_t)ARG0),                      \
			((uint64_t)ARG1),                      \
			((uint64_t)ARG2),                      \
			((uint64_t)ARG3), 0, 0))

#define syscall5(NUMBER, ARG0, ARG1, ARG2, ARG3, ARG4) ( \
	syscall(((uint64_t)NUMBER),                          \
			((uint64_t)ARG0),                            \
			((uint64_t)ARG1),                            \
			((uint64_t)ARG2),                            \
			((uint64_t)ARG3),                            \
			((uint64_t)ARG4),                            \
			0))
/**
 * @fn halt
 * @param void
 * @brief terminates Pintos by calling power_off()
 * @return void
 *
 */
void halt(void)
{
	syscall0(SYS_HALT);
	NOT_REACHED();
}

/**
 * @fn exit
 * @param int status
 * @brief terminates the current user program
 * @return status to the kernel (0 on success, else are errors)
 *
 *
 */
void exit(int status)
{
	syscall1(SYS_EXIT, status);
	NOT_REACHED();
}
/**
 * @fn fork
 * @param const char *thread_name
 * @brief create new process which is the clone of current process with the name THREAD_NAME, no need to clone register values except callee-saved registers (%RBX, %RSP , % RBP, %R12 ~ %R15)
 * @return pid_t : pid of the child process
 */
pid_t fork(const char *thread_name)
{
	return (pid_t)syscall1(SYS_FORK, thread_name);
}
/**
 * @fn exec
 * @param const char *file
 * @brief change current process to the executable whose name is given in file(==cmd_line), passing any given arguments
 * @return int never return if successful else, terminate process with exit state -1 (if program cannot run or load)
 */
int exec(const char *file)
{
	return (pid_t)syscall1(SYS_EXEC, file);
}
/**
 * @fn 			wait
 * @param 		pid_t pid
 * @brief 		waits for a child process pid and retrives the child's exit status. If pid is still alive, wait till it terminates.
 *				If pid does not refer to a direct child of the calling process or the process that calls wait has already call wait on pid ➡️ WAIT MUST FAIL AND IMMEDIATELY RETURN -1
 * @return 		int status that pid passed to exit else -1 if kernel terminated child
 *
 */
int wait(pid_t pid)
{
	return syscall1(SYS_WAIT, pid);
}
/**
 * @fn			create
 * @param 		const char *file
 * @param 		unsigned int initial_size
 * @brief		creates a new file called file intially initial_size bytes in size (⚠️opening new file is a separate operation!)
 * @return		bool true if successful, false otherwise
 */
bool create(const char *file, unsigned initial_size)
{
	return syscall2(SYS_CREATE, file, initial_size);
}
/**
 * @fn			remove
 * @param 		const char *file
 * @brief		deletes a new file called file (⚠️opening/closing files and removing files are independent operations!)
 * @return		bool true if successful, false otherwise
 */
bool remove(const char *file)
{
	return syscall1(SYS_REMOVE, file);
}

/**
 * @fn			open
 * @param 		const char *file
 * @brief		opens a new file called file (⚠️opening/closing files and removing files are independent operations!)
 * @return		int fd (dile_descriptor : nonnegative interger handle) on success, -1 if file cannot be opened
 * 				⚠️ fd 0 (STDIN_FILENO) is standard input and fd 1 is standard output
 * 				each process has an independent set of file descriptors, respectively inherited by child processes
 */
int open(const char *file)
{ /*🪲*/
	return syscall1(SYS_OPEN, file);
}

/**
 * @fn			filesize
 * @param 		int fd
 * @return		int size(bytes) of the file open as fd
 */
int filesize(int fd)
{
	return syscall1(SYS_FILESIZE, fd);
}

/**
 * @fn			read
 * @param 		int fd
 * @param 		void *buffer
 * @param 		unsigned size
 * @brief		Reads size bytes from the file open as fd into buffer.
 * 				fd 0 reads from the keyboard using input_getc()
 *
 * @return		int number of bytes actually read (0 at end of file), or -1 if the file could not be read (due to a condition other than end of file)
 */
int read(int fd, void *buffer, unsigned size)
{
	return syscall3(SYS_READ, fd, buffer, size);
}
/**
 * @fn			write
 *
 * @param 		int fd
 * @param 		void *buffer
 * @param 		unsigned size
 *
 * @brief		Writes size bytes from buffer to the open file fd (as many as up to end-of-file)
 *
 * @return		int number of bytes actually written (which may be less than size if some bytes could not be written),
 * 				0 if no bytes could be written at all.
 */
int write(int fd, const void *buffer, unsigned size)
{
	return syscall3(SYS_WRITE, fd, buffer, size);
}

/**
 * @fn			seek
 *
 * @param 		int fd
 * @param 		unsigned position
 *
 * @brief		Changes the next byte to be read or written in open file fd to position,
 * 				expressed in bytes from the beginning of the file (therefore position of 0 == file's start)
 *
 * @return		void
 */
void seek(int fd, unsigned position)
{
	syscall2(SYS_SEEK, fd, position);
}

/**
 * @fn			tell
 *
 * @param 		int fd
 *
 * @return		unsigned int the position of the next byte to be read or written in open file fd,
 * 				expressed in bytes from the beginning of the file
 */
unsigned
tell(int fd)
{
	return syscall1(SYS_TELL, fd);
}

/**
 * @fn			close
 *
 * @param 		int fd
 *
 * @brief		Closes file descriptor fd.
 * 				Exiting or terminating a process implicitly closes all its open file descriptors
 * 				as if by calling this funciton for each one
 *
 * @return		void
 */
void close(int fd)
{
	syscall1(SYS_CLOSE, fd);
}

int dup2(int oldfd, int newfd)
{
	return syscall2(SYS_DUP2, oldfd, newfd);
}

void *
mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	return (void *)syscall5(SYS_MMAP, addr, length, writable, fd, offset);
}

void munmap(void *addr)
{
	syscall1(SYS_MUNMAP, addr);
}

bool chdir(const char *dir)
{
	return syscall1(SYS_CHDIR, dir);
}

bool mkdir(const char *dir)
{
	return syscall1(SYS_MKDIR, dir);
}

bool readdir(int fd, char name[READDIR_MAX_LEN + 1])
{
	return syscall2(SYS_READDIR, fd, name);
}

bool isdir(int fd)
{
	return syscall1(SYS_ISDIR, fd);
}

int inumber(int fd)
{
	return syscall1(SYS_INUMBER, fd);
}

int symlink(const char *target, const char *linkpath)
{
	return syscall2(SYS_SYMLINK, target, linkpath);
}

int mount(const char *path, int chan_no, int dev_no)
{
	return syscall3(SYS_MOUNT, path, chan_no, dev_no);
}

int umount(const char *path)
{
	return syscall1(SYS_UMOUNT, path);
}
