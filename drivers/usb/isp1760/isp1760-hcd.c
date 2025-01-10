// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the NXP ISP1760 chip
 *
 * Copyright 2021 Linaro, Rui Miguel Silva <rui.silva@linaro.org>
 *
 */

#include <hexdump.h>
#include <asm/cache.h>
#include <cpu_func.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/usb/usb_urb_compat.h>
#include <usb.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <asm/unaligned.h>

#include "isp1760-core.h"
#include "isp1760-hcd.h"
#include "isp1760-regs.h"
#include "isp1760-uboot.h"

#define ISP1760_LANGUAGE_DESC		"\4\3\19\4"
#define ISP1760_VENDOR_DESC		"\16\3U\0-\0B\0o\0o\0t\0"
#define ISP1760_PRODUCT_NAME_DESC	"\52\3I\0S\0P\0-\0 \0H\0o\0s\0t\0 \0C\0o\0n\0t\0r\0o\0l\0l\0e\0r\0"

static struct kmem_cache *qtd_cachep;
static struct kmem_cache *qh_cachep;
static struct kmem_cache *urb_listitem_cachep;

typedef void (packet_enqueue)(struct usb_hcd *hcd, struct isp1760_qh *qh,
			      struct isp1760_qtd *qtd);

static inline struct isp1760_hcd *hcd_to_priv(struct usb_hcd *hcd)
{
	return hcd->hcd_priv;
}

#define dw_to_le32(x)	(cpu_to_le32((__force u32)x))
#define le32_to_dw(x)	((__force __dw)(le32_to_cpu(x)))

/* urb state*/
#define DELETE_URB		(0x0008)
#define NO_TRANSFER_ACTIVE	(0xffffffff)

/* Philips Proprietary Transfer Descriptor (PTD) */
typedef __u32 __bitwise __dw;
struct ptd {
	__dw dw0;
	__dw dw1;
	__dw dw2;
	__dw dw3;
	__dw dw4;
	__dw dw5;
	__dw dw6;
	__dw dw7;
};

struct ptd_le32 {
	__le32 dw0;
	__le32 dw1;
	__le32 dw2;
	__le32 dw3;
	__le32 dw4;
	__le32 dw5;
	__le32 dw6;
	__le32 dw7;
};

#define PTD_OFFSET		0x0400
#define ISO_PTD_OFFSET		0x0400
#define INT_PTD_OFFSET		0x0800
#define ATL_PTD_OFFSET		0x0c00
#define PAYLOAD_OFFSET		0x1000

#define ISP_BANK_0		0x00
#define ISP_BANK_1		0x01
#define ISP_BANK_2		0x02
#define ISP_BANK_3		0x03

#define TO_DW(x)	((__force __dw)x)
#define TO_U32(x)	((__force u32)x)

 /* ATL */
 /* DW0 */
#define DW0_VALID_BIT			TO_DW(1)
#define FROM_DW0_VALID(x)		(TO_U32(x) & 0x01)
#define TO_DW0_LENGTH(x)		TO_DW((((u32)x) << 3))
#define TO_DW0_MAXPACKET(x)		TO_DW((((u32)x) << 18))
#define TO_DW0_MULTI(x)			TO_DW((((u32)x) << 29))
#define TO_DW0_ENDPOINT(x)		TO_DW((((u32)x) << 31))
/* DW1 */
#define TO_DW1_DEVICE_ADDR(x)		TO_DW((((u32)x) << 3))
#define TO_DW1_PID_TOKEN(x)		TO_DW((((u32)x) << 10))
#define DW1_TRANS_BULK			TO_DW(((u32)2 << 12))
#define DW1_TRANS_INT			TO_DW(((u32)3 << 12))
#define DW1_TRANS_SPLIT			TO_DW(((u32)1 << 14))
#define DW1_SE_USB_LOSPEED		TO_DW(((u32)2 << 16))
#define TO_DW1_PORT_NUM(x)		TO_DW((((u32)x) << 18))
#define TO_DW1_HUB_NUM(x)		TO_DW((((u32)x) << 25))
/* DW2 */
#define TO_DW2_DATA_START_ADDR(x)	TO_DW((((u32)x) << 8))
#define TO_DW2_RL(x)			TO_DW(((x) << 25))
#define FROM_DW2_RL(x)			((TO_U32(x) >> 25) & 0xf)
/* DW3 */
#define FROM_DW3_NRBYTESTRANSFERRED(x)		TO_U32((x) & 0x3fff)
#define FROM_DW3_SCS_NRBYTESTRANSFERRED(x)	TO_U32((x) & 0x07ff)
#define TO_DW3_NAKCOUNT(x)		TO_DW(((x) << 19))
#define FROM_DW3_NAKCOUNT(x)		((TO_U32(x) >> 19) & 0xf)
#define TO_DW3_CERR(x)			TO_DW(((x) << 23))
#define FROM_DW3_CERR(x)		((TO_U32(x) >> 23) & 0x3)
#define TO_DW3_DATA_TOGGLE(x)		TO_DW(((x) << 25))
#define FROM_DW3_DATA_TOGGLE(x)		((TO_U32(x) >> 25) & 0x1)
#define TO_DW3_PING(x)			TO_DW(((x) << 26))
#define FROM_DW3_PING(x)		((TO_U32(x) >> 26) & 0x1)
#define DW3_ERROR_BIT			TO_DW((1 << 28))
#define DW3_BABBLE_BIT			TO_DW((1 << 29))
#define DW3_HALT_BIT			TO_DW((1 << 30))
#define DW3_ACTIVE_BIT			TO_DW((1 << 31))
#define FROM_DW3_ACTIVE(x)		((TO_U32(x) >> 31) & 0x01)

#define INT_UNDERRUN			BIT(2)
#define INT_BABBLE			BIT(1)
#define INT_EXACT			BIT(0)

#define SETUP_PID	(2)
#define IN_PID		(1)
#define OUT_PID		(0)

/* Errata 1 */
#define RL_COUNTER	(0)
#define NAK_COUNTER	(0)
#define ERR_COUNTER	(3)

struct isp1760_qtd {
	u8 packet_type;
	void *data_buffer;
	u32 payload_addr;

	/* the rest is HCD-private */
	struct list_head qtd_list;
	struct urb *urb;
	size_t length;
	size_t actual_length;

	/*
	 * QTD_ENQUEUED:	waiting for transfer (inactive)
	 * QTD_PAYLOAD_ALLOC:	chip mem has been allocated for payload
	 * QTD_XFER_STARTED:	valid ptd has been written to isp176x - only
	 *			interrupt handler may touch this qtd!
	 * QTD_XFER_COMPLETE:	payload has been transferred successfully
	 * QTD_RETIRE:		transfer error/abort qtd
	 */
#define QTD_ENQUEUED		0
#define QTD_PAYLOAD_ALLOC	1
#define QTD_XFER_STARTED	2
#define QTD_XFER_COMPLETE	3
#define QTD_RETIRE		4
	u32 status;
};

/* Queue head, one for each active endpoint */
struct isp1760_qh {
	struct list_head qh_list;
	struct list_head qtd_list;
	int epnum;
	u32 toggle;
	u32 ping;
	int slot;
	int tt_buffer_dirty;	/* See USB2.0 spec section 11.17.5 */
};

struct urb_listitem {
	struct list_head urb_list;
	struct urb *urb;
};

static const u32 isp1763_hc_portsc1_fields[] = {
	[PORT_OWNER]		= BIT(13),
	[PORT_POWER]		= BIT(12),
	[PORT_LSTATUS]		= BIT(10),
	[PORT_RESET]		= BIT(8),
	[PORT_SUSPEND]		= BIT(7),
	[PORT_RESUME]		= BIT(6),
	[PORT_PE]		= BIT(2),
	[PORT_CSC]		= BIT(1),
	[PORT_CONNECT]		= BIT(0),
};

static struct descriptor {
	struct usb_device_descriptor device;
	struct usb_config_descriptor config;
	struct usb_interface_descriptor interface;
	struct usb_endpoint_descriptor endpoint;
} __packed rh_descriptor = {
	{
		/* usb 2.0 root hub device descriptor */
		0x12,       /*  __u8  bLength; */
		USB_DT_DEVICE, /* __u8 bDescriptorType; Device */
		0x0002, /*  __le16 bcdUSB; v2.0 */

		0x09,       /*  __u8  bDeviceClass; HUB_CLASSCODE */
		0x00,       /*  __u8  bDeviceSubClass; */
		0x00,       /*  __u8  bDeviceProtocol; [ usb 2.0 no TT ] */
		0x40,       /*  __u8  bMaxPacketSize0; 64 Bytes */

		0x6b1d,     /*  __le16 idVendor; Linux Foundation 0x1d6b */
		0x0200,     /*  __le16 idProduct; device 0x0002 */
		0x0001,     /*  __le16 bcdDevice */

		0x03,       /*  __u8  iManufacturer; */
		0x02,       /*  __u8  iProduct; */
		0x01,       /*  __u8  iSerialNumber; */
		0x01        /*  __u8  bNumConfigurations; */
	}, {
		/* one configuration */
		0x09,       /*  __u8  bLength; */
		USB_DT_CONFIG, /* __u8 bDescriptorType; Configuration */
		0x1900,     /*  __le16 wTotalLength; */
		0x01,       /*  __u8  bNumInterfaces; (1) */
		0x01,       /*  __u8  bConfigurationValue; */
		0x00,       /*  __u8  iConfiguration; */
		0xc0,       /*  __u8  bmAttributes;
			     *		Bit 7: must be set,
			     *		6: Self-powered,
			     *		5: Remote wakeup,
			     *		4..0: resvd
			     */
		0x00,       /*  __u8  MaxPower; */
	}, {
		/* one interface */
		0x09,       /*  __u8  if_bLength; */
		USB_DT_INTERFACE, /* __u8 if_bDescriptorType; Interface */
		0x00,       /*  __u8  if_bInterfaceNumber; */
		0x00,       /*  __u8  if_bAlternateSetting; */
		0x01,       /*  __u8  if_bNumEndpoints; */
		0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
		0x00,       /*  __u8  if_bInterfaceSubClass; */
		0x00,       /*  __u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
		0x00,       /*  __u8  if_iInterface; */
	}, {
		/* one endpoint (status change endpoint) */
		0x07,       /*  __u8  ep_bLength; */
		USB_DT_ENDPOINT, /* __u8 ep_bDescriptorType; Endpoint */
		0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
		0x03,       /*  __u8  ep_bmAttributes; Interrupt */
			/* __le16 ep_wMaxPacketSize; 1 + (MAX_ROOT_PORTS / 8) */
			/* see hub.c:hub_configure() for details. */
		(USB_MAXCHILDREN + 1 + 7) / 8, 0x00,
		0x0c        /*  __u8  ep_bInterval; (256ms -- usb 2.0 spec) */
	},
};

