/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ARM64_KVM_HYP_HYp_DEBUG_H__
#define __ARM64_KVM_HYP_HYP_DEBUG_H__
#include <asm/kvm_host.h>


struct dgb_buf {
	u64 size;
	u64 datalen;
	u8 data[];
};
extern struct dgb_buf *dbg_buffer;

/**
 *gdb helper function on hypervisor. Jump to EL2 and waits
 *    on the spin lock until gdb is stopped.
 *
 *@param irrelevant for now
 *@return 0
 *
 */
int attach_gdb(u64 param);

/**
 * do debug function on hypervisor
 *
 * @cmm function id to be executed
 * @param1 the 1st parameter of the function
 * @param2 the 2nd parameter of the function
 * @param3 the 3rd parameter of the function
 * @param4 the 4th parameter of the function
 *
 * @return retutn value of the function
 */

int hyp_dbg(u64 cmd, u64 param1, u64 param2, u64 param3, u64 param4);

/**
 * Print and count the amount of guest/hypervisor ram visible to the host
 *
 * @param handle guest to query. if 0 do query for hypervisor
 * @param size the size of query
 * @param lock make the pages read only for the guest
 * @return int count of shared pages or -errno
 */
int print_mappings(u32 id, u64 addr, u64 size);

/**
 * Initialise shared RAM for hyp-debuggergit
 *
 *  @param pfn the start page of shared memory
 *  @param size the size of the sharedf memory
 *  @return int count of shared pages or -errno
 */
int count_shared(pkvm_handle_t handle, u64 size, u64 lock);

#endif
