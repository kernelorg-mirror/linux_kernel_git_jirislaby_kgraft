#ifndef _ASM_ARM_FENTRY
#define _ASM_ARM_FENTRY

#define MCOUNT_INSN_SIZE	4 /* sizeof mcount call */

#ifdef CONFIG_FENTRY_RECORD_LIB

#ifndef __ASSEMBLY__

#define MCOUNT_ADDR		((unsigned long)(__gnu_mcount_nc))

struct fentry_arch {
#ifdef CONFIG_OLD_MCOUNT
	bool	old_mcount;
#endif
};

extern void __gnu_mcount_nc(void);

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	/* With Thumb-2, the recorded addresses have the lsb set */
	return addr & ~1;
}

#endif /* !__ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_ARM_FENTRY */
