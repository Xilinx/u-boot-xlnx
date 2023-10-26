// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2012
 * Altera Corporation <www.altera.com>
 */

#include <common.h>
#include <clk.h>
#include <log.h>
#include <asm-generic/io.h>
#include <dm.h>
#include <fdtdec.h>
#include <malloc.h>
#include <reset.h>
#include <spi.h>
#include <spi-mem.h>
#include <dm/device_compat.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/sizes.h>
#include <zynqmp_firmware.h>
#include "cadence_qspi.h"
#include <dt-bindings/power/xlnx-versal-power.h>

#define NSEC_PER_SEC			1000000000L

#define CQSPI_STIG_READ			0
#define CQSPI_STIG_WRITE		1
#define CQSPI_READ			2
#define CQSPI_WRITE			3

__weak int cadence_qspi_apb_dma_read(struct cadence_spi_priv *priv,
				     const struct spi_mem_op *op)
{
	return 0;
}

__weak int cadence_qspi_versal_flash_reset(struct udevice *dev)
{
	return 0;
}

static int cadence_spi_write_speed(struct udevice *bus, uint hz)
{
	struct cadence_spi_priv *priv = dev_get_priv(bus);

	cadence_qspi_apb_config_baudrate_div(priv->regbase,
					     priv->ref_clk_hz, hz);

	/* Reconfigure delay timing if speed is changed. */
	cadence_qspi_apb_delay(priv->regbase, priv->ref_clk_hz, hz,
			       priv->tshsl_ns, priv->tsd2d_ns,
			       priv->tchsh_ns, priv->tslch_ns);

	return 0;
}

static int cadence_spi_read_id(struct cadence_spi_priv *priv, u8 len,
			       u8 *idcode)
{
	int err;

	struct spi_mem_op op = SPI_MEM_OP(SPI_MEM_OP_CMD(0x9F, 1),
					  SPI_MEM_OP_NO_ADDR,
					  SPI_MEM_OP_NO_DUMMY,
					  SPI_MEM_OP_DATA_IN(len, idcode, 1));

	err = cadence_qspi_apb_command_read_setup(priv, &op);
	if (!err)
		err = cadence_qspi_apb_command_read(priv, &op);

	return err;
}

/* Calibration sequence to determine the read data capture delay register */
static int spi_calibration(struct udevice *bus, uint hz)
{
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	void *base = priv->regbase;
	unsigned int idcode = 0, temp = 0;
	int err = 0, i, range_lo = -1, range_hi = -1;

	/* start with slowest clock (1 MHz) */
	cadence_spi_write_speed(bus, 1000000);

	/* configure the read data capture delay register to 0 */
	cadence_qspi_apb_readdata_capture(base, 1, 0);

	/* Enable QSPI */
	cadence_qspi_apb_controller_enable(base);

	if (!priv->ddr_init) {
		/* read the ID which will be our golden value */
		err = cadence_spi_read_id(priv, CQSPI_READ_ID_LEN, (u8 *)&idcode);
		if (err) {
			puts("SF: Calibration failed (read)\n");
			return err;
		}

		/* Save flash id's for further use */
		priv->device_id[0] = (u8)idcode;
		priv->device_id[1] = (u8)(idcode >> 8);
		priv->device_id[2] = (u8)(idcode >> 16);
	} else {
		idcode = priv->device_id[0] | priv->device_id[1] << 8 |
			 priv->device_id[2] << 16;
	}

	/* use back the intended clock and find low range */
	cadence_spi_write_speed(bus, hz);
	for (i = 0; i < CQSPI_READ_CAPTURE_MAX_DELAY; i++) {
		/* Disable QSPI */
		cadence_qspi_apb_controller_disable(base);

		/* reconfigure the read data capture delay register */
		cadence_qspi_apb_readdata_capture(base, 1, i);

		/* Enable back QSPI */
		cadence_qspi_apb_controller_enable(base);

		/* issue a RDID to get the ID value */
		err = cadence_spi_read_id(priv, CQSPI_READ_ID_LEN, (u8 *)&temp);
		if (err) {
			puts("SF: Calibration failed (read)\n");
			return err;
		}

		/* search for range lo */
		if (range_lo == -1 && temp == idcode) {
			range_lo = i;
			continue;
		}

		/* search for range hi */
		if (range_lo != -1 && temp != idcode) {
			range_hi = i - 1;
			break;
		}
		range_hi = i;
	}

	if (range_lo == -1) {
		puts("SF: Calibration failed (low range)\n");
		return err;
	}

	/* Disable QSPI for subsequent initialization */
	cadence_qspi_apb_controller_disable(base);

	/* configure the final value for read data capture delay register */
	cadence_qspi_apb_readdata_capture(base, 1, (range_hi + range_lo) / 2);
	debug("SF: Read data capture delay calibrated to %i (%i - %i)\n",
	      (range_hi + range_lo) / 2, range_lo, range_hi);

	/* just to ensure we do once only when speed or chip select change */
	priv->qspi_calibrated_hz = hz;
	priv->qspi_calibrated_cs = priv->cs;

	return 0;
}

