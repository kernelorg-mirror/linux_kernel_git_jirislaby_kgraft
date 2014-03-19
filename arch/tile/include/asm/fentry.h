/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_FENTRY_H
#define _ASM_TILE_FENTRY_H

#ifdef CONFIG_FENTRY_RECORD_LIB

#define MCOUNT_ADDR ((unsigned long)(__mcount))
#define MCOUNT_INSN_SIZE 8		/* sizeof mcount call */

#ifndef __ASSEMBLY__

extern void __mcount(void);

static inline unsigned long fentry_call_adjust(unsigned long addr)
{
	return addr;
}

struct fentry_arch {
};

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* _ASM_TILE_FENTRY_H */
