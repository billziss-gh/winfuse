/**
 * @file path-test.c
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

#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>

#define SHARED_KM_SHARED_H_INCLUDED
#define PAGED_CODE()
#include <shared/km/path.c>

void path_prefix_test(void)
{
    PSTR ipaths[] =
    {
        "",
        "/",
        "//",
        "/a",
        "//a",
        "//a/",
        "//a//",
        "a/",
        "a//",
        "a/b",
        "a//b",
        "foo///bar//baz",
        "foo///bar//baz/",
        "foo///bar//baz//",
        "foo",
        "/foo/bar/baz",
    };
    PSTR opaths[] =
    {
        "", "",
        "/", "",
        "/", "",
        "/", "a",
        "/", "a",
        "/", "a/",
        "/", "a//",
        "a", "",
        "a", "",
        "a", "b",
        "a", "b",
        "foo", "bar//baz",
        "foo", "bar//baz/",
        "foo", "bar//baz//",
        "foo", "",
        "/", "foo/bar/baz",
    };

    for (size_t i = 0; sizeof ipaths / sizeof ipaths[0] > i; i++)
    {
        STRING Path, Prefix, Remain;

        Path.Length = Path.MaximumLength = (USHORT)strlen(ipaths[i]);
        Path.Buffer = ipaths[i];

        FusePosixPathPrefix(&Path, &Prefix, &Remain);

        ASSERT(Prefix.Length == strlen(opaths[2 * i + 0]));
        ASSERT(Prefix.Length == Prefix.MaximumLength);
        ASSERT(0 == memcmp(opaths[2 * i + 0], Prefix.Buffer, Prefix.Length));

        ASSERT(Remain.Length == strlen(opaths[2 * i + 1]));
        ASSERT(Remain.Length == Remain.MaximumLength);
        ASSERT(0 == memcmp(opaths[2 * i + 1], Remain.Buffer, Remain.Length));
    }
}

void path_suffix_test(void)
{
    PSTR ipaths[] =
    {
        "",
        "/",
        "//",
        "/a",
        "//a",
        "//a/",
        "//a//",
        "a/",
        "a//",
        "a/b",
        "a//b",
        "foo///bar//baz",
        "foo///bar//baz/",
        "foo///bar//baz//",
        "foo",
        "/foo/bar/baz",
    };
    PSTR opaths[] =
    {
        "", "",
        "/", "",
        "/", "",
        "/", "a",
        "/", "a",
        "//a", "",
        "//a", "",
        "a", "",
        "a", "",
        "a", "b",
        "a", "b",
        "foo///bar", "baz",
        "foo///bar//baz", "",
        "foo///bar//baz", "",
        "foo", "",
        "/foo/bar", "baz",
    };

    for (size_t i = 0; sizeof ipaths / sizeof ipaths[0] > i; i++)
    {
        STRING Path, Remain, Suffix;

        Path.Length = Path.MaximumLength = (USHORT)strlen(ipaths[i]);
        Path.Buffer = ipaths[i];

        FusePosixPathSuffix(&Path, &Remain, &Suffix);

        ASSERT(Remain.Length == strlen(opaths[2 * i + 0]));
        ASSERT(Remain.Length == Remain.MaximumLength);
        ASSERT(0 == memcmp(opaths[2 * i + 0], Remain.Buffer, Remain.Length));

        ASSERT(Suffix.Length == strlen(opaths[2 * i + 1]));
        ASSERT(Suffix.Length == Suffix.MaximumLength);
        ASSERT(0 == memcmp(opaths[2 * i + 1], Suffix.Buffer, Suffix.Length));
    }
}

void path_tests(void)
{
    TEST(path_prefix_test);
    TEST(path_suffix_test);
}
