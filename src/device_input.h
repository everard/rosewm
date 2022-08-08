// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_B3E4C8C7C82B4EECB510E9E2A2F0FC35
#define H_B3E4C8C7C82B4EECB510E9E2A2F0FC35

#include "device_input_keyboard.h"
#include "device_input_pointer.h"
#include "device_input_tablet.h"

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_server_context;
struct wlr_input_device;

////////////////////////////////////////////////////////////////////////////////
// Input device definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_input_device_type {
    rose_input_device_type_unknown,
    rose_input_device_type_keyboard,
    rose_input_device_type_pointer,
    rose_input_device_type_tablet
};

struct rose_input {
    // Device's type.
    enum rose_input_device_type type;

    // Pointer to the server context.
    struct rose_server_context* ctx;

    // Pointer to the underlying input device.
    struct wlr_input_device* dev;

    // Device variant.
    union {
        struct rose_keyboard keyboard;
        struct rose_pointer pointer;
        struct rose_tablet tablet;
    };

    // Event listeners.
    struct wl_listener listener_destroy;

    // List link.
    struct wl_list link;

    // Device's ID.
    unsigned id;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_input_initialize(struct rose_server_context* ctx,
                      struct wlr_input_device* dev);

// Note: This function is called automatically upon input device's destruction.
void
rose_input_destroy(struct rose_input* input);

#endif // H_B3E4C8C7C82B4EECB510E9E2A2F0FC35
