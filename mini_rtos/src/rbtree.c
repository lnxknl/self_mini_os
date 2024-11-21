#include <stddef.h>
#include <stdlib.h>
#include "rbtree.h"

/* Helper Functions */
static rb_node_t *rb_create_node(rb_tree_t *tree, tcb_t *task) {
    rb_node_t *node = (rb_node_t *)malloc(sizeof(rb_node_t));
    if (!node) return NULL;
    
    node->parent = tree->nil;
    node->left = tree->nil;
    node->right = tree->nil;
    node->color = RB_RED;
    node->task = task;
    node->key = tree->get_key(task);
    
    return node;
}

static void rb_left_rotate(rb_tree_t *tree, rb_node_t *x) {
    rb_node_t *y = x->right;
    x->right = y->left;
    
    if (y->left != tree->nil) {
        y->left->parent = x;
    }
    
    y->parent = x->parent;
    
    if (x->parent == tree->nil) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    
    y->left = x;
    x->parent = y;
}

static void rb_right_rotate(rb_tree_t *tree, rb_node_t *y) {
    rb_node_t *x = y->left;
    y->left = x->right;
    
    if (x->right != tree->nil) {
        x->right->parent = y;
    }
    
    x->parent = y->parent;
    
    if (y->parent == tree->nil) {
        tree->root = x;
    } else if (y == y->parent->right) {
        y->parent->right = x;
    } else {
        y->parent->left = x;
    }
    
    x->right = y;
    y->parent = x;
}

