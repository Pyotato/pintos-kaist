#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/flags.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <list.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "intrinsic.h"
#include "vm/vm.h"
//! ADD : for project 4
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "string.h"
#include "filesys/fat.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/*              system call by inkyu            */
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int exec(const *cmd_line);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
int wait(tid_t tid);
tid_t fork(const char *thread_name, struct intr_frame *f);
int dup2(int oldfd, int newfd);
static void munmap(void *addr);
static void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);

// TODO ============================ for Project 4 =================================
bool is_dir(int fd);
bool sys_chdir(const char *path_name);
bool sys_mkdir(const char *dir);
bool sys_readdir(int fd, char *name);
struct cluster_t *sys_inumber(int fd);
int symlink (const char *target, const char *linkpath);

/*              system call need func by inkyu            */
int add_file_to_fdt(struct file *file);
static struct file *find_file_by_fd(int fd);
void check_address(const uint64_t *uaddr);
static void check_writable_addr(void *ptr);
void remove_file_from_fdt(int fd);

const int STDIN = 1;
const int STDOUT = 2;
struct lock file_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081					/* Segment selector msr */
#define MSR_LSTAR 0xc0000082				/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
													((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);
	lock_init(&file_lock);
	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
						FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// TODO: Your implementation goes here.
	thread_current()->saved_sp = f->rsp;
	switch (f->R.rax)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_EXEC:
		if (exec(f->R.rdi) == -1)
			exit(-1);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_DUP2:
		f->R.rax = dup2(f->R.rdi, f->R.rsi);
		break;
	case SYS_MMAP:
		f->R.rax = (uint64_t)mmap((void *)f->R.rdi, (size_t)f->R.rsi, (int)f->R.rdx, (int)f->R.r10, (off_t)f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap((void *)f->R.rdi);
		break;
		//! for project 4
	case SYS_ISDIR:
		f->R.rax = is_dir(f->R.rdi);
		break;
	case SYS_CHDIR:
		f->R.rax = sys_chdir(f->R.rdi);
		break;
	case SYS_MKDIR:
		f->R.rax = sys_mkdir(f->R.rdi);
		break;
	case SYS_READDIR:
		f->R.rax = sys_readdir(f->R.rdi, f->R.rsi);
		break;
	case SYS_INUMBER:
		f->R.rax = sys_inumber(f->R.rdi);
		break;
	case SYS_SYMLINK:
		f->R.rax = symlink(f->R.rdi, f->R.rsi);
		break;
	default:
		break;
	}
}

void halt(void)
{
	power_off();
}

void exit(int status)
{
	struct thread *cur = thread_current();
	cur->exit_status = status;
	thread_exit();
}

bool create(const char *file, unsigned initial_size)
{
	
	check_address(file);
	lock_acquire(&file_lock);
	bool success = filesys_create(file, initial_size);
	lock_release(&file_lock);

	return success;
}

bool remove(const char *file)
{
	check_address(file);
	lock_acquire(&file_lock);
	bool success = filesys_remove(file);
	lock_release(&file_lock);
	return success;
}

int open(const char *file)
{
	check_address(file);
	
	lock_acquire(&file_lock);
	struct file *fileobj = filesys_open(file);
	lock_release(&file_lock);
	if (fileobj == NULL)
	{
		return -1;
	}

	int fd = add_file_to_fdt(fileobj);

	if (fd == -1)
	{
		file_close(fileobj);
	}

	return fd;
}

void close(int fd)
{
	/*       fd를이용하여 file 받음 by inkyu           */
	struct file *objfile = find_file_by_fd(fd);
	struct thread *cur = thread_current();

	if (objfile == NULL)
		return;

	if (fd == 0 || objfile == STDIN)
	{
		cur->stdin_count--;
	}
	else if (fd == 1 || objfile == STDOUT)
	{
		cur->stdout_count--;
	}

	if (fd <= 1 || objfile <= 2)
		return;

	/*      table 에서 삭제           */
	remove_file_from_fdt(fd);
	//lock_acquire(&file_lock);
	if (objfile->dupCount == 0)
		file_close(objfile);
	else if (objfile->dupCount > 0)
		objfile->dupCount--;
	//lock_release(&file_lock);
	return;
}

