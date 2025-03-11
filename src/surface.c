// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>

#include <stddef.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

#define add_signal_(x, f) \
    wl_signal_add(&((x)->events.f), &(surface->listener_##f))

////////////////////////////////////////////////////////////////////////////////
// State handling-related utility functions.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_surface_state_equal(
    struct rose_surface_state* x, struct rose_surface_state* y) {
    return (
        (x->width == y->width) && (x->height == y->height) &&
        (x->is_activated == y->is_activated) &&
        (x->is_maximized == y->is_maximized) &&
        (x->is_fullscreen == y->is_fullscreen));
}

static bool
rose_surface_is_decoration_configured(struct rose_surface* surface) {
    return (
        (surface->type != rose_surface_type_toplevel) ||
        (surface->xdg_decoration == NULL) ||
        (surface->xdg_decoration->current.mode ==
         WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE));
}

static void
rose_surface_state_sync(struct rose_surface* surface) {
    // Save previous state.
    surface->state.previous = surface->state.current;

    // Update current state.
    if(surface->type == rose_surface_type_subsurface) {
        surface->state.current = (struct rose_surface_state){
            .x = surface->subsurface->current.x,
            .y = surface->subsurface->current.y,
            .width = surface->subsurface->surface->current.width,
            .height = surface->subsurface->surface->current.height};
    } else if(surface->type == rose_surface_type_temporary) {
        surface->state.current = (struct rose_surface_state){
            .x = surface->xdg_surface->popup->current.geometry.x,
            .y = surface->xdg_surface->popup->current.geometry.y,
            .width = surface->xdg_surface->surface->current.width,
            .height = surface->xdg_surface->surface->current.height};
    } else if(surface->type == rose_surface_type_toplevel) {
        surface->state.current = (struct rose_surface_state){
            .x = surface->state.current.x,
            .y = surface->state.current.y,
            .width = surface->xdg_surface->surface->current.width,
            .height = surface->xdg_surface->surface->current.height,
            .is_activated = surface->xdg_surface->toplevel->current.activated,
            .is_maximized = surface->xdg_surface->toplevel->current.maximized,
            .is_minimized = surface->state.pending.is_minimized,
            .is_fullscreen =
                surface->xdg_surface->toplevel->current.fullscreen};
    }
}

static void
rose_surface_set_decoration_mode(struct rose_surface* surface) {
    // Do nothing if surface's decoration is already configured.
    if(rose_surface_is_decoration_configured(surface)) {
        return;
    }

    // Otherwise, always request server-side decorations.
    wlr_xdg_toplevel_decoration_v1_set_mode(
        surface->xdg_decoration,
        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

    // And start surface's transaction, if needed.
    if((surface->widget_type == rose_surface_widget_type_none) &&
       !(surface->is_transaction_running)) {
        // Set the flag.
        surface->is_transaction_running = true;

        // Start workspace's transaction, if needed.
        rose_workspace_transaction_start(surface->parent.workspace);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Surface snapshot construction-related utility function and type.
////////////////////////////////////////////////////////////////////////////////

struct rose_surface_snapshot_construction_context {
    struct rose_workspace* workspace;
    int dx, dy;
};

static void
rose_surface_construct_snapshot(
    struct wlr_surface* surface, int x, int y, void* data) {
    // Obtain the construction context.
    struct rose_surface_snapshot_construction_context* context = data;

#define obtain_surface_snapshot_(surface)                          \
    if((surface)->data != NULL) {                                  \
        surface_snapshot =                                         \
            &(((struct rose_surface*)((surface)->data))            \
                  ->snapshots[rose_surface_snapshot_type_normal]); \
    }

    // Obtain the corresponding snapshot.
    struct rose_surface_snapshot* surface_snapshot = NULL;
    if(true) {
        // First, try obtaining an XDG surface snapshot.
        struct wlr_xdg_surface* xdg_surface =
            wlr_xdg_surface_try_from_wlr_surface(surface);

        if(xdg_surface != NULL) {
            obtain_surface_snapshot_(xdg_surface);
        } else {
            // If the surface is not an XDG surface, then try obtaining a
            // subsurface snapshot.
            struct wlr_subsurface* subsurface =
                wlr_subsurface_try_from_wlr_surface(surface);

            if(subsurface != NULL) {
                obtain_surface_snapshot_(subsurface);
            }
        }
    }

#undef obtain_surface_snapshot_

    // If surface's snapshot exists, then add it to workspace's snapshot.
    if(surface_snapshot != NULL) {
        // Initialize the snapshot.
        struct rose_surface_snapshot_parameters parameters = {
            .type = rose_surface_snapshot_type_normal,
            .surface = surface,
            .x = context->dx + x,
            .y = context->dy + y};

        rose_surface_snapshot_destroy(surface_snapshot);
        rose_surface_snapshot_initialize(surface_snapshot, parameters);

        // And add it to workspace's snapshot.
        wl_list_insert(
            &(context->workspace->transaction.snapshot.surfaces),
            &(surface_snapshot->link));
    }
}

////////////////////////////////////////////////////////////////////////////////
// Surface creating utility function declaration.
////////////////////////////////////////////////////////////////////////////////

static struct rose_surface*
rose_surface_create(enum rose_surface_type type);

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_surface_decoration_request_mode(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_decoration_request_mode);

    // Configure surface's decoration.
    rose_surface_set_decoration_mode(surface);
}

static void
rose_handle_event_surface_decoration_destroy(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_decoration_destroy);

    // Remove listeners from signals.
    wl_list_remove(&(surface->listener_decoration_request_mode.link));
    wl_list_remove(&(surface->listener_decoration_destroy.link));

    wl_list_init(&(surface->listener_decoration_request_mode.link));
    wl_list_init(&(surface->listener_decoration_destroy.link));

    // Unlink the decoration.
    surface->xdg_decoration = NULL;
}

static void
rose_handle_event_surface_pointer_constraint_set_region(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface = wl_container_of(
        listener, surface, listener_pointer_constraint_set_region);

    // Do nothing if the surface is output's widget.
    if(surface->widget_type != rose_surface_widget_type_none) {
        return;
    }

    // Obtain parent workspace.
    struct rose_workspace* workspace = surface->parent.workspace;

    // Do nothing else if the workspace is not current.
    if(!rose_workspace_is_current(workspace)) {
        return;
    }

    // Notify the workspace.
    if(workspace->focused_surface == surface) {
        struct wlr_pointer_motion_event event = {
            .time_msec = workspace->pointer.movement_time};

        rose_workspace_notify_pointer_move(workspace, event);
    }
}

static void
rose_handle_event_surface_pointer_constraint_destroy(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_pointer_constraint_destroy);

    // Remove listeners from signals.
    wl_list_remove(&(surface->listener_pointer_constraint_set_region.link));
    wl_list_remove(&(surface->listener_pointer_constraint_destroy.link));

    wl_list_init(&(surface->listener_pointer_constraint_set_region.link));
    wl_list_init(&(surface->listener_pointer_constraint_destroy.link));

    // Unlink the constraint.
    surface->pointer_constraint = NULL;
}

static void
rose_handle_event_surface_request_maximize(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_request_maximize);

    // Do nothing if the surface is not initialized yet.
    if(!(surface->xdg_surface->initialized)) {
        return;
    }

    // Configure the surface within its workspace.
    if(surface->widget_type == rose_surface_widget_type_none) {
        rose_workspace_surface_configure(
            surface->parent.workspace, surface,
            (struct rose_surface_configuration_parameters){
                .flags = rose_surface_configure_maximized |
                         rose_surface_configure_no_transaction,
                .is_maximized =
                    surface->xdg_surface->toplevel->requested.maximized});
    }

    // Note: This is required by the XDG shell protocol.
    wlr_xdg_surface_schedule_configure(surface->xdg_surface);
}

