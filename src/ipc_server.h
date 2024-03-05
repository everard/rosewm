// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_CCDF8B8A9BB149EDB2E82AB68ADEEF4E
#define H_CCDF8B8A9BB149EDB2E82AB68ADEEF4E

#include "ipc_types.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_server_context;
struct rose_ipc_server;

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_ipc_server*
rose_ipc_server_initialize(struct rose_server_context* context);

void
rose_ipc_server_destroy(struct rose_ipc_server* server);

////////////////////////////////////////////////////////////////////////////////
// Data transmission interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_ipc_server_dispatch_command(struct rose_ipc_server* server,
                                 struct rose_ipc_command command);

void
rose_ipc_server_broadcast_status(struct rose_ipc_server* server,
                                 struct rose_ipc_status status);

#endif // H_CCDF8B8A9BB149EDB2E82AB68ADEEF4E
