// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_6C9E507A3B654A24A0641FB14C006B4D
#define H_6C9E507A3B654A24A0641FB14C006B4D

#include "ipc_io_context.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_server_context;

////////////////////////////////////////////////////////////////////////////////
// IPC connection definition.
////////////////////////////////////////////////////////////////////////////////

enum {
    rose_ipc_connection_dispatcher_queue_size_max =
        rose_ipc_buffer_size_max / rose_ipc_command_size
};

struct rose_ipc_connection {
    // Pointer to the server context.
    struct rose_server_context* context;

    // Pointer to the parent container.
    struct rose_ipc_connection_container* container;

    // IO context.
    struct rose_ipc_io_context io_context;

    // Connection's watchdog timer.
    struct wl_event_source* watchdog_timer;

    // Connection's state.
    union {
        // Dispatcher's state.
        struct {
            struct {
                struct rose_ipc_command
                    data[rose_ipc_connection_dispatcher_queue_size_max];
                size_t size;
            } queue;
        } dispatcher;

        // Status notifier's state.
        struct {
            struct rose_ipc_buffer buffer;
            ptrdiff_t server_state_offset;
        } status;
    };

    // List link.
    struct wl_list link;

    // Connection's type.
    enum rose_ipc_connection_type type;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization-related definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_connection_parameters {
    // IPC socket's file descriptor.
    int socket_fd;

    // Pointer to the server context.
    struct rose_server_context* context;

    // Pointer to the parent container.
    struct rose_ipc_connection_container* container;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_connection_initialize(
    struct rose_ipc_connection_parameters parameters);

void
rose_ipc_connection_destroy(struct rose_ipc_connection* connection);

////////////////////////////////////////////////////////////////////////////////
// Data transmission interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_connection_dispatch_command(
    struct rose_ipc_connection* connection, struct rose_ipc_command command);

void
rose_ipc_connection_send_status(
    struct rose_ipc_connection* connection, struct rose_ipc_status status);

////////////////////////////////////////////////////////////////////////////////
// Server configuration interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_connection_dispatch_configuration_request(
    struct rose_ipc_connection* connection, struct rose_ipc_buffer_ref request);

#endif // H_6C9E507A3B654A24A0641FB14C006B4D
