// Copyright Nezametdinov E. Ildus 2024.
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

        .core_action_count = rose_core_action_type_count_,
        .menu_action_count = rose_menu_action_type_count_,
        .ipc_action_count = 5,

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

        .menu_actions =
            {{.shortcut = {{{XKB_KEY_Up}}},
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
              .type = rose_menu_action_type_switch_line_type}},

        .ipc_actions = {
            {.shortcut = {{{0}, {XKB_KEY_r}}}, .ipc_command = {{0x00}}},

            {.shortcut = {{{0}, {XKB_KEY_Shift_L}, {XKB_KEY_r}}},
             .ipc_command = {{0x01}}},

            {.shortcut = {{{XKB_KEY_XF86AudioLowerVolume}}},
             .ipc_command = {{'V', 'O', 'L', 'U', 'M', 'E', '-'}}},

            {.shortcut = {{{XKB_KEY_XF86AudioRaiseVolume}}},
             .ipc_command = {{'V', 'O', 'L', 'U', 'M', 'E', '+'}}},

            {.shortcut = {{{XKB_KEY_XF86AudioMute}}},
             .ipc_command = {{'V', 'O', 'L', 'U', 'M', 'E', '0'}}}}};

////////////////////////////////////////////////////////////////////////////////
// IO-related utility functions.
////////////////////////////////////////////////////////////////////////////////

static uint32_t
rose_keyboard_control_scheme_unpack_uint32(
    unsigned char buffer[static sizeof(uint32_t)]) {
    // Unpack the value starting from the least significant byte to the most
    // significant byte.
    uint32_t x = 0;
    for(size_t i = 0; i != sizeof(uint32_t); ++i) {
        x |= ((uint32_t)(buffer[i])) << ((uint32_t)(i * CHAR_BIT));
    }

    return x;
}

