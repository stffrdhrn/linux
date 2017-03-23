/*
 * Copyright (C) 2014 Stefan Kristiansson <stefan.kristiansson@saunalahti.fi>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * Note:
 * The portable implementations of 1 and 2 byte xchg and cmpxchg using a 4
 * byte cmpxchg is sourced heavily from the sh implementation.
 */

#ifndef __ASM_OPENRISC_CMPXCHG_H
#define __ASM_OPENRISC_CMPXCHG_H

#include  <linux/types.h>
#include  <linux/bitops.h>

/*
 * This function doesn't exist, so you'll get a linker error
 * if something tries to do an invalid cmpxchg().
 */

#define __HAVE_ARCH_CMPXCHG 1

static inline unsigned long cmpxchg_u32(volatile void *ptr,
		unsigned long old, unsigned long new)
{
	__asm__ __volatile__(
		"1:	l.lwa %0, 0(%1)		\n"
		"	l.sfeq %0, %2		\n"
		"	l.bnf 2f		\n"
		"	 l.nop			\n"
		"	l.swa 0(%1), %3		\n"
		"	l.bnf 1b		\n"
		"	 l.nop			\n"
		"2:				\n"
		: "=&r"(old)
		: "r"(ptr), "r"(old), "r"(new)
		: "cc", "memory");

	return old;
}

static inline unsigned long xchg_u32(volatile void *ptr,
		unsigned long val)
{
	__asm__ __volatile__(
		"1:	l.lwa %0, 0(%1)		\n"
		"	l.swa 0(%1), %2		\n"
		"	l.bnf 1b		\n"
		"	 l.nop			\n"
		: "=&r"(val)
		: "r"(ptr), "r"(val)
		: "cc", "memory");

	return val;
}

static inline u32 cmpxchg_ux(volatile void *ptr, u32 old, u32 new, int size)
{
	int off = (unsigned long)ptr % sizeof(u32);
	volatile u32 *p = ptr - off;
#ifdef __BIG_ENDIAN
	int bitoff = (sizeof(u32) - size - off) * BITS_PER_BYTE;
#else
	int bitoff = off * BITS_PER_BYTE;
#endif
	u32 bitmask = ((0x1 << size * BITS_PER_BYTE) - 1) << bitoff;
	u32 old_in, old_out, newv;
	u32 ret;

	do {
		old_in = READ_ONCE(*p);
		old_in = (old_in & ~bitmask) | (old << bitoff);
		newv = (old_in & ~bitmask) | (new << bitoff);

		/* Do 32 bit cmpxchg */
		old_out = cmpxchg_u32(p, old_in, newv);

		ret = (old_out & bitmask) >> bitoff;
	} while (old_in != old_out && old == ret);
	/*
 	 * Keep trying if cmpxchg failed, but at data level
 	 * we would indicate success.
         */

	return ret;
}

static inline unsigned long cmpxchg_u16(volatile u16 *m,
		unsigned long old, unsigned long new)
{
	return cmpxchg_ux(m, old, new, sizeof *m);
}

static inline unsigned long cmpxchg_u8(volatile u8 *m,
		unsigned long old, unsigned long new)
{
	return cmpxchg_ux(m, old, new, sizeof *m);
}

/* xchg */

static inline u32 xchg_ux(volatile void *ptr, u32 x, int size)
{
	int off = (unsigned long)ptr % sizeof(u32);
	volatile u32 *p = ptr - off;
#ifdef __BIG_ENDIAN
	int bitoff = (sizeof(u32) - size - off) * BITS_PER_BYTE;
#else
	int bitoff = off * BITS_PER_BYTE;
#endif
	u32 bitmask = ((0x1 << size * BITS_PER_BYTE) - 1) << bitoff;
	u32 oldv, newv;
	u32 ret;

	do {
		oldv = READ_ONCE(*p);
		ret = (oldv & bitmask) >> bitoff;
		newv = (oldv & ~bitmask) | (x << bitoff);
	} while (cmpxchg_u32(p, oldv, newv) != oldv);

	return ret;
}

static inline unsigned long xchg_u16(volatile u16 *m, unsigned long val)
{
	return xchg_ux(m, val, sizeof *m);
}

static inline unsigned long xchg_u8(volatile u8 *m, unsigned long val)
{
	return xchg_ux(m, val, sizeof *m);
}

/* This function doesn't exist, so you'll get a linker error
 * if something tries to do an invalid cmpxchg(). */
extern void __cmpxchg_called_with_bad_pointer(void);

static inline unsigned long __cmpxchg(volatile void * ptr, unsigned long old,
		unsigned long new, int size)
{
	switch (size) {
	case 4:
		return cmpxchg_u32(ptr, old, new);
	case 2:
		return cmpxchg_u16(ptr, old, new);
	case 1:
		return cmpxchg_u8(ptr, old, new);
	}
	__cmpxchg_called_with_bad_pointer();
	return old;
}


#define cmpxchg(ptr, o, n)						\
	({								\
		(__typeof__(*(ptr))) __cmpxchg((ptr),			\
					       (unsigned long)(o),	\
					       (unsigned long)(n),	\
					       sizeof(*(ptr)));		\
	})

/*
 * This function doesn't exist, so you'll get a linker error if
 * something tries to do an invalidly-sized xchg().
 */
extern void __xchg_called_with_bad_pointer(void);

static inline unsigned long __xchg(volatile void * ptr, unsigned long with, int size)
{
	switch (size) {
	case 4:
		return xchg_u32(ptr, with);
	case 2:
		return xchg_u16(ptr, with);
	case 1:
		return xchg_u8(ptr, with);
	}
	__xchg_called_with_bad_pointer();
	return with;
}



#define xchg(ptr, with) 						\
	({								\
		(__typeof__(*(ptr))) __xchg((ptr),			\
					    (unsigned long)(with),	\
					    sizeof(*(ptr)));		\
	})

#endif /* __ASM_OPENRISC_CMPXCHG_H */