/*
 * Access functions for isp176x registers regmap fields
 */
static u32 isp1760_hcd_read(struct usb_hcd *hcd, u32 field)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	return isp1760_field_read(priv->fields, field);
}

/*
 * We need, in isp1763, to write directly the values to the portsc1
 * register so it will make the other values to trigger.
 */
static void isp1760_hcd_portsc1_set_clear(struct isp1760_hcd *priv, u32 field,
					  u32 val)
{
	u32 bit = isp1763_hc_portsc1_fields[field];
	u32 port_status = readl(priv->base + ISP1763_HC_PORTSC1);

	if (val)
		writel(port_status | bit, priv->base + ISP1763_HC_PORTSC1);
	else
		writel(port_status & ~bit, priv->base + ISP1763_HC_PORTSC1);
}

static void isp1760_hcd_write(struct usb_hcd *hcd, u32 field, u32 val)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	if (unlikely(priv->is_isp1763 &&
		     (field >= PORT_OWNER && field <= PORT_CONNECT)))
		return isp1760_hcd_portsc1_set_clear(priv, field, val);

	isp1760_field_write(priv->fields, field, val);
}

static void isp1760_hcd_set(struct usb_hcd *hcd, u32 field)
{
	isp1760_hcd_write(hcd, field, 0xFFFFFFFF);
}

static void isp1760_hcd_clear(struct usb_hcd *hcd, u32 field)
{
	isp1760_hcd_write(hcd, field, 0);
}

static int isp1760_hcd_set_and_wait(struct usb_hcd *hcd, u32 field,
				    u32 timeout_us)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	u32 val;

	isp1760_hcd_set(hcd, field);

	return regmap_field_read_poll_timeout(priv->fields[field], val,
					      val, 0, timeout_us);
}

static int isp1760_hcd_set_and_wait_swap(struct usb_hcd *hcd, u32 field,
					 u32 timeout_us)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	u32 val;

	isp1760_hcd_set(hcd, field);

	return regmap_field_read_poll_timeout(priv->fields[field], val,
					      !val, 0, timeout_us);
}

static int isp1760_hcd_clear_and_wait(struct usb_hcd *hcd, u32 field,
				      u32 timeout_us)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	u32 val;

	isp1760_hcd_clear(hcd, field);

	return regmap_field_read_poll_timeout(priv->fields[field], val,
					      !val, 0, timeout_us);
}

static bool isp1760_hcd_is_set(struct usb_hcd *hcd, u32 field)
{
	return !!isp1760_hcd_read(hcd, field);
}

static bool isp1760_hcd_ppc_is_set(struct usb_hcd *hcd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	if (priv->is_isp1763)
		return true;

	return isp1760_hcd_is_set(hcd, HCS_PPC);
}

static u32 isp1760_hcd_n_ports(struct usb_hcd *hcd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	if (priv->is_isp1763)
		return 1;

	return isp1760_hcd_read(hcd, HCS_N_PORTS);
}

/*
 * Access functions for isp176x memory (offset >= 0x0400).
 *
 * bank_reads8() reads memory locations prefetched by an earlier write to
 * HC_MEMORY_REG (see isp176x datasheet). Unless you want to do fancy multi-
 * bank optimizations, you should use the more generic mem_read() below.
 *
 * For access to ptd memory, use the specialized ptd_read() and ptd_write()
 * below.
 *
 * These functions copy via MMIO data to/from the device. memcpy_{to|from}io()
 * doesn't quite work because some people have to enforce 32-bit access
 */
static void bank_reads8(void __iomem *src_base, u32 src_offset, u32 bank_addr,
			__u32 *dst, u32 bytes)
{
	__u32 __iomem *src;
	u32 val;
	__u8 *src_byteptr;
	__u8 *dst_byteptr;

	src = src_base + (bank_addr | src_offset);

	if (src_offset < PAYLOAD_OFFSET) {
		while (bytes >= 4) {
			*dst = readl_relaxed(src);
			bytes -= 4;
			src++;
			dst++;
		}
	} else {
		while (bytes >= 4) {
			*dst = __raw_readl(src);
			bytes -= 4;
			src++;
			dst++;
		}
	}

	if (!bytes)
		return;

	/* in case we have 3, 2 or 1 by left. The dst buffer may not be fully
	 * allocated.
	 */
	if (src_offset < PAYLOAD_OFFSET)
		val = readl_relaxed(src);
	else
		val = __raw_readl(src);

	dst_byteptr = (void *)dst;
	src_byteptr = (void *)&val;
	while (bytes > 0) {
		*dst_byteptr = *src_byteptr;
		dst_byteptr++;
		src_byteptr++;
		bytes--;
	}
}

static void isp1760_mem_read(struct usb_hcd *hcd, u32 src_offset, void *dst,
			     u32 bytes)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	isp1760_hcd_write(hcd, MEM_BANK_SEL, ISP_BANK_0);
	isp1760_hcd_write(hcd, MEM_START_ADDR, src_offset);
	ndelay(100);

	bank_reads8(priv->base, src_offset, ISP_BANK_0, dst, bytes);
}

/*
 * ISP1763 does not have the banks direct host controller memory access,
 * needs to use the HC_DATA register. Add data read/write according to this,
 * and also adjust 16bit access.
 */
static void isp1763_mem_read(struct usb_hcd *hcd, u16 srcaddr,
			     u16 *dstptr, u32 bytes)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	/* Write the starting device address to the hcd memory register */
	isp1760_reg_write(priv->regs, ISP1763_HC_MEMORY, srcaddr);
	ndelay(100); /* Delay between consecutive access */

	/* As long there are at least 16-bit to read ... */
	while (bytes >= 2) {
		*dstptr = __raw_readw(priv->base + ISP1763_HC_DATA);
		bytes -= 2;
		dstptr++;
	}

	/* If there are no more bytes to read, return */
	if (bytes <= 0)
		return;

	*((u8 *)dstptr) = (u8)(readw(priv->base + ISP1763_HC_DATA) & 0xFF);
}

static void mem_read(struct usb_hcd *hcd, u32 src_offset, __u32 *dst,
		     u32 bytes)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	if (!priv->is_isp1763)
		return isp1760_mem_read(hcd, src_offset, (u16 *)dst, bytes);

	isp1763_mem_read(hcd, (u16)src_offset, (u16 *)dst, bytes);
}

static void isp1760_mem_write(void __iomem *dst_base, u32 dst_offset,
			      __u32 const *src, u32 bytes)
{
	__u32 __iomem *dst;

	dst = dst_base + dst_offset;

	if (dst_offset < PAYLOAD_OFFSET) {
		while (bytes >= 4) {
			writel_relaxed(*src, dst);
			bytes -= 4;
			src++;
			dst++;
		}
	} else {
		while (bytes >= 4) {
			__raw_writel(*src, dst);
			bytes -= 4;
			src++;
			dst++;
		}
	}

	if (!bytes)
		return;
	/* in case we have 3, 2 or 1 bytes left. The buffer is allocated and the
	 * extra bytes should not be read by the HW.
	 */

	if (dst_offset < PAYLOAD_OFFSET)
		writel_relaxed(*src, dst);
	else
		__raw_writel(*src, dst);
}

static void isp1763_mem_write(struct usb_hcd *hcd, u16 dstaddr, u16 *src,
			      u32 bytes)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	/* Write the starting device address to the hcd memory register */
	isp1760_reg_write(priv->regs, ISP1763_HC_MEMORY, dstaddr);
	ndelay(100); /* Delay between consecutive access */

	while (bytes >= 2) {
		/* Get and write the data; then adjust the data ptr and len */
		__raw_writew(*src, priv->base + ISP1763_HC_DATA);
		bytes -= 2;
		src++;
	}

	/* If there are no more bytes to process, return */
	if (bytes <= 0)
		return;

	/*
	 * The only way to get here is if there is a single byte left,
	 * get it and write it to the data reg;
	 */
	writew(*((u8 *)src), priv->base + ISP1763_HC_DATA);
}

static void mem_write(struct usb_hcd *hcd, u32 dst_offset, __u32 *src,
		      u32 bytes)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	if (!priv->is_isp1763)
		return isp1760_mem_write(priv->base, dst_offset, src, bytes);

	isp1763_mem_write(hcd, dst_offset, (u16 *)src, bytes);
}

/*
 * Read and write ptds. 'ptd_offset' should be one of ISO_PTD_OFFSET,
 * INT_PTD_OFFSET, and ATL_PTD_OFFSET. 'slot' should be less than 32.
 */
static void isp1760_ptd_read(struct usb_hcd *hcd, u32 ptd_offset, u32 slot,
			     struct ptd *ptd)
{
	u16 src_offset = ptd_offset + slot * sizeof(*ptd);
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	isp1760_hcd_write(hcd, MEM_BANK_SEL, ISP_BANK_0);
	isp1760_hcd_write(hcd, MEM_START_ADDR, src_offset);
	ndelay(90);

	bank_reads8(priv->base, src_offset, ISP_BANK_0, (void *)ptd,
		    sizeof(*ptd));
}

static void isp1763_ptd_read(struct usb_hcd *hcd, u32 ptd_offset, u32 slot,
			     struct ptd *ptd)
{
	u16 src_offset = ptd_offset + slot * sizeof(*ptd);
	struct ptd_le32 le32_ptd;

	isp1763_mem_read(hcd, src_offset, (u16 *)&le32_ptd, sizeof(le32_ptd));
	/* Normalize the data obtained */
	ptd->dw0 = le32_to_dw(le32_ptd.dw0);
	ptd->dw1 = le32_to_dw(le32_ptd.dw1);
	ptd->dw2 = le32_to_dw(le32_ptd.dw2);
	ptd->dw3 = le32_to_dw(le32_ptd.dw3);
	ptd->dw4 = le32_to_dw(le32_ptd.dw4);
	ptd->dw5 = le32_to_dw(le32_ptd.dw5);
	ptd->dw6 = le32_to_dw(le32_ptd.dw6);
	ptd->dw7 = le32_to_dw(le32_ptd.dw7);
}

static void ptd_read(struct usb_hcd *hcd, u32 ptd_offset, u32 slot,
		     struct ptd *ptd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	if (!priv->is_isp1763)
		return isp1760_ptd_read(hcd, ptd_offset, slot, ptd);

	isp1763_ptd_read(hcd, ptd_offset, slot, ptd);
}

static void isp1763_ptd_write(struct usb_hcd *hcd, u32 ptd_offset, u32 slot,
			      struct ptd *cpu_ptd)
{
	u16 dst_offset = ptd_offset + slot * sizeof(*cpu_ptd);
	struct ptd_le32 ptd;

