// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2015 - 2016, Xilinx, Inc,
 * Michal Simek <michal.simek@amd.com>
 * Siva Durga Prasad Paladugu <siva.durga.prasad.paladugu@amd.com>
 */

#include <console.h>
#include <compiler.h>
#include <cpu_func.h>
#include <fpga.h>
#include <log.h>
#include <zynqmppl.h>
#include <zynqmp_firmware.h>
#include <asm/cache.h>
#include <linux/bitops.h>
#include <linux/sizes.h>
#include <asm/arch/sys_proto.h>
#include <memalign.h>

#define DUMMY_WORD	0xffffffff

/* Xilinx binary format header */
static const u32 bin_format[] = {
	DUMMY_WORD, /* Dummy words */
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	DUMMY_WORD,
	0x000000bb, /* Sync word */
	0x11220044, /* Sync word */
	DUMMY_WORD,
	DUMMY_WORD,
	0xaa995566, /* Sync word */
};

#define SWAP_NO		1
#define SWAP_DONE	2

/*
 * Load the whole word from unaligned buffer
 * Keep in your mind that it is byte loading on little-endian system
 */
static u32 load_word(const void *buf, u32 swap)
{
	u32 word = 0;
	u8 *bitc = (u8 *)buf;
	int p;

	if (swap == SWAP_NO) {
		for (p = 0; p < 4; p++) {
			word <<= 8;
			word |= bitc[p];
		}
	} else {
		for (p = 3; p >= 0; p--) {
			word <<= 8;
			word |= bitc[p];
		}
	}

	return word;
}

static u32 check_header(const void *buf)
{
	u32 i, pattern;
	int swap = SWAP_NO;
	u32 *test = (u32 *)buf;

	debug("%s: Let's check bitstream header\n", __func__);

	/* Checking that passing bin is not a bitstream */
	for (i = 0; i < ARRAY_SIZE(bin_format); i++) {
		pattern = load_word(&test[i], swap);

		/*
		 * Bitstreams in binary format are swapped
		 * compare to regular bistream.
		 * Do not swap dummy word but if swap is done assume
		 * that parsing buffer is binary format
		 */
		if ((__swab32(pattern) != DUMMY_WORD) &&
		    (__swab32(pattern) == bin_format[i])) {
			swap = SWAP_DONE;
			debug("%s: data swapped - let's swap\n", __func__);
		}

		debug("%s: %d/%px: pattern %x/%x bin_format\n", __func__, i,
		      &test[i], pattern, bin_format[i]);
	}
	debug("%s: Found bitstream header at %px %s swapinng\n", __func__,
	      buf, swap == SWAP_NO ? "without" : "with");

	return swap;
}

static void *check_data(u8 *buf, size_t bsize, u32 *swap)
{
	u32 word, p = 0; /* possition */

	/* Because buf doesn't need to be aligned let's read it by chars */
	for (p = 0; p < bsize; p++) {
		word = load_word(&buf[p], SWAP_NO);
		debug("%s: word %x %x/%px\n", __func__, word, p, &buf[p]);

		/* Find the first bitstream dummy word */
		if (word == DUMMY_WORD) {
			debug("%s: Found dummy word at position %x/%px\n",
			      __func__, p, &buf[p]);
			*swap = check_header(&buf[p]);
			if (*swap) {
				/* FIXME add full bitstream checking here */
				return &buf[p];
			}
		}
		/* Loop can be huge - support CTRL + C */
		if (ctrlc())
			return NULL;
	}
	return NULL;
}

static ulong zynqmp_align_dma_buffer(u32 *buf, u32 len, u32 swap)
{
	u32 *new_buf;
	u32 i;

	if ((ulong)buf != ALIGN((ulong)buf, ARCH_DMA_MINALIGN)) {
		new_buf = (u32 *)ALIGN((ulong)buf, ARCH_DMA_MINALIGN);

		/*
		 * This might be dangerous but permits to flash if
		 * ARCH_DMA_MINALIGN is greater than header size
		 */
		if (new_buf > (u32 *)buf) {
			debug("%s: Aligned buffer is after buffer start\n",
			      __func__);
			new_buf -= ARCH_DMA_MINALIGN;
		}
		printf("%s: Align buffer at %px to %px(swap %d)\n", __func__,
		       buf, new_buf, swap);

		for (i = 0; i < (len/4); i++)
			new_buf[i] = load_word(&buf[i], swap);

		buf = new_buf;
	} else if ((swap != SWAP_DONE) &&
		   (zynqmp_firmware_version() <= PMUFW_V1_0)) {
		/* For bitstream which are aligned */
		new_buf = buf;

		printf("%s: Bitstream is not swapped(%d) - swap it\n", __func__,
		       swap);

		for (i = 0; i < (len/4); i++)
			new_buf[i] = load_word(&buf[i], swap);
	}

	return (ulong)buf;
}

