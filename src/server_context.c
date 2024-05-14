// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "filesystem.h"
#include "server_context.h"

#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_seat.h>

#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_tablet_v2.h>

#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_data_device.h>

#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_viewporter.h>

#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/interfaces/wlr_keyboard.h>

#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

// Computes the size of the given array.
#define array_size_(a) ((size_t)(sizeof(a) / sizeof((a)[0])))

// Adds an iteration statement which runs through elements of the given array.
#define for_each_(type, x, array)                                \
    for(type* x = array, *sentinel = array + array_size_(array); \
        x != sentinel; ++x)

////////////////////////////////////////////////////////////////////////////////
// String buffer definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_utf8_string_size_max = 4095 };

struct rose_utf8_string_buffer {
    char data[rose_utf8_string_size_max + 1];
};

////////////////////////////////////////////////////////////////////////////////
// String utility functions.
////////////////////////////////////////////////////////////////////////////////

static struct rose_utf8_string_buffer
rose_utf8_string_concat(char* a, char* b) {
    // Zero-initialize a new string buffer.
    struct rose_utf8_string_buffer buffer = {};

    // Append strings to the buffer.
    strncat(buffer.data, a, rose_utf8_string_size_max);
    strncat(buffer.data, b, rose_utf8_string_size_max - strlen(buffer.data));

    // Return the buffer.
    return buffer;
}

static struct rose_utf8_string
rose_utf8_string_read(char const* file_path) {
    return rose_convert_ntbs_to_utf8(
        (char*)(rose_filesystem_read_ntbs(file_path).data));
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_text_rendering_context*
rose_text_rendering_context_initialize_from_file(char const* file_path) {
    // Initialization fails if configuration file is not specified.
    if(file_path == NULL) {
        return NULL;
    }

    // Read configuration file.
    struct rose_memory file = rose_filesystem_read_ntbs(file_path);
    if(file.size == 0) {
        return NULL;
    }

    // Parse configuration file.
    for(size_t i = 0; i != file.size; ++i) {
        switch(file.data[i]) {
            case '\r':
            // fall-through
            case '\n':
                file.data[i] = '\0';
                break;

            default:
                break;
        }
    }

    // Initialize an empty array for fonts.
    struct rose_memory fonts[8] = {};
    size_t font_count = 0;

    // Read fonts.
    for(size_t offset = 0; offset != file.size;) {
        // Limit the number of fonts.
        if(font_count == array_size_(fonts)) {
            break;
        }

        // Obtain the font file path.
        char* font_file_path = (char*)(file.data + offset);

        // Update the offset.
        offset += strlen(font_file_path) + 1;

        // If the path is an empty string, then skip it.
        if(font_file_path[0] == '\0') {
            continue;
        }

        // Read font from the current path.
        if((fonts[font_count++] = rose_filesystem_read_data(font_file_path))
               .size == 0) {
            for(size_t i = 0; i != font_count; ++i) {
                rose_free(&(fonts[i]));
            }

            break;
        }
    }

    // Deallocate configuration file data.
    rose_free(&file);

    // Initialize text rendering context.
    struct rose_text_rendering_context_parameters parameters = {
        .fonts = fonts, .font_count = font_count};

    return rose_text_rendering_context_initialize(parameters);
}

////////////////////////////////////////////////////////////////////////////////
// Process starting/querying utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_server_context_start_processes(struct rose_server_context* context) {
#define start_(type)                                                           \
    if((context->config.argument_lists.type.data != NULL) &&                   \
       (context->processes.type##_pid == (pid_t)(-1))) {                       \
        context->processes.type##_pid = rose_execute_command_in_child_process( \
            context->config.argument_lists.type);                              \
    }

    start_(background);
    start_(dispatcher);
    start_(notification_daemon);
    start_(panel);
    start_(screen_locker);

#undef start_
}

static pid_t
rose_obtain_parent_pid(pid_t pid) {
    // Initialize an empty path string.
    char path[1024] = {};

    // Construct a path to the stats file.
    if(true) {
        int n = snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        if((n <= 0) || (n >= (int)(sizeof(path)))) {
            return (pid_t)(-1);
        }
    }

    // Open the stats file.
    FILE* file = fopen(path, "r");
    if(file == NULL) {
        return (pid_t)(-1);
    }

    // Read the file.
    int parent_pid = -1;
    fscanf(file, "%*d %*s %*c %d", &parent_pid);

    // Close the file.
    fclose(file);

    // Return the PID.
    return (pid_t)(parent_pid);
}

