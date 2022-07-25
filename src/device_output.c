// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"
#include "rendering.h"

#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_surface.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

////////////////////////////////////////////////////////////////////////////////
// Text buffer initialization/destruction utility functions.
////////////////////////////////////////////////////////////////////////////////

static struct rose_output_text_buffer*
rose_output_text_buffer_initialize( //
    struct rose_output_text_buffer* buffer, struct wlr_renderer* renderer,
    int w, int h) {
    // Clamp the dimensions.
    w = clamp_(w, 1, 32768);
    h = clamp_(h, 1, 32768);

    // Do nothing else if the buffer is already initialized.
    if((buffer->pixels != NULL) && (buffer->texture != NULL) &&
       (buffer->pixels->w == w) && (buffer->pixels->h == h)) {
        goto end;
    }

    // Compute and validate pixel buffer's data size.
#define mul_(x, y)                     \
    {                                  \
        size_t z = (x) * (size_t)(y);  \
        if((z / (size_t)(y)) == (x)) { \
            (x) = z;                   \
        } else {                       \
            goto end;                  \
        }                              \
    }

    size_t pixel_buffer_data_size = (size_t)(w);
    mul_(pixel_buffer_data_size, h);
    mul_(pixel_buffer_data_size, 4);

#undef mul_

    // Compute and validate pixel buffer's full size.
    size_t pixel_buffer_data_offset = sizeof(struct rose_pixel_buffer);
    size_t pixel_buffer_size =
        pixel_buffer_data_offset + pixel_buffer_data_size;

    if(pixel_buffer_size < pixel_buffer_data_size) {
        goto end;
    }

    // Destroy existing buffer.
    free(buffer->pixels);
    wlr_texture_destroy(buffer->texture);

    // Initialize a new buffer.
    *buffer = (struct rose_output_text_buffer){
        .pixels = malloc(pixel_buffer_size),
        .pixel_buffer_data_size = pixel_buffer_data_size};

    // Do nothing else if memory allocation failed.
    if(buffer->pixels == NULL) {
        goto end;
    }

    // Initialize the pixel buffer.
    *(buffer->pixels) = (struct rose_pixel_buffer){
        .data = (unsigned char*)(buffer->pixels) + pixel_buffer_data_offset,
        .w = w,
        .h = h};

    // Clear the pixel buffer.
    memset(buffer->pixels->data, 0, buffer->pixel_buffer_data_size);

    // Create buffer's texture.
    buffer->texture = wlr_texture_from_pixels(
        renderer, DRM_FORMAT_ARGB8888, w * 4, w, h, buffer->pixels->data);

end:
    return buffer;
}

static void
rose_output_text_buffer_destroy(struct rose_output_text_buffer* buffer) {
    // Free pixel buffer's memory.
    free(buffer->pixels);

    // Destroy the texture.
    wlr_texture_destroy(buffer->texture);

    // Zero-initialize the buffer.
    *buffer = (struct rose_output_text_buffer){};
}

static bool
rose_output_text_buffer_is_initialized(struct rose_output_text_buffer* buffer) {
    return ((buffer->pixels != NULL) && (buffer->texture != NULL));
}

////////////////////////////////////////////////////////////////////////////////
// Title-string-composition-related utility functions.
////////////////////////////////////////////////////////////////////////////////

enum {
    rose_output_utf8_buffer_size = 2048,
    rose_output_utf8_string_size_max = rose_output_utf8_buffer_size - 1
};

static struct rose_utf32_string
rose_output_workspace_compose_title_string(struct rose_workspace* workspace) {
    // If there is no workspace, then return empty string.
    if(workspace == NULL) {
        return (struct rose_utf32_string){};
    }

    // Obtain relevant IDs.
    unsigned id_output = workspace->output->id;
    unsigned id_workspace = workspace->id;

    // Initialize resulting string buffer.
    char string[rose_output_utf8_buffer_size] = {};

    // Compose title string (use U+F26C and U+F24D Unicode characters).
    if(workspace->focused_surface == NULL) {
        // If there is no focused surface, then only print output's and
        // workspace's IDs.
        char const* format = u8"\xEF\x89\xAC %02d / %02d";
        snprintf(string, rose_output_utf8_string_size_max, format, id_output,
                 id_workspace);
    } else {
        // Otherwise, initialize a buffer for focused surface's title.
        char surface_title[rose_output_utf8_buffer_size] = {};

        // Fill it.
        if(true) {
            struct rose_utf8_string title = rose_convert_ntbs_to_utf8(
                workspace->focused_surface->xdg_surface->toplevel->title);

            memcpy(surface_title, title.data,
                   min_(title.size, rose_output_utf8_string_size_max));
        }

        // And print all the relevant data.
        char const* format = u8"\xEF\x89\xAC %02d / %02d \xEF\x89\x8D %s";
        snprintf(string, rose_output_utf8_string_size_max, format, id_output,
                 id_workspace, surface_title);
    }

    // Convert resulting UTF-8 string to UTF-32 string.
    return rose_convert_utf8_to_utf32(rose_convert_ntbs_to_utf8(string));
}

////////////////////////////////////////////////////////////////////////////////
// Text-buffer-manipulation-related utility functions.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_text_buffers_update_type {
    rose_output_text_buffers_update_normal,
    rose_output_text_buffers_update_forced
};

