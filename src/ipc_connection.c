// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#define _GNU_SOURCE
#include "server_context.h"
#include "ipc_connection.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>

#ifdef HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif

#include <unistd.h>
#include <fcntl.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Type transitioning utility function.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_ipc_connection_transition(struct rose_ipc_connection* connection,
                               enum rose_ipc_connection_type type) {
    // Remove watchdog timer, if needed.
    if(connection->watchdog_timer != NULL) {
        connection->watchdog_timer =
            (wl_event_source_remove(connection->watchdog_timer), NULL);
    }

    // Set connection's type.
    connection->type = type;

    // Initialize connection's state.
    switch(connection->type) {
        case rose_ipc_connection_type_configurator:
            break;

        case rose_ipc_connection_type_dispatcher:
            connection->dispatcher.queue.size = 0;
            break;

        case rose_ipc_connection_type_status:
            connection->status.buffer.size = 0;
            connection->status.server_state_offset = -1;
            break;

        case rose_ipc_connection_type_none:
            // fall-through
        default:
            return false;
    }

    // Check connection's access rights.
    if(true) {
        pid_t pid = 0;

        // Obtain credentials of the process which opened the socket.
#ifdef __FreeBSD__
#ifdef HAVE_XUCRED_CR_PID
#if HAVE_XUCRED_CR_PID
        if(true) {
            struct xucred ucred;
            socklen_t ucred_size = sizeof(ucred);

            if((getsockopt(connection->io_context.socket_fd, SOL_LOCAL,
                           LOCAL_PEERCRED, &ucred, &ucred_size) < 0) ||
               (ucred.cr_version != XUCRED_VERSION)) {
                return false;
            } else {
                pid = ucred.cr_pid;
            }
        }
#endif
#endif
#elif defined(SO_PEERCRED)
        if(true) {
            struct ucred ucred;
            socklen_t ucred_size = sizeof(ucred);

            if(getsockopt(connection->io_context.socket_fd, SOL_SOCKET,
                          SO_PEERCRED, &ucred, &ucred_size) < 0) {
                return false;
            } else {
                pid = ucred.pid;
            }
        }
#else
#error "IPC connection: no way to obtain peer's PID"
#endif

        // Check access rights of the process.
        if(!rose_server_context_check_ipc_access_rights(
               connection->context, pid, connection->type)) {
            return false;
        }
    }

    // Move connection to the appropriate list.
    wl_list_remove(&(connection->link));
    wl_list_insert( //
        &(connection->container->connections[connection->type]),
        &(connection->link));

    // Type switch succeeded.
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Dispatcher-related utility functions.
////////////////////////////////////////////////////////////////////////////////

enum { rose_n_ipc_command_arguments_max = 255 };

static void
rose_ipc_connection_execute_command(struct rose_ipc_connection* connection,
                                    struct rose_ipc_buffer_ref command_buffer) {
    // Validate the given buffer.
    if(command_buffer.size == 0) {
        goto end;
    }

    // Read command's access rights.
    rose_command_access_rights_mask rights = command_buffer.data[0];

    // Shrink command's buffer.
    command_buffer.data++;
    command_buffer.size--;

    // Validate remaining part of the buffer.
    // Note: This ensures that the command and its arguments are always
    // zero-terminated.
    if((command_buffer.size == 0) ||
       (command_buffer.data[command_buffer.size - 1] != '\0')) {
        goto end;
    }

    // Allocate memory for the command and its arguments.
    // Note: The first element in the list is the command itself, and the last
    // element is always NULL.
    char* command_and_args[rose_n_ipc_command_arguments_max + 1] = {};

    // Read the command and its arguments.
    for(char **arg = command_and_args,
             **sentinel = command_and_args + rose_n_ipc_command_arguments_max;
        (arg != sentinel) && (command_buffer.size != 0); ++arg) {
        // Save the argument.
        *arg = (char*)(command_buffer.data);

        // Skip until the null character.
        for(; *(command_buffer.data++) != '\0'; command_buffer.size--) {
        }
    }

    // Execute the command.
    rose_command_list_execute_command(
        connection->context->command_list, rights, command_and_args);
end:
    return;
}

static void
rose_ipc_connection_dispatch_command_queue(
    struct rose_ipc_connection* connection) {
    // Do nothing if connection has an active transmission.
    if(rose_ipc_is_tx_active(&(connection->io_context))) {
        return;
    }

    // Do nothing if command queue is empty.
    if(connection->dispatcher.queue.size == 0) {
        return;
    }

    // Allocate storage for command buffer.
    unsigned char storage[rose_ipc_buffer_size_max];

    // Construct the command buffer.
    struct rose_ipc_buffer_ref buffer = {
        .data = storage,
        .size = connection->dispatcher.queue.size * rose_ipc_command_size};

#define pass_(command) (command).data, sizeof((command).data)

    // Fill the command buffer.
    for(size_t i = 0; i != connection->dispatcher.queue.size; ++i) {
        memcpy(buffer.data + i * rose_ipc_command_size,
               pass_(connection->dispatcher.queue.data[i]));
    }

#undef pass_

    // Clear the queue.
    connection->dispatcher.queue.size = 0;

    // Transmit the buffer.
    rose_ipc_tx(&(connection->io_context), buffer);
}

////////////////////////////////////////////////////////////////////////////////
// Status-reporting-related utility functions.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_ipc_connection_queue_status(struct rose_ipc_connection* connection,
                                 struct rose_ipc_status status) {
    static size_t const data_sizes[] = {
        // rose_ipc_status_type_server_state
        rose_ipc_status_server_state_size,
        // rose_ipc_status_type_keyboard_keymap
        0,
        // rose_ipc_status_type_keyboard_control_scheme
        0,
        // rose_ipc_status_type_theme
        0,
        // rose_ipc_status_type_input_initialized
        rose_ipc_status_device_id_size,
        // rose_ipc_status_type_input_destroyed
        rose_ipc_status_device_id_size,
        // rose_ipc_status_type_output_initialized
        rose_ipc_status_device_id_size,
        // rose_ipc_status_type_output_destroyed
        rose_ipc_status_device_id_size};

#define array_size_(x) (sizeof(x) / sizeof((x)[0]))

    // Ignore status messages of unknown type.
    if((status.type < 0) || (status.type >= array_size_(data_sizes))) {
        return true;
    }

#undef array_size_

    // Compute serialized size of the given status message.
    size_t status_size = 1 + data_sizes[status.type];

#define pass_(x) (x), sizeof((x))

    // If the given status message reports server's state, and such state has
    // already been queued in connection's status buffer, then overwrite it.
    if((status.type == rose_ipc_status_type_server_state) &&
       (connection->status.server_state_offset != -1)) {
        // Write the state.
        memcpy(connection->status.buffer.data +
                   connection->status.server_state_offset + 1,
               pass_(status.server_state));

        // Do nothing else.
        return true;
    }

    // Check if connection's status buffer has enough space.
    if((connection->status.buffer.size + status_size) >
       rose_ipc_buffer_size_max) {
        return false;
    }

    // If the given status message reports server's state, then save its
    // position in connection's status buffer.
    if(status.type == rose_ipc_status_type_server_state) {
        connection->status.server_state_offset =
            (ptrdiff_t)(connection->status.buffer.size);
    }

    // Write status message's type.
    connection->status.buffer.data[connection->status.buffer.size++] =
        (unsigned char)(status.type);

    // Write status message's data.
    switch(status.type) {
        case rose_ipc_status_type_server_state:
            memcpy(
                connection->status.buffer.data + connection->status.buffer.size,
                pass_(status.server_state));

            break;

        case rose_ipc_status_type_keyboard_keymap:
            // fall-through
        case rose_ipc_status_type_keyboard_control_scheme:
            // fall-through
        case rose_ipc_status_type_theme:
            // Note: No data.

            break;

        case rose_ipc_status_type_input_initialized:
            // fall-through
        case rose_ipc_status_type_input_destroyed:
            // fall-through
        case rose_ipc_status_type_output_initialized:
            // fall-through
        case rose_ipc_status_type_output_destroyed:
            memcpy(
                connection->status.buffer.data + connection->status.buffer.size,
                &(status.device_id), sizeof(status.device_id));

            // fall-through

        default:
            break;
    }

#undef pass_

    // Update connection's status buffer size.
    connection->status.buffer.size += data_sizes[status.type];

    // Operation succeeded.
    return true;
}

