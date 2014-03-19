/*
 * arch/xtensa/include/asm/fentry.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2013 Tensilica Inc.
 */

#ifndef _XTENSA_FENTRY_H
#define _XTENSA_FENTRY_H

#ifdef CONFIG_FENTRY_RECORD_LIB

#define MCOUNT_ADDR ((unsigned long)(_mcount))
#define MCOUNT_INSN_SIZE 3

#ifndef __ASSEMBLY__
extern void _mcount(void);
#define mcount _mcount
#endif /* __ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _XTENSA_FENTRY_H */
