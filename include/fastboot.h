/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2008 - 2009
 * Windriver, <www.windriver.com>
 * Tom Rix <Tom.Rix@windriver.com>
 *
 * Copyright 2011 Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * Copyright 2014 Linaro, Ltd.
 * Rob Herring <robh@kernel.org>
 */
#ifndef _FASTBOOT_H_
#define _FASTBOOT_H_

#include <linux/types.h>

#define FASTBOOT_VERSION	"0.4"

/*
 * Signals u-boot fastboot code to send multiple responses by
 * calling response generating function repeatedly until a OKAY/FAIL
 * is generated as final response.
 *
 * This status code is only used internally to signal, must NOT
 * be sent to host.
 */
#define FASTBOOT_MULTIRESPONSE_START	("MORE")

/* The 64 defined bytes plus \0 */
#define FASTBOOT_COMMAND_LEN	(64 + 1)
#define FASTBOOT_RESPONSE_LEN	(64 + 1)

/**
 * All known commands to fastboot
 */
enum {
	FASTBOOT_COMMAND_GETVAR = 0,
	FASTBOOT_COMMAND_DOWNLOAD,
	FASTBOOT_COMMAND_FLASH,
	FASTBOOT_COMMAND_ERASE,
	FASTBOOT_COMMAND_BOOT,
	FASTBOOT_COMMAND_CONTINUE,
	FASTBOOT_COMMAND_REBOOT,
	FASTBOOT_COMMAND_REBOOT_BOOTLOADER,
	FASTBOOT_COMMAND_REBOOT_FASTBOOTD,
	FASTBOOT_COMMAND_REBOOT_RECOVERY,
	FASTBOOT_COMMAND_SET_ACTIVE,
	FASTBOOT_COMMAND_OEM_FORMAT,
	FASTBOOT_COMMAND_OEM_PARTCONF,
	FASTBOOT_COMMAND_OEM_BOOTBUS,
	FASTBOOT_COMMAND_OEM_RUN,
	FASTBOOT_COMMAND_OEM_CONSOLE,
	FASTBOOT_COMMAND_OEM_BOARD,
	FASTBOOT_COMMAND_ACMD,
	FASTBOOT_COMMAND_UCMD,
	FASTBOOT_COMMAND_COUNT
};

/**
 * Reboot reasons
 */
enum fastboot_reboot_reason {
	FASTBOOT_REBOOT_REASON_BOOTLOADER,
	FASTBOOT_REBOOT_REASON_FASTBOOTD,
	FASTBOOT_REBOOT_REASON_RECOVERY,
	FASTBOOT_REBOOT_REASONS_COUNT
};

/**
 * fastboot_response() - Writes a response of the form "$tag$reason".
 *
 * @tag: The first part of the response
 * @response: Pointer to fastboot response buffer
 * @format: printf style format string
 */
void fastboot_response(const char *tag, char *response,
		       const char *format, ...)
	__attribute__ ((format (__printf__, 3, 4)));

/**
 * fastboot_fail() - Write a FAIL response of the form "FAIL$reason".
 *
 * @reason: Pointer to returned reason string
 * @response: Pointer to fastboot response buffer
 */
void fastboot_fail(const char *reason, char *response);

/**
 * fastboot_okay() - Write an OKAY response of the form "OKAY$reason".
 *
 * @reason: Pointer to returned reason string, or NULL to send a bare "OKAY"
 * @response: Pointer to fastboot response buffer
 */
void fastboot_okay(const char *reason, char *response);

/**
 * fastboot_set_reboot_flag() - Set flag to indicate reboot-bootloader
 *
 * Set flag which indicates that we should reboot into the bootloader
 * following the reboot that fastboot executes after this function.
 *
 * This function should be overridden in your board file with one
 * which sets whatever flag your board specific Android bootloader flow
 * requires in order to re-enter the bootloader.
 */
int fastboot_set_reboot_flag(enum fastboot_reboot_reason reason);

/**
 * fastboot_set_progress_callback() - set progress callback
 *
 * @progress: Pointer to progress callback
 *
 * Set a callback which is invoked periodically during long running operations
 * (flash and erase). This can be used (for example) by the UDP transport to
 * send INFO responses to keep the client alive whilst those commands are
 * executing.
 */
void fastboot_set_progress_callback(void (*progress)(const char *msg));

/*
 * fastboot_init() - initialise new fastboot protocol session
 *
 * @buf_addr: Pointer to download buffer, or NULL for default
 * @buf_size: Size of download buffer, or zero for default
 */
void fastboot_init(void *buf_addr, u32 buf_size);

/**
 * fastboot_boot() - Execute fastboot boot command
 *
 * If ${fastboot_bootcmd} is set, run that command to execute the boot
 * process, if that returns, then exit the fastboot server and return
 * control to the caller.
 *
 * Otherwise execute "bootm <fastboot_buf_addr>", if that fails, reset
 * the board.
 */
void fastboot_boot(void);

/**
 * fastboot_handle_boot() - Shared implementation of system reaction to
 * fastboot commands
 *
 * Making desceisions about device boot state (stay in fastboot, reboot
 * to bootloader, reboot to OS, etc).
 */
void fastboot_handle_boot(int command, bool success);

/**
 * fastboot_handle_command() - Handle fastboot command
 *
 * @cmd_string: Pointer to command string
 * @response: Pointer to fastboot response buffer
 *
 * Return: Executed command, or -1 if not recognized
 */
int fastboot_handle_command(char *cmd_string, char *response);

/**
 * fastboot_data_remaining() - return bytes remaining in current transfer
 *
 * Return: Number of bytes left in the current download
 */
u32 fastboot_data_remaining(void);

/**
 * fastboot_data_download() - Copy image data to fastboot_buf_addr.
 *
 * @fastboot_data: Pointer to received fastboot data
 * @fastboot_data_len: Length of received fastboot data
 * @response: Pointer to fastboot response buffer
 *
 * Copies image data from fastboot_data to fastboot_buf_addr. Writes to
 * response. fastboot_bytes_received is updated to indicate the number
 * of bytes that have been transferred.
 */
void fastboot_data_download(const void *fastboot_data,
			    unsigned int fastboot_data_len, char *response);

/**
 * fastboot_data_complete() - Mark current transfer complete
 *
 * @response: Pointer to fastboot response buffer
 *
 * Set image_size and ${filesize} to the total size of the downloaded image.
 */
void fastboot_data_complete(char *response);

/**
 * fastboot_handle_multiresponse() - Called for each response to send
 *
 * @cmd: Command id that requested multiresponse
 * @response: Pointer to fastboot response buffer
 */
void fastboot_multiresponse(int cmd, char *response);

void fastboot_acmd_complete(void);
#endif /* _FASTBOOT_H_ */
