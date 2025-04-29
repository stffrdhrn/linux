// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 FORTH-ICS/CARV
 *  Nick Kossifidis <mick@ics.forth.gr>
 */

#include <linux/kexec.h>
#include <asm/kexec.h>		/* For or1k_kexec_* symbol defines */
#include <linux/smp.h>		/* For smp_send_stop () */
#include <asm/cacheflush.h>	/* For local_flush_icache_all() */
#include <asm/barrier.h>	/* For smp_wmb() */
#include <asm/page.h>		/* For PAGE_MASK */
#include <linux/libfdt.h>	/* For fdt_check_header() */
#include <linux/compiler.h>	/* For unreachable() */
#include <linux/cpu.h>		/* For cpu_down() */
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

/*
 * machine_kexec_prepare - Initialize kexec
 *
 * This function is called from do_kexec_load, when the user has
 * provided us with an image to be loaded. Its goal is to validate
 * the image and prepare the control code buffer as needed.
 * Note that kimage_alloc_init has already been called and the
 * control buffer has already been allocated.
 */
int
machine_kexec_prepare(struct kimage *image)
{
	struct kimage_arch *internal = &image->arch;
	struct fdt_header fdt = {0};
	void *control_code_buffer = NULL;
	unsigned int control_code_buffer_sz = 0;
	int i = 0;

	/* Find the Flattened Device Tree and save its physical address */
	for (i = 0; i < image->nr_segments; i++) {
		if (image->segment[i].memsz <= sizeof(fdt))
			continue;

		if (image->file_mode)
			memcpy(&fdt, image->segment[i].buf, sizeof(fdt));
		else if (copy_from_user(&fdt, image->segment[i].buf, sizeof(fdt)))
			continue;

		if (fdt_check_header(&fdt))
			continue;

		/* Remember the fdt address to pass during startup */
		internal->fdt_addr = (unsigned long) image->segment[i].mem;
		break;
	}

	if (!internal->fdt_addr) {
		pr_err("Device tree not included in the provided image\n");
		return -EINVAL;
	}

	/* Copy the assembler code for relocation to the control page */
	if (image->type != KEXEC_TYPE_CRASH) {
		control_code_buffer = page_address(image->control_code_page);
		control_code_buffer_sz = page_size(image->control_code_page);

		if (unlikely(or1k_kexec_relocate_size > control_code_buffer_sz)) {
			pr_err("Relocation code doesn't fit within a control page\n");
			return -EINVAL;
		}

		memcpy(control_code_buffer, or1k_kexec_relocate,
			or1k_kexec_relocate_size);
	}

	return 0;
}


/*
 * machine_kexec_cleanup - Cleanup any leftovers from
 *			   machine_kexec_prepare
 *
 * This function is called by kimage_free to handle any arch-specific
 * allocations done on machine_kexec_prepare. Since we didn't do any
 * allocations there, this is just an empty function. Note that the
 * control buffer is freed by kimage_free.
 */
void
machine_kexec_cleanup(struct kimage *image)
{
}


/*
 * machine_shutdown - Prepare for a kexec reboot
 *
 * This function is called by kernel_kexec just before machine_kexec
 * below. Its goal is to prepare the rest of the system (the other
 * harts and possibly devices etc) for a kexec reboot.
 */
void machine_shutdown(void)
{
	smp_shutdown_nonboot_cpus(reboot_cpu);
}

/*
 * machine_crash_shutdown - Prepare to kexec after a kernel crash
 *
 * This function is called by crash_kexec just before machine_kexec
 * and its goal is to shutdown non-crashing cpus and save registers.
 */
void
machine_crash_shutdown(struct pt_regs *regs)
{
	local_irq_disable();

	/* shutdown non-crashing cpus */
	crash_smp_send_stop();

	crash_save_cpu(regs, smp_processor_id());
	machine_kexec_mask_interrupts();

	pr_info("Starting crashdump kernel...\n");
}

/*
 * Invalidate all destiniation pages to make them visible
 * by the next kernel.  The kexec code builds a page list
 * with physical addresses.
 */
static void local_kimage_icache_inv(struct kimage *image)
{
	kimage_entry_t *ptr, entry;
	unsigned long destination = 0;
	struct page *page;

	for (ptr = &image->head; (entry = *ptr) && !(entry & IND_DONE);
	     ptr = (*ptr & IND_INDIRECTION) ?
	       phys_to_virt(*ptr & PAGE_MASK) : ptr + 1) {
		if (entry & IND_DESTINATION) {
			destination = entry & PAGE_MASK;
		} else if (entry & IND_SOURCE) {
			page = virt_to_page(__va(destination));
			local_icache_page_inv(page);
			destination += PAGE_SIZE;
		}
	}
}

/*
 * machine_kexec - Jump to the loaded kimage
 *
 * This function is called by kernel_kexec which is called by the
 * reboot system call when the reboot cmd is LINUX_REBOOT_CMD_KEXEC,
 * or by crash_kernel which is called by the kernel's arch-specific
 * trap handler in case of a kernel panic. It's the final stage of
 * the kexec process where the pre-loaded kimage is ready to be
 * executed. We assume at this point that all other harts are
 * suspended and this hart will be the new boot hart.
 */
void __noreturn
machine_kexec(struct kimage *image)
{
	struct kimage_arch *internal = &image->arch;
	unsigned long jump_addr = (unsigned long) image->start;
	unsigned long first_ind_entry = (unsigned long) &image->head;
	unsigned long fdt_addr = internal->fdt_addr;
	void *control_code_buffer = page_address(image->control_code_page);
	or1k_kexec_method kexec_method = NULL;

	BUG_ON(num_online_cpus() > 1);

	kexec_method = control_code_buffer;

	/* If the jump address is not setup default to reset vector */
	if (!jump_addr) {
		jump_addr = 0x100;
	}

	pr_notice("Will call new kernel at %08lx\n", jump_addr);
	pr_notice("FDT image at %08lx\n", fdt_addr);

	/* Make sure the relocation code is visible to the cpu */
	local_kimage_icache_inv(image);

	/* Jump to the relocation code */
	pr_notice("Bye...\n");
	kexec_method(first_ind_entry, jump_addr, fdt_addr);
	unreachable();
}
