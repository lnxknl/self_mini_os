#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Hash table entry flags */
#define ENTRY_EMPTY     0x00
#define ENTRY_OCCUPIED  0x01
#define ENTRY_DELETED   0x02

/* Default hash table size */
#define DEFAULT_HASH_SIZE   256
#define MAX_LOAD_FACTOR     0.75f
#define MIN_LOAD_FACTOR     0.25f

/* Hash function type */
typedef uint32_t (*hash_func_t)(const void *key, size_t key_size);

/* Compare function type */
typedef bool (*compare_func_t)(const void *key1, const void *key2, size_t key_size);

/* Hash table entry structure */
typedef struct {
    void *key;          /* Key pointer */
    void *value;        /* Value pointer */
    size_t key_size;    /* Size of the key */
    size_t value_size;  /* Size of the value */
    uint8_t flags;      /* Entry status flags */
} hash_entry_t;

/* Hash table structure */
typedef struct {
    hash_entry_t *entries;      /* Array of entries */
    size_t size;               /* Current size of the table */
    size_t count;              /* Number of occupied entries */
    hash_func_t hash_func;     /* Hash function */
    compare_func_t comp_func;  /* Key comparison function */
} hashtable_t;

/* Hash table creation and destruction */
hashtable_t *ht_create(size_t initial_size, 
                      hash_func_t hash_func, 
                      compare_func_t comp_func);
void ht_destroy(hashtable_t *table);

/* Hash table operations */
int ht_insert(hashtable_t *table, 
              const void *key, size_t key_size,
              const void *value, size_t value_size);
int ht_remove(hashtable_t *table, const void *key, size_t key_size);
void *ht_get(hashtable_t *table, const void *key, size_t key_size);
bool ht_contains(hashtable_t *table, const void *key, size_t key_size);

/* Hash table maintenance */
int ht_resize(hashtable_t *table, size_t new_size);
void ht_clear(hashtable_t *table);

/* Hash table statistics */
size_t ht_count(const hashtable_t *table);
float ht_load_factor(const hashtable_t *table);

/* Built-in hash functions */
uint32_t ht_hash_string(const void *key, size_t key_size);
uint32_t ht_hash_int(const void *key, size_t key_size);
uint32_t ht_hash_bytes(const void *key, size_t key_size);

/* Built-in compare functions */
bool ht_compare_string(const void *key1, const void *key2, size_t key_size);
bool ht_compare_int(const void *key1, const void *key2, size_t key_size);
bool ht_compare_bytes(const void *key1, const void *key2, size_t key_size);

/* Iterator support */
typedef struct {
    hashtable_t *table;
    size_t current_index;
} ht_iterator_t;

void ht_iterator_init(hashtable_t *table, ht_iterator_t *iterator);
hash_entry_t *ht_iterator_next(ht_iterator_t *iterator);
void ht_iterator_reset(ht_iterator_t *iterator);

#endif /* HASHTABLE_H */
