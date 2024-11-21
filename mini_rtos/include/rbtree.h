#ifndef RBTREE_H
#define RBTREE_H

#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"

/* Color definition for Red-Black Tree nodes */
typedef enum {
    RB_BLACK = 0,
    RB_RED = 1
} rb_color_t;

/* Red-Black Tree Node Structure */
typedef struct rb_node {
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
    rb_color_t color;
    tcb_t *task;           /* Task Control Block */
    uint32_t key;          /* Key for ordering (can be priority, deadline, etc.) */
} rb_node_t;

/* Red-Black Tree Structure */
typedef struct {
    rb_node_t *root;
    rb_node_t *nil;        /* Sentinel node */
    uint32_t size;         /* Number of nodes in tree */
    uint32_t (*get_key)(tcb_t *task);  /* Key extraction function */
} rb_tree_t;

/* Tree Operations */
void rb_init(rb_tree_t *tree, uint32_t (*key_func)(tcb_t *task));
void rb_insert(rb_tree_t *tree, tcb_t *task);
void rb_delete(rb_tree_t *tree, rb_node_t *node);
rb_node_t *rb_minimum(rb_tree_t *tree, rb_node_t *node);
rb_node_t *rb_maximum(rb_tree_t *tree, rb_node_t *node);
rb_node_t *rb_successor(rb_tree_t *tree, rb_node_t *node);
rb_node_t *rb_predecessor(rb_tree_t *tree, rb_node_t *node);

/* Search Operations */
rb_node_t *rb_search(rb_tree_t *tree, uint32_t key);
rb_node_t *rb_find_task(rb_tree_t *tree, tcb_t *task);

/* Tree Traversal */
void rb_inorder_walk(rb_tree_t *tree, void (*visit)(rb_node_t *node));
void rb_preorder_walk(rb_tree_t *tree, void (*visit)(rb_node_t *node));

/* Utility Functions */
uint32_t rb_size(rb_tree_t *tree);
bool rb_is_empty(rb_tree_t *tree);
void rb_clear(rb_tree_t *tree);

#endif /* RBTREE_H */
