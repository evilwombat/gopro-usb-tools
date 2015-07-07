/*
 *  Copyright (c) 2013, evilwombat
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gp_api.h"

#define CMDLINE_LEN 256

#define CAMTYPE_UNKNOWN	-1
#define CAMTYPE_H2	2
#define CAMTYPE_H3B	3
#define CAMTYPE_H3PB	4
#define CAMTYPE_H4	5

int write_atags(libusb_device_handle *dev, const char *cmdline,
		 unsigned int initrd_addr, unsigned int initrd_size,
		 unsigned int tag_addr)
{
	int cmd_tag_len;
	char cmdline_buf[CMDLINE_LEN];
	int cmd_len = strlen(cmdline);

	if (initrd_size)
		cmd_len += 25;

	if (cmd_len > CMDLINE_LEN) {
		printf("Kernel command line too long.\n");
		return -1;
	}

	if (initrd_size)
		snprintf(cmdline_buf, CMDLINE_LEN, "%s initrd=0x%08x,0x%08x", cmdline, initrd_addr, initrd_size);
	else
		snprintf(cmdline_buf, CMDLINE_LEN, "%s", cmdline);

	cmd_len = strlen(cmdline_buf);
	cmd_tag_len = 2 + (cmd_len + 3) / 4;
	gp_write_reg(dev, tag_addr + 0x00, 0x00000002);	// core
	gp_write_reg(dev, tag_addr + 0x04, 0x54410001);
	gp_write_reg(dev, tag_addr + 0x08, cmd_tag_len);
	gp_write_reg(dev, tag_addr + 0x0c, 0x54410009);
	gp_write_string(dev, tag_addr + 0x10, cmdline_buf);

	gp_write_reg(dev, tag_addr + 0x08 + cmd_tag_len * 4, 0x00000000);
	gp_write_reg(dev, tag_addr + 0x0c + cmd_tag_len * 4, 0x00000000);	// end of tag list
	return 0;
}

/* We assume the entry point for our kernel will be 0xc3000000 */
int gp_load_linux(libusb_device_handle *dev, const char *kernel,
		  const char *initrd, const char *cmdline,
		  unsigned int mach_type)
{
	uint32_t initrd_base = 0xc7000000;
	int initrd_size, ret;
	struct stat st;

	gp_load_file(dev, kernel, 0xc3008000);

	if (initrd) {
		ret = stat(initrd, &st);
		if (ret) {
			printf("Could not get initrd size: %d\n", ret);
			return ret;
		}

		initrd_size = (int) st.st_size;
		gp_load_file(dev, initrd, 0xc7000000);
	} else
		initrd_size = 0;

	printf("Patching in some init code...\n");
	printf("Setting ARM machine type to 0x%04x\n", mach_type);
	gp_write_reg(dev, 0xc3000000, 0xe3a01e40 | ((mach_type >> 4) & 0xf0));	// mov r1, mach_type >> 4
	gp_write_reg(dev, 0xc3000004, 0xe3811000 | (mach_type & 0xff));		// orr r1, r1, mach_type & 0xff

	gp_write_reg(dev, 0xc3000008, 0xe3a024c3);	// mov r2, 0xc3000000
	gp_write_reg(dev, 0xc300000C, 0xe3822020);	// orr r2, r2, 0x20	; atags at c3000020
	gp_write_reg(dev, 0xc3000010, 0xea001ffa);	// b 0xc3008000	; kernel start

	return write_atags(dev, cmdline, initrd_base, initrd_size, 0xc3000020);
}

