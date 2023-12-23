// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"
#include "ipc_connection.h"

#include <stdlib.h>
#include <string.h>

#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define cast_(type, x) ((type)(x))

////////////////////////////////////////////////////////////////////////////////
// Device descriptor definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_ipc_device_name_size = 64 };

struct rose_ipc_device_descriptor {
    unsigned id;
    char name[rose_ipc_device_name_size];
};

////////////////////////////////////////////////////////////////////////////////
// Configuration protocol definitions.
////////////////////////////////////////////////////////////////////////////////

enum rose_ipc_configure_request_type {
    // Server state query.
    rose_ipc_configure_request_type_obtain_keymap,
    rose_ipc_configure_request_type_obtain_device_count,

    // Device state query.
    rose_ipc_configure_request_type_obtain_input_state,
    rose_ipc_configure_request_type_obtain_output_state,

    // State setting.
    rose_ipc_configure_request_type_set_keyboard_layout,
    rose_ipc_configure_request_type_set_pointer_state,
    rose_ipc_configure_request_type_set_output_state,

    // Server state update.
    rose_ipc_configure_request_type_update_server_state
};

enum rose_ipc_configure_result {
    rose_ipc_configure_result_success,
    rose_ipc_configure_result_failure,
    rose_ipc_configure_result_invalid_request,
    rose_ipc_configure_result_device_not_found
};

////////////////////////////////////////////////////////////////////////////////
// Serialized object size definitions.
////////////////////////////////////////////////////////////////////////////////

enum {
    rose_ipc_serialized_size_device_descriptor =
        sizeof(unsigned) + rose_ipc_device_name_size // id, name
    ,
    rose_ipc_serialized_size_output_mode =
        3 * sizeof(int) // width, height, refresh rate
    ,
    rose_ipc_serialized_size_pointer_configure_parameters =
        sizeof(unsigned) // flags,
        + 1              // acceleration_type,
        + sizeof(float)  // speed
    ,
    rose_ipc_serialized_size_output_configure_parameters =
        sizeof(unsigned)                       // flags,
        + 1                                    // adaptive_sync_state,
        + 1                                    // transform,
        + sizeof(double)                       // scale,
        + rose_ipc_serialized_size_output_mode // mode
};

////////////////////////////////////////////////////////////////////////////////
// Device descriptor acquisition utility functions.
////////////////////////////////////////////////////////////////////////////////

#define define_descriptor_acquisition_(object)                         \
    /* Initialize descriptor object. */                                \
    struct rose_ipc_device_descriptor descriptor = {.id = object->id}; \
                                                                       \
    /* Obtain device's name. */                                        \
    if(object->device->name != NULL) {                                 \
        size_t name_size = strlen(object->device->name);               \
                                                                       \
        memcpy(descriptor.name, object->device->name,                  \
               min_(name_size, sizeof(descriptor.name)));              \
    }                                                                  \
                                                                       \
    /* Return constructed descriptor. */                               \
    return descriptor;

static struct rose_ipc_device_descriptor
rose_input_device_descriptor_obtain(struct rose_input* input) {
    define_descriptor_acquisition_(input);
}

static struct rose_ipc_device_descriptor
rose_output_device_descriptor_obtain(struct rose_output* output) {
    define_descriptor_acquisition_(output);
}

#undef define_descriptor_acquisition_

////////////////////////////////////////////////////////////////////////////////
// IO utility functions: Basic Types, Read.
////////////////////////////////////////////////////////////////////////////////

#define define_read_(type)                                   \
    /* Zero-initialize the resulting value. */               \
    type r = 0;                                              \
                                                             \
    /* Read the value from the buffer. */                    \
    memcpy(&r, buffer->data, min_(sizeof(r), buffer->size)); \
                                                             \
    /* Shrink the buffer. */                                 \
    buffer->data += min_(sizeof(r), buffer->size);           \
    buffer->size -= min_(sizeof(r), buffer->size);           \
                                                             \
    /* Return resulting value. */                            \
    return r;

static unsigned char
rose_ipc_buffer_ref_read_byte(struct rose_ipc_buffer_ref* buffer) {
    define_read_(unsigned char);
}

static unsigned
rose_ipc_buffer_ref_read_uint(struct rose_ipc_buffer_ref* buffer) {
    define_read_(unsigned);
}

static int
rose_ipc_buffer_ref_read_int(struct rose_ipc_buffer_ref* buffer) {
    define_read_(int);
}

static float
rose_ipc_buffer_ref_read_float(struct rose_ipc_buffer_ref* buffer) {
    define_read_(float);
}

