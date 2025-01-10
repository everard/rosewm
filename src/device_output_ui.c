// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define max_(a, b) ((a) > (b) ? (a) : (b))

////////////////////////////////////////////////////////////////////////////////
// Panel query utility function.
////////////////////////////////////////////////////////////////////////////////

static struct rose_ui_panel
rose_output_ui_obtain_panel(struct rose_output_ui* ui) {
    // Obtain the parent output.
    struct rose_output* output = ui->output;

    // Initialize the resulting panel from the theme.
    struct rose_ui_panel result = output->context->config.theme.panel;

    // Update the panel from output's focused workspace.
    if(output->focused_workspace != NULL) {
        // Obtain the panel.
        result = output->focused_workspace->panel;

        // Compute panel's visibility flag.
        if(result.is_visible) {
            if(output->focused_workspace->focused_surface != NULL) {
                result.is_visible = !(output->focused_workspace->focused_surface
                                          ->state.pending.is_fullscreen);
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Surface configuration-related utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_output_ui_position_surface(
    struct rose_output_ui* ui, struct rose_surface* surface) {
    // Obtain parent output's state.
    struct rose_output_state output_state =
        rose_output_state_obtain(ui->output);

    // Compute output's effective resolution.
    if(true) {
        output_state.width =
            (int)(0.5 + (output_state.width / output_state.scale));

        output_state.height =
            (int)(0.5 + (output_state.height / output_state.scale));
    }

    // Obtain the panel.
    struct rose_ui_panel panel = rose_output_ui_obtain_panel(ui);

    // Compute surface's configuration parameters.
    struct rose_surface_configuration_parameters parameters = {
        .flags = rose_surface_configure_position,
        .width = surface->xdg_surface->surface->current.width,
        .height = surface->xdg_surface->surface->current.height};

    switch(surface->widget_type) {
        case rose_surface_widget_type_screen_lock:
            // fall-through

        case rose_surface_widget_type_background:
            parameters.x = (output_state.width - parameters.width) / 2;
            parameters.y = (output_state.height - parameters.height) / 2;
            break;

        case rose_surface_widget_type_notification: {
            int const margin = 5;

            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    parameters.x =
                        output_state.width - parameters.width - margin;

                    parameters.y = margin;
                    break;

                case rose_ui_panel_position_top:
                    parameters.x =
                        output_state.width - parameters.width - margin;

                    parameters.y = margin + (panel.is_visible ? panel.size : 0);
                    break;

                case rose_ui_panel_position_right:
                    parameters.x = margin;
                    parameters.y = margin;
                    break;

                case rose_ui_panel_position_left:
                    parameters.x =
                        output_state.width - parameters.width - margin;

                    parameters.y = margin;
                    break;

                default:
                    break;
            }

            break;
        }

        case rose_surface_widget_type_prompt: {
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    break;

                case rose_ui_panel_position_right:
                    break;

                case rose_ui_panel_position_left:
                    parameters.x = (panel.is_visible ? panel.size : 0);
                    break;

                case rose_ui_panel_position_top:
                    parameters.y = (panel.is_visible ? panel.size : 0);
                    break;

                default:
                    break;
            }

            break;
        }

        case rose_surface_widget_type_panel:
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    parameters.y = output_state.height - panel.size;
                    // fall-through

                case rose_ui_panel_position_top:
                    parameters.x = output_state.width / 2;
                    break;

                case rose_ui_panel_position_right:
                    parameters.x = output_state.width - panel.size;
                    parameters.y = output_state.height / 2;
                    break;

                case rose_ui_panel_position_left:
                    // fall-through

                default:
                    break;
            }

            break;

        default:
            break;
    }

    // Configure the surface.
    rose_surface_configure(surface, parameters);
}

static void
rose_output_ui_configure_surface(
    struct rose_output_ui* ui, struct rose_surface* surface) {
    // Obtain parent output's state.
    struct rose_output_state output_state =
        rose_output_state_obtain(ui->output);

    // Compute output's effective resolution.
    if(true) {
        output_state.width =
            (int)(0.5 + (output_state.width / output_state.scale));

        output_state.height =
            (int)(0.5 + (output_state.height / output_state.scale));
    }

    // Obtain the panel.
    struct rose_ui_panel panel = rose_output_ui_obtain_panel(ui);

    // Initialize surface's configuration parameters.
    struct rose_surface_configuration_parameters parameters = {
        .flags = rose_surface_configure_size | rose_surface_configure_activated,
        .width = 1,
        .height = 1,
        .is_activated = true};

    // Compute surface's size.
    switch(surface->widget_type) {
        case rose_surface_widget_type_screen_lock:
            // fall-through

        case rose_surface_widget_type_background:
            parameters.width = output_state.width;
            parameters.height = output_state.height;
            break;

        case rose_surface_widget_type_notification: {
            int const margin = 10;
            int const offset = (panel.is_visible ? panel.size : 0);

            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    // fall-through

                case rose_ui_panel_position_top:
                    parameters.width = output_state.width / 2 - margin;

                    parameters.height =
                        (output_state.height - offset) / 2 - margin;

                    break;

                case rose_ui_panel_position_right:
                    // fall-through

                case rose_ui_panel_position_left:
                    parameters.width =
                        (output_state.width - offset) / 2 - margin;

                    parameters.height = output_state.height / 2 - margin;
                    break;

                default:
                    break;
            }

            break;
        }

        case rose_surface_widget_type_prompt: {
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    // fall-through

                case rose_ui_panel_position_top:
                    parameters.width = output_state.width;
                    parameters.height = panel.size;
                    break;

                case rose_ui_panel_position_right:
                    // fall-through

                case rose_ui_panel_position_left:
                    parameters.width = output_state.width -
                                       (panel.is_visible ? panel.size : 0);

                    parameters.height = panel.size;
                    break;

                default:
                    break;
            }

            break;
        }

        case rose_surface_widget_type_panel: {
            switch(panel.position) {
                case rose_ui_panel_position_bottom:
                    // fall-through

                case rose_ui_panel_position_top:
                    parameters.width = output_state.width / 2;
                    parameters.height = panel.size;
                    break;

                case rose_ui_panel_position_right:
                    // fall-through

                case rose_ui_panel_position_left:
                    parameters.width = panel.size;
                    parameters.height = output_state.height / 2;
                    break;

                default:
                    break;
            }

            break;
        }

        default:
            break;
    }

    // Make sure surface's size is positive.
    parameters.width = max_(parameters.width, 1);
    parameters.height = max_(parameters.height, 1);

    // Configure the surface.
    rose_surface_configure(surface, parameters);
}

