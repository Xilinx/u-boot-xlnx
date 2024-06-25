/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2012
 * Altera Corporation <www.altera.com>
 */

#ifndef __CADENCE_QSPI_H__
#define __CADENCE_QSPI_H__

#include <reset.h>
#include <linux/mtd/spi-nor.h>
#include <spi-mem.h>
#include <wait_bit.h>

#define CQSPI_IS_ADDR(cmd_len)		(cmd_len > 1 ? 1 : 0)

#define CQSPI_NO_DECODER_MAX_CS		4
#define CQSPI_DECODER_MAX_CS		16
#define CQSPI_READ_CAPTURE_MAX_DELAY	16

#define CQSPI_CS0				0
#define CQSPI_CS1				1

#define CQSPI_REG_POLL_US                       1 /* 1us */
#define CQSPI_REG_RETRY                         10000
#define CQSPI_POLL_IDLE_RETRY                   3

#define CQSPI_TIMEOUT_MS			1000

/* Transfer mode */
#define CQSPI_INST_TYPE_SINGLE                  0
#define CQSPI_INST_TYPE_DUAL                    1
#define CQSPI_INST_TYPE_QUAD                    2
#define CQSPI_INST_TYPE_OCTAL                   3

#define CQSPI_STIG_DATA_LEN_MAX                 8

#define CQSPI_DUMMY_CLKS_PER_BYTE               8
#define CQSPI_DUMMY_BYTES_MAX                   4
#define CQSPI_DUMMY_CLKS_MAX                    31

#define CQSPI_TX_TAP_MASTER			0x1E
#define CQSPI_MAX_DLL_TAPS			127

#define CQSPI_DLL_MODE_MASTER			0
#define CQSPI_DLL_MODE_BYPASS			1

#define CQSPI_EDGE_MODE_SDR			0
#define CQSPI_EDGE_MODE_DDR			1

#define SILICON_VER_MASK			0xFF
#define SILICON_VER_1				0x10

#define CQSPI_READ_ID				0x9F
#define CQSPI_READ_ID_LEN			3
#define CQSPI_READID_LOOP_MAX			10
#define TERA_MACRO				1000000000000l
#define TAP_GRAN_SEL_MIN_FREQ			120000000

#define OSPI_CTRL_RST				0xF1260304

#define CMD_4BYTE_FAST_READ			0x0C
#define CMD_4BYTE_OCTAL_READ			0x7c
#define CMD_4BYTE_READ				0x13

/****************************************************************************
 * Controller's configuration and status register (offset from QSPI_BASE)
 ****************************************************************************/
#define CQSPI_REG_CONFIG                        0x00
#define CQSPI_REG_CONFIG_ENABLE                 BIT(0)
#define CQSPI_REG_CONFIG_CLK_POL                BIT(1)
#define CQSPI_REG_CONFIG_CLK_PHA                BIT(2)
#define CQSPI_REG_CONFIG_PHY_ENABLE_MASK        BIT(3)
#define CQSPI_REG_CONFIG_DIRECT                 BIT(7)
#define CQSPI_REG_CONFIG_DECODE                 BIT(9)
#define CQSPI_REG_CONFIG_ENBL_DMA               BIT(15)
#define CQSPI_REG_CONFIG_XIP_IMM                BIT(18)
#define CQSPI_REG_CONFIG_DTR_PROT_EN_MASK       BIT(24)
#define CQSPI_REG_CONFIG_CHIPSELECT_LSB         10
#define CQSPI_REG_CONFIG_BAUD_LSB               19
#define CQSPI_REG_CONFIG_DTR_PROTO		BIT(24)
#define CQSPI_REG_CONFIG_DUAL_OPCODE		BIT(30)
#define CQSPI_REG_CONFIG_IDLE_LSB               31
#define CQSPI_REG_CONFIG_CHIPSELECT_MASK        0xF
#define CQSPI_REG_CONFIG_BAUD_MASK              0xF

