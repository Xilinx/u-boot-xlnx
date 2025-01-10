/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Ryder Lee <ryder.lee@mediatek.com>
 */

#ifndef __DRV_CLK_MTK_H
#define __DRV_CLK_MTK_H

#include <linux/bitops.h>
#define CLK_XTAL			0
#define MHZ				(1000 * 1000)

/* flags in struct mtk_clk_tree */

/* clk id == 0 doesn't mean it's xtal clk
 * This doesn't apply when CLK_PARENT_MIXED is defined.
 * With CLK_PARENT_MIXED declare CLK_PARENT_XTAL for the
 * relevant parent.
 */
#define CLK_BYPASS_XTAL			BIT(0)

#define HAVE_RST_BAR			BIT(0)
#define CLK_DOMAIN_SCPSYS		BIT(0)
#define CLK_MUX_SETCLR_UPD		BIT(1)

#define CLK_GATE_SETCLR			BIT(0)
#define CLK_GATE_SETCLR_INV		BIT(1)
#define CLK_GATE_NO_SETCLR		BIT(2)
#define CLK_GATE_NO_SETCLR_INV		BIT(3)
#define CLK_GATE_MASK			GENMASK(3, 0)

#define CLK_PARENT_APMIXED		BIT(4)
#define CLK_PARENT_TOPCKGEN		BIT(5)
#define CLK_PARENT_INFRASYS		BIT(6)
#define CLK_PARENT_XTAL			BIT(7)
/*
 * For CLK_PARENT_MIXED to correctly work, is required to
 * define in clk_tree flags the clk type using the alias.
 */
#define CLK_PARENT_MIXED		BIT(8)
#define CLK_PARENT_MASK			GENMASK(8, 4)

/* alias to reference clk type */
#define CLK_APMIXED			CLK_PARENT_APMIXED
#define CLK_TOPCKGEN			CLK_PARENT_TOPCKGEN
#define CLK_INFRASYS			CLK_PARENT_INFRASYS

#define ETHSYS_HIFSYS_RST_CTRL_OFS	0x34

/* struct mtk_pll_data - hardware-specific PLLs data */
struct mtk_pll_data {
	const int id;
	u32 reg;
	u32 pwr_reg;
	u32 en_mask;
	u32 pd_reg;
	int pd_shift;
	u32 flags;
	u32 rst_bar_mask;
	u64 fmax;
	u64 fmin;
	int pcwbits;
	int pcwibits;
	u32 pcw_reg;
	int pcw_shift;
	u32 pcw_chg_reg;
};

/**
 * struct mtk_fixed_clk - fixed clocks
 *
 * @id:		index of clocks
 * @parent:	index of parnet clocks
 * @rate:	fixed rate
 */
struct mtk_fixed_clk {
	const int id;
	const int parent;
	unsigned long rate;
};

#define FIXED_CLK(_id, _parent, _rate) {		\
		.id = _id,				\
		.parent = _parent,			\
		.rate = _rate,				\
	}

/**
 * struct mtk_fixed_factor - fixed multiplier and divider clocks
 *
 * @id:		index of clocks
 * @parent:	index of parnet clocks
 * @mult:	multiplier
 * @div:	divider
 * @flag:	hardware-specific flags
 */
struct mtk_fixed_factor {
	const int id;
	const int parent;
	u32 mult;
	u32 div;
	u32 flags;
};

#define FACTOR(_id, _parent, _mult, _div, _flags) {	\
		.id = _id,				\
		.parent = _parent,			\
		.mult = _mult,				\
		.div = _div,				\
		.flags = _flags,			\
	}

/**
 * struct mtk_parent -  clock parent with flags. Needed for MUX that
 *			parent with mixed infracfg and topckgen.
 *
 * @id:			index of parent clocks
 * @flags:		hardware-specific flags (parent location,
 *			infracfg, topckgen, APMIXED, xtal ...)
 */
struct mtk_parent {
	const int id;
	u16 flags;
};

#define PARENT(_id, _flags) {				\
		.id = _id,				\
		.flags = _flags,			\
	}

/**
 * struct mtk_composite - aggregate clock of mux, divider and gate clocks
 *
 * @id:			index of clocks
 * @parent:		index of parnet clocks
 * @parent:		index of parnet clocks
 * @parent_flags:	table of parent clocks with flags
 * @mux_reg:		hardware-specific mux register
 * @gate_reg:		hardware-specific gate register
 * @mux_mask:		mask to the mux bit field
 * @mux_shift:		shift to the mux bit field
 * @gate_shift:		shift to the gate bit field
 * @num_parents:	number of parent clocks
 * @flags:		hardware-specific flags
 */
struct mtk_composite {
	const int id;
	union {
		const int *parent;
		const struct mtk_parent *parent_flags;
	};
	u32 mux_reg;
	u32 mux_set_reg;
	u32 mux_clr_reg;
	u32 upd_reg;
	u32 gate_reg;
	u32 mux_mask;
	signed char mux_shift;
	signed char upd_shift;
	signed char gate_shift;
	signed char num_parents;
	u16 flags;
};

