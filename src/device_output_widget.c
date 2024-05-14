// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))
#define max_(a, b) ((a) > (b) ? (a) : (b))

#define add_signal_(x, f) \
    wl_signal_add(&((x)->events.f), &(widget->listener_##f))

////////////////////////////////////////////////////////////////////////////////
// Layout-related utility functions and types.
////////////////////////////////////////////////////////////////////////////////

struct rose_screen_extent {
    int width, height;
};

static struct rose_screen_extent
rose_screen_extent_obtain(struct rose_output* output) {
    // Obtain output's state.
    struct rose_output_state output_state = rose_output_state_obtain(output);

    // Compute width and height.
    return (struct rose_screen_extent){
        .width = (int)(0.5 + (output_state.width / output_state.scale)),
        .height = (int)(0.5 + (output_state.height / output_state.scale))};
}

static struct rose_output_widget_state
rose_output_widget_compute_layout(
    struct rose_output* output, enum rose_output_widget_type type, int width,
    int height) {
    // Initialize widget's position.
    int x = 0, y = 0;

    // Obtain panel's data.
    struct rose_ui_panel panel = output->context->config.theme.panel;
    if(output->focused_workspace != NULL) {
        panel = output->focused_workspace->panel;

        if(panel.is_visible) {
            if(output->focused_workspace->focused_surface != NULL) {
                panel.is_visible = !(output->focused_workspace->focused_surface
                                         ->state.pending.is_fullscreen);
            }
        }
    }

    // Obtain screen extent.
    struct rose_screen_extent screen_extent = rose_screen_extent_obtain(output);

    // Perform type-dependent computation.
    switch(type) {
        case rose_output_widget_type_screen_lock:
            // fall-through
        case rose_output_widget_type_background: {
            x = (screen_extent.width - width) / 2;
            y = (screen_extent.height - height) / 2;

            break;
        }

        case rose_output_widget_type_notification: {
            int const margin = 5;

            // Perform computations depending on panel's position.
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    x = screen_extent.width - width - margin;
                    y = margin;

                    break;

                case rose_ui_panel_position_top:
                    x = screen_extent.width - width - margin;
                    y = margin + (panel.is_visible ? panel.size : 0);

                    break;

                case rose_ui_panel_position_right:
                    x = margin;
                    y = margin;

                    break;

                case rose_ui_panel_position_left:
                    x = screen_extent.width - width - margin;
                    y = margin;

                    break;

                default:
                    break;
            }

            break;
        }

        case rose_output_widget_type_prompt: {
            // Perform computations depending on panel's position.
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    break;

                case rose_ui_panel_position_right:
                    break;

                case rose_ui_panel_position_left:
                    x = (panel.is_visible ? panel.size : 0);
                    break;

                case rose_ui_panel_position_top:
                    y = (panel.is_visible ? panel.size : 0);
                    break;

                default:
                    break;
            }

            break;
        }

        case rose_output_widget_type_panel:
            // Perform computations depending on panel's position.
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    y = screen_extent.height - panel.size;
                    // fall-through
                case rose_ui_panel_position_top:
                    x = screen_extent.width / 2;
                    break;

                case rose_ui_panel_position_right:
                    x = screen_extent.width - panel.size;
                    y = screen_extent.height / 2;

                    break;

                case rose_ui_panel_position_left:
                    // fall-through
                default:
                    break;
            }

            break;

        default:
            break;
    }

    // Return calculated layout.
    return (struct rose_output_widget_state){
        .x = x, .y = y, .width = width, .height = height};
}

////////////////////////////////////////////////////////////////////////////////
// Initialization-related utility function declaration.
////////////////////////////////////////////////////////////////////////////////