static void
rose_handle_event_surface_request_fullscreen(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_request_fullscreen);

    // Do nothing if the surface is not initialized yet.
    if(!(surface->xdg_surface->initialized)) {
        return;
    }

    // Configure the surface within its workspace.
    if(surface->widget_type == rose_surface_widget_type_none) {
        rose_workspace_surface_configure(
            surface->parent.workspace, surface,
            (struct rose_surface_configuration_parameters){
                .flags = rose_surface_configure_fullscreen |
                         rose_surface_configure_no_transaction,
                .is_fullscreen =
                    surface->xdg_surface->toplevel->requested.fullscreen});
    }

    // Note: This is required by the XDG shell protocol.
    wlr_xdg_surface_schedule_configure(surface->xdg_surface);
}

static void
rose_handle_event_surface_set_title(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_set_title);

    // Set the flag.
    surface->is_name_updated = true;

    // Notify the parent workspace of this event.
    if(surface->widget_type == rose_surface_widget_type_none) {
        rose_workspace_notify_surface_name_update(
            surface->parent.workspace, surface);
    }
}

static void
rose_handle_event_surface_set_app_id(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_set_app_id);

    // Set the flag.
    surface->is_name_updated = true;

    // Notify the parent workspace of this event.
    if(surface->widget_type == rose_surface_widget_type_none) {
        rose_workspace_notify_surface_name_update(
            surface->parent.workspace, surface);
    }
}

