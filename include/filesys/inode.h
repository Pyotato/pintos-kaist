#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"
//! ADD
#include "filesys/fat.h"
#include "lib/kernel/list.h"

struct bitmap;

void inode_init (void);
bool inode_create (disk_sector_t, off_t, uint32_t);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

// TODO ========================= for Project 4 ================================
bool inode_is_dir(const struct inode* inode);
bool link_inode_create (disk_sector_t sector, char* path_name);

/* On-disk inode.
 * Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
	disk_sector_t start;                /* First data sector. */
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */

    //! ADD
    uint32_t is_dir;    /* 디렉토리 구분 */
    uint32_t is_link;   /* symlink 구분 */

    //^ 멤버 추가시마다 512바이트 맞추기
	char link_name[492];               /* Not used. */
};

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	disk_sector_t sector;               /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};

// TODO END ========================= for Project 4 ================================

#endif /* filesys/inode.h */
