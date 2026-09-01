// SPDX-License-Identifier: GPL-2.0
/*
 * AMD MMI Display DMA (DCDMA) driver
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#define LOG_CATEGORY UCLASS_DMA

#include <cpu_func.h>
#include <dm.h>
#include <dma-uclass.h>
#include <malloc.h>
#include <video_dma.h>
#include <vsprintf.h>

#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/build_bug.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/stddef.h>

#define DCDMA_NUM_CHANNELS		8

#define DCDMA_WPROTS			0x0
#define DCDMA_GBL			0x104

#define DCDMA_ISR			0x50
#define DCDMA_ISR_NO_OSTAND_TRAN(ch)	BIT(8 + (ch))
#define DCDMA_ISR_DATA_AXI_ERR(ch)	BIT(16 + (ch))
#define DCDMA_ISR_DSCR_ERR(ch)		BIT(24 + (ch))

#define DCDMA_CH0TOCH5_EISR		0x90
#define DCDMA_EISR_NUM_CHANNELS		6
#define DCDMA_EISR_DSCR_RD_AXI_ERR(ch)	BIT(0 + (ch))
#define DCDMA_EISR_DSCR_PRE_ERR(ch)	BIT(6 + (ch))
#define DCDMA_EISR_DSCR_CRC_ERR(ch)	BIT(12 + (ch))

#define DCDMA_CH_BASE(n)		(0x200 + (n) * 0x100)
#define DCDMA_CH_DSCR_ADDR_HI		0x0
#define DCDMA_CH_DSCR_ADDR_HI_MASK	0xffff	/* register is only 16 bits wide */
#define DCDMA_CH_DSCR_ADDR_LO		0x4
#define DCDMA_CH_CNTL			0x18
#define DCDMA_CH_STATUS			0x1c
#define DCDMA_CH_ENABLE			BIT(0)
#define DCDMA_CH_PAUSE			BIT(1)
#define DCDMA_CH_STATUS_OTRAN_MASK	GENMASK(28, 20)
#define DCDMA_CH_STATUS_OTRAN_SHIFT	20
#define DCDMA_CH_STATUS_PREAMBLE_MASK	GENMASK(19, 12)
#define DCDMA_CH_STATUS_PREAMBLE_SHIFT	12

#define DCDMA_DESC_ALIGN		256
#define DCDMA_DESC_PREAMBLE		0xa5u
#define DCDMA_DESC_DUMP_BYTES		32
/* " %02x" per byte: space + 2 hex digits + NUL in the snprintf slice. */
#define DCDMA_HEX_CHARS_PER_BYTE	3
/* desc->line_stride is in 16-byte units, not raw bytes. */
#define DCDMA_LINE_STRIDE_UNIT_SHIFT	4
/* Field widths of desc->line_size/line_stride, for overflow checking. */
#define DCDMA_LINE_SIZE_BITS		18
#define DCDMA_LINE_STRIDE_BITS		14
/* Field width of desc->src_addr, for overflow checking. */
#define DCDMA_SRC_ADDR_BITS		48

/*
 * A fetch can legitimately take up to a frame (~16.7 ms at 640x480@60Hz)
 * to validate, since the descriptor-validity gate is tied to the DC's
 * per-frame video timing. 20000us gives >1 frame of margin.
 */
#define DCDMA_START_TIMEOUT_US		20000
#define DCDMA_POLL_US			10
#define DCDMA_TRIGGER_RETRIES		5
#define DCDMA_RETRY_DELAY_MS		25

/*
 * Hardware descriptor, 256-byte aligned. Kept as C bitfields rather than
 * hand-rolled shifts/masks. This field order/width layout is a byte-for-byte
 * match of Linux's struct mmi_dcdma_hw_desc (drivers/dma/xilinx/mmi_dcdma.c)
 * for the same IP, which is the vendor-confirmed reference for this
 * hardware -- notably it also has no padding after @desc_id, so @ctrl
 * below intentionally starts at byte offset 2, not on a 32-bit word
 * boundary; that is the real, working hardware layout, not a bug. The
 * static_assert()s after the struct pin this down so a future change
 * can't silently shift it.
 */
