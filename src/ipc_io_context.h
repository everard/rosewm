// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_5286243876C54530AF095095C7BD6287
#define H_5286243876C54530AF095095C7BD6287

#include "ipc_types.h"

////////////////////////////////////////////////////////////////////////////////
// IPC IO result definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_ipc_io_result {
    rose_ipc_io_result_failure,
    rose_ipc_io_result_success,
    rose_ipc_io_result_partial
};

////////////////////////////////////////////////////////////////////////////////
// IPC IO callback function type definitions.
////////////////////////////////////////////////////////////////////////////////

typedef void (*rose_ipc_rx_callback_fn)(
    void*, enum rose_ipc_io_result, struct rose_ipc_buffer_ref);

typedef void (*rose_ipc_tx_callback_fn)(void*, enum rose_ipc_io_result);

////////////////////////////////////////////////////////////////////////////////
// IPC IO context definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_io_context {
    // IPC socket's file descriptor.
    int socket_fd;

    // Packets.
    struct rose_ipc_packet rx_packet;
    struct rose_ipc_packet tx_packet;

    // IO event sources.
    struct wl_event_source* rx_event_source;
    struct wl_event_source* tx_event_source;

    // IO callback function pointers.
    rose_ipc_rx_callback_fn rx_callback_fn;
    rose_ipc_tx_callback_fn tx_callback_fn;

    // External context, passed to the callback functions.
    void* external_context;
};

////////////////////////////////////////////////////////////////////////////////
// IPC IO context initialization-related definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_io_context_parameters {
    // IPC socket's file descriptor.
    int socket_fd;

    // Server's event loop.
    struct wl_event_loop* event_loop;

    // IO callback function pointers.
    rose_ipc_rx_callback_fn rx_callback_fn;
    rose_ipc_tx_callback_fn tx_callback_fn;

    // External context, passed to the callback functions.
    void* external_context;
};

////////////////////////////////////////////////////////////////////////////////
// IPC IO context initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_ipc_io_context_initialize(
    struct rose_ipc_io_context* io_context,
    struct rose_ipc_io_context_parameters parameters);

void
rose_ipc_io_context_destroy(struct rose_ipc_io_context* io_context);

////////////////////////////////////////////////////////////////////////////////
// Data transmission interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_tx(
    struct rose_ipc_io_context* io_context, struct rose_ipc_buffer_ref buffer);

////////////////////////////////////////////////////////////////////////////////
// IPC IO context's state query interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_ipc_is_tx_active(struct rose_ipc_io_context* io_context);

#endif // H_5286243876C54530AF095095C7BD6287