static int cadence_spi_set_speed(struct udevice *bus, uint hz)
{
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	int err;

	if (!hz || hz > priv->max_hz)
		hz = priv->max_hz;
	/* Disable QSPI */
	cadence_qspi_apb_controller_disable(priv->regbase);

	/*
	 * If the device tree already provides a read delay value, use that
	 * instead of calibrating.
	 */
	if (priv->read_delay >= 0) {
		cadence_spi_write_speed(bus, hz);
		cadence_qspi_apb_readdata_capture(priv->regbase, 1,
						  priv->read_delay);
	} else if (priv->previous_hz != hz ||
		   priv->qspi_calibrated_hz != hz ||
		   priv->qspi_calibrated_cs != priv->cs) {
		/*
		 * Calibration required for different current SCLK speed,
		 * requested SCLK speed or chip select
		 */
		err = spi_calibration(bus, hz);
		if (err)
			return err;

		/* prevent calibration run when same as previous request */
		priv->previous_hz = hz;
	}

	/* Enable QSPI */
	cadence_qspi_apb_controller_enable(priv->regbase);

	debug("%s: speed=%d\n", __func__, hz);

	return 0;
}

static int cadence_spi_child_pre_probe(struct udevice *bus)
{
	struct spi_slave *slave = dev_get_parent_priv(bus);

	slave->bytemode = SPI_4BYTE_MODE;

	return 0;
}

__weak int cadence_qspi_versal_set_dll_mode(struct udevice *dev)
{
	return -ENOTSUPP;
}

static int cadence_spi_probe(struct udevice *bus)
{
	struct cadence_spi_plat *plat = dev_get_plat(bus);
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	struct clk clk;
	int ret;

	priv->regbase		= plat->regbase;
	priv->ahbbase		= plat->ahbbase;
	priv->is_dma		= plat->is_dma;
	priv->is_decoded_cs	= plat->is_decoded_cs;
	priv->fifo_depth	= plat->fifo_depth;
	priv->fifo_width	= plat->fifo_width;
	priv->trigger_address	= plat->trigger_address;
	priv->read_delay	= plat->read_delay;
	priv->ahbsize		= plat->ahbsize;
	priv->max_hz		= plat->max_hz;

	priv->page_size		= plat->page_size;
	priv->block_size	= plat->block_size;
	priv->tshsl_ns		= plat->tshsl_ns;
	priv->tsd2d_ns		= plat->tsd2d_ns;
	priv->tchsh_ns		= plat->tchsh_ns;
	priv->tslch_ns		= plat->tslch_ns;

	if (CONFIG_IS_ENABLED(ZYNQMP_FIRMWARE))
		xilinx_pm_request(PM_REQUEST_NODE, PM_DEV_OSPI,
				  ZYNQMP_PM_CAPABILITY_ACCESS, ZYNQMP_PM_MAX_QOS,
				  ZYNQMP_PM_REQUEST_ACK_NO, NULL);

	if (priv->ref_clk_hz == 0) {
		ret = clk_get_by_index(bus, 0, &clk);
		if (ret) {
#ifdef CONFIG_HAS_CQSPI_REF_CLK
			priv->ref_clk_hz = CONFIG_CQSPI_REF_CLK;
#elif defined(CONFIG_ARCH_SOCFPGA)
			priv->ref_clk_hz = cm_get_qspi_controller_clk_hz();
#else
			return ret;
#endif
		} else {
			priv->ref_clk_hz = clk_get_rate(&clk);
			clk_free(&clk);
			if (IS_ERR_VALUE(priv->ref_clk_hz))
				return priv->ref_clk_hz;
		}
	}

	priv->resets = devm_reset_bulk_get_optional(bus);
	if (priv->resets)
		reset_deassert_bulk(priv->resets);

	if (!priv->qspi_is_init) {
		cadence_qspi_apb_controller_init(priv);
		priv->qspi_is_init = 1;
	}

	priv->edge_mode = CQSPI_EDGE_MODE_SDR;
	priv->dll_mode = CQSPI_DLL_MODE_BYPASS;

	/* Select dll mode */
	ret = cadence_qspi_versal_set_dll_mode(bus);
	if (ret == -ENOTSUPP)
		debug("DLL mode set to bypass mode : %x\n", ret);

	priv->wr_delay = 50 * DIV_ROUND_UP(NSEC_PER_SEC, priv->ref_clk_hz);

	/* Versal and Versal-NET use spi calibration to set read delay */
	if (CONFIG_IS_ENABLED(ARCH_VERSAL) ||
	    CONFIG_IS_ENABLED(ARCH_VERSAL_NET))
		if (priv->read_delay >= 0)
			priv->read_delay = -1;

	/* Reset ospi flash device */
	return cadence_qspi_versal_flash_reset(bus);
}

