// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Zynq MPSoC Firmware driver
 *
 * Copyright (C) 2018-2019 Xilinx, Inc.
 * Copyright (C) 2022 - 2026, Advanced Micro Devices, Inc.
 */

#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <asm/io.h>
#include <cpu_func.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <log.h>
#include <versalpl.h>
#include <zynqmp_firmware.h>
#include <asm/cache.h>
#include <asm/ptrace.h>
#include <asm/system.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#if defined(CONFIG_ARCH_VERSAL2)
#include "versal2-scmi-map.h"

/* EEMI node ID encoding: a full node ID has a non-zero Class field */
#define EEMI_NODEID_CLASS_FIELD_MASK	GENMASK(31, 26)

/* Node ID which no device owns; the firmware rejects a call made with it */
#define EEMI_INVALID_NODEID	U32_MAX
#endif

#if defined(CONFIG_ZYNQMP_IPI)
#include <mailbox.h>

#define PMUFW_PAYLOAD_ARG_CNT	8

#define XST_PM_NO_ACCESS	2002L
#define XST_PM_ALREADY_CONFIGURED	2009L

static struct zynqmp_power {
	struct mbox_chan tx_chan;
	struct mbox_chan rx_chan;
} zynqmp_power __section(".data");

#define NODE_ID_LOCATION	5

static unsigned int xpm_configobject[] = {
	/**********************************************************************/
	/* HEADER */
	2,	/* Number of remaining words in the header */
	1,	/* Number of sections included in config object */
	PM_CONFIG_OBJECT_TYPE_OVERLAY,	/* Type of Config object as overlay */
	/**********************************************************************/
	/* SLAVE SECTION */

	PM_CONFIG_SLAVE_SECTION_ID,	/* Section ID */
	1,				/* Number of slaves */

	0, /* Node ID which will be changed below */
	PM_SLAVE_FLAG_IS_SHAREABLE,
	PM_CONFIG_IPI_PSU_CORTEXA53_0_MASK |
	PM_CONFIG_IPI_PSU_CORTEXR5_0_MASK |
	PM_CONFIG_IPI_PSU_CORTEXR5_1_MASK, /* IPI Mask */
};

static unsigned int xpm_configobject_close[] = {
	/**********************************************************************/
	/* HEADER */
	2,	/* Number of remaining words in the header */
	1,	/* Number of sections included in config object */
	PM_CONFIG_OBJECT_TYPE_OVERLAY,	/* Type of Config object as overlay */
	/**********************************************************************/
	/* SET CONFIG SECTION */
	PM_CONFIG_SET_CONFIG_SECTION_ID,
	0U,	/* Loading permission to Overlay config object */
};

int zynqmp_pmufw_config_close(void)
{
	return zynqmp_pmufw_load_config_object(xpm_configobject_close,
					       sizeof(xpm_configobject_close));
}

int zynqmp_pmufw_node(u32 id)
{
	static bool check = true;
	static bool permission = true;

	if (check) {
		check = false;

		if (zynqmp_pmufw_node(NODE_OCM_BANK_0) == -EACCES) {
			printf("PMUFW:  No permission to change config object\n");
			permission = false;
		}
	}

	if (!permission)
		return 0;

	/* Record power domain id */
	xpm_configobject[NODE_ID_LOCATION] = id;

	return zynqmp_pmufw_load_config_object(xpm_configobject,
					       sizeof(xpm_configobject));
}

static int do_pm_probe(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_FIRMWARE,
					  DM_DRIVER_GET(zynqmp_power),
					  &dev);
	if (ret)
		debug("%s: Probing device failed: %d\n", __func__, ret);

	return ret;
}

static int ipi_req(const u32 *req, size_t req_len, u32 *res, size_t res_maxlen)
{
	struct zynqmp_ipi_msg msg;
	int ret;
	u32 buffer[PAYLOAD_ARG_CNT];

	if (!res)
		res = buffer;

	if (req_len > PMUFW_PAYLOAD_ARG_CNT ||
	    res_maxlen > PMUFW_PAYLOAD_ARG_CNT)
		return -EINVAL;

	if (!(zynqmp_power.tx_chan.dev) || !(zynqmp_power.rx_chan.dev)) {
		ret = do_pm_probe();
		if (ret)
			return ret;
	}

	debug("%s, Sending IPI message with ID: 0x%0x\n", __func__, req[0]);
	msg.buf = (u32 *)req;
	msg.len = req_len;
	ret = mbox_send(&zynqmp_power.tx_chan, &msg);
	if (ret) {
		debug("%s: Sending message failed\n", __func__);
		return ret;
	}

	msg.buf = res;
	msg.len = res_maxlen;
	ret = mbox_recv(&zynqmp_power.rx_chan, &msg, 100);
	if (ret)
		debug("%s: Receiving message failed\n", __func__);

	return ret;
}

