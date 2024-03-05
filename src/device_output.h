// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_0DF3C518ADEA43DB9AA264FB4CF22816
#define H_0DF3C518ADEA43DB9AA264FB4CF22816

#include "device_output_ui.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_raster;
struct rose_server_context;

struct rose_surface;
struct rose_workspace;

struct wlr_cursor;
struct wlr_surface;

struct wlr_output;
struct wlr_output_layout;

////////////////////////////////////////////////////////////////////////////////
// Output mode definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_mode {
    int width, height, rate;
};

////////////////////////////////////////////////////////////////////////////////
// Output mode list definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_output_mode_max_count = 128 };

struct rose_output_mode_list {
    struct rose_output_mode data[rose_output_mode_max_count];
    size_t size;
};

////////////////////////////////////////////////////////////////////////////////
// Output adaptive sync state definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_adaptive_sync_state {
    rose_output_adaptive_sync_state_disabled,
    rose_output_adaptive_sync_state_enabled
};

////////////////////////////////////////////////////////////////////////////////
// Output cursor type definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_cursor_type {
    rose_output_cursor_type_unspecified,
    rose_output_cursor_type_default,
    rose_output_cursor_type_moving,
    rose_output_cursor_type_resizing_north,
    rose_output_cursor_type_resizing_south,
    rose_output_cursor_type_resizing_east,
    rose_output_cursor_type_resizing_west,
    rose_output_cursor_type_resizing_north_east,
    rose_output_cursor_type_resizing_north_west,
    rose_output_cursor_type_resizing_south_east,
    rose_output_cursor_type_resizing_south_west,
    rose_output_cursor_type_client,
    rose_output_cursor_type_count_
};

////////////////////////////////////////////////////////////////////////////////
// Output definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_output {
    // Pointer to the server context.
    struct rose_server_context* context;

    // Pointer to the underlying output device.
    struct wlr_output* device;

    // Layout this output device belongs to.
    // Note: The sole purpose of this object is being a glue between this output
    // device and its cursor.
    struct wlr_output_layout* layout;

    // List of available modes.
    struct rose_output_mode_list modes;

    // Associated cursor.
    struct {
        // Cursor's underlying implementation.
        // Note: Lightweight alternative is no longer usable.
        struct wlr_cursor* underlying;

        // Cursor's client-set surface.
        struct wlr_surface* surface;

        // Client-set surface's hotspot coordinates.
        int32_t hotspot_x, hotspot_y;

        // Cursor's type which defines its visual representation.
        enum rose_output_cursor_type type;

        // Flags.
        bool is_surface_set, has_moved;
    } cursor;

    // List of workspaces.
    struct wl_list workspaces;

    // User interface.
    struct rose_output_ui ui;

    // A text which is currently being shown in the menu, if the menu is
    // visible.
    struct rose_ui_menu_text ui_menu_text;

    // Focus.
    struct rose_surface* focused_surface;
    struct rose_workspace* focused_workspace;

    // Rasters for the title bar and the menu.
    struct {
        struct rose_raster *title, *menu;
    } rasters;

    // Event listeners.
    struct wl_listener listener_frame;
    struct wl_listener listener_needs_frame;

    struct wl_listener listener_commit;
    struct wl_listener listener_damage;

    struct wl_listener listener_destroy;
    struct wl_listener listener_cursor_client_surface_destroy;

    // List links.
    struct wl_list link;

    // Output's ID and number of frames rendered without damage.
    unsigned id, frame_without_damage_count;

    // Flags.
    bool is_scanned_out, is_frame_scheduled, is_rasters_update_requested;
};

////////////////////////////////////////////////////////////////////////////////
// Output state definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_state {
    // Output's ID.
    unsigned id;

    // Adaptive sync state.
    enum rose_output_adaptive_sync_state adaptive_sync_state;

    // Output's transform.
    enum wl_output_transform transform;

    // Output's DPI and refresh rate.
    int dpi, rate;

    // Output's geometry.
    int width, height;

    // Output's scaling factor.
    double scale;

    // Output's flags.
    bool is_scanned_out, is_frame_scheduled, is_rasters_update_requested;
};

////////////////////////////////////////////////////////////////////////////////
// Configuration-related definitions.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_configure_type {
    rose_output_configure_adaptive_sync = 0x01,
    rose_output_configure_transform = 0x02,
    rose_output_configure_scale = 0x04,
    rose_output_configure_mode = 0x08
};

// Output's configuration mask. Is a bitwise OR of zero or more values from the
// rose_output_configure_type enumeration.
typedef unsigned rose_output_configure_mask;

struct rose_output_configure_parameters {
    // Output's configuration flags.
    rose_output_configure_mask flags;

    // Output's requested adaptive sync state.
    enum rose_output_adaptive_sync_state adaptive_sync_state;

    // Output's requested transform.
    enum wl_output_transform transform;

    // Output's requested scaling factor.
    double scale;

    // Output's requested mode.
    struct rose_output_mode mode;
};

////////////////////////////////////////////////////////////////////////////////
// Focus direction definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_focus_direction {
    rose_output_focus_direction_backward,
    rose_output_focus_direction_forward
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_initialize(struct rose_server_context* context,
                       struct wlr_output* device);

// Note: This function is called automatically upon output device's destruction.
void
rose_output_destroy(struct rose_output* output);

////////////////////////////////////////////////////////////////////////////////
// Configuration interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_output_configure(struct rose_output* output,
                      struct rose_output_configure_parameters parameters);

////////////////////////////////////////////////////////////////////////////////
// Workspace focusing interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_focus_workspace(struct rose_output* output,
                            struct rose_workspace* workspace);

void
rose_output_focus_workspace_relative(
    struct rose_output* output, enum rose_output_focus_direction direction);

////////////////////////////////////////////////////////////////////////////////
// Workspace addition/removal interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_add_workspace(struct rose_output* output,
                          struct rose_workspace* workspace);

void
rose_output_remove_workspace(struct rose_output* output,
                             struct rose_workspace* workspace);

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_request_redraw(struct rose_output* output);

void
rose_output_schedule_frame(struct rose_output* output);

////////////////////////////////////////////////////////////////////////////////
// State query interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_state
rose_output_state_obtain(struct rose_output* output);

struct rose_output_mode_list
rose_output_mode_list_obtain(struct rose_output* output);

////////////////////////////////////////////////////////////////////////////////
// Cursor manipulation interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_cursor_set(struct rose_output* output,
                       enum rose_output_cursor_type type);

void
rose_output_cursor_warp(struct rose_output* output, double x, double y);

void
rose_output_cursor_client_surface_set( //
    struct rose_output* output, struct wlr_surface* surface, int32_t hotspot_x,
    int32_t hotspot_y);

#endif // H_0DF3C518ADEA43DB9AA264FB4CF22816
