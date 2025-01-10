// Copyright Nezametdinov E. Ildus 2025.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "rendering.h"
#include "rendering_raster.h"
#include "server_context.h"

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/render/swapchain.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define unused_(x) ((void)(x))

#define remove_signal_(f)                             \
    {                                                 \
        wl_list_remove(&(output->listener_##f.link)); \
        wl_list_init(&(output->listener_##f.link));   \
    }

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

////////////////////////////////////////////////////////////////////////////////
// Title string composition utility function.
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
        char const* format = "\xEF\x89\xAC %02d / %02d";
        snprintf(
            string, rose_output_utf8_string_size_max, format, id_output,
            id_workspace);
    } else {
        // Otherwise, initialize a buffer for focused surface's title.
        char surface_title[rose_output_utf8_buffer_size] = {};

        // Fill it.
        if(true) {
            struct rose_utf8_string title = rose_convert_ntbs_to_utf8(
                workspace->focused_surface->xdg_surface->toplevel->title);

            memcpy(
                surface_title, title.data,
                min_(title.size, rose_output_utf8_string_size_max));
        }

        // And print all the relevant data.
        char const* format = "\xEF\x89\xAC %02d / %02d \xEF\x89\x8D %s";
        snprintf(
            string, rose_output_utf8_string_size_max, format, id_output,
            id_workspace, surface_title);
    }

    // Convert resulting UTF-8 string to UTF-32 string.
    return rose_convert_utf8_to_utf32(rose_convert_ntbs_to_utf8(string));
}

////////////////////////////////////////////////////////////////////////////////
// Raster initialization utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_raster*
rose_output_raster_initialize(
    struct rose_raster* raster, struct wlr_renderer* renderer, int width,
    int height) {
    // Clamp the dimensions.
    width = clamp_(width, 1, 32768);
    height = clamp_(height, 1, 32768);

    // Do nothing else if the raster is already initialized.
    if((raster != NULL) && (raster->base.width == width) &&
       (raster->base.height == height)) {
        return raster;
    }

    // Destroy existing raster, if any.
    if(raster != NULL) {
        rose_raster_destroy(raster);
    }

    // Initialize a new raster.
    return rose_raster_initialize(renderer, width, height);
}

////////////////////////////////////////////////////////////////////////////////
// Raster manipulation-related utility functions and type.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_rasters_update_type {
    rose_output_rasters_update_normal,
    rose_output_rasters_update_forced
};

static void
rose_output_update_rasters(
    struct rose_output* output,
    enum rose_output_rasters_update_type update_type) {
    // Obtain output's focused workspace.
    struct rose_workspace* workspace = output->focused_workspace;

    // Do nothing else if there is no focused workspace.
    if(workspace == NULL) {
        return;
    }

    // Obtain output's state.
    struct rose_output_state output_state = rose_output_state_obtain(output);

    // Obtain text rendering-related data.
    struct rose_text_rendering_context* text_rendering_context =
        output->context->text_rendering_context;

    struct rose_text_rendering_parameters text_rendering_parameters = {
        .font_size = output->context->config.theme.font_size,
        .dpi = output_state.dpi};

    // Obtain the color scheme.
    struct rose_color_scheme const* color_scheme =
        &(output->context->config.theme.color_scheme);

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

    // Update title's raster, if needed.
    while(panel.is_visible) {
        // Stop the update, if needed.
        if((update_type == rose_output_rasters_update_normal) &&
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

        // Compute raster's dimensions.
        int width =
                ((((panel.position == rose_ui_panel_position_left) ||
                   (panel.position == rose_ui_panel_position_right))
                      ? output_state.height
                      : output_state.width) /
                 2),
            height = (int)(panel.size * output_state.scale + 0.5);

        // Initialize the raster.
        struct rose_raster* raster = output->rasters.title =
            rose_output_raster_initialize(
                output->rasters.title, output->context->renderer, width,
                height);

        // Stop the update if the raster is not initialized.
        if(raster == NULL) {
            break;
        }

        // Clear the raster.
        rose_raster_clear(raster);

        // Set text's color.
        text_rendering_parameters.color = color_scheme->panel_foreground;

        // Initialize a pixel buffer for text rendering.
        struct rose_pixel_buffer pixels = {
            .data = raster->pixels,
            .width = raster->base.width,
            .height = raster->base.height};

        // Compose and render the title.
        rose_render_string(
            text_rendering_context, text_rendering_parameters,
            rose_output_workspace_compose_title_string(workspace), pixels);

        // Update raster's texture.
        pixman_region32_t region = {
            .extents = {
                .x1 = 0, .y1 = 0, .x2 = pixels.width, .y2 = pixels.height}};

        rose_raster_texture_update(raster, &region);

        // Note: Update succeeded.
        break;
    }

    // Update menu's raster, if needed.
    for(struct rose_ui_menu* menu = &(output->ui.menu);
        menu->is_visible &&
        (menu->is_updated ||
         (update_type == rose_output_rasters_update_forced));) {
        // Compute raster's dimensions.
        int width = (int)(menu->area.width * output_state.scale + 0.5),
            height = (int)(menu->area.height * output_state.scale + 0.5);

        // Initialize the raster.
        struct rose_raster* raster = output->rasters.menu =
            rose_output_raster_initialize(
                output->rasters.menu, output->context->renderer, width, height);

        // Stop the update if the raster is not initialized.
        if(raster == NULL) {
            break;
        }

        // Obtain menu's previous and current texts.
        struct rose_ui_menu_text text_prev = output->ui_menu_text;
        struct rose_ui_menu_text text = rose_ui_menu_text_obtain(menu);

        // Save menu's current text.
        output->ui_menu_text = text;

        // Set text's color.
        text_rendering_parameters.color = color_scheme->menu_foreground;

        // Render the text line-by-line, compute raster's updated area.
        int updated_area[2] = {-1, 0};
        if(true) {
            // Compute menu line's height in raster's space.
            int line_height =
                (int)(menu->layout.line_height * output_state.scale + 0.5);

            // Initialize a pixel buffer for a text line.
            struct rose_pixel_buffer line_pixels = {
                .width = raster->base.width, .height = line_height};

            // Compute text line's stride.
            ptrdiff_t line_stride = 4 * line_pixels.width * line_pixels.height;

            // Render the lines.
            int space_left = raster->base.height;
            for(ptrdiff_t i = 0; i < text.line_count; ++i) {
                // Stop rendering if there is no space left in the raster.
                if(space_left <= 0) {
                    break;
                }

                // Compute current line's parameters.
                line_pixels.data = raster->pixels + i * line_stride;
                line_pixels.height = min_(line_pixels.height, space_left);

                // Update raster's available space.
                space_left -= line_height;

#define line_diff_(a, b)   \
    ((a.size != b.size) || \
     (memcmp(a.data, b.data, a.size * sizeof(a.data[0])) != 0))

                // A flag which shows that current line must be rendered.
                bool must_render_line =
                    (menu->is_layout_updated) || (i >= text_prev.line_count) ||
                    (line_diff_(text.lines[i], text_prev.lines[i]));

#undef line_diff_

                // Render the line, if needed.
                if(must_render_line) {
                    // Compute line's offset.
                    int dy = i * line_height;

                    // Compute updated area.
                    updated_area[0] =
                        ((updated_area[0] < 0) ? dy : updated_area[0]);
                    updated_area[1] = dy + line_pixels.height;

                    // Clear line's pixel buffer.
                    memset(
                        line_pixels.data, 0,
                        4 * line_pixels.width * line_pixels.height);

                    // Render the current line.
                    rose_render_string(
                        text_rendering_context, text_rendering_parameters,
                        text.lines[i], line_pixels);
                }
            }
        }

        // Update raster's texture, if needed.
        if(updated_area[0] >= 0) {
            pixman_region32_t region = {
                .extents = {
                    .x1 = 0,
                    .y1 = updated_area[0],
                    .x2 = raster->base.width,
                    .y2 = updated_area[1]}};

            rose_raster_texture_update(raster, &region);
        }

        // Clear menu's flags.
        menu->is_updated = menu->is_layout_updated = false;

        // Note: Update succeeded.
        break;
    }
}

static void
rose_output_request_rasters_update(struct rose_output* output) {
    // Set the flag.
    output->is_rasters_update_requested = true;

    // Schedule a frame.
    rose_output_schedule_frame(output);
}

////////////////////////////////////////////////////////////////////////////////
// Workspace-related utility functions.
////////////////////////////////////////////////////////////////////////////////

static struct rose_workspace*
rose_output_select_next_workspace(
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
       wl_list_empty(&(output->context->workspaces_without_output))) {
        if(!wl_list_empty(&(output->context->workspaces))) {
            // Obtain the first available workspace.
            struct rose_workspace* workspace = wl_container_of(
                output->context->workspaces.prev, workspace, link);

            // Remove it from the list of available workspaces.
            wl_list_remove(&(workspace->link));
            wl_list_init(&(workspace->link));

            // Add it to the list of workspaces without output.
            wl_list_remove(&(workspace->link_output));
            wl_list_insert(
                &(workspace->context->workspaces_without_output),
                &(workspace->link_output));
        }
    }

    // Add workspaces to the output.
    if(true) {
        struct rose_workspace* workspace = NULL;
        struct rose_workspace* _ = NULL;

        wl_list_for_each_safe(
            workspace, _, &(output->context->workspaces_without_output),
            link_output) {
            // Remove the workspace from the list of available workspaces.
            wl_list_remove(&(workspace->link));
            wl_list_init(&(workspace->link));

            // Link the workspace with the output.
            wl_list_remove(&(workspace->link_output));
            wl_list_insert(
                rose_workspace_find_position_in_list(
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
                rose_surface_output_enter(surface, output);
            }
        }
    }

    // Update output's focus, if needed.
    if(output == output->context->current_workspace->output) {
        rose_output_focus_workspace(output, output->context->current_workspace);
    } else if(output->focused_workspace == NULL) {
        rose_output_focus_workspace_relative(
            output, rose_output_focus_direction_forward);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Damage handling utility functions.
////////////////////////////////////////////////////////////////////////////////

static struct rose_output_damage
rose_output_damage_construct(pixman_region32_t const* region) {
    return (struct rose_output_damage){
        .x = region->extents.x1,
        .y = region->extents.y1,
        .width = (region->extents.x2 - region->extents.x1),
        .height = (region->extents.y2 - region->extents.y1)};
}

static struct rose_output_damage
rose_output_damage_compute_union(
    struct rose_output_damage a, struct rose_output_damage b) {
    if((a.width == 0) || (a.height == 0)) {
        return b;
    } else if((b.width == 0) || (b.height == 0)) {
        return a;
    }

#define shift_(damage)            \
    (damage).width += (damage).x; \
    (damage).height += (damage).y;

    shift_(a);
    shift_(b);

#undef shift_

    a.x = min_(a.x, b.x);
    a.y = min_(a.y, b.y);

    a.width = max_(a.width, b.width) - a.x;
    a.height = max_(a.height, b.height) - a.y;

    return a;
}

static struct rose_output_damage
rose_output_damage_transform(
    struct rose_output_damage source, struct rose_output_state state) {
#define scale_(x) (int)(0.5 + (x) * state.scale)

    // Scale the source rectangle.
    if(true) {
        source.width += source.x;
        source.height += source.y;

        source.x = scale_(source.x);
        source.y = scale_(source.y);
        source.width = scale_(source.width) - source.x;
        source.height = scale_(source.height) - source.y;
    }

#undef scale_

    // Initialize resulting damage rectangle.
    struct rose_output_damage result = source;

    // Transform rectangle's size.
    if(state.transform % 2 != 0) {
        result.width = source.height;
        result.height = source.width;
    }

    // Transform rectangle's position.
    switch(state.transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
            break;

        case WL_OUTPUT_TRANSFORM_90:
            result.x = source.y;
            result.y = state.width - source.x - source.width;
            break;

        case WL_OUTPUT_TRANSFORM_180:
            result.x = state.width - source.x - source.width;
            result.y = state.height - source.y - source.height;
            break;

        case WL_OUTPUT_TRANSFORM_270:
            result.x = state.height - source.y - source.height;
            result.y = source.x;
            break;

        case WL_OUTPUT_TRANSFORM_FLIPPED:
            result.x = state.width - source.x - source.width;
            break;

        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            result.x = state.height - source.y - source.height;
            result.y = state.width - source.x - source.width;
            break;

        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            result.y = state.height - source.y - source.height;
            break;

        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            result.x = source.y;
            result.y = source.x;
            break;

        default:
            break;
    }

    return result;
}

static struct rose_output_damage
rose_output_damage_obtain(struct wlr_surface* surface) {
    // Initialize an empty region.
    pixman_region32_t region;
    pixman_region32_init(&region);

    // Construct resulting damage.
    struct rose_output_damage result = rose_output_damage_construct(
        (wlr_surface_get_effective_damage(surface, &region), &region));

    // Return resulting damage.
    return pixman_region32_fini(&region), result;
}

////////////////////////////////////////////////////////////////////////////////
// Surface notification-related utility function.
////////////////////////////////////////////////////////////////////////////////

static void
rose_output_surface_send_frame_done(
    struct wlr_surface* surface, int x, int y, void* data) {
    unused_(x), unused_(y);

    // Send frame done event to the surface.
    wlr_surface_send_frame_done(surface, data);
}

////////////////////////////////////////////////////////////////////////////////
// Event handlers.
////////////////////////////////////////////////////////////////////////////////

static void
rose_handle_event_output_frame(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_frame);

    // Reset this flag, since at this point no frame is scheduled.
    output->is_frame_scheduled = false;

    // Do nothing else if the output is disabled.
    if(!output->device->enabled) {
        return;
    }

    // Get current timestamp.
    struct timespec timestamp = {};
    clock_gettime(CLOCK_MONOTONIC, &timestamp);

    // Determine if redraw is required.
    bool is_redraw_required =
        (output->is_rasters_update_requested) ||
        (output->damage_tracker.frame_without_damage_count < 2);

    // Update output's damage tracking data.
    output->damage_tracker.frame_without_damage_count++;
    output->damage_tracker.frame_without_damage_count =
        min_(output->damage_tracker.frame_without_damage_count, 2);

    // Obtain output's focused workspace.
    struct rose_workspace* workspace = output->focused_workspace;

    // If there is a focused workspace, then perform additional actions.
    if(workspace != NULL) {
        // Force workspace's running transaction to stop if too much time has
        // passed since it started.
        if(workspace->transaction.sentinel > 0) {
            if(fabs(difftime(
                   timestamp.tv_sec,
                   workspace->transaction.start_time.tv_sec)) >= 1.0) {
                rose_workspace_transaction_commit(workspace);
            }
        }
    }

    // Update output's rasters, if needed.
    if(is_redraw_required) {
        // Determine the type of the update.
        enum rose_output_rasters_update_type update_type =
            (output->is_rasters_update_requested
                 ? rose_output_rasters_update_forced
                 : rose_output_rasters_update_normal);

        // Clear the flag.
        output->is_rasters_update_requested = false;

        // Update the rasters.
        rose_output_update_rasters(output, update_type);
    }

    // If there is no need to do full redraw, then perform additional actions
    // depending on whether or not the cursor has moved, and whether or not the
    // output was in direct scan-out mode.
    if(!is_redraw_required) {
        if(output->cursor.has_moved &&
           (output->cursor.drag_and_drop_surface == NULL)) {
            if(output->is_scanned_out) {
                // If the output was in direct scan-out mode, then try using
                // this mode again by rendering output's content.
                rose_render_content(output);
            } else {
                // Otherwise, swap buffers.
                if(output->device->swapchain != NULL) {
                    // Initialize an empty state.
                    struct wlr_output_state state = {};
                    wlr_output_state_init(&state);

                    // Initialize buffer age.
                    int buffer_age = -1;

                    // Acquire the next buffer from output's swapchain.
                    struct wlr_buffer* buffer = wlr_swapchain_acquire(
                        output->device->swapchain, &buffer_age);

                    // Consume damage.
                    rose_output_consume_damage(output, buffer_age);

                    // Set acquired buffer as current.
                    wlr_output_state_set_buffer(&state, buffer);
                    wlr_buffer_unlock(buffer);

                    // Commit the state.
                    wlr_output_commit_state(output->device, &state);

                    // Clean-up the state data.
                    wlr_output_state_finish(&state);
                } else {
                    wlr_output_schedule_frame(output->device);
                }
            }

            // Go to the end of the routine to update the flags.
            goto end;
        } else if(
            output->cursor.has_moved &&
            (output->cursor.drag_and_drop_surface != NULL)) {
            // Proceed with rendering.
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
    if(!(output->context->is_screen_locked)) {
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
                    wlr_xdg_surface_for_each_surface(
                        surface->xdg_surface,
                        rose_output_surface_send_frame_done, &timestamp);
                }
            }
        }
    }

    // Send frame done events to all visible widgets.
    if(true) {
        struct rose_surface* surface = NULL;
        for(ptrdiff_t i = 0; i != rose_surface_widget_type_count_; ++i) {
            wl_list_for_each(
                surface, &(output->ui.surfaces_mapped[i]), link_mapped) {
                if(rose_output_ui_is_surface_visible(&(output->ui), surface)) {
                    wlr_xdg_surface_for_each_surface(
                        surface->xdg_surface,
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

    // Obtain the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_needs_frame);

    // Schedule a frame.
    output->is_frame_scheduled =
        (wlr_output_schedule_frame(output->device), true);
}

static void
rose_handle_event_output_commit(struct wl_listener* listener, void* data) {
    // Obtain the event.
    struct wlr_output_event_commit* event = data;

    // Obtain the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_commit);

    // Compute the state mask.
    uint32_t mask = WLR_OUTPUT_STATE_SCALE | WLR_OUTPUT_STATE_TRANSFORM |
                    WLR_OUTPUT_STATE_MODE;

    if((event->state->committed & mask) != 0) {
        // Update the UI.
        rose_output_ui_update(&(output->ui));

        // Notify all workspaces which belong to this output.
        if(true) {
            struct rose_workspace* workspace;
            wl_list_for_each(workspace, &(output->workspaces), link_output) {
                rose_workspace_notify_output_mode(workspace, output);
            }
        }

        // Request update of output's rasters.
        rose_output_request_rasters_update(output);
    }
}

static void
rose_handle_event_output_damage(struct wl_listener* listener, void* data) {
    // Obtain the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_damage);

    // Obtain the event.
    struct wlr_output_event_damage* event = data;

    // Damage the output.
    rose_output_add_damage(output, rose_output_damage_construct(event->damage));
}

static void
rose_handle_event_output_destroy(struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_destroy);

    // Destroy the output.
    rose_output_destroy(output);
}

static void
rose_handle_event_output_cursor_surface_destroy(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the output.
    struct rose_output* output =
        wl_container_of(listener, output, listener_cursor_surface_destroy);

    // Remove listener from the signal.
    remove_signal_(cursor_surface_destroy);

    // Reset the surface pointer and clear the flag.
    output->cursor.surface = NULL;
    output->cursor.is_surface_set = false;
}

static void
rose_handle_event_output_cursor_drag_and_drop_surface_destroy(
    struct wl_listener* listener, void* data) {
    unused_(data);

    // Obtain the output.
    struct rose_output* output = wl_container_of(
        listener, output, listener_cursor_drag_and_drop_surface_destroy);

    // Remove listener from the signal.
    remove_signal_(cursor_drag_and_drop_surface_destroy);

    // Reset the surface pointer.
    output->cursor.drag_and_drop_surface = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_initialize(
    struct rose_server_context* context, struct wlr_output* device) {
    // Initialize output's rendering subsystem.
    if(!wlr_output_init_render(device, context->allocator, context->renderer)) {
        return;
    }

    // Create and initialize a new cursor.
    struct wlr_cursor* cursor = wlr_cursor_create();
    if(cursor == NULL) {
        return;
    }

    // Create and initialize a new layout.
    struct wlr_output_layout* layout =
        wlr_output_layout_create(context->display);

    if(layout == NULL) {
        wlr_cursor_destroy(cursor);
        return;
    }

    // Allocate and initialize a new output.
    struct rose_output* output = malloc(sizeof(struct rose_output));
    if(output == NULL) {
        wlr_output_layout_destroy(layout);
        wlr_cursor_destroy(cursor);
        return;
    } else {
        *output = (struct rose_output){
            .context = context,
            .device = device,
            .layout = layout,
            .cursor = {.underlying = cursor}};
    }

    // Add output to the layout.
    wlr_output_layout_add_auto(layout, device);

    // Configure the cursor.
    wlr_cursor_attach_output_layout(cursor, layout);
    wlr_cursor_map_to_output(cursor, device);

    // Initialize an empty state.
    struct wlr_output_state state = {};
    wlr_output_state_init(&state);

    // Set device's mode, if needed.
    if(!wl_list_empty(&(device->modes))) {
        wlr_output_state_set_mode(&state, wlr_output_preferred_mode(device));
    }

    // Enable the device.
    wlr_output_state_set_enabled(&state, true);
    wlr_output_commit_state(device, &state);
    wlr_output_state_finish(&state);

    // Initialize the list of output modes.
    if(true) {
        struct wlr_output_mode* mode = NULL;
        wl_list_for_each(mode, &(device->modes), link) {
            // Limit the number of modes.
            if(output->modes.size == rose_output_mode_list_size_max) {
                break;
            }

            // Save the current mode.
            output->modes.data[output->modes.size++] =
                (struct rose_output_mode){
                    .width = mode->width,
                    .height = mode->height,
                    .rate = mode->refresh};
        }
    }

    // Initialize output's list of workspaces.
    wl_list_init(&(output->workspaces));

    // Initialize output's UI.
    rose_output_ui_initialize(&(output->ui), output);

    // Add this output to the list.
    wl_list_insert(&(context->outputs), &(output->link));

    // Set output's ID.
    if(output->link.next != &(output->context->outputs)) {
        output->id = (wl_container_of(output->link.next, output, link))->id + 1;
    }

    // Broadcast output's initialization event though IPC.
    if(true) {
        struct rose_ipc_status status = {
            .type = rose_ipc_status_type_output_initialized,
            .device_id = output->id};

        rose_ipc_server_broadcast_status(output->context->ipc_server, status);
    }

#define add_signal_(f)                                               \
    {                                                                \
        output->listener_##f.notify = rose_handle_event_output_##f;  \
        wl_signal_add(&(device->events.f), &(output->listener_##f)); \
    }

#define initialize_(f)                                              \
    {                                                               \
        output->listener_##f.notify = rose_handle_event_output_##f; \
        wl_list_init(&(output->listener_##f.link));                 \
    }

    // Register and initialize listeners.
    add_signal_(frame);
    add_signal_(needs_frame);

    add_signal_(commit);
    add_signal_(damage);

    add_signal_(destroy);
    initialize_(cursor_surface_destroy);
    initialize_(cursor_drag_and_drop_surface_destroy);

#undef initialize_
#undef add_signal_

    // Set output cursor's type.
    rose_output_cursor_set(output, rose_output_cursor_type_default);

    // Request update of output's rasters.
    rose_output_request_rasters_update(output);

    // Add workspaces to the output.
    rose_output_add_workspaces(output);

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_output, .data = output};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->context->menus_visible), link) {
            rose_ui_menu_notify_line_add(menu, line);
        }
    }

    // Apply preferences.
    rose_output_apply_preferences(output, context->preference_list);
}

void
rose_output_destroy(struct rose_output* output) {
    // Hide the menu.
    rose_ui_menu_hide(&(output->ui.menu));

    // Broadcast output's destruction event though IPC.
    if(true) {
        struct rose_ipc_status status = {
            .type = rose_ipc_status_type_output_destroyed,
            .device_id = output->id};

        rose_ipc_server_broadcast_status(output->context->ipc_server, status);
    }

    // Update IDs of all outputs which precede this one.
    for(struct rose_output* x = output;
        x->link.prev != &(output->context->outputs);) {
        // Obtain preceding output.
        x = wl_container_of(x->link.prev, x, link);

        // Decrement its ID and request update of its rasters.
        x->id--, rose_output_request_rasters_update(x);
    }

    // Destroy output's rasters.
    rose_raster_destroy(output->rasters.title);
    rose_raster_destroy(output->rasters.menu);

    // Remove listeners from signals.
    remove_signal_(frame);
    remove_signal_(needs_frame);

    remove_signal_(commit);
    remove_signal_(damage);

    remove_signal_(destroy);
    remove_signal_(cursor_surface_destroy);
    remove_signal_(cursor_drag_and_drop_surface_destroy);

    // Destroy the cursor.
    wlr_cursor_destroy(output->cursor.underlying);

    // Destroy the layout.
    wlr_output_layout_destroy(output->layout);

    // Select output's successor, make sure that the output is not its own
    // successor.
    struct rose_output* successor =
        ((output->link.next == &(output->context->outputs))
             ? wl_container_of(output->context->outputs.next, successor, link)
             : wl_container_of(output->link.next, successor, link));

    successor = ((successor == output) ? NULL : successor);

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_output, .data = output};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->context->menus_visible), link) {
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
                rose_surface_output_leave(surface, output);
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
                wl_list_insert(
                    rose_workspace_find_position_in_list(
                        &(workspace->context->workspaces), workspace,
                        offsetof(struct rose_workspace, link)),
                    &(workspace->link));

                // And reset its panel.
                workspace->panel = workspace->panel_saved =
                    output->context->config.theme.panel;
            } else {
                // Otherwise, add it to the list of workspaces without output.
                wl_list_insert(
                    &(workspace->context->workspaces_without_output),
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
        wl_list_for_each(menu, &(output->context->menus_visible), link) {
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
rose_output_configure(
    struct rose_output* output,
    struct rose_output_configuration_parameters parameters) {
    // If requested configuration is a no-op, then return success.
    if(parameters.flags == 0) {
        return true;
    }

    // If the given output is disabled, then configuration fails.
    if((output->device == NULL) || !output->device->enabled) {
        return false;
    }

    // Check the validity of the specified parameters.

    if((parameters.flags & rose_output_configure_transform) != 0) {
        // Note: These bounds are determined by the Wayland protocol.
        if((parameters.transform < 0) || (parameters.transform > 7)) {
            return false;
        }
    }

    if((parameters.flags & rose_output_configure_scale) != 0) {
        // Note: The scaling factor can not be NaN or infinity.
        if(!isfinite(parameters.scale)) {
            return false;
        }

        // Note: The scaling factor must have sane value.
        if((parameters.scale < 0.1) || (parameters.scale > 25.0)) {
            return false;
        }
    }

    // Note: Requested mode is not checked, since if it does not belong to the
    // list of possible modes, then it will not be applied.

    // Initialize an empty state.
    struct wlr_output_state state = {};
    wlr_output_state_init(&state);

    // Set specified parameters.

    if((parameters.flags & rose_output_configure_adaptive_sync) != 0) {
        wlr_output_state_set_adaptive_sync_enabled(
            &state, parameters.adaptive_sync_state);
    }

    if((parameters.flags & rose_output_configure_transform) != 0) {
        wlr_output_state_set_transform(&state, parameters.transform);
    }

    if((parameters.flags & rose_output_configure_scale) != 0) {
        wlr_output_state_set_scale(&state, (float)(parameters.scale));
    }

    while((parameters.flags & rose_output_configure_mode) != 0) {
        // If default mode has been requested, and the list of modes is not
        // empty, then set such mode.
        if(((parameters.mode.width == 0) &&  //
            (parameters.mode.height == 0) && //
            (parameters.mode.rate == 0)) &&
           !wl_list_empty(&(output->device->modes))) {
            wlr_output_state_set_mode(
                &state, wlr_output_preferred_mode(output->device));

            break;
        }

        // Set the requested mode if it matches one of the available modes.
        struct wlr_output_mode* mode = NULL;
        wl_list_for_each(mode, &(output->device->modes), link) {
            if((mode->width == parameters.mode.width) &&
               (mode->height == parameters.mode.height) &&
               (mode->refresh == parameters.mode.rate)) {
                wlr_output_state_set_mode(&state, mode);
                break;
            }
        }

        // Finish mode setting.
        break;
    }

    // Commit output's state.
    bool result = wlr_output_commit_state(output->device, &state);
    wlr_output_state_finish(&state);

    // Update device preference list, if needed.
    if(result && (output->context->preference_list != NULL)) {
        // Initialize device preference.
        struct rose_device_preference preference = {
            .device_name = rose_output_name_obtain(output),
            .device_type = rose_device_type_output,
            .parameters = {.output = parameters}};

        // Perform update operation.
        rose_device_preference_list_update(
            output->context->preference_list, preference);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Workspace focusing interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_focus_workspace(
    struct rose_output* output, struct rose_workspace* workspace) {
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
                workspace, workspace->pointer.movement_time,
                workspace_prev->pointer.x, workspace_prev->pointer.y);
        } else {
            rose_workspace_pointer_warp(
                workspace, workspace->pointer.movement_time,
                workspace->pointer.x, workspace->pointer.y);
        }

        // Update output's UI components.
        rose_output_ui_update(&(output->ui));

        // And request workspace's redraw.
        rose_workspace_request_redraw(workspace);
    } else {
        // Otherwise, mark the output as damaged.
        output->damage_tracker.frame_without_damage_count = 0;

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

    // Request update of output's rasters.
    rose_output_request_rasters_update(output);
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
rose_output_add_workspace(
    struct rose_output* output, struct rose_workspace* workspace) {
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
    wl_list_insert(
        rose_workspace_find_position_in_list(
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
            rose_surface_output_enter(surface, output);
        }
    }

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_workspace, .data = workspace};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->context->menus_visible), link) {
            rose_ui_menu_notify_line_add(menu, line);
        }
    }
}

void
rose_output_remove_workspace(
    struct rose_output* output, struct rose_workspace* workspace) {
    // Do nothing if the given output does not contain the workspace.
    if(output != workspace->output) {
        return;
    }

    // Notify all visible menus.
    if(true) {
        struct rose_ui_menu_line line = {
            .type = rose_ui_menu_line_type_workspace, .data = workspace};

        struct rose_ui_menu* menu = NULL;
        wl_list_for_each(menu, &(output->context->menus_visible), link) {
            rose_ui_menu_notify_line_remove(menu, line);
        }
    }

    // Send output leave events to all surfaces which belong to the workspace.
    if(true) {
        struct rose_surface* surface = NULL;
        wl_list_for_each(surface, &(workspace->surfaces), link) {
            rose_surface_output_leave(surface, output);
        }
    }

    // Update output's focus, if needed.
    if(output->focused_workspace == workspace) {
        // Select workspace's successor.
        struct rose_workspace* successor = rose_output_select_next_workspace(
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
        wl_list_insert(
            rose_workspace_find_position_in_list(
                &(workspace->context->workspaces), workspace,
                offsetof(struct rose_workspace, link)),
            &(workspace->link));

        // And reset its panel.
        workspace->panel = workspace->panel_saved =
            output->context->config.theme.panel;
    } else {
        // Otherwise, add it to the list of workspaces without output.
        wl_list_insert(
            &(workspace->context->workspaces_without_output),
            &(workspace->link_output));
    }
}

////////////////////////////////////////////////////////////////////////////////
// Damage handling interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_damage
rose_output_consume_damage(struct rose_output* output, int buffer_age) {
#define array_size_(a) ((ptrdiff_t)(sizeof(a) / sizeof((a)[0])))

    // Obtain damage array's size.
    ptrdiff_t const damage_array_size =
        array_size_(output->damage_tracker.damage);

    // Initialize an empty result.
    struct rose_output_damage result = {};

    // Obtain damage for the given age.
    if((buffer_age > 0) && (buffer_age < damage_array_size)) {
        // If the given age is inside the range, then obtain and transform
        // corresponding damage.
        result = rose_output_damage_transform(
            output->damage_tracker.damage[buffer_age],
            rose_output_state_obtain(output));
    } else {
        // If the given age is outside the range, then set the entire output's
        // area as the resulting damage.
        result.width = output->device->width;
        result.height = output->device->height;
    }

    // Shift damage in the tracker.
    output->damage_tracker.damage[0] = (struct rose_output_damage){};
    for(ptrdiff_t i = damage_array_size - 1; i != 0; --i) {
        output->damage_tracker.damage[i] = output->damage_tracker.damage[i - 1];
    }

    // Return the resulting damage.
    return result;
}

void
rose_output_add_damage(
    struct rose_output* output, struct rose_output_damage damage) {
    // Mark the output as damaged.
    output->damage_tracker.frame_without_damage_count = 0;

    // Add the damage.
    for(ptrdiff_t i = 0; i != array_size_(output->damage_tracker.damage); ++i) {
        output->damage_tracker.damage[i] = rose_output_damage_compute_union(
            output->damage_tracker.damage[i], damage);
    }

    // Schedule a frame.
    rose_output_schedule_frame(output);
}

void
rose_output_add_surface_damage(
    struct rose_output* output, struct rose_surface* surface) {
    // Initialize an empty damage.
    struct rose_output_damage damage = {};

    // Obtain surface's damage.
    if((surface->state.previous.x != surface->state.current.x) ||
       (surface->state.previous.y != surface->state.current.y) ||
       (surface->state.previous.width != surface->state.current.width) ||
       (surface->state.previous.height != surface->state.current.height)) {
        int shift = ((surface->type == rose_surface_type_toplevel) ? -5 : 0);
        int stretch = ((surface->type == rose_surface_type_toplevel) ? 10 : 0);

        // Damage previous surface's area.
        damage = (struct rose_output_damage){
            .x = surface->state.previous.x + shift,
            .y = surface->state.previous.y + shift,
            .width = surface->state.previous.width + stretch,
            .height = surface->state.previous.height + stretch};

        // Damage current surface's area.
        damage = rose_output_damage_compute_union(
            (struct rose_output_damage){
                .x = surface->state.current.x + shift,
                .y = surface->state.current.y + shift,
                .width = surface->state.current.width + stretch,
                .height = surface->state.current.height + stretch},
            damage);
    } else {
        if(surface->type != rose_surface_type_temporary) {
            // Obtain surface's damage.
            damage = rose_output_damage_obtain(
                (surface->type == rose_surface_type_subsurface)
                    ? surface->subsurface->surface
                    : surface->xdg_surface->surface);

            // Add surface's offset.
            damage.x += surface->state.current.x;
            damage.y += surface->state.current.y;
        } else {
            // Note: Testing showed wrong damage for temporary surfaces, so the
            // entire area is damaged.
            damage = (struct rose_output_damage){
                .x = surface->state.current.x,
                .y = surface->state.current.y,
                .width = surface->state.current.width,
                .height = surface->state.current.height};
        }
    }

    // Offset the damage.
    while(surface->type != rose_surface_type_toplevel) {
        // Obtain the parent surface.
        if(true) {
            struct wlr_surface* parent = NULL;
            if(surface->type == rose_surface_type_subsurface) {
                parent = surface->subsurface->parent;
            } else {
                parent = surface->xdg_surface->popup->parent;
            }

            if(parent != NULL) {
                struct wlr_xdg_surface* xdg_surface =
                    wlr_xdg_surface_try_from_wlr_surface(parent);

                if(xdg_surface != NULL) {
                    surface = (struct rose_surface*)(xdg_surface->data);
                } else {
                    struct wlr_subsurface* subsurface =
                        wlr_subsurface_try_from_wlr_surface(parent);

                    if(subsurface != NULL) {
                        surface = (struct rose_surface*)(subsurface->data);
                    } else {
                        break;
                    }
                }
            } else {
                surface = NULL;
            }
        }

        // Add parent surface's offset.
        if(surface != NULL) {
            damage.x += surface->state.current.x;
            damage.y += surface->state.current.y;
        } else {
            break;
        }
    }

    // Add the damage.
    rose_output_add_damage(output, damage);
}

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_request_redraw(struct rose_output* output) {
    // Obtain output's state.
    struct rose_output_state state = rose_output_state_obtain(output);

    // Damage the entire output.
    if(true) {
        struct rose_output_damage damage = {
            .width = (int)(0.5 + (state.width / state.scale)),
            .height = (int)(0.5 + (state.height / state.scale))};

        rose_output_add_damage(output, damage);
    }
}

void
rose_output_schedule_frame(struct rose_output* output) {
    if(!output->is_frame_scheduled) {
        output->is_frame_scheduled = true;

        wlr_output_schedule_frame(output->device);
    }
}

////////////////////////////////////////////////////////////////////////////////
// State query interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_state
rose_output_state_obtain(struct rose_output* output) {
    // Compute output's DPI.
    int dpi = (int)(output->device->scale * 96.0 + 0.5), width = 0, height = 0;

    // Compute output's width and height.
    if(true) {
        bool flip = ((output->device->transform % 2) != 0);
        width = (flip ? output->device->height : output->device->width);
        height = (flip ? output->device->width : output->device->height);
    }

    // Return output's state.
    return (struct rose_output_state){
        .id = output->id,
        .adaptive_sync_state = (enum rose_output_adaptive_sync_state)(
            output->device->adaptive_sync_status),
        .transform = output->device->transform,
        .dpi = dpi,
        .rate = output->device->refresh,
        .width = width,
        .height = height,
        .scale = output->device->scale,
        .is_scanned_out = output->is_scanned_out,
        .is_frame_scheduled = output->is_frame_scheduled,
        .is_rasters_update_requested = output->is_rasters_update_requested};
}

struct rose_output_mode_list
rose_output_mode_list_obtain(struct rose_output* output) {
    return output->modes;
}

////////////////////////////////////////////////////////////////////////////////
// Cursor manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_cursor_set(
    struct rose_output* output, enum rose_output_cursor_type type) {
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
       (output->cursor.is_surface_set)) {
        wlr_cursor_set_surface(
            output->cursor.underlying, output->cursor.surface,
            output->cursor.hotspot_x, output->cursor.hotspot_y);
    } else {
        struct rose_cursor_image image =
            rose_server_context_obtain_cursor_image(
                output->context, output->cursor.type, output->device->scale);

        wlr_cursor_set_buffer(
            output->cursor.underlying, &(image.raster->base), image.hotspot_x,
            image.hotspot_y, 1.0f);
    }

    // Set cursor's movement flag.
    output->cursor.has_moved = true;

    // Schedule a frame, if needed.
    rose_output_schedule_frame(output);
}

void
rose_output_cursor_warp(struct rose_output* output, double x, double y) {
    // Set cursor's position.
    wlr_cursor_warp_closest(output->cursor.underlying, NULL, x, y);

    // Set cursor's movement flag.
    output->cursor.has_moved = true;

    // Schedule a frame, if needed.
    rose_output_schedule_frame(output);
}

void
rose_output_cursor_client_surface_set(
    struct rose_output* output, struct wlr_surface* surface, int32_t hotspot_x,
    int32_t hotspot_y) {
    // Remove listener from the signal.
    remove_signal_(cursor_surface_destroy);

    // Set the flag.
    output->cursor.is_surface_set = true;

    // Set the surface and its parameters.
    output->cursor.surface = surface;
    output->cursor.hotspot_x = hotspot_x;
    output->cursor.hotspot_y = hotspot_y;

    // Register listener, if needed.
    if(surface != NULL) {
        wl_signal_add(
            &(surface->events.destroy),
            &(output->listener_cursor_surface_destroy));
    }
}

void
rose_output_cursor_drag_and_drop_surface_set(
    struct rose_output* output, struct wlr_surface* surface) {
    // Remove listener from the signal.
    remove_signal_(cursor_drag_and_drop_surface_destroy);

    // Set the surface.
    output->cursor.drag_and_drop_surface = surface;

    // Register listener, if needed.
    if(surface != NULL) {
        wl_signal_add(
            &(surface->events.destroy),
            &(output->listener_cursor_drag_and_drop_surface_destroy));
    }

    // Request redraw.
    rose_output_request_redraw(output);
}
