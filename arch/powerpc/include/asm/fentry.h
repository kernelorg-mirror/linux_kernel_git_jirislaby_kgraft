#ifndef _ASM_POWERPC_FENTRY
#define _ASM_POWERPC_FENTRY

#define MCOUNT_INSN_SIZE	4 /* sizeof mcount call */

#ifdef CONFIG_FENTRY_RECORD_LIB

#ifndef __ASSEMBLY__

#include <linux/module.h>

#define MCOUNT_ADDR		((long)(_mcount))

struct fentry_arch {
	struct module *mod;
};

extern void _mcount(void);

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
       /* relocation of fentry call site is the same as the address */
       return addr;
}

#endif /* !__ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_POWERPC_FENTRY */
