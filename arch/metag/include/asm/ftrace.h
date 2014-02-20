#ifndef _ASM_METAG_FTRACE
#define _ASM_METAG_FTRACE

#ifdef CONFIG_FUNCTION_TRACER
#define MCOUNT_INSN_SIZE	8 /* sizeof mcount call */

#ifndef __ASSEMBLY__
extern void mcount_wrapper(void);
#define MCOUNT_ADDR		((long)(mcount_wrapper))

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

struct fentry_arch {
	/* No extra data needed on metag */
};
#endif /* __ASSEMBLY__ */

#endif /* CONFIG_FUNCTION_TRACER */

#endif /* _ASM_METAG_FTRACE */