static double
rose_ipc_buffer_ref_read_double(struct rose_ipc_buffer_ref* buffer) {
    define_read_(double);
}

#undef define_read_

////////////////////////////////////////////////////////////////////////////////
// IO utility functions: Basic Types, Write.
////////////////////////////////////////////////////////////////////////////////

#define define_write_                                                        \
    /* Compute the amount of space left in the buffer. */                    \
    size_t buffer_size_left = rose_ipc_buffer_size_max - buffer->size;       \
                                                                             \
    /* Write the value to the buffer. */                                     \
    memcpy(                                                                  \
        buffer->data + buffer->size, &x, min_(buffer_size_left, sizeof(x))); \
                                                                             \
    /* Increase buffer's size by the amount of bytes written. */             \
    buffer->size += min_(buffer_size_left, sizeof(x));

static void
rose_ipc_buffer_write_byte(struct rose_ipc_buffer* buffer, unsigned char x) {
    define_write_;
}

static void
rose_ipc_buffer_write_uint(struct rose_ipc_buffer* buffer, unsigned x) {
    define_write_;
}

static void
rose_ipc_buffer_write_int(struct rose_ipc_buffer* buffer, int x) {
    define_write_;
}

static void
rose_ipc_buffer_write_float(struct rose_ipc_buffer* buffer, float x) {
    define_write_;
}

static void
rose_ipc_buffer_write_double(struct rose_ipc_buffer* buffer, double x) {
    define_write_;
}

#undef define_write_

////////////////////////////////////////////////////////////////////////////////
// IO utility functions: Aggregate Types, Read.
////////////////////////////////////////////////////////////////////////////////

static void
rose_ipc_buffer_ref_read_string(struct rose_ipc_buffer_ref* buffer,
                                char* string, size_t string_size) {
    // Read the string.
    memcpy(string, buffer->data, min_(string_size, buffer->size));

    // Shrink the buffer.
    buffer->data += min_(string_size, buffer->size);
    buffer->size -= min_(string_size, buffer->size);
}

static struct rose_ipc_device_descriptor
rose_ipc_buffer_ref_read_device_descriptor(struct rose_ipc_buffer_ref* buffer) {
    // Initialize descriptor object, read device's ID.
    struct rose_ipc_device_descriptor descriptor = {
        .id = rose_ipc_buffer_ref_read_uint(buffer)};

    // Read device's name.
    rose_ipc_buffer_ref_read_string(
        buffer, descriptor.name, sizeof(descriptor.name));

    // Return resulting descriptor.
    return descriptor;
}

////////////////////////////////////////////////////////////////////////////////
// IO utility functions: Aggregate Types, Write.
////////////////////////////////////////////////////////////////////////////////

static void
rose_ipc_buffer_write_string(struct rose_ipc_buffer* buffer, char* string,
                             size_t string_size) {
    // Compute the amount of space left in the buffer.
    size_t buffer_size_left = rose_ipc_buffer_size_max - buffer->size;

    // Write the string to the buffer.
    memcpy(buffer->data + buffer->size, string,
           min_(buffer_size_left, string_size));

    // Increase buffer's size by the amount of bytes written.
    buffer->size += min_(buffer_size_left, string_size);
}

static void
rose_ipc_buffer_write_device_descriptor(
    struct rose_ipc_buffer* buffer,
    struct rose_ipc_device_descriptor descriptor) {
    // Write device's ID.
    rose_ipc_buffer_write_uint(buffer, descriptor.id);

    // Write device's name.
    rose_ipc_buffer_write_string(
        buffer, descriptor.name, sizeof(descriptor.name));
}

