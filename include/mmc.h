/*
 * Copyright 2008,2010 Freescale Semiconductor, Inc
 * Andy Fleming
 *
 * Based (loosely) on the Linux code
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _MMC_H_
#define _MMC_H_

#include <linux/list.h>
#include <linux/sizes.h>
#include <linux/compiler.h>
#include <part.h>

/* SD/MMC version bits; 8 flags, 8 major, 8 minor, 8 change */
#define SD_VERSION_SD	(1U << 31)
#define MMC_VERSION_MMC	(1U << 30)

#define MAKE_SDMMC_VERSION(a, b, c)	\
	((((u32)(a)) << 16) | ((u32)(b) << 8) | (u32)(c))
#define MAKE_SD_VERSION(a, b, c)	\
	(SD_VERSION_SD | MAKE_SDMMC_VERSION(a, b, c))
#define MAKE_MMC_VERSION(a, b, c)	\
	(MMC_VERSION_MMC | MAKE_SDMMC_VERSION(a, b, c))

#define EXTRACT_SDMMC_MAJOR_VERSION(x)	\
	(((u32)(x) >> 16) & 0xff)
#define EXTRACT_SDMMC_MINOR_VERSION(x)	\
	(((u32)(x) >> 8) & 0xff)
#define EXTRACT_SDMMC_CHANGE_VERSION(x)	\
	((u32)(x) & 0xff)

#define SD_VERSION_3		MAKE_SD_VERSION(3, 0, 0)
#define SD_VERSION_2		MAKE_SD_VERSION(2, 0, 0)
#define SD_VERSION_1_0		MAKE_SD_VERSION(1, 0, 0)
#define SD_VERSION_1_10		MAKE_SD_VERSION(1, 10, 0)

#define MMC_VERSION_UNKNOWN	MAKE_MMC_VERSION(0, 0, 0)
#define MMC_VERSION_1_2		MAKE_MMC_VERSION(1, 2, 0)
#define MMC_VERSION_1_4		MAKE_MMC_VERSION(1, 4, 0)
#define MMC_VERSION_2_2		MAKE_MMC_VERSION(2, 2, 0)
#define MMC_VERSION_3		MAKE_MMC_VERSION(3, 0, 0)
#define MMC_VERSION_4		MAKE_MMC_VERSION(4, 0, 0)
#define MMC_VERSION_4_1		MAKE_MMC_VERSION(4, 1, 0)
#define MMC_VERSION_4_2		MAKE_MMC_VERSION(4, 2, 0)
#define MMC_VERSION_4_3		MAKE_MMC_VERSION(4, 3, 0)
#define MMC_VERSION_4_41	MAKE_MMC_VERSION(4, 4, 1)
#define MMC_VERSION_4_5		MAKE_MMC_VERSION(4, 5, 0)
#define MMC_VERSION_5_0		MAKE_MMC_VERSION(5, 0, 0)
#define MMC_VERSION_5_1		MAKE_MMC_VERSION(5, 1, 0)

#define MMC_MODE_HS		(1 << 0)
#define MMC_MODE_HS_52MHz	(1 << 1)
#define MMC_MODE_4BIT		(1 << 2)
#define MMC_MODE_8BIT		(1 << 3)
#define MMC_MODE_SPI		(1 << 4)
#define MMC_MODE_DDR_52MHz	(1 << 5)
#define MMC_MODE_UHS_SDR12	(1 << 6)
#define MMC_MODE_UHS_SDR25	(1 << 7)
#define MMC_MODE_UHS_SDR50	(1 << 8)
#define MMC_MODE_UHS_SDR104	(1 << 9)
#define MMC_MODE_UHS_DDR50	(1 << 10)
#define MMC_MODE_NEEDS_TUNING	(1 << 11)
#define MMC_MODE_HS200		(1 << 12)

#define MMC_MODE_UHS	(MMC_MODE_UHS_SDR12 | MMC_MODE_UHS_SDR25 |	\
			 MMC_MODE_UHS_SDR50 | MMC_MODE_UHS_SDR104 |	\
			 MMC_MODE_UHS_DDR50)
#define SD_DATA_4BIT	0x00040000

#define IS_SD(x)	((x)->version & SD_VERSION_SD)
#define IS_MMC(x)	((x)->version & MMC_VERSION_MMC)

#define MMC_DATA_READ		1
#define MMC_DATA_WRITE		2

