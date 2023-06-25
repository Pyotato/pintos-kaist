#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "devices/disk.h"

#define INVALID_SLOT_IDX SIZE_MAX

struct page;
enum vm_type;

struct anon_page {
  struct thread* owner;
    size_t swap_slot_idx;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