////////////////////////////////////////////////////////////////////////////////
// Server configuration interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_connection_dispatch_configuration_request(
    struct rose_ipc_connection* connection,
    struct rose_ipc_buffer_ref request) {
    static size_t const payload_sizes[] = {
        // rose_ipc_configure_request_type_obtain_keymap
        0,
        // rose_ipc_configure_request_type_obtain_device_count
        0,
        // rose_ipc_configure_request_type_obtain_input_state
        sizeof(unsigned),
        // rose_ipc_configure_request_type_obtain_output_state
        sizeof(unsigned),
        // rose_ipc_configure_request_type_set_keyboard_layout
        1,
        // rose_ipc_configure_request_type_set_pointer_state
        rose_ipc_serialized_size_device_descriptor +
            rose_ipc_serialized_size_pointer_configure_parameters,
        // rose_ipc_configure_request_type_set_output_state
        rose_ipc_serialized_size_device_descriptor +
            rose_ipc_serialized_size_output_configure_parameters,
        // rose_ipc_configure_request_type_update_server_state
        1};

    // Obtain a pointer to the server context.
    struct rose_server_context* context = connection->context;

    // Initialize response.
    struct rose_ipc_buffer response = {};

    // Respond with failure if the given request is not valid.
    if(request.size == 0) {
        rose_ipc_buffer_write_byte(
            &response, rose_ipc_configure_result_invalid_request);

        goto end;
    }

    // Obtain request's type.
    enum rose_ipc_configure_request_type request_type =
        rose_ipc_buffer_ref_read_byte(&request);

#define array_size_(x) (sizeof(x) / sizeof((x)[0]))

    // Respond with failure if request's type is not valid.
    if((request_type < 0) || (request_type >= array_size_(payload_sizes))) {
        rose_ipc_buffer_write_byte(
            &response, rose_ipc_configure_result_invalid_request);

        goto end;
    }

#undef array_size_

    // Respond with failure if request's payload has invalid size.
    if(request.size != payload_sizes[request_type]) {
        rose_ipc_buffer_write_byte(
            &response, rose_ipc_configure_result_invalid_request);

        goto end;
    }

    // Process the request depending on its type.
    switch(request_type) {
        case rose_ipc_configure_request_type_obtain_keymap:
            // Write operation's result.
            rose_ipc_buffer_write_byte(
                &response, rose_ipc_configure_result_success);

            // Write the number of keyboard layouts in the keymap.
            rose_ipc_buffer_write_uint(
                &response, context->keyboard_context->n_layouts);

            // Write keyboard layouts.
            rose_ipc_buffer_write_string( //
                &response, context->config.keyboard_layouts.data,
                context->config.keyboard_layouts.size);

            break;

        case rose_ipc_configure_request_type_obtain_device_count: {
            // Obtain server context's state.
            struct rose_server_context_state state =
                rose_server_context_state_obtain(context);

            // Write operation's result.
            rose_ipc_buffer_write_byte(
                &response, rose_ipc_configure_result_success);

            // Write the number of input and output devices.
            rose_ipc_buffer_write_uint(&response, state.n_inputs);
            rose_ipc_buffer_write_uint(&response, state.n_outputs);

            break;
        }

        case rose_ipc_configure_request_type_obtain_input_state: {
            // Obtain an input with the requested ID.
            struct rose_input* input = rose_server_context_obtain_input(
                context, rose_ipc_buffer_ref_read_uint(&request));

            // Respond with failure if there is no such input.
            if(input == NULL) {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_device_not_found);

                break;
            }

            // Obtain input's descriptor.
            struct rose_ipc_device_descriptor descriptor =
                rose_input_device_descriptor_obtain(input);

            // Write operation's result.
            rose_ipc_buffer_write_byte(
                &response, rose_ipc_configure_result_success);

            // Write input's type.
            rose_ipc_buffer_write_byte(&response, input->type);

            // Write input's descriptor.
            rose_ipc_buffer_write_device_descriptor(&response, descriptor);

            // Write input's state which depends on input's type.
            switch(input->type) {
                case rose_input_device_type_pointer: {
                    // Obtain pointer's state.
                    struct rose_pointer_state state =
                        rose_pointer_state_obtain(&(input->pointer));

                    // Write pointer's acceleration type.
                    rose_ipc_buffer_write_byte(
                        &response, state.acceleration_type);

                    // Write pointer's speed.
                    rose_ipc_buffer_write_float(&response, state.speed);

                    // Write pointer's acceleration support flag.
                    rose_ipc_buffer_write_byte(
                        &response, state.is_acceleration_supported);

                    break;
                }

                default:
                    break;
            }

            break;
        }

        case rose_ipc_configure_request_type_obtain_output_state: {
            // Obtain an output with the requested ID.
            struct rose_output* output = rose_server_context_obtain_output(
                context, rose_ipc_buffer_ref_read_uint(&request));

            // Respond with failure if there is no such output.
            if(output == NULL) {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_device_not_found);

                break;
            }

            // Obtain output's descriptor.
            struct rose_ipc_device_descriptor descriptor =
                rose_output_device_descriptor_obtain(output);

            // Obtain output's state.
            struct rose_output_state state = rose_output_state_obtain(output);

            // Obtain the list of output's modes.
            struct rose_output_mode_list modes =
                rose_output_mode_list_obtain(output);

            // Write operation's result.
            rose_ipc_buffer_write_byte(
                &response, rose_ipc_configure_result_success);

            // Write output's descriptor.
            rose_ipc_buffer_write_device_descriptor(&response, descriptor);

            // Write output's adaptive sync state.
            rose_ipc_buffer_write_byte(&response, state.adaptive_sync_state);

            // Write output's transform.
            rose_ipc_buffer_write_byte(&response, state.transform);

            // Write output's DPI and refresh rate.
            rose_ipc_buffer_write_int(&response, state.dpi);
            rose_ipc_buffer_write_int(&response, state.rate);

            // Write output's geometry.
            rose_ipc_buffer_write_int(&response, state.w);
            rose_ipc_buffer_write_int(&response, state.h);

            // Write output's scaling factor.
            rose_ipc_buffer_write_double(&response, state.scale);

            // Write the list of output's modes.
            rose_ipc_buffer_write_uint(&response, cast_(unsigned, modes.size));
            for(size_t i = 0; i != modes.size; ++i) {
                rose_ipc_buffer_write_int(&response, modes.data[i].w);
                rose_ipc_buffer_write_int(&response, modes.data[i].h);
                rose_ipc_buffer_write_int(&response, modes.data[i].rate);
            }

            break;
        }

        case rose_ipc_configure_request_type_set_keyboard_layout: {
            // Set requested layout and write operation's result.
            if(rose_server_context_set_keyboard_layout(
                   context, rose_ipc_buffer_ref_read_byte(&request))) {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_success);
            } else {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_failure);
            }

            break;
        }

        case rose_ipc_configure_request_type_set_pointer_state: {
            // Read target device's descriptor.
            struct rose_ipc_device_descriptor descriptor =
                rose_ipc_buffer_ref_read_device_descriptor(&request);

            // Read pointer's configuration parameters.
            struct rose_pointer_configure_parameters params = {
                .flags = rose_ipc_buffer_ref_read_uint(&request),
                .acceleration_type = rose_ipc_buffer_ref_read_byte(&request),
                .speed = rose_ipc_buffer_ref_read_float(&request)};

            // Obtain an input device with the requested descriptor.
            struct rose_input* input =
                rose_server_context_obtain_input(context, descriptor.id);

            // Respond with failure if there is no such device.
            if((input == NULL) ||
               (input->type != rose_input_device_type_pointer) ||
               (memcmp(rose_input_device_descriptor_obtain(input).name,
                       descriptor.name, sizeof(descriptor.name)) != 0)) {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_device_not_found);

                break;
            }

            // Configure the pointer and write operation's result.
            if(rose_pointer_configure(&(input->pointer), params)) {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_success);
            } else {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_failure);
            }

            break;
        }

        case rose_ipc_configure_request_type_set_output_state: {
            // Read target device's descriptor.
            struct rose_ipc_device_descriptor descriptor =
                rose_ipc_buffer_ref_read_device_descriptor(&request);

            // Read output's configuration parameters.
            struct rose_output_configure_parameters params = {
                .flags = rose_ipc_buffer_ref_read_uint(&request),
                .adaptive_sync_state = rose_ipc_buffer_ref_read_byte(&request),
                .transform = rose_ipc_buffer_ref_read_byte(&request),
                .scale = rose_ipc_buffer_ref_read_double(&request),
                .mode = {.w = rose_ipc_buffer_ref_read_int(&request),
                         .h = rose_ipc_buffer_ref_read_int(&request),
                         .rate = rose_ipc_buffer_ref_read_int(&request)}};

            // Obtain an output device with the requested descriptor.
            struct rose_output* output =
                rose_server_context_obtain_output(context, descriptor.id);

            // Respond with failure if there is no such device.
            if((output == NULL) ||
               (memcmp(rose_output_device_descriptor_obtain(output).name,
                       descriptor.name, sizeof(descriptor.name)) != 0)) {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_device_not_found);

                break;
            }

            // Configure the output and write operation's result.
            if(rose_output_configure(output, params)) {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_success);
            } else {
                rose_ipc_buffer_write_byte(
                    &response, rose_ipc_configure_result_failure);
            }

            break;
        }

        case rose_ipc_configure_request_type_update_server_state:
            // Write operation's result.
            rose_ipc_buffer_write_byte(
                &response, rose_ipc_configure_result_success);

            // Configure the context.
            rose_server_context_configure(
                context, rose_ipc_buffer_ref_read_byte(&request));

            break;

        default:
            rose_ipc_buffer_write_byte(
                &response, rose_ipc_configure_result_invalid_request);

            break;
    }

end:
    // Send the response.
    struct rose_ipc_buffer_ref buffer = {
        .data = response.data, .size = response.size};

    rose_ipc_tx(&(connection->io_context), buffer);
}