int add_file_to_fdt(struct file *file)
{
	struct thread *cur = thread_current();
	struct file **fdt = cur->fdTable;
	//파일 개수 및 새로 열 파일의 fd값 raise
	while (cur->fdIdx < FDCOUNT_LIMIT && fdt[cur->fdIdx])
	{
		//printf("\n%d\n", cur->fdIdx);
		cur->fdIdx++;
	}

	// fdt full limit 512개
	if (cur->fdIdx >= FDCOUNT_LIMIT)
		return -1;

	fdt[cur->fdIdx] = file;
	return cur->fdIdx;
}

int filesize(int fd)
{
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj == NULL)
		return -1;
	return file_length(fileobj);
}

static struct file *find_file_by_fd(int fd)
{
	struct thread *cur = thread_current();
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return NULL;

	return cur->fdTable[fd];
}

void check_address(const uint64_t *uaddr)
{
	struct thread *cur = thread_current();

	if (uaddr == NULL)
		exit(-1);
	if (!(is_user_vaddr(uaddr)))
		exit(-1);
	uint64_t *pte = pml4e_walk(cur->pml4, (const uint64_t)uaddr, 0);
	if (pte == NULL)
		exit(-1);
	//printf("\n\ncheck address start\n\n");
	//printf("\n\ncheck spt %p\n\n", (&thread_current() -> spt) -> page_table ->buckets);
	struct page *page = spt_find_page(&thread_current()->spt, uaddr);

	if (page == NULL)
		exit(-1);
}

int exec(const *cmd_line)
{
	check_address(cmd_line);

	char *file_name[30];
	memcpy(file_name, cmd_line, strlen(cmd_line) + 1);
	if (process_exec(file_name) == -1)
		return -1;

	NOT_REACHED();
	return 0;
}

void remove_file_from_fdt(int fd)
{
	struct thread *cur = thread_current();

	// Error - invalid fd
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;

	cur->fdTable[fd] = NULL;
}

int read(int fd, void *buffer, unsigned size)
{

	check_address(buffer);

	check_writable_addr(buffer);

	int length;
	struct thread *cur = thread_current();
	struct file *fileobj = find_file_by_fd(fd);
	// printf("\n\nfileobj : %p\n\n", fileobj);
	// printf("\n\ncontents : %d\n\n", fileobj->pos);
	if (fileobj == NULL)
		return -1;

	/*     extra 문제에서 더이상 stdin이 연결된애가 없으면 읽기 금지      */
	if (fd == 0 && cur->stdin_count != 0)
	{
		int i;
		unsigned char *buf = buffer;
		for (i = 0; i < size; i++)
		{
			char c = input_getc();
			*buf++ = c;
			if (c == '\0')
				break;
		}
		length = i;
	}
	else if (fd == 1)
	{
		length = -1;
	}
	else if (fd > 1)
	{
		lock_acquire(&file_lock);
		//printf("\n\ncheck : %p\n\n", buffer);
		length = file_read(fileobj, buffer, size);
		//printf("\n\n%d\n\n", length);
		lock_release(&file_lock);
	}
	return length;
}

int write(int fd, const void *buffer, unsigned size)
{
	check_address(buffer);
	if (size == 0)
		return 0;
	int length;
	struct thread *cur = thread_current();
	struct file *fileobj = find_file_by_fd(fd);

	if (fileobj == NULL)
		return -1;

	/*        fd가 stdout인경우 putbuf를 이용하여 화면에 출력          */
	/*     extra 문제에서 더이상 stdout이 연결된애가 없으면 쓰기 금지      */
	if (fileobj == STDOUT && cur->stdout_count != 0)
	{
		putbuf(buffer, size);
		length = size;
	}
	else if (fileobj == STDIN)
	{
		length = -1;
	}
	else if (fileobj > 2)
	{
		lock_acquire(&file_lock);
		length = file_write(fileobj, buffer, size);
		lock_release(&file_lock);
	}

	return length;
}

