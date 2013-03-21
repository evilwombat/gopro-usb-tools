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

/* We assume the entry point for our kernel will be 0xc3000000 */
int gp_load_linux(libusb_device_handle *dev, const char *kernel,
		  const char *initrd, const char *cmdline)
{
	char cmdline_buf[CMDLINE_LEN];
	int cmd_len = strlen(cmdline);
	uint32_t initrd_base = 0xc7000000;
	int initrd_size, cmd_tag_len, ret;
	struct stat st;

	if (initrd)
		cmd_len += 25;

	if (cmd_len > CMDLINE_LEN) {
		printf("Kernel command line too long.\n");
		return -1;
	}

	gp_load_file(dev, kernel, 0xc3008000);

	if (initrd) {
		ret = stat(initrd, &st);
		if (ret) {
			printf("Could not get initrd size: %d\n", ret);
			return ret;
		}

		initrd_size = (int) st.st_size;
		gp_load_file(dev, initrd, 0xc7000000);
	}
	
	printf("Patching in some init code...\n");
	gp_write_reg(dev, 0xc3000000, 0xe3a01e40);	// mov r1, 0x400
	gp_write_reg(dev, 0xc3000004, 0xe38110c7);	// orr r1, r1, 0x0c7	; machine type
	gp_write_reg(dev, 0xc3000008, 0xe3a024c3);	// mov r2, 0xc3000000
	gp_write_reg(dev, 0xc300000C, 0xe3822020);	// orr r2, r2, 0x20	; atags at c3000020
	gp_write_reg(dev, 0xc3000010, 0xea001ffa);	// b 0xc3008000	; kernel start

	if (initrd) {
		snprintf(cmdline_buf, CMDLINE_LEN, "%s initrd=0x%08x,0x%08x", cmdline, initrd_base, initrd_size);
	} else
		snprintf(cmdline_buf, CMDLINE_LEN, "%s", cmdline);
	
	cmd_len = strlen(cmdline_buf);
	cmd_tag_len = 2 + (cmd_len + 3) / 4;
	gp_write_reg(dev, 0xc3000020, 0x00000002);	// core
	gp_write_reg(dev, 0xc3000024, 0x54410001);
	gp_write_reg(dev, 0xc3000028, cmd_tag_len);
	gp_write_reg(dev, 0xc300002c, 0x54410009);
	gp_write_string(dev, 0xc3000030, cmdline_buf);

	gp_write_reg(dev, 0xc3000028 + cmd_tag_len * 4 + 0x00, 0x00000000);
  	gp_write_reg(dev, 0xc3000028 + cmd_tag_len * 4 + 0x04, 0x00000000);	// end of tag list

	return 0;
}

