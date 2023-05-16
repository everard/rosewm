// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "surface_snapshot.h"

#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_snapshot_initialize(
    struct rose_surface_snapshot* snapshot,
    struct rose_surface_snapshot_parameters params) {
    // Obtain the visible region of the surface's buffer.
    struct wlr_fbox box = {};
    wlr_surface_get_buffer_source_box(params.surface, &box);

    // Set snapshot's parameters.
    *snapshot = (struct rose_surface_snapshot){
        .type = params.type,
        // Select the transform based on snapshot's type.
        .transform = ((params.type == rose_surface_snapshot_type_normal)
                          ? params.surface->current.transform
                          : WL_OUTPUT_TRANSFORM_NORMAL),
        .x = params.x,
        .y = params.y,
        .w = params.surface->current.width,
        .h = params.surface->current.height,
        .buffer_region = {
            .x = box.x, .y = box.y, .w = box.width, .h = box.height}};

    // Initialize list link.
    wl_list_init(&(snapshot->link));

    // Obtain and lock surface's buffer, if needed.
    if(wlr_surface_has_buffer(params.surface) &&
       (snapshot->type == rose_surface_snapshot_type_normal)) {
        snapshot->buffer = wlr_buffer_lock(&(params.surface->buffer->base));
    }
}

void
rose_surface_snapshot_destroy(struct rose_surface_snapshot* snapshot) {
    // Release the buffer, if any.
    snapshot->buffer = (wlr_buffer_unlock(snapshot->buffer), NULL);

    // Remove the snapshot from the list.
    wl_list_remove(&(snapshot->link));
    wl_list_init(&(snapshot->link));
}
