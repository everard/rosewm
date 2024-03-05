// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_A2A240823BDF420B83340167CDA6465B
#define H_A2A240823BDF420B83340167CDA6465B

#include "ipc_types.h"
#include "action.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct xkb_keymap;

////////////////////////////////////////////////////////////////////////////////
// Keyboard keysym definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard_keysym {
    uint32_t value;
};

////////////////////////////////////////////////////////////////////////////////
// Keyboard shortcut definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_keyboard_shortcut_size_max = 5 };

struct rose_keyboard_shortcut {
    struct rose_keyboard_keysym keysyms[rose_keyboard_shortcut_size_max];
};

////////////////////////////////////////////////////////////////////////////////
// Keyboard actions definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard_core_action {
    struct rose_keyboard_shortcut shortcut;
    enum rose_core_action_type type;
};

struct rose_keyboard_menu_action {
    struct rose_keyboard_shortcut shortcut;
    enum rose_menu_action_type type;
};

struct rose_keyboard_ipc_action {
    struct rose_keyboard_shortcut shortcut;
    struct rose_ipc_command ipc_command;
};

////////////////////////////////////////////////////////////////////////////////
// Keyboard control scheme definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_keyboard_control_scheme_ipc_action_max_count = 255 };

struct rose_keyboard_control_scheme {
    // Keysym used as leader.
    struct rose_keyboard_keysym leader_keysym;

    // Counts of actions of different types.
    size_t core_action_count, menu_action_count, ipc_action_count;

    // Array of core actions.
    struct rose_keyboard_core_action
        core_actions[2 * rose_core_action_type_count_];

    // Array of menu actions.
    struct rose_keyboard_menu_action
        menu_actions[2 * rose_menu_action_type_count_];

    // Array of IPC actions.
    struct rose_keyboard_ipc_action
        ipc_actions[rose_keyboard_control_scheme_ipc_action_max_count];
};

////////////////////////////////////////////////////////////////////////////////
// Keyboard context definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard_context {
    // Keymaps (one is main, another one is used for detecting shortcuts).
    struct xkb_keymap *keymap, *keymap_raw;

    // Current layout index, and total number of layouts in the keymap.
    unsigned layout_index, layout_count;
};

////////////////////////////////////////////////////////////////////////////////
// Keyboard keysym comparison interface.
////////////////////////////////////////////////////////////////////////////////

int
rose_keyboard_keysym_compare(struct rose_keyboard_keysym const* x,
                             struct rose_keyboard_keysym const* y);

////////////////////////////////////////////////////////////////////////////////
// Keyboard shortcut comparison interface.
////////////////////////////////////////////////////////////////////////////////

int
rose_keyboard_shortcut_compare(struct rose_keyboard_shortcut const* x,
                               struct rose_keyboard_shortcut const* y);

////////////////////////////////////////////////////////////////////////////////
// Keyboard control scheme initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard_control_scheme*
rose_keyboard_control_scheme_initialize(char const* file_name);

void
rose_keyboard_control_scheme_destroy(
    struct rose_keyboard_control_scheme* scheme);

////////////////////////////////////////////////////////////////////////////////
// Keyboard context initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard_context*
rose_keyboard_context_initialize(char const* keyboard_layouts);

void
rose_keyboard_context_destroy(struct rose_keyboard_context* context);

#endif // H_A2A240823BDF420B83340167CDA6465B