struct amd_mmi_dcdma_desc_ctrl {
	u32 preamble			: 8;
	u32 update_en			: 1;
	u32 ignore_done			: 1;
	u32 last_descriptor		: 1;
	u32 last_descriptor_frame	: 1;
	u32 crc_en			: 1;
	u32 axi_burst			: 1;
	u32 axi_cache			: 4;
	u32 axi_prot			: 2;
	u32 axi_awcache			: 4;
	u32 axi_awqos			: 4;
	u32 reserved			: 4;
} __packed;

struct amd_mmi_dcdma_desc {
	u32 desc_id			: 16;
	struct amd_mmi_dcdma_desc_ctrl ctrl;
	u32 data_size			: 32;
	u64 src_addr			: 48;
	u64 next_desc			: 48;
	u32 tlb_prefetch_en		:  1;
	u32 tlb_prefetch_blk_size	: 14;
	u32 tlb_prefetch_blk_offset	: 14;
	u32 line_or_tile		:  1;
	u32 line_size			: 18;	/* bytes per line */
	u32 line_stride			: 14;	/* 16-byte units */
	u32 tile_type			:  1;
	u32 tile_pitch			: 14;
	u32 target_addr			:  1;
	u32 irq_en			:  1;
	u32 reserved0			:  1;
	u64 presentation_ts		: 64;
	u32 reserved1			: 32;
	u32 checksum			: 32;
} __packed __aligned(DCDMA_DESC_ALIGN);

static_assert(offsetof(struct amd_mmi_dcdma_desc, ctrl) == 2,
	      "amd_mmi_dcdma_desc layout diverged from Linux's mmi_dcdma_hw_desc");
static_assert(sizeof(struct amd_mmi_dcdma_desc) == DCDMA_DESC_ALIGN,
	      "amd_mmi_dcdma_desc must be exactly DCDMA_DESC_ALIGN bytes");

struct amd_mmi_dcdma_chan {
	bool requested;
	struct amd_mmi_dcdma_desc *desc;
};

struct amd_mmi_dcdma_priv {
	void __iomem *base;
	struct amd_mmi_dcdma_chan chan[DCDMA_NUM_CHANNELS];
};

static inline u32 dcdma_rd(struct amd_mmi_dcdma_priv *p, u32 reg)
{
	return readl(p->base + reg);
}

static inline void dcdma_wr(struct amd_mmi_dcdma_priv *p, u32 reg, u32 val)
{
	writel(val, p->base + reg);
}

/**
 * dcdma_ch_preamble() - read CH_STATUS and extract the PREAMBLE field
 * @priv: the amd_mmi_dcdma private data
 * @ch: channel index
 * @status_out: if non-NULL, set to the raw CH_STATUS value read
 *
 * Shared by the trigger-validation poll loop and the failure/debug dumps,
 * all of which need this same mask/shift.
 *
 * Return: the decoded PREAMBLE field.
 */
static u32 dcdma_ch_preamble(struct amd_mmi_dcdma_priv *priv,
			     unsigned long ch, u32 *status_out)
{
	u32 status = dcdma_rd(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_STATUS);

	if (status_out)
		*status_out = status;

	return (status & DCDMA_CH_STATUS_PREAMBLE_MASK) >>
	       DCDMA_CH_STATUS_PREAMBLE_SHIFT;
}

/*
 * cntl/status: raw CH_CNTL/CH_STATUS register values.
 * preamble: STATUS.PREAMBLE field, decoded.
 * isr/eisr: raw DCDMA_ISR/CH0TOCH5_EISR register values.
 * no_ostand_tran/dscr_err/dscr_rd_axi_err: individual ISR/EISR error bits
 * for this channel, decoded.
 */
