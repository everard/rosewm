// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

////////////////////////////////////////////////////////////////////////////////
// Action execution interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_execute_core_action(struct rose_server_context* context,
                         enum rose_core_action_type action_type) {
    struct {
        struct rose_workspace* workspace;
        struct rose_surface* surface;
        struct rose_output* output;
    } focus = {.workspace = context->current_workspace,
               .surface = context->current_workspace->focused_surface,
               .output = context->current_workspace->output};

    switch(action_type) {
        // Main actions.
        case rose_core_action_type_terminate_display:
            wl_display_terminate(context->display);
            break;

        case rose_core_action_type_switch_keyboard_layout: {
            if(context->keyboard_context->layout_count > 1) {
                rose_server_context_set_keyboard_layout(
                    context, (context->keyboard_context->layout_index + 1) %
                                 context->keyboard_context->layout_count);
            }

            break;
        }

        case rose_core_action_type_toggle_keyboard_shortcuts_inhibiting:
            // Switch the flag.
            context->are_keyboard_shortcuts_inhibited =
                !(context->are_keyboard_shortcuts_inhibited);

            // Broadcast the change through IPC.
            rose_ipc_server_broadcast_status(
                context->ipc_server,
                rose_server_context_obtain_status(context));

            break;

        // Surface-related actions.
        case rose_core_action_type_surface_close:
            if(focus.surface != NULL) {
                rose_surface_request_close(focus.surface);
            }

            break;

        case rose_core_action_type_surface_focus_prev:
            rose_workspace_focus_surface_relative(
                focus.workspace, rose_workspace_focus_direction_backward);

            break;

        case rose_core_action_type_surface_focus_next:
            rose_workspace_focus_surface_relative(
                focus.workspace, rose_workspace_focus_direction_forward);

            break;

        case rose_core_action_type_surface_toggle_maximized:
            if(focus.surface != NULL) {
                struct rose_surface_configure_parameters parameters = {
                    .flags = rose_surface_configure_maximized,
                    .is_maximized = !(
                        rose_surface_state_obtain(focus.surface).is_maximized)};

                rose_workspace_surface_configure(
                    focus.workspace, focus.surface, parameters);
            }

            break;

        case rose_core_action_type_surface_toggle_fullscreen:
            if(focus.surface != NULL) {
                struct rose_surface_configure_parameters parameters = {
                    .flags = rose_surface_configure_fullscreen,
                    .is_fullscreen = !(rose_surface_state_obtain(focus.surface)
                                           .is_fullscreen)};

                rose_workspace_surface_configure(
                    focus.workspace, focus.surface, parameters);
            }

            break;

        case rose_core_action_type_surface_move_to_workspace_new:
            if((focus.surface != NULL) && (focus.output != NULL)) {
                if(!wl_list_empty(&(context->workspaces))) {
                    // Obtain the first workspace from the list of free
                    // workspaces.
                    struct rose_workspace* workspace = wl_container_of(
                        context->workspaces.prev, workspace, link);

                    // Add it to the focused output, and focus the workspace.
                    rose_output_add_workspace(focus.output, workspace);
                    rose_output_focus_workspace(focus.output, workspace);

                    // Add the focused surface to the workspace.
                    rose_workspace_add_surface(workspace, focus.surface);
                }
            }

            break;

        case rose_core_action_type_surface_move_to_workspace:
            if((focus.surface != NULL) && (focus.output != NULL)) {
                // Show the menu.
                rose_ui_menu_show(
                    &(focus.output->ui.menu), rose_ui_menu_line_type_surface);

                // Select the current surface.
                rose_ui_menu_perform_action(
                    &(focus.output->ui.menu), rose_ui_menu_action_select);

                // Show the list of workspaces.
                rose_ui_menu_switch_line_type(&(focus.output->ui.menu));
            }

            break;

        case rose_core_action_type_surface_move_to_output:
            if((focus.surface != NULL) && (focus.output != NULL)) {
                // Show the menu.
                rose_ui_menu_show(
                    &(focus.output->ui.menu), rose_ui_menu_line_type_surface);

                // Select the current surface.
                rose_ui_menu_perform_action(
                    &(focus.output->ui.menu), rose_ui_menu_action_select);

                // Show the list of outputs.
                rose_ui_menu_switch_line_type(&(focus.output->ui.menu));
                rose_ui_menu_switch_line_type(&(focus.output->ui.menu));
            }

            break;

        // Workspace-related actions.
        case rose_core_action_type_workspace_add:
            if(focus.output != NULL) {
                if(!wl_list_empty(&(context->workspaces))) {
                    // Obtain the first workspace from the list of free
                    // workspaces.
                    struct rose_workspace* workspace = wl_container_of(
                        context->workspaces.prev, workspace, link);

                    // Add it to the focused output, and focus the workspace.
                    rose_output_add_workspace(focus.output, workspace);
                    rose_output_focus_workspace(focus.output, workspace);
                }
            }

            break;

        case rose_core_action_type_workspace_move:
            if(focus.output != NULL) {
                rose_ui_menu_show(
                    &(focus.output->ui.menu), rose_ui_menu_line_type_workspace);

                rose_ui_menu_perform_action(
                    &(focus.output->ui.menu), rose_ui_menu_action_select);
            }

            break;

        case rose_core_action_type_workspace_focus_prev:
            if(focus.output != NULL) {
                rose_output_focus_workspace_relative(
                    focus.output, rose_output_focus_direction_backward);
            }

            break;

        case rose_core_action_type_workspace_focus_next:
            if(focus.output != NULL) {
                rose_output_focus_workspace_relative(
                    focus.output, rose_output_focus_direction_forward);
            }

            break;

        case rose_core_action_type_workspace_toggle_panel: {
            // Obtain current workspace's panel.
            struct rose_ui_panel panel = focus.workspace->panel;

            // Flip its visibility flag.
            panel.is_visible = !(panel.is_visible);

            // Update the panel.
            rose_workspace_set_panel(focus.workspace, panel);

            break;
        }

        case rose_core_action_type_workspace_toggle_menu:
            if(focus.output != NULL) {
                rose_ui_menu_toggle(&(focus.output->ui.menu));
            }

            break;

        // Terminal-related actions.
        case rose_core_action_type_run_terminal:
            rose_execute_command(context->config.argument_lists.terminal);
            break;

        case rose_core_action_type_run_terminal_ipc:
            // Start a new terminal instance as compositor's child process.
            rose_command_list_execute_command(
                context->command_list, context->config.argument_lists.terminal,
                rose_command_access_ipc);

            break;

        default:
            break;
    }
}

void
rose_execute_menu_action(struct rose_ui_menu* menu,
                         enum rose_menu_action_type action_type) {
    switch(action_type) {
        case rose_menu_action_type_move_mark_up:
            rose_ui_menu_move_mark(menu, -1);
            break;

        case rose_menu_action_type_move_mark_down:
            rose_ui_menu_move_mark(menu, +1);
            break;

        case rose_menu_action_type_move_page_up:
            rose_ui_menu_move_head(menu, -(menu->layout.line_max_count));
            break;

        case rose_menu_action_type_move_page_down:
            rose_ui_menu_move_head(menu, +(menu->layout.line_max_count));
            break;

        case rose_menu_action_type_cancel:
            rose_ui_menu_perform_action(menu, rose_ui_menu_action_cancel);
            break;

        case rose_menu_action_type_commit:
            rose_ui_menu_perform_action(menu, rose_ui_menu_action_commit);
            break;

        case rose_menu_action_type_select:
            rose_ui_menu_perform_action(menu, rose_ui_menu_action_select);
            break;

        case rose_menu_action_type_switch_line_type:
            rose_ui_menu_switch_line_type(menu);
            break;

        default:
            break;
    }
}
