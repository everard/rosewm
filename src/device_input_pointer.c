// Copyright Nezametdinov E. Ildus 2022.
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
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_pointer_axis(struct wl_listener* listener, void* data) {
    // Terminology is a bit confusing, since Wayland uses the word "pointer" to
    // describe pointing device, such as mouse, touch pad, etc.

    // Obtain a pointer to the device.
    // Note: A C pointer to the pointing device. I'm so terribly sorry.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_axis);

    // Obtain current workspace.
    struct rose_workspace* workspace =
        pointer->parent->context->current_workspace;

    // Obtain event data.
    struct wlr_event_pointer_axis* wlr_event = data;
    struct rose_pointer_event_axis event = //
        {.time_msec = wlr_event->time_msec,
         .delta_discrete = wlr_event->delta_discrete,
         .delta = wlr_event->delta,
         .orientation =
             (enum rose_pointer_axis_orientation)(wlr_event->orientation),
         .source = (enum rose_pointer_axis_source)(wlr_event->source)};

    // Notify current workspace of this event.
    rose_workspace_notify_pointer_axis(workspace, event);
}

static void
rose_handle_event_pointer_button(struct wl_listener* listener, void* data) {
    // Obtain a pointer to the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_button);

    // Obtain current workspace.
    struct rose_workspace* workspace =
        pointer->parent->context->current_workspace;

    // Obtain event data.
    struct wlr_event_pointer_button* wlr_event = data;
    struct rose_pointer_event_button event = //
        {.time_msec = wlr_event->time_msec,
         .button = wlr_event->button,
         .state = ((wlr_event->state == WLR_BUTTON_RELEASED)
                       ? rose_pointer_button_state_released
                       : rose_pointer_button_state_pressed)};

    // Notify current workspace of this event.
    rose_workspace_notify_pointer_button(workspace, event);
}

static void
rose_handle_event_pointer_motion(struct wl_listener* listener, void* data) {
    // Obtain a pointer to the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_motion);

    // Obtain current workspace.
    struct rose_workspace* workspace =
        pointer->parent->context->current_workspace;

    // Obtain event data.
    struct wlr_event_pointer_motion* wlr_event = data;
    struct rose_pointer_event_motion event = //
        {.time_msec = wlr_event->time_msec,
         .dx = wlr_event->delta_x,
         .dy = wlr_event->delta_y,
         .dx_unaccel = wlr_event->unaccel_dx,
         .dy_unaccel = wlr_event->unaccel_dy};

    // Notify current workspace of this event.
    rose_workspace_notify_pointer_move(workspace, event);
}

static void
rose_handle_event_pointer_motion_absolute(struct wl_listener* listener,
                                          void* data) {
    // Obtain a pointer to the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_motion_absolute);

    // Obtain current workspace.
    struct rose_workspace* workspace =
        pointer->parent->context->current_workspace;

    // Obtain event data.
    struct wlr_event_pointer_motion_absolute* wlr_event = data;
    struct rose_pointer_event_motion_absolute event = //
        {.time_msec = wlr_event->time_msec,
         .x = wlr_event->x,
         .y = wlr_event->y};

    // Notify current workspace of this event.
    rose_workspace_notify_pointer_warp(workspace, event);
}

static void
rose_handle_event_pointer_frame(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain a pointer to the device.
    struct rose_pointer* pointer =
        wl_container_of(listener, pointer, listener_frame);

    // Notify the seat of this event.
    wlr_seat_pointer_notify_frame(pointer->parent->context->seat);
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_pointer_initialize(struct rose_pointer* pointer,
                        struct rose_input* parent) {
    // Initialize the pointer device.
    *pointer = (struct rose_pointer){.parent = parent};

    // Register listeners.
#define add_signal_(f)                                                         \
    {                                                                          \
        pointer->listener_##f.notify = rose_handle_event_pointer_##f;          \
        wl_signal_add(                                                         \
            &((parent->device->pointer)->events.f), &(pointer->listener_##f)); \
    }

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
rose_pointer_configure(struct rose_pointer* pointer,
                       struct rose_pointer_configure_parameters params) {
    // If requested configuration is a no-op, then return success.
    if(params.flags == 0) {
        return true;
    }

    // If the pointer device is not a libinput device, then configuration fails.
    if(!wlr_input_device_is_libinput(pointer->parent->device)) {
        return false;
    }

    // Obtain a pointer to the underlying libinput device.
    struct libinput_device* dev_libinput =
        wlr_libinput_get_device_handle(pointer->parent->device);

    // If there is no such device, then configuration fails.
    if(dev_libinput == NULL) {
        return false;
    }

    // If the device does not support acceleration, then configuration fails.
    if(!libinput_device_config_accel_is_available(dev_libinput)) {
        return false;
    }

    // Check the validity of the specified parameters.

    if((params.flags & rose_pointer_configure_acceleration_type) != 0) {
        // Note: There are only flat and adaptive acceleration types.
        if((params.acceleration_type != rose_pointer_acceleration_type_flat) &&
           (params.acceleration_type !=
            rose_pointer_acceleration_type_adaptive)) {
            return false;
        }
    }

    if((params.flags & rose_pointer_configure_speed) != 0) {
        // Note: The speed can not be NaN or infinity.
        if(!isfinite(params.speed)) {
            return false;
        }
    }

    // Set specified parameters.

    if((params.flags & rose_pointer_configure_acceleration_type) != 0) {
        // Note: There are only flat and adaptive acceleration types.
        if(params.acceleration_type == rose_pointer_acceleration_type_flat) {
            libinput_device_config_accel_set_profile(
                dev_libinput, LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
        } else {
            libinput_device_config_accel_set_profile(
                dev_libinput, LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
        }
    }

    if((params.flags & rose_pointer_configure_speed) != 0) {
        // Note: The speed is clamped to the [-1; 1] interval.
        libinput_device_config_accel_set_speed(
            dev_libinput, clamp_(params.speed, -1.0, 1.0));
    }

    // Update device preference list, if needed.
    if(pointer->parent->context->preference_list != NULL) {
        // Initialize device preference.
        struct rose_device_preference preference = {
            .device_name = rose_input_name_obtain(pointer->parent),
            .device_type = rose_device_type_pointer,
            .params = {.pointer = params}};

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
        // Obtain a pointer to the underlying libinput device.
        struct libinput_device* dev_libinput =
            wlr_libinput_get_device_handle(pointer->parent->device);

        // Obtain acceleration data, if available.
        if((dev_libinput != NULL) &&
           libinput_device_config_accel_is_available(dev_libinput)) {
            // Obtain acceleration type.
            switch(libinput_device_config_accel_get_profile(dev_libinput)) {
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
            state.speed = libinput_device_config_accel_get_speed(dev_libinput);
            state.is_acceleration_supported = true;
        }
    }

    return state;
}
