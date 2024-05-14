// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "ui_menu.h"
#include "server_context.h"

#include <wlr/types/wlr_xdg_shell.h>
#include <linux/input-event-codes.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))

#define abs_(x) (((x) < 0) ? -(x) : (x))
#define clamp_(x, a, b) max_((a), min_((x), (b)))

////////////////////////////////////////////////////////////////////////////////
// Line type-casting utility functions.
////////////////////////////////////////////////////////////////////////////////

static struct rose_ui_menu_line
rose_ui_menu_line_type_upcast(struct rose_ui_menu_line line) {
    // Initialize an empty line of the resulting type.
    struct rose_ui_menu_line result = {
        .type = min_(line.type + 1, rose_ui_menu_line_type_output)};

    // Up-cast the line depending on its type.
    switch(line.type) {
        case rose_ui_menu_line_type_surface: {
            struct rose_surface* surface = line.data;
            if(surface != NULL) {
                result.data = surface->workspace;
            }

            break;
        }

        case rose_ui_menu_line_type_workspace: {
            struct rose_workspace* workspace = line.data;
            if(workspace != NULL) {
                result.data = workspace->output;
            }

            break;
        }

        case rose_ui_menu_line_type_output:
            result.data = line.data;
            break;

        default:
            break;
    }

    return result;
}

static struct rose_ui_menu_line
rose_ui_menu_line_type_cast(
    struct rose_ui_menu_line line, enum rose_ui_menu_line_type type) {
    // Do nothing if the given line's type matches the requested type.
    if(line.type == type) {
        return line;
    }

    // Up-cast the line until it matches the requested type.
    for(int i = 0, diff = type - line.type; i < diff; ++i) {
        line = rose_ui_menu_line_type_upcast(line);
    }

    // Return the resulting line.
    return line;
}

////////////////////////////////////////////////////////////////////////////////
// Line comparison utility functions.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_ui_menu_line_is_empty(struct rose_ui_menu_line line) {
    return (line.data == NULL);
}

static bool
rose_ui_menu_line_is_equal(
    struct rose_ui_menu_line x, struct rose_ui_menu_line y) {
    return ((x.type == y.type) && (x.data == y.data));
}

static bool
rose_ui_menu_line_is_included(
    struct rose_ui_menu_line x, struct rose_ui_menu_line s) {
    // If any of the given lines is empty, then the first line can not belong to
    // the second line.
    if(rose_ui_menu_line_is_empty(x) || rose_ui_menu_line_is_empty(s)) {
        return false;
    }

    return rose_ui_menu_line_is_equal(
        rose_ui_menu_line_type_cast(x, s.type), s);
}

