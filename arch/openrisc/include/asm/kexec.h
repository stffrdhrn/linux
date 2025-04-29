/* SPDX-License-Identifier: GPL-2.0 */
/*
 * OpenRISC kexec.h
 *
 * Architecture API for kexec.
 *
 * Copyright (C) 2024 Stafford Horne <shorne@gmail.com>
 */

#ifndef __ASM_OPENRISC_KEXEC_H
#define __ASM_OPENRISC_KEXEC_H

#include <asm/page.h>    /* For PAGE_SIZE */

/* Maximum physical address we can use pages from */
#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)

/* Maximum address we can reach in physical address mode */
#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)

/* Maximum address we can use for the control code buffer */
#define KEXEC_CONTROL_MEMORY_LIMIT (-1UL)

/* Reserve a page for the control code buffer */
#define KEXEC_CONTROL_PAGE_SIZE PAGE_SIZE

#define KEXEC_ARCH KEXEC_ARCH_OPENRISC

extern void or1k_crash_save_regs(struct pt_regs *newregs);

static inline void
crash_setup_regs(struct pt_regs *newregs,
		 struct pt_regs *oldregs)
{
	if (oldregs)
		memcpy(newregs, oldregs, sizeof(struct pt_regs));
	else
		or1k_crash_save_regs(newregs);
}

#define ARCH_HAS_KIMAGE_ARCH

struct kimage_arch {
	unsigned long fdt_addr;
};

extern const unsigned char or1k_kexec_relocate[];
extern const unsigned int or1k_kexec_relocate_size;

typedef void (*or1k_kexec_method)(unsigned long first_ind_entry,
				  unsigned long jump_addr,
				  unsigned long fdt_addr);

#endif