static int zynqmp_validate_bitstream(xilinx_desc *desc, const void *buf,
				   size_t bsize, u32 blocksize, u32 *swap)
{
	ulong *buf_start;
	ulong diff;

	buf_start = check_data((u8 *)buf, blocksize, swap);

	if (!buf_start)
		return FPGA_FAIL;

	/* Check if data is postpone from start */
	diff = (ulong)buf_start - (ulong)buf;
	if (diff) {
		printf("%s: Bitstream is not validated yet (diff %lx)\n",
		       __func__, diff);
		return FPGA_FAIL;
	}

	if ((ulong)buf < SZ_1M) {
		printf("%s: Bitstream has to be placed up to 1MB (%px)\n",
		       __func__, buf);
		return FPGA_FAIL;
	}

	return 0;
}

#if CONFIG_IS_ENABLED(FPGA_LOAD_SECURE)
static int zynqmp_check_compatible(xilinx_desc *desc, int flags)
{
	/*
	 * If no flags set, the image may be legacy, but we need to
	 * signal caller this situation with specific error code.
	 */
	if (!flags)
		return -ENODATA;

	/* For legacy bitstream images no need for other methods exist */
	if ((flags & desc->flags) && flags == FPGA_LEGACY)
		return 0;

	/*
	 * Other images are handled in secure callback loads(). Check
	 * callback existence besides image type support.
	 */
	if (desc->operations->loads && (flags & desc->flags))
		return 0;

	return -ENODEV;
}
#endif

static int zynqmp_load(xilinx_desc *desc, const void *buf, size_t bsize,
		     bitstream_type bstype, int flags)
{
	ALLOC_CACHE_ALIGN_BUFFER(u32, bsizeptr, 1);
	u32 swap = 0;
	ulong bin_buf;
	int ret;
	u32 buf_lo, buf_hi;
	u32 bsize_req = (u32)bsize;
	u32 ret_payload[PAYLOAD_ARG_CNT];
#if CONFIG_IS_ENABLED(FPGA_LOAD_SECURE)
	struct fpga_secure_info info = { 0 };

	ret = zynqmp_check_compatible(desc, flags);
	if (ret) {
		if (ret != -ENODATA) {
			puts("Missing loads() operation or unsupported bitstream type\n");
			return FPGA_FAIL;
		}
		/* If flags is not set, the image treats as legacy */
		flags = FPGA_LEGACY;
	}

	switch (flags) {
	case FPGA_LEGACY:
		break;	/* Handle the legacy image later in this function */
#if CONFIG_IS_ENABLED(FPGA_LOAD_SECURE)
	case FPGA_XILINX_ZYNQMP_DDRAUTH:
		/* DDR authentication */
		info.authflag = ZYNQMP_FPGA_AUTH_DDR;
		info.encflag = FPGA_NO_ENC_OR_NO_AUTH;
		return desc->operations->loads(desc, buf, bsize, &info);
	case FPGA_XILINX_ZYNQMP_ENC:
		/* Encryption using device key */
		info.authflag = FPGA_NO_ENC_OR_NO_AUTH;
		info.encflag = FPGA_ENC_DEV_KEY;
		return desc->operations->loads(desc, buf, bsize, &info);
#endif
	default:
		printf("Unsupported bitstream type %d\n", flags);
		return FPGA_FAIL;
	}
#endif

	if (zynqmp_firmware_version() <= PMUFW_V1_0) {
		puts("WARN: PMUFW v1.0 or less is detected\n");
		puts("WARN: Not all bitstream formats are supported\n");
		puts("WARN: Please upgrade PMUFW\n");
		if (zynqmp_validate_bitstream(desc, buf, bsize, bsize, &swap))
			return FPGA_FAIL;
		bsizeptr = (u32 *)&bsize;
		flush_dcache_range((ulong)bsizeptr,
				   (ulong)bsizeptr + sizeof(size_t));
		bsize_req = (u32)(uintptr_t)bsizeptr;
		bstype |= BIT(ZYNQMP_FPGA_BIT_NS);
	} else {
		bstype = 0;
	}

	bin_buf = zynqmp_align_dma_buffer((u32 *)buf, bsize, swap);

	flush_dcache_range(bin_buf, bin_buf + bsize);

	buf_lo = (u32)bin_buf;
	buf_hi = upper_32_bits(bin_buf);

	ret = xilinx_pm_request(PM_FPGA_LOAD, buf_lo, buf_hi,
				bsize_req, bstype, ret_payload);
	if (ret)
		printf("PL FPGA LOAD failed with err: 0x%08x\n", ret);

	return ret;
}