#define MMC_CMD_GO_IDLE_STATE		0
#define MMC_CMD_SEND_OP_COND		1
#define MMC_CMD_ALL_SEND_CID		2
#define MMC_CMD_SET_RELATIVE_ADDR	3
#define MMC_CMD_SET_DSR			4
#define MMC_CMD_SWITCH			6
#define MMC_CMD_SELECT_CARD		7
#define MMC_CMD_SEND_EXT_CSD		8
#define MMC_CMD_SEND_CSD		9
#define MMC_CMD_SEND_CID		10
#define MMC_CMD_STOP_TRANSMISSION	12
#define MMC_CMD_SEND_STATUS		13
#define MMC_CMD_SET_BLOCKLEN		16
#define MMC_CMD_READ_SINGLE_BLOCK	17
#define MMC_CMD_READ_MULTIPLE_BLOCK	18
#define MMC_CMD_SEND_TUNING_BLOCK	19
#define MMC_CMD_SEND_TUNING_BLOCK_HS200	21
#define MMC_CMD_SET_BLOCK_COUNT         23
#define MMC_CMD_WRITE_SINGLE_BLOCK	24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK	25
#define MMC_CMD_ERASE_GROUP_START	35
#define MMC_CMD_ERASE_GROUP_END		36
#define MMC_CMD_ERASE			38
#define MMC_CMD_APP_CMD			55
#define MMC_CMD_SPI_READ_OCR		58
#define MMC_CMD_SPI_CRC_ON_OFF		59
#define MMC_CMD_RES_MAN			62

#define MMC_CMD62_ARG1			0xefac62ec
#define MMC_CMD62_ARG2			0xcbaea7


#define SD_CMD_SEND_RELATIVE_ADDR	3
#define SD_CMD_SWITCH_FUNC		6
#define SD_CMD_SEND_IF_COND		8
#define SD_CMD_SWITCH_UHS18V		11

#define SD_CMD_APP_SET_BUS_WIDTH	6
#define SD_CMD_APP_SD_STATUS		13
#define SD_CMD_ERASE_WR_BLK_START	32
#define SD_CMD_ERASE_WR_BLK_END		33
#define SD_CMD_APP_SEND_OP_COND		41
#define SD_CMD_APP_SEND_SCR		51

/* SCR definitions in different words */
#define SD_HIGHSPEED_BUSY	0x00020000
#define SD_HIGHSPEED_SUPPORTED	0x00020000
#define SD_UHS_SPEED_SDR104	0x00080000
#define SD_UHS_SPEED_SDR50	0x00040000
#define SD_UHS_SPEED_DDR50	0x00100000
#define SD_UHS_SPEED_SDR25	0x00020000

#define OCR_BUSY		0x80000000
#define OCR_HCS			0x40000000
#define OCR_VOLTAGE_MASK	0x007FFF80
#define OCR_ACCESS_MODE		0x60000000

#define SD_OCR_S18R		(1 << 24)

#define MMC_ERASE_ARG		0x00000000
#define MMC_SECURE_ERASE_ARG	0x80000000
#define MMC_TRIM_ARG		0x00000001
#define MMC_DISCARD_ARG		0x00000003
#define MMC_SECURE_TRIM1_ARG	0x80000001
#define MMC_SECURE_TRIM2_ARG	0x80008000

#define MMC_STATUS_MASK		(~0x0206BF7F)
#define MMC_STATUS_SWITCH_ERROR	(1 << 7)
#define MMC_STATUS_RDY_FOR_DATA (1 << 8)
#define MMC_STATUS_CURR_STATE	(0xf << 9)
#define MMC_STATUS_ERROR	(1 << 19)

#define MMC_STATE_PRG		(7 << 9)

