// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "server_context.h"
#include "map.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define min_(a, b) ((a) < (b) ? (a) : (b))

#define cast_(type, x) ((type)(x))
#define container_of_(ptr, type, subobject) \
    cast_(type*, cast_(char*, ptr) - offsetof(type, subobject))

////////////////////////////////////////////////////////////////////////////////
// Device database entry definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_device_database_entry {
    struct rose_device_preference preference;
    struct rose_map_node node;
    struct wl_list link;
};

////////////////////////////////////////////////////////////////////////////////
// Device database definition.
////////////////////////////////////////////////////////////////////////////////

enum { rose_device_database_size_max = 128 };

struct rose_device_database {
    struct rose_device_database_entry storage[rose_device_database_size_max];
    size_t size;

    struct rose_map_node* map_root;
    struct wl_list order;
};

////////////////////////////////////////////////////////////////////////////////
// Preference list definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_device_preference_list {
    char* file_name;
    struct rose_device_database databases[rose_device_type_count_];
};

////////////////////////////////////////////////////////////////////////////////
// Device database node comparison utility functions.
////////////////////////////////////////////////////////////////////////////////

static int
rose_device_database_node_compare(
    struct rose_map_node const* x, struct rose_map_node const* y) {
    return memcmp(
        container_of_(x, struct rose_device_database_entry const, node)
            ->preference.device_name.data,
        container_of_(y, struct rose_device_database_entry const, node)
            ->preference.device_name.data,
        rose_device_name_size);
}

static int
rose_device_database_key_compare(void const* k, struct rose_map_node const* x) {
    return memcmp(
        cast_(struct rose_device_name const*, k)->data,
        container_of_(x, struct rose_device_database_entry const, node)
            ->preference.device_name.data,
        rose_device_name_size);
}

////////////////////////////////////////////////////////////////////////////////
// Device database manipulating utility function.
////////////////////////////////////////////////////////////////////////////////

