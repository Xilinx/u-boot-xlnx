// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 MediaTek Inc. All Rights Reserved.
 *
 * Author: Mingming Lee <Mingming.Lee@mediatek.com>
 *
 * MediaTek I2C Interface driver
 */

#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <i2c.h>
#include <log.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/errno.h>

#define I2C_RS_TRANSFER			BIT(4)
#define I2C_HS_NACKERR			BIT(2)
#define I2C_ACKERR			BIT(1)
#define I2C_TRANSAC_COMP		BIT(0)
#define I2C_TRANSAC_START		BIT(0)
#define I2C_RS_MUL_CNFG			BIT(15)
#define I2C_RS_MUL_TRIG			BIT(14)
#define I2C_DCM_DISABLE			0x0000
#define I2C_IO_CONFIG_OPEN_DRAIN	0x0003
#define I2C_IO_CONFIG_PUSH_PULL		0x0000
#define I2C_SOFT_RST			0x0001
#define I2C_FIFO_ADDR_CLR		0x0001
#define I2C_DELAY_LEN			0x0002
#define I2C_ST_START_CON		0x8001
#define I2C_FS_START_CON		0x1800
#define I2C_TIME_CLR_VALUE		0x0000
#define I2C_TIME_DEFAULT_VALUE		0x0003
#define I2C_WRRD_TRANAC_VALUE		0x0002
#define I2C_RD_TRANAC_VALUE		0x0001

#define I2C_DMA_CON_TX			0x0000
#define I2C_DMA_CON_RX			0x0001
#define I2C_DMA_START_EN		0x0001
#define I2C_DMA_INT_FLAG_NONE		0x0000
#define I2C_DMA_CLR_FLAG		0x0000
#define I2C_DMA_TX_RX			0x0000
#define I2C_DMA_HARD_RST		0x0002

#define MAX_ST_MODE_SPEED		100000
#define MAX_FS_MODE_SPEED		400000
#define MAX_HS_MODE_SPEED		3400000
#define MAX_SAMPLE_CNT_DIV		8
#define MAX_STEP_CNT_DIV		64
#define MAX_HS_STEP_CNT_DIV		8
#define I2C_DEFAULT_CLK_DIV		4

#define MAX_I2C_ADDR			0x7f
#define MAX_I2C_LEN			0xff
#define TRANS_ADDR_ONLY			BIT(8)
#define TRANSFER_TIMEOUT		50000  /* us */
#define I2C_FIFO_STAT1_MASK		0x001f
#define TIMING_SAMPLE_OFFSET		8
#define HS_SAMPLE_OFFSET		12
#define HS_STEP_OFFSET			8

#define I2C_CONTROL_WRAPPER		BIT(0)
#define I2C_CONTROL_RS			BIT(1)
#define I2C_CONTROL_DMA_EN		BIT(2)
#define I2C_CONTROL_CLK_EXT_EN		BIT(3)
#define I2C_CONTROL_DIR_CHANGE		BIT(4)
#define I2C_CONTROL_ACKERR_DET_EN	BIT(5)
#define I2C_CONTROL_TRANSFER_LEN_CHANGE BIT(6)
#define I2C_CONTROL_DMAACK		BIT(8)
#define I2C_CONTROL_ASYNC		BIT(9)

#define I2C_MASTER_WR			BIT(0)
#define I2C_MASTER_RD			BIT(1)
#define I2C_MASTER_WRRD			(I2C_MASTER_WR | I2C_MASTER_RD)

enum I2C_REGS_OFFSET {
	REG_PORT,
	REG_SLAVE_ADDR,
	REG_INTR_MASK,
	REG_INTR_STAT,
	REG_CONTROL,
	REG_TRANSFER_LEN,
	REG_TRANSAC_LEN,
	REG_DELAY_LEN,
	REG_TIMING,
	REG_START,
	REG_EXT_CONF,
	REG_FIFO_STAT1,
	REG_LTIMING,
	REG_FIFO_STAT,
	REG_FIFO_THRESH,
	REG_FIFO_ADDR_CLR,
	REG_IO_CONFIG,
	REG_RSV_DEBUG,
	REG_HS,
	REG_SOFTRESET,
	REG_DCM_EN,
	REG_PATH_DIR,
	REG_DEBUGSTAT,
	REG_DEBUGCTRL,
	REG_TRANSFER_LEN_AUX,
	REG_CLOCK_DIV,
	REG_SCL_HL_RATIO,
	REG_SCL_HS_HL_RATIO,
	REG_SCL_MIS_COMP_POINT,
	REG_STA_STOP_AC_TIME,
	REG_HS_STA_STOP_AC_TIME,
	REG_DATA_TIME,
};

