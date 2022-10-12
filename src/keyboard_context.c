// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "keyboard_context.h"

#include <xkbcommon/xkbcommon.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Default keyboard control scheme definition.
////////////////////////////////////////////////////////////////////////////////

static struct rose_keyboard_control_scheme const
    rose_keyboard_default_control_scheme = {
        .leader_keysym = {XKB_KEY_Super_L},
        .n_core_actions = rose_n_core_action_types,
        .n_menu_actions = rose_n_menu_action_types,

        .core_actions =
            {{.shortcut = {{{0}, {XKB_KEY_Shift_L}, {XKB_KEY_q}}},
              .type = rose_core_action_type_terminate_display},

             {.shortcut = {{{XKB_KEY_Control_L}, {XKB_KEY_Shift_L}}},
              .type = rose_core_action_type_switch_keyboard_layout},

             {.shortcut = {{{0}, {XKB_KEY_b}}},
              .type =
                  rose_core_action_type_toggle_keyboard_shortcuts_inhibiting},

             {.shortcut = {{{0}, {XKB_KEY_q}}},
              .type = rose_core_action_type_surface_close},

             {.shortcut = {{{0}, {XKB_KEY_a}}},
              .type = rose_core_action_type_surface_focus_prev},

             {.shortcut = {{{0}, {XKB_KEY_s}}},
              .type = rose_core_action_type_surface_focus_next},

             {.shortcut = {{{0}, {XKB_KEY_d}}},
              .type = rose_core_action_type_surface_toggle_maximized},

             {.shortcut = {{{0}, {XKB_KEY_f}}},
              .type = rose_core_action_type_surface_toggle_fullscreen},

             {.shortcut = {{{0}, {XKB_KEY_Shift_L}, {XKB_KEY_e}}},
              .type = rose_core_action_type_surface_move_to_workspace_new},

             {.shortcut = {{{0}, {XKB_KEY_Shift_L}, {XKB_KEY_w}}},
              .type = rose_core_action_type_surface_move_to_workspace},

             {.shortcut = {{{0}, {XKB_KEY_Shift_L}, {XKB_KEY_o}}},
              .type = rose_core_action_type_surface_move_to_output},

             {.shortcut = {{{0}, {XKB_KEY_w}}},
              .type = rose_core_action_type_workspace_add},

             {.shortcut = {{{0}, {XKB_KEY_Shift_L}, {XKB_KEY_z}}},
              .type = rose_core_action_type_workspace_move},

             {.shortcut = {{{0}, {XKB_KEY_z}}},
              .type = rose_core_action_type_workspace_focus_prev},

             {.shortcut = {{{0}, {XKB_KEY_x}}},
              .type = rose_core_action_type_workspace_focus_next},

             {.shortcut = {{{0}, {XKB_KEY_p}}},
              .type = rose_core_action_type_workspace_toggle_panel},

             {.shortcut = {{{0}, {XKB_KEY_Tab}}},
              .type = rose_core_action_type_workspace_toggle_menu},

             {.shortcut = {{{0}, {XKB_KEY_Return}}},
              .type = rose_core_action_type_run_terminal},

             {.shortcut = {{{0}, {XKB_KEY_Shift_L}, {XKB_KEY_Return}}},
              .type = rose_core_action_type_run_terminal_ipc}},

        .menu_actions = {{.shortcut = {{{XKB_KEY_Up}}},
                          .type = rose_menu_action_type_move_mark_up},

                         {.shortcut = {{{XKB_KEY_Down}}},
                          .type = rose_menu_action_type_move_mark_down},

                         {.shortcut = {{{XKB_KEY_Page_Up}}},
                          .type = rose_menu_action_type_move_page_up},

                         {.shortcut = {{{XKB_KEY_Page_Down}}},
                          .type = rose_menu_action_type_move_page_down},

                         {.shortcut = {{{XKB_KEY_Escape}}},
                          .type = rose_menu_action_type_cancel},

                         {.shortcut = {{{XKB_KEY_Return}}},
                          .type = rose_menu_action_type_commit},

                         {.shortcut = {{{XKB_KEY_space}}},
                          .type = rose_menu_action_type_select},

                         {.shortcut = {{{XKB_KEY_Tab}}},
                          .type = rose_menu_action_type_switch_line_type}}};

