// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_FDEAC0DEC4E94DF387CFAB74ABE394AD
#define H_FDEAC0DEC4E94DF387CFAB74ABE394AD

#include "command.h"
#include "device_input.h"
#include "device_output.h"
#include "device_preference_list.h"
#include "drag_and_drop.h"
#include "keyboard_context.h"

#include "ipc_server.h"
#include "rendering.h"
#include "rendering_raster.h"
#include "rendering_theme.h"
#include "surface.h"
#include "workspace.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct wlr_backend;
struct wlr_session;
struct wlr_renderer;
struct wlr_allocator;

struct wlr_relative_pointer_manager_v1;
struct wlr_pointer_constraints_v1;
struct wlr_tablet_manager_v2;

struct wlr_seat;

////////////////////////////////////////////////////////////////////////////////
// Cursor image definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_cursor_image {
    struct rose_raster* raster;
    int32_t hotspot_x;
    int32_t hotspot_y;
};

////////////////////////////////////////////////////////////////////////////////
// Server context definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_server_context {
    // Associated contexts.
    struct rose_text_rendering_context* text_rendering_context;
    struct rose_keyboard_context* keyboard_context;

    struct {
        struct wlr_xcursor_manager* manager;
        struct rose_cursor_image images[rose_output_cursor_type_count_];
    } cursor_context;

    // Wayland display.
    struct wl_display* display;

    // Wayland event loop.
    struct wl_event_loop* event_loop;

    // Event sources.
    struct wl_event_source* event_source_sigint;
    struct wl_event_source* event_source_sigterm;
    struct wl_event_source* event_source_sigchld;
    struct wl_event_source* event_source_timer;

    // Backend abstraction.
    struct wlr_backend* backend;
    struct wlr_session* session;
    struct wlr_renderer* renderer;
    struct wlr_allocator* allocator;

    // Wayland protocols.
    struct wlr_relative_pointer_manager_v1* relative_pointer_manager;
    struct wlr_pointer_constraints_v1* pointer_constraints;
    struct wlr_tablet_manager_v2* tablet_manager;

    // Seat abstraction.
    struct wlr_seat* seat;

    // Current workspace which receives input events from the seat.
    struct rose_workspace* current_workspace;

    // Static storage.
    struct {
        struct rose_workspace workspace[64];
    } storage;

    // Configuration.
    struct {
        // Paths to configuration directories.
        struct rose_utf8_string paths[2];

        // Argument lists for system processes.
        struct {
            struct rose_command_argument_list background;
            struct rose_command_argument_list dispatcher;
            struct rose_command_argument_list notification_daemon;
            struct rose_command_argument_list panel;
            struct rose_command_argument_list screen_locker;
            struct rose_command_argument_list terminal;
        } argument_lists;

        // Keyboard layouts.
        struct rose_utf8_string keyboard_layouts;

        // Keyboard control scheme.
        struct rose_keyboard_control_scheme* keyboard_control_scheme;

        // Theme.
        struct rose_theme theme;
    } config;

    // System processes.
    struct {
        pid_t background_pid;
        pid_t dispatcher_pid;
        pid_t notification_daemon_pid;
        pid_t panel_pid;
        pid_t screen_locker_pid;
    } processes;

    // IPC server.
    struct rose_ipc_server* ipc_server;

    // Command list. Contains a map of running commands with access rights.
    struct rose_command_list* command_list;

    // Device preference list.
    struct rose_device_preference_list* preference_list;

    // Event listeners.
    struct wl_listener listener_backend_new_input;
    struct wl_listener listener_backend_new_output;

    struct wl_listener listener_seat_request_set_cursor;
    struct wl_listener listener_seat_request_set_selection;
    struct wl_listener listener_seat_request_set_primary_selection;
    struct wl_listener listener_seat_request_start_drag;
    struct wl_listener listener_seat_start_drag;

    struct wl_listener listener_xdg_new_toplevel;
    struct wl_listener listener_xdg_new_toplevel_decoration;
    struct wl_listener listener_pointer_constraints_new_constraint;

    // List of visible menus.
    struct wl_list menus_visible;

    // Lists of workspaces.
    struct wl_list workspaces;
    struct wl_list workspaces_without_output;

    // Lists of input and output devices.
    struct wl_list inputs;
    struct wl_list inputs_keyboards;
    struct wl_list inputs_tablets;
    struct wl_list outputs;

    // Flags.
    bool is_screen_locked, is_waiting_for_user_interaction, is_timer_armed;
    bool are_keyboard_shortcuts_inhibited;
};

////////////////////////////////////////////////////////////////////////////////
// Server context state definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_server_context_state {
    unsigned input_device_count, output_device_count;
};

////////////////////////////////////////////////////////////////////////////////
// Configuration-related definitions.
////////////////////////////////////////////////////////////////////////////////

enum rose_server_context_configuration_type {
    // Updating configuration from files on disk.
    rose_server_context_configure_keyboard_control_scheme = 0x01,
    rose_server_context_configure_keyboard_layouts = 0x02,
    rose_server_context_configure_theme = 0x04,

    // Screen locking/unlocking.
    rose_server_context_configure_screen_lock = 0x08,
    rose_server_context_configure_screen_unlock = 0x10
};

// Server context's configuration mask. Is a bitwise OR of zero or more values
// from the rose_server_context_configuration_type enumeration.
typedef unsigned rose_server_context_configuration_mask;

struct rose_server_context_configuration_parameters {
    rose_server_context_configuration_mask flags;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_initialize(struct rose_server_context* context);

void
rose_server_context_destroy(struct rose_server_context* context);

////////////////////////////////////////////////////////////////////////////////
// Configuration interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_set_keyboard_layout(
    struct rose_server_context* context, unsigned layout_index);

void
rose_server_context_configure(
    struct rose_server_context* context,
    struct rose_server_context_configuration_parameters parameters);

////////////////////////////////////////////////////////////////////////////////
// Cursor image acquisition interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_cursor_image
rose_server_context_obtain_cursor_image(
    struct rose_server_context* context, enum rose_output_cursor_type type,
    float scale);

////////////////////////////////////////////////////////////////////////////////
// Device acquisition interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_input*
rose_server_context_obtain_input(
    struct rose_server_context* context, unsigned id);

struct rose_output*
rose_server_context_obtain_output(
    struct rose_server_context* context, unsigned id);

////////////////////////////////////////////////////////////////////////////////
// State query interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_status
rose_server_context_obtain_status(struct rose_server_context* context);

struct rose_server_context_state
rose_server_context_state_obtain(struct rose_server_context* context);

////////////////////////////////////////////////////////////////////////////////
// IPC access rights checking interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_check_ipc_access_rights(
    struct rose_server_context* context, pid_t pid,
    enum rose_ipc_connection_type connection_type);

#endif // H_FDEAC0DEC4E94DF387CFAB74ABE394AD
