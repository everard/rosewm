// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "device_output_ui.h"

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>

////////////////////////////////////////////////////////////////////////////////
// Surface selection utility function.
////////////////////////////////////////////////////////////////////////////////

static struct wlr_surface*
rose_output_ui_select_surface_at(struct rose_output_ui* ui, double x, double y,
                                 double* x_local, double* y_local) {
    // Iterate through the list of mapped widgets.
    //
    // Note: Only normal widgets can be selected. Special widgets are treated
    // separately: screen lock widget will be automatically selected when the
    // screen is locked; background widget will never be selected.
    struct rose_output_widget* widget = NULL;
    for(ptrdiff_t i = rose_output_n_widget_types - 1;
        i >= rose_output_n_special_widget_types; --i) {
        wl_list_for_each(widget, &(ui->widgets_mapped[i]), link_mapped) {
            // Skip widgets which are not visible.
            if(!rose_output_widget_is_visible(widget)) {
                continue;
            }

            // Obtain widget's state.
            struct rose_output_widget_state state =
                rose_output_widget_state_obtain(widget);

            // Find a surface which belongs to the current widget.
            struct wlr_surface* surface = NULL;
            if((x >= state.x) && (x <= (state.x + state.w)) && //
               (y >= state.y) && (y <= (state.y + state.h))) {
                // If the point is inside the widget's rectangle, then find any
                // surface under the given coordinates.
                surface = wlr_xdg_surface_surface_at( //
                    widget->xdg_surface, x - state.x, y - state.y, x_local,
                    y_local);
            } else {
                // Otherwise, if the point is outside the widget's rectangle,
                // find a pop-up surface under the given coordinates.
                surface = wlr_xdg_surface_popup_surface_at(
                    widget->xdg_surface, x - state.x, y - state.y, x_local,
                    y_local);
            }

            // Return the found surface, if any.
            if(surface != NULL) {
                return surface;
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
rose_output_ui_initialize(struct rose_output_ui* ui,
                          struct rose_output* output) {
    // Initialize the UI object.
    *ui = (struct rose_output_ui){.output = output};

    // Initialize the menu.
    rose_ui_menu_initialize(&(ui->menu), output);

    // Initialize lists of widgets.
    for(ptrdiff_t i = 0; i != rose_output_n_widget_types; ++i) {
        wl_list_init(&(ui->widgets[i]));
        wl_list_init(&(ui->widgets_mapped[i]));
    }
}

void
rose_output_ui_destroy(struct rose_output_ui* ui) {
    // Destroy the menu.
    rose_ui_menu_destroy(&(ui->menu));

    // Destroy all widgets.
    for(ptrdiff_t i = 0; i != rose_output_n_widget_types; ++i) {
        struct rose_output_widget* widget = NULL;
        struct rose_output_widget* _ = NULL;

        wl_list_for_each_safe(widget, _, &(ui->widgets[i]), link) {
            // Close the underlying top-level XDG surface.
            wlr_xdg_toplevel_send_close(widget->xdg_surface->toplevel);

            // Destroy the widget.
            rose_output_widget_destroy(widget);
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

    // Configure all widgets.
    struct rose_output_widget* widget = NULL;
    for(ptrdiff_t i = 0; i != rose_output_n_widget_types; ++i) {
        wl_list_for_each(widget, &(ui->widgets[i]), link) {
            // Note: Each widget configures itself based on its type.
            rose_output_widget_configure(widget);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Selection interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_ui_selection
rose_output_ui_select(struct rose_output_ui* ui, double x, double y) {
    // Initialize an empty selection.
    struct rose_output_ui_selection result = {};

    // Find UI component under the given coordinates.
    if(ui->menu.is_visible &&
       ((x >= ui->menu.area.x) && (x <= (ui->menu.area.x + ui->menu.area.w)) &&
        (y >= ui->menu.area.y) && (y <= (ui->menu.area.y + ui->menu.area.h)))) {
        // If the given coordinates are inside menu's area, then select the
        // menu.
        result.type = rose_output_ui_selection_type_menu;
        result.menu = &(ui->menu);
    } else {
        // Otherwise, find a widget's surface under the given coordinates.
        double x_local = 0.0, y_local = 0.0;
        struct wlr_surface* surface =
            rose_output_ui_select_surface_at(ui, x, y, &x_local, &y_local);

        if(surface != NULL) {
            result.type = rose_output_ui_selection_type_widget;
            result.widget.surface = surface;
            result.widget.x_local = x_local;
            result.widget.y_local = y_local;
        }
    }

    return result;
}