static void
rose_output_update_text_buffers(
    struct rose_output* output,
    enum rose_output_text_buffers_update_type update_type) {
    // Obtain a pointer to the output's focused workspace.
    struct rose_workspace* workspace = output->focused_workspace;

    // Do nothing else if there is no focused workspace.
    if(workspace == NULL) {
        return;
    }

    // Obtain output's state.
    struct rose_output_state output_state = rose_output_state_obtain(output);

    // Obtain text-rendering-related data.
    struct rose_text_rendering_context* text_rendering_ctx =
        output->ctx->text_rendering_ctx;

    struct rose_text_rendering_parameters text_rendering_params = {
        .font_size = output->ctx->config.font_size, .dpi = output_state.dpi};

    struct rose_color_scheme const* color_scheme =
        &(output->ctx->config.color_scheme);

    // Obtain panel's data.
    struct rose_ui_panel panel = workspace->panel;
    if(panel.is_visible) {
        // Note: The panel is invisible if workspace's focused surface is in
        // fullscreen mode.
        if((workspace->focused_surface != NULL) &&
           (workspace->focused_surface->state.pending.is_fullscreen)) {
            panel.is_visible = false;
        }
    }

    // Update title's text buffer, if needed.
    while(panel.is_visible) {
        // Stop the update, if needed.
        if((update_type == rose_output_text_buffers_update_normal) &&
           (output->focused_surface == workspace->focused_surface) &&
           ((output->focused_surface == NULL) ||
            !(output->focused_surface->is_name_updated))) {
            break;
        }

        // Update the focused surface.
        output->focused_surface = workspace->focused_surface;
        if(output->focused_surface != NULL) {
            output->focused_surface->is_name_updated = false;
        }

        // Compute text buffer's dimensions.
        int buffer_w = ((((panel.position == rose_ui_panel_position_left) ||
                          (panel.position == rose_ui_panel_position_right))
                             ? output_state.h
                             : output_state.w) /
                        2),
            buffer_h = (int)(panel.size * output_state.scale + 0.5);

        // Initialize the text buffer.
        struct rose_output_text_buffer* text_buffer =
            rose_output_text_buffer_initialize( //
                &(output->text_buffers.title), output->ctx->renderer, buffer_w,
                buffer_h);

        // Stop the update if the text buffer is not initialized.
        if(!rose_output_text_buffer_is_initialized(text_buffer)) {
            break;
        }

        // Set text's color and clear text buffer's pixels.
        text_rendering_params.color = color_scheme->panel_foreground;
        memset(
            text_buffer->pixels->data, 0, text_buffer->pixel_buffer_data_size);

        // Compose and render the title.
        rose_render_string( //
            text_rendering_ctx, text_rendering_params,
            rose_output_workspace_compose_title_string(workspace),
            *(text_buffer->pixels));

        // Update text buffer's texture.
        wlr_texture_write_pixels( //
            text_buffer->texture, text_buffer->pixels->w * 4,
            text_buffer->pixels->w, text_buffer->pixels->h, 0, 0, 0, 0,
            text_buffer->pixels->data);

        // Note: Update succeeded.
        break;
    }

    // Update menu's text buffer, if needed.
    for(struct rose_ui_menu* menu = &(output->ui.menu);
        menu->is_visible &&
        (menu->is_updated ||
         (update_type == rose_output_text_buffers_update_forced));) {
        // Compute text buffer's dimensions.
        int buffer_w = (int)(menu->area.w * output_state.scale + 0.5),
            buffer_h = (int)(menu->area.h * output_state.scale + 0.5);

        // Initialize the text buffer.
        struct rose_output_text_buffer* text_buffer =
            rose_output_text_buffer_initialize( //
                &(output->text_buffers.menu), output->ctx->renderer, buffer_w,
                buffer_h);

        // Stop the update if the text buffer is not initialized.
        if(!rose_output_text_buffer_is_initialized(text_buffer)) {
            break;
        }

        // Obtain menu's previous and current texts.
        struct rose_ui_menu_text text_prev = output->ui_menu_text;
        struct rose_ui_menu_text text = rose_ui_menu_text_obtain(menu);

        // Save menu's current text.
        output->ui_menu_text = text;

        // Set text's color.
        text_rendering_params.color = color_scheme->menu_foreground;

        // Render the text line-by-line, compute text buffer's updated area.
        int updated_area[2] = {-1, 0};
        if(true) {
            // Compute menu line's height in text buffer's space.
            int line_h = (int)(menu->layout.line_h * output_state.scale + 0.5);

            // Initialize a pixel buffer for a text line.
            struct rose_pixel_buffer line_pixels = {
                .w = text_buffer->pixels->w, .h = line_h};

            // Compute text line's stride.
            ptrdiff_t line_stride = 4 * line_pixels.w * line_pixels.h;

            // Render the lines.
            int h_space_left = text_buffer->pixels->h;
            for(ptrdiff_t i = 0; i < text.n_lines; ++i) {
                // Stop rendering if there is no space left in the text buffer.
                if(h_space_left <= 0) {
                    break;
                }

                // Compute current line's parameters.
                line_pixels.data = text_buffer->pixels->data + i * line_stride;
                line_pixels.h = min_(line_pixels.h, h_space_left);

                // Update text buffer's available space.
                h_space_left -= line_h;

#define line_diff_(a, b)   \
    ((a.size != b.size) || \
     (memcmp(a.data, b.data, a.size * sizeof(a.data[0])) != 0))

                // A flag which shows that current line must be rendered.
                bool must_render_line =
                    (menu->is_layout_updated) || (i >= text_prev.n_lines) ||
                    (line_diff_(text.lines[i], text_prev.lines[i]));

#undef line_diff_

                // Render the line, if needed.
                if(must_render_line) {
                    // Compute line's offset.
                    int dy = i * line_h;

                    // Compute updated area.
                    updated_area[0] =
                        ((updated_area[0] < 0) ? dy : updated_area[0]);
                    updated_area[1] = dy + line_pixels.h;

                    // Clear line's pixel buffer.
                    memset(
                        line_pixels.data, 0, 4 * line_pixels.w * line_pixels.h);

                    // Render the current line.
                    rose_render_string( //
                        text_rendering_ctx, text_rendering_params,
                        text.lines[i], line_pixels);
                }
            }
        }

        // Update text buffer's texture, if needed.
        if(updated_area[0] >= 0) {
            wlr_texture_write_pixels( //
                text_buffer->texture, text_buffer->pixels->w * 4,
                text_buffer->pixels->w, updated_area[1] - updated_area[0], 0,
                updated_area[0], 0, updated_area[0], text_buffer->pixels->data);
        }

        // Clear menu's flags.
        menu->is_updated = menu->is_layout_updated = false;

        // Note: Update succeeded.
        break;
    }
}