/**
 * Send a configuration object to the PMU firmware.
 *
 * @cfg_obj: Pointer to the configuration object
 * @size:    Size of @cfg_obj in bytes
 * Return:   0 on success otherwise negative errno.
 */
int zynqmp_pmufw_load_config_object(const void *cfg_obj, size_t size)
{
	int err;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	if (IS_ENABLED(CONFIG_XPL_BUILD))
		printf("Loading new PMUFW cfg obj (%ld bytes)\n", size);

	flush_dcache_range((ulong)cfg_obj, (ulong)(cfg_obj + size));

	err = xilinx_pm_request(PM_SET_CONFIGURATION, (u32)(u64)cfg_obj, 0, 0,
				0, 0, 0, ret_payload);
	if (err == XST_PM_NO_ACCESS) {
		return -EACCES;
	}

	if (err == XST_PM_ALREADY_CONFIGURED) {
		debug("PMUFW Node is already configured\n");
		return -ENODEV;
	}

	if (err)
		printf("Cannot load PMUFW configuration object (%d)\n", err);

	if (ret_payload[0])
		printf("PMUFW returned 0x%08x status!\n", ret_payload[0]);

	if ((err || ret_payload[0]) && IS_ENABLED(CONFIG_XPL_BUILD))
		panic("PMUFW config object loading failed in EL3\n");

	return 0;
}

static int zynqmp_power_probe(struct udevice *dev)
{
	struct udevice *ipi_dev;
	ofnode ipi_node;
	int ret;

	debug("%s, (dev=%p)\n", __func__, dev);

	/*
	 * Probe all IPI parent node driver. It is important to have IPI
	 * devices available when requested by mbox_get_by* API.
	 * If IPI device isn't available, then mailbox request fails and
	 * that causes system boot failure.
	 * To avoid this make sure all IPI parent drivers are probed here,
	 * and IPI parent driver binds each child node to mailbox driver.
	 * This way mbox_get_by_* API will have correct mailbox device
	 * driver probed.
	 */
	ofnode_for_each_compatible_node(ipi_node, "xlnx,zynqmp-ipi-mailbox") {
		ret = uclass_get_device_by_ofnode(UCLASS_NOP, ipi_node, &ipi_dev);
		if (ret) {
			dev_err(dev, "failed to get IPI device from node %s\n",
				ofnode_get_name(ipi_node));
			return ret;
		}
	}

	ret = mbox_get_by_name(dev, "tx", &zynqmp_power.tx_chan);
	if (ret) {
		debug("%s: Cannot find tx mailbox\n", __func__);
		return ret;
	}

	ret = mbox_get_by_name(dev, "rx", &zynqmp_power.rx_chan);
	if (ret) {
		debug("%s: Cannot find rx mailbox\n", __func__);
		return ret;
	}

	ret = zynqmp_firmware_version();
	printf("PMUFW:\tv%d.%d\n",
	       ret >> ZYNQMP_PM_VERSION_MAJOR_SHIFT,
	       ret & ZYNQMP_PM_VERSION_MINOR_MASK);

	return 0;
};

static const struct udevice_id zynqmp_power_ids[] = {
	{ .compatible = "xlnx,zynqmp-power" },
	{ }
};

U_BOOT_DRIVER(zynqmp_power) = {
	.name = "zynqmp_power",
	.id = UCLASS_FIRMWARE,
	.of_match = zynqmp_power_ids,
	.probe = zynqmp_power_probe,
};
#endif

unsigned int zynqmp_firmware_version(void)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];
	static u32 pm_api_version = ZYNQMP_PM_VERSION_INVALID;

	/*
	 * Get PMU version only once and later
	 * just return stored values instead of
	 * asking PMUFW again.
	 **/
	if (pm_api_version == ZYNQMP_PM_VERSION_INVALID) {

		ret = xilinx_pm_request(PM_GET_API_VERSION, 0, 0, 0, 0,
					0, 0, ret_payload);
		if (ret)
			panic("PMUFW is not found - Please load it!\n");

		pm_api_version = ret_payload[1];
		if (pm_api_version < ZYNQMP_PM_VERSION)
			panic("PMUFW version error. Expected: v%d.%d\n",
			      ZYNQMP_PM_VERSION_MAJOR, ZYNQMP_PM_VERSION_MINOR);
	}

	return pm_api_version;
};

