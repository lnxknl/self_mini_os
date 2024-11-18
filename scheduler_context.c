#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/processor.h>
#include <asm/fpu/internal.h>
#include "scheduler_context.h"
#include "task_scheduler_advanced.h"

// Context Management Implementation
int init_task_context(struct task_context_ext *ctx) {
    if (!ctx)
        return -EINVAL;

    memset(&ctx->context, 0, sizeof(struct task_context));
    ctx->context.flags = CTX_SAVE_FPU | CTX_USER;
    ctx->last_cpu = -1;
    ctx->last_switch = 0;
    ctx->switch_count = 0;

    // Initialize priority inheritance state
    init_pi_state(&ctx->pi_state);

    return 0;
}

void save_task_context(struct task_context_ext *ctx) {
    struct pt_regs *regs = task_pt_regs(current);
    
    // Save CPU registers
    memcpy(&ctx->context.regs, regs, sizeof(struct pt_regs));
    
    // Save FPU state if needed
    if (ctx->context.flags & CTX_SAVE_FPU) {
        copy_fxregs_to_kernel(&ctx->context.fpu_state);
    }
    
    // Save CR3 (page table base) and stack pointers
    ctx->context.cr3 = __read_cr3();
    ctx->context.kernel_sp = current_stack_pointer();
    ctx->context.user_sp = regs->sp;
    
    // Update statistics
    ctx->last_switch = ktime_get_ns();
    ctx->switch_count++;
}

void restore_task_context(struct task_context_ext *ctx) {
    struct pt_regs *regs = task_pt_regs(current);
    
    // Restore CPU registers
    memcpy(regs, &ctx->context.regs, sizeof(struct pt_regs));
    
    // Restore FPU state if needed
    if (ctx->context.flags & CTX_SAVE_FPU) {
        copy_kernel_to_fxregs(&ctx->context.fpu_state);
    }
    
    // Restore CR3 and stack pointers
    __write_cr3(ctx->context.cr3);
    
    if (ctx->context.flags & CTX_USER) {
        regs->sp = ctx->context.user_sp;
    } else {
        regs->sp = ctx->context.kernel_sp;
    }
}

int switch_to_task(struct task_context_ext *prev,
                  struct task_context_ext *next) {
    unsigned long flags;
    
    if (!prev || !next)
        return -EINVAL;

    local_irq_save(flags);
    
    // Save current task's context
    save_task_context(prev);
    
    // Switch page tables if needed
    if (prev->context.cr3 != next->context.cr3) {
        load_new_mm(next->context.cr3);
    }
    
    // Restore next task's context
    restore_task_context(next);
    
    // Update CPU affinity information
    next->last_cpu = smp_processor_id();
    
    local_irq_restore(flags);
    return 0;
}

// CPU Affinity Implementation
struct cpu_affinity_group *create_affinity_group(const cpumask_t *mask,
                                               unsigned int flags) {
    struct cpu_affinity_group *group;
    
    if (!mask || cpumask_empty(mask))
        return ERR_PTR(-EINVAL);
        
    group = kzalloc(sizeof(*group), GFP_KERNEL);
    if (!group)
        return ERR_PTR(-ENOMEM);
        
    cpumask_copy(&group->cpumask, mask);
    group->flags = flags;
    INIT_LIST_HEAD(&group->tasks);
    spin_lock_init(&group->lock);
    
    return group;
}

int set_task_affinity(struct task_extension *task,
                     struct cpu_affinity_group *group) {
    unsigned long flags;
    
    if (!task || !group)
        return -EINVAL;
        
    spin_lock_irqsave(&group->lock, flags);
    
    // Remove from old group if any
    if (task->ctx.affinity) {
        spin_lock(&task->ctx.affinity->lock);
        list_del(&task->ctx.affinity_node);
        spin_unlock(&task->ctx.affinity->lock);
    }
    
    task->ctx.affinity = group;
    list_add_tail(&task->ctx.affinity_node, &group->tasks);
    
    spin_unlock_irqrestore(&group->lock, flags);
    return 0;
}

