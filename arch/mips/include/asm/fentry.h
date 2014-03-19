/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2009 DSLab, Lanzhou University, China
 * Author: Wu Zhangjin <wuzhangjin@gmail.com>
 */

#ifndef _ASM_MIPS_FENTRY_H
#define _ASM_MIPS_FENTRY_H

#define MCOUNT_INSN_SIZE 4		/* sizeof mcount call */

#ifdef CONFIG_FENTRY_RECORD_LIB

#ifndef __ASSEMBLY__

#define MCOUNT_ADDR ((unsigned long)(_mcount))

extern void _mcount(void);
#define mcount _mcount

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

struct fentry_arch {
};

#endif /* !__ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_MIPS_FENTRY_H */