static void
rose_output_request_text_buffers_update(struct rose_output* output) {
    // Set the flag.
    output->is_text_buffers_update_requested = true;

    // Schedule a frame.
    rose_output_schedule_frame(output);
}

////////////////////////////////////////////////////////////////////////////////
// Workspace-related utility functions.
////////////////////////////////////////////////////////////////////////////////

static struct rose_workspace*
rose_output_select_next_workspace( //
    struct rose_output* output, struct rose_workspace* workspace,
    enum rose_output_focus_direction direction) {
    // If there is no workspace, or the workspace does not belong to the given
    // output, then select the first workspace of the output.
    if((workspace == NULL) || (output != workspace->output)) {
        if(wl_list_empty(&(output->workspaces))) {
            return NULL;
        }

        return wl_container_of(output->workspaces.prev, workspace, link_output);
    }

    // Select the next workspace in the given direction.
#define select_(d)                                                       \
    ((workspace->link_output.d == &(output->workspaces))                 \
         ? wl_container_of(output->workspaces.d, workspace, link_output) \
         : wl_container_of(workspace->link_output.d, workspace, link_output))

    if(direction == rose_output_focus_direction_backward) {
        return select_(next);
    } else {
        return select_(prev);
    }

#undef select_
}

static void
rose_output_add_workspaces(struct rose_output* output) {
    // If the output has no workspaces, and the list of workspaces without
    // output is empty, then add the first available workspace to that list.
    if(wl_list_empty(&(output->workspaces)) &&
       wl_list_empty(&(output->ctx->workspaces_without_output))) {
        if(!wl_list_empty(&(output->ctx->workspaces))) {
            // Obtain the first available workspace.
            struct rose_workspace* workspace =
                wl_container_of(output->ctx->workspaces.prev, workspace, link);

            // Remove it from the list of available workspaces.
            wl_list_remove(&(workspace->link));
            wl_list_init(&(workspace->link));

            // Add it to the list of workspaces without output.
            wl_list_remove(&(workspace->link_output));
            wl_list_insert(&(workspace->ctx->workspaces_without_output),
                           &(workspace->link_output));
        }
    }

    // Add workspaces to the output.
    if(true) {
        struct rose_workspace* workspace = NULL;
        struct rose_workspace* _ = NULL;

        wl_list_for_each_safe( //
            workspace, _, &(output->ctx->workspaces_without_output),
            link_output) {
            // Remove the workspace from the list of available workspaces.
            wl_list_remove(&(workspace->link));
            wl_list_init(&(workspace->link));

            // Link the workspace with the output.
            wl_list_remove(&(workspace->link_output));
            wl_list_insert(rose_workspace_find_position_in_list(
                               &(output->workspaces), workspace,
                               offsetof(struct rose_workspace, link_output)),
                           &(workspace->link_output));

            workspace->output = output;

            // Configure the workspace.
            rose_workspace_notify_output_mode(workspace, output);
        }
    }

    // Send output enter events to all surfaces which belong to the output.
    if(true) {
        struct rose_workspace* workspace = NULL;
        struct rose_surface* surface = NULL;

        wl_list_for_each(workspace, &(output->workspaces), link_output) {
            wl_list_for_each(surface, &(workspace->surfaces), link) {
                wlr_surface_send_enter(
                    surface->xdg_surface->surface, output->dev);
            }
        }
    }

    // Update output's focus, if needed.
    if(output == output->ctx->current_workspace->output) {
        rose_output_focus_workspace(output, output->ctx->current_workspace);
    } else if(output->focused_workspace == NULL) {
        rose_output_focus_workspace_relative(
            output, rose_output_focus_direction_forward);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Surface-notification-related utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_output_surface_send_frame_done( //
    struct wlr_surface* surface, int x, int y, void* data) {
    unused_(x), unused_(y);

    // Send frame done event to the surface.
    wlr_surface_send_frame_done(surface, data);
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_output_mode(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain a pointer to the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_mode);

    // Update the UI.
    rose_output_ui_update(&(output->ui));

    // Notify all workspaces which belong to this output.
    if(true) {
        struct rose_workspace* workspace;
        wl_list_for_each(workspace, &(output->workspaces), link_output) {
            rose_workspace_notify_output_mode(workspace, output);
        }
    }

    // Request update of output's text buffers.
    rose_output_request_text_buffers_update(output);
}

static void
rose_handle_event_output_frame(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain a pointer to the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_frame);

    // Reset this flag, since at this point no frame is scheduled.
    output->is_frame_scheduled = false;

    // Do nothing else if the output is disabled.
    if(!output->dev->enabled) {
        return;
    }

    // Get current timestamp.
    struct timespec timestamp = {};
    clock_gettime(CLOCK_MONOTONIC, &timestamp);

    // Determine if redraw is required.
    bool is_redraw_required = //
        (output->is_text_buffers_update_requested) ||
        (output->n_frames_without_damage < 2);

    // Update output's damage tracking data.
    output->n_frames_without_damage++;
    output->n_frames_without_damage = min_(output->n_frames_without_damage, 2);

    // Obtain a pointer to the output's focused workspace.
    struct rose_workspace* workspace = output->focused_workspace;

    // If there is a focused workspace, then perform additional actions.
    if(workspace != NULL) {
        // Force workspace's running transaction to stop if too much time has
        // passed since it started.
        if(workspace->transaction.sentinel > 0) {
            if(fabs(difftime(timestamp.tv_sec,
                             workspace->transaction.start_time.tv_sec)) >=
               1.0) {
                rose_workspace_transaction_commit(workspace);
            }
        }

        // Determine if redraw is required.
        is_redraw_required =
            is_redraw_required || (workspace->n_frames_without_commits < 2);

        // Update workspace's damage tracking data.
        workspace->n_frames_without_commits++;
        workspace->n_frames_without_commits =
            min_(workspace->n_frames_without_commits, 2);
    }

    // Update output's text buffers, if needed.
    if(is_redraw_required) {
        // Determine the type of the update.
        enum rose_output_text_buffers_update_type update_type =
            (output->is_text_buffers_update_requested
                 ? rose_output_text_buffers_update_forced
                 : rose_output_text_buffers_update_normal);

        // Clear the flag.
        output->is_text_buffers_update_requested = false;

        // Update the buffers.
        rose_output_update_text_buffers(output, update_type);
    }

    // If there is no need to do full redraw, then perform additional actions
    // depending on whether or not the cursor has moved, and whether or not the
    // output was in direct scan-out mode.
    if(!is_redraw_required) {
        if(output->cursor.has_moved) {
            if(output->is_scanned_out) {
                // If the output was in direct scan-out mode, then try using
                // this mode again by rendering output's content.
                rose_render_content(output);
            } else {
                // Otherwise, swap buffers.
                if(wlr_output_attach_render(output->dev, NULL)) {
                    wlr_output_commit(output->dev);
                } else {
                    wlr_output_schedule_frame(output->dev);
                }
            }

            // Go to the end of the routine to update the flags.
            goto end;
        } else {
            // Do nothing else.
            return;
        }
    }

    // Render output's content.
    rose_render_content(output);

    // Update the timestamp.
    // Note: Because rendering operation takes some time, the previous timestamp
    // might have become outdated at this point.
    clock_gettime(CLOCK_MONOTONIC, &timestamp);

    // Send frame done events to all relevant surfaces.
    if(!(output->ctx->is_screen_locked)) {
        // If there is a focused workspace, then send required frame done events
        // to its surfaces.
        if(workspace != NULL) {
            struct rose_surface* surface = NULL;
            if(workspace->transaction.sentinel > 0) {
                // If there is a running workspace transaction, then send frame
                // done events to all mapped surfaces which are part of this
                // transaction.
                wl_list_for_each(
                    surface, &(workspace->surfaces_mapped), link_mapped) {
                    if(surface->is_transaction_running) {
                        wlr_surface_send_frame_done(
                            surface->xdg_surface->surface, &timestamp);
                    }
                }
            } else {
                // Otherwise, send frame done events to all visible surfaces.
                wl_list_for_each(
                    surface, &(workspace->surfaces_visible), link_visible) {
                    wlr_xdg_surface_for_each_surface( //
                        surface->xdg_surface,
                        rose_output_surface_send_frame_done, &timestamp);
                }
            }
        }
    }

    // Send frame done events to all visible widgets.
    if(true) {
        struct rose_output_widget* widget = NULL;
        for(ptrdiff_t i = 0; i != rose_output_n_widget_types; ++i) {
            wl_list_for_each(
                widget, &(output->ui.widgets_mapped[i]), link_mapped) {
                if(rose_output_widget_is_visible(widget)) {
                    wlr_xdg_surface_for_each_surface(
                        widget->xdg_surface,
                        rose_output_surface_send_frame_done, &timestamp);
                }
            }
        }
    }

end:

    // Update output's flags.
    output->is_frame_scheduled = true;
    output->cursor.has_moved = false;
}

static void
rose_handle_event_output_needs_frame(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain a pointer to the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_needs_frame);

    // Schedule a frame.
    output->is_frame_scheduled = (wlr_output_schedule_frame(output->dev), true);
}

