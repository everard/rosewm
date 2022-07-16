// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_90988947122C4A99B7ED48C2EC268033
#define H_90988947122C4A99B7ED48C2EC268033

////////////////////////////////////////////////////////////////////////////////
// Map node's definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_node {
    struct rose_map_node* parent;
    struct rose_map_node* children[2];

    signed char balance;
};

////////////////////////////////////////////////////////////////////////////////
// Map insertion result's definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_insertion_result {
    // Map's new root.
    struct rose_map_node* root;

    // Either newly inserted node, or already existing one with the key which
    // compares equal to the key of the node whose insertion in the map has been
    // attempted.
    struct rose_map_node* node;
};

////////////////////////////////////////////////////////////////////////////////
// Comparison function definitions.
////////////////////////////////////////////////////////////////////////////////

// This function must compare keys of the given nodes.
typedef int (*rose_map_node_comparison_fn)(struct rose_map_node const*,
                                           struct rose_map_node const*);

// This function must compare the given key with the node's key.
typedef int (*rose_map_key_comparison_fn)(void const*,
                                          struct rose_map_node const*);

////////////////////////////////////////////////////////////////////////////////
// Map node insertion/removal interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_insertion_result
rose_map_insert(struct rose_map_node* root, struct rose_map_node* node,
                rose_map_node_comparison_fn compare);

// Note: This function returns map's new root.
struct rose_map_node*
rose_map_remove(struct rose_map_node* root, struct rose_map_node* node);

////////////////////////////////////////////////////////////////////////////////
// Map search interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_node*
rose_map_find(struct rose_map_node* root, void const* k,
              rose_map_key_comparison_fn compare);

struct rose_map_node*
rose_map_lower_bound(struct rose_map_node* root, void const* k,
                     rose_map_key_comparison_fn compare);

////////////////////////////////////////////////////////////////////////////////
// Map boundary access interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_node*
rose_map_lower(struct rose_map_node* root);

struct rose_map_node*
rose_map_upper(struct rose_map_node* root);

////////////////////////////////////////////////////////////////////////////////
// Map iteration interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_map_node*
rose_map_node_obtain_next(struct rose_map_node* node);

struct rose_map_node*
rose_map_node_obtain_prev(struct rose_map_node* node);

#endif // H_90988947122C4A99B7ED48C2EC268033
