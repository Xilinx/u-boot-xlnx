/**
 * (C) Copyright 2014, Cavium Inc.
 * (C) Copyright 2017, Xilinx Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
**/

#include <asm-offsets.h>
#include <config.h>
#include <efi_loader.h>
#include <version.h>
#include <asm/macro.h>
#include <asm/psci.h>
#include <asm/system.h>

/*
 * Issue the hypervisor call
 *
 * x0~x7: input arguments
 * x0~x3: output arguments
 */
static void __efi_runtime hvc_call(struct pt_regs *args)
{
	asm volatile(
		"ldr x0, %0\n"
		"ldr x1, %1\n"
		"ldr x2, %2\n"
		"ldr x3, %3\n"
		"ldr x4, %4\n"
		"ldr x5, %5\n"
		"ldr x6, %6\n"
		"ldr x7, %7\n"
		"hvc	#0\n"
		"str x0, %0\n"
		"str x1, %1\n"
		"str x2, %2\n"
		"str x3, %3\n"
		: "+m" (args->regs[0]), "+m" (args->regs[1]),
		  "+m" (args->regs[2]), "+m" (args->regs[3])
		: "m" (args->regs[4]), "m" (args->regs[5]),
		  "m" (args->regs[6]), "m" (args->regs[7])
		: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
		  "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
		  "x16", "x17");
}

/*
 * void smc_call(arg0, arg1...arg7)
 *
 * issue the secure monitor call
 *
 * x0~x7: input arguments
 * x0~x3: output arguments
 */

void __efi_runtime smc_call(struct pt_regs *args)
{
	asm volatile(
		"ldr x0, %0\n"
		"ldr x1, %1\n"
		"ldr x2, %2\n"
		"ldr x3, %3\n"
		"ldr x4, %4\n"
		"ldr x5, %5\n"
		"ldr x6, %6\n"
		"smc	#0\n"
		"str x0, %0\n"
		"str x1, %1\n"
		"str x2, %2\n"
		"str x3, %3\n"
		: "+m" (args->regs[0]), "+m" (args->regs[1]),
		  "+m" (args->regs[2]), "+m" (args->regs[3])
		: "m" (args->regs[4]), "m" (args->regs[5]),
		  "m" (args->regs[6])
		: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
		  "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
		  "x16", "x17");
}

/*
 * For now, all systems we support run at least in EL2 and thus
 * trigger PSCI calls to EL3 using SMC. If anyone ever wants to
 * use PSCI on U-Boot running below a hypervisor, please detect
 * this and set the flag accordingly.
 */
static const __efi_runtime_data bool use_smc_for_psci = true;

void __noreturn __efi_runtime psci_system_reset(void)
{
	struct pt_regs regs;

	regs.regs[0] = ARM_PSCI_0_2_FN_SYSTEM_RESET;

	if (use_smc_for_psci)
		smc_call(&regs);
	else
		hvc_call(&regs);

	while (1)
		;
}

void __noreturn __efi_runtime psci_system_off(void)
{
	struct pt_regs regs;

	regs.regs[0] = ARM_PSCI_0_2_FN_SYSTEM_OFF;

	if (use_smc_for_psci)
		smc_call(&regs);
	else
		hvc_call(&regs);

	while (1)
		;
}

#ifdef CONFIG_CMD_POWEROFF
int do_poweroff(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	puts("poweroff ...\n");

	udelay(50000); /* wait 50 ms */

	disable_interrupts();

	psci_system_off();

	/*NOTREACHED*/
	return 0;
}
#endif

#ifdef CONFIG_PSCI_RESET
void reset_misc(void)
{
	psci_system_reset();
}

#ifdef CONFIG_EFI_LOADER
void __efi_runtime EFIAPI efi_reset_system(
			enum efi_reset_type reset_type,
			efi_status_t reset_status,
			unsigned long data_size, void *reset_data)
{
	switch (reset_type) {
	case EFI_RESET_COLD:
	case EFI_RESET_WARM:
		psci_system_reset();
		break;
	case EFI_RESET_SHUTDOWN:
		psci_system_off();
		break;
	}

	while (1) { }
}
#endif /* CONFIG_EFI_LOADER */
#endif /* CONFIG_PSCI_RESET */
