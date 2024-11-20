/*
 * Linux Kernel Design Patterns Guide
 * 
 * This header provides examples and explanations of common design patterns
 * used throughout the Linux kernel.
 */

#ifndef KERNEL_PATTERNS_H
#define KERNEL_PATTERNS_H

#include <stddef.h>
#include <stdint.h>

/*
 * 1. Singleton Pattern
 * Used for: Global kernel objects that should only have one instance
 * Examples: 
 * - init_task (first process)
 * - kmem_cache (slab allocator)
 */
struct singleton_example {
    static struct singleton_example *instance;
    /* private data */
};

/*
 * 2. Container Pattern
 * Used for: Embedding structs within other structs
 * Example: list_head in Linux data structures
 */
struct list_head {
    struct list_head *next, *prev;
};

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/*
 * 3. Observer Pattern
 * Used for: Notifier chains, callbacks
 * Examples: 
 * - Module notifications
 * - Network protocol handlers
 * - Device hotplug events
 */
struct notifier_block {
    int (*notifier_call)(struct notifier_block *self, 
                        unsigned long event, void *data);
    struct notifier_block *next;
    int priority;
};

/*
 * 4. Iterator Pattern
 * Used for: Traversing kernel data structures
 * Examples:
 * - Process list traversal
 * - Memory region scanning
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_entry(pos, head, member) \
    for (pos = container_of((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = container_of(pos->member.next, typeof(*pos), member))

/*
 * 5. Factory Pattern
 * Used for: Creating objects dynamically
 * Examples:
 * - Socket creation
 * - Device driver initialization
 */
struct device_driver;
typedef struct device_driver* (*driver_factory_fn)(void);

/*
 * 6. Strategy Pattern
 * Used for: Swappable algorithms/behaviors
 * Examples:
 * - I/O schedulers
 * - Process schedulers
 * - File systems
 */
struct file_operations {
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
};

/*
 * 7. Proxy Pattern
 * Used for: Controlled access to objects
 * Examples:
 * - System calls
 * - Virtual file system (VFS)
 */
struct proxy_example {
    void *real_object;
    int (*check_access)(void);
};

/*
 * 8. Facade Pattern
 * Used for: Simplifying complex subsystems
 * Examples:
 * - Device model
 * - Network stack APIs
 */
struct system_facade {
    void (*simple_operation)(void);
    /* Hides complex subsystem details */
};

/*
 * 9. Command Pattern
 * Used for: Encapsulating operations
 * Examples:
 * - Work queues
 * - Delayed work
 */
struct work_struct {
    void (*func)(struct work_struct *work);
    /* other members */
};

/*
 * 10. State Pattern
 * Used for: Managing object state
 * Examples:
 * - Process states
 * - Network connection states
 */
enum process_state {
    TASK_RUNNING,
    TASK_INTERRUPTIBLE,
    TASK_UNINTERRUPTIBLE,
    /* ... */
};

/*
 * 11. Reference Counting Pattern
 * Used for: Memory management
 * Examples:
 * - kobject reference counting
 * - Module usage counting
 */
struct kref {
    atomic_t refcount;
};

/*
 * 12. RCU (Read-Copy-Update) Pattern
 * Used for: Lock-free data structure access
 * Examples:
 * - dcache (directory entry cache)
 * - Network routing tables
 */
struct rcu_head {
    struct rcu_head *next;
    void (*func)(struct rcu_head *head);
};

/*
 * 13. Barrier Pattern
 * Used for: Synchronization
 * Examples:
 * - Memory barriers
 * - CPU barriers
 */
#define barrier() __asm__ __volatile__("": : :"memory")

/*
 * 14. Completion Pattern
 * Used for: Synchronization and signaling
 * Examples:
 * - Module initialization
 * - Device probing
 */
struct completion {
    unsigned int done;
    wait_queue_head_t wait;
};

/*
 * 15. Reactor Pattern
 * Used for: Event handling
 * Examples:
 * - epoll
 * - Input device handling
 */
struct poll_table_struct;
typedef void (*poll_queue_proc)(struct file *, wait_queue_head_t *, struct poll_table_struct *);

#endif /* KERNEL_PATTERNS_H */