static bool
rose_ui_menu_line_is_skipped(
    struct rose_ui_menu_line line, struct rose_ui_menu_line skip) {
    if(rose_ui_menu_line_is_included(line, skip)) {
        return true;
    }

    if(line.type == rose_ui_menu_line_type_surface) {
        struct rose_surface* surface = line.data;
        if(surface != NULL) {
            return !(surface->is_mapped);
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
// Line selection utility functions and types.
////////////////////////////////////////////////////////////////////////////////

enum rose_ui_menu_line_selection_direction {
    rose_ui_menu_line_selection_direction_backward,
    rose_ui_menu_line_selection_direction_forward
};

static struct rose_ui_menu_line
rose_ui_menu_line_select_next(
    struct rose_ui_menu_line line,
    enum rose_ui_menu_line_selection_direction direction) {
    // If the given line is empty, then there is nothing to select.
    if(rose_ui_menu_line_is_empty(line)) {
        return line;
    }

    // Select the next line in the given direction depending on line's type.
    switch(line.type) {
        case rose_ui_menu_line_type_surface: {
            struct rose_surface* surface = line.data;

#define select_(d)                                         \
    ((surface->link.d != &(surface->workspace->surfaces))  \
         ? wl_container_of(surface->link.d, surface, link) \
         : NULL)

            if(direction == rose_ui_menu_line_selection_direction_backward) {
                line.data = select_(next);
            } else {
                line.data = select_(prev);
            }

#undef select_

            break;
        }

        case rose_ui_menu_line_type_workspace: {
            struct rose_workspace* workspace = line.data;
            if(workspace->output == NULL) {
                break;
            }

#define select_(d)                                                           \
    ((workspace->link_output.d != &(workspace->output->workspaces))          \
         ? wl_container_of(workspace->link_output.d, workspace, link_output) \
         : NULL)

            if(direction == rose_ui_menu_line_selection_direction_backward) {
                line.data = select_(next);
            } else {
                line.data = select_(prev);
            }

#undef select_

            break;
        }

        case rose_ui_menu_line_type_output: {
            struct rose_output* output = line.data;

#define select_(d)                                       \
    ((output->link.d != &(output->context->outputs))     \
         ? wl_container_of(output->link.d, output, link) \
         : NULL)

            if(direction == rose_ui_menu_line_selection_direction_backward) {
                line.data = select_(next);
            } else {
                line.data = select_(prev);
            }

#undef select_

            break;
        }

        default:
            break;
    }

    return line;
}

static struct rose_ui_menu_line
rose_ui_menu_line_select(
    struct rose_ui_menu_line line, struct rose_ui_menu_line skip, int delta) {
    // If the given line is empty, then there is nothing to select.
    if(rose_ui_menu_line_is_empty(line)) {
        return line;
    }

    // Compute selection direction based on the given delta.
    enum rose_ui_menu_line_selection_direction direction =
        (delta < 0 ? rose_ui_menu_line_selection_direction_backward
                   : rose_ui_menu_line_selection_direction_forward);

    // Select the next line in the given direction.
    struct rose_ui_menu_line line_next = line;
    for(int i = 0; i < abs_(delta);) {
        // Select the next line.
        line_next = rose_ui_menu_line_select_next(line_next, direction);

        // If selected line is empty, then break out of the cycle.
        if(rose_ui_menu_line_is_empty(line_next)) {
            if(rose_ui_menu_line_is_skipped(line, skip)) {
                line = line_next;
            }

            break;
        }

        // Skip selected line, if needed.
        if(rose_ui_menu_line_is_skipped(line_next, skip)) {
            continue;
        }

        // Save selected line and increment the counter.
        line = line_next;
        i++;
    }

    return line;
}

////////////////////////////////////////////////////////////////////////////////
// Line movement utility function.
////////////////////////////////////////////////////////////////////////////////

static void
rose_ui_menu_line_move(
    struct rose_ui_menu_line line, struct rose_ui_menu_line destination) {
    // If either the source line, or its destination, is not specified, then do
    // nothing.
    if(rose_ui_menu_line_is_empty(line) ||
       rose_ui_menu_line_is_empty(destination)) {
        return;
    }

    // Move the source line depending on its type.
    switch(line.type) {
        case rose_ui_menu_line_type_surface: {
            // Obtain the surface.
            struct rose_surface* surface = line.data;

            // Move the surface depending on the type of its destination.
            switch(destination.type) {
                case rose_ui_menu_line_type_surface:
                    if(surface->workspace != NULL) {
                        rose_workspace_reposition_surface(
                            surface->workspace, surface, destination.data);
                    }

                    break;

                case rose_ui_menu_line_type_workspace:
                    rose_workspace_add_surface(destination.data, surface);
                    break;

                case rose_ui_menu_line_type_output: {
                    struct rose_output* output = destination.data;

                    // Move the surface to output's focused workspace, if any.
                    if(output->focused_workspace != NULL) {
                        rose_workspace_add_surface(
                            output->focused_workspace, surface);
                    }

                    break;
                }

                default:
                    break;
            }

            break;
        }

        case rose_ui_menu_line_type_workspace: {
            // Obtain the workspace.
            struct rose_workspace* workspace = line.data;

            // A workspace can only be moved to an output.
            if(destination.type == rose_ui_menu_line_type_output) {
                // Do nothing if workspace's output has no other workspaces.
                if((workspace->output != NULL) &&
                   (workspace->output->workspaces.prev ==
                    &(workspace->link_output)) &&
                   (workspace->output->workspaces.next ==
                    &(workspace->link_output))) {
                    break;
                }

                // Move the workspace.
                rose_output_add_workspace(destination.data, workspace);
            }

            break;
        }

        case rose_ui_menu_line_type_output:
            // Note: An output can not be moved.
            break;

        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Layout computation utility function.
////////////////////////////////////////////////////////////////////////////////

static void
rose_ui_menu_layout_compute(struct rose_ui_menu* menu) {
    // Precondition: The menu is active.

    // Compute menu's area.
    if(true) {
        // Obtain output's focused workspace.
        struct rose_workspace* workspace = menu->output->focused_workspace;

        // Obtain the panel.
        struct rose_ui_panel panel = workspace->panel;
        if(panel.is_visible) {
            if((workspace->focused_surface != NULL) &&
               (workspace->focused_surface->state.pending.is_fullscreen)) {
                panel.is_visible = false;
            }
        }

        // Compute panel's influence.
        int d = (panel.is_visible ? panel.size : 0);

        // Compute menu's area depending on panel's position.
        menu->area.x = menu->area.y = 0;
        menu->area.width = workspace->width / 2;
        menu->area.height = workspace->height;

        switch(panel.position) {
            case rose_ui_panel_position_top:
                menu->area.y += d;
                // fall-through
            case rose_ui_panel_position_bottom:
                menu->area.height -= d;
                break;

            case rose_ui_panel_position_left:
                menu->area.x += d;
                menu->area.width -= d;

                break;

            case rose_ui_panel_position_right:
                menu->area.x += menu->area.width;
                menu->area.width -= d;

                break;

            default:
                break;
        }
    }

    // Compute line's height.
    if(true) {
        // Obtain output's state.
        struct rose_output_state output_state =
            rose_output_state_obtain(menu->output);

        // Prepare text-rendering-related data.
        struct rose_text_rendering_context* text_rendering_context =
            menu->output->context->text_rendering_context;

        struct rose_text_rendering_parameters text_rendering_parameters = {
            .font_size = menu->output->context->config.theme.font_size,
            .dpi = output_state.dpi};

        // Compute line's height. Use "M" string as a reference.
        struct rose_utf32_string string = {.data = U"M", .size = 1};
        menu->layout.line_height =
            (int)((2.0 * rose_compute_string_extent(
                             text_rendering_context, text_rendering_parameters,
                             string)
                             .height) /
                      output_state.scale +
                  0.5);

        // Make sure line's height is positive.
        menu->layout.line_height = max_(2, menu->layout.line_height);
    }

    // Compute maximum number of lines in the menu.
    menu->layout.line_max_count = menu->area.height / menu->layout.line_height;
    menu->layout.line_max_count =
        clamp_(menu->layout.line_max_count, 1, rose_ui_menu_line_max_count);

    // Compute menu's margins.
    menu->layout.margin_x = 1;
    menu->layout.margin_y = (menu->area.height - (menu->layout.line_max_count *
                                                  menu->layout.line_height)) /
                            2;

    // Mark menu's layout as updated.
    menu->is_layout_updated = true;
}

////////////////////////////////////////////////////////////////////////////////
// State manipulation utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_ui_menu_refresh(struct rose_ui_menu* menu, struct rose_ui_menu_line skip) {
    // Precondition: The menu is active.

    // Clear the page.
    menu->page =
        (struct rose_ui_menu_page){.mark_index = menu->page.mark_index};

    // Initialize an empty line for menu's mark.
    struct rose_ui_menu_line mark = {.type = menu->line_type};

    // Set menu's head, if needed.
    if(rose_ui_menu_line_is_empty(menu->head)) {
        // Obtain the relevant objects.
        struct rose_workspace* workspace = menu->output->focused_workspace;
        struct rose_surface* surface =
            ((workspace->focused_surface != NULL)
                 ? workspace->focused_surface
                 : (wl_list_empty(&(workspace->surfaces_mapped))
                        ? NULL
                        : wl_container_of(
                              workspace->surfaces_mapped.next, surface,
                              link_mapped)));

        // Initialize the mark depending on its type.
        switch(mark.type) {
            case rose_ui_menu_line_type_surface:
                mark.data = surface;
                break;

            case rose_ui_menu_line_type_workspace:
                mark.data = workspace;
                break;

            case rose_ui_menu_line_type_output:
                mark.data = menu->output;
                break;

            default:
                break;
        }

        // Set menu's head.
        menu->head = rose_ui_menu_line_select(
            mark, skip, -menu->layout.line_max_count / 2);
    }

    // Adjust menu's head.
    if(true) {
        // Search for acceptable head.
        int const search_directions[] = {-1, +1};
        for(ptrdiff_t i = 0; i != 2; ++i) {
            // If the head is acceptable, then break out of the cycle.
            if(!rose_ui_menu_line_is_skipped(menu->head, skip)) {
                break;
            }

            // Select a line in the given direction.
            struct rose_ui_menu_line line = rose_ui_menu_line_select(
                menu->head, skip, search_directions[i]);

            // If the line is not empty, then adjust menu's head.
            if(!rose_ui_menu_line_is_empty(line)) {
                menu->head = line;
            }
        }

        // If the head is still not acceptable, then clear it.
        if(rose_ui_menu_line_is_skipped(menu->head, skip)) {
            menu->head = (struct rose_ui_menu_line){.type = menu->line_type};
        }
    }

    // Populate menu's page.
    if(true) {
        struct rose_ui_menu_line line = menu->head, line_prev = line;
        for(int i = 0; i != menu->layout.line_max_count; ++i) {
            // If current line is empty, then stop.
            if(rose_ui_menu_line_is_empty(line)) {
                break;
            }

            // Add current line to the page.
            menu->page.lines[menu->page.line_count++] = line_prev = line;

            // Select the next line.
            line = rose_ui_menu_line_select(line, skip, 1);

            // If the next line equals to the previous line, then stop.
            if(rose_ui_menu_line_is_equal(line, line_prev)) {
                break;
            }
        }
    }

    // Update menu's selection index, if needed.
    if(!rose_ui_menu_line_is_empty(menu->selection)) {
        // Find selection on the page.
        menu->page.selection_index = -1;
        for(ptrdiff_t i = 0; i != menu->page.line_count; ++i) {
            // If selection is visible on the page, then save its index, and
            // break out of the cycle.
            if(rose_ui_menu_line_is_equal(
                   menu->selection, menu->page.lines[i])) {
                menu->page.selection_index = i;
                break;
            }
        }
    }

    // Update mark's index.
    if(!rose_ui_menu_line_is_empty(mark)) {
        menu->page.mark_index = 0;
        for(ptrdiff_t i = 0; i != menu->page.line_count; ++i) {
            if(rose_ui_menu_line_is_equal(mark, menu->page.lines[i])) {
                menu->page.mark_index = i;
                break;
            }
        }
    } else {
        menu->page.mark_index =
            ((menu->page.line_count != 0)
                 ? min_(menu->page.mark_index, menu->page.line_count - 1)
                 : 0);
    }
}

static void
rose_ui_menu_set_line_type(
    struct rose_ui_menu* menu, enum rose_ui_menu_line_type type) {
    // Precondition: The menu is active.

    // Do nothing if the menu doesn't change its line type.
    if(menu->line_type == type) {
        return;
    }

    // Set the line type and clear menu's head.
    menu->head.type = menu->line_type = type;
    menu->head.data = NULL;

    // Refresh the menu.
    rose_ui_menu_refresh(menu, menu->head);

    // Request focused workspace's redraw.
    rose_workspace_request_redraw(menu->output->focused_workspace);
}

////////////////////////////////////////////////////////////////////////////////
// State checking utility function.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_ui_menu_is_active(struct rose_ui_menu* menu) {
    if((menu == NULL) || !(menu->is_visible)) {
        return false;
    }

    if(menu->output->focused_workspace == NULL) {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_initialize(struct rose_ui_menu* menu, struct rose_output* output) {
    // Initialize the object.
    *menu = (struct rose_ui_menu){.output = output};

    // Initialize list link.
    wl_list_init(&(menu->link));
}

void
rose_ui_menu_destroy(struct rose_ui_menu* menu) {
    rose_ui_menu_hide(menu);
}

////////////////////////////////////////////////////////////////////////////////
// State manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_show(
    struct rose_ui_menu* menu, enum rose_ui_menu_line_type line_type) {
    // Do nothing if there is no menu specified, or the specified menu is
    // already visible.
    if((menu == NULL) || (menu->is_visible)) {
        return;
    }

    // Do nothing if menu's output has no focused workspace.
    if(menu->output->focused_workspace == NULL) {
        return;
    }

    // Add the menu to the list of visible menus.
    wl_list_remove(&(menu->link));
    wl_list_insert(&(menu->output->context->menus_visible), &(menu->link));

    // Set menu's flags.
    menu->is_visible = true;
    menu->is_updated = true;

    // Set menu's line type.
    menu->line_type = line_type;

    // Clear the menu.
    menu->head = menu->selection = (struct rose_ui_menu_line){};
    menu->page = (struct rose_ui_menu_page){};

    // Compute menu's layout.
    rose_ui_menu_layout_compute(menu);

    // Refresh the menu.
    rose_ui_menu_refresh(menu, menu->head);

    // Request focused workspace's redraw.
    rose_workspace_request_redraw(menu->output->focused_workspace);
}

void
rose_ui_menu_hide(struct rose_ui_menu* menu) {
    // Do nothing if there is no menu specified, or the specified menu is not
    // visible.
    if((menu == NULL) || !(menu->is_visible)) {
        return;
    }

    // Remove the menu from the list of visible menus.
    wl_list_remove(&(menu->link));
    wl_list_init(&(menu->link));

    // Clear menu's visibility flag.
    menu->is_visible = false;

    // Clear the menu.
    menu->head = menu->selection = (struct rose_ui_menu_line){};
    menu->page = (struct rose_ui_menu_page){};

    // If menu's output has focused workspace, then request its redraw.
    if(menu->output->focused_workspace != NULL) {
        rose_workspace_request_redraw(menu->output->focused_workspace);
    }
}

void
rose_ui_menu_toggle(struct rose_ui_menu* menu) {
    // Do nothing if there is no menu specified.
    if(menu == NULL) {
        return;
    }

    // Show or hide the menu depending on its current state.
    if(menu->is_visible) {
        rose_ui_menu_hide(menu);
    } else {
        rose_ui_menu_show(menu, rose_ui_menu_line_type_surface);
    }
}

void
rose_ui_menu_update(struct rose_ui_menu* menu) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // Mark the menu as updated.
    menu->is_updated = true;

    // Compute menu's layout.
    rose_ui_menu_layout_compute(menu);

    // Refresh the menu.
    if(true) {
        struct rose_ui_menu_line skip = {};
        rose_ui_menu_refresh(menu, skip);
    }

    // Request focused workspace's redraw.
    rose_workspace_request_redraw(menu->output->focused_workspace);
}

////////////////////////////////////////////////////////////////////////////////
// Action interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_switch_line_type(struct rose_ui_menu* menu) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // A flag which shows that there is a workspace selected.
    bool is_workspace_selected =
        (menu->selection.type == rose_ui_menu_line_type_workspace) &&
        (menu->selection.data != NULL);

    // Select target line type.
    enum rose_ui_menu_line_type line_type =
        (is_workspace_selected
             ? rose_ui_menu_line_type_output
             : ((menu->line_type + 1) % rose_ui_menu_line_type_count_));

    // Mark the menu as updated.
    menu->is_updated = true;

    // Set selected line type.
    rose_ui_menu_set_line_type(menu, line_type);
}

void
rose_ui_menu_move_head(struct rose_ui_menu* menu, int direction) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // If direction vector is zero, then menu's update is not needed.
    if(direction == 0) {
        return;
    }

    // Sanitize direction vector.
    direction = clamp_(
        direction, -menu->layout.line_max_count, menu->layout.line_max_count);

    // Mark the menu as updated.
    menu->is_updated = true;

    // Move menu's head and refresh the menu.
    if(true) {
        struct rose_ui_menu_line skip = {};

        // Select a new head.
        menu->head = rose_ui_menu_line_select(menu->head, skip, direction);

        // Refresh the menu.
        rose_ui_menu_refresh(menu, skip);
    }

    // Request focused workspace's redraw.
    rose_workspace_request_redraw(menu->output->focused_workspace);
}

void
rose_ui_menu_move_mark(struct rose_ui_menu* menu, int direction) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // If menu's page is empty, or direction vector is zero, then update is not
    // needed.
    if((menu->page.line_count == 0) || (direction == 0)) {
        return;
    }

    // Mark the menu as updated.
    menu->is_updated = true;

    // Update mark's index.
    menu->page.mark_index += clamp_(
        direction, -menu->layout.line_max_count, menu->layout.line_max_count);

    // Compute head's movement direction.
    if(menu->page.mark_index < 0) {
        direction = menu->page.mark_index;
    } else if(
        (menu->page.line_count <= menu->page.mark_index) &&
        (menu->page.line_count == menu->layout.line_max_count)) {
        direction = menu->page.mark_index - menu->page.line_count + 1;
    } else {
        direction = 0;
    }

    // Clamp mark's index.
    menu->page.mark_index =
        clamp_(menu->page.mark_index, 0, menu->page.line_count - 1);

    // Move the head.
    rose_ui_menu_move_head(menu, direction);

    // Request focused workspace's redraw.
    rose_workspace_request_redraw(menu->output->focused_workspace);
}

