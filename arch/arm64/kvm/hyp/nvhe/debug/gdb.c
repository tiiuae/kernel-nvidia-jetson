
#include <linux/kernel.h>
#include <nvhe/spinlock.h>
#include <hyp/hyp_print.h>
#pragma GCC push_options
#pragma GCC optimize ("Og")
#include <nvhe/mem_protect.h>

static DEFINE_HYP_SPINLOCK(dbg_lock);

void init_gdb(void)
{

	const char path[] = "../../linux-host/arch/arm64/kvm/hyp/nvhe/debug/el2.bin";

	hyp_print("\nWelcome to pkvm debugging by gdb\n\n");
	hyp_print("Hit ^C to gdb window, and enter following  commands:\n\n");
	hyp_print("symbol-file\n");
	hyp_print("add-symbol-file %s -s .bss 0x%llx -s .text 0x%llx\n",
		   path,
		   __hyp_bss_start,
		   ((u64)__hyp_text_start + 0x2000) & ~0xfff);
	hyp_print("symbol-file ../../linux-host/vmlinux\n");
	hyp_print("\nthen you can set breakpoints to EL2\n");

	hyp_print("check that the current thread is in the spin_lock\n");
	hyp_print("i threads\n\n");

	hyp_print("to continue, enter commands:\n");
	hyp_print("p openlock(0)\n");
	hyp_print("cont\n");
	hyp_print("cont\n");

	hyp_spin_lock(&dbg_lock);
	hyp_print("continuing..\n");

}

int openlock(void)
{
	hyp_spin_unlock(&dbg_lock);
	hyp_print("lock open\n");
	return 0;

}

int __attach_gdb(u64 param)
{
	if (!hyp_spin_is_locked(&dbg_lock))
		hyp_spin_lock(&dbg_lock);
	init_gdb();

	return 0;
}

#pragma GCC pop_options


