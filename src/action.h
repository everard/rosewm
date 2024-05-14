// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_B874428317F44488BADD725175626A91
#define H_B874428317F44488BADD725175626A91

#include <wayland-server-core.h>

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_server_context;
struct rose_ui_menu;

////////////////////////////////////////////////////////////////////////////////
// Action definitions.
////////////////////////////////////////////////////////////////////////////////

enum rose_core_action_type {
    // Main actions.
    rose_core_action_type_terminate_display,
    rose_core_action_type_switch_keyboard_layout,
    rose_core_action_type_toggle_keyboard_shortcuts_inhibiting,

    // Surface-related actions.
    rose_core_action_type_surface_close,
    rose_core_action_type_surface_focus_prev,
    rose_core_action_type_surface_focus_next,
    rose_core_action_type_surface_toggle_maximized,
    rose_core_action_type_surface_toggle_fullscreen,
    rose_core_action_type_surface_move_to_workspace_new,
    rose_core_action_type_surface_move_to_workspace,
    rose_core_action_type_surface_move_to_output,

    // Workspace-related actions.
    rose_core_action_type_workspace_add,
    rose_core_action_type_workspace_move,
    rose_core_action_type_workspace_focus_prev,
    rose_core_action_type_workspace_focus_next,
    rose_core_action_type_workspace_toggle_panel,
    rose_core_action_type_workspace_toggle_menu,

    // Terminal-related actions.
    rose_core_action_type_run_terminal,
    rose_core_action_type_run_terminal_ipc,

    // Total number of actions.
    rose_core_action_type_count_
};

enum rose_menu_action_type {
    // Menu's cursor movement.
    rose_menu_action_type_move_mark_up,
    rose_menu_action_type_move_mark_down,

    // Menu's page scrolling.
    rose_menu_action_type_move_page_up,
    rose_menu_action_type_move_page_down,

    // Menu's actions.
    rose_menu_action_type_cancel,
    rose_menu_action_type_commit,
    rose_menu_action_type_select,
    rose_menu_action_type_switch_line_type,

    // Total number of actions.
    rose_menu_action_type_count_
};

////////////////////////////////////////////////////////////////////////////////
// Action execution interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_execute_core_action(
    struct rose_server_context* context,
    enum rose_core_action_type action_type);

void
rose_execute_menu_action(
    struct rose_ui_menu* menu, enum rose_menu_action_type action_type);

#endif // H_B874428317F44488BADD725175626A91
