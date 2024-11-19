#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/cpumask.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Resource Management Demo");
MODULE_DESCRIPTION("Demonstrates Linux kernel resource management concepts");

/* Memory Management Example */
static void demonstrate_memory_management(void) {
    struct page *page;
    void *mem;
    
    /* Buddy allocator example */
    page = alloc_pages(GFP_KERNEL, 0);  /* Allocate 2^0 = 1 page */
    if (page) {
        pr_info("Successfully allocated a page\n");
        __free_pages(page, 0);
    }
    
    /* Slab allocator example */
    mem = kmalloc(1024, GFP_KERNEL);  /* Allocate 1KB using slab */
    if (mem) {
        pr_info("Successfully allocated 1KB from slab\n");
        kfree(mem);
    }
}

/* Process Management Example */
static void demonstrate_process_management(void) {
    struct task_struct *task = current;
    
    pr_info("Current process: %s (PID: %d)\n", 
            task->comm, task->pid);
    pr_info("Process state: %ld\n", task->state);
    pr_info("CPU allowed: %*pbl\n", 
            cpumask_pr_args(task->cpus_ptr));
}

/* Resource Control Example */
static void demonstrate_resource_control(void) {
    struct task_struct *task = current;
    struct cgroup_subsys_state *css;
    
    rcu_read_lock();
    css = task_css(task, cpu_cgrp_id);
    if (css) {
        pr_info("Process belongs to CPU cgroup\n");
    }
    rcu_read_unlock();
}

/* Device Management Example */
static void demonstrate_device_management(void) {
    struct device *dev;
    
    /* Example of device tree traversal */
    pr_info("Device management demonstration\n");
}

/* Module Init */
static int __init resource_demo_init(void) {
    pr_info("Resource Management Demo Module loaded\n");
    
    demonstrate_memory_management();
    demonstrate_process_management();
    demonstrate_resource_control();
    demonstrate_device_management();
    
    return 0;
}

/* Module Exit */
static void __exit resource_demo_exit(void) {
    pr_info("Resource Management Demo Module unloaded\n");
}

module_init(resource_demo_init);
module_exit(resource_demo_exit);
