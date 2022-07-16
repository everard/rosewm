# DESCRIPTION
Rose WM is a lightweight Wayland Compositor. At its core it is a simple Window
Manager, but its functionality can be extended by a number of system processes,
effectively making it a full-fledged Desktop Environment.

The following diagram shows how the system works.

```
+--------+         +------------------+
|        |  start  |                  |
| ROSEWM |-------->| SYSTEM PROCESSES |
|        |         |                  |
|        |<--IPC-->|                  |
+--------+         +------------------+
```

Window Manager starts zero or more system processes which communicate with it
through a Unix socket. Path to this IPC socket is specified in the
_ROSE_IPC_ENDPOINT_ environment variable.

There are the following types of system processes:
* background - displays background images on outputs;
* dispatcher - displays command prompt (similar to dmenu);
* notification daemon - displays notifications;
* panel - displays panel widgets on outputs;
* screen locker - displays lock screens on outputs.

# CONFIGURATION
TODO

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
