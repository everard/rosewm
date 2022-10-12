// Copyright Nezametdinov E. Ildus 2022.
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

struct rose_keyboard_control_scheme {
    struct rose_keyboard_keysym leader_keysym;

    size_t n_core_actions, n_menu_actions, n_ipc_actions;
    struct rose_keyboard_core_action core_actions[2 * rose_n_core_action_types];
    struct rose_keyboard_menu_action menu_actions[2 * rose_n_menu_action_types];
    struct rose_keyboard_ipc_action* ipc_actions;
};

////////////////////////////////////////////////////////////////////////////////
// Keyboard context definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard_context {
    struct xkb_keymap* keymap;
    struct xkb_keymap* keymap_raw;
    unsigned layout_idx, n_layouts;
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
