/*
 * SPI testing utility (using spidev driver)
 *
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 *
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include "tux.c"

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spidev0.0";
static uint8_t mode = 3;
static uint8_t bits = 9;
static uint32_t speed = 10000000;

static int transfer(int fd, uint16_t word)
{
	struct spi_ioc_transfer transfer = {
		.tx_buf = (unsigned long)&word,
		.rx_buf = (unsigned long)NULL,
		.len = 2,
		.speed_hz = 0,
		.bits_per_word = 9,
	};

	return ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd, i, j, k = 0, t = 0, row, col;

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	transfer(fd, 0x21);
	transfer(fd, 0x90);
	transfer(fd, 0x20);
	transfer(fd, 0x0c);
	transfer(fd, 0x8e);

	for (col = 0; col < 8; col++) {
		transfer(fd, 0x40 | col);
		transfer(fd, 0x8e);

		for (row = 0; row < 75; row++) {
			t = 0;
			for (i = 0; i < 8; i++)
				t = (t << 1) | ((tux_image.pixel_data[(row * 64 + col * 8 + i) * 4]) ? 0 : 1);

			transfer(fd, 0x100 | t);
		}
	}
	close(fd);
	return ret;
}