int zynqmp_pm_reset_assert(const u32 reset,
			   const enum zynqmp_pm_reset_action assert_flag)
{
	return xilinx_pm_request(PM_RESET_ASSERT, reset, assert_flag, 0, 0,
				 0, 0, NULL);
}

int xilinx_pm_get_chipid(u32 *idcode, u32 *version)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = xilinx_pm_request(PM_GET_CHIPID, 0, 0, 0, 0, 0, 0, ret_payload);
	if (ret)
		return ret;

	if (idcode)
		*idcode = ret_payload[1];
	if (version)
		*version = ret_payload[2];

	return 0;
}

int zynqmp_pm_fpga_load(u64 address)
{
	u32 buf_lo = lower_32_bits(address);
	u32 buf_hi = upper_32_bits(address);
	u32 ret_payload[PAYLOAD_ARG_CNT];

	if (IS_ENABLED(CONFIG_ARCH_VERSAL2))
		return xilinx_pm_request(VERSAL_PM_LOAD_PDI, VERSAL_PM_PDI_TYPE,
					 buf_hi, buf_lo, 0, 0, 0, ret_payload);

	return xilinx_pm_request(VERSAL_PM_LOAD_PDI, VERSAL_PM_PDI_TYPE,
				 buf_lo, buf_hi, 0, 0, 0, ret_payload);
}

#if defined(CONFIG_ARCH_VERSAL2)
/*
 * Poll the M-PHY TX/RX config-ready status until it settles or @timeout_us
 * elapses. Legacy EEMI firmware only offers the per-read status primitive, so
 * the poll loop lives here rather than in the UFS driver; the timeout budget is
 * owned by the caller.
 */
int zynqmp_pm_wait_mphy_tx_rx_config_ready(u32 timeout_us)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	while (timeout_us--) {
		ret = xilinx_pm_request(PM_IOCTL, PM_REGNODE_PMC_IOU_SLCR,
					IOCTL_READ_REG, TXRX_CFGRDY_OFFSET, 0, 0,
					0, ret_payload);
		if (ret)
			return ret;

		if (!(ret_payload[1] & TX_RX_CFG_RDY_MASK))
			return 0;

		udelay(1);
	}

	return -ETIMEDOUT;
}

int zynqmp_pm_wait_sram_init_done(u32 timeout_us)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	while (timeout_us--) {
		ret = xilinx_pm_request(PM_IOCTL, PM_REGNODE_PMC_IOU_SLCR,
					IOCTL_READ_REG, SRAM_CSR_OFFSET, 0, 0,
					0, ret_payload);
		if (ret)
			return ret;

		if (ret_payload[1] & SRAM_CSR_INIT_DONE_MASK)
			return 0;

		udelay(1);
	}

	return -ETIMEDOUT;
}

int zynqmp_pm_set_sram_bypass(void)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	u32 sram_csr;
	int ret;

	ret = zynqmp_pm_is_function_supported(PM_IOCTL, IOCTL_MASK_WRITE_REG);
	if (ret) {
		printf("%s: IOCTL_MASK_WRITE_REG is not supported : %d\n"
			, __func__, ret);
		return 0;
	}

	ret = xilinx_pm_request(PM_IOCTL, PM_REGNODE_PMC_IOU_SLCR,
				IOCTL_READ_REG, SRAM_CSR_OFFSET, 0, 0,
				0, ret_payload);
	if (ret)
		return ret;

	sram_csr = ret_payload[1];
	sram_csr &= ~SRAM_CSR_EXT_LD_DONE_MASK;
	sram_csr |= SRAM_CSR_BYPASS_MASK;

	return xilinx_pm_request(PM_IOCTL, PM_REGNODE_PMC_IOU_SLCR,
				 IOCTL_MASK_WRITE_REG, SRAM_CSR_OFFSET,
				 GENMASK(2, 1), sram_csr, 0, NULL);
}

int zynqmp_pm_get_ufs_calibration_values(u32 *value)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!value)
		return -EINVAL;

	ret = zynqmp_pm_is_function_supported(PM_IOCTL, IOCTL_READ_REG);
	if (ret) {
		printf("%s: IOCTL_READ_REG is not supported : %d\n"
			, __func__, ret);
		return 0;
	}

	ret = xilinx_pm_request(PM_IOCTL, PM_REGNODE_EFUSE_CACHE,
				IOCTL_READ_REG, UFS_CAL_1_OFFSET, 0, 0,
				0, ret_payload);
	if (ret)
		return ret;

	*value = ret_payload[1];

	return ret;
}
#endif /* CONFIG_ARCH_VERSAL2 */

