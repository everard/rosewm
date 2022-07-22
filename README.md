# DESCRIPTION
Rose WM is a lightweight Wayland Compositor. At its core it is a simple Window
Manager, but its functionality can be extended by a number of system processes,
effectively making it a full-fledged Desktop Environment.

The following diagram shows how the system works.

```
+--------+                   +------------------+
|        |  WAYLAND PROTOCOL |                  |
|        |<----------------->|                  |
| ROSEWM |                   | SYSTEM PROCESSES |
|        | ROSE IPC PROTOCOL |                  |
|        |<----------------->|                  |
+--------+                   +------------------+
```

Window Manager starts zero or more system processes which communicate with it
through Wayland protocol, and, additionally through a separate IPC protocol with
its dedicated Unix socket (_$ROSE_IPC_ENDPOINT_ environment variable contains a
path to this socket).

_Note: In this document all environment variables are prefixed with the "$"
symbol._

There are the following system processes:
 * BACKGROUND - displays background images on outputs;
 * DISPATCHER - displays command prompt, handles IPC commands (more on this
   later);
 * NOTIFICATION DAEMON - displays notifications;
 * PANEL - displays panel widgets on outputs;
 * SCREEN LOCKER - displays lock screens on outputs.

These system processes must operate through the normal xdg_shell protocol, with
the following additional requirements:
 * when Window Manager sends close event to any xdg_surface, system process
   _must_ destroy such surface;
 * BACKGROUND, PANEL and SCREEN LOCKER system processes are expected to create
   an xdg_toplevel for each output global. Window Manager will automatically
   send close events to relevant surfaces which belong to these system
   processes.

# CONFIGURATION
There is a number of configuration files. These files are looked-up in the
following configuration directories (in the specified order):
 1. _$HOME_/.config/rosewm
 2. /etc/rosewm

If a certain configuration file is not found in either of these directories,
then default configuration is used.

Here's a table of configuration options.

| OPTION              | CONFIGURATION FILE NAME    | MANDATORY? |
|---------------------|----------------------------|------------|
| Fonts               | fonts                      | Yes        |
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

Note: Each path _must_ end with a new line character.

Example:
```
/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf
/usr/share/fonts/fontawesome/fontawesome-webfont.ttf

```

## KEYBOARD SHORTCUTS
TODO

## KEYBOARD LAYOUTS
Keyboard layouts are configured with a simple text file, which contains
comma-separated list of keyboard layouts. No new line characters should be
present in this configuration file.

## COMMAND LINE ARGUMENTS FOR SYSTEM PROCESSES AND TERMINAL
Command line arguments which are used to start different processes (system
processes and terminal) are specified through null-character-terminated list.

Example #1:
```
/usr/bin/xfce4-terminal\0
```

Example #2:
```
/usr/bin/python3\0/usr/local/bin/dispatcher.py\0
```

Note: Here "\0" denotes a null character.

# ROSE IPC PROTOCOL
TODO

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

Build system uses pkg-config to obtain compiler and linker flags for dependencies.

Dependencies:
 * WLRoots version 14.1
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
