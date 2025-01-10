// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_C77C7556E4AD4F86B73504A2F5399EC0
#define H_C77C7556E4AD4F86B73504A2F5399EC0

#include <wayland-server-protocol.h>

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_input;
struct wlr_tablet_v2_tablet;
struct wlr_tablet_v2_tablet_tool;

////////////////////////////////////////////////////////////////////////////////
// Tablet tool's motion event definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_tablet_tool_event_motion {
    // Associated protocol objects.
    struct wlr_tablet_v2_tablet* tablet;
    struct wlr_tablet_v2_tablet_tool* tool;

    // Event's data.
    uint32_t time;
    double x, y, dx, dy;
};

////////////////////////////////////////////////////////////////////////////////
// Tablet definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_tablet {
    // Parent input device.
    struct rose_input* parent;

    // Protocol object.
    struct wlr_tablet_v2_tablet* handle;

    // Event listeners.
    struct wl_listener listener_axis;
    struct wl_listener listener_proximity;
    struct wl_listener listener_button;
    struct wl_listener listener_tip;

    // List of associated tools.
    struct wl_list tools;

    // List link.
    struct wl_list link;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_tablet_initialize(struct rose_tablet* tablet, struct rose_input* parent);

void
rose_tablet_destroy(struct rose_tablet* tablet);

////////////////////////////////////////////////////////////////////////////////
// Input focusing interface.
////////////////////////////////////////////////////////////////////////////////

// Clears focus of all associated tablet tools.
void
rose_tablet_clear_focus(struct rose_tablet* tablet);

#endif // H_C77C7556E4AD4F86B73504A2F5399EC0
