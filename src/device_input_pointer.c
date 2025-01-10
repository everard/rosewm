// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_pointer.h>

#include <math.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
//
// Note: Terminology is a bit confusing, since Wayland uses the word "pointer"
// to describe pointing device, such as mouse, touch pad, etc.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_pointer_axis(struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_axis);

    // Obtain the current workspace.
    struct rose_workspace* workspace =
        pointer->parent->context->current_workspace;

    // Obtain the event.
    struct wlr_pointer_axis_event* event = data;

    // Notify the workspace of this event.
    rose_workspace_notify_pointer_axis(workspace, *event);
}

static void
rose_handle_event_pointer_button(struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_button);

    // Obtain the current workspace.
    struct rose_workspace* workspace =
        pointer->parent->context->current_workspace;

    // Obtain the event.
    struct wlr_pointer_button_event* event = data;

    // Notify the workspace of this event.
    rose_workspace_notify_pointer_button(workspace, *event);
}

static void
rose_handle_event_pointer_motion(struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_motion);

    // Obtain the current workspace.
    struct rose_workspace* workspace =
        pointer->parent->context->current_workspace;

    // Obtain the event.
    struct wlr_pointer_motion_event* event = data;

    // Notify the workspace of this event.
    rose_workspace_notify_pointer_move(workspace, *event);
}

static void
rose_handle_event_pointer_motion_absolute(
    struct wl_listener* listener, void* data) {
    // Obtain the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_motion_absolute);

    // Obtain the current workspace.
    struct rose_workspace* workspace =
        pointer->parent->context->current_workspace;

    // Obtain the event.
    struct wlr_pointer_motion_absolute_event* event = data;

    // Notify the workspace of this event.
    rose_workspace_notify_pointer_warp(workspace, *event);
}

static void
rose_handle_event_pointer_frame(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_frame);

    // Notify the seat of this event.
    wlr_seat_pointer_notify_frame(pointer->parent->context->seat);
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_pointer_initialize(
    struct rose_pointer* pointer, struct rose_input* parent) {
    // Initialize the pointer device.
    *pointer = (struct rose_pointer){.parent = parent};

    // Obtain the underlying input device.
    struct wlr_pointer* device =
        wlr_pointer_from_input_device(pointer->parent->device);

#define add_signal_(f)                                                \
    {                                                                 \
        pointer->listener_##f.notify = rose_handle_event_pointer_##f; \
        wl_signal_add(&(device->events.f), &(pointer->listener_##f)); \
    }

    // Register listeners.
    add_signal_(axis);
    add_signal_(button);
    add_signal_(motion);
    add_signal_(motion_absolute);
    add_signal_(frame);

#undef add_signal_

    // Apply preferences.
    rose_pointer_apply_preferences(pointer, parent->context->preference_list);
}

void
rose_pointer_destroy(struct rose_pointer* pointer) {
    // Remove listeners from signals.
    wl_list_remove(&(pointer->listener_axis.link));
    wl_list_remove(&(pointer->listener_button.link));
    wl_list_remove(&(pointer->listener_motion.link));
    wl_list_remove(&(pointer->listener_motion_absolute.link));
    wl_list_remove(&(pointer->listener_frame.link));
}

////////////////////////////////////////////////////////////////////////////////
// Configuration interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_pointer_configure(
    struct rose_pointer* pointer,
    struct rose_pointer_configuration_parameters parameters) {
    // If requested configuration is a no-op, then return success.
    if(parameters.flags == 0) {
        return true;
    }

    // If the pointer device is not a libinput device, then configuration fails.
    if(!wlr_input_device_is_libinput(pointer->parent->device)) {
        return false;
    }

    // Obtain the underlying libinput device.
    struct libinput_device* device =
        wlr_libinput_get_device_handle(pointer->parent->device);

    // If there is no such device, then configuration fails.
    if(device == NULL) {
        return false;
    }

    // If the device does not support acceleration, then configuration fails.
    if(!libinput_device_config_accel_is_available(device)) {
        return false;
    }

    // Check the validity of the specified parameters.

    if((parameters.flags & rose_pointer_configure_acceleration_type) != 0) {
        if((parameters.acceleration_type !=
            rose_pointer_acceleration_type_flat) &&
           (parameters.acceleration_type !=
            rose_pointer_acceleration_type_adaptive)) {
            return false;
        }
    }

    if((parameters.flags & rose_pointer_configure_speed) != 0) {
        if(!isfinite(parameters.speed)) {
            return false;
        }
    }

    // Set specified parameters.

    if((parameters.flags & rose_pointer_configure_acceleration_type) != 0) {
        if(parameters.acceleration_type ==
           rose_pointer_acceleration_type_flat) {
            libinput_device_config_accel_set_profile(
                device, LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
        } else {
            libinput_device_config_accel_set_profile(
                device, LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
        }
    }

    if((parameters.flags & rose_pointer_configure_speed) != 0) {
        libinput_device_config_accel_set_speed(
            device, clamp_(parameters.speed, -1.0, 1.0));
    }

    // Update device preference list, if needed.
    if(pointer->parent->context->preference_list != NULL) {
        // Initialize device preference.
        struct rose_device_preference preference = {
            .device_name = rose_input_name_obtain(pointer->parent),
            .device_type = rose_device_type_pointer,
            .parameters = {.pointer = parameters}};

        // Perform update operation.
        rose_device_preference_list_update(
            pointer->parent->context->preference_list, preference);
    }

    // Configuration succeeded.
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// State query interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_pointer_state
rose_pointer_state_obtain(struct rose_pointer* pointer) {
    // Initialize a state object.
    struct rose_pointer_state state = {.id = pointer->parent->id};

    // Obtain underlying device's state.
    if(wlr_input_device_is_libinput(pointer->parent->device)) {
        // Obtain the underlying libinput device.
        struct libinput_device* device =
            wlr_libinput_get_device_handle(pointer->parent->device);

        // Obtain acceleration data, if available.
        if((device != NULL) &&
           libinput_device_config_accel_is_available(device)) {
            // Obtain acceleration type.
            switch(libinput_device_config_accel_get_profile(device)) {
                case LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT:
                    state.acceleration_type =
                        rose_pointer_acceleration_type_flat;

                    break;

                case LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE:
                    state.acceleration_type =
                        rose_pointer_acceleration_type_adaptive;

                    break;

                default:
                    break;
            }

            // Obtain the speed.
            state.speed = libinput_device_config_accel_get_speed(device);

            // And set the flag.
            state.is_acceleration_supported = true;
        }
    }

    return state;
}
