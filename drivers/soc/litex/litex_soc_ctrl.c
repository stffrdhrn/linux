// SPDX-License-Identifier: GPL-2.0
/*
 * LiteX SoC Controller Driver
 *
 * Copyright (C) 2020 Antmicro <www.antmicro.com>
 *
 */

#include <linux/litex.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/io.h>

/*
 * The parameters below are true for LiteX SoC
 * configured for 8-bit CSR Bus, 32-bit aligned.
 *
 * Supporting other configurations will require
 * extending the logic in this header.
 */
#define LITEX_REG_SIZE             0x4
#define LITEX_SUBREG_SIZE          0x1
#define LITEX_SUBREG_SIZE_BIT      (LITEX_SUBREG_SIZE * 8)

static DEFINE_SPINLOCK(csr_lock);

/*
 * LiteX SoC Generator, depending on the configuration,
 * can split a single logical CSR (Control & Status Register)
 * into a series of consecutive physical registers.
 *
 * For example, in the configuration with 8-bit CSR Bus,
 * 32-bit aligned (the default one for 32-bit CPUs) a 32-bit
 * logical CSR will be generated as four 32-bit physical registers,
 * each one containing one byte of meaningful data.
 *
 * For details see: https://github.com/enjoy-digital/litex/wiki/CSR-Bus
 *
 * The purpose of `litex_set_reg`/`litex_get_reg` is to implement
 * the logic of writing to/reading from the LiteX CSR in a single
 * place that can be then reused by all LiteX drivers.
 */
void litex_set_reg(void __iomem *reg, unsigned long reg_size,
		    unsigned long val)
{
	unsigned long shifted_data, shift, i;
	unsigned long flags;

	spin_lock_irqsave(&csr_lock, flags);

	for (i = 0; i < reg_size; ++i) {
		shift = ((reg_size - i - 1) * LITEX_SUBREG_SIZE_BIT);
		shifted_data = val >> shift;

		writel((u32 __force)cpu_to_le32(shifted_data), reg + (LITEX_REG_SIZE * i));
	}

	spin_unlock_irqrestore(&csr_lock, flags);
}
EXPORT_SYMBOL_GPL(litex_set_reg);

unsigned long litex_get_reg(void __iomem *reg, unsigned long reg_size)
{
	unsigned long shifted_data, shift, i;
	unsigned long result = 0;
	unsigned long flags;

	spin_lock_irqsave(&csr_lock, flags);

	for (i = 0; i < reg_size; ++i) {
		shifted_data = le32_to_cpu((__le32 __force)readl(reg + (LITEX_REG_SIZE * i)));

		shift = ((reg_size - i - 1) * LITEX_SUBREG_SIZE_BIT);
		result |= (shifted_data << shift);
	}

	spin_unlock_irqrestore(&csr_lock, flags);

	return result;
}
EXPORT_SYMBOL_GPL(litex_get_reg);

#define SCRATCH_REG_OFF         0x04
#define SCRATCH_REG_SIZE        4
#define SCRATCH_REG_VALUE       0x12345678
#define SCRATCH_TEST_VALUE      0xdeadbeef

/*
 * Check LiteX CSR read/write access
 *
 * This function reads and writes a scratch register in order
 * to verify if CSR access works.
 *
 * In case any problems are detected, the driver should panic.
 *
 * Access to the LiteX CSR is, by design, done in CPU native
 * endianness. The driver should not dynamically configure
 * access functions when the endianness mismatch is detected.
 * Such situation indicates problems in the soft SoC design
 * and should be solved at the LiteX generator level,
 * not in the software.
 */
static int litex_check_csr_access(void __iomem *reg_addr)
{
	unsigned long reg;

	reg = litex_get_reg(reg_addr + SCRATCH_REG_OFF, SCRATCH_REG_SIZE);

	if (reg != SCRATCH_REG_VALUE) {
		panic("Scratch register read error! Expected: 0x%x but got: 0x%lx",
			SCRATCH_REG_VALUE, reg);
		return -EINVAL;
	}

	litex_set_reg(reg_addr + SCRATCH_REG_OFF,
		SCRATCH_REG_SIZE, SCRATCH_TEST_VALUE);
	reg = litex_get_reg(reg_addr + SCRATCH_REG_OFF, SCRATCH_REG_SIZE);

	if (reg != SCRATCH_TEST_VALUE) {
		panic("Scratch register write error! Expected: 0x%x but got: 0x%lx",
			SCRATCH_TEST_VALUE, reg);
		return -EINVAL;
	}

	/* restore original value of the SCRATCH register */
	litex_set_reg(reg_addr + SCRATCH_REG_OFF,
		SCRATCH_REG_SIZE, SCRATCH_REG_VALUE);

	/* Set flag for other drivers */
	pr_info("LiteX SoC Controller driver initialized");

	return 0;
}

struct litex_soc_ctrl_device {
	void __iomem *base;
};

static const struct of_device_id litex_soc_ctrl_of_match[] = {
	{.compatible = "litex,soc-controller"},
	{},
};

MODULE_DEVICE_TABLE(of, litex_soc_ctrl_of_match);

static int litex_soc_ctrl_probe(struct platform_device *pdev)
{
	int result;
	struct device *dev;
	struct device_node *node;
	struct litex_soc_ctrl_device *soc_ctrl_dev;

	dev = &pdev->dev;
	node = dev->of_node;
	if (!node)
		return -ENODEV;

	soc_ctrl_dev = devm_kzalloc(dev, sizeof(*soc_ctrl_dev), GFP_KERNEL);
	if (!soc_ctrl_dev)
		return -ENOMEM;

	soc_ctrl_dev->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(soc_ctrl_dev->base))
		return PTR_ERR(soc_ctrl_dev->base);

	result = litex_check_csr_access(soc_ctrl_dev->base);
	if (result) {
		// LiteX CSRs access is broken which means that
		// none of LiteX drivers will most probably
		// operate correctly
		BUG();
	}

	return 0;
}

static struct platform_driver litex_soc_ctrl_driver = {
	.driver = {
		.name = "litex-soc-controller",
		.of_match_table = of_match_ptr(litex_soc_ctrl_of_match)
	},
	.probe = litex_soc_ctrl_probe,
};

module_platform_driver(litex_soc_ctrl_driver);
MODULE_DESCRIPTION("LiteX SoC Controller driver");
MODULE_AUTHOR("Antmicro <www.antmicro.com>");
MODULE_LICENSE("GPL v2");