static void
rose_ipc_connection_transmit_status_buffer(
    struct rose_ipc_connection* connection) {
    // Do nothing if connection has an active transmission.
    if(rose_ipc_is_tx_active(&(connection->io_context))) {
        return;
    }

    // Obtain the status buffer.
    struct rose_ipc_buffer_ref status_buffer = {
        .data = connection->status.buffer.data,
        .size = connection->status.buffer.size};

    // Reset connection's state.
    connection->status.buffer.size = 0;
    connection->status.server_state_offset = -1;

    // Transmit the status buffer, if it is not empty.
    if(status_buffer.size != 0) {
        rose_ipc_tx(&(connection->io_context), status_buffer);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Data-transmission-related event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_ipc_connection_handle_rx(void* context, enum rose_ipc_io_result result,
                              struct rose_ipc_buffer_ref buffer) {
    // Obtain a pointer to the connection.
    struct rose_ipc_connection* connection = context;

    // Handle operation's result.
    if(result != rose_ipc_io_result_success) {
        goto error;
    }

    // Process received packet.
    switch(connection->type) {
        case rose_ipc_connection_type_none:
            // Validate received buffer's size.
            if(buffer.size != 1) {
                goto error;
            }

            // Change connection's type.
            if(!rose_ipc_connection_transition(connection, buffer.data[0])) {
                goto error;
            }

            // If connection is now of status-reporting type, then send the
            // current status.
            if(connection->type == rose_ipc_connection_type_status) {
                rose_ipc_connection_send_status(
                    connection,
                    rose_server_context_obtain_status(connection->context));
            }

            return;

        case rose_ipc_connection_type_configurator:
            rose_ipc_connection_dispatch_configuration_request(
                connection, buffer);
            return;

        case rose_ipc_connection_type_dispatcher:
            rose_ipc_connection_execute_command(connection, buffer);
            return;

        case rose_ipc_connection_type_status:
            // fall-through
        default:
            break;
    }

error:
    // On error, destroy the connection.
    rose_ipc_connection_destroy(connection);
}

static void
rose_ipc_connection_handle_tx(void* context, enum rose_ipc_io_result result) {
    // Obtain a pointer to the connection.
    struct rose_ipc_connection* connection = context;

    // Handle operation's result.
    if(result != rose_ipc_io_result_success) {
        goto error;
    }

    // Perform additional actions depending on connection's type.
    switch(connection->type) {
        case rose_ipc_connection_type_dispatcher:
            rose_ipc_connection_dispatch_command_queue(connection);
            break;

        case rose_ipc_connection_type_status:
            rose_ipc_connection_transmit_status_buffer(connection);
            break;

        case rose_ipc_connection_type_none:
            // fall-through
        case rose_ipc_connection_type_configurator:
            // fall-through
        default:
            break;
    }

    return;

error:
    // On error, destroy the connection.
    rose_ipc_connection_destroy(connection);
}

////////////////////////////////////////////////////////////////////////////////
// Watchdog timer event handler.
////////////////////////////////////////////////////////////////////////////////

static int
rose_handle_event_ipc_connection_watchdog_timer_expiry(void* data) {
    return rose_ipc_connection_destroy(data), 0;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_connection_initialize(struct rose_ipc_connection_parameters params) {
    // Allocate memory for a new IPC connection.
    struct rose_ipc_connection* connection =
        malloc(sizeof(struct rose_ipc_connection));

    if(connection == NULL) {
        close(params.socket_fd);
        return;
    } else {
        *connection = (struct rose_ipc_connection){
            .context = params.context, .container = params.container};
    }

    // Add connection to the list.
    wl_list_insert( //
        &(connection->container->connections[rose_ipc_connection_type_none]),
        &(connection->link));

    // Initialize connection's IO context.
    if(true) {
        struct rose_ipc_io_context_parameters io_context_params = {
            .socket_fd = params.socket_fd,
            .event_loop = params.context->event_loop,
            .rx_callback_fn = rose_ipc_connection_handle_rx,
            .tx_callback_fn = rose_ipc_connection_handle_tx,
            .external_context = connection};

        if(!rose_ipc_io_context_initialize(
               &(connection->io_context), io_context_params)) {
            goto error;
        }
    }

    // Create watchdog timer.
    connection->watchdog_timer = wl_event_loop_add_timer(
        connection->context->event_loop,
        rose_handle_event_ipc_connection_watchdog_timer_expiry, connection);

    if(connection->watchdog_timer == NULL) {
        goto error;
    }

    // Start watchdog timer.
    wl_event_source_timer_update(connection->watchdog_timer, 1000);

    // Initialization succeeded.
    return;

error:
    // On error, destroy the connection.
    rose_ipc_connection_destroy(connection);
}

void
rose_ipc_connection_destroy(struct rose_ipc_connection* connection) {
    // Remove connection from the list.
    wl_list_remove(&(connection->link));

    // Remove watchdog timer, if any.
    if(connection->watchdog_timer != NULL) {
        wl_event_source_remove(connection->watchdog_timer);
    }

    // Destroy IO context.
    rose_ipc_io_context_destroy(&(connection->io_context));

    // Free memory.
    free(connection);
}

////////////////////////////////////////////////////////////////////////////////
// Data transmission interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_connection_dispatch_command(struct rose_ipc_connection* connection,
                                     struct rose_ipc_command command) {
    // Do nothing if connection is not of command-dispatching type.
    if(connection->type != rose_ipc_connection_type_dispatcher) {
        return;
    }

    // Add the command to the queue.
    connection->dispatcher.queue.size =
        ((connection->dispatcher.queue.size ==
          rose_ipc_connection_dispatcher_queue_size_max)
             ? connection->dispatcher.queue.size - 1
             : connection->dispatcher.queue.size);

    connection->dispatcher.queue.data[connection->dispatcher.queue.size++] =
        command;

    // Dispatch command queue.
    rose_ipc_connection_dispatch_command_queue(connection);
}

void
rose_ipc_connection_send_status(struct rose_ipc_connection* connection,
                                struct rose_ipc_status status) {
    // Do nothing if connection is not of status-reporting type.
    if(connection->type != rose_ipc_connection_type_status) {
        return;
    }

    // Queue the given status in connection's status buffer.
    if(!rose_ipc_connection_queue_status(connection, status)) {
        rose_ipc_connection_destroy(connection);
        return;
    }

    // Transmit connection's status buffer.
    rose_ipc_connection_transmit_status_buffer(connection);
}