enum DMA_REGS_OFFSET {
	REG_INT_FLAG = 0x0,
	REG_INT_EN = 0x04,
	REG_EN = 0x08,
	REG_RST = 0x0c,
	REG_CON = 0x18,
	REG_TX_MEM_ADDR = 0x1c,
	REG_RX_MEM_ADDR = 0x20,
	REG_TX_LEN = 0x24,
	REG_RX_LEN = 0x28,
};

static const uint mt_i2c_regs_v1[] = {
	[REG_PORT] = 0x0,
	[REG_SLAVE_ADDR] = 0x4,
	[REG_INTR_MASK] = 0x8,
	[REG_INTR_STAT] = 0xc,
	[REG_CONTROL] = 0x10,
	[REG_TRANSFER_LEN] = 0x14,
	[REG_TRANSAC_LEN] = 0x18,
	[REG_DELAY_LEN] = 0x1c,
	[REG_TIMING] = 0x20,
	[REG_START] = 0x24,
	[REG_EXT_CONF] = 0x28,
	[REG_FIFO_STAT1] = 0x2c,
	[REG_FIFO_STAT] = 0x30,
	[REG_FIFO_THRESH] = 0x34,
	[REG_FIFO_ADDR_CLR] = 0x38,
	[REG_IO_CONFIG] = 0x40,
	[REG_RSV_DEBUG] = 0x44,
	[REG_HS] = 0x48,
	[REG_SOFTRESET] = 0x50,
	[REG_SOFTRESET] = 0x50,
	[REG_DCM_EN] = 0x54,
	[REG_DEBUGSTAT] = 0x64,
	[REG_DEBUGCTRL] = 0x68,
	[REG_TRANSFER_LEN_AUX] = 0x6c,
	[REG_CLOCK_DIV] = 0x70,
	[REG_SCL_HL_RATIO] = 0x74,
	[REG_SCL_HS_HL_RATIO] = 0x78,
	[REG_SCL_MIS_COMP_POINT] = 0x7c,
	[REG_STA_STOP_AC_TIME] = 0x80,
	[REG_HS_STA_STOP_AC_TIME] = 0x84,
	[REG_DATA_TIME] = 0x88,
};

static const uint mt_i2c_regs_v2[] = {
	[REG_PORT] = 0x0,
	[REG_SLAVE_ADDR] = 0x4,
	[REG_INTR_MASK] = 0x8,
	[REG_INTR_STAT] = 0xc,
	[REG_CONTROL] = 0x10,
	[REG_TRANSFER_LEN] = 0x14,
	[REG_TRANSAC_LEN] = 0x18,
	[REG_DELAY_LEN] = 0x1c,
	[REG_TIMING] = 0x20,
	[REG_START] = 0x24,
	[REG_EXT_CONF] = 0x28,
	[REG_LTIMING] = 0x2c,
	[REG_HS] = 0x30,
	[REG_IO_CONFIG] = 0x34,
	[REG_FIFO_ADDR_CLR] = 0x38,
	[REG_TRANSFER_LEN_AUX] = 0x44,
	[REG_CLOCK_DIV] = 0x48,
	[REG_SOFTRESET] = 0x50,
	[REG_DEBUGSTAT] = 0xe0,
	[REG_DEBUGCTRL] = 0xe8,
	[REG_FIFO_STAT] = 0xf4,
	[REG_FIFO_THRESH] = 0xf8,
	[REG_DCM_EN] = 0xf88,
};

