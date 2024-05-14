// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "rendering_theme.h"

#include <stddef.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

////////////////////////////////////////////////////////////////////////////////
// Initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_theme
rose_theme_initialize_default() {
    return (struct rose_theme){
        .font_size = 16,
        .panel =
            {.position = rose_ui_panel_position_top,
             .size = 40,
             .is_visible = true},
        .color_scheme = rose_color_scheme_initialize_default()};
}

bool
rose_theme_initialize(char const* file_path, struct rose_theme* result) {
    // Open the file.
    FILE* file = fopen(file_path, "rb");
    if(file == NULL) {
        return false;
    }

    // Initialize an empty theme.
    struct rose_theme theme = {.panel = {.is_visible = true}};

    // Read font size.
    theme.font_size = fgetc(file);
    theme.font_size = clamp_(theme.font_size, 1, 144);

    // Read panel's position.
    switch(theme.panel.position = fgetc(file)) {
        case rose_ui_panel_position_top:
            // fall-through
        case rose_ui_panel_position_bottom:
            // fall-through
        case rose_ui_panel_position_left:
            // fall-through
        case rose_ui_panel_position_right:
            break;

        default:
            return (fclose(file), false);
    }

    // Read panel's size.
    theme.panel.size = fgetc(file);
    theme.panel.size = clamp_(theme.panel.size, 1, 128);

#define read_color_(c)                                  \
    if(fread(c.rgba8, sizeof(c.rgba8), 1, file) != 1) { \
        return (fclose(file), false);                   \
    }                                                   \
                                                        \
    for(ptrdiff_t i = 0; i != 4; ++i) {                 \
        c.rgba32[i] = (float)(c.rgba8[i] / 255.0);      \
    }

    // Read color scheme.
    read_color_(theme.color_scheme.panel_background);
    read_color_(theme.color_scheme.panel_foreground);
    read_color_(theme.color_scheme.panel_highlight);
    read_color_(theme.color_scheme.menu_background);
    read_color_(theme.color_scheme.menu_foreground);
    read_color_(theme.color_scheme.menu_highlight0);
    read_color_(theme.color_scheme.menu_highlight1);
    read_color_(theme.color_scheme.surface_background0);
    read_color_(theme.color_scheme.surface_background1);
    read_color_(theme.color_scheme.surface_resizing_background0);
    read_color_(theme.color_scheme.surface_resizing_background1);
    read_color_(theme.color_scheme.surface_resizing);
    read_color_(theme.color_scheme.workspace_background);

#undef read_color_

    // Close the file, write the theme: initialization succeeded.
    return fclose(file), (*result = theme), true;
}
