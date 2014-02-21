/*
 * Code for replacing ftrace calls with jumps.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 *
 * Thanks goes out to P.A. Semi, Inc for supplying me with a PPC64 box.
 *
 * Added function graph tracer code, taken from x86 that was written
 * by Frederic Weisbecker, and ported to PPC by Steven Rostedt.
 *
 */

#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/uaccess.h>
#include <linux/fentry.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/list.h>

#include <asm/cacheflush.h>
#include <asm/code-patching.h>
#include <asm/ftrace.h>
#include <asm/syscall.h>


#ifdef CONFIG_DYNAMIC_FTRACE
#ifdef CONFIG_MODULES
#ifdef CONFIG_PPC64
static int
__ftrace_make_call(struct fentry *rec, unsigned long addr)
{
	unsigned int op[2];
	unsigned long ip = rec->ip;

	/* read where this goes */
	if (probe_kernel_read(op, (void *)ip, MCOUNT_INSN_SIZE * 2))
		return -EFAULT;

	/*
	 * It should be pointing to two nops or
	 *  b +8; ld r2,40(r1)
	 */
	if (((op[0] != 0x48000008) || (op[1] != 0xe8410028)) &&
	    ((op[0] != PPC_INST_NOP) || (op[1] != PPC_INST_NOP))) {
		printk(KERN_ERR "Expected NOPs but have %x %x\n", op[0], op[1]);
		return -EINVAL;
	}

	/* If we never set up a trampoline to ftrace_caller, then bail */
	if (!rec->arch.mod->arch.tramp) {
		printk(KERN_ERR "No ftrace trampoline\n");
		return -EINVAL;
	}

	/* create the branch to the trampoline */
	op[0] = create_branch((unsigned int *)ip,
			      rec->arch.mod->arch.tramp, BRANCH_SET_LINK);
	if (!op[0]) {
		printk(KERN_ERR "REL24 out of range!\n");
		return -EINVAL;
	}

	/* ld r2,40(r1) */
	op[1] = 0xe8410028;

	pr_devel("write to %lx\n", rec->ip);

	if (probe_kernel_write((void *)ip, op, MCOUNT_INSN_SIZE * 2))
		return -EPERM;

	flush_icache_range(ip, ip + 8);

	return 0;
}
#else
static int
__ftrace_make_call(struct fentry *rec, unsigned long addr)
{
	unsigned int op;
	unsigned long ip = rec->ip;

	/* read where this goes */
	if (probe_kernel_read(&op, (void *)ip, MCOUNT_INSN_SIZE))
		return -EFAULT;

	/* It should be pointing to a nop */
	if (op != PPC_INST_NOP) {
		printk(KERN_ERR "Expected NOP but have %x\n", op);
		return -EINVAL;
	}

	/* If we never set up a trampoline to ftrace_caller, then bail */
	if (!rec->arch.mod->arch.tramp) {
		printk(KERN_ERR "No ftrace trampoline\n");
		return -EINVAL;
	}

	/* create the branch to the trampoline */
	op = create_branch((unsigned int *)ip,
			   rec->arch.mod->arch.tramp, BRANCH_SET_LINK);
	if (!op) {
		printk(KERN_ERR "REL24 out of range!\n");
		return -EINVAL;
	}

	pr_devel("write to %lx\n", rec->ip);

	if (patch_instruction((unsigned int *)ip, op))
		return -EPERM;

	return 0;
}
#endif /* CONFIG_PPC64 */
#endif /* CONFIG_MODULES */

int ftrace_make_call(struct fentry *rec, unsigned long addr)
{
	unsigned long ip = rec->ip;
	unsigned int old, new;

	/*
	 * If the calling address is more that 24 bits away,
	 * then we had to use a trampoline to make the call.
	 * Otherwise just update the call site.
	 */
	if (test_24bit_addr(ip, addr)) {
		/* within range */
		old = PPC_INST_NOP;
		new = fentry_call_replace(ip, addr, 1);
		return fentry_modify_code(ip, old, new);
	}

#ifdef CONFIG_MODULES
	/*
	 * Out of range jumps are called from modules.
	 * Being that we are converting from nop, it had better
	 * already have a module defined.
	 */
	if (!rec->arch.mod) {
		printk(KERN_ERR "No module loaded\n");
		return -EINVAL;
	}

	return __ftrace_make_call(rec, addr);
#else
	/* We should not get here without modules */
	return -EINVAL;
#endif /* CONFIG_MODULES */
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	unsigned long ip = (unsigned long)(&ftrace_call);
	unsigned int old, new;
	int ret;

	old = *(unsigned int *)&ftrace_call;
	new = fentry_call_replace(ip, (unsigned long)func, 1);
	ret = fentry_modify_code(ip, old, new);

	return ret;
}

