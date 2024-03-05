// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "filesystem.h"

#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Filesystem query interface implementation.
////////////////////////////////////////////////////////////////////////////////

size_t
rose_filesystem_obtain_file_size(char const* file_path) {
    struct stat file_stat = {};
    if(stat(file_path, &file_stat) != 0) {
        return 0;
    }

    if((file_stat.st_size < 1) || ((uintmax_t)(file_stat.st_size) > SIZE_MAX)) {
        return 0;
    }

    return (size_t)(file_stat.st_size);
}

////////////////////////////////////////////////////////////////////////////////
// Data reading interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_memory
rose_filesystem_read_data(char const* file_path) {
    // Initialize an empty memory object.
    struct rose_memory memory = {};

    // Obtain file size.
    size_t file_size = rose_filesystem_obtain_file_size(file_path);
    if(file_size == 0) {
        return memory;
    }

    // Open the file.
    FILE* file = fopen(file_path, "rb");
    if(file == NULL) {
        return memory;
    }

    // Allocate memory and read data from the file.
    memory = rose_allocate(file_size);
    if(memory.data != NULL) {
        if(fread(memory.data, memory.size, 1, file) != 1) {
            rose_free(&memory);
        }
    }

    // Close the file and return data.
    return fclose(file), memory;
}

struct rose_memory
rose_filesystem_read_ntbs(char const* file_path) {
    // Initialize an empty memory object.
    struct rose_memory memory = {};

    // Obtain file size.
    size_t file_size = rose_filesystem_obtain_file_size(file_path);
    if((file_size == 0) || (file_size == SIZE_MAX)) {
        return memory;
    }

    // Open the file.
    FILE* file = fopen(file_path, "rb");
    if(file == NULL) {
        return memory;
    }

    // Allocate memory and read data from the file.
    memory = rose_allocate(file_size + 1);
    if(memory.data != NULL) {
        // Ensure zero-termination.
        memory.data[file_size] = '\0';

        // Read the data.
        if(fread(memory.data, file_size, 1, file) != 1) {
            rose_free(&memory);
        }
    }

    // Close the file and return data.
    return fclose(file), memory;
}