#define CQSPI_REG_RD_INSTR                      0x04
#define CQSPI_REG_RD_INSTR_OPCODE_LSB           0
#define CQSPI_REG_RD_INSTR_TYPE_INSTR_LSB       8
#define CQSPI_REG_RD_INSTR_TYPE_ADDR_LSB        12
#define CQSPI_REG_RD_INSTR_TYPE_DATA_LSB        16
#define CQSPI_REG_RD_INSTR_MODE_EN_LSB          20
#define CQSPI_REG_RD_INSTR_DUMMY_LSB            24
#define CQSPI_REG_RD_INSTR_TYPE_INSTR_MASK      0x3
#define CQSPI_REG_RD_INSTR_TYPE_ADDR_MASK       0x3
#define CQSPI_REG_RD_INSTR_TYPE_DATA_MASK       0x3
#define CQSPI_REG_RD_INSTR_DUMMY_MASK           0x1F
#define CQSPI_REG_RD_INSTR_DDR_ENABLE		BIT(10)

#define CQSPI_REG_WR_INSTR                      0x08
#define CQSPI_REG_WR_INSTR_OPCODE_LSB           0
#define CQSPI_REG_WR_INSTR_TYPE_ADDR_LSB	12
#define CQSPI_REG_WR_INSTR_TYPE_DATA_LSB	16

#define CQSPI_REG_DELAY                         0x0C
#define CQSPI_REG_DELAY_TSLCH_LSB               0
#define CQSPI_REG_DELAY_TCHSH_LSB               8
#define CQSPI_REG_DELAY_TSD2D_LSB               16
#define CQSPI_REG_DELAY_TSHSL_LSB               24
#define CQSPI_REG_DELAY_TSLCH_MASK              0xFF
#define CQSPI_REG_DELAY_TCHSH_MASK              0xFF
#define CQSPI_REG_DELAY_TSD2D_MASK              0xFF
#define CQSPI_REG_DELAY_TSHSL_MASK              0xFF

#define CQSPI_REG_RD_DATA_CAPTURE               0x10
#define CQSPI_REG_RD_DATA_CAPTURE_BYPASS        BIT(0)
#define CQSPI_REG_READCAPTURE_DQS_ENABLE        BIT(8)
#define CQSPI_REG_RD_DATA_CAPTURE_DELAY_LSB     1
#define CQSPI_REG_RD_DATA_CAPTURE_DELAY_MASK    0xF

#define CQSPI_REG_SIZE                          0x14
#define CQSPI_REG_SIZE_ADDRESS_LSB              0
#define CQSPI_REG_SIZE_PAGE_LSB                 4
#define CQSPI_REG_SIZE_BLOCK_LSB                16
#define CQSPI_REG_SIZE_ADDRESS_MASK             0xF
#define CQSPI_REG_SIZE_PAGE_MASK                0xFFF
#define CQSPI_REG_SIZE_BLOCK_MASK               0x3F

#define CQSPI_REG_SRAMPARTITION                 0x18
#define CQSPI_REG_INDIRECTTRIGGER               0x1C

#define CQSPI_REG_REMAP                         0x24
#define CQSPI_REG_MODE_BIT                      0x28

#define CQSPI_REG_SDRAMLEVEL                    0x2C
#define CQSPI_REG_SDRAMLEVEL_RD_LSB             0
#define CQSPI_REG_SDRAMLEVEL_WR_LSB             16
#define CQSPI_REG_SDRAMLEVEL_RD_MASK            0xFFFF
#define CQSPI_REG_SDRAMLEVEL_WR_MASK            0xFFFF

#define CQSPI_REG_WR_COMPLETION_CTRL		0x38
#define CQSPI_REG_WR_DISABLE_AUTO_POLL		BIT(14)
#define CQSPI_REG_WRCOMPLETION			0x38
#define CQSPI_REG_WRCOMPLETION_POLLCNT_MASK	0xFF0000
#define CQSPI_REG_WRCOMPLETION_POLLCNY_LSB	16
#define CQSPI_REG_WRCOMPLETION_POLLCNT		3

#define CQSPI_REG_IRQSTATUS                     0x40
#define CQSPI_REG_IRQMASK                       0x44
#define CQSPI_REG_ECO				0x48