#if defined(CONFIG_ARCH_VERSAL) || defined(CONFIG_ARCH_VERSAL2)
/**
 * zynqmp_pm_xilocp_is_feature_supported() - Query PLM XilOCP for API support.
 * @api_id: XilOCP API function ID to check (e.g. XOCP_API_EXTEND_HWPCR)
 *
 * Sends XOCP_API_FEATURES (0) to PLM XilOCP with @api_id as the query
 * argument.  PLM returns success if that API is compiled-in and available.
 *
 * Return: 0 if supported, PLM error code if not supported or call failed.
 */
int zynqmp_pm_xilocp_is_feature_supported(u32 api_id)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];

	return xilinx_pm_request(PLM_IPI_HDR(1, XILOCP_MODULE_ID, XOCP_API_FEATURES),
				 api_id, 0, 0, 0, 0, 0, ret_payload);
}

/**
 * zynqmp_pm_xilocp_extend_hwpcr() - Extend a Hardware PCR via PLM XilOCP.
 * @pcr_num:   HW PCR index (0..7)
 * @hash_addr: Physical address of the 48-byte hash buffer
 *             (cache-flushed by caller)
 * @size:      Buffer size in bytes (must be 48)
 *
 * Calls PLM XilOCP XOCP_API_EXTEND_HWPCR over the TF-A SMC PASS_THROUGH path.
 *
 * Return: 0 on success, PLM error code on failure.
 */
int zynqmp_pm_xilocp_extend_hwpcr(u32 pcr_num, u64 hash_addr, u32 size)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = xilinx_pm_request(PLM_IPI_HDR(4, XILOCP_MODULE_ID, XOCP_API_EXTEND_HWPCR),
				pcr_num,
				lower_32_bits(hash_addr),
				upper_32_bits(hash_addr),
				size, 0, 0, ret_payload);
	if (ret)
		printf("%s: HW PCR %u extend failed: %d\n",
		       __func__, pcr_num, ret);

	return ret;
}

/**
 * zynqmp_pm_xilocp_get_hwpcr() - Read Hardware PCR value(s) via PLM XilOCP.
 * @pcr_mask:  Bitmask of PCRs to read; bit N selects HW PCR N.
 * @buf_addr:  Physical address of the output buffer (invalidated by caller)
 * @buf_size:  Buffer size in bytes (48 * number of PCRs selected)
 *
 * Calls PLM XilOCP XOCP_API_GET_HWPCR over the TF-A SMC PASS_THROUGH path.
 * PLM DMA-writes the PCR value(s) to buf_addr; caller must invalidate cache
 * after this call before reading the buffer.
 *
 * Return: 0 on success, PLM error code on failure.
 */
int zynqmp_pm_xilocp_get_hwpcr(u32 pcr_mask, u64 buf_addr, u32 buf_size)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = xilinx_pm_request(PLM_IPI_HDR(4, XILOCP_MODULE_ID, XOCP_API_GET_HWPCR),
				pcr_mask,
				lower_32_bits(buf_addr),
				upper_32_bits(buf_addr),
				buf_size, 0, 0, ret_payload);
	if (ret)
		printf("%s: HW PCR mask 0x%x read failed: %d\n",
		       __func__, pcr_mask, ret);

	return ret;
}

u32 zynqmp_pm_get_pmc_global_pggs_reg(u32 reg_addr)
{
	int ret;
	u32 value = 0;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	if (reg_addr == PMC_GLOBAL_PGGS3_REG) {
		value = 0;
	} else if (reg_addr == PMC_GLOBAL_PGGS4_REG) {
		value = 1;
	} else {
		printf("%s: not supported pggs register  0x%x\n",
		       __func__, reg_addr);
		return 0;
	}

	ret = zynqmp_pm_is_function_supported(PM_IOCTL, IOCTL_READ_PGGS);
	if (ret) {
		ret = zynqmp_pm_is_function_supported(PM_IOCTL, IOCTL_READ_REG);
		if (ret) {
			printf("%s: IOCTL_READ_REG is not supported : %d\n"
				, __func__, ret);
			return 0;
		}

		/* find node ID from the pggs3 offset */
		value = PM_REG_PGGS3 + value;

		ret = xilinx_pm_request(PM_IOCTL, value,
					IOCTL_READ_REG, 0, 0, 0, 0,
					ret_payload);
		if (ret) {
			printf("%s: node 0x%x get pggs register failed\n",
			       __func__, value);
			return 0;
		}
	} else {
		ret = xilinx_pm_request(PM_IOCTL, PMC_GLOBAL_PGGS3_REG_NODE,
					IOCTL_READ_PGGS, value, 0, 0, 0,
					ret_payload);
		if (ret) {
			printf("%s: node 0x%x get pggs register failed\n",
			       __func__, PMC_GLOBAL_PGGS3_REG_NODE);
			return 0;
		}
	}

	return ret_payload[1];
}
#endif /* CONFIG_ARCH_VERSAL || CONFIG_ARCH_VERSAL2 */

