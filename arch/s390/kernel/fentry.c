/*
 * Function entry architecture backend.
 *
 * Copyright IBM Corp. 2009
 *
 *   Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>,
 *		Martin Schwidefsky <schwidefsky@de.ibm.com>
 */

#include <linux/fentry.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

void ftrace_disable_code(void);

#ifdef CONFIG_64BIT
/*
 * The 64-bit mcount code looks like this:
 *	stg	%r14,8(%r15)		# offset 0
 * >	larl	%r1,<&counter>		# offset 6
 * >	brasl	%r14,_mcount		# offset 12
 *	lg	%r14,8(%r15)		# offset 18
 * Total length is 24 bytes. The middle two instructions of the mcount
 * block get overwritten by ftrace_make_nop / ftrace_make_call.
 * The 64-bit enabled ftrace code block looks like this:
 *	stg	%r14,8(%r15)		# offset 0
 * >	lg	%r1,__LC_FTRACE_FUNC	# offset 6
 * >	lgr	%r0,%r0			# offset 12
 * >	basr	%r14,%r1		# offset 16
 *	lg	%r14,8(%15)		# offset 18
 * The return points of the mcount/ftrace function have the same offset 18.
 * The 64-bit disable ftrace code block looks like this:
 *	stg	%r14,8(%r15)		# offset 0
 * >	jg	.+18			# offset 6
 * >	lgr	%r0,%r0			# offset 12
 * >	basr	%r14,%r1		# offset 16
 *	lg	%r14,8(%15)		# offset 18
 * The jg instruction branches to offset 24 to skip as many instructions
 * as possible.
 */
asm(
	"	.align	4\n"
	"ftrace_disable_code:\n"
	"	jg	0f\n"
	"	lgr	%r0,%r0\n"
	"	basr	%r14,%r1\n"
	"0:\n");

#else /* CONFIG_64BIT */
/*
 * The 31-bit mcount code looks like this:
 *	st	%r14,4(%r15)		# offset 0
 * >	bras	%r1,0f			# offset 4
 * >	.long	_mcount			# offset 8
 * >	.long	<&counter>		# offset 12
 * > 0:	l	%r14,0(%r1)		# offset 16
 * >	l	%r1,4(%r1)		# offset 20
 *	basr	%r14,%r14		# offset 24
 *	l	%r14,4(%r15)		# offset 26
 * Total length is 30 bytes. The twenty bytes starting from offset 4
 * to offset 24 get overwritten by ftrace_make_nop / ftrace_make_call.
 * The 31-bit enabled ftrace code block looks like this:
 *	st	%r14,4(%r15)		# offset 0
 * >	l	%r14,__LC_FTRACE_FUNC	# offset 4
 * >	j	0f			# offset 8
 * >	.fill	12,1,0x07		# offset 12
 *   0:	basr	%r14,%r14		# offset 24
 *	l	%r14,4(%r14)		# offset 26
 * The return points of the mcount/ftrace function have the same offset 26.
 * The 31-bit disabled ftrace code block looks like this:
 *	st	%r14,4(%r15)		# offset 0
 * >	j	.+26			# offset 4
 * >	j	0f			# offset 8
 * >	.fill	12,1,0x07		# offset 12
 *   0:	basr	%r14,%r14		# offset 24
 *	l	%r14,4(%r14)		# offset 26
 * The j instruction branches to offset 30 to skip as many instructions
 * as possible.
 */
asm(
	"	.align	4\n"
	"ftrace_disable_code:\n"
	"	j	1f\n"
	"	j	0f\n"
	"	.fill	12,1,0x07\n"
	"0:	basr	%r14,%r14\n"
	"1:\n");

#endif /* CONFIG_64BIT */

int fentry_make_nop(struct module *mod, struct fentry *rec,
		    unsigned long addr)
{
	if (probe_kernel_write((void *) rec->ip, ftrace_disable_code,
			       MCOUNT_INSN_SIZE))
		return -EPERM;
	return 0;
}

int __init fentry_dyn_arch_init(void)
{
	return 0;
}
