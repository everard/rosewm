// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"
#include "rendering.h"

#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/render/wlr_renderer.h>

////////////////////////////////////////////////////////////////////////////////
// Matrix-computation-related utility functions and types.
////////////////////////////////////////////////////////////////////////////////

struct rose_matrix {
    float a[9];
};

struct rose_rectangle {
    int x, y, w, h;
    enum wl_output_transform transform;
};

static struct rose_matrix
rose_project_rectangle(struct rose_output* output,
                       struct rose_rectangle rectangle) {
    // Scale the rectangle.
    float scale = output->device->scale;

    float x = (float)(rectangle.x) * scale;
    float y = (float)(rectangle.y) * scale;

    float w = floor(0.5f + (float)(rectangle.w) * scale);
    float h = floor(0.5f + (float)(rectangle.h) * scale);

    // Make identity transform.
    struct rose_matrix r =  //
        {{1.0f, 0.0f, 0.0f, //
          0.0f, 1.0f, 0.0f, //
          0.0f, 0.0f, 1.0f}};

    // Apply rectangle's extents.
    wlr_matrix_translate(r.a, x, y);
    wlr_matrix_scale(r.a, w, h);

    // Apply rectangle's transform.
    if(rectangle.transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        wlr_matrix_translate(r.a, 0.5f, 0.5f);
        wlr_matrix_transform(
            r.a, wlr_output_transform_invert(rectangle.transform));
        wlr_matrix_translate(r.a, -0.5f, -0.5f);
    }

    // Apply output's transform.
    wlr_matrix_multiply(r.a, output->device->transform_matrix, r.a);

    // Return computed transform.
    return r;
}

////////////////////////////////////////////////////////////////////////////////
// Rendering-related utility functions and types.
////////////////////////////////////////////////////////////////////////////////

static void
rose_render_rectangle(struct rose_output* output, struct rose_color color,
                      struct rose_rectangle rectangle) {
    // Project the rectangle to the output and render it with the given color.
    wlr_render_quad_with_matrix(output->context->renderer, color.v,
                                rose_project_rectangle(output, rectangle).a);
}

static void
rose_render_rectangle_with_texture( //
    struct rose_output* output, struct wlr_texture* texture,
    struct rose_rectangle rectangle) {
    // Do nothing if there is no texture.
    if(texture == NULL) {
        return;
    }

    // Project the rectangle to the output and render it with the given texture.
    wlr_render_texture_with_matrix( //
        output->context->renderer, texture,
        rose_project_rectangle(output, rectangle).a, 1.0f);
}

struct rose_render_surface_context {
    struct rose_output* output;
    int dx, dy;
};

static void
rose_render_surface(struct wlr_surface* surface, int x, int y, void* data) {
    // Obtain a pointer to the rendering context.
    struct rose_render_surface_context* render_surface_context = data;

    // Obtain a pointer to the output.
    struct rose_output* output = render_surface_context->output;

    // Compute a rectangle which will represent the given surface.
    struct rose_rectangle rectangle = //
        {.x = x + render_surface_context->dx,
         .y = y + render_surface_context->dy,
         .w = surface->current.width,
         .h = surface->current.height,
         .transform = surface->current.transform};

    // Render the rectangle with surface's texture.
    rose_render_rectangle_with_texture(
        output, wlr_surface_get_texture(surface), rectangle);

    // Send presentation feedback.
    wlr_presentation_surface_sampled_on_output(
        output->context->presentation, surface, output->device);
}

static void
rose_render_surface_decoration(struct rose_output* output,
                               struct rose_rectangle surface_rectangle) {
    // Obtain the color scheme.
    struct rose_color_scheme* color_scheme =
        &(output->context->config.color_scheme);

    // Update surface's rectangle.
    surface_rectangle.x -= 5;
    surface_rectangle.y -= 5;

    surface_rectangle.w += 10;
    surface_rectangle.h += 10;

    // Render surface's background frame.
    rose_render_rectangle(
        output, color_scheme->surface_background1, surface_rectangle);

    surface_rectangle.x += 1;
    surface_rectangle.y += 1;

    surface_rectangle.w -= 2;
    surface_rectangle.h -= 2;

    rose_render_rectangle(
        output, color_scheme->surface_background0, surface_rectangle);
}