int zynqmp_pm_set_gem_config(u32 node, enum pm_gem_config_type config, u32 value)
{
	int ret;

	ret = xilinx_pm_request(PM_IOCTL, node, IOCTL_SET_GEM_CONFIG,
				config, value, 0, 0, NULL);
	if (ret)
		printf("%s: node %d: set_gem_config %d failed\n",
		       __func__, node, config);

	return ret;
}

int zynqmp_pm_set_sd_config(u32 node, enum pm_sd_config_type config, u32 value)
{
	int ret;

	ret = xilinx_pm_request(PM_IOCTL, node, IOCTL_SET_SD_CONFIG,
				config, value, 0, 0, NULL);
	if (ret)
		printf("%s: node %d: set_sd_config %d failed\n",
		       __func__, node, config);

	return ret;
}

u32 zynqmp_pm_get_bootmode_reg(void)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	ret = zynqmp_pm_is_function_supported(PM_IOCTL, IOCTL_READ_REG);
	if (ret) {
		printf("%s: IOCTL_READ_REG is not supported failed with error code: %d\n"
		       , __func__, ret);
		return 0;
	}

	ret = xilinx_pm_request(PM_IOCTL, CRP_BOOT_MODE_REG_NODE, IOCTL_READ_REG,
				CRP_BOOT_MODE_REG_OFFSET, 0, 0, 0, ret_payload);
	if (ret) {
		printf("%s: node 0x%x: get_bootmode 0x%x failed\n",
		       __func__, CRP_BOOT_MODE_REG_NODE, CRP_BOOT_MODE_REG_OFFSET);
		return 0;
	}

	return ret_payload[1];
}

#if defined(CONFIG_ARCH_VERSAL) || defined(CONFIG_ARCH_VERSAL2)
u32 zynqmp_pm_get_pmc_multi_boot_reg(void)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	ret = zynqmp_pm_is_function_supported(PM_IOCTL, IOCTL_READ_REG);
	if (ret) {
		printf("%s: IOCTL_READ_REG is not supported failed with error code: %d\n"
		       , __func__, ret);
		return 0;
	}

	ret = xilinx_pm_request(PM_IOCTL, PM_REG_PMC_GLOBAL_NODE, IOCTL_READ_REG,
				PMC_MULTI_BOOT_MODE_REG_OFFSET, 0, 0, 0,
				ret_payload);
	if (ret) {
		printf("%s: node 0x%x: get_bootmode 0x%x failed\n",
		       __func__, PM_REG_PMC_GLOBAL_NODE, PMC_MULTI_BOOT_MODE_REG_OFFSET);
		return 0;
	}

	return ret_payload[1];
}
#endif

#if defined(CONFIG_ARCH_VERSAL)
u32 versal_pmc_multi_boot(void)
{
	/* At EL3 the SMC path to firmware is unavailable, read directly */
	if (current_el() == 3)
		return versal_multi_boot_reg();

	return zynqmp_pm_get_pmc_multi_boot_reg() & PMC_MULTI_BOOT_MASK;
}

u8 versal_get_bootmode(void)
{
	u32 reg;

	/* At EL3 the SMC path to firmware is unavailable, read directly */
	if (current_el() == 3)
		reg = versal_bootmode_reg();
	else
		reg = zynqmp_pm_get_bootmode_reg();

	if (reg >> BOOT_MODE_ALT_SHIFT)
		reg >>= BOOT_MODE_ALT_SHIFT;

	return reg & BOOT_MODES_MASK;
}
#endif