static int cadence_spi_remove(struct udevice *dev)
{
	struct cadence_spi_priv *priv = dev_get_priv(dev);
	int ret = 0;

	if (priv->resets)
		ret = reset_release_bulk(priv->resets);

	return ret;
}

static int cadence_spi_set_mode(struct udevice *bus, uint mode)
{
	struct cadence_spi_priv *priv = dev_get_priv(bus);

	/* Disable QSPI */
	cadence_qspi_apb_controller_disable(priv->regbase);

	/* Set SPI mode */
	cadence_qspi_apb_set_clk_mode(priv->regbase, mode);

	/* Enable Direct Access Controller */
	if (priv->use_dac_mode)
		cadence_qspi_apb_dac_mode_enable(priv->regbase);

	/* Enable QSPI */
	cadence_qspi_apb_controller_enable(priv->regbase);

	return 0;
}

static int cadence_qspi_rx_dll_tuning(struct cadence_spi_priv *priv,
				      u32 txtap, u8 extra_dummy)
{
	void *regbase = priv->regbase;
	int ret, i, j;
	u8 id[CQSPI_READ_ID_LEN + 1], min_rxtap = 0, max_rxtap = 0, avg_rxtap,
	max_tap, windowsize, dummy_flag = 0, max_index = 0, min_index = 0;
	s8 max_windowsize = -1;
	bool id_matched, rxtapfound = false;
	struct spi_mem_op op =
		SPI_MEM_OP(SPI_MEM_OP_CMD(CQSPI_READ_ID | (CQSPI_READ_ID << 8), 8),
			   SPI_MEM_OP_NO_ADDR,
			   SPI_MEM_OP_DUMMY(8, 8),
			   SPI_MEM_OP_DATA_IN(CQSPI_READ_ID_LEN, id, 8));

	op.cmd.nbytes = 2;
	op.dummy.nbytes *= 2;
	op.cmd.dtr = true;
	op.addr.dtr = true;
	op.data.dtr = true;

	max_tap = CQSPI_MAX_DLL_TAPS;
	/*
	 * Rx dll tuning is done by setting tx delay and increment rx
	 * delay and check for correct flash id's by reading from flash.
	 */
	for (i = 0; i <= max_tap; i++) {
		/* Set DLL reset bit */
		writel((txtap | i | CQSPI_REG_PHY_CONFIG_RESET_FLD_MASK),
		       regbase + CQSPI_REG_PHY_CONFIG);
		/*
		 * Re-synchronisation delay lines to update them
		 * with values from TX DLL Delay and RX DLL Delay fields
		 */
		writel((CQSPI_REG_PHY_CONFIG_RESYNC_FLD_MASK | txtap | i |
		       CQSPI_REG_PHY_CONFIG_RESET_FLD_MASK),
		       regbase + CQSPI_REG_PHY_CONFIG);
		/* Check lock of loopback */
		if (priv->dll_mode == CQSPI_DLL_MODE_MASTER) {
			ret = wait_for_bit_le32
				(regbase + CQSPI_REG_DLL_LOWER,
				 CQSPI_REG_DLL_LOWER_LPBK_LOCK_MASK, 1,
				 CQSPI_TIMEOUT_MS, 0);
			if (ret) {
				printf("LOWER_DLL_LOCK bit err: %i\n", ret);
				return ret;
			}
		}

		ret = cadence_qspi_apb_command_read_setup(priv, &op);
		if (!ret) {
			ret = cadence_qspi_apb_command_read(priv, &op);
			if (ret < 0) {
				printf("error %d reading JEDEC ID\n", ret);
				return ret;
			}
		}

		id_matched = true;
		for (j = 0; j < CQSPI_READ_ID_LEN; j++) {
			if (priv->device_id[j] != id[j]) {
				id_matched = false;
				break;
			}
		}

		if (id_matched && !rxtapfound) {
			if (priv->dll_mode == CQSPI_DLL_MODE_MASTER) {
				min_rxtap =
				readl(regbase + CQSPI_REG_DLL_OBSVBLE_UPPER) &
				      CQSPI_REG_DLL_UPPER_RX_FLD_MASK;
				max_rxtap = min_rxtap;
				max_index = i;
				min_index = i;
			} else {
				min_rxtap = i;
				max_rxtap = i;
			}
			rxtapfound = true;
		}

		if (id_matched && rxtapfound) {
			if (priv->dll_mode == CQSPI_DLL_MODE_MASTER) {
				max_rxtap =
					readl(regbase +
					      CQSPI_REG_DLL_OBSVBLE_UPPER) &
					      CQSPI_REG_DLL_UPPER_RX_FLD_MASK;
				max_index = i;
			} else {
				max_rxtap = i;
			}
		}

		if ((!id_matched || i == max_tap) && rxtapfound) {
			windowsize = max_rxtap - min_rxtap + 1;
			if (windowsize > max_windowsize) {
				dummy_flag = extra_dummy;
				max_windowsize = windowsize;
				if (priv->dll_mode == CQSPI_DLL_MODE_MASTER)
					avg_rxtap = (max_index + min_index);
				else
					avg_rxtap = (max_rxtap + min_rxtap);
				avg_rxtap /= 2;
			}

			if (windowsize >= 3)
				i = max_tap;

			rxtapfound = false;
		}
	}

	if (!extra_dummy) {
		rxtapfound = false;
		min_rxtap = 0;
		max_rxtap = 0;
	}

	if (!dummy_flag)
		priv->extra_dummy = false;

	if (max_windowsize < 3)
		return -EINVAL;

	return avg_rxtap;
}

