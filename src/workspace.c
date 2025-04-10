// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>

////////////////////////////////////////////////////////////////////////////////
// Rectangle definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_workspace_rectangle {
    int x, y, width, height;
};

////////////////////////////////////////////////////////////////////////////////
// Area computing utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_workspace_rectangle
rose_workspace_compute_main_area(struct rose_workspace* workspace) {
    // Initialize workspace area.
    struct rose_workspace_rectangle area = {
        .width = workspace->width, .height = workspace->height};

    if(workspace->panel.is_visible) {
        // If the panel is visible, then subtract its area.
        switch(workspace->panel.position) {
            case rose_ui_panel_position_top:
                area.y += workspace->panel.size;
                // fall-through

            case rose_ui_panel_position_bottom:
                area.height -= workspace->panel.size;
                break;

            case rose_ui_panel_position_left:
                area.x += workspace->panel.size;
                // fall-through

            case rose_ui_panel_position_right:
                area.width -= workspace->panel.size;
                break;

            default:
                break;
        }
    }

    // Return computed area.
    return area;
}

////////////////////////////////////////////////////////////////////////////////
// Surface selecting utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_surface*
rose_workspace_select_next_surface(
    struct rose_workspace* workspace, struct rose_surface* surface,
    enum rose_workspace_focus_direction direction) {
    // Precondition: The surface is either not specified (NULL), or belongs to
    // the given workspace.

    // If the surface is not specified, then select the first mapped surface of
    // the workspace.
    if(surface == NULL) {
        if(wl_list_empty(&(workspace->surfaces_mapped))) {
            return NULL;
        }

        return wl_container_of(
            workspace->surfaces_mapped.next, surface, link_mapped);
    }

#define select_(d)                                         \
    ((surface->link.d != &(workspace->surfaces))           \
         ? wl_container_of(surface->link.d, surface, link) \
         : wl_container_of(workspace->surfaces.d, surface, link))

#define select_next_mapped_(d)                                             \
    for(struct rose_surface* sentinel = surface;;) {                       \
        if(((surface = select_(d)) == sentinel) || (surface->is_mapped)) { \
            break;                                                         \
        }                                                                  \
    }

    // Select the next mapped surface in the given direction.
    if(direction == rose_workspace_focus_direction_backward) {
        select_next_mapped_(next);
    } else {
        select_next_mapped_(prev);
    }

#undef select_next_mapped_
#undef select_

    if(surface->is_mapped) {
        return surface;
    } else {
        return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Layout computing utility function.
////////////////////////////////////////////////////////////////////////////////

static void
rose_workspace_layout_compute(struct rose_workspace* workspace) {
    struct rose_surface* surface = NULL;
    struct rose_surface* _ = NULL;

    // Obtain the focused surface.
    surface = workspace->focused_surface;

    // If there is a focused surface, and it is mapped, then move it to the top
    // of the list of mapped surfaces.
    if((surface != NULL) && (surface->is_mapped)) {
        wl_list_remove(&(surface->link_mapped));
        wl_list_insert(&(workspace->surfaces_mapped), &(surface->link_mapped));
    }

    // Compute workspace's main area.
    struct rose_workspace_rectangle main_area =
        rose_workspace_compute_main_area(workspace);

    // Iterate through the list of mapped surfaces.
    wl_list_for_each(surface, &(workspace->surfaces_mapped), link_mapped) {
        // If the surface is maximized or in fullscreen mode, then
        // configure it, and do nothing else.
        if(surface->state.pending.is_maximized ||
           surface->state.pending.is_fullscreen) {
            // Obtain its extent from the workspace.
            struct rose_workspace_rectangle extent =
                (surface->state.pending.is_fullscreen
                     ? (struct rose_workspace_rectangle){.width =
                                                             workspace->width,
                                                         .height =
                                                             workspace->height}
                     : main_area);

            // Configure it.
            rose_surface_configure(
                surface, (struct rose_surface_configuration_parameters){
                             .flags = rose_surface_configure_size |
                                      rose_surface_configure_position,
                             .width = extent.width,
                             .height = extent.height,
                             .x = extent.x,
                             .y = extent.y});

            // Break out of the cycle.
            break;
        }
    }

    // Clear the list of visible surfaces.
    wl_list_for_each_safe(
        surface, _, &(workspace->surfaces_visible), link_visible) {
        // Reset surface's visibility flag.
        surface->is_visible = false;

        // Remove it from the list of visible surfaces.
        wl_list_remove(&(surface->link_visible));
        wl_list_init(&(surface->link_visible));
    }

    // Build a new list of visible surfaces: iterate through the list of mapped
    // surfaces again.
    wl_list_for_each(surface, &(workspace->surfaces_mapped), link_mapped) {
        // Set surface's visibility flag.
        surface->is_visible = true;

        // Add the surface to the list of visible surfaces.
        wl_list_remove(&(surface->link_visible));
        wl_list_insert(
            &(workspace->surfaces_visible), &(surface->link_visible));

        // If the surface is maximized or in fullscreen mode, then break out of
        // the cycle.
        if(surface->state.pending.is_maximized ||
           surface->state.pending.is_fullscreen) {
            break;
        }
    }

    // Request workspace's redraw.
    rose_workspace_request_redraw(workspace);
}

////////////////////////////////////////////////////////////////////////////////
// Layout manipulating utility function and type.
////////////////////////////////////////////////////////////////////////////////

enum rose_workspace_layout_update_type {
    rose_workspace_layout_update_surface_add,
    rose_workspace_layout_update_surface_remove
};

static void
rose_workspace_layout_update(
    enum rose_workspace_layout_update_type type,
    struct rose_workspace* workspace, struct rose_surface* surface) {
    // Precondition: The surface belongs to the given workspace.

    if(type == rose_workspace_layout_update_surface_add) {
        // If the surface is added, then append it to the list of mapped
        // surfaces.
        wl_list_remove(&(surface->link_mapped));
        wl_list_insert(&(workspace->surfaces_mapped), &(surface->link_mapped));

        // Position the surface.
        if(true) {
            // Obtain workspace's main area.
            struct rose_workspace_rectangle main_area =
                rose_workspace_compute_main_area(workspace);

            // Compute and save surface's initial position.
            surface->state.saved.x = main_area.x;
            surface->state.saved.y = main_area.y;

#define shift_(v, d)                                       \
    ((v) +=                                                \
     ((main_area.d > surface->state.current.d)             \
          ? ((main_area.d - surface->state.current.d) / 2) \
          : 0))

            shift_(surface->state.saved.x, width);
            shift_(surface->state.saved.y, height);

#undef shift_

            // Set surface's position.
            surface->state.current.x = surface->state.pending.x =
                surface->state.saved.x;

            surface->state.current.y = surface->state.pending.y =
                surface->state.saved.y;
        }

        // Notify all visible menus.
        if(true) {
            struct rose_ui_menu_line line = {
                .type = rose_ui_menu_line_type_surface, .data = surface};

            struct rose_ui_menu* menu = NULL;
            wl_list_for_each(menu, &(workspace->context->menus_visible), link) {
                rose_ui_menu_notify_line_add(menu, line);
            }
        }

        // Focus the surface.
        rose_workspace_focus_surface(workspace, surface);
    } else {
        // If the surface is removed, then update workspace's focus.
        if(workspace->focused_surface == surface) {
            // Select surface's successor.
            struct rose_surface* successor =
                ((surface->link_mapped.next != &(workspace->surfaces_mapped))
                     ? wl_container_of(
                           surface->link_mapped.next, surface, link_mapped)
                     : (wl_list_empty(&(workspace->surfaces_mapped))
                            ? NULL
                            : wl_container_of(
                                  workspace->surfaces_mapped.next, surface,
                                  link_mapped)));

            // Focus the successor, but make sure that the surface is not its
            // own successor.
            rose_workspace_focus_surface(
                workspace, ((successor == surface) ? NULL : successor));
        }

        // Notify all visible menus.
        if(true) {
            struct rose_ui_menu_line line = {
                .type = rose_ui_menu_line_type_surface, .data = surface};

            struct rose_ui_menu* menu = NULL;
            wl_list_for_each(menu, &(workspace->context->menus_visible), link) {
                rose_ui_menu_notify_line_remove(menu, line);
            }
        }

        // Remove the surface from the list of mapped surfaces.
        wl_list_remove(&(surface->link_mapped));
        wl_list_init(&(surface->link_mapped));

        // Clear its visibility flag and remove it from the list of visible
        // surfaces.
        surface->is_visible = false;

        wl_list_remove(&(surface->link_visible));
        wl_list_init(&(surface->link_visible));

        // Recompute workspace's layout with the surface removed.
        rose_workspace_layout_compute(workspace);

        // Commit surface's running transaction, if any.
        if(surface->is_transaction_running) {
            rose_surface_transaction_commit(surface);
            rose_workspace_transaction_update(workspace);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Transaction's watchdog timer event handler.
////////////////////////////////////////////////////////////////////////////////

int
rose_handle_event_workspace_transaction_timer_expiry(void* data) {
    return rose_workspace_transaction_commit(data), 0;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_workspace_initialize(
    struct rose_workspace* workspace, struct rose_server_context* context) {
    // Initialize workspace object.
    *workspace = (struct rose_workspace){
        .context = context, .width = 640, .height = 480};

    // Set workspace's ID.
    workspace->id = (unsigned)(workspace - context->storage.workspace);

    // Initialize list link.
    wl_list_init(&(workspace->link_output));

    // Initialize lists of surfaces which will belong to this workspace.
    wl_list_init(&(workspace->surfaces));
    wl_list_init(&(workspace->surfaces_mapped));
    wl_list_init(&(workspace->surfaces_visible));

    // Add this workspace to the list of available workspaces.
    wl_list_insert(&(context->workspaces), &(workspace->link));

    // Initialize pointer's timer.
    workspace->pointer.timer = wl_event_loop_add_timer(
        context->event_loop, rose_handle_event_workspace_pointer_timer_expiry,
        workspace);

    // Initialize transaction's data.
    wl_list_init(&(workspace->transaction.snapshot.surfaces));
    workspace->transaction.timer = wl_event_loop_add_timer(
        context->event_loop,
        rose_handle_event_workspace_transaction_timer_expiry, workspace);

    // Ensure that workspace's timers are successfully initialized.
    if((workspace->pointer.timer == NULL) ||
       (workspace->transaction.timer == NULL)) {
        goto error;
    }

    // Initialization succeeded.
    return true;

error:
    // On error, destroy the workspace.
    return rose_workspace_destroy(workspace), false;
}

void
rose_workspace_destroy(struct rose_workspace* workspace) {
    // Commit the running transaction, if any.
    rose_workspace_transaction_commit(workspace);

    // Destroy all surfaces which belong to this workspace.
    if(true) {
        struct rose_surface* surface = NULL;
        struct rose_surface* _ = NULL;

        wl_list_for_each_safe(surface, _, &(workspace->surfaces), link) {
            // Close and destroy the surface.
            rose_surface_request_close(surface);
            rose_surface_destroy(surface);
        }
    }

    // Remove timers.
    if(workspace->transaction.timer != NULL) {
        wl_event_source_remove(workspace->transaction.timer);
    }

    if(workspace->pointer.timer != NULL) {
        wl_event_source_remove(workspace->pointer.timer);
    }

    // Remove this workspace from the output it belongs to, if any.
    if(workspace->output != NULL) {
        rose_output_remove_workspace(workspace->output, workspace);
    }

    // Remove this workspace from the lists.
    wl_list_remove(&(workspace->link));
    wl_list_remove(&(workspace->link_output));
}

////////////////////////////////////////////////////////////////////////////////
// List ordering interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct wl_list*
rose_workspace_find_position_in_list(
    struct wl_list* head, struct rose_workspace* workspace,
    size_t link_offset) {
    // Obtain given workspace's ID.
    unsigned id_target = workspace->id;

    // Find the relevant position.
    struct wl_list* position = head->prev;
    for(; position != head; position = position->prev) {
#define cast_(ptr, type, offset) ((type*)(((char*)(ptr)) - offset))

        // Obtain current workspace's ID.
        unsigned id_current =
            cast_(position, struct rose_workspace, link_offset)->id;

#undef cast_

        // Break out of the cycle, if needed.
        if(id_target < id_current) {
            break;
        }
    }

    // Return the position.
    return position;
}

////////////////////////////////////////////////////////////////////////////////
// Input focusing interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_make_current(struct rose_workspace* workspace) {
    // Obtain parent output's prompt.
    struct rose_surface* prompt =
        ((workspace->output != NULL)
             ? (!wl_list_empty(
                    workspace->output->ui.surfaces_mapped +
                    rose_surface_widget_type_prompt)
                    ? (wl_container_of(
                          workspace->output->ui
                              .surfaces_mapped[rose_surface_widget_type_prompt]
                              .next,
                          prompt, link_mapped))
                    : NULL)
             : NULL);

    // Deactivate existing pointer constraint, if needed.
    if((workspace->context->current_workspace != workspace) ||
       (prompt != NULL) || (workspace->context->is_screen_locked)) {
        if((workspace->context->current_workspace->focused_surface != NULL) &&
           (workspace->context->current_workspace->focused_surface
                ->pointer_constraint != NULL)) {
            wlr_pointer_constraint_v1_send_deactivated(
                workspace->context->current_workspace->focused_surface
                    ->pointer_constraint);
        }
    }

    // Set the given workspace as current for input events.
    workspace->context->current_workspace = workspace;

    // Handle input focus.
    if(workspace->context->is_screen_locked) {
        // If the screen is locked, then obtain parent output's screen lock.
        struct rose_surface* screen_lock =
            ((workspace->output != NULL)
                 ? (!wl_list_empty(
                        workspace->output->ui.surfaces_mapped +
                        rose_surface_widget_type_screen_lock)
                        ? (wl_container_of(
                              workspace->output->ui
                                  .surfaces_mapped
                                      [rose_surface_widget_type_screen_lock]
                                  .next,
                              screen_lock, link_mapped))
                        : NULL)
                 : NULL);

        // Handle input focus.
        if(screen_lock != NULL) {
            // If there is a screen lock, then make it accept input events.
            rose_surface_make_current(screen_lock, workspace->context->seat);
        } else {
            // Otherwise, end all grabs.
            wlr_seat_keyboard_end_grab(workspace->context->seat);
            wlr_seat_pointer_end_grab(workspace->context->seat);

            // Clear keyboard and pointer focus.
            wlr_seat_keyboard_clear_focus(workspace->context->seat);
            wlr_seat_pointer_clear_focus(workspace->context->seat);

            // And clear focus of all tablets.
            if(true) {
                struct rose_tablet* tablet = NULL;
                struct rose_tablet* _ = NULL;

                wl_list_for_each_safe(
                    tablet, _, &(workspace->context->inputs_tablets), link) {
                    rose_tablet_clear_focus(tablet);
                }
            }
        }
    } else if(prompt != NULL) {
        // If there is a prompt, then make it accept input events.
        rose_surface_make_current(prompt, workspace->context->seat);
    } else if(workspace->focused_surface != NULL) {
        // If there is a focused surface, then make it accept input events.
        rose_surface_make_current(
            workspace->focused_surface, workspace->context->seat);

        // And activate its pointer constraint, if any.
        if(workspace->focused_surface->pointer_constraint != NULL) {
            wlr_pointer_constraint_v1_send_activated(
                workspace->focused_surface->pointer_constraint);
        }
    } else {
        // Otherwise, end all grabs.
        wlr_seat_keyboard_end_grab(workspace->context->seat);
        wlr_seat_pointer_end_grab(workspace->context->seat);

        // Clear keyboard and pointer focus.
        wlr_seat_keyboard_clear_focus(workspace->context->seat);
        wlr_seat_pointer_clear_focus(workspace->context->seat);

        // And clear focus of all tablets.
        if(true) {
            struct rose_tablet* tablet = NULL;
            struct rose_tablet* _ = NULL;

            wl_list_for_each_safe(
                tablet, _, &(workspace->context->inputs_tablets), link) {
                rose_tablet_clear_focus(tablet);
            }
        }
    }

    // Update pointer's focus by warping the pointer to its current location.
    rose_workspace_pointer_warp(
        workspace, workspace->pointer.movement_time, workspace->pointer.x,
        workspace->pointer.y);
}

bool
rose_workspace_is_current(struct rose_workspace* workspace) {
    // Check if the given workspace is set to accept input events.
    return (workspace == workspace->context->current_workspace);
}

////////////////////////////////////////////////////////////////////////////////
// Surface focusing interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_focus_surface(
    struct rose_workspace* workspace, struct rose_surface* surface) {
    // If there is no surface to focus, then there is no need to set any surface
    // parameters either.
    if(surface == NULL) {
        goto apply;
    }

    // Do nothing if the surface isn't a normal toplevel surface.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type != rose_surface_widget_type_none)) {
        return;
    }

    // Do nothing if the given workspace does not contain the surface.
    if(surface->parent.workspace != workspace) {
        return;
    }

    if(surface->is_mapped) {
        // If the surface is mapped, then activate it without starting a new
        // transaction.
        rose_surface_configure(
            surface, (struct rose_surface_configuration_parameters){
                         .flags = rose_surface_configure_activated |
                                  rose_surface_configure_no_transaction,
                         .is_activated = true});

    } else {
        // If the surface isn't mapped, then the given workspace shall have no
        // focused surfaces.
        surface = NULL;
    }

apply:

    // Deactivate previously focused surface without starting a new transaction,
    // cancel any interactive mode of the workspace.
    if((workspace->focused_surface != NULL) &&
       (workspace->focused_surface != surface)) {
        // Configure previously focused surface.
        rose_surface_configure(
            workspace->focused_surface,
            (struct rose_surface_configuration_parameters){
                .flags = rose_surface_configure_activated |
                         rose_surface_configure_no_transaction,
                .is_activated = false});

        // Deactivate its pointer constraint, if any.
        if(workspace->focused_surface->pointer_constraint != NULL) {
            wlr_pointer_constraint_v1_send_deactivated(
                workspace->focused_surface->pointer_constraint);
        }

        // Cancel any interactive mode.
        rose_workspace_cancel_interactive_mode(workspace);
    }

    // Focus the surface (or reset the focus if there is no surface).
    workspace->focused_surface = surface;

    // Update input focus, if needed.
    if(rose_workspace_is_current(workspace)) {
        rose_workspace_make_current(workspace);
    }

    // Recompute workspace's layout.
    rose_workspace_layout_compute(workspace);
}

void
rose_workspace_focus_surface_relative(
    struct rose_workspace* workspace,
    enum rose_workspace_focus_direction direction) {
    rose_workspace_focus_surface(
        workspace, rose_workspace_select_next_surface(
                       workspace, workspace->focused_surface, direction));
}

////////////////////////////////////////////////////////////////////////////////
// Surface configuration interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_surface_configure(
    struct rose_workspace* workspace, struct rose_surface* surface,
    struct rose_surface_configuration_parameters parameters) {
    // Do nothing if the surface isn't a normal toplevel surface.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type != rose_surface_widget_type_none)) {
        return;
    }

    // Do nothing if the given workspace does not contain the surface.
    if(surface->parent.workspace != workspace) {
        return;
    }

    // Save surface's previous state and compute part of the next state.
    struct rose_surface_state state_prev = rose_surface_state_obtain(surface),
                              state_next = state_prev;

    if((parameters.flags & rose_surface_configure_maximized) != 0) {
        state_next.is_maximized = parameters.is_maximized;
    }

    if((parameters.flags & rose_surface_configure_fullscreen) != 0) {
        state_next.is_fullscreen = parameters.is_fullscreen;
    }

    // Update configuration.

    if((state_prev.is_maximized || state_prev.is_fullscreen) &&
       !(state_next.is_maximized || state_next.is_fullscreen)) {
        // If the surface is returned to its original state, then request update
        // of its size and coordinates.
        if((parameters.flags & rose_surface_configure_size) == 0) {
            // Configure required flags.
            parameters.flags &=
                ~((unsigned)(rose_surface_configure_no_transaction));
            parameters.flags |=
                rose_surface_configure_size | rose_surface_configure_position;

            // Specify saved width, height, and coordinates.
            parameters.x = surface->state.saved.x;
            parameters.y = surface->state.saved.y;

            parameters.width = surface->state.saved.width;
            parameters.height = surface->state.saved.height;
        }
    } else if(
        !(state_prev.is_maximized || state_prev.is_fullscreen) &&
        (state_next.is_maximized || state_next.is_fullscreen)) {
        // If the surface is being maximized or set to fullscreen, then save its
        // previous state.
        surface->state.saved = state_prev;
    }

    if(state_next.is_maximized || state_next.is_fullscreen) {
        // If the surface will be maximized or set to fullscreen mode, then
        // don't configure its position or size.
        parameters.flags &= ~(
            (rose_surface_configuration_mask)(rose_surface_configure_size |
                                              rose_surface_configure_position));
    }

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

    // Constrain configuration parameters.
    if((parameters.flags &
        (rose_surface_configure_size | rose_surface_configure_position)) != 0) {
        parameters.width = clamp_(parameters.width, 1, workspace->width);
        parameters.height = clamp_(parameters.height, 1, workspace->height);

        struct rose_workspace_rectangle bounds =
            rose_workspace_compute_main_area(workspace);

        bool is_size_configured =
            ((parameters.flags & rose_surface_configure_size) != 0);

        int x_min = bounds.x + (is_size_configured ? -parameters.width
                                                   : -state_prev.width);
        int y_min = bounds.y + (is_size_configured ? -parameters.height
                                                   : -state_prev.height);

        parameters.x = clamp_(parameters.x, x_min, workspace->width);
        parameters.y = clamp_(parameters.y, y_min, workspace->height);
    }

#undef min_
#undef max_
#undef clamp_

    // Configure the surface.
    rose_surface_configure(surface, parameters);

    // Recompute workspace's layout.
    rose_workspace_layout_compute(workspace);
}

////////////////////////////////////////////////////////////////////////////////
// Surface addition/removal/reordering interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_add_surface(
    struct rose_workspace* workspace, struct rose_surface* surface) {
    // Do nothing if the surface isn't a normal toplevel surface.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type != rose_surface_widget_type_none)) {
        return;
    }

    // Do nothing if the surface doesn't change its workspace.
    if(workspace == surface->parent.workspace) {
        return;
    }

    // A flag which shows that there is a need to send output enter event to the
    // surface.
    bool need_to_send_enter_event = true;

    // Remove the surface from its previous workspace, if needed.
    if(surface->parent.workspace != NULL) {
        // Send output leave event to the surface, if needed.
        if((surface->parent.workspace->output != workspace->output) &&
           (surface->parent.workspace->output != NULL)) {
            rose_surface_output_leave(
                surface, surface->parent.workspace->output);
        }

        // Clear the flag if the surface does not change its output.
        if(surface->parent.workspace->output == workspace->output) {
            need_to_send_enter_event = false;
        }

        // Remove the surface from its workspace.
        rose_workspace_remove_surface(surface->parent.workspace, surface);
    }

    // Link the surface with its new workspace.
    wl_list_insert(&(workspace->surfaces), &(surface->link));
    surface->parent.workspace = workspace;

    // Send output enter event to the surface, if needed.
    if(need_to_send_enter_event && (workspace->output != NULL)) {
        rose_surface_output_enter(surface, workspace->output);
    }

    // If the surface is already mapped, then update workspace's layout.
    if(surface->is_mapped) {
        rose_workspace_layout_update(
            rose_workspace_layout_update_surface_add, workspace, surface);
    }

    // If the workspace does not belong to any outputs, then add this workspace
    // to the first available output.
    if(workspace->output == NULL) {
        if(!wl_list_empty(&(workspace->context->outputs))) {
            // Obtain the first available output.
            struct rose_output* output =
                wl_container_of(workspace->context->outputs.prev, output, link);

            // Add the workspace to the output.
            rose_output_add_workspace(output, workspace);
        } else {
            // Remove the workspace from the list of available workspaces.
            wl_list_remove(&(workspace->link));
            wl_list_init(&(workspace->link));

            // Add the workspace to the list of workspaces without output.
            wl_list_remove(&(workspace->link_output));
            wl_list_insert(
                &(workspace->context->workspaces_without_output),
                &(workspace->link_output));
        }
    }
}

