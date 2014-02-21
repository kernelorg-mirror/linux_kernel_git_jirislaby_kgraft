/*
 * Dynamic function tracer architecture backend.
 *
 * Copyright IBM Corp. 2009
 *
 *   Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>,
 *		Martin Schwidefsky <schwidefsky@de.ibm.com>
 */

#include <linux/hardirq.h>
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <linux/kernel.h>
#include <linux/fentry.h>
#include <linux/types.h>
#include <linux/kprobes.h>
#include <trace/syscall.h>
#include <asm/asm-offsets.h>
#include "entry.h"

#ifdef CONFIG_DYNAMIC_FTRACE

void ftrace_enable_insn(void);

#ifdef CONFIG_64BIT
asm(
	"	.align	4\n"
	"ftrace_enable_insn:\n"
	"	lg	%r1,"__stringify(__LC_FTRACE_FUNC)"\n");

#define FTRACE_INSN_SIZE	6

#else /* CONFIG_64BIT */
asm(
	"	.align	4\n"
	"ftrace_enable_insn:\n"
	"	l	%r14,"__stringify(__LC_FTRACE_FUNC)"\n");

#define FTRACE_INSN_SIZE	4

#endif /* CONFIG_64BIT */

int ftrace_make_call(struct fentry *rec, unsigned long addr)
{
	if (probe_kernel_write((void *) rec->ip, ftrace_enable_insn,
			       FTRACE_INSN_SIZE))
		return -EPERM;
	return 0;
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	return 0;
}

#endif /* CONFIG_DYNAMIC_FTRACE */

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
/*
 * Hook the return address and push it in the stack of return addresses
 * in current thread info.
 */
unsigned long __kprobes prepare_ftrace_return(unsigned long parent,
					      unsigned long ip)
{
	struct ftrace_graph_ent trace;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		goto out;
	ip = (ip & PSW_ADDR_INSN) - MCOUNT_INSN_SIZE;
	trace.func = ip;
	trace.depth = current->curr_ret_stack + 1;
	/* Only trace if the calling function expects to. */
	if (!ftrace_graph_entry(&trace))
		goto out;
	if (ftrace_push_return_trace(parent, ip, &trace.depth, 0) == -EBUSY)
		goto out;
	parent = (unsigned long) return_to_handler;
out:
	return parent;
}

#ifdef CONFIG_DYNAMIC_FTRACE
/*
 * Patch the kernel code at ftrace_graph_caller location. The instruction
 * there is branch relative and save to prepare_ftrace_return. To disable
 * the call to prepare_ftrace_return we patch the bras offset to point
 * directly after the instructions. To enable the call we calculate
 * the original offset to prepare_ftrace_return and put it back.
 */
int ftrace_enable_ftrace_graph_caller(void)
{
	unsigned short offset;

	offset = ((void *) prepare_ftrace_return -
		  (void *) ftrace_graph_caller) / 2;
	return probe_kernel_write((void *) ftrace_graph_caller + 2,
				  &offset, sizeof(offset));
}

int ftrace_disable_ftrace_graph_caller(void)
{
	static unsigned short offset = 0x0002;

	return probe_kernel_write((void *) ftrace_graph_caller + 2,
				  &offset, sizeof(offset));
}

#endif /* CONFIG_DYNAMIC_FTRACE */
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */
