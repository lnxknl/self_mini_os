#include <string.h>
#include <stdlib.h>
#include "hashtable.h"

/* FNV-1a hash constants */
#define FNV_PRIME 16777619
#define FNV_OFFSET_BASIS 2166136261

/* Helper Functions */
static bool is_prime(size_t n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    
    for (size_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return false;
        }
    }
    return true;
}

static size_t next_prime(size_t n) {
    if (n <= 1) return 2;
    
    size_t prime = n;
    bool found = false;
    
    while (!found) {
        prime++;
        if (is_prime(prime)) {
            found = true;
        }
    }
    return prime;
}

static void *allocate_and_copy(const void *src, size_t size) {
    void *dest = malloc(size);
    if (dest) {
        memcpy(dest, src, size);
    }
    return dest;
}

/* Hash Functions */
uint32_t ht_hash_string(const void *key, size_t key_size) {
    const unsigned char *str = (const unsigned char *)key;
    uint32_t hash = FNV_OFFSET_BASIS;
    
    while (*str) {
        hash ^= *str++;
        hash *= FNV_PRIME;
    }
    return hash;
}

uint32_t ht_hash_int(const void *key, size_t key_size) {
    uint32_t k = *(const uint32_t *)key;
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k;
}

uint32_t ht_hash_bytes(const void *key, size_t key_size) {
    const unsigned char *data = (const unsigned char *)key;
    uint32_t hash = FNV_OFFSET_BASIS;
    
    for (size_t i = 0; i < key_size; i++) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

/* Compare Functions */
bool ht_compare_string(const void *key1, const void *key2, size_t key_size) {
    return strcmp((const char *)key1, (const char *)key2) == 0;
}

bool ht_compare_int(const void *key1, const void *key2, size_t key_size) {
    return *(const uint32_t *)key1 == *(const uint32_t *)key2;
}

bool ht_compare_bytes(const void *key1, const void *key2, size_t key_size) {
    return memcmp(key1, key2, key_size) == 0;
}

/* Hash Table Creation/Destruction */
hashtable_t *ht_create(size_t initial_size, hash_func_t hash_func, compare_func_t comp_func) {
    hashtable_t *table = (hashtable_t *)malloc(sizeof(hashtable_t));
    if (!table) return NULL;
    
    /* Ensure size is prime */
    initial_size = next_prime(initial_size ? initial_size : DEFAULT_HASH_SIZE);
    
    table->entries = (hash_entry_t *)calloc(initial_size, sizeof(hash_entry_t));
    if (!table->entries) {
        free(table);
        return NULL;
    }
    
    table->size = initial_size;
    table->count = 0;
    table->hash_func = hash_func ? hash_func : ht_hash_bytes;
    table->comp_func = comp_func ? comp_func : ht_compare_bytes;
    
    return table;
}

void ht_destroy(hashtable_t *table) {
    if (!table) return;
    
    /* Free all entries */
    for (size_t i = 0; i < table->size; i++) {
        if (table->entries[i].flags & ENTRY_OCCUPIED) {
            free(table->entries[i].key);
            free(table->entries[i].value);
        }
    }
    
    free(table->entries);
    free(table);
}

/* Hash Table Operations */
static size_t find_slot(hashtable_t *table, const void *key, size_t key_size, bool *found) {
    uint32_t hash = table->hash_func(key, key_size);
    size_t index = hash % table->size;
    size_t original_index = index;
    size_t deleted_slot = table->size;
    
    *found = false;
    
    do {
        if (table->entries[index].flags == ENTRY_EMPTY) {
            return (deleted_slot != table->size) ? deleted_slot : index;
        }
        
        if (table->entries[index].flags == ENTRY_DELETED) {
            if (deleted_slot == table->size) {
                deleted_slot = index;
            }
        } else if (table->entries[index].key_size == key_size &&
                  table->comp_func(table->entries[index].key, key, key_size)) {
            *found = true;
            return index;
        }
        
        index = (index + 1) % table->size;
    } while (index != original_index);
    
    return (deleted_slot != table->size) ? deleted_slot : original_index;
}

int ht_insert(hashtable_t *table, const void *key, size_t key_size,
              const void *value, size_t value_size) {
    if (!table || !key || !value) return -1;
    
    /* Check load factor and resize if necessary */
    float load_factor = (float)(table->count + 1) / table->size;
    if (load_factor > MAX_LOAD_FACTOR) {
        if (ht_resize(table, table->size * 2) != 0) {
            return -1;
        }
    }
    
    bool found;
    size_t index = find_slot(table, key, key_size, &found);
    
    if (found) {
        /* Update existing entry */
        free(table->entries[index].value);
        table->entries[index].value = allocate_and_copy(value, value_size);
        table->entries[index].value_size = value_size;
        return 0;
    }
    
    /* Create new entry */
    void *key_copy = allocate_and_copy(key, key_size);
    void *value_copy = allocate_and_copy(value, value_size);
    
    if (!key_copy || !value_copy) {
        free(key_copy);
        free(value_copy);
        return -1;
    }
    
    table->entries[index].key = key_copy;
    table->entries[index].value = value_copy;
    table->entries[index].key_size = key_size;
    table->entries[index].value_size = value_size;
    table->entries[index].flags = ENTRY_OCCUPIED;
    table->count++;
    
    return 0;
}

int ht_remove(hashtable_t *table, const void *key, size_t key_size) {
    if (!table || !key) return -1;
    
    bool found;
    size_t index = find_slot(table, key, key_size, &found);
    
    if (!found) return -1;
    
    /* Mark entry as deleted and free memory */
    free(table->entries[index].key);
    free(table->entries[index].value);
    table->entries[index].flags = ENTRY_DELETED;
    table->count--;
    
    /* Check load factor and resize if necessary */
    float load_factor = (float)table->count / table->size;
    if (load_factor < MIN_LOAD_FACTOR && table->size > DEFAULT_HASH_SIZE) {
        ht_resize(table, table->size / 2);
    }
    
    return 0;
}

void *ht_get(hashtable_t *table, const void *key, size_t key_size) {
    if (!table || !key) return NULL;
    
    bool found;
    size_t index = find_slot(table, key, key_size, &found);
    
    if (!found) return NULL;
    
    return table->entries[index].value;
}

bool ht_contains(hashtable_t *table, const void *key, size_t key_size) {
    if (!table || !key) return false;
    
    bool found;
    find_slot(table, key, key_size, &found);
    return found;
}

/* Hash Table Maintenance */
int ht_resize(hashtable_t *table, size_t new_size) {
    if (!table || new_size < 1) return -1;
    
    new_size = next_prime(new_size);
    hash_entry_t *new_entries = (hash_entry_t *)calloc(new_size, sizeof(hash_entry_t));
    if (!new_entries) return -1;
    
    /* Save old entries */
    hash_entry_t *old_entries = table->entries;
    size_t old_size = table->size;
    
    /* Update table */
    table->entries = new_entries;
    table->size = new_size;
    table->count = 0;
    
    /* Reinsert all entries */
    for (size_t i = 0; i < old_size; i++) {
        if (old_entries[i].flags & ENTRY_OCCUPIED) {
            ht_insert(table, old_entries[i].key, old_entries[i].key_size,
                     old_entries[i].value, old_entries[i].value_size);
            free(old_entries[i].key);
            free(old_entries[i].value);
        }
    }
    
    free(old_entries);
    return 0;
}

void ht_clear(hashtable_t *table) {
    if (!table) return;
    
    for (size_t i = 0; i < table->size; i++) {
        if (table->entries[i].flags & ENTRY_OCCUPIED) {
            free(table->entries[i].key);
            free(table->entries[i].value);
            table->entries[i].flags = ENTRY_EMPTY;
        }
    }
    table->count = 0;
}

/* Hash Table Statistics */
size_t ht_count(const hashtable_t *table) {
    return table ? table->count : 0;
}

float ht_load_factor(const hashtable_t *table) {
    return table ? (float)table->count / table->size : 0.0f;
}

/* Iterator Implementation */
void ht_iterator_init(hashtable_t *table, ht_iterator_t *iterator) {
    if (!table || !iterator) return;
    
    iterator->table = table;
    iterator->current_index = 0;
}

hash_entry_t *ht_iterator_next(ht_iterator_t *iterator) {
    if (!iterator || !iterator->table) return NULL;
    
    while (iterator->current_index < iterator->table->size) {
        size_t current = iterator->current_index++;
        if (iterator->table->entries[current].flags & ENTRY_OCCUPIED) {
            return &iterator->table->entries[current];
        }
    }
    
    return NULL;
}

void ht_iterator_reset(ht_iterator_t *iterator) {
    if (iterator) {
        iterator->current_index = 0;
    }
}
