#ifndef MMU_H
#define MMU_H

#include <stdint.h>
#include <stdbool.h>

/* MMU Translation Table Levels */
#define MMU_LEVEL_1     1
#define MMU_LEVEL_2     2
#define MMU_LEVEL_3     3

/* Page Sizes */
#define PAGE_SIZE_4K    0x1000
#define PAGE_SIZE_2M    0x200000
#define PAGE_SIZE_1G    0x40000000

/* Memory Types */
typedef enum {
    MEMORY_TYPE_DEVICE_NGNRNE = 0,  /* Device, non-gathering, non-reordering, no early write ack */
    MEMORY_TYPE_DEVICE_NGNRE  = 1,  /* Device, non-gathering, non-reordering, early write ack */
    MEMORY_TYPE_DEVICE_GRE    = 2,  /* Device, gathering, reordering, early write ack */
    MEMORY_TYPE_NORMAL_NC     = 3,  /* Normal memory, non-cacheable */
    MEMORY_TYPE_NORMAL_WT     = 4,  /* Normal memory, write-through */
    MEMORY_TYPE_NORMAL_WB     = 5   /* Normal memory, write-back */
} memory_type_t;

/* Access Permissions */
typedef enum {
    AP_NO_ACCESS     = 0,
    AP_RW_EL1       = 1,  /* Read/Write at EL1, no access at EL0 */
    AP_RW_ALL       = 2,  /* Read/Write at all levels */
    AP_RO_EL1       = 3,  /* Read-only at EL1, no access at EL0 */
    AP_RO_ALL       = 4   /* Read-only at all levels */
} access_perm_t;

/* Page Table Entry Attributes */
typedef struct {
    memory_type_t mem_type;
    access_perm_t ap;
    bool executable;
    bool shareable;
    bool global;
} page_attrs_t;

/* Virtual Memory Region */
typedef struct vm_region {
    uint64_t virt_addr;    /* Virtual address start */
    uint64_t phys_addr;    /* Physical address start */
    uint64_t size;         /* Region size */
    page_attrs_t attrs;    /* Page attributes */
    struct vm_region *next;
} vm_region_t;

/* MMU Translation Table Entry */
typedef uint64_t tt_entry_t;

/* MMU Context */
typedef struct {
    tt_entry_t *ttbr0_l1;  /* Level 1 translation table base (user space) */
    tt_entry_t *ttbr1_l1;  /* Level 1 translation table base (kernel space) */
    vm_region_t *regions;  /* List of mapped regions */
} mmu_context_t;

/* MMU Initialization and Setup */
void mmu_init(void);
void mmu_enable(void);
void mmu_disable(void);

/* Memory Mapping Functions */
int mmu_map_region(uint64_t virt_addr, uint64_t phys_addr, 
                   uint64_t size, page_attrs_t *attrs);
int mmu_unmap_region(uint64_t virt_addr, uint64_t size);

/* Address Translation */
uint64_t mmu_virt_to_phys(uint64_t virt_addr);
uint64_t mmu_phys_to_virt(uint64_t phys_addr);

/* TLB Management */
void mmu_invalidate_tlb_all(void);
void mmu_invalidate_tlb_va(uint64_t virt_addr);

/* Cache Management */
void mmu_invalidate_dcache_all(void);
void mmu_clean_dcache_all(void);
void mmu_flush_dcache_all(void);

/* Memory Barrier Operations */
void mmu_dsb(void);
void mmu_dmb(void);
void mmu_isb(void);

/* Page Table Management */
tt_entry_t *mmu_create_table(void);
void mmu_free_table(tt_entry_t *table);

/* Utility Functions */
bool mmu_is_aligned(uint64_t addr, uint64_t size);
uint64_t mmu_align_up(uint64_t addr, uint64_t align);
uint64_t mmu_align_down(uint64_t addr, uint64_t align);

#endif /* MMU_H */