static int cadence_spi_setdlldelay(struct udevice *bus)
{
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	void *regbase = priv->regbase;
	u32 txtap;
	int ret, rxtap;
	u8 extra_dummy;

	ret = wait_for_bit_le32(regbase + CQSPI_REG_CONFIG,
				1 << CQSPI_REG_CONFIG_IDLE_LSB,
				1, CQSPI_TIMEOUT_MS, 0);
	if (ret) {
		printf("spi_wait_idle error : 0x%x\n", ret);
		return ret;
	}

	if (priv->dll_mode == CQSPI_DLL_MODE_MASTER) {
		/* Drive DLL reset bit to low */
		writel(0, regbase + CQSPI_REG_PHY_CONFIG);

		/* Set initial delay value */
		writel(CQSPI_REG_PHY_INITIAL_DLY,
		       regbase + CQSPI_REG_PHY_MASTER_CTRL);
		/* Set DLL reset bit */
		writel(CQSPI_REG_PHY_CONFIG_RESET_FLD_MASK,
		       regbase + CQSPI_REG_PHY_CONFIG);

		/* Check for loopback lock */
		ret = wait_for_bit_le32(regbase + CQSPI_REG_DLL_LOWER,
					CQSPI_REG_DLL_LOWER_LPBK_LOCK_MASK,
					1, CQSPI_TIMEOUT_MS, 0);
		if (ret) {
			printf("Loopback lock bit error (%i)\n", ret);
			return ret;
		}

		/* Re-synchronize slave DLLs */
		writel(CQSPI_REG_PHY_CONFIG_RESET_FLD_MASK,
		       regbase + CQSPI_REG_PHY_CONFIG);
		writel(CQSPI_REG_PHY_CONFIG_RESET_FLD_MASK |
		       CQSPI_REG_PHY_CONFIG_RESYNC_FLD_MASK,
		       regbase + CQSPI_REG_PHY_CONFIG);

		txtap = CQSPI_TX_TAP_MASTER <<
			CQSPI_REG_PHY_CONFIG_TX_DLL_DLY_LSB;
	}

	priv->extra_dummy = false;
	for (extra_dummy = 0; extra_dummy <= 1; extra_dummy++) {
		if (extra_dummy)
			priv->extra_dummy = true;

		rxtap = cadence_qspi_rx_dll_tuning(priv, txtap, extra_dummy);
		if (extra_dummy && rxtap < 0) {
			printf("Failed RX dll tuning\n");
			return rxtap;
		}
	}
	debug("RXTAP: %d\n", rxtap);

	writel((txtap | rxtap | CQSPI_REG_PHY_CONFIG_RESET_FLD_MASK),
	       regbase + CQSPI_REG_PHY_CONFIG);
	writel((CQSPI_REG_PHY_CONFIG_RESYNC_FLD_MASK | txtap | rxtap |
	       CQSPI_REG_PHY_CONFIG_RESET_FLD_MASK),
	       regbase + CQSPI_REG_PHY_CONFIG);

	if (priv->dll_mode == CQSPI_DLL_MODE_MASTER) {
		ret = wait_for_bit_le32(regbase + CQSPI_REG_DLL_LOWER,
					CQSPI_REG_DLL_LOWER_LPBK_LOCK_MASK,
					1, CQSPI_TIMEOUT_MS, 0);
		if (ret) {
			printf("LOWER_DLL_LOCK bit err: %i\n", ret);
			return ret;
		}
	}

	return 0;
}

