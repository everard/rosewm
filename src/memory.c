// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "memory.h"
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// Allocation/deallocation interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_memory
rose_allocate(size_t size) {
    // Initialize an empty memory object.
    struct rose_memory memory = {};

    // Allocate memory.
    if((size != 0) && ((memory.data = malloc(size)) != NULL)) {
        memory.size = size;
    }

    // Return result of allocation.
    return memory;
}

void
rose_free(struct rose_memory* memory) {
    if(memory != NULL) {
        free(memory->data);

        memory->data = NULL;
        memory->size = 0;
    }
}
