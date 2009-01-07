/*
 * (C) Copyright 2007 Michal Simek
 * (C) Copyright 2004 Atmark Techno, Inc.
 *
 * Michal  SIMEK <monstr@monstr.eu>
 * Yasushi SHOJI <yashi@atmark-techno.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <image.h>
#include <zlib.h>
#include <asm/byteorder.h>

#define DEBUG

DECLARE_GLOBAL_DATA_PTR;

int do_bootm_linux(int flag, int argc, char *argv[], bootm_headers_t *images)
{
	/* First parameter is mapped to $r5 for kernel boot args */
	void	(*theKernel) (char *, ulong, ulong);
	char	*commandline = getenv ("bootargs");
	ulong	rd_data_start, rd_data_end;
	char	*of_flat_tree = NULL;
	int	ret;

	ulong	ep = 0;
	ulong	of_size = 0;

	ret = boot_get_fdt (flag, argc, argv, images,
					&of_flat_tree, &of_size);
	if (ret)
		goto error;
	
	if ((flag != 0) && (flag != BOOTM_STATE_OS_GO))
		return 1;

	theKernel = (void (*)(char *, ulong, ulong))images->ep;

	/* find ramdisk */
	ret = boot_get_ramdisk (argc, argv, images, IH_ARCH_MICROBLAZE,
			&rd_data_start, &rd_data_end);
	if (ret)
		goto error;

	show_boot_progress (15);

#ifdef DEBUG
	printf ("## Transferring control to Linux (at address %08lx), 0x%08x " \
				"ramdisk 0x%08x, FDT 0x%08x...\n",
		(ulong) theKernel, ep, rd_data_start, (ulong) of_flat_tree);
#endif

	theKernel (commandline, rd_data_start, (ulong) of_flat_tree);

	/* does not return */

error:
	puts ("error\n");
	return 1;
}
