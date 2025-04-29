/* SPDX-License-Identifier: GPL-2.0 */

#include <asm/spr_defs.h>
#include <asm/asm-offsets.h>
#include <asm/ptrace.h>
#include <linux/linkage.h>	/* For SYM_* macros */

.section ".text"
/*
 * Store system registers to a pt_regs structure.
 *
 * void or1k_crash_save_regs(struct pt_regs *newregs)
 */
SYM_CODE_START(or1k_crash_save_regs)
	l.sw    PT_SP(r3),r1
	l.sw    PT_GPR2(r3),r2
	l.sw    PT_GPR3(r3),r3
	l.sw    PT_GPR5(r3),r5
	l.sw    PT_GPR6(r3),r6
	l.sw    PT_GPR7(r3),r7
	l.sw    PT_GPR8(r3),r8
	l.sw    PT_GPR9(r3),r9
	l.sw    PT_GPR10(r3),r10
	l.sw    PT_GPR11(r3),r11
	l.sw    PT_GPR12(r3),r12
	l.sw    PT_GPR13(r3),r13
	l.sw    PT_GPR14(r3),r14
	l.sw    PT_GPR15(r3),r15
	l.sw    PT_GPR16(r3),r16
	l.sw    PT_GPR17(r3),r17
	l.sw    PT_GPR18(r3),r18
	l.sw    PT_GPR19(r3),r19
	l.sw    PT_GPR20(r3),r20
	l.sw    PT_GPR21(r3),r21
	l.sw    PT_GPR22(r3),r22
	l.sw    PT_GPR23(r3),r23
	l.sw    PT_GPR24(r3),r24
	l.sw    PT_GPR25(r3),r25
	l.sw    PT_GPR26(r3),r26
	l.sw    PT_GPR27(r3),r27
	l.sw    PT_GPR28(r3),r28
	l.sw    PT_GPR29(r3),r29
	l.sw    PT_GPR30(r3),r30
	l.sw    PT_GPR31(r3),r31

	/* Use the link address for the PC */
	l.sw    PT_PC(r3),r9

	l.mfspr	r31,r0,SPR_SR
	l.sw    PT_SR(r3),r31

	l.jr r9
SYM_CODE_END(or1k_crash_save_regs)