#define MUX_GATE_FLAGS(_id, _parents, _reg, _shift, _width, _gate,	\
		       _flags) {					\
		.id = _id,						\
		.mux_reg = _reg,					\
		.mux_shift = _shift,					\
		.mux_mask = BIT(_width) - 1,				\
		.gate_reg = _reg,					\
		.gate_shift = _gate,					\
		.parent = _parents,					\
		.num_parents = ARRAY_SIZE(_parents),			\
		.flags = _flags,					\
	}

#define MUX_GATE(_id, _parents, _reg, _shift, _width, _gate)		\
	MUX_GATE_FLAGS(_id, _parents, _reg, _shift, _width, _gate, 0)

#define MUX_MIXED_FLAGS(_id, _parents, _reg, _shift, _width, _flags) {	\
		.id = _id,						\
		.mux_reg = _reg,					\
		.mux_shift = _shift,					\
		.mux_mask = BIT(_width) - 1,				\
		.gate_shift = -1,					\
		.parent_flags = _parents,				\
		.num_parents = ARRAY_SIZE(_parents),			\
		.flags = CLK_PARENT_MIXED | (_flags),			\
	}
#define MUX_MIXED(_id, _parents, _reg, _shift, _width)			\
	MUX_MIXED_FLAGS(_id, _parents, _reg, _shift, _width, 0)

#define MUX_FLAGS(_id, _parents, _reg, _shift, _width, _flags) {	\
		.id = _id,						\
		.mux_reg = _reg,					\
		.mux_shift = _shift,					\
		.mux_mask = BIT(_width) - 1,				\
		.gate_shift = -1,					\
		.parent = _parents,					\
		.num_parents = ARRAY_SIZE(_parents),			\
		.flags = _flags,					\
	}
#define MUX(_id, _parents, _reg, _shift, _width)			\
	MUX_FLAGS(_id, _parents, _reg, _shift, _width, 0)

#define MUX_CLR_SET_UPD_FLAGS(_id, _parents, _mux_ofs, _mux_set_ofs,\
			_mux_clr_ofs, _shift, _width, _gate,		\
			_upd_ofs, _upd, _flags) {			\
		.id = _id,						\
		.mux_reg = _mux_ofs,					\
		.mux_set_reg = _mux_set_ofs,			\
		.mux_clr_reg = _mux_clr_ofs,			\
		.upd_reg = _upd_ofs,					\
		.upd_shift = _upd,					\
		.mux_shift = _shift,					\
		.mux_mask = BIT(_width) - 1,				\
		.gate_reg = _mux_ofs,					\
		.gate_shift = _gate,					\
		.parent = _parents,					\
		.num_parents = ARRAY_SIZE(_parents),			\
		.flags = _flags,					\
	}

struct mtk_gate_regs {
	u32 sta_ofs;
	u32 clr_ofs;
	u32 set_ofs;
};

/**
 * struct mtk_gate - gate clocks
 *
 * @id:		index of gate clocks
 * @parent:	index of parnet clocks
 * @regs:	hardware-specific mux register
 * @shift:	shift to the gate bit field
 * @flags:	hardware-specific flags
 */
struct mtk_gate {
	const int id;
	const int parent;
	const struct mtk_gate_regs *regs;
	int shift;
	u32 flags;
};

/* struct mtk_clk_tree - clock tree */
struct mtk_clk_tree {
	unsigned long xtal_rate;
	unsigned long xtal2_rate;
	/*
	 * Clock ID offset are remapped with an auxiliary table.
	 * Enable this by defining .id_offs_map.
	 * This is needed for upstream linux kernel <soc>-clk.h that
	 * have mixed clk ID and doesn't have clear distinction between
	 * ID for factor, mux and gates.
	 */
	const int *id_offs_map; /* optional, table clk.h to driver ID */
	const int fdivs_offs;
	const int muxes_offs;
	const int gates_offs;
	const struct mtk_pll_data *plls;
	const struct mtk_fixed_clk *fclks;
	const struct mtk_fixed_factor *fdivs;
	const struct mtk_composite *muxes;
	const struct mtk_gate *gates;
	u32 flags;
};

struct mtk_clk_priv {
	struct udevice *parent;
	void __iomem *base;
	const struct mtk_clk_tree *tree;
};

struct mtk_cg_priv {
	struct udevice *parent;
	void __iomem *base;
	const struct mtk_clk_tree *tree;
	const struct mtk_gate *gates;
};

extern const struct clk_ops mtk_clk_apmixedsys_ops;
extern const struct clk_ops mtk_clk_topckgen_ops;
extern const struct clk_ops mtk_clk_infrasys_ops;
extern const struct clk_ops mtk_clk_gate_ops;

int mtk_common_clk_init(struct udevice *dev,
			const struct mtk_clk_tree *tree);
int mtk_common_clk_infrasys_init(struct udevice *dev,
				 const struct mtk_clk_tree *tree);
int mtk_common_clk_gate_init(struct udevice *dev,
			     const struct mtk_clk_tree *tree,
			     const struct mtk_gate *gates);

#endif /* __DRV_CLK_MTK_H */
