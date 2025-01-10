// Copyright Nezametdinov E. Ildus 2025.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "rendering.h"
#include "rendering_raster.h"
#include "server_context.h"

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/transform.h>

////////////////////////////////////////////////////////////////////////////////
// Rectangle definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_rectangle {
    // Rectangle's position and size.
    int x, y, width, height;

    // Rectangle's transformation in output's coordinate system.
    enum wl_output_transform transform;

    // A flag which shows that the rectangle is already transformed to the
    // output buffer's coordinates.
    bool is_transformed;
};

////////////////////////////////////////////////////////////////////////////////
// Rendering context definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_rendering_context {
    // Current output and its damage.
    struct rose_output* output;
    struct rose_output_damage damage;

    // Scissor rectangle. Limits rendering to the damaged area.
    pixman_region32_t scissor_rectangle;

    // Resulting output state and active rendering pass.
    struct wlr_output_state state;
    struct wlr_render_pass* pass;
};

////////////////////////////////////////////////////////////////////////////////
// Surface rendering context definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_surface_rendering_context {
    struct rose_rendering_context* parent;
    int dx, dy;
};

////////////////////////////////////////////////////////////////////////////////
// Rendering context initialization/finalization utility functions.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_rendering_context_initialize(
    struct rose_rendering_context* context, struct rose_output* output) {
    // Set the output.
    context->output = output;

    // Initialize an empty resulting state.
    wlr_output_state_init(&(context->state));

    // Configure output's primary swapchain.
    if(!wlr_output_configure_primary_swapchain(
           output->device, &(context->state), &(output->device->swapchain))) {
        return wlr_output_state_finish(&(context->state)), false;
    }

    // Start rendering operation, obtain current buffer age.
    int buffer_age = -1;
    context->pass = wlr_output_begin_render_pass(
        output->device, &(context->state), &buffer_age, NULL);

    // Consume current damage.
    context->damage = rose_output_consume_damage(output, buffer_age);

    // Initialize scissor rectangle.
    pixman_region32_init_rect(
        &(context->scissor_rectangle), //
        context->damage.x,             //
        context->damage.y,             //
        context->damage.width,         //
        context->damage.height);

    // Initialization succeeded.
    return true;
}

static void
rose_rendering_context_finalize(struct rose_rendering_context* context) {
    // Render software cursors.
    wlr_output_add_software_cursors_to_render_pass(
        context->output->device, context->pass, NULL);

    // Finish rendering operation.
    wlr_render_pass_submit(context->pass);

    // Commit resulting state.
    wlr_output_commit_state(context->output->device, &(context->state));

    // Clean-up resulting state.
    wlr_output_state_finish(&(context->state));

    // Clean-up the scissor rectangle.
    pixman_region32_fini(&(context->scissor_rectangle));
}

////////////////////////////////////////////////////////////////////////////////
// Rectangle transforming utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_rectangle
rose_rectangle_transform(
    struct rose_rectangle source, struct rose_output_state state) {
#define scale_(x) (int)((double)(x) * state.scale + 0.5)

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

    // Initialize resulting rectangle.
    struct rose_rectangle result = source;
    result.transform =
        wlr_output_transform_compose(state.transform, source.transform);

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

////////////////////////////////////////////////////////////////////////////////
// Rendering utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_render_rectangle(
    struct rose_rendering_context* context, struct rose_color color,
    struct rose_rectangle rectangle) {
    // Transform the rectangle.
    if(!(rectangle.is_transformed)) {
        rectangle = rose_rectangle_transform(
            rectangle, rose_output_state_obtain(context->output));
    }

    // Render the rectangle.
    struct wlr_render_rect_options options = {
        .box =
            {.x = rectangle.x,
             .y = rectangle.y,
             .width = rectangle.width,
             .height = rectangle.height},
        .color =
            {.r = color.rgba32[0],
             .g = color.rgba32[1],
             .b = color.rgba32[2],
             .a = color.rgba32[3]},
        .clip = &(context->scissor_rectangle)};

    wlr_render_pass_add_rect(context->pass, &options);
}