#define CQSPI_REG_INDIRECTRD                    0x60
#define CQSPI_REG_INDIRECTRD_START              BIT(0)
#define CQSPI_REG_INDIRECTRD_CANCEL             BIT(1)
#define CQSPI_REG_INDIRECTRD_INPROGRESS         BIT(2)
#define CQSPI_REG_INDIRECTRD_DONE               BIT(5)

#define CQSPI_REG_INDIRECTRDWATERMARK           0x64
#define CQSPI_REG_INDIRECTRDSTARTADDR           0x68
#define CQSPI_REG_INDIRECTRDBYTES               0x6C

#define CQSPI_REG_CMDCTRL                       0x90
#define CQSPI_REG_CMDCTRL_EXECUTE               BIT(0)
#define CQSPI_REG_CMDCTRL_INPROGRESS            BIT(1)
#define CQSPI_REG_CMDCTRL_DUMMY_LSB             7
#define CQSPI_REG_CMDCTRL_WR_BYTES_LSB          12
#define CQSPI_REG_CMDCTRL_WR_EN_LSB             15
#define CQSPI_REG_CMDCTRL_ADD_BYTES_LSB         16
#define CQSPI_REG_CMDCTRL_ADDR_EN_LSB           19
#define CQSPI_REG_CMDCTRL_RD_BYTES_LSB          20
#define CQSPI_REG_CMDCTRL_RD_EN_LSB             23
#define CQSPI_REG_CMDCTRL_OPCODE_LSB            24
#define CQSPI_REG_CMDCTRL_DUMMY_MASK            0x1F
#define CQSPI_REG_CMDCTRL_WR_BYTES_MASK         0x7
#define CQSPI_REG_CMDCTRL_ADD_BYTES_MASK        0x3
#define CQSPI_REG_CMDCTRL_RD_BYTES_MASK         0x7
#define CQSPI_REG_CMDCTRL_OPCODE_MASK           0xFF

#define CQSPI_REG_INDIRECTWR                    0x70
#define CQSPI_REG_INDIRECTWR_START              BIT(0)
#define CQSPI_REG_INDIRECTWR_CANCEL             BIT(1)
#define CQSPI_REG_INDIRECTWR_INPROGRESS         BIT(2)
#define CQSPI_REG_INDIRECTWR_DONE               BIT(5)

#define CQSPI_REG_INDIRECTWRWATERMARK           0x74
#define CQSPI_REG_INDIRECTWRSTARTADDR           0x78
#define CQSPI_REG_INDIRECTWRBYTES               0x7C

#define CQSPI_REG_CMDADDRESS                    0x94
#define CQSPI_REG_CMDREADDATALOWER              0xA0
#define CQSPI_REG_CMDREADDATAUPPER              0xA4
#define CQSPI_REG_CMDWRITEDATALOWER             0xA8
#define CQSPI_REG_CMDWRITEDATAUPPER             0xAC

#define CQSPI_REG_OP_EXT_LOWER                  0xE0
#define CQSPI_REG_OP_EXT_READ_LSB               24
#define CQSPI_REG_OP_EXT_WRITE_LSB              16
#define CQSPI_REG_OP_EXT_STIG_LSB               0

#define CQSPI_REG_PHY_CONFIG                    0xB4
#define CQSPI_REG_PHY_CONFIG_RESYNC_FLD_MASK	0x80000000
#define CQSPI_REG_PHY_CONFIG_RESET_FLD_MASK     0x40000000
#define CQSPI_REG_PHY_CONFIG_TX_DLL_DLY_LSB	16

#define CQSPI_REG_PHY_MASTER_CTRL              0xB8
#define CQSPI_REG_PHY_INITIAL_DLY              0x4
#define CQSPI_REG_DLL_LOWER                    0xBC
#define CQSPI_REG_DLL_LOWER_LPBK_LOCK_MASK     0x8000
#define CQSPI_REG_DLL_LOWER_DLL_LOCK_MASK      0x1

#define CQSPI_REG_DLL_OBSVBLE_UPPER		0xC0
#define CQSPI_REG_DLL_UPPER_RX_FLD_MASK		0x7F