static int priv_setup_ddrmode(struct udevice *bus)
{
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	void *regbase = priv->regbase;
	int ret;

	ret = wait_for_bit_le32(regbase + CQSPI_REG_CONFIG,
				1 << CQSPI_REG_CONFIG_IDLE_LSB,
				1, CQSPI_TIMEOUT_MS, 0);
	if (ret) {
		printf("spi_wait_idle error : 0x%x\n", ret);
		return ret;
	}

	/* Disable QSPI */
	cadence_qspi_apb_controller_disable(regbase);

	/* Disable DAC mode */
	if (priv->use_dac_mode) {
		clrbits_le32(regbase + CQSPI_REG_CONFIG,
			     CQSPI_REG_CONFIG_DIRECT);
		priv->use_dac_mode = false;
	}

	setbits_le32(regbase + CQSPI_REG_CONFIG,
		     CQSPI_REG_CONFIG_PHY_ENABLE_MASK);

	/* Program POLL_CNT */
	clrsetbits_le32(regbase + CQSPI_REG_WRCOMPLETION,
			CQSPI_REG_WRCOMPLETION_POLLCNT_MASK,
			CQSPI_REG_WRCOMPLETION_POLLCNT <<
			CQSPI_REG_WRCOMPLETION_POLLCNY_LSB);

	setbits_le32(regbase + CQSPI_REG_CONFIG,
		     CQSPI_REG_CONFIG_DTR_PROT_EN_MASK);

	clrsetbits_le32(regbase + CQSPI_REG_RD_DATA_CAPTURE,
			(CQSPI_REG_RD_DATA_CAPTURE_DELAY_MASK <<
			 CQSPI_REG_RD_DATA_CAPTURE_DELAY_LSB),
			CQSPI_REG_READCAPTURE_DQS_ENABLE);

	/* Enable QSPI */
	cadence_qspi_apb_controller_enable(regbase);

	return 0;
}

static int cadence_spi_setup_ddrmode(struct udevice *bus)
{
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	int ret;

	if (priv->ddr_init)
		return 0;

	ret = priv_setup_ddrmode(bus);
	if (ret)
		return ret;

	priv->edge_mode = CQSPI_EDGE_MODE_DDR;
	ret = cadence_spi_setdlldelay(bus);
	if (ret) {
		printf("DDR tuning failed with error %d\n", ret);
		return ret;
	}
	priv->ddr_init = true;

	return 0;
}

