#ifndef ASM_FENTRY_H
#define ASM_FENTRY_H

#ifdef CC_USING_FENTRY
# define fentry_hook	__fentry__
#else
# define fentry_hook	mcount
#endif

#ifndef __ASSEMBLY__
extern void fentry_hook(void);
#endif /* __ASSEMBLY__ */

#endif /* ASM_FENTRY_H */