void
rose_workspace_remove_surface(
    struct rose_workspace* workspace, struct rose_surface* surface) {
    // Do nothing if the surface isn't a normal toplevel surface.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type != rose_surface_widget_type_none)) {
        return;
    }

    // Do nothing if the given workspace does not contain the surface.
    if(surface->parent.workspace != workspace) {
        return;
    }

    // If the surface is mapped, then update workspace's layout.
    if(surface->is_mapped) {
        rose_workspace_layout_update(
            rose_workspace_layout_update_surface_remove, workspace, surface);
    }

    // Clear surface's visibility flag.
    surface->is_visible = false;

    // Sever all links between the surface and the workspace.
    wl_list_remove(&(surface->link));
    wl_list_remove(&(surface->link_layout));
    wl_list_remove(&(surface->link_mapped));
    wl_list_remove(&(surface->link_visible));

    surface->parent.workspace = NULL;

    wl_list_init(&(surface->link));
    wl_list_init(&(surface->link_layout));
    wl_list_init(&(surface->link_mapped));
    wl_list_init(&(surface->link_visible));

    // Commit surface's running transaction, if any.
    if(surface->is_transaction_running) {
        rose_surface_transaction_commit(surface);
        rose_workspace_transaction_update(workspace);
    }

    // If the workspace becomes empty, then perform additional actions.
    if(wl_list_empty(&(workspace->surfaces))) {
        if(workspace->output == NULL) {
            if(!rose_workspace_is_current(workspace)) {
                // If the workspace does not belong to any outputs, and it is
                // not current, then add it to the list of available workspaces.
                wl_list_remove(&(workspace->link));
                wl_list_insert(
                    rose_workspace_find_position_in_list(
                        &(workspace->context->workspaces), workspace,
                        offsetof(struct rose_workspace, link)),
                    &(workspace->link));

                // Remove it from the list of workspaces without output.
                wl_list_remove(&(workspace->link_output));
                wl_list_init(&(workspace->link_output));

                // And reset its panel.
                workspace->panel = workspace->panel_saved =
                    workspace->context->config.theme.panel;
            }
        } else if(workspace->output->focused_workspace != workspace) {
            // Otherwise, if the workspace is not a focused workspace of its
            // output, then remove it from its output.
            rose_output_remove_workspace(workspace->output, workspace);
        }
    }
}

