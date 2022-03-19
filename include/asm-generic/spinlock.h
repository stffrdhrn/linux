/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_SPINLOCK_H
#define __ASM_GENERIC_SPINLOCK_H

/*
 * Using ticket-spinlock.h as generic for SMP support.
 */
#ifdef CONFIG_SMP
#include <asm-generic/ticket-lock.h>
#ifdef CONFIG_QUEUED_RWLOCKS
#include <asm-generic/qrwlock.h>
#else
#error Please select ARCH_USE_QUEUED_RWLOCKS in architecture Kconfig
#endif
#endif

#endif /* __ASM_GENERIC_SPINLOCK_H */
