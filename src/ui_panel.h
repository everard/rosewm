// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_C4C9CC86D05C4005B52AF4D257B53263
#define H_C4C9CC86D05C4005B52AF4D257B53263

////////////////////////////////////////////////////////////////////////////////
// Panel definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_ui_panel_position {
    // Layout: horizontal.
    rose_ui_panel_position_top,
    // Layout: horizontal.
    rose_ui_panel_position_bottom,
    // Layout: vertical.
    rose_ui_panel_position_left,
    // Layout: vertical.
    rose_ui_panel_position_right
};

struct rose_ui_panel {
    // Panel's position which determines panel's layout.
    enum rose_ui_panel_position position;

    // Panel's size: either its width, or height, depending on panel's position.
    int size;

    // Panel's visibility flag.
    bool is_visible;
};

#endif // H_C4C9CC86D05C4005B52AF4D257B53263