void
rose_ui_menu_perform_action(
    struct rose_ui_menu* menu, enum rose_ui_menu_action_type type) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // Perform the action.
    switch(type) {
        case rose_ui_menu_action_cancel:
            // Cancel current operation.
            if(!rose_ui_menu_line_is_empty(menu->selection)) {
                // If there is a line selected, then restore menu's line type.
                rose_ui_menu_set_line_type(menu, menu->selection.type);

                // And clear menu's selection.
                menu->selection = (struct rose_ui_menu_line){};
            } else {
                // Otherwise, hide the menu.
                rose_ui_menu_hide(menu);
            }

            break;

        case rose_ui_menu_action_commit: {
            // Obtain the marked line.
            struct rose_ui_menu_line line = {};
            if(menu->page.mark_index < menu->page.line_count) {
                line = menu->page.lines[menu->page.mark_index];
            }

            // If the marked line is empty, then hide the menu, and do nothing
            // else.
            if(rose_ui_menu_line_is_empty(line)) {
                rose_ui_menu_hide(menu);
                return;
            }

            // Commit current operation.
            if(rose_ui_menu_line_is_empty(menu->selection)) {
                // If menu's selection is empty, then focus marked line.
                bool must_hide_menu = true;
                switch(line.type) {
                    case rose_ui_menu_line_type_surface: {
                        struct rose_surface* surface = line.data;
                        struct rose_workspace* workspace = surface->workspace;

                        if(workspace != NULL) {
                            rose_workspace_focus_surface(workspace, surface);
                        }

                        break;
                    }

                    case rose_ui_menu_line_type_workspace: {
                        must_hide_menu = false;

                        struct rose_workspace* workspace = line.data;
                        struct rose_output* output = workspace->output;

                        if(output != NULL) {
                            rose_output_focus_workspace(output, workspace);
                        }

                        break;
                    }

                    case rose_ui_menu_line_type_output: {
                        struct rose_output* output = line.data;
                        if(output->focused_workspace != NULL) {
                            rose_workspace_make_current(
                                output->focused_workspace);
                        }

                        break;
                    }

                    default:
                        break;
                }

                // And hide the menu, if needed.
                if(must_hide_menu) {
                    rose_ui_menu_hide(menu);
                }
            } else {
                // Otherwise, obtain selected object.
                struct rose_ui_menu_line selection = menu->selection;

                // Move it to its new location.
                rose_ui_menu_line_move(selection, line);

                // Restore line type.
                rose_ui_menu_set_line_type(menu, selection.type);

                // And clear the selection.
                menu->selection = (struct rose_ui_menu_line){};
            }

            break;
        }

        case rose_ui_menu_action_select:
            // Select marked line.
            if(menu->page.mark_index < menu->page.line_count) {
                menu->selection =
                    menu->page.lines
                        [menu->page.selection_index = menu->page.mark_index];
            } else {
                menu->selection = (struct rose_ui_menu_line){};
            }

            // If the line represents an output, then cancel selection.
            if(menu->selection.type == rose_ui_menu_line_type_output) {
                menu->selection = (struct rose_ui_menu_line){};
            }

            // If there is a selection, then adjust menu's line type.
            if(!rose_ui_menu_line_is_empty(menu->selection)) {
                // Note: Workspace can only be moved to an output.
                if(menu->selection.type == rose_ui_menu_line_type_workspace) {
                    rose_ui_menu_set_line_type(
                        menu, rose_ui_menu_line_type_output);
                }
            }

            break;
    }

    // Mark the menu as updated.
    menu->is_updated = true;

    // Request focused workspace's redraw.
    rose_workspace_request_redraw(menu->output->focused_workspace);
}

