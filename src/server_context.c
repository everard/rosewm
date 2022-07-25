// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_tablet_v2.h>

#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_data_device.h>

#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/interfaces/wlr_keyboard.h>

#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define array_size_(x) ((size_t)(sizeof(x) / sizeof(x[0])))
#define for_each_(type, x, array)                                \
    for(type* x = array, *sentinel = array + array_size_(array); \
        x != sentinel; ++x)

#define unused_(x) ((void)(x))
#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

////////////////////////////////////////////////////////////////////////////////
// Constants.
////////////////////////////////////////////////////////////////////////////////

enum {
    rose_configuration_file_size_max = 16384,
    rose_utf8_string_size_max = 4096
};

////////////////////////////////////////////////////////////////////////////////
// Theme definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_theme {
    int font_size;
    struct rose_ui_panel panel;
    struct rose_color_scheme color_scheme;
};

static struct rose_theme const rose_default_theme = {
    .font_size = 16,
    .panel = {.position = rose_ui_panel_position_top,
              .size = 40,
              .is_visible = true},
    .color_scheme = {.panel_background = {{0.15f, 0.15f, 0.15f, 1.0f}},
                     .panel_foreground = {{1.0f, 1.0f, 1.0f, 1.0f}},
                     .menu_background = {{0.13f, 0.13f, 0.13f, 1.0f}},
                     .menu_foreground = {{1.0f, 1.0f, 1.0f, 1.0f}},
                     .menu_highlight0 = {{0.23f, 0.1f, 0.1f, 1.0f}},
                     .menu_highlight1 = {{0.33f, 0.1f, 0.1f, 1.0f}},
                     .surface_background0 = {{0.8f, 0.8f, 0.8f, 1.0f}},
                     .surface_background1 = {{0.6f, 0.6f, 0.6f, 1.0f}},
                     .surface_resizing_background0 = {{0.8f, 0.8f, 0.8f, 0.5f}},
                     .surface_resizing_background1 = {{0.6f, 0.6f, 0.6f, 0.5f}},
                     .surface_resizing = {{0.1f, 0.1f, 0.1f, 0.5f}},
                     .workspace_background = {{0.2f, 0.2f, 0.2f, 1.0f}}}};

////////////////////////////////////////////////////////////////////////////////
// String buffer definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_utf8_string_buffer {
    char data[rose_utf8_string_size_max + 1];
};

////////////////////////////////////////////////////////////////////////////////
// String utility functions.
////////////////////////////////////////////////////////////////////////////////

static struct rose_utf8_string_buffer
rose_utf8_string_concat(char* string0, char* string1) {
    // Zero-initialize a new string buffer.
    struct rose_utf8_string_buffer buffer = {};

    // Append strings to the buffer.
    strncat(buffer.data, string0, rose_utf8_string_size_max);
    strncat(
        buffer.data, string1, rose_utf8_string_size_max - strlen(buffer.data));

    // Return the buffer.
    return buffer;
}

////////////////////////////////////////////////////////////////////////////////
// IO utility functions.
////////////////////////////////////////////////////////////////////////////////

static size_t
rose_obtain_configuration_file_size(char const* file_name) {
    struct stat file_stat = {};
    if(stat(file_name, &file_stat) != 0) {
        return 0;
    }

    if((file_stat.st_size < 1) ||
       (file_stat.st_size > rose_configuration_file_size_max)) {
        return 0;
    }

    return (size_t)(file_stat.st_size);
}

static struct rose_text_rendering_context_parameters
rose_text_rendering_context_parameters_read(char const* file_name) {
    // Initialize an empty object.
    struct rose_text_rendering_context_parameters params = {};

    // Read font paths from the file, if needed.
    if(file_name != NULL) {
        // Obtain file's size.
        size_t file_size = rose_obtain_configuration_file_size(file_name);
        if(file_size == 0) {
            goto end;
        }

        // Open the file with the given name.
        FILE* file = fopen(file_name, "rb");
        if(file == NULL) {
            goto end;
        }

        // Read the data.
        char data[rose_configuration_file_size_max] = {};
        if(fread(data, file_size, 1, file) != 1) {
            fclose(file);
            goto end;
        }

        // Close the file.
        fclose(file);

        // Ensure that the file has proper format.
        if((data[0] == '\n') || (data[file_size - 1] != '\n')) {
            goto end;
        }

        // Count the number of font paths and ensure zero-termination.
        size_t n_fonts = 0;
        for(size_t i = 0; i != file_size; ++i) {
            if(data[i] == '\n') {
                data[i] = '\0';
                ++n_fonts;
            }
        }

        // Compute the size of the font list in bytes.
        size_t font_list_size = n_fonts * sizeof(char*);
        if((font_list_size / n_fonts) != sizeof(char*)) {
            goto end;
        }

        size_t font_list_data_offset = font_list_size;
        if((font_list_size += file_size) < file_size) {
            goto end;
        }

        // Allocate memory for the font list.
        char** font_list = malloc(font_list_size);
        if(font_list == NULL) {
            goto end;
        }

        params.font_names = (char const**)(font_list);
        params.n_fonts = n_fonts;

        // Initialize the list.
        memcpy((char*)(font_list) + font_list_data_offset, data, file_size);
        for(size_t i = 0; i != n_fonts; ++i) {
            font_list[i] = NULL;
        }

        for(size_t i = 0, j = 0, k = 0; i != file_size; ++i) {
            if(data[i] == '\0') {
                font_list[j++] = (char*)(font_list) + font_list_data_offset + k;
                k = i + 1;
            }
        }
    }

end:
    return params;
}