void
rose_workspace_reposition_surface(
    struct rose_workspace* workspace, struct rose_surface* surface,
    struct rose_surface* destination) {
    // Do nothing if either the source surface, or its destination, is not
    // specified, or if the source surface does not change its position.
    if((surface == NULL) || (destination == NULL) || (surface == destination)) {
        return;
    }

    // Do nothing if either the source surface, or its destination, isn't a
    // normal toplevel surface.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type != rose_surface_widget_type_none) ||
       (destination->type != rose_surface_type_toplevel) ||
       (destination->widget_type != rose_surface_widget_type_none)) {
        return;
    }

    // Do nothing if either the source surface, or its destination, does not
    // belong to the given workspace.
    if((workspace != surface->parent.workspace) ||
       (workspace != destination->parent.workspace)) {
        return;
    }

    struct rose_ui_menu_line line = {
        .type = rose_ui_menu_line_type_surface, .data = surface};

    // Notify all visible menus that the surface has been removed from its
    // previous position.
    if(true) {
        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(workspace->context->menus_visible), link) {
            rose_ui_menu_notify_line_remove(menu, line);
        }
    }

    // Move the surface.
    wl_list_remove(&(surface->link));
    wl_list_insert(&(destination->link), &(surface->link));

    // Notify all visible menus that the surface has been added to its new
    // position.
    if(true) {
        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(workspace->context->menus_visible), link) {
            rose_ui_menu_notify_line_add(menu, line);

            // Move menu's head, if needed.
            if(destination == menu->head.data) {
                rose_ui_menu_move_head(menu, -1);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_set_panel(
    struct rose_workspace* workspace, struct rose_ui_panel panel) {
    // Update the panel.
    workspace->panel_saved = workspace->panel;
    workspace->panel = panel;

    // If the workspace is the focused workspace of its output, then update
    // output's UI components.
    if((workspace->output != NULL) &&
       (workspace->output->focused_workspace == workspace)) {
        rose_output_ui_update(&(workspace->output->ui));
    }

    // Recompute workspace's layout.
    rose_workspace_layout_compute(workspace);

    // Update pointer's focus by warping the pointer to its current location.
    rose_workspace_pointer_warp(
        workspace, workspace->pointer.movement_time, workspace->pointer.x,
        workspace->pointer.y);

    // If there is no running transaction, then update panel's data.
    if(workspace->transaction.sentinel <= 0) {
        workspace->panel_saved = workspace->panel;
    }
}

void
rose_workspace_request_redraw(struct rose_workspace* workspace) {
    if((workspace->output != NULL) &&
       (workspace->output->focused_workspace == workspace)) {
        rose_output_request_redraw(workspace->output);
    }
}

void
rose_workspace_cancel_interactive_mode(struct rose_workspace* workspace) {
    workspace->mode = rose_workspace_mode_normal;
}

void
rose_workspace_commit_interactive_mode(struct rose_workspace* workspace) {
    // Do nothing if the workspace is in normal mode.
    if(workspace->mode == rose_workspace_mode_normal) {
        return;
    }

    if((workspace->focused_surface != NULL) &&
       !(workspace->focused_surface->state.pending.is_maximized ||
         workspace->focused_surface->state.pending.is_fullscreen)) {
        // If there is a focused surface, then compute its parameters.
        struct rose_surface_configuration_parameters parameters = {
            .flags = rose_surface_configure_position,
            .x = workspace->focused_surface->state.saved.x,
            .y = workspace->focused_surface->state.saved.y,
            .width = workspace->focused_surface->state.pending.width,
            .height = workspace->focused_surface->state.pending.height};

        if(workspace->mode == rose_workspace_mode_interactive_move) {
            // If the workspace is in interactive move mode, then compute a new
            // position for the focused surface based on pointer's shift.
            parameters.x +=
                (int)(workspace->pointer.x - workspace->pointer.x_saved);

            parameters.y +=
                (int)(workspace->pointer.y - workspace->pointer.y_saved);
        } else {
            // Otherwise, the workspace is in interactive resize mode, so both
            // surface's size and position need to be configured.
            parameters.flags |= rose_surface_configure_size;

            // Compute pointer's shift.
            int dx = (int)(workspace->pointer.x - workspace->pointer.x_saved);
            int dy = (int)(workspace->pointer.y - workspace->pointer.y_saved);

            // Compute surface's parameters based on the mode.

            // Coordinate: x, resize: east.
            if((workspace->mode ==
                rose_workspace_mode_interactive_resize_east) ||
               (workspace->mode ==
                rose_workspace_mode_interactive_resize_north_east) ||
               (workspace->mode ==
                rose_workspace_mode_interactive_resize_south_east)) {
                if(dx >= 0) {
                    parameters.width += dx;
                } else {
                    if(-dx <= parameters.width) {
                        parameters.width -= -dx;
                    } else {
                        dx += parameters.width;
                        parameters.x += dx;
                        parameters.width = -dx;
                    }
                }
            }

            // Coordinate: x, resize: west.
            if((workspace->mode ==
                rose_workspace_mode_interactive_resize_west) ||
               (workspace->mode ==
                rose_workspace_mode_interactive_resize_north_west) ||
               (workspace->mode ==
                rose_workspace_mode_interactive_resize_south_west)) {
                if(dx > 0) {
                    if(dx <= parameters.width) {
                        parameters.width -= dx;
                        parameters.x += dx;
                    } else {
                        parameters.x += parameters.width;
                        parameters.width = dx - parameters.width;
                    }
                } else {
                    parameters.x += dx;
                    parameters.width -= dx;
                }
            }

            // Coordinate: y, resize: north.
            if((workspace->mode ==
                rose_workspace_mode_interactive_resize_north) ||
               (workspace->mode ==
                rose_workspace_mode_interactive_resize_north_east) ||
               (workspace->mode ==
                rose_workspace_mode_interactive_resize_north_west)) {
                if(dy > 0) {
                    if(dy <= parameters.height) {
                        parameters.height -= dy;
                        parameters.y += dy;
                    } else {
                        parameters.y += parameters.height;
                        parameters.height = dy - parameters.height;
                    }
                } else {
                    parameters.y += dy;
                    parameters.height -= dy;
                }
            }

            // Coordinate: y, resize: south.
            if((workspace->mode ==
                rose_workspace_mode_interactive_resize_south) ||
               (workspace->mode ==
                rose_workspace_mode_interactive_resize_south_east) ||
               (workspace->mode ==
                rose_workspace_mode_interactive_resize_south_west)) {
                if(dy >= 0) {
                    parameters.height += dy;
                } else {
                    if(-dy <= parameters.height) {
                        parameters.height -= -dy;
                    } else {
                        dy += parameters.height;
                        parameters.y += dy;
                        parameters.height = -dy;
                    }
                }
            }
        }

        // Configure the surface.
        rose_workspace_surface_configure(
            workspace, workspace->focused_surface, parameters);
    }

    // Return the workspace to the normal mode.
    workspace->mode = rose_workspace_mode_normal;
}

////////////////////////////////////////////////////////////////////////////////
// Event notification interface implementation: output device.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_notify_output_mode(
    struct rose_workspace* workspace, struct rose_output* output) {
    // Do nothing if the given workspace does not belong to the output.
    if(workspace->output != output) {
        return;
    }

    // Change the size of the workspace.
    struct rose_output_state output_state = rose_output_state_obtain(output);
    workspace->width = (int)(0.5 + (output_state.width / output_state.scale));
    workspace->height = (int)(0.5 + (output_state.height / output_state.scale));

    // Recompute workspace's layout.
    rose_workspace_layout_compute(workspace);

    // Update pointer's position, if needed.
    if((workspace->pointer.x > workspace->width) ||
       (workspace->pointer.y > workspace->height)) {
        rose_workspace_pointer_warp(
            workspace, workspace->pointer.movement_time, workspace->pointer.x,
            workspace->pointer.y);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Event notification interface implementation: surface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_notify_surface_name_update(
    struct rose_workspace* workspace, struct rose_surface* surface) {
    // Do nothing if the surface isn't a normal toplevel surface.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type != rose_surface_widget_type_none)) {
        return;
    }

    // Do nothing if the given workspace does not contain the surface.
    if(surface->parent.workspace != workspace) {
        return;
    }

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_surface, .data = surface};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(workspace->context->menus_visible), link) {
            rose_ui_menu_notify_line_update(menu, line);
        }
    }

    // Request workspace's redraw, if needed.
    if(workspace->focused_surface == surface) {
        rose_workspace_request_redraw(workspace);
    }
}