static int __ftrace_replace_code(struct fentry *rec, int enable)
{
	unsigned long ftrace_addr = (unsigned long)FTRACE_ADDR;
	int ret;

	ret = ftrace_update_record(rec, enable);

	switch (ret) {
	case FTRACE_UPDATE_IGNORE:
		return 0;
	case FTRACE_UPDATE_MAKE_CALL:
		return ftrace_make_call(rec, ftrace_addr);
	case FTRACE_UPDATE_MAKE_NOP:
		return fentry_make_nop(NULL, rec, ftrace_addr);
	}

	return 0;
}

void ftrace_replace_code(int enable)
{
	struct ftrace_rec_iter *iter;
	struct fentry *rec;
	int ret;

	for (iter = ftrace_rec_iter_start(); iter;
	     iter = ftrace_rec_iter_next(iter)) {
		rec = ftrace_rec_iter_record(iter);
		ret = __ftrace_replace_code(rec, enable);
		if (ret) {
			ftrace_bug(ret, rec->ip);
			return;
		}
	}
}

void arch_ftrace_update_code(int command)
{
	if (command & FTRACE_UPDATE_CALLS)
		ftrace_replace_code(1);
	else if (command & FTRACE_DISABLE_CALLS)
		ftrace_replace_code(0);

	if (command & FTRACE_UPDATE_TRACE_FUNC)
		ftrace_update_ftrace_func(ftrace_trace_function);

	if (command & FTRACE_START_FUNC_RET)
		ftrace_enable_ftrace_graph_caller();
	else if (command & FTRACE_STOP_FUNC_RET)
		ftrace_disable_ftrace_graph_caller();
}
#endif /* CONFIG_DYNAMIC_FTRACE */

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

#ifdef CONFIG_DYNAMIC_FTRACE
extern void ftrace_graph_call(void);
extern void ftrace_graph_stub(void);

int ftrace_enable_ftrace_graph_caller(void)
{
	unsigned long ip = (unsigned long)(&ftrace_graph_call);
	unsigned long addr = (unsigned long)(&ftrace_graph_caller);
	unsigned long stub = (unsigned long)(&ftrace_graph_stub);
	unsigned int old, new;

	old = fentry_call_replace(ip, stub, 0);
	new = fentry_call_replace(ip, addr, 0);

	return fentry_modify_code(ip, old, new);
}

int ftrace_disable_ftrace_graph_caller(void)
{
	unsigned long ip = (unsigned long)(&ftrace_graph_call);
	unsigned long addr = (unsigned long)(&ftrace_graph_caller);
	unsigned long stub = (unsigned long)(&ftrace_graph_stub);
	unsigned int old, new;

	old = fentry_call_replace(ip, addr, 0);
	new = fentry_call_replace(ip, stub, 0);

	return fentry_modify_code(ip, old, new);
}
#endif /* CONFIG_DYNAMIC_FTRACE */

#ifdef CONFIG_PPC64
extern void mod_return_to_handler(void);
#endif

/*
 * Hook the return address and push it in the stack of return addrs
 * in current thread info.
 */
void prepare_ftrace_return(unsigned long *parent, unsigned long self_addr)
{
	unsigned long old;
	int faulted;
	struct ftrace_graph_ent trace;
	unsigned long return_hooker = (unsigned long)&return_to_handler;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		return;

#ifdef CONFIG_PPC64
	/* non core kernel code needs to save and restore the TOC */
	if (REGION_ID(self_addr) != KERNEL_REGION_ID)
		return_hooker = (unsigned long)&mod_return_to_handler;
#endif

	return_hooker = ppc_function_entry((void *)return_hooker);

	/*
	 * Protect against fault, even if it shouldn't
	 * happen. This tool is too much intrusive to
	 * ignore such a protection.
	 */
	asm volatile(
		"1: " PPC_LL "%[old], 0(%[parent])\n"
		"2: " PPC_STL "%[return_hooker], 0(%[parent])\n"
		"   li %[faulted], 0\n"
		"3:\n"

		".section .fixup, \"ax\"\n"
		"4: li %[faulted], 1\n"
		"   b 3b\n"
		".previous\n"

		".section __ex_table,\"a\"\n"
			PPC_LONG_ALIGN "\n"
			PPC_LONG "1b,4b\n"
			PPC_LONG "2b,4b\n"
		".previous"

		: [old] "=&r" (old), [faulted] "=r" (faulted)
		: [parent] "r" (parent), [return_hooker] "r" (return_hooker)
		: "memory"
	);

	if (unlikely(faulted)) {
		ftrace_graph_stop();
		WARN_ON(1);
		return;
	}

	trace.func = self_addr;
	trace.depth = current->curr_ret_stack + 1;

	/* Only trace if the calling function expects to */
	if (!ftrace_graph_entry(&trace)) {
		*parent = old;
		return;
	}

	if (ftrace_push_return_trace(old, self_addr, &trace.depth, 0) == -EBUSY)
		*parent = old;
}
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */

#if defined(CONFIG_FTRACE_SYSCALLS) && defined(CONFIG_PPC64)
unsigned long __init arch_syscall_addr(int nr)
{
	return sys_call_table[nr*2];
}
#endif /* CONFIG_FTRACE_SYSCALLS && CONFIG_PPC64 */