////////////////////////////////////////////////////////////////////////////////
// State query interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_ui_menu_has_selection(struct rose_ui_menu* menu) {
    // If the menu is not active, then it has no selection.
    return rose_ui_menu_is_active(menu) &&
           !rose_ui_menu_line_is_empty(menu->selection);
}

enum {
    rose_ui_menu_utf8_buffer_size = 2048,
    rose_ui_menu_utf8_string_size_max = rose_ui_menu_utf8_buffer_size - 1
};

struct rose_ui_menu_text
rose_ui_menu_text_obtain(struct rose_ui_menu* menu) {
    // Initialize an empty text.
    struct rose_ui_menu_text text = {};

    // If there is no menu specified, or the specified menu is not visible, then
    // return empty text.
    if((menu == NULL) || !(menu->is_visible)) {
        return text;
    }

    // Initialize string buffers.
    char line_buffer[rose_ui_menu_utf8_buffer_size] = {};
    char name_buffer[rose_ui_menu_utf8_buffer_size] = {};

    // Fill the lines from menu's page.
    for(ptrdiff_t i = 0; i != menu->page.line_count; ++i, ++text.line_count) {
        // Obtain the current line.
        struct rose_ui_menu_line line = menu->page.lines[i];

        // Obtain prefix string.
        char* prefix =
            ((rose_ui_menu_has_selection(menu) && (menu->page.mark_index == i))
                 ? "\xEF\x83\x9A"
                 : "");

#define write_name_                                                     \
    if(true) {                                                          \
        /* Limit name's size. */                                        \
        name.size = min_(name.size, rose_ui_menu_utf8_string_size_max); \
                                                                        \
        /* Ensure zero-termination. */                                  \
        name_buffer[name.size] = '\0';                                  \
                                                                        \
        /* Perform write operation. */                                  \
        memcpy(name_buffer, name.data, name.size);                      \
    }

        // Print the line depending on its type.
        switch(line.type) {
            case rose_ui_menu_line_type_surface: {
                struct rose_surface* surface = line.data;

                // Obtain surface's name.
                struct rose_utf8_string name = rose_convert_ntbs_to_utf8(
                    surface->xdg_surface->toplevel->title);

                // Write the name to the name buffer.
                write_name_;

                // Format the line.
                if(name.size != 0) {
                    char const* format = "%s\xEF\x89\x8D %s";
                    snprintf(
                        line_buffer, rose_ui_menu_utf8_string_size_max, format,
                        prefix, name_buffer);
                } else {
                    char const* format = "%s\xEF\x89\x8D ---";
                    snprintf(
                        line_buffer, rose_ui_menu_utf8_string_size_max, format,
                        prefix);
                }

                break;
            }

            case rose_ui_menu_line_type_workspace: {
                struct rose_workspace* workspace = line.data;

                // Obtain workspace's topmost surface.
                struct rose_surface* surface = workspace->focused_surface;
                if(surface == NULL) {
                    if(!wl_list_empty(&(workspace->surfaces_mapped))) {
                        surface = wl_container_of(
                            workspace->surfaces_mapped.next, surface,
                            link_mapped);
                    }
                }

                // Obtain surface's name.
                struct rose_utf8_string name = rose_convert_ntbs_to_utf8(
                    ((surface != NULL) ? surface->xdg_surface->toplevel->title
                                       : ""));

                // Write the name to the name buffer.
                write_name_;

                // Format the line.
                if(name.size != 0) {
                    char const* format = "%s\xEF\x81\x84 %02d %s";
                    snprintf(
                        line_buffer, rose_ui_menu_utf8_string_size_max, format,
                        prefix, workspace->id, name_buffer);
                } else {
                    char const* format = "%s\xEF\x81\x84 %02d ---";
                    snprintf(
                        line_buffer, rose_ui_menu_utf8_string_size_max, format,
                        prefix, workspace->id);
                }

                break;
            }

            case rose_ui_menu_line_type_output: {
                struct rose_output* output = line.data;

                // Obtain output's name.
                struct rose_utf8_string name =
                    rose_convert_ntbs_to_utf8(output->device->name);

                // Write the name to the name buffer.
                write_name_;

                // Format the line.
                if(name.size != 0) {
                    char const* format = "%s\xEF\x89\xAC %02d %s";
                    snprintf(
                        line_buffer, rose_ui_menu_utf8_string_size_max, format,
                        prefix, output->id, name_buffer);
                } else {
                    char const* format = "%s\xEF\x89\xAC %02d";
                    snprintf(
                        line_buffer, rose_ui_menu_utf8_string_size_max, format,
                        prefix, output->id);
                }

                break;
            }

            default:
                break;
        }

#undef write_name_

        // Ensure that line buffer is zero-terminated.
        line_buffer[rose_ui_menu_utf8_string_size_max] = '\0';

        // Write resulting line.
        text.lines[i] =
            rose_convert_utf8_to_utf32(rose_convert_ntbs_to_utf8(line_buffer));
    }

    // Return the text.
    return text;
}

