// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "command.h"
#include "filesystem.h"
#include "map.h"

#include <wayland-server-core.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////
// Command definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_command {
    // Command's PID and access rights.
    pid_t pid;
    rose_command_access_rights_mask rights;

    // A node in the map of commands.
    struct rose_map_node node;
};

////////////////////////////////////////////////////////////////////////////////
// Command list definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_command_list {
    struct rose_map_node* root;
};

////////////////////////////////////////////////////////////////////////////////
// Command map comparison utility functions.
////////////////////////////////////////////////////////////////////////////////

static int
rose_command_key_compare(void const* k, struct rose_map_node const* x) {
    pid_t pid = *((pid_t const*)(k));
    struct rose_command const* command = wl_container_of(x, command, node);

    if(pid < command->pid) {
        return -1;
    } else if(pid > command->pid) {
        return +1;
    } else {
        return 0;
    }
}

static int
rose_command_node_compare(struct rose_map_node const* x,
                          struct rose_map_node const* y) {
    struct rose_command const* command = wl_container_of(x, command, node);
    return rose_command_key_compare(&(command->pid), y);
}

////////////////////////////////////////////////////////////////////////////////
// Command list initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_command_list*
rose_command_list_initialize() {
    // Allocate and initialize a new command list.
    struct rose_command_list* command_list =
        malloc(sizeof(struct rose_command_list));

    if(command_list != NULL) {
        *command_list = (struct rose_command_list){};
    }

    // Return created command list.
    return command_list;
}

void
rose_command_list_destroy(struct rose_command_list* command_list) {
    // Do nothing if command list is not specified.
    if(command_list == NULL) {
        return;
    }

    // Destroy all elements of the list.
    if(true) {
        struct rose_map_node* node = rose_map_lower(command_list->root);
        struct rose_map_node* next = rose_map_node_obtain_next(node);

        for(; node != NULL;
            node = next, next = rose_map_node_obtain_next(next)) {
            // Obtain the current command.
            struct rose_command* command = wl_container_of(node, command, node);

            // Remove the node from the map.
            command_list->root = rose_map_remove(command_list->root, node);

            // Free memory.
            free(command);
        }
    }

    // Free command list's memory.
    free(command_list);
}

////////////////////////////////////////////////////////////////////////////////
// Command list manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

bool
rose_command_list_execute_command(
    struct rose_command_list* command_list,
    struct rose_command_argument_list argument_list,
    rose_command_access_rights_mask rights) {
    // Do nothing if command list is not specified.
    if(command_list == NULL) {
        return false;
    }

    // If no access rights are specified, then execute the command as a
    // stand-alone process.
    if(rights == 0) {
        return (rose_execute_command(argument_list), true);
    }

    // Allocate and initialize a new command.
    struct rose_command* command = malloc(sizeof(struct rose_command));

    if(command == NULL) {
        return false;
    } else {
        // Start a child process with the given arguments, and save its PID.
        *command = (struct rose_command){
            .pid = rose_execute_command_in_child_process(argument_list),
            .rights = rights};
    }

    // Check if the child process has been started successfully.
    if(command->pid == (pid_t)(-1)) {
        return (free(command), false);
    }

    // Add command to the list.
    struct rose_map_insertion_result result = rose_map_insert(
        command_list->root, &(command->node), rose_command_node_compare);

    // Update map's root.
    command_list->root = result.root;

    // Check if the command has been successfully added to the list.
    if(result.node != &(command->node)) {
        return (free(command), false);
    }

    return true;
}

