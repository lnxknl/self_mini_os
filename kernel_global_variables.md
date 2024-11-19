# Most Important Global Variables in the Linux Kernel

## 1. Process Management

### `init_task`
- Type: `struct task_struct`
- Purpose: The first process (PID 1)
- Usage: Root of process tree
- Location: `init/init_task.c`
- Critical for: System initialization

### `current`
- Type: Per-CPU pointer to `struct task_struct`
- Purpose: Points to currently running task
- Usage: Process context identification
- Location: Architecture-specific
- Access: Through `current` macro

### `runqueues`
- Type: Per-CPU `struct rq`
- Purpose: CPU run queues
- Usage: Process scheduling
- Location: `kernel/sched/core.c`
- Critical for: Task scheduling

## 2. Memory Management

### `mem_map`
- Type: Array of `struct page`
- Purpose: Physical page descriptors
- Usage: Page frame management
- Location: `mm/page_alloc.c`
- Critical for: Memory allocation

### `vmlist`
- Type: `struct vm_struct *`
- Purpose: Kernel virtual memory areas
- Usage: vmalloc space management
- Location: `mm/vmalloc.c`
- Critical for: Virtual memory allocation

### `pgdat_list`
- Type: List of `struct pglist_data`
- Purpose: NUMA node memory
- Usage: Memory zone management
- Location: `mm/page_alloc.c`
- Critical for: NUMA support

## 3. File System

### `super_blocks`
- Type: List of `struct super_block`
- Purpose: Mounted filesystems
- Usage: Filesystem management
- Location: `fs/super.c`
- Critical for: VFS operations

### `file_systems`
- Type: List of `struct file_system_type`
- Purpose: Registered filesystems
- Usage: Filesystem type management
- Location: `fs/filesystems.c`
- Critical for: Filesystem registration

### `inodes_stat`
- Type: `struct inodes_stat_t`
- Purpose: Inode statistics
- Usage: Inode management
- Location: `fs/inode.c`
- Critical for: Inode tracking

## 4. Network Stack

### `dev_base`
- Type: List of `struct net_device`
- Purpose: Network interfaces
- Usage: Network device management
- Location: `net/core/dev.c`
- Critical for: Network operations

### `tcp_hashinfo`
- Type: `struct tcp_hashinfo`
- Purpose: TCP connection tracking
- Usage: TCP socket management
- Location: `net/ipv4/tcp_ipv4.c`
- Critical for: TCP/IP stack

### `netns_ids`
- Type: `struct ida`
- Purpose: Network namespace IDs
- Usage: Network namespace management
- Location: `net/core/net_namespace.c`
- Critical for: Network isolation

## 5. Block Layer

### `major_names`
- Type: Array of `struct blk_major_name`
- Purpose: Block device major numbers
- Usage: Device number management
- Location: `block/genhd.c`
- Critical for: Block device registration

### `all_bdevs`
- Type: List of `struct block_device`
- Purpose: Block devices
- Usage: Block device management
- Location: `block/genhd.c`
- Critical for: Block I/O

## 6. Time Management

### `jiffies`
- Type: Unsigned long
- Purpose: System uptime in ticks
- Usage: Timing and delays
- Location: `kernel/time/timer.c`
- Critical for: Time tracking

### `xtime`
- Type: `struct timekeeper`
- Purpose: Current time
- Usage: System time management
- Location: `kernel/time/timekeeping.c`
- Critical for: Time keeping

## 7. Interrupt Management

### `irq_desc`
- Type: Array of `struct irq_desc`
- Purpose: IRQ descriptors
- Usage: Interrupt management
- Location: `kernel/irq/handle.c`
- Critical for: Interrupt handling

### `softirq_vec`
- Type: Array of `struct softirq_action`
- Purpose: Softirq handlers
- Usage: Soft interrupt management
- Location: `kernel/softirq.c`
- Critical for: Deferred work

## 8. Per-CPU Variables

### `per_cpu_offset`
- Type: Array of offsets
- Purpose: Per-CPU area addressing
- Usage: Per-CPU variable access
- Location: `arch/x86/kernel/setup_percpu.c`
- Critical for: Per-CPU data

### `cpu_online_mask`
- Type: `struct cpumask`
- Purpose: Online CPU tracking
- Usage: CPU hotplug management
- Location: `kernel/cpu.c`
- Critical for: SMP operations

## 9. System State

### `system_state`
- Type: `int`
- Purpose: System boot state
- Usage: Boot process tracking
- Location: `kernel/main.c`
- Critical for: Boot sequence

### `panic_notifiers`
- Type: `struct atomic_notifier_head`
- Purpose: Panic handlers
- Usage: Crash handling
- Location: `kernel/panic.c`
- Critical for: System crash handling

## Important Considerations

### 1. Access Patterns
- Most globals are protected by locks
- Some use RCU for read access
- Per-CPU variables avoid locking
- Memory barriers often required

### 2. Initialization
- Many initialized at boot time
- Some dynamically allocated
- Boot sequence dependent
- Architecture-specific setup

### 3. Synchronization
- Lock protection required
- RCU for read-heavy data
- Atomic operations
- Memory ordering rules

### 4. Performance Impact
- Cache line sharing
- False sharing prevention
- NUMA considerations
- Access patterns optimization

### 5. Debugging Challenges
- State tracking complexity
- Race condition detection
- Memory corruption issues
- Initialization ordering

## Best Practices

1. **Access Protection**
   - Use appropriate locks
   - Consider RCU where applicable
   - Respect memory barriers
   - Check initialization state

2. **Performance Considerations**
   - Minimize global variable use
   - Prefer per-CPU variables
   - Consider cache effects
   - Use appropriate access patterns

3. **Debugging Support**
   - Add state validation
   - Include debug information
   - Maintain consistency checks
   - Support crash dump analysis

4. **Documentation**
   - Document locking requirements
   - Specify access patterns
   - Note initialization requirements
   - Describe usage constraints
