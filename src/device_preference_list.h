// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_3A4FD2923BE04F3593CABFD5E44662AC
#define H_3A4FD2923BE04F3593CABFD5E44662AC

#include "device_input.h"
#include "device_output.h"

////////////////////////////////////////////////////////////////////////////////
// Device name definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_device_name_size = 64 };

struct rose_device_name {
    char data[rose_device_name_size];
};

////////////////////////////////////////////////////////////////////////////////
// Device type definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_device_type {
    rose_device_type_pointer,
    rose_device_type_output,
    rose_device_type_count_
};

////////////////////////////////////////////////////////////////////////////////
// Device preference definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_device_preference {
    // Target device name.
    struct rose_device_name device_name;

    // Target device type.
    enum rose_device_type device_type;

    // Device configuration parameters.
    union {
        struct rose_pointer_configure_parameters pointer;
        struct rose_output_configure_parameters output;
    } parameters;
};

////////////////////////////////////////////////////////////////////////////////
// Device preference list declaration.
////////////////////////////////////////////////////////////////////////////////

struct rose_device_preference_list;

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_device_preference_list*
rose_device_preference_list_initialize(char const* file_name);

void
rose_device_preference_list_destroy(
    struct rose_device_preference_list* preference_list);

////////////////////////////////////////////////////////////////////////////////
// Manipulation interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_device_preference_list_update(
    struct rose_device_preference_list* preference_list,
    struct rose_device_preference preference);

////////////////////////////////////////////////////////////////////////////////
// Application interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_pointer_apply_preferences(
    struct rose_pointer* pointer,
    struct rose_device_preference_list* preference_list);

void
rose_output_apply_preferences(
    struct rose_output* output,
    struct rose_device_preference_list* preference_list);

////////////////////////////////////////////////////////////////////////////////
// Device name acquisition interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_device_name
rose_input_name_obtain(struct rose_input* input);

struct rose_device_name
rose_output_name_obtain(struct rose_output* output);

#endif // H_3A4FD2923BE04F3593CABFD5E44662AC