static struct rose_keyboard_shortcut
rose_keyboard_control_scheme_read_shortcut(FILE* file) {
    unsigned char buffer[rose_keyboard_shortcut_size_max * sizeof(uint32_t)] =
        {};

    // Read the shortcut to a buffer.
    fread(buffer, sizeof(buffer), 1, file);

    // Unpack the shortcut keysym-by-keysym.
    struct rose_keyboard_shortcut shortcut = {};
    for(size_t i = 0; i != rose_keyboard_shortcut_size_max; ++i) {
        shortcut.keysyms[i].value = rose_keyboard_control_scheme_unpack_uint32(
            buffer + i * sizeof(uint32_t));
    }

    return shortcut;
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard action comparison utility functions.
////////////////////////////////////////////////////////////////////////////////

#define action_shortcut_(var, type) \
    &(((struct rose_keyboard_##type##_action const*)(var))->shortcut)

static int
rose_keyboard_control_scheme_compare_core_actions(
    void const* x, void const* y) {
    return rose_keyboard_shortcut_compare(
        action_shortcut_(x, core), action_shortcut_(y, core));
}

static int
rose_keyboard_control_scheme_compare_menu_actions(
    void const* x, void const* y) {
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
rose_keyboard_keysym_compare(
    struct rose_keyboard_keysym const* x,
    struct rose_keyboard_keysym const* y) {
    return ((x->value == y->value) ? 0 : ((x->value < y->value) ? -1 : 1));
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard shortcut comparison interface implementation.
////////////////////////////////////////////////////////////////////////////////

int
rose_keyboard_shortcut_compare(
    struct rose_keyboard_shortcut const* x,
    struct rose_keyboard_shortcut const* y) {
    for(ptrdiff_t i = 0; i != rose_keyboard_shortcut_size_max; ++i) {
        int result =
            rose_keyboard_keysym_compare(&(x->keysyms[i]), &(y->keysyms[i]));

        if(result != 0) {
            return result;
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
            ptrdiff_t leader_index = fgetc(file);

#define array_size_(x) ((ptrdiff_t)(sizeof(x) / sizeof(x[0])))

            // Make sure that the index is within the valid interval.
            if((leader_index < 0) ||
               (leader_index >= array_size_(leader_keysyms))) {
                fclose(file);
                goto error;
            }

#undef array_size_

            // Set the leader.
            scheme->leader_keysym.value = leader_keysyms[leader_index];
        }

        // Read the numbers of different actions.
        scheme->core_action_count = (size_t)(fgetc(file));
        scheme->menu_action_count = (size_t)(fgetc(file));
        scheme->ipc_action_count = (size_t)(fgetc(file));

        // Validate the numbers read.
        if(((scheme->core_action_count < rose_core_action_type_count_) ||
            (scheme->core_action_count > 2 * rose_core_action_type_count_)) ||
           ((scheme->menu_action_count < rose_menu_action_type_count_) ||
            (scheme->menu_action_count > 2 * rose_menu_action_type_count_)) ||
           (scheme->ipc_action_count >
            rose_keyboard_control_scheme_ipc_action_max_count)) {
            fclose(file);
            goto error;
        }

#define read_actions_(category)                                   \
    for(size_t i = 0; i < scheme->category##_action_count; ++i) { \
        /* Read the shortcut. */                                  \
        scheme->category##_actions[i].shortcut =                  \
            rose_keyboard_control_scheme_read_shortcut(file);     \
                                                                  \
        /* Read the type. */                                      \
        scheme->category##_actions[i].type =                      \
            (enum rose_##category##_action_type)(fgetc(file));    \
                                                                  \
        /* Validate the type. */                                  \
        if(scheme->category##_actions[i].type >=                  \
           rose_##category##_action_type_count_) {                \
            fclose(file);                                         \
            goto error;                                           \
        }                                                         \
    }

        // Read core and menu actions from the file.
        read_actions_(core);
        read_actions_(menu);

#undef read_actions_

        // Read IPC actions.
        for(size_t i = 0; i != scheme->ipc_action_count; ++i) {
            // Read the shortcut.
            scheme->ipc_actions[i].shortcut =
                rose_keyboard_control_scheme_read_shortcut(file);

            // Read IPC command.
            fread(
                scheme->ipc_actions[i].ipc_command.data,
                sizeof(scheme->ipc_actions[i].ipc_command.data), 1, file);
        }

        // Close the file.
        fclose(file);
    }

#define update_shortcuts_(category)                                        \
    for(size_t i = 0; i != scheme->category##_action_count; ++i) {         \
        if(scheme->category##_actions[i].shortcut.keysyms[0].value == 0) { \
            scheme->category##_actions[i].shortcut.keysyms[0] =            \
                scheme->leader_keysym;                                     \
        }                                                                  \
    }

    // Update shortcuts for actions.
    update_shortcuts_(core);
    update_shortcuts_(menu);
    update_shortcuts_(ipc);

#undef update_shortcuts_

#define sort_actions_(category)                                      \
    qsort(                                                           \
        scheme->category##_actions, scheme->category##_action_count, \
        sizeof(*scheme->category##_actions),                         \
        rose_keyboard_control_scheme_compare_##category##_actions)

    // Sort actions by shortcuts.
    sort_actions_(core);
    sort_actions_(menu);
    sort_actions_(ipc);

#undef sort_actions_

#define validate_actions_(category)                                       \
    if(scheme->category##_action_count != 0) {                            \
        for(size_t i = 0; i < scheme->category##_action_count - 1; ++i) { \
            if(rose_keyboard_control_scheme_compare_##category##_actions( \
                   &(scheme->category##_actions[i + 0]),                  \
                   &(scheme->category##_actions[i + 1])) == 0) {          \
                goto error;                                               \
            }                                                             \
        }                                                                 \
    }

    // Validate actions. Make sure different actions have different shortcuts.
    validate_actions_(core);
    validate_actions_(menu);
    validate_actions_(ipc);

#undef validate_actions_

#define validate_actions_(category)                                           \
    if(true) {                                                                \
        /* Initialize an array of shortcut counts for all action types. */    \
        int shortcut_counts[rose_##category##_action_type_count_] = {};       \
                                                                              \
        /* Compute the number of shortcuts for each action type. */           \
        for(size_t i = 0; i < scheme->category##_action_count; ++i) {         \
            (shortcut_counts[(                                                \
                ptrdiff_t)(scheme->category##_actions[i].type)])++;           \
        }                                                                     \
                                                                              \
        /* Make sure all action types have a shortcut assigned. */            \
        for(ptrdiff_t i = 0; i < rose_##category##_action_type_count_; ++i) { \
            if(shortcut_counts[i] == 0) {                                     \
                goto error;                                                   \
            }                                                                 \
        }                                                                     \
    }

    // Validate actions. Make sure all action types have a shortcut assigned.
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
    context->layout_count = (unsigned)(xkb_keymap_num_layouts(context->keymap));

    // Initialization succeeded.
    return context;

error:
    // On error, destroy the context.
    rose_keyboard_context_destroy(context), xkb_context_unref(xkb_context);

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
