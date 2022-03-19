/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_SPINLOCK_TYPES_H
#define __ASM_GENERIC_SPINLOCK_TYPES_H

/*
 * Using ticket spinlock as generic for SMP support.
 */
#ifdef CONFIG_SMP
#include <asm-generic/ticket-lock-types.h>
#include <asm-generic/qrwlock_types.h>
#else
#error The asm-generic/spinlock_types.h is not for CONFIG_SMP=n
#endif

#endif /* __ASM_GENERIC_SPINLOCK_TYPES_H */
