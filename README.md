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
via Wayland protocol and/or via a separate IPC protocol with its own Unix socket
(_$ROSE_IPC_ENDPOINT_ environment variable contains a path to this socket).

_Note: In this document all environment variables are prefixed with the "$"
symbol._

Here's a table of system processes with their descriptions.

| NAME                | DESCRIPTION                                    |
|---------------------|------------------------------------------------|
| BACKGROUND          | Displays background images on outputs.         |
| DISPATCHER          | Displays command prompt, handles IPC commands. |
| NOTIFICATION DAEMON | Displays notifications.                        |
| PANEL               | Displays panel widgets on outputs.             |
| SCREEN LOCKER       | Displays lock screens on outputs.              |

These processes _must_ create visible windows via the xdg_shell Wayland protocol
(in other words, no non-standard Wayland protocols are required).

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

Note: Here "\n" denotes the new line character. Each path _must_ end with a new
line character.

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
TODO

## KEYBOARD LAYOUTS
Keyboard layouts are configured with a simple text file which contains
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
