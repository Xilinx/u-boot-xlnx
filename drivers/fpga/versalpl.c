// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2019, Xilinx, Inc,
 * Siva Durga Prasad Paladugu <siva.durga.prasad.paladugu@amd.com>>
 */

#include <cpu_func.h>
#include <log.h>
#include <memalign.h>
#include <versalpl.h>
#include <zynqmp_firmware.h>
#include <asm/cache.h>
#include <linux/errno.h>

static ulong versal_align_dma_buffer(ulong *buf, u32 len)
{
	ulong *new_buf;

	if ((ulong)buf != ALIGN((ulong)buf, ARCH_DMA_MINALIGN)) {
		new_buf = (ulong *)ALIGN((ulong)buf, ARCH_DMA_MINALIGN);
		memmove(new_buf, buf, len);
		buf = new_buf;
	}

	return (ulong)buf;
}

__weak int zynqmp_pm_fpga_load(u64 address)
{
	return -ENOSYS;
}

static int versal_load(xilinx_desc *desc, const void *buf, size_t bsize,
		       bitstream_type bstype, int flags)
{
	ulong bin_buf;
	int ret;

	bin_buf = versal_align_dma_buffer((ulong *)buf, bsize);

	debug("%s called!\n", __func__);
	flush_dcache_range(bin_buf, bin_buf + bsize);

	ret = zynqmp_pm_fpga_load(bin_buf);
	if (ret)
		printf("PL FPGA LOAD failed with err: 0x%08x\n", ret);

	return ret;
}

struct xilinx_fpga_op versal_op = {
	.load = versal_load,
};
