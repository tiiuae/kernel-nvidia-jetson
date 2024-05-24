
#include <linux/kernel.h>
#include <nvhe/mem_protect.h>
#include <hyp/hyp_print.h>

int print_mappings(u32 id, u64 addr, u64 size);

int init_dbg(u64 pfn, u64 size)
{
	int ret;
	u64 host_addr = hyp_pfn_to_phys(pfn);
	u64 hyp_addr = (u64)__hyp_va(host_addr);

	ret = hyp_pin_shared_mem((void*) hyp_addr, (void *)(hyp_addr + size));
	if (ret)
		return ret;

	dbg_buffer = (struct dgb_buf *) hyp_addr;
	dbg_buffer->size = size ;
	dbg_buffer->datalen = 0;
	return 0;
}

int deinit_dbg(void)
{
	u64 to = (u64) dbg_buffer + dbg_buffer->size;

	hyp_unpin_shared_mem(dbg_buffer, (void *)to);
	dbg_buffer = 0;

	return 0;
}

int hyp_dbg(u64 cmd, u64 param1, u64 param2, u64 param3, u64 param4)
{
	hyp_print("hyp_dbg %llx\n", cmd);

	switch(cmd) {
		case 0:
			init_dbg(param1, param2);
			break;
		case 1:
			deinit_dbg();
			break;
		case 2:
			print_mappings(param1, param2, param3);
			hyp_print("dlen %lld\n",dbg_buffer->datalen);
			break;
		}

	return 0;
}