////////////////////////////////////////////////////////////////////////////////
// Event notification interface implementation: pointer device.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_notify_pointer_axis(
    struct rose_ui_menu* menu, struct rose_pointer_event_axis event) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // Move menu's head depending on event's data.
    rose_ui_menu_move_head(menu, ((event.delta < 0.0) ? -3 : 3));

    // Notify the menu of pointer movement.
    rose_ui_menu_notify_pointer_warp(
        menu, event.time_msec, menu->pointer.x, menu->pointer.y);
}

void
rose_ui_menu_notify_pointer_button(
    struct rose_ui_menu* menu, struct rose_pointer_event_button event) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // Do nothing if button isn't pressed.
    if(event.state != rose_pointer_button_state_pressed) {
        return;
    }

    // Obtain pointer's coordinates.
    double x = menu->pointer.x;
    double y = menu->pointer.y;

    // Do nothing if the pointer is outside menu's area.
    if(((x < menu->area.x) || (x > (menu->area.x + menu->area.width))) ||
       ((y < menu->area.y) || (y > (menu->area.y + menu->area.height)))) {
        return;
    }

    // Compute highlighted line's index.
    int line_index =
        ((y - menu->area.y - menu->layout.margin_y) / menu->layout.line_height);

    // Perform additional actions depending on which button is pressed.
    if(event.button == BTN_LEFT) {
        // If left mouse button is pressed, and highlighted line is not empty,
        // then commit current operation.
        if((line_index >= 0) && (line_index < menu->page.line_count)) {
            // Set mark's index.
            menu->page.mark_index = line_index;

            // Commit current operation.
            rose_ui_menu_perform_action(menu, rose_ui_menu_action_commit);
        }
    } else if(event.button == BTN_RIGHT) {
        // If right mouse button is pressed, then perform selection operation.
        if(rose_ui_menu_line_is_empty(menu->selection)) {
            // If menu's selection is empty, then select highlighted line, if
            // any.
            if((line_index >= 0) && (line_index < menu->page.line_count)) {
                // Set mark's index.
                menu->page.mark_index = line_index;

                // Select marked line.
                rose_ui_menu_perform_action(menu, rose_ui_menu_action_select);
            }
        } else {
            // Otherwise, clear menu's selection by cancelling current
            // operation.
            rose_ui_menu_perform_action(menu, rose_ui_menu_action_cancel);
        }
    }
}

