#ifndef LINUX_FENTRY_H
#define LINUX_FENTRY_H

#include <asm/fentry.h>

#ifdef CONFIG_FENTRY_RECORD_LIB

/*
 * The fentry record's flags field is split into two parts.
 * the first part which is '0-FENTRY_REF_MAX' is a counter of
 * the number of callbacks that have registered the function that
 * the fentry descriptor represents.
 *
 * The second part is a mask:
 *  ENABLED - the function is being ftraced
 *  REGS    - the record wants the function to save regs
 *  REGS_EN - the function is set up to save regs.
 *
 * When a new ftrace_ops is registered and wants a function to save
 * pt_regs, the rec->flag REGS is set. When the function has been
 * set up to save regs, the REG_EN flag is set. Once a function
 * starts saving regs it will do so until all ftrace_ops are removed
 * from tracing that function.
 */
enum {
	FENTRY_FL_ENABLED	= (1UL << 29),
	FENTRY_FL_REGS		= (1UL << 30),
	FENTRY_FL_REGS_EN	= (1UL << 31)
};

#define FENTRY_FL_MASK		(0x7UL << 29)
#define FENTRY_REF_MAX		((1UL << 29) - 1)

struct fentry {
	unsigned long		ip; /* address of fentry call-site */
	unsigned long		flags;
	struct fentry_arch	arch;
};

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* LINUX_FENTRY_H */
