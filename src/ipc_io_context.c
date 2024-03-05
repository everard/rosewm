// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "ipc_io_context.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>

#include <unistd.h>
#include <fcntl.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <errno.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

// This macro is used for passing buffer's data and size as arguments.
#define pass_(buffer) (buffer).data, (buffer).size

////////////////////////////////////////////////////////////////////////////////
// Data transmission-related utility functions.
////////////////////////////////////////////////////////////////////////////////

size_t
rose_ipc_packet_unpack_payload_size(
    unsigned char header[static rose_ipc_packet_header_size]) {
    // Obtain payload's size which is stored in little-endian byte order.
    uint16_t result = 0;
    for(ptrdiff_t i = 0; i != rose_ipc_packet_header_size; ++i) {
        result |= ((uint16_t)(header[i])) << ((uint16_t)(i * CHAR_BIT));
    }

    // Return the result.
    return (size_t)(result);
}

enum rose_ipc_io_result
rose_ipc_rx_more(struct rose_ipc_io_context* io_context) {
    // Compute required data size.
    size_t data_size_required = rose_ipc_packet_header_size;
    if(io_context->rx_packet.size >= rose_ipc_packet_header_size) {
        // If packet's header has been received completely, then compute
        // payload's size.
        size_t payload_size =
            rose_ipc_packet_unpack_payload_size(io_context->rx_packet.data);

        // Validate payload's size.
        if((payload_size > rose_ipc_buffer_size_max) ||
           (payload_size <
            (io_context->rx_packet.size - rose_ipc_packet_header_size))) {
            return rose_ipc_io_result_failure;
        }

        // Compute total packet's size.
        data_size_required += payload_size;
    }

    // Obtain a reference to the remainder of the packet.
    struct rose_ipc_buffer_ref remainder = {
        .data = io_context->rx_packet.data + io_context->rx_packet.size,
        .size = data_size_required - io_context->rx_packet.size};

    // Read data from the socket.
    ssize_t n = recv(io_context->socket_fd, pass_(remainder), 0);

    // Handle IO operation's result.
    if(n == -1) {
        if((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            // Note: Read failed due to unrecoverable error.
            return rose_ipc_io_result_failure;
        } else {
            // Note: Nothing has been read from the socket.
            n = 0;
        }
    }

    // Update packet's size.
    io_context->rx_packet.size += (size_t)(n);

    // Perform computations depending on the size of received data.
    if(io_context->rx_packet.size == rose_ipc_packet_header_size) {
        // If only packet's header has been received completely, then compute
        // payload's size.
        size_t payload_size =
            rose_ipc_packet_unpack_payload_size(io_context->rx_packet.data);

        // Validate payload's size.
        if(payload_size > rose_ipc_buffer_size_max) {
            return rose_ipc_io_result_failure;
        }

        // Signal operation's success.
        return ((payload_size == 0) ? rose_ipc_io_result_success
                                    : rose_ipc_io_result_partial);
    } else {
        // Otherwise, signal operation's success.
        return ((io_context->rx_packet.size == data_size_required)
                    ? rose_ipc_io_result_success
                    : rose_ipc_io_result_partial);
    }
}

enum rose_ipc_io_result
rose_ipc_tx_more(struct rose_ipc_io_context* io_context) {
    // Write data to the socket.
    ssize_t n = write(io_context->socket_fd, pass_(io_context->tx_packet));

    // Handle IO operation's result.
    if(n == -1) {
        if((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            // Note: Write failed due to unrecoverable error.
            return rose_ipc_io_result_failure;
        } else {
            // Note: Nothing has been written to the socket.
            n = 0;
        }
    }

    // Update packet's remaining size.
    io_context->tx_packet.size -= (size_t)(n);

    // Shift remaining data.
    memmove(io_context->tx_packet.data, io_context->tx_packet.data + n,
            io_context->tx_packet.size);

    // Update event source.
    // Note: Check for socket's writable event if the packet has not been
    // written completely.
    if(true) {
        // Compute corresponding mask.
        uint32_t mask =
            ((io_context->tx_packet.size == 0) ? 0 : WL_EVENT_WRITABLE);

        // Set the mask.
        if(wl_event_source_fd_update(io_context->tx_event_source, mask) == -1) {
            return rose_ipc_io_result_failure;
        }
    }

    // Signal operation's success.
    return ((io_context->tx_packet.size != 0) ? rose_ipc_io_result_partial
                                              : rose_ipc_io_result_success);
}

////////////////////////////////////////////////////////////////////////////////
// IPC event handlers.
////////////////////////////////////////////////////////////////////////////////

static int
rose_handle_event_ipc_rx_data(int fd, uint32_t mask, void* data) {
    // Obtain the IO context.
    struct rose_ipc_io_context* io_context = data;

    // Declare a buffer which will contain received data.
    struct rose_ipc_buffer_ref buffer = {};

    // Check for errors.
    if((mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) != 0) {
        goto error;
    }

    // Check if the socket is readable.
    for(char _[1]; true;) {
        // Check if the socket has pending data.
        ssize_t n = recv(fd, _, sizeof(_), MSG_PEEK);

        // Handle IO operation's result.
        if(n == -1) {
            if(errno == EINTR) {
                // If read has been interrupted by a signal, then try again.
                continue;
            } else if((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                // Note: Read failed due to unrecoverable error.
                goto error;
            } else {
                // Note: The socket has no pending data.
                n = 0;
            }
        }

        // Do nothing if the socket has no pending data.
        if(n == 0) {
            return 0;
        }

        // If the socket has pending data, then break out of the cycle and
        // continue execution.
        break;
    }

    // Read data from the socket and perform additional actions depending on the
    // result of this operation.
    switch(rose_ipc_rx_more(io_context)) {
        case rose_ipc_io_result_success:
            // Obtain packet's data.
            buffer.data =
                io_context->rx_packet.data + rose_ipc_packet_header_size;

            buffer.size =
                rose_ipc_packet_unpack_payload_size(io_context->rx_packet.data);

            // Reset packet's size.
            io_context->rx_packet.size = 0;

            // Signal operation's success.
            io_context->rx_callback_fn(io_context->external_context,
                                       rose_ipc_io_result_success, buffer);

            // fall-through
        case rose_ipc_io_result_partial:
            // Request event's recheck.
            // Note: Recheck is needed since more packets could still reside in
            // the socket's receiving queue.
            return 1;

        case rose_ipc_io_result_failure:
            // fall-through
        default:
            break;
    }

error:
    // On error, signal operation's failure.
    return io_context->rx_callback_fn(io_context->external_context,
                                      rose_ipc_io_result_failure, buffer),
           0;
}

static int
rose_handle_event_ipc_tx_data(int fd, uint32_t mask, void* data) {
    unused_(fd);

    // Obtain the IO context.
    struct rose_ipc_io_context* io_context = data;

    // Check for errors.
    if((mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) != 0) {
        goto error;
    }

    // Write remaining data to the socket.
    enum rose_ipc_io_result result;
    do {
        if((result = rose_ipc_tx_more(io_context)) ==
           rose_ipc_io_result_success) {
            // If IO operation completed successfully, then signal success.
            io_context->tx_callback_fn(
                io_context->external_context, rose_ipc_io_result_success);
        } else {
            // Otherwise, break out of the cycle.
            break;
        }
    } while(io_context->tx_packet.size != 0);

    // Perform additional actions depending on the result of IO operation.
    switch(result) {
        case rose_ipc_io_result_success:
            // fall-through
        case rose_ipc_io_result_partial:
            return 0;

        case rose_ipc_io_result_failure:
            // fall-through
        default:
            break;
    }

error:
    // On error, signal operation's failure.
    return io_context->tx_callback_fn(
               io_context->external_context, rose_ipc_io_result_failure),
           0;
}

////////////////////////////////////////////////////////////////////////////////
// IPC IO context initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_ipc_io_context_initialize(
    struct rose_ipc_io_context* io_context,
    struct rose_ipc_io_context_parameters parameters) {
    // Initialize the IO context.
    *io_context = (struct rose_ipc_io_context){
        .socket_fd = parameters.socket_fd,
        .rx_callback_fn = parameters.rx_callback_fn,
        .tx_callback_fn = parameters.tx_callback_fn,
        .external_context = parameters.external_context};

    // Add event sources for handling IO operations.
    io_context->rx_event_source = wl_event_loop_add_fd(
        parameters.event_loop, parameters.socket_fd, WL_EVENT_READABLE,
        rose_handle_event_ipc_rx_data, io_context);

    io_context->tx_event_source = wl_event_loop_add_fd( //
        parameters.event_loop, parameters.socket_fd, 0,
        rose_handle_event_ipc_tx_data, io_context);

    if((io_context->rx_event_source == NULL) ||
       (io_context->tx_event_source == NULL)) {
        goto error;
    }

    // Register RX event source for recheck.
    wl_event_source_check(io_context->rx_event_source);

    // Initialization succeeded.
    return true;

error:
    // On error, destroy the context.
    return rose_ipc_io_context_destroy(io_context), false;
}

void
rose_ipc_io_context_destroy(struct rose_ipc_io_context* io_context) {
    // Shutdown IPC socket.
    if(io_context->socket_fd != -1) {
        shutdown(io_context->socket_fd, SHUT_RDWR);
    }

    // Remove event sources which handle IO operations.
    if(io_context->rx_event_source != NULL) {
        wl_event_source_remove(io_context->rx_event_source);
    }

    if(io_context->tx_event_source != NULL) {
        wl_event_source_remove(io_context->tx_event_source);
    }

    // Close IPC socket.
    if(io_context->socket_fd != -1) {
        close(io_context->socket_fd);
    }

    // Clear context's data.
    *io_context = (struct rose_ipc_io_context){.socket_fd = -1};
}

////////////////////////////////////////////////////////////////////////////////
// Data transmission interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_tx(struct rose_ipc_io_context* io_context,
            struct rose_ipc_buffer_ref buffer) {
    // Validate transmission buffer's size.
    if(buffer.size > rose_ipc_buffer_size_max) {
        goto error;
    }

    // Validate IO context's state.
    if(rose_ipc_is_tx_active(io_context)) {
        goto error;
    }

    // Compose the packet.
    if(true) {
        // Write payload's size in little-endian byte order.
        for(ptrdiff_t i = 0; i != rose_ipc_packet_header_size; ++i) {
            io_context->tx_packet.data[i] =
                (unsigned char)(((uint16_t)(buffer.size)) >>
                                ((uint16_t)(i * CHAR_BIT)));
        }

        // Compute packet's total size.
        io_context->tx_packet.size = rose_ipc_packet_header_size + buffer.size;

        // Write the payload.
        memmove(io_context->tx_packet.data + rose_ipc_packet_header_size,
                buffer.data, buffer.size);
    }

    // Transmit the packet and perform additional actions depending on the
    // result of IO operation.
    switch(rose_ipc_tx_more(io_context)) {
        case rose_ipc_io_result_success:
            io_context->tx_callback_fn(
                io_context->external_context, rose_ipc_io_result_success);

            // fall-through
        case rose_ipc_io_result_partial:
            return;

        case rose_ipc_io_result_failure:
            // fall-through
        default:
            break;
    }

error:
    // On error, signal operation's failure.
    io_context->tx_callback_fn(
        io_context->external_context, rose_ipc_io_result_failure);
}

////////////////////////////////////////////////////////////////////////////////
// IPC IO context's state query interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_ipc_is_tx_active(struct rose_ipc_io_context* io_context) {
    return (io_context->tx_packet.size != 0);
}