	ptd.dw0 = dw_to_le32(cpu_ptd->dw0);
	ptd.dw1 = dw_to_le32(cpu_ptd->dw1);
	ptd.dw2 = dw_to_le32(cpu_ptd->dw2);
	ptd.dw3 = dw_to_le32(cpu_ptd->dw3);
	ptd.dw4 = dw_to_le32(cpu_ptd->dw4);
	ptd.dw5 = dw_to_le32(cpu_ptd->dw5);
	ptd.dw6 = dw_to_le32(cpu_ptd->dw6);
	ptd.dw7 = dw_to_le32(cpu_ptd->dw7);

	isp1763_mem_write(hcd, dst_offset,  (u16 *)&ptd.dw0,
			  8 * sizeof(ptd.dw0));
}

static void isp1760_ptd_write(void __iomem *base, u32 ptd_offset, u32 slot,
			      struct ptd *ptd)
{
	u32 dst_offset = ptd_offset + slot * sizeof(*ptd);

	isp1760_mem_write(base, dst_offset + sizeof(ptd->dw0),
			  (__force u32 *)&ptd->dw1, 7 * sizeof(ptd->dw1));
	/*
	 * Make sure dw0 gets written last (after other dw's and after payload)
	 *  since it contains the enable bit
	 */
	wmb();
	isp1760_mem_write(base, dst_offset, (__force u32 *)&ptd->dw0,
			  sizeof(ptd->dw0));
}

static void ptd_write(struct usb_hcd *hcd, u32 ptd_offset, u32 slot,
		      struct ptd *ptd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);

	if (!priv->is_isp1763)
		return isp1760_ptd_write(priv->base, ptd_offset, slot, ptd);

	isp1763_ptd_write(hcd, ptd_offset, slot, ptd);
}

/* memory management of the 60kb on the chip from 0x1000 to 0xffff */
static void init_memory(struct isp1760_hcd *priv)
{
	const struct isp1760_memory_layout *mem = priv->memory_layout;
	int i, j, curr;
	u32 payload_addr;

	payload_addr = PAYLOAD_OFFSET;

	for (i = 0, curr = 0; i < ARRAY_SIZE(mem->blocks); i++, curr += j) {
		for (j = 0; j < mem->blocks[i]; j++) {
			priv->memory_pool[curr + j].start = payload_addr;
			priv->memory_pool[curr + j].size = mem->blocks_size[i];
			priv->memory_pool[curr + j].free = 1;
			payload_addr += priv->memory_pool[curr + j].size;
		}
	}

	WARN_ON(payload_addr - priv->memory_pool[0].start >
		mem->payload_area_size);
}

static void alloc_mem(struct usb_hcd *hcd, struct isp1760_qtd *qtd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	const struct isp1760_memory_layout *mem = priv->memory_layout;
	int i;

	WARN_ON(qtd->payload_addr);

	if (!qtd->length)
		return;

	for (i = 0; i < mem->payload_blocks; i++) {
		if (priv->memory_pool[i].size >= qtd->length &&
		    priv->memory_pool[i].free) {
			priv->memory_pool[i].free = 0;
			qtd->payload_addr = priv->memory_pool[i].start;
			return;
		}
	}
}

static void free_mem(struct usb_hcd *hcd, struct isp1760_qtd *qtd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	const struct isp1760_memory_layout *mem = priv->memory_layout;
	int i;

	if (!qtd->payload_addr)
		return;

	for (i = 0; i < mem->payload_blocks; i++) {
		if (priv->memory_pool[i].start == qtd->payload_addr) {
			WARN_ON(priv->memory_pool[i].free);
			priv->memory_pool[i].free = 1;
			qtd->payload_addr = 0;
			return;
		}
	}

	WARN_ON(1);
	qtd->payload_addr = 0;
}

/* reset a non-running (STS_HALT == 1) controller */
static int ehci_reset(struct usb_hcd *hcd)
{
	return isp1760_hcd_set_and_wait_swap(hcd, CMD_RESET, 250 * 1000);
}

static struct isp1760_qh *qh_alloc(gfp_t flags)
{
	struct isp1760_qh *qh;

	qh = kmem_cache_alloc(qh_cachep, flags);
	if (!qh)
		return NULL;

	memset(qh, '\0', qh_cachep->sz);
	INIT_LIST_HEAD(&qh->qh_list);
	INIT_LIST_HEAD(&qh->qtd_list);
	qh->slot = -1;

	return qh;
}

static void qh_free(struct isp1760_qh *qh)
{
	WARN_ON(!list_empty(&qh->qtd_list));
	WARN_ON(qh->slot > -1);
	kmem_cache_free(qh_cachep, qh);
}

/* one-time init, only for memory state */
static int priv_init(struct usb_hcd *hcd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	u32 isoc_cache;
	u32 isoc_thres;
	int i;

	for (i = 0; i < QH_END; i++)
		INIT_LIST_HEAD(&priv->qh_list[i]);

	/*
	 * hw default: 1K periodic list heads, one per frame.
	 * periodic_size can shrink by USBCMD update if hcc_params allows.
	 */
	priv->periodic_size = DEFAULT_I_TDPS;

	if (priv->is_isp1763) {
		priv->i_thresh = 2;
		return 0;
	}

	/* controllers may cache some of the periodic schedule ... */
	isoc_cache = isp1760_hcd_read(hcd, HCC_ISOC_CACHE);
	isoc_thres = isp1760_hcd_read(hcd, HCC_ISOC_THRES);

	/* full frame cache */
	if (isoc_cache)
		priv->i_thresh = 8;
	else /* N microframes cached */
		priv->i_thresh = 2 + isoc_thres;

	return 0;
}

static int isp1760_hc_setup(struct usb_hcd *hcd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	u32 atx_reset;
	int result;
	u32 scratch;
	u32 pattern;

	if (priv->is_isp1763)
		pattern = 0xcafe;
	else
		pattern = 0xdeadcafe;

	isp1760_hcd_write(hcd, HC_SCRATCH, pattern);

	/* Change bus pattern */
	isp1760_hcd_read(hcd, HC_CHIP_ID_HIGH);
	scratch = isp1760_hcd_read(hcd, HC_SCRATCH);
	if (scratch != pattern) {
		printf("Scratch test failed. 0x%08x\n", scratch);
		return -ENODEV;
	}

	/*
	 * The RESET_HC bit in the SW_RESET register is supposed to reset the
	 * host controller without touching the CPU interface registers, but at
	 * least on the ISP1761 it seems to behave as the RESET_ALL bit and
	 * reset the whole device. We thus can't use it here, so let's reset
	 * the host controller through the EHCI USB Command register. The device
	 * has been reset in core code anyway, so this shouldn't matter.
	 */
	isp1760_hcd_clear(hcd, ISO_BUF_FILL);
	isp1760_hcd_clear(hcd, INT_BUF_FILL);
	isp1760_hcd_clear(hcd, ATL_BUF_FILL);

	isp1760_hcd_set(hcd, HC_ATL_PTD_SKIPMAP);
	isp1760_hcd_set(hcd, HC_INT_PTD_SKIPMAP);
	isp1760_hcd_set(hcd, HC_ISO_PTD_SKIPMAP);

	result = ehci_reset(hcd);
	if (result)
		return result;

	/* Step 11 passed */

	/* ATL reset */
	if (priv->is_isp1763)
		atx_reset = SW_RESET_RESET_ATX;
	else
		atx_reset = ALL_ATX_RESET;

	isp1760_hcd_set(hcd, atx_reset);
	mdelay(10);
	isp1760_hcd_clear(hcd, atx_reset);

	if (priv->is_isp1763) {
		isp1760_hcd_set(hcd, HW_OTG_DISABLE);
		isp1760_hcd_set(hcd, HW_SW_SEL_HC_DC_CLEAR);
		isp1760_hcd_set(hcd, HW_HC_2_DIS_CLEAR);
		isp1760_hcd_set(hcd, HW_DM_PULLDOWN);
		isp1760_hcd_set(hcd, HW_DP_PULLDOWN);
		mdelay(10);

		isp1760_hcd_set(hcd, HW_INTF_LOCK);
	}

	isp1760_hcd_set(hcd, HC_INT_IRQ_ENABLE);
	isp1760_hcd_set(hcd, HC_ATL_IRQ_ENABLE);

	return priv_init(hcd);
}

static u32 base_to_chip(u32 base)
{
	return ((base - 0x400) >> 3);
}

static int last_qtd_of_urb(struct isp1760_qtd *qtd, struct isp1760_qh *qh)
{
	struct urb *urb;

	if (list_is_last(&qtd->qtd_list, &qh->qtd_list))
		return 1;

	urb = qtd->urb;
	qtd = list_entry(qtd->qtd_list.next, typeof(*qtd), qtd_list);

	return (qtd->urb != urb);
}

/* magic numbers that can affect system performance */
#define	EHCI_TUNE_CERR		3	/* 0-3 qtd retries; 0 == don't stop */
#define	EHCI_TUNE_RL_HS		4	/* nak throttle; see 4.9 */
#define	EHCI_TUNE_RL_TT		0
#define	EHCI_TUNE_MULT_HS	1	/* 1-3 transactions/uframe; 4.10.3 */
#define	EHCI_TUNE_MULT_TT	1
#define	EHCI_TUNE_FLS		2	/* (small) 256 frame schedule */

static void create_ptd_atl(struct isp1760_qh *qh, struct isp1760_qtd *qtd,
			   struct ptd *ptd)
{
	u32 maxpacket;
	u32 multi;
	u32 rl = RL_COUNTER;
	u32 nak = NAK_COUNTER;
	u8 portnr;
	u8 hubaddr;

	memset(ptd, 0, sizeof(*ptd));

	/* according to 3.6.2, max packet len can not be > 0x400 */
	maxpacket = usb_maxpacket(qtd->urb->dev, qtd->urb->pipe);
	multi =  1 + ((maxpacket >> 11) & 0x3);
	maxpacket &= 0x7ff;

	/* DW0 */
	ptd->dw0 = DW0_VALID_BIT;
	ptd->dw0 |= TO_DW0_LENGTH(qtd->length);
	ptd->dw0 |= TO_DW0_MAXPACKET(maxpacket);
	ptd->dw0 |= TO_DW0_ENDPOINT(usb_pipeendpoint(qtd->urb->pipe));

	/* DW1 */
	ptd->dw1 = TO_DW((usb_pipeendpoint(qtd->urb->pipe) >> 1));
	ptd->dw1 |= TO_DW1_DEVICE_ADDR(usb_pipedevice(qtd->urb->pipe));
	ptd->dw1 |= TO_DW1_PID_TOKEN(qtd->packet_type);

	if (usb_pipebulk(qtd->urb->pipe))
		ptd->dw1 |= DW1_TRANS_BULK;
	else if  (usb_pipeint(qtd->urb->pipe))
		ptd->dw1 |= DW1_TRANS_INT;

