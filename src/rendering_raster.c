// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "rendering_raster.h"

#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>

#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define cast_(type, x) ((type)(x))
#define unused_(x) cast_(void, (x))

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

////////////////////////////////////////////////////////////////////////////////
// Buffer interface implementation.
////////////////////////////////////////////////////////////////////////////////

static void
rose_raster_buffer_destroy(struct wlr_buffer* buffer) {
    free(buffer);
}

static bool
rose_raster_buffer_begin_data_ptr_access( //
    struct wlr_buffer* buffer, uint32_t flags, void** data, uint32_t* format,
    size_t* stride) {
    unused_(flags);

    // Set the fields.
    *data = cast_(struct rose_raster*, buffer)->pixels;
    *stride = cast_(size_t, buffer->width) * 4U;
    *format = DRM_FORMAT_ARGB8888;

    // This operation always succeeds.
    return true;
}

static void
rose_raster_buffer_end_data_ptr_access(struct wlr_buffer* buffer) {
    unused_(buffer);
}

static struct wlr_buffer_impl const rose_raster_buffer_implementation = {
    .destroy = rose_raster_buffer_destroy,
    .begin_data_ptr_access = rose_raster_buffer_begin_data_ptr_access,
    .end_data_ptr_access = rose_raster_buffer_end_data_ptr_access};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_raster*
rose_raster_initialize(struct wlr_renderer* renderer, int width, int height) {
    // Initialize a new raster object.
    struct rose_raster* raster =
        rose_raster_initialize_without_texture(width, height);

    if(raster != NULL) {
        // Initialize raster's texture.
        raster->texture = wlr_texture_from_buffer(renderer, &(raster->base));
        if(raster->texture == NULL) {
            return rose_raster_destroy(raster), NULL;
        }
    }

    // Return initialized raster object.
    return raster;
}

struct rose_raster*
rose_raster_initialize_without_texture(int width, int height) {
    // Clamp the dimensions.
    width = clamp_(width, 1, 32768);
    height = clamp_(height, 1, 32768);

    // Compute raster's pixel data size.
    // Note: This computation never wraps around.
    uint64_t data_size = cast_(uint64_t, width) * cast_(uint64_t, height) * 4U;

    // Compute and validate raster object's size.
    size_t object_size = //
        sizeof(struct rose_raster) + cast_(size_t, data_size);

    if(object_size < data_size) {
        return NULL;
    }

    // Allocate and initialize a new raster object.
    struct rose_raster* raster = malloc(object_size);
    if(raster == NULL) {
        return NULL;
    } else {
        // Zero-initialize the raster.
        *raster = (struct rose_raster){};

        // Initialize raster's buffer interface.
        wlr_buffer_init(
            &(raster->base), &rose_raster_buffer_implementation, width, height);
    }

    // Clear the pixels.
    rose_raster_clear(raster);

    // Return initialized raster object.
    return raster;
}

void
rose_raster_destroy(struct rose_raster* raster) {
    if(raster != NULL) {
        // Destroy raster's texture, if any.
        if(raster->texture != NULL) {
            raster->texture = (wlr_texture_destroy(raster->texture), NULL);
        }

        // Drop the buffer.
        wlr_buffer_drop(&(raster->base));
    }
}

////////////////////////////////////////////////////////////////////////////////
// Pixel data clearing interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_raster_clear(struct rose_raster* raster) {
    // Compute pixel data size.
    size_t data_size = //
        cast_(size_t, raster->base.width) * cast_(size_t, raster->base.height) *
        4U;

    // Clear pixel data.
    memset(raster->pixels, 0, data_size);
}

////////////////////////////////////////////////////////////////////////////////
// Texture updating interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_raster_texture_update( //
    struct rose_raster* raster, pixman_region32_t* region) {
    // Update the texture, if any.
    if(raster->texture != NULL) {
        wlr_texture_update_from_buffer(
            raster->texture, &(raster->base), region);
    }
}
