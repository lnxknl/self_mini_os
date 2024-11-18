#ifndef _SCHEDULER_CONTEXT_H
#define _SCHEDULER_CONTEXT_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <asm/processor.h>
#include <asm/msr.h>

// CPU affinity definitions
#define MAX_CPU_AFFINITY_GROUPS 16
#define AFFINITY_STRICT     0x01    // Must run on specified CPUs
#define AFFINITY_PREFERRED  0x02    // Prefer specified CPUs
#define AFFINITY_ISOLATED   0x04    // Run on isolated CPUs

// Priority inheritance
#define MAX_INHERITANCE_DEPTH 8
#define PI_ENABLED    0x01
#define PI_RECURSIVE  0x02
#define PI_IMMEDIATE  0x04

// Context switch flags
#define CTX_SAVE_FPU     0x01
#define CTX_SAVE_VEC     0x02
#define CTX_KERNEL       0x04
#define CTX_USER         0x08

// Task context structure
struct task_context {
    struct pt_regs regs;           // CPU registers
    struct fpu fpu_state;          // FPU state
    unsigned long cr3;             // Page table base
    unsigned long kernel_sp;       // Kernel stack pointer
    unsigned long user_sp;         // User stack pointer
    unsigned int flags;            // Context flags
};

// CPU affinity group
struct cpu_affinity_group {
    cpumask_t cpumask;            // CPU mask
    unsigned int flags;            // Affinity flags
    struct list_head tasks;        // Tasks in this group
    spinlock_t lock;              // Group lock
};

// Priority inheritance chain
struct pi_chain {
    struct task_extension *owner;          // Current owner
    struct task_extension *inheritor;      // Task inheriting priority
    int original_prio;                     // Original priority
    int inherited_prio;                    // Inherited priority
    unsigned long long inheritance_time;    // When inheritance started
    struct list_head waiters;              // Waiting tasks
    struct pi_chain *next;                 // Next in chain
};

// Priority inheritance state
struct pi_state {
    spinlock_t lock;                      // State lock
    unsigned int flags;                   // PI flags
    int depth;                           // Current inheritance depth
    struct pi_chain *chains;             // Active inheritance chains
    struct list_head inherited_from;      // Tasks we inherit from
    struct list_head inherited_to;        // Tasks inheriting from us
};

// Extended task structure for context management
struct task_context_ext {
    struct task_context context;          // Task context
    struct cpu_affinity_group *affinity;  // CPU affinity
    struct pi_state pi_state;            // Priority inheritance state
    unsigned long last_cpu;              // Last CPU executed on
    unsigned long long last_switch;       // Last context switch time
    unsigned int switch_count;            // Number of context switches
};

// Function declarations for context management
int init_task_context(struct task_context_ext *ctx);
void save_task_context(struct task_context_ext *ctx);
void restore_task_context(struct task_context_ext *ctx);
int switch_to_task(struct task_context_ext *prev,
                  struct task_context_ext *next);

// Function declarations for CPU affinity
struct cpu_affinity_group *create_affinity_group(const cpumask_t *mask,
                                               unsigned int flags);
int set_task_affinity(struct task_extension *task,
                     struct cpu_affinity_group *group);
bool check_cpu_affinity(struct task_extension *task, int cpu);

// Function declarations for priority inheritance
int init_pi_state(struct pi_state *pi);
int inherit_priority(struct task_extension *from,
                    struct task_extension *to);
void revert_inheritance(struct task_extension *task);
int check_priority_chain(struct task_extension *task);

#endif /* _SCHEDULER_CONTEXT_H */
