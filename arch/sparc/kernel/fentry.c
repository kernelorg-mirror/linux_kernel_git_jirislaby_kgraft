#include <linux/fentry.h>
#include <linux/kernel.h>

static const u32 fentry_nop = 0x01000000;

int fentry_modify_code(unsigned long ip, u32 old, u32 new)
{
	u32 replaced;
	int faulted;

	__asm__ __volatile__(
	"1:	cas	[%[ip]], %[old], %[new]\n"
	"	flush	%[ip]\n"
	"	mov	0, %[faulted]\n"
	"2:\n"
	"	.section .fixup,#alloc,#execinstr\n"
	"	.align	4\n"
	"3:	sethi	%%hi(2b), %[faulted]\n"
	"	jmpl	%[faulted] + %%lo(2b), %%g0\n"
	"	 mov	1, %[faulted]\n"
	"	.previous\n"
	"	.section __ex_table,\"a\"\n"
	"	.align	4\n"
	"	.word	1b, 3b\n"
	"	.previous\n"
	: "=r" (replaced), [faulted] "=r" (faulted)
	: [new] "0" (new), [old] "r" (old), [ip] "r" (ip)
	: "memory");

	if (replaced != old && replaced != new)
		faulted = 2;

	return faulted;
}

int fentry_make_nop(struct module *mod, struct fentry *rec, unsigned long addr)
{
	unsigned long ip = rec->ip;
	u32 old, new;

	old = fentry_call_replace(ip, addr);
	new = fentry_nop;
	return fentry_modify_code(ip, old, new);
}

int __init fentry_dyn_arch_init(void)
{
	return 0;
}
