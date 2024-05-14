// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/backend.h>
#include <wlr/backend/session.h>

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_input_device.h>

#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

////////////////////////////////////////////////////////////////////////////////
// Keyboard action comparison utility functions.
////////////////////////////////////////////////////////////////////////////////

#define action_shortcut_(type) \
    &(((struct rose_keyboard_##type##_action const*)(action))->shortcut)

static int
rose_keyboard_core_action_compare(void const* key, void const* action) {
    return rose_keyboard_shortcut_compare(key, action_shortcut_(core));
}

static int
rose_keyboard_menu_action_compare(void const* key, void const* action) {
    return rose_keyboard_shortcut_compare(key, action_shortcut_(menu));
}

static int
rose_keyboard_ipc_action_compare(void const* key, void const* action) {
    return rose_keyboard_shortcut_compare(key, action_shortcut_(ipc));
}

#undef action_shortcut_

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_keyboard_key(struct wl_listener* listener, void* data) {
    // Obtain the keyboard.
    struct rose_keyboard* keyboard =
        wl_container_of(listener, keyboard, listener_key);

    // Obtain event data.
    struct wlr_keyboard_key_event* event = data;

    // Obtain the server context.
    struct rose_server_context* context = keyboard->parent->context;

    // Obtain the underlying input device.
    struct wlr_keyboard* device =
        wlr_keyboard_from_input_device(keyboard->parent->device);

    // Compute key code for the key.
    xkb_keycode_t keycode = event->keycode + 8;

#define for_each_keysym_(x, keysyms)                                       \
    for(xkb_keysym_t const *x = (keysyms).data, *end = x + (keysyms).size; \
        x != end; ++x)

    // Manage the session (switch the VT, if needed).
    // Note: User shall be able to switch the VT even if the screen is locked,
    // or keyboard shortcuts are inhibited.
    if(event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // Obtain the session.
        struct wlr_session* session = context->session;

        // If there is a session, then perform additional actions.
        if(session != NULL) {
            // Obtain the effective shift level.
            xkb_level_index_t level =
                xkb_state_key_get_level(device->xkb_state, keycode, 0);

            // Obtain the keysyms.
            struct {
                xkb_keysym_t const* data;
                size_t size;
            } keysyms;

            keysyms.size = xkb_keymap_key_get_syms_by_level(
                context->keyboard_context->keymap_raw, keycode, 0, level,
                &(keysyms.data));

            // Iterate through keysyms array, and check if special keysym has
            // been generated.
            for_each_keysym_(keysym, keysyms) {
                if((*keysym < XKB_KEY_XF86Switch_VT_1) ||
                   (*keysym > XKB_KEY_XF86Switch_VT_12)) {
                    continue;
                }

                // If one of the special keysyms has been generated, then change
                // the VT, and do nothing else.
                wlr_session_change_vt(
                    session, (unsigned)(*keysym - XKB_KEY_XF86Switch_VT_1 + 1));

                return;
            }
        }
    }

    // Obtain the keysyms without any modifiers applied.
    struct {
        xkb_keysym_t const* data;
        size_t size;
    } keysyms;

    keysyms.size = xkb_keymap_key_get_syms_by_level(
        context->keyboard_context->keymap_raw, keycode, 0, 0, &(keysyms.data));

    // Update keyboard's state: add or remove keysyms generated from pressed
    // keys.
    if(event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for_each_keysym_(keysym, keysyms) {
            if(keyboard->keysyms_pressed.size ==
               rose_keyboard_shortcut_size_max) {
                break;
            }

            for(size_t i = 0; i != keyboard->keysyms_pressed.size; ++i) {
                if(*keysym == keyboard->keysyms_pressed.data[i].value) {
                    goto next;
                }
            }

            keyboard->keysyms_pressed.data[keyboard->keysyms_pressed.size++]
                .value = *keysym;

        next:
        }
    } else {
        for_each_keysym_(keysym, keysyms) {
            for(size_t i = 0; i != keyboard->keysyms_pressed.size; ++i) {
                if(*keysym == keyboard->keysyms_pressed.data[i].value) {
                    for(size_t j = i; j < (keyboard->keysyms_pressed.size - 1);
                        ++j) {
                        keyboard->keysyms_pressed.data[j].value =
                            keyboard->keysyms_pressed.data[j + 1].value;
                    }

                    keyboard->keysyms_pressed.size--;
                    break;
                }
            }
        }
    }

    // Update user interaction flag, if needed.
    if(true) {
        // Obtain the leader's keysym.
        xkb_keysym_t target_keysym =
            context->config.keyboard_control_scheme->leader_keysym.value;

        // Check if the leader has been pressed/released.
        for_each_keysym_(keysym, keysyms) {
            if(*keysym == target_keysym) {
                context->is_waiting_for_user_interaction =
                    (event->state == WL_KEYBOARD_KEY_STATE_PRESSED);

                break;
            }
        }
    }

    // If the screen is not locked, and the key is pressed, then process
    // keyboard shortcuts.
    // Note: Screen lock inhibits all types of keyboard actions.
    if((event->state == WL_KEYBOARD_KEY_STATE_PRESSED) &&
       !(context->is_screen_locked)) {
        // Obtain current shortcut based on keyboard's state.
        struct rose_keyboard_shortcut shortcut = {};

        memcpy(
            shortcut.keysyms, keyboard->keysyms_pressed.data,
            keyboard->keysyms_pressed.size *
                sizeof(struct rose_keyboard_keysym));

        // Obtain a core action which corresponds to the shortcut.
        struct rose_keyboard_core_action* core_action = NULL;
        if(true) {
            // Obtain the size of the list of core actions.
            size_t core_action_count =
                context->config.keyboard_control_scheme->core_action_count;

            // Obtain the list of core actions.
            struct rose_keyboard_core_action* core_actions =
                context->config.keyboard_control_scheme->core_actions;

            // Find an action which corresponds to the shortcut.
            core_action = bsearch(
                &shortcut, core_actions, core_action_count,
                sizeof(*core_actions), rose_keyboard_core_action_compare);

            // If shortcuts are inhibited, then only allow the action which
            // toggles said inhibiting.
            if(context->are_keyboard_shortcuts_inhibited &&
               (core_action != NULL)) {
                if(core_action->type !=
                   rose_core_action_type_toggle_keyboard_shortcuts_inhibiting) {
                    core_action = NULL;
                }
            }
        }

        // If a menu is visible, then handle its actions, and do nothing else.
        if((context->current_workspace->output != NULL) &&
           (context->current_workspace->output->ui.menu.is_visible)) {
            // Obtain the menu.
            struct rose_ui_menu* menu =
                &(context->current_workspace->output->ui.menu);

            // Obtain the size of the list of menu actions.
            size_t menu_action_count =
                context->config.keyboard_control_scheme->menu_action_count;

            // Obtain the list of menu actions.
            struct rose_keyboard_menu_action* menu_actions =
                context->config.keyboard_control_scheme->menu_actions;

            // Find an action which corresponds to the shortcut.
            struct rose_keyboard_menu_action* menu_action = bsearch(
                &shortcut, menu_actions, menu_action_count,
                sizeof(*menu_actions), rose_keyboard_menu_action_compare);

            // Execute the menu action, if any.
            if(menu_action != NULL) {
                rose_execute_menu_action(menu, menu_action->type);
            }

            // Hide the menu, if required.
            if((core_action != NULL) &&
               (core_action->type ==
                rose_core_action_type_workspace_toggle_menu)) {
                rose_execute_core_action(context, core_action->type);
            }

            // Do nothing else.
            return;
        }

        // Execute the core action, if any.
        if(core_action != NULL) {
            rose_execute_core_action(context, core_action->type);

            if(core_action->type !=
               rose_core_action_type_switch_keyboard_layout) {
                return;
            }
        }

        // Obtain and send an IPC command which corresponds to the shortcut, if
        // needed.
        if(!(context->are_keyboard_shortcuts_inhibited)) {
            // Obtain the size of the list of IPC actions.
            size_t ipc_action_count =
                context->config.keyboard_control_scheme->ipc_action_count;

            // Obtain the list of IPC actions.
            struct rose_keyboard_ipc_action* ipc_actions =
                context->config.keyboard_control_scheme->ipc_actions;

            // Find an action which corresponds to the shortcut.
            struct rose_keyboard_ipc_action* ipc_action = bsearch(
                &shortcut, ipc_actions, ipc_action_count, sizeof(*ipc_actions),
                rose_keyboard_ipc_action_compare);

            // Execute the IPC action, if any.
            if(ipc_action != NULL) {
                // Dispatch the IPC command which corresponds to the given
                // action, and do nothing else.
                return rose_ipc_server_dispatch_command(
                    context->ipc_server, ipc_action->ipc_command);
            }
        }
    }

#undef for_each_keysym_

    // Notify the seat of this event.
    wlr_seat_set_keyboard(context->seat, device);
    wlr_seat_keyboard_notify_key(
        context->seat, event->time_msec, event->keycode, event->state);
}

static void
rose_handle_event_keyboard_modifiers(struct wl_listener* listener, void* data) {
    // Obtain the keyboard.
    struct rose_keyboard* keyboard =
        wl_container_of(listener, keyboard, listener_modifiers);

    // Obtain the underlying input device.
    struct wlr_keyboard* device = data;

    // Obtain the seat.
    struct wlr_seat* seat = keyboard->parent->context->seat;

    // Notify the seat of this event, if needed.
    if(device == wlr_seat_get_keyboard(seat)) {
        wlr_seat_keyboard_notify_modifiers(seat, &(device->modifiers));
    }
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_keyboard_initialize(
    struct rose_keyboard* keyboard, struct rose_input* parent) {
    // Initialize the keyboard.
    *keyboard = (struct rose_keyboard){.parent = parent};

    // Add it to the list.
    wl_list_insert(&(parent->context->inputs_keyboards), &(keyboard->link));

    // Obtain the underlying input device.
    struct wlr_keyboard* device =
        wlr_keyboard_from_input_device(keyboard->parent->device);

    // Set its keymap.
    wlr_keyboard_set_keymap(device, parent->context->keyboard_context->keymap);

#define add_signal_(f)                                                  \
    {                                                                   \
        keyboard->listener_##f.notify = rose_handle_event_keyboard_##f; \
        wl_signal_add(&(device->events.f), &(keyboard->listener_##f));  \
    }

    // Register listeners.
    add_signal_(key);
    add_signal_(modifiers);

#undef add_signal_

    // Set seat's keyboard, if needed.
    if(wlr_seat_get_keyboard(parent->context->seat) == NULL) {
        wlr_seat_set_keyboard(parent->context->seat, device);
    }

    // Update keyboard focus.
    rose_workspace_make_current(parent->context->current_workspace);
}

void
rose_keyboard_destroy(struct rose_keyboard* keyboard) {
    // Remove the keyboard from the list.
    wl_list_remove(&(keyboard->link));

    // Remove listeners from signals.
    wl_list_remove(&(keyboard->listener_key.link));
    wl_list_remove(&(keyboard->listener_modifiers.link));
}