static void
rose_handle_event_output_damage(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain a pointer to the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_damage);

    // Reset the number of frames without damage taken.
    output->n_frames_without_damage = 0;
}

static void
rose_handle_event_output_destroy(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain a pointer to the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_destroy);

    // Destroy the output.
    rose_output_destroy(output);
}

static void
rose_handle_event_output_cursor_client_surface_destroy(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain a pointer to the output.
    struct rose_output* output = wl_container_of(
        listener, output, listener_cursor_client_surface_destroy);

    // Remove listener from the signal.
    wl_list_remove(&(output->listener_cursor_client_surface_destroy.link));
    wl_list_init(&(output->listener_cursor_client_surface_destroy.link));

    // Reset the surface pointer and clear the flag.
    output->cursor.client_surface = NULL;
    output->cursor.is_client_surface_set = false;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_initialize(struct rose_server_context* ctx,
                       struct wlr_output* dev) {
    // Initialize output's rendering subsystem.
    if(!wlr_output_init_render(dev, ctx->allocator, ctx->renderer)) {
        return;
    }

    // Set device's mode, if needed.
    if(!wl_list_empty(&(dev->modes))) {
        wlr_output_set_mode(dev, wlr_output_preferred_mode(dev));
    }

    // Create and initialize a new cursor.
    struct wlr_output_cursor* wlr_cursor = wlr_output_cursor_create(dev);
    if(wlr_cursor == NULL) {
        return;
    }

    // Allocate and initialize a new output.
    struct rose_output* output = malloc(sizeof(struct rose_output));
    if(output == NULL) {
        return;
    } else {
        *output = (struct rose_output){
            .ctx = ctx, .dev = dev, .cursor = {.wlr_cursor = wlr_cursor}};
    }

    // Enable the device.
    wlr_output_enable(dev, true);
    wlr_output_commit(dev);

    // Initialize the list of output modes.
    if(true) {
        struct wlr_output_mode* mode = NULL;
        wl_list_for_each(mode, &(dev->modes), link) {
            // Limit the number of modes.
            if(output->modes.size == rose_n_output_modes_max) {
                break;
            }

            // Save the current mode.
            output->modes.data[output->modes.size++] =
                (struct rose_output_mode){
                    .w = mode->width, .h = mode->height, .rate = mode->refresh};
        }
    }

    // Initialize output's list of workspaces.
    wl_list_init(&(output->workspaces));

    // Initialize output's UI.
    rose_output_ui_initialize(&(output->ui), output);

    // Create a new global for this output.
    wlr_output_create_global(output->dev);

    // Add this output to the list.
    wl_list_insert(&(ctx->outputs), &(output->link));

    // Set output's ID.
    if(output->link.next != &(output->ctx->outputs)) {
        output->id = (wl_container_of(output->link.next, output, link))->id + 1;
    }

    // Broadcast output's initialization event though IPC, if needed.
    if(output->ctx->ipc_server != NULL) {
        rose_ipc_server_broadcast_status(
            output->ctx->ipc_server,
            (struct rose_ipc_status){
                .type = rose_ipc_status_type_output_initialized,
                .device_id = output->id});
    }

    // Register listeners.
#define add_signal_(f)                                              \
    {                                                               \
        output->listener_##f.notify = rose_handle_event_output_##f; \
        wl_signal_add(&(dev->events.f), &(output->listener_##f));   \
    }

#define initialize_(f)                                              \
    {                                                               \
        output->listener_##f.notify = rose_handle_event_output_##f; \
        wl_list_init(&(output->listener_##f.link));                 \
    }

    add_signal_(mode);
    add_signal_(frame);
    add_signal_(needs_frame);

    add_signal_(damage);
    add_signal_(destroy);
    initialize_(cursor_client_surface_destroy);

#undef initialize_
#undef add_signal_

    // Set output cursor's type.
    rose_output_cursor_set(output, rose_output_cursor_type_default);

    // Request update of output's text buffers.
    rose_output_request_text_buffers_update(output);

    // Add workspaces to the output.
    rose_output_add_workspaces(output);

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_output, .data = output};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->ctx->menus_visible), link) {
            rose_ui_menu_notify_line_add(menu, line);
        }
    }

    // Apply preferences.
    rose_output_apply_preferences(output, ctx->preference_list);
}

void
rose_output_destroy(struct rose_output* output) {
    // Hide the menu.
    rose_ui_menu_hide(&(output->ui.menu));

    // Broadcast output's destruction event though IPC, if needed.
    if(output->ctx->ipc_server != NULL) {
        rose_ipc_server_broadcast_status(
            output->ctx->ipc_server,
            (struct rose_ipc_status){
                .type = rose_ipc_status_type_output_destroyed,
                .device_id = output->id});
    }

    // Update IDs of all outputs which precede this one.
    for(struct rose_output* x = output;
        x->link.prev != &(output->ctx->outputs);) {
        // Obtain preceding output.
        x = wl_container_of(x->link.prev, x, link);

        // Decrement its ID and request update of its text buffers.
        x->id--, rose_output_request_text_buffers_update(x);
    }

    // Destroy output's text buffers.
    rose_output_text_buffer_destroy(&(output->text_buffers.title));
    rose_output_text_buffer_destroy(&(output->text_buffers.menu));

    // Remove listeners from signals.
    wl_list_remove(&(output->listener_mode.link));
    wl_list_remove(&(output->listener_frame.link));
    wl_list_remove(&(output->listener_needs_frame.link));

    wl_list_remove(&(output->listener_damage.link));
    wl_list_remove(&(output->listener_destroy.link));
    wl_list_remove(&(output->listener_cursor_client_surface_destroy.link));

    // Destroy the global.
    wlr_output_destroy_global(output->dev);

    // Select output's successor, make sure that the output is not its own
    // successor.
    struct rose_output* successor =
        ((output->link.next == &(output->ctx->outputs))
             ? wl_container_of(output->ctx->outputs.next, successor, link)
             : wl_container_of(output->link.next, successor, link));

    successor = ((successor == output) ? NULL : successor);

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_output, .data = output};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->ctx->menus_visible), link) {
            rose_ui_menu_notify_line_remove(menu, line);
        }
    }

    // Remove the output from the list.
    wl_list_remove(&(output->link));

    // Destroy the UI.
    rose_output_ui_destroy(&(output->ui));

    // Send output leave events to all surfaces which belong to the output.
    if(true) {
        struct rose_workspace* workspace = NULL;
        struct rose_surface* surface = NULL;

        wl_list_for_each(workspace, &(output->workspaces), link_output) {
            wl_list_for_each(surface, &(workspace->surfaces), link) {
                wlr_surface_send_leave(
                    surface->xdg_surface->surface, output->dev);
            }
        }
    }

    // Move all workspaces which belong to the output.
    if(true) {
        struct rose_workspace* workspace = NULL;
        struct rose_workspace* _ = NULL;

        // Remove all workspaces from the output.
        wl_list_for_each_safe(
            workspace, _, &(output->workspaces), link_output) {
            // Sever all links between the workspace and the output.
            wl_list_remove(&(workspace->link_output));
            wl_list_init(&(workspace->link_output));

            workspace->output = NULL;

            // Add the workspace to the appropriate list.
            if(wl_list_empty(&(workspace->surfaces)) &&
               !rose_workspace_is_current(workspace)) {
                // If the workspace is empty and not current, then add it to the
                // list of available workspaces.
                wl_list_remove(&(workspace->link));
                wl_list_insert(rose_workspace_find_position_in_list(
                                   &(workspace->ctx->workspaces), workspace,
                                   offsetof(struct rose_workspace, link)),
                               &(workspace->link));

                // And reset its panel.
                workspace->panel = workspace->panel_saved =
                    output->ctx->config.panel;
            } else {
                // Otherwise, add it to the list of workspaces without output.
                wl_list_insert(&(workspace->ctx->workspaces_without_output),
                               &(workspace->link_output));
            }
        }

        // If there is output's successor, then add workspaces to it.
        if(successor != NULL) {
            rose_output_add_workspaces(successor);
        }
    }

    // Update all visible menus.
    if(true) {
        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->ctx->menus_visible), link) {
            rose_ui_menu_update(menu);
        }
    }

    // Free memory.
    free(output);
}

