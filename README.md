bareDOOM
-------

bareDOOM is a patchset for the [barebox](https://barebox.org) bootloader
that integrates DOOM with the available barebox frameworks for framebuffer,
input, file system and so on ... etc. This allows it to run everywhere
where barebox is running, be it on a x86 laptop under UEFI, on industrial
machinery or even a RISC-V emulator compiled to WebAssembly.
Head over to [barebox.org](https://barebox.org/jsbarebox/?graphic=1) to try
the latter out.

As barebox often functions as bare metal hardware bring up kit, it can
be useful for porting DOOM to new boards.

What works?
-----------

- Video
- PC-Speaker Sound
- Input
- Loading IWADs from external file
- No hardcoded Framebuffer format
- Both 32-bit and 64bit systems
- little-endian and big-endian systems

Interested in porting?
----------------------

Interact with me on Github or shoot me a message on the ``#barebox``
IRC channel on Libera.Chat (also bridged to [Matrix](https://riot.im/app/#/room/#barebox:matrix.org)).

barebox uses a [mailing list](https://lists.infradead.org/mailman/listinfo/barebox)
for contributions and discussions.

License
-------

barebox and bareDOOM are free software: you can redistribute them and/or modify
them under the terms of the GNU General Public License, version 2, as published
by the Free Software Foundation.

See README and LICENCES for more information.
