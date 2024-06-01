/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef STRING_CACHE_H
#define STRING_CACHE_H

#define STRING_CACHE_INVALID_HANDLE 0

/*! \class StringCache

    A static string cache with zero dynamic memory allocations.
    Immutable read only strings are only added to the cache but never removed.
    Mutable string can be used for things like per resource names.
*/
enum StringCacheMode
{
    StringCacheMutable,
    StringCacheImmutable
};

/*! Adds string to the global string cache.

    \returns non zero handle or STRING_CACHE_INVALID_HANDLE.
 */
unsigned StringCacheAdd(const char *str, unsigned length, StringCacheMode mode);
bool StringCacheGet(unsigned handle, const char **str, unsigned *length);

#endif // STRING_CACHE_H