static void
rose_handle_event_surface_map(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_map);

    // Set the flag.
    surface->is_mapped = true;

    // Synchronize surface's state.
    rose_surface_state_sync(surface);

    // Update surface's pending state.
    surface->state.pending = surface->state.current;

    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Notify the parent of this event.
    if(master->widget_type == rose_surface_widget_type_none) {
        rose_workspace_notify_surface_map(master->parent.workspace, surface);
    } else {
        rose_output_ui_notify_surface_map(master->parent.ui, surface);
    }
}

static void
rose_handle_event_surface_unmap(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_unmap);

    // Clear the flag.
    surface->is_mapped = false;

    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Notify the parent of this event.
    if(master->widget_type == rose_surface_widget_type_none) {
        rose_workspace_notify_surface_unmap(master->parent.workspace, surface);
    } else {
        rose_output_ui_notify_surface_unmap(master->parent.ui, surface);
    }
}

static void
rose_handle_event_surface_commit(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_commit);

    // Synchronize surface's state.
    rose_surface_state_sync(surface);

    // Perform additional actions for non-toplevel surfaces.
    if(surface->type != rose_surface_type_toplevel) {
        // Update surface's pending state.
        surface->state.pending = surface->state.current;
    }

    // Perform additional actions for toplevel surfaces.
    if((surface->type == rose_surface_type_toplevel) &&
       (surface->xdg_surface->initial_commit)) {
        // Update surface's pending state.
        surface->state.pending = surface->state.current;

        // Configure surface's decoration.
        rose_surface_set_decoration_mode(surface);
    } else if(surface->type == rose_surface_type_toplevel) {
        // Commit surface's transaction, if needed. A widget surface shall never
        // have a running transaction.
        if(surface->is_transaction_running &&
           rose_surface_state_equal(
               &(surface->state.current), &(surface->state.pending)) &&
           rose_surface_is_decoration_configured(surface)) {
            // Stop the transaction.
            surface->is_transaction_running = false;

            // Update parent workspace's transaction.
            rose_workspace_transaction_update(surface->parent.workspace);
        }

        // If there is no running transaction, then update surface's state and
        // coordinates.
        if(!(surface->is_transaction_running)) {
            // Update surface's coordinates.
            surface->state.current.x = surface->state.pending.x;
            surface->state.current.y = surface->state.pending.y;

            // Update surface's pending state.
            surface->state.pending = surface->state.current;
        }
    }

    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Notify the parent of this event.
    if(master->widget_type == rose_surface_widget_type_none) {
        rose_workspace_notify_surface_commit(master->parent.workspace, surface);
    } else {
        rose_output_ui_notify_surface_commit(master->parent.ui, surface);
    }
}

