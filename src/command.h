// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_912497DA005D417288BB624C83B68F94
#define H_912497DA005D417288BB624C83B68F94

#include <sys/types.h>
#include <stdbool.h>

////////////////////////////////////////////////////////////////////////////////
// Command list declaration.
////////////////////////////////////////////////////////////////////////////////

struct rose_command_list;

////////////////////////////////////////////////////////////////////////////////
// Command access rights definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_command_access_rights {
    rose_command_access_ipc = 1,
    rose_command_access_wayland_privileged_protocols = 2
};

typedef unsigned rose_command_access_rights_mask;

////////////////////////////////////////////////////////////////////////////////
// Command list initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_command_list*
rose_command_list_initialize();

void
rose_command_list_destroy(struct rose_command_list* command_list);

////////////////////////////////////////////////////////////////////////////////
// Command list manipulation interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_command_list_execute_command( //
    struct rose_command_list* command_list,
    rose_command_access_rights_mask rights, char* command_and_args[]);

void
rose_command_list_notify_command_termination(
    struct rose_command_list* command_list, pid_t command_pid);

////////////////////////////////////////////////////////////////////////////////
// Command list query interface.
////////////////////////////////////////////////////////////////////////////////

rose_command_access_rights_mask
rose_command_list_query_access_rights(struct rose_command_list* command_list,
                                      pid_t command_pid);

////////////////////////////////////////////////////////////////////////////////
// Command execution interface.
////////////////////////////////////////////////////////////////////////////////

pid_t
rose_execute_command_in_child_process(char* command_and_args[]);

void
rose_execute_command(char* command_and_args[]);

#endif // H_912497DA005D417288BB624C83B68F94
