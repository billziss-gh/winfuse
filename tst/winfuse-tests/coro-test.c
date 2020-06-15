/**
 * @file coro-test.c
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

#include <windows.h>
#include <tlib/testsuite.h>
#include <shared/km/coro.h>

static void bprintf(char **b, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int len = wvsprintfA(*b, format, ap);
    va_end(ap);
    *b += len;
}

static int coro_break_dotest(int brk, short stack[8], char **b)
{
    coro_block (stack)
    {
        bprintf(b, "%s\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_yield_dotest(int brk, short stack[8], char **b)
{
    coro_block (stack)
    {
        bprintf(b, "%s:10\n", __FUNCTION__);
        coro_yield;

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_loopyield_dotest(int brk, short stack[8], int *state, char **b)
{
    coro_block (stack)
    {
        while ((*state)++ < 3)
        {
            bprintf(b, "%s:10:%d\n", __FUNCTION__, *state);
            coro_yield;
        }

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_await_break_dotest(int brk, short stack[8], char **b)
{
    coro_block (stack)
    {
        bprintf(b, "%s:10\n", __FUNCTION__);
        coro_await (coro_break_dotest(brk, stack, b));

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_await_yield_dotest(int brk, short stack[8], char **b)
{
    coro_block (stack)
    {
        bprintf(b, "%s:10\n", __FUNCTION__);
        coro_await (coro_yield_dotest(brk, stack, b));

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_await_loopyield_dotest(int brk, short stack[8], int *state, char **b)
{
    coro_block (stack)
    {
        bprintf(b, "%s:10\n", __FUNCTION__);
        coro_await (coro_loopyield_dotest(brk, stack, state, b));

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_loopawait_yield_dotest(int brk, short stack[8], int *state, char **b)
{
    coro_block (stack)
    {
        while ((*state)++ < 3)
        {
            bprintf(b, "%s:10:%d\n", __FUNCTION__, *state);
            coro_await (coro_yield_dotest(brk, stack, b));
        }

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_await_await_yield_dotest(int brk, short stack[8], char **b)
{
    coro_block (stack)
    {
        bprintf(b, "%s:10\n", __FUNCTION__);
        coro_await (coro_await_yield_dotest(brk, stack, b));

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_await_loopawait_yield_dotest(int brk, short stack[8], int *state, char **b)
{
    coro_block (stack)
    {
        bprintf(b, "%s:10\n", __FUNCTION__);
        coro_await (coro_loopawait_yield_dotest(brk, stack, state, b));

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static int coro_loopawait_loopawait_yield_dotest(int brk, short stack[8], int *state, char **b)
{
    coro_block (stack)
    {
        while ((*state)++ < 2)
        {
            bprintf(b, "%s:10:%d\n", __FUNCTION__, *state);
            state[1] = 0;
            coro_await (coro_loopawait_yield_dotest(brk, stack, state + 1, b));
        }

        bprintf(b, "%s:20\n", __FUNCTION__);
        if (brk) coro_break;
    }

    return coro_active();
}

static void coro_dotest(int brk)
{
    short stack[8];
    int state[2];
    char buf[2048], *b;

    memset(stack, 0, sizeof stack);
    b = buf;
    ASSERT(!coro_break_dotest(brk, stack, &b));
    ASSERT(!coro_break_dotest(brk, stack, &b));
    ASSERT(0 == strcmp(buf, "coro_break_dotest\n"));

    memset(stack, 0, sizeof stack);
    b = buf;
    ASSERT( coro_yield_dotest(brk, stack, &b));
    ASSERT(!coro_yield_dotest(brk, stack, &b));
    ASSERT(!coro_yield_dotest(brk, stack, &b));
    ASSERT(0 == strcmp(buf, "coro_yield_dotest:10\ncoro_yield_dotest:20\n"));

    memset(stack, 0, sizeof stack);
    state[0] = 0;
    b = buf;
    ASSERT( coro_loopyield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopyield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopyield_dotest(brk, stack, state, &b));
    ASSERT(!coro_loopyield_dotest(brk, stack, state, &b));
    ASSERT(!coro_loopyield_dotest(brk, stack, state, &b));
    ASSERT(0 == strcmp(buf,
        "coro_loopyield_dotest:10:1\n"
        "coro_loopyield_dotest:10:2\n"
        "coro_loopyield_dotest:10:3\n"
        "coro_loopyield_dotest:20\n"));

    memset(stack, 0, sizeof stack);
    b = buf;
    ASSERT(!coro_await_break_dotest(brk, stack, &b));
    ASSERT(!coro_await_break_dotest(brk, stack, &b));
    ASSERT(0 == strcmp(buf, "coro_await_break_dotest:10\ncoro_break_dotest\ncoro_await_break_dotest:20\n"));

    memset(stack, 0, sizeof stack);
    b = buf;
    ASSERT( coro_await_yield_dotest(brk, stack, &b));
    ASSERT(!coro_await_yield_dotest(brk, stack, &b));
    ASSERT(!coro_await_yield_dotest(brk, stack, &b));
    ASSERT(0 == strcmp(buf, "coro_await_yield_dotest:10\ncoro_yield_dotest:10\ncoro_yield_dotest:20\ncoro_await_yield_dotest:20\n"));

    memset(stack, 0, sizeof stack);
    state[0] = 0;
    b = buf;
    ASSERT( coro_await_loopyield_dotest(brk, stack, state, &b));
    ASSERT( coro_await_loopyield_dotest(brk, stack, state, &b));
    ASSERT( coro_await_loopyield_dotest(brk, stack, state, &b));
    ASSERT(!coro_await_loopyield_dotest(brk, stack, state, &b));
    ASSERT(!coro_await_loopyield_dotest(brk, stack, state, &b));
    ASSERT(0 == strcmp(buf,
        "coro_await_loopyield_dotest:10\n"
        "coro_loopyield_dotest:10:1\n"
        "coro_loopyield_dotest:10:2\n"
        "coro_loopyield_dotest:10:3\n"
        "coro_loopyield_dotest:20\n"
        "coro_await_loopyield_dotest:20\n"));

    memset(stack, 0, sizeof stack);
    state[0] = 0;
    b = buf;
    ASSERT( coro_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(!coro_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(!coro_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(0 == strcmp(buf,
        "coro_loopawait_yield_dotest:10:1\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:10:2\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:10:3\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:20\n"));

    memset(stack, 0, sizeof stack);
    b = buf;
    ASSERT( coro_await_await_yield_dotest(brk, stack, &b));
    ASSERT(!coro_await_await_yield_dotest(brk, stack, &b));
    ASSERT(!coro_await_await_yield_dotest(brk, stack, &b));
    ASSERT(0 == strcmp(buf,
        "coro_await_await_yield_dotest:10\n"
        "coro_await_yield_dotest:10\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_await_yield_dotest:20\n"
        "coro_await_await_yield_dotest:20\n"));

    memset(stack, 0, sizeof stack);
    state[0] = 0;
    b = buf;
    ASSERT( coro_await_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_await_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_await_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(!coro_await_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(!coro_await_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(0 == strcmp(buf,
        "coro_await_loopawait_yield_dotest:10\n"
        "coro_loopawait_yield_dotest:10:1\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:10:2\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:10:3\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:20\n"
        "coro_await_loopawait_yield_dotest:20\n"));

    memset(stack, 0, sizeof stack);
    state[0] = 0;
    b = buf;
    ASSERT( coro_loopawait_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopawait_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopawait_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopawait_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopawait_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT( coro_loopawait_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(!coro_loopawait_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(!coro_loopawait_loopawait_yield_dotest(brk, stack, state, &b));
    ASSERT(0 == strcmp(buf,
        "coro_loopawait_loopawait_yield_dotest:10:1\n"
        "coro_loopawait_yield_dotest:10:1\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:10:2\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:10:3\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:20\n"
        "coro_loopawait_loopawait_yield_dotest:10:2\n"
        "coro_loopawait_yield_dotest:10:1\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:10:2\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:10:3\n"
        "coro_yield_dotest:10\n"
        "coro_yield_dotest:20\n"
        "coro_loopawait_yield_dotest:20\n"
        "coro_loopawait_loopawait_yield_dotest:20\n"));
}

static void coro_test(void)
{
    coro_dotest(0);
    coro_dotest(1);
}

void coro_tests(void)
{
    TEST(coro_test);
}
