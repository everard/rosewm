// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_D93B01AD136D4EB7BD50F1B2C3D69386
#define H_D93B01AD136D4EB7BD50F1B2C3D69386

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_server_context;
struct wlr_drag;

////////////////////////////////////////////////////////////////////////////////
// Action interface.
////////////////////////////////////////////////////////////////////////////////

// Note: Might immediately cancel if there is not enough memory.
void
rose_drag_and_drop_start(
    struct rose_server_context* context, struct wlr_drag* drag);

#endif // H_D93B01AD136D4EB7BD50F1B2C3D69386
