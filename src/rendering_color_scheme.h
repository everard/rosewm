// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_DA36B4F63EDC4E18B6EF685C80263F4D
#define H_DA36B4F63EDC4E18B6EF685C80263F4D

////////////////////////////////////////////////////////////////////////////////
// Color definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_color {
    float v[4];
};

////////////////////////////////////////////////////////////////////////////////
// Color scheme definition.
////////////////////////////////////////////////////////////////////////////////

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
// Initialization interface.
////////////////////////////////////////////////////////////////////////////////

// Initializes default color scheme.
struct rose_color_scheme
rose_color_scheme_initialize_default();

#endif // H_DA36B4F63EDC4E18B6EF685C80263F4D
