/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_M68K_PCI_H
#define _ASM_M68K_PCI_H

#define	pcibios_assign_all_busses()	1

#define	PCIBIOS_MIN_IO		0x00000100
#define	PCIBIOS_MIN_MEM		0x02000000

static inline int pci_get_legacy_ide_irq(struct pci_dev *dev, int channel)
{
	return channel ? 15 : 14;
}

#endif /* _ASM_M68K_PCI_H */
