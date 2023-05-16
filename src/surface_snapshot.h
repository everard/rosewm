// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_304FC2583FE74B86AC25AC347AC03F25
#define H_304FC2583FE74B86AC25AC347AC03F25

#include <wayland-server-protocol.h>

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct wlr_buffer;
struct wlr_surface;

////////////////////////////////////////////////////////////////////////////////
// Surface snapshot definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_surface_snapshot_type {
    // Note: Represents surface's content.
    rose_surface_snapshot_type_normal,
    // Note: Represents surface's decoration.
    rose_surface_snapshot_type_decoration,
    rose_n_surface_snapshot_types
};

struct rose_surface_snapshot {
    // Snapshot's type.
    enum rose_surface_snapshot_type type;

    // Surface's transform.
    enum wl_output_transform transform;

    // Surface's position and size.
    int x, y, w, h;

    // Surface's buffer.
    struct wlr_buffer* buffer;

    // Visible region of the buffer.
    struct {
        double x, y, w, h;
    } buffer_region;

    // List link.
    struct wl_list link;
};

////////////////////////////////////////////////////////////////////////////////
// Surface snapshot initialization-related definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_surface_snapshot_parameters {
    // Snapshot's type.
    enum rose_surface_snapshot_type type;

    // Target surface.
    struct wlr_surface* surface;

    // Target surface's position.
    int x, y;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_surface_snapshot_initialize(
    struct rose_surface_snapshot* snapshot,
    struct rose_surface_snapshot_parameters params);

// Precondition: The snapshot must be removable from its list.
void
rose_surface_snapshot_destroy(struct rose_surface_snapshot* snapshot);

#endif // H_304FC2583FE74B86AC25AC347AC03F25