#if defined(CONFIG_ARCH_VERSAL_NET)
u8 versal_net_get_bootmode(void)
{
	u32 reg;

	/* At EL3 the SMC path to firmware is unavailable, read directly */
	if (current_el() == 3)
		reg = versal_net_bootmode_reg();
	else
		reg = zynqmp_pm_get_bootmode_reg();

	if (reg >> BOOT_MODE_ALT_SHIFT)
		reg >>= BOOT_MODE_ALT_SHIFT;

	return reg & BOOT_MODES_MASK;
}
#endif

#if defined(CONFIG_ARCH_ZYNQMP)
int zynqmp_mmio_write(const u32 address, const u32 mask, const u32 value)
{
	/* At EL3 or in SPL the firmware (SMC) path is unavailable */
	if (IS_ENABLED(CONFIG_XPL_BUILD) || current_el() == 3)
		return zynqmp_mmio_rawwrite(address, mask, value);

	return xilinx_pm_request(PM_MMIO_WRITE, address, mask, value,
				 0, 0, 0, NULL);
}

int zynqmp_mmio_read(const u32 address, u32 *value)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!value)
		return -EINVAL;

	/* At EL3 or in SPL the firmware (SMC) path is unavailable */
	if (IS_ENABLED(CONFIG_XPL_BUILD) || current_el() == 3)
		return zynqmp_mmio_rawread(address, value);

	ret = xilinx_pm_request(PM_MMIO_READ, address, 0, 0, 0, 0, 0,
				ret_payload);
	*value = ret_payload[1];

	return ret;
}
#endif

#if defined(CONFIG_ARCH_VERSAL2)
u32 versal2_pmc_multi_boot(void)
{
	/* At EL3 the SMC path to firmware is unavailable, read directly */
	if (current_el() == 3)
		return versal2_multi_boot_reg();

	return zynqmp_pm_get_pmc_multi_boot_reg() & PMC_MULTI_BOOT_MASK;
}
#endif

#if defined(CONFIG_ARCH_VERSAL_NET) || defined(CONFIG_ARCH_VERSAL2)
static u32 zynqmp_pm_get_pmc_tap_reg(u32 offset)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	ret = zynqmp_pm_is_function_supported(PM_IOCTL, IOCTL_READ_REG);
	if (ret) {
		printf("%s: IOCTL_READ_REG is not supported failed with error code: %d\n"
		       , __func__, ret);
		return 0;
	}

	ret = xilinx_pm_request(PM_IOCTL, PM_REGNODE_PMC_TAP, IOCTL_READ_REG,
				offset, 0, 0, 0, ret_payload);
	if (ret) {
		printf("%s: node 0x%x: pmc_tap offset 0x%x failed\n",
		       __func__, PM_REGNODE_PMC_TAP, offset);
		return 0;
	}

	return ret_payload[1];
}

u32 zynqmp_pm_get_pmc_tap_idcode(void)
{
	return zynqmp_pm_get_pmc_tap_reg(PMC_TAP_IDCODE_OFFSET);
}

u32 zynqmp_pm_get_pmc_tap_version(void)
{
	return zynqmp_pm_get_pmc_tap_reg(PMC_TAP_VERSION_OFFSET);
}

u32 zynqmp_pm_get_pmc_tap_usercode(void)
{
	return zynqmp_pm_get_pmc_tap_reg(PMC_TAP_USERCODE_OFFSET);
}
#endif

int zynqmp_pm_feature(const u32 api_id)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	/* Check feature check API version */
	ret = xilinx_pm_request(PM_FEATURE_CHECK, api_id, 0, 0, 0,
				0, 0, ret_payload);
	if (ret)
		return ret;

	/* Return feature check version */
	return ret_payload[1] & FIRMWARE_VERSION_MASK;
}

int zynqmp_pm_is_function_supported(const u32 api_id, const u32 id)
{
	int ret;
	u32 *bit_mask;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	/* Input arguments validation */
	if (id >= 64 || (api_id != PM_IOCTL && api_id != PM_QUERY_DATA))
		return -EINVAL;

	/* Check feature check API version */
	ret = xilinx_pm_request(PM_FEATURE_CHECK, PM_FEATURE_CHECK, 0, 0, 0,
				0, 0, ret_payload);
	if (ret)
		return ret;

	/* Check if feature check version 2 is supported or not */
	if ((ret_payload[1] & FIRMWARE_VERSION_MASK) == PM_API_VERSION_2) {
		/*
		 * Call feature check for IOCTL/QUERY API to get IOCTL ID or
		 * QUERY ID feature status.
		 */

		ret = xilinx_pm_request(PM_FEATURE_CHECK, api_id, 0, 0, 0,
					0, 0, ret_payload);
		if (ret)
			return ret;

		bit_mask = &ret_payload[2];
		if ((bit_mask[(id / 32)] & BIT((id % 32))) == 0)
			return -EOPNOTSUPP;
	} else {
		return -ENODATA;
	}

	return 0;
}