#define MMC_VDD_165_195		0x00000080	/* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_20_21		0x00000100	/* VDD voltage 2.0 ~ 2.1 */
#define MMC_VDD_21_22		0x00000200	/* VDD voltage 2.1 ~ 2.2 */
#define MMC_VDD_22_23		0x00000400	/* VDD voltage 2.2 ~ 2.3 */
#define MMC_VDD_23_24		0x00000800	/* VDD voltage 2.3 ~ 2.4 */
#define MMC_VDD_24_25		0x00001000	/* VDD voltage 2.4 ~ 2.5 */
#define MMC_VDD_25_26		0x00002000	/* VDD voltage 2.5 ~ 2.6 */
#define MMC_VDD_26_27		0x00004000	/* VDD voltage 2.6 ~ 2.7 */
#define MMC_VDD_27_28		0x00008000	/* VDD voltage 2.7 ~ 2.8 */
#define MMC_VDD_28_29		0x00010000	/* VDD voltage 2.8 ~ 2.9 */
#define MMC_VDD_29_30		0x00020000	/* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31		0x00040000	/* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32		0x00080000	/* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33		0x00100000	/* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34		0x00200000	/* VDD voltage 3.3 ~ 3.4 */
#define MMC_VDD_34_35		0x00400000	/* VDD voltage 3.4 ~ 3.5 */
#define MMC_VDD_35_36		0x00800000	/* VDD voltage 3.5 ~ 3.6 */

#define MMC_SWITCH_MODE_CMD_SET		0x00 /* Change the command set */
#define MMC_SWITCH_MODE_SET_BITS	0x01 /* Set bits in EXT_CSD byte
						addressed by index which are
						1 in value field */
#define MMC_SWITCH_MODE_CLEAR_BITS	0x02 /* Clear bits in EXT_CSD byte
						addressed by index, which are
						1 in value field */
#define MMC_SWITCH_MODE_WRITE_BYTE	0x03 /* Set target byte to value */

#define SD_SWITCH_CHECK		0
#define SD_SWITCH_SWITCH	1

/*
 * EXT_CSD fields
 */
#define EXT_CSD_ENH_START_ADDR		136	/* R/W */
#define EXT_CSD_ENH_SIZE_MULT		140	/* R/W */
#define EXT_CSD_GP_SIZE_MULT		143	/* R/W */
#define EXT_CSD_PARTITION_SETTING	155	/* R/W */
#define EXT_CSD_PARTITIONS_ATTRIBUTE	156	/* R/W */
#define EXT_CSD_MAX_ENH_SIZE_MULT	157	/* R */
#define EXT_CSD_PARTITIONING_SUPPORT	160	/* RO */
#define EXT_CSD_RST_N_FUNCTION		162	/* R/W */
#define EXT_CSD_BKOPS_EN		163	/* R/W & R/W/E */
#define EXT_CSD_WR_REL_PARAM		166	/* R */
#define EXT_CSD_WR_REL_SET		167	/* R/W */
#define EXT_CSD_RPMB_MULT		168	/* RO */
#define EXT_CSD_ERASE_GROUP_DEF		175	/* R/W */
#define EXT_CSD_BOOT_BUS_WIDTH		177
#define EXT_CSD_PART_CONF		179	/* R/W */
#define EXT_CSD_BUS_WIDTH		183	/* R/W */
#define EXT_CSD_HS_TIMING		185	/* R/W */
#define EXT_CSD_REV			192	/* RO */
#define EXT_CSD_CARD_TYPE		196	/* RO */
#define EXT_CSD_SEC_CNT			212	/* RO, 4 bytes */
#define EXT_CSD_HC_WP_GRP_SIZE		221	/* RO */
#define EXT_CSD_HC_ERASE_GRP_SIZE	224	/* RO */
#define EXT_CSD_BOOT_MULT		226	/* RO */
#define EXT_CSD_BKOPS_SUPPORT		502	/* RO */

/*
 * EXT_CSD field definitions
 */

#define EXT_CSD_CMD_SET_NORMAL		(1 << 0)
#define EXT_CSD_CMD_SET_SECURE		(1 << 1)
#define EXT_CSD_CMD_SET_CPSECURE	(1 << 2)

#define EXT_CSD_CARD_TYPE_26	(1 << 0)	/* Card can run at 26MHz */
#define EXT_CSD_CARD_TYPE_52	(1 << 1)	/* Card can run at 52MHz */
#define EXT_CSD_CARD_TYPE_DDR_1_8V	(1 << 2)
#define EXT_CSD_CARD_TYPE_DDR_1_2V	(1 << 3)
#define EXT_CSD_CARD_TYPE_DDR_52	(EXT_CSD_CARD_TYPE_DDR_1_8V \
					| EXT_CSD_CARD_TYPE_DDR_1_2V)
#define EXT_CSD_CARD_TYPE_HS200_1_8V	(1 << 4)
#define EXT_CSD_CARD_TYPE_HS200_1_2V	(1 << 5)
#define EXT_CSD_CARD_TYPE_HS200		(EXT_CSD_CARD_TYPE_HS200_1_8V \
					| EXT_CSD_CARD_TYPE_HS200_1_2V)