	if (qtd->urb->dev->speed != USB_SPEED_HIGH) {
		/* split transaction */

		ptd->dw1 |= DW1_TRANS_SPLIT;
		if (qtd->urb->dev->speed == USB_SPEED_LOW)
			ptd->dw1 |= DW1_SE_USB_LOSPEED;

		if (!qtd->urb->dev->dev->parent_priv_) {
			portnr = qtd->urb->dev->portnr;
			hubaddr = qtd->urb->dev->devnum;
		} else {
			usb_find_usb2_hub_address_port(qtd->urb->dev, &hubaddr,
						       &portnr);
		}

		ptd->dw1 |= TO_DW1_PORT_NUM(portnr);
		ptd->dw1 |= TO_DW1_HUB_NUM(hubaddr);

		/* SE bit for Split INT transfers */
		if (usb_pipeint(qtd->urb->pipe) &&
		    qtd->urb->dev->speed == USB_SPEED_LOW)
			ptd->dw1 |= DW1_SE_USB_LOSPEED;

		rl = 0;
		nak = 0;
	} else {
		ptd->dw0 |= TO_DW0_MULTI(multi);
		if (usb_pipecontrol(qtd->urb->pipe) ||
		    usb_pipebulk(qtd->urb->pipe))
			ptd->dw3 |= TO_DW3_PING(qh->ping);
	}
	/* DW2 */
	ptd->dw2 = 0;
	ptd->dw2 |= TO_DW2_DATA_START_ADDR(base_to_chip(qtd->payload_addr));
	ptd->dw2 |= TO_DW2_RL(rl);

	/* DW3 */
	ptd->dw3 |= TO_DW3_NAKCOUNT(nak);
	ptd->dw3 |= TO_DW3_DATA_TOGGLE(qh->toggle);

	if (usb_pipecontrol(qtd->urb->pipe)) {
		if (qtd->data_buffer == qtd->urb->setup_packet)
			ptd->dw3 &= ~TO_DW3_DATA_TOGGLE(1);
		else if (last_qtd_of_urb(qtd, qh))
			ptd->dw3 |= TO_DW3_DATA_TOGGLE(1);
	}

	ptd->dw3 |= DW3_ACTIVE_BIT;
	/* Cerr */
	ptd->dw3 |= TO_DW3_CERR(ERR_COUNTER);
}

static void transform_add_int(struct isp1760_qh *qh, struct isp1760_qtd *qtd,
			      struct ptd *ptd)
{
	struct usb_host_endpoint *hep = qtd->urb->ep;
	struct usb_endpoint_descriptor *epd = &hep->desc;
	u32 usof;
	u32 period;

	/*
	 * Most of this is guessing. ISP1761 datasheet is quite unclear, and
	 * the algorithm from the original Philips driver code, which was
	 * pretty much used in this driver before as well, is quite horrendous
	 * and, i believe, incorrect. The code below follows the datasheet and
	 * USB2.0 spec as far as I can tell, and plug/unplug seems to be much
	 * more reliable this way (fingers crossed...).
	 */

	if (qtd->urb->dev->speed == USB_SPEED_HIGH) {
		/* urb->interval is in units of microframes (1/8 ms) */
		period = epd->bInterval >> 3;

		if (epd->bInterval > 4)
			usof = 0x01; /* One bit set => interval 1 ms * uFrame-match */
		else if (epd->bInterval > 2)
			usof = 0x22; /* Two bits set => interval 1/2 ms */
		else if (epd->bInterval > 1)
			usof = 0x55; /* Four bits set => interval 1/4 ms */
		else
			usof = 0xff; /* All bits set => interval 1/8 ms */
	} else {
		/* urb->interval is in units of frames (1 ms) */
		period = epd->bInterval;
		/* Execute Start Split on any of the four first uFrames */
		usof = 0x0f;

		/*
		 * First 8 bits in dw5 is uSCS and "specifies which uSOF the
		 * complete split needs to be sent. Valid only for IN." Also,
		 * "All bits can be set to one for every transfer." (p 82,
		 * ISP1761 data sheet.) 0x1c is from Philips driver. Where did
		 * that number come from? 0xff seems to work fine...
		 */
		/* Execute Complete Split on any uFrame */
		ptd->dw5 = TO_DW(0xff);
	}

	/* Ensure equal or shorter period than requested */
	period = period >> 1;
	/* Mask off too large values and lowest unused 3 bits */
	period &= 0xf8;

	ptd->dw2 |= TO_DW(period);
	ptd->dw4 = TO_DW(usof);
}

static void create_ptd_int(struct isp1760_qh *qh, struct isp1760_qtd *qtd,
			   struct ptd *ptd)
{
	create_ptd_atl(qh, qtd, ptd);
	transform_add_int(qh, qtd, ptd);
}

static void isp1760_urb_done(struct usb_hcd *hcd, struct urb *urb)
{
	if (usb_pipein(urb->pipe) && usb_pipetype(urb->pipe) != PIPE_CONTROL) {
		void *ptr;

		for (ptr = urb->transfer_buffer;
		     ptr < urb->transfer_buffer + urb->transfer_buffer_length;
		     ptr += PAGE_SIZE)
			flush_dcache_range((unsigned  long)ptr,
					   (unsigned long)ptr + PAGE_SIZE);
	}

	/* complete() can reenter this HCD */
	usb_hcd_unlink_urb_from_ep(hcd, urb);
	usb_hcd_giveback_urb(hcd, urb, urb->status);
}

static struct isp1760_qtd *qtd_alloc(gfp_t flags, struct urb *urb,
				     u8 packet_type)
{
	struct isp1760_qtd *qtd;

	qtd = kmem_cache_alloc(qtd_cachep, flags);
	if (!qtd)
		return NULL;

	memset(qtd, '\0', sizeof(*qtd));
	INIT_LIST_HEAD(&qtd->qtd_list);
	qtd->urb = urb;
	qtd->packet_type = packet_type;
	qtd->status = QTD_ENQUEUED;
	qtd->actual_length = 0;

	return qtd;
}

static void qtd_free(struct isp1760_qtd *qtd)
{
	WARN_ON(qtd->payload_addr);
	kmem_cache_free(qtd_cachep, qtd);
}

static void start_bus_transfer(struct usb_hcd *hcd, u32 ptd_offset, int slot,
			       struct isp1760_slotinfo *slots,
			       struct isp1760_qtd *qtd, struct isp1760_qh *qh,
			       struct ptd *ptd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	const struct isp1760_memory_layout *mem = priv->memory_layout;
	int skip_map;

	WARN_ON((slot < 0) || (slot > mem->slot_num - 1));
	WARN_ON(qtd->length && !qtd->payload_addr);
	WARN_ON(slots[slot].qtd);
	WARN_ON(slots[slot].qh);
	WARN_ON(qtd->status != QTD_PAYLOAD_ALLOC);

	if (priv->is_isp1763)
		ndelay(100);

	/* Make sure done map has not triggered from some unlinked transfer */
	if (ptd_offset == ATL_PTD_OFFSET) {
		skip_map = isp1760_hcd_read(hcd, HC_ATL_PTD_SKIPMAP);
		isp1760_hcd_write(hcd, HC_ATL_PTD_SKIPMAP,
				  skip_map | (1 << slot));
		priv->atl_done_map |= isp1760_hcd_read(hcd, HC_ATL_PTD_DONEMAP);
		priv->atl_done_map &= ~(1 << slot);
	} else {
		skip_map = isp1760_hcd_read(hcd, HC_INT_PTD_SKIPMAP);
		isp1760_hcd_write(hcd, HC_INT_PTD_SKIPMAP,
				  skip_map | (1 << slot));
		priv->int_done_map |= isp1760_hcd_read(hcd, HC_INT_PTD_DONEMAP);
		priv->int_done_map &= ~(1 << slot);
	}

	skip_map &= ~(1 << slot);
	qh->slot = slot;
	qtd->status = QTD_XFER_STARTED;
	slots[slot].qtd = qtd;
	slots[slot].qh = qh;

	ptd_write(hcd, ptd_offset, slot, ptd);

	if (ptd_offset == ATL_PTD_OFFSET)
		isp1760_hcd_write(hcd, HC_ATL_PTD_SKIPMAP, skip_map);
	else
		isp1760_hcd_write(hcd, HC_INT_PTD_SKIPMAP, skip_map);
}

static int is_short_bulk(struct isp1760_qtd *qtd)
{
	return (usb_pipebulk(qtd->urb->pipe) &&
		(qtd->actual_length < qtd->length));
}

static void collect_qtds(struct usb_hcd *hcd, struct isp1760_qh *qh,
			 struct list_head *urb_list)
{
	struct isp1760_qtd *qtd, *qtd_next;
	struct urb_listitem *urb_listitem;
	int last_qtd;

	list_for_each_entry_safe(qtd, qtd_next, &qh->qtd_list, qtd_list) {
		if (qtd->status < QTD_XFER_COMPLETE)
			break;

		last_qtd = last_qtd_of_urb(qtd, qh);

		if (!last_qtd && qtd->status == QTD_RETIRE)
			qtd_next->status = QTD_RETIRE;

		if (qtd->status == QTD_XFER_COMPLETE) {
			if (qtd->actual_length) {
				switch (qtd->packet_type) {
				case IN_PID:
					mem_read(hcd, qtd->payload_addr,
						 qtd->data_buffer,
						 qtd->actual_length);
					fallthrough;
				case OUT_PID:
					qtd->urb->actual_length +=
							qtd->actual_length;
					fallthrough;
				case SETUP_PID:
					break;
				}
			}

			if (is_short_bulk(qtd)) {
				if (qtd->urb->transfer_flags & URB_SHORT_NOT_OK)
					qtd->urb->status = -EREMOTEIO;
				if (!last_qtd)
					qtd_next->status = QTD_RETIRE;
			}
		}

		if (qtd->payload_addr)
			free_mem(hcd, qtd);

		if (last_qtd) {
			if (qtd->status == QTD_RETIRE &&
			    qtd->urb->status == -EINPROGRESS)
				qtd->urb->status = -EPIPE;

			/* Defer calling of urb_done() since it releases lock */
			urb_listitem = kmem_cache_alloc(urb_listitem_cachep,
							GFP_ATOMIC);
			if (unlikely(!urb_listitem))
				break; /* Try again on next call */
			urb_listitem->urb = qtd->urb;
			list_add_tail(&urb_listitem->urb_list, urb_list);
		}

		list_del(&qtd->qtd_list);
		qtd_free(qtd);
	}
}