static void
rose_handle_event_surface_new_subsurface(
    struct wl_listener* listener, void* data) {
    // Obtain the underlying subsurface.
    struct wlr_subsurface* subsurface = data;

    // Obtain the master surface.
    struct rose_surface* master = NULL;
    if(true) {
        struct rose_surface* surface =
            wl_container_of(listener, surface, listener_new_subsurface);

        master =
            ((surface->type == rose_surface_type_toplevel) ? surface
                                                           : surface->master);
    }

    // Create a new surface and link it with its underlying implementation.
    struct rose_surface* surface =
        rose_surface_create(rose_surface_type_subsurface);

    if(surface == NULL) {
        return;
    } else {
        subsurface->data = surface;
        surface->subsurface = subsurface;
    }

    // Set surface's parameters.
    surface->master = master;

    // Add the surface to the list of subsurfaces.
    wl_list_insert(&(master->subsurfaces), &(surface->link));

    // Register listeners.
    add_signal_(subsurface->surface, map);
    add_signal_(subsurface->surface, unmap);
    add_signal_(subsurface->surface, commit);

    add_signal_(subsurface->surface, new_subsurface);
    add_signal_(subsurface, destroy);
}

static void
rose_handle_event_surface_new_popup(struct wl_listener* listener, void* data) {
    // Obtain the base XDG surface.
    struct wlr_xdg_surface* xdg_surface = ((struct wlr_xdg_popup*)(data))->base;

    // Obtain the master surface.
    struct rose_surface* master = NULL;
    if(true) {
        struct rose_surface* surface =
            wl_container_of(listener, surface, listener_new_popup);

        master =
            ((surface->type == rose_surface_type_toplevel) ? surface
                                                           : surface->master);
    }

    // Create a new surface and link it with its underlying implementation.
    struct rose_surface* surface =
        rose_surface_create(rose_surface_type_temporary);

    if(surface == NULL) {
        return;
    } else {
        xdg_surface->data = surface;
        surface->xdg_surface = xdg_surface;
    }

    // Set surface's parameters.
    surface->master = master;

    // Add the surface to the list of temporary surfaces.
    wl_list_insert(&(master->temporaries), &(surface->link));

    // Register listeners.
    add_signal_(xdg_surface->surface, map);
    add_signal_(xdg_surface->surface, unmap);
    add_signal_(xdg_surface->surface, commit);

    add_signal_(xdg_surface->surface, new_subsurface);
    add_signal_(xdg_surface, new_popup);
    add_signal_(xdg_surface->popup, destroy);
}

static void
rose_handle_event_surface_destroy(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the surface.
    struct rose_surface* surface =
        wl_container_of(listener, surface, listener_destroy);

    // Destroy the surface.
    rose_surface_destroy(surface);
}

////////////////////////////////////////////////////////////////////////////////
// Surface creating utility function definition.
////////////////////////////////////////////////////////////////////////////////