void seek(int fd, unsigned position)
{
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj <= 2)
		return;
	lock_acquire(&file_lock);
	fileobj->pos = position;
	lock_release(&file_lock);
	return;
}

unsigned tell(int fd)
{
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj <= 2)
		return;
	return fileobj->pos;
}

int wait(tid_t tid)
{
	process_wait(tid);
}

tid_t fork(const char *thread_name, struct intr_frame *f)
{
	/* 왜 성고 실패가 반복되냐 생각해봤는데 */
	/* fork, mmap또한 파일이 읽고 수정중인데 작동되는게 아니라고 생각된다. */
	lock_acquire(&file_lock);
	tid_t tid = process_fork(thread_name, f);
	lock_release(&file_lock);
	return tid;
}

int dup2(int oldfd, int newfd)
{
	struct thread *cur = thread_current();
	struct file *objfile = find_file_by_fd(oldfd);
	if (objfile == NULL)
		return -1;
	if (oldfd == newfd)
		return newfd;

	if (objfile == STDIN)
		cur->stdin_count++;
	else if (objfile == STDOUT)
		cur->stdout_count++;
	else
		objfile->dupCount++;
	close(newfd);
	cur->fdTable[newfd] = objfile;

	return newfd;
}

static void
check_writable_addr(void *ptr)
{
	struct page *page = spt_find_page(&thread_current()->spt, ptr);
	if (page == NULL || !(page->writable))
		exit(-1);
}

static void *
mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	if (fd == 0 || fd == 1)
		return NULL;

	if (addr == 0 || (!is_user_vaddr(addr)))
		return NULL;
	if ((uint64_t)addr % PGSIZE != 0)
		return NULL;
	if (offset % PGSIZE != 0)
		return NULL;
	if ((uint64_t)addr + length == 0)
		return NULL;
	if (!is_user_vaddr((uint64_t)addr + length))
		return NULL;
	for (uint64_t i = (uint64_t)addr; i < (uint64_t)addr + length; i += PGSIZE)
	{
		if (spt_find_page(&thread_current()->spt, (void *)i) != NULL)
			return NULL;
	}
	if (length == 0)
		return NULL;

	struct file *file = find_file_by_fd(fd);
	if (file == NULL)
		return NULL;
	lock_acquire(&file_lock);
	void *target_address = do_mmap(addr, length, writable, file, offset);
	lock_release(&file_lock);
	return target_address;
}

static void
munmap(void *addr)
{
	lock_acquire(&file_lock);
	do_munmap(addr);
	lock_release(&file_lock);
}

// TODO =============================== for Project 4 ==========================================
//: file의 directory 여부 판단
bool is_dir(int fd)
{
	struct file *target = find_file_by_fd(fd);
	if (target == NULL)
		return false;

	return inode_is_dir(file_get_inode(target));
}

//: 현재 directory 위치 변경
bool sys_chdir(const char *path_name)
{
	if (path_name == NULL)
		return false;

	/* name의파일경로를cp_name에복사*/
	char *cp_name = (char *)malloc(strlen(path_name) + 1);
	strlcpy(cp_name, path_name, strlen(path_name) + 1);

	struct dir *chdir = NULL;
	/* PATH_NAME의절대/상대경로에따른디렉터리정보저장(구현)*/
	if (cp_name[0] == '/')
	{
		chdir = dir_open_root();
	}
	else
		chdir = dir_reopen(thread_current()->cur_dir);

	/* dir경로를분석하여디렉터리를반환*/
	//! 무조건 경로가 들어올 것이므로, nextToken 불필요
	char *token, *nextToken, *savePtr;
	token = strtok_r(cp_name, "/", &savePtr);

	struct inode *inode = NULL;
	while (token != NULL)
	{
		/* dir에서token이름의파일을검색하여inode의정보를저장*/
		if (!dir_lookup(chdir, token, &inode))
		{
			dir_close(chdir);
			return false;
		}

		/* inode가파일일경우NULL 반환*/
		if (!inode_is_dir(inode))
		{
			dir_close(chdir);
			return false;
		}
		/* dir의디렉터리정보를메모리에서해지*/
		dir_close(chdir);

		/* inode의디렉터리정보를dir에저장*/
		chdir = dir_open(inode);

		/* token에검색할경로이름저장*/
		token = strtok_r(NULL, "/", &savePtr);
	}
	/* 스레드의현재작업디렉터리를변경*/
	dir_close(thread_current()->cur_dir);
	thread_current()->cur_dir = chdir;
	free(cp_name);
	return true;
}

