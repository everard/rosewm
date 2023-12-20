// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_13A2516DF1AF47968C7A0CF09882D5CD
#define H_13A2516DF1AF47968C7A0CF09882D5CD

#include <wlr/types/wlr_buffer.h>

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct wlr_renderer;
struct wlr_texture;

////////////////////////////////////////////////////////////////////////////////
// Raster definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_raster {
    // Note: Raster implements buffer's interface.
    struct wlr_buffer base;

    // A texture which can be used in rendering.
    struct wlr_texture* texture;

    // Pixel data.
    unsigned char pixels[];
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

// Note: This function initializes raster's texture.
struct rose_raster*
rose_raster_initialize(struct wlr_renderer* renderer, int w, int h);

// Note: This function does not initialize raster's texture.
struct rose_raster*
rose_raster_initialize_without_texture(int w, int h);

// Note: This function destroys raster's texture immediately, but might postpone
// memory freeing (in case the underlying buffer is locked).
void
rose_raster_destroy(struct rose_raster* raster);

////////////////////////////////////////////////////////////////////////////////
// Pixel data clearing interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_raster_clear(struct rose_raster* raster);

////////////////////////////////////////////////////////////////////////////////
// Texture updating interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_raster_texture_update( //
    struct rose_raster* raster, pixman_region32_t* region);

#endif // H_13A2516DF1AF47968C7A0CF09882D5CD