int gp_boot_linux(libusb_device_handle *dev)
{
	int ret = 0;
        ret = gp_load_file(dev, "v222-bld.bin", 0xc0000000);
	if (ret) {
		printf("Could not load v222-bld.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	ret = gp_load_file(dev, "v222-hal-reloc.bin", 0xc00a0000);
	if (ret) {
		printf("Could not load v222-hal-reloc.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	printf("Patching in a vector to 0xc3000000...\n");
	gp_write_reg(dev, 0xc00024c4, 0xe3a0f4C3);	// Jump to 0xc3000000

	gp_load_linux(dev, "zImage", "initrd.lzma",
		      "mem=200M@0xc3000000 console=ttyS0,115200n8 root=/dev/ram0 init=/bin/sh ");

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
        ret = gp_load_file(dev, "v222-bld.bin", 0xc0000000);
	if (ret) {
		printf("Could not load v222-bld.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	ret = gp_load_file(dev, "v222-hal-reloc.bin", 0xc00a0000);
	if (ret) {
		printf("Could not load v222-hal-reloc.bin - did you run prepare-boostrap?\n");
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
        ret = gp_load_file(dev, "v222-bld.bin", 0xc0000000);
	if (ret) {
		printf("Could not load v222-bld.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	ret = gp_load_file(dev, "relocate.bin", 0xc7000000);
	if (ret) {
		printf("Could not load relocate.bin\n");
		return -1;
	}
	
	ret = gp_load_file(dev, "v222-hal-reloc.bin", 0xc8000000);
	if (ret) {
		printf("Could not load v222-hal-reloc.bin - did you run prepare-boostrap?\n");
		return -1;
	}

	ret = gp_load_file(dev, rtos_file, 0xc9000000);    /* v124 section_3 or v222 section_9 */
	if (ret) {
		printf("Could not load RTOS file %s\n", rtos_file);
		printf("This should be section_3 from the v124 firmware, or section_9 from\n");
		printf("the v198 / v222 firmware, depending on what you are doing.\n");
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

void print_usage(const char *name)
{
	printf("Usage:\n");
	printf("      %s --bootloader\n", name);
	printf("      Load v222 BLD and fixed-up HAL, and jump to BLD\n");
	printf("\n");
	printf("      %s --rtos [rtos_section]\n", name);
	printf("      Try to boot the given RTOS image using the v222 fixed-up HAL\n");
	printf("      In fw v124, this is section_3\n");
	printf("      In fw v198 and v222, this is section_9 or section_3\n");
	printf("\n");
	printf("      %s --linux\n", name);
	printf("      Boot a Linux kernel. If this works, the camera should show up as\n");
	printf("      a USB Ethernet device and you should be able to use it to telnet\n");
	printf("      to it at 10.9.9.1\n");
	printf("\n");
}

/* Return 0 on 'looks correct' */
int check_usage(int argc, char ** argv)
{
	if (argc <= 1)
		return -1;

	if (argc == 2 && strcmp(argv[1], "--bootloader") == 0)
		return 0;

	if (argc == 2 && strcmp(argv[1], "--linux") == 0)
		return 0;

	if (argc == 3 && strcmp(argv[1], "--rtos") == 0)
		return 0;
	
	return -1;
}

int main(int argc, char **argv)
{
	int ret, i;
	libusb_device_handle *usb_dev;
	printf("\nevilwombat's gopro boot thingy v0.02 (alt ddr)\n\n");
	printf("MAKE SURE YOU HAVE READ THE INSTRUCTIONS!\n");
	printf("The author makes absolutely NO GUARANTEES of the correctness of this program\n");
	printf("and takes absolutely NO RESPONSIBILITY OR LIABILITY for any consequences that\n");
	printf("arise from its use. This program could SEVERELY mess up your camera, totally\n");
	printf("destroy it, cause it to catch fire. It could also destroy your computer, burn\n");
	printf("down your house, etc. The author takes no responsibility for the consequences\n");
	printf("of using this program. Use it at your own risk! You have been warned.\n");
	printf("\n");
	
	if (check_usage(argc, argv)) {
		print_usage(argv[0]);
		return -1;
	}
	
	printf("Initializing libusb\n");
	ret = libusb_init(NULL);

	if (ret) {
		printf("Error initializing libusb: %d\n", ret);
		return ret;
	}

	usb_dev = libusb_open_device_with_vid_pid(NULL, 0x4255, 0x0001);
	
	if (!usb_dev) {
		printf("Could not the camera USB device.\n");
		printf(" - Do you have permissions to access the USB device?\n");
		printf(" - Is the camera plugged in? Is it in USB command mode?\n");
		return -1;
	}
	
	ret = gp_init_interface(usb_dev);
	if (ret) {
		printf("Could not initialize USB interface: %d\n", ret);
		libusb_close(usb_dev);
		return -1;
	}

	ret = gp_init_ddr(usb_dev, hero2_alt_ddr_init_seq);
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
		printf("Okay, loading and booting the BLD bootloader\n");
		gp_boot_bld(usb_dev);
	} else if (strcmp(argv[1], "--linux") == 0) {
		printf("Okay, loading and booting Linux\n");
		gp_boot_linux(usb_dev);
	} else if (strcmp(argv[1], "--rtos") == 0) {
		if (argc != 3) {
			print_usage(argv[0]);
			libusb_close(usb_dev);
			return -1;
		}
		
		printf("Okay, loading and booting RTOS image %s\n", argv[2]);
		gp_boot_rtos(usb_dev, argv[2]);
	} else {
		print_usage(argv[0]);
		return -1;
	}
	
	libusb_close(usb_dev);
	return 0;
}
