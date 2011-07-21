/*
 * (C) Copyright 2008 Michal Simek <monstr@monstr.eu>
 * Clean driver and add xilinx constant from header file
 *
 * (C) Copyright 2004 Atmark Techno, Inc.
 * Yasushi SHOJI <yashi@atmark-techno.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>

#if defined (CONFIG_SERIAL_MULTI)
#include <serial.h>
#endif

#define RX_FIFO		0 /* receive FIFO, read only */
#define TX_FIFO		1 /* transmit FIFO, write only */
#define STATUS_REG	2 /* status register, read only */

#define SR_TX_FIFO_FULL		0x08 /* transmit FIFO full */
#define SR_RX_FIFO_VALID_DATA	0x01 /* data in receive FIFO */
#define SR_RX_FIFO_FULL		0x02 /* receive FIFO full */

static u32 *userial_ports[4] = {
#ifdef XILINX_UARTLITE_BASEADDR
	(u32 *)XILINX_UARTLITE_BASEADDR,
#else
	NULL,
#endif
#ifdef XILINX_UARTLITE_BASEADDR1
	(u32 *)XILINX_UARTLITE_BASEADDR1,
#else
	NULL,
#endif
#ifdef XILINX_UARTLITE_BASEADDR2
	(u32 *)XILINX_UARTLITE_BASEADDR2,
#else
	NULL,
#endif
#ifdef XILINX_UARTLITE_BASEADDR3
	(u32 *)XILINX_UARTLITE_BASEADDR3
#else
	NULL
#endif
};

void uartlite_serial_putc(const char c, const int port)
{
	if (c == '\n')
		uartlite_serial_putc('\r', port);
	while (in_be32(userial_ports[port] + STATUS_REG) & SR_TX_FIFO_FULL);
	out_be32(userial_ports[port] + TX_FIFO, (unsigned char) (c & 0xff));
}

void uartlite_serial_puts(const char * s, const int port)
{
	while (*s) {
		uartlite_serial_putc(*s++, port);
	}
}

int uartlite_serial_getc(const int port)
{
	while (!(in_be32(userial_ports[port] + STATUS_REG) &
							SR_RX_FIFO_VALID_DATA));
	return in_be32(userial_ports[port] + RX_FIFO) & 0xff;
}

int uartlite_serial_tstc(const int port)
{
	return (in_be32(userial_ports[port] + STATUS_REG) &
							SR_RX_FIFO_VALID_DATA);
}

#if !defined(CONFIG_SERIAL_MULTI)
int serial_init(void)
{
	/* FIXME: Nothing for now. We should initialize fifo, etc */
	return 0;
}

void serial_setbrg(void)
{
	/* FIXME: what's this for? */
}

void serial_putc(const char c)
{
	uartlite_serial_putc(c, 0);
}

void serial_puts(const char *s)
{
	uartlite_serial_puts(s, 0);
}

int serial_getc(void)
{
	return uartlite_serial_getc(0);
}

int serial_tstc(void)
{
	return uartlite_serial_tstc(0);
}
#endif

#if defined(CONFIG_SERIAL_MULTI)
/* Multi serial device functions */
#define DECLARE_ESERIAL_FUNCTIONS(port) \
    int  userial##port##_init (void) {return(0);}\
    void userial##port##_setbrg (void) {}\
    int  userial##port##_getc (void) {return uartlite_serial_getc(port);}\
    int  userial##port##_tstc (void) {return uartlite_serial_tstc(port);}\
    void userial##port##_putc (const char c) {uartlite_serial_putc(c, port);}\
    void userial##port##_puts (const char *s) {uartlite_serial_puts(s, port);}

/* Serial device descriptor */
#define INIT_ESERIAL_STRUCTURE(port,name,bus) {\
	name,\
	bus,\
	userial##port##_init,\
	NULL,\
	userial##port##_setbrg,\
	userial##port##_getc,\
	userial##port##_tstc,\
	userial##port##_putc,\
	userial##port##_puts, }

DECLARE_ESERIAL_FUNCTIONS(0);
struct serial_device uartlite_serial0_device =
	INIT_ESERIAL_STRUCTURE(0,"ttyUL0","ttyUL0");
DECLARE_ESERIAL_FUNCTIONS(1);
struct serial_device uartlite_serial1_device =
	INIT_ESERIAL_STRUCTURE(1,"ttyUL1","ttyUL1");
DECLARE_ESERIAL_FUNCTIONS(2);
struct serial_device uartlite_serial2_device =
	INIT_ESERIAL_STRUCTURE(2,"ttyUL2","ttyUL2");
DECLARE_ESERIAL_FUNCTIONS(3);
struct serial_device uartlite_serial3_device =
	INIT_ESERIAL_STRUCTURE(3,"ttyUL3","ttyUL3");

#endif /* CONFIG_SERIAL_MULTI */