struct dcdma_ch_status {
	u32 cntl;
	u32 status;
	u32 preamble;
	u32 isr;
	u32 eisr;
	bool no_ostand_tran;
	bool dscr_err;
	bool dscr_rd_axi_err;
};

/**
 * dcdma_read_ch_status() - read and decode the channel/global status
 * @priv: the amd_mmi_dcdma private data
 * @ch: channel index
 * @s: set to the decoded channel/global status
 */
static void dcdma_read_ch_status(struct amd_mmi_dcdma_priv *priv,
				 unsigned long ch, struct dcdma_ch_status *s)
{
	s->cntl = dcdma_rd(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_CNTL);
	s->preamble = dcdma_ch_preamble(priv, ch, &s->status);
	s->isr = dcdma_rd(priv, DCDMA_ISR);
	s->no_ostand_tran = !!(s->isr & DCDMA_ISR_NO_OSTAND_TRAN(ch));
	s->dscr_err = !!(s->isr & DCDMA_ISR_DSCR_ERR(ch));

	/*
	 * EISR only carries valid bits for channels 0-5; DCDMA_EISR_*(ch)
	 * would alias onto a different channel's bits for ch >= 6, so treat
	 * those channels as having no EISR state rather than decoding
	 * garbage.
	 */
	if (ch < DCDMA_EISR_NUM_CHANNELS) {
		s->eisr = dcdma_rd(priv, DCDMA_CH0TOCH5_EISR);
		s->dscr_rd_axi_err = !!(s->eisr & DCDMA_EISR_DSCR_RD_AXI_ERR(ch));
	} else {
		s->eisr = 0;
		s->dscr_rd_axi_err = false;
	}
}

/**
 * amd_mmi_dcdma_of_to_plat() - map the DCDMA register region
 * @dev: the amd_mmi_dcdma device
 *
 * Return: 0 if OK, -ve on error.
 */
static int amd_mmi_dcdma_of_to_plat(struct udevice *dev)
{
	struct amd_mmi_dcdma_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->base = ioremap(addr, SZ_4K);
	if (!priv->base)
		return -ENOMEM;

	return 0;
}

/**
 * amd_mmi_dcdma_probe() - unlock DCDMA's write-protected registers
 * @dev: the amd_mmi_dcdma device
 *
 * Return: 0 if OK, -ve on error.
 */
static int amd_mmi_dcdma_probe(struct udevice *dev)
{
	struct amd_mmi_dcdma_priv *priv = dev_get_priv(dev);

	/* Disable write protection so channel/global registers can be programmed */
	dcdma_wr(priv, DCDMA_WPROTS, 0);

	dev_dbg(dev, "DCDMA: probe base=%p WPROTS=0x%x\n", priv->base,
		dcdma_rd(priv, DCDMA_WPROTS));

	return 0;
}

/**
 * amd_mmi_dcdma_request() - claim a DCDMA channel for exclusive use
 * @dma: the channel to request (dma->id selects which one)
 *
 * Return: 0 if OK, -EINVAL if dma->id is out of range, -EBUSY if the
 *	   channel is already requested.
 */
static int amd_mmi_dcdma_request(struct dma *dma)
{
	struct amd_mmi_dcdma_priv *priv = dev_get_priv(dma->dev);
	struct amd_mmi_dcdma_chan *chan;

	if (dma->id >= DCDMA_NUM_CHANNELS)
		return -EINVAL;

	chan = &priv->chan[dma->id];
	if (chan->requested)
		return -EBUSY;

	chan->requested = true;

	dev_dbg(dma->dev, "DCDMA: ch%lu requested\n", dma->id);

	return 0;
}

/**
 * amd_mmi_dcdma_rfree() - stop and release a previously-requested DCDMA channel
 * @dma: the channel to release (dma->id selects which one)
 *
 * Return: 0 if OK, -EINVAL if dma->id is out of range.
 */
