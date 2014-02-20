#ifndef _ASM_S390_FTRACE_H
#define _ASM_S390_FTRACE_H

#ifndef __ASSEMBLY__

extern void _mcount(void);

struct fentry_arch { };

#define MCOUNT_ADDR ((long)_mcount)


static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

#endif /* __ASSEMBLY__ */

#ifdef CONFIG_64BIT
#define MCOUNT_INSN_SIZE  12
#else
#define MCOUNT_INSN_SIZE  22
#endif

#endif /* _ASM_S390_FTRACE_H */