static int cadence_spi_setup_strmode(struct udevice *bus)
{
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	void *base = priv->regbase;
	int ret;

	if (!priv->ddr_init)
		return 0;

	/* Reset ospi controller */
	ret = cadence_spi_versal_ctrl_reset(priv);
	if (ret) {
		printf("Cadence ctrl reset failed err: %d\n", ret);
		return ret;
	}

	ret = wait_for_bit_le32(base + CQSPI_REG_CONFIG,
				1 << CQSPI_REG_CONFIG_IDLE_LSB,
				1, CQSPI_TIMEOUT_MS, 0);
	if (ret) {
		printf("spi_wait_idle error : 0x%x\n", ret);
		return ret;
	}

	cadence_qspi_apb_controller_init(priv);
	priv->edge_mode = CQSPI_EDGE_MODE_SDR;
	priv->extra_dummy = 0;
	priv->previous_hz = 0;
	priv->qspi_calibrated_hz = 0;

	/* Setup default speed and calibrate */
	ret = cadence_spi_set_speed(bus, 0);
	if (ret)
		return ret;

	priv->ddr_init = false;

	return 0;
}

static int cadence_spi_mem_exec_op(struct spi_slave *spi,
				   const struct spi_mem_op *op)
{
	struct udevice *bus = spi->dev->parent;
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	void *base = priv->regbase;
	int err = 0;
	u32 mode;

	if (!op->cmd.dtr) {
		err = cadence_spi_setup_strmode(bus);
		if (err)
			return err;
	}

	if (!CONFIG_IS_ENABLED(SPI_FLASH_DTR_ENABLE) && op->cmd.dtr)
		return 0;

	if (spi->flags & SPI_XFER_U_PAGE)
		priv->cs = CQSPI_CS1;
	else
		priv->cs = CQSPI_CS0;

	/* Set Chip select */
	cadence_qspi_apb_chipselect(base, priv->cs, priv->is_decoded_cs);

	if (op->data.dir == SPI_MEM_DATA_IN && op->data.buf.in) {
		if (!op->addr.nbytes)
			mode = CQSPI_STIG_READ;
		else
			mode = CQSPI_READ;
	} else {
		if (!op->addr.nbytes || !op->data.buf.out)
			mode = CQSPI_STIG_WRITE;
		else
			mode = CQSPI_WRITE;
	}

	switch (mode) {
	case CQSPI_STIG_READ:
		err = cadence_qspi_apb_command_read_setup(priv, op);
		if (!err)
			err = cadence_qspi_apb_command_read(priv, op);
		break;
	case CQSPI_STIG_WRITE:
		err = cadence_qspi_apb_command_write_setup(priv, op);
		if (!err)
			err = cadence_qspi_apb_command_write(priv, op);
		break;
	case CQSPI_READ:
		err = cadence_qspi_apb_read_setup(priv, op);
		if (!err) {
			if (priv->is_dma)
				err = cadence_qspi_apb_dma_read(priv, op);
			else
				err = cadence_qspi_apb_read_execute(priv, op);
		}
		break;
	case CQSPI_WRITE:
		err = cadence_qspi_apb_write_setup(priv, op);
		if (!err)
			err = cadence_qspi_apb_write_execute(priv, op);
		break;
	default:
		err = -1;
		break;
	}

	if (CONFIG_IS_ENABLED(SPI_FLASH_DTR_ENABLE) &&
	    (spi->flags & SPI_XFER_SET_DDR))
		err = cadence_spi_setup_ddrmode(bus);

	return err;
}

bool cadence_spi_mem_dtr_supports_op(struct spi_slave *slave,
				     const struct spi_mem_op *op)
{
	/*
	 * In DTR mode, except op->cmd all other parameters like address,
	 * dummy and data could be 0.
	 * So lets only check if the cmd buswidth and number of opcode bytes
	 * are true for DTR to support.
	 */
	if (op->cmd.buswidth == 8 && op->cmd.nbytes % 2)
		return false;

	return true;
}

static bool cadence_spi_mem_supports_op(struct spi_slave *slave,
					const struct spi_mem_op *op)
{
	bool all_true, all_false;

	all_true = op->cmd.dtr && op->addr.dtr && op->dummy.dtr &&
		   op->data.dtr;
	all_false = !op->cmd.dtr && !op->addr.dtr && !op->dummy.dtr &&
		    !op->data.dtr;

	/* Mixed DTR modes not supported. */
	if (!(all_true || all_false))
		return false;

	if (all_true)
		return cadence_spi_mem_dtr_supports_op(slave, op);
	else
		return spi_mem_default_supports_op(slave, op);
}

