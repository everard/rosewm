// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_94570FB344F04A87BCC4AAC1D4ACF666
#define H_94570FB344F04A87BCC4AAC1D4ACF666

#include "device_output_widget.h"
#include "ui_menu.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct wlr_surface;

////////////////////////////////////////////////////////////////////////////////
// UI definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_ui {
    // Pointer to the parent output.
    struct rose_output* output;

    // Menu.
    struct rose_ui_menu menu;

    // Lists of widgets.
    struct wl_list widgets[rose_output_n_widget_types];
    struct wl_list widgets_mapped[rose_output_n_widget_types];
};

////////////////////////////////////////////////////////////////////////////////
// Selection definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_ui_selection_type {
    rose_output_ui_selection_type_none,
    rose_output_ui_selection_type_menu,
    rose_output_ui_selection_type_widget
};

struct rose_output_ui_selection {
    enum rose_output_ui_selection_type type;

    // Type-dependent data.
    union {
        // Selected menu.
        struct rose_ui_menu* menu;

        // Selected widget's surface with surface-local coordinates.
        struct {
            struct wlr_surface* surface;
            double x_local, y_local;
        } widget;
    };
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_initialize(struct rose_output_ui* ui,
                          struct rose_output* output);

void
rose_output_ui_destroy(struct rose_output_ui* ui);

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_ui_update(struct rose_output_ui* ui);

////////////////////////////////////////////////////////////////////////////////
// Selection interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_ui_selection
rose_output_ui_select(struct rose_output_ui* ui, double x, double y);

#endif // H_94570FB344F04A87BCC4AAC1D4ACF666
