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

enum { rose_output_mode_list_size_max = 128 };

struct rose_output_mode_list {
    struct rose_output_mode data[rose_output_mode_list_size_max];
    size_t size;
};

////////////////////////////////////////////////////////////////////////////////
// Output damage definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_output_damage {
    int x, y, width, height;
};

////////////////////////////////////////////////////////////////////////////////
// Output's adaptive sync state definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_output_adaptive_sync_state {
    rose_output_adaptive_sync_state_disabled,
    rose_output_adaptive_sync_state_enabled
};

////////////////////////////////////////////////////////////////////////////////
// Output's cursor type definition.
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
    // Parent server context.
    struct rose_server_context* context;

    // Underlying output device.
    struct wlr_output* device;

    // Layout this output device belongs to.
    struct wlr_output_layout* layout;

    // List of available modes.
    struct rose_output_mode_list modes;

    // Associated cursor.
    struct {
        // Cursor's underlying implementation.
        struct wlr_cursor* underlying;

        // Cursor's client-set surface.
        struct wlr_surface* surface;

        // Cursor's drag and drop surface.
        struct wlr_surface* drag_and_drop_surface;

        // Client-set surface's hotspot coordinates.
        int32_t hotspot_x, hotspot_y;

        // Cursor's type which defines its visual representation.
        enum rose_output_cursor_type type;

        // Flags.
        bool is_surface_set, has_moved;
    } cursor;

    // List of workspaces.
    struct wl_list workspaces;

    // Damage tracker.
    struct {
        // Damage array for different buffer ages.
        struct rose_output_damage damage[8];

        // Number of frames rendered without damage.
        unsigned frame_without_damage_count;
    } damage_tracker;

    // User interface.
    struct rose_output_ui ui;

    // A text which is currently being shown in the menu, if the menu is
    // visible.
    struct rose_ui_menu_text ui_menu_text;

    // Focus.
    struct rose_surface* focused_surface;
    struct rose_workspace* focused_workspace;

    // Rasters for the focused surface's title and the menu.
    struct {
        struct rose_raster *title, *menu;
    } rasters;

    // Event listeners.
    struct wl_listener listener_frame;
    struct wl_listener listener_needs_frame;

    struct wl_listener listener_commit;
    struct wl_listener listener_damage;

    struct wl_listener listener_destroy;
    struct wl_listener listener_cursor_surface_destroy;
    struct wl_listener listener_cursor_drag_and_drop_surface_destroy;

    // List link.
    struct wl_list link;

    // Output's ID.
    unsigned id;

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

enum rose_output_configuration_type {
    rose_output_configure_adaptive_sync = 0x01,
    rose_output_configure_transform = 0x02,
    rose_output_configure_scale = 0x04,
    rose_output_configure_mode = 0x08
};

// Output's configuration mask. Is a bitwise OR of zero or more values from the
// rose_output_configuration_type enumeration.
typedef unsigned rose_output_configuration_mask;

struct rose_output_configuration_parameters {
    // Output's configuration flags.
    rose_output_configuration_mask flags;

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
rose_output_initialize(
    struct rose_server_context* context, struct wlr_output* device);

// Note: This function is called automatically upon output device's destruction.
void
rose_output_destroy(struct rose_output* output);

////////////////////////////////////////////////////////////////////////////////
// Configuration interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_output_configure(
    struct rose_output* output,
    struct rose_output_configuration_parameters parameters);

////////////////////////////////////////////////////////////////////////////////
// Workspace focusing interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_focus_workspace(
    struct rose_output* output, struct rose_workspace* workspace);

void
rose_output_focus_workspace_relative(
    struct rose_output* output, enum rose_output_focus_direction direction);

////////////////////////////////////////////////////////////////////////////////
// Workspace addition/removal interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_output_add_workspace(
    struct rose_output* output, struct rose_workspace* workspace);

void
rose_output_remove_workspace(
    struct rose_output* output, struct rose_workspace* workspace);

////////////////////////////////////////////////////////////////////////////////
// Damage handling interface.
////////////////////////////////////////////////////////////////////////////////

// Returns damage for the given buffer age and shifts damage array in a single
// operation.
struct rose_output_damage
rose_output_consume_damage(struct rose_output* output, int buffer_age);

void
rose_output_add_damage(
    struct rose_output* output, struct rose_output_damage damage);

void
rose_output_add_surface_damage(
    struct rose_output* output, struct rose_surface* surface);

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
rose_output_cursor_set(
    struct rose_output* output, enum rose_output_cursor_type type);

void
rose_output_cursor_warp(struct rose_output* output, double x, double y);

void
rose_output_cursor_client_surface_set(
    struct rose_output* output, struct wlr_surface* surface, int32_t hotspot_x,
    int32_t hotspot_y);

void
rose_output_cursor_drag_and_drop_surface_set(
    struct rose_output* output, struct wlr_surface* surface);

#endif // H_0DF3C518ADEA43DB9AA264FB4CF22816