static void
rose_render_rectangle_with_texture(
    struct rose_rendering_context* context, struct wlr_texture* texture,
    struct wlr_fbox* region, struct rose_rectangle rectangle) {
    // Do nothing if there is no texture.
    if(texture == NULL) {
        return;
    }

    // Transform the rectangle.
    if(!(rectangle.is_transformed)) {
        rectangle = rose_rectangle_transform(
            rectangle, rose_output_state_obtain(context->output));
    }

    // Render the rectangle.
    struct wlr_render_texture_options options = {
        .texture = texture,
        .src_box = ((region != NULL) ? *region : (struct wlr_fbox){}),
        .dst_box =
            {.x = rectangle.x,
             .y = rectangle.y,
             .width = rectangle.width,
             .height = rectangle.height},
        .clip = &(context->scissor_rectangle),
        .transform = rectangle.transform};

    wlr_render_pass_add_texture(context->pass, &options);
}

static void
rose_render_surface(struct wlr_surface* surface, int x, int y, void* data) {
    // Obtain surface rendering context.
    struct rose_surface_rendering_context* context = data;

    // Compute a rectangle which will represent the given surface.
    struct rose_rectangle rectangle = {
        .x = x + context->dx,
        .y = y + context->dy,
        .width = surface->current.width,
        .height = surface->current.height,
        .transform = surface->current.transform};

    // Obtain the visible region of the surface's buffer.
    struct wlr_fbox region = {};
    wlr_surface_get_buffer_source_box(surface, &region);

    // Render the rectangle with surface's texture.
    rose_render_rectangle_with_texture(
        context->parent, wlr_surface_get_texture(surface), &region, rectangle);

    // Send presentation feedback.
    wlr_presentation_surface_textured_on_output(
        surface, context->parent->output->device);
}

static void
rose_render_surface_decoration(
    struct rose_rendering_context* context,
    struct rose_color_scheme const* color_scheme,
    struct rose_rectangle surface_rectangle) {
    // Update surface's rectangle.
    surface_rectangle.x -= 5;
    surface_rectangle.y -= 5;

    surface_rectangle.width += 10;
    surface_rectangle.height += 10;

    // Render surface's background frame.
    rose_render_rectangle(
        context, color_scheme->surface_background1, surface_rectangle);

    surface_rectangle.x += 1;
    surface_rectangle.y += 1;

    surface_rectangle.width -= 2;
    surface_rectangle.height -= 2;

    rose_render_rectangle(
        context, color_scheme->surface_background0, surface_rectangle);
}

