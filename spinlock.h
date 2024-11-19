#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>

// Basic spinlock structure
typedef struct {
    volatile uint32_t lock;
} spinlock_t;

// Reader-writer spinlock structure
typedef struct {
    volatile uint32_t readers;     // Number of active readers
    volatile uint32_t writers;     // Number of waiting writers
    volatile bool writing;         // Whether a writer is active
    spinlock_t lock;              // Internal lock for state changes
} rwspinlock_t;

// Raw spinlock structure (architecture-specific)
typedef struct {
    volatile uint32_t raw_lock;
    volatile uint32_t owner_cpu;   // CPU that owns the lock
    volatile uint32_t next_ticket; // Next available ticket
    volatile uint32_t owner_ticket;// Current owner's ticket
} raw_spinlock_t;

// Basic spinlock operations
void spin_lock_init(spinlock_t* lock);
void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);
bool spin_trylock(spinlock_t* lock);
bool spin_is_locked(spinlock_t* lock);

// Reader-writer spinlock operations
void rwspin_lock_init(rwspinlock_t* lock);
void rwspin_read_lock(rwspinlock_t* lock);
void rwspin_read_unlock(rwspinlock_t* lock);
void rwspin_write_lock(rwspinlock_t* lock);
void rwspin_write_unlock(rwspinlock_t* lock);
bool rwspin_try_read_lock(rwspinlock_t* lock);
bool rwspin_try_write_lock(rwspinlock_t* lock);

// Raw spinlock operations (ticket-based)
void raw_spin_lock_init(raw_spinlock_t* lock);
void raw_spin_lock(raw_spinlock_t* lock);
void raw_spin_unlock(raw_spinlock_t* lock);
bool raw_spin_trylock(raw_spinlock_t* lock);
bool raw_spin_is_locked(raw_spinlock_t* lock);

// CPU yield function for spinning
void cpu_relax(void);

// Memory barrier functions
void smp_mb(void);    // Full memory barrier
void smp_rmb(void);   // Read memory barrier
void smp_wmb(void);   // Write memory barrier

#endif // SPINLOCK_H
