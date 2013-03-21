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

/*
 * Memory controller initializiation sequence commands. These commands are
 * processed only by gp_init_ddr(), which translates them into a series of write
 * commands
 */
#define SEQ_WRITE	1	/* Write given value to given register	*/
#define SEQ_DELAY	2	/* Wait 10mS				*/
#define SEQ_DONE	3	/* End of init sequence			*/

struct gp_ddr_cmd {
	uint32_t cmd;
	uint32_t addr;
	uint32_t val;
};

/*
 * Existing memory controller init sequence, as defined in gp_ddr.c
  */
extern struct gp_ddr_cmd hero2_ddr_init_seq[];
extern struct gp_ddr_cmd hero2_alt_ddr_init_seq[];

/*
 * int gp_init_interface(libusb_device_handle *dev)
 * 
 * 	Initialize camera USB device after it's been found using
 * 	libusb_open_device_with_vid_pid.
 *
 * 	Returns 0 on success.
 */
int gp_init_interface(libusb_device_handle *dev);

/*
 * uint32_t gp_read_reg(libusb_device_handle *dev, uint32_t addr)
 *
 *	Read a camera register or memory location at the given address.
 *
 * 	Returns 0 and prints error on failure.
 */
uint32_t gp_read_reg(libusb_device_handle *dev, uint32_t addr);

/*
 * int gp_write_reg(libusb_device_handle *dev, uint32_t addr, uint32_t val)
 *
 *	Write a given 32-bit value to a camera register or memory location
 *	at the given physical address.
 *
 * 	Returns 0 on success.
 */
int gp_write_reg(libusb_device_handle *dev, uint32_t addr, uint32_t val);

/*
 * int gp_exec(libusb_device_handle *dev, uint32_t addr)
 *
 *	Tell camera to exit USB command mode and to jump to the given physical
 * 	address. The camera will not accept further commands from us until it
 * 	has been reset.
 *
 *	Returns 0 on success.
 */
int gp_exec(libusb_device_handle *dev, uint32_t addr);

/*
 * int gp_init_ddr(libusb_device_handle *dev, struct gp_ddr_cmd *seq)
 *
 * 	Initialize the camera's memory controller, so that reads/writes to RAM
 * 	(at address 0xc0000000 - 0xcfffffff) become possible.
 *
 * 	seq - memory controller initializiation sequence to use
 *
 * 	Returns 0 on success
 */
int gp_init_ddr(libusb_device_handle *dev, struct gp_ddr_cmd *seq);

/*
 * int gp_test_ddr(libusb_device_handle *dev)
 *
 *	Perform a very basic test of the memory controller.
 *
 *	Returns 0 if test passes or non-zero if it fails 
 */ 
int gp_test_ddr(libusb_device_handle *dev);

/*
 * int gp_write_byte(libusb_device_handle *dev, uint32_t addr, unsigned char val)
 *
 *	Write a single byte to a given (non-word-aligned) physical address using
 * 	a series of gp_read_reg and gp_write_reg calls to perform a
 *	read-modify-write operation on the given word.
 *
 * 	Returns 0 on success.
 */
int gp_write_byte(libusb_device_handle *dev, uint32_t addr, unsigned char val);

/*
 * int gp_write_string(libusb_device_handle *dev, uint32_t addr, const char *str)
 *
 *	Write a given ASCII string at a given (non-word-aligned) physical
 * 	address, including the trailing \0 character, using a series of
 * 	gp_write_byte calls.
 *
 * 	Returns 0 on success.
 */
int gp_write_string(libusb_device_handle *dev, uint32_t addr, const char *str);

/*
 * int gp_load_file(libusb_device_handle *dev, const char *name, uint32_t addr)
 *
 *	Load the given file into the camera's memory, at the given physical
 * 	address.
 *
 * 	The target physical address *MUST* be 4-byte-aligned (word-aligned).
 * 	The file length does NOT have to be a multiple of 4 bytes.
 *
 * 	Returns 0 on success.
 */
int gp_load_file(libusb_device_handle *dev, const char *name, uint32_t addr);
