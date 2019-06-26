/**
 * @file winfuse/path.c
 *
 * @copyright 2019 Bill Zissimopoulos
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

#include <winfuse/driver.h>

VOID FusePosixPathPrefix(PSTRING Path, PSTRING Prefix, PSTRING Remain);
VOID FusePosixPathSuffix(PSTRING Path, PSTRING Remain, PSTRING Suffix);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FusePosixPathPrefix)
#pragma alloc_text(PAGE, FusePosixPathSuffix)
#endif

VOID FusePosixPathPrefix(PSTRING Path, PSTRING Prefix, PSTRING Remain)
{
    PAGED_CODE();

    PSTR P = Path->Buffer, EndP = P + Path->Length / sizeof(*P);

    if (EndP > P && '/' == *P)
    {
        Prefix->Length = 1;
        Prefix->Buffer = Path->Buffer;
    }
    else
    {
        while (EndP > P && '/' != *P)
            P++;

        Prefix->Length = (USHORT)((P - Path->Buffer) * sizeof *P);
        Prefix->Buffer = Path->Buffer;
    }

    while (EndP > P && '/' == *P)
        P++;

    Remain->Length = (USHORT)((EndP - P) * sizeof *P);
    Remain->Buffer = P;

    Prefix->MaximumLength = Prefix->Length;
    Remain->MaximumLength = Remain->Length;
}

VOID FusePosixPathSuffix(PSTRING Path, PSTRING Remain, PSTRING Suffix)
{
    PAGED_CODE();

    PSTR P = Path->Buffer, EndP = P + Path->Length / sizeof(*P);

    Remain->Length = Path->Length;
    Remain->Buffer = Path->Buffer;

    Suffix->Length = 0;
    Suffix->Buffer = Path->Buffer;

    while (EndP > P)
        if ('/' == *P)
        {
            Remain->Length = (USHORT)((P - Path->Buffer) * sizeof *P);
            if (0 == Remain->Length)
                Remain->Length = 1;

            while (EndP > P && '/' == *P)
                P++;

            Suffix->Length = (USHORT)((EndP - P) * sizeof *P);
            Suffix->Buffer = P;
        }
        else
            P++;

    Remain->MaximumLength = Remain->Length;
    Suffix->MaximumLength = Suffix->Length;
}
