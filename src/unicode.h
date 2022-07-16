// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_1E8BDDBF5E3745C082E4A42E8CCC5B0C
#define H_1E8BDDBF5E3745C082E4A42E8CCC5B0C

#include <stddef.h>
#include <uchar.h>

////////////////////////////////////////////////////////////////////////////////
// Unicode string definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_utf32_string_size_max = 128 };

struct rose_utf32_string {
    char32_t data[rose_utf32_string_size_max];
    size_t size;
};

struct rose_utf8_string {
    char* data;
    size_t size;
};

////////////////////////////////////////////////////////////////////////////////
// String conversion interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_utf8_string
rose_convert_ntbs_to_utf8(char* string);

struct rose_utf32_string
rose_convert_utf8_to_utf32(struct rose_utf8_string string);

#endif // H_1E8BDDBF5E3745C082E4A42E8CCC5B0C
