#ifndef _ASM_IA64_FENTRY_H
#define _ASM_IA64_FENTRY_H

#define MCOUNT_INSN_SIZE        32 /* sizeof mcount call */

#ifdef CONFIG_FENTRY_RECORD_LIB

#ifndef __ASSEMBLY__

extern void _mcount(unsigned long pfs, unsigned long r1, unsigned long b0, unsigned long r0);
#define mcount _mcount

/* In IA64, MCOUNT_ADDR is set in link time, so it's not a constant at compile time */
#define MCOUNT_ADDR (((struct fnptr *)mcount)->ip)

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	/* second bundle, insn 2 */
	return addr - 0x12;
}

struct fentry_arch {
};

#endif /* !__ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_IA64_FENTRY_H */