static void
rose_device_database_insert(
    struct rose_device_database* database,
    struct rose_device_preference preference) {
    // Precondition: All preferences in the database have the same device type
    // as the given preference.

    // Obtain a database entry.
    struct rose_device_database_entry* entry = NULL;
    while(true) {
        // Find an existing node with the given device name.
        struct rose_map_node* node = rose_map_find(
            database->map_root, &(preference.device_name),
            rose_device_database_key_compare);

        if(node != NULL) {
            // If such node has been found, then obtain the database entry which
            // contains this node.
            entry = wl_container_of(node, entry, node);

            // Move the entry to the front of the list.
            wl_list_remove(&(entry->link));
            wl_list_insert(&(database->order), &(entry->link));

            // And break out of the cycle.
            break;
        }

        // Obtain a storage for a new entry.
        if(database->size != rose_device_database_size_max) {
            // If the database is not full, then obtain a new entry from
            // preallocated storage.
            entry = &(database->storage[database->size++]);
        } else {
            // Otherwise, obtain the last entry in the list.
            entry = wl_container_of(database->order.prev, entry, link);

            // Remove it from the list.
            wl_list_remove(&(entry->link));

            // Remove it from the map.
            database->map_root =
                rose_map_remove(database->map_root, &(entry->node));
        }

        // Set device's name and type for the entry.
        entry->preference.device_name = preference.device_name;
        entry->preference.device_type = preference.device_type;

        // Initialize entry's data.
        switch(preference.device_type) {
            case rose_device_type_pointer:
                entry->preference.parameters.pointer =
                    (struct rose_pointer_configuration_parameters){};

                break;

            case rose_device_type_output:
                entry->preference.parameters.output =
                    (struct rose_output_configuration_parameters){};

                break;

            default:
                break;
        }

        // Insert the entry to the front of the list.
        wl_list_insert(&(database->order), &(entry->link));

        // Insert the entry into the map.
        database->map_root = rose_map_insert(
                                 database->map_root, &(entry->node),
                                 rose_device_database_node_compare)
                                 .root;

        break;
    }

    // Merge the preference.
    switch(preference.device_type) {
        case rose_device_type_pointer: {
            // Obtain current flags.
            unsigned flags = preference.parameters.pointer.flags;

            // Merge the flags.
            entry->preference.parameters.pointer.flags |= flags;

#define merge_(type, x)                                                     \
    if((flags & rose_##type##_configure_##x) != 0) {                        \
        entry->preference.parameters.type.x = preference.parameters.type.x; \
    }

            // Merge the parameters.
            merge_(pointer, acceleration_type);
            merge_(pointer, speed);

            break;
        }

        case rose_device_type_output: {
            // Obtain current flags.
            unsigned flags = preference.parameters.output.flags;

            // Merge the flags.
            entry->preference.parameters.output.flags |= flags;

            // Merge the parameters.
            if((flags & rose_output_configure_adaptive_sync) != 0) {
                entry->preference.parameters.output.adaptive_sync_state =
                    preference.parameters.output.adaptive_sync_state;
            }

            merge_(output, transform);
            merge_(output, scale);
            merge_(output, mode);

#undef merge_

            break;
        }

        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
// File IO utility functions.
////////////////////////////////////////////////////////////////////////////////

static bool
rose_device_preference_read(
    struct rose_device_preference* preference, FILE* file) {
    // Zero-initialize the preference.
    *preference = (struct rose_device_preference){};

#define read_array_(array)                          \
    if(fread(array, sizeof(array), 1, file) != 1) { \
        return false;                               \
    }

#define read_object_(object)                             \
    if(fread(&(object), sizeof(object), 1, file) != 1) { \
        return false;                                    \
    }

#define read_integer_(integer)                   \
    if(true) {                                   \
        /* Read 32-bit integer value. */         \
        int32_t x = 0;                           \
        if(fread(&x, sizeof(x), 1, file) != 1) { \
            return false;                        \
        }                                        \
                                                 \
        /* Cast the value to the target type. */ \
        integer = cast_(int, x);                 \
    }

    // Read device's name.
    read_array_(preference->device_name.data);

    // Read device's type and parameters.
    switch(fgetc(file)) {
        case rose_device_type_pointer:
            // Zero-initialize pointer's parameters.
            preference->device_type = rose_device_type_pointer;
            preference->parameters.pointer =
                (struct rose_pointer_configuration_parameters){};

            // Read pointer's parameters.
            preference->parameters.pointer.flags = fgetc(file);
            preference->parameters.pointer.acceleration_type = fgetc(file);
            read_object_(preference->parameters.pointer.speed);

            break;

        case rose_device_type_output:
            // Zero-initialize output's parameters.
            preference->device_type = rose_device_type_output;
            preference->parameters.output =
                (struct rose_output_configuration_parameters){};

            // Read output's parameters.
            preference->parameters.output.flags = fgetc(file);
            preference->parameters.output.adaptive_sync_state = fgetc(file);
            preference->parameters.output.transform = fgetc(file);

            read_object_(preference->parameters.output.scale);
            read_integer_(preference->parameters.output.mode.width);
            read_integer_(preference->parameters.output.mode.height);
            read_integer_(preference->parameters.output.mode.rate);

            break;

        default:
            return false;
    }

#undef read_integer_
#undef read_object_
#undef read_array_

    // Operation succeeded.
    return true;
}

static bool
rose_device_preference_write(
    struct rose_device_preference* preference, FILE* file) {
#define write_array_(array)                          \
    if(fwrite(array, sizeof(array), 1, file) != 1) { \
        return false;                                \
    }

#define write_object_(object)                             \
    if(fwrite(&(object), sizeof(object), 1, file) != 1) { \
        return false;                                     \
    }

#define write_integer_(integer)                   \
    if(true) {                                    \
        /* Write 32-bit integer value. */         \
        uint32_t x = cast_(uint32_t, integer);    \
        if(fwrite(&x, sizeof(x), 1, file) != 1) { \
            return false;                         \
        }                                         \
    }

    // Write device's name.
    write_array_(preference->device_name.data);

    // Write device's type.
    fputc(preference->device_type, file);

    // Write device's parameters.
    switch(preference->device_type) {
        case rose_device_type_pointer:
            // Write pointer's parameters.
            fputc(preference->parameters.pointer.flags, file);
            fputc(preference->parameters.pointer.acceleration_type, file);
            write_object_(preference->parameters.pointer.speed);

            break;

        case rose_device_type_output:
            // Write output's parameters.
            fputc(preference->parameters.output.flags, file);
            fputc(preference->parameters.output.adaptive_sync_state, file);
            fputc(preference->parameters.output.transform, file);

            write_object_(preference->parameters.output.scale);
            write_integer_(preference->parameters.output.mode.width);
            write_integer_(preference->parameters.output.mode.height);
            write_integer_(preference->parameters.output.mode.rate);

            break;

        default:
            return false;
    }

#undef write_integer_
#undef write_object_
#undef write_array_

    // Operation succeeded.
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_device_preference_list*
rose_device_preference_list_initialize(char const* file_name) {
    // Compute storage sizes.
    size_t file_name_size = ((file_name != NULL) ? (strlen(file_name) + 1) : 0);
    size_t data_size = sizeof(struct rose_device_preference_list);

    // Allocate and initialize a new preference list.
    struct rose_device_preference_list* preference_list =
        malloc(data_size + file_name_size);

    if(preference_list == NULL) {
        return NULL;
    } else {
        *preference_list = (struct rose_device_preference_list){};
    }

    // Initialize the databases.
    for(ptrdiff_t i = 0; i != rose_device_type_count_; ++i) {
        wl_list_init(&(preference_list->databases[i].order));
    }

    // Save the name of the file with device preferences, if needed.
    if(file_name_size > 1) {
        // Compute a pointer to the file name's storage.
        preference_list->file_name = cast_(char*, preference_list) + data_size;

        // Copy the file name.
        memcpy(preference_list->file_name, file_name, file_name_size);
    }

    // Read preferences from the file, if needed.
    while(preference_list->file_name != NULL) {
        // Open the file with the given name.
        FILE* file = fopen(preference_list->file_name, "rb");
        if(file == NULL) {
            break;
        }

        // Read preferences from the file.
        for(int i = 0;
            i < (rose_device_type_count_ * rose_device_database_size_max);
            i++) {
            // Read the next preference.
            struct rose_device_preference preference;
            if(!rose_device_preference_read(&preference, file)) {
                break;
            }

            // Insert it into the corresponding database.
            rose_device_database_insert(
                preference_list->databases + preference.device_type,
                preference);
        }

        // Close the file.
        fclose(file);
        break;
    }

    // Return created preference list.
    return preference_list;
}

void
rose_device_preference_list_destroy(
    struct rose_device_preference_list* preference_list) {
    // Do nothing if there is no preference list.
    if(preference_list == NULL) {
        return;
    }

    // Write preferences to the file, if needed.
    while(preference_list->file_name != NULL) {
        // Open the file with the given name.
        FILE* file = fopen(preference_list->file_name, "wb");
        if(file == NULL) {
            break;
        }

        // Write preferences to the file.
        for(ptrdiff_t i = 0; i != rose_device_type_count_; ++i) {
            struct rose_device_database_entry* entry = NULL;
            wl_list_for_each_reverse(
                entry, &(preference_list->databases[i].order), link) {
                if(!rose_device_preference_write(&(entry->preference), file)) {
                    goto end;
                }
            }
        }

    end:
        // Close the file.
        fclose(file);
        break;
    }

    // Free memory.
    free(preference_list);
}

////////////////////////////////////////////////////////////////////////////////
// Manipulation interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
rose_device_preference_list_update(
    struct rose_device_preference_list* preference_list,
    struct rose_device_preference preference) {
    if((preference.device_type >= 0) &&
       (preference.device_type < rose_device_type_count_)) {
        rose_device_database_insert(
            preference_list->databases + preference.device_type, preference);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Application interface implementation.
////////////////////////////////////////////////////////////////////////////////

#define configure_(type)                                                  \
    struct rose_map_node* node = rose_map_find(                           \
        preference_list->databases[rose_device_type_##type].map_root,     \
        &device_name, rose_device_database_key_compare);                  \
                                                                          \
    if(node != NULL) {                                                    \
        struct rose_device_database_entry* entry =                        \
            wl_container_of(node, entry, node);                           \
                                                                          \
        rose_##type##_configure(type, entry->preference.parameters.type); \
    }

void
rose_pointer_apply_preferences(
    struct rose_pointer* pointer,
    struct rose_device_preference_list* preference_list) {
    // Obtain device's name.
    struct rose_device_name device_name =
        rose_input_name_obtain(pointer->parent);

    // Configure the device.
    configure_(pointer);
}

void
rose_output_apply_preferences(
    struct rose_output* output,
    struct rose_device_preference_list* preference_list) {
    // Obtain device's name.
    struct rose_device_name device_name = rose_output_name_obtain(output);

    // Configure the device.
    configure_(output);
}

#undef configure_

////////////////////////////////////////////////////////////////////////////////
// Device name acquisition interface implementation.
////////////////////////////////////////////////////////////////////////////////

#define acquire_name_(object)                            \
    /* Initialize an empty name. */                      \
    struct rose_device_name device_name = {};            \
                                                         \
    /* Obtain device's name. */                          \
    if(object->device->name != NULL) {                   \
        size_t name_size = strlen(object->device->name); \
        memcpy(                                          \
            device_name.data, object->device->name,      \
            min_(name_size, sizeof(device_name.data)));  \
    }                                                    \
                                                         \
    /* Return the name. */                               \
    return device_name;

struct rose_device_name
rose_input_name_obtain(struct rose_input* input) {
    acquire_name_(input);
}

struct rose_device_name
rose_output_name_obtain(struct rose_output* output) {
    acquire_name_(output);
}
