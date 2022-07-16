// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_EA01F7650FA4419F8B00BB4A2007EC35
#define H_EA01F7650FA4419F8B00BB4A2007EC35

#include "ui_panel.h"

#include <stdint.h>
#include <time.h>

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_output;
struct rose_server_context;

////////////////////////////////////////////////////////////////////////////////
// Workspace definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_workspace_mode {
    rose_workspace_mode_normal,
    rose_workspace_mode_interactive_move,
    rose_workspace_mode_interactive_resize_north,
    rose_workspace_mode_interactive_resize_south,
    rose_workspace_mode_interactive_resize_east,
    rose_workspace_mode_interactive_resize_west,
    rose_workspace_mode_interactive_resize_north_east,
    rose_workspace_mode_interactive_resize_north_west,
    rose_workspace_mode_interactive_resize_south_east,
    rose_workspace_mode_interactive_resize_south_west
};

struct rose_workspace {
    // Pointers to the server context and output.
    struct rose_server_context* ctx;
    struct rose_output* output;

    // Lists of surfaces.
    struct wl_list surfaces;
    struct wl_list surfaces_mapped;
    struct wl_list surfaces_visible;

    // Pointer's state.
    struct {
        // Position data.
        double x, y, x_saved, y_saved;

        // Last movement time.
        uint32_t movement_time_msec;

        // Timer's state.
        struct wl_event_source* timer;
        bool is_timer_armed;
    } pointer;

    // Extents.
    int w, h;

    // Focus.
    struct rose_surface* focused_surface;

    // Panel's state.
    struct rose_ui_panel panel, panel_saved;

    // List links.
    struct wl_list link;
    struct wl_list link_output;

    // Transaction's state.
    struct {
        // Number of surfaces with running transaction.
        long long sentinel;

        // Snapshot data.
        struct {
            struct wl_list surfaces;
            struct rose_ui_panel panel;
        } snapshot;

        // Transaction's starting time.
        struct timespec start_time;

        // Transaction's watchdog timer.
        struct wl_event_source* timer;
    } transaction;

    // Workspace's ID and number of frames rendered without new surface commits.
    unsigned id, n_frames_without_commits;

    // Current mode.
    enum rose_workspace_mode mode;
};

////////////////////////////////////////////////////////////////////////////////
// Focus direction definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_workspace_focus_direction {
    rose_workspace_focus_direction_backward,
    rose_workspace_focus_direction_forward
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_workspace_initialize(struct rose_workspace* workspace,
                          struct rose_server_context* ctx);

void
rose_workspace_destroy(struct rose_workspace* workspace);

////////////////////////////////////////////////////////////////////////////////
// List ordering interface.
////////////////////////////////////////////////////////////////////////////////

// Finds workspace's position in the list specified by the given head. List link
// is determined by the given offset.
struct wl_list*
rose_workspace_find_position_in_list( //
    struct wl_list* head, struct rose_workspace* workspace, size_t link_offset);

////////////////////////////////////////////////////////////////////////////////
// Input focusing interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_make_current(struct rose_workspace* workspace);

bool
rose_workspace_is_current(struct rose_workspace* workspace);

////////////////////////////////////////////////////////////////////////////////
// Surface focusing interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_focus_surface(struct rose_workspace* workspace,
                             struct rose_surface* surface);

void
rose_workspace_focus_surface_relative(
    struct rose_workspace* workspace,
    enum rose_workspace_focus_direction direction);

////////////////////////////////////////////////////////////////////////////////
// Surface configuration interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_surface_configure( //
    struct rose_workspace* workspace, struct rose_surface* surface,
    struct rose_surface_configure_parameters params);

////////////////////////////////////////////////////////////////////////////////
// Surface addition/removal/reordering interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_add_surface(struct rose_workspace* workspace,
                           struct rose_surface* surface);

void
rose_workspace_remove_surface(struct rose_workspace* workspace,
                              struct rose_surface* surface);

void
rose_workspace_reposition_surface( //
    struct rose_workspace* workspace, struct rose_surface* surface,
    struct rose_surface* destination);

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_set_panel(struct rose_workspace* workspace,
                         struct rose_ui_panel panel);

void
rose_workspace_request_redraw(struct rose_workspace* workspace);

void
rose_workspace_cancel_interactive_mode(struct rose_workspace* workspace);

void
rose_workspace_commit_interactive_mode(struct rose_workspace* workspace);

////////////////////////////////////////////////////////////////////////////////
// Pointer manipulation interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_pointer_warp(struct rose_workspace* workspace,
                            uint32_t time_msec, double x, double y);

////////////////////////////////////////////////////////////////////////////////
// Event notification interface: pointer device.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_notify_pointer_axis(struct rose_workspace* workspace,
                                   struct rose_pointer_event_axis event);

void
rose_workspace_notify_pointer_button(struct rose_workspace* workspace,
                                     struct rose_pointer_event_button event);

void
rose_workspace_notify_pointer_move(struct rose_workspace* workspace,
                                   struct rose_pointer_event_motion event);

void
rose_workspace_notify_pointer_warp(
    struct rose_workspace* workspace,
    struct rose_pointer_event_motion_absolute event);

////////////////////////////////////////////////////////////////////////////////
// Event notification interface: output device.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_notify_output_mode(struct rose_workspace* workspace,
                                  struct rose_output* output);

////////////////////////////////////////////////////////////////////////////////
// Event notification interface: surface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_notify_surface_name_update(struct rose_workspace* workspace,
                                          struct rose_surface* surface);

void
rose_workspace_notify_surface_map(struct rose_workspace* workspace,
                                  struct rose_surface* surface);

void
rose_workspace_notify_surface_unmap(struct rose_workspace* workspace,
                                    struct rose_surface* surface);

void
rose_workspace_notify_surface_commit(struct rose_workspace* workspace,
                                     struct rose_surface* surface);

////////////////////////////////////////////////////////////////////////////////
// Transaction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_transaction_start(struct rose_workspace* workspace);

void
rose_workspace_transaction_update(struct rose_workspace* workspace);

void
rose_workspace_transaction_commit(struct rose_workspace* workspace);

#endif // H_EA01F7650FA4419F8B00BB4A2007EC35
