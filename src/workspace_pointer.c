// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>

#include <wlr/util/region.h>
#include <pixman.h>

#include <linux/input-event-codes.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
// Point vs surface relation computing utility function.
////////////////////////////////////////////////////////////////////////////////

enum rose_workspace_point_surface_relation {
    rose_workspace_point_surface_relation_inside,
    rose_workspace_point_surface_relation_outside,
    rose_workspace_point_surface_relation_touches_north,
    rose_workspace_point_surface_relation_touches_south,
    rose_workspace_point_surface_relation_touches_east,
    rose_workspace_point_surface_relation_touches_west,
    rose_workspace_point_surface_relation_touches_north_east,
    rose_workspace_point_surface_relation_touches_north_west,
    rose_workspace_point_surface_relation_touches_south_east,
    rose_workspace_point_surface_relation_touches_south_west
};

static enum rose_workspace_point_surface_relation
rose_workspace_point_relate(double x, double y, struct rose_surface* surface) {
    // Define rectangle type.
    struct rectangle {
        double x0, y0, x1, y1;
    };

    // Obtain surface's state.
    struct rose_surface_state state = rose_surface_state_obtain(surface);

    // Compute surface's rectangle.
    struct rectangle surface_rectangle = {
        .x0 = state.x,
        .y0 = state.y,
        .x1 = state.x + state.width,
        .y1 = state.y + state.height};

#define is_inside_(x, y, region)                       \
    ((((x) >= (region).x0) && ((y) >= (region).y0)) && \
     (((x) <= (region).x1) && ((y) <= (region).y1)))

    // Check if the point is inside the surface's rectangle.
    if(is_inside_(x, y, surface_rectangle)) {
        return rose_workspace_point_surface_relation_inside;
    }

    // Check if the point is inside one of the surface's pop-ups.
    if(wlr_xdg_surface_popup_surface_at(
           surface->xdg_surface, x - state.x, y - state.y, NULL, NULL) !=
       NULL) {
        return rose_workspace_point_surface_relation_inside;
    }

    // Check if the point is on the edge of the surface's decoration. Surfaces
    // which are maximized or in fullscreen mode have no decorations.
    if(!(state.is_maximized || state.is_fullscreen)) {
        // Construct rectangles for decoration's boundary areas.
        static double const d = 5.0, d_2 = 10.0;
        struct rectangle rectangles
            [rose_workspace_point_surface_relation_touches_south_west -
             rose_workspace_point_surface_relation_touches_north + 1] = {
                // rose_workspace_point_surface_relation_touches_north
                {.x0 = surface_rectangle.x0,
                 .y0 = surface_rectangle.y0 - d,
                 .x1 = surface_rectangle.x1,
                 .y1 = surface_rectangle.y0},
                // rose_workspace_point_surface_relation_touches_south
                {.x0 = surface_rectangle.x0,
                 .y0 = surface_rectangle.y1,
                 .x1 = surface_rectangle.x1,
                 .y1 = surface_rectangle.y1 + d},
                // rose_workspace_point_surface_relation_touches_east
                {.x0 = surface_rectangle.x1,
                 .y0 = surface_rectangle.y0,
                 .x1 = surface_rectangle.x1 + d,
                 .y1 = surface_rectangle.y1},
                // rose_workspace_point_surface_relation_touches_west
                {.x0 = surface_rectangle.x0 - d,
                 .y0 = surface_rectangle.y0,
                 .x1 = surface_rectangle.x0,
                 .y1 = surface_rectangle.y1},
                // rose_workspace_point_surface_relation_touches_north_east
                {.x0 = surface_rectangle.x1,
                 .y0 = surface_rectangle.y0 - d_2,
                 .x1 = surface_rectangle.x1 + d_2,
                 .y1 = surface_rectangle.y0},
                // rose_workspace_point_surface_relation_touches_north_west
                {.x0 = surface_rectangle.x0 - d_2,
                 .y0 = surface_rectangle.y0 - d_2,
                 .x1 = surface_rectangle.x0,
                 .y1 = surface_rectangle.y0},
                // rose_workspace_point_surface_relation_touches_south_east
                {.x0 = surface_rectangle.x1,
                 .y0 = surface_rectangle.y1,
                 .x1 = surface_rectangle.x1 + d_2,
                 .y1 = surface_rectangle.y1 + d_2},
                // rose_workspace_point_surface_relation_touches_south_west
                {.x0 = surface_rectangle.x0 - d_2,
                 .y0 = surface_rectangle.y1,
                 .x1 = surface_rectangle.x0,
                 .y1 = surface_rectangle.y1 + d_2}};

#define array_size_(a) ((size_t)(sizeof(a) / sizeof(a[0])))

        // Check if the point is inside any of the constructed rectangles.
        for(size_t i = 0; i != array_size_(rectangles); ++i) {
            if(is_inside_(x, y, rectangles[i])) {
                return (
                    rose_workspace_point_surface_relation_touches_north +
                    (int)(i));
            }
        }
    }

#undef array_size_
#undef is_inside_

    // The point is outside the surface.
    return rose_workspace_point_surface_relation_outside;
}

