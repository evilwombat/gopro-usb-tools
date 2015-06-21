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
#include "fw-patch.h"

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

#define ARRAY_SIZE(v)  (sizeof(v) / sizeof(*(v)))

struct section_type {
	const char *filename;
	unsigned int start;
	unsigned int size;
	struct patch_entry *patch;
	unsigned int patch_size;
};

struct fw_type {
	const char *name;
	unsigned int size;
	struct section_type *sections;
};

struct section_type hero2_v312_sections[] = {
	{
		.filename = "v312-bld.bin",
		.start = 8448,
		.size = 163216,
	},
	{
		.filename = "v312-hal-reloc.bin",
		.start = 172288,
		.size = 55724,
		.patch = hal_patch_v312,
		.patch_size = ARRAY_SIZE(hal_patch_v312),
	},
	{ }
};

struct section_type h3b_v300_sections[] = {
	{
		.filename = "h3b-v300-bld.bin",
		.start = 6400,
		.size = 154916,
	},
	{
		.filename = "h3b-v300-bld-uart.bin",
		.start = 6400,
		.size = 154916,
		.patch = h3b_v300_bld_uart_patch,
		.patch_size = ARRAY_SIZE(h3b_v300_bld_uart_patch),
	},
	{
		.filename = "h3b-v300-hal-reloc.bin",
		.start = 162048,
		.size = 48856,
		.patch = h3b_v300_hal_patch,
		.patch_size = ARRAY_SIZE(h3b_v300_hal_patch),
	},
	{
		.filename = "h3b-v300-rtos-patched.bin",
		.start = 211200,
		.size = 7364612,
		.patch = h3b_v300_rtos_patch,
		.patch_size = ARRAY_SIZE(h3b_v300_rtos_patch),
	},
	{ },
};

struct section_type h3pb_v104_sections[] = {
	{
		.filename = "h3pb-v104-bld.bin",
		.start = 102656,
		.size = 162432,
	},
	{
		.filename = "h3pb-v104-hal-reloc.bin",
		.start = 266496,
		.size = 48856,
		.patch = h3pb_v104_hal_patch,
		.patch_size = ARRAY_SIZE(h3pb_v104_hal_patch),
	},
	{
		.filename = "h3pb-v104-rtos-patched.bin",
		.start = 315648,
		.size = 8085508,
		.patch = h3pb_v104_rtos_patch,
		.patch_size = ARRAY_SIZE(h3pb_v104_rtos_patch),
	},
	{ },
};

struct section_type h3pb_v200_sections[] = {
	{
		.filename = "h3pb-v200-bld.bin",
		.start = 102656,
		.size = 162656,
	},
	{
		.filename = "h3pb-v200-hal-reloc.bin",
		.start = 266496,
		.size = 47356,
		.patch = h3pb_v200_hal_patch,
		.patch_size = ARRAY_SIZE(h3pb_v200_hal_patch),
	},
	{
		.filename = "h3pb-v200-rtos-patched.bin",
		.start = 315648,
		.size = 8241156,
		.patch = h3pb_v200_rtos_patch,
		.patch_size = ARRAY_SIZE(h3pb_v200_rtos_patch),
	},
	{ },
};

struct fw_type fw_list[] =
{
	{
		.name = "Hero2 v312 Firmware",
		.size = 52850688,
		.sections = hero2_v312_sections,
	},
	{
		.name = "Hero3 (Black) v3.00 Firmware",
		.size = 41054208,
		.sections = h3b_v300_sections,
	},
	{
		.name = "Hero3 Plus (Black) v1.04 Firmware",
		.size = 44294132,
		.sections = h3pb_v104_sections,
	},

	{
		.name = "Hero3 Plus (Black) v2.00 Firmware",
		.size = 44439548,
		.sections = h3pb_v200_sections,
	},

	{ }
};

void print_version_message(const char *name)
{
	struct fw_type *fw = fw_list;
	printf("\n");
	printf("This does not look like a supported firmware file.\n");
	printf("You MUST run %s on one of the supported firmware update files,\n", name);
	printf("regardless of what firmware you are actually trying to load. Please\n");
	printf("read the instructions that led you here.\n\n");
	printf("Supported firmware files:\n");
	while(fw->name) {
		printf("  * %s\n", fw->name);
		fw++;
	}
	printf("\n");
}

int main(int argc, char **argv)
{
	int ret = 0;
	char *fname;
	struct fw_type *fw = fw_list;
	struct section_type *section;
	FILE *fd;
	struct stat st;

	printf("evilwombat's gopro bootstrap fwcutter tool v0.05\n\n");
	
	if (argc != 2) {
		printf("Usage: %s [HD2-firmware.bin from the *v312* hero2 update or HD3.03-firmware.bin for the H3 Black or HD3.11-firmware.bin for the H3+ Black]\n", argv[0]);
		return -1;
	}
	
	fname = argv[1];

	ret = stat(fname, &st);
	if (ret) {
		printf("Error: Could not stat %s\n", fname);
		return -1;
	}

	while (fw->name) {
		if (fw->size == st.st_size)
			break;
		fw++;
	}

	if (fw->name == NULL) {
		print_version_message(argv[0]);
		return -1;
	}

	printf("Detected firmware file type: \"%s\"\n", fw->name);

	fd = fopen(fname, "rb");
	if (!fd) {
		printf("Could not open %s\n", fname);
		return -1;
	}

	section = fw->sections;

	while (section->filename) {
		printf("Creating %s\n", section->filename);
		fseek(fd, section->start, SEEK_SET);
		ret = save_section(fd, section->filename, section->size, section->patch, section->patch_size);

		if (ret) {
			printf("Could not save %s\n", section->filename);
			fclose(fd);
			return -1;
		}
		section++;
	}
	
	printf("Done.\n");
	fclose(fd);
	return 0;
}
