#
# (C) Copyright 2000-2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# (C) Copyright 2008
# Ricardo Ribalda-Universidad Autonoma de Madrid-ricardo.ribalda@uam.es
# Work supported by Qtechnology http://www.qtec.com
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

include $(TOPDIR)/config.mk
ifneq ($(OBJTREE),$(SRCTREE))
endif

# Conditionally add Ethernet
OBJS-$(CONFIG_XILINX_LL_TEMAC)    += ../xilinx_lltemac/xlltemac_adapter.o \
                           ../xilinx_lltemac/xlltemac.o \
                           ../xilinx_lltemac/xlltemac_control.o \
                           ../common/xlldma.o \
                           ../common/xlldma_bdring.o \
                           ../common/xipif_v1_23_b.o \
                           ../common/xbasic_types.o \
                           ../common/xversion.o \
                           ../common/xio_dcr.o
CFLAGS-$(CONFIG_XILINX_LL_TEMAC)  += -I../xilinx_lltemac

# Conditionally add IIC
OBJS-$(CONFIG_XILINX_IIC)        += ../xilinx_iic/iic_adapter.o \
                           ../xilinx_iic/xiic_l.o \
                           ../common/xipif_v1_23_b.o \
                           ../common/xbasic_types.o \
                           ../common/xversion.o
CFLAGS-$(CONFIG_XILINX_IIC)      += -I../xilinx_iic

INCS		+= -I../common -I../ppc440-generic
CFLAGS		+= $(INCS) $(CFLAGS-y)
HOST_CFLAGS	+= $(INCS)

LIB	= $(obj)lib$(BOARD).a


COBJS	+= ../../xilinx/ppc440-generic/xilinx_ppc440_generic.o $(OBJS-y)

SOBJS	+= ../../xilinx/ppc440-generic/init.o

SRCS	:= $(SOBJS:.o=.S) $(COBJS:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS))
SOBJS	:= $(addprefix $(obj),$(SOBJS))

$(LIB):	$(OBJS) $(SOBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -f $(SOBJS) $(OBJS)

distclean:	clean
	rm -f $(LIB) core *.bak .depend

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