static void
rose_render_widgets(
    struct rose_rendering_context* context,
    enum rose_surface_widget_type starting_widget_type,
    enum rose_surface_widget_type sentinel_widget_type) {
    // Iterate through the supplied range of widget types.
    struct rose_surface* surface = NULL;
    for(ptrdiff_t i = starting_widget_type; i != sentinel_widget_type; ++i) {
        wl_list_for_each(
            surface, &(context->output->ui.surfaces_mapped[i]), link_mapped) {
            // Skip invisible surfaces.
            if(!rose_output_ui_is_surface_visible(
                   &(context->output->ui), surface)) {
                continue;
            }

            // Obtain current surface's state.
            struct rose_surface_state surface_state =
                rose_surface_state_obtain(surface);

            // Initialize surface rendering context.
            struct rose_surface_rendering_context surface_rendering_context = {
                .parent = context,
                .dx = surface_state.x,
                .dy = surface_state.y};

            // Render the main surface.
            wlr_surface_for_each_surface(
                surface->xdg_surface->surface, rose_render_surface,
                &surface_rendering_context);

            // Render surface's children.
            wlr_xdg_surface_for_each_popup_surface(
                surface->xdg_surface, rose_render_surface,
                &surface_rendering_context);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Content rendering interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_render_content(struct rose_output* output) {
    // Obtain output's focused workspace.
    struct rose_workspace* workspace = output->focused_workspace;

    // Obtain the color scheme.
    struct rose_color_scheme color_scheme =
        output->context->config.theme.color_scheme;

    // Clear this flag. At this point the output is not in direct scan-out mode.
    output->is_scanned_out = false;

    // If the screen is locked, or the given output has no focused workspace,
    // then render output's visible widgets, and do nothing else.
    if((output->context->is_screen_locked) || (workspace == NULL)) {
        // Initialize rendering context.
        struct rose_rendering_context context = {};
        if(!rose_rendering_context_initialize(&context, output)) {
            return;
        }

        if(!pixman_region32_not_empty(&(context.scissor_rectangle))) {
            return rose_rendering_context_finalize(&context);
        }

        // Fill-in with solid color.
        if(true) {
            struct rose_rectangle rectangle = {
                .width = output->device->width,
                .height = output->device->height,
                .is_transformed = true};

            rose_render_rectangle(
                &context, color_scheme.workspace_background, rectangle);
        }

        // Render all widgets.
        rose_render_widgets(&context, 0, rose_surface_widget_type_count_);

        // Finish rendering operation.
        return rose_rendering_context_finalize(&context);
    }

    // Obtain panel's data.
    struct rose_ui_panel panel = workspace->panel;
    if(panel.is_visible && (workspace->focused_surface != NULL)) {
        panel.is_visible =
            !(workspace->focused_surface->state.pending.is_fullscreen);
    }

    // Obtain output's state.
    struct rose_output_state output_state = rose_output_state_obtain(output);

    // Obtain output's menu.
    struct rose_ui_menu* menu = &(output->ui.menu);

    // Try using direct scan-out.
    while(output->cursor.drag_and_drop_surface == NULL) {
        // Obtain workspace's focused surface.
        struct rose_surface* focused_surface = workspace->focused_surface;

        // If the workspace has no focused surface, or if any of the UI
        // components are visible, or the workspace is not in normal mode, then
        // direct scan-out is not possible.
        if((focused_surface == NULL) ||
           (panel.is_visible || menu->is_visible) ||
           (workspace->mode != rose_workspace_mode_normal)) {
            break;
        }

        // Obtain focused surface's state.
        struct rose_surface_state focused_surface_state =
            rose_surface_state_obtain(focused_surface);

        // Obtain focused surface's underlying implementation.
        struct wlr_surface* underlying = focused_surface->xdg_surface->surface;

        // If the surface is not positioned properly, or it has child entities,
        // then scan-out is not possible.
        if((focused_surface_state.x != 0) || //
           (focused_surface_state.y != 0) || //
           (underlying == NULL) ||
           !wl_list_empty(&(focused_surface->subsurfaces)) ||
           !wl_list_empty(&(focused_surface->temporaries))) {
            break;
        }

        // If focused surface's state does not match output's state, then
        // scan-out is not possible.
        if((underlying->current.transform != output_state.transform) ||
           (underlying->current.scale != output_state.scale)) {
            break;
        }

        // If any of the normal UI widgets is visible, then scan-out is not
        // possible.
        if(true) {
            // At this point there are no visible widgets.
            bool is_found = false;

            // Search for a visible widget surface.
            struct rose_surface* surface = NULL;
            for(ptrdiff_t i = rose_surface_special_widget_type_count_;
                i != rose_surface_widget_type_count_; ++i) {
                wl_list_for_each(
                    surface, &(output->ui.surfaces_mapped[i]), link_mapped) {
                    if(rose_output_ui_is_surface_visible(
                           &(output->ui), surface)) {
                        goto found;
                    }
                }

                continue;

            found:
                is_found = true;
                break;
            }

            // If such widget has been found, then scan-out is not possible.
            if(is_found) {
                break;
            }
        }

        // Initialize an empty state.
        struct wlr_output_state state = {};
        wlr_output_state_init(&state);

        // Configure primary swapchain.
        if(!wlr_output_configure_primary_swapchain(
               output->device, &state, &(output->device->swapchain))) {
            return wlr_output_state_finish(&state);
        }

        // Try attaching focused surface's buffer.
        wlr_output_state_set_buffer(&state, &(underlying->buffer->base));
        if(!wlr_output_test_state(output->device, &state)) {
            wlr_output_state_finish(&state);
            break;
        }

        // Try committing the rendering operation.
        if(!wlr_output_commit_state(output->device, &state)) {
            wlr_output_state_finish(&state);
            break;
        }

        // Send presentation feedback.
        wlr_presentation_surface_scanned_out_on_output(
            underlying, output->device);

        // Mark the output as scanned-out.
        output->is_scanned_out = true;

        // Do nothing else.
        return wlr_output_state_finish(&state);
    }

    // Initialize rendering context.
    struct rose_rendering_context context = {};
    if(!rose_rendering_context_initialize(&context, output)) {
        return;
    }

    if(!pixman_region32_not_empty(&(context.scissor_rectangle))) {
        return rose_rendering_context_finalize(&context);
    }

    // Fill-in with solid color.
    if(true) {
        struct rose_rectangle rectangle = {
            .width = output->device->width,
            .height = output->device->height,
            .is_transformed = true};

        rose_render_rectangle(
            &context, color_scheme.workspace_background, rectangle);
    }

    // Render background widget.
    rose_render_widgets(
        &context, rose_surface_widget_type_background,
        rose_surface_widget_type_background + 1);

    // Render the workspace.
    if(workspace->transaction.sentinel > 0) {
        // If the workspace has a running transaction, then render its snapshot.
        struct rose_surface_snapshot* surface_snapshot = NULL;
        wl_list_for_each(
            surface_snapshot, &(workspace->transaction.snapshot.surfaces),
            link) {
            // Compute a rectangle which will represent the given snapshot.
            struct rose_rectangle rectangle = {
                .x = surface_snapshot->x,
                .y = surface_snapshot->y,
                .width = surface_snapshot->width,
                .height = surface_snapshot->height,
                .transform = surface_snapshot->transform};

            // Perform type-dependent rendering operation.
            if((surface_snapshot->type == rose_surface_snapshot_type_normal) &&
               (surface_snapshot->buffer != NULL)) {
                // If the snapshot represents a surface, then obtain its
                // texture.
                struct wlr_texture* texture =
                    ((struct wlr_client_buffer*)(surface_snapshot->buffer))
                        ->texture;

                // Obtain the visible region of the surface's buffer.
                struct wlr_fbox region = {
                    .x = surface_snapshot->buffer_region.x,
                    .y = surface_snapshot->buffer_region.y,
                    .width = surface_snapshot->buffer_region.width,
                    .height = surface_snapshot->buffer_region.height};

                // And render it.
                rose_render_rectangle_with_texture(
                    &context, texture, &region, rectangle);
            } else if(
                surface_snapshot->type ==
                rose_surface_snapshot_type_decoration) {
                // Otherwise, render the decoration it represents.
                rose_render_surface_decoration(
                    &context, &color_scheme, rectangle);
            }
        }

        // Update panel's data from transaction's snapshot.
        panel = workspace->transaction.snapshot.panel;
    } else {
        // Otherwise, render all visible surfaces.
        struct rose_surface* surface = NULL;
        wl_list_for_each(
            surface, &(workspace->surfaces_visible), link_visible) {
            // Obtain surface's state.
            struct rose_surface_state surface_state =
                rose_surface_state_obtain(surface);

            // Initialize surface rendering context.
            struct rose_surface_rendering_context surface_rendering_context = {
                .parent = &context,
                .dx = surface_state.x,
                .dy = surface_state.y};

            // Render surface's decoration, if needed.
            if(!(surface_state.is_maximized || surface_state.is_fullscreen) &&
               ((surface->xdg_decoration == NULL) ||
                (surface->xdg_decoration->current.mode ==
                 WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE))) {
                struct rose_rectangle rectangle = {
                    .x = surface_rendering_context.dx,
                    .y = surface_rendering_context.dy,
                    .width = surface_state.width,
                    .height = surface_state.height};

                rose_render_surface_decoration(
                    &context, &color_scheme, rectangle);
            }

            // Render the surface.
            wlr_xdg_surface_for_each_surface(
                surface->xdg_surface, rose_render_surface,
                &surface_rendering_context);
        }
    }

    // Render the panel, if needed.
    if(panel.is_visible) {
        // Initialize panel's rectangle.
        struct rose_rectangle rectangle = {
            .width = workspace->width, .height = workspace->height};

        // Perform computations depending on panel's position.
        switch(panel.position) {
            case rose_ui_panel_position_bottom:
                rectangle.y = workspace->height - panel.size;
                // fall-through

            case rose_ui_panel_position_top:
                rectangle.height = panel.size;
                break;

            case rose_ui_panel_position_right:
                rectangle.x = workspace->width - panel.size;
                // fall-through

            case rose_ui_panel_position_left:
                rectangle.width = panel.size;
                break;

            default:
                break;
        }

        // Render the rectangle.
        rose_render_rectangle(
            &context, color_scheme.panel_background, rectangle);

        // Start rendering panel's text.
        int dx = 1;

        // Obtain title bar's raster.
        struct rose_raster* raster = output->rasters.title;

        // Render the title bar.
        if(raster != NULL) {
            bool is_tilted = (panel.position == rose_ui_panel_position_left) ||
                             (panel.position == rose_ui_panel_position_right);

            // Compute the extent.
            rectangle.width =
                (is_tilted ? raster->base.height : raster->base.width);

            rectangle.height =
                (is_tilted ? raster->base.width : raster->base.height);

#define scale_(x) (int)((double)(x) / output_state.scale + 0.5)

            // Scale the rectangle.
            rectangle.width = scale_(rectangle.width);
            rectangle.height = scale_(rectangle.height);

#undef scale_

            // Compute the position.
            if(panel.position == rose_ui_panel_position_left) {
                rectangle.y = workspace->height - rectangle.height - dx;
                rectangle.transform = WL_OUTPUT_TRANSFORM_270;
            } else if(panel.position == rose_ui_panel_position_right) {
                rectangle.y += dx;
                rectangle.transform = WL_OUTPUT_TRANSFORM_90;
            } else {
                rectangle.x += dx;
            }

            // Render the texture.
            rose_render_rectangle_with_texture(
                &context, raster->texture, NULL, rectangle);
        }
    }

    // Render normal widgets.
    rose_render_widgets(
        &context, rose_surface_special_widget_type_count_,
        rose_surface_widget_type_count_);

    // Render the menu, if needed.
    if(menu->is_visible) {
        // Initialize menu's area rectangle.
        struct rose_rectangle rectangle = {
            .x = menu->area.x,
            .y = menu->area.y,
            .width = menu->area.width,
            .height = menu->area.height};

        // Render menu's background area.
        rose_render_rectangle(
            &context, color_scheme.menu_background, rectangle);

        // Render menu's mark.
        if(true) {
            rectangle.height = menu->layout.line_height;
            rectangle.y += menu->page.mark_index * menu->layout.line_height +
                           menu->layout.margin_y;

            rose_render_rectangle(
                &context, color_scheme.menu_highlight0, rectangle);
        }

        // Render menu's selected line, if needed.
        if(rose_ui_menu_has_selection(menu)) {
            if(menu->page.selection_index >= 0) {
                rectangle.y =
                    menu->area.y +
                    menu->page.selection_index * menu->layout.line_height +
                    menu->layout.margin_y;

                rose_render_rectangle(
                    &context, color_scheme.menu_highlight1, rectangle);
            }
        }

        // Obtain menu text's raster.
        struct rose_raster* raster = output->rasters.menu;

        // Render the text.
        if(raster != NULL) {
            // Adjust the rectangle.
            rectangle.x = menu->area.x + menu->layout.margin_x;
            rectangle.y = menu->area.y + menu->layout.margin_y;
            rectangle.height = menu->page.line_count * menu->layout.line_height;

            // Specify texture's cropping box.
            struct wlr_fbox box = {
                .width = rectangle.width * output_state.scale,
                .height = rectangle.height * output_state.scale};

            // Render the texture.
            rose_render_rectangle_with_texture(
                &context, raster->texture, &box, rectangle);
        }
    }

    // Render drag and drop surface, if needed.
    if(output->cursor.drag_and_drop_surface != NULL) {
        // Initialize surface rendering context.
        struct rose_surface_rendering_context surface_rendering_context = {
            .parent = &context,
            .dx = workspace->pointer.x,
            .dy = workspace->pointer.y};

        // Render the surface and all its subsurfaces.
        wlr_surface_for_each_surface(
            output->cursor.drag_and_drop_surface, rose_render_surface,
            &surface_rendering_context);
    }

    // Render additional rectangle for the surface which is currently being
    // resized.
    if((workspace->focused_surface != NULL) &&
       (workspace->mode != rose_workspace_mode_normal) &&
       (workspace->mode != rose_workspace_mode_interactive_move)) {
        struct rose_rectangle rectangle = {
            .x = workspace->focused_surface->state.saved.x,
            .y = workspace->focused_surface->state.saved.y,
            .width = workspace->focused_surface->state.pending.width,
            .height = workspace->focused_surface->state.pending.height};

        // Compute pointer's shift.
        int dx = (int)(workspace->pointer.x - workspace->pointer.x_saved);
        int dy = (int)(workspace->pointer.y - workspace->pointer.y_saved);

        // Compute rectangle based on workspace's mode.

        // Coordinate: x, resize: east.
        if((workspace->mode == rose_workspace_mode_interactive_resize_east) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_north_east) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_south_east)) {
            if(dx >= 0) {
                rectangle.width += dx;
            } else {
                if(-dx <= rectangle.width) {
                    rectangle.width -= -dx;
                } else {
                    dx += rectangle.width;
                    rectangle.x += dx;
                    rectangle.width = -dx;
                }
            }
        }

        // Coordinate: x, resize: west.
        if((workspace->mode == rose_workspace_mode_interactive_resize_west) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_north_west) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_south_west)) {
            if(dx > 0) {
                if(dx <= rectangle.width) {
                    rectangle.width -= dx;
                    rectangle.x += dx;
                } else {
                    rectangle.x += rectangle.width;
                    rectangle.width = dx - rectangle.width;
                }
            } else {
                rectangle.x += dx;
                rectangle.width += -dx;
            }
        }

        // Coordinate: y, resize: north.
        if((workspace->mode == rose_workspace_mode_interactive_resize_north) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_north_east) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_north_west)) {
            if(dy > 0) {
                if(dy <= rectangle.height) {
                    rectangle.height -= dy;
                    rectangle.y += dy;
                } else {
                    rectangle.y += rectangle.height;
                    rectangle.height = dy - rectangle.height;
                }
            } else {
                rectangle.y += dy;
                rectangle.height += -dy;
            }
        }

        // Coordinate: y, resize: south.
        if((workspace->mode == rose_workspace_mode_interactive_resize_south) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_south_east) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_south_west)) {
            if(dy >= 0) {
                rectangle.height += dy;
            } else {
                if(-dy <= rectangle.height) {
                    rectangle.height -= -dy;
                } else {
                    dy += rectangle.height;
                    rectangle.y += dy;
                    rectangle.height = -dy;
                }
            }
        }

        // Render the rectangle.
        rose_render_rectangle(
            &context, color_scheme.surface_resizing, rectangle);

