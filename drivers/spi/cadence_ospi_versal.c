// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2018 Xilinx
 *
 * Cadence QSPI controller DMA operations
 */

#include <clk.h>
#include <common.h>
#include <memalign.h>
#include <wait_bit.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/cache.h>
#include <cpu_func.h>
#include <zynqmp_firmware.h>
#include <asm/arch/hardware.h>
#include <soc.h>
#include "cadence_qspi.h"
#include <dt-bindings/power/xlnx-versal-power.h>

int cadence_qspi_apb_dma_read(struct cadence_spi_priv *priv,
			      const struct spi_mem_op *op)
{
	u32 reg, ret, rx_rem, n_rx, bytes_to_dma, data, status;
	u8 opcode, addr_bytes, *rxbuf, dummy_cycles, unaligned_byte;

	n_rx = op->data.nbytes;
	rxbuf = op->data.buf.in;
	rx_rem = n_rx % 4;
	bytes_to_dma = n_rx - rx_rem;

	if (bytes_to_dma) {
		cadence_qspi_apb_enable_linear_mode(false);
		reg = readl(priv->regbase + CQSPI_REG_CONFIG);
		reg |= CQSPI_REG_CONFIG_ENBL_DMA;
		writel(reg, priv->regbase + CQSPI_REG_CONFIG);

		writel(bytes_to_dma, priv->regbase + CQSPI_REG_INDIRECTRDBYTES);

		writel(CQSPI_DFLT_INDIR_TRIG_ADDR_RANGE,
		       priv->regbase + CQSPI_REG_INDIR_TRIG_ADDR_RANGE);
		writel(CQSPI_DFLT_DMA_PERIPH_CFG,
		       priv->regbase + CQSPI_REG_DMA_PERIPH_CFG);
		writel(lower_32_bits((unsigned long)rxbuf), priv->regbase +
		       CQSPI_DMA_DST_ADDR_REG);
		writel(upper_32_bits((unsigned long)rxbuf), priv->regbase +
		       CQSPI_DMA_DST_ADDR_MSB_REG);
		writel(priv->trigger_address, priv->regbase +
		       CQSPI_DMA_SRC_RD_ADDR_REG);
		writel(bytes_to_dma, priv->regbase +
		       CQSPI_DMA_DST_SIZE_REG);
		flush_dcache_range((unsigned long)rxbuf,
				   (unsigned long)rxbuf + bytes_to_dma);
		writel(CQSPI_DFLT_DST_CTRL_REG_VAL,
		       priv->regbase + CQSPI_DMA_DST_CTRL_REG);

		/* Start the indirect read transfer */
		writel(CQSPI_REG_INDIRECTRD_START, priv->regbase +
		       CQSPI_REG_INDIRECTRD);
		/* Wait for dma to complete transfer */
		ret = cadence_qspi_apb_wait_for_dma_cmplt(priv);
		if (ret)
			return ret;

		/* Clear indirect completion status */
		writel(CQSPI_REG_INDIRECTRD_DONE, priv->regbase +
		       CQSPI_REG_INDIRECTRD);
		rxbuf += bytes_to_dma;

		/* Disable DMA on completion */
		reg = readl(priv->regbase + CQSPI_REG_CONFIG);
		reg &= ~CQSPI_REG_CONFIG_ENBL_DMA;
		writel(reg, priv->regbase + CQSPI_REG_CONFIG);
	}

	if (rx_rem) {
		reg = readl(priv->regbase + CQSPI_REG_INDIRECTRDSTARTADDR);
		reg += bytes_to_dma;
		writel(reg, priv->regbase + CQSPI_REG_CMDADDRESS);

		addr_bytes = readl(priv->regbase + CQSPI_REG_SIZE) &
				   CQSPI_REG_SIZE_ADDRESS_MASK;

		opcode = (u8)readl(priv->regbase + CQSPI_REG_RD_INSTR);
		if (opcode == CMD_4BYTE_OCTAL_READ &&
		    priv->edge_mode != CQSPI_EDGE_MODE_DDR)
			opcode = CMD_4BYTE_FAST_READ;

		/* Set up command opcode extension. */
		status = readl(priv->regbase + CQSPI_REG_CONFIG);
		if (status & CQSPI_REG_CONFIG_DTR_PROTO) {
			ret = cadence_qspi_setup_opcode_ext(priv, op,
							    CQSPI_REG_OP_EXT_STIG_LSB);
			if (ret)
				return ret;
		}

		reg = opcode << CQSPI_REG_CMDCTRL_OPCODE_LSB;
		reg |= (0x1 << CQSPI_REG_CMDCTRL_RD_EN_LSB);
		reg |= (addr_bytes & CQSPI_REG_CMDCTRL_ADD_BYTES_MASK) <<
			CQSPI_REG_CMDCTRL_ADD_BYTES_LSB;
		reg |= (0x1 << CQSPI_REG_CMDCTRL_ADDR_EN_LSB);
		dummy_cycles = (readl(priv->regbase + CQSPI_REG_RD_INSTR) >>
				CQSPI_REG_RD_INSTR_DUMMY_LSB) &
				CQSPI_REG_RD_INSTR_DUMMY_MASK;
		reg |= (dummy_cycles & CQSPI_REG_CMDCTRL_DUMMY_MASK) <<
			CQSPI_REG_CMDCTRL_DUMMY_LSB;
		if (priv->edge_mode == CQSPI_EDGE_MODE_DDR && (rx_rem % 2) != 0)
			unaligned_byte = 1;
		else
			unaligned_byte = 0;
		reg |= (((rx_rem - 1 + unaligned_byte) &
			CQSPI_REG_CMDCTRL_RD_BYTES_MASK) <<
			CQSPI_REG_CMDCTRL_RD_BYTES_LSB);
		ret = cadence_qspi_apb_exec_flash_cmd(priv->regbase, reg);
		if (ret)
			return ret;

		data = readl(priv->regbase + CQSPI_REG_CMDREADDATALOWER);
		memcpy(rxbuf, &data, rx_rem);
	}

	return 0;
}