//: directory 생성
bool sys_mkdir(const char *dir)
{
	lock_acquire(&filesys_lock);
	bool tmp = filesys_create_dir(dir);

	lock_release(&filesys_lock);
	return tmp;
}

//: directory 내 파일 존재 여부 확인
bool sys_readdir(int fd, char *name)
{
	if (name == NULL)
		return false;
	
	/* fd리스트에서fd에대한file정보를얻어옴*/
	struct file *target = find_file_by_fd(fd);
	if (target == NULL)
		return false;

	/* fd의file->inode가디렉터리인지검사*/
	if (!inode_is_dir(file_get_inode(target)))
		return false;

	/* p_file을dir자료구조로포인팅*/
	struct dir *p_file = target;
	if (p_file->pos == 0)
		dir_seek(p_file, 2 * sizeof(struct dir_entry)); //! ".", ".." 제외

	/* 디렉터리의엔트에서“.”,”..” 이름을제외한파일이름을name에저장*/
	bool result = dir_readdir(p_file, name);
	//! 닫으면 오류가 생김
	// file_close(target);
	// dir_close(p_file);
	return result;
}

//: file의 inode가 기록된 sector 찾기
struct cluster_t *sys_inumber(int fd)
{
	struct file *target = find_file_by_fd(fd);
	if (target == NULL)
		return false;

	return inode_get_inumber(file_get_inode(target));
}

//: 바로가기 file 생성
int symlink(const char *target, const char *linkpath)
{
	//! SOFT LINK
	bool success = false;
	char *cp_link = (char *)malloc(strlen(linkpath) + 1);
	strlcpy(cp_link, linkpath, strlen(linkpath) + 1);

	/* cp_name의경로분석*/
	char *file_link = (char *)malloc(strlen(cp_link) + 1);
	struct dir *dir = parse_path(cp_link, file_link);

	cluster_t inode_cluster = fat_create_chain(0);

	//! link file 전용 inode 생성 및 directory에 추가
	success = (dir != NULL && link_inode_create(inode_cluster, target) && dir_add(dir, file_link, inode_cluster));

	if (!success && inode_cluster != 0)
		fat_remove_chain(inode_cluster, 0);

	dir_close(dir);
	free(cp_link);
	free(file_link);

	return success - 1;

	//! HARD LINK
	// char* cp_link = (char *)malloc(strlen(linkpath) + 1);
	// strlcpy(cp_link, linkpath, strlen(linkpath) + 1);
	// char* target_link = (char *)malloc(strlen(linkpath) + 1);
	// strlcpy(target_link, linkpath, strlen(linkpath) + 1);

	// char* cp_file_link = (char *)malloc(strlen(linkpath) + 1);
	// char* target_file_link = (char *)malloc(strlen(linkpath) + 1);

	// struct dir* cur_dir = parse_path(cp_link, cp_file_link);
	// struct dir* target_dir = parse_path(target_link, target_file_link);

	// // printf("현재 스레드의 섹터 넘버 :: %d\n",inode_get_inumber(dir_get_inode(cur_dir)));
	// // printf("타겟 스레드의 섹터 넘버 :: %d\n",inode_get_inumber(dir_get_inode(target_dir)));

	// bool success = dir_add (cur_dir, linkpath, inode_get_inumber(dir_get_inode(target_dir)));

	// dir_close(cur_dir);
	// dir_close(target_dir);

	// free(cp_link);
	// free(target_link);
	// free(cp_file_link);
	// free(target_file_link);

	// return success - 1;

	// printf("만들 파일 :: %s\n", linkpath);
}

// TODO END =============================== for Project 4 ==========================================