static struct rose_output_widget*
rose_output_widget_create(
    enum rose_output_widget_type type,
    enum rose_output_widget_surface_type surface_type);

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_output_widget_surface_map(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the widget.
    struct rose_output_widget* widget =
        wl_container_of(listener, widget, listener_map);

    // Obtain the parent UI object.
    struct rose_output_ui* ui =
        ((widget->surface_type == rose_output_widget_surface_type_toplevel)
             ? widget->ui
             : widget->master->ui);

    // If widget's underlying surface is top-level XDG surface, then perform
    // additional actions.
    if(widget->surface_type == rose_output_widget_surface_type_toplevel) {
        // Add the widget to the list of mapped widgets.
        wl_list_insert(
            &(ui->widgets_mapped[widget->type]), &(widget->link_mapped));

        // Configure the widget.
        rose_output_widget_configure(widget);

        // Handle input focus, if needed.
        if((widget->type == rose_output_widget_type_screen_lock) ||
           (widget->type == rose_output_widget_type_prompt)) {
            rose_workspace_make_current(ui->output->context->current_workspace);
        }
    }

    // Request output's redraw, if needed.
    if(rose_output_widget_is_visible(widget)) {
        rose_output_request_redraw(ui->output);
    }
}

static void
rose_handle_event_output_widget_surface_unmap(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the widget.
    struct rose_output_widget* widget =
        wl_container_of(listener, widget, listener_unmap);

    // Obtain the parent output.
    struct rose_output* output =
        ((widget->surface_type == rose_output_widget_surface_type_toplevel)
             ? widget->output
             : widget->master->output);

    // Request output's redraw, if needed.
    if(rose_output_widget_is_visible(widget)) {
        rose_output_request_redraw(output);
    }

    // If widget's underlying surface is top-level XDG surface, then perform
    // additional actions.
    if(widget->surface_type == rose_output_widget_surface_type_toplevel) {
        // Remove the widget from the list of mapped widgets.
        wl_list_remove(&(widget->link_mapped));
        wl_list_init(&(widget->link_mapped));

        // Handle input focus, if needed.
        if((widget->type == rose_output_widget_type_screen_lock) ||
           (widget->type == rose_output_widget_type_prompt)) {
            rose_workspace_make_current(output->context->current_workspace);
        }
    }
}

static void
rose_handle_event_output_widget_surface_commit(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the widget.
    struct rose_output_widget* widget =
        wl_container_of(listener, widget, listener_commit);

    // Obtain the parent output.
    struct rose_output* output =
        ((widget->surface_type == rose_output_widget_surface_type_toplevel)
             ? widget->output
             : widget->master->output);

    // Position the widget, if needed.
    if(widget->surface_type == rose_output_widget_surface_type_toplevel) {
        if((widget->type < rose_output_special_widget_type_count_) ||
           (widget->type == rose_output_widget_type_notification)) {
            widget->state = rose_output_widget_compute_layout(
                output, widget->type,
                widget->xdg_surface->surface->current.width,
                widget->xdg_surface->surface->current.height);
        }
    }

    // Request output's redraw, if needed.
    if(rose_output_widget_is_visible(widget)) {
        rose_output_request_redraw(output);
    }
}

static void
rose_handle_event_output_widget_surface_new_subsurface(
    struct wl_listener* listener, void* data) {
    // Obtain the underlying subsurface.
    struct wlr_subsurface* subsurface = data;

    // Obtain the master widget.
    struct rose_output_widget* master =
        wl_container_of(listener, master, listener_new_subsurface);

    master =
        ((master->surface_type == rose_output_widget_surface_type_toplevel)
             ? master
             : master->master);

    // Create and initialize a new subsurface.
    struct rose_output_widget* widget = rose_output_widget_create(
        master->type, rose_output_widget_surface_type_subsurface);

    if(widget == NULL) {
        return;
    }

    // Set master widget.
    widget->master = master;

    // Send output enter event to the underlying surface.
    wlr_surface_send_enter(subsurface->surface, master->output->device);

    // Add the widget to the list of subsurfaces.
    wl_list_insert(&(master->subsurfaces), &(widget->link));

    // Register listeners.
    add_signal_(subsurface->surface, map);
    add_signal_(subsurface->surface, unmap);
    add_signal_(subsurface->surface, commit);

    add_signal_(subsurface->surface, new_subsurface);
    add_signal_(subsurface, destroy);
}

