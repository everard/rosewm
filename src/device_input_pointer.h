// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#ifndef H_C81C1008866749028A84C6AC9667E2E1
#define H_C81C1008866749028A84C6AC9667E2E1

#include <wayland-server-core.h>

////////////////////////////////////////////////////////////////////////////////
// Forward declarations.
////////////////////////////////////////////////////////////////////////////////

struct rose_input;

////////////////////////////////////////////////////////////////////////////////
// Acceleration type enumeration definition.
////////////////////////////////////////////////////////////////////////////////

enum rose_pointer_acceleration_type {
    rose_pointer_acceleration_type_flat,
    rose_pointer_acceleration_type_adaptive
};

////////////////////////////////////////////////////////////////////////////////
// Pointer definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_pointer {
    // Parent input device.
    struct rose_input* parent;

    // Event listeners.
    struct wl_listener listener_axis;
    struct wl_listener listener_button;
    struct wl_listener listener_motion;
    struct wl_listener listener_motion_absolute;
    struct wl_listener listener_frame;
};

////////////////////////////////////////////////////////////////////////////////
// Pointer state definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_pointer_state {
    // Device's ID.
    unsigned id;

    // Device's acceleration type. If the device does not support acceleration,
    // then this field is set to an arbitrary value.
    enum rose_pointer_acceleration_type acceleration_type;

    // Device's speed (normalized to [-1; 1]). If the device does not support
    // acceleration, then this field is set to 0.
    float speed;

    // Flags.
    bool is_acceleration_supported;
};

////////////////////////////////////////////////////////////////////////////////
// Configuration-related definitions.
////////////////////////////////////////////////////////////////////////////////

enum rose_pointer_configuration_type {
    rose_pointer_configure_acceleration_type = 0x01,
    rose_pointer_configure_speed = 0x02
};

// Pointer's configuration mask. Is a bitwise OR of zero or more values from the
// rose_pointer_configuration_type enumeration.
typedef unsigned rose_pointer_configuration_mask;

struct rose_pointer_configuration_parameters {
    // Pointer's configuration flags.
    rose_pointer_configuration_mask flags;

    // Pointer's requested acceleration type.
    enum rose_pointer_acceleration_type acceleration_type;

    // Pointer's requested speed. During configuration operation this parameter
    // will be clamped to the [-1; 1] interval.
    float speed;
};

////////////////////////////////////////////////////////////////////////////////
// Initialization/destruction interface.
////////////////////////////////////////////////////////////////////////////////

void
rose_pointer_initialize(
    struct rose_pointer* pointer, struct rose_input* parent);

// Note: This function is called automatically upon parent device's destruction.
void
rose_pointer_destroy(struct rose_pointer* pointer);

////////////////////////////////////////////////////////////////////////////////
// Configuration interface.
////////////////////////////////////////////////////////////////////////////////

bool
rose_pointer_configure(
    struct rose_pointer* pointer,
    struct rose_pointer_configuration_parameters parameters);

////////////////////////////////////////////////////////////////////////////////
// State query interface.
////////////////////////////////////////////////////////////////////////////////

struct rose_pointer_state
rose_pointer_state_obtain(struct rose_pointer* pointer);

#endif // H_C81C1008866749028A84C6AC9667E2E1
