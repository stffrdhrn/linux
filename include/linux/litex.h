/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Common LiteX header providing
 * helper functions for accessing CSRs.
 *
 * Implementation of the functions is provided by
 * the LiteX SoC Controller driver.
 *
 * Copyright (C) 2019-2020 Antmicro <www.antmicro.com>
 */

#ifndef _LINUX_LITEX_H
#define _LINUX_LITEX_H

#include <linux/io.h>
#include <linux/types.h>
#include <linux/compiler_types.h>

void litex_set_reg(void __iomem *reg, unsigned long reg_sz, unsigned long val);

unsigned long litex_get_reg(void __iomem *reg, unsigned long reg_sz);


#endif /* _LINUX_LITEX_H */
