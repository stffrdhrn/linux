/* SPDX-License-Identifier: GPL-2.0 */

#include <asm/page.h>
#include <asm/spr_defs.h>
#include <linux/kexec.h>
#include <linux/linkage.h>

/*
 * The kimage entries are setup as a linked list of pages in the
 * following arrangement:
 *
 *          +----------------------+    +-----------------------+
 * entry -> | ind_page|IND_INDRECT | -> | dest|IND_DESTINATION  |
 *          +----------------------+    | source|IND_SOURCE     |
 *                                      | source|IND_SOURCE     |
 *                                      | ...                   |
 *                                      | ind_page|IND_INDIRECT |
 *                                      +-----------------------+
 *                                        |
 *                                        V
 *                                      +-----------------------+
 *                                      | dest|IND_DESTINATION  |
 *                                      | source|IND_SOURCE     |
 *                                      | ...                   |
 *                                      | IND_DONE              |
 *                                      +-----------------------+
 *
 * source   - a pointer to a page aligned memory location that needs
 *            to be copied.
 * dest     - a pointer to a page aligned memory region that we will
 *            copy source data to.  Note this is not allocated as such,
 *            this is where we write resources for our new kernel to be
 *            booted and it may be overwriting the old kernel.
 * ind_page - a pointer to a page aligned page which we need to follow
 *            to get the next directory entry.
 */

#define KEXEC_SR_MASK ~(SPR_SR_IEE | SPR_SR_TEE | SPR_SR_IME | SPR_SR_DME)

	.section ".rodata"
	.align 4
SYM_CODE_START(or1k_kexec_relocate)

	/*
	 * r3: Pointer to the first image entry
	 * r4: (const) Phys address to jump to after relocation
	 * r5: (const) Phys address of the FDT image
	 * r6: Pointer to the source address for the relocation
	 * r7: Pointer to the destination address for the relocation
	 */
	l.movhi	r6,0x0
	l.movhi	r7,0x0

	/* Disable mmu and interrupts */
	l.mfspr r19,r0,SPR_SR
	l.movhi	r17,hi(KEXEC_SR_MASK)
	l.ori	r17,r17,lo(KEXEC_SR_MASK)
	l.and   r19,r19,r17
	l.mtspr r0,r19,SPR_ESR_BASE

	l.jal _mmu_off
	 l.movhi r19,hi(-KERNELBASE)
_mmu_off:
	l.addi	r9,r9,(_clear_irqs-_mmu_off)	/* calc jump location */
	l.add	r17,r19,r9			/* tophys */
	l.mtspr	r0,r17,SPR_EPCR_BASE
	l.rfe

_clear_irqs:
	/* clear interrupts */
	l.movhi r17,0xffff
	l.ori	r17,r17,0xffff
	l.mtspr r0,r17,SPR_PICSR

	/* Convert entry pointer to physical address */
	l.add	r3,r3,r19

	/* Process entries in a loop */
_next_entry:
	l.lwz	r17,0(r3)		/* r17 = *image->entry */
	l.addi	r3,r3,0x4		/* image->entry++ */

	l.andi	r19,r17,IND_DESTINATION

	l.sfeq	r19,r0
	l.bf	_test_indirection
	 l.andi	r19,r17,IND_INDIRECTION
	l.addi	r19,r0,~IND_DESTINATION
	l.j	_next_entry
	 l.and	r7,r17,r19	/* store the new destination */
_test_indirection:
	/* IND_INDIRECTION entry ? -> update next entry ptr (PA) */
	l.sfeq	r19,r0
	l.bf	_test_done
	 l.andi	r19,r17,IND_DONE
	l.addi	r19,r0,~IND_INDIRECTION
	l.j	_next_entry
	 l.and	r3,r17,r19 /* update r3 with new ind_page */
_test_done:
	/* IND_DONE entry ? -> jump to done label */
	l.sfeq	r19,r0
	l.bf	_test_source
	 l.andi	r19,r17,IND_SOURCE
	l.j	_kexec_boot
	 l.nop
_test_source:
	/*
	 * IND_SOURCE entry ? -> copy page word by word to the
	 * destination address we got from IND_DESTINATION
	 */
	l.sfeq	r19,r0
	l.bf	_next_entry	/* Unknown entry type, ignore it */
	 l.nop
	l.addi	r19,r0,~IND_SOURCE
	l.and	r6,r17,r19
	l.ori	r19,r0,(PAGE_SIZE/0x4)	/* i = num words per page */
	l.lwz	r17,0(r6)	/* t17 = *src_ptr */
_copy_loop:
	l.sw	0(r7),r17	/* *dst_ptr = *src_ptr */
	l.addi	r6,r6,0x4	/* stc_ptr++ */
	l.addi	r7,r7,0x4	/* dst_ptr++ */
	l.addi	r19,r19,-0x1	/* i-- */
	l.sfeq	r19,r0		/* copy done ? */
	l.bf	_next_entry
	 l.nop
	l.j	_copy_loop
	 l.lwz	r17,0(r6)	/* slot: r17 = *src_ptr */

_kexec_boot:
	/* Pass the arg to the next kernel */
	l.or	r3,r5,r5

	/* Cleanup */
	l.movhi	r5,0x0
	l.movhi	r6,0x0
	l.movhi	r7,0x0
	l.movhi	r8,0x0
	l.movhi	r9,0x0
	l.movhi	r10,0x0
	l.movhi	r11,0x0
	l.movhi	r12,0x0
	l.movhi	r13,0x0
	l.movhi	r14,0x0
	l.movhi	r15,0x0
	l.movhi	r16,0x0
	l.movhi	r17,0x0
	l.movhi	r18,0x0
	l.movhi	r19,0x0
	l.movhi	r20,0x0
	l.movhi	r21,0x0
	l.movhi	r22,0x0
	l.movhi	r23,0x0
	l.movhi	r24,0x0
	l.movhi	r25,0x0
	l.movhi	r26,0x0
	l.movhi	r27,0x0
	l.movhi	r28,0x0
	l.movhi	r29,0x0
	l.movhi	r30,0x0
	l.movhi	r31,0x0

	/* Clear exception context */
	l.mtspr r0,r0,SPR_EEAR_BASE
	l.mtspr r0,r0,SPR_EPCR_BASE
	l.mtspr r0,r0,SPR_ESR_BASE

	/*
	 * Make sure the relocated code is visible
	 * and jump to the new kernel
	 */
	l.msync

	l.jr	r4
	 l.nop

SYM_CODE_END(or1k_kexec_relocate)
or1k_kexec_relocate_end:

.section ".rodata"
SYM_DATA(or1k_kexec_relocate_size,
	.long or1k_kexec_relocate_end - or1k_kexec_relocate)

