#include "spinlock.h"
#include <windows.h>
#include <intrin.h>

// CPU relaxation for efficient spinning
void cpu_relax(void) {
#ifdef _MSC_VER
    YieldProcessor();
#else
    __asm__ volatile("pause" ::: "memory");
#endif
}

// Memory barriers
void smp_mb(void) {
    MemoryBarrier();
}

void smp_rmb(void) {
#ifdef _MSC_VER
    _ReadBarrier();
#else
    __asm__ volatile("lfence" ::: "memory");
#endif
}

void smp_wmb(void) {
#ifdef _MSC_VER
    _WriteBarrier();
#else
    __asm__ volatile("sfence" ::: "memory");
#endif
}

// Basic spinlock implementation
void spin_lock_init(spinlock_t* lock) {
    lock->lock = 0;
}

void spin_lock(spinlock_t* lock) {
    while (1) {
        // Try to acquire the lock
        if (!InterlockedExchange(&lock->lock, 1)) {
            // Successfully acquired
            break;
        }

        // Wait for the lock to be potentially free
        while (lock->lock) {
            cpu_relax();
        }
    }
    smp_mb();  // Full memory barrier after acquiring
}

void spin_unlock(spinlock_t* lock) {
    smp_mb();  // Full memory barrier before releasing
    lock->lock = 0;
}

bool spin_trylock(spinlock_t* lock) {
    if (!InterlockedExchange(&lock->lock, 1)) {
        smp_mb();  // Memory barrier on success
        return true;
    }
    return false;
}

bool spin_is_locked(spinlock_t* lock) {
    return (lock->lock != 0);
}

// Reader-writer spinlock implementation
void rwspin_lock_init(rwspinlock_t* lock) {
    lock->readers = 0;
    lock->writers = 0;
    lock->writing = false;
    spin_lock_init(&lock->lock);
}

void rwspin_read_lock(rwspinlock_t* lock) {
    while (1) {
        spin_lock(&lock->lock);
        if (!lock->writing && lock->writers == 0) {
            // Safe to read
            lock->readers++;
            spin_unlock(&lock->lock);
            break;
        }
        spin_unlock(&lock->lock);
        cpu_relax();
    }
    smp_mb();
}

void rwspin_read_unlock(rwspinlock_t* lock) {
    smp_mb();
    spin_lock(&lock->lock);
    lock->readers--;
    spin_unlock(&lock->lock);
}

void rwspin_write_lock(rwspinlock_t* lock) {
    // Indicate write intention
    spin_lock(&lock->lock);
    lock->writers++;
    spin_unlock(&lock->lock);

    while (1) {
        spin_lock(&lock->lock);
        if (!lock->writing && lock->readers == 0) {
            // Safe to write
            lock->writing = true;
            spin_unlock(&lock->lock);
            break;
        }
        spin_unlock(&lock->lock);
        cpu_relax();
    }
    smp_mb();
}

void rwspin_write_unlock(rwspinlock_t* lock) {
    smp_mb();
    spin_lock(&lock->lock);
    lock->writing = false;
    lock->writers--;
    spin_unlock(&lock->lock);
}

bool rwspin_try_read_lock(rwspinlock_t* lock) {
    spin_lock(&lock->lock);
    if (!lock->writing && lock->writers == 0) {
        lock->readers++;
        spin_unlock(&lock->lock);
        smp_mb();
        return true;
    }
    spin_unlock(&lock->lock);
    return false;
}

bool rwspin_try_write_lock(rwspinlock_t* lock) {
    spin_lock(&lock->lock);
    if (!lock->writing && lock->readers == 0) {
        lock->writing = true;
        lock->writers++;
        spin_unlock(&lock->lock);
        smp_mb();
        return true;
    }
    spin_unlock(&lock->lock);
    return false;
}

// Raw spinlock implementation (ticket-based)
void raw_spin_lock_init(raw_spinlock_t* lock) {
    lock->raw_lock = 0;
    lock->owner_cpu = (uint32_t)-1;
    lock->next_ticket = 0;
    lock->owner_ticket = 0;
}

void raw_spin_lock(raw_spinlock_t* lock) {
    uint32_t ticket = InterlockedIncrement(&lock->next_ticket) - 1;
    
    // Wait for our turn
    while (lock->owner_ticket != ticket) {
        cpu_relax();
    }
    
    // We got the lock
    lock->owner_cpu = GetCurrentProcessorNumber();
    smp_mb();
}

void raw_spin_unlock(raw_spinlock_t* lock) {
    smp_mb();
    lock->owner_cpu = (uint32_t)-1;
    InterlockedIncrement(&lock->owner_ticket);
}

bool raw_spin_trylock(raw_spinlock_t* lock) {
    uint32_t ticket = lock->next_ticket;
    if (ticket == lock->owner_ticket) {
        uint32_t new_ticket = ticket + 1;
        if (InterlockedCompareExchange(&lock->next_ticket, new_ticket, ticket) == ticket) {
            lock->owner_cpu = GetCurrentProcessorNumber();
            smp_mb();
            return true;
        }
    }
    return false;
}

bool raw_spin_is_locked(raw_spinlock_t* lock) {
    return (lock->next_ticket != lock->owner_ticket);
}