static const uint mt_i2c_regs_v3[] = {
	[REG_PORT] = 0x0,
	[REG_INTR_MASK] = 0x8,
	[REG_INTR_STAT] = 0xc,
	[REG_CONTROL] = 0x10,
	[REG_TRANSFER_LEN] = 0x14,
	[REG_TRANSAC_LEN] = 0x18,
	[REG_DELAY_LEN] = 0x1c,
	[REG_TIMING] = 0x20,
	[REG_START] = 0x24,
	[REG_EXT_CONF] = 0x28,
	[REG_LTIMING] = 0x2c,
	[REG_HS] = 0x30,
	[REG_IO_CONFIG] = 0x34,
	[REG_FIFO_ADDR_CLR] = 0x38,
	[REG_TRANSFER_LEN_AUX] = 0x44,
	[REG_CLOCK_DIV] = 0x48,
	[REG_SOFTRESET] = 0x50,
	[REG_SLAVE_ADDR] = 0x94,
	[REG_DEBUGSTAT] = 0xe4,
	[REG_DEBUGCTRL] = 0xe8,
	[REG_FIFO_STAT] = 0xf4,
	[REG_FIFO_THRESH] = 0xf8,
	[REG_DCM_EN] = 0xf88,
};

struct mtk_i2c_soc_data {
	const uint *regs;
	uint dma_sync: 1;
	uint ltiming_adjust: 1;
};

struct mtk_i2c_priv {
	/* set in i2c probe */
	void __iomem *base;		/* i2c base addr */
	void __iomem *pdmabase;		/* dma base address*/
	struct clk clk_main;		/* main clock for i2c bus */
	struct clk clk_dma;		/* DMA clock for i2c via DMA */
	struct clk clk_arb;		/* DMA clock for i2c ARB */
	struct clk clk_pmic;		/* DMA clock for i2c PMIC */
	const struct mtk_i2c_soc_data *soc_data; /* Compatible data for different IC */
	int op;				/* operation mode */
	bool zero_len;			/* Only transfer slave address, no data */
	bool pushpull;			/* push pull mode or open drain mode */
	bool filter_msg;		/* filter msg error log */
	bool auto_restart;		/* restart mode */
	bool ignore_restart_irq;	/* ignore restart IRQ */
	uint speed;			/* i2c speed, unit: hz */
};

static inline void i2c_writel(struct mtk_i2c_priv *priv, uint reg, uint value)
{
	u32 offset = priv->soc_data->regs[reg];

	writel(value, priv->base + offset);
}

static inline uint i2c_readl(struct mtk_i2c_priv *priv, uint offset)
{
	return readl(priv->base + priv->soc_data->regs[offset]);
}

static int mtk_i2c_clk_enable(struct mtk_i2c_priv *priv)
{
	int ret;

	ret = clk_enable(&priv->clk_main);
	if (ret)
		return log_msg_ret("enable clk_main", ret);

	ret = clk_enable(&priv->clk_dma);
	if (ret)
		return log_msg_ret("enable clk_dma", ret);

	if (priv->clk_arb.dev) {
		ret = clk_enable(&priv->clk_arb);
		if (ret)
			return log_msg_ret("enable clk_arb", ret);
	}

	if (priv->clk_pmic.dev) {
		ret = clk_enable(&priv->clk_pmic);
		if (ret)
			return log_msg_ret("enable clk_pmic", ret);
	}

	return 0;
}

static int mtk_i2c_clk_disable(struct mtk_i2c_priv *priv)
{
	int ret;

	if (priv->clk_pmic.dev) {
		ret = clk_disable(&priv->clk_pmic);
		if (ret)
			return log_msg_ret("disable clk_pmic", ret);
	}

	if (priv->clk_arb.dev) {
		ret = clk_disable(&priv->clk_arb);
		if (ret)
			return log_msg_ret("disable clk_arb", ret);
	}

	ret = clk_disable(&priv->clk_dma);
	if (ret)
		return log_msg_ret("disable clk_dma", ret);

	ret = clk_disable(&priv->clk_main);
	if (ret)
		return log_msg_ret("disable clk_main", ret);

	return 0;
}

static void mtk_i2c_init_hw(struct mtk_i2c_priv *priv)
{
	uint control_reg;

	writel(I2C_DMA_HARD_RST, priv->pdmabase + REG_RST);
	writel(I2C_DMA_CLR_FLAG, priv->pdmabase + REG_RST);
	i2c_writel(priv, REG_SOFTRESET, I2C_SOFT_RST);
	/* set ioconfig */
	if (priv->pushpull)
		i2c_writel(priv, REG_IO_CONFIG, I2C_IO_CONFIG_PUSH_PULL);
	else
		i2c_writel(priv, REG_IO_CONFIG, I2C_IO_CONFIG_OPEN_DRAIN);

	i2c_writel(priv, REG_DCM_EN, I2C_DCM_DISABLE);
	control_reg = I2C_CONTROL_ACKERR_DET_EN | I2C_CONTROL_CLK_EXT_EN;
	if (priv->soc_data->dma_sync)
		control_reg |= I2C_CONTROL_DMAACK | I2C_CONTROL_ASYNC;
	i2c_writel(priv, REG_CONTROL, control_reg);
	i2c_writel(priv, REG_DELAY_LEN, I2C_DELAY_LEN);
}

