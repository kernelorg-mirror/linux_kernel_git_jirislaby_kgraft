#ifndef LINUX_FENTRY_H
#define LINUX_FENTRY_H

#include <linux/module.h>
#include <linux/mutex.h>

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

struct fentry_page {
	struct fentry_page	*next;
	struct fentry		*records;
	int			index;
	int			size;
};

extern struct mutex fentry_lock;
extern struct fentry_page *fentry_pages_start;
extern unsigned long fentry_count;
extern bool fentry_enabled;

#define for_each_fentry(pg, rec, i)						\
	for (pg = fentry_pages_start, i = 0, rec = &pg->records[i]; pg;		\
			({ if (++i >= pg->index) {				\
				pg = pg->next;					\
				i = 0;						\
			}							\
			if (pg)							\
				rec = &pg->records[i]; }))

/**
 * fentry_make_nop - convert code into nop
 * @mod: module structure if called by module load initialization
 * @rec: the fentry call site record
 * @addr: the address that the call site should be calling
 *
 * This is a very sensitive operation and great care needs
 * to be taken by the arch.  The operation should carefully
 * read the location, check to see if what is read is indeed
 * what we expect it to be, and then on success of the compare,
 * it should write to the location.
 *
 * The code segment at @rec->ip should be a caller to @addr
 *
 * Return must be:
 *  0 on success
 *  -EFAULT on error reading the location
 *  -EINVAL on a failed compare of the contents
 *  -EPERM  on error writing to the location
 * Any other value will be considered a failure.
 */
extern int fentry_make_nop(struct module *mod,
			   struct fentry *rec, unsigned long addr);

extern void fentry_put_BUG(void);
extern void fentry_init(void);
extern int fentry_dyn_arch_init(void);

static inline bool fentry_fine(void)
{
	return fentry_enabled;
}

#else /* CONFIG_FENTRY_RECORD_LIB */

static inline void fentry_init(void) { }

#endif /* CONFIG_FENTRY_RECORD_LIB */

#endif /* LINUX_FENTRY_H */