#define EXT_CSD_CARD_TYPE_MASK		(EXT_CSD_CARD_TYPE_26 | \
					EXT_CSD_CARD_TYPE_52 | \
					EXT_CSD_CARD_TYPE_DDR_52 | \
					EXT_CSD_CARD_TYPE_HS200)

#define EXT_CSD_HS_TIMING_HIGH_SPEED	1
#define EXT_CSD_HS_TIMING_HS200		2

#define EXT_CSD_BUS_WIDTH_1	0	/* Card is in 1 bit mode */
#define EXT_CSD_BUS_WIDTH_4	1	/* Card is in 4 bit mode */
#define EXT_CSD_BUS_WIDTH_8	2	/* Card is in 8 bit mode */
#define EXT_CSD_DDR_BUS_WIDTH_4	5	/* Card is in 4 bit DDR mode */
#define EXT_CSD_DDR_BUS_WIDTH_8	6	/* Card is in 8 bit DDR mode */

#define EXT_CSD_BOOT_ACK_ENABLE			(1 << 6)
#define EXT_CSD_BOOT_PARTITION_ENABLE		(1 << 3)
#define EXT_CSD_PARTITION_ACCESS_ENABLE		(1 << 0)
#define EXT_CSD_PARTITION_ACCESS_DISABLE	(0 << 0)

#define EXT_CSD_BOOT_ACK(x)		(x << 6)
#define EXT_CSD_BOOT_PART_NUM(x)	(x << 3)
#define EXT_CSD_PARTITION_ACCESS(x)	(x << 0)

#define EXT_CSD_BOOT_BUS_WIDTH_MODE(x)	(x << 3)
#define EXT_CSD_BOOT_BUS_WIDTH_RESET(x)	(x << 2)
#define EXT_CSD_BOOT_BUS_WIDTH_WIDTH(x)	(x)

#define EXT_CSD_PARTITION_SETTING_COMPLETED	(1 << 0)

#define EXT_CSD_ENH_USR		(1 << 0)	/* user data area is enhanced */
#define EXT_CSD_ENH_GP(x)	(1 << ((x)+1))	/* GP part (x+1) is enhanced */

#define EXT_CSD_HS_CTRL_REL	(1 << 0)	/* host controlled WR_REL_SET */

#define EXT_CSD_WR_DATA_REL_USR		(1 << 0)	/* user data area WR_REL */
#define EXT_CSD_WR_DATA_REL_GP(x)	(1 << ((x)+1))	/* GP part (x+1) WR_REL */

#define R1_ILLEGAL_COMMAND		(1 << 22)
#define R1_APP_CMD			(1 << 5)

#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_136	(1 << 1)		/* 136 bit response */
#define MMC_RSP_CRC	(1 << 2)		/* expect valid crc */
#define MMC_RSP_BUSY	(1 << 3)		/* card may send busy */
#define MMC_RSP_OPCODE	(1 << 4)		/* response contains opcode */

#define MMC_RSP_NONE	(0)
#define MMC_RSP_R1	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R1b	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE| \
			MMC_RSP_BUSY)
#define MMC_RSP_R2	(MMC_RSP_PRESENT|MMC_RSP_136|MMC_RSP_CRC)
#define MMC_RSP_R3	(MMC_RSP_PRESENT)
#define MMC_RSP_R4	(MMC_RSP_PRESENT)
#define MMC_RSP_R5	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R6	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R7	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)

#define MMCPART_NOAVAILABLE	(0xff)
#define PART_ACCESS_MASK	(0x7)
#define PART_ACCESS_BOOT0	(0x2)
#define PART_SUPPORT		(0x1)
#define ENHNCD_SUPPORT		(0x2)
#define PART_ENH_ATTRIB		(0x1f)

/* Maximum block size for MMC */
#define MMC_MAX_BLOCK_LEN	512

/* The number of MMC physical partitions.  These consist of:
 * boot partitions (2), general purpose partitions (4) in MMC v4.4.
 */
#define MMC_NUM_BOOT_PARTITION	2
#define MMC_PART_RPMB           3       /* RPMB partition number */