static char**
rose_argument_list_read(char const* file_name) {
    // Initialize an empty argument list.
    char** arg_list = NULL;

    // Read an argument list from the file, if needed.
    if(file_name != NULL) {
        // Obtain file's size.
        size_t file_size = rose_obtain_configuration_file_size(file_name);
        if(file_size == 0) {
            goto end;
        }

        // Open the file with the given name.
        FILE* file = fopen(file_name, "rb");
        if(file == NULL) {
            goto end;
        }

        // Read the data.
        char data[rose_configuration_file_size_max] = {};
        if(fread(data, file_size, 1, file) != 1) {
            fclose(file);
            goto end;
        }

        // Close the file.
        fclose(file);

        // Ensure that the file has proper format.
        if((data[0] == '\0') || (data[file_size - 1] != '\0')) {
            goto end;
        }

        // Count the number of arguments.
        size_t n_args = 1;
        for(size_t i = 0; i != file_size; ++i) {
            if(data[i] == '\0') {
                ++n_args;
            }
        }

        // Compute the size of the argument list in bytes.
        size_t arg_list_size = n_args * sizeof(char*);
        if((arg_list_size / n_args) != sizeof(char*)) {
            goto end;
        }

        size_t arg_list_data_offset = arg_list_size;
        if((arg_list_size += file_size) < file_size) {
            goto end;
        }

        // Allocate memory for the argument list.
        arg_list = malloc(arg_list_size);
        if(arg_list == NULL) {
            goto end;
        }

        // Initialize the argument list.
        memcpy((char*)(arg_list) + arg_list_data_offset, data, file_size);
        for(size_t i = 0; i != n_args; ++i) {
            arg_list[i] = NULL;
        }

        for(size_t i = 0, j = 0, k = 0; i != file_size; ++i) {
            if(data[i] == '\0') {
                arg_list[j++] = (char*)(arg_list) + arg_list_data_offset + k;
                k = i + 1;
            }
        }
    }

end:
    return arg_list;
}

static struct rose_utf8_string
rose_utf8_string_read(char const* file_name) {
    // Initialize an empty string.
    struct rose_utf8_string string = {};

    // Read the string from the file, if needed.
    if(file_name != NULL) {
        // Obtain file's size.
        size_t file_size = rose_obtain_configuration_file_size(file_name);
        if((file_size == 0) || (file_size > rose_utf8_string_size_max)) {
            goto end;
        }

        // Open the file with the given name.
        FILE* file = fopen(file_name, "rb");
        if(file == NULL) {
            goto end;
        }

        // Allocate memory for the string.
        char* file_data = calloc(1, file_size + 1);
        if(file_data == NULL) {
            fclose(file);
            goto end;
        }

        // Read the string from the file.
        if(fread(file_data, file_size, 1, file) != 1) {
            free(file_data);
            fclose(file);
            goto end;
        }

        // Set string's data and size.
        string.data = file_data;
        string.size = file_size;

        // Close the file.
        fclose(file);
    }

end:
    return string;
}

static bool
rose_theme_read(char const* file_name, struct rose_theme* dst) {
    // If the file is not specified, then read fails.
    if(file_name == NULL) {
        return false;
    }

    // Open the file with the given name.
    FILE* file = fopen(file_name, "rb");
    if(file == NULL) {
        return false;
    }

    // Initialize a theme.
    struct rose_theme theme = rose_default_theme;

    // Read font size.
    theme.font_size = fgetc(file);
    theme.font_size = clamp_(theme.font_size, 1, 144);

    // Read panel's position.
    theme.panel.position = fgetc(file);
    switch(theme.panel.position) {
        case rose_ui_panel_position_top:
            // fall-through
        case rose_ui_panel_position_bottom:
            // fall-through
        case rose_ui_panel_position_left:
            // fall-through
        case rose_ui_panel_position_right:
            break;

        default:
            goto error;
    }

    // Read panel's size.
    theme.panel.size = fgetc(file);
    theme.panel.size = clamp_(theme.panel.size, 1, 128);

#define read_color_(c)                                \
    if(true) {                                        \
        unsigned char data[4] = {};                   \
        if(fread(data, sizeof(data), 1, file) != 1) { \
            goto error;                               \
        }                                             \
                                                      \
        for(size_t i = 0; i != sizeof(data); ++i) {   \
            if(data[i] == 0) {                        \
                c.v[i] = 0.0f;                        \
            } else if(data[i] >= 255) {               \
                c.v[i] = 1.0f;                        \
            } else {                                  \
                c.v[i] = (data[i] / 255.0f);          \
                c.v[i] = clamp_(c.v[i], 0.0f, 1.0f);  \
            }                                         \
        }                                             \
    }

    // Read color scheme.
    read_color_(theme.color_scheme.panel_background);
    read_color_(theme.color_scheme.panel_foreground);
    read_color_(theme.color_scheme.panel_highlight);
    read_color_(theme.color_scheme.menu_background);
    read_color_(theme.color_scheme.menu_foreground);
    read_color_(theme.color_scheme.menu_highlight0);
    read_color_(theme.color_scheme.menu_highlight1);
    read_color_(theme.color_scheme.surface_background0);
    read_color_(theme.color_scheme.surface_background1);
    read_color_(theme.color_scheme.surface_resizing_background0);
    read_color_(theme.color_scheme.surface_resizing_background1);
    read_color_(theme.color_scheme.surface_resizing);
    read_color_(theme.color_scheme.workspace_background);

#undef read_color_

    // Close the file.
    fclose(file);

    // Write the theme.
    return (*dst = theme), true;

error:
    return fclose(file), false;
}