smc_call_handler_t __data smc_call_handler;

static int smc_call_legacy(u32 api_id, u32 arg0, u32 arg1, u32 arg2,
			   u32 arg3, u32 arg4, u32 arg5, u32 *ret_payload)
{
	u32 module_id = FIELD_GET(PLM_MODULE_ID_MASK, api_id);
	struct pt_regs regs;

	if (module_id == XILOCP_MODULE_ID) {
		/* XilOCP module: dispatch via TF-A PASS_THROUGH */
		regs.regs[0] = PM_SIP_SVC | PASS_THROUGH_FW_CMD_ID;
		regs.regs[1] = ((u64)arg0 << 32) |
				FIELD_PREP(PLM_MODULE_ID_MASK, module_id) |
				(api_id & API_ID_MASK);
		regs.regs[2] = ((u64)arg2 << 32) | arg1;
		regs.regs[3] = ((u64)arg4 << 32) | arg3;
	} else {
		regs.regs[0] = PM_SIP_SVC | api_id;
		regs.regs[1] = ((u64)arg1 << 32) | arg0;
		regs.regs[2] = ((u64)arg3 << 32) | arg2;
		regs.regs[3] = arg4;
	}

	smc_call(&regs);

	if (ret_payload) {
		ret_payload[0] = (u32)regs.regs[0];
		ret_payload[1] = upper_32_bits(regs.regs[0]);
		ret_payload[2] = (u32)regs.regs[1];
		ret_payload[3] = upper_32_bits(regs.regs[1]);
		ret_payload[4] = (u32)regs.regs[2];
		ret_payload[5] = upper_32_bits((u32)regs.regs[2]);
		ret_payload[6] = (u32)regs.regs[3];
	}

	return (ret_payload) ? ret_payload[0] : 0;
}

static int smc_call_enhanced(u32 api_id, u32 arg0, u32 arg1, u32 arg2,
			     u32 arg3, u32 arg4, u32 arg5, u32 *ret_payload)
{
	struct pt_regs regs;
	u32 module_id = FIELD_GET(PLM_MODULE_ID_MASK, api_id);

	if (module_id == 0)
		module_id = PM_MODULE_ID;

	regs.regs[0] = PM_SIP_SVC | PASS_THROUGH_FW_CMD_ID;
	regs.regs[1] = ((u64)arg0 << 32U) |
			FIELD_PREP(PLM_MODULE_ID_MASK, module_id) |
			(api_id & API_ID_MASK);
	regs.regs[2] = arg1 | ((u64)arg2 << 32);
	regs.regs[3] = arg3 | ((u64)arg4 << 32);
	regs.regs[4] = arg5;

	smc_call(&regs);

	if (ret_payload) {
		ret_payload[0] = regs.regs[0];
		ret_payload[1] = upper_32_bits(regs.regs[0]);
		ret_payload[2] = (u32)regs.regs[1];
		ret_payload[3] = upper_32_bits(regs.regs[1]);
		ret_payload[4] = (u32)regs.regs[2];
		ret_payload[5] = upper_32_bits((u32)regs.regs[2]);
		ret_payload[6] = (u32)regs.regs[3];
	}

	return regs.regs[0];
}

#if defined(CONFIG_ARCH_VERSAL2)
/**
 * zynqmp_pm_translate_node_id() - translate a compact SCMI device ID into
 *				   the EEMI node ID the platform management
 *				   firmware expects
 * @node_id: Compact SCMI device ID (e.g. from a "power-domains" phandle argument).
 *
 * On AMD Versal Gen 2, some device tree properties still name devices
 * with the compact SCMI IDs used by the SCMI protocol, even where the
 * caller (e.g. a legacy EEMI ioctl through xilinx_pm_request()) needs the
 * real EEMI node ID instead. This translates such a compact ID via
 * versal2_scmi_device_map[]. Values that already look like a full node ID
 * (non-zero Class field, bits [31:26]), and every non-Versal Gen 2 platform,
 * are returned unchanged.
 *
 * Return: the EEMI node ID to use, or EEMI_INVALID_NODEID if a Versal Gen 2
 *         compact ID has no entry in the translation table.
 */
