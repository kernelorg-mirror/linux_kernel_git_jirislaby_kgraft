#ifndef _ASM_S390_FENTRY_H
#define _ASM_S390_FENTRY_H

#ifdef CONFIG_64BIT
#define MCOUNT_INSN_SIZE  12
#else
#define MCOUNT_INSN_SIZE  22
#endif

#ifdef CONFIG_FENTRY_RECORD_LIB

#ifndef __ASSEMBLY__

extern void _mcount(void);

struct fentry_arch { };

#define MCOUNT_ADDR ((long)_mcount)

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

static inline void fentry_put_BUG(void)
{
	/* to be implemented */
}

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_S390_FENTRY_H */