////////////////////////////////////////////////////////////////////////////////
// Configuration interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_output_configure(struct rose_output* output,
                      struct rose_output_configure_parameters params) {
    // If requested configuration is a no-op, then return success.
    if(params.flags == 0) {
        return true;
    }

    // If the given output is disabled, then configuration fails.
    if((output->dev == NULL) || !output->dev->enabled) {
        return false;
    }

    // Check the validity of the specified parameters.

    if((params.flags & rose_output_configure_transform) != 0) {
        // Note: These bounds are determined by the Wayland protocol.
        if((params.transform < 0) || (params.transform > 7)) {
            return false;
        }
    }

    if((params.flags & rose_output_configure_scale) != 0) {
        // Note: The scaling factor can not be NaN or infinity.
        if(!isfinite(params.scale)) {
            return false;
        }

        // Note: The scaling factor must have sane value.
        if((params.scale < 0.1) || (params.scale > 25.0)) {
            return false;
        }
    }

    // Note: Requested mode is not checked, since if it does not belong to the
    // list of possible modes, then it will not be applied.

    // Set specified parameters.

    if((params.flags & rose_output_configure_transform) != 0) {
        wlr_output_set_transform(output->dev, params.transform);
    }

    if((params.flags & rose_output_configure_scale) != 0) {
        wlr_output_set_scale(output->dev, (float)(params.scale));
    }

    while((params.flags & rose_output_configure_mode) != 0) {
        // If default mode has been requested, and the list of modes is not
        // empty, then set such mode.
        if(((params.mode.w == 0) && //
            (params.mode.h == 0) && //
            (params.mode.rate == 0)) &&
           !wl_list_empty(&(output->dev->modes))) {
            wlr_output_set_mode(
                output->dev, wlr_output_preferred_mode(output->dev));

            break;
        }

        // Set the requested mode if it matches one of the available modes.
        struct wlr_output_mode* mode = NULL;
        wl_list_for_each(mode, &(output->dev->modes), link) {
            if((mode->width == params.mode.w) &&
               (mode->height == params.mode.h) &&
               (mode->refresh == params.mode.rate)) {
                wlr_output_set_mode(output->dev, mode);
                break;
            }
        }

        // Finish mode setting.
        break;
    }

    // Update device preference list, if needed.
    if(output->ctx->preference_list != NULL) {
        // Initialize device preference.
        struct rose_device_preference preference = {
            .device_name = rose_output_name_obtain(output),
            .device_type = rose_device_type_output,
            .params = {.output = params}};

        // Perform update operation.
        rose_device_preference_list_update(
            output->ctx->preference_list, preference);
    }

    // Commit output's state.
    if(wlr_output_commit(output->dev)) {
        // If commit is successful, then handle related event.
        rose_handle_event_output_mode(&(output->listener_mode), NULL);
    }

    // Signal operation's success.
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Workspace focusing interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_focus_workspace(struct rose_output* output,
                            struct rose_workspace* workspace) {
    // Do nothing if the given output does not change its focus, or if the given
    // output does not contain the workspace.
    if((workspace == output->focused_workspace) ||
       ((workspace != NULL) && (output != workspace->output))) {
        return;
    }

    // Obtain previously focused workspace.
    struct rose_workspace* workspace_prev = output->focused_workspace;

    // Update the focus.
    output->focused_workspace = workspace;

    // Handle the focus.
    if(workspace != NULL) {
        // If there is a focused workspace, then make it current.
        rose_workspace_make_current(workspace);

        // Move its pointer.
        if(workspace_prev != NULL) {
            rose_workspace_pointer_warp(
                workspace, workspace->pointer.movement_time_msec,
                workspace_prev->pointer.x, workspace_prev->pointer.y);
        } else {
            rose_workspace_pointer_warp(
                workspace, workspace->pointer.movement_time_msec,
                workspace->pointer.x, workspace->pointer.y);
        }

        // Update output's UI components.
        rose_output_ui_update(&(output->ui));

        // And request workspace's redraw.
        rose_workspace_request_redraw(workspace);
    } else {
        // Otherwise, mark the output as damaged.
        output->n_frames_without_damage = 0;

        // And hide the menu.
        rose_ui_menu_hide(&(output->ui.menu));
    }

    // Perform actions with previously focused workspace, if any.
    if(workspace_prev != NULL) {
        // If previously focused workspace is empty, and it is not output's only
        // workspace, then remove it.
        if(!((output->workspaces.prev == &(workspace_prev->link_output)) &&
             (output->workspaces.next == &(workspace_prev->link_output))) &&
           wl_list_empty(&(workspace_prev->surfaces))) {
            rose_output_remove_workspace(output, workspace_prev);
        }
    }

    // Request update of output's text buffers.
    rose_output_request_text_buffers_update(output);
}

void
rose_output_focus_workspace_relative(
    struct rose_output* output, enum rose_output_focus_direction direction) {
    // Focus the next workspace in the given direction.
    rose_output_focus_workspace(
        output, rose_output_select_next_workspace(
                    output, output->focused_workspace, direction));
}

////////////////////////////////////////////////////////////////////////////////
// Workspace addition/removal interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_add_workspace(struct rose_output* output,
                          struct rose_workspace* workspace) {
    // Do nothing if the workspace doesn't change its output.
    if(output == workspace->output) {
        return;
    }

    // Remove the workspace from its previous output, if needed.
    if(workspace->output != NULL) {
        rose_output_remove_workspace(workspace->output, workspace);
    }

    // Remove the workspace from the list of available workspaces.
    wl_list_remove(&(workspace->link));
    wl_list_init(&(workspace->link));

    // Link the workspace with its new output.
    wl_list_remove(&(workspace->link_output));
    wl_list_insert(rose_workspace_find_position_in_list(
                       &(output->workspaces), workspace,
                       offsetof(struct rose_workspace, link_output)),
                   &(workspace->link_output));

    workspace->output = output;

    // Configure the workspace.
    rose_workspace_notify_output_mode(workspace, output);

    // Send output enter events to all the surfaces which belong to the
    // workspace.
    if(true) {
        struct rose_surface* surface = NULL;
        wl_list_for_each(surface, &(workspace->surfaces), link) {
            wlr_surface_send_enter(surface->xdg_surface->surface, output->dev);
        }
    }

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_workspace, .data = workspace};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->ctx->menus_visible), link) {
            rose_ui_menu_notify_line_add(menu, line);
        }
    }
}

