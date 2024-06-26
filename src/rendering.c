// Copyright Nezametdinov E. Ildus 2024.
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

////////////////////////////////////////////////////////////////////////////////
// Matrix definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_matrix {
    float a[9];
};

////////////////////////////////////////////////////////////////////////////////
// Rectangle definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_rectangle {
    int x, y, width, height;
    enum wl_output_transform transform;
};

////////////////////////////////////////////////////////////////////////////////
// Rectangle projecting utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_matrix
rose_project_rectangle(
    struct rose_output* output, struct rose_rectangle rectangle) {
    // Initialize resulting matrix by scaling and rotating the rectangle.
    struct rose_matrix result =                            //
        {{1.0f, 0.0f, rectangle.x * output->device->scale, //
          0.0f, 1.0f, rectangle.y * output->device->scale, //
          0.0f, 0.0f, 1.0f}};

    result.a[0] = floor(0.5f + rectangle.width * output->device->scale);
    result.a[4] = floor(0.5f + rectangle.height * output->device->scale);

    if(rectangle.transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        wlr_matrix_translate(result.a, 0.5f, 0.5f);
        wlr_matrix_transform(
            result.a, wlr_output_transform_invert(rectangle.transform));
        wlr_matrix_translate(result.a, -0.5f, -0.5f);
    }

    // Apply output's transform.
    wlr_matrix_multiply(result.a, output->device->transform_matrix, result.a);

    // Return computed transform.
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Rendering utility functions and types.
////////////////////////////////////////////////////////////////////////////////

static void
rose_render_rectangle(
    struct rose_output* output, struct rose_color color,
    struct rose_rectangle rectangle) {
    // Project the rectangle to the output and render it with the given color.
    wlr_render_quad_with_matrix(
        output->context->renderer, color.rgba32,
        rose_project_rectangle(output, rectangle).a);
}

static void
rose_render_rectangle_with_texture(
    struct rose_output* output, struct wlr_texture* texture,
    struct wlr_fbox* region, struct rose_rectangle rectangle) {
    // Do nothing if there is no texture.
    if(texture == NULL) {
        return;
    }

    // Project the rectangle to the output and render it with the given texture.
    if(region != NULL) {
        // Render only the given region of the texture.
        wlr_render_subtexture_with_matrix(
            output->context->renderer, texture, region,
            rose_project_rectangle(output, rectangle).a, 1.0f);
    } else {
        // Render the entire texture.
        wlr_render_texture_with_matrix(
            output->context->renderer, texture,
            rose_project_rectangle(output, rectangle).a, 1.0f);
    }
}

struct rose_surface_rendering_context {
    struct rose_output* output;
    int dx, dy;
};

static void
rose_render_surface(struct wlr_surface* surface, int x, int y, void* data) {
    // Obtain surface rendering context.
    struct rose_surface_rendering_context* context = data;

    // Obtain the output.
    struct rose_output* output = context->output;

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
        output, wlr_surface_get_texture(surface), &region, rectangle);

    // Send presentation feedback.
    wlr_presentation_surface_textured_on_output(
        output->context->presentation, surface, output->device);
}

static void
rose_render_surface_decoration(
    struct rose_output* output, struct rose_rectangle surface_rectangle) {
    // Obtain the color scheme.
    struct rose_color_scheme* color_scheme =
        &(output->context->config.theme.color_scheme);

    // Update surface's rectangle.
    surface_rectangle.x -= 5;
    surface_rectangle.y -= 5;

    surface_rectangle.width += 10;
    surface_rectangle.height += 10;

    // Render surface's background frame.
    rose_render_rectangle(
        output, color_scheme->surface_background1, surface_rectangle);

    surface_rectangle.x += 1;
    surface_rectangle.y += 1;

    surface_rectangle.width -= 2;
    surface_rectangle.height -= 2;

    rose_render_rectangle(
        output, color_scheme->surface_background0, surface_rectangle);
}