int cadence_qspi_apb_wait_for_dma_cmplt(struct cadence_spi_priv *priv)
{
	u32 timeout = CQSPI_DMA_TIMEOUT;

	while (!(readl(priv->regbase + CQSPI_DMA_DST_I_STS_REG) &
		 CQSPI_DMA_DST_I_STS_DONE) && timeout--)
		udelay(1);

	if (!timeout) {
		printf("DMA timeout\n");
		return -ETIMEDOUT;
	}

	writel(readl(priv->regbase + CQSPI_DMA_DST_I_STS_REG),
	       priv->regbase + CQSPI_DMA_DST_I_STS_REG);
	return 0;
}

static const struct soc_attr matches[] = {
	{ .family = "Versal", .revision = "v2" },
	{ }
};

/*
 * cadence_qspi_versal_set_dll_mode checks for silicon version
 * and set the DLL mode.
 * Returns 0 in case of success, -ENOTSUPP in case of failure.
 */
int cadence_qspi_versal_set_dll_mode(struct udevice *dev)
{
	struct cadence_spi_priv *priv = dev_get_priv(dev);
	const struct soc_attr *attr;

	attr = soc_device_match(matches);
	if (attr) {
		priv->dll_mode = CQSPI_DLL_MODE_MASTER;
		return 0;
	}

	return -ENOTSUPP;
}

int cadence_spi_versal_ctrl_reset(struct cadence_spi_priv *priv)
{
	int ret;

	if (CONFIG_IS_ENABLED(ZYNQMP_FIRMWARE)) {
		/* Assert ospi controller */
		ret = reset_assert(priv->resets->resets);
		if (ret)
			return ret;

		udelay(10);

		/* Deassert ospi controller */
		ret = reset_deassert(priv->resets->resets);
		if (ret)
			return ret;
	} else {
		/* Assert ospi controller */
		setbits_le32((u32 *)OSPI_CTRL_RST, 1);

		udelay(10);

		/* Deassert ospi controller */
		clrbits_le32((u32 *)OSPI_CTRL_RST, 1);
	}

	return 0;
}

int cadence_qspi_versal_flash_reset(struct udevice *dev)
{
	/* CRP WPROT */
	writel(0, WPROT_CRP);
	/* GPIO Reset */
	writel(0, RST_GPIO);

	/* disable IOU write protection */
	writel(0, WPROT_LPD_MIO);

	/* set direction as output */
	writel((readl(BOOT_MODE_DIR) | BIT(FLASH_RESET_GPIO)),
	       BOOT_MODE_DIR);

	/* Data output enable */
	writel((readl(BOOT_MODE_OUT) | BIT(FLASH_RESET_GPIO)),
	       BOOT_MODE_OUT);

	/* IOU SLCR write enable */
	writel(0, WPROT_PMC_MIO);

	/* set MIO as GPIO */
	writel(0x60, MIO_PIN_12);

	/* Set value 1 to pin */
	writel((readl(BANK0_OUTPUT) | BIT(FLASH_RESET_GPIO)), BANK0_OUTPUT);
	udelay(10);

	/* Disable Tri-state */
	writel((readl(BANK0_TRI) & ~BIT(FLASH_RESET_GPIO)), BANK0_TRI);
	udelay(5);

	/* Set value 0 to pin */
	writel((readl(BANK0_OUTPUT) & ~BIT(FLASH_RESET_GPIO)), BANK0_OUTPUT);
	udelay(150);

	/* Set value 1 to pin */
	writel((readl(BANK0_OUTPUT) | BIT(FLASH_RESET_GPIO)), BANK0_OUTPUT);
	udelay(1200);

	return 0;
}

void cadence_qspi_apb_enable_linear_mode(bool enable)
{
	if (IS_ENABLED(CONFIG_ZYNQMP_FIRMWARE)) {
		if (enable)
			/* ahb read mode */
			xilinx_pm_request(PM_IOCTL, PM_DEV_OSPI,
					  IOCTL_OSPI_MUX_SELECT,
					  PM_OSPI_MUX_SEL_LINEAR, 0, NULL);
		else
			/* DMA mode */
			xilinx_pm_request(PM_IOCTL, PM_DEV_OSPI,
					  IOCTL_OSPI_MUX_SELECT,
					  PM_OSPI_MUX_SEL_DMA, 0, NULL);
	} else {
		if (enable)
			writel(readl(VERSAL_AXI_MUX_SEL) |
			       VERSAL_OSPI_LINEAR_MODE, VERSAL_AXI_MUX_SEL);
		else
			writel(readl(VERSAL_AXI_MUX_SEL) &
			       ~VERSAL_OSPI_LINEAR_MODE, VERSAL_AXI_MUX_SEL);
	}
}
