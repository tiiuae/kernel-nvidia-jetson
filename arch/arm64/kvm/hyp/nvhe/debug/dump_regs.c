// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <nvhe/dump_regs.h>

#include <hyp/hyp_print.h>
#include <nvhe/chacha.h>
#include <asm/barrier.h>
#include <asm/page-def.h>
#include <nvhe/mm.h>

void debug_dump_csrs(void)
{
	hyp_ramlog_reg(TTBR0_EL2);
	hyp_ramlog_reg(TTBR0_EL1);
	hyp_ramlog_reg(TTBR1_EL1);
	hyp_ramlog_reg(ESR_EL2);
	hyp_ramlog_reg(HPFAR_EL2);
	hyp_ramlog_reg(FAR_EL2);
	hyp_ramlog_reg(VTTBR_EL2);
}
