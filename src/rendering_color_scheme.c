// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "rendering_color_scheme.h"
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// Initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_color_scheme
rose_color_scheme_initialize_default() {
    // Initialize 8-bit RGBA colors.
    struct rose_color_scheme scheme = {
        .panel_background = {.rgba8 = {0x26, 0x26, 0x26, 0xFF}},
        .panel_foreground = {.rgba8 = {0xFF, 0xFF, 0xFF, 0xFF}},
        .panel_highlight = {.rgba8 = {0x40, 0x26, 0x26, 0xFF}},
        .menu_background = {.rgba8 = {0x21, 0x21, 0x21, 0xFF}},
        .menu_foreground = {.rgba8 = {0xFF, 0xFF, 0xFF, 0xFF}},
        .menu_highlight0 = {.rgba8 = {0x3B, 0x1E, 0x1E, 0xFF}},
        .menu_highlight1 = {.rgba8 = {0x54, 0x1E, 0x1E, 0xFF}},
        .surface_background0 = {.rgba8 = {0xCC, 0xCC, 0xCC, 0xFF}},
        .surface_background1 = {.rgba8 = {0x99, 0x99, 0x99, 0xFF}},
        .surface_resizing_background0 = {.rgba8 = {0xCC, 0xCC, 0xCC, 0x80}},
        .surface_resizing_background1 = {.rgba8 = {0x99, 0x99, 0x99, 0x80}},
        .surface_resizing = {.rgba8 = {0x1E, 0x1E, 0x1E, 0x80}},
        .workspace_background = {.rgba8 = {0x33, 0x33, 0x33, 0xFF}}};

#define convert_(x)                                              \
    for(ptrdiff_t i = 0; i != 4; ++i) {                          \
        scheme.x.rgba32[i] = (float)(scheme.x.rgba8[i] / 255.0); \
    }

    // Initialize 32-bit RGBA colors.
    convert_(panel_background);
    convert_(panel_foreground);
    convert_(panel_highlight);
    convert_(menu_background);
    convert_(menu_foreground);
    convert_(menu_highlight0);
    convert_(menu_highlight1);
    convert_(surface_background0);
    convert_(surface_background1);
    convert_(surface_resizing_background0);
    convert_(surface_resizing_background1);
    convert_(surface_resizing);
    convert_(workspace_background);

#undef convert_

    // Return initialized color scheme.
    return scheme;
}
