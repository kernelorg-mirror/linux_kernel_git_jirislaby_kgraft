#ifndef _ASM_MICROBLAZE_FTRACE
#define _ASM_MICROBLAZE_FTRACE

#ifdef CONFIG_FUNCTION_TRACER

#define MCOUNT_ADDR		((long)(_mcount))
#define MCOUNT_INSN_SIZE	8 /* sizeof mcount call */

#ifndef __ASSEMBLY__
extern void _mcount(void);
extern void ftrace_call_graph(void);
#endif

#ifdef CONFIG_DYNAMIC_FTRACE
/* reloction of mcount call site is the same as the address */
static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

struct fentry_arch {
};
#endif /* CONFIG_DYNAMIC_FTRACE */

#endif /* CONFIG_FUNCTION_TRACER */
#endif /* _ASM_MICROBLAZE_FTRACE */