/*
 * Calculate i2c port speed
 *
 * Hardware design:
 * i2c_bus_freq = parent_clk / (clock_div * 2 * sample_cnt * step_cnt)
 * clock_div: fixed in hardware, but may be various in different SoCs
 *
 * The calculation want to pick the highest bus frequency that is still
 * less than or equal to target_speed. The calculation try to get
 * sample_cnt and step_cn
 * @param[in]
 *     clk_src: i2c clock source
 * @param[out]
 *     timing_step_cnt: step cnt calculate result
 * @param[out]
 *     timing_sample_cnt: sample cnt calculate result
 * @return
 *     0, set speed successfully.
 *     -EINVAL, Unsupported speed.
 */
static int mtk_i2c_calculate_speed(uint clk_src,
				   uint target_speed,
				   uint *timing_step_cnt,
				   uint *timing_sample_cnt)
{
	uint base_sample_cnt = MAX_SAMPLE_CNT_DIV;
	uint base_step_cnt;
	uint max_step_cnt;
	uint sample_cnt;
	uint step_cnt;
	uint opt_div;
	uint best_mul;
	uint cnt_mul;

	if (target_speed > MAX_HS_MODE_SPEED)
		target_speed = MAX_HS_MODE_SPEED;

	if (target_speed > MAX_FS_MODE_SPEED)
		max_step_cnt = MAX_HS_STEP_CNT_DIV;
	else
		max_step_cnt = MAX_STEP_CNT_DIV;

	base_step_cnt = max_step_cnt;
	/* Find the best combination */
	opt_div = DIV_ROUND_UP(clk_src >> 1, target_speed);
	best_mul = MAX_SAMPLE_CNT_DIV * max_step_cnt;

	/*
	 * Search for the best pair (sample_cnt, step_cnt) with
	 * 0 < sample_cnt < MAX_SAMPLE_CNT_DIV
	 * 0 < step_cnt < max_step_cnt
	 * sample_cnt * step_cnt >= opt_div
	 * optimizing for sample_cnt * step_cnt being minimal
	 */
	for (sample_cnt = 1; sample_cnt <= MAX_SAMPLE_CNT_DIV; sample_cnt++) {
		step_cnt = DIV_ROUND_UP(opt_div, sample_cnt);
		cnt_mul = step_cnt * sample_cnt;
		if (step_cnt > max_step_cnt)
			continue;

		if (cnt_mul < best_mul) {
			best_mul = cnt_mul;
			base_sample_cnt = sample_cnt;
			base_step_cnt = step_cnt;
			if (best_mul == opt_div)
				break;
		}
	}

	sample_cnt = base_sample_cnt;
	step_cnt = base_step_cnt;

	if ((clk_src / (2 * sample_cnt * step_cnt)) > target_speed) {
		/*
		 * In this case, hardware can't support such
		 * low i2c_bus_freq
		 */
		debug("Unsupported speed(%uhz)\n", target_speed);
		return log_msg_ret("calculate speed", -EINVAL);
	}

	*timing_step_cnt = step_cnt - 1;
	*timing_sample_cnt = sample_cnt - 1;

	return 0;
}

/*
 * mtk_i2c_set_speed
 *
 * @par Description
 *     Calculate i2c speed and write sample_cnt, step_cnt to TIMING register.
 * @param[in]
 *     dev: udevice pointer, struct udevice contains i2c source clock,
 *     clock divide and speed.
 * @return
 *     0, set speed successfully.\n
 *     error code from mtk_i2c_calculate_speed().
 */