static int amd_mmi_dcdma_rfree(struct dma *dma)
{
	struct amd_mmi_dcdma_priv *priv = dev_get_priv(dma->dev);
	struct amd_mmi_dcdma_chan *chan;

	if (dma->id >= DCDMA_NUM_CHANNELS)
		return -EINVAL;

	chan = &priv->chan[dma->id];

	dcdma_wr(priv, DCDMA_CH_BASE(dma->id) + DCDMA_CH_CNTL, 0);

	free(chan->desc);
	chan->desc = NULL;
	chan->requested = false;

	return 0;
}

/**
 * dcdma_wait_vsync_or_delay() - synchronize the next trigger attempt
 * @meta: transfer metadata, including the optional wait_vsync callback
 * @attempt: 1-based attempt number, used to skip the delay before the very
 *	     first attempt
 *
 * Blocks for a fresh VSYNC edge if the caller has a source to gate on
 * (@meta->wait_vsync), else just paces retries with a fixed delay.
 *
 * Return: true if gated on a real VSYNC edge, false if paced by a fixed
 *	   delay (or if wait_vsync timed out and this attempt is going in
 *	   unsynced).
 */
static bool dcdma_wait_vsync_or_delay(struct video_dma_meta *meta, int attempt)
{
	if (meta->wait_vsync) {
		int vret = meta->wait_vsync(meta->vsync_ctx);

		if (vret)
			return false;
		return true;
	}

	if (attempt > 1)
		mdelay(DCDMA_RETRY_DELAY_MS);

	return false;
}

/**
 * dcdma_program_and_trigger() - program the descriptor address, enable the
 *				  channel, and trigger the fetch
 * @priv: the amd_mmi_dcdma private data
 * @ch: channel index
 * @desc_pa: physical address of the descriptor to program
 *
 * Return: true if the CH_PAUSE bit was found set (and cleared) after
 *	   enabling the channel, false otherwise.
 */
static bool dcdma_program_and_trigger(struct amd_mmi_dcdma_priv *priv,
				      unsigned long ch, phys_addr_t desc_pa)
{
	bool pause_cleared;
	u32 cntl;

	/* Disable the channel before (re)programming it */
	dcdma_wr(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_CNTL, 0);

	dcdma_wr(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_DSCR_ADDR_LO,
		 lower_32_bits(desc_pa));
	dcdma_wr(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_DSCR_ADDR_HI,
		 upper_32_bits(desc_pa) & DCDMA_CH_DSCR_ADDR_HI_MASK);

	dcdma_wr(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_CNTL, DCDMA_CH_ENABLE);

	cntl = dcdma_rd(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_CNTL);
	pause_cleared = !!(cntl & DCDMA_CH_PAUSE);
	if (pause_cleared)
		dcdma_wr(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_CNTL,
			 DCDMA_CH_ENABLE);

	dcdma_wr(priv, DCDMA_GBL, BIT(ch));

	return pause_cleared;
}

/**
 * dcdma_poll_validated() - poll for the just-triggered descriptor to latch
 * @priv: the amd_mmi_dcdma private data
 * @ch: channel index
 * @status_out: set to the raw CH_STATUS value at the point of success or
 *		timeout
 * @elapsed_us_out: set to the elapsed time in microseconds if validated
 *
 * Return: true if validated within DCDMA_START_TIMEOUT_US, false on timeout.
 */
static bool dcdma_poll_validated(struct amd_mmi_dcdma_priv *priv,
				 unsigned long ch, u32 *status_out,
				 int *elapsed_us_out)
{
	int timeout;

	for (timeout = DCDMA_START_TIMEOUT_US; timeout > 0;
	     timeout -= DCDMA_POLL_US) {
		if (dcdma_ch_preamble(priv, ch, status_out) == DCDMA_DESC_PREAMBLE) {
			*elapsed_us_out = DCDMA_START_TIMEOUT_US - timeout;
			return true;
		}
		udelay(DCDMA_POLL_US);
	}

	return false;
}