static void rb_insert_fixup(rb_tree_t *tree, rb_node_t *z) {
    while (z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            rb_node_t *y = z->parent->parent->right;
            
            if (y->color == RB_RED) {
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rb_left_rotate(tree, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rb_right_rotate(tree, z->parent->parent);
            }
        } else {
            rb_node_t *y = z->parent->parent->left;
            
            if (y->color == RB_RED) {
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rb_right_rotate(tree, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rb_left_rotate(tree, z->parent->parent);
            }
        }
    }
    tree->root->color = RB_BLACK;
}

static void rb_transplant(rb_tree_t *tree, rb_node_t *u, rb_node_t *v) {
    if (u->parent == tree->nil) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

static void rb_delete_fixup(rb_tree_t *tree, rb_node_t *x) {
    while (x != tree->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            rb_node_t *w = x->parent->right;
            
            if (w->color == RB_RED) {
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rb_left_rotate(tree, x->parent);
                w = x->parent->right;
            }
            
            if (w->left->color == RB_BLACK && w->right->color == RB_BLACK) {
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->right->color == RB_BLACK) {
                    w->left->color = RB_BLACK;
                    w->color = RB_RED;
                    rb_right_rotate(tree, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->right->color = RB_BLACK;
                rb_left_rotate(tree, x->parent);
                x = tree->root;
            }
        } else {
            rb_node_t *w = x->parent->left;
            
            if (w->color == RB_RED) {
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rb_right_rotate(tree, x->parent);
                w = x->parent->left;
            }
            
            if (w->right->color == RB_BLACK && w->left->color == RB_BLACK) {
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->left->color == RB_BLACK) {
                    w->right->color = RB_BLACK;
                    w->color = RB_RED;
                    rb_left_rotate(tree, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->left->color = RB_BLACK;
                rb_right_rotate(tree, x->parent);
                x = tree->root;
            }
        }
    }
    x->color = RB_BLACK;
}

/* Public Interface Implementation */
void rb_init(rb_tree_t *tree, uint32_t (*key_func)(tcb_t *task)) {
    tree->nil = (rb_node_t *)malloc(sizeof(rb_node_t));
    tree->nil->color = RB_BLACK;
    tree->nil->left = NULL;
    tree->nil->right = NULL;
    tree->nil->parent = NULL;
    tree->nil->task = NULL;
    
    tree->root = tree->nil;
    tree->size = 0;
    tree->get_key = key_func;
}

void rb_insert(rb_tree_t *tree, tcb_t *task) {
    rb_node_t *z = rb_create_node(tree, task);
    if (!z) return;
    
    rb_node_t *y = tree->nil;
    rb_node_t *x = tree->root;
    
    while (x != tree->nil) {
        y = x;
        if (z->key < x->key) {
            x = x->left;
        } else {
            x = x->right;
        }
    }
    
    z->parent = y;
    
    if (y == tree->nil) {
        tree->root = z;
    } else if (z->key < y->key) {
        y->left = z;
    } else {
        y->right = z;
    }
    
    tree->size++;
    rb_insert_fixup(tree, z);
}

void rb_delete(rb_tree_t *tree, rb_node_t *z) {
    rb_node_t *y = z;
    rb_node_t *x;
    rb_color_t y_original_color = y->color;
    
    if (z->left == tree->nil) {
        x = z->right;
        rb_transplant(tree, z, z->right);
    } else if (z->right == tree->nil) {
        x = z->left;
        rb_transplant(tree, z, z->left);
    } else {
        y = rb_minimum(tree, z->right);
        y_original_color = y->color;
        x = y->right;
        
        if (y->parent == z) {
            x->parent = y;
        } else {
            rb_transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        
        rb_transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }
    
    if (y_original_color == RB_BLACK) {
        rb_delete_fixup(tree, x);
    }
    
    free(z);
    tree->size--;
}

rb_node_t *rb_minimum(rb_tree_t *tree, rb_node_t *node) {
    while (node->left != tree->nil) {
        node = node->left;
    }
    return node;
}

rb_node_t *rb_maximum(rb_tree_t *tree, rb_node_t *node) {
    while (node->right != tree->nil) {
        node = node->right;
    }
    return node;
}

rb_node_t *rb_successor(rb_tree_t *tree, rb_node_t *x) {
    if (x->right != tree->nil) {
        return rb_minimum(tree, x->right);
    }
    
    rb_node_t *y = x->parent;
    while (y != tree->nil && x == y->right) {
        x = y;
        y = y->parent;
    }
    return y;
}

rb_node_t *rb_predecessor(rb_tree_t *tree, rb_node_t *x) {
    if (x->left != tree->nil) {
        return rb_maximum(tree, x->left);
    }
    
    rb_node_t *y = x->parent;
    while (y != tree->nil && x == y->left) {
        x = y;
        y = y->parent;
    }
    return y;
}

rb_node_t *rb_search(rb_tree_t *tree, uint32_t key) {
    rb_node_t *x = tree->root;
    
    while (x != tree->nil && key != x->key) {
        if (key < x->key) {
            x = x->left;
        } else {
            x = x->right;
        }
    }
    
    return (x != tree->nil) ? x : NULL;
}

rb_node_t *rb_find_task(rb_tree_t *tree, tcb_t *task) {
    rb_node_t *x = tree->root;
    uint32_t key = tree->get_key(task);
    
    while (x != tree->nil) {
        if (task == x->task) {
            return x;
        }
        if (key < x->key) {
            x = x->left;
        } else {
            x = x->right;
        }
    }
    
    return NULL;
}

void rb_inorder_walk(rb_tree_t *tree, void (*visit)(rb_node_t *node)) {
    void rb_inorder_walk_recursive(rb_tree_t *tree, rb_node_t *node, void (*visit)(rb_node_t *node)) {
        if (node != tree->nil) {
            rb_inorder_walk_recursive(tree, node->left, visit);
            visit(node);
            rb_inorder_walk_recursive(tree, node->right, visit);
        }
    }
    
    rb_inorder_walk_recursive(tree, tree->root, visit);
}

void rb_preorder_walk(rb_tree_t *tree, void (*visit)(rb_node_t *node)) {
    void rb_preorder_walk_recursive(rb_tree_t *tree, rb_node_t *node, void (*visit)(rb_node_t *node)) {
        if (node != tree->nil) {
            visit(node);
            rb_preorder_walk_recursive(tree, node->left, visit);
            rb_preorder_walk_recursive(tree, node->right, visit);
        }
    }
    
    rb_preorder_walk_recursive(tree, tree->root, visit);
}

uint32_t rb_size(rb_tree_t *tree) {
    return tree->size;
}

bool rb_is_empty(rb_tree_t *tree) {
    return tree->root == tree->nil;
}

void rb_clear(rb_tree_t *tree) {
    void rb_clear_recursive(rb_tree_t *tree, rb_node_t *node) {
        if (node != tree->nil) {
            rb_clear_recursive(tree, node->left);
            rb_clear_recursive(tree, node->right);
            free(node);
        }
    }
    
    rb_clear_recursive(tree, tree->root);
    tree->root = tree->nil;
    tree->size = 0;
}
