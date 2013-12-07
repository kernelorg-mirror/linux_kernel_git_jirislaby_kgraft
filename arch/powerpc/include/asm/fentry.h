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

struct fentry;

extern void _mcount(void);
extern int fentry_make_nop(struct module *mod, struct fentry *rec,
		unsigned long addr);
extern unsigned int fentry_call_replace(unsigned long ip, unsigned long addr,
		int link);
extern int fentry_modify_code(unsigned long ip, unsigned int old,
		unsigned int new);
extern int test_24bit_addr(unsigned long ip, unsigned long addr);

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
       /* relocation of fentry call site is the same as the address */
       return addr;
}

static inline void fentry_put_BUG(void)
{
	/* to be implemented */
}

#endif /* !__ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_POWERPC_FENTRY */
