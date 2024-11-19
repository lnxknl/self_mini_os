# Linux Kernel Low-Level Primitives

## 1. Atomic Operations

### 1.1 Basic Atomic Types
```c
typedef struct {
    int counter;
} atomic_t;

typedef struct {
    long counter;
} atomic64_t;
```

### 1.2 Atomic Operations API
```c
// Basic operations
void atomic_set(atomic_t *v, int i);
int atomic_read(const atomic_t *v);
void atomic_add(int i, atomic_t *v);
void atomic_sub(int i, atomic_t *v);
void atomic_inc(atomic_t *v);
void atomic_dec(atomic_t *v);

// Test operations
int atomic_dec_and_test(atomic_t *v);
int atomic_inc_and_test(atomic_t *v);
int atomic_add_negative(int i, atomic_t *v);

// Compound operations
int atomic_add_return(int i, atomic_t *v);
int atomic_sub_return(int i, atomic_t *v);
int atomic_inc_return(atomic_t *v);
int atomic_dec_return(atomic_t *v);
```

## 2. Memory Barriers

### 2.1 Basic Memory Barriers
```c
// Full memory barriers
void mb(void);    // Read & Write barrier
void rmb(void);   // Read barrier
void wmb(void);   // Write barrier

// SMP memory barriers
void smp_mb(void);
void smp_rmb(void);
void smp_wmb(void);

// Compiler barriers
void barrier(void);
void smp_read_barrier_depends(void);
```

### 2.2 Architecture-Specific Barriers
```c
// x86 specific
#define mfence() asm volatile("mfence":::"memory")
#define sfence() asm volatile("sfence":::"memory")
#define lfence() asm volatile("lfence":::"memory")
```

## 3. Spinlocks

### 3.1 Basic Spinlock
```c
typedef struct {
    volatile unsigned int slock;
} spinlock_t;

// Operations
void spin_lock_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
int spin_trylock(spinlock_t *lock);
int spin_is_locked(spinlock_t *lock);
```

### 3.2 Reader-Writer Spinlocks
```c
typedef struct {
    volatile unsigned int lock;
} rwlock_t;

// Operations
void read_lock(rwlock_t *lock);
void read_unlock(rwlock_t *lock);
void write_lock(rwlock_t *lock);
void write_unlock(rwlock_t *lock);
```

### 3.3 Raw Spinlocks
```c
typedef struct {
    arch_spinlock_t raw_lock;
} raw_spinlock_t;

// Operations
void raw_spin_lock(raw_spinlock_t *lock);
void raw_spin_unlock(raw_spinlock_t *lock);
int raw_spin_trylock(raw_spinlock_t *lock);
```

## 4. Mutexes

### 4.1 Basic Mutex
```c
struct mutex {
    atomic_long_t owner;
    spinlock_t wait_lock;
    struct list_head wait_list;
};

// Operations
void mutex_init(struct mutex *lock);
void mutex_lock(struct mutex *lock);
void mutex_unlock(struct mutex *lock);
int mutex_trylock(struct mutex *lock);
```

### 4.2 RT Mutex
```c
struct rt_mutex {
    raw_spinlock_t wait_lock;
    struct rb_root_cached waiters;
    struct task_struct *owner;
};

// Operations
void rt_mutex_init(struct rt_mutex *lock);
void rt_mutex_lock(struct rt_mutex *lock);
void rt_mutex_unlock(struct rt_mutex *lock);
```

## 5. Semaphores

### 5.1 Counting Semaphore
```c
struct semaphore {
    raw_spinlock_t lock;
    unsigned int count;
    struct list_head wait_list;
};

// Operations
void sema_init(struct semaphore *sem, int val);
void down(struct semaphore *sem);
int down_trylock(struct semaphore *sem);
void up(struct semaphore *sem);
```

### 5.2 Reader-Writer Semaphore
```c
struct rw_semaphore {
    atomic_long_t count;
    struct list_head wait_list;
    raw_spinlock_t wait_lock;
    struct optimistic_spin_queue osq;
};

// Operations
void down_read(struct rw_semaphore *sem);
void up_read(struct rw_semaphore *sem);
void down_write(struct rw_semaphore *sem);
void up_write(struct rw_semaphore *sem);
```

## 6. RCU (Read-Copy-Update)

### 6.1 Basic RCU API
```c
// Reader-side
rcu_read_lock();
rcu_read_unlock();
rcu_dereference(p);    // Access RCU-protected pointer

// Writer-side
rcu_assign_pointer(p, v);  // Publish new version
synchronize_rcu();         // Wait for readers
call_rcu(&head, callback); // Asynchronous cleanup
```

