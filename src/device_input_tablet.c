// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

////////////////////////////////////////////////////////////////////////////////
// Tablet tool definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_tablet_tool {
    // Parent server context.
    struct rose_server_context* context;

    // Protocol object.
    struct wlr_tablet_v2_tablet_tool* handle;

    // Event listeners.
    struct wl_listener listener_set_cursor;
    struct wl_listener listener_destroy;

    // Accumulated event data.
    double x, y, dx, dy, tilt_x, tilt_y;

    // List link.
    struct wl_list link;
};

////////////////////////////////////////////////////////////////////////////////
// Tablet tool event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_tablet_tool_set_cursor(
    struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_tablet_tool* tool =
        wl_container_of(listener, tool, listener_set_cursor);

    // Obtain the event.
    struct wlr_tablet_v2_event_cursor* event = data;

    // Obtain the current output.
    struct rose_output* output = tool->context->current_workspace->output;

    // If such output exists, then set its cursor.
    if(output != NULL) {
        rose_output_cursor_client_surface_set(
            output, event->surface, event->hotspot_x, event->hotspot_y);

        rose_output_cursor_set(output, rose_output_cursor_type_unspecified);
        rose_output_cursor_set(output, rose_output_cursor_type_client);
    }
}

static void
rose_handle_event_tablet_tool_destroy(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the device.
    struct rose_tablet_tool* tool =
        wl_container_of(listener, tool, listener_destroy);

    // Remove it from the list.
    wl_list_remove(&(tool->link));

    // Remove listeners from signals.
    wl_list_remove(&(tool->listener_set_cursor.link));
    wl_list_remove(&(tool->listener_destroy.link));

    // Free memory.
    free(tool);
}

////////////////////////////////////////////////////////////////////////////////
// Tablet tool acquisition utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_tablet_tool*
rose_tablet_tool_obtain(
    struct rose_tablet* tablet, struct wlr_tablet_tool* underlying) {
    // If the tablet is not properly initialized, then it can't have any
    // associated tools.
    if(tablet->handle == NULL) {
        return NULL;
    }

    // Obtain the server context.
    struct rose_server_context* context = tablet->parent->context;

    // Obtain the tablet tool.
    struct rose_tablet_tool* tool = underlying->data;

    if(tool != NULL) {
        // If such tool exists, then add it to the list of tablet's tools.
        wl_list_remove(&(tool->link));
        wl_list_insert(&(tablet->tools), &(tool->link));
    } else {
        // Otherwise, allocate and initialize a new tablet tool.
        tool = malloc(sizeof(struct rose_tablet_tool));

        if(tool == NULL) {
            goto error;
        } else {
            *tool = (struct rose_tablet_tool){.context = context};
        }

        // Create protocol object.
        tool->handle = wlr_tablet_tool_create(
            context->tablet_manager, context->seat, underlying);

        if(tool->handle == NULL) {
            goto error;
        } else {
            underlying->data = tool;
        }

        // Add the tool to the list.
        wl_list_insert(&(tablet->tools), &(tool->link));

#define add_signal_(x, f)                                              \
    {                                                                  \
        tool->listener_##f.notify = rose_handle_event_tablet_tool_##f; \
        wl_signal_add(&((x)->events.f), &(tool->listener_##f));        \
    }

        // Register listeners.
        add_signal_(tool->handle, set_cursor);
        add_signal_(underlying, destroy);

#undef add_signal_
    }

    // Return the tool.
    return tool;

error:
    // On error, free memory.
    return free(tool), NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Tablet event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_tablet_axis(struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_tablet* tablet =
        wl_container_of(listener, tablet, listener_axis);

    // Obtain the event.
    struct wlr_tablet_tool_axis_event* event = data;

    // Obtain associated tool.
    struct rose_tablet_tool* tool =
        rose_tablet_tool_obtain(tablet, event->tool);

    // Handle the event, if needed.
    if(tool != NULL) {
        // Accumulate motion data.
        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_X) != 0) {
            tool->x = event->x;
            tool->dx = event->dx;
        }

        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_Y) != 0) {
            tool->y = event->y;
            tool->dy = event->dy;
        }

        // Accumulate tilt data.
        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_X) != 0) {
            tool->tilt_x = event->tilt_x;
        }

        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_Y) != 0) {
            tool->tilt_y = event->tilt_y;
        }

        // Send motion event, if needed.
        if((event->updated_axes &
            (WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y)) != 0) {
            // Construct motion event.
            struct rose_tablet_tool_event_motion motion_event = {
                .tablet = tablet->handle,
                .tool = tool->handle,
                .time = event->time_msec,
                .x = tool->x,
                .y = tool->y,
                .dx = tool->dx,
                .dy = tool->dy};

            // And notify current workspace of this event.
            rose_workspace_notify_tablet_tool_warp(
                tool->context->current_workspace, motion_event);
        }

        // Send tilt event, if needed.
        if((event->updated_axes &
            (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) != 0) {
            wlr_send_tablet_v2_tablet_tool_tilt(
                tool->handle, tool->tilt_x, tool->tilt_y);
        }

        // Send distance event, if needed.
        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE) != 0) {
            wlr_send_tablet_v2_tablet_tool_distance(
                tool->handle, event->distance);
        }

        // Send pressure event, if needed.
        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE) != 0) {
            wlr_send_tablet_v2_tablet_tool_pressure(
                tool->handle, event->pressure);
        }

        // Send rotation event, if needed.
        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION) != 0) {
            wlr_send_tablet_v2_tablet_tool_rotation(
                tool->handle, event->rotation);
        }

        // Send slider event, if needed.
        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER) != 0) {
            wlr_send_tablet_v2_tablet_tool_slider(tool->handle, event->slider);
        }

        // Send wheel event, if needed.
        if((event->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL) != 0) {
            wlr_send_tablet_v2_tablet_tool_wheel(
                tool->handle, event->wheel_delta, 0);
        }
    }
}

