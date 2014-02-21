/*
 * Code for replacing ftrace calls with jumps.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 *
 * Thanks goes to Ingo Molnar, for suggesting the idea.
 * Mathieu Desnoyers, for suggesting postponing the modifications.
 * Arjan van de Ven, for keeping me straight, and explaining to me
 * the dangers of modifying code on the run.
 */

#include <linux/bug.h>
#include <linux/fentry.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <asm/insn.h>
#include <asm/nops.h>
#include <asm/sections.h>

int fentry_make_nop(struct module *mod,
		    struct fentry *rec, unsigned long addr)
{
	union insn_call_jmp_union insn;
	unsigned const char *new;
	unsigned long ip = rec->ip;

	insn_call_jmp(&insn, true, ip, addr);
	new = ideal_nops[NOP_ATOMIC5];

	/*
	 * On boot up, and when modules are loaded, the MCOUNT_ADDR
	 * is converted to a nop, and will never become MCOUNT_ADDR
	 * again. This code is either running before SMP (on boot up)
	 * or before the code will ever be executed (module load).
	 * We do not want to use the breakpoint version in this case,
	 * just modify the code directly.
	 */
	if (addr == MCOUNT_ADDR)
		return text_poke_direct(rec->ip, insn.code, new,
				MCOUNT_INSN_SIZE);

	/* Normal cases use add_brk_on_nop */
	WARN_ONCE(1, "invalid use of ftrace_make_nop");
	return -EINVAL;
}

int __init fentry_dyn_arch_init(void)
{
	return 0;
}
