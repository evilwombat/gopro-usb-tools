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

/*
 * See gp_api.h for API documentation
 */

/*
	#define DEBUG
*/

int gp_init_interface(libusb_device_handle *dev)
{
	int ret;
	ret = libusb_reset_device(dev);

	if (ret) {
		printf("Could not reset device: %d\n", ret);
		return ret;
	}

	ret = libusb_set_configuration(dev, 0);

	if (ret)
		printf("Could not set device configuration: %d. Continuing anyway.\n", ret);

/*	
	// I don't think we need this but "it seems like a good idea"
	ret = libusb_set_interface_alt_setting(dev, 0, 0);
	if (ret) {
		printf("Set interface alt ret = %d. Continuing anyway.\n", ret);
	}

	// I don't think we need this but "it seems like a good idea"...
	ret = libusb_claim_interface(dev, 0);
	if (ret) {
		printf("Claim interface ret = %d. Continuing anyway.\n", ret);
	}
*/

	return 0;
}

uint32_t gp_read_reg(libusb_device_handle *dev, uint32_t addr)
{
	unsigned char buf[4];
	int ret;
	uint32_t val = 0;

	if (addr & 3) {
		printf("Error: Address %08x not aligned to word boundary\n", addr);
		return -1;
	}
	
	ret = libusb_control_transfer(dev, 0xC0, 0, addr >> 16, addr & 0xFFFF, buf, 4, 1000);

	if (ret != 4) {
		printf("Read of register %08x failed: %d\n", addr, ret);
		return 0;
	}

	val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

#ifdef DEBUG
	printf("Read %08x: %08x\n", addr, val);
#endif

	return val;
}

int gp_write_reg(libusb_device_handle *dev, uint32_t addr, uint32_t val)
{
	unsigned char buf[12];
	int ret;

	if (addr & 3) {
		printf("Error: Address %08x not aligned to word boundary\n", addr);
		return -1;
	}
	
	buf[0] = 0x00;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x30;

	buf[4] = addr;
	buf[5] = addr >> 8;
	buf[6] = addr >> 16;
	buf[7] = addr >> 24;

	buf[8] = val;
	buf[9] = val >> 8;
	buf[10] = val >> 16;
	buf[11] = val >> 24;

	ret = libusb_control_transfer(dev, 0x40, 0, 0, 0, buf, 12, 1000);

#ifdef DEBUG
	printf("Write %08x to %08x: %d (%s)\n", val, addr, ret, ret == 12 ? "ok" : "hmmm...");
#endif

	if (ret != 12) {
		printf("Write %08x to %08x failed: %d\n", val, addr, ret);
		return ret;
	}

	return 0;
}

int gp_exec(libusb_device_handle *dev, uint32_t addr)
{
	unsigned char buf[12];
	int ret;

	buf[0] = 0x00;
	buf[1] = 0x00;
	buf[2] = 0x01;
	buf[3] = 0x00;

	buf[4] = addr;
	buf[5] = addr >> 8;
	buf[6] = addr >> 16;
	buf[7] = addr >> 24;

	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x00;

	ret = libusb_control_transfer(dev, 0x40, 0, 0, 0, buf, 12, 1000);

	printf("Exec %08x: %d (%s)\n", addr, ret, ret == 12 ? "ok" : "hmmm...");

	if (ret != 12) {
		printf("Exec %08x failed: %d\n", addr, ret);
		return ret;
	}

	return 0;
}

int gp_init_ddr(libusb_device_handle *dev, struct gp_ddr_cmd *seq)
{
	int i = 0, ret;
	printf("Initializing DDR");
	while (seq[i].cmd != SEQ_DONE) {
		if (seq[i].cmd == SEQ_WRITE) {
			ret = gp_write_reg(dev, seq[i].addr, seq[i].val);
			if (ret) {
				printf("DDR init write failed: %d\n", ret);
				return ret;
			}
			printf(".");
			fflush(stdout);
		}

		if (seq[i].cmd == SEQ_DELAY)
			usleep(10000);
		i++;
	}
	printf(" done\n");
	return 0;
}

int gp_test_ddr(libusb_device_handle *dev)
{
	uint32_t tmp, addr, expect, magic1 = 3126727440U, magic2 = 13834356;
	int ret, test_count = 1000;
	unsigned int i;

	printf("Testing DDR...");

	for (i = 0; i < test_count; i++) {
		addr = 0xc0000000 + i * 4;
		expect = ((i * magic2) ^ magic1) * i;
		ret = gp_write_reg(dev, addr, expect);
		if (ret) {
			printf(" FAILED\n");
			printf("Could not write to DDR offset %08x: %d\n", addr, ret);
			return -1;
		}
		if (i % 100 == 0) {
			printf(".");
			fflush(stdout);
		}
	}

	for (i = 0; i < test_count; i++) {
		addr = 0xc0000000 + i * 4;
		expect = ((i * magic2) ^ magic1) * i;
		tmp = gp_read_reg(dev, addr);

		if (tmp != expect) {
			printf(" FAILED\n");
			printf("Mismatch at DDR offset %08x: expected %08x, got %08x\n",
			       addr, expect, tmp);
			return -1;
		}

		if (i % 100 == 0) {
			printf(".");
			fflush(stdout);
		}
	}

	printf(" passed\n");
	return 0;
}

int gp_write_byte(libusb_device_handle *dev, uint32_t addr, unsigned char val)
{
	uint32_t tmp, mask;
	uint32_t word = addr & ~(0x03);
	int offset = addr & 0x03;

	tmp = gp_read_reg(dev, word);

	mask = 0xff << (offset * 8);
	tmp &= ~mask;
	tmp |= val << (offset * 8);

	return gp_write_reg(dev, word, tmp);
}

int gp_write_string(libusb_device_handle *dev, uint32_t addr, const char *str)
{
	int i, len;
	len = strlen(str);

	for (i = 0; i <= len; i++)	/* Include trailing \0 */
		gp_write_byte(dev, addr++, str[i]);
}

int gp_load_file(libusb_device_handle *dev, const char *name, uint32_t addr)
{
	struct stat st;
	int i, ret;
	unsigned char c;
	uint32_t v = 0;
	int size;

	ret = stat(name, &st);

	if (ret) {
		printf("Could not get size of %s\n", name);
		return ret;
	}

	size = (int)st.st_size;

	FILE *fd = fopen(name, "rb");
	if (!fd) {
		fprintf(stderr, "Could not open %s\n", name);
		return -1;
	}

	printf("Loading %s to address %08x\n", name, addr);

	for (i = 0; i < (size & 0xfffffffc); i++) {
		c = fgetc(fd);
		v = (v >> 8) | ((c & 0xff) << 24);
		if (i > 0 && (i & 3) == 3) {
			gp_write_reg(dev, addr, v);
			addr += 4;
			v = 0;
		}

		if (size > 100 && i % (st.st_size / 100) == 0) {
			printf("  %d / %d (%d%%)\r", i, size, (100 * i / size));
			fflush(stdout);
		}
	}

	for (i = 0; i < (size % 4); i++) {
		c = fgetc(fd);
		gp_write_byte(dev, addr + i, c);
	}

	fclose(fd);
	printf(" ... done                    \n");
	return 0;
}