////////////////////////////////////////////////////////////////////////////////
// Process starting utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_server_context_start_processes(struct rose_server_context* ctx) {
#define start_(type)                                                       \
    if((ctx->processes.type##_pid == (pid_t)(-1)) &&                       \
       (ctx->config.type##_arg_list != NULL)) {                            \
        ctx->processes.type##_pid = rose_execute_command_in_child_process( \
            ctx->config.type##_arg_list);                                  \
    }

    start_(background);
    start_(dispatcher);
    start_(notification_daemon);
    start_(panel);
    start_(screen_locker);

#undef start_
}

////////////////////////////////////////////////////////////////////////////////
// Filter utility function. Prevents non-privileged clients from accessing
// privileged protocols.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_filter_global(struct wl_client const* client,
                   struct wl_global const* global, void* data) {
    // Obtain a pointer to the server context.
    struct rose_server_context* ctx = data;

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
        return ((rose_command_list_query_access_rights(
                     ctx->command_list, client_pid) &
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
    // Obtain a pointer to the server context.
    struct rose_server_context* ctx = data;

    // If the signal is SIGINT or SIGTERM, then stop the compositor.
    if((signal_number == SIGINT) || (signal_number == SIGTERM)) {
        wl_display_terminate(ctx->display);
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
                ctx->command_list, child_pid);

#define check_(name)                                                     \
    if(child_pid == ctx->processes.name##_pid) {                         \
        /* Reset the PID of the process. */                              \
        ctx->processes.name##_pid = (pid_t)(-1);                         \
                                                                         \
        /* Arm the timer which will try to restart system processes. */  \
        if(!(ctx->is_timer_armed)) {                                     \
            ctx->is_timer_armed = true;                                  \
            wl_event_source_timer_update(ctx->event_source_timer, 1000); \
        }                                                                \
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
    // Obtain a pointer to the server context.
    struct rose_server_context* ctx =
        wl_container_of(listener, ctx, listener_backend_new_input);

    // Initialize a new input device.
    rose_input_initialize(ctx, data);
}

static void
rose_handle_event_backend_new_output(struct wl_listener* listener, void* data) {
    // Obtain a pointer to the server context.
    struct rose_server_context* ctx =
        wl_container_of(listener, ctx, listener_backend_new_output);

    // Initialize a new output device.
    rose_output_initialize(ctx, data);
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers: seat.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_seat_request_set_cursor(struct wl_listener* listener,
                                          void* data) {
    // Obtain a pointer to the server context.
    struct rose_server_context* ctx =
        wl_container_of(listener, ctx, listener_seat_request_set_cursor);

    // Obtain a pointer to the event.
    struct wlr_seat_pointer_request_set_cursor_event* event = data;

    // Obtain a pointer to the current output.
    struct rose_output* output = ctx->current_workspace->output;

    // If such output exists, then set its cursor.
    if(output != NULL) {
        rose_output_cursor_client_surface_set(
            output, event->surface, event->hotspot_x, event->hotspot_y);

        rose_output_cursor_set(output, rose_output_cursor_type_unspecified);
        rose_output_cursor_set(output, rose_output_cursor_type_client);
    }
}

static void
rose_handle_event_seat_request_set_selection(struct wl_listener* listener,
                                             void* data) {
    // Obtain a pointer to the server context.
    struct rose_server_context* ctx =
        wl_container_of(listener, ctx, listener_seat_request_set_selection);

    // Obtain a pointer to the event.
    struct wlr_seat_request_set_selection_event* event = data;

    // Satisfy the request.
    wlr_seat_set_selection(ctx->seat, event->source, event->serial);
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers: Wayland protocols.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_xdg_new_surface(struct wl_listener* listener, void* data) {
    // Obtain a pointer to the server context.
    struct rose_server_context* ctx =
        wl_container_of(listener, ctx, listener_xdg_new_surface);

    // Obtain a pointer to the XDG surface.
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
    } else if(client_pid == ctx->processes.notification_daemon_pid) {
        // Find a suitable output for this notification surface.
        struct rose_output* output =
            ((ctx->current_workspace->output != NULL)
                 ? ctx->current_workspace->output
                 : (wl_list_empty(&(ctx->outputs))
                        ? NULL
                        : wl_container_of(ctx->outputs.next, output, link)));

        if(output != NULL) {
            // If such output exists, then initialize the widget.
            rose_output_widget_initialize(&(output->ui), xdg_surface->toplevel,
                                          rose_output_widget_type_notification);

            // And do nothing else.
            return;
        }
    } else if(client_pid == ctx->processes.screen_locker_pid) {
        // Add the surface as a screen lock widget to an output which does not
        // contain any screen lock widgets.
        struct rose_output* output = NULL;
        wl_list_for_each(output, &(ctx->outputs), link) {
            // Skip any outputs which already contain screen lock widgets.
            if(!wl_list_empty(&(
                   output->ui.widgets[rose_output_widget_type_screen_lock]))) {
                continue;
            }

            // Initialize the widget.
            rose_output_widget_initialize( //
                &(output->ui), xdg_surface->toplevel,
                rose_output_widget_type_screen_lock);

            // Do nothing else.
            return;
        }
    } else if(client_pid == ctx->processes.background_pid) {
        // Add the surface as a background widget to an output which does not
        // contain any background widgets.
        struct rose_output* output = NULL;
        wl_list_for_each(output, &(ctx->outputs), link) {
            // Skip any outputs which already contain background widgets.
            if(!wl_list_empty(
                   &(output->ui.widgets[rose_output_widget_type_background]))) {
                continue;
            }

            // Initialize the widget.
            rose_output_widget_initialize( //
                &(output->ui), xdg_surface->toplevel,
                rose_output_widget_type_background);

            // Do nothing else.
            return;
        }
    } else if(client_pid == ctx->processes.dispatcher_pid) {
        if(ctx->current_workspace->output != NULL) {
            // Initialize the widget.
            rose_output_widget_initialize( //
                &(ctx->current_workspace->output->ui), xdg_surface->toplevel,
                rose_output_widget_type_prompt);

            // And do nothing else.
            return;
        }
    } else if(client_pid == ctx->processes.panel_pid) {
        // Add the surface as a panel widget to an output which does not contain
        // any panel widgets.
        struct rose_output* output = NULL;
        wl_list_for_each(output, &(ctx->outputs), link) {
            // Skip any outputs which already contain panel widgets.
            if(!wl_list_empty(
                   &(output->ui.widgets[rose_output_widget_type_panel]))) {
                continue;
            }

            // Initialize the widget.
            rose_output_widget_initialize( //
                &(output->ui), xdg_surface->toplevel,
                rose_output_widget_type_panel);

            // Do nothing else.
            return;
        }
    }

    // If the surface is not a widget, then construct surface parameters.
    struct rose_surface_parameters params = {
        .workspace = ctx->current_workspace,
        .toplevel = xdg_surface->toplevel,
        .pointer_constraint = wlr_pointer_constraints_v1_constraint_for_surface(
            ctx->pointer_constraints, xdg_surface->surface, ctx->seat)};

    // And initialize a normal top-level surface.
    rose_surface_initialize(params);
}

static void
rose_handle_event_xdg_new_toplevel_decoration(struct wl_listener* listener,
                                              void* data) {
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
    // Obtain a pointer to the server context.
    struct rose_server_context* ctx = data;

    // Clear timer's flag.
    ctx->is_timer_armed = false;

    // Restart system processes.
    return rose_server_context_start_processes(ctx), 0;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_initialize(struct rose_server_context* ctx) {
    // Initialize the server context object.
    *ctx = (struct rose_server_context){
        .processes = {.background_pid = (pid_t)(-1),
                      .dispatcher_pid = (pid_t)(-1),
                      .notification_daemon_pid = (pid_t)(-1),
                      .panel_pid = (pid_t)(-1),
                      .screen_locker_pid = (pid_t)(-1)}};

    // Initialize lists.
    wl_list_init(&(ctx->menus_visible));
    wl_list_init(&(ctx->workspaces));
    wl_list_init(&(ctx->workspaces_without_output));

    wl_list_init(&(ctx->inputs));
    wl_list_init(&(ctx->inputs_keyboards));
    wl_list_init(&(ctx->outputs));

#define initialize_(f) ctx->listener_##f.notify = rose_handle_event_##f;

    // Initialize event listeners.
    initialize_(backend_new_input);
    initialize_(backend_new_output);

    initialize_(seat_request_set_cursor);
    initialize_(seat_request_set_selection);

    initialize_(xdg_new_surface);
    initialize_(xdg_new_toplevel_decoration);
    initialize_(pointer_constraints_new_constraint);

#undef initialize_

    // Allocate memory for configuration paths.
    for_each_(struct rose_utf8_string, path, ctx->config.paths) {
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
    strcpy(ctx->config.paths[0].data,
           rose_utf8_string_concat(home_path, "/.config/rosewm/").data);

    strcpy(ctx->config.paths[1].data,
           rose_utf8_string_concat("", "/etc/rosewm/").data);

    // Read the theme.
    if(true) {
        // Initialize default theme.
        struct rose_theme theme = rose_default_theme;

        // Try reading the theme from one of the configuration files.
        for_each_(struct rose_utf8_string, path, ctx->config.paths) {
            if(rose_theme_read(
                   rose_utf8_string_concat(path->data, "theme").data, &theme)) {
                break;
            }
        }

        // Save the theme.
        ctx->config.font_size = theme.font_size;
        ctx->config.panel = theme.panel;
        ctx->config.color_scheme = theme.color_scheme;
    }

    // Read keyboard layouts.
    for_each_(struct rose_utf8_string, path, ctx->config.paths) {
        ctx->config.keyboard_layouts = rose_utf8_string_read(
            rose_utf8_string_concat(path->data, "keyboard_layouts").data);

        if(ctx->config.keyboard_layouts.data != NULL) {
            break;
        }
    }

#define read_arg_list_(type)                                            \
    for_each_(struct rose_utf8_string, path, ctx->config.paths) {       \
        ctx->config.type##_arg_list = rose_argument_list_read(          \
            rose_utf8_string_concat(path->data, "system_" #type).data); \
                                                                        \
        if(ctx->config.type##_arg_list != NULL) {                       \
            break;                                                      \
        }                                                               \
    }

    // Read argument lists for system processes.
    read_arg_list_(background);
    read_arg_list_(dispatcher);
    read_arg_list_(notification_daemon);
    read_arg_list_(panel);
    read_arg_list_(screen_locker);
    read_arg_list_(terminal);

#undef read_arg_list_

    // Note: There must always be an argument list to start a terminal emulator.
    if(ctx->config.terminal_arg_list == NULL) {
        return false;
    }

#define try_(expression)       \
    if((expression) == NULL) { \
        return false;          \
    }

    // Initialize device preference list.
    if(true) {
        // Construct a full path to a file which contains device preferences.
        struct rose_utf8_string_buffer file_path = rose_utf8_string_concat(
            ctx->config.paths[0].data, "device_preferences");

        // Try initializing device preference list.
        try_(ctx->preference_list =
                 rose_device_preference_list_initialize(file_path.data));
    }

    // Initialize keyboard control scheme.
    for_each_(struct rose_utf8_string, path, ctx->config.paths) {
        ctx->config.keyboard_control_scheme =
            rose_keyboard_control_scheme_initialize(
                rose_utf8_string_concat(path->data, "keyboard_control_scheme")
                    .data);

        if(ctx->config.keyboard_control_scheme != NULL) {
            break;
        }
    }

    if(ctx->config.keyboard_control_scheme == NULL) {
        try_(ctx->config.keyboard_control_scheme =
                 rose_keyboard_control_scheme_initialize(NULL));
    }

    // Initialize text rendering context.
    for_each_(struct rose_utf8_string, path, ctx->config.paths) {
        // Read initialization parameters.
        struct rose_text_rendering_context_parameters params =
            rose_text_rendering_context_parameters_read(
                rose_utf8_string_concat(path->data, "fonts").data);

        // Initialize text rendering context, if needed.
        if(params.n_fonts != 0) {
            ctx->text_rendering_ctx =
                rose_text_rendering_context_initialize(params);

            // Free memory.
            free(params.font_names);

            // If initialization succeeded, then break out of the cycle.
            if(ctx->text_rendering_ctx != NULL) {
                break;
            }
        }
    }

    if(ctx->text_rendering_ctx == NULL) {
        return false;
    }

    // Initialize keyboard context.
    try_(ctx->keyboard_ctx = rose_keyboard_context_initialize(
             ctx->config.keyboard_layouts.data));

    // Create a display object and obtain its event loop.
    try_(ctx->display = wl_display_create());
    try_(ctx->event_loop = wl_display_get_event_loop(ctx->display));

    // Set filtering function which will prevent non-privileged clients from
    // accessing privileged protocols.
    wl_display_set_global_filter(ctx->display, rose_filter_global, ctx);

    // Configure event loop's file descriptor.
    if(true) {
        // Obtain event loop's file descriptor.
        int event_loop_fd = wl_event_loop_get_fd(ctx->event_loop);

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

    // Create event sources for SIGINT, SIGTERM and SIGCHLD.
    try_(ctx->event_source_sigint = wl_event_loop_add_signal(
             ctx->event_loop, SIGINT, rose_handle_signal, ctx));

    try_(ctx->event_source_sigterm = wl_event_loop_add_signal(
             ctx->event_loop, SIGTERM, rose_handle_signal, ctx));

    try_(ctx->event_source_sigchld = wl_event_loop_add_signal(
             ctx->event_loop, SIGCHLD, rose_handle_signal, ctx));

    // Create event source for timer.
    try_(ctx->event_source_timer = wl_event_loop_add_timer(
             ctx->event_loop, rose_handle_event_server_context_timer_expiry,
             ctx));

    // Initialize cursor context.
    if(true) {
        // Create cursor manager.
        try_(ctx->cursor_ctx.manager = wlr_xcursor_manager_create(NULL, 24));

        // Load cursor theme.
        if(!wlr_xcursor_manager_load(ctx->cursor_ctx.manager, 1.0f)) {
            return false;
        }

        // Define a mapping between cursor names and cursor types.
        char const* names[rose_n_output_cursor_types] = {
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
        for(ptrdiff_t i = 0; i < rose_n_output_cursor_types; ++i) {
            ctx->cursor_ctx.cursors[i] = wlr_xcursor_manager_get_xcursor(
                ctx->cursor_ctx.manager, names[i], 1.0f);

            if(ctx->cursor_ctx.cursors[i] == NULL) {
                try_(ctx->cursor_ctx.cursors[i] =
                         wlr_xcursor_manager_get_xcursor(
                             ctx->cursor_ctx.manager, names[0], 1.0f));
            }
        }
    }

#define add_signal_(x, ns, f) \
    wl_signal_add(&((x)->events.f), &(ctx->listener_##ns##_##f))

    // Initialize the backend.
    try_(ctx->backend = wlr_backend_autocreate(ctx->display));
    add_signal_(ctx->backend, backend, new_input);
    add_signal_(ctx->backend, backend, new_output);

    // Initialize the renderer.
    try_(ctx->renderer = wlr_renderer_autocreate(ctx->backend));
    if(!wlr_renderer_init_wl_display(ctx->renderer, ctx->display)) {
        return false;
    }

    // Initialize the allocator.
    try_(ctx->allocator =
             wlr_allocator_autocreate(ctx->backend, ctx->renderer));

    // Initialize the compositor.
    try_(wlr_compositor_create(ctx->display, ctx->renderer));

    // Initialize the seat.
    try_(ctx->seat = wlr_seat_create(ctx->display, "seat0"));
    add_signal_(ctx->seat, seat, request_set_cursor);
    add_signal_(ctx->seat, seat, request_set_selection);

    // Initialize Wayland protocols: relative-pointer.
    try_(ctx->relative_pointer_manager =
             wlr_relative_pointer_manager_v1_create(ctx->display));

    // Initialize Wayland protocols: pointer-constraints.
    try_(ctx->pointer_constraints =
             wlr_pointer_constraints_v1_create(ctx->display));
    add_signal_(ctx->pointer_constraints, pointer_constraints, new_constraint);

    // Initialize Wayland protocols: tablet.
    try_(ctx->tablet_manager = wlr_tablet_v2_create(ctx->display));

    // Initialize Wayland protocols: presentation-time.
    try_(ctx->presentation =
             wlr_presentation_create(ctx->display, ctx->backend));

    // Initialize Wayland protocols: data device, primary selection.
    try_(wlr_data_device_manager_create(ctx->display));
    try_(wlr_primary_selection_v1_device_manager_create(ctx->display));

    // Initialize Wayland protocols: xdg-shell, xdg-decoration-manager.
    if(true) {
        struct wlr_xdg_shell* xdg_shell = wlr_xdg_shell_create(ctx->display);
        struct wlr_xdg_decoration_manager_v1* xdg_decoration_manager =
            wlr_xdg_decoration_manager_v1_create(ctx->display);

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
            wlr_server_decoration_manager_create(ctx->display);

        if(server_decoration_manager != NULL) {
            wlr_server_decoration_manager_set_default_mode(
                server_decoration_manager,
                WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
        }
    }

    // Initialize privileged Wayland protocols.
    // Note: Screen capture-related protocols are potentially unsafe. Granting
    // unlimited access to these protocols can threaten user's security.
    try_(wlr_screencopy_manager_v1_create(ctx->display));
    try_(wlr_export_dmabuf_manager_v1_create(ctx->display));

    // Initialize workspaces.
    for_each_(struct rose_workspace, workspace, ctx->storage.workspace) {
        if(!rose_workspace_initialize(workspace, ctx)) {
            return false;
        } else {
            rose_workspace_set_panel(workspace, ctx->config.panel);
        }
    }

    // Set the first workspace as current.
    ctx->current_workspace = ctx->storage.workspace;

    // Initialize IPC server and command list.
    try_(ctx->ipc_server = rose_ipc_server_initialize(ctx));
    try_(ctx->command_list = rose_command_list_initialize());

#undef try_

    // Add Wayland socket.
    char const* socket = wl_display_add_socket_auto(ctx->display);
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
    return rose_server_context_start_processes(ctx), true;
}

void
rose_server_context_destroy(struct rose_server_context* ctx) {
    // Free memory.
    free(ctx->config.background_arg_list);
    free(ctx->config.dispatcher_arg_list);
    free(ctx->config.notification_daemon_arg_list);
    free(ctx->config.panel_arg_list);
    free(ctx->config.screen_locker_arg_list);
    free(ctx->config.terminal_arg_list);
    free(ctx->config.keyboard_layouts.data);

    for_each_(struct rose_utf8_string, path, ctx->config.paths) {
        free(path->data);
    }

    // Destroy keyboard control scheme.
    if(ctx->config.keyboard_control_scheme != NULL) {
        rose_keyboard_control_scheme_destroy(
            ctx->config.keyboard_control_scheme);
    }

    // Destroy workspaces.
    if(true) {
        struct rose_workspace* workspace = NULL;
        struct rose_workspace* _ = NULL;

        wl_list_for_each_safe(workspace, _, &(ctx->workspaces), link) {
            rose_workspace_destroy(workspace);
        }

        wl_list_for_each_safe(
            workspace, _, &(ctx->workspaces_without_output), link_output) {
            rose_workspace_destroy(workspace);
        }

        struct rose_output* output;
        wl_list_for_each(output, &(ctx->outputs), link) {
            wl_list_for_each_safe(
                workspace, _, &(output->workspaces), link_output) {
                rose_workspace_destroy(workspace);
            }
        }
    }

    // Destroy text rendering context.
    if(ctx->text_rendering_ctx != NULL) {
        rose_text_rendering_context_destroy(ctx->text_rendering_ctx);
    }

    // Destroy keyboard context.
    if(ctx->keyboard_ctx != NULL) {
        rose_keyboard_context_destroy(ctx->keyboard_ctx);
    }

    // Destroy cursor context.
    if(ctx->cursor_ctx.manager != NULL) {
        wlr_xcursor_manager_destroy(ctx->cursor_ctx.manager);
    }

    // Destroy command list.
    if(ctx->command_list != NULL) {
        rose_command_list_destroy(ctx->command_list);
    }

    // Destroy device preference list.
    if(ctx->preference_list != NULL) {
        rose_device_preference_list_destroy(ctx->preference_list);
    }

    // Destroy event sources.
    if(true) {
        struct wl_event_source* sources[] = //
            {ctx->event_source_sigint, ctx->event_source_sigterm,
             ctx->event_source_sigchld, ctx->event_source_timer};

        for(size_t i = 0; i != array_size_(sources); ++i) {
            if(sources[i] != NULL) {
                wl_event_source_remove(sources[i]);
            }
        }
    }

    // Destroy the display.
    if(ctx->display != NULL) {
        wl_display_destroy_clients(ctx->display);
        wl_display_destroy(ctx->display);
    }

    // Destroy the renderer.
    if(ctx->renderer != NULL) {
        wlr_renderer_destroy(ctx->renderer);
    }

    // Destroy the allocator.
    if(ctx->allocator != NULL) {
        wlr_allocator_destroy(ctx->allocator);
    }

#define kill_(type)                                \
    if(ctx->processes.type##_pid != (pid_t)(-1)) { \
        kill(ctx->processes.type##_pid, SIGKILL);  \
    }

    // Kill system processes.
    kill_(background);
    kill_(dispatcher);
    kill_(notification_daemon);
    kill_(panel);
    kill_(screen_locker);

#undef kill_
}

////////////////////////////////////////////////////////////////////////////////
// Configuration interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_set_keyboard_layout( //
    struct rose_server_context* ctx, unsigned layout_idx) {
    // If requested layout is invalid, then fail.
    if(layout_idx >= ctx->keyboard_ctx->n_layouts) {
        return false;
    }

    // Set keyboard layout.
    ctx->keyboard_ctx->layout_idx = layout_idx;
    xkb_layout_index_t group_idx =
        (xkb_layout_index_t)(ctx->keyboard_ctx->layout_idx);

    // Update layouts of all keyboard devices.
    struct rose_keyboard* keyboard = NULL;
    wl_list_for_each(keyboard, &(ctx->inputs_keyboards), link) {
        // Obtain underlying input device and its modifiers.
        struct wlr_keyboard* wlr_keyboard = keyboard->parent->dev->keyboard;
        struct wlr_keyboard_modifiers modifiers = wlr_keyboard->modifiers;

        // Update keyboard's modifiers.
        wlr_keyboard_notify_modifiers( //
            wlr_keyboard, modifiers.depressed, modifiers.latched,
            modifiers.locked, group_idx);
    }

    // Broadcast the change through IPC, if needed.
    if((ctx->keyboard_ctx->n_layouts > 1) && (ctx->ipc_server != NULL)) {
        rose_ipc_server_broadcast_status(
            ctx->ipc_server, rose_server_context_obtain_status(ctx));
    }

    // Operation succeeded.
    return true;
}

void
rose_server_context_configure(struct rose_server_context* ctx,
                              rose_server_context_configure_mask flags) {
    // If requested configuration is a no-op, then do nothing.
    if(flags == 0) {
        return;
    }

    // Configure keyboard control scheme, if requested.
    if((flags & rose_server_context_configure_keyboard_control_scheme) != 0) {
        // Initialize keyboard control scheme.
        struct rose_keyboard_control_scheme* keyboard_control_scheme = NULL;
        for_each_(struct rose_utf8_string, path, ctx->config.paths) {
            keyboard_control_scheme = rose_keyboard_control_scheme_initialize(
                rose_utf8_string_concat(path->data, "keyboard_control_scheme")
                    .data);

            if(keyboard_control_scheme != NULL) {
                // If new scheme has been successfully initialized, then destroy
                // previous scheme.
                if(ctx->config.keyboard_control_scheme != NULL) {
                    rose_keyboard_control_scheme_destroy(
                        ctx->config.keyboard_control_scheme);
                }

                // Set the new scheme.
                ctx->config.keyboard_control_scheme = keyboard_control_scheme;

                // Broadcast the change through IPC, if needed.
                if(ctx->ipc_server != NULL) {
                    rose_ipc_server_broadcast_status(
                        ctx->ipc_server,
                        (struct rose_ipc_status){
                            .type =
                                rose_ipc_status_type_keyboard_control_scheme});
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
        for_each_(struct rose_utf8_string, path, ctx->config.paths) {
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
            struct rose_keyboard_context* keyboard_ctx =
                rose_keyboard_context_initialize(keyboard_layouts.data);

            if(keyboard_ctx != NULL) {
                // If initialization succeeded, then destroy previous keyboard
                // context and save the new one.
                rose_keyboard_context_destroy(ctx->keyboard_ctx);
                ctx->keyboard_ctx = keyboard_ctx;

                // Destroy previous keyboard layouts and save the new ones.
                free(ctx->config.keyboard_layouts.data);
                ctx->config.keyboard_layouts = keyboard_layouts;

                // Set new keymap for all keyboard devices.
                struct rose_keyboard* keyboard = NULL;
                wl_list_for_each(keyboard, &(ctx->inputs_keyboards), link) {
                    // Obtain underlying input device.
                    struct wlr_keyboard* wlr_keyboard =
                        keyboard->parent->dev->keyboard;

                    // Set the keymap.
                    wlr_keyboard_set_keymap(
                        wlr_keyboard, ctx->keyboard_ctx->keymap);
                }

                // Broadcast the change through IPC, if needed.
                if(ctx->ipc_server != NULL) {
                    rose_ipc_server_broadcast_status(
                        ctx->ipc_server,
                        (struct rose_ipc_status){
                            .type = rose_ipc_status_type_keyboard_keymap});

                    rose_ipc_server_broadcast_status(
                        ctx->ipc_server,
                        rose_server_context_obtain_status(ctx));
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
        struct rose_theme theme = rose_default_theme;

        // Try reading the theme from one of the configuration files.
        for_each_(struct rose_utf8_string, path, ctx->config.paths) {
            if(rose_theme_read(
                   rose_utf8_string_concat(path->data, "theme").data, &theme)) {
                // If the theme has been successfully read, then save it.
                ctx->config.font_size = theme.font_size;
                ctx->config.panel = theme.panel;
                ctx->config.color_scheme = theme.color_scheme;

                // Request redraw operation.
                struct rose_output* output;
                wl_list_for_each(output, &(ctx->outputs), link) {
                    if(output->focused_workspace != NULL) {
                        rose_workspace_request_redraw(
                            output->focused_workspace);
                    } else {
                        rose_output_request_redraw(output);
                    }
                }

                // Broadcast the change through IPC, if needed.
                if(ctx->ipc_server != NULL) {
                    rose_ipc_server_broadcast_status(
                        ctx->ipc_server,
                        (struct rose_ipc_status){
                            .type = rose_ipc_status_type_theme});
                }

                // And break out of the cycle.
                break;
            }
        }
    }

    // Lock the screen, if requested.
    if((flags & rose_server_context_configure_screen_lock) != 0) {
        if(!(ctx->is_screen_locked)) {
            // Set the flag.
            ctx->is_screen_locked = true;

            // Update input focus.
            rose_workspace_make_current(ctx->current_workspace);

            // Request redraw operation.
            struct rose_output* output;
            wl_list_for_each(output, &(ctx->outputs), link) {
                if(output->focused_workspace != NULL) {
                    rose_workspace_request_redraw(output->focused_workspace);
                } else {
                    rose_output_request_redraw(output);
                }
            }

            // Broadcast the change through IPC, if needed.
            if(ctx->ipc_server != NULL) {
                rose_ipc_server_broadcast_status(
                    ctx->ipc_server, rose_server_context_obtain_status(ctx));
            }
        }
    }

    // Unlock the screen, if requested.
    if((flags & rose_server_context_configure_screen_unlock) != 0) {
        if(ctx->is_screen_locked) {
            // Clear the flag.
            ctx->is_screen_locked = false;

            // Update input focus.
            rose_workspace_make_current(ctx->current_workspace);

            // Request redraw operation.
            struct rose_output* output;
            wl_list_for_each(output, &(ctx->outputs), link) {
                if(output->focused_workspace != NULL) {
                    rose_workspace_request_redraw(output->focused_workspace);
                } else {
                    rose_output_request_redraw(output);
                }
            }

            // Broadcast the change through IPC, if needed.
            if(ctx->ipc_server != NULL) {
                rose_ipc_server_broadcast_status(
                    ctx->ipc_server, rose_server_context_obtain_status(ctx));
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Cursor image acquisition interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct wlr_xcursor_image*
rose_server_context_get_cursor_image( //
    struct rose_server_context* ctx, enum rose_output_cursor_type type,
    float scale) {
    // Note: Scaling factor is not used.
    unused_(scale);

    // Return cursor's image.
    return ctx->cursor_ctx
        .cursors[(ptrdiff_t)(min_(type, (rose_n_output_cursor_types - 1)))]
        ->images[0];
}

////////////////////////////////////////////////////////////////////////////////
// Device acquisition interface implementation.
////////////////////////////////////////////////////////////////////////////////

#define define_search_(type)                          \
    struct rose_##type* device = NULL;                \
    wl_list_for_each(device, &(ctx->type##s), link) { \
        if(device->id == id) {                        \
            break;                                    \
        }                                             \
    }                                                 \
                                                      \
    return device;

struct rose_input*
rose_server_context_obtain_input(struct rose_server_context* ctx, unsigned id) {
    define_search_(input);
}

struct rose_output*
rose_server_context_obtain_output(struct rose_server_context* ctx,
                                  unsigned id) {
    define_search_(output);
}

#undef define_search_

////////////////////////////////////////////////////////////////////////////////
// State query interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_status
rose_server_context_obtain_status(struct rose_server_context* ctx) {
    struct rose_ipc_status status = {
        .type = rose_ipc_status_type_server_state,
        .server_state = {ctx->is_screen_locked,
                         ctx->are_keyboard_shortcuts_inhibited,
                         ctx->keyboard_ctx->layout_idx}};

    return status;
}

struct rose_server_context_state
rose_server_context_state_obtain(struct rose_server_context* ctx) {
    // Initialize an empty state object.
    struct rose_server_context_state state = {};

    // Compute the number of input devices.
    if(!wl_list_empty(&(ctx->inputs))) {
        struct rose_input* input =
            wl_container_of(ctx->inputs.next, input, link);

        state.n_inputs = input->id + 1;
    }

    // Compute the number of output devices.
    if(!wl_list_empty(&(ctx->outputs))) {
        struct rose_output* output =
            wl_container_of(ctx->outputs.next, output, link);

        state.n_outputs = output->id + 1;
    }

    // Return server context's state.
    return state;
}

////////////////////////////////////////////////////////////////////////////////
// IPC access rights checking interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_server_context_check_ipc_access_rights(
    struct rose_server_context* ctx, pid_t pid,
    enum rose_ipc_connection_type connection_type) {
    // System processes have full IPC access.
    if((pid == ctx->processes.screen_locker_pid) ||
       (pid == ctx->processes.dispatcher_pid) ||
       (pid == ctx->processes.panel_pid)) {
        return true;
    }

    // Determine access rights depending on IPC connection's type and client's
    // PID.
    switch(connection_type) {
        case rose_ipc_connection_type_configurator:
            // fall-through

        case rose_ipc_connection_type_dispatcher:
            if((rose_command_list_query_access_rights(ctx->command_list, pid) &
                rose_command_access_ipc) != 0) {
                return true;
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