#define ENQUEUE_DEPTH	2
static void enqueue_qtds(struct usb_hcd *hcd, struct isp1760_qh *qh)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	const struct isp1760_memory_layout *mem = priv->memory_layout;
	int slot_num = mem->slot_num;
	int ptd_offset;
	struct isp1760_slotinfo *slots;
	int curr_slot, free_slot;
	int n;
	struct ptd ptd;
	struct isp1760_qtd *qtd;

	if (unlikely(list_empty(&qh->qtd_list)))
		return;

	/* Make sure this endpoint's TT buffer is clean before queueing ptds */
	if (qh->tt_buffer_dirty)
		return;

	if (usb_pipeint(list_entry(qh->qtd_list.next, struct isp1760_qtd,
				   qtd_list)->urb->pipe)) {
		ptd_offset = INT_PTD_OFFSET;
		slots = priv->int_slots;
	} else {
		ptd_offset = ATL_PTD_OFFSET;
		slots = priv->atl_slots;
	}

	free_slot = -1;
	for (curr_slot = 0; curr_slot < slot_num; curr_slot++) {
		if (free_slot == -1 && !slots[curr_slot].qtd)
			free_slot = curr_slot;
		if (slots[curr_slot].qh == qh)
			break;
	}

	n = 0;
	list_for_each_entry(qtd, &qh->qtd_list, qtd_list) {
		if (qtd->status == QTD_ENQUEUED) {
			WARN_ON(qtd->payload_addr);
			alloc_mem(hcd, qtd);
			if (qtd->length && !qtd->payload_addr)
				break;

			if (qtd->length && (qtd->packet_type == SETUP_PID ||
					    qtd->packet_type == OUT_PID)) {
				mem_write(hcd, qtd->payload_addr,
					  qtd->data_buffer, qtd->length);
			}

			qtd->status = QTD_PAYLOAD_ALLOC;
		}

		if (qtd->status == QTD_PAYLOAD_ALLOC) {
			/* Start xfer for this endpoint if not already done */
			if ((curr_slot > slot_num - 1) && (free_slot > -1)) {
				if (usb_pipeint(qtd->urb->pipe))
					create_ptd_int(qh, qtd, &ptd);
				else
					create_ptd_atl(qh, qtd, &ptd);

				start_bus_transfer(hcd, ptd_offset, free_slot,
						   slots, qtd, qh, &ptd);
				curr_slot = free_slot;
			}

			n++;
			if (n >= ENQUEUE_DEPTH)
				break;
		}
	}
}

static void schedule_ptds(struct usb_hcd *hcd)
{
	struct isp1760_hcd *priv;
	struct isp1760_qh *qh, *qh_next;
	struct list_head *ep_queue;
	LIST_HEAD(urb_list);
	struct urb_listitem *urb_listitem, *urb_listitem_next;
	int i;

	if (!hcd) {
		WARN_ON(1);
		return;
	}

	priv = hcd_to_priv(hcd);

	/*
	 * check finished/retired xfers, transfer payloads, call urb_done()
	 */
	for (i = 0; i < QH_END; i++) {
		ep_queue = &priv->qh_list[i];
		list_for_each_entry_safe(qh, qh_next, ep_queue, qh_list)
			collect_qtds(hcd, qh, &urb_list);
	}

	list_for_each_entry_safe(urb_listitem, urb_listitem_next, &urb_list,
				 urb_list) {
		isp1760_urb_done(hcd, urb_listitem->urb);
		kmem_cache_free(urb_listitem_cachep, urb_listitem);
	}

	/*
	 * Schedule packets for transfer.
	 *
	 * According to USB2.0 specification:
	 *
	 * 1st prio: interrupt xfers, up to 80 % of bandwidth
	 * 2nd prio: control xfers
	 * 3rd prio: bulk xfers
	 *
	 * ... but let's use a simpler scheme here (mostly because ISP1761 doc
	 * is very unclear on how to prioritize traffic):
	 *
	 * 1) Enqueue any queued control transfers, as long as payload chip mem
	 *    and PTD ATL slots are available.
	 * 2) Enqueue any queued INT transfers, as long as payload chip mem
	 *    and PTD INT slots are available.
	 * 3) Enqueue any queued bulk transfers, as long as payload chip mem
	 *    and PTD ATL slots are available.
	 *
	 * Use double buffering (ENQUEUE_DEPTH==2) as a compromise between
	 * conservation of chip mem and performance.
	 *
	 * I'm sure this scheme could be improved upon!
	 */
	for (i = 0; i < QH_END; i++) {
		ep_queue = &priv->qh_list[i];
		list_for_each_entry_safe(qh, qh_next, ep_queue, qh_list)
			enqueue_qtds(hcd, qh);
	}
}

#define PTD_STATE_QTD_DONE	1
#define PTD_STATE_QTD_RELOAD	2
#define PTD_STATE_URB_RETIRE	3

static int check_int_transfer(struct usb_hcd *hcd, struct ptd *ptd,
			      struct urb *urb)
{
	u32 dw4;
	int i;

	dw4 = TO_U32(ptd->dw4);
	dw4 >>= 8;

	/*
	 * FIXME: ISP1761 datasheet does not say what to do with these. Do we
	 * need to handle these errors? Is it done in hardware?
	 */
	if (ptd->dw3 & DW3_HALT_BIT) {
		urb->status = -EPROTO; /* Default unknown error */

		for (i = 0; i < 8; i++) {
			switch (dw4 & 0x7) {
			case INT_UNDERRUN:
				printf("underrun during uFrame %d\n", i);
				urb->status = -ECOMM; /* Could not write data */
				break;
			case INT_EXACT:
				printf("transaction error uFrame %d\n", i);
				/* timeout, bad CRC, PID error etc. */
				urb->status = -EPROTO;
				break;
			case INT_BABBLE:
				printf("babble error during uFrame %d\n", i);
				urb->status = -EOVERFLOW;
				break;
			}
			dw4 >>= 3;
		}

		return PTD_STATE_URB_RETIRE;
	}

	return PTD_STATE_QTD_DONE;
}

static int check_atl_transfer(struct usb_hcd *hcd, struct ptd *ptd,
			      struct urb *urb)
{
	WARN_ON(!ptd);
	if (ptd->dw3 & DW3_HALT_BIT) {
		if (ptd->dw3 & DW3_BABBLE_BIT)
			urb->status = -EOVERFLOW;
		else if (FROM_DW3_CERR(ptd->dw3))
			urb->status = -EPIPE;  /* Stall */
		else
			urb->status = -EPROTO; /* Unknown */

		/*
		 * useful debug
		 * printf("%s: ptd error:\n"
		 * "        dw0: %08x dw1: %08x dw2: %08x dw3: %08x\n"
		 * "        dw4: %08x dw5: %08x dw6: %08x dw7: %08x\n",
		 * __func__,
		 * ptd->dw0, ptd->dw1, ptd->dw2, ptd->dw3,
		 * ptd->dw4, ptd->dw5, ptd->dw6, ptd->dw7);
		 */

		return PTD_STATE_URB_RETIRE;
	}

	/* Transfer Error, *but* active and no HALT -> reload */
	if ((ptd->dw3 & DW3_ERROR_BIT) && (ptd->dw3 & DW3_ACTIVE_BIT))
		return PTD_STATE_QTD_RELOAD;

	/*
	 * NAKs are handled in HW by the chip. Usually if the
	 * device is not able to send data fast enough.
	 * This happens mostly on slower hardware.
	 */
	if (!FROM_DW3_NAKCOUNT(ptd->dw3) && (ptd->dw3 & DW3_ACTIVE_BIT))
		return PTD_STATE_QTD_RELOAD;

	return PTD_STATE_QTD_DONE;
}

static void handle_done_ptds(struct usb_hcd *hcd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	struct isp1760_slotinfo *slots;
	struct isp1760_qtd *qtd;
	struct isp1760_qh *qh;
	struct ptd ptd;
	u32 ptd_offset;
	int modified;
	int skip_map;
	int state;
	int slot;

	skip_map = isp1760_hcd_read(hcd, HC_INT_PTD_SKIPMAP);
	priv->int_done_map &= ~skip_map;
	skip_map = isp1760_hcd_read(hcd, HC_ATL_PTD_SKIPMAP);
	priv->atl_done_map &= ~skip_map;

	modified = priv->int_done_map || priv->atl_done_map;

	while (priv->int_done_map || priv->atl_done_map) {
		if (priv->int_done_map) {
			/* INT ptd */
			slot = __ffs(priv->int_done_map);
			priv->int_done_map &= ~(1 << slot);
			slots = priv->int_slots;
			/*
			 * This should not trigger, and could be removed if
			 * noone have any problems with it triggering:
			 */
			if (!slots[slot].qh) {
				WARN_ON(1);
				continue;
			}
			ptd_offset = INT_PTD_OFFSET;
			ptd_read(hcd, INT_PTD_OFFSET, slot, &ptd);
			state = check_int_transfer(hcd, &ptd,
						   slots[slot].qtd->urb);
		} else {
			/* ATL ptd */
			slot = __ffs(priv->atl_done_map);
			priv->atl_done_map &= ~(1 << slot);
			slots = priv->atl_slots;
			/*
			 * This should not trigger, and could be removed if
			 * noone have any problems with it triggering:
			 */
			if (!slots[slot].qh) {
				WARN_ON(1);
				continue;
			}
			ptd_offset = ATL_PTD_OFFSET;
			ptd_read(hcd, ATL_PTD_OFFSET, slot, &ptd);
			state = check_atl_transfer(hcd, &ptd,
						   slots[slot].qtd->urb);
		}

		qtd = slots[slot].qtd;
		slots[slot].qtd = NULL;
		qh = slots[slot].qh;
		slots[slot].qh = NULL;
		qh->slot = -1;

		WARN_ON(qtd->status != QTD_XFER_STARTED);

		switch (state) {
		case PTD_STATE_QTD_DONE:
			if (usb_pipeint(qtd->urb->pipe) &&
			    qtd->urb->dev->speed != USB_SPEED_HIGH)
				qtd->actual_length =
				       FROM_DW3_SCS_NRBYTESTRANSFERRED(ptd.dw3);
			else
				qtd->actual_length =
					FROM_DW3_NRBYTESTRANSFERRED(ptd.dw3);

			qtd->status = QTD_XFER_COMPLETE;

			if (list_is_last(&qtd->qtd_list, &qh->qtd_list) ||
			    is_short_bulk(qtd))
				qtd = NULL;
			else
				qtd = list_entry(qtd->qtd_list.next,
						 typeof(*qtd), qtd_list);

			qh->toggle = FROM_DW3_DATA_TOGGLE(ptd.dw3);
			qh->ping = FROM_DW3_PING(ptd.dw3);

			break;

		case PTD_STATE_QTD_RELOAD: /* QTD_RETRY, for atls only */
			qtd->status = QTD_PAYLOAD_ALLOC;
			ptd.dw0 |= DW0_VALID_BIT;
			/* RL counter = ERR counter */
			ptd.dw3 &= ~TO_DW3_NAKCOUNT(0xf);
			ptd.dw3 |= TO_DW3_NAKCOUNT(FROM_DW2_RL(ptd.dw2));
			ptd.dw3 &= ~TO_DW3_CERR(3);
			ptd.dw3 |= TO_DW3_CERR(ERR_COUNTER);

			qh->toggle = FROM_DW3_DATA_TOGGLE(ptd.dw3);
			qh->ping = FROM_DW3_PING(ptd.dw3);
			break;

		case PTD_STATE_URB_RETIRE:
			qtd->status = QTD_RETIRE;
			qtd = NULL;
			qh->toggle = 0;
			qh->ping = 0;
			break;

		default:
			WARN_ON(1);
			continue;
		}

		if (qtd && qtd->status == QTD_PAYLOAD_ALLOC) {
			if (slots == priv->int_slots) {
				if (state == PTD_STATE_QTD_RELOAD)
					dev_err(priv->dev,
						"PTD_STATE_QTD_RELOAD on interrupt packet\n");
				if (state != PTD_STATE_QTD_RELOAD)
					create_ptd_int(qh, qtd, &ptd);
			} else {
				if (state != PTD_STATE_QTD_RELOAD)
					create_ptd_atl(qh, qtd, &ptd);
			}

			start_bus_transfer(hcd, ptd_offset, slot, slots, qtd,
					   qh, &ptd);
		}
	}

	if (modified)
		schedule_ptds(hcd);
}