static int cadence_spi_of_to_plat(struct udevice *bus)
{
	struct cadence_spi_plat *plat = dev_get_plat(bus);
	struct cadence_spi_priv *priv = dev_get_priv(bus);
	ofnode subnode;

	plat->regbase = (void *)devfdt_get_addr_index(bus, 0);
	plat->ahbbase = (void *)devfdt_get_addr_size_index(bus, 1,
			&plat->ahbsize);
	plat->is_decoded_cs = dev_read_bool(bus, "cdns,is-decoded-cs");
	plat->fifo_depth = dev_read_u32_default(bus, "cdns,fifo-depth", 128);
	plat->fifo_width = dev_read_u32_default(bus, "cdns,fifo-width", 4);
	plat->trigger_address = dev_read_u32_default(bus,
						     "cdns,trigger-address",
						     0);
	/* Use DAC mode only when MMIO window is at least 8M wide */
	if (plat->ahbsize >= SZ_8M)
		priv->use_dac_mode = true;

	plat->is_dma = dev_read_bool(bus, "cdns,is-dma");

	/* All other paramters are embedded in the child node */
	subnode = dev_read_first_subnode(bus);
	if (!ofnode_valid(subnode)) {
		printf("Error: subnode with SPI flash config missing!\n");
		return -ENODEV;
	}

	/* Use 500 KHz as a suitable default */
	plat->max_hz = ofnode_read_u32_default(subnode, "spi-max-frequency",
					       500000);

	/* Read other parameters from DT */
	plat->page_size = ofnode_read_u32_default(subnode, "page-size", 256);
	plat->block_size = ofnode_read_u32_default(subnode, "block-size", 16);
	plat->tshsl_ns = ofnode_read_u32_default(subnode, "cdns,tshsl-ns",
						 200);
	plat->tsd2d_ns = ofnode_read_u32_default(subnode, "cdns,tsd2d-ns",
						 255);
	plat->tchsh_ns = ofnode_read_u32_default(subnode, "cdns,tchsh-ns", 20);
	plat->tslch_ns = ofnode_read_u32_default(subnode, "cdns,tslch-ns", 20);
	/*
	 * Read delay should be an unsigned value but we use a signed integer
	 * so that negative values can indicate that the device tree did not
	 * specify any signed values and we need to perform the calibration
	 * sequence to find it out.
	 */
	plat->read_delay = ofnode_read_s32_default(subnode, "cdns,read-delay",
						   -1);

	debug("%s: regbase=%p ahbbase=%p max-frequency=%d page-size=%d\n",
	      __func__, plat->regbase, plat->ahbbase, plat->max_hz,
	      plat->page_size);

	return 0;
}

static const struct spi_controller_mem_ops cadence_spi_mem_ops = {
	.exec_op = cadence_spi_mem_exec_op,
	.supports_op = cadence_spi_mem_supports_op,
};

static const struct dm_spi_ops cadence_spi_ops = {
	.set_speed	= cadence_spi_set_speed,
	.set_mode	= cadence_spi_set_mode,
	.mem_ops	= &cadence_spi_mem_ops,
	/*
	 * cs_info is not needed, since we require all chip selects to be
	 * in the device tree explicitly
	 */
};

static const struct udevice_id cadence_spi_ids[] = {
	{ .compatible = "cdns,qspi-nor" },
	{ .compatible = "ti,am654-ospi" },
	{ }
};

U_BOOT_DRIVER(cadence_spi) = {
	.name = "cadence_spi",
	.id = UCLASS_SPI,
	.of_match = cadence_spi_ids,
	.ops = &cadence_spi_ops,
	.of_to_plat = cadence_spi_of_to_plat,
	.plat_auto	= sizeof(struct cadence_spi_plat),
	.priv_auto	= sizeof(struct cadence_spi_priv),
	.probe = cadence_spi_probe,
	.child_pre_probe = cadence_spi_child_pre_probe,
	.remove = cadence_spi_remove,
	.flags = DM_FLAG_OS_PREPARE,
};
