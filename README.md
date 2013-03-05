WARNING!!
=========

WARNING! Everything here is completely unsupported. Use everything at your own
risk. The tools here can very easily kill your camera and cause other problems.
The author takes no responsibility or liability for anything that may arise as
a consequence of using this software. USE THESE TOOLS AT YOUR OWN RISK.


Introduction
============

EVERYTHING HERE IS MEANT FOR ADVANCED USERS who have a hard-bricked Hero2
camera (as in, the camera will not turn on at all) and who have no other
choice but to throw the camera away. If you can get warranty support from GoPro
instead, BY ALL MEANS USE IT. But don't try to do some experimental stuff on
your own and THEN try to get warranty support - once you start doing anything
experimental to the camera (including ANYTHING written here), you have voided
your warranty.


USB Command Mode
================

These are extremely unofficial and highly experimental tools for booting a
Hero2 camera using the built-in USB command mode.

To enter the Hero2 built-in USB command mode, you can do the following:
 1. Disconnect USB from camera
 2. Remove battery
 3. Insert battery
 4. Press and HOLD the Shutter button
 5. Plug in USB
 6. Press the Power button
 7. Release the Shutter button

There might not be anything showing on the screen when the Power button is
pressed. If the charging LED was on, it might turn off. Otherwise, the camera
will still "look" completely dead. But, the camera should enumerate as a USB
device with a VID/PID of 4255:0001. If so, we are in business. At this point,
it is possible to send simple commands over USB. They are:
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
specific to this version. See "Preparing the necessary bootstrap files" below
for how to do this.


Compiling and Dependencies
==========================
The gpboot program requires libusb-1.0.0 to interact with the camera, so you
will need to install the libusb-1.0.0 development files before compiling. On
an Ubuntu system, you can do this with the following command:

 $ sudo apt-get install libusb-1.0.0-dev

Once that is installed, you can build gpboot and prepare-bootstrap with a
simple make command:

 $ make


A warning about permissions
---------------------------
On some systems, you may need to add some kind of a udev rule to make the
camera accessible by non-root users. Or, you could run gpboot with the
'sudo' command.


