// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_F02F67582A6840809A3C21E637B74BD8
#define H_F02F67582A6840809A3C21E637B74BD8

#include "keyboard_context.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_input;

////////////////////////////////////////////////////////////////////////////////
// Keyboard definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard {
    // Pointer to the parent input device.
    struct rose_input* parent;

    // Keyboard's state which holds keysyms generated from pressed keys.
    struct {
        struct rose_keyboard_keysym data[rose_keyboard_shortcut_size_max];
        size_t size;
    } keysyms_pressed;

    // Event listeners.
    struct wl_listener listener_key;
    struct wl_listener listener_modifiers;

    // List link.
    struct wl_list link;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_keyboard_initialize(
    struct rose_keyboard* keyboard, struct rose_input* parent);

void
rose_keyboard_destroy(struct rose_keyboard* keyboard);

#endif // H_F02F67582A6840809A3C21E637B74BD8