////////////////////////////////////////////////////////////////////////////////
// Filter utility function. Prevents non-privileged clients from accessing
// privileged protocols.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_filter_global(
    struct wl_client const* client, struct wl_global const* global,
    void* data) {
    // Obtain the server context.
    struct rose_server_context* context = data;

    // Obtain interface of the given global.
    struct wl_interface const* interface = wl_global_get_interface(global);

    // Define the prefix of the privileged protocol names.
    static char prefix[] = {'z', 'w', 'l', 'r', '_'};

    // Check the name of the interface object.
    if((strlen(interface->name) > sizeof(prefix)) &&
       (memcmp(interface->name, prefix, sizeof(prefix)) == 0)) {
        // If the interface corresponds to a privileged protocol, then obtain
        // client's PID.
        pid_t client_pid = -1;
        wl_client_get_credentials(
            (struct wl_client*)(client), &client_pid, NULL, NULL);

        // Privileged protocols require privileged clients.
        return (
            (rose_command_list_query_access_rights(
                 context->command_list, client_pid) &
             rose_command_access_wayland_privileged_protocols) != 0);
    }

    // Normal protocols are accessible by all clients.
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers: signals.
////////////////////////////////////////////////////////////////////////////////

static int
rose_handle_signal(int signal_number, void* data) {
    // Obtain the server context.
    struct rose_server_context* context = data;

    // If the signal is SIGINT or SIGTERM, then stop the compositor.
    if((signal_number == SIGINT) || (signal_number == SIGTERM)) {
        wl_display_terminate(context->display);
    }

    // If the signal is SIGCHLD, then clean-up all terminated processes.
    if(signal_number == SIGCHLD) {
        for(pid_t child_pid = -1; true;) {
            // Wait for the state of a signaled process.
            int status = 0;
            while((child_pid = waitpid((pid_t)(-1), &status, WNOHANG)) == -1) {
                if(errno != EINTR) {
                    break;
                }
            }

            // If there is no such process, then break out of the cycle.
            if((child_pid == (pid_t)(-1)) || (child_pid == 0)) {
                break;
            }

            // Skip any process which has not terminated.
            if(!WIFEXITED(status) && !WIFSIGNALED(status)) {
                continue;
            }

            // Notify the command list.
            rose_command_list_notify_command_termination(
                context->command_list, child_pid);

#define check_(name)                                                         \
    if(child_pid == context->processes.name##_pid) {                         \
        /* Reset the PID of the process. */                                  \
        context->processes.name##_pid = (pid_t)(-1);                         \
                                                                             \
        /* Arm the timer which will try to restart system processes. */      \
        if(!(context->is_timer_armed)) {                                     \
            context->is_timer_armed = true;                                  \
            wl_event_source_timer_update(context->event_source_timer, 1000); \
        }                                                                    \
    }

            // Check if any of the system processes has terminated.
            check_(background);
            check_(dispatcher);
            check_(notification_daemon);
            check_(panel);
            check_(screen_locker);

#undef check_
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers: backend.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_backend_new_input(struct wl_listener* listener, void* data) {
    // Obtain the server context.
    struct rose_server_context* context =
        wl_container_of(listener, context, listener_backend_new_input);

    // Initialize a new input device.
    rose_input_initialize(context, data);
}

static void
rose_handle_event_backend_new_output(struct wl_listener* listener, void* data) {
    // Obtain the server context.
    struct rose_server_context* context =
        wl_container_of(listener, context, listener_backend_new_output);

    // Initialize a new output device.
    rose_output_initialize(context, data);
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers: seat.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_seat_request_set_cursor(
    struct wl_listener* listener, void* data) {
    // Obtain the server context.
    struct rose_server_context* context =
        wl_container_of(listener, context, listener_seat_request_set_cursor);

    // Obtain the event.
    struct wlr_seat_pointer_request_set_cursor_event* event = data;

    // Obtain the current output.
    struct rose_output* output = context->current_workspace->output;

    // If such output exists, then set its cursor.
    if(output != NULL) {
        rose_output_cursor_client_surface_set(
            output, event->surface, event->hotspot_x, event->hotspot_y);

        rose_output_cursor_set(output, rose_output_cursor_type_unspecified);
        rose_output_cursor_set(output, rose_output_cursor_type_client);
    }
}

static void
rose_handle_event_seat_request_set_selection(
    struct wl_listener* listener, void* data) {
    // Obtain the server context.
    struct rose_server_context* context =
        wl_container_of(listener, context, listener_seat_request_set_selection);

    // Obtain the event.
    struct wlr_seat_request_set_selection_event* event = data;

    // Satisfy the request.
    wlr_seat_set_selection(context->seat, event->source, event->serial);
}

static void
rose_handle_event_seat_request_set_primary_selection(
    struct wl_listener* listener, void* data) {
    // Obtain the server context.
    struct rose_server_context* context = wl_container_of(
        listener, context, listener_seat_request_set_primary_selection);

    // Obtain the event.
    struct wlr_seat_request_set_primary_selection_event* event = data;

    // Satisfy the request.
    wlr_seat_set_primary_selection(context->seat, event->source, event->serial);
}

static void
rose_handle_event_seat_request_start_drag(
    struct wl_listener* listener, void* data) {
    // Obtain the server context.
    struct rose_server_context* context =
        wl_container_of(listener, context, listener_seat_request_start_drag);

    // Obtain the event.
    struct wlr_seat_request_start_drag_event* event = data;

    // Satisfy the request.
    if(wlr_seat_validate_pointer_grab_serial(
           context->seat, event->origin, event->serial)) {
        wlr_seat_start_pointer_drag(context->seat, event->drag, event->serial);
    } else {
        wlr_data_source_destroy(event->drag->source);
    }
}

static void
rose_handle_event_seat_start_drag(struct wl_listener* listener, void* data) {
    // Obtain the server context.
    struct rose_server_context* context =
        wl_container_of(listener, context, listener_seat_start_drag);

    // Start the drag and drop action.
    rose_drag_and_drop_start(context, data);
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers: Wayland protocols.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_xdg_new_surface(struct wl_listener* listener, void* data) {
    // Obtain the server context.
    struct rose_server_context* context =
        wl_container_of(listener, context, listener_xdg_new_surface);

    // Obtain the XDG surface.
    struct wlr_xdg_surface* xdg_surface = data;

    // Do nothing if the surface is not a top-level XDG surface.
    if(xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        return;
    }

    // Obtain client's PID.
    pid_t client_pid = -1;
    wl_client_get_credentials(
        xdg_surface->client->client, &client_pid, NULL, NULL);

    // Perform actions depending on client's PID.
    if((client_pid == (pid_t)(-1)) || (client_pid == 0)) {
        // Note: Clients with unknown PID can not make widget surfaces.
    } else if(client_pid == context->processes.notification_daemon_pid) {
        // Find a suitable output for this notification surface.
        struct rose_output* output =
            ((context->current_workspace->output != NULL)
                 ? context->current_workspace->output
                 : (wl_list_empty(&(context->outputs))
                        ? NULL
                        : wl_container_of(
                              context->outputs.next, output, link)));

        if(output != NULL) {
            // If such output exists, then initialize the widget.
            rose_output_widget_initialize(
                &(output->ui), xdg_surface->toplevel,
                rose_output_widget_type_notification);

            // And do nothing else.
            return;
        }
    } else if(client_pid == context->processes.screen_locker_pid) {
        // Add the surface as a screen lock widget to an output which does not
        // contain any screen lock widgets.
        struct rose_output* output = NULL;
        wl_list_for_each(output, &(context->outputs), link) {
            // Skip any outputs which already contain screen lock widgets.
            if(!wl_list_empty(&(
                   output->ui.widgets[rose_output_widget_type_screen_lock]))) {
                continue;
            }

            // Initialize the widget.
            rose_output_widget_initialize(
                &(output->ui), xdg_surface->toplevel,
                rose_output_widget_type_screen_lock);

            // Do nothing else.
            return;
        }
    } else if(client_pid == context->processes.background_pid) {
        // Add the surface as a background widget to an output which does not
        // contain any background widgets.
        struct rose_output* output = NULL;
        wl_list_for_each(output, &(context->outputs), link) {
            // Skip any outputs which already contain background widgets.
            if(!wl_list_empty(
                   &(output->ui.widgets[rose_output_widget_type_background]))) {
                continue;
            }

            // Initialize the widget.
            rose_output_widget_initialize(
                &(output->ui), xdg_surface->toplevel,
                rose_output_widget_type_background);

            // Do nothing else.
            return;
        }
    } else if(client_pid == context->processes.dispatcher_pid) {
        if(context->current_workspace->output != NULL) {
            // Initialize the widget.
            rose_output_widget_initialize(
                &(context->current_workspace->output->ui),
                xdg_surface->toplevel, rose_output_widget_type_prompt);

            // And do nothing else.
            return;
        }
    } else if(client_pid == context->processes.panel_pid) {
        // Add the surface as a panel widget to an output which does not contain
        // any panel widgets.
        struct rose_output* output = NULL;
        wl_list_for_each(output, &(context->outputs), link) {
            // Skip any outputs which already contain panel widgets.
            if(!wl_list_empty(
                   &(output->ui.widgets[rose_output_widget_type_panel]))) {
                continue;
            }

            // Initialize the widget.
            rose_output_widget_initialize(
                &(output->ui), xdg_surface->toplevel,
                rose_output_widget_type_panel);

            // Do nothing else.
            return;
        }
    }

    // If the surface is not a widget, then construct surface parameters.
    struct rose_surface_parameters parameters = {
        .workspace = context->current_workspace,
        .toplevel = xdg_surface->toplevel,
        .pointer_constraint = wlr_pointer_constraints_v1_constraint_for_surface(
            context->pointer_constraints, xdg_surface->surface, context->seat)};

    // And initialize a normal top-level surface.
    rose_surface_initialize(parameters);
}

static void
rose_handle_event_xdg_new_toplevel_decoration(
    struct wl_listener* listener, void* data) {
    // Initialize the surface decoration.
    unused_(listener), rose_surface_decoration_initialize(data);
}

static void
rose_handle_event_pointer_constraints_new_constraint(
    struct wl_listener* listener, void* data) {
    // Initialize the pointer constraint.
    unused_(listener), rose_surface_pointer_constraint_initialize(data);
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers: timer.
////////////////////////////////////////////////////////////////////////////////

int
rose_handle_event_server_context_timer_expiry(void* data) {
    // Obtain the server context.
    struct rose_server_context* context = data;

    // Clear timer's flag.
    context->is_timer_armed = false;

    // Restart system processes.
    return rose_server_context_start_processes(context), 0;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_initialize(struct rose_server_context* context) {
    // Initialize the server context object.
    *context = (struct rose_server_context){
        .processes = {
            .background_pid = (pid_t)(-1),
            .dispatcher_pid = (pid_t)(-1),
            .notification_daemon_pid = (pid_t)(-1),
            .panel_pid = (pid_t)(-1),
            .screen_locker_pid = (pid_t)(-1)}};

    // Initialize lists.
    wl_list_init(&(context->menus_visible));
    wl_list_init(&(context->workspaces));
    wl_list_init(&(context->workspaces_without_output));

    wl_list_init(&(context->inputs));
    wl_list_init(&(context->inputs_keyboards));
    wl_list_init(&(context->inputs_tablets));
    wl_list_init(&(context->outputs));

#define initialize_(f) context->listener_##f.notify = rose_handle_event_##f;

    // Initialize event listeners.
    initialize_(backend_new_input);
    initialize_(backend_new_output);

    initialize_(seat_request_set_cursor);
    initialize_(seat_request_set_selection);
    initialize_(seat_request_set_primary_selection);
    initialize_(seat_request_start_drag);
    initialize_(seat_start_drag);

    initialize_(xdg_new_surface);
    initialize_(xdg_new_toplevel_decoration);
    initialize_(pointer_constraints_new_constraint);

#undef initialize_

    // Allocate memory for configuration paths.
    for_each_(struct rose_utf8_string, path, context->config.paths) {
        path->data = calloc(1, rose_utf8_string_size_max + 1);
        if(path->data == NULL) {
            return false;
        }
    }

    // Obtain a path to the user's home directory.
    char* home_path = getenv("HOME");
    if(home_path == NULL) {
        return false;
    }

    // Construct paths to the configuration directories.
    strcpy(
        context->config.paths[0].data,
        rose_utf8_string_concat(home_path, "/.config/rosewm/").data);

    strcpy(
        context->config.paths[1].data,
        rose_utf8_string_concat("", "/etc/rosewm/").data);

    // Read the theme.
    if(true) {
        // Initialize default theme.
        context->config.theme = rose_theme_initialize_default();

        // Try reading the theme from one of the configuration files.
        for_each_(struct rose_utf8_string, path, context->config.paths) {
            if(rose_theme_initialize(
                   rose_utf8_string_concat(path->data, "theme").data,
                   &(context->config.theme))) {
                break;
            }
        }
    }

    // Read keyboard layouts.
    for_each_(struct rose_utf8_string, path, context->config.paths) {
        context->config.keyboard_layouts = rose_utf8_string_read(
            rose_utf8_string_concat(path->data, "keyboard_layouts").data);

        if(context->config.keyboard_layouts.data != NULL) {
            break;
        }
    }

#define read_argument_list_(type)                                           \
    for_each_(struct rose_utf8_string, path, context->config.paths) {       \
        context->config.argument_lists.type =                               \
            rose_command_argument_list_initialize(                          \
                rose_utf8_string_concat(path->data, "system_" #type).data); \
                                                                            \
        if(context->config.argument_lists.type.data != NULL) {              \
            break;                                                          \
        }                                                                   \
    }

    // Read argument lists for system processes.
    read_argument_list_(background);
    read_argument_list_(dispatcher);
    read_argument_list_(notification_daemon);
    read_argument_list_(panel);
    read_argument_list_(screen_locker);
    read_argument_list_(terminal);

#undef read_argument_list_

    // Note: There must always be an argument list to start a terminal emulator.
    if(context->config.argument_lists.terminal.data == NULL) {
        return false;
    }

#define try_(expression)       \
    if((expression) == NULL) { \
        return false;          \
    }

    // Initialize device preference list.
    if(true) {
        // Construct a full path to the file containing device preferences.
        struct rose_utf8_string_buffer file_path = rose_utf8_string_concat(
            context->config.paths[0].data, "device_preferences");

        // Try initializing device preference list.
        try_(
            context->preference_list =
                rose_device_preference_list_initialize(file_path.data));
    }

    // Initialize keyboard control scheme.
    for_each_(struct rose_utf8_string, path, context->config.paths) {
        context->config.keyboard_control_scheme =
            rose_keyboard_control_scheme_initialize(
                rose_utf8_string_concat(path->data, "keyboard_control_scheme")
                    .data);

        if(context->config.keyboard_control_scheme != NULL) {
            break;
        }
    }

    if(context->config.keyboard_control_scheme == NULL) {
        try_(
            context->config.keyboard_control_scheme =
                rose_keyboard_control_scheme_initialize(NULL));
    }

    // Initialize text rendering context.
    for_each_(struct rose_utf8_string, path, context->config.paths) {
        context->text_rendering_context =
            rose_text_rendering_context_initialize_from_file(
                rose_utf8_string_concat(path->data, "fonts").data);

        if(context->text_rendering_context != NULL) {
            break;
        }
    }

    if(context->text_rendering_context == NULL) {
        return false;
    }

    // Initialize keyboard context.
    try_(
        context->keyboard_context = rose_keyboard_context_initialize(
            context->config.keyboard_layouts.data));

    // Create a display object and obtain its event loop.
    try_(context->display = wl_display_create());
    try_(context->event_loop = wl_display_get_event_loop(context->display));

    // Set filtering function which will prevent non-privileged clients from
    // accessing privileged protocols.
    wl_display_set_global_filter(context->display, rose_filter_global, context);

    // Configure event loop's file descriptor.
    if(true) {
        // Obtain event loop's file descriptor.
        int event_loop_fd = wl_event_loop_get_fd(context->event_loop);

        // Obtain the flags.
        int flags = fcntl(event_loop_fd, F_GETFD);
        if(flags == -1) {
            return false;
        }

        // Make sure event loop's file descriptor is closed when subprocesses
        // are started.
        if(fcntl(event_loop_fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
            return false;
        }
    }

    // Create event sources for SIGINT, SIGTERM, and SIGCHLD.
    try_(
        context->event_source_sigint = wl_event_loop_add_signal(
            context->event_loop, SIGINT, rose_handle_signal, context));

    try_(
        context->event_source_sigterm = wl_event_loop_add_signal(
            context->event_loop, SIGTERM, rose_handle_signal, context));

    try_(
        context->event_source_sigchld = wl_event_loop_add_signal(
            context->event_loop, SIGCHLD, rose_handle_signal, context));

    // Create event source for timer.
    try_(
        context->event_source_timer = wl_event_loop_add_timer(
            context->event_loop, rose_handle_event_server_context_timer_expiry,
            context));

    // Initialize cursor context.
    if(true) {
        // Create cursor manager.
        try_(
            context->cursor_context.manager =
                wlr_xcursor_manager_create(NULL, 24));

        // Load cursor theme.
        if(!wlr_xcursor_manager_load(context->cursor_context.manager, 1.0f)) {
            return false;
        }

        // Define a mapping between cursor names and cursor types.
        char const* names[rose_output_cursor_type_count_] = {
            // rose_output_cursor_type_unspecified
            "left_ptr",
            // rose_output_cursor_type_default
            "left_ptr",
            // rose_output_cursor_type_moving
            "move",
            // rose_output_cursor_type_resizing_north
            "sb_v_double_arrow",
            // rose_output_cursor_type_resizing_south
            "sb_v_double_arrow",
            // rose_output_cursor_type_resizing_east
            "sb_h_double_arrow",
            // rose_output_cursor_type_resizing_west
            "sb_h_double_arrow",
            // rose_output_cursor_type_resizing_north_east
            "fd_double_arrow",
            // rose_output_cursor_type_resizing_north_west
            "bd_double_arrow",
            // rose_output_cursor_type_resizing_south_east
            "bd_double_arrow",
            // rose_output_cursor_type_resizing_south_west
            "fd_double_arrow",
            // rose_output_cursor_type_client
            "left_ptr"};

        // Obtain images for all output cursor types.
        for(ptrdiff_t i = 0; i < rose_output_cursor_type_count_; ++i) {
            // Obtain an image from the theme.
            struct wlr_xcursor_image* image = NULL;
            if(true) {
                // Obtain a cursor from the theme.
                struct wlr_xcursor* cursor = wlr_xcursor_manager_get_xcursor(
                    context->cursor_context.manager, names[i], 1.0f);

                if(cursor == NULL) {
                    try_(
                        cursor = wlr_xcursor_manager_get_xcursor(
                            context->cursor_context.manager, names[0], 1.0f));
                }

                // Obtain the first image from the cursor.
                image = cursor->images[0];
            }

            // Initialize a new raster.
            struct rose_raster* raster = rose_raster_initialize_without_texture(
                image->width, image->height);

            // Make sure initialization succeeded.
            try_(raster);

            // Copy image to the raster.
            memcpy(
                raster->pixels, image->buffer,
                raster->base.width * raster->base.height * 4U);

            // Save the cursor image to the context.
            context->cursor_context.images[i] = (struct rose_cursor_image){
                .raster = raster,
                .hotspot_x = image->hotspot_x,
                .hotspot_y = image->hotspot_y};
        }
    }

#define add_signal_(x, ns, f) \
    wl_signal_add(&((x)->events.f), &(context->listener_##ns##_##f))

    // Initialize the backend.
    try_(
        context->backend =
            wlr_backend_autocreate(context->display, &(context->session)));

    add_signal_(context->backend, backend, new_input);
    add_signal_(context->backend, backend, new_output);

    // Initialize the renderer.
    try_(context->renderer = wlr_renderer_autocreate(context->backend));
    if(!wlr_renderer_init_wl_display(context->renderer, context->display)) {
        return false;
    }

    // Initialize the allocator.
    try_(
        context->allocator =
            wlr_allocator_autocreate(context->backend, context->renderer));

    // Initialize the compositor.
    try_(wlr_compositor_create(context->display, 5, context->renderer));
    try_(wlr_subcompositor_create(context->display));

    // Initialize the seat.
    try_(context->seat = wlr_seat_create(context->display, "seat0"));
    add_signal_(context->seat, seat, request_set_cursor);
    add_signal_(context->seat, seat, request_set_selection);
    add_signal_(context->seat, seat, request_set_primary_selection);
    add_signal_(context->seat, seat, request_start_drag);
    add_signal_(context->seat, seat, start_drag);

    // Initialize Wayland protocols: relative-pointer.
    try_(
        context->relative_pointer_manager =
            wlr_relative_pointer_manager_v1_create(context->display));

    // Initialize Wayland protocols: pointer-constraints.
    try_(
        context->pointer_constraints =
            wlr_pointer_constraints_v1_create(context->display));

    add_signal_(
        context->pointer_constraints, pointer_constraints, new_constraint);

    // Initialize Wayland protocols: tablet.
    try_(context->tablet_manager = wlr_tablet_v2_create(context->display));

    // Initialize Wayland protocols: presentation-time.
    try_(
        context->presentation =
            wlr_presentation_create(context->display, context->backend));

    // Initialize Wayland protocols: data device, primary selection.
    try_(wlr_data_device_manager_create(context->display));
    try_(wlr_primary_selection_v1_device_manager_create(context->display));

    // Initialize Wayland protocols: viewporter.
    try_(wlr_viewporter_create(context->display));

    // Initialize Wayland protocols: xdg-shell, xdg-decoration-manager.
    if(true) {
        struct wlr_xdg_shell* xdg_shell =
            wlr_xdg_shell_create(context->display, 5);

        struct wlr_xdg_decoration_manager_v1* xdg_decoration_manager =
            wlr_xdg_decoration_manager_v1_create(context->display);

        if((xdg_shell == NULL) || (xdg_decoration_manager == NULL)) {
            return false;
        }

        add_signal_(xdg_shell, xdg, new_surface);
        add_signal_(xdg_decoration_manager, xdg, new_toplevel_decoration);
    }

#undef add_signal_

    // Initialize Wayland protocols: server-decoration.
    if(true) {
        struct wlr_server_decoration_manager* server_decoration_manager =
            wlr_server_decoration_manager_create(context->display);

        if(server_decoration_manager != NULL) {
            wlr_server_decoration_manager_set_default_mode(
                server_decoration_manager,
                WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
        }
    }

    // Initialize privileged Wayland protocols.
    // Note: Screen capture-related protocols are potentially unsafe. Granting
    // unlimited access to these protocols can threaten user's security.
    try_(wlr_screencopy_manager_v1_create(context->display));
    try_(wlr_export_dmabuf_manager_v1_create(context->display));

    // Initialize workspaces.
    for_each_(struct rose_workspace, workspace, context->storage.workspace) {
        if(!rose_workspace_initialize(workspace, context)) {
            return false;
        } else {
            rose_workspace_set_panel(workspace, context->config.theme.panel);
        }
    }

    // Set the first workspace as current.
    context->current_workspace = context->storage.workspace;

    // Initialize IPC server and command list.
    try_(context->ipc_server = rose_ipc_server_initialize(context));
    try_(context->command_list = rose_command_list_initialize());

#undef try_

    // Add Wayland socket.
    char const* socket = wl_display_add_socket_auto(context->display);
    if(socket == NULL) {
        return false;
    }

    // Set environment variables.
    if(setenv("WAYLAND_DISPLAY", socket, 1) != 0) {
        return false;
    }

    // Ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // Start system processes.
    return rose_server_context_start_processes(context), true;
}

void
rose_server_context_destroy(struct rose_server_context* context) {
    // Destroy event sources.
    if(true) {
        struct wl_event_source* sources[] = {
            context->event_source_sigint,  //
            context->event_source_sigterm, //
            context->event_source_sigchld, //
            context->event_source_timer};

        for(size_t i = 0; i != array_size_(sources); ++i) {
            if(sources[i] != NULL) {
                wl_event_source_remove(sources[i]);
            }
        }
    }

#define kill_(type)                                    \
    if(context->processes.type##_pid != (pid_t)(-1)) { \
        kill(context->processes.type##_pid, SIGTERM);  \
    }

    // Kill system processes.
    kill_(background);
    kill_(dispatcher);
    kill_(notification_daemon);
    kill_(panel);
    kill_(screen_locker);

#undef kill_

    // Destroy input devices.
    if(true) {
        struct rose_input* input = NULL;
        struct rose_input* _ = NULL;

        wl_list_for_each_safe(input, _, &(context->inputs), link) {
            rose_input_destroy(input);
        }
    }

    // Destroy output devices.
    if(true) {
        struct rose_output* output = NULL;
        struct rose_output* _ = NULL;

        wl_list_for_each_safe(output, _, &(context->outputs), link) {
            rose_output_destroy(output);
        }
    }

    // Destroy workspaces.
    if(true) {
        struct rose_workspace* workspace = NULL;
        struct rose_workspace* _ = NULL;

        wl_list_for_each_safe(workspace, _, &(context->workspaces), link) {
            rose_workspace_destroy(workspace);
        }

        wl_list_for_each_safe(
            workspace, _, &(context->workspaces_without_output), link_output) {
            rose_workspace_destroy(workspace);
        }
    }

    // Destroy the display.
    if(context->display != NULL) {
        wl_display_destroy_clients(context->display);
        wl_display_destroy(context->display);
    }

    // Destroy the renderer.
    if(context->renderer != NULL) {
        wlr_renderer_destroy(context->renderer);
    }

    // Destroy the allocator.
    if(context->allocator != NULL) {
        wlr_allocator_destroy(context->allocator);
    }

    // Free memory.
    free(context->config.argument_lists.background.data);
    free(context->config.argument_lists.dispatcher.data);
    free(context->config.argument_lists.notification_daemon.data);
    free(context->config.argument_lists.panel.data);
    free(context->config.argument_lists.screen_locker.data);
    free(context->config.argument_lists.terminal.data);
    free(context->config.keyboard_layouts.data);

    for_each_(struct rose_utf8_string, path, context->config.paths) {
        free(path->data);
    }

    // Destroy keyboard control scheme.
    if(context->config.keyboard_control_scheme != NULL) {
        rose_keyboard_control_scheme_destroy(
            context->config.keyboard_control_scheme);
    }

    // Destroy text rendering context.
    if(context->text_rendering_context != NULL) {
        rose_text_rendering_context_destroy(context->text_rendering_context);
    }

    // Destroy keyboard context.
    if(context->keyboard_context != NULL) {
        rose_keyboard_context_destroy(context->keyboard_context);
    }

    // Destroy cursor manager.
    if(context->cursor_context.manager != NULL) {
        wlr_xcursor_manager_destroy(context->cursor_context.manager);
    }

    // Destroy cursor images.
    for_each_(struct rose_cursor_image, image, context->cursor_context.images) {
        rose_raster_destroy(image->raster);
    }

    // Destroy command list.
    if(context->command_list != NULL) {
        rose_command_list_destroy(context->command_list);
    }

    // Destroy device preference list.
    if(context->preference_list != NULL) {
        rose_device_preference_list_destroy(context->preference_list);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Configuration interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_set_keyboard_layout(
    struct rose_server_context* context, unsigned layout_index) {
    // If requested layout is invalid, then fail.
    if(layout_index >= context->keyboard_context->layout_count) {
        return false;
    }

    // Set keyboard layout.
    context->keyboard_context->layout_index = layout_index;
    xkb_layout_index_t group_index =
        (xkb_layout_index_t)(context->keyboard_context->layout_index);

    // Update layouts of all keyboard devices.
    struct rose_keyboard* keyboard = NULL;
    wl_list_for_each(keyboard, &(context->inputs_keyboards), link) {
        // Obtain underlying input device.
        struct wlr_keyboard* device =
            wlr_keyboard_from_input_device(keyboard->parent->device);

        // Obtain its modifiers.
        struct wlr_keyboard_modifiers modifiers = device->modifiers;

        // Update keyboard's modifiers.
        wlr_keyboard_notify_modifiers(
            device, modifiers.depressed, modifiers.latched, modifiers.locked,
            group_index);
    }

    // Broadcast the change through IPC, if needed.
    if(context->keyboard_context->layout_count > 1) {
        rose_ipc_server_broadcast_status(
            context->ipc_server, rose_server_context_obtain_status(context));
    }

    // Operation succeeded.
    return true;
}

void
rose_server_context_configure(
    struct rose_server_context* context,
    rose_server_context_configure_mask flags) {
    // If requested configuration is a no-op, then do nothing.
    if(flags == 0) {
        return;
    }

    // Configure keyboard control scheme, if requested.
    if((flags & rose_server_context_configure_keyboard_control_scheme) != 0) {
        // Initialize keyboard control scheme.
        struct rose_keyboard_control_scheme* keyboard_control_scheme = NULL;
        for_each_(struct rose_utf8_string, path, context->config.paths) {
            keyboard_control_scheme = rose_keyboard_control_scheme_initialize(
                rose_utf8_string_concat(path->data, "keyboard_control_scheme")
                    .data);

            if(keyboard_control_scheme != NULL) {
                // If new scheme has been successfully initialized, then destroy
                // previous scheme.
                if(context->config.keyboard_control_scheme != NULL) {
                    rose_keyboard_control_scheme_destroy(
                        context->config.keyboard_control_scheme);
                }

                // Set the new scheme.
                context->config.keyboard_control_scheme =
                    keyboard_control_scheme;

                // Broadcast the change through IPC.
                if(true) {
                    struct rose_ipc_status status = {
                        .type = rose_ipc_status_type_keyboard_control_scheme};

                    rose_ipc_server_broadcast_status(
                        context->ipc_server, status);
                }

                // And break out of the cycle.
                break;
            }
        }
    }

    // Configure keyboard layouts, if requested.
    if((flags & rose_server_context_configure_keyboard_layouts) != 0) {
        // Read keyboard layouts.
        struct rose_utf8_string keyboard_layouts = {};
        for_each_(struct rose_utf8_string, path, context->config.paths) {
            keyboard_layouts = rose_utf8_string_read(
                rose_utf8_string_concat(path->data, "keyboard_layouts").data);

            if(keyboard_layouts.data != NULL) {
                break;
            }
        }

        // Reload keyboard context, if needed.
        if(keyboard_layouts.data != NULL) {
            // If keyboard layouts have been successfully read, then try
            // initializing new keyboard context.
            struct rose_keyboard_context* keyboard_context =
                rose_keyboard_context_initialize(keyboard_layouts.data);

            if(keyboard_context != NULL) {
                // If initialization succeeded, then destroy previous keyboard
                // context and save the new one.
                rose_keyboard_context_destroy(context->keyboard_context);
                context->keyboard_context = keyboard_context;

                // Destroy previous keyboard layouts and save the new ones.
                free(context->config.keyboard_layouts.data);
                context->config.keyboard_layouts = keyboard_layouts;

                // Set new keymap for all keyboard devices.
                struct rose_keyboard* keyboard = NULL;
                wl_list_for_each(keyboard, &(context->inputs_keyboards), link) {
                    // Obtain underlying input device.
                    struct wlr_keyboard* device =
                        wlr_keyboard_from_input_device(
                            keyboard->parent->device);

                    // Set the keymap.
                    wlr_keyboard_set_keymap(
                        device, context->keyboard_context->keymap);
                }

                // Broadcast the change through IPC.
                if(true) {
                    struct rose_ipc_status status = {
                        .type = rose_ipc_status_type_keyboard_keymap};

                    rose_ipc_server_broadcast_status(
                        context->ipc_server, status);

                    rose_ipc_server_broadcast_status(
                        context->ipc_server,
                        rose_server_context_obtain_status(context));
                }
            } else {
                // Otherwise, free memory.
                free(keyboard_layouts.data);
            }
        }
    }

    // Configure the theme, if requested.
    if((flags & rose_server_context_configure_theme) != 0) {
        // Initialize default theme.
        context->config.theme = rose_theme_initialize_default();

        // Try reading the theme from one of the configuration files.
        for_each_(struct rose_utf8_string, path, context->config.paths) {
            if(rose_theme_initialize(
                   rose_utf8_string_concat(path->data, "theme").data,
                   &(context->config.theme))) {
                break;
            }
        }

        // Request redraw operation.
        struct rose_output* output;
        wl_list_for_each(output, &(context->outputs), link) {
            if(output->focused_workspace != NULL) {
                rose_workspace_request_redraw(output->focused_workspace);
            } else {
                rose_output_request_redraw(output);
            }
        }

        // Broadcast the change through IPC.
        if(true) {
            struct rose_ipc_status status = {
                .type = rose_ipc_status_type_theme};

            rose_ipc_server_broadcast_status(context->ipc_server, status);
        }
    }

    // Lock the screen, if requested.
    if((flags & rose_server_context_configure_screen_lock) != 0) {
        if(!(context->is_screen_locked)) {
            // Set the flag.
            context->is_screen_locked = true;

            // Update input focus.
            rose_workspace_make_current(context->current_workspace);

            // Request redraw operation.
            struct rose_output* output;
            wl_list_for_each(output, &(context->outputs), link) {
                if(output->focused_workspace != NULL) {
                    rose_workspace_request_redraw(output->focused_workspace);
                } else {
                    rose_output_request_redraw(output);
                }
            }

            // Broadcast the change through IPC.
            rose_ipc_server_broadcast_status(
                context->ipc_server,
                rose_server_context_obtain_status(context));
        }
    }

    // Unlock the screen, if requested.
    if((flags & rose_server_context_configure_screen_unlock) != 0) {
        if(context->is_screen_locked) {
            // Clear the flag.
            context->is_screen_locked = false;

            // Update input focus.
            rose_workspace_make_current(context->current_workspace);

            // Request redraw operation.
            struct rose_output* output;
            wl_list_for_each(output, &(context->outputs), link) {
                if(output->focused_workspace != NULL) {
                    rose_workspace_request_redraw(output->focused_workspace);
                } else {
                    rose_output_request_redraw(output);
                }
            }

            // Broadcast the change through IPC.
            rose_ipc_server_broadcast_status(
                context->ipc_server,
                rose_server_context_obtain_status(context));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Cursor image acquisition interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_cursor_image
rose_server_context_get_cursor_image(
    struct rose_server_context* context, enum rose_output_cursor_type type,
    float scale) {
    // Note: Scaling factor is not used.
    unused_(scale);

    // Make sure the requested cursor type exists.
    if((type < 0) || (type >= rose_output_cursor_type_count_)) {
        type = rose_output_cursor_type_default;
    }

    // Return cursor's image.
    return context->cursor_context.images[(ptrdiff_t)(type)];
}

////////////////////////////////////////////////////////////////////////////////
// Device acquisition interface implementation.
////////////////////////////////////////////////////////////////////////////////

#define define_search_(type)                              \
    struct rose_##type* device = NULL;                    \
    wl_list_for_each(device, &(context->type##s), link) { \
        if(device->id == id) {                            \
            break;                                        \
        }                                                 \
    }                                                     \
                                                          \
    return device;

struct rose_input*
rose_server_context_obtain_input(
    struct rose_server_context* context, unsigned id) {
    define_search_(input);
}

struct rose_output*
rose_server_context_obtain_output(
    struct rose_server_context* context, unsigned id) {
    define_search_(output);
}

#undef define_search_

////////////////////////////////////////////////////////////////////////////////
// State query interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_status
rose_server_context_obtain_status(struct rose_server_context* context) {
    struct rose_ipc_status status = {
        .type = rose_ipc_status_type_server_state,
        .server_state = {
            context->is_screen_locked,
            context->are_keyboard_shortcuts_inhibited,
            context->keyboard_context->layout_index}};

    return status;
}

struct rose_server_context_state
rose_server_context_state_obtain(struct rose_server_context* context) {
    // Initialize an empty state object.
    struct rose_server_context_state state = {};

    // Compute the number of input devices.
    if(!wl_list_empty(&(context->inputs))) {
        struct rose_input* input =
            wl_container_of(context->inputs.next, input, link);

        state.input_device_count = input->id + 1;
    }

    // Compute the number of output devices.
    if(!wl_list_empty(&(context->outputs))) {
        struct rose_output* output =
            wl_container_of(context->outputs.next, output, link);

        state.output_device_count = output->id + 1;
    }

    // Return server context's state.
    return state;
}

////////////////////////////////////////////////////////////////////////////////
// IPC access rights checking interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_check_ipc_access_rights(
    struct rose_server_context* context, pid_t pid,
    enum rose_ipc_connection_type connection_type) {
    // System processes have full IPC access.
    if((pid == context->processes.screen_locker_pid) ||
       (pid == context->processes.dispatcher_pid) ||
       (pid == context->processes.panel_pid)) {
        return true;
    }

    // Determine access rights depending on IPC connection's type and client's
    // PID.
    switch(connection_type) {
        case rose_ipc_connection_type_configurator:
            // fall-through
        case rose_ipc_connection_type_dispatcher:
            for(int i = 0; i != 3; ++i) {
                if((rose_command_list_query_access_rights(
                        context->command_list, pid) &
                    rose_command_access_ipc) != 0) {
                    return true;
                }

                if((pid = rose_obtain_parent_pid(pid)) == (pid_t)(-1)) {
                    return false;
                }
            }

            break;

        case rose_ipc_connection_type_status:
            // Any process can access the status info.
            return true;

        default:
            break;
    }

    return false;
}