/**
 * dcdma_clear_trigger_errors() - clear latched ISR/EISR error bits after a
 *				   failed (but not final) trigger attempt
 * @priv: the amd_mmi_dcdma private data
 * @ch: channel index
 *
 * Return: true if any ISR/EISR error bit was found set (and cleared) for
 *	   this attempt, false otherwise.
 */
static bool dcdma_clear_trigger_errors(struct amd_mmi_dcdma_priv *priv,
				       unsigned long ch)
{
	struct dcdma_ch_status s;
	u32 isr_mask, eisr_mask;
	bool had_error = false;

	dcdma_read_ch_status(priv, ch, &s);

	isr_mask = DCDMA_ISR_NO_OSTAND_TRAN(ch) | DCDMA_ISR_DATA_AXI_ERR(ch) |
		   DCDMA_ISR_DSCR_ERR(ch);
	if (s.isr & isr_mask) {
		dcdma_wr(priv, DCDMA_ISR, s.isr & isr_mask);
		had_error = true;
	}

	eisr_mask = DCDMA_EISR_DSCR_RD_AXI_ERR(ch) | DCDMA_EISR_DSCR_PRE_ERR(ch) |
		    DCDMA_EISR_DSCR_CRC_ERR(ch);
	if (s.eisr & eisr_mask) {
		dcdma_wr(priv, DCDMA_CH0TOCH5_EISR, s.eisr & eisr_mask);
		had_error = true;
	}

	return had_error;
}

/**
 * dcdma_dump_and_giveup() - last-resort diagnostic dump once every trigger
 *			      attempt has failed
 * @priv: the amd_mmi_dcdma private data
 * @dev: the amd_mmi_dcdma device
 * @ch: channel index
 * @desc: the descriptor that was never validated
 * @desc_pa: physical address of @desc
 * @vsync_timeout_n: number of attempts (out of DCDMA_TRIGGER_RETRIES) where
 *		      wait_vsync timed out
 * @pause_cleared_n: number of attempts where CH_PAUSE had to be cleared
 * @error_n: number of non-final attempts where an ISR/EISR error bit was set
 */
static void dcdma_dump_and_giveup(struct amd_mmi_dcdma_priv *priv,
				  struct udevice *dev, unsigned long ch,
				  struct amd_mmi_dcdma_desc *desc,
				  phys_addr_t desc_pa,
				  unsigned int vsync_timeout_n,
				  unsigned int pause_cleared_n,
				  unsigned int error_n)
{
	struct dcdma_ch_status s;
	u8 *raw = (u8 *)desc;

	dcdma_read_ch_status(priv, ch, &s);

	dev_dbg(dev, "DCDMA: ch%lu CNTL=0x%x (EN=%d PAUSE=%d) STATUS=0x%x PREAMBLE=0x%x\n",
		ch, s.cntl, !!(s.cntl & DCDMA_CH_ENABLE),
		!!(s.cntl & DCDMA_CH_PAUSE), s.status, s.preamble);
	dev_dbg(dev, "DCDMA: ch%lu ISR=0x%x EISR=0x%x (no_ostand=%d dscr_err=%d dscr_rd_axi_err=%d)\n",
		ch, s.isr, s.eisr, s.no_ostand_tran, s.dscr_err,
		s.dscr_rd_axi_err);

	dev_dbg(dev, "DCDMA: ch%lu descriptor (desc_pa=0x%llx): preamble=0x%x\n",
		ch, (u64)desc_pa, desc->ctrl.preamble);
	dev_dbg(dev, "DCDMA: ch%lu descriptor last_desc=%d last_frame=%d ignore_done=%d\n",
		ch, desc->ctrl.last_descriptor, desc->ctrl.last_descriptor_frame,
		desc->ctrl.ignore_done);
	dev_dbg(dev, "DCDMA: ch%lu descriptor data_size=%u src_addr=0x%llx next_desc=0x%llx\n",
		ch, desc->data_size, (u64)desc->src_addr, (u64)desc->next_desc);
	dev_dbg(dev, "DCDMA: ch%lu descriptor line_size=%u line_stride=%u\n",
		ch, desc->line_size, desc->line_stride);

	{
		char hex[DCDMA_DESC_DUMP_BYTES * DCDMA_HEX_CHARS_PER_BYTE + 1] = "";

		for (int i = 0; i < DCDMA_DESC_DUMP_BYTES; i++)
			snprintf(hex + i * DCDMA_HEX_CHARS_PER_BYTE,
				 DCDMA_HEX_CHARS_PER_BYTE + 1, " %02x", raw[i]);

		dev_dbg(dev, "DCDMA: ch%lu descriptor raw bytes[0:%d]:%s\n",
			ch, DCDMA_DESC_DUMP_BYTES - 1, hex);
	}

	dev_dbg(dev, "DCDMA: ch%lu DSCR_ADDR_LO=0x%x DSCR_ADDR_HI=0x%x GBL=0x%x WPROTS=0x%x\n",
		ch, dcdma_rd(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_DSCR_ADDR_LO),
		dcdma_rd(priv, DCDMA_CH_BASE(ch) + DCDMA_CH_DSCR_ADDR_HI),
		dcdma_rd(priv, DCDMA_GBL), dcdma_rd(priv, DCDMA_WPROTS));

	dev_err(dev, "DCDMA: ch%lu exhausted %d attempts (DC not consuming?): %u vsync-to %u pause-clr %u err\n",
		ch, DCDMA_TRIGGER_RETRIES, vsync_timeout_n, pause_cleared_n,
		error_n);
}