static irqreturn_t isp1760_irq(int irq, void *__hci)
{
	struct usb_hcd *hcd = __hci;
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	irqreturn_t irqret = IRQ_NONE;
	u32 int_reg;
	u32 imask;

	imask = isp1760_hcd_read(hcd, HC_INTERRUPT);
	if (unlikely(!imask))
		return irqret;

	int_reg = priv->is_isp1763 ? ISP1763_HC_INTERRUPT :
		ISP176x_HC_INTERRUPT;
	isp1760_reg_write(priv->regs, int_reg, imask);

	priv->int_done_map |= isp1760_hcd_read(hcd, HC_INT_PTD_DONEMAP);
	priv->atl_done_map |= isp1760_hcd_read(hcd, HC_ATL_PTD_DONEMAP);

	handle_done_ptds(hcd);

	irqret = IRQ_HANDLED;

	return irqret;
}

static int isp1763_run(struct usb_hcd *hcd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	int retval;
	u32 chipid_h;
	u32 chipid_l;
	u32 chip_rev;
	u32 ptd_atl_int;
	u32 ptd_iso;

	chipid_h = isp1760_hcd_read(hcd, HC_CHIP_ID_HIGH);
	chipid_l = isp1760_hcd_read(hcd, HC_CHIP_ID_LOW);
	chip_rev = isp1760_hcd_read(hcd, HC_CHIP_REV);
	printf("USB ISP %02x%02x HW rev. %d started\n", chipid_h,
	       chipid_l, chip_rev);

	isp1760_hcd_clear(hcd, ISO_BUF_FILL);
	isp1760_hcd_clear(hcd, INT_BUF_FILL);
	isp1760_hcd_clear(hcd, ATL_BUF_FILL);

	isp1760_hcd_set(hcd, HC_ATL_PTD_SKIPMAP);
	isp1760_hcd_set(hcd, HC_INT_PTD_SKIPMAP);
	isp1760_hcd_set(hcd, HC_ISO_PTD_SKIPMAP);
	ndelay(100);
	isp1760_hcd_clear(hcd, HC_ATL_PTD_DONEMAP);
	isp1760_hcd_clear(hcd, HC_INT_PTD_DONEMAP);
	isp1760_hcd_clear(hcd, HC_ISO_PTD_DONEMAP);

	isp1760_hcd_set(hcd, HW_OTG_DISABLE);
	isp1760_reg_write(priv->regs, ISP1763_HC_OTG_CTRL_CLEAR, BIT(7));
	isp1760_reg_write(priv->regs, ISP1763_HC_OTG_CTRL_CLEAR, BIT(15));
	mdelay(10);

	isp1760_hcd_set(hcd, HC_INT_IRQ_ENABLE);
	isp1760_hcd_set(hcd, HC_ATL_IRQ_ENABLE);

	isp1760_hcd_set(hcd, HW_GLOBAL_INTR_EN);

	isp1760_hcd_clear(hcd, HC_ATL_IRQ_MASK_AND);
	isp1760_hcd_clear(hcd, HC_INT_IRQ_MASK_AND);
	isp1760_hcd_clear(hcd, HC_ISO_IRQ_MASK_AND);

	isp1760_hcd_set(hcd, HC_ATL_IRQ_MASK_OR);
	isp1760_hcd_set(hcd, HC_INT_IRQ_MASK_OR);
	isp1760_hcd_set(hcd, HC_ISO_IRQ_MASK_OR);

	ptd_atl_int = 0x8000;
	ptd_iso = 0x0001;

	isp1760_hcd_write(hcd, HC_ATL_PTD_LASTPTD, ptd_atl_int);
	isp1760_hcd_write(hcd, HC_INT_PTD_LASTPTD, ptd_atl_int);
	isp1760_hcd_write(hcd, HC_ISO_PTD_LASTPTD, ptd_iso);

	isp1760_hcd_set(hcd, ATL_BUF_FILL);
	isp1760_hcd_set(hcd, INT_BUF_FILL);

	isp1760_hcd_clear(hcd, CMD_LRESET);
	isp1760_hcd_clear(hcd, CMD_RESET);

	retval = isp1760_hcd_set_and_wait(hcd, CMD_RUN, 250 * 1000);
	if (retval)
		return retval;

	down_write(&ehci_cf_port_reset_rwsem);
	retval = isp1760_hcd_set_and_wait(hcd, FLAG_CF, 250 * 1000);
	up_write(&ehci_cf_port_reset_rwsem);
	if (retval)
		return retval;

	return 0;
}

static int isp1760_run(struct usb_hcd *hcd)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	int retval;
	u32 chipid_h;
	u32 chipid_l;
	u32 chip_rev;
	u32 ptd_atl_int;
	u32 ptd_iso;

	/*
	 * ISP1763 have some differences in the setup and order to enable
	 * the ports, disable otg, setup buffers, and ATL, INT, ISO status.
	 * So, just handle it a separate sequence.
	 */
	if (priv->is_isp1763)
		return isp1763_run(hcd);

	/* Set PTD interrupt AND & OR maps */
	isp1760_hcd_clear(hcd, HC_ATL_IRQ_MASK_AND);
	isp1760_hcd_clear(hcd, HC_INT_IRQ_MASK_AND);
	isp1760_hcd_clear(hcd, HC_ISO_IRQ_MASK_AND);

	isp1760_hcd_set(hcd, HC_ATL_IRQ_MASK_OR);
	isp1760_hcd_set(hcd, HC_INT_IRQ_MASK_OR);
	isp1760_hcd_set(hcd, HC_ISO_IRQ_MASK_OR);

	/* step 23 passed */

	isp1760_hcd_set(hcd, HW_GLOBAL_INTR_EN);

	isp1760_hcd_clear(hcd, CMD_LRESET);
	isp1760_hcd_clear(hcd, CMD_RESET);

	retval = isp1760_hcd_set_and_wait(hcd, CMD_RUN, 250 * 1000);
	if (retval)
		return retval;

	/*
	 * XXX
	 * Spec says to write FLAG_CF as last config action, priv code grabs
	 * the semaphore while doing so.
	 */
	down_write(&ehci_cf_port_reset_rwsem);

	retval = isp1760_hcd_set_and_wait(hcd, FLAG_CF, 250 * 1000);
	up_write(&ehci_cf_port_reset_rwsem);
	if (retval)
		return retval;

	chipid_h = isp1760_hcd_read(hcd, HC_CHIP_ID_HIGH);
	chipid_l = isp1760_hcd_read(hcd, HC_CHIP_ID_LOW);
	chip_rev = isp1760_hcd_read(hcd, HC_CHIP_REV);
	dev_info(priv->dev, "USB ISP %02x%02x HW rev. %d started\n",
		 chipid_h, chipid_l, chip_rev);

	/* PTD Register Init Part 2, Step 28 */

	/* Setup registers controlling PTD checking */
	ptd_atl_int = 0x80000000;
	ptd_iso = 0x00000001;

	isp1760_hcd_write(hcd, HC_ATL_PTD_LASTPTD, ptd_atl_int);
	isp1760_hcd_write(hcd, HC_INT_PTD_LASTPTD, ptd_atl_int);
	isp1760_hcd_write(hcd, HC_ISO_PTD_LASTPTD, ptd_iso);

	isp1760_hcd_set(hcd, HC_ATL_PTD_SKIPMAP);
	isp1760_hcd_set(hcd, HC_INT_PTD_SKIPMAP);
	isp1760_hcd_set(hcd, HC_ISO_PTD_SKIPMAP);

	isp1760_hcd_set(hcd, ATL_BUF_FILL);
	isp1760_hcd_set(hcd, INT_BUF_FILL);

	/* GRR this is run-once init(), being done every time the HC starts.
	 * So long as they're part of class devices, we can't do it init()
	 * since the class device isn't created that early.
	 */
	return 0;
}

static int qtd_fill(struct isp1760_qtd *qtd, void *databuffer, size_t len)
{
	qtd->data_buffer = databuffer;

	qtd->length = len;

	return qtd->length;
}

static void qtd_list_free(struct list_head *qtd_list)
{
	struct isp1760_qtd *qtd, *qtd_next;

	list_for_each_entry_safe(qtd, qtd_next, qtd_list, qtd_list) {
		list_del(&qtd->qtd_list);
		qtd_free(qtd);
	}
}

/*
 * Packetize urb->transfer_buffer into list of packets of size wMaxPacketSize.
 * Also calculate the PID type (SETUP/IN/OUT) for each packet.
 */
