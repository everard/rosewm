// Copyright Nezametdinov E. Ildus 2023.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_C48D90CC169942EFA59FF5625ABC1C95
#define H_C48D90CC169942EFA59FF5625ABC1C95

#include <stdbool.h>

#include "rendering_color_scheme.h"
#include "ui_panel.h"

////////////////////////////////////////////////////////////////////////////////
// Theme definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_theme {
    int font_size;
    struct rose_ui_panel panel;
    struct rose_color_scheme color_scheme;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_theme
rose_theme_initialize_default();

// Note: If initialization fails, then resulting theme is not modified.
bool
rose_theme_initialize(char const* file_path, struct rose_theme* result);

#endif // H_C48D90CC169942EFA59FF5625ABC1C95