static int mtk_i2c_set_speed(struct udevice *dev, uint speed)
{
	struct mtk_i2c_priv *priv = dev_get_priv(dev);
	uint high_speed_reg;
	uint sample_cnt;
	uint timing_reg;
	uint step_cnt;
	uint clk_src;
	int ret = 0;

	priv->speed = speed;
	if (mtk_i2c_clk_enable(priv))
		return log_msg_ret("set_speed enable clk", -1);

	clk_src = clk_get_rate(&priv->clk_main) / I2C_DEFAULT_CLK_DIV;
	i2c_writel(priv, REG_CLOCK_DIV, (I2C_DEFAULT_CLK_DIV - 1));
	if (priv->speed > MAX_FS_MODE_SPEED) {
		/* Set master code speed register */
		ret = mtk_i2c_calculate_speed(clk_src, MAX_FS_MODE_SPEED,
					      &step_cnt, &sample_cnt);
		if (ret < 0)
			goto exit;

		timing_reg = (sample_cnt << TIMING_SAMPLE_OFFSET) | step_cnt;
		i2c_writel(priv, REG_TIMING, timing_reg);
		/* Set the high speed mode register */
		ret = mtk_i2c_calculate_speed(clk_src, priv->speed,
					      &step_cnt, &sample_cnt);
		if (ret < 0)
			goto exit;

		high_speed_reg = I2C_TIME_DEFAULT_VALUE |
				(sample_cnt << HS_SAMPLE_OFFSET) |
				(step_cnt << HS_STEP_OFFSET);
		i2c_writel(priv, REG_HS, high_speed_reg);
		if (priv->soc_data->ltiming_adjust) {
			timing_reg = (sample_cnt << 12) | (step_cnt << 9);
			i2c_writel(priv, REG_LTIMING, timing_reg);
		}
	} else {
		ret = mtk_i2c_calculate_speed(clk_src, priv->speed,
					      &step_cnt, &sample_cnt);
		if (ret < 0)
			goto exit;

		timing_reg = (sample_cnt << TIMING_SAMPLE_OFFSET) | step_cnt;
		/* Disable the high speed transaction */
		high_speed_reg = I2C_TIME_CLR_VALUE;
		i2c_writel(priv, REG_TIMING, timing_reg);
		i2c_writel(priv, REG_HS, high_speed_reg);
		if (priv->soc_data->ltiming_adjust) {
			timing_reg = (sample_cnt << 6) | step_cnt;
			i2c_writel(priv, REG_LTIMING, timing_reg);
		}
	}

exit:
	if (mtk_i2c_clk_disable(priv))
		return log_msg_ret("set_speed disable clk", -1);

	return ret;
}

/*
 * mtk_i2c_do_transfer
 *
 * @par Description
 *     Configure i2c register and trigger transfer.
 * @param[in]
 *     priv: mtk_i2cmtk_i2c_priv pointer, struct mtk_i2c_priv contains register base\n
 *     address, operation mode, interrupt status and i2c driver data.
 * @param[in]
 *     msgs: i2c_msg pointer, struct i2c_msg contains slave\n
 *     address, operation mode, msg length and data buffer.
 * @param[in]
 *     num: i2c_msg number.
 * @param[in]
 *     left_num: left i2c_msg number.
 * @return
 *     0, i2c transfer successfully.\n
 *     -ETIMEDOUT, i2c transfer timeout.\n
 *     -EREMOTEIO, i2c transfer ack error.
 */