Windows Version
===============
An even more experimental (and even less tested) but pre-built Windows version
of prepare-bootstrap and gpboot is included here. These should operate like the
Linux version (hopefully). Since Windows insists on having a specific driver
for each USB device, I've included a quick "driver" for the camera when it is
in command mode. This driver was made with Zadig.exe (see here:
http://www.libusb.org/wiki/windows_backend) but I know very little about
Windows and its drivers or how they work. Still, you should be able to open a
Windows command prompt and use prepare-bootstrap.exe and gpboot.exe as
described here. Miraculously, when running gpboot.exe --linux, the USB/Ethernet
device created by the camera does in fact get recognized by Windows, but I had
to unplug/replug USB to the camera after Linux was booted to make sure the
network was fully working. For anyone interested, the Windows version was built
using MinGW / MSYS.


Preparing the necessary bootstrap files
=======================================

To prepare the BLD and modified HAL, get the v222 HD2-firmware.bin file and
execute the following command:

 $ ./prepare-bootstrap HD2-firmware.bin

This will produce the v222-bld.bin and v222-hal-reloc.bin files. Now gpboot 
will have the files it needs to bootstrap the camera. Again, this
prepare-bootstrap commands MUST be run on the v222 firmware update file,
regardless of what you are actually trying to load and boot in the end.


To actually boot the camera, you can use the 'gpboot' command.

The gpboot tool supports three loading modes. They are:
 - Load bootloader
 - Load RTOS
 - Load Linux


Loading the Bootloader (BLD) only
=================================
 $ ./gpboot --bootloader

This will load the BLD (and the pre-modified HAL) and jump into the bootloader.
If everything else in the camera is okay, the camera should boot normally from
this point (and you can use some FW update method to reflash the real
bootloader in flash if it is somehow damaged). If you have serial connected to
the camera, you can short the TX/RX pins to drop into the bootloader console
prior to running this command.


Loading the RTOS
================
 $ ./gpboot --rtos rtos_file

This is probably the most "useful" booting method, but it is also risky.
This will load the bootloader, modified HAL, and the specified RTOS file to the
camera. The HAL / RTOS will be loaded at high addresses, along with a small
relocate.bin file that knows to copy them where they need to be (in case the
bootloader tries to overwrite the low memory for some reason). The bootloader
will be patched in RAM to jump to the relocate.bin file rather than booting
normally, to make sure it does not try to load some corrupted / damaged version
from flash for whatever reason.

To actually get the RTOS files, see
https://github.com/evilwombat/gopro-fw-tools
for a utility that can pull them out of existing HD2-firmware.bin image. The
RTOS image is NOT the HD2-firmware.bin file in its entirety, but is actually
a section inside this file. Since there are several firmware versions
available, there are several RTOS images you can use. In general, it seems like
a good idea to try to use the RTOS image from the firmware that you were trying
to install when the camera got bricked. So, if you were trying to downgrade
from v222 to v124, it makes sense to use the RTOS from the v222 firmware. But,
I don't have enough data to say "what works best". To save you the trouble of
running the FW parser, the following DD commands should be able to extract an
RTOS image from an existing firmware file. Note that each command is specific
to the firmware version, and using the wrong command with the wrong firmware
file will result in a useless RTOS image.

	If you want the RTOS from the v124 HD2-firmware.bin file:
	$ dd if=HD2-firmware.bin skip=213248 conv=notrunc of=rtos_v124.bin count=6344704 bs=1

	If you want the RTOS from the v198 HD2-firmware.bin file:
	$ dd if=HD2-firmware.bin skip=15862016 conv=notrunc of=rtos_v198.bin count=6246400 bs=1

	If you want the RTOS from the v222 HD2-firmware file:
	$ dd if=HD2-firmware.bin skip=15866112 conv=notrunc of=rtos_v222.bin count=6250496 bs=1

	(Also keep in mind that v198 and v222 firmware files have an
	 'alternate' RTOS image as well - this is section_3 if you use
	 fwparser, but I do not know what the difference between the main and
	 the alternate images is).

If done correctly (and if you are lucky), the camera will boot up to the point
that it will be execute RTOS commands from autoexec.ash on the sdcard, which
could possibly be used to restore the camera to a working state. Keep in mind
that sometimes something goes wonky with the A:\ drive on the RTOS, so it may
be necessary to change directory from A:\ to D:\ in autoexec.ash before doing
any interesting commands ( cd d:\ ).

I do not have an exact autoexec.ash sequence that will reflash the camera to
a working state that works 100% of the time, but perhaps the autoexec.ash on
gopro's website is a good start. Keep in mind that 'reboot' from autoexec.ash
WILL NOT WORK if the camera is booted using the USB mode, meaning that for
an update to finish, you might have to boot the camera using USB several times,
with a delay of a few minutes before you turn off the camera and boot again,
to let the camera "do its thing". This applies to performing updates as well,
since the camera does "naturally" try to reboot once or twice during the update
process (which again, won't work in this mode and you may have to do the
'reboot' by hand).


Loading Linux
=============

 $ ./gpboot --linux

This is probably the most advanced booting method, and the least useful. This
command will load a pre-build Linux kernel (zImage) and a pre-built initrd
(initrd.lzma) to the camera and boot them. The camera will show no signs of
life on its LCD or LEDs, but it should show up as a newly-detected USB Ethernet
device. Once that happens, you can telnet to the camera on address 10.9.9.1
and hopefully get a little Linux shell. This may allow you to examine the state
of the flash (also, cat /proc/mtd) and be able to do linux commands
interactively. It is my hope that this can be a useful method to restore the
flash on a camera that is otherwise totally wiped, but at the moment there is
no known way to reflash the camera from within Linux that has worked. But, I
haven't really tried this all that much. I did include 'nanddump' and
'nandwrite' into the busybox build, and they might come in handy (just be
CAREFUL!). How the state of the flash relates to the CRCs in the partition
table (PTB, also mtd1) is still not quite clear to me, either.
There should also be kernel messages and a shell on the camera's serial port,
at 115200 baud.

The kernel commandline options are hard-coded into gpboot.c, and if you are
advanced enough that you want to change them, you can modify the file and
rebuild :). If you want to build your own kernel, see sources/sources.txt.
The kernel here is basically the kernel that was posted on gopro's website
(http://gopro.com/support/open-source) with some modifications to make it
actually compile, and some fixes. See sources/ for the patches applied to the
kernel (on top of gopro's tarball). There should be a kernel config file in
there as well. There is also a busybox config file, but I did not modify
busybox (and you can get the source from busybox.net - see sources.txt).

Why did I use LZMA as the compression method over the more widely-used GZIP?
It seems to result in smaller files, which means shorter times needed to load
the stuff over USB.


Well, that's that - have fun, be careful, and again, do everything at your own
risk, and don't blame me for anything that goes wrong. But I hope someone can
find this stuff useful, and can make a reliable hard-debrick method.

-- evilwombat
