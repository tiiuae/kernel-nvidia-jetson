# SPDX-License-Identifier: GPL-2.0
lib-objs := clear_page.o copy_page.o memcpy.o memset.o
lib-objs := $(addprefix ../../../lib/, $(lib-objs))

hyp-obj-y := timer-sr.o sysreg-sr.o debug-sr.o switch.o tlb.o hyp-init.o host.o \
	 hyp-main.o hyp-smp.o psci-relay.o early_alloc.o page_alloc.o \
	 cache.o setup.o mm.o mem_protect.o sys_regs.o pkvm.o stacktrace.o ffa.o iommu.o \
	 serial.o
hyp-obj-y += ../vgic-v3-sr.o ../aarch32.o ../vgic-v2-cpuif-proxy.o ../entry.o \
	../fpsimd.o ../hyp-entry.o ../exception.o ../pgtable.o
ifeq ($(CONFIG_KVM_ARM_HYP_DEBUG_UART),y)
hyp-obj-y += debug/print.o debug/subr_prf.o debug/strlen.o
endif
ifeq ($(CONFIG_KVM_ARM_HYP_DEBUG_HYP_CALLS),y)
hyp-obj-y += debug/gdb.o debug/hyp-dbg.o debug/parse_attr.o  debug/print_mappings.o debug/count_shared.o
endif

hyp-obj-$(CONFIG_TRACING) += clock.o events.o trace.o
hyp-obj-$(CONFIG_DEBUG_LIST) += list_debug.o
hyp-obj-$(CONFIG_MODULES) += modules.o
hyp-obj-y += $(lib-objs)

$(obj)/hyp.lds: $(src)/hyp.lds.S FORCE
	$(call if_changed_dep,cpp_lds_S)

include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.nvhe
obj-y := kvm_nvhe.o
KCSAN_SANITIZE := n

ifeq ($CONFIG_KVM_ARM_HYP_DEBUG_HYP_CALLS,y)
extra-y += debug/dummy.o debug/el2.bin
LDFLAGS_el2.bin := -T
$(obj)/debug/el2.bin: $(obj)/debug/hyp-dbg.lds $(addprefix $(obj)/,$(hyp-obj) ) $(obj)/debug/dummy.o FORCE
	$(call if_changed,ld)
endif