### 6.2 SRCU (Sleepable RCU)
```c
struct srcu_struct {
    struct srcu_array *per_cpu_ref;
    spinlock_t queue_lock;
    struct list_head batch_queue;
    struct callback_head *batch_check0;
    struct callback_head *batch_check1;
};

// Operations
void srcu_read_lock(struct srcu_struct *sp);
void srcu_read_unlock(struct srcu_struct *sp, int idx);
void synchronize_srcu(struct srcu_struct *sp);
```

## 7. Completion Variables

### 7.1 Basic Completion
```c
struct completion {
    unsigned int done;
    wait_queue_head_t wait;
};

// Operations
void init_completion(struct completion *x);
void wait_for_completion(struct completion *x);
void complete(struct completion *x);
void complete_all(struct completion *x);
```

## 8. Memory Ordering Primitives

### 8.1 Load/Store Barriers
```c
// Load-Load barrier
#define read_barrier_depends() \
    __asm__ __volatile__("" : : : "memory")

// Store-Store barrier
#define wmb() \
    __asm__ __volatile__("sfence" ::: "memory")

// Full barrier
#define mb() \
    __asm__ __volatile__("mfence" ::: "memory")
```

### 8.2 Atomic Exchange Operations
```c
// Compare and swap
int atomic_cmpxchg(atomic_t *v, int old, int new);
long atomic_long_cmpxchg(atomic_long_t *v, long old, long new);

// Exchange
int atomic_xchg(atomic_t *v, int new);
long atomic_long_xchg(atomic_long_t *v, long new);
```

## 9. Per-CPU Variables

### 9.1 Basic Per-CPU
```c
DEFINE_PER_CPU(type, name);
DECLARE_PER_CPU(type, name);

// Access operations
get_cpu_var(var);
put_cpu_var(var);
per_cpu(var, cpu);
```

### 9.2 Per-CPU Operations
```c
void *alloc_percpu(type);
void free_percpu(void *ptr);
per_cpu_ptr(ptr, cpu);
```

## 10. Bit Operations

### 10.1 Atomic Bit Operations
```c
void set_bit(int nr, volatile unsigned long *addr);
void clear_bit(int nr, volatile unsigned long *addr);
void change_bit(int nr, volatile unsigned long *addr);
int test_and_set_bit(int nr, volatile unsigned long *addr);
int test_and_clear_bit(int nr, volatile unsigned long *addr);
int test_and_change_bit(int nr, volatile unsigned long *addr);
```

### 10.2 Non-Atomic Bit Operations
```c
void __set_bit(int nr, volatile unsigned long *addr);
void __clear_bit(int nr, volatile unsigned long *addr);
void __change_bit(int nr, volatile unsigned long *addr);
int __test_and_set_bit(int nr, volatile unsigned long *addr);
int __test_and_clear_bit(int nr, volatile unsigned long *addr);
int __test_and_change_bit(int nr, volatile unsigned long *addr);
```

## 11. Low-Level Timer Mechanisms

### 11.1 Hardware Timer Interface
```c
struct clock_event_device {
    void (*event_handler)(struct clock_event_device *);
    int (*set_next_event)(unsigned long evt, struct clock_event_device *);
    int (*set_state_periodic)(struct clock_event_device *);
    int (*set_state_oneshot)(struct clock_event_device *);
    int (*tick_resume)(struct clock_event_device *);
};
```

### 11.2 High Resolution Timer
```c
struct hrtimer {
    struct timerqueue_node node;
    ktime_t _softexpires;
    enum hrtimer_restart (*function)(struct hrtimer *);
    struct hrtimer_clock_base *base;
    u8 state;
};
```

## 12. Low-Level Interrupt Handling

### 12.1 Hardware IRQ Descriptor
```c
struct irq_desc {
    struct irq_data irq_data;
    unsigned int status_use_accessors;
    struct irq_chip *chip;
    irq_flow_handler_t handle_irq;
    struct irqaction *action;
};
```

### 12.2 Interrupt Controller Interface
```c
struct irq_chip {
    void (*irq_mask)(struct irq_data *data);
    void (*irq_unmask)(struct irq_data *data);
    void (*irq_eoi)(struct irq_data *data);
    int (*irq_set_type)(struct irq_data *data, unsigned int flow_type);
    int (*irq_set_affinity)(struct irq_data *data, const struct cpumask *dest, bool force);
};
```