#define MMC_TIMING_UHS_SDR12	0
#define MMC_TIMING_UHS_SDR25	1
#define MMC_TIMING_UHS_SDR50	2
#define MMC_TIMING_UHS_SDR104	3
#define MMC_TIMING_UHS_DDR50	4
#define MMC_TIMING_HS200	5
#define MMC_TIMING_HS		1

/* Driver model support */

/**
 * struct mmc_uclass_priv - Holds information about a device used by the uclass
 */
struct mmc_uclass_priv {
	struct mmc *mmc;
};

/**
 * mmc_get_mmc_dev() - get the MMC struct pointer for a device
 *
 * Provided that the device is already probed and ready for use, this value
 * will be available.
 *
 * @dev:	Device
 * @return associated mmc struct pointer if available, else NULL
 */
struct mmc *mmc_get_mmc_dev(struct udevice *dev);

/* End of driver model support */

struct mmc_cid {
	unsigned long psn;
	unsigned short oid;
	unsigned char mid;
	unsigned char prv;
	unsigned char mdt;
	char pnm[7];
};

struct mmc_cmd {
	ushort cmdidx;
	uint resp_type;
	uint cmdarg;
	uint response[4];
};

struct mmc_data {
	union {
		char *dest;
		const char *src; /* src buffers don't get written to */
	};
	uint flags;
	uint blocks;
	uint blocksize;
};

/* forward decl. */
struct mmc;

#ifdef CONFIG_DM_MMC_OPS
struct dm_mmc_ops {
	/**
	 * send_cmd() - Send a command to the MMC device
	 *
	 * @dev:	Device to receive the command
	 * @cmd:	Command to send
	 * @data:	Additional data to send/receive
	 * @return 0 if OK, -ve on error
	 */
	int (*send_cmd)(struct udevice *dev, struct mmc_cmd *cmd,
			struct mmc_data *data);

	/**
	 * set_ios() - Set the I/O speed/width for an MMC device
	 *
	 * @dev:	Device to update
	 * @return 0 if OK, -ve on error
	 */
	int (*set_ios)(struct udevice *dev);

	/**
	 * get_cd() - See whether a card is present
	 *
	 * @dev:	Device to check
	 * @return 0 if not present, 1 if present, -ve on error
	 */
	int (*get_cd)(struct udevice *dev);

	/**
	 * get_wp() - See whether a card has write-protect enabled
	 *
	 * @dev:	Device to check
	 * @return 0 if write-enabled, 1 if write-protected, -ve on error
	 */
	int (*get_wp)(struct udevice *dev);

	/**
	* set_volatge() - Switches host for new voltage
	* @dev:>-------Device to check
	*
	* @return 0 on success otherwise error value
	*/
	int (*set_voltage)(struct udevice *dev);

	/**
	 * set_uhs() - Sets the controller for specific uhs mode
	 * @dev:>-------Device to check
	 *
	 * @return 0 after setting uhs mode
	 */
	int (*set_uhs)(struct udevice *dev);

	/**
	* execute_tuning() - Executes the required tuning sequence
	* @dev:>-------Device to check
	* @opcode:>----Command to be sent for tuning
	*
	* @return 0 on success otherwise error value
	*/
	int (*execute_tuning)(struct udevice *dev, u8 opcode);
};

#define mmc_get_ops(dev)        ((struct dm_mmc_ops *)(dev)->driver->ops)

int dm_mmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
		    struct mmc_data *data);
int dm_mmc_set_ios(struct udevice *dev);
int dm_mmc_get_cd(struct udevice *dev);
int dm_mmc_get_wp(struct udevice *dev);
int dm_mmc_set_voltage(struct udevice *dev);
int dm_mmc_set_uhs(struct udevice *dev);
int dm_mmc_execute_tuning(struct udevice *dev);

/* Transition functions for compatibility */
int mmc_set_ios(struct mmc *mmc);
int mmc_getcd(struct mmc *mmc);
int mmc_getwp(struct mmc *mmc);
int mmc_set_voltage(struct mmc *mmc);
int mmc_switch_uhs(struct mmc *mmc);
int mmc_execute_tuning(struct mmc *mmc);
#else
struct mmc_ops {
	int (*send_cmd)(struct mmc *mmc,
			struct mmc_cmd *cmd, struct mmc_data *data);
	void (*set_ios)(struct mmc *mmc);
	int (*init)(struct mmc *mmc);
	int (*getcd)(struct mmc *mmc);
	int (*getwp)(struct mmc *mmc);
	int (*set_voltage)(struct mmc *mmc);
	int (*set_uhs)(struct mmc *mmc);
	int (*execute_tuning)(struct mmc *mmc, u8 opcode);
};
#endif

