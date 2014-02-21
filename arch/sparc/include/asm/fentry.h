#ifndef _ASM_SPARC64_FENTRY
#define _ASM_SPARC64_FENTRY

#ifdef CONFIG_FENTRY_RECORD_LIB

#define MCOUNT_ADDR		((long)(_mcount))
#define MCOUNT_INSN_SIZE	4 /* sizeof mcount call */

#ifndef __ASSEMBLY__

struct fentry_arch {
};

extern void _mcount(void);
extern int fentry_modify_code(unsigned long ip, u32 old, u32 new);

/* relocation of fentry call site is the same as the address */
static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

static inline u32 fentry_call_replace(unsigned long ip, unsigned long addr)
{
	u32 call;
	s32 off;

	off = ((s32)addr - (s32)ip);
	call = 0x40000000 | ((u32)off >> 2);

	return call;
}

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_SPARC64_FENTRY */
