// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_2AE0EAD7C9DB4AA4AFE79218D8FAF38E
#define H_2AE0EAD7C9DB4AA4AFE79218D8FAF38E

#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// Memory definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_memory {
    unsigned char* data;
    size_t size;
};

////////////////////////////////////////////////////////////////////////////////
// Allocation/deallocation interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_memory
rose_allocate(size_t size);

void
rose_free(struct rose_memory* memory);

#endif // H_2AE0EAD7C9DB4AA4AFE79218D8FAF38E
