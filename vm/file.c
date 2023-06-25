/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};


/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	struct file* file = ((struct mmap_info*)page ->uninit.aux)->file;
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	file_page -> file = file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	if (file_page->file == NULL) return false;
	//printf("\n\ncheck : %p\n\n", page);
	file_seek (file_page->file, file_page->ofs);
	off_t read_size = file_read (file_page->file, kva, file_page->size);
	if (read_size != file_page->size) return false;
	if (read_size < PGSIZE)
		memset (kva + read_size, 0, PGSIZE - read_size);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *curr = thread_current ();
	
	if (pml4_is_dirty (curr->pml4, page->va)) {
		file_seek (file_page->file, file_page->ofs);
		file_write (file_page->file, page->va, file_page->size);
		pml4_set_dirty (curr->pml4, page->va, false);
	}

	// Set "not present" to page, and clear.
	pml4_clear_page (curr->pml4, page->va);
	page->frame = NULL;

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	// TODO: On mmap_exit sometimes empty file content
	struct file_page *file_page = &page->file;
	//if dirty, write back to file
	if (pml4_is_dirty (thread_current() -> pml4, page -> va)){
		file_seek (file_page->file, file_page->ofs);
		file_write (file_page->file, page->va, file_page->size);
	}
	file_close (file_page->file);

	if (page->frame != NULL) {
		list_remove (&page->frame->elem);
		free (page->frame);
	}
}



struct mmap_file_info{
	uint64_t mapid;
	struct list_elem elem;
	struct file* file;
};

static bool
lazy_load_file (struct page* page, void* aux){
	struct mmap_info* mi = (struct mmap_info*) aux;
	file_seek (mi->file, mi->offset);
	page -> file.size = file_read (mi->file, page->va, mi->read_bytes);
	page -> file.ofs = mi->offset;
	if (page->file.size != PGSIZE){
		memset (page->va + page->file.size, 0, PGSIZE - page->file.size);
	}
	pml4_set_dirty (thread_current()->pml4, page->va, false);
	free(mi);
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
			/* length는 pgsize의 배수여야 한다.*/

			void * ori_addr = addr;
			size_t read_bytes = length > file_length(file) ? file_length(file) : length;
			while (read_bytes > 0){
				size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
				//printf("\n\ncheck : %ld \n\n", read_bytes);
				struct mmap_info *aux = calloc(sizeof(struct mmap_info),1);
				
				aux->file = file_reopen(file);
				aux->offset = offset;
				aux->read_bytes = page_read_bytes;
				//printf("\n\n%p\n\n", aux->file);
				
				if(!vm_alloc_page_with_initializer (VM_FILE, (void*) ((uint64_t) addr), writable, lazy_load_file, aux))
					return;
				read_bytes -= page_read_bytes;
				offset += page_read_bytes;
				addr += PGSIZE;
			}
			
			struct mmap_file_info* mfi = malloc (sizeof (struct mmap_file_info));
			mfi->mapid = ori_addr;
			//printf("\n\ncheck : %p thread :%s \n\n", mfi->mapid, thread_current()->name);
			list_push_back(&thread_current()->mmap_file_list, &mfi->elem);
			
			return ori_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	
	if (list_empty (&thread_current()->mmap_file_list)) return;
	struct list *mmap_list = &thread_current()->mmap_file_list;
	//printf("\n\nbegin : %p end : %p\n\n", list_begin (&mmap_list), list_end (&mmap_list));
	for (struct list_elem* i = list_begin (mmap_list); i != list_end (mmap_list); i = list_next (i))
	{
		struct mmap_file_info* mfi = list_entry (i, struct mmap_file_info, elem);
		//printf("\n\ncheck : %p thread :%s \n\n", mfi->mapid, thread_current()->name);
		if (mfi -> mapid == (uint64_t) addr){
			/* mmap된 파일 리스트에서만 제거해준다 */
			struct page* page = spt_find_page(&thread_current() -> spt, addr);
			struct file_page *file_page UNUSED = &page->file;
			list_remove(&mfi->elem);
			if (pml4_is_dirty (thread_current()->pml4, page->va)) {
				file_seek (file_page->file, file_page->ofs);
				file_write (file_page->file, page->va, file_page->size);
				pml4_set_dirty (thread_current()->pml4, page->va, false);
			}
			pml4_clear_page(thread_current()->pml4, page->va);
			return;
		}
	}
}