static int mtk_i2c_do_transfer(struct mtk_i2c_priv *priv,
			       struct i2c_msg *msgs,
			       int num, int left_num)
{
	struct i2c_msg *msg_rx = NULL;
	uint restart_flag = 0;
	uint trans_error = 0;
	uint irq_stat = 0;
	uint tmo_poll = 0;
	uint control_reg;
	bool tmo = false;
	uint start_reg;
	uint addr_reg;
	int ret = 0;

	if (priv->auto_restart)
		restart_flag = I2C_RS_TRANSFER;

	control_reg = i2c_readl(priv, REG_CONTROL) &
		~(I2C_CONTROL_DIR_CHANGE | I2C_CONTROL_RS);

	if (priv->speed > MAX_FS_MODE_SPEED || num > 1)
		control_reg |= I2C_CONTROL_RS;

	if (priv->op == I2C_MASTER_WRRD)
		control_reg |= I2C_CONTROL_DIR_CHANGE | I2C_CONTROL_RS;

	control_reg |= I2C_CONTROL_DMA_EN;
	i2c_writel(priv, REG_CONTROL, control_reg);

	/* set start condition */
	if (priv->speed <= MAX_ST_MODE_SPEED)
		i2c_writel(priv, REG_EXT_CONF, I2C_ST_START_CON);
	else
		i2c_writel(priv, REG_EXT_CONF, I2C_FS_START_CON);

	addr_reg = msgs->addr << 1;
	if (priv->op == I2C_MASTER_RD)
		addr_reg |= I2C_M_RD;
	if (priv->zero_len)
		i2c_writel(priv, REG_SLAVE_ADDR, addr_reg | TRANS_ADDR_ONLY);
	else
		i2c_writel(priv, REG_SLAVE_ADDR, addr_reg);

	/* clear interrupt status */
	i2c_writel(priv, REG_INTR_STAT, restart_flag | I2C_HS_NACKERR |
		   I2C_ACKERR | I2C_TRANSAC_COMP);
	i2c_writel(priv, REG_FIFO_ADDR_CLR, I2C_FIFO_ADDR_CLR);

	/* enable interrupt */
	i2c_writel(priv, REG_INTR_MASK, restart_flag | I2C_HS_NACKERR |
		   I2C_ACKERR | I2C_TRANSAC_COMP);

	/* set transfer and transaction len */
	if (priv->op == I2C_MASTER_WRRD) {
		i2c_writel(priv, REG_TRANSFER_LEN, msgs->len);
		i2c_writel(priv, REG_TRANSFER_LEN_AUX, (msgs + 1)->len);
		i2c_writel(priv, REG_TRANSAC_LEN, I2C_WRRD_TRANAC_VALUE);
	} else {
		i2c_writel(priv, REG_TRANSFER_LEN, msgs->len);
		i2c_writel(priv, REG_TRANSAC_LEN, num);
	}

	/* Clear DMA interrupt flag */
	writel(I2C_DMA_INT_FLAG_NONE, priv->pdmabase + REG_INT_FLAG);

	/* Flush cache for first msg */
	flush_cache((ulong)msgs->buf, msgs->len);

	/*
	 * prepare buffer data to start transfer
	 * three cases here: read, write, write then read
	 */
	if (priv->op & I2C_MASTER_WR) {
		/* Set DMA direction TX (w/ or w/o RX) */
		writel(I2C_DMA_CON_TX, priv->pdmabase + REG_CON);

		/* Write the tx buffer address to dma register */
		writel((ulong)msgs->buf, priv->pdmabase + REG_TX_MEM_ADDR);
		/* Write the tx length to dma register */
		writel(msgs->len, priv->pdmabase + REG_TX_LEN);

		if (priv->op & I2C_MASTER_RD) {
			/* write then read */
			msg_rx = msgs + 1;

			/* Flush cache for second msg */
			flush_cache((ulong)msg_rx->buf, msg_rx->len);
		}
	}

	if (priv->op & I2C_MASTER_RD) {
		if (!msg_rx) {
			/* Set DMA direction RX */
			writel(I2C_DMA_CON_RX, priv->pdmabase + REG_CON);

			msg_rx = msgs;
		}

		/* Write the rx buffer address to dma register */
		writel((ulong)msg_rx->buf, priv->pdmabase + REG_RX_MEM_ADDR);
		/* Write the rx length to dma register */
		writel(msg_rx->len, priv->pdmabase + REG_RX_LEN);
	}

	writel(I2C_DMA_START_EN, priv->pdmabase + REG_EN);

	if (!priv->auto_restart) {
		start_reg = I2C_TRANSAC_START;
	} else {
		start_reg = I2C_TRANSAC_START | I2C_RS_MUL_TRIG;
		if (left_num >= 1)
			start_reg |= I2C_RS_MUL_CNFG;
	}
	i2c_writel(priv, REG_START, start_reg);

	for (;;) {
		irq_stat = i2c_readl(priv, REG_INTR_STAT);

		/* ignore the first restart irq after the master code */
		if (priv->ignore_restart_irq && (irq_stat & restart_flag)) {
			priv->ignore_restart_irq = false;
			irq_stat = 0;
			i2c_writel(priv, REG_START, I2C_RS_MUL_CNFG |
				   I2C_RS_MUL_TRIG | I2C_TRANSAC_START);
		}

		if (irq_stat & (I2C_TRANSAC_COMP | restart_flag)) {
			tmo = false;
			if (irq_stat & (I2C_HS_NACKERR | I2C_ACKERR))
				trans_error = 1;

			break;
		}
		udelay(1);
		if (tmo_poll++ >= TRANSFER_TIMEOUT) {
			tmo = true;
			break;
		}
	}

	/* clear interrupt mask */
	i2c_writel(priv, REG_INTR_MASK, ~(restart_flag | I2C_HS_NACKERR |
		  I2C_ACKERR | I2C_TRANSAC_COMP));

	if (tmo || trans_error != 0) {
		if (tmo) {
			ret = -ETIMEDOUT;
			if (!priv->filter_msg)
				debug("I2C timeout! addr: 0x%x,\n", msgs->addr);
		} else {
			ret = -EREMOTEIO;
			if (!priv->filter_msg)
				debug("I2C ACKERR! addr: 0x%x,IRQ:0x%x\n",
				      msgs->addr, irq_stat);
		}
		mtk_i2c_init_hw(priv);
	}

	return ret;
}

