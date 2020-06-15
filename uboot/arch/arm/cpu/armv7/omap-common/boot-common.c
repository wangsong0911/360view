/*
 * boot-common.c
 *
 * Common bootmode functions for omap based boards
 *
 * Copyright (C) 2011, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <asm/omap_common.h>
#include <asm/arch/omap.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/sys_proto.h>
#include <asm/ti-common/edma.h>
#include <watchdog.h>
#include <usb.h>

DECLARE_GLOBAL_DATA_PTR;

void save_omap_boot_params(void)
{
	u32 rom_params = *((u32 *)OMAP_SRAM_SCRATCH_BOOT_PARAMS);
	u8 boot_device;
	u32 dev_desc, dev_data;

	if ((rom_params <  NON_SECURE_SRAM_START) ||
	    (rom_params > NON_SECURE_SRAM_END))
		return;

	/*
	 * rom_params can be type casted to omap_boot_parameters and
	 * used. But it not correct to assume that romcode structure
	 * encoding would be same as u-boot. So use the defined offsets.
	 */
	gd->arch.omap_boot_params.omap_bootdevice = boot_device =
				   *((u8 *)(rom_params + BOOT_DEVICE_OFFSET));

	gd->arch.omap_boot_params.ch_flags =
				*((u8 *)(rom_params + CH_FLAGS_OFFSET));

	if ((boot_device >= MMC_BOOT_DEVICES_START) &&
	    (boot_device <= MMC_BOOT_DEVICES_END)) {
#if !defined(CONFIG_AM33XX) && !defined(CONFIG_TI81XX) && \
	!defined(CONFIG_AM43XX)
		if ((omap_hw_init_context() ==
				      OMAP_INIT_CONTEXT_UBOOT_AFTER_SPL)) {
			gd->arch.omap_boot_params.omap_bootmode =
			*((u8 *)(rom_params + BOOT_MODE_OFFSET));
		} else
#endif
		{
			dev_desc = *((u32 *)(rom_params + DEV_DESC_PTR_OFFSET));
			dev_data = *((u32 *)(dev_desc + DEV_DATA_PTR_OFFSET));
			gd->arch.omap_boot_params.omap_bootmode =
					*((u32 *)(dev_data + BOOT_MODE_OFFSET));
		}
	}

#if defined(CONFIG_DRA7XX) || defined(CONFIG_AM57XX)
	/*
	 * We get different values for QSPI_1 and QSPI_4 being used, but
	 * don't actually care about this difference.  Rather than
	 * mangle the later code, if we're coming in as QSPI_4 just
	 * change to the QSPI_1 value.
	 */
	if (gd->arch.omap_boot_params.omap_bootdevice == 11)
		gd->arch.omap_boot_params.omap_bootdevice = BOOT_DEVICE_SPI;
#endif
}

#ifdef CONFIG_SPL_BUILD
u32 spl_boot_device(void)
{
	return (u32) (gd->arch.omap_boot_params.omap_bootdevice);
}

u32 spl_boot_mode(void)
{
	u32 val = gd->arch.omap_boot_params.omap_bootmode;

#ifdef CONFIG_SPL_SPI_PROD_OS_BOOT
	if (val == SPI_MODE_PROD)
		return SPI_MODE_PROD;
#endif
	if (val == MMCSD_MODE_RAW)
		return MMCSD_MODE_RAW;
	else if (val == MMCSD_MODE_FAT)
		return MMCSD_MODE_FAT;
	else
#ifdef CONFIG_SUPPORT_EMMC_BOOT
		return MMCSD_MODE_EMMCBOOT;
#else
		return MMCSD_MODE_UNDEFINED;
#endif
}

void spl_board_init(void)
{
#if defined(CONFIG_SPL_DMA_SUPPORT) && defined(CONFIG_TI_EDMA)
	edma_init(0);
	edma_request_channel(1,1,0);
#endif
#ifdef CONFIG_SPL_NAND_SUPPORT
	gpmc_init();
#endif
#if defined(CONFIG_AM33XX) && defined(CONFIG_SPL_MUSB_NEW_SUPPORT)
	arch_misc_init();
#endif
#if defined(CONFIG_AM43XX) && defined(CONFIG_SPL_USBETH_SUPPORT)
	board_usb_init(0, USB_INIT_DEVICE);
#endif
#if defined(CONFIG_HW_WATCHDOG)
	hw_watchdog_init();
#endif
#ifdef CONFIG_AM33XX
	am33xx_spl_board_init();
#endif
#if defined(CONFIG_DRA7XX)
	board_init();
#endif
#if defined(CONFIG_OMAP_SECURE)
	if (get_device_type() == GP_DEVICE) {
		puts("SPL: Built for secure part, but this isn't a secure part...aborting!\n");
		hang();
	}
#endif
}

int board_mmc_init(bd_t *bis)
{
	switch (spl_boot_device()) {
	case BOOT_DEVICE_MMC1:
		omap_mmc_init(0, 0, 0, -1, -1);
		break;
#ifdef CONFIG_PERIPHERAL_BOOT
	case BOOT_DEVICE_UART:
#endif
	case BOOT_DEVICE_USB:
	case BOOT_DEVICE_MMC2:
	case BOOT_DEVICE_MMC2_2:
		omap_mmc_init(1, 0, 0, -1, -1);
		break;

	/*
	 * We are in the SPL. We are booting from SPI but also initializing
	 * MMC. We assume that this is being done for
	 *
	 * 1. Linux boot with boot time optimizations as eMMC is faster than
	 * SPI for data transfers. (or)
	 *
	 * 2. Android boot which uses eMMC by default.
	 *
	 * We will initialize eMMC interface as it has faster read speeds.
	 */
	case BOOT_DEVICE_SPI:
		omap_mmc_init(1, 0, 0, -1, -1);
		break;
	}
	return 0;
}

void __noreturn jump_to_image_no_args(struct spl_image_info *spl_image)
{
	typedef void __noreturn (*image_entry_noargs_t)(u32 *);
	image_entry_noargs_t image_entry =
			(image_entry_noargs_t) spl_image->entry_point;

	debug("image entry point: 0x%X\n", spl_image->entry_point);
	/* Pass the saved boot_params from rom code */
	image_entry((u32 *)&gd->arch.omap_boot_params);
}
#endif