struct mmc_config {
	const char *name;
#ifndef CONFIG_DM_MMC_OPS
	const struct mmc_ops *ops;
#endif
	uint host_caps;
	uint voltages;
	uint f_min;
	uint f_max;
	uint b_max;
	unsigned char part_type;
};

struct sd_ssr {
	unsigned int au;		/* In sectors */
	unsigned int erase_timeout;	/* In milliseconds */
	unsigned int erase_offset;	/* In milliseconds */
};

/*
 * With CONFIG_DM_MMC enabled, struct mmc can be accessed from the MMC device
 * with mmc_get_mmc_dev().
 *
 * TODO struct mmc should be in mmc_private but it's hard to fix right now
 */
struct mmc {
#ifndef CONFIG_BLK
	struct list_head link;
#endif
	const struct mmc_config *cfg;	/* provided configuration */
	uint version;
	void *priv;
	uint has_init;
	int high_capacity;
	uint bus_width;
	uint clock;
	uint card_caps;
	uint ocr;
	uint dsr;
	uint dsr_imp;
	uint scr[2];
	uint csd[4];
	uint cid[4];
	ushort rca;
	u8 part_support;
	u8 part_attr;
	u8 wr_rel_set;
	char part_config;
	uint tran_speed;
	uint read_bl_len;
	uint write_bl_len;
	uint erase_grp_size;	/* in 512-byte sectors */
	uint hc_wp_grp_size;	/* in 512-byte sectors */
	struct sd_ssr	ssr;	/* SD status register */
	u64 capacity;
	u64 capacity_user;
	u64 capacity_boot;
	u64 capacity_rpmb;
	u64 capacity_gp[4];
	u64 enh_user_start;
	u64 enh_user_size;
#ifndef CONFIG_BLK
	struct blk_desc block_dev;
#endif
	char op_cond_pending;	/* 1 if we are waiting on an op_cond command */
	char init_in_progress;	/* 1 if we have done mmc_start_init() */
	char preinit;		/* start init as early as possible */
	int ddr_mode;
#ifdef CONFIG_DM_MMC
	struct udevice *dev;	/* Device for this MMC controller */
#endif
	u8 is_uhs;
	u8 uhsmode;
	u8 forcehs;
};

struct mmc_hwpart_conf {
	struct {
		uint enh_start;	/* in 512-byte sectors */
		uint enh_size;	/* in 512-byte sectors, if 0 no enh area */
		unsigned wr_rel_change : 1;
		unsigned wr_rel_set : 1;
	} user;
	struct {
		uint size;	/* in 512-byte sectors */
		unsigned enhanced : 1;
		unsigned wr_rel_change : 1;
		unsigned wr_rel_set : 1;
	} gp_part[4];
};

enum mmc_hwpart_conf_mode {
	MMC_HWPART_CONF_CHECK,
	MMC_HWPART_CONF_SET,
	MMC_HWPART_CONF_COMPLETE,
};

struct mmc *mmc_create(const struct mmc_config *cfg, void *priv);

/**
 * mmc_bind() - Set up a new MMC device ready for probing
 *
 * A child block device is bound with the IF_TYPE_MMC interface type. This
 * allows the device to be used with CONFIG_BLK
 *
 * @dev:	MMC device to set up
 * @mmc:	MMC struct
 * @cfg:	MMC configuration
 * @return 0 if OK, -ve on error
 */
int mmc_bind(struct udevice *dev, struct mmc *mmc,
	     const struct mmc_config *cfg);
void mmc_destroy(struct mmc *mmc);

/**
 * mmc_unbind() - Unbind a MMC device's child block device
 *
 * @dev:	MMC device
 * @return 0 if OK, -ve on error
 */
int mmc_unbind(struct udevice *dev);
int mmc_initialize(bd_t *bis);
int mmc_init(struct mmc *mmc);
int mmc_read(struct mmc *mmc, u64 src, uchar *dst, int size);
void mmc_set_clock(struct mmc *mmc, uint clock);
struct mmc *find_mmc_device(int dev_num);
int mmc_set_dev(int dev_num);
void print_mmc_devices(char separator);

/**
 * get_mmc_num() - get the total MMC device number
 *
 * @return 0 if there is no MMC device, else the number of devices
 */