////////////////////////////////////////////////////////////////////////////////
// IO-related utility functions and definitions.
////////////////////////////////////////////////////////////////////////////////

enum { rose_n_keyboard_ipc_actions_max = 256 };

static uint32_t
rose_keyboard_control_scheme_unpack_uint32(
    unsigned char buf[static sizeof(uint32_t)]) {
    // Unpack the value starting from the least significant byte to the most
    // significant byte.
    uint32_t x = 0;
    for(size_t i = 0; i != sizeof(uint32_t); ++i) {
        x |= ((uint32_t)(buf[i])) << ((uint32_t)(i * CHAR_BIT));
    }

    return x;
}

static struct rose_keyboard_shortcut
rose_keyboard_control_scheme_read_shortcut(FILE* file) {
    // Read the shortcut to a buffer.
    unsigned char buf[rose_keyboard_shortcut_size_max * sizeof(uint32_t)] = {};
    fread(buf, sizeof(buf), 1, file);

    // Unpack the shortcut keysym-by-keysym.
    struct rose_keyboard_shortcut shortcut = {};
    for(size_t i = 0; i != rose_keyboard_shortcut_size_max; ++i) {
        shortcut.keysyms[i].value = rose_keyboard_control_scheme_unpack_uint32(
            buf + i * sizeof(uint32_t));
    }

    return shortcut;
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard action comparison utility functions.
////////////////////////////////////////////////////////////////////////////////

#define action_shortcut_(var, type) \
    &(((struct rose_keyboard_##type##_action const*)(var))->shortcut)

static int
rose_keyboard_control_scheme_compare_core_actions(void const* x,
                                                  void const* y) {
    return rose_keyboard_shortcut_compare(
        action_shortcut_(x, core), action_shortcut_(y, core));
}

static int
rose_keyboard_control_scheme_compare_menu_actions(void const* x,
                                                  void const* y) {
    return rose_keyboard_shortcut_compare(
        action_shortcut_(x, menu), action_shortcut_(y, menu));
}

static int
rose_keyboard_control_scheme_compare_ipc_actions(void const* x, void const* y) {
    return rose_keyboard_shortcut_compare(
        action_shortcut_(x, ipc), action_shortcut_(y, ipc));
}

#undef action_shortcut_

////////////////////////////////////////////////////////////////////////////////
// Keyboard keysym comparison interface implementation.
////////////////////////////////////////////////////////////////////////////////

int
rose_keyboard_keysym_compare(struct rose_keyboard_keysym const* x,
                             struct rose_keyboard_keysym const* y) {
    return ((x->value == y->value) ? 0 : ((x->value < y->value) ? -1 : 1));
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard shortcut comparison interface implementation.
////////////////////////////////////////////////////////////////////////////////

int
rose_keyboard_shortcut_compare(struct rose_keyboard_shortcut const* x,
                               struct rose_keyboard_shortcut const* y) {
    for(ptrdiff_t i = 0; i != rose_keyboard_shortcut_size_max; ++i) {
        int r =
            rose_keyboard_keysym_compare(&(x->keysyms[i]), &(y->keysyms[i]));

        if(r != 0) {
            return r;
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard control scheme initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard_control_scheme*
rose_keyboard_control_scheme_initialize(char const* file_name) {
    // Allocate and initialize a new control scheme.
    struct rose_keyboard_control_scheme* scheme =
        malloc(sizeof(struct rose_keyboard_control_scheme));

    if(scheme == NULL) {
        return scheme;
    } else {
        *scheme = rose_keyboard_default_control_scheme;
    }

    // Load the scheme, if needed.
    if(file_name != NULL) {
        // Open the file with the given name.
        FILE* file = fopen(file_name, "rb");
        if(file == NULL) {
            goto error;
        }

        // Read the leader.
        if(true) {
            // Define possible leader keysyms.
            static uint32_t const leader_keysyms[] = {
                XKB_KEY_Super_L, XKB_KEY_Super_R, XKB_KEY_Alt_L, XKB_KEY_Alt_R,
                XKB_KEY_Menu};

            // Read the index of the leader keysym.
            ptrdiff_t leader_idx = fgetc(file);

#define array_size_(x) ((ptrdiff_t)(sizeof(x) / sizeof(x[0])))

            // Make sure that the index is within the valid interval.
            if((leader_idx < 0) ||
               (leader_idx >= array_size_(leader_keysyms))) {
                fclose(file);
                goto error;
            }

#undef array_size_

            // Set the leader.
            scheme->leader_keysym.value = leader_keysyms[leader_idx];
        }

        // Read the number of different actions.
        scheme->n_core_actions = (size_t)(fgetc(file));
        scheme->n_menu_actions = (size_t)(fgetc(file));
        scheme->n_ipc_actions = (size_t)(fgetc(file));

        // Validate the numbers read.
        if(((scheme->n_core_actions < rose_n_core_action_types) ||
            (scheme->n_core_actions > 2 * rose_n_core_action_types)) ||
           ((scheme->n_menu_actions < rose_n_menu_action_types) ||
            (scheme->n_menu_actions > 2 * rose_n_menu_action_types)) ||
           (scheme->n_ipc_actions > rose_n_keyboard_ipc_actions_max)) {
            fclose(file);
            goto error;
        }

        // Read core and menu actions from the file.
#define read_actions_(category)                                  \
    for(size_t i = 0; i < scheme->n_##category##_actions; ++i) { \
        /* Read the shortcut. */                                 \
        scheme->category##_actions[i].shortcut =                 \
            rose_keyboard_control_scheme_read_shortcut(file);    \
                                                                 \
        /* Read the type. */                                     \
        scheme->category##_actions[i].type =                     \
            (enum rose_##category##_action_type)(fgetc(file));   \
                                                                 \
        /* Validate the type. */                                 \
        if(scheme->category##_actions[i].type >=                 \
           rose_n_##category##_action_types) {                   \
            fclose(file);                                        \
            goto error;                                          \
        }                                                        \
    }

        read_actions_(core);
        read_actions_(menu);

#undef read_actions_

        // Read IPC actions, if needed.
        if(scheme->n_ipc_actions != 0) {
            // Allocate memory for the IPC actions.
            scheme->ipc_actions =
                malloc(scheme->n_ipc_actions *
                       sizeof(struct rose_keyboard_ipc_action));

            if(scheme->ipc_actions == NULL) {
                fclose(file);
                goto error;
            }

            // Read the actions.
            for(size_t i = 0; i != scheme->n_ipc_actions; ++i) {
                // Read IPC command.
                fread(scheme->ipc_actions[i].ipc_command.data,
                      sizeof(scheme->ipc_actions[i].ipc_command.data), 1, file);

                // Read the shortcut.
                scheme->ipc_actions[i].shortcut =
                    rose_keyboard_control_scheme_read_shortcut(file);
            }
        }

        // Close the file.
        fclose(file);
    }

    // Update shortcuts for actions.
#define update_shortcuts_(category)                                        \
    for(size_t i = 0; i != scheme->n_##category##_actions; ++i) {          \
        if(scheme->category##_actions[i].shortcut.keysyms[0].value == 0) { \
            scheme->category##_actions[i].shortcut.keysyms[0] =            \
                scheme->leader_keysym;                                     \
        }                                                                  \
    }

    update_shortcuts_(core);
    update_shortcuts_(menu);
    update_shortcuts_(ipc);

#undef update_shortcuts_

    // Sort actions by shortcuts.
#define sort_actions_(category)                                       \
    qsort(scheme->category##_actions, scheme->n_##category##_actions, \
          sizeof(*scheme->category##_actions),                        \
          rose_keyboard_control_scheme_compare_##category##_actions)

    sort_actions_(core);
    sort_actions_(menu);
    sort_actions_(ipc);

#undef sort_actions_

    // Validate actions. Make sure different actions have different shortcuts.
#define validate_actions_(category)                                   \
    for(size_t i = 0; i < scheme->n_##category##_actions - 1; ++i) {  \
        if(rose_keyboard_control_scheme_compare_##category##_actions( \
               &(scheme->category##_actions[i + 0]),                  \
               &(scheme->category##_actions[i + 1])) == 0) {          \
            goto error;                                               \
        }                                                             \
    }

    validate_actions_(core);
    validate_actions_(menu);

    if(scheme->n_ipc_actions != 0) {
        validate_actions_(ipc);
    }

#undef validate_actions_

    // Validate actions. Make sure all action types have a shortcut assigned.
    int n_core_action_shortcuts[rose_n_core_action_types] = {};
    int n_menu_action_shortcuts[rose_n_menu_action_types] = {};

#define validate_actions_(category)                                   \
    for(size_t i = 0; i < scheme->n_##category##_actions; ++i) {      \
        (n_##category##_action_shortcuts[(                            \
            ptrdiff_t)(scheme->category##_actions[i].type)])++;       \
    }                                                                 \
                                                                      \
    for(ptrdiff_t i = 0; i < rose_n_##category##_action_types; ++i) { \
        if(n_##category##_action_shortcuts[i] == 0) {                 \
            goto error;                                               \
        }                                                             \
    }

    validate_actions_(core);
    validate_actions_(menu);

#undef validate_actions_

    // Initialization succeeded.
    return scheme;

error:
    // On error, destroy the scheme.
    return rose_keyboard_control_scheme_destroy(scheme), NULL;
}

void
rose_keyboard_control_scheme_destroy(
    struct rose_keyboard_control_scheme* scheme) {
    // Free memory.
    free(scheme->ipc_actions);
    free(scheme);
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard context initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_keyboard_context*
rose_keyboard_context_initialize(char const* keyboard_layouts) {
    // Allocate and initialize a new keyboard context.
    struct rose_keyboard_context* context =
        malloc(sizeof(struct rose_keyboard_context));

    if(context == NULL) {
        return context;
    } else {
        *context = (struct rose_keyboard_context){};
    }

    // Create an XKB context.
    struct xkb_context* xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if(xkb_context == NULL) {
        goto error;
    }

    // Create the main keymap.
    if(true) {
        // Specify keyboard layouts.
        struct xkb_rule_names rules = {.layout = keyboard_layouts};

        // Create the keymap.
        context->keymap = xkb_keymap_new_from_names(
            xkb_context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

        if(context->keymap == NULL) {
            goto error;
        }
    }

    // Create the keymap which will be used for processing compositor's
    // bindings.
    context->keymap_raw = xkb_keymap_new_from_names(
        xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

    if(context->keymap_raw == NULL) {
        goto error;
    }

    // Decrement reference count of the XKB context.
    xkb_context_unref(xkb_context);

    // Obtain the number of keyboard layouts in the main keymap.
    context->n_layouts = (unsigned)(xkb_keymap_num_layouts(context->keymap));

    // Initialization succeeded.
    return context;

error:
    // On error, destroy the context.
    rose_keyboard_context_destroy(context);
    xkb_context_unref(xkb_context);

    // Initialization failed.
    return NULL;
}

void
rose_keyboard_context_destroy(struct rose_keyboard_context* context) {
    // Destroy the keymaps.
    xkb_keymap_unref(context->keymap);
    xkb_keymap_unref(context->keymap_raw);

    // Free memory.
    free(context);
}