#if CONFIG_IS_ENABLED(FPGA_LOAD_SECURE)
static int zynqmp_loads(xilinx_desc *desc, const void *buf, size_t bsize,
			struct fpga_secure_info *fpga_sec_info)
{
	int ret;
	u32 buf_lo, buf_hi;
	u32 ret_payload[PAYLOAD_ARG_CNT];
	u8 flag = 0;

	flush_dcache_range((ulong)buf, (ulong)buf +
			   ALIGN(bsize, CONFIG_SYS_CACHELINE_SIZE));

	if (!fpga_sec_info->encflag)
		flag |= BIT(ZYNQMP_FPGA_BIT_ENC_DEV_KEY);

	if (fpga_sec_info->userkey_addr &&
	    fpga_sec_info->encflag == FPGA_ENC_USR_KEY) {
		flush_dcache_range((ulong)fpga_sec_info->userkey_addr,
				   (ulong)fpga_sec_info->userkey_addr +
				   ALIGN(KEY_PTR_LEN,
					 CONFIG_SYS_CACHELINE_SIZE));
		flag |= BIT(ZYNQMP_FPGA_BIT_ENC_USR_KEY);
	}

	if (!fpga_sec_info->authflag)
		flag |= BIT(ZYNQMP_FPGA_BIT_AUTH_OCM);

	if (fpga_sec_info->authflag == ZYNQMP_FPGA_AUTH_DDR)
		flag |= BIT(ZYNQMP_FPGA_BIT_AUTH_DDR);

	buf_lo = lower_32_bits((ulong)buf);
	buf_hi = upper_32_bits((ulong)buf);

	if ((u32)(uintptr_t)fpga_sec_info->userkey_addr)
		ret = xilinx_pm_request(PM_FPGA_LOAD, buf_lo,
				buf_hi,
				(u32)(uintptr_t)fpga_sec_info->userkey_addr,
				flag, ret_payload);
	else
		ret = xilinx_pm_request(PM_FPGA_LOAD, buf_lo,
					buf_hi, (u32)bsize,
					flag, ret_payload);

	if (ret)
		puts("PL FPGA LOAD fail\n");
	else
		puts("Bitstream successfully loaded\n");

	return ret;
}
#endif

static int zynqmp_pcap_info(xilinx_desc *desc)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	ret = xilinx_pm_request(PM_FPGA_GET_STATUS, 0, 0, 0,
				0, ret_payload);
	if (!ret)
		printf("PCAP status\t0x%x\n", ret_payload[1]);

	return ret;
}

static int __maybe_unused zynqmp_str2flag(xilinx_desc *desc, const char *str)
{
	if (!strncmp(str, "u-boot,fpga-legacy", 18))
		return FPGA_LEGACY;
#if CONFIG_IS_ENABLED(FPGA_LOAD_SECURE)
	if (!strncmp(str, "u-boot,zynqmp-fpga-ddrauth", 26))
		return FPGA_XILINX_ZYNQMP_DDRAUTH;

	if (!strncmp(str, "u-boot,zynqmp-fpga-enc", 22))
		return FPGA_XILINX_ZYNQMP_ENC;
#endif
	return 0;
}

struct xilinx_fpga_op zynqmp_op = {
	.load = zynqmp_load,
	.info = zynqmp_pcap_info,
#if CONFIG_IS_ENABLED(FPGA_LOAD_SECURE)
	.loads = zynqmp_loads,
	.str2flag = zynqmp_str2flag,
#endif
};