static void
rose_handle_event_tablet_proximity(struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_tablet* tablet =
        wl_container_of(listener, tablet, listener_proximity);

    // Obtain the event.
    struct wlr_tablet_tool_proximity_event* event = data;

    // Obtain associated tool.
    struct rose_tablet_tool* tool =
        rose_tablet_tool_obtain(tablet, event->tool);

    // Handle the event, if needed.
    if(tool != NULL) {
        if(event->state == WLR_TABLET_TOOL_PROXIMITY_OUT) {
            // If the tool left tablet's proximity, then send appropriate event.
            wlr_send_tablet_v2_tablet_tool_proximity_out(tool->handle);
        } else {
            // Otherwise, obtain motion data.
            tool->x = event->x;
            tool->y = event->y;

            // Construct motion event.
            struct rose_tablet_tool_event_motion motion_event = {
                .tablet = tablet->handle,
                .tool = tool->handle,
                .time = event->time_msec,
                .x = tool->x,
                .y = tool->y};

            // And notify current workspace of this event.
            rose_workspace_notify_tablet_tool_warp(
                tool->context->current_workspace, motion_event);
        }
    }
}

static void
rose_handle_event_tablet_button(struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_tablet* tablet =
        wl_container_of(listener, tablet, listener_button);

    // Obtain the event.
    struct wlr_tablet_tool_button_event* event = data;

    // Obtain associated tool.
    struct rose_tablet_tool* tool =
        rose_tablet_tool_obtain(tablet, event->tool);

    // Send button event.
    if(tool != NULL) {
        wlr_send_tablet_v2_tablet_tool_button(
            tool->handle, event->button, (int)(event->state));
    }
}

static void
rose_handle_event_tablet_tip(struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_tablet* tablet =
        wl_container_of(listener, tablet, listener_tip);

    // Obtain the event.
    struct wlr_tablet_tool_tip_event* event = data;

    // Obtain associated tool.
    struct rose_tablet_tool* tool =
        rose_tablet_tool_obtain(tablet, event->tool);

    // Send corresponding event, if needed.
    if(tool != NULL) {
        if(event->state == WLR_TABLET_TOOL_TIP_UP) {
            wlr_send_tablet_v2_tablet_tool_up(tool->handle);
        } else {
            wlr_send_tablet_v2_tablet_tool_down(tool->handle);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_tablet_initialize(struct rose_tablet* tablet, struct rose_input* parent) {
    // Obtain the server context.
    struct rose_server_context* context = parent->context;

    // Obtain the underlying device.
    struct wlr_tablet* device = wlr_tablet_from_input_device(parent->device);

    // Initialize the tablet object.
    *tablet = (struct rose_tablet){.parent = parent};

    // Add it to the list.
    wl_list_insert(&(context->inputs_tablets), &(tablet->link));

    // Initialize the list of associated tools.
    wl_list_init(&(tablet->tools));

#define add_signal_(f)                                                 \
    {                                                                  \
        tablet->listener_##f.notify = rose_handle_event_tablet_##f;    \
        wl_signal_add(&((device)->events.f), &(tablet->listener_##f)); \
    }

    // Register listeners.
    add_signal_(axis);
    add_signal_(proximity);
    add_signal_(button);
    add_signal_(tip);

#undef add_signal_

    // Create protocol object.
    tablet->handle = wlr_tablet_create(
        context->tablet_manager, context->seat, parent->device);
}

void
rose_tablet_destroy(struct rose_tablet* tablet) {
    // Remove device from the list.
    wl_list_remove(&(tablet->link));

    // Clear the list of associated tools.
    if(true) {
        struct rose_tablet_tool* tool = NULL;
        struct rose_tablet_tool* _ = NULL;

        wl_list_for_each_safe(tool, _, &(tablet->tools), link) {
            wl_list_remove(&(tool->link));
            wl_list_init(&(tool->link));
        }
    }

    // Remove listeners from signals.
    wl_list_remove(&(tablet->listener_axis.link));
    wl_list_remove(&(tablet->listener_proximity.link));
    wl_list_remove(&(tablet->listener_button.link));
    wl_list_remove(&(tablet->listener_tip.link));
}

////////////////////////////////////////////////////////////////////////////////
// Input focusing interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_tablet_clear_focus(struct rose_tablet* tablet) {
    struct rose_tablet_tool* tool = NULL;
    struct rose_tablet_tool* _ = NULL;

    // Send proximity events.
    wl_list_for_each_safe(tool, _, &(tablet->tools), link) {
        wlr_send_tablet_v2_tablet_tool_proximity_out(tool->handle);
    }
}
