// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "map.h"

#include <stdbool.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

// Computes index of the given node relative to its parent.
#define child_index_(node)                                                 \
    ((((node)->parent == NULL) || ((node)->parent->children[0] == (node))) \
         ? 0                                                               \
         : 1)

////////////////////////////////////////////////////////////////////////////////
// Node-manipulation-related utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_map_node_link(struct rose_map_node* parent, struct rose_map_node* child,
                   ptrdiff_t child_i) {
    if(child != NULL) {
        child->parent = parent;
    }

    if(parent != NULL) {
        parent->children[child_i] = child;
    }
}

static struct rose_map_node*
rose_map_node_rotate(struct rose_map_node* x) {
    // Precondition: (x != NULL) && (x->balance != 0).

    ptrdiff_t const a_i = ((x->balance < 0) ? 0 : 1), b_i = ((a_i + 1) % 2),
                    c_i = child_index_(x);

    struct rose_map_node* y = x->children[a_i];
    struct rose_map_node* z = y->children[b_i];

    rose_map_node_link(x->parent, y, c_i);
    rose_map_node_link(x, z, a_i);
    rose_map_node_link(y, x, b_i);

    return y;
}

static struct rose_map_node*
rose_map_node_rebalance(struct rose_map_node* x) {
    // Precondition: (x != NULL) && (|x->balance| > 1).

    struct rose_map_node* y = x->children[(x->balance < 0) ? 0 : 1];
    bool need_double_rotation = ((x->balance < 0) && (y->balance > 0)) ||
                                ((x->balance > 0) && (y->balance < 0));

    if(need_double_rotation) {
        struct rose_map_node* z = rose_map_node_rotate(y);
        rose_map_node_rotate(x);

        switch(z->balance) {
            case 0:
                x->balance = y->balance = 0;
                break;

            case -1:
                if(x->balance < 0) {
                    y->balance = z->balance = 0;
                    x->balance = +1;
                } else {
                    x->balance = z->balance = 0;
                    y->balance = +1;
                }
                break;

            case +1:
                if(x->balance < 0) {
                    x->balance = z->balance = 0;
                    y->balance = -1;
                } else {
                    y->balance = z->balance = 0;
                    x->balance = -1;
                }
                break;
        }

        return z;
    } else {
        switch(rose_map_node_rotate(x)->balance) {
            case -1:
                // fall-through
            case +1:
                x->balance = y->balance = 0;
                break;

            case 0:
                if(x->balance < 0) {
                    x->balance = -1;
                    y->balance = +1;
                } else {
                    x->balance = +1;
                    y->balance = -1;
                }
                break;
        }

        return y;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Map-rebalancing-related utility functions and types.
////////////////////////////////////////////////////////////////////////////////

enum rose_map_rebalance_type {
    rose_map_rebalance_type_insert,
    rose_map_rebalance_type_remove
};

static struct rose_map_node*
rose_map_rebalance(struct rose_map_node* root, struct rose_map_node* node,
                   ptrdiff_t child_i, enum rose_map_rebalance_type type) {
    struct rose_map_node* moved_node = NULL;

    while(node != NULL) {
        if(type == rose_map_rebalance_type_insert) {
            node->balance += ((child_i == 0) ? -1 : +1);
            if(node->balance == 0) {
                break;
            }
        } else {
            node->balance += ((child_i == 0) ? +1 : -1);
            if((node->balance == -1) || (node->balance == +1)) {
                break;
            }
        }

        if((node->balance > 1) || (node->balance < -1)) {
            node = rose_map_node_rebalance(moved_node = node);

            if(type == rose_map_rebalance_type_insert) {
                break;
            } else {
                if(node->balance != 0) {
                    break;
                }
            }
        }

        child_i = child_index_(node);
        node = node->parent;
    }

    return ((root == moved_node) ? root->parent : root);
}

////////////////////////////////////////////////////////////////////////////////
// Map node insertion/removal interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_insertion_result
rose_map_insert(struct rose_map_node* root, struct rose_map_node* node,
                rose_map_node_comparison_fn compare) {
    // Initialize insert operation's result.
    struct rose_map_insertion_result result = {.root = root, .node = node};

    // Initialize the node.
    if(node != NULL) {
        *node = (struct rose_map_node){};
    }

    // If the map is empty, then insert operation is simple.
    if(root == NULL) {
        return (result.root = node), result;
    }

    // Find the position where the node shall be inserted.
    struct rose_map_node* position = root;
    ptrdiff_t child_i = 0;

#define node_eq_(x, y) (compare((x), (y)) == 0)
#define node_gt_(x, y) (compare((x), (y)) > 0)

    while(true) {
        if(node_eq_(node, position)) {
            return (result.node = position), result;
        }

        child_i = node_gt_(node, position);
        if(position->children[child_i] == NULL) {
            break;
        }

        position = position->children[child_i];
    }

#undef node_eq_
#undef node_gt_

    // Perform insert operation, obtain map's new root.
    result.root = (rose_map_node_link(position, node, child_i),
                   rose_map_rebalance(root, position, child_i,
                                      rose_map_rebalance_type_insert));

    return result;
}

struct rose_map_node*
rose_map_remove(struct rose_map_node* root, struct rose_map_node* node) {
    // Do nothing if the map is empty, or if there is no node specified.
    if((root == NULL) || (node == NULL)) {
        return root;
    }

    // Obtain the index of the node relative to its parent.
    ptrdiff_t child_i = child_index_(node);

    // Perform removal operation depending on how many children the node has.
    if((node->children[0] == NULL) || (node->children[1] == NULL)) {
        // Node has at most one child.

        struct rose_map_node* next = // Select non-null child (if any).
            ((node->children[0] != NULL) ? node->children[0]
                                         : node->children[1]);

        if(root == node) {
            if((root = next) != NULL) {
                next->parent = NULL;
            }
        } else {
            root = (rose_map_node_link(node->parent, next, child_i),
                    rose_map_rebalance(root, node->parent, child_i,
                                       rose_map_rebalance_type_remove));
        }
    } else {
        // Node has two children.

        // Find in-order successor.
        struct rose_map_node* next = node->children[1];
        for(; next->children[0] != NULL; next = next->children[0]) {
        }

        // Update the root, if needed.
        root = ((root == node) ? next : root);

        // Update in-order successor's links, and rebalance the tree.
        rose_map_node_link(next, node->children[0], 0);
        next->balance = node->balance;

        if(next->parent == node) {
            root = (rose_map_node_link(node->parent, next, child_i),
                    rose_map_rebalance(
                        root, next, 1, rose_map_rebalance_type_remove));
        } else {
            struct rose_map_node* parent_next = next->parent;
            ptrdiff_t child_i_next = child_index_(next);

            rose_map_node_link(parent_next, next->children[1], child_i_next);
            rose_map_node_link(node->parent, next, child_i);
            rose_map_node_link(next, node->children[1], 1);

            root = rose_map_rebalance(root, parent_next, child_i_next,
                                      rose_map_rebalance_type_remove);
        }
    }

    return root;
}

////////////////////////////////////////////////////////////////////////////////
// Map search interface implementation.
////////////////////////////////////////////////////////////////////////////////

#define key_eq_(k, node) (compare((k), (node)) == 0)
#define key_lt_(k, node) (compare((k), (node)) < 0)
#define key_gt_(k, node) (compare((k), (node)) > 0)

struct rose_map_node*
rose_map_find(struct rose_map_node* root, void const* k,
              rose_map_key_comparison_fn compare) {
    struct rose_map_node* node = root;

    while(node != NULL) {
        if(key_eq_(k, node)) {
            break;
        }

        node = node->children[(ptrdiff_t)key_gt_(k, node)];
    }

    return node;
}

struct rose_map_node*
rose_map_lower_bound(struct rose_map_node* root, void const* k,
                     rose_map_key_comparison_fn compare) {
    struct rose_map_node* node = root;
    struct rose_map_node* prev = node;

    while(node != NULL) {
        if(key_eq_(k, node)) {
            return node;
        }

        prev = node;
        node = node->children[(ptrdiff_t)key_gt_(k, node)];
    }

    if(prev != NULL) {
        return (key_lt_(k, prev) ? prev : rose_map_node_obtain_next(prev));
    }

    return NULL;
}

#undef key_eq_
#undef key_lt_
#undef key_gt_

////////////////////////////////////////////////////////////////////////////////
// Map boundary access interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_node*
rose_map_lower(struct rose_map_node* root) {
    struct rose_map_node* node = root;

    if(node != NULL) {
        for(; node->children[0] != NULL; node = node->children[0]) {
        }
    }

    return node;
}

struct rose_map_node*
rose_map_upper(struct rose_map_node* root) {
    struct rose_map_node* node = root;

    if(node != NULL) {
        for(; node->children[1] != NULL; node = node->children[1]) {
        }
    }

    return node;
}

////////////////////////////////////////////////////////////////////////////////
// Map iteration interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_node*
rose_map_node_obtain_next(struct rose_map_node* node) {
    if(node != NULL) {
        if(node->children[1] != NULL) {
            for(node = node->children[1]; node->children[0] != NULL;
                node = node->children[0]) {
            }
        } else {
            ptrdiff_t child_i = 0;

            do {
                child_i = child_index_(node);
                node = node->parent;
            } while((child_i != 0) && (node != NULL));
        }
    }

    return node;
}

struct rose_map_node*
rose_map_node_obtain_prev(struct rose_map_node* node) {
    if(node != NULL) {
        if(node->children[0] != NULL) {
            for(node = node->children[0]; node->children[1] != NULL;
                node = node->children[1]) {
            }
        } else {
            ptrdiff_t child_i = 0;

            do {
                child_i = child_index_(node);
                node = node->parent;
            } while((child_i != 1) && (node != NULL));
        }
    }

    return node;
}
