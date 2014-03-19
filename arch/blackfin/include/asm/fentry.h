/*
 * Blackfin fentry code
 *
 * Copyright 2009 Analog Devices Inc.
 * Licensed under the GPL-2 or later.
 */

#ifndef _ASM_BFIN_FENTRY
#define _ASM_BFIN_FENTRY

#ifdef CONFIG_FENTRY_RECORD_LIB

#define MCOUNT_INSN_SIZE	6 /* sizeof "[++sp] = rets; call __mcount;" */

#ifndef __ASSEMBLY__

struct fentry_arch {
	/* No extra data needed for Blackfin */
};

extern void _mcount(void);
#define MCOUNT_ADDR ((unsigned long)_mcount)

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

#endif /* !__ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_BFIN_FENTRY */
