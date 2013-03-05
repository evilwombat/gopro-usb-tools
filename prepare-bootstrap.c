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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hal-patch-v222.h"

void print_version_message(const char *name)
{
	printf("\n\n");
	printf("This does not look like the v222 HD2-firmware.bin update file.\n");
	printf("You MUST run %s on the v222 Hero2 firmware update file,\n", name);
	printf("regardless of what firmware you are actually trying to load.\n");
	printf("\n");
}

int save_section(FILE *fd, const char *output_name, int length, struct patch_entry *pat, int pat_len)
{
	FILE *ofd;
	int i, j;
	char t;
	ofd = fopen(output_name, "wb+");

	if (!ofd) {
		printf("Could not write to %s\n", output_name);
		return -1;
	}

	for (i = 0; i < length; i++) {
		fread(&t, 1, 1, fd);

		if (pat && pat_len != 0)
			for (j = 0; j < pat_len; j++)
				if (pat[j].offset == i)
					t = pat[j].val;
		
		fwrite(&t, 1, 1, ofd);
	}
	fclose(ofd);
}

#define BLD_NAME	"v222-bld.bin"
#define BLD_START	8448
#define BLD_SIZE	162496

#define HAL_NAME	"v222-hal-reloc.bin"
#define HAL_START	172288
#define HAL_SIZE	55724

#define ARRAY_SIZE(v)  (sizeof(v) / sizeof(*(v)))

int main(int argc, char **argv)
{
	int ret = 0;
	char *fname;
	FILE *fd;
	struct stat st;

	printf("evilwombat's gopro bootstrap fwcutter tool v0.01\n");
	
	if (argc != 2) {
		printf("Usage: %s [HD2-firmware.bin from the *v222* update]\n", argv[0]);
		return -1;
	}
	
	fname = argv[1];

	ret = stat(fname, &st);
	if (ret) {
		printf("Error: Could not stat %s\n", fname);
		return -1;
	}

	if (st.st_size != 52606976) {
		print_version_message(argv[0]);
		return -1;
	}

	fd = fopen(fname, "rb");
	if (!fd) {
		printf("Could not open %s\n", fname);
		return -1;
	}

	printf("Creating %s\n", BLD_NAME);
	fseek(fd, BLD_START, SEEK_SET);
	ret = save_section(fd, BLD_NAME, BLD_SIZE, NULL, 0);

	if (ret) {
		printf("Could not save %s\n", BLD_NAME);
		fclose(fd);
		return -1;
	}

	printf("Creating %s\n", HAL_NAME);
	fseek(fd, HAL_START, SEEK_SET);
	ret = save_section(fd, HAL_NAME, HAL_SIZE, hal_patch_v222, ARRAY_SIZE(hal_patch_v222));

	if (ret) {
		printf("Could not save %s\n", HAL_NAME);
		fclose(fd);
		return -1;
	}
	
	printf("Done.\n");
	fclose(fd);
	return 0;
}