static void
rose_handle_event_output_widget_surface_new_popup(
    struct wl_listener* listener, void* data) {
    // Obtain the base XDG surface.
    struct wlr_xdg_surface* xdg_surface = ((struct wlr_xdg_popup*)(data))->base;

    // Obtain the master widget.
    struct rose_output_widget* master =
        wl_container_of(listener, master, listener_new_popup);

    master =
        ((master->surface_type == rose_output_widget_surface_type_toplevel)
             ? master
             : master->master);

    // Create and initialize a new temporary widget.
    struct rose_output_widget* widget = rose_output_widget_create(
        master->type, rose_output_widget_surface_type_temporary);

    if(widget == NULL) {
        return;
    }

    // Set master widget.
    widget->master = master;

    // Send output enter event to the underlying surface.
    wlr_surface_send_enter(xdg_surface->surface, master->output->device);

    // Add the widget to the list of temporaries.
    wl_list_insert(&(master->temporaries), &(widget->link));

    // Register listeners.
    add_signal_(xdg_surface->surface, map);
    add_signal_(xdg_surface->surface, unmap);
    add_signal_(xdg_surface->surface, commit);

    add_signal_(xdg_surface->surface, new_subsurface);
    add_signal_(xdg_surface, new_popup);
    add_signal_(xdg_surface, destroy);

    // Obtain screen extent.
    struct rose_screen_extent screen_extent =
        rose_screen_extent_obtain(master->output);

    // Set constraining box.
    struct wlr_box constraints = {
        .x = -master->state.x,
        .y = -master->state.y,
        .width = screen_extent.width,
        .height = screen_extent.height};

    wlr_xdg_popup_unconstrain_from_box(xdg_surface->popup, &constraints);
}

static void
rose_handle_event_output_widget_surface_destroy(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the widget.
    struct rose_output_widget* widget =
        wl_container_of(listener, widget, listener_destroy);

    // Destroy the widget.
    rose_output_widget_destroy(widget);
}