void
rose_output_remove_workspace(struct rose_output* output,
                             struct rose_workspace* workspace) {
    // Do nothing if the given output does not contain the workspace.
    if(output != workspace->output) {
        return;
    }

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_workspace, .data = workspace};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->ctx->menus_visible), link) {
            rose_ui_menu_notify_line_remove(menu, line);
        }
    }

    // Send output leave events to all surfaces which belong to the workspace.
    if(true) {
        struct rose_surface* surface = NULL;
        wl_list_for_each(surface, &(workspace->surfaces), link) {
            wlr_surface_send_leave(surface->xdg_surface->surface, output->dev);
        }
    }

    // Update output's focus, if needed.
    if(output->focused_workspace == workspace) {
        // Select workspace's successor.
        struct rose_workspace* successor = //
            rose_output_select_next_workspace(
                output, workspace, rose_output_focus_direction_forward);

        // Focus the successor, but make sure that the workspace is not its
        // own successor.
        rose_output_focus_workspace(
            output, ((successor == workspace) ? NULL : successor));
    }

    // Sever all links between the workspace and the output.
    wl_list_remove(&(workspace->link_output));
    wl_list_init(&(workspace->link_output));

    workspace->output = NULL;

    // Add the workspace to the appropriate list.
    if(wl_list_empty(&(workspace->surfaces)) &&
       !rose_workspace_is_current(workspace)) {
        // If the workspace is empty and not current, then add it to the list of
        // available workspaces.
        wl_list_remove(&(workspace->link));
        wl_list_insert(rose_workspace_find_position_in_list(
                           &(workspace->ctx->workspaces), workspace,
                           offsetof(struct rose_workspace, link)),
                       &(workspace->link));

        // And reset its panel.
        workspace->panel = workspace->panel_saved = output->ctx->config.panel;
    } else {
        // Otherwise, add it to the list of workspaces without output.
        wl_list_insert(&(workspace->ctx->workspaces_without_output),
                       &(workspace->link_output));
    }
}

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_request_redraw(struct rose_output* output) {
    // Reset the number of frames without damage taken.
    output->n_frames_without_damage = 0;

    // Schedule a frame.
    rose_output_schedule_frame(output);
}

