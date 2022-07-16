// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_EFF6D364369E4B6EB572041F713892B3
#define H_EFF6D364369E4B6EB572041F713892B3

#include <wayland-server-core.h>

////////////////////////////////////////////////////////////////////////////////
// IPC buffer definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_ipc_buffer_size_max = 8 * 1024 };

struct rose_ipc_buffer_ref {
    unsigned char* data;
    size_t size;
};

struct rose_ipc_buffer {
    unsigned char data[rose_ipc_buffer_size_max];
    size_t size;
};

////////////////////////////////////////////////////////////////////////////////
// IPC packet definition.
////////////////////////////////////////////////////////////////////////////////

enum {
    rose_ipc_packet_header_size = sizeof(uint16_t),
    rose_ipc_packet_size_max =
        rose_ipc_packet_header_size + rose_ipc_buffer_size_max
};

struct rose_ipc_packet {
    unsigned char data[rose_ipc_packet_size_max];
    size_t size;
};

////////////////////////////////////////////////////////////////////////////////
// IPC command definition. Meaning of particular command is tied to dispatcher.
////////////////////////////////////////////////////////////////////////////////

enum { rose_ipc_command_size = 64 };

struct rose_ipc_command {
    unsigned char data[rose_ipc_command_size];
};

////////////////////////////////////////////////////////////////////////////////
// IPC status-related definitions.
////////////////////////////////////////////////////////////////////////////////

enum {
    rose_ipc_status_server_state_size = 4,
    rose_ipc_status_device_id_size = sizeof(unsigned)
};

enum rose_ipc_status_type {
    rose_ipc_status_type_server_state,
    rose_ipc_status_type_keyboard_keymap,
    rose_ipc_status_type_keyboard_control_scheme,
    rose_ipc_status_type_theme,
    rose_ipc_status_type_input_initialized,
    rose_ipc_status_type_input_destroyed,
    rose_ipc_status_type_output_initialized,
    rose_ipc_status_type_output_destroyed
};

struct rose_ipc_status {
    enum rose_ipc_status_type type;

    union {
        unsigned char server_state[rose_ipc_status_server_state_size];
        unsigned device_id;
    };
};

////////////////////////////////////////////////////////////////////////////////
// IPC connection-related definitions.
////////////////////////////////////////////////////////////////////////////////

enum { rose_n_ipc_connections_max = 32 };

enum rose_ipc_connection_type {
    rose_ipc_connection_type_none,
    rose_ipc_connection_type_configurator,
    rose_ipc_connection_type_dispatcher,
    rose_ipc_connection_type_status,
    rose_n_ipc_connection_types
};

struct rose_ipc_connection_container {
    struct wl_list connections[rose_n_ipc_connection_types];
    size_t size;
};

#endif // H_EFF6D364369E4B6EB572041F713892B3
