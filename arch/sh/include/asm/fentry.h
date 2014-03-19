#ifndef __ASM_SH_FENTRY_H
#define __ASM_SH_FENTRY_H

#ifdef CONFIG_FENTRY_RECORD_LIB

#define MCOUNT_INSN_SIZE	4 /* sizeof mcount call */

#ifndef __ASSEMBLY__

extern void mcount(void);

#define MCOUNT_ADDR		((long)(mcount))

struct fentry_arch {
	/* No extra data needed on sh */
};

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	/* 'addr' is the memory table address. */
	return addr;
}

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* __ASM_SH_FENTRY_H */
