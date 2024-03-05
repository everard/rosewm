// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_455AE4730B6B453BAAEF6327458B9C48
#define H_455AE4730B6B453BAAEF6327458B9C48

#include "rendering_color_scheme.h"
#include "unicode.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_memory;

////////////////////////////////////////////////////////////////////////////////
// Text rendering context declaration.
// Note: Pointer to this type shall be used as an opaque handle.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context;

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization parameters definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context_parameters {
    // A pointer to an array of font data.
    struct rose_memory* fonts;

    // Size of the array.
    size_t font_count;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering parameters definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_parameters {
    int font_size, dpi, max_width;
    struct rose_color color;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering extent definition. This type is used as a return type of
// rendering operations.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_extent {
    int width, height;
};

////////////////////////////////////////////////////////////////////////////////
// Pixel buffer definition. This type is used as a target for rendering.
////////////////////////////////////////////////////////////////////////////////

struct rose_pixel_buffer {
    // A pointer to the region of memory containing pixel data in 8 bit per
    // channel RGBA format.
    unsigned char* data;

    // Width, height, and pitch (pitch is the byte size of the line of pixels).
    int width, height, pitch;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

// Note: This function takes ownership of the fonts which have been passed in
// initialization parameters. If this function fails, then font data shall be
// freed.
struct rose_text_rendering_context*
rose_text_rendering_context_initialize(
    struct rose_text_rendering_context_parameters parameters);

void
rose_text_rendering_context_destroy(
    struct rose_text_rendering_context* context);

////////////////////////////////////////////////////////////////////////////////
// Text rendering interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_extent
rose_compute_string_extent( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters parameters,
    struct rose_utf32_string string);

struct rose_text_rendering_extent
rose_render_string( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters parameters,
    struct rose_utf32_string string, //
    struct rose_pixel_buffer pixel_buffer);

#endif // H_455AE4730B6B453BAAEF6327458B9C48
