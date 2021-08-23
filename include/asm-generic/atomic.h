// SPDX-License-Identifier: GPL-2.0-only
/*
 *  based on linux/include/asm-arm/atomic.h
 *
 *  Copyright (c) 1996 Russell King.
 */

#ifndef __ASM_GENERIC_ATOMIC_H
#define __ASM_GENERIC_ATOMIC_H

#ifdef CONFIG_SMP
#error SMP not supported
#endif
#define ATOMIC_INIT(i)	{ (i) }

#ifdef CONFIG_64BIT
typedef struct { s64 counter; } atomic64_t;

#define atomic64_read(v)	((v)->counter)
#define atomic64_set(v, i)	(((v)->counter) = (i))

static inline void atomic64_add(s64 i, volatile atomic64_t *v)
{
	v->counter += i;
}

static inline void atomic64_sub(s64 i, volatile atomic64_t *v)
{
	v->counter -= i;
}

static inline void atomic64_inc(volatile atomic64_t *v)
{
	v->counter += 1;
}

static inline void atomic64_dec(volatile atomic64_t *v)
{
	v->counter -= 1;
}

static inline int atomic64_dec_and_test(volatile atomic64_t *v)
{
	s64 val;

	val = v->counter;
	v->counter = val -= 1;

	return val == 0;
}

static inline int atomic64_add_negative(s64 i, volatile atomic64_t *v)
{
	s64 val;

	val = v->counter;
	v->counter = val += i;

	return val < 0;
}

#else
typedef struct { volatile int counter; } atomic_t;

#define ATOMIC_INIT(i)	{ (i) }

#define atomic_read(v)	((v)->counter)
#define atomic_set(v,i)	(((v)->counter) = (i))

static inline void atomic_add(int i, volatile atomic_t *v)
{
	v->counter += i;
}

static inline void atomic_sub(int i, volatile atomic_t *v)
{
	v->counter -= i;
}

static inline void atomic_inc(volatile atomic_t *v)
{
	v->counter += 1;
}

static inline void atomic_dec(volatile atomic_t *v)
{
	v->counter -= 1;
}

static inline int atomic_dec_and_test(volatile atomic_t *v)
{
	int val;

	val = v->counter;
	v->counter = val -= 1;

	return val == 0;
}

static inline int atomic_add_negative(int i, volatile atomic_t *v)
{
	int val;

	val = v->counter;
	v->counter = val += i;

	return val < 0;
}

static inline void atomic_clear_mask(unsigned long mask, unsigned long *addr)
{
	*addr &= ~mask;
}
#endif

/* Atomic operations are already serializing on ARM */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#endif
