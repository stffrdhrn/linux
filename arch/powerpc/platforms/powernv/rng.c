// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2013, Michael Ellerman, IBM Corporation.
 */

#define pr_fmt(fmt)	"powernv-rng: " fmt

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <asm/archrandom.h>
#include <asm/cputable.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/smp.h>
#include "powernv.h"

#define DARN_ERR 0xFFFFFFFFFFFFFFFFul

struct powernv_rng {
	void __iomem *regs;
	void __iomem *regs_real;
	unsigned long mask;
};

static DEFINE_PER_CPU(struct powernv_rng *, powernv_rng);

static struct {
	struct powernv_rng rng;
	spinlock_t lock;
} early_state __initdata = {
	.lock = __SPIN_LOCK_UNLOCKED(powernv_early_rng)
};

int powernv_hwrng_present(void)
{
	struct powernv_rng *rng;

	rng = get_cpu_var(powernv_rng);
	put_cpu_var(rng);
	return rng != NULL;
}

static unsigned long rng_whiten(struct powernv_rng *rng, unsigned long val)
{
	unsigned long parity;

	/* Calculate the parity of the value */
	asm (".machine push;   \
	      .machine power7; \
	      popcntd %0,%1;   \
	      .machine pop;"
	     : "=r" (parity) : "r" (val));

	/* xor our value with the previous mask */
	val ^= rng->mask;

	/* update the mask based on the parity of this value */
	rng->mask = (rng->mask << 1) | (parity & 1);

	return val;
}

int powernv_get_random_real_mode(unsigned long *v)
{
	struct powernv_rng *rng;

	rng = raw_cpu_read(powernv_rng);

	*v = rng_whiten(rng, __raw_rm_readq(rng->regs_real));

	return 1;
}

static int powernv_get_random_darn(unsigned long *v)
{
	unsigned long val;

	/* Using DARN with L=1 - 64-bit conditioned random number */
	asm volatile(PPC_DARN(%0, 1) : "=r"(val));

	if (val == DARN_ERR)
		return 0;

	*v = val;

	return 1;
}

static int __init initialize_darn(void)
{
	unsigned long val;
	int i;

	if (!cpu_has_feature(CPU_FTR_ARCH_300))
		return -ENODEV;

	for (i = 0; i < 10; i++) {
		if (powernv_get_random_darn(&val)) {
			ppc_md.get_random_seed = powernv_get_random_darn;
			return 0;
		}
	}
	return -EIO;
}

static int __init powernv_get_random_long_early(unsigned long *v)
{
	unsigned long flags;

	spin_lock_irqsave(&early_state.lock, flags);
	*v = rng_whiten(&early_state.rng, in_be64(early_state.rng.regs));
	spin_unlock_irqrestore(&early_state.lock, flags);

	return 1;
}

int powernv_get_random_long(unsigned long *v)
{
	struct powernv_rng *rng;

	rng = get_cpu_var(powernv_rng);

	*v = rng_whiten(rng, in_be64(rng->regs));

	put_cpu_var(rng);

	return 1;
}
EXPORT_SYMBOL_GPL(powernv_get_random_long);

static __init void rng_init_per_cpu(struct powernv_rng *rng,
				    struct device_node *dn)
{
	int chip_id, cpu;

	chip_id = of_get_ibm_chip_id(dn);
	if (chip_id == -1)
		pr_warn("No ibm,chip-id found for %pOF.\n", dn);

	for_each_possible_cpu(cpu) {
		if (per_cpu(powernv_rng, cpu) == NULL ||
		    cpu_to_chip_id(cpu) == chip_id) {
			per_cpu(powernv_rng, cpu) = rng;
		}
	}
}

static __init int rng_create(struct device_node *dn)
{
	struct powernv_rng *rng;
	struct resource res;
	unsigned long val;

	rng = kzalloc(sizeof(*rng), GFP_KERNEL);
	if (!rng)
		return -ENOMEM;

	if (of_address_to_resource(dn, 0, &res)) {
		kfree(rng);
		return -ENXIO;
	}

	rng->regs_real = (void __iomem *)res.start;

	rng->regs = of_iomap(dn, 0);
	if (!rng->regs) {
		kfree(rng);
		return -ENXIO;
	}

	val = in_be64(rng->regs);
	rng->mask = val;

	rng_init_per_cpu(rng, dn);

	ppc_md.get_random_seed = powernv_get_random_long;

	return 0;
}

void __init powernv_rng_init(void)
{
	struct device_node *dn;
	struct resource res;

	/* Prefer darn over the rest. */
	if (!initialize_darn())
		return;

	dn = of_find_compatible_node(NULL, NULL, "ibm,power-rng");
	if (!dn)
		return;
	if (of_address_to_resource(dn, 0, &res))
		return;
	early_state.rng.regs_real = (void __iomem *)res.start;
	early_state.rng.regs = of_iomap(dn, 0);
	if (!early_state.rng.regs)
		return;
	early_state.rng.mask = in_be64(early_state.rng.regs);
	ppc_md.get_random_seed = powernv_get_random_long_early;
}

static __init int powernv_rng_late_init(void)
{
	struct device_node *dn;

	/*
	 * If this didn't get initialized early on, then we're using darn,
	 * or this isn't available at all, so return early.
	 */
	if (ppc_md.get_random_seed != powernv_get_random_long_early)
		return 0;
	ppc_md.get_random_seed = NULL;

	for_each_compatible_node(dn, NULL, "ibm,power-rng") {
		if (rng_create(dn))
			continue;
		/* Create devices for hwrng driver */
		of_platform_device_create(dn, NULL, NULL);
	}
	return 0;
}
machine_subsys_initcall(powernv, powernv_rng_late_init);