#define max_packet(wMaxPacketSize) ((wMaxPacketSize) & 0x07ff)
static void packetize_urb(struct usb_hcd *hcd, struct urb *urb,
			  struct list_head *head, gfp_t flags)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	const struct isp1760_memory_layout *mem = priv->memory_layout;
	struct isp1760_qtd *qtd;
	void *buf;
	int len, maxpacketsize;
	u8 packet_type;

	/*
	 * URBs map to sequences of QTDs:  one logical transaction
	 */

	if (!urb->transfer_buffer && urb->transfer_buffer_length) {
		/* XXX This looks like usb storage / SCSI bug */
		dev_err(priv->dev, "buf is null, dma is %08lx len is %d\n",
			(unsigned long)urb->transfer_dma,
			urb->transfer_buffer_length);
		WARN_ON(1);
	}

	if (usb_pipein(urb->pipe))
		packet_type = IN_PID;
	else
		packet_type = OUT_PID;

	if (usb_pipecontrol(urb->pipe)) {
		qtd = qtd_alloc(flags, urb, SETUP_PID);
		if (!qtd)
			goto cleanup;
		qtd_fill(qtd, urb->setup_packet, sizeof(struct usb_ctrlrequest));
		list_add_tail(&qtd->qtd_list, head);

		/* for zero length DATA stages, STATUS is always IN */
		if (urb->transfer_buffer_length == 0)
			packet_type = IN_PID;
	}

	maxpacketsize = max_packet(usb_maxpacket(urb->dev, urb->pipe));

	/*
	 * buffer gets wrapped in one or more qtds;
	 * last one may be "short" (including zero len)
	 * and may serve as a control status ack
	 */
	buf = urb->transfer_buffer;
	len = urb->transfer_buffer_length;

	for (;;) {
		int this_qtd_len;

		qtd = qtd_alloc(flags, urb, packet_type);
		if (!qtd)
			goto cleanup;

		if (len > mem->blocks_size[ISP176x_BLOCK_NUM - 1])
			this_qtd_len = mem->blocks_size[ISP176x_BLOCK_NUM - 1];
		else
			this_qtd_len = len;

		this_qtd_len = qtd_fill(qtd, buf, this_qtd_len);
		list_add_tail(&qtd->qtd_list, head);

		len -= this_qtd_len;
		buf += this_qtd_len;

		if (len <= 0)
			break;
	}

	/*
	 * control requests may need a terminating data "status" ack;
	 * bulk ones may need a terminating short packet (zero length).
	 */
	if (urb->transfer_buffer_length != 0) {
		int one_more = 0;

		if (usb_pipecontrol(urb->pipe)) {
			one_more = 1;
			if (packet_type == IN_PID)
				packet_type = OUT_PID;
			else
				packet_type = IN_PID;
		} else if (usb_pipebulk(urb->pipe) && maxpacketsize &&
			   (urb->transfer_flags & URB_ZERO_PACKET) &&
			   !(urb->transfer_buffer_length % maxpacketsize)) {
			one_more = 1;
		}
		if (one_more) {
			qtd = qtd_alloc(flags, urb, packet_type);
			if (!qtd)
				goto cleanup;

			/* never any data in such packets */
			qtd_fill(qtd, NULL, 0);
			list_add_tail(&qtd->qtd_list, head);
		}
	}

	return;

cleanup:
	qtd_list_free(head);
}

static int isp1760_urb_enqueue(struct usb_hcd *hcd, struct urb *urb,
			       gfp_t mem_flags)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	struct isp1760_qh *qh = NULL;
	struct list_head *ep_queue;
	LIST_HEAD(new_qtds);
	int qh_in_queue;
	int retval;
	int epnum;

	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		ep_queue = &priv->qh_list[QH_CONTROL];
		break;
	case PIPE_BULK:
		ep_queue = &priv->qh_list[QH_BULK];
		break;
	case PIPE_INTERRUPT:
		ep_queue = &priv->qh_list[QH_INTERRUPT];
		break;
	case PIPE_ISOCHRONOUS:
		printf("isochronous USB packets not yet supported\n");
		return -EPIPE;
	default:
		printf("unknown pipe type\n");
		return -EPIPE;
	}

	if (usb_pipein(urb->pipe))
		urb->actual_length = 0;

	packetize_urb(hcd, urb, &new_qtds, mem_flags);
	if (list_empty(&new_qtds))
		return -ENOMEM;

	retval = usb_hcd_link_urb_to_ep(hcd, urb);
	if (retval) {
		qtd_list_free(&new_qtds);
		goto out;
	}

	epnum = usb_pipeendpoint(urb->pipe);

	qh_in_queue = 0;
	list_for_each_entry(qh, ep_queue, qh_list) {
		if (qh->epnum == epnum) {
			qh_in_queue = 1;
			break;
		}
	}

	if (!qh_in_queue) {
		qh = qh_alloc(GFP_ATOMIC);
		if (!qh) {
			retval = -ENOMEM;
			usb_hcd_unlink_urb_from_ep(hcd, urb);
			qtd_list_free(&new_qtds);
			goto out;
		}

		qh->epnum = epnum;
		list_add_tail(&qh->qh_list, ep_queue);
		urb->ep->hcpriv = qh;
	}

	list_splice_tail(&new_qtds, &qh->qtd_list);
	schedule_ptds(hcd);

out:
	return retval;
}

static void kill_transfer(struct usb_hcd *hcd, struct urb *urb,
			  struct isp1760_qh *qh)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	int skip_map;

	WARN_ON(qh->slot == -1);

	/*
	 * We need to forcefully reclaim the slot since some transfers never
	 * return, e.g. interrupt transfers and NAKed bulk transfers.
	 */
	if (usb_pipecontrol(urb->pipe) || usb_pipebulk(urb->pipe)) {
		if (qh->slot != -1) {
			skip_map = isp1760_hcd_read(hcd, HC_ATL_PTD_SKIPMAP);
			skip_map |= (1 << qh->slot);
			isp1760_hcd_write(hcd, HC_ATL_PTD_SKIPMAP, skip_map);
			ndelay(100);
		}
		priv->atl_slots[qh->slot].qh = NULL;
		priv->atl_slots[qh->slot].qtd = NULL;
	} else {
		if (qh->slot != -1) {
			skip_map = isp1760_hcd_read(hcd, HC_INT_PTD_SKIPMAP);
			skip_map |= (1 << qh->slot);
			isp1760_hcd_write(hcd, HC_INT_PTD_SKIPMAP, skip_map);
		}
		priv->int_slots[qh->slot].qh = NULL;
		priv->int_slots[qh->slot].qtd = NULL;
	}

	qh->slot = -1;
}

/*
 * Retire the qtds beginning at 'qtd' and belonging all to the same urb, killing
 * any active transfer belonging to the urb in the process.
 */
static void dequeue_urb_from_qtd(struct usb_hcd *hcd, struct isp1760_qh *qh,
				 struct isp1760_qtd *qtd)
{
	struct urb *urb;
	int urb_was_running;

	urb = qtd->urb;
	urb_was_running = 0;
	list_for_each_entry_from(qtd, &qh->qtd_list, qtd_list) {
		if (qtd->urb != urb)
			break;

		if (qtd->status >= QTD_XFER_STARTED)
			urb_was_running = 1;
		if (last_qtd_of_urb(qtd, qh) &&
		    qtd->status >= QTD_XFER_COMPLETE)
			urb_was_running = 0;

		if (qtd->status == QTD_XFER_STARTED)
			kill_transfer(hcd, urb, qh);
		qtd->status = QTD_RETIRE;
	}
}

int isp1760_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct isp1760_qtd *qtd;
	struct isp1760_qh *qh;
	int retval = 0;

	retval = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (retval)
		goto out;

	qh = urb->ep->hcpriv;
	if (!qh) {
		retval = -EINVAL;
		goto out;
	}

	list_for_each_entry(qtd, &qh->qtd_list, qtd_list)
		if (qtd->urb == urb) {
			dequeue_urb_from_qtd(hcd, qh, qtd);
			list_move(&qtd->qtd_list, &qh->qtd_list);
			break;
		}

	urb->status = status;
	schedule_ptds(hcd);

out:
	return retval;
}

static void isp1760_hub_descriptor(struct isp1760_hcd *priv,
				   struct usb_hub_descriptor *desc)
{
	int ports;
	u16 temp;

	ports = isp1760_hcd_n_ports(priv->hcd);

	desc->bDescriptorType = USB_DT_HUB;
	/* priv 1.0, 2.3.9 says 20ms max */
	desc->bPwrOn2PwrGood = 10;
	desc->bHubContrCurrent = 0;

	desc->bNbrPorts = ports;
	temp = 1 + (ports / 8);
	desc->bLength = 7 + 2 * temp;

	/* ports removable, and usb 1.0 legacy PortPwrCtrlMask */
	memset(&desc->u.hs.DeviceRemovable[0], 0, temp);
	memset(&desc->u.hs.DeviceRemovable[temp], 0xff, temp);

	/* per-port overcurrent reporting */
	temp = HUB_CHAR_INDV_PORT_OCPM;
	if (isp1760_hcd_ppc_is_set(priv->hcd))
		/* per-port power control */
		temp |= HUB_CHAR_INDV_PORT_LPSM;
	else
		/* no power switching */
		temp |= HUB_CHAR_NO_LPSM;
	desc->wHubCharacteristics = cpu_to_le16(temp);
}

#define	PORT_WAKE_BITS	(PORT_WKOC_E | PORT_WKDISC_E | PORT_WKCONN_E)

static void check_reset_complete(struct usb_hcd *hcd, int index)
{
	if (!(isp1760_hcd_is_set(hcd, PORT_CONNECT)))
		return;

	/* if reset finished and it's still not enabled -- handoff */
	if (!isp1760_hcd_is_set(hcd, PORT_PE)) {
		printf("port %d full speed --> companion\n", index + 1);

		isp1760_hcd_set(hcd, PORT_OWNER);

		isp1760_hcd_clear(hcd, PORT_CSC);
	} else {
		printf("port %d high speed\n", index + 1);
	}
}

static int isp1760_hub_control(struct usb_hcd *hcd, struct usb_device *dev,
			       unsigned long pipe, void *buffer, int length,
			       struct devrequest *setup)
{
	struct isp1760_hcd *priv = hcd_to_priv(hcd);
	u16 typeReq, wValue, wIndex;
	char *buf = buffer;
	void *src = NULL;
	int src_len = 0;
	int retval = 0;
	u32 status;
	int ports;

	if (!setup)
		return -EINVAL;

	ports = isp1760_hcd_n_ports(hcd);

	typeReq = setup->request | (setup->requesttype << 8);
	wValue = le16_to_cpu(setup->value);
	wIndex = le16_to_cpu(setup->index);

	/*
	 * FIXME:  support SetPortFeatures USB_PORT_FEAT_INDICATOR.
	 * HCS_INDICATOR may say we can change LEDs to off/amber/green.
	 * (track current state ourselves) ... blink for diagnostics,
	 * power, "this is the one", etc.  EHCI spec supports this.
	 */

