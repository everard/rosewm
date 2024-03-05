// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"
#include "ipc_connection.h"
#include "ipc_server.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

////////////////////////////////////////////////////////////////////////////////
// IPC server definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_server {
    // Pointer to the server context.
    struct rose_server_context* context;

    // Listening socket's file descriptor and address.
    int socket_fd;
    struct sockaddr_un socket_addr;

    // Event source, event happens when connection is established.
    struct wl_event_source* event_source;

    // Listener for destruction event of the Wayland display.
    struct wl_listener listener_display_destroy;

    // Container of active IPC connections.
    struct rose_ipc_connection_container container;
};

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
////////////////////////////////////////////////////////////////////////////////

static int
rose_handle_event_ipc_server_connection(int fd, uint32_t mask, void* data) {
    unused_(mask);

    // Accept incoming connection.
    if((fd = accept(fd, NULL, NULL)) == -1) {
        return 0;
    }

    // Obtain the IPC server.
    struct rose_ipc_server* server = data;

    // Configure connected socket.
    if(true) {
        // Obtain socket's flags.
        int flags_fd = 0, flags_fl = 0;
        if(((flags_fd = fcntl(fd, F_GETFD)) == -1) ||
           ((flags_fl = fcntl(fd, F_GETFL)) == -1)) {
            goto error;
        }

        // Set socket's properties.
        if((fcntl(fd, F_SETFD, flags_fd | FD_CLOEXEC) == -1) ||
           (fcntl(fd, F_SETFL, flags_fl | O_NONBLOCK) == -1)) {
            goto error;
        }
    }

    // Initialize a new connection.
    struct rose_ipc_connection_parameters parameters = {
        .socket_fd = fd,
        .context = server->context,
        .container = &(server->container)};

    return rose_ipc_connection_initialize(parameters), 0;

error:
    // On error, close client's socket.
    return close(fd), 0;
}

static void
rose_handle_event_display_destroy(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the IPC server.
    struct rose_ipc_server* server =
        wl_container_of(listener, server, listener_display_destroy);

    // Destroy the IPC server.
    rose_ipc_server_destroy(server);
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_server*
rose_ipc_server_initialize(struct rose_server_context* context) {
    // Allocate memory for a new IPC server.
    struct rose_ipc_server* server = malloc(sizeof(struct rose_ipc_server));

    if(server == NULL) {
        return NULL;
    } else {
        *server = (struct rose_ipc_server){.context = context, .socket_fd = -1};
    }

    // Initialize lists of connections.
    for(ptrdiff_t i = 0; i != rose_ipc_connection_type_count_; ++i) {
        wl_list_init(&(server->container.connections[i]));
    }

    // Initialize event listeners.
    server->listener_display_destroy.notify = rose_handle_event_display_destroy;
    wl_display_add_destroy_listener(
        context->display, &(server->listener_display_destroy));

    // Create a UNIX socket which will be listening for incoming connections.
    server->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(server->socket_fd == -1) {
        goto error;
    }

    // Set socket's properties.
    if((fcntl(server->socket_fd, F_SETFD, FD_CLOEXEC) == -1) ||
       (fcntl(server->socket_fd, F_SETFL, O_NONBLOCK) == -1)) {
        goto error;
    }

    // Initialize socket's address.
    if(true) {
        // Specify socket's family.
        server->socket_addr.sun_family = AF_UNIX;

        // Select runtime directory.
        char* runtime_dir = getenv("XDG_RUNTIME_DIR");
        runtime_dir = ((runtime_dir == NULL) ? "/tmp" : runtime_dir);

#define pair_(x) (x), sizeof((x))

        // Write socket's path to the string.
        int n = snprintf( //
            pair_(server->socket_addr.sun_path), "%s/rose.wm.%u.%i.socket",
            runtime_dir, getuid(), getpid());

#undef pair_

        // Check if the path has been successfully written.
        if((n <= 0) || (n >= (int)(sizeof(server->socket_addr.sun_path)))) {
            server->socket_addr.sun_path[0] = '\0';
            goto error;
        }
    }

    // Bind socket to the address.
    if(bind(server->socket_fd, (struct sockaddr*)(&(server->socket_addr)),
            sizeof(server->socket_addr)) == -1) {
        goto error;
    }

    // Start listening for incoming connections.
    if(listen(server->socket_fd, 4) == -1) {
        goto error;
    }

    // Add event source for handling incoming connections.
    server->event_source = wl_event_loop_add_fd(
        context->event_loop, server->socket_fd, WL_EVENT_READABLE,
        rose_handle_event_ipc_server_connection, server);

    if(server->event_source == NULL) {
        goto error;
    }

    // Set environment variable.
    setenv("ROSE_IPC_ENDPOINT", server->socket_addr.sun_path, true);

    // Initialization succeeded.
    return server;

error:
    // On error, destroy the IPC server.
    return rose_ipc_server_destroy(server), NULL;
}

void
rose_ipc_server_destroy(struct rose_ipc_server* server) {
    // Remove event listener.
    wl_list_remove(&(server->listener_display_destroy.link));

    // Remove event source which handles incoming connections.
    if(server->event_source != NULL) {
        wl_event_source_remove(server->event_source);
    }

    // Close listening socket.
    if(server->socket_fd != -1) {
        close(server->socket_fd);
    }

    // Remove socket's binding address.
    if(server->socket_addr.sun_path[0] != '\0') {
        unlink(server->socket_addr.sun_path);
    }

    // Destroy all active connections.
    if(true) {
        struct rose_ipc_connection* connection = NULL;
        struct rose_ipc_connection* _ = NULL;

        for(ptrdiff_t i = 0; i != rose_ipc_connection_type_count_; ++i) {
            wl_list_for_each_safe(
                connection, _, &(server->container.connections[i]), link) {
                rose_ipc_connection_destroy(connection);
            }
        }
    }

    // Free memory.
    free(server);
}

////////////////////////////////////////////////////////////////////////////////
// Data transmission interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_server_dispatch_command(struct rose_ipc_server* server,
                                 struct rose_ipc_command command) {
    struct wl_list* list =
        &(server->container.connections[rose_ipc_connection_type_dispatcher]);

    struct rose_ipc_connection* connection = NULL;
    struct rose_ipc_connection* _ = NULL;

    wl_list_for_each_safe(connection, _, list, link) {
        rose_ipc_connection_dispatch_command(connection, command);
    }
}

void
rose_ipc_server_broadcast_status(struct rose_ipc_server* server,
                                 struct rose_ipc_status status) {
    struct wl_list* list =
        &(server->container.connections[rose_ipc_connection_type_status]);

    struct rose_ipc_connection* connection = NULL;
    struct rose_ipc_connection* _ = NULL;

    wl_list_for_each_safe(connection, _, list, link) {
        rose_ipc_connection_send_status(connection, status);
    }
}