void
rose_workspace_notify_surface_map(
    struct rose_workspace* workspace, struct rose_surface* surface) {
    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Do nothing if the master surface does not belong to the given workspace.
    if((master->widget_type != rose_surface_widget_type_none) ||
       (master->parent.workspace != workspace)) {
        return;
    }

    // Handle subsurfaces and temporary surfaces.
    if(surface->type != rose_surface_type_toplevel) {
        if(master->is_visible) {
            rose_workspace_request_redraw(workspace);
        }

        return;
    }

    // Update workspace's layout.
    rose_workspace_layout_update(
        rose_workspace_layout_update_surface_add, workspace, surface);
}

void
rose_workspace_notify_surface_unmap(
    struct rose_workspace* workspace, struct rose_surface* surface) {
    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Do nothing if the master surface does not belong to the given workspace.
    if((master->widget_type != rose_surface_widget_type_none) ||
       (master->parent.workspace != workspace)) {
        return;
    }

    // Handle subsurfaces and temporary surfaces.
    if(surface->type != rose_surface_type_toplevel) {
        if(master->is_visible) {
            rose_workspace_request_redraw(workspace);
        }

        return;
    }

    // Update workspace's layout.
    rose_workspace_layout_update(
        rose_workspace_layout_update_surface_remove, workspace, surface);
}