////////////////////////////////////////////////////////////////////////////////
// Initialization-related utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_output_widget*
rose_output_widget_create(
    enum rose_output_widget_type type,
    enum rose_output_widget_surface_type surface_type) {
    // Allocate memory for a new widget.
    struct rose_output_widget* widget =
        malloc(sizeof(struct rose_output_widget));

    if(widget == NULL) {
        return NULL;
    } else {
        *widget = (struct rose_output_widget){
            .type = type, .surface_type = surface_type};
    }

    // Initialize event listeners.
#define initialize_(f)                                   \
    {                                                    \
        widget->listener_##f.notify =                    \
            rose_handle_event_output_widget_surface_##f; \
        wl_list_init(&(widget->listener_##f.link));      \
    }

    initialize_(map);
    initialize_(unmap);
    initialize_(commit);

    initialize_(new_subsurface);
    initialize_(new_popup);
    initialize_(destroy);

#undef initialize_

    // Initialize lists of child entities.
    wl_list_init(&(widget->subsurfaces));
    wl_list_init(&(widget->temporaries));

    // Initialize list links.
    wl_list_init(&(widget->link));
    wl_list_init(&(widget->link_mapped));

    // Return initialized widget.
    return widget;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_widget_initialize(
    struct rose_output_ui* ui, struct wlr_xdg_toplevel* toplevel,
    enum rose_output_widget_type type) {
    // Obtain the base XDG surface.
    struct wlr_xdg_surface* xdg_surface = toplevel->base;

    // Create a new widget.
    struct rose_output_widget* widget = rose_output_widget_create(
        type, rose_output_widget_surface_type_toplevel);

    if(widget == NULL) {
        wlr_xdg_toplevel_send_close(toplevel);
        return;
    }

    // Set widget's parameters.
    widget->output = ui->output;
    widget->ui = ui;
    widget->xdg_surface = xdg_surface;

    // Send output enter event to the underlying surface.
    wlr_surface_send_enter(xdg_surface->surface, ui->output->device);

    // Register listeners.
    add_signal_(xdg_surface->surface, map);
    add_signal_(xdg_surface->surface, unmap);
    add_signal_(xdg_surface->surface, commit);

    add_signal_(xdg_surface->surface, new_subsurface);
    add_signal_(xdg_surface, new_popup);
    add_signal_(xdg_surface, destroy);

    // Add the widget to the UI.
    wl_list_insert(&(ui->widgets[type]), &(widget->link));

    // Configure the widget.
    rose_output_widget_configure(widget);
}

void
rose_output_widget_destroy(struct rose_output_widget* widget) {
    // Remove listeners from signals.
    wl_list_remove(&(widget->listener_map.link));
    wl_list_remove(&(widget->listener_unmap.link));
    wl_list_remove(&(widget->listener_commit.link));

    wl_list_remove(&(widget->listener_new_subsurface.link));
    wl_list_remove(&(widget->listener_new_popup.link));
    wl_list_remove(&(widget->listener_destroy.link));

    // Destroy all child entities.
    if(true) {
        struct rose_output_widget* x = NULL;
        struct rose_output_widget* _ = NULL;

        wl_list_for_each_safe(x, _, &(widget->subsurfaces), link) {
            rose_output_widget_destroy(x);
        }

        wl_list_for_each_safe(x, _, &(widget->temporaries), link) {
            rose_output_widget_destroy(x);
        }
    }

    // Obtain the parent output.
    struct rose_output* output =
        ((widget->surface_type == rose_output_widget_surface_type_toplevel)
             ? widget->output
             : widget->master->output);

    // Request output's redraw, if needed.
    if(rose_output_widget_is_visible(widget)) {
        rose_output_request_redraw(output);
    }

    // Remove the widget from the lists.
    wl_list_remove(&(widget->link));
    wl_list_remove(&(widget->link_mapped));

    // Free memory.
    free(widget);
}

////////////////////////////////////////////////////////////////////////////////
// Configuration interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_widget_make_current(struct rose_output_widget* widget) {
    // Only configure widgets whose underlying surface is top-level XDG surface.
    if(widget->surface_type != rose_output_widget_surface_type_toplevel) {
        return;
    }

    // Acquire keyboard focus.
    struct wlr_seat* seat = widget->output->context->seat;
    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);

    if(keyboard != NULL) {
        wlr_seat_keyboard_notify_enter(
            seat, widget->xdg_surface->surface, keyboard->keycodes,
            keyboard->num_keycodes, &(keyboard->modifiers));
    }
}

void
rose_output_widget_configure(struct rose_output_widget* widget) {
    // Only configure widgets whose underlying surface is top-level XDG surface.
    if(widget->surface_type != rose_output_widget_surface_type_toplevel) {
        return;
    }

    // Obtain the widget's parent output.
    struct rose_output* output = widget->output;

    // Obtain panel's data.
    struct rose_ui_panel panel = output->context->config.theme.panel;
    if(output->focused_workspace != NULL) {
        panel = output->focused_workspace->panel;

        if(panel.is_visible) {
            if(output->focused_workspace->focused_surface != NULL) {
                panel.is_visible = !(output->focused_workspace->focused_surface
                                         ->state.pending.is_fullscreen);
            }
        }
    }

    // Obtain screen extent.
    struct rose_screen_extent screen_extent = rose_screen_extent_obtain(output);

    // Compute widget's size.
    int width = 1, height = 1;
    switch(widget->type) {
        case rose_output_widget_type_screen_lock:
            // fall-through
        case rose_output_widget_type_background: {
            width = screen_extent.width;
            height = screen_extent.height;

            break;
        }

        case rose_output_widget_type_notification: {
            int const margin = 10;
            int const offset = (panel.is_visible ? panel.size : 0);

            // Perform computations depending on panel's position.
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    // fall-through
                case rose_ui_panel_position_top:
                    width = screen_extent.width / 2 - margin;
                    height = (screen_extent.height - offset) / 2 - margin;

                    break;

                case rose_ui_panel_position_right:
                    // fall-through
                case rose_ui_panel_position_left:
                    width = (screen_extent.width - offset) / 2 - margin;
                    height = screen_extent.height / 2 - margin;

                    break;

                default:
                    break;
            }

            break;
        }

        case rose_output_widget_type_prompt: {
            // Perform computations depending on panel's position.
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    // fall-through
                case rose_ui_panel_position_top:
                    width = screen_extent.width;
                    height = panel.size;

                    break;

                case rose_ui_panel_position_right:
                    // fall-through
                case rose_ui_panel_position_left:
                    width = screen_extent.width -
                            (panel.is_visible ? panel.size : 0);

                    height = panel.size;

                    break;

                default:
                    break;
            }

            break;
        }

        case rose_output_widget_type_panel: {
            // Perform computations depending on panel's position.
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    // fall-through
                case rose_ui_panel_position_top:
                    width = screen_extent.width / 2;
                    height = panel.size;

                    break;

                case rose_ui_panel_position_right:
                    // fall-through
                case rose_ui_panel_position_left:
                    width = panel.size;
                    height = screen_extent.height / 2;

                    break;

                default:
                    break;
            }

            break;
        }

        default:
            break;
    }

    // Clamp widget's size.
    width = max_(width, 1);
    height = max_(height, 1);

    // Set widget surface's size.
    wlr_xdg_toplevel_set_size(widget->xdg_surface->toplevel, width, height);

    // Compute widget's layout.
    widget->state =
        rose_output_widget_compute_layout(output, widget->type, width, height);

    // Request activated and maximized state for the underlying surface.
    wlr_xdg_toplevel_set_activated(widget->xdg_surface->toplevel, true);
    wlr_xdg_toplevel_set_maximized(widget->xdg_surface->toplevel, true);
}