/*
 * mtk_i2c_transfer
 *
 * @par Description
 *     Common i2c transfer API. Set i2c transfer mode according to i2c_msg\n
 *     information, then call mtk_i2c_do_transfer() to configure i2c register\n
 *     and trigger transfer.
 * @param[in]
 *     dev: udevice pointer, struct udevice contains struct mtk_i2c_priv, \n
 *	   struct mtk_i2c_priv contains register base\n
 *     address, operation mode, interrupt status and i2c driver data.
 * @param[in]
 *     msgs: i2c_msg pointer, struct i2c_msg contains slave\n
 *     address, operation mode, msg length and data buffer.
 * @param[in]
 *     num: i2c_msg number.
 * @return
 *     i2c_msg number, i2c transfer successfully.\n
 *     -EINVAL, msg length is more than 16\n
 *     use DMA MODE or slave address more than 0x7f.\n
 *     error code from mtk_i2c_init_base().\n
 *     error code from mtk_i2c_set_speed().\n
 *     error code from mtk_i2c_do_transfer().
 */
static int mtk_i2c_transfer(struct udevice *dev, struct i2c_msg *msg,
			    int nmsgs)
{
	struct mtk_i2c_priv *priv = dev_get_priv(dev);
	int left_num;
	uint num_cnt;
	int ret;

	priv->auto_restart = true;
	left_num = nmsgs;
	if (mtk_i2c_clk_enable(priv))
		return log_msg_ret("transfer enable clk", -1);

	for (num_cnt = 0; num_cnt < nmsgs; num_cnt++) {
		if (((msg + num_cnt)->addr) > MAX_I2C_ADDR) {
			ret = -EINVAL;
			goto err_exit;
		}
		if ((msg + num_cnt)->len > MAX_I2C_LEN) {
			ret = -EINVAL;
			goto err_exit;
		}
	}

	/* check if we can skip restart and optimize using WRRD mode */
	if (priv->auto_restart && nmsgs == 2) {
		if (!(msg[0].flags & I2C_M_RD) && (msg[1].flags & I2C_M_RD) &&
		    msg[0].addr == msg[1].addr) {
			priv->auto_restart = false;
		}
	}

	if (priv->auto_restart && nmsgs >= 2 && priv->speed > MAX_FS_MODE_SPEED)
		/* ignore the first restart irq after the master code,
		 * otherwise the first transfer will be discarded.
		 */
		priv->ignore_restart_irq = true;
	else
		priv->ignore_restart_irq = false;

	while (left_num--) {
		/* transfer slave address only to support devices detect */
		if (!msg->buf)
			priv->zero_len = true;
		else
			priv->zero_len = false;

		if (msg->flags & I2C_M_RD)
			priv->op = I2C_MASTER_RD;
		else
			priv->op = I2C_MASTER_WR;

		if (!priv->auto_restart) {
			if (nmsgs > 1) {
				/* combined two messages into one transaction */
				priv->op = I2C_MASTER_WRRD;
				left_num--;
			}
		}
		ret = mtk_i2c_do_transfer(priv, msg, nmsgs, left_num);
		if (ret < 0)
			goto err_exit;
		msg++;
	}
	ret = 0;

err_exit:
	if (mtk_i2c_clk_disable(priv))
		return log_msg_ret("transfer disable clk", -1);

	return ret;
}