#define array_size_(a) ((size_t)(sizeof(a) / sizeof(a[0])))

        // Compute and render rectangle's decorations.
        if(true) {
            struct rose_rectangle frame_parts[] = {
                {.x = rectangle.x - 4,
                 .y = rectangle.y - 4,
                 .width = rectangle.width + 8,
                 .height = 4},
                {.x = rectangle.x - 4,
                 .y = rectangle.y + rectangle.height,
                 .width = rectangle.width + 8,
                 .height = 4},
                {.x = rectangle.x - 4,
                 .y = rectangle.y,
                 .width = 4,
                 .height = rectangle.height},
                {.x = rectangle.x + rectangle.width,
                 .y = rectangle.y,
                 .width = 4,
                 .height = rectangle.height}};

            for(size_t i = 0; i < array_size_(frame_parts); ++i) {
                rose_render_rectangle(
                    &context, color_scheme.surface_resizing_background0,
                    frame_parts[i]);
            }
        }

        if(true) {
            struct rose_rectangle frame_parts[] = {
                {.x = rectangle.x - 5,
                 .y = rectangle.y - 5,
                 .width = rectangle.width + 10,
                 .height = 1},
                {.x = rectangle.x - 5,
                 .y = rectangle.y + rectangle.height + 4,
                 .width = rectangle.width + 10,
                 .height = 1},
                {.x = rectangle.x - 5,
                 .y = rectangle.y - 4,
                 .width = 1,
                 .height = rectangle.height + 8},
                {.x = rectangle.x + rectangle.width + 4,
                 .y = rectangle.y - 4,
                 .width = 1,
                 .height = rectangle.height + 8}};

            for(size_t i = 0; i < array_size_(frame_parts); ++i) {
                rose_render_rectangle(
                    &context, color_scheme.surface_resizing_background1,
                    frame_parts[i]);
            }
        }
    }

#ifdef ROSE_RENDER_DAMAGE
    // Render the damage.
    if(true) {
        // Construct rectangles which represent damaged region.
        struct rose_rectangle rectangles[] = {
            {.x = context.damage.x,
             .y = context.damage.y,
             .width = 2,
             .height = context.damage.height,
             .is_transformed = true},
            {.x = context.damage.x,
             .y = context.damage.y,
             .width = context.damage.width,
             .height = 2,
             .is_transformed = true},
            {.x = context.damage.x + context.damage.width - 2,
             .y = context.damage.y,
             .width = 2,
             .height = context.damage.height,
             .is_transformed = true},
            {.x = context.damage.x,
             .y = context.damage.y + context.damage.height - 2,
             .width = context.damage.width,
             .height = 2,
             .is_transformed = true}};

        // Initialize damage's color.
        struct rose_color color_red = {.rgba32 = {0xFF, 0, 0, 0xFF}};

        // Render the damage.
        for(size_t i = 0; i < array_size_(rectangles); ++i) {
            rose_render_rectangle(&context, color_red, rectangles[i]);
        }
    }
#endif

    // Finish rendering operation.
    return rose_rendering_context_finalize(&context);
}
