/*
 * Copyright (C) 2014 Stefan Kristiansson <stefan.kristiansson@saunalahti.fi>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __ASM_OPENRISC_SMP_H
#define __ASM_OPENRISC_SMP_H

#include <asm/spr.h>
#include <asm/spr_defs.h>

#define raw_smp_processor_id()	(current_thread_info()->cpu)
#define hard_smp_processor_id()	mfspr(SPR_COREID)

extern void smp_init_cpus(void);
extern void arch_send_call_function_single_ipi(int cpu);
extern void arch_send_call_function_ipi_mask(const struct cpumask *mask);

extern void set_smp_cross_call(void (*)(const struct cpumask *, unsigned int),
			       unsigned int irq);
extern void handle_IPI(unsigned int ipi_msg);

/* For CPU hotplug */
extern int __cpu_disable(void);
static inline void __cpu_die(unsigned int cpu) { }

/* Hotplug and normal smp spinwait release */
extern asmlinkage void secondary_start_kernel(void);
extern void secondary_hotplug_release(void);
extern void play_dead(void);
extern unsigned long secondary_stack;

#endif /* __ASM_OPENRISC_SMP_H */
