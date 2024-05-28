#define STAGE1          1
#define STAGE2          2

#include <linux/kernel.h>
#include <nvhe/mem_protect.h>

#define S1_PXN_SHIFT            53
#define S1_PXN                  (1UL << S1_PXN_SHIFT)

#define S1_UXN_SHIFT            54
#define S1_UXN                  (1UL << S1_UXN_SHIFT)

/* Stage 2 */
#define S2_XN_SHIFT             53
#define S2_XN_MASK              (0x3UL << S2_XN_SHIFT)
#define S2_EXEC_EL1EL0          (0x0UL << S2_XN_SHIFT)
#define S2_EXEC_EL0             (0x1UL << S2_XN_SHIFT)
#define S2_EXEC_NONE            (0x2UL << S2_XN_SHIFT)
#define S2_EXEC_EL1             (0x3UL << S2_XN_SHIFT)

#define S2AP_SHIFT              6
#define S2AP_MASK               (0x3UL << S2AP_SHIFT)
#define S2AP_NONE               (0 << S2AP_SHIFT)
#define S2AP_READ               (1UL << S2AP_SHIFT)
#define S2AP_WRITE              (2UL << S2AP_SHIFT)
#define S2AP_RW                 (3UL << S2AP_SHIFT)

/* Stage 2 MemAttr[3:2] */
#define S2_MEM_ATTR_SHIFT       2
#define S2_MEM_TYPE_SHIFT       (S2_MEM_ATTR_SHIFT + 2)
#define S1_AP_SHIFT             6
#define S1_AP_MASK              (0x3UL << S1_AP_SHIFT)

#define S1_AP_RW_N              0UL
#define S1_AP_RW_RW             (1UL << S1_AP_SHIFT)
#define S1_AP_RO_N              (2UL << S1_AP_SHIFT)
#define S1_AP_RO_RO             (3UL << S1_AP_SHIFT)

/* Stage 2 MemAttr[3:2] */
#define S2_MEM_ATTR_SHIFT       2
#define S2_MEM_TYPE_SHIFT       (S2_MEM_ATTR_SHIFT + 2)
#define S2_MEM_TYPE_MASK        (0x3 << S2_MEM_TYPE_SHIFT)
#define S2_DEVICE               (0x0 << S2_MEM_TYPE_SHIFT)

int hyp_snprint(char *s, size_t slen, const char *fmt, ...);

char *parse_attrs(char *p, uint64_t attrs, uint64_t stage)
{
	const char *pv_access = "R-";
	const char *upv_access = "--";
	const char *state = "";
	char pv_perm = '-';
	char upv_perm = '-';
	const char *mtype = "";

	if (p == 0) {
		if (stage == STAGE2)
			return "prv usr type";
		else
			return "prv usr";
	}

	if (stage == STAGE1) {
		pv_perm = (attrs & S1_PXN) ? '-' : 'X';
		upv_perm = (attrs & S1_UXN) ? '-' : 'X';

		switch (attrs & S1_AP_MASK) {
		case S1_AP_RW_N:
			pv_access = "RW";
			upv_access = "--";
			break;
		case S1_AP_RW_RW:
			pv_access = "RW";
			upv_access = "RW";
			if (pv_perm == 'X') {
				/* Not executable, because AArch64 execution
				 * treats all regions writable at EL0 as being PXN
				 */
				pv_perm = 'x';
			}
			break;
		case S1_AP_RO_N:
			pv_access = "R-";
			upv_access = "--";
			break;
		case S1_AP_RO_RO:
		pv_access = "R-";
			upv_access = "R-";
			break;
		}
	} else if (stage == STAGE2) {
		switch (attrs & S2_XN_MASK) {
		case S2_EXEC_EL1EL0:
			pv_perm =  'X';
			upv_perm = 'X';
			break;
		case S2_EXEC_EL0:
			pv_perm = '-';
			upv_perm = 'X';
			break;
		case S2_EXEC_NONE:
			pv_perm = '-';
			upv_perm = '-';
			break;
		case S2_EXEC_EL1:
			pv_perm = 'X';
			upv_perm = '-';
		}

		switch (attrs & S2AP_MASK) {
		case S2AP_NONE:
			pv_access = "--";
			upv_access = "--";
			break;
		case S2AP_READ:
			pv_access = "R-";
			upv_access = "R-";
			break;
		case S2AP_WRITE:
			pv_access = "-W";
			upv_access = "-W";
			break;
		case S2AP_RW:
			pv_access = "RW";
			upv_access = "RW";
			break;
		}

		mtype = ((attrs & S2_MEM_TYPE_MASK) == S2_DEVICE) ? "Device" :
				       "Normal";
	} else
		return "Unknown stage?";

	 switch (attrs & PKVM_PAGE_STATE_PROT_MASK) {
	 case PKVM_PAGE_SHARED_OWNED:
		state = "SHARED_OWNED   ";
		break;
	 case PKVM_PAGE_SHARED_BORROWED:
		state = "SHARED_BORROWED";
		break;
	 }

	hyp_snprint(p, 128, "%s%c %s%c %s %s", pv_access, pv_perm, upv_access,
		    upv_perm, mtype, state);
	return p;
}
