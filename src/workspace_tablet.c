// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"

#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_tablet_v2.h>

////////////////////////////////////////////////////////////////////////////////
// Event notification interface implementation: tablet device.
////////////////////////////////////////////////////////////////////////////////

void
rose_workspace_notify_tablet_tool_warp(
    struct rose_workspace* workspace,
    struct rose_tablet_tool_event_motion event) {
    // Construct pointer event.
    struct rose_pointer_event_motion_absolute pointer_event = {
        .time_msec = event.time_msec, .x = event.x, .y = event.y};

    // Notify the workspace of pointer event.
    rose_workspace_notify_pointer_warp(workspace, pointer_event);

    // Obtain the seat.
    struct wlr_seat* seat = workspace->context->seat;

    // Obtain focused surface.
    struct wlr_surface* surface = seat->pointer_state.focused_surface;

    // Send tablet events.
    if(surface != NULL) {
        // Send proximity event.
        wlr_send_tablet_v2_tablet_tool_proximity_in(
            event.tool, event.tablet, surface);

        // Send motion event.
        wlr_send_tablet_v2_tablet_tool_motion(
            event.tool, seat->pointer_state.sx, seat->pointer_state.sy);
    } else {
        // Send proximity event.
        wlr_send_tablet_v2_tablet_tool_proximity_out(event.tool);
    }
}
