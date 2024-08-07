/*
 * Copyright (c) 2021-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "deconz/u_assert.h"
#include "deconz/atom_table.h"
#include <utils/stringcache.h>

unsigned StringCacheAdd(const char *str, unsigned length, StringCacheMode mode)
{
    if (!str || length == 0)
    {
        return STRING_CACHE_INVALID_HANDLE;
    }

    if (mode == StringCacheImmutable)
    {
        AT_AtomIndex ati;
        if (AT_AddAtom(str, length, &ati))
        {
            return ati.index;
        }
    }
    else if (mode == StringCacheMutable)
    {
        U_ASSERT(0); // TODO implement
    }

    return STRING_CACHE_INVALID_HANDLE;
}

bool StringCacheGet(unsigned handle, const char **str, unsigned *length)
{
    U_ASSERT(str);
    U_ASSERT(length);

    if (handle == STRING_CACHE_INVALID_HANDLE)
    {
        *str = nullptr;
        *length = 0;
        return false;
    }

    AT_Atom atom;
    AT_AtomIndex ati;
    ati.index = handle;
    atom = AT_GetAtomByIndex(ati);

    if (atom.len != 0 && atom.data && atom.data[atom.len] == '\0')
    {
        *str = (char*)atom.data;
        *length = atom.len;
        return true;
    }

    *str = nullptr;
    *length = 0;
    return false;
}
