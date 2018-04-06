#ifndef _ASM_OPENRISC_BARRIER_H
#define _ASM_OPENRISC_BARRIER_H

#define mb() asm volatile ("l.msync":::"memory")

#include <asm-generic/barrier.h>

#endif /* _ASM_OPENRISC_BARRIER_H */
