// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
// Dedicated to my family and my late aunt Rauza/Saniya. Everybody called her
// Rose. She was like a second mother to me, and the kindest person I've ever
// known. She always helped people and never asked anything in return.
// - May her place be in Heaven.
//
#include "server_context.h"

#include <wayland-server-core.h>
#include <wlr/backend.h>

#include <stddef.h>
#include <stdlib.h>

int
main() {
    // Initialize the server context.
    struct rose_server_context context = {};
    if(!rose_server_context_initialize(&context)) {
        rose_server_context_destroy(&context);
        return EXIT_FAILURE;
    }

    // Start server's backend.
    if(!wlr_backend_start(context.backend)) {
        rose_server_context_destroy(&context);
        return EXIT_FAILURE;
    }

    // Start server's event loop.
    wl_display_run(context.display);

    // Destroy the context upon exit.
    rose_server_context_destroy(&context);
    return EXIT_SUCCESS;
}
