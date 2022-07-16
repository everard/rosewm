// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_7C346532827B4C42BA078CA25929AE6C
#define H_7C346532827B4C42BA078CA25929AE6C

#include "unicode.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_output;

////////////////////////////////////////////////////////////////////////////////
// Rendering-related definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_color {
    float v[4];
};

struct rose_pixel_buffer {
    unsigned char* data;
    int w, h;
};

struct rose_color_scheme {
    struct rose_color panel_background;
    struct rose_color panel_foreground;
    struct rose_color panel_highlight;
    struct rose_color menu_background;
    struct rose_color menu_foreground;
    struct rose_color menu_highlight0;
    struct rose_color menu_highlight1;
    struct rose_color surface_background0;
    struct rose_color surface_background1;
    struct rose_color surface_resizing_background0;
    struct rose_color surface_resizing_background1;
    struct rose_color surface_resizing;
    struct rose_color workspace_background;
};

////////////////////////////////////////////////////////////////////////////////
// Text-rendering-related declarations and definitions.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context;

struct rose_text_rendering_context_parameters {
    char const** font_names;
    size_t n_fonts;
};

struct rose_text_rendering_parameters {
    int font_size, dpi, w_max;
    struct rose_color color;
};

struct rose_text_rendering_extents {
    int w, h;
};

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context*
rose_text_rendering_context_initialize(
    struct rose_text_rendering_context_parameters params);

void
rose_text_rendering_context_destroy(struct rose_text_rendering_context* ctx);

////////////////////////////////////////////////////////////////////////////////
// Text rendering interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_extents
rose_compute_string_extents( //
    struct rose_text_rendering_context* ctx,
    struct rose_text_rendering_parameters params,
    struct rose_utf32_string string);

void
rose_render_string( //
    struct rose_text_rendering_context* ctx,
    struct rose_text_rendering_parameters params,
    struct rose_utf32_string string, struct rose_pixel_buffer pixel_buffer);

////////////////////////////////////////////////////////////////////////////////
// Content rendering interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_render_content(struct rose_output* output);

#endif // H_7C346532827B4C42BA078CA25929AE6C
