// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <nvhe/mem_protect.h>
#include <nvhe/ramlog.h>
#include <hyp/hyp_print.h>
#include <hyp/hyp_debug.h>

struct dgb_buf *dbg_buffer;

int init_dbg(u64 pfn, u64 size)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 hyp_addr = (u64)__hyp_va(host_addr);

	hyp_print("shared_mem %llx size %llx\n", hyp_addr, size);
	ret = hyp_pin_shared_mem((void *) hyp_addr, (void *)(hyp_addr + size));
	if (ret)
		return ret;

	dbg_buffer = (struct dgb_buf *) hyp_addr;
	dbg_buffer->size = size;
	dbg_buffer->datalen = 0;
	return 0;
}

/**
 *  Uninitialise shared RAM for hyp-debugger
 *  @param pfn the start page of shared memory
 *  @return int 0
 */
int deinit_dbg(void)
{
	u64 to = (u64) dbg_buffer + dbg_buffer->size;

	hyp_unpin_shared_mem(dbg_buffer, (void *)to);
	dbg_buffer = 0;

	return 0;
}

int hyp_dbg(u64 cmd, u64 param1, u64 param2, u64 param3, u64 param4)
{
	hyp_print("hyp_cmd  (%llx) %llx %llx %llx %llx\n", cmd, param1, param2,
			param3, param4);

	switch (cmd) {
	case 0:
		init_dbg(param1, param2);
		break;
	case 1:
		deinit_dbg();
		break;
	case 2:
		print_mappings(param1, param2, param3);
		break;
	case 3:
		count_shared(param1, param2, param3);
		break;
	case 4:
		output_rlog(param1);
	}

	return 0;
}


