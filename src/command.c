// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "command.h"
#include "map.h"

#include <wayland-server-core.h>
#include <signal.h>
#include <unistd.h>

#include <stddef.h>
#include <stdlib.h>

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
            // Obtain a pointer to the current command.
            struct rose_command* command = wl_container_of(node, command, node);

            // Remove the node from the map.
            command_list->root = rose_map_remove(command_list->root, node);

            // Free command's memory.
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
rose_command_list_execute_command( //
    struct rose_command_list* command_list,
    rose_command_access_rights_mask rights, char* command_and_args[]) {
    // Do nothing if command list is not specified.
    if(command_list == NULL) {
        return false;
    }

    // Allocate and initialize a new command.
    struct rose_command* command = malloc(sizeof(struct rose_command));

    if(command == NULL) {
        return false;
    } else {
        // Start a child process with the given arguments, and save its PID.
        *command = (struct rose_command){
            .pid = rose_execute_command_in_child_process(command_and_args),
            .rights = rights};
    }

    // Check if the child process has been started successfully.
    if(command->pid == (pid_t)(-1)) {
        goto error;
    }

    // Add command to the list.
    struct rose_map_insertion_result result = rose_map_insert(
        command_list->root, &(command->node), rose_command_node_compare);

    // Update map's root.
    command_list->root = result.root;

    // If the command has been successfully added to the list, then signal
    // operation's success.
    if(result.node == &(command->node)) {
        return true;
    }

error:
    // On error, free command's memory and signal operation's failure.
    return free(command), false;
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
        // Obtain a pointer to the command.
        struct rose_command* command = wl_container_of(node, command, node);

        // Free command's memory.
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

    // Find command's node with the given PID.
    struct rose_map_node* node = rose_map_find(
        command_list->root, &command_pid, rose_command_key_compare);

    // If such command exists, then return its rights.
    if(node != NULL) {
        // Obtain a pointer to the command.
        struct rose_command* command = wl_container_of(node, command, node);

        // Return command's access rights.
        return command->rights;
    }

    // At this point the command does not belong to the list, and has no special
    // access rights.
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Command execution interface implementation.
////////////////////////////////////////////////////////////////////////////////

pid_t
rose_execute_command_in_child_process(char* command_and_args[]) {
    // Fork and save child's PID.
    pid_t child_pid = -1;
    if((child_pid = fork()) == -1) {
        return -1;
    }

    // Handle fork.
    if(child_pid == 0) {
        // Fork: the following code is only executed in the child process.

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
        execvp(command_and_args[0], command_and_args);

        // Terminate the child process, if needed.
        exit(EXIT_FAILURE);
    }

    // Fork: the following code is only executed in the parent process.
    // Return child's PID.
    return child_pid;
}

void
rose_execute_command(char* command_and_args[]) {
    // Fork for the first time.
    if(fork() == 0) {
        // Fork: the following code is only executed in the child process.

        // Execute the command.
        int status = EXIT_SUCCESS;
        if(rose_execute_command_in_child_process(command_and_args) == -1) {
            status = EXIT_FAILURE;
        }

        // Terminate the child process.
        exit(status);
    }
}
