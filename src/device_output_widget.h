// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_8047E3440EA24792A9E942CDA0A8710E
#define H_8047E3440EA24792A9E942CDA0A8710E

#include <wayland-server-protocol.h>

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_output;
struct rose_output_ui;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;

////////////////////////////////////////////////////////////////////////////////
// Widget definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_widget_type {
    // Special widgets.
    rose_output_widget_type_screen_lock,
    rose_output_widget_type_background,
    rose_output_n_special_widget_types,

    // Normal widgets.
    rose_output_widget_type_notification = rose_output_n_special_widget_types,
    rose_output_widget_type_prompt,
    rose_output_widget_type_panel,
    rose_output_n_widget_types
};

enum rose_output_widget_surface_type {
    rose_output_widget_surface_type_subsurface,
    rose_output_widget_surface_type_temporary,
    rose_output_widget_surface_type_toplevel
};

struct rose_output_widget_state {
    int x, y, w, h;
};

struct rose_output_widget {
    // Type of the widget and type of its underlying surface.
    enum rose_output_widget_type type;
    enum rose_output_widget_surface_type surface_type;

    // Position and size.
    struct rose_output_widget_state state;

    // Type-specific data.
    union {
        // Toplevel surface data.
        struct {
            // Pointer to the parent output.
            struct rose_output* output;

            // Pointer to the parent UI object.
            struct rose_output_ui* ui;

            // Pointer to underlying XDG surface.
            struct wlr_xdg_surface* xdg_surface;
        };

        // Temporary surface and subsurface data (these are the same).
        struct {
            struct rose_output_widget* master;
        };
    };

    // Event listeners.
    struct wl_listener listener_map;
    struct wl_listener listener_unmap;
    struct wl_listener listener_commit;

    struct wl_listener listener_new_subsurface;
    struct wl_listener listener_new_popup;
    struct wl_listener listener_destroy;

    // Lists of child entities.
    struct wl_list subsurfaces;
    struct wl_list temporaries;

    // List links.
    struct wl_list link;
    struct wl_list link_mapped;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_widget_initialize( //
    struct rose_output_ui* ui, struct wlr_xdg_toplevel* toplevel,
    enum rose_output_widget_type type);

void
rose_output_widget_destroy(struct rose_output_widget* widget);

////////////////////////////////////////////////////////////////////////////////
// Configuration interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_widget_make_current(struct rose_output_widget* widget);

void
rose_output_widget_configure(struct rose_output_widget* widget);

////////////////////////////////////////////////////////////////////////////////
// State query interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_widget_state
rose_output_widget_state_obtain(struct rose_output_widget* widget);

bool
rose_output_widget_is_visible(struct rose_output_widget* widget);

#endif // H_8047E3440EA24792A9E942CDA0A8710E
