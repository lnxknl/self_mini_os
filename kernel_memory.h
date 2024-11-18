#ifndef _KERNEL_MEMORY_H
#define _KERNEL_MEMORY_H

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mempool.h>
#include <linux/page-flags.h>
#include <linux/hugetlb.h>
#include <linux/dma-mapping.h>

/* Memory Zones
 * ZONE_DMA     : 0-16MB   - For DMA-capable devices
 * ZONE_NORMAL  : 16MB-896MB - Regular memory
 * ZONE_HIGHMEM : >896MB   - High memory region
 */
#define ZONE_DMA_SIZE      (16 * 1024 * 1024)
#define ZONE_NORMAL_SIZE   (896 * 1024 * 1024)

/* Page Size Constants */
#define PAGE_SIZE_4K       (4 * 1024)
#define PAGE_SIZE_2M       (2 * 1024 * 1024)
#define PAGE_SIZE_1G       (1024 * 1024 * 1024)

/* Memory Types */
enum memory_type {
    MEM_KERNEL,        // Kernel internal memory
    MEM_USER,          // User space memory
    MEM_DMA,           // DMA-capable memory
    MEM_HIGHMEM        // High memory region
};

/* Page Table Entry Flags */
#define PTE_PRESENT     0x001
#define PTE_WRITABLE    0x002
#define PTE_USER        0x004
#define PTE_ACCESSED    0x020
#define PTE_DIRTY       0x040
#define PTE_HUGE        0x080
#define PTE_GLOBAL      0x100

/* Virtual Memory Area (VMA) Structure */
struct kernel_vma {
    unsigned long start;           // Start address
    unsigned long end;             // End address
    unsigned long flags;           // VMA flags
    struct mm_struct *mm;          // Memory descriptor
    pgprot_t prot;                // Page protection
    unsigned long pgoff;           // Page offset
    struct vm_operations_struct *vm_ops;  // VMA operations
};

/* Memory Region Descriptor */
struct mem_region {
    unsigned long start_pfn;       // Start page frame number
    unsigned long end_pfn;         // End page frame number
    enum memory_type type;         // Memory type
    struct list_head list;         // List of regions
    unsigned long flags;           // Region flags
};

/* Page Cache Entry */
struct page_cache_entry {
    struct page *page;             // Page descriptor
    unsigned long index;           // Page index
    struct address_space *mapping; // Address space
    unsigned long flags;           // Cache flags
};

/* Memory Pool Configuration */
struct mempool_config {
    size_t min_nr;                // Minimum number of elements
    size_t curr_nr;               // Current number of elements
    size_t max_nr;                // Maximum number of elements
    gfp_t gfp_mask;               // Allocation flags
    void *pool_data;              // Pool-specific data
    mempool_alloc_t *alloc;       // Allocation function
    mempool_free_t *free;         // Free function
    spinlock_t lock;              // Pool lock
};

/* NUMA Node Memory */
struct numa_memory {
    int node_id;                  // NUMA node ID
    unsigned long free_pages;     // Number of free pages
    unsigned long total_pages;    // Total pages in node
    struct list_head zones;       // List of zones in node
    spinlock_t lock;             // Node lock
};

/* Slab Allocator Cache */
struct slab_cache {
    size_t size;                  // Object size
    size_t align;                 // Object alignment
    unsigned int flags;           // Cache flags
    const char *name;             // Cache name
    void (*ctor)(void *);        // Object constructor
    void (*dtor)(void *);        // Object destructor
    struct kmem_cache *kmem_cache; // Kernel slab cache
};

/* Function Declarations */

/* Page Table Management */
int init_page_table(struct mm_struct *mm);
pte_t *get_pte(struct mm_struct *mm, unsigned long addr);
int set_pte(pte_t *pte, unsigned long val);

/* Memory Allocation */
void *kmalloc_ext(size_t size, gfp_t flags, int node);
void *vmalloc_ext(size_t size);
struct page *alloc_pages_ext(gfp_t flags, unsigned int order);

/* Virtual Memory Management */
int map_vm_area(struct kernel_vma *vma);
int protect_vm_area(struct kernel_vma *vma, pgprot_t prot);
void unmap_vm_area(struct kernel_vma *vma);

/* Memory Pool Operations */
struct mempool_config *create_mempool(size_t min_nr, size_t max_nr);
void *mempool_alloc_ext(struct mempool_config *pool, gfp_t flags);
void mempool_free_ext(void *element, struct mempool_config *pool);

/* NUMA Operations */
struct numa_memory *init_numa_node(int node_id);
int numa_allocate_memory(struct numa_memory *node, size_t size);
void numa_free_memory(struct numa_memory *node, void *ptr);

/* Slab Cache Operations */
struct slab_cache *create_slab_cache(const char *name, size_t size);
void *slab_cache_alloc(struct slab_cache *cache, gfp_t flags);
void slab_cache_free(struct slab_cache *cache, void *obj);

/* Memory Compaction */
int compact_memory_zone(struct zone *zone);
int migrate_pages_ext(struct list_head *from, struct list_head *to);

/* TLB Management */
void flush_tlb_mm(struct mm_struct *mm);
void flush_tlb_range(struct vm_area_struct *vma,
                    unsigned long start, unsigned long end);

/* Page Cache Operations */
struct page_cache_entry *find_get_page_cache(struct address_space *mapping,
                                           pgoff_t index);
int add_to_page_cache(struct page *page, struct address_space *mapping,
                     pgoff_t index, gfp_t flags);

#endif /* _KERNEL_MEMORY_H */