void
rose_ui_menu_notify_pointer_warp(
    struct rose_ui_menu* menu, uint32_t time_msec, double x, double y) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // Do nothing if the pointer is outside menu's area.
    if(((x < menu->area.x) || (x > (menu->area.x + menu->area.width))) ||
       ((y < menu->area.y) || (y > (menu->area.y + menu->area.height)))) {
        return;
    }

    // Save pointer's position and movement time.
    menu->pointer.x = x;
    menu->pointer.y = y;
    menu->pointer.movement_time_msec = time_msec;

    // Save mark's current index.
    int mark_index = menu->page.mark_index;

    // Compute mark's new index.
    menu->page.mark_index =
        ((y - menu->area.y - menu->layout.margin_y) / menu->layout.line_height);

    menu->page.mark_index =
        clamp_(menu->page.mark_index, 0, menu->page.line_count - 1);

    // Request redraw operation, if mark's index has changed.
    if(mark_index != menu->page.mark_index) {
        // Mark the menu as updated.
        menu->is_updated = true;

        // Request focused workspace's redraw.
        rose_workspace_request_redraw(menu->output->focused_workspace);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Event notification interface implementation: line.
////////////////////////////////////////////////////////////////////////////////

void
rose_ui_menu_notify_line_add(
    struct rose_ui_menu* menu, struct rose_ui_menu_line line) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // Refresh the menu.
    if(true) {
        struct rose_ui_menu_line skip = {};
        rose_ui_menu_refresh(menu, skip);
    }

    // Request redraw operation, if needed.
    for(ptrdiff_t i = 0; i != menu->page.line_count; ++i) {
        if(rose_ui_menu_line_is_included(menu->page.lines[i], line)) {
            // If the added line is visible, then mark the menu as updated.
            menu->is_updated = true;

            // Request focused workspace's redraw.
            rose_workspace_request_redraw(menu->output->focused_workspace);

            // And break out of the cycle.
            break;
        }
    }
}