////////////////////////////////////////////////////////////////////////////////
// UI component selecting utility function and types.
////////////////////////////////////////////////////////////////////////////////

enum rose_workspace_ui_selection_type {
    rose_workspace_ui_selection_type_none,
    rose_workspace_ui_selection_type_panel,
    rose_workspace_ui_selection_type_output
};

struct rose_workspace_ui_selection {
    enum rose_workspace_ui_selection_type type;
    struct rose_output_ui_selection output_ui_selection;
};

static struct rose_workspace_ui_selection
rose_workspace_ui_select(struct rose_workspace* workspace, double x, double y) {
    // Initialize an empty selection.
    struct rose_workspace_ui_selection result = {};

    // Check if a point with given coordinates is inside workspace's panel.
    if(true) {
        // Obtain panel's data.
        struct rose_ui_panel panel = workspace->panel;
        if(panel.is_visible) {
            if((workspace->focused_surface != NULL) &&
               (workspace->focused_surface->state.pending.is_fullscreen)) {
                panel.is_visible = false;
            }
        }

        // If the panel is visible, then perform checks depending on its
        // position.
        if(panel.is_visible) {
            switch(panel.position) {
                case rose_ui_panel_position_top:
                    if((y < panel.size) && (x <= (workspace->width / 2))) {
                        result.type = rose_workspace_ui_selection_type_panel;
                    }

                    break;

                case rose_ui_panel_position_bottom:
                    if((y >= (workspace->height - panel.size)) &&
                       (x <= (workspace->width / 2))) {
                        result.type = rose_workspace_ui_selection_type_panel;
                    }

                    break;

                case rose_ui_panel_position_left:
                    if((x < panel.size) && (y >= (workspace->height / 2))) {
                        result.type = rose_workspace_ui_selection_type_panel;
                    }

                    break;

                case rose_ui_panel_position_right:
                    if((x >= (workspace->width - panel.size)) &&
                       (y <= (workspace->height / 2))) {
                        result.type = rose_workspace_ui_selection_type_panel;
                    }

                    break;

                default:
                    break;
            }
        }

        // If the panel has been selected, then do nothing else.
        if(result.type != rose_workspace_ui_selection_type_none) {
            return result;
        }
    }

    // If the workspace belongs to an output, then perform additional checks.
    if(workspace->output != NULL) {
        // Select output's UI component.
        result.output_ui_selection =
            rose_output_ui_select(&(workspace->output->ui), x, y);

        // If such component exists, then set corresponding selection type.
        if(result.output_ui_selection.type !=
           rose_output_ui_selection_type_none) {
            result.type = rose_workspace_ui_selection_type_output;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Cursor manipulating utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_workspace_output_cursor_sync(struct rose_workspace* workspace) {
    // Do nothing if the given workspace does not belong to any output, or if
    // the workspace isn't focused.
    if((workspace->output == NULL) ||
       (workspace->output->focused_workspace != workspace)) {
        return;
    }

    // Warp output's cursor to the pointer's position.
    rose_output_cursor_warp(
        workspace->output, workspace->pointer.x, workspace->pointer.y);
}

static void
rose_workspace_output_cursor_set(
    struct rose_workspace* workspace, enum rose_output_cursor_type type) {
    // Do nothing if the given workspace does not belong to any output, or if
    // the workspace isn't focused.
    if((workspace->output == NULL) ||
       (workspace->output->focused_workspace != workspace)) {
        return;
    }

    // Set cursor's type.
    rose_output_cursor_set(workspace->output, type);
}

////////////////////////////////////////////////////////////////////////////////
// Mode setting utility function.
////////////////////////////////////////////////////////////////////////////////

static void
rose_workspace_mode_set(
    struct rose_workspace* workspace, uint32_t time_msec,
    enum rose_workspace_mode mode) {
    // Set the specified mode.
    workspace->mode = mode;

    // Do nothing else if the mode is normal.
    if(mode == rose_workspace_mode_normal) {
        return;
    }

    // If there is no focused surface, or if such surface is maximized, or in
    // fullscreen mode, then set normal mode and do nothing else.
    if((workspace->focused_surface == NULL) ||
       (workspace->focused_surface->state.pending.is_maximized ||
        workspace->focused_surface->state.pending.is_fullscreen)) {
        workspace->mode = rose_workspace_mode_normal;
        return;
    }

    // Save pointer's position.
    workspace->pointer.x_saved = workspace->pointer.x;
    workspace->pointer.y_saved = workspace->pointer.y;

    // Save focused surface's state.
    workspace->focused_surface->state.saved =
        workspace->focused_surface->state.current;

    // Disarm pointer's timer.
    if(workspace->pointer.is_timer_armed) {
        workspace->pointer.is_timer_armed = false;
        wl_event_source_timer_update(workspace->pointer.timer, 0);
    }

    // Warp the pointer to its current location.
    rose_workspace_pointer_warp(
        workspace, time_msec, workspace->pointer.x, workspace->pointer.y);
}

////////////////////////////////////////////////////////////////////////////////
// Surface selecting utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_surface*
rose_workspace_select_surface_at(
    struct rose_workspace* workspace, double x, double y) {
    // Iterate through the list of mapped surfaces.
    struct rose_surface* surface = NULL;

    wl_list_for_each(surface, &(workspace->surfaces_mapped), link_mapped) {
        // If the surface is maximized or in fullscreen mode, or the given
        // coordinates are not outside the surface's boundaries, then select
        // such surface.
        if((surface->state.pending.is_maximized ||
            surface->state.pending.is_fullscreen) ||
           (rose_workspace_point_relate(x, y, surface) !=
            rose_workspace_point_surface_relation_outside)) {
            return surface;
        }
    }

    // No surface has been found under the given coordinates.
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Private interface implementation: pointer's timer expiry event handler.
////////////////////////////////////////////////////////////////////////////////

int
rose_handle_event_workspace_pointer_timer_expiry(void* data) {
    // Obtain the workspace.
    struct rose_workspace* workspace = data;

    // Clear timer's flag.
    workspace->pointer.is_timer_armed = false;

    // If the screen is locked, then do nothing else.
    if(workspace->context->is_screen_locked) {
        return 0;
    }

    // If the workspace is not in normal mode, or if workspace's pointer is
    // inside the UI area, then do nothing else.
    if((workspace->mode != rose_workspace_mode_normal) ||
       (rose_workspace_ui_select(
            workspace, workspace->pointer.x, workspace->pointer.y)
            .type != rose_workspace_ui_selection_type_none)) {
        return 0;
    }

    // Select a surface under the pointer's coordinates.
    struct rose_surface* surface = rose_workspace_select_surface_at(
        workspace, workspace->pointer.x, workspace->pointer.y);

    if(surface == NULL) {
        // If there is no such surface, then set output cursor's type to
        // default.
        rose_workspace_output_cursor_set(
            workspace, rose_output_cursor_type_default);
    } else {
        // Otherwise, compute pointer-surface relation.
        enum rose_workspace_point_surface_relation relation =
            rose_workspace_point_relate(
                workspace->pointer.x, workspace->pointer.y, surface);

        // And set appropriate cursor type for the output which contains this
        // workspace.
        enum rose_output_cursor_type cursor_type =
            (((relation == rose_workspace_point_surface_relation_inside) ||
              (relation == rose_workspace_point_surface_relation_outside))
                 ? rose_output_cursor_type_default
                 : (rose_output_cursor_type_resizing_north +
                    (relation -
                     rose_workspace_point_surface_relation_touches_north)));

        rose_workspace_output_cursor_set(workspace, cursor_type);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Pointer manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_pointer_warp(
    struct rose_workspace* workspace, uint32_t time_msec, double x, double y) {
    // Save pointer's previous position.
    double x_prev = workspace->pointer.x;
    double y_prev = workspace->pointer.y;

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

    // Update pointer's position.
    workspace->pointer.x = clamp_(x, 0.0, (double)(workspace->width));
    workspace->pointer.y = clamp_(y, 0.0, (double)(workspace->height));

    x = clamp_(x, 0.0, (double)(workspace->width - 1));
    y = clamp_(y, 0.0, (double)(workspace->height - 1));

    // Update pointer's last movement time.
    workspace->pointer.movement_time_msec = time_msec;

    // Synchronize cursor's position with pointer's position.
    rose_workspace_output_cursor_sync(workspace);

    // If the workspace has no focused surface, or such surface is maximized or
    // in fullscreen mode, then cancel any interactive mode.
    if((workspace->focused_surface == NULL) ||
       (workspace->focused_surface->state.pending.is_maximized ||
        workspace->focused_surface->state.pending.is_fullscreen)) {
        rose_workspace_cancel_interactive_mode(workspace);
    }

    // If the screen is locked, then cancel any interactive mode.
    if(workspace->context->is_screen_locked) {
        rose_workspace_cancel_interactive_mode(workspace);
    }

    // Do nothing else if the given workspace is not current.
    if(!rose_workspace_is_current(workspace)) {
        return;
    }

    // Obtain current seat.
    struct wlr_seat* seat = workspace->context->seat;

    // Perform additional actions depending on current mode.
    if(workspace->context->is_screen_locked) {
        // If the screen is locked, then obtain parent output's screen lock
        // widget.
        struct rose_output_widget* screen_lock =
            ((workspace->output != NULL)
                 ? (!wl_list_empty(
                        workspace->output->ui.widgets_mapped +
                        rose_output_widget_type_screen_lock)
                        ? (wl_container_of(
                              workspace->output->ui
                                  .widgets_mapped
                                      [rose_output_widget_type_screen_lock]
                                  .next,
                              screen_lock, link_mapped))
                        : NULL)
                 : NULL);

        if(screen_lock != NULL) {
            // If there is such widget, then set cursor's type accordingly.
            rose_workspace_output_cursor_set(
                workspace, rose_output_cursor_type_client);

            // Compute pointer's surface-local coordinates.
            x -= screen_lock->state.x;
            y -= screen_lock->state.y;

            // Find a surface under pointer's coordinates.
            double x_local = 0.0, y_local = 0.0;
            struct wlr_surface* surface = wlr_xdg_surface_surface_at(
                screen_lock->xdg_surface, x, y, &x_local, &y_local);

            if(surface != NULL) {
                // If such surface exists, then notify the seat that the surface
                // has pointer focus.
                wlr_seat_pointer_notify_enter(seat, surface, x_local, y_local);

                // And send motion event.
                if(surface == seat->pointer_state.focused_surface) {
                    wlr_seat_pointer_notify_motion(
                        seat, time_msec, x_local, y_local);
                }
            } else {
                // Otherwise, clear pointer's focus.
                wlr_seat_pointer_clear_focus(seat);
            }
        } else {
            // Otherwise, clear pointer's focus.
            wlr_seat_pointer_clear_focus(seat);
        }
    } else if(workspace->mode == rose_workspace_mode_normal) {
        // If the workspace is in normal mode, then perform actions depending on
        // pointer's relation to UI components and workspace's focused surface.

        // If there is a focused surface, and it has a pointer constraint, then
        // apply such constraint.
        if((workspace->focused_surface != NULL) &&
           (workspace->focused_surface->pointer_constraint != NULL)) {
            struct wlr_pointer_constraint_v1* pointer_constraint =
                workspace->focused_surface->pointer_constraint;

            // Obtain the underlying surface.
            struct wlr_surface* surface =
                workspace->focused_surface->xdg_surface->surface;

            // Select constraining region.
            pixman_region32_t* region =
                (((pointer_constraint->current.committed &
                   WLR_POINTER_CONSTRAINT_V1_STATE_REGION) != 0)
                     ? &(pointer_constraint->current.region)
                     : &(surface->input_region));

            if(!pixman_region32_not_empty(region)) {
                region = &(surface->input_region);
            }

            // Apply the constraint depending on its type.
            if(pointer_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
                // If the pointer is locked in place, then compute pointer's
                // surface-local coordinates.
                x = x_prev - workspace->focused_surface->state.current.x;
                y = y_prev - workspace->focused_surface->state.current.y;

                // Constrain the pointer to the region.
                if(!pixman_region32_contains_point(region, x, y, NULL)) {
                    // If the pointer is outside the region, then obtain the
                    // first box of the region.
                    pixman_box32_t* box =
                        pixman_region32_rectangles(region, NULL);

                    // And move the pointer to the box's top-left corner.
                    x = box->x1;
                    y = box->y1;
                }

                // Notify the seat that the surface has pointer focus.
                wlr_seat_pointer_notify_enter(seat, surface, x, y);

                // Compute pointer's workspace coordinates.
                x += workspace->focused_surface->state.current.x;
                y += workspace->focused_surface->state.current.y;

                // Update pointer's position.
                workspace->pointer.x =
                    clamp_(x, 0.0, (double)(workspace->width));

                workspace->pointer.y =
                    clamp_(y, 0.0, (double)(workspace->height));

                // Synchronize cursor's position with pointer's position.
                rose_workspace_output_cursor_sync(workspace);

                // Set cursor's type.
                rose_workspace_output_cursor_set(
                    workspace, rose_output_cursor_type_client);

                // Do nothing else (do not emit seat events).
                return;
            } else {
                // If the pointer is confined to a region, then compute
                // pointer's surface-local coordinates.
                x_prev -= workspace->focused_surface->state.current.x;
                y_prev -= workspace->focused_surface->state.current.y;

                x -= workspace->focused_surface->state.current.x;
                y -= workspace->focused_surface->state.current.y;

                // Constrain the pointer to the region.
                if(!wlr_region_confine(region, x_prev, y_prev, x, y, &x, &y)) {
                    // If the pointer is outside the region, then obtain the
                    // first box of the region.
                    pixman_box32_t* box =
                        pixman_region32_rectangles(region, NULL);

                    // And move the pointer to the box's top-left corner.
                    x = box->x1;
                    y = box->y1;
                }

                // Compute pointer's workspace coordinates.
                x += workspace->focused_surface->state.current.x;
                y += workspace->focused_surface->state.current.y;

                // Update pointer's position.
                workspace->pointer.x =
                    clamp_(x, 0.0, (double)(workspace->width));

                workspace->pointer.y =
                    clamp_(y, 0.0, (double)(workspace->height));

                // Synchronize cursor's position with pointer's position.
                rose_workspace_output_cursor_sync(workspace);
            }
        }

#undef min_
#undef max_
#undef clamp_

        // Select UI component under the pointer's coordinates.
        struct rose_workspace_ui_selection ui_selection =
            rose_workspace_ui_select(workspace, x, y);

        // A flag which shows that the pointer is inside UI area.
        bool is_pointer_inside_ui_area =
            (ui_selection.type != rose_workspace_ui_selection_type_none);

        // Compute relation between the pointer and workspace's focused surface,
        // if any.
        enum rose_workspace_point_surface_relation relation_surface =
            ((is_pointer_inside_ui_area || (workspace->focused_surface == NULL))
                 ? rose_workspace_point_surface_relation_outside
                 : rose_workspace_point_relate(
                       x, y, workspace->focused_surface));

        if(relation_surface == rose_workspace_point_surface_relation_inside) {
            // If the pointer is inside the focused surface, then set cursor's
            // type accordingly.
            rose_workspace_output_cursor_set(
                workspace, rose_output_cursor_type_client);

            // Compute pointer's surface-local coordinates.
            x -= workspace->focused_surface->state.current.x;
            y -= workspace->focused_surface->state.current.y;

            // Find a surface under pointer's coordinates.
            double x_local = 0.0, y_local = 0.0;
            struct wlr_surface* surface = wlr_xdg_surface_surface_at(
                workspace->focused_surface->xdg_surface, x, y, &x_local,
                &y_local);

            // If such surface exists, then send required seat events.
            if(surface != NULL) {
                // Notify the seat that the surface has pointer focus.
                wlr_seat_pointer_notify_enter(seat, surface, x_local, y_local);

                // Send motion event.
                wlr_seat_pointer_notify_motion(
                    seat, time_msec, x_local, y_local);
            }
        } else if(
            relation_surface == rose_workspace_point_surface_relation_outside) {
            // If the pointer is outside the focused surface, then perform
            // additional actions.
            if(is_pointer_inside_ui_area) {
                // If the pointer is inside UI area, then set default cursor
                // type for the output which contains this workspace.
                rose_workspace_output_cursor_set(
                    workspace, rose_output_cursor_type_default);

                // A flag which shows that pointer's focus must be cleared.
                bool should_clear_focus = true;

                // Notify the relevant UI component.
                if(ui_selection.type ==
                   rose_workspace_ui_selection_type_output) {
                    switch(ui_selection.output_ui_selection.type) {
                        case rose_output_ui_selection_type_menu:
                            // Notify the menu of this event.
                            rose_ui_menu_notify_pointer_warp(
                                ui_selection.output_ui_selection.menu,
                                time_msec, x, y);

                            break;

                        case rose_output_ui_selection_type_widget:
                            // Notify output's widget of this event.
                            if(ui_selection.output_ui_selection.widget
                                   .surface != NULL) {
                                // Notify the seat that the surface has pointer
                                // focus.
                                wlr_seat_pointer_notify_enter(
                                    seat,
                                    ui_selection.output_ui_selection.widget
                                        .surface,
                                    ui_selection.output_ui_selection.widget
                                        .x_local,
                                    ui_selection.output_ui_selection.widget
                                        .y_local);

                                // Send motion event.
                                if(ui_selection.output_ui_selection.widget
                                       .surface ==
                                   seat->pointer_state.focused_surface) {
                                    wlr_seat_pointer_notify_motion(
                                        seat, time_msec,
                                        ui_selection.output_ui_selection.widget
                                            .x_local,
                                        ui_selection.output_ui_selection.widget
                                            .y_local);
                                }
                            }

                            // Pointer's focus must not be cleared, since widget
                            // is just a surface.
                            should_clear_focus = false;

                            break;

                        case rose_output_ui_selection_type_none:
                            // fall-through
                        default:
                            // Do nothing.
                            break;
                    }
                }

                // And clear pointer's focus, if needed.
                if(should_clear_focus) {
                    wlr_seat_pointer_clear_focus(seat);
                }
            } else {
                // Otherwise, arm pointer's timer.
                if(!(workspace->pointer.is_timer_armed)) {
                    workspace->pointer.is_timer_armed = true;
                    wl_event_source_timer_update(workspace->pointer.timer, 100);
                }

                // And clear pointer's focus.
                wlr_seat_pointer_clear_focus(seat);
            }
        } else {
            // Otherwise, set appropriate cursor type for the output which
            // contains this workspace.
            rose_workspace_output_cursor_set(
                workspace,
                (rose_output_cursor_type_resizing_north +
                 (relation_surface -
                  rose_workspace_point_surface_relation_touches_north)));

            // And clear pointer's focus.
            wlr_seat_pointer_clear_focus(seat);
        }
    } else {
        // Otherwise, set appropriate cursor type for the output which contains
        // this workspace.
        rose_workspace_output_cursor_set(
            workspace, (rose_output_cursor_type_default +
                        (workspace->mode - rose_workspace_mode_normal)));

        // Clear pointer's focus.
        wlr_seat_pointer_clear_focus(seat);

        // Request workspace's redraw.
        rose_workspace_request_redraw(workspace);

        // If the workspace is in interactive move mode, then compute and set a
        // new position for the focused surface based on pointer's shift.
        if(workspace->mode == rose_workspace_mode_interactive_move) {
            // Compute surface's coordinates.
            int x = workspace->focused_surface->state.saved.x +
                    (int)(workspace->pointer.x - workspace->pointer.x_saved);

            int y = workspace->focused_surface->state.saved.y +
                    (int)(workspace->pointer.y - workspace->pointer.y_saved);

            // Configure the surface.
            rose_surface_configure(
                workspace->focused_surface,
                (struct rose_surface_configure_parameters){
                    .flags = rose_surface_configure_position, .x = x, .y = y});
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Event notification interface implementation: pointer device.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_notify_pointer_axis(
    struct rose_workspace* workspace, struct rose_pointer_event_axis event) {
    // If the screen is locked, then notify the seat of this event, and do
    // nothing else.
    if(workspace->context->is_screen_locked) {
        goto notify_seat;
    }

    // Do nothing if the workspace is not in normal mode.
    if(workspace->mode != rose_workspace_mode_normal) {
        return;
    }

    if(true) {
        // Select UI component under the pointer's coordinates.
        struct rose_workspace_ui_selection ui_selection =
            rose_workspace_ui_select(
                workspace, workspace->pointer.x, workspace->pointer.y);

        // If such component exists, then pass this event to it, and do nothing
        // else.
        if(ui_selection.type == rose_workspace_ui_selection_type_output) {
            // Process the event depending on where the pointer is.
            switch(ui_selection.output_ui_selection.type) {
                case rose_output_ui_selection_type_menu:
                    // Notify the menu of this event.
                    rose_ui_menu_notify_pointer_axis(
                        ui_selection.output_ui_selection.menu, event);

                    break;

                case rose_output_ui_selection_type_widget:
                    // Notify the widget of this event.
                    goto notify_seat;

                case rose_output_ui_selection_type_none:
                    // fall-through
                default:
                    break;
            }

            // Do nothing else.
            return;
        }
    }

notify_seat:
    enum wlr_axis_orientation orientation = (int)(event.orientation);
    enum wlr_axis_source source = (int)(event.source);

    wlr_seat_pointer_notify_axis(
        workspace->context->seat, event.time_msec, orientation, event.delta,
        event.delta_discrete, source);
}

void
rose_workspace_notify_pointer_button(
    struct rose_workspace* workspace, struct rose_pointer_event_button event) {
    // Obtain current seat.
    struct wlr_seat* seat = workspace->context->seat;

    // If the screen is locked, then cancel any interactive mode, notify the
    // seat of this event, and do nothing else.
    if(workspace->context->is_screen_locked) {
        // Cancel interactive mode.
        rose_workspace_cancel_interactive_mode(workspace);

        // Notify the seat.
        wlr_seat_pointer_notify_button(
            seat, event.time_msec, event.button,
            ((event.state == rose_pointer_button_state_released)
                 ? WLR_BUTTON_RELEASED
                 : WLR_BUTTON_PRESSED));

        // Do nothing else.
        return;
    }

    // If the workspace has no focused surface, or such surface is maximized or
    // in fullscreen mode, then cancel any interactive mode.
    if((workspace->focused_surface == NULL) ||
       (workspace->focused_surface->state.pending.is_maximized ||
        workspace->focused_surface->state.pending.is_fullscreen)) {
        rose_workspace_cancel_interactive_mode(workspace);
    }

    // Select UI component under the pointer's coordinates.
    struct rose_workspace_ui_selection ui_selection = rose_workspace_ui_select(
        workspace, workspace->pointer.x, workspace->pointer.y);

    // If the workspace is in interactive mode, or workspace's pointer is not
    // inside the UI area, then hide the menu.
    if((workspace->mode != rose_workspace_mode_normal) ||
       (ui_selection.type == rose_workspace_ui_selection_type_none)) {
        if(workspace->output != NULL) {
            rose_ui_menu_hide(&(workspace->output->ui.menu));
        }
    }

    // If left mouse button is released, and the workspace is in interactive
    // mode, then commit such mode, warp the pointer to its current location,
    // and do nothing else.
    if((event.button == BTN_LEFT) &&
       (event.state == rose_pointer_button_state_released) &&
       (workspace->mode != rose_workspace_mode_normal)) {
        // Commit interactive mode.
        rose_workspace_commit_interactive_mode(workspace);

        // Warp the pointer to its current location.
        rose_workspace_pointer_warp(
            workspace, event.time_msec, workspace->pointer.x,
            workspace->pointer.y);

        // Do nothing else.
        return;
    }

    // If the workspace is in normal mode, and the pointer is inside the UI
    // area, then pass this event to selected UI component, and do nothing else.
    if((workspace->mode == rose_workspace_mode_normal) &&
       (ui_selection.type != rose_workspace_ui_selection_type_none)) {
        // Process the event depending on where the pointer is.
        switch(ui_selection.type) {
            case rose_workspace_ui_selection_type_panel:
                // If left mouse button is pressed, then toggle the menu.
                if((event.button == BTN_LEFT) &&
                   (event.state == rose_pointer_button_state_pressed)) {
                    if(workspace->output != NULL) {
                        rose_ui_menu_toggle(&(workspace->output->ui.menu));
                    }
                }

                break;

            case rose_workspace_ui_selection_type_output:
                switch(ui_selection.output_ui_selection.type) {
                    case rose_output_ui_selection_type_menu:
                        // Notify the menu of this event.
                        rose_ui_menu_notify_pointer_button(
                            ui_selection.output_ui_selection.menu, event);

                        break;

                    case rose_output_ui_selection_type_widget:
                        // Notify the seat of this event.
                        wlr_seat_pointer_notify_button(
                            seat, event.time_msec, event.button,
                            ((event.state == rose_pointer_button_state_released)
                                 ? WLR_BUTTON_RELEASED
                                 : WLR_BUTTON_PRESSED));

                        break;

                    case rose_output_ui_selection_type_none:
                        // fall-through
                    default:
                        break;
                }

            case rose_workspace_ui_selection_type_none:
                // fall-through
            default:
                break;
        }

        // Do nothing else.
        return;
    }

    // If left mouse button is pressed, and the workspace is in normal mode,
    // then preform additional actions.
    if((event.button == BTN_LEFT) &&
       (event.state == rose_pointer_button_state_pressed) &&
       (workspace->mode == rose_workspace_mode_normal)) {
        // If there is no focused surface, then find a surface under the
        // pointer's coordinates, and focus it.
        if(workspace->focused_surface == NULL) {
            rose_workspace_focus_surface(
                workspace,
                rose_workspace_select_surface_at(
                    workspace, workspace->pointer.x, workspace->pointer.y));
        }

        // Perform the following actions at most twice (this can happen when
        // left mouse button is pressed, and the pointer is outside the focused
        // surface, so the focus can shift to another surface).
        for(int i = 0; i != 2; ++i) {
            // If at this point there is a focused surface, then perform
            // additional actions.
            if(workspace->focused_surface != NULL) {
                // Compute pointer-surface relation.
                enum rose_workspace_point_surface_relation relation =
                    rose_workspace_point_relate(
                        workspace->pointer.x, workspace->pointer.y,
                        workspace->focused_surface);

                // If the surface is either maximized, or in fullscreen mode,
                // then notify the seat of current event, and break out of the
                // cycle.
                if(workspace->focused_surface->state.pending.is_maximized ||
                   workspace->focused_surface->state.pending.is_fullscreen) {
                    // Only notify the seat when the pointer is inside the
                    // surface.
                    if(relation ==
                       rose_workspace_point_surface_relation_inside) {
                        wlr_seat_pointer_notify_button(
                            seat, event.time_msec, event.button,
                            WLR_BUTTON_PRESSED);
                    }

                    // Break out of the cycle.
                    break;
                }

                // Otherwise, the surface is in its normal state, so perform
                // additional checks.
                if(relation == rose_workspace_point_surface_relation_inside) {
                    // If the pointer is inside the surface, then either notify
                    // the seat of current event, or start interactive move
                    // mode.
                    if(workspace->context->is_waiting_for_user_interaction) {
                        // Start interactive move mode.
                        rose_workspace_mode_set(
                            workspace, event.time_msec,
                            rose_workspace_mode_interactive_move);
                    } else {
                        // Notify the seat.
                        wlr_seat_pointer_notify_button(
                            seat, event.time_msec, event.button,
                            WLR_BUTTON_PRESSED);
                    }
                } else if(
                    relation == rose_workspace_point_surface_relation_outside) {
                    // If the pointer is outside the surface, then find a
                    // surface under the pointer's coordinates and focus it.
                    rose_workspace_focus_surface(
                        workspace, rose_workspace_select_surface_at(
                                       workspace, workspace->pointer.x,
                                       workspace->pointer.y));

                    // Start over again.
                    continue;
                } else {
                    // Otherwise, start interactive mode.
#define resize_mode_                                \
    (rose_workspace_mode_interactive_resize_north + \
     (relation - rose_workspace_point_surface_relation_touches_north))

                    rose_workspace_mode_set(
                        workspace, event.time_msec,
                        (workspace->context->is_waiting_for_user_interaction
                             ? rose_workspace_mode_interactive_move
                             : resize_mode_));

#undef resize_mode_
                }
            }

            // Break out of the cycle.
            break;
        }

        return;
    }

    // If the workspace is in normal mode, then notify the seat of this event.
    if(workspace->mode == rose_workspace_mode_normal) {
        wlr_seat_pointer_notify_button(
            seat, event.time_msec, event.button,
            ((event.state == rose_pointer_button_state_released)
                 ? WLR_BUTTON_RELEASED
                 : WLR_BUTTON_PRESSED));
    }
}

void
rose_workspace_notify_pointer_move(
    struct rose_workspace* workspace, struct rose_pointer_event_motion event) {
    // Move the pointer by the given delta.
    rose_workspace_pointer_warp(
        workspace, event.time_msec, workspace->pointer.x + event.dx,
        workspace->pointer.y + event.dy);

    // Compute event's time in nanoseconds.
    uint64_t time_nsec = event.time_msec * 1000;

    // Send relative pointer motion event.
    wlr_relative_pointer_manager_v1_send_relative_motion(
        workspace->context->relative_pointer_manager, workspace->context->seat,
        time_nsec, event.dx, event.dy, event.dx_unaccel, event.dy_unaccel);
}

void
rose_workspace_notify_pointer_warp(
    struct rose_workspace* workspace,
    struct rose_pointer_event_motion_absolute event) {
    // Compute workspace-local coordinates from normalized coordinates, and warp
    // the pointer.
    rose_workspace_pointer_warp(
        workspace, event.time_msec, event.x * workspace->width,
        event.y * workspace->height);
}
