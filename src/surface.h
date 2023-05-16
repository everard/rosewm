// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_A76D639897FD4AFA9AE6F973C9700F38
#define H_A76D639897FD4AFA9AE6F973C9700F38

#include "surface_snapshot.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_output;
struct rose_workspace;

struct wlr_xdg_surface;
struct wlr_xdg_toplevel;
struct wlr_xdg_toplevel_decoration_v1;

struct wlr_surface;
struct wlr_subsurface;

struct wlr_seat;
struct wlr_pointer_constraint_v1;

////////////////////////////////////////////////////////////////////////////////
// Surface state definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_surface_state {
    int x, y, w, h;
    bool is_activated, is_maximized, is_fullscreen;
};

////////////////////////////////////////////////////////////////////////////////
// Surface definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_surface_type {
    rose_surface_type_subsurface,
    rose_surface_type_temporary,
    rose_surface_type_toplevel
};

struct rose_surface {
    // Type of the surface.
    enum rose_surface_type type;

    // Surface's state vector.
    struct {
        struct rose_surface_state current;
        struct rose_surface_state pending;
        struct rose_surface_state saved;
    } state;

    // Pointers to the underlying implementation.
    struct wlr_subsurface* subsurface;
    struct wlr_xdg_surface* xdg_surface;

    // Surface's type-specific data.
    union {
        // Toplevel surface data.
        struct {
            // Pointer to the parent workspace.
            struct rose_workspace* workspace;

            // Pointers to surface's decoration and pointer constraint.
            struct wlr_xdg_toplevel_decoration_v1* xdg_decoration;
            struct wlr_pointer_constraint_v1* pointer_constraint;
        };

        // Temporary surface and subsurface data (these are the same).
        struct {
            // Pointer to the master surface.
            struct rose_surface* master;
        };
    };

    // Event listeners.
    struct wl_listener listener_decoration_request_mode;
    struct wl_listener listener_decoration_destroy;

    struct wl_listener listener_pointer_constraint_set_region;
    struct wl_listener listener_pointer_constraint_destroy;

    struct wl_listener listener_request_maximize;
    struct wl_listener listener_request_fullscreen;

    struct wl_listener listener_set_title;
    struct wl_listener listener_set_app_id;

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
    struct wl_list link_layout;
    struct wl_list link_mapped;
    struct wl_list link_visible;

    // Storage for surface's snapshots.
    struct rose_surface_snapshot snapshots[rose_n_surface_snapshot_types];

    // Flags.
    bool is_mapped, is_visible, is_name_updated;
    bool is_transaction_running;
};

////////////////////////////////////////////////////////////////////////////////
// Surface initialization-related definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_surface_parameters {
    // Surface's parent workspace.
    struct rose_workspace* workspace;

    // Underlying top-level XDG surface.
    struct wlr_xdg_toplevel* toplevel;

    // Associated pointer constraint.
    struct wlr_pointer_constraint_v1* pointer_constraint;
};

////////////////////////////////////////////////////////////////////////////////
// Surface configuration-related definitions.
////////////////////////////////////////////////////////////////////////////////

enum rose_surface_configure_type {
    rose_surface_configure_size = 0x01,
    rose_surface_configure_position = 0x02,
    rose_surface_configure_activated = 0x04,
    rose_surface_configure_maximized = 0x08,
    rose_surface_configure_fullscreen = 0x10,
    rose_surface_configure_no_transaction = 0x20
};

// Surface's configuration mask. Is a bitwise OR of zero or more values from the
// rose_surface_configure_type enumeration.
typedef unsigned rose_surface_configure_mask;

struct rose_surface_configure_parameters {
    // Surface's configuration flags.
    rose_surface_configure_mask flags;

    // Surface's requested position and size.
    int x, y, w, h;

    // Surface's requested state.
    bool is_activated, is_maximized, is_fullscreen;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_initialize(struct rose_surface_parameters params);

void
rose_surface_destroy(struct rose_surface* surface);

////////////////////////////////////////////////////////////////////////////////
// Extension initialization interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_decoration_initialize(
    struct wlr_xdg_toplevel_decoration_v1* xdg_decoration);

void
rose_surface_pointer_constraint_initialize(
    struct wlr_pointer_constraint_v1* pointer_constraint);

////////////////////////////////////////////////////////////////////////////////
// State change requesting interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_request_close(struct rose_surface* surface);

////////////////////////////////////////////////////////////////////////////////
// Output entering/leaving interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_output_enter( //
    struct rose_surface* surface, struct rose_output* output);

void
rose_surface_output_leave( //
    struct rose_surface* surface, struct rose_output* output);

////////////////////////////////////////////////////////////////////////////////
// Configuration interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_make_current(struct rose_surface* surface, struct wlr_seat* seat);

void
rose_surface_configure(struct rose_surface* surface,
                       struct rose_surface_configure_parameters params);

////////////////////////////////////////////////////////////////////////////////
// State query interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_surface_state
rose_surface_state_obtain(struct rose_surface* surface);

////////////////////////////////////////////////////////////////////////////////
// Transaction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_transaction_initialize_snapshot(struct rose_surface* surface);

void
rose_surface_transaction_destroy_snapshot(struct rose_surface* surface);

void
rose_surface_transaction_commit(struct rose_surface* surface);

#endif // H_A76D639897FD4AFA9AE6F973C9700F38
