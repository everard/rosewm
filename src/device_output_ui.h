// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_94570FB344F04A87BCC4AAC1D4ACF666
#define H_94570FB344F04A87BCC4AAC1D4ACF666

#include "ui_menu.h"
#include "surface.h"

////////////////////////////////////////////////////////////////////////////////
// UI definition.
//
// Note: UI acts as a special type of workspace which contains output's widgets.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_ui {
    // Parent output.
    struct rose_output* output;

    // Menu.
    struct rose_ui_menu menu;

    // Lists of surfaces which act as parent output's widgets.
    struct wl_list surfaces[rose_surface_widget_type_count_];
    struct wl_list surfaces_mapped[rose_surface_widget_type_count_];
};

////////////////////////////////////////////////////////////////////////////////
// Selection definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_ui_selection_type {
    rose_output_ui_selection_type_none,
    rose_output_ui_selection_type_menu,
    rose_output_ui_selection_type_surface
};

struct rose_output_ui_selection {
    // Type of the selection.
    enum rose_output_ui_selection_type type;

    // Type-dependent data.
    union {
        // Selected menu.
        struct {
            struct rose_ui_menu* menu;
        };

        // Selected surface with surface-local coordinates.
        struct {
            struct wlr_surface* surface;
            double x_local, y_local;
        };
    };
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_initialize(
    struct rose_output_ui* ui, struct rose_output* output);

void
rose_output_ui_destroy(struct rose_output_ui* ui);

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_update(struct rose_output_ui* ui);

////////////////////////////////////////////////////////////////////////////////
// Surface addition/removal interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_add_surface(
    struct rose_output_ui* ui, struct rose_surface* surface);

void
rose_output_ui_remove_surface(
    struct rose_output_ui* ui, struct rose_surface* surface);

////////////////////////////////////////////////////////////////////////////////
// Selection interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_ui_selection
rose_output_ui_select(struct rose_output_ui* ui, double x, double y);

////////////////////////////////////////////////////////////////////////////////
// Query interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_output_ui_is_surface_visible(
    struct rose_output_ui* ui, struct rose_surface* surface);

////////////////////////////////////////////////////////////////////////////////
// Event notification interface: surface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_notify_surface_map(
    struct rose_output_ui* ui, struct rose_surface* surface);

void
rose_output_ui_notify_surface_unmap(
    struct rose_output_ui* ui, struct rose_surface* surface);

void
rose_output_ui_notify_surface_commit(
    struct rose_output_ui* ui, struct rose_surface* surface);

#endif // H_94570FB344F04A87BCC4AAC1D4ACF666