void
rose_output_schedule_frame(struct rose_output* output) {
    // Only schedule a frame if it hasn't been already scheduled.
    if(!output->is_frame_scheduled) {
        output->is_frame_scheduled = true;

        wlr_output_schedule_frame(output->dev);
    }
}

////////////////////////////////////////////////////////////////////////////////
// State query interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_state
rose_output_state_obtain(struct rose_output* output) {
    // Compute output's DPI.
    int dpi = (int)(output->dev->scale * 96.0 + 0.5), w = 0, h = 0;

    // Compute output's width and height.
    if(true) {
        bool flip = ((output->dev->transform % 2) != 0);
        w = (flip ? output->dev->height : output->dev->width);
        h = (flip ? output->dev->width : output->dev->height);
    }

    // Return output's state.
    return (struct rose_output_state){
        .id = output->id,
        .transform = output->dev->transform,
        .dpi = dpi,
        .rate = output->dev->refresh,
        .w = w,
        .h = h,
        .scale = output->dev->scale,
        .is_scanned_out = output->is_scanned_out,
        .is_frame_scheduled = output->is_frame_scheduled,
        .is_text_buffers_update_requested =
            output->is_text_buffers_update_requested};
}

struct rose_output_mode_list
rose_output_mode_list_obtain(struct rose_output* output) {
    return output->modes;
}

