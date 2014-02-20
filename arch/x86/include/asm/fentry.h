#ifndef ASM_FENTRY_H
#define ASM_FENTRY_H

#ifdef CONFIG_FENTRY_RECORD_LIB

#ifdef CC_USING_FENTRY
# define fentry_hook	__fentry__
#else
# define fentry_hook	mcount
#endif

#define MCOUNT_ADDR		((long)(fentry_hook))
#define MCOUNT_INSN_SIZE	5 /* sizeof mcount call */

#ifndef __ASSEMBLY__

struct fentry_arch {
	/* No extra data needed for x86 */
};

extern void fentry_hook(void);

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	/*
	 * addr is the address of the fentry call instruction.
	 * recordmcount does the necessary offset calculation.
	 */
	return addr;
}

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* ASM_FENTRY_H */
