// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_9D04EE19ACDA4D83AF5DADA8E1D75BDB
#define H_9D04EE19ACDA4D83AF5DADA8E1D75BDB

#include "memory.h"

////////////////////////////////////////////////////////////////////////////////
// Filesystem query interface.
////////////////////////////////////////////////////////////////////////////////

size_t
rose_filesystem_obtain_file_size(char const* file_path);

////////////////////////////////////////////////////////////////////////////////
// Data reading interface.
////////////////////////////////////////////////////////////////////////////////

// Reads data from the file to a memory buffer.
struct rose_memory
rose_filesystem_read_data(char const* file_path);

// Reads data from the file to a memory buffer as a zero-terminated byte string.
struct rose_memory
rose_filesystem_read_ntbs(char const* file_path);

#endif // H_9D04EE19ACDA4D83AF5DADA8E1D75BDB