#define CQSPI_REG_EXT_OP_LOWER			0xE0
#define CQSPI_REG_EXT_STIG_OP_MASK		0xFF
#define CQSPI_REG_EXT_READ_OP_MASK		0xFF000000
#define CQSPI_REG_EXT_READ_OP_SHIFT		24
#define CQSPI_REG_EXT_WRITE_OP_MASK		0xFF0000
#define CQSPI_REG_EXT_WRITE_OP_SHIFT		16
#define CQSPI_REG_DMA_SRC_ADDR			0x1000
#define CQSPI_REG_DMA_DST_ADDR			0x1800
#define CQSPI_REG_DMA_DST_SIZE			0x1804
#define CQSPI_REG_DMA_DST_STS			0x1808
#define CQSPI_REG_DMA_DST_CTRL			0x180C
#define CQSPI_REG_DMA_DST_CTRL_VAL		0xF43FFA00

#define CQSPI_DMA_DST_ADDR_REG                  0x1800
#define CQSPI_DMA_DST_SIZE_REG                  0x1804
#define CQSPI_DMA_DST_STS_REG                   0x1808
#define CQSPI_DMA_DST_CTRL_REG                  0x180C
#define CQSPI_DMA_DST_I_STS_REG                 0x1814
#define CQSPI_DMA_DST_I_ENBL_REG                0x1818
#define CQSPI_DMA_DST_I_DISBL_REG               0x181C
#define CQSPI_DMA_DST_CTRL2_REG                 0x1824
#define CQSPI_DMA_DST_ADDR_MSB_REG              0x1828

#define CQSPI_DMA_SRC_RD_ADDR_REG               0x1000

#define CQSPI_REG_DMA_PERIPH_CFG                0x20
#define CQSPI_REG_INDIR_TRIG_ADDR_RANGE         0x80
#define CQSPI_DFLT_INDIR_TRIG_ADDR_RANGE        6
#define CQSPI_DFLT_DMA_PERIPH_CFG               0x602
#define CQSPI_DFLT_DST_CTRL_REG_VAL             0xF43FFA00

#define CQSPI_DMA_DST_I_STS_DONE                BIT(1)
#define CQSPI_DMA_TIMEOUT                       10000000

#define CQSPI_REG_IS_IDLE(base)				\
	((readl((base) + CQSPI_REG_CONFIG) >>		\
	CQSPI_REG_CONFIG_IDLE_LSB) & 0x1)

#define CQSPI_GET_RD_SRAM_LEVEL(reg_base)		\
	(((readl((reg_base) + CQSPI_REG_SDRAMLEVEL)) >>	\
	CQSPI_REG_SDRAMLEVEL_RD_LSB) & CQSPI_REG_SDRAMLEVEL_RD_MASK)

#define CQSPI_GET_WR_SRAM_LEVEL(reg_base)		\
	(((readl((reg_base) + CQSPI_REG_SDRAMLEVEL)) >>	\
	CQSPI_REG_SDRAMLEVEL_WR_LSB) & CQSPI_REG_SDRAMLEVEL_WR_MASK)

/* Interrupt status bits */
#define CQSPI_REG_IRQ_UNDERFLOW                 BIT(1)
#define CQSPI_REG_IRQ_IND_COMP                  BIT(2)
#define CQSPI_REG_IRQ_WATERMARK                 BIT(6)

#define CQSPI_IRQ_MASK_WR			(CQSPI_REG_IRQ_IND_COMP        | \
						CQSPI_REG_IRQ_WATERMARK        | \
						CQSPI_REG_IRQ_UNDERFLOW)

#define CQSPI_IRQ_STATUS_MASK			GENMASK(16, 0)

struct cadence_spi_plat {
	unsigned int	max_hz;
	void		*regbase;
	void		*ahbbase;
	bool		is_decoded_cs;
	u32		fifo_depth;
	u32		fifo_width;
	u32		trigger_address;
	fdt_addr_t	ahbsize;
	bool		use_dac_mode;
	int		read_delay;

	/* Flash parameters */
	u32		page_size;
	u32		block_size;
	u32		tshsl_ns;
	u32		tsd2d_ns;
	u32		tchsh_ns;
	u32		tslch_ns;

	bool            is_dma;
};

