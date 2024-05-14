// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_seat.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>

#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

////////////////////////////////////////////////////////////////////////////////
// Drag and drop action definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_drag_and_drop_action {
    // Parent server context.
    struct rose_server_context* context;

    // Event listener.
    struct wl_listener listener_destroy;
};

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
////////////////////////////////////////////////////////////////////////////////

void
rose_handle_event_drag_and_drop_action_destroy(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the action.
    struct rose_drag_and_drop_action* action =
        wl_container_of(listener, action, listener_destroy);

    // Remove listeners from signals.
    wl_list_remove(&(action->listener_destroy.link));

    // Reset current output cursor's drag and drop surface.
    struct rose_output* output = action->context->current_workspace->output;
    if(output != NULL) {
        rose_output_cursor_drag_and_drop_surface_set(output, NULL);
    }

    // Free memory.
    free(action);
}

////////////////////////////////////////////////////////////////////////////////
// Action interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_drag_and_drop_start(
    struct rose_server_context* context, struct wlr_drag* drag) {
    // Allocate and initialize a new drag and drop action object.
    struct rose_drag_and_drop_action* action =
        malloc(sizeof(struct rose_drag_and_drop_action));

    if(action != NULL) {
        *action = (struct rose_drag_and_drop_action){.context = context};
    } else {
        return wlr_data_source_destroy(drag->source);
    }

    // Register listener.
    wl_signal_add(&(drag->events.destroy), &(action->listener_destroy));

    action->listener_destroy.notify =
        rose_handle_event_drag_and_drop_action_destroy;

    // Manage current output cursor's drag and drop surface.
    struct rose_output* output = context->current_workspace->output;
    if((output != NULL) && (drag->icon != NULL)) {
        rose_output_cursor_drag_and_drop_surface_set(
            output, drag->icon->surface);
    }
}
