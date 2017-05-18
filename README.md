HelloDevice
===========

HelloDevice is a simple daemon that sits and monitors X11 input devices.
Whenever a device is enabled or disabled, it executes the command configured
by the user for this device, passing in the device name and device ID.
The custom command may then be used to e.g. set up a specific keyboard
layout for this device.

External program calling convention
-----------------------------------

The external program is executed as follows
````
$ custom-command -t [added|removed|present] -i <device ID> <device name>
````
Where `added` means a device has been added, `removed` means a device has
been removed, and `present` means the device was already present when
HelloDevice started up. For obvious reasons, you can only get `present`
immediately after startup.

The device ID  is the X11 device ID as returned by e.g. `xinput list`. Note:
the X11 device ID is **not** the same as the `/dev/input/event` device node
number.

The device name is the X11 device name as returned by - you guessed it -
`xinput list`. Note: the X11 device name **may not** be the same as the
kernel device name although on **most** systems for **most** devices it is
the same.

Configuration
-------------

Configuration lives in `$XDG_CONFIG_HOME/HelloDevice/HelloDevice.conf`.
`$XDG_CONFIG_HOME` usually resolves to `$HOME/.config`.

This directory will be prepended to `$PATH`, so feel free to drop any
configuration scripts in there.

Configuration Syntax
--------------------

The configuration file looks like this:
````
[General]
command=/path/to/command
````

Yes, exciting, isn't it? More features to follow, maybe, some day.

Make sure the command is executable.

License
-------

HelloDevice is licensed under the MIT license.

> Permission is hereby granted, free of charge, to any person obtaining a
> copy of this software and associated documentation files (the "Software"),
> to deal in the Software without restriction, including without limitation
> the rights to use, copy, modify, merge, publish, distribute, sublicense,
> and/or sell copies of the Software, and to permit persons to whom the
> Software is furnished to do so, subject to the following conditions: [...]

