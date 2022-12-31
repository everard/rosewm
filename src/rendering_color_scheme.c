// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "rendering_color_scheme.h"

////////////////////////////////////////////////////////////////////////////////
// Initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_color_scheme
rose_color_scheme_initialize_default() {
    // Initialize default color scheme.
    // Note: Configuration routines are defined in a separate module.
    return (struct rose_color_scheme){
        .panel_background = {{0.15f, 0.15f, 0.15f, 1.0f}},
        .panel_foreground = {{1.0f, 1.0f, 1.0f, 1.0f}},
        .panel_highlight = {{0.25f, 0.15f, 0.15f, 1.0f}},
        .menu_background = {{0.13f, 0.13f, 0.13f, 1.0f}},
        .menu_foreground = {{1.0f, 1.0f, 1.0f, 1.0f}},
        .menu_highlight0 = {{0.23f, 0.1f, 0.1f, 1.0f}},
        .menu_highlight1 = {{0.33f, 0.1f, 0.1f, 1.0f}},
        .surface_background0 = {{0.8f, 0.8f, 0.8f, 1.0f}},
        .surface_background1 = {{0.6f, 0.6f, 0.6f, 1.0f}},
        .surface_resizing_background0 = {{0.8f, 0.8f, 0.8f, 0.5f}},
        .surface_resizing_background1 = {{0.6f, 0.6f, 0.6f, 0.5f}},
        .surface_resizing = {{0.1f, 0.1f, 0.1f, 0.5f}},
        .workspace_background = {{0.2f, 0.2f, 0.2f, 1.0f}}};
}
