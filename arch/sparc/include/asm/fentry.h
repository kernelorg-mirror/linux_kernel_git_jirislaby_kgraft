#ifndef _ASM_SPARC64_FENTRY
#define _ASM_SPARC64_FENTRY

#ifdef CONFIG_FENTRY_RECORD_LIB

#define MCOUNT_ADDR		((long)(_mcount))
#define MCOUNT_INSN_SIZE	4 /* sizeof mcount call */

#ifndef __ASSEMBLY__

struct fentry_arch {
};

extern void _mcount(void);

/* relocation of fentry call site is the same as the address */
static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_SPARC64_FENTRY */
