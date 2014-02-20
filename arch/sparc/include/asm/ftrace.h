#ifndef _ASM_SPARC64_FTRACE
#define _ASM_SPARC64_FTRACE

#ifdef CONFIG_DYNAMIC_FTRACE
#define MCOUNT_ADDR		((long)(_mcount))
#define MCOUNT_INSN_SIZE	4 /* sizeof mcount call */

#ifndef __ASSEMBLY__
extern void _mcount(void);

/* reloction of mcount call site is the same as the address */
static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

struct fentry_arch {
};

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_DYNAMIC_FTRACE */

#endif /* _ASM_SPARC64_FTRACE */