void
rose_ui_menu_notify_line_remove(
    struct rose_ui_menu* menu, struct rose_ui_menu_line line) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // Check if the removed line includes any of the menu's lines.
    for(ptrdiff_t i = 0; i != menu->page.line_count; ++i) {
        if(rose_ui_menu_line_is_included(menu->page.lines[i], line)) {
            // If a removed line is visible, then mark the menu as updated.
            menu->is_updated = true;

            // Refresh the menu.
            rose_ui_menu_refresh(menu, line);

            // Request focused workspace's redraw.
            rose_workspace_request_redraw(menu->output->focused_workspace);

            // And break out of the cycle.
            break;
        }
    }

    // Clear menu's selection, if it belongs to the removed line.
    if(rose_ui_menu_line_is_included(menu->selection, line)) {
        // Clear the selection.
        menu->selection = (struct rose_ui_menu_line){};

        // Mark the menu as updated.
        menu->is_updated = true;

        // Request focused workspace's redraw.
        rose_workspace_request_redraw(menu->output->focused_workspace);
    }
}

void
rose_ui_menu_notify_line_update(
    struct rose_ui_menu* menu, struct rose_ui_menu_line line) {
    // Do nothing if the menu is not active.
    if(!rose_ui_menu_is_active(menu)) {
        return;
    }

    // Request redraw operation, if needed.
    for(ptrdiff_t i = 0; i != menu->page.line_count; ++i) {
        if(rose_ui_menu_line_is_equal(menu->page.lines[i], line)) {
            // If updated line is visible, then mark the menu as updated.
            menu->is_updated = true;

            // Request focused workspace's redraw.
            rose_workspace_request_redraw(menu->output->focused_workspace);

            // And break out of the cycle.
            break;
        }
    }
}
