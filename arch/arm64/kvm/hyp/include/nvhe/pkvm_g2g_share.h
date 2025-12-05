/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef ARCH_ARM64_KVM_HYP_INCLUDE_NVHE_PKVM_G2G_SHARE_H_
#define ARCH_ARM64_KVM_HYP_INCLUDE_NVHE_PKVM_G2G_SHARE_H_

extern phys_addr_t g2g_share_base;
extern phys_addr_t g2g_share_size;

bool pkvm_g2g_share(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code);
bool pkvm_g2g_share_query(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code);
bool pkvm_g2g_unshare(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code);
void pkvm_g2g_share_teardown(pkvm_handle_t handle);

#endif /* ARCH_ARM64_KVM_HYP_INCLUDE_NVHE_PKVM_G2G_SHARE_H_ */
