# DESCRIPTION
Rose WM is a lightweight Wayland Compositor. At its core it is a simple Window
Manager, but its functionality can be extended by a number of system processes,
effectively making it a full-fledged Desktop Environment.

## STRUCTURE
The Compositor performs the following tasks.
 * It maintains lists of input and output devices.
 * It maintains lists of desktop surfaces created by clients.
 * It processes and delivers events from input devices.
 * It displays desktop surfaces on outputs.

The Compositor uses the concept of _workspace_ which contains desktop surfaces.
Each desktop surface can only belong to a single workspace at a time, and each
workspace can only belong to a single output at a time.

The Compositor has _current workspace_ which receives all input events. Each
workspace has a single _focused surface_ which receives input events if this
workspace is current. Each output displays a single workspace at a time which is
its _focused workspace_.

The Compositor has a panel which displays output's index, focused workspace's
index, and focused surface's name on each output. Panel's functionality can be
extended by the PANEL system process (see [below](#system-processes)).

The Compositor also can show a menu which contains lists of outputs, workspaces,
and surfaces. This menu can move and focus workspaces and surfaces.

## SYSTEM PROCESSES
Compositor's functionality can be extended via a number of system processes. The
following diagram shows how these processes communicate with the Compositor.

```
+--------+                   +------------------+
|        |  WAYLAND PROTOCOL |                  |
|        |<----------------->|                  |
| ROSEWM |                   | SYSTEM PROCESSES |
|        | ROSE IPC PROTOCOL |                  |
|        |<----------------->|                  |
+--------+                   +------------------+
```

The [ROSE IPC PROTOCOL](#rose-ipc-protocol) is a separate protocol through which
clients can query Compositor's state, configure its inputs and outputs, lock and
unlock the screen.

The ROSE IPC PROTOCOL has its own Unix Domain Socket. Path to the socket is
specified in the _$ROSE_IPC_ENDPOINT_ environment variable.

_Note: In this document all environment variables are prefixed with the "$"
symbol._

The following table lists system processes with their descriptions.

| NAME                | DESCRIPTION                                    |
|---------------------|------------------------------------------------|
| BACKGROUND          | Displays background images on outputs.         |
| DISPATCHER          | Displays command prompt, handles IPC commands. |
| NOTIFICATION DAEMON | Displays notifications.                        |
| PANEL               | Displays panel widgets on outputs.             |
| SCREEN LOCKER       | Displays lock screens on outputs.              |

These processes _must_ create visible windows via the xdg_shell Wayland protocol
(in other words, no non-standard Wayland protocols are required: usual GUI libs
can be used).

System processes _must_ obey compositor's commands: when close event is received
by any surface which belongs to such process, the surface _must_ be destroyed as
soon as possible.

When a system process creates an xdg_toplevel, the compositor configures the
size of the surface. For some system processes this is exact requirement, while
for others it is a maximal limit. The following table specifies which behaviour
is expected from which system process.

| PROCESS             | LIMIT TYPE |
|---------------------|------------|
| BACKGROUND          | Exact      |
| DISPATCHER          | Exact      |
| NOTIFICATION DAEMON | Maximal    |
| PANEL               | Exact      |
| SCREEN LOCKER       | Maximal    |

Some system processes are expected to create an xdg_toplevel for each output.
The following table specifies such processes.

| PROCESS             | CREATES XDG_TOPLEVEL FOR EACH OUTPUT? |
|---------------------|---------------------------------------|
| BACKGROUND          | Yes                                   |
| DISPATCHER          | No                                    |
| NOTIFICATION DAEMON | No                                    |
| PANEL               | Yes                                   |
| SCREEN LOCKER       | Yes                                   |

# CONFIGURATION
The Compositor is configured via a number of configuration files. These files
are looked-up in the following directories (in the specified order):
 1. _$HOME_/.config/rosewm
 2. /etc/rosewm

If a certain configuration file is not found in either of these directories,
then default configuration is used.

Here's a table of configuration options.

| OPTION              | CONFIGURATION FILE NAME    | MANDATORY? |
|---------------------|----------------------------|------------|
| Fonts               | fonts                      | Yes        |
| Theme               | theme                      | No         |
| Keyboard shortcuts  | keyboard_control_scheme    | No         |
| Keyboard layouts    | keyboard_layouts           | No         |
| BACKGROUND          | system_background          | No         |
| DISPATCHER          | system_dispatcher          | No         |
| NOTIFICATION DAEMON | system_notification_daemon | No         |
| PANEL               | system_panel               | No         |
| SCREEN LOCKER       | system_screen_locker       | No         |
| TERMINAL            | system_terminal            | Yes        |

Mandatory configuration files _must_ be present in at least one of the
configuration directories.

## FONTS
Fonts are configured with a simple text file which contains new-line-separated
list of paths to font files. At least one path _must_ be specified, and the
second path _should_ be a path to FontAwesome.

Example:
```
/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf\n
/usr/share/fonts/fontawesome/fontawesome-webfont.ttf\n
```

Note: Here `\n` denotes the new line character. Each path _must_ end with the
new line character.

## THEME
Theme can be configured with a binary file. The format of such file is specified
in the following table.

| FIELD                  | TYPE                              |
|------------------------|-----------------------------------|
| font size              | byte                              |
| panel's position       | byte                              |
| panel's size           | byte                              |
| color scheme           | array of 4-byte RGBA color values |

Panel's position can only take the values specified in the following table.

| VALUE | POSITION |
|-------|----------|
| 0     | TOP      |
| 1     | BOTTOM   |
| 2     | LEFT     |
| 3     | RIGHT    |

Color scheme contains the following colors, in this order:
* panel's background color,
* panel's foreground color,
* panel's highlight color,
* menu's background color,
* menu's foreground color,
* menu's highlight color #1,
* menu's highlight color #2,
* surface's background color #1,
* surface's background color #2,
* surface's resizing background color #1,
* surface's resizing background color #2,
* surface's resizing foreground color,
* workspace's background color.

## KEYBOARD SHORTCUTS
Keyboard shortcuts can be configured with a binary file. The format of such file
is specified in the following table.

| FIELD         | TYPE                                              |
|---------------|---------------------------------------------------|
| leader        | byte                                              |
| $N_{core}$    | byte                                              |
| $N_{menu}$    | byte                                              |
| $N_{IPC}$     | byte                                              |
| core actions  | array of $N_{core}$ objects of _core action_ type |
| menu actions  | array of $N_{menu}$ objects of _menu action_ type |
| IPC actions   | array of $N_{IPC}$ objects of _IPC action_ type   |

Leader can only take the values specified in the following table.

| VALUE | KEY               |
|-------|-------------------|
| 0     | `XKB_KEY_Super_L` |
| 1     | `XKB_KEY_Super_R` |
| 2     | `XKB_KEY_Alt_L`   |
| 3     | `XKB_KEY_Alt_R`   |
| 4     | `XKB_KEY_Menu`    |

Requirements:
 * $N_{core} \in$ `[rose_n_core_action_types, 2 * rose_n_core_action_types]`
 * $N_{menu} \in$ `[rose_n_menu_action_types, 2 * rose_n_menu_action_types]`
 * $N_{IPC} \in [0, 128]$

The constants `rose_n_core_action_types` and `rose_n_menu_action_types` are
defined in the [src/action.h](src/action.h) file.

### CORE/MENU ACTION TYPE
Both the _core action_ and _menu action_ types have the same format. This format
is specified in the following table.

| FIELD              | TYPE                      |
|--------------------|---------------------------|
| shortcut           | object of _shortcut_ type |
| index              | byte                      |

Requirements:
 * core action's index field must take values from the `rose_core_action_type`
enumeration;
 * menu action's index field must take values from the `rose_menu_action_type`
enumeration;

These enumerations and constants are defined in the
[src/action.h](src/action.h) file.

### IPC ACTION TYPE
_IPC action_ type has the following format.

| FIELD              | TYPE                      |
|--------------------|---------------------------|
| IPC command        | array of 64 bytes         |
| shortcut           | object of _shortcut_ type |

### SHORTCUT TYPE
Shortcut type is an array of 5 keysyms. Each keysym is represented by a 32-bit
unsigned integer value packed into array of bytes, starting from value's least
significant byte, all the way up to the most significant byte (little endian).

If the first keysym in a shortcut has value 0, then this value is replaced by
the leader's keysym.

If a shortcut consists of less than 5 keysyms, then unused keysyms must be set
to 0.

## KEYBOARD LAYOUTS
Keyboard layouts can be configured with a simple text file which contains
comma-separated list of layouts names. No new line characters should be present
in this configuration file.

## COMMAND LINE ARGUMENTS FOR PROCESSES
Command line arguments which are used for starting different processes (system
processes and terminal) are specified via null-character-terminated list of
strings.

Example #1 [file: `system_terminal`]:
```
/usr/bin/xfce4-terminal\0
```

Example #2 [file: `system_dispatcher`]:
```
/usr/bin/python3\0/usr/local/bin/dispatcher.py\0
```

Note: Here `\0` denotes the null character.

# ROSE IPC PROTOCOL
ROSE IPC PROTOCOL is a simple packet-based protocol. Packets have the following
format.

| FIELD         | TYPE                                   |
|---------------|----------------------------------------|
| $L_{payload}$ | 16-bit unsigned integer, little endian |
| payload       | array of $L_{payload}$ bytes           |

Requirement: Payload's size can not exceed 8 KiB.

Once a client establishes connection with the Compositor, it _must_ select
protocol's variant by sending a packet with a single-byte payload. This payload
can only take the values specified in the following table.

| VALUE | PROTOCOL VARIANT |
|-------|------------------|
| 1     | CONFIGURATOR     |
| 2     | DISPATCHER       |
| 3     | STATUS           |

Example packet #1, selects CONFIGURATOR protocol variant [hex]:
```
 01 00   01
+-----+ +--+
   ^      ^
   |      |
   |      +-- Payload: protocol variant
   |
   +-- Payload's size
```

Example packet #2, selects DISPATCHER protocol variant [hex]:
```
 01 00   02
+-----+ +--+
   ^      ^
   |      |
   |      +-- Payload: protocol variant
   |
   +-- Payload's size
```

Example packet #3, selects STATUS protocol variant [hex]:
```
 01 00   03
+-----+ +--+
   ^      ^
   |      |
   |      +-- Payload: protocol variant
   |
   +-- Payload's size
```

After a client selects protocol variant, the Compositor verifies client's access
rights:
 * STATUS protocol is accessible by all clients;
 * CONFIGURATOR and DISPATCHER protocols are only accessible by the DISPATCHER,
PANEL, SCREEN LOCKER system processes, and by any process which has been started
via DISPATCHER protocol with IPC access rights.

## CONFIGURATOR
Through this half-duplex protocol variant clients can query and modify the state
if the Compositor:
 * query the keymap (keyboard layouts and current layout index),
 * query the number of input and output devices,
 * query the state of input and output devices,
 * configure input and output devices,
 * switch keyboard layout,
 * lock/unlock the screen,
 * reload configuration files.

This protocol variant is defined in the
[src/ipc_connection_configurator.c](src/ipc_connection_configurator.c) file (The
implementation in this case _is_ specification).

## DISPATCHER
Through this full-duplex protocol variant the Compositor can send IPC commands
to connected clients, and clients can request execution of privileged processes.

Compositor packet's payload contains one or more IPC commands. Each IPC command
is an array of 64 bytes. Clients interpret these IPC commands in their own way.

Client packet's payload has the following format.

| FIELD                  | TYPE                                       |
|------------------------|--------------------------------------------|
| access rights          | byte                                       |
| command line arguments | array of null-character-terminated strings |

The access rights field is a bitwise OR of zero or more values from the
`rose_command_access_rights` enumeration. This enumeration is defined in the
[src/command.h](src/command.h) file.

Example client's packet #1, starts a terminal emulator with IPC access rights:
```
\x19\x00\x01/usr/bin/xfce4-terminal\0
```

Example client's packet #2, starts an application which has IPC access rights,
and access to the privileged Wayland protocols:
```
\x2B\x00\x03/usr/bin/python3\0/usr/local/bin/my_app.py\0
```

Note: Here `\x` indicates the start of a hexadecimal escape sequence, and `\0`
denotes the null character.

## STATUS
Through this protocol variant the Compositor can notify all connected clients of
its status change. Each packet contains one or more status messages. Each status
message has the following format.

| FIELD | TYPE                                          |
|-------|-----------------------------------------------|
| type  | byte                                          |
| info  | depends on the type of the message, see below |

The following table describes the values which can appear in the type field of a
status message, along with the corresponding contents of message's info field.

| VALUE | DESCRIPTION                      | INFO'S TYPE      | INFO           |
|-------|----------------------------------|------------------|----------------|
| 0     | changed: state vector            | array of 4 bytes | see below      |
| 1     | changed: keymap                  | _empty_          | ---            |
| 2     | changed: keyboard control scheme | _empty_          | ---            |
| 3     | changed: theme                   | _empty_          | ---            |
| 4     | initialized: input device        | unsigned integer | device's index |
| 5     | destroyed:   input device        | unsigned integer | device's index |
| 6     | initialized: output device       | unsigned integer | device's index |
| 7     | destroyed:   output device       | unsigned integer | device's index |

Server's state vector contains the following data, in this order:
 * screen lock flag,
 * keyboard shortcuts inhibition flag,
 * keyboard layout index.

# COMPILATION
To compile the program, run:
```
make protocols
make
```

To copy the program to the `/usr/local/bin/` directory, run:
```
sudo make install
```

To remove the program from the `/usr/local/bin/` directory, run:
```
sudo make uninstall
```

Build system uses `pkg-config` to obtain compiler and linker flags for
dependencies.

Dependencies:
 * WLRoots version 0.15
 * wayland-protocols
 * wayland-scanner
 * wayland-server
 * libinput
 * xkbcommon
 * pixman-1
 * freetype2
 * fribidi

# LICENSE
Copyright Nezametdinov E. Ildus 2022.

Distributed under the GNU General Public License, Version 3.
(See accompanying file LICENSE_GPL_3_0.txt or copy at
https://www.gnu.org/licenses/gpl-3.0.txt)

# DEDICATION
Dedicated to my family and my late aunt Rauza/Saniya. Everybody called her Rose.
She was like a second mother to me, and the kindest person I've ever known. She
always helped people and never asked anything in return.

Cənəttə urını bulsın!
(Tatar. May her place be in Heaven!)