	switch (typeReq) {
	case DeviceOutRequest | USB_REQ_SET_ADDRESS:
		break;
	case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
		/* Nothing to do */
		break;
	case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
		switch (wValue & 0xff00) {
		case USB_DT_DEVICE << 8:
			src = &rh_descriptor.device;
			src_len = 0x12;
			break;
		case USB_DT_CONFIG << 8:
			src = &rh_descriptor.config;
			src_len = 0x09;
			break;
		case USB_DT_STRING << 8:
			switch (wValue & 0xff) {
			case 0:	/* Language */
				src = ISP1760_LANGUAGE_DESC;
				src_len = 4;
				break;
			case 1:	/* Vendor String  */
				src = ISP1760_VENDOR_DESC;
				src_len = 14;
				break;
			case 2:	/* Product Name */
				src = ISP1760_PRODUCT_NAME_DESC;
				src_len = 42;
				break;
			default:
				goto error;
			}
			break;
		}
		break;
	case ClearHubFeature:
		switch (wValue) {
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			/* no hub-wide feature/status flags */
			break;
		default:
			goto error;
		}
		break;
	case ClearPortFeature:
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;

		/*
		 * Even if OWNER is set, so the port is owned by the
		 * companion controller, hub_wq needs to be able to clear
		 * the port-change status bits (especially
		 * USB_PORT_STAT_C_CONNECTION).
		 */

		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			isp1760_hcd_clear(hcd, PORT_PE);
			break;
		case USB_PORT_FEAT_C_ENABLE:
			/* XXX error? */
			break;
		case USB_PORT_FEAT_SUSPEND:
			if (isp1760_hcd_is_set(hcd, PORT_RESET))
				goto error;

			if (isp1760_hcd_is_set(hcd, PORT_SUSPEND)) {
				if (!isp1760_hcd_is_set(hcd, PORT_PE))
					goto error;
				/* resume signaling for 20 msec */
				isp1760_hcd_clear(hcd, PORT_CSC);
				isp1760_hcd_set(hcd, PORT_RESUME);

				priv->reset_done = get_timer(0) + 40;
			}
			break;
		case USB_PORT_FEAT_C_SUSPEND:
			/* we auto-clear this feature */
			break;
		case USB_PORT_FEAT_POWER:
			if (isp1760_hcd_ppc_is_set(hcd))
				isp1760_hcd_clear(hcd, PORT_POWER);
			break;
		case USB_PORT_FEAT_C_CONNECTION:
			isp1760_hcd_set(hcd, PORT_CSC);
			break;
		case USB_PORT_FEAT_C_OVER_CURRENT:
			/* XXX error ?*/
			break;
		case USB_PORT_FEAT_C_RESET:
			/* GetPortStatus clears reset */
			break;
		default:
			goto error;
		}
		isp1760_hcd_read(hcd, CMD_RUN);
		break;
	case GetHubDescriptor:
		isp1760_hub_descriptor(priv, (struct usb_hub_descriptor *)buf);
		break;
	case GetHubStatus:
		/* no hub-wide feature/status flags */
		memset(buf, 0, 4);
		break;
	case GetPortStatus:
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;
		status = 0;

		/* wPortChange bits */
		if (isp1760_hcd_is_set(hcd, PORT_CSC))
			status |= USB_PORT_STAT_C_CONNECTION << 16;

		/* whoever resumes must GetPortStatus to complete it!! */
		if (isp1760_hcd_is_set(hcd, PORT_RESUME)) {
			status |= USB_PORT_STAT_C_SUSPEND << 16;

			if (!priv->reset_done) {
				priv->reset_done = get_timer(0) + 20;
			} else if (get_timer(0) > priv->reset_done) {
				/* stop resume signaling */
				isp1760_hcd_clear(hcd, PORT_CSC);

				retval = isp1760_hcd_clear_and_wait(hcd,
								    PORT_RESUME,
								    2000);
				if (retval != 0) {
					printf("port %d resume error %d\n",
					       wIndex + 1, retval);
					goto error;
				}
			}
		}

		/* whoever resets must GetPortStatus to complete it!! */
		if (isp1760_hcd_is_set(hcd, PORT_RESET) &&
		    get_timer(0) > priv->reset_done) {
			status |= USB_PORT_STAT_C_RESET << 16;
			priv->reset_done = 0;

			/* force reset to complete */
			/* REVISIT:  some hardware needs 550+ usec to clear
			 * this bit; seems too long to spin routinely...
			 */
			retval = isp1760_hcd_clear_and_wait(hcd, PORT_RESET,
							    750);
			if (retval != 0) {
				printf("port %d reset error %d\n", wIndex + 1,
				       retval);
				goto error;
			}

			/* see what we found out */
			check_reset_complete(hcd, wIndex);
		}
		/*
		 * Even if OWNER is set, there's no harm letting hub_wq
		 * see the wPortStatus values (they should all be 0 except
		 * for PORT_POWER anyway).
		 */

		if (isp1760_hcd_is_set(hcd, PORT_OWNER))
			printf("PORT_OWNER is set\n");

		if (isp1760_hcd_is_set(hcd, PORT_CONNECT)) {
			status |= USB_PORT_STAT_CONNECTION;

			/* status may be from integrated TT */
			status |= USB_PORT_STAT_HIGH_SPEED;
		}
		if (isp1760_hcd_is_set(hcd, PORT_PE))
			status |= USB_PORT_STAT_ENABLE;
		if (isp1760_hcd_is_set(hcd, PORT_SUSPEND) &&
		    isp1760_hcd_is_set(hcd, PORT_RESUME))
			status |= USB_PORT_STAT_SUSPEND;
		if (isp1760_hcd_is_set(hcd, PORT_RESET))
			status |= USB_PORT_STAT_RESET;
		if (isp1760_hcd_is_set(hcd, PORT_POWER))
			status |= USB_PORT_STAT_POWER;

		put_unaligned(cpu_to_le32(status), (__le32 *)buf);
		break;
	case SetHubFeature:
		switch (wValue) {
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			/* no hub-wide feature/status flags */
			break;
		default:
			goto error;
		}
		break;
	case SetPortFeature:
		wIndex &= 0xff;
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;

		if (isp1760_hcd_is_set(hcd, PORT_OWNER))
			break;

		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			isp1760_hcd_set(hcd, PORT_PE);
			break;

		case USB_PORT_FEAT_SUSPEND:
			if (!isp1760_hcd_is_set(hcd, PORT_PE) ||
			    isp1760_hcd_is_set(hcd, PORT_RESET))
				goto error;

			isp1760_hcd_set(hcd, PORT_SUSPEND);
			break;
		case USB_PORT_FEAT_POWER:
			if (isp1760_hcd_ppc_is_set(hcd))
				isp1760_hcd_set(hcd, PORT_POWER);
			break;
		case USB_PORT_FEAT_RESET:
			if (isp1760_hcd_is_set(hcd, PORT_RESUME))
				goto error;
			/* line status bits may report this as low speed,
			 * which can be fine if this root hub has a
			 * transaction translator built in.
			 */
			if ((isp1760_hcd_is_set(hcd, PORT_CONNECT) &&
			     !isp1760_hcd_is_set(hcd, PORT_PE)) &&
			    (isp1760_hcd_read(hcd, PORT_LSTATUS) == 1)) {
				isp1760_hcd_set(hcd, PORT_OWNER);
			} else {
				isp1760_hcd_set(hcd, PORT_RESET);
				isp1760_hcd_clear(hcd, PORT_PE);

				priv->reset_done = get_timer(0) + 50;
			}
			break;
		default:
			goto error;
		}
		break;

	default:
		printf("root: unknown request: 0x%0x\n", typeReq);
		goto error;
	}

	if (src_len) {
		length = min(src_len, length);

		if (src && length > 0)
			memcpy(buffer, src, length);
		else
			printf("zero copy USB descriptor\n");
	}

	dev->act_len = length;
	dev->status = 0;

	return 0;

error:
	/* "stall" on error */
	dev->act_len = 0;
	dev->status = USB_ST_STALLED;
	return -EPIPE;
}

int __init isp1760_init_kmem_once(void)
{
	urb_listitem_cachep = kmem_cache_create("isp1760_urb_listitem",
						sizeof(struct urb_listitem), 0,
						SLAB_TEMPORARY |
						SLAB_MEM_SPREAD, NULL);

	if (!urb_listitem_cachep)
		return -ENOMEM;

	qtd_cachep = kmem_cache_create("isp1760_qtd",
				       sizeof(struct isp1760_qtd), 0,
				       SLAB_TEMPORARY | SLAB_MEM_SPREAD, NULL);

	if (!qtd_cachep)
		goto destroy_urb_listitem;

	qh_cachep = kmem_cache_create("isp1760_qh", sizeof(struct isp1760_qh),
				      0, SLAB_TEMPORARY | SLAB_MEM_SPREAD,
				      NULL);

	if (!qh_cachep)
		goto destroy_qtd;

	return 0;

destroy_qtd:
	kmem_cache_destroy(qtd_cachep);

destroy_urb_listitem:
	kmem_cache_destroy(urb_listitem_cachep);

	return -ENOMEM;
}

void isp1760_deinit_kmem_cache(void)
{
	kmem_cache_destroy(qtd_cachep);
	kmem_cache_destroy(qh_cachep);
	kmem_cache_destroy(urb_listitem_cachep);
}

int isp1760_hcd_lowlevel_init(struct isp1760_hcd *priv)
{
	int ret;

	ret = isp1760_hc_setup(priv->hcd);
	if (ret < 0)
		return ret;

	ret = isp1760_run(priv->hcd);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct usb_urb_ops isp1760_urb_ops = {
	.urb_enqueue = isp1760_urb_enqueue,
	.urb_dequeue = isp1760_urb_dequeue,
	.hub_control = isp1760_hub_control,
	.isr = isp1760_irq,
};

int isp1760_hcd_register(struct isp1760_hcd *priv, struct resource *mem,
			 int irq, unsigned long irqflags,
			 struct udevice *dev)
{
	const struct isp1760_memory_layout *mem_layout = priv->memory_layout;
	struct isp1760_host_data *host = dev_get_priv(dev);
	struct usb_hcd *hcd = &host->hcd;
	int ret;

	priv->hcd = hcd;

	hcd->hcd_priv = priv;

	priv->hcd = hcd;

	hcd->urb_ops = &isp1760_urb_ops;

	priv->atl_slots = kcalloc(mem_layout->slot_num,
				  sizeof(struct isp1760_slotinfo), GFP_KERNEL);
	if (!priv->atl_slots)
		return -ENOMEM;

	priv->int_slots = kcalloc(mem_layout->slot_num,
				  sizeof(struct isp1760_slotinfo), GFP_KERNEL);
	if (!priv->int_slots) {
		ret = -ENOMEM;
		goto free_atl_slots;
	}

	host->host_speed = USB_SPEED_HIGH;

	init_memory(priv);

	return 0;

free_atl_slots:
	kfree(priv->atl_slots);

	return ret;
}

void isp1760_hcd_unregister(struct isp1760_hcd *priv)
{
	struct isp1760_qh *qh, *qh_next;
	int i;

	for (i = 0; i < QH_END; i++)
		list_for_each_entry_safe(qh, qh_next, &priv->qh_list[i],
					 qh_list) {
			qtd_list_free(&qh->qtd_list);
			list_del(&qh->qh_list);
			qh_free(qh);
		}

	kfree(priv->atl_slots);
	kfree(priv->int_slots);
}
