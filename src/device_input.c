// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_input_device.h>

#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

////////////////////////////////////////////////////////////////////////////////
// Utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_update_seat_capabilities(struct wlr_seat* seat) {
    // In this compositor a seat shall always have pointer and keyboard
    // capability, regardless of existence of such devices.
    wlr_seat_set_capabilities(
        seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_input_destroy(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain a pointer to the input device.
    struct rose_input* input =
        wl_container_of(listener, input, listener_destroy);

    // Destroy the device.
    rose_input_destroy(input);
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_input_initialize(struct rose_server_context* ctx,
                      struct wlr_input_device* dev) {
    // Allocate and initialize a new input.
    struct rose_input* input = malloc(sizeof(struct rose_input));

    if(input == NULL) {
        return;
    } else {
        *input = (struct rose_input){.ctx = ctx, .dev = dev};
    }

    // Add it to the list.
    wl_list_insert(&(ctx->inputs), &(input->link));

    // Set input's ID.
    if(input->link.next != &(ctx->inputs)) {
        input->id = (wl_container_of(input->link.next, input, link))->id + 1;
    }

    // Broadcast input's initialization event though IPC, if needed.
    if(input->ctx->ipc_server != NULL) {
        rose_ipc_server_broadcast_status(
            input->ctx->ipc_server,
            (struct rose_ipc_status){
                .type = rose_ipc_status_type_input_initialized,
                .device_id = input->id});
    }

    // Register listeners.
    input->listener_destroy.notify = rose_handle_event_input_destroy;
    wl_signal_add(&(dev->events.destroy), &(input->listener_destroy));

    // Perform device-specific initialization.
    switch(dev->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            input->type = (rose_keyboard_initialize(&(input->keyboard), input),
                           rose_input_device_type_keyboard);
            break;

        case WLR_INPUT_DEVICE_POINTER:
            input->type = (rose_pointer_initialize(&(input->pointer), input),
                           rose_input_device_type_pointer);
            break;

        default:
            break;
    }

    // Update seat's capabilities.
    rose_update_seat_capabilities(input->ctx->seat);
}

void
rose_input_destroy(struct rose_input* input) {
    // Broadcast input's destruction event though IPC, if needed.
    if(input->ctx->ipc_server != NULL) {
        rose_ipc_server_broadcast_status(
            input->ctx->ipc_server,
            (struct rose_ipc_status){
                .type = rose_ipc_status_type_input_destroyed,
                .device_id = input->id});
    }

    // Update IDs of all inputs which precede this one.
    for(struct rose_input* x = input; x->link.prev != &(input->ctx->inputs);) {
        (x = wl_container_of(x->link.prev, x, link))->id--;
    }

    // Remove listeners from signals.
    wl_list_remove(&(input->listener_destroy.link));

    // Remove this input from the list.
    wl_list_remove(&(input->link));

    // Perform device-specific destruction.
    switch(input->dev->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            rose_keyboard_destroy(&(input->keyboard));
            break;

        case WLR_INPUT_DEVICE_POINTER:
            rose_pointer_destroy(&(input->pointer));
            break;

        default:
            break;
    }

    // Update seat's capabilities.
    rose_update_seat_capabilities(input->ctx->seat);

    // Free memory.
    free(input);
}