int get_mmc_num(void);
int mmc_switch_part(struct mmc *mmc, unsigned int part_num);
int mmc_hwpart_config(struct mmc *mmc, const struct mmc_hwpart_conf *conf,
		      enum mmc_hwpart_conf_mode mode);

#ifndef CONFIG_DM_MMC_OPS
int mmc_getcd(struct mmc *mmc);
int board_mmc_getcd(struct mmc *mmc);
int mmc_getwp(struct mmc *mmc);
int board_mmc_getwp(struct mmc *mmc);
#endif

int mmc_set_dsr(struct mmc *mmc, u16 val);
/* Function to change the size of boot partition and rpmb partitions */
int mmc_boot_partition_size_change(struct mmc *mmc, unsigned long bootsize,
					unsigned long rpmbsize);
/* Function to modify the PARTITION_CONFIG field of EXT_CSD */
int mmc_set_part_conf(struct mmc *mmc, u8 ack, u8 part_num, u8 access);
/* Function to modify the BOOT_BUS_WIDTH field of EXT_CSD */
int mmc_set_boot_bus_width(struct mmc *mmc, u8 width, u8 reset, u8 mode);
/* Function to modify the RST_n_FUNCTION field of EXT_CSD */
int mmc_set_rst_n_function(struct mmc *mmc, u8 enable);
/* Functions to read / write the RPMB partition */
int mmc_rpmb_set_key(struct mmc *mmc, void *key);
int mmc_rpmb_get_counter(struct mmc *mmc, unsigned long *counter);
int mmc_rpmb_read(struct mmc *mmc, void *addr, unsigned short blk,
		  unsigned short cnt, unsigned char *key);
int mmc_rpmb_write(struct mmc *mmc, void *addr, unsigned short blk,
		   unsigned short cnt, unsigned char *key);
#ifdef CONFIG_CMD_BKOPS_ENABLE
int mmc_set_bkops_enable(struct mmc *mmc);
#endif

/**
 * Start device initialization and return immediately; it does not block on
 * polling OCR (operation condition register) status.  Then you should call
 * mmc_init, which would block on polling OCR status and complete the device
 * initializatin.
 *
 * @param mmc	Pointer to a MMC device struct
 * @return 0 on success, IN_PROGRESS on waiting for OCR status, <0 on error.
 */
int mmc_start_init(struct mmc *mmc);

/**
 * Set preinit flag of mmc device.
 *
 * This will cause the device to be pre-inited during mmc_initialize(),
 * which may save boot time if the device is not accessed until later.
 * Some eMMC devices take 200-300ms to init, but unfortunately they
 * must be sent a series of commands to even get them to start preparing
 * for operation.
 *
 * @param mmc		Pointer to a MMC device struct
 * @param preinit	preinit flag value
 */
void mmc_set_preinit(struct mmc *mmc, int preinit);

#ifdef CONFIG_MMC_SPI
#define mmc_host_is_spi(mmc)	((mmc)->cfg->host_caps & MMC_MODE_SPI)
#else
#define mmc_host_is_spi(mmc)	0
#endif
struct mmc *mmc_spi_init(uint bus, uint cs, uint speed, uint mode);

void board_mmc_power_init(void);
int board_mmc_init(bd_t *bis);
int cpu_mmc_init(bd_t *bis);
int mmc_get_env_addr(struct mmc *mmc, int copy, u32 *env_addr);
int mmc_get_env_dev(void);

struct pci_device_id;

/**
 * pci_mmc_init() - set up PCI MMC devices
 *
 * This finds all the matching PCI IDs and sets them up as MMC devices.
 *
 * @name:		Name to use for devices
 * @mmc_supported:	PCI IDs to search for, terminated by {0, 0}
 */
int pci_mmc_init(const char *name, struct pci_device_id *mmc_supported);

/* Set block count limit because of 16 bit register limit on some hardware*/
#ifndef CONFIG_SYS_MMC_MAX_BLK_COUNT
#define CONFIG_SYS_MMC_MAX_BLK_COUNT 65535
#endif

/**
 * mmc_get_blk_desc() - Get the block descriptor for an MMC device
 *
 * @mmc:	MMC device
 * @return block device if found, else NULL
 */
struct blk_desc *mmc_get_blk_desc(struct mmc *mmc);

#endif /* _MMC_H_ */
