WARNING! Everything here is completely unsupported. Use everything at your own
risk. The tools here can very easily kill your camera and cause other problems.
The author takes no responsibility or liability for anything that may arise as
a consequence of using this software. USE THESE TOOLS AT YOUR OWN RISK.

These are xtremely unofficial and highly experimental tools for booting a Hero2
camera using the built-in USB command mode.

This mode can be accessed using the following procedure:
1. Disconnect USB from camera
2. Remove battery
3. Insert battery
4. Press and HOLD the Shutter button
5. Plug in USB
6. Press the Power button
7. Release the Shutter button

The camera should enumerate as a USB device with a VID/PID of 4255:0001. If so,
we are in business. At this point, it is possible to send simple commands over
USB. They are:
- Read from an arbitrary 32-bit word by physical address
- Write to an arbitrary 32-bit word by physical address
- Leave USB command mode and jump to specified physical address

These commands can be used to initalize the memory controller, load arbitrary
code into RAM, and execute it. 

The code that may be loaded could be of our own creation entirely, or it can be
one or more sections of the the original GoPro firmware (which may have been
unpacked with https://github.com/evilwombat/gopro-fw-tools). So in theory it
should be possible to load the HAL section and the bootloader (or the HAL and
the RTOS section) to the camera and jump to it, in an attempt to resume an
interrupted firmware update. Or, it may be possible to load a Linux kernel
that was modified for running directly on the camera (unlike the existing
kernel on the camera, which runs in a thread on the RTOS).

However, when loading Linux or the RTOS, we still need to load a portion of the
BLD bootloader to perform basic HW initalization (like serial), etc.
Another problem is that before the camera software runs, the BLD bootloader
loads the HAL section and makes some modifications to it, presumably to 
relocate the HAL to a place where the Linux kernel and the RTOS can get to it
(since the HAL code does not seem to be position-independent). Without these
modifications, the raw HAL section found in the HD2-firmware.bin file is not
usable.

So, to load and execute anything interesting, such as Linux or the RTOS, we
need to do the following things:
- Load the BLD bootloader (for things like serial init)
- Load a slightly modified (fixed-up) HAL
- Load the thing we want to run (RTOS or Linux, and possibly an initrd)
- Patch the BLD bootloader with an instruction to jump to our code rather than
  trying to boot normally.

I am not able to include the BLD and modified HAL binaries directly in the
repo, but there is a tool to extract the binaries (and perform the HAL fix-ups)
from the v222 HD2-firmware.bin file. Regardless of which RTOS version or Linux
kernel we are trying to load, the BLD/HAL must come from the v222 version of
the HD2-firmware.bin file, since the offsets and modifications that we make are
specific to this version. 

To prepare the BLD and modified HAL, get the v222 HD2-firmware.bin file and
execute the following command:
$ ./prepare-bootstrap HD2-firmware.bin

This will produce the v222-bld.bin and v222-hal-reloc.bin files. Now gpboot 
will have the files it needs to bootstrap the camera.

I will finish writing the rest after I've cleaned up some gpboot stuff.