static struct rose_surface*
rose_surface_create(enum rose_surface_type type) {
    // Allocate memory for a new surface.
    struct rose_surface* surface = malloc(sizeof(struct rose_surface));
    if(surface == NULL) {
        return NULL;
    } else {
        *surface = (struct rose_surface){.type = type, .is_name_updated = true};
    }

    // Initialize event listeners.
#define initialize_(f)                                                \
    {                                                                 \
        surface->listener_##f.notify = rose_handle_event_surface_##f; \
        wl_list_init(&(surface->listener_##f.link));                  \
    }

    initialize_(decoration_request_mode);
    initialize_(decoration_destroy);

    initialize_(pointer_constraint_set_region);
    initialize_(pointer_constraint_destroy);

    initialize_(request_maximize);
    initialize_(request_fullscreen);

    initialize_(set_title);
    initialize_(set_app_id);

    initialize_(map);
    initialize_(unmap);
    initialize_(commit);

    initialize_(new_subsurface);
    initialize_(new_popup);
    initialize_(destroy);

#undef initialize_

    // Initialize lists of child entities.
    wl_list_init(&(surface->subsurfaces));
    wl_list_init(&(surface->temporaries));

    // Initialize list links.
    wl_list_init(&(surface->link));
    wl_list_init(&(surface->link_layout));
    wl_list_init(&(surface->link_mapped));
    wl_list_init(&(surface->link_visible));

    // Initialize the snapshots.
    for(ptrdiff_t i = 0; i != rose_surface_snapshot_type_count_; ++i) {
        wl_list_init(&(surface->snapshots[i].link));
    }

    return surface;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_initialize(struct rose_surface_parameters parameters) {
    // Obtain the base XDG surface.
    struct wlr_xdg_surface* xdg_surface = parameters.toplevel->base;

    // Create a new surface and link it with its underlying implementation.
    struct rose_surface* surface =
        rose_surface_create(rose_surface_type_toplevel);

    if(surface == NULL) {
        wlr_xdg_toplevel_send_close(xdg_surface->toplevel);
        return;
    } else {
        xdg_surface->data = surface;
        surface->xdg_surface = xdg_surface;
    }

    // Set surface's parameters.
    surface->widget_type = parameters.widget_type;
    if(surface->widget_type == rose_surface_widget_type_none) {
        surface->parent.workspace = NULL;
    } else {
        surface->parent.ui = NULL;
    }

    surface->xdg_decoration = NULL;
    surface->pointer_constraint = NULL;

    // Register listeners.
    add_signal_(parameters.toplevel, request_maximize);
    add_signal_(parameters.toplevel, request_fullscreen);

    add_signal_(parameters.toplevel, set_title);
    add_signal_(parameters.toplevel, set_app_id);

    add_signal_(xdg_surface->surface, map);
    add_signal_(xdg_surface->surface, unmap);
    add_signal_(xdg_surface->surface, commit);

    add_signal_(xdg_surface->surface, new_subsurface);
    add_signal_(xdg_surface, new_popup);
    add_signal_(parameters.toplevel, destroy);

    // Add the surface to its parent.
    if(surface->widget_type == rose_surface_widget_type_none) {
        rose_workspace_add_surface(parameters.parent.workspace, surface);
    } else {
        rose_output_ui_add_surface(parameters.parent.ui, surface);
    }

    // Initialize surface's pointer constraint, if any.
    if(parameters.pointer_constraint != NULL) {
        rose_surface_pointer_constraint_initialize(
            parameters.pointer_constraint);
    }
}

void
rose_surface_destroy(struct rose_surface* surface) {
    // Remove listeners from signals.
    wl_list_remove(&(surface->listener_decoration_request_mode.link));
    wl_list_remove(&(surface->listener_decoration_destroy.link));

    wl_list_remove(&(surface->listener_pointer_constraint_set_region.link));
    wl_list_remove(&(surface->listener_pointer_constraint_destroy.link));

    wl_list_remove(&(surface->listener_request_maximize.link));
    wl_list_remove(&(surface->listener_request_fullscreen.link));

    wl_list_remove(&(surface->listener_set_title.link));
    wl_list_remove(&(surface->listener_set_app_id.link));

    wl_list_remove(&(surface->listener_map.link));
    wl_list_remove(&(surface->listener_unmap.link));
    wl_list_remove(&(surface->listener_commit.link));

    wl_list_remove(&(surface->listener_new_subsurface.link));
    wl_list_remove(&(surface->listener_new_popup.link));
    wl_list_remove(&(surface->listener_destroy.link));

    // Destroy all subsurfaces and temporary surfaces.
    if(true) {
        struct rose_surface* x = NULL;
        struct rose_surface* _ = NULL;

        wl_list_for_each_safe(x, _, &(surface->subsurfaces), link) {
            rose_surface_destroy(x);
        }

        wl_list_for_each_safe(x, _, &(surface->temporaries), link) {
            rose_surface_destroy(x);
        }
    }

    // Destroy surface's snapshots.
    for(ptrdiff_t i = 0; i != rose_surface_snapshot_type_count_; ++i) {
        rose_surface_snapshot_destroy(&(surface->snapshots[i]));
    }

    // Remove the link between the surface and its underlying implementation.
    if(surface->type == rose_surface_type_subsurface) {
        surface->subsurface->data = NULL;
    } else {
        surface->xdg_surface->data = NULL;
    }

    // Perform type-dependent destruction.
    if(surface->type == rose_surface_type_toplevel) {
        if(surface->widget_type == rose_surface_widget_type_none) {
            rose_workspace_remove_surface(surface->parent.workspace, surface);
        } else {
            rose_output_ui_remove_surface(surface->parent.ui, surface);
        }
    } else {
        wl_list_remove(&(surface->link));
    }

    // Free memory.
    free(surface);
}

////////////////////////////////////////////////////////////////////////////////
// Extension initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_decoration_initialize(
    struct wlr_xdg_toplevel_decoration_v1* xdg_decoration) {
    // Obtain the surface.
    struct rose_surface* surface = xdg_decoration->toplevel->base->data;

    // Do nothing else if there is no top-level surface, or if it already has a
    // decoration.
    if((surface == NULL) || (surface->type != rose_surface_type_toplevel) ||
       (surface->xdg_decoration != NULL)) {
        return;
    }

    // Register listeners.
    wl_signal_add(
        &(xdg_decoration->events.request_mode),
        &(surface->listener_decoration_request_mode));

    wl_signal_add(
        &(xdg_decoration->events.destroy),
        &(surface->listener_decoration_destroy));

    // Set the decoration.
    surface->xdg_decoration = xdg_decoration;
}

void
rose_surface_pointer_constraint_initialize(
    struct wlr_pointer_constraint_v1* pointer_constraint) {
    // Obtain the target XDG surface.
    struct wlr_xdg_surface* xdg_surface =
        wlr_xdg_surface_try_from_wlr_surface(pointer_constraint->surface);

    if(xdg_surface == NULL) {
        return;
    }

    // Obtain the surface.
    struct rose_surface* surface = xdg_surface->data;

    // Do nothing else if there is no top-level surface, or if it already has a
    // constraint.
    if((surface == NULL) || (surface->type != rose_surface_type_toplevel) ||
       (surface->pointer_constraint != NULL)) {
        return;
    }

    // Register listeners.
    wl_signal_add(
        &(pointer_constraint->events.set_region),
        &(surface->listener_pointer_constraint_set_region));

    wl_signal_add(
        &(pointer_constraint->events.destroy),
        &(surface->listener_pointer_constraint_destroy));

    // Set the pointer constraint.
    surface->pointer_constraint = pointer_constraint;

    // Do nothing else if the surface is output's widget.
    if(surface->widget_type != rose_surface_widget_type_none) {
        return;
    }

    // Obtain parent workspace.
    struct rose_workspace* workspace = surface->parent.workspace;

    // Do nothing else if the workspace is not current.
    if(!rose_workspace_is_current(workspace)) {
        return;
    }

    // Notify the workspace.
    if(workspace->focused_surface == surface) {
        struct wlr_pointer_motion_event event = {
            .time_msec = workspace->pointer.movement_time};

        // Activate the constraint.
        wlr_pointer_constraint_v1_send_activated(pointer_constraint);

        // Move workspace's pointer.
        rose_workspace_notify_pointer_move(workspace, event);
    }
}

////////////////////////////////////////////////////////////////////////////////
// State change requesting interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_request_close(struct rose_surface* surface) {
    if(surface->type == rose_surface_type_toplevel) {
        wlr_xdg_toplevel_send_close(surface->xdg_surface->toplevel);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Output entering/leaving interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_output_enter(
    struct rose_surface* surface, struct rose_output* output) {
    // Note: This operation is only meaningful for top-level surfaces.
    if(surface->type == rose_surface_type_toplevel) {
        // Send the event to the main surface.
        wlr_surface_send_enter(surface->xdg_surface->surface, output->device);

        // Send the event to all subsurfaces and temporary surfaces.
        if(true) {
            struct rose_surface* x = NULL;
            struct rose_surface* _ = NULL;

            wl_list_for_each_safe(x, _, &(surface->subsurfaces), link) {
                wlr_surface_send_enter(x->subsurface->surface, output->device);
            }

            wl_list_for_each_safe(x, _, &(surface->temporaries), link) {
                wlr_surface_send_enter(x->xdg_surface->surface, output->device);
            }
        }
    }
}

void
rose_surface_output_leave(
    struct rose_surface* surface, struct rose_output* output) {
    // Note: This operation is only meaningful for top-level surfaces.
    if(surface->type == rose_surface_type_toplevel) {
        // Send the event to the main surface.
        wlr_surface_send_leave(surface->xdg_surface->surface, output->device);

        // Send the event to all subsurfaces and temporary surfaces.
        if(true) {
            struct rose_surface* x = NULL;
            struct rose_surface* _ = NULL;

            wl_list_for_each_safe(x, _, &(surface->subsurfaces), link) {
                wlr_surface_send_leave(x->subsurface->surface, output->device);
            }

            wl_list_for_each_safe(x, _, &(surface->temporaries), link) {
                wlr_surface_send_leave(x->xdg_surface->surface, output->device);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Configuration interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_make_current(struct rose_surface* surface, struct wlr_seat* seat) {
    // Only configure top-level surfaces.
    if(surface->type != rose_surface_type_toplevel) {
        return;
    }

    // Acquire keyboard focus.
    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);

    if(keyboard != NULL) {
        wlr_seat_keyboard_notify_enter(
            seat, surface->xdg_surface->surface, keyboard->keycodes,
            keyboard->num_keycodes, &(keyboard->modifiers));
    }
}

void
rose_surface_configure(
    struct rose_surface* surface,
    struct rose_surface_configuration_parameters parameters) {
    // Only configure top-level surfaces.
    if(surface->type != rose_surface_type_toplevel) {
        return;
    }

    // Set surface's parameters and compute target state.
    struct rose_surface_state target = surface->state.pending;
    if(true) {
        if((parameters.flags & rose_surface_configure_size) != 0) {
            target.width = parameters.width;
            target.height = parameters.height;

            wlr_xdg_toplevel_set_size(
                surface->xdg_surface->toplevel, parameters.width,
                parameters.height);
        }

        if((parameters.flags & rose_surface_configure_position) != 0) {
            target.x = parameters.x;
            target.y = parameters.y;
        }

        if((parameters.flags & rose_surface_configure_activated) != 0) {
            target.is_activated = parameters.is_activated;
            wlr_xdg_toplevel_set_activated(
                surface->xdg_surface->toplevel, parameters.is_activated);
        }

        if((parameters.flags & rose_surface_configure_maximized) != 0) {
            target.is_maximized = parameters.is_maximized;
            wlr_xdg_toplevel_set_maximized(
                surface->xdg_surface->toplevel, parameters.is_maximized);
        }

        if((parameters.flags & rose_surface_configure_minimized) != 0) {
            target.is_minimized = parameters.is_minimized;
        }

        if((parameters.flags & rose_surface_configure_fullscreen) != 0) {
            target.is_fullscreen = parameters.is_fullscreen;
            wlr_xdg_toplevel_set_fullscreen(
                surface->xdg_surface->toplevel, parameters.is_fullscreen);
        }
    }

    // Update surface's transaction, if needed.
    if((parameters.flags & rose_surface_configure_no_transaction) == 0) {
        if(!rose_surface_state_equal(&target, &(surface->state.pending))) {
            if(surface->widget_type == rose_surface_widget_type_none) {
                if(!(surface->is_transaction_running)) {
                    // Set the flag.
                    surface->is_transaction_running = true;

                    // Start workspace's transaction, if needed.
                    rose_workspace_transaction_start(surface->parent.workspace);
                }
            }
        }
    }

    // If there is no running transaction, then apply immediate updates.
    if(!(surface->is_transaction_running)) {
        // Update position.
        surface->state.previous.x = surface->state.current.x;
        surface->state.previous.y = surface->state.current.y;
        surface->state.current.x = target.x;
        surface->state.current.y = target.y;

        // Update flags.
        if(true) {
            surface->state.previous.is_minimized =
                surface->state.current.is_minimized;

            surface->state.current.is_minimized = target.is_minimized;
        }

        // Handle surface's movement.
        if((target.x != surface->state.pending.x) ||
           (target.y != surface->state.pending.y)) {
            // Obtain the parent output.
            struct rose_output* output =
                ((surface->widget_type == rose_surface_widget_type_none)
                     ? surface->parent.workspace->output
                     : surface->parent.ui->output);

            // Damage the output.
            if(output != NULL) {
                // Determine surface's visibility.
                bool is_surface_visible = false;
                if(surface->widget_type == rose_surface_widget_type_none) {
                    is_surface_visible =
                        (surface->is_visible &&
                         !(output->context->is_screen_locked));
                } else {
                    is_surface_visible = rose_output_ui_is_surface_visible(
                        surface->parent.ui, surface);
                };

                // Add surface's damage to the output.
                if(is_surface_visible) {
                    rose_output_add_surface_damage(output, surface);
                }
            }
        }
    }

    // Update pending state.
    surface->state.pending = target;
}

////////////////////////////////////////////////////////////////////////////////
// State query interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_surface_state
rose_surface_state_obtain(struct rose_surface* surface) {
    return surface->state.current;
}

////////////////////////////////////////////////////////////////////////////////
// Transaction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_transaction_initialize_snapshot(struct rose_surface* surface) {
    // This function is meaningful only for top-level surfaces which are not
    // output's widgets.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type != rose_surface_widget_type_none)) {
        return;
    }

    // Construct snapshots for the surface itself and all of its child entities.
    struct rose_surface_snapshot_construction_context context = {
        .workspace = surface->parent.workspace,
        .dx = surface->state.current.x,
        .dy = surface->state.current.y};

    wlr_xdg_surface_for_each_surface(
        surface->xdg_surface, rose_surface_construct_snapshot, &context);

    // If the surface is decorated, then construct its decoration's snapshot.
    if(!(surface->state.current.is_maximized ||
         surface->state.current.is_fullscreen) &&
       rose_surface_is_decoration_configured(surface)) {
        // Obtain the snapshot.
        struct rose_surface_snapshot* surface_snapshot =
            &(surface->snapshots[rose_surface_snapshot_type_decoration]);

        // Initialize snapshot's parameters.
        struct rose_surface_snapshot_parameters parameters = {
            .type = rose_surface_snapshot_type_decoration,
            .surface = surface->xdg_surface->surface,
            .x = context.dx,
            .y = context.dy};

        // Initialize the snapshot.
        rose_surface_snapshot_destroy(surface_snapshot);
        rose_surface_snapshot_initialize(surface_snapshot, parameters);

        // Add surface's snapshot to workspace's snapshot.
        wl_list_insert(
            &(surface->parent.workspace->transaction.snapshot.surfaces),
            &(surface_snapshot->link));
    }
}

void
rose_surface_transaction_commit(struct rose_surface* surface) {
    // This function is meaningful only for top-level surfaces.
    if(surface->type != rose_surface_type_toplevel) {
        return;
    }

    // Destroy surface's snapshots.
    for(ptrdiff_t i = 0; i != rose_surface_snapshot_type_count_; ++i) {
        rose_surface_snapshot_destroy(&(surface->snapshots[i]));
    }

    // Destroy snapshots of all child entities.
    if(true) {
        struct rose_surface* x = NULL;

        wl_list_for_each(x, &(surface->subsurfaces), link) {
            for(ptrdiff_t i = 0; i != rose_surface_snapshot_type_count_; ++i) {
                rose_surface_snapshot_destroy(&(x->snapshots[i]));
            }
        }

        wl_list_for_each(x, &(surface->temporaries), link) {
            for(ptrdiff_t i = 0; i != rose_surface_snapshot_type_count_; ++i) {
                rose_surface_snapshot_destroy(&(x->snapshots[i]));
            }
        }
    }

    // Do nothing else if there is no running transaction.
    if(!(surface->is_transaction_running)) {
        return;
    }

    // Synchronize surface's state.
    rose_surface_state_sync(surface);

    // Update surface's position.
    surface->state.current.x = surface->state.pending.x;
    surface->state.current.y = surface->state.pending.y;

    // Update surface's pending state.
    surface->state.pending = surface->state.current;

    // Stop the transaction.
    surface->is_transaction_running = false;
}
