// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_455AE4730B6B453BAAEF6327458B9C48
#define H_455AE4730B6B453BAAEF6327458B9C48

#include "rendering_color_scheme.h"
#include "unicode.h"

////////////////////////////////////////////////////////////////////////////////
// Text rendering context declaration.
// Note: Pointer to this type shall be used as an opaque handle.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context;

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization-related definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context_parameters {
    char const** font_names;
    size_t n_fonts;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering-related definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_parameters {
    int font_size, dpi, w_max;
    struct rose_color color;
};

struct rose_text_rendering_extents {
    int w, h;
};

struct rose_pixel_buffer {
    unsigned char* data;
    int w, h, pitch;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context*
rose_text_rendering_context_initialize(
    struct rose_text_rendering_context_parameters params);

void
rose_text_rendering_context_destroy(
    struct rose_text_rendering_context* context);

////////////////////////////////////////////////////////////////////////////////
// Text rendering interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_extents
rose_compute_string_extents( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters params,
    struct rose_utf32_string string);

struct rose_text_rendering_extents
rose_render_string( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters params,
    struct rose_utf32_string string, //
    struct rose_pixel_buffer pixel_buffer);

#endif // H_455AE4730B6B453BAAEF6327458B9C48
