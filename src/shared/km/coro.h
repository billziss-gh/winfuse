/**
 * @file shared/km/coro.h
 *
 * @copyright 2019-2020 Bill Zissimopoulos
 */
/*
 * This file is part of WinFuse.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * Affero General Public License version 3 as published by the Free
 * Software Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the AGPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#ifndef SHARED_KM_CORO_H_INCLUDED
#define SHARED_KM_CORO_H_INCLUDED

/*
 * Nested coroutines
 *
 * This is a simple implementation of nested coroutines for C using macros. It introduces
 * the macros coro_block, coro_await, coro_yield and coro_break that are used to create coroutines,
 * suspend/resume them and exit them. This implementation supports nested coroutines in that
 * a coroutine may invoke another coroutine and the whole stack of coroutines may be suspended
 * and later resumed.
 *
 * The implementation is able to do this by maintaining a stack of program "points" where
 * coroutine execution may be suspended and later resumed. The implementation achieves this by
 * (ab)using a peculiarity of the C switch statement in that it allows case: labels to appear
 * anywhere within the body of a switch statement including nested compound statements.
 *
 * While this implementation is able to maintain the stack of program "points" where execution
 * may be resumed, it is not able to automatically maintain important state such as activation
 * records (local variables). It is the responsibility of the programmer using these macros
 * to maintain this state. Usually this is done by moving all local variables together with the
 * space for the program "points" stack into a heap allocation that is then passed as an argument
 * to the coroutines.
 *
 * The original coroutine implementation was done some time around 2010 and did not support
 * nesting. It was used to facilitate the implementation of complex iterators in a C++ library.
 * It was inspired by Simon Tatham's "Coroutines in C":
 *     http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
 *
 * Reference
 *
 * The coroutine state is maintained in an integer array (S) whose address is passed to the
 * "block" statement. The array's number of elements must be 2 plus the number of expected
 * coroutines (e.g. if you expect to have up to 3 nested coroutines, the array must have at
 * least 5 elements). The array must be initialized to 0 on initial entry to a coroutine block.
 *
 *     coro_block(S)
 *         This macro introduces a coroutine statement block { ... } where the "await", "yield",
 *         and "break" statements can be used. There can only be one such block within a function.
 *     coro_await(E)
 *         This macro executes the expression E, which should be an invocation of a coroutine.
 *         The nested coroutine may suspend itself, in which case the "await" statement exits
 *         the current coroutine statement block. The coroutine block can be reentered in which
 *         case execution will continue within the coroutine invoked by E, unless the coroutine
 *         is complete. It is an error to use "await" outside of a coroutine block.
 *     coro_yield
 *         This macro exits the current coroutine statement block. The coroutine block can be
 *         reentered in which case execution will continue after the "yield" statement. It is
 *         an error to use "yield" outside of a coroutine block.
 *     coro_break
 *         This macro exits the current coroutine statement block and marks it as "complete".
 *         If the coroutine block is reentered it exits immediately. It is an error to use "break"
 *         outside of a coroutine block.
 */
#define coro_block(S)       short *coro_S__ = (S); if (!coro_active()) coro_X__:; else \
                            switch (coro_enter__()) for (; 0,0; coro_leave__(-1)) case 0:
#define coro_await__(N, E)  do { E; coro_cond_leave__(N); goto coro_X__; case N:; } while (1,1)
#define coro_yield__(N)     do { coro_leave__(N); goto coro_X__; case N:; } while (0,0)
#define coro_break          do { coro_leave__(-1); goto coro_X__; } while (0,0)
#define coro_active()       (-1 != coro_below__)
#define coro_below__        (coro_S__[coro_S__[0] + 1])
#define coro_enter__()      (coro_S__[++coro_S__[0]])
#define coro_leave__(N)     (coro_below__ = 0, coro_S__[coro_S__[0]--] = N)
#define coro_cond_leave__(N)if (!coro_active()) { coro_below__ = 0; break; } (coro_S__[coro_S__[0]--] = N)
#if defined(__COUNTER__)
#define coro_await(...)     coro_await__((__COUNTER__ + 1), (__VA_ARGS__))
#define coro_yield          coro_yield__((__COUNTER__ + 1))
#else
#define coro_await(...)     coro_await__(__LINE__, (__VA_ARGS__))
#define coro_yield          coro_yield__(__LINE__)
#endif

#endif
