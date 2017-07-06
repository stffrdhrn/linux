/*
 * OpenRISC timer API
 *
 * Copyright (C) 2008 by Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2017 by Stafford Horne (shorne@gmail.com)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_OR1K_TIMER_H
#define __ASM_OR1K_TIMER_H

#include <linux/compiler.h>

void openrisc_timer_set(unsigned long count);
void openrisc_timer_set_next(unsigned long delta);

#ifdef CONFIG_SMP
void synchronise_count_master(int cpu);
void synchronise_count_slave(int cpu);
#else
static inline void synchronise_count_master(int cpu) {}
static inline void synchronise_count_slave(int cpu) {}
#endif

#endif /* __ASM_OR1K_TIMER_H */