bool check_cpu_affinity(struct task_extension *task, int cpu) {
    struct cpu_affinity_group *group;
    bool allowed = true;
    
    if (!task || cpu < 0 || cpu >= nr_cpu_ids)
        return false;
        
    group = task->ctx.affinity;
    if (!group)
        return true;  // No affinity restrictions
        
    if (group->flags & AFFINITY_STRICT) {
        allowed = cpumask_test_cpu(cpu, &group->cpumask);
    } else if (group->flags & AFFINITY_PREFERRED) {
        // Prefer affinity CPUs but allow others if necessary
        allowed = cpumask_test_cpu(cpu, &group->cpumask) ||
                 task->group->load_avg > (MAX_CPU_CAPACITY * 80 / 100);
    }
    
    return allowed;
}

// Priority Inheritance Implementation
int init_pi_state(struct pi_state *pi) {
    if (!pi)
        return -EINVAL;
        
    spin_lock_init(&pi->lock);
    pi->flags = PI_ENABLED;
    pi->depth = 0;
    pi->chains = NULL;
    INIT_LIST_HEAD(&pi->inherited_from);
    INIT_LIST_HEAD(&pi->inherited_to);
    
    return 0;
}

int inherit_priority(struct task_extension *from,
                    struct task_extension *to) {
    struct pi_chain *chain;
    unsigned long flags;
    int ret = 0;
    
    if (!from || !to)
        return -EINVAL;
        
    if (to->pi_state.depth >= MAX_INHERITANCE_DEPTH)
        return -ELOOP;
        
    spin_lock_irqsave(&to->pi_state.lock, flags);
    
    // Create new inheritance chain
    chain = kzalloc(sizeof(*chain), GFP_ATOMIC);
    if (!chain) {
        ret = -ENOMEM;
        goto out;
    }
    
    chain->owner = to;
    chain->inheritor = from;
    chain->original_prio = to->nice;
    chain->inherited_prio = from->nice;
    chain->inheritance_time = ktime_get_ns();
    INIT_LIST_HEAD(&chain->waiters);
    
    // Update priorities
    to->nice = min(to->nice, from->nice);
    to->pi_state.depth++;
    
    // Link chain
    chain->next = to->pi_state.chains;
    to->pi_state.chains = chain;
    
    // Update inheritance relationships
    list_add_tail(&from->pi_node, &to->pi_state.inherited_from);
    list_add_tail(&to->pi_node, &from->pi_state.inherited_to);
    
    // Recursive inheritance if enabled
    if ((to->pi_state.flags & PI_RECURSIVE) && !list_empty(&to->pi_state.inherited_to)) {
        struct task_extension *next;
        list_for_each_entry(next, &to->pi_state.inherited_to, pi_node) {
            inherit_priority(to, next);
        }
    }
    
out:
    spin_unlock_irqrestore(&to->pi_state.lock, flags);
    return ret;
}

void revert_inheritance(struct task_extension *task) {
    struct pi_chain *chain, *tmp;
    unsigned long flags;
    
    if (!task)
        return;
        
    spin_lock_irqsave(&task->pi_state.lock, flags);
    
    // Remove all inheritance chains
    chain = task->pi_state.chains;
    while (chain) {
        tmp = chain->next;
        
        // Restore original priority
        if (chain->owner == task) {
            task->nice = chain->original_prio;
        }
        
        // Remove from inheritance lists
        list_del(&chain->inheritor->pi_node);
        list_del(&chain->owner->pi_node);
        
        kfree(chain);
        chain = tmp;
    }
    
    task->pi_state.chains = NULL;
    task->pi_state.depth = 0;
    
    spin_unlock_irqrestore(&task->pi_state.lock, flags);
}

int check_priority_chain(struct task_extension *task) {
    struct pi_chain *chain;
    int max_prio = task->nice;
    unsigned long flags;
    
    if (!task)
        return -EINVAL;
        
    spin_lock_irqsave(&task->pi_state.lock, flags);
    
    // Find highest priority in inheritance chain
    for (chain = task->pi_state.chains; chain; chain = chain->next) {
        if (chain->inherited_prio < max_prio) {
            max_prio = chain->inherited_prio;
        }
    }
    
    spin_unlock_irqrestore(&task->pi_state.lock, flags);
    return max_prio;
}