void
rose_workspace_notify_surface_commit(
    struct rose_workspace* workspace, struct rose_surface* surface) {
    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Do nothing if the master surface does not belong to the given workspace.
    if((master->widget_type != rose_surface_widget_type_none) ||
       (master->parent.workspace != workspace)) {
        return;
    }

    // Perform initial configuration, if needed.
    if((surface->type != rose_surface_type_subsurface) &&
       (surface->xdg_surface->initial_commit)) {
        if(surface->type == rose_surface_type_toplevel) {
            // Save surface's initial state.
            surface->state.saved = surface->state.current;

            // Configure the surface.
            rose_surface_configure(
                surface,
                (struct rose_surface_configuration_parameters){
                    .flags = rose_surface_configure_size |
                             rose_surface_configure_maximized |
                             rose_surface_configure_fullscreen,
                    .width = surface->state.saved.width,
                    .height = surface->state.saved.height,
                    .is_maximized =
                        surface->xdg_surface->toplevel->requested.maximized,
                    .is_fullscreen =
                        surface->xdg_surface->toplevel->requested.fullscreen});
        } else {
            // Constrain temporary surface.
            struct wlr_box constraints = {
                .x = -master->state.current.x,
                .y = -master->state.current.y,
                .width = workspace->width,
                .height = workspace->height};

            wlr_xdg_popup_unconstrain_from_box(
                surface->xdg_surface->popup, &constraints);
        }

        return;
    }

    // Do nothing else if the given workspace is not focused.
    if((workspace->output == NULL) ||
       (workspace->output->focused_workspace != workspace)) {
        return;
    }

    // Damage workspace's output.
    if(master->is_visible) {
        rose_output_add_surface_damage(workspace->output, surface);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Transaction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_transaction_start(struct rose_workspace* workspace) {
    // Update transaction's state.
    if(workspace->transaction.sentinel++ != 0) {
        return;
    }

    // Start transaction's watchdog timer.
    wl_event_source_timer_update(workspace->transaction.timer, 300);

    // Set the starting time of the transaction.
    clock_gettime(CLOCK_MONOTONIC, &(workspace->transaction.start_time));

    // A flag which shows that the panel is currently hidden, even if it has its
    // visibility flag set.
    bool is_panel_hidden = false;

    // Create snapshots for all visible surfaces.
    struct rose_surface* surface = NULL;
    wl_list_for_each_reverse(
        surface, &(workspace->surfaces_visible), link_visible) {
        // Check if the panel is currently hidden. This happens if the first
        // visible surface is in fullscreen mode.
        if(workspace->surfaces_visible.prev == &(surface->link_visible)) {
            is_panel_hidden =
                surface->xdg_surface->toplevel->current.fullscreen;
        }

        // Create surface's snapshot.
        rose_surface_transaction_initialize_snapshot(surface);
    }

    // Create a snapshot for the panel.
    workspace->transaction.snapshot.panel = workspace->panel_saved;
    if(workspace->transaction.snapshot.panel.is_visible) {
        workspace->transaction.snapshot.panel.is_visible = !is_panel_hidden;
    }

    // Request workspace's redraw.
    rose_workspace_request_redraw(workspace);
}

void
rose_workspace_transaction_update(struct rose_workspace* workspace) {
    // Update transaction's sentinel and commit the transaction, if needed.
    if((--(workspace->transaction.sentinel)) <= 0) {
        rose_workspace_transaction_commit(workspace);
    }
}

void
rose_workspace_transaction_commit(struct rose_workspace* workspace) {
    // Reset transaction's state.
    workspace->transaction.sentinel = 0;

    // Disarm transaction's watchdog timer.
    wl_event_source_timer_update(workspace->transaction.timer, 0);

    // Destroy the snapshot.
    if(true) {
        struct rose_surface_snapshot* surface_snapshot = NULL;
        struct rose_surface_snapshot* _ = NULL;

        wl_list_for_each_safe(
            surface_snapshot, _, &(workspace->transaction.snapshot.surfaces),
            link) {
            rose_surface_snapshot_destroy(surface_snapshot);
        }
    }

    // Commit all running surface transactions.
    if(true) {
        struct rose_surface* surface = NULL;
        wl_list_for_each(surface, &(workspace->surfaces), link) {
            rose_surface_transaction_commit(surface);
        }
    }

    // Update panel's data.
    workspace->panel_saved = workspace->panel;

    // Request workspace's redraw.
    rose_workspace_request_redraw(workspace);
}
