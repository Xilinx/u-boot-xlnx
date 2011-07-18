/*
 * U-boot driver for Xilinx Dragonfire UART.
 * Based on existing U-boot serial drivers.
 */

#include <common.h>
#include <asm/io.h>

/* Some clock/baud constants */
#define XDFUART_BDIV          15 /* Default/reset BDIV value */
#define XDFUART_BASECLK 3125000L /* master/(bdiv+1) */

/* UART register offsets */
#define XDFUART_CR_OFFSET          0x00  /* Control Register [8:0] */
#define XDFUART_MR_OFFSET          0x04  /* Mode Register [10:0] */
#define XDFUART_BAUDGEN_OFFSET     0x18  /* Baud Rate Generator [15:0] */
#define XDFUART_SR_OFFSET          0x2C  /* Channel Status [11:0] */
#define XDFUART_FIFO_OFFSET        0x30  /* FIFO [15:0] or [7:0] */
#define XDFUART_BAUDDIV_OFFSET     0x34  /* Baud Rate Divider [7:0] */

/* Channel status register
 *
 * The channel status register (CSR) is provided to enable the control logic
 * to monitor the status of bits in the channel interrupt status register,
 * even if these are masked out by the interrupt mask register.
 *
 */
#define XDFUART_SR_TXFULL   0x00000010	 /* TX FIFO full */
#define XDFUART_SR_RXEMPTY  0x00000002	 /* RX FIFO empty */

/* Some access macros */
#define xdfuart_readl(reg) \
	readl((void*)UART_BASE + XDFUART_##reg##_OFFSET)
#define xdfuart_writel(reg,value) \
	writel((value),(void*)UART_BASE + XDFUART_##reg##_OFFSET)

DECLARE_GLOBAL_DATA_PTR;

/* Set up the baud rate in gd struct */
void serial_setbrg(void)
{
	/*              master clock
	 * Baud rate = ---------------
	 *              bgen*(bdiv+1)
	 *
	 * master clock = 50MHz (I think?)
	 * bdiv remains at default (reset) 15
	 *
	 * Simplify RHS to base/bgen, where base == master/16
	 */
	long base = XDFUART_BASECLK;
	long baud = gd->baudrate;
	long bgen = base / baud;
	long base_err = base - (bgen*baud);
	long mod_bgen = bgen + (base_err >= 0 ? 1 : -1);
	long mod_err = base - (mod_bgen*baud);

	/* you'd think there'd be an abs() macro somewhere in include/... */
	mod_err = mod_err < 0 ? -mod_err : mod_err;
	base_err = base_err < 0 ? -base_err : base_err;

	bgen = mod_err < base_err ? mod_bgen : bgen;

	xdfuart_writel(BAUDDIV,XDFUART_BDIV); /* Ensure it's 15 */
	xdfuart_writel(BAUDGEN,bgen);
}

#define XDFUART_CR_TX_EN       0x00000010  /* TX enabled */
#define XDFUART_CR_RX_EN       0x00000004  /* RX enabled */
#define XDFUART_CR_TXRST       0x00000002  /* TX logic reset */
#define XDFUART_CR_RXRST       0x00000001  /* RX logic reset */

#define XDFUART_MR_PARITY_NONE      0x00000020  /* No parity mode */

/* Initialize the UART, with...some settings. */
int serial_init(void)
{
	/* RX/TX enabled & reset */
	xdfuart_writel(CR, XDFUART_CR_TX_EN | XDFUART_CR_RX_EN | \
			XDFUART_CR_TXRST | XDFUART_CR_RXRST);
	xdfuart_writel(MR, XDFUART_MR_PARITY_NONE); /* 8 bit, no parity */
	serial_setbrg();
	return 0;
}

/* Write a char to the Tx buffer */
void serial_putc(char c)
{
	while ((xdfuart_readl(SR) & XDFUART_SR_TXFULL) != 0);
	if (c == '\n') {
		xdfuart_writel(FIFO,'\r');
		while ((xdfuart_readl(SR) & XDFUART_SR_TXFULL) != 0);
	}
	xdfuart_writel(FIFO,c);
}

/* Write a null-terminated string to the UART */
void serial_puts(const char *s)
{
	while (*s)
		serial_putc(*s++);
}

/* Get a char from Rx buffer */
int serial_getc(void)
{
	while (!serial_tstc())
		;
	return xdfuart_readl(FIFO);
}

/* Test character presence in Rx buffer */
int serial_tstc(void)
{
	return (xdfuart_readl(SR) & XDFUART_SR_RXEMPTY) == 0;
}