////////////////////////////////////////////////////////////////////////////////
// Surface selecting utility function.
////////////////////////////////////////////////////////////////////////////////

static struct wlr_surface*
rose_output_ui_select_surface_at(
    struct rose_output_ui* ui, double x, double y, double* x_local,
    double* y_local) {
    // Iterate through the list of mapped surfaces.
    struct rose_surface* surface = NULL;
    for(ptrdiff_t i = rose_surface_widget_type_count_ - 1;
        i >= rose_surface_special_widget_type_count_; --i) {
        wl_list_for_each(surface, &(ui->surfaces_mapped[i]), link_mapped) {
            // Skip surfaces which are not visible.
            if(!rose_output_ui_is_surface_visible(ui, surface)) {
                continue;
            }

            // Obtain surface's state.
            struct rose_surface_state state =
                rose_surface_state_obtain(surface);

            // Find a Wayland surface which belongs to the current surface.
            struct wlr_surface* result = NULL;
            if((x >= state.x) && (x <= (state.x + state.width)) && //
               (y >= state.y) && (y <= (state.y + state.height))) {
                // If the point is inside the surface's rectangle, then find any
                // Wayland surface under the given coordinates.
                result = wlr_xdg_surface_surface_at(
                    surface->xdg_surface, x - state.x, y - state.y, x_local,
                    y_local);
            } else {
                // Otherwise, if the point is outside the surface's rectangle,
                // find a pop-up surface under the given coordinates.
                result = wlr_xdg_surface_popup_surface_at(
                    surface->xdg_surface, x - state.x, y - state.y, x_local,
                    y_local);
            }

            // Return the found surface, if any.
            if(result != NULL) {
                return result;
            }
        }
    }

    // No surface has been found under the given coordinates.
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_initialize(
    struct rose_output_ui* ui, struct rose_output* output) {
    // Initialize the UI object.
    *ui = (struct rose_output_ui){.output = output};

    // Initialize the menu.
    rose_ui_menu_initialize(&(ui->menu), output);

    // Initialize lists of surfaces.
    for(ptrdiff_t i = 0; i != rose_surface_widget_type_count_; ++i) {
        wl_list_init(&(ui->surfaces[i]));
        wl_list_init(&(ui->surfaces_mapped[i]));
    }
}

void
rose_output_ui_destroy(struct rose_output_ui* ui) {
    // Destroy the menu.
    rose_ui_menu_destroy(&(ui->menu));

    // Destroy all surfaces.
    for(ptrdiff_t i = 0; i != rose_surface_widget_type_count_; ++i) {
        struct rose_surface* surface = NULL;
        struct rose_surface* _ = NULL;

        wl_list_for_each_safe(surface, _, &(ui->surfaces[i]), link) {
            rose_surface_request_close(surface), rose_surface_destroy(surface);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_update(struct rose_output_ui* ui) {
    // Update the menu.
    rose_ui_menu_update(&(ui->menu));

    // Configure all mapped surfaces.
    struct rose_surface* surface = NULL;
    for(ptrdiff_t i = 0; i != rose_surface_widget_type_count_; ++i) {
        wl_list_for_each(surface, &(ui->surfaces_mapped[i]), link_mapped) {
            rose_output_ui_position_surface(ui, surface);
            rose_output_ui_configure_surface(ui, surface);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Surface addition/removal interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_add_surface(
    struct rose_output_ui* ui, struct rose_surface* surface) {
    // Do nothing if the surface isn't a toplevel widget surface.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type == rose_surface_widget_type_none)) {
        return;
    }

    // Do nothing if the surface doesn't change its parent.
    if(ui == surface->parent.ui) {
        return;
    }

    // Send output enter event to the surface.
    rose_surface_output_enter(surface, ui->output);

    // Link the surface with the UI.
    wl_list_insert(&(ui->surfaces[surface->widget_type]), &(surface->link));
    surface->parent.ui = ui;

    // If the surface is mapped, then notify the UI.
    if(surface->is_mapped) {
        rose_output_ui_notify_surface_map(ui, surface);
    }
}

void
rose_output_ui_remove_surface(
    struct rose_output_ui* ui, struct rose_surface* surface) {
    // Do nothing if the surface isn't a toplevel widget surface.
    if((surface->type != rose_surface_type_toplevel) ||
       (surface->widget_type == rose_surface_widget_type_none)) {
        return;
    }

    // Do nothing if the given UI doesn't contain the surface.
    if(ui != surface->parent.ui) {
        return;
    }

    // Send output leave event to the surface.
    rose_surface_output_leave(surface, ui->output);

    // Request output's redraw.
    if(rose_output_ui_is_surface_visible(ui, surface)) {
        rose_output_request_redraw(ui->output);
    }

    // Sever all links between the surface and the UI.
    wl_list_remove(&(surface->link));
    wl_list_remove(&(surface->link_mapped));

    surface->parent.ui = NULL;

    wl_list_init(&(surface->link));
    wl_list_init(&(surface->link_mapped));
}

////////////////////////////////////////////////////////////////////////////////
// Selection interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_ui_selection
rose_output_ui_select(struct rose_output_ui* ui, double x, double y) {
    // Initialize an empty result.
    struct rose_output_ui_selection result = {};

    // Find a UI component under the given coordinates.
    if(ui->menu.is_visible &&
       ((x >= (ui->menu.area.x)) &&
        (x <= (ui->menu.area.x + ui->menu.area.width)) &&
        (y >= (ui->menu.area.y)) &&
        (y <= (ui->menu.area.y + ui->menu.area.height)))) {
        // If the given coordinates are inside menu's area, then select the
        // menu.
        result.type = rose_output_ui_selection_type_menu;
        result.menu = &(ui->menu);
    } else {
        // Otherwise, find a surface under the given coordinates.
        result.surface = rose_output_ui_select_surface_at(
            ui, x, y, &(result.x_local), &(result.y_local));

        if(result.surface != NULL) {
            result.type = rose_output_ui_selection_type_surface;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Query interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_output_ui_is_surface_visible(
    struct rose_output_ui* ui, struct rose_surface* surface) {
    // Obtain the master surface.
    surface =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // If the surface does not belong to the given UI, then it is not visible.
    if((surface->widget_type == rose_surface_widget_type_none) ||
       (surface->parent.ui != ui)) {
        return false;
    }

    // Surfaces which are not mapped are not visible.
    if(wl_list_empty(&(surface->link_mapped))) {
        return false;
    }

    // When the screen is locked only surfaces which represent special widgets
    // are visible.
    if(ui->output->context->is_screen_locked) {
        return (surface->widget_type < rose_surface_special_widget_type_count_);
    }

    // Perform type-dependent visibility test.
    if(surface->widget_type == rose_surface_widget_type_screen_lock) {
        return false;
    } else if(surface->widget_type == rose_surface_widget_type_panel) {
        return rose_output_ui_obtain_panel(ui).is_visible;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Event notification interface implementation: surface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_notify_surface_map(
    struct rose_output_ui* ui, struct rose_surface* surface) {
    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Do nothing if the master surface does not belong to the given UI.
    if((master->widget_type == rose_surface_widget_type_none) ||
       (master->parent.ui != ui)) {
        return;
    }

    // Handle the toplevel surface.
    if(surface->type == rose_surface_type_toplevel) {
        // Add the surface to the list of mapped surfaces.
        wl_list_remove(&(surface->link_mapped));
        wl_list_insert(
            &(ui->surfaces_mapped[surface->widget_type]),
            &(surface->link_mapped));

        // Configure the surface.
        rose_output_ui_position_surface(ui, surface);
        rose_output_ui_configure_surface(ui, surface);

        // Handle input focus.
        if((surface->widget_type == rose_surface_widget_type_screen_lock) ||
           (surface->widget_type == rose_surface_widget_type_prompt)) {
            rose_workspace_make_current(ui->output->context->current_workspace);
        }
    }

    // Request output's redraw.
    if(rose_output_ui_is_surface_visible(ui, surface)) {
        rose_output_request_redraw(ui->output);
    }
}

void
rose_output_ui_notify_surface_unmap(
    struct rose_output_ui* ui, struct rose_surface* surface) {
    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Do nothing if the master surface does not belong to the given UI.
    if((master->widget_type == rose_surface_widget_type_none) ||
       (master->parent.ui != ui)) {
        return;
    }

    // Request output's redraw.
    if(rose_output_ui_is_surface_visible(ui, surface)) {
        rose_output_request_redraw(ui->output);
    }

    // Handle the toplevel surface.
    if(surface->type == rose_surface_type_toplevel) {
        // Remove the surface from the list of mapped surfaces.
        wl_list_remove(&(surface->link_mapped));
        wl_list_init(&(surface->link_mapped));

        // Handle input focus.
        if((surface->widget_type == rose_surface_widget_type_screen_lock) ||
           (surface->widget_type == rose_surface_widget_type_prompt)) {
            rose_workspace_make_current(ui->output->context->current_workspace);
        }
    }
}

void
rose_output_ui_notify_surface_commit(
    struct rose_output_ui* ui, struct rose_surface* surface) {
    // Obtain the master surface.
    struct rose_surface* master =
        ((surface->type == rose_surface_type_toplevel) ? surface
                                                       : surface->master);

    // Do nothing if the master surface does not belong to the given UI.
    if((master->widget_type == rose_surface_widget_type_none) ||
       (master->parent.ui != ui)) {
        return;
    }

    // Handle initial surface commit.
    if((surface->type != rose_surface_type_subsurface) &&
       (surface->xdg_surface->initial_commit)) {
        if(surface->type == rose_surface_type_toplevel) {
            // Configure the toplevel surface.
            rose_output_ui_configure_surface(ui, surface);
        } else {
            // Obtain parent output's state.
            struct rose_output_state output_state =
                rose_output_state_obtain(ui->output);

            // Compute output's effective resolution.
            if(true) {
                output_state.width =
                    (int)(0.5 + (output_state.width / output_state.scale));

                output_state.height =
                    (int)(0.5 + (output_state.height / output_state.scale));
            }

            // Constrain the temporary surface.
            struct wlr_box constraints = {
                .x = -master->state.current.x,
                .y = -master->state.current.y,
                .width = output_state.width,
                .height = output_state.height};

            wlr_xdg_popup_unconstrain_from_box(
                surface->xdg_surface->popup, &constraints);
        }
    }

    // Position the toplevel surface.
    if((surface->type == rose_surface_type_toplevel) &&
       !(surface->xdg_surface->initial_commit)) {
        rose_output_ui_position_surface(ui, surface);
    }

    // Damage the parent output.
    if(rose_output_ui_is_surface_visible(ui, surface)) {
        rose_output_add_surface_damage(ui->output, surface);
    }
}