////////////////////////////////////////////////////////////////////////////////
// State query interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_widget_state
rose_output_widget_state_obtain(struct rose_output_widget* widget) {
    // State is meaningful only for widgets whose underlying surface is
    // top-level XDG surface.
    return widget->state;
}

bool
rose_output_widget_is_visible(struct rose_output_widget* widget) {
    // A child widget is visible if its master widget is visible.
    if(widget->surface_type != rose_output_widget_surface_type_toplevel) {
        return rose_output_widget_is_visible(widget->master);
    }

    // Normal widgets are not visible if the screen is locked.
    if(widget->output->context->is_screen_locked &&
       (widget->type >= rose_output_special_widget_type_count_)) {
        return false;
    }

    // Screen lock widget is not visible if the screen is not locked.
    if(!(widget->output->context->is_screen_locked) &&
       (widget->type == rose_output_widget_type_screen_lock)) {
        return false;
    }

    // A widget is not visible if its surface is not mapped.
    if(wl_list_empty(&(widget->link_mapped))) {
        return false;
    }

    // Panel's widget is visible if the panel is visible.
    if(widget->type == rose_output_widget_type_panel) {
        // Obtain output's focused workspace.
        struct rose_workspace* workspace = widget->output->focused_workspace;

        // Compute panel's visibility flag.
        bool is_panel_visible =
            ((workspace == NULL) ? false : workspace->panel.is_visible);

        if(is_panel_visible) {
            if(workspace->focused_surface != NULL) {
                is_panel_visible =
                    !(workspace->focused_surface->state.pending.is_fullscreen);
            }
        }

        // Return panel's visibility flag.
        return is_panel_visible;
    }

    // The widget is visible.
    return true;
}