static u32 zynqmp_pm_translate_node_id(u32 node_id)
{
	/* A full node ID is already formatted, its Class field is non-zero */
	if (node_id & EEMI_NODEID_CLASS_FIELD_MASK)
		return node_id;

	if (node_id >= ARRAY_SIZE(versal2_scmi_device_map))
		return EEMI_INVALID_NODEID;

	return versal2_scmi_device_map[node_id];
}
#endif /* CONFIG_ARCH_VERSAL2 */

int __maybe_unused xilinx_pm_request(u32 api_id, u32 arg0, u32 arg1, u32 arg2,
				     u32 arg3, u32 arg4, u32 arg5, u32 *ret_payload)
{
	debug("%s at EL%d, API ID: 0x%0x, 0x%0x, 0x%0x, 0x%0x, 0x%0x, 0x%0x, 0x%0x\n",
	      __func__, current_el(), api_id, arg0, arg1, arg2, arg3, arg4, arg5);

#ifdef CONFIG_ARCH_VERSAL2
	/*
	 * On Versal Gen 2, "power-domains" carries a compact SCMI device ID
	 * rather than a raw EEMI node ID; translate it so any PM_IOCTL call
	 * made through this function uses the node ID the platform
	 * management firmware actually expects.
	 */
	if (api_id == PM_IOCTL) {
		arg0 = zynqmp_pm_translate_node_id(arg0);
		if (arg0 == EEMI_INVALID_NODEID)
			return -EINVAL;
	}
#endif

	if (IS_ENABLED(CONFIG_XPL_BUILD) || current_el() == 3) {
#if defined(CONFIG_ZYNQMP_IPI)
		/*
		 * Use fixed payload and arg size as the EL2 call. The firmware
		 * is capable to handle PMUFW_PAYLOAD_ARG_CNT bytes but the
		 * firmware API is limited by the SMC call size
		 */
		u32 regs[] = {api_id, arg0, arg1, arg2, arg3, arg4, arg5};
		int ret;

		if (api_id == PM_FPGA_LOAD) {
			/* Swap addr_hi/low because of incompatibility */
			u32 temp = regs[1];

			regs[1] = regs[2];
			regs[2] = temp;
		}

		ret = ipi_req(regs, PAYLOAD_ARG_CNT, ret_payload,
			      PAYLOAD_ARG_CNT);
		if (ret)
			return ret;

		return (ret_payload) ? ret_payload[0] : 0;
#else
		return -EPERM;
#endif
	}

	return smc_call_handler(api_id, arg0, arg1, arg2, arg3, arg4,
				arg5, ret_payload);
}

static const struct udevice_id zynqmp_firmware_ids[] = {
	{ .compatible = "xlnx,zynqmp-firmware", .data = (ulong)smc_call_legacy },
	{ .compatible = "xlnx,versal-firmware", .data = (ulong)smc_call_legacy},
	{ .compatible = "xlnx,versal-net-firmware", .data = (ulong)smc_call_legacy },
	{ .compatible = "xlnx,versal2-firmware", .data = (ulong)smc_call_enhanced},
	{ }
};

static int zynqmp_firmware_bind(struct udevice *dev)
{
	int ret;
	struct udevice *child;

	smc_call_handler = (smc_call_handler_t)dev_get_driver_data(dev);
	if (!smc_call_handler)
		return -EINVAL;

	if (CONFIG_IS_ENABLED(POWER_DOMAIN) &&
	    IS_ENABLED(CONFIG_ZYNQMP_POWER_DOMAIN)) {
		ret = device_bind_driver_to_node(dev, "zynqmp_power_domain",
						 "zynqmp_power_domain",
						 dev_ofnode(dev), &child);
		if (ret) {
			printf("zynqmp power domain driver is not bound: %d\n", ret);
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_TPM2_VERSAL2_SMC)) {
		ret = zynqmp_pm_xilocp_is_feature_supported(XOCP_API_EXTEND_HWPCR);
		if (!ret)
			ret = zynqmp_pm_xilocp_is_feature_supported(XOCP_API_GET_HWPCR);

		if (!ret) {
			ret = device_bind_driver(dev, "tpm2_versal2_smc",
						 "tpm2_versal2_smc", &child);
			if (ret)
				printf("TPM2 Versal driver bind failed: %d\n", ret);
		}
	}

	return 0;
}

U_BOOT_DRIVER(zynqmp_firmware) = {
	.id = UCLASS_FIRMWARE,
	.name = "zynqmp_firmware",
	.of_match = zynqmp_firmware_ids,
	.bind = zynqmp_firmware_bind,
};