static void
rose_render_output_widgets(struct rose_output* output, ptrdiff_t start_idx,
                           ptrdiff_t sentinel_idx) {
    // Obtain output's state.
    struct rose_output_state output_state = rose_output_state_obtain(output);

    // Iterate through the entire UI.
    struct rose_output_widget* widget = NULL;
    for(ptrdiff_t i = start_idx; i != sentinel_idx; ++i) {
        wl_list_for_each(widget, &(output->ui.widgets_mapped[i]), link_mapped) {
            // Skip invisible widgets.
            if(!rose_output_widget_is_visible(widget)) {
                continue;
            }

            // Obtain current widget's state.
            struct rose_output_widget_state state =
                rose_output_widget_state_obtain(widget);

            // Initialize surface rendering context.
            struct rose_render_surface_context context = {
                .output = output, .dx = state.x, .dy = state.y};

#define scale_(x) (int)((x) * (output_state.scale) + 0.5)

            // Compute scissor rectangle.
            struct wlr_box scissor_rect = //
                {.x = scale_(state.x),
                 .y = scale_(state.y),
                 .width = scale_(state.w),
                 .height = scale_(state.h)};

#undef scale_

            // Set scissor rectangle and render widget's main surface.
            wlr_renderer_scissor(output->context->renderer, &scissor_rect);
            wlr_surface_for_each_surface( //
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
    // Obtain pointers to the renderer and output's focused workspace.
    struct rose_workspace* workspace = output->focused_workspace;
    struct wlr_renderer* renderer = output->context->renderer;

    // Obtain the color scheme.
    struct rose_color_scheme color_scheme =
        output->context->config.color_scheme;

    // Clear this flag. At this point the output is not in direct scan-out mode.
    output->is_scanned_out = false;

    // If the screen is locked, or the given output has no focused workspaces,
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
        wlr_renderer_clear(renderer, color_scheme.workspace_background.v);

        // Render widgets.
        rose_render_output_widgets(output, 0, rose_output_n_widget_types);

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
    while(true) {
        // Obtain a pointer to workspace's focused surface.
        struct rose_surface* surface = workspace->focused_surface;

        // If the workspace has no focused surface, or if any of the UI
        // components are visible, or the workspace is not in normal mode, then
        // direct scan-out is not possible.
        if((surface == NULL) || (panel.is_visible || menu->is_visible) ||
           (workspace->mode != rose_workspace_mode_normal)) {
            goto no_scanout;
        }

        struct rose_surface_state state = rose_surface_state_obtain(surface);
        struct wlr_surface* wlr_surface = surface->xdg_surface->surface;

        // If the surface is not positioned properly, or it has child entities,
        // then scan-out is not possible.
        if(((state.x != 0) || (state.y != 0)) || (wlr_surface == NULL) ||
           !wl_list_empty(&(surface->subsurfaces)) ||
           !wl_list_empty(&(surface->temporaries))) {
            goto no_scanout;
        }

        // If surface's state does not match output's state, then scan-out is
        // not possible.
        if((wlr_surface->current.transform != output_state.transform) ||
           (wlr_surface->current.scale != output_state.scale)) {
            goto no_scanout;
        }

        // If any of the normal UI widgets is visible, then scan-out is not
        // possible.
        struct rose_output_widget* widget = NULL;
        for(ptrdiff_t i = rose_output_n_special_widget_types;
            i != rose_output_n_widget_types; ++i) {
            wl_list_for_each(
                widget, &(output->ui.widgets_mapped[i]), link_mapped) {
                if(rose_output_widget_is_visible(widget)) {
                    goto no_scanout;
                }
            }
        }

        // Try attaching surface's buffer.
        wlr_output_attach_buffer(output->device, &(wlr_surface->buffer->base));
        if(!wlr_output_test(output->device)) {
            goto no_scanout;
        }

        // Try committing the rendering operation.
        if(!wlr_output_commit(output->device)) {
            goto no_scanout;
        }

        // Send presentation feedback.
        wlr_presentation_surface_sampled_on_output(
            output->context->presentation, wlr_surface, output->device);

        // Mark the output as scanned-out.
        output->is_scanned_out = true;

        // Do nothing else.
        return;

    no_scanout:
        break;
    }

    // Attach the renderer to the output.
    if(!wlr_output_attach_render(output->device, NULL)) {
        return;
    }

    // Start rendering operation, render background.
    wlr_renderer_begin(renderer, output->device->width, output->device->height);
    wlr_renderer_clear(renderer, color_scheme.workspace_background.v);

    rose_render_output_widgets(output, rose_output_widget_type_background,
                               rose_output_widget_type_background + 1);

    // Render the workspace.
    if(workspace->transaction.sentinel > 0) {
        // If the workspace has a running transaction, then render its snapshot.
        struct rose_surface_snapshot* surface_snapshot = NULL;
        wl_list_for_each(surface_snapshot,
                         &(workspace->transaction.snapshot.surfaces), link) {
            // Compute a rectangle which will represent the given snapshot.
            struct rose_rectangle rectangle = {
                .x = surface_snapshot->x,
                .y = surface_snapshot->y,
                .w = surface_snapshot->w,
                .h = surface_snapshot->h,
                .transform = surface_snapshot->transform};

            // Perform type-dependent rendering operation.
            if((surface_snapshot->type == rose_surface_snapshot_type_normal) &&
               (surface_snapshot->buffer != NULL)) {
                // If the snapshot represents a surface, then obtain its
                // texture.
                struct wlr_texture* texture =
                    ((struct wlr_client_buffer*)(surface_snapshot->buffer))
                        ->texture;

                // And render it.
                rose_render_rectangle_with_texture(output, texture, rectangle);
            } else if(surface_snapshot->type ==
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
            struct rose_render_surface_context context = {
                .output = output, .dx = state.x, .dy = state.y};

            // Render surface's decoration, if needed.
            if(!(state.is_maximized || state.is_fullscreen)) {
                struct rose_rectangle rectangle = //
                    {.x = context.dx,
                     .y = context.dy,
                     .w = state.w,
                     .h = state.h,
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
            .w = workspace->w,
            .h = workspace->h,
            .transform = WL_OUTPUT_TRANSFORM_NORMAL};

        // Perform computations depending on panel's position.
        switch(panel.position) {
            case rose_ui_panel_position_bottom:
                rectangle.y = workspace->h - panel.size;
                // fall-through

            case rose_ui_panel_position_top:
                rectangle.h = panel.size;
                break;

            case rose_ui_panel_position_right:
                rectangle.x = workspace->w - panel.size;
                // fall-through

            case rose_ui_panel_position_left:
                rectangle.w = panel.size;
                break;

            default:
                break;
        }

        // Render the rectangle.
        rose_render_rectangle(output, color_scheme.panel_background, rectangle);

        // Start rendering panel's text.
        int dx = 1;

        // Render the title bar.
        if(true) {
            // Obtain title bar's text buffer.
            struct rose_output_text_buffer* text_buffer =
                &(output->text_buffers.title);

            // Render the text buffer.
            if(text_buffer->texture != NULL) {
                bool is_tilted =
                    (panel.position == rose_ui_panel_position_left) ||
                    (panel.position == rose_ui_panel_position_right);

                // Compute text's extents.
                rectangle.w = (is_tilted ? text_buffer->pixels->h
                                         : text_buffer->pixels->w);

                rectangle.h = (is_tilted ? text_buffer->pixels->w
                                         : text_buffer->pixels->h);

#define scale_(x) (int)((double)(x) / output_state.scale + 0.5)

                // Scale text buffer's rectangle.
                rectangle.w = scale_(rectangle.w);
                rectangle.h = scale_(rectangle.h);

#undef scale_

                // Compute text's position.
                if(panel.position == rose_ui_panel_position_left) {
                    rectangle.y = workspace->h - rectangle.h - dx;
                    rectangle.transform = WL_OUTPUT_TRANSFORM_270;
                } else if(panel.position == rose_ui_panel_position_right) {
                    rectangle.y += dx;
                    rectangle.transform = WL_OUTPUT_TRANSFORM_90;
                } else {
                    rectangle.x += dx;
                }

                // Render the text.
                rose_render_rectangle_with_texture(
                    output, text_buffer->texture, rectangle);
            }
        }
    }

    // Render normal widgets.
    rose_render_output_widgets(
        output, rose_output_n_special_widget_types, rose_output_n_widget_types);

    // Render the menu, if needed.
    if(menu->is_visible) {
        // Initialize menu's area rectangle.
        struct rose_rectangle rectangle = {
            .x = menu->area.x,
            .y = menu->area.y,
            .w = menu->area.w,
            .h = menu->area.h,
            .transform = WL_OUTPUT_TRANSFORM_NORMAL};

        // Render menu's background area.
        rose_render_rectangle(output, color_scheme.menu_background, rectangle);

        // Render menu's mark.
        if(true) {
            rectangle.h = menu->layout.line_h;
            rectangle.y += menu->page.mark_idx * menu->layout.line_h +
                           menu->layout.margin_y;

            rose_render_rectangle(
                output, color_scheme.menu_highlight0, rectangle);
        }

        // Render menu's selected line, if needed.
        if(rose_ui_menu_has_selection(menu)) {
            if(menu->page.selection_idx >= 0) {
                rectangle.y = //
                    menu->area.y +
                    menu->page.selection_idx * menu->layout.line_h +
                    menu->layout.margin_y;

                rose_render_rectangle(
                    output, color_scheme.menu_highlight1, rectangle);
            }
        }

        // Obtain menu's text buffer.
        struct rose_output_text_buffer* text_buffer =
            &(output->text_buffers.menu);

        // Render the text buffer.
        if(text_buffer->texture != NULL) {
            rectangle.x = menu->area.x + menu->layout.margin_x;
            rectangle.y = menu->area.y + menu->layout.margin_y;
            rectangle.h = menu->page.n_lines * menu->layout.line_h;

            // Specify texture's cropping box.
            struct wlr_fbox box = //
                {.width = rectangle.w * output_state.scale,
                 .height = rectangle.h * output_state.scale};

            // Crop and render text buffer's texture.
            wlr_render_subtexture_with_matrix( //
                renderer, text_buffer->texture, &box,
                rose_project_rectangle(output, rectangle).a, 1.0f);
        }
    }

    // Render additional rectangle for the surface which is currently being
    // resized.
    if((workspace->focused_surface != NULL) &&
       (workspace->mode != rose_workspace_mode_normal) &&
       (workspace->mode != rose_workspace_mode_interactive_move)) {
        struct rose_rectangle rectangle = {
            .x = workspace->focused_surface->state.saved.x,
            .y = workspace->focused_surface->state.saved.y,
            .w = workspace->focused_surface->state.pending.w,
            .h = workspace->focused_surface->state.pending.h};

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
                rectangle.w += dx;
            } else {
                if(-dx <= rectangle.w) {
                    rectangle.w -= -dx;
                } else {
                    dx += rectangle.w;
                    rectangle.x += dx;
                    rectangle.w = -dx;
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
                if(dx <= rectangle.w) {
                    rectangle.w -= dx;
                    rectangle.x += dx;
                } else {
                    rectangle.x += rectangle.w;
                    rectangle.w = dx - rectangle.w;
                }
            } else {
                rectangle.x += dx;
                rectangle.w += -dx;
            }
        }

        // Coordinate: y, resize: north.
        if((workspace->mode == rose_workspace_mode_interactive_resize_north) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_north_east) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_north_west)) {
            if(dy > 0) {
                if(dy <= rectangle.h) {
                    rectangle.h -= dy;
                    rectangle.y += dy;
                } else {
                    rectangle.y += rectangle.h;
                    rectangle.h = dy - rectangle.h;
                }
            } else {
                rectangle.y += dy;
                rectangle.h += -dy;
            }
        }

        // Coordinate: y, resize: south.
        if((workspace->mode == rose_workspace_mode_interactive_resize_south) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_south_east) ||
           (workspace->mode ==
            rose_workspace_mode_interactive_resize_south_west)) {
            if(dy >= 0) {
                rectangle.h += dy;
            } else {
                if(-dy <= rectangle.h) {
                    rectangle.h -= -dy;
                } else {
                    dy += rectangle.h;
                    rectangle.y += dy;
                    rectangle.h = -dy;
                }
            }
        }

        // Render the rectangle.
        rose_render_rectangle(output, color_scheme.surface_resizing, rectangle);

#define array_size_(a) ((size_t)(sizeof(a) / sizeof(a[0])))

        // Compute and render rectangle's decorations.
        if(true) {
            struct rose_rectangle frame_parts[] = //
                {{.x = rectangle.x - 4,
                  .y = rectangle.y - 4,
                  .w = rectangle.w + 8,
                  .h = 4},
                 {.x = rectangle.x - 4,
                  .y = rectangle.y + rectangle.h,
                  .w = rectangle.w + 8,
                  .h = 4},
                 {.x = rectangle.x - 4,
                  .y = rectangle.y,
                  .w = 4,
                  .h = rectangle.h},
                 {.x = rectangle.x + rectangle.w,
                  .y = rectangle.y,
                  .w = 4,
                  .h = rectangle.h}};

            for(size_t i = 0; i < array_size_(frame_parts); ++i) {
                rose_render_rectangle( //
                    output, color_scheme.surface_resizing_background0,
                    frame_parts[i]);
            }
        }

        if(true) {
            struct rose_rectangle frame_parts[] = //
                {{.x = rectangle.x - 5,
                  .y = rectangle.y - 5,
                  .w = rectangle.w + 10,
                  .h = 1},
                 {.x = rectangle.x - 5,
                  .y = rectangle.y + rectangle.h + 4,
                  .w = rectangle.w + 10,
                  .h = 1},
                 {.x = rectangle.x - 5,
                  .y = rectangle.y - 4,
                  .w = 1,
                  .h = rectangle.h + 8},
                 {.x = rectangle.x + rectangle.w + 4,
                  .y = rectangle.y - 4,
                  .w = 1,
                  .h = rectangle.h + 8}};

            for(size_t i = 0; i < array_size_(frame_parts); ++i) {
                rose_render_rectangle( //
                    output, color_scheme.surface_resizing_background1,
                    frame_parts[i]);
            }
        }

#undef array_size_
    }

    // Finish rendering operation.
    wlr_output_render_software_cursors(output->device, NULL);
    wlr_renderer_end(renderer);

    // Commit the rendering operation.
    wlr_output_commit(output->device);
}
