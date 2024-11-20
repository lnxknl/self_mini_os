#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include "rtos_types.h"

/* Virtual Memory Page States */
typedef enum {
    PAGE_FREE = 0,
    PAGE_ALLOCATED,
    PAGE_SWAPPED,
    PAGE_RESERVED
} page_state_t;

/* Page Table Entry */
typedef struct {
    uint32_t virtual_addr;
    uint32_t physical_addr;
    uint8_t present : 1;
    uint8_t writable : 1;
    uint8_t user_access : 1;
    uint8_t write_through : 1;
    uint8_t cache_disabled : 1;
    uint8_t accessed : 1;
    uint8_t dirty : 1;
    uint8_t reserved : 1;
    uint8_t global : 1;
    uint8_t cow : 1;  /* Copy on Write */
} page_table_entry_t;

/* Page Directory Entry */
typedef struct {
    page_table_entry_t *page_table;
    uint8_t present : 1;
    uint8_t writable : 1;
    uint8_t user_access : 1;
    uint8_t write_through : 1;
    uint8_t cache_disabled : 1;
    uint8_t accessed : 1;
    uint8_t reserved : 1;
    uint8_t page_size : 1;  /* 0 = 4KB, 1 = 4MB */
    uint8_t global : 1;
} page_directory_entry_t;

/* Virtual Memory Space */
typedef struct {
    page_directory_entry_t *page_directory;
    uint32_t heap_start;
    uint32_t heap_end;
    uint32_t stack_start;
    uint32_t stack_end;
} virtual_memory_space_t;

/* Page Frame Allocator */
typedef struct {
    uint32_t *bitmap;
    uint32_t total_frames;
    uint32_t free_frames;
    mutex_t *lock;
} frame_allocator_t;

/* Memory Management Functions */
void vmm_init(void);
virtual_memory_space_t *vmm_create_space(void);
void vmm_delete_space(virtual_memory_space_t *space);
void *vmm_alloc_pages(virtual_memory_space_t *space, uint32_t count);
void vmm_free_pages(virtual_memory_space_t *space, void *addr, uint32_t count);

/* Page Table Management */
int map_page(virtual_memory_space_t *space, uint32_t virtual_addr, 
             uint32_t physical_addr, uint32_t flags);
int unmap_page(virtual_memory_space_t *space, uint32_t virtual_addr);
uint32_t get_physical_address(virtual_memory_space_t *space, uint32_t virtual_addr);

/* Memory Protection */
int set_page_protection(virtual_memory_space_t *space, uint32_t virtual_addr, 
                       uint32_t flags);
int validate_address_range(virtual_memory_space_t *space, uint32_t start, 
                         uint32_t end, uint32_t flags);

/* Copy-on-Write Support */
int enable_cow(virtual_memory_space_t *space, uint32_t virtual_addr);
int handle_cow_fault(virtual_memory_space_t *space, uint32_t fault_addr);

/* Page Swapping */
int swap_out_page(virtual_memory_space_t *space, uint32_t virtual_addr);
int swap_in_page(virtual_memory_space_t *space, uint32_t virtual_addr);

/* TLB Management */
void flush_tlb_entry(uint32_t virtual_addr);
void flush_tlb_all(void);

/* Memory Statistics */
typedef struct {
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t swapped_pages;
    uint32_t cow_pages;
    uint32_t page_faults;
    uint32_t tlb_misses;
} vmm_stats_t;

void get_vmm_stats(vmm_stats_t *stats);

#endif /* VMM_H */
