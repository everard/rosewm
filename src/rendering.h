// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_7C346532827B4C42BA078CA25929AE6C
#define H_7C346532827B4C42BA078CA25929AE6C

#include "rendering_text.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_output;

////////////////////////////////////////////////////////////////////////////////
// Content rendering interface.
////////////////////////////////////////////////////////////////////////////////

// Renders the visible content (focused workspace) of the given output.
void
rose_render_content(struct rose_output* output);

#endif // H_7C346532827B4C42BA078CA25929AE6C