static int mtk_i2c_of_to_plat(struct udevice *dev)
{
	struct mtk_i2c_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = dev_remap_addr_index(dev, 0);
	priv->pdmabase = dev_remap_addr_index(dev, 1);
	ret = clk_get_by_index(dev, 0, &priv->clk_main);
	if (ret)
		return log_msg_ret("clk_get_by_index 0", ret);

	ret = clk_get_by_index(dev, 1, &priv->clk_dma);

	/* optional i2c clock */
	clk_get_by_name(dev, "arb", &priv->clk_arb);
	clk_get_by_name(dev, "pmic", &priv->clk_pmic);

	return ret;
}

static int mtk_i2c_probe(struct udevice *dev)
{
	struct mtk_i2c_priv *priv = dev_get_priv(dev);

	priv->soc_data = (struct mtk_i2c_soc_data *)dev_get_driver_data(dev);

	if (mtk_i2c_clk_enable(priv))
		return log_msg_ret("probe enable clk", -1);

	mtk_i2c_init_hw(priv);
	if (mtk_i2c_clk_disable(priv))
		return log_msg_ret("probe disable clk", -1);

	return 0;
}

static int mtk_i2c_deblock(struct udevice *dev)
{
	struct mtk_i2c_priv *priv = dev_get_priv(dev);

	if (mtk_i2c_clk_enable(priv))
		return log_msg_ret("deblock enable clk", -1);

	mtk_i2c_init_hw(priv);

	if (mtk_i2c_clk_disable(priv))
		return log_msg_ret("deblock disable clk", -1);

	return 0;
}

static const struct mtk_i2c_soc_data mt76xx_soc_data = {
	.regs = mt_i2c_regs_v1,
	.dma_sync = 0,
	.ltiming_adjust = 0,
};

static const struct mtk_i2c_soc_data mt7981_soc_data = {
	.regs = mt_i2c_regs_v3,
	.dma_sync = 1,
	.ltiming_adjust = 1,
};

static const struct mtk_i2c_soc_data mt7986_soc_data = {
	.regs = mt_i2c_regs_v1,
	.dma_sync = 1,
	.ltiming_adjust = 0,
};

static const struct mtk_i2c_soc_data mt8183_soc_data = {
	.regs = mt_i2c_regs_v2,
	.dma_sync = 1,
	.ltiming_adjust = 0,
};

static const struct mtk_i2c_soc_data mt8518_soc_data = {
	.regs = mt_i2c_regs_v1,
	.dma_sync = 0,
	.ltiming_adjust = 0,
};

static const struct mtk_i2c_soc_data mt8512_soc_data = {
	.regs = mt_i2c_regs_v1,
	.dma_sync = 1,
	.ltiming_adjust = 0,
};

static const struct dm_i2c_ops mtk_i2c_ops = {
	.xfer		= mtk_i2c_transfer,
	.set_bus_speed	= mtk_i2c_set_speed,
	.deblock	= mtk_i2c_deblock,
};

static const struct udevice_id mtk_i2c_ids[] = {
	{
		.compatible = "mediatek,mt7622-i2c",
		.data = (ulong)&mt76xx_soc_data,
	}, {
		.compatible = "mediatek,mt7623-i2c",
		.data = (ulong)&mt76xx_soc_data,
	}, {
		.compatible = "mediatek,mt7629-i2c",
		.data = (ulong)&mt76xx_soc_data,
	}, {
		.compatible = "mediatek,mt7981-i2c",
		.data = (ulong)&mt7981_soc_data,
	}, {
		.compatible = "mediatek,mt7986-i2c",
		.data = (ulong)&mt7986_soc_data,
	}, {
		.compatible = "mediatek,mt8183-i2c",
		.data = (ulong)&mt8183_soc_data,
	}, {
		.compatible = "mediatek,mt8512-i2c",
		.data = (ulong)&mt8512_soc_data,
	}, {
		.compatible = "mediatek,mt8518-i2c",
		.data = (ulong)&mt8518_soc_data,
	}
};

U_BOOT_DRIVER(mtk_i2c) = {
	.name		= "mtk_i2c",
	.id		= UCLASS_I2C,
	.of_match	= mtk_i2c_ids,
	.of_to_plat	= mtk_i2c_of_to_plat,
	.probe		= mtk_i2c_probe,
	.priv_auto	= sizeof(struct mtk_i2c_priv),
	.ops		= &mtk_i2c_ops,
};