////////////////////////////////////////////////////////////////////////////////
// Cursor manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_cursor_set(struct rose_output* output,
                       enum rose_output_cursor_type type) {
    // Do nothing if output's cursor doesn't change its type.
    if(output->cursor.type == type) {
        return;
    }

    // Set cursor's type.
    output->cursor.type = type;

    // Do nothing else if the type is "unspecified".
    if(output->cursor.type == rose_output_cursor_type_unspecified) {
        return;
    }

    // Set cursor's image.
    if((output->cursor.type == rose_output_cursor_type_client) &&
       (output->cursor.is_client_surface_set)) {
        // If output's cursor has a client-set surface, and corresponding type
        // is specified, then use such surface.
        wlr_output_cursor_set_surface(
            output->cursor.wlr_cursor, output->cursor.client_surface,
            output->cursor.hotspot_x, output->cursor.hotspot_y);
    } else {
        // Otherwise, find corresponding image for the specified type.
        struct wlr_xcursor_image* image = rose_server_context_get_cursor_image(
            output->ctx, output->cursor.type, output->dev->scale);

        // And set this image.
        wlr_output_cursor_set_image(
            output->cursor.wlr_cursor, image->buffer, image->width * 4,
            image->width, image->height, image->hotspot_x, image->hotspot_y);
    }

    // Set cursor's movement flag.
    output->cursor.has_moved = true;

    // Schedule a frame, if needed.
    rose_output_schedule_frame(output);
}

void
rose_output_cursor_warp(struct rose_output* output, double x, double y) {
    // Set cursor's position.
    wlr_output_cursor_move(output->cursor.wlr_cursor, x, y);

    // Set cursor's movement flag.
    output->cursor.has_moved = true;

    // Schedule a frame, if needed.
    rose_output_schedule_frame(output);
}

void
rose_output_cursor_client_surface_set( //
    struct rose_output* output, struct wlr_surface* surface, int32_t hotspot_x,
    int32_t hotspot_y) {
    // Remove listener from the signal.
    wl_list_remove(&(output->listener_cursor_client_surface_destroy.link));
    wl_list_init(&(output->listener_cursor_client_surface_destroy.link));

    // Set the flag.
    output->cursor.is_client_surface_set = true;

    // Set the surface and its parameters.
    output->cursor.client_surface = surface;
    output->cursor.hotspot_x = hotspot_x;
    output->cursor.hotspot_y = hotspot_y;

    // Register listener, if needed.
    if(surface != NULL) {
        wl_signal_add(&(surface->events.destroy),
                      &(output->listener_cursor_client_surface_destroy));
    }
}