void
rose_command_list_notify_command_termination(
    struct rose_command_list* command_list, pid_t command_pid) {
    // Do nothing if command list is not specified.
    if(command_list == NULL) {
        return;
    }

    // Find command's node with the given PID.
    struct rose_map_node* node = rose_map_find(
        command_list->root, &command_pid, rose_command_key_compare);

    // Remove the node from the map.
    command_list->root = rose_map_remove(command_list->root, node);

    // Destroy the command.
    if(node != NULL) {
        // Obtain the command.
        struct rose_command* command = wl_container_of(node, command, node);

        // Free memory.
        free(command);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Command list query interface implementation.
////////////////////////////////////////////////////////////////////////////////

rose_command_access_rights_mask
rose_command_list_query_access_rights(struct rose_command_list* command_list,
                                      pid_t command_pid) {
    // If command list is not specified, or if it is empty, then the command has
    // no special access rights.
    if((command_list == NULL) || (command_list->root == NULL)) {
        return 0;
    }

    // A command with invalid PID has no special access rights; a command with
    // PID 1 or PID 0 also has no special access rights.
    if((command_pid == (pid_t)(-1)) || (command_pid <= 1)) {
        return 0;
    }

    // Find a command with the given PID.
    struct rose_map_node* node = rose_map_find(
        command_list->root, &command_pid, rose_command_key_compare);

    // If such command exists, then return its rights.
    if(node != NULL) {
        // Obtain the command.
        struct rose_command* command = wl_container_of(node, command, node);

        // Return command's access rights.
        return command->rights;
    }

    // At this point the command does not belong to the list, and has no special
    // access rights.
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Command argument list initialization interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_command_argument_list
rose_command_argument_list_initialize(char const* file_path) {
    // Read argument list.
    struct rose_memory memory = rose_filesystem_read_data(file_path);
    if((memory.size != 0) && (memory.size <= UINT16_MAX)) {
        if(memory.data[memory.size - 1] == '\0') {
            return (struct rose_command_argument_list){
                .data = (char*)(memory.data), .size = memory.size};
        }
    }

    // Return empty argument list on error.
    return (rose_free(&memory), (struct rose_command_argument_list){});
}

////////////////////////////////////////////////////////////////////////////////
// Command execution interface implementation.
////////////////////////////////////////////////////////////////////////////////

enum { rose_command_argument_max_count = 255 };

pid_t
rose_execute_command_in_child_process(
    struct rose_command_argument_list argument_list) {
    // Ensure that the argument list is always zero-terminated.
    if((argument_list.size == 0) ||
       (argument_list.data[argument_list.size - 1] != '\0')) {
        return -1;
    }

    // Fork and save child's PID.
    pid_t child_pid = -1;
    if((child_pid = fork()) == -1) {
        return -1;
    }

    // Handle fork.
    if(child_pid == 0) {
        // Fork: the following code is only executed in the child process.

        // Allocate memory for command line arguments.
        char* arguments[rose_command_argument_max_count + 1] = {};

        // Parse argument list.
        for(char **argument = arguments,
                 **sentinel = arguments + rose_command_argument_max_count;
            (argument != sentinel) && (argument_list.size != 0); ++argument) {
            // Save the argument and obtain its size.
            size_t argument_size = strlen(*argument = argument_list.data) + 1;

            // Shrink the argument list. This is safe because the list is always
            // zero-terminated.
            argument_list.data += argument_size;
            argument_list.size -= argument_size;
        }

        // Set SID.
        setsid();

        // Reset signal handlers.
        signal(SIGALRM, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        signal(SIGHUP, SIG_DFL);
        signal(SIGINT, SIG_DFL);

        // Execute the command.
        execvp(arguments[0], arguments);

        // Terminate the child process, if needed.
        exit(EXIT_FAILURE);
    }

    // Fork: the following code is only executed in the parent process.
    // Return child's PID.
    return child_pid;
}

void
rose_execute_command(struct rose_command_argument_list argument_list) {
    // Ensure that the argument list is always zero-terminated.
    if((argument_list.size == 0) ||
       (argument_list.data[argument_list.size - 1] != '\0')) {
        return;
    }

    // Fork for the first time.
    if(fork() == 0) {
        // Fork: the following code is only executed in the child process.

        // Execute the command.
        int status = EXIT_SUCCESS;
        if(rose_execute_command_in_child_process(argument_list) == -1) {
            status = EXIT_FAILURE;
        }

        // Terminate the child process.
        exit(status);
    }
}
