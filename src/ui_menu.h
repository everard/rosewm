// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_BA71ADF1CED54B6AAB1D54FD2FDCD50F
#define H_BA71ADF1CED54B6AAB1D54FD2FDCD50F

#include "device_input_pointer.h"
#include "unicode.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_output;

////////////////////////////////////////////////////////////////////////////////
// Menu line definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_ui_menu_line_type {
    rose_ui_menu_line_type_surface,
    rose_ui_menu_line_type_workspace,
    rose_ui_menu_line_type_output,
    rose_ui_menu_line_type_count_
};

struct rose_ui_menu_line {
    enum rose_ui_menu_line_type type;
    void* data;
};

////////////////////////////////////////////////////////////////////////////////
// Menu page and text definitions.
////////////////////////////////////////////////////////////////////////////////

enum { rose_ui_menu_line_max_count = 50 };

struct rose_ui_menu_page {
    struct rose_ui_menu_line lines[rose_ui_menu_line_max_count];
    int line_count, mark_index, selection_index;
};

struct rose_ui_menu_text {
    struct rose_utf32_string lines[rose_ui_menu_line_max_count];
    int line_count;
};

////////////////////////////////////////////////////////////////////////////////
// Menu definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_ui_menu {
    // Pointer to the parent output.
    struct rose_output* output;

    // Pointer's state.
    struct {
        // Position data.
        double x, y;

        // Last movement time.
        uint32_t movement_time_msec;
    } pointer;

    // Menu's rectangular area.
    struct {
        int x, y, width, height;
    } area;

    // Layout data.
    struct {
        int margin_x, margin_y;
        int line_height, line_max_count;
    } layout;

    // Current state.
    enum rose_ui_menu_line_type line_type;
    struct rose_ui_menu_line head, selection;
    struct rose_ui_menu_page page;

    // List link.
    struct wl_list link;

    // Flags.
    bool is_visible, is_updated, is_layout_updated;
};

////////////////////////////////////////////////////////////////////////////////
// Menu action definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_ui_menu_action_type {
    rose_ui_menu_action_cancel,
    rose_ui_menu_action_commit,
    rose_ui_menu_action_select
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_initialize(struct rose_ui_menu* menu, struct rose_output* output);

void
rose_ui_menu_destroy(struct rose_ui_menu* menu);

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_show(
    struct rose_ui_menu* menu, enum rose_ui_menu_line_type line_type);

void
rose_ui_menu_hide(struct rose_ui_menu* menu);

void
rose_ui_menu_toggle(struct rose_ui_menu* menu);

void
rose_ui_menu_update(struct rose_ui_menu* menu);

////////////////////////////////////////////////////////////////////////////////
// Action interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_switch_line_type(struct rose_ui_menu* menu);

void
rose_ui_menu_move_head(struct rose_ui_menu* menu, int direction);

void
rose_ui_menu_move_mark(struct rose_ui_menu* menu, int direction);

void
rose_ui_menu_perform_action(
    struct rose_ui_menu* menu, enum rose_ui_menu_action_type type);

////////////////////////////////////////////////////////////////////////////////
// State query interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_ui_menu_has_selection(struct rose_ui_menu* menu);

struct rose_ui_menu_text
rose_ui_menu_text_obtain(struct rose_ui_menu* menu);

////////////////////////////////////////////////////////////////////////////////
// Event notification interface: pointer device.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_notify_pointer_axis(
    struct rose_ui_menu* menu, struct rose_pointer_event_axis event);

void
rose_ui_menu_notify_pointer_button(
    struct rose_ui_menu* menu, struct rose_pointer_event_button event);

void
rose_ui_menu_notify_pointer_warp(
    struct rose_ui_menu* menu, uint32_t time_msec, double x, double y);

////////////////////////////////////////////////////////////////////////////////
// Event notification interface: line.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_notify_line_add(
    struct rose_ui_menu* menu, struct rose_ui_menu_line line);

void
rose_ui_menu_notify_line_remove(
    struct rose_ui_menu* menu, struct rose_ui_menu_line line);

void
rose_ui_menu_notify_line_update(
    struct rose_ui_menu* menu, struct rose_ui_menu_line line);

#endif // H_BA71ADF1CED54B6AAB1D54FD2FDCD50F