/**
 * amd_mmi_dcdma_send() - build a descriptor and trigger a DCDMA transfer
 * @dma: the requested channel to send on (dma->id selects which one)
 * @src: source framebuffer address in memory
 * @len: total transfer length in bytes
 * @metadata: struct video_dma_meta -- line_size/stride and cyclic/one-shot
 *	      mode, plus the optional vsync-gating callback used before each
 *	      trigger attempt
 *
 * Allocates (on first call) and (re)programs one aligned DMA descriptor,
 * flushes both the descriptor and the source buffer from dcache since this
 * path is not cache-coherent, then triggers the channel, retrying up to
 * DCDMA_TRIGGER_RETRIES times if the hardware does not validate the
 * descriptor. A cyclic-mode descriptor points its next_desc back at itself,
 * so a single successful trigger keeps re-fetching and streaming the same
 * buffer until the channel is stopped.
 *
 * Return: 0 once the descriptor is validated, -EINVAL for a bad channel id/
 *	   missing metadata/oversized line_size or stride, -ENOMEM if the
 *	   descriptor allocation fails, -ETIMEDOUT if every trigger attempt
 *	   is exhausted without validation.
 */
static int amd_mmi_dcdma_send(struct dma *dma, void *src, size_t len,
			      void *metadata)
{
	unsigned int vsync_timeout_n = 0, pause_cleared_n = 0, error_n = 0;
	struct amd_mmi_dcdma_priv *priv = dev_get_priv(dma->dev);
	struct video_dma_meta *meta = metadata;
	struct amd_mmi_dcdma_chan *chan;
	struct amd_mmi_dcdma_desc *desc;
	phys_addr_t desc_pa, src_pa;
	u32 line_size, stride;
	bool cyclic;

	if (dma->id >= DCDMA_NUM_CHANNELS || !meta || !src || !len)
		return -EINVAL;

	chan = &priv->chan[dma->id];
	if (!chan->requested)
		return -EINVAL;

	if (!chan->desc) {
		chan->desc = memalign(DCDMA_DESC_ALIGN, sizeof(*chan->desc));
		if (!chan->desc)
			return -ENOMEM;
	}
	desc = chan->desc;
	desc_pa = (phys_addr_t)desc;
	src_pa = (phys_addr_t)src;
	cyclic = meta->mode == VIDEO_DMA_CYCLIC;

	line_size = ALIGN(meta->line_size, BIT(DCDMA_LINE_STRIDE_UNIT_SHIFT));
	stride = ALIGN(meta->stride, BIT(DCDMA_LINE_STRIDE_UNIT_SHIFT));

	if (line_size >= BIT(DCDMA_LINE_SIZE_BITS) ||
	    (stride >> DCDMA_LINE_STRIDE_UNIT_SHIFT) >= BIT(DCDMA_LINE_STRIDE_BITS))
		return -EINVAL;

	if (len > U32_MAX || (u64)src_pa >= BIT_ULL(DCDMA_SRC_ADDR_BITS))
		return -EINVAL;

	dev_dbg(dma->dev, "DCDMA: ch%lu send src=0x%llx len=%zu line_size=%u stride=%u mode=%s\n",
		dma->id, (u64)src_pa, len, line_size, stride,
		cyclic ? "cyclic" : "oneshot");

	memset(desc, 0, sizeof(*desc));
	desc->ctrl.preamble = DCDMA_DESC_PREAMBLE;
	desc->ctrl.last_descriptor_frame = 1;
	if (cyclic)
		desc->ctrl.ignore_done = 1;
	else
		desc->ctrl.last_descriptor = 1;

	desc->data_size = len;
	desc->src_addr = src_pa;
	desc->line_size = line_size;
	desc->line_stride = stride >> DCDMA_LINE_STRIDE_UNIT_SHIFT;

	if (cyclic)
		desc->next_desc = desc_pa;

	flush_dcache_range((ulong)desc,
			   ALIGN((ulong)desc + sizeof(*desc),
				 CONFIG_SYS_CACHELINE_SIZE));
	flush_dcache_range((ulong)src,
			   ALIGN((ulong)src + len, CONFIG_SYS_CACHELINE_SIZE));

	for (int attempt = 1; attempt <= DCDMA_TRIGGER_RETRIES; attempt++) {
		bool vsync_gated;
		int elapsed_us;
		u32 status;

		vsync_gated = dcdma_wait_vsync_or_delay(meta, attempt);
		if (meta->wait_vsync && !vsync_gated)
			vsync_timeout_n++;

		if (dcdma_program_and_trigger(priv, dma->id, desc_pa))
			pause_cleared_n++;

		if (dcdma_poll_validated(priv, dma->id, &status, &elapsed_us)) {
			dev_dbg(dma->dev,
				"DCDMA: ch%lu ok: %d attempt(s) %dus gated=%d STATUS=0x%x to=%u pause=%u err=%u\n",
				dma->id, attempt, elapsed_us, vsync_gated, status,
				vsync_timeout_n, pause_cleared_n, error_n);
			return 0;
		}

		if (attempt == DCDMA_TRIGGER_RETRIES)
			dcdma_dump_and_giveup(priv, dma->dev, dma->id, desc,
					      desc_pa, vsync_timeout_n,
					      pause_cleared_n, error_n);
		else if (dcdma_clear_trigger_errors(priv, dma->id))
			error_n++;
	}

	return -ETIMEDOUT;
}

static const struct dma_ops amd_mmi_dcdma_ops = {
	.request	= amd_mmi_dcdma_request,
	.rfree		= amd_mmi_dcdma_rfree,
	.send		= amd_mmi_dcdma_send,
};

static const struct udevice_id amd_mmi_dcdma_ids[] = {
	{ .compatible = "amd,mmi-dcdma-1.0" },
	{ }
};

U_BOOT_DRIVER(amd_mmi_dcdma) = {
	.name		= "amd_mmi_dcdma",
	.id		= UCLASS_DMA,
	.of_match	= amd_mmi_dcdma_ids,
	.of_to_plat	= amd_mmi_dcdma_of_to_plat,
	.probe		= amd_mmi_dcdma_probe,
	.ops		= &amd_mmi_dcdma_ops,
	.priv_auto	= sizeof(struct amd_mmi_dcdma_priv),
};