struct cadence_spi_priv {
	unsigned int	ref_clk_hz;
	unsigned int	max_hz;
	void		*regbase;
	void		*ahbbase;
	unsigned int	fifo_depth;
	unsigned int	fifo_width;
	unsigned int	trigger_address;
	fdt_addr_t      ahbsize;
	size_t		cmd_len;
	u8		cmd_buf[32];
	size_t		data_len;

	int		qspi_is_init;
	unsigned int    cs;
	unsigned int	qspi_calibrated_hz;
	unsigned int	qspi_calibrated_cs;
	unsigned int	previous_hz;
	u32		wr_delay;
	int		read_delay;

	struct reset_ctl_bulk *resets;
	u32		page_size;
	u32		block_size;
	u32		tshsl_ns;
	u32		tsd2d_ns;
	u32		tchsh_ns;
	u32		tslch_ns;
	u8              device_id[CQSPI_READ_ID_LEN];
	u8              edge_mode;
	u8              dll_mode;
	bool		extra_dummy;
	bool		ddr_init;
	bool		is_decoded_cs;
	bool		use_dac_mode;
	bool		is_dma;

	/* Transaction protocol parameters. */
	u8		inst_width;
	u8		addr_width;
	u8		data_width;
	bool		dtr;
};

/* Functions call declaration */
void cadence_qspi_apb_controller_init(struct cadence_spi_priv *priv);
void cadence_qspi_apb_controller_enable(void *reg_base_addr);
void cadence_qspi_apb_controller_disable(void *reg_base_addr);
void cadence_qspi_apb_dac_mode_enable(void *reg_base);

int cadence_qspi_apb_command_read_setup(struct cadence_spi_priv *priv,
					const struct spi_mem_op *op);
int cadence_qspi_apb_command_read(struct cadence_spi_priv *priv,
				  const struct spi_mem_op *op);
int cadence_qspi_apb_command_write_setup(struct cadence_spi_priv *priv,
					 const struct spi_mem_op *op);
int cadence_qspi_apb_command_write(struct cadence_spi_priv *priv,
				   const struct spi_mem_op *op);

int cadence_qspi_apb_read_setup(struct cadence_spi_priv *priv,
				const struct spi_mem_op *op);
int cadence_qspi_apb_read_execute(struct cadence_spi_priv *priv,
				  const struct spi_mem_op *op);
int cadence_qspi_apb_write_setup(struct cadence_spi_priv *priv,
				 const struct spi_mem_op *op);
int cadence_qspi_apb_write_execute(struct cadence_spi_priv *priv,
				   const struct spi_mem_op *op);

void cadence_qspi_apb_chipselect(void *reg_base,
	unsigned int chip_select, unsigned int decoder_enable);
void cadence_qspi_apb_set_clk_mode(void *reg_base, uint mode);
void cadence_qspi_apb_config_baudrate_div(void *reg_base,
	unsigned int ref_clk_hz, unsigned int sclk_hz);
void cadence_qspi_apb_delay(void *reg_base,
	unsigned int ref_clk, unsigned int sclk_hz,
	unsigned int tshsl_ns, unsigned int tsd2d_ns,
	unsigned int tchsh_ns, unsigned int tslch_ns);
void cadence_qspi_apb_enter_xip(void *reg_base, char xip_dummy);
void cadence_qspi_apb_readdata_capture(void *reg_base,
	unsigned int bypass, unsigned int delay);
unsigned int cm_get_qspi_controller_clk_hz(void);
int cadence_qspi_apb_dma_read(struct cadence_spi_priv *priv,
			      const struct spi_mem_op *op);
int cadence_qspi_apb_wait_for_dma_cmplt(struct cadence_spi_priv *priv);
int cadence_qspi_apb_exec_flash_cmd(void *reg_base, unsigned int reg);
int cadence_qspi_versal_flash_reset(struct udevice *dev);
int cadence_qspi_versal_set_dll_mode(struct udevice *dev);
void cadence_qspi_apb_enable_linear_mode(bool enable);
int cadence_spi_versal_ctrl_reset(struct cadence_spi_priv *priv);
int cadence_qspi_setup_opcode_ext(struct cadence_spi_priv *priv,
				  const struct spi_mem_op *op,
				  unsigned int shift);
#endif /* __CADENCE_QSPI_H__ */