int gp_h3b_boot_bld(libusb_device_handle *dev, const char *hal_file, const char *bld_file)
{
	int ret = 0;
	unsigned int temp;

	ret = gp_load_file(dev, bld_file, 0xc0000000);
	if (ret) {
		printf("Could not load BLD file %s\n", bld_file);
		return -1;
	}

	ret = gp_load_file(dev, hal_file, 0xc00a0000);
	if (ret) {
		printf("Could not load %s - did you run prepare-boostrap?\n", hal_file);
		return -1;
	}

	printf("Poking at I2S MUX (?) register...\n");
	temp = gp_read_reg(dev, 0x6000a050);
	temp &= ~0x0A;
	gp_write_reg(dev, 0x6000a050, temp);

	printf("Okay, here goes nothing...\n");

	ret = gp_exec(dev, 0xc0000000);
	if (ret) {
		printf("Exec failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int gp_h3b_boot_linux(libusb_device_handle *dev, const char *hal_file)
{
	int ret = 0;
	unsigned int temp;

	ret = gp_load_file(dev, "h3b-v300-bld.bin", 0xc0000000);
	if (ret) {
		printf("Could not load h3b-v300-bld.bin - did you run prepare-boostrap?\n");
		printf("You should run prepare-bootstrap on the Hero3 Black v300 firmware\n");
		printf("file, to generate some of the files needed to make Linux work...\n");
		return -1;
	}

	ret = gp_load_file(dev, hal_file, 0xc00a0000);
	if (ret) {
		printf("Could not load %s - did you run prepare-boostrap?\n", hal_file);
		return -1;
	}

	printf("Patching in a vector to 0xc3000000...\n");
	gp_write_reg(dev, 0xc0002a50, 0xe3a0f4c3);	// Jump to 0xc3000000

	printf("Poking at I2S MUX (?) register...\n");
	temp = gp_read_reg(dev, 0x6000a050);
	temp &= ~0x0A;
	gp_write_reg(dev, 0x6000a050, temp);

	gp_load_linux(dev, "zImage-a7", "initrd.lzma",
		      "mem=200M@0xc3000000 console=tty0 console=ttyS0,115200n8 root=/dev/ram0 init=/bin/sh ", 0xe11);

	printf("Okay, here goes nothing...\n");

	ret = gp_exec(dev, 0xc0000000);
	if (ret) {
		printf("Exec failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int gp_boot_linux(libusb_device_handle *dev)
{
	int ret = 0;
        ret = gp_load_file(dev, "v312-bld.bin", 0xc0000000);
	if (ret) {
		printf("Could not load v312-bld.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	ret = gp_load_file(dev, "v312-hal-reloc.bin", 0xc00a0000);
	if (ret) {
		printf("Could not load v312-hal-reloc.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	printf("Patching in a vector to 0xc3000000...\n");
	gp_write_reg(dev, 0xc00024c4, 0xe3a0f4C3);	// Jump to 0xc3000000

	gp_load_linux(dev, "zImage", "initrd.lzma",
		      "mem=200M@0xc3000000 console=tty0 console=ttyS0,115200n8 root=/dev/ram0 init=/bin/sh ", 0x4c7);

	printf("Okay, here goes nothing...\n");

	ret = gp_exec(dev, 0xc0000000);
	if (ret) {
		printf("Exec failed: %d\n", ret);
		return ret;
	}
	
	return 0;
}

int gp_boot_bld(libusb_device_handle *dev)
{
	int ret = 0;
        ret = gp_load_file(dev, "v312-bld.bin", 0xc0000000);
	if (ret) {
		printf("Could not load v312-bld.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	ret = gp_load_file(dev, "v312-hal-reloc.bin", 0xc00a0000);
	if (ret) {
		printf("Could not load v312-hal-reloc.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	printf("Okay, here goes nothing...\n");

	ret = gp_exec(dev, 0xc0000000);
	if (ret) {
		printf("Exec failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int gp_boot_rtos(libusb_device_handle *dev, const char *rtos_file)
{
	int ret = 0;
        ret = gp_load_file(dev, "v312-bld.bin", 0xc0000000);
	if (ret) {
		printf("Could not load v312-bld.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	ret = gp_load_file(dev, "relocate.bin", 0xc7000000);
	if (ret) {
		printf("Could not load relocate.bin\n");
		return -1;
	}
	
	ret = gp_load_file(dev, "v312-hal-reloc.bin", 0xc8000000);
	if (ret) {
		printf("Could not load v312-hal-reloc.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	ret = gp_load_file(dev, rtos_file, 0xc9000000);    /* v124 section_3 or v222 section_9 */
	if (ret) {
		printf("Could not load RTOS file %s\n", rtos_file);
		printf("This should be section_3 from the v124 firmware, or section_9 from\n");
		printf("the v198 / v222 / v312 firmware, depending on what you are doing.\n");
		return -1;
	}

	printf("Patching in a jump to our relocator..\n");
	gp_write_reg(dev, 0xc00024c4, 0xe3a0f4c7);          /* Jump to relocator */
	
	printf("Okay, here goes nothing...\n");
	ret = gp_exec(dev, 0xc0000000);
	if (ret) {
		printf("Exec failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int gp_h3b_boot_rtos(libusb_device_handle *dev, const char *hal_file, const char *rtos_file)
{
	int ret = 0;

	ret = gp_load_file(dev, "evilbootstrap.bin", 0xc0000000);
	if (ret) {
		printf("Could not load evilbootstrap.bin\n");
		return -1;
	}

	ret = gp_load_file(dev, hal_file, 0xc00a0000);
	if (ret) {
		printf("Could not load %s - did you run prepare-boostrap?\n", hal_file);
		return -1;
	}

	ret = gp_load_file(dev, rtos_file, 0xc0100000);
	if (ret) {
		printf("Could not load RTOS file %s\n", rtos_file);
		printf("This should be the patched RTOS file that was made by prepare-bootstrap\n");
		return -1;
	}

	printf("Okay, here goes nothing...\n");

	/* Jump to evilbootstrap. This will blink the LEDs for a few seconds and then jump to the
	 * RTOS at address 0xc0100000. The delay gives us time to unplug USB before the RTOS
	 * boots, or else it will go into USB storage mode, which isn't very useful unless all
	 * we want is an expensive card reader.
	 */
	ret = gp_exec(dev, 0xc0000000);

	if (ret) {
		printf("Exec failed: %d\n", ret);
		return ret;
	}

	return 0;
}

void print_usage(const char *name)
{
	printf("Usage:\n");
	printf("  For Hero2 Cameras: \n");
	printf("      %s --bootloader\n", name);
	printf("      Load v312 BLD and fixed-up HAL on a Hero2 camera, and jump to BLD\n");
	printf("\n");
	printf("      %s --rtos [rtos_section]\n", name);
	printf("      Try to boot the given RTOS image using the v312 fixed-up HAL\n");
	printf("      on a Hero2 camera\n");
	printf("      In fw v124, this is section_3\n");
	printf("      In fw v198 / v222 / v312, this is section_9 or section_3\n");
	printf("\n");
	printf("      %s --linux\n", name);
	printf("      (Hero2) - Boot a Linux kernel. If this works, the camera should show up\n");
	printf("      as a USB Ethernet device and you should be able to use it to telnet\n");
	printf("      to it at 10.9.9.1\n");
	printf("\n\n");
	printf("  For Hero3 Black Cameras:\n");
	printf("      %s --h3b-linux\n", name);
	printf("      Boot a Linux kernel on an H3 Black. If this works, the camera should show\n");
	printf("      up as a USB Ethernet device and you should be able to use it to telnet\n");
	printf("      to it at 10.9.9.1\n");
	printf("\n");
	printf("      %s --h3b-bld [bld file]\n", name);
	printf("      Try to load the given BLD bootloader using the H3B v300 fixed-up HAL.\n");
	printf("      This may be useful if you have a camera with a corrupted BST or BLD,\n");
	printf("      partition, or if you have soldered a UART connection to your camera.\n");
	printf("\n");
	printf("      %s --h3b-rtos [rtos file]\n", name);
	printf("      Try to boot the given RTOS image using the H3B v300 fixed-up HAL\n");
	printf("      You will likely want to use the patched H3B RTOS image that was\n");
	printf("      generated by prepare-bootstrap. An unpatched RTOS section is probably\n");
	printf("      not going to boot correctly on an H3B camera\n");
	printf("\n\n");
	printf("  For Hero3+ Black Cameras:\n");
	printf("      %s --h3pb-rtos [rtos file]\n", name);
	printf("      %s --h3pb-linux\n", name);
	printf("      Like the above, but for Hero3+ Black cameras\n");
	printf("\n");
	printf("\n");
}

/* Return non-zero CAMTYPE_* value on correct options */
int get_camera_option(int argc, char ** argv)
{
	if (argc <= 1)
		return CAMTYPE_UNKNOWN;

	if (argc == 2 && strcmp(argv[1], "--bootloader") == 0)
		return CAMTYPE_H2;

	if (argc == 2 && strcmp(argv[1], "--linux") == 0)
		return CAMTYPE_H2;

	if (argc == 3 && strcmp(argv[1], "--rtos") == 0)
		return CAMTYPE_H2;

	if (argc == 2 && strcmp(argv[1], "--h3b-linux") == 0)
		return CAMTYPE_H3B;

	if (argc == 3 && strcmp(argv[1], "--h3b-rtos") == 0)
		return CAMTYPE_H3B;

	if (argc == 3 && strcmp(argv[1], "--h3b-bld") == 0)
		return CAMTYPE_H3B;

	if (argc == 2 && strcmp(argv[1], "--h3pb-linux") == 0)
		return CAMTYPE_H3PB;

	if (argc == 3 && strcmp(argv[1], "--h3pb-rtos") == 0)
		return CAMTYPE_H3PB;
	
	if (argc == 2 && strcmp(argv[1], "--hero4-ddr-test") == 0)
		return CAMTYPE_H4;

	return -1;
}

int main(int argc, char **argv)
{
	int ret, i, cam_type;
	libusb_device_handle *usb_dev;
	printf("\nevilwombat's gopro boot thingy v0.08\n\n");
	printf("MAKE SURE YOU HAVE READ THE INSTRUCTIONS!\n");
	printf("The author makes absolutely NO GUARANTEES of the correctness of this program\n");
	printf("and takes absolutely NO RESPONSIBILITY OR LIABILITY for any consequences that\n");
	printf("arise from its use. This program could SEVERELY mess up your camera, totally\n");
	printf("destroy it, cause it to catch fire. It could also destroy your computer, burn\n");
	printf("down your house, etc. The author takes no responsibility for the consequences\n");
	printf("of using this program. Use it at your own risk! You have been warned.\n");
	printf("\n");

	cam_type = get_camera_option(argc, argv);
	
	if (cam_type == CAMTYPE_UNKNOWN) {
		print_usage(argv[0]);
		return -1;
	}
	
	printf("Initializing libusb\n");
	ret = libusb_init(NULL);

	if (ret) {
		printf("Error initializing libusb: %d\n", ret);
		return ret;
	}

	if (cam_type == CAMTYPE_H2)
		usb_dev = libusb_open_device_with_vid_pid(NULL, 0x4255, 0x0001);
	else if (cam_type == CAMTYPE_H4)
		usb_dev = libusb_open_device_with_vid_pid(NULL, 0x4255, 0x0009);
	else
		usb_dev = libusb_open_device_with_vid_pid(NULL, 0x4255, 0x0003);
	
	if (!usb_dev) {
		printf("Could not find the camera USB device.\n");
		printf(" - Is the camera plugged in? Is it in USB command mode?\n");
		printf(" - If you are using Linux, do you have permissions to access the USB device?\n");
		return -1;
	}
	
	ret = gp_init_interface(usb_dev);
	if (ret) {
		printf("Could not initialize USB interface: %d\n", ret);
		libusb_close(usb_dev);
		return -1;
	}

	if (cam_type == CAMTYPE_H2)
		ret = gp_init_ddr(usb_dev, hero2_alt_ddr_init_seq);

	if (cam_type == CAMTYPE_H3B)
		ret = gp_init_ddr(usb_dev, hero3black_ddr_init_seq);

	if (cam_type == CAMTYPE_H3PB)
		ret = gp_init_ddr(usb_dev, hero3plusblack_ddr_init_seq);

	if (cam_type == CAMTYPE_H4)
		ret = gp_init_ddr(usb_dev, hero4_ddr_init_seq);


	if (ret) {
		printf("Could not initialize DDR: %d\n", ret);
		return -1;
	}

	ret = gp_test_ddr(usb_dev);
	if (ret) {
		printf("DDR test failed: %d\n", ret);
		return -1;
	}

	if (strcmp(argv[1], "--bootloader") == 0) {
		printf("Okay, loading and booting the BLD bootloader on a Hero2 camera\n");
		gp_boot_bld(usb_dev);
	} else if (strcmp(argv[1], "--linux") == 0) {
		printf("Okay, loading and booting Linux on a Hero2 camera\n");
		gp_boot_linux(usb_dev);
	} else if (strcmp(argv[1], "--rtos") == 0) {
		printf("Okay, loading and booting RTOS image %s on a Hero2 camera\n", argv[2]);
		gp_boot_rtos(usb_dev, argv[2]);
	} else if (strcmp(argv[1], "--h3b-linux") == 0) {
		printf("Okay, loading and booting Linux on a Hero3 Black camera\n");
		gp_h3b_boot_linux(usb_dev, "h3b-v300-hal-reloc.bin");
	} else if (strcmp(argv[1], "--h3b-rtos") == 0) {
		printf("Okay, loading and booting RTOS image %s on a Hero3 Black camera\n", argv[2]);
		gp_h3b_boot_rtos(usb_dev,  "h3b-v300-hal-reloc.bin", argv[2]);
	} else if (strcmp(argv[1], "--h3b-bld") == 0) {
		printf("Okay, loading and booting BLD image %s on a Hero3 Black camera\n", argv[2]);
		gp_h3b_boot_bld(usb_dev,  "h3b-v300-hal-reloc.bin", argv[2]);
	} else if (strcmp(argv[1], "--h3pb-linux") == 0) {
		printf("Okay, loading and booting Linux on a Hero3+ Black camera\n");
		gp_h3b_boot_linux(usb_dev, "h3pb-v200-hal-reloc.bin");
	} else if (strcmp(argv[1], "--h3pb-rtos") == 0) {
		printf("Okay, loading and booting RTOS image %s on a Hero3+ Black camera\n", argv[2]);
		gp_h3b_boot_rtos(usb_dev,  "h3pb-v200-hal-reloc.bin", argv[2]);
	} else {
		print_usage(argv[0]);
		return -1;
	}
	
	libusb_close(usb_dev);
	return 0;
}