static void
rose_render_output_widgets(
    struct rose_output* output,
    enum rose_output_widget_type starting_widget_type,
    enum rose_output_widget_type sentinel_widget_type) {
    // Obtain output's state.
    struct rose_output_state output_state = rose_output_state_obtain(output);

    // Iterate through the supplied range of UI widget types.
    struct rose_output_widget* widget = NULL;
    for(ptrdiff_t i = starting_widget_type; i != sentinel_widget_type; ++i) {
        wl_list_for_each(widget, &(output->ui.widgets_mapped[i]), link_mapped) {
            // Skip invisible widgets.
            if(!rose_output_widget_is_visible(widget)) {
                continue;
            }

            // Obtain current widget's state.
            struct rose_output_widget_state state =
                rose_output_widget_state_obtain(widget);

            // Initialize surface rendering context.
            struct rose_surface_rendering_context context = {
                .output = output, .dx = state.x, .dy = state.y};

#define scale_(x) (int)((x) * (output_state.scale) + 0.5)

            // Compute scissor rectangle.
            struct wlr_box scissor_rectangle = {
                .x = scale_(state.x),
                .y = scale_(state.y),
                .width = scale_(state.width),
                .height = scale_(state.height)};

#undef scale_

            // Set scissor rectangle and render widget's main surface.
            wlr_renderer_scissor(output->context->renderer, &scissor_rectangle);
            wlr_surface_for_each_surface(
                widget->xdg_surface->surface, rose_render_surface, &context);

            // Reset scissor rectangle and render widget's child pop-up
            // surfaces, if any.
            wlr_renderer_scissor(output->context->renderer, NULL);
            wlr_xdg_surface_for_each_popup_surface(
                widget->xdg_surface, rose_render_surface, &context);
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

    // Obtain the renderer.
    struct wlr_renderer* renderer = output->context->renderer;

    // Obtain the color scheme.
    struct rose_color_scheme color_scheme =
        output->context->config.theme.color_scheme;

    // Clear this flag. At this point the output is not in direct scan-out mode.
    output->is_scanned_out = false;

    // If the screen is locked, or the given output has no focused workspace,
    // then render output's visible widgets, and do nothing else.
    if((output->context->is_screen_locked) || (workspace == NULL)) {
        // Attach the renderer to the output.
        if(!wlr_output_attach_render(output->device, NULL)) {
            return;
        }

        // Start rendering operation.
        wlr_renderer_begin(
            renderer, output->device->width, output->device->height);

        // Fill-in with solid color.
        wlr_renderer_clear(renderer, color_scheme.workspace_background.rgba32);

        // Render all widgets.
        rose_render_output_widgets(output, 0, rose_output_widget_type_count_);

        // Finish rendering operation.
        wlr_output_render_software_cursors(output->device, NULL);
        wlr_renderer_end(renderer);

        // Commit the rendering operation.
        wlr_output_commit(output->device);

        // Do nothing else.
        return;
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
        struct rose_surface* surface = workspace->focused_surface;

        // If the workspace has no focused surface, or if any of the UI
        // components are visible, or the workspace is not in normal mode, then
        // direct scan-out is not possible.
        if((surface == NULL) || (panel.is_visible || menu->is_visible) ||
           (workspace->mode != rose_workspace_mode_normal)) {
            break;
        }

        struct rose_surface_state state = rose_surface_state_obtain(surface);
        struct wlr_surface* wlr_surface = surface->xdg_surface->surface;

        // If the surface is not positioned properly, or it has child entities,
        // then scan-out is not possible.
        if(((state.x != 0) || (state.y != 0)) || (wlr_surface == NULL) ||
           !wl_list_empty(&(surface->subsurfaces)) ||
           !wl_list_empty(&(surface->temporaries))) {
            break;
        }

        // If surface's state does not match output's state, then scan-out is
        // not possible.
        if((wlr_surface->current.transform != output_state.transform) ||
           (wlr_surface->current.scale != output_state.scale)) {
            break;
        }

        // If any of the normal UI widgets is visible, then scan-out is not
        // possible.
        if(true) {
            // At this point there are no visible widgets.
            bool is_widget_found = false;

            // Search for a visible widget.
            struct rose_output_widget* widget = NULL;
            for(ptrdiff_t i = rose_output_special_widget_type_count_;
                i != rose_output_widget_type_count_; ++i) {
                wl_list_for_each(
                    widget, &(output->ui.widgets_mapped[i]), link_mapped) {
                    if(rose_output_widget_is_visible(widget)) {
                        // Note: The widget has been found.
                        goto found;
                    }
                }

                continue;

            found:
                is_widget_found = true;
                break;
            }

            // If such widget has been found, then scan-out is not possible.
            if(is_widget_found) {
                break;
            }
        }

        // Try attaching surface's buffer.
        wlr_output_attach_buffer(output->device, &(wlr_surface->buffer->base));
        if(!wlr_output_test(output->device)) {
            break;
        }

        // Try committing the rendering operation.
        if(!wlr_output_commit(output->device)) {
            break;
        }

        // Send presentation feedback.
        wlr_presentation_surface_scanned_out_on_output(
            output->context->presentation, wlr_surface, output->device);

        // Mark the output as scanned-out.
        output->is_scanned_out = true;

        // Do nothing else.
        return;
    }

    // Attach the renderer to the output.
    if(!wlr_output_attach_render(output->device, NULL)) {
        return;
    }

    // Start rendering operation, render background.
    wlr_renderer_begin(renderer, output->device->width, output->device->height);
    wlr_renderer_clear(renderer, color_scheme.workspace_background.rgba32);

    // Render background widget.
    rose_render_output_widgets(
        output, rose_output_widget_type_background,
        rose_output_widget_type_background + 1);

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
                    output, texture, &region, rectangle);
            } else if(
                surface_snapshot->type ==
                rose_surface_snapshot_type_decoration) {
                // Otherwise, render the decoration it represents.
                rose_render_surface_decoration(output, rectangle);
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
            struct rose_surface_state state =
                rose_surface_state_obtain(surface);

            // Initialize surface rendering context.
            struct rose_surface_rendering_context context = {
                .output = output, .dx = state.x, .dy = state.y};

            // Render surface's decoration, if needed.
            if(!(state.is_maximized || state.is_fullscreen) &&
               ((surface->xdg_decoration == NULL) ||
                (surface->xdg_decoration->current.mode ==
                 WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE))) {
                struct rose_rectangle rectangle = {
                    .x = context.dx,
                    .y = context.dy,
                    .width = state.width,
                    .height = state.height,
                    .transform = WL_OUTPUT_TRANSFORM_NORMAL};

                rose_render_surface_decoration(output, rectangle);
            }

            // Render the surface.
            wlr_xdg_surface_for_each_surface(
                surface->xdg_surface, rose_render_surface, &context);
        }
    }

    // Render the panel, if needed.
    if(panel.is_visible) {
        // Initialize panel's rectangle.
        struct rose_rectangle rectangle = {
            .width = workspace->width,
            .height = workspace->height,
            .transform = WL_OUTPUT_TRANSFORM_NORMAL};

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
        rose_render_rectangle(output, color_scheme.panel_background, rectangle);

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
                output, raster->texture, NULL, rectangle);
        }
    }

    // Render normal widgets.
    rose_render_output_widgets(
        output, rose_output_special_widget_type_count_,
        rose_output_widget_type_count_);

    // Render the menu, if needed.
    if(menu->is_visible) {
        // Initialize menu's area rectangle.
        struct rose_rectangle rectangle = {
            .x = menu->area.x,
            .y = menu->area.y,
            .width = menu->area.width,
            .height = menu->area.height,
            .transform = WL_OUTPUT_TRANSFORM_NORMAL};

        // Render menu's background area.
        rose_render_rectangle(output, color_scheme.menu_background, rectangle);

        // Render menu's mark.
        if(true) {
            rectangle.height = menu->layout.line_height;
            rectangle.y += menu->page.mark_index * menu->layout.line_height +
                           menu->layout.margin_y;

            rose_render_rectangle(
                output, color_scheme.menu_highlight0, rectangle);
        }

        // Render menu's selected line, if needed.
        if(rose_ui_menu_has_selection(menu)) {
            if(menu->page.selection_index >= 0) {
                rectangle.y =
                    menu->area.y +
                    menu->page.selection_index * menu->layout.line_height +
                    menu->layout.margin_y;

                rose_render_rectangle(
                    output, color_scheme.menu_highlight1, rectangle);
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

            // Crop and render raster's texture.
            wlr_render_subtexture_with_matrix(
                renderer, raster->texture, &box,
                rose_project_rectangle(output, rectangle).a, 1.0f);
        }
    }

    // Render drag and drop surface, if needed.
    if(output->cursor.drag_and_drop_surface != NULL) {
        // Initialize surface rendering context.
        struct rose_surface_rendering_context context = {
            .output = output,
            .dx = workspace->pointer.x,
            .dy = workspace->pointer.y};

        // Render the surface and all its subsurfaces.
        wlr_surface_for_each_surface(
            output->cursor.drag_and_drop_surface, rose_render_surface,
            &context);
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
        rose_render_rectangle(output, color_scheme.surface_resizing, rectangle);

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
                    output, color_scheme.surface_resizing_background0,
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
                    output, color_scheme.surface_resizing_background1,
                    frame_parts[i]);
            }
        }

#undef array_size_
    }

    // Finish rendering operation.
    wlr_output_render_software_cursors(output->device, NULL);
    wlr_renderer_end(renderer);

    // Commit rendering operation.
    wlr_output_commit(output->device